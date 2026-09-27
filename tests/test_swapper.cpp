#include <doctest/doctest.h>

#include "Core/Swapper.hpp"
#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

namespace
{
Catalog catalog_with_tracks(const test::TempDir& dir)
{
    REQUIRE(write_text_file_atomic(dir.file("bosses.json"), R"({"bosses": [
        {"id": "Boss_Sirene", "name": "Sirène"}
    ]})"));
    REQUIRE(write_text_file_atomic(dir.file("tracks.json"), R"({"tracks": [
        {"id": "Track_Lumiere", "name": "Lumière"},
        {"id": "Track_UneVie", "name": "Une Vie à T'aimer"}
    ]})"));
    Catalog catalog;
    REQUIRE(catalog.load_bosses(dir.file("bosses.json")));
    REQUIRE(catalog.load_tracks(dir.file("tracks.json")));
    return catalog;
}
} // namespace

TEST_CASE("config vazia nunca troca nada: e a garantia central do mod")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    const OverrideTable overrides;
    const Swapper swapper{overrides, catalog};

    const auto decision = swapper.decide("Boss_Sirene", "Track_Original");
    CHECK_FALSE(decision.swap);
    CHECK(decision.reason == SwapReason::NoOverride);
}

TEST_CASE("override explicito troca a faixa")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_UneVie");

    const Swapper swapper{overrides, catalog};
    const auto decision = swapper.decide("Boss_Sirene", "Track_Lumiere");
    CHECK(decision.swap);
    CHECK(decision.track_id == "Track_UneVie");
    CHECK(decision.reason == SwapReason::Applied);
}

TEST_CASE("toggle global desligado vence o override")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_UneVie");
    overrides.set_enabled(false);

    const Swapper swapper{overrides, catalog};
    const auto decision = swapper.decide("Boss_Sirene", "Track_Lumiere");
    CHECK_FALSE(decision.swap);
    CHECK(decision.reason == SwapReason::GloballyDisabled);
}

TEST_CASE("override para a propria faixa original nao reinicia a musica")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_Lumiere");

    const Swapper swapper{overrides, catalog};
    const auto decision = swapper.decide("Boss_Sirene", "Track_Lumiere");
    CHECK_FALSE(decision.swap);
    CHECK(decision.reason == SwapReason::AlreadyTheTrack);
}

TEST_CASE("faixa inexistente no catalogo mantem a original em vez de causar silencio")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_QueNaoExiste");

    const Swapper swapper{overrides, catalog};
    const auto decision = swapper.decide("Boss_Sirene", "Track_Lumiere");
    CHECK_FALSE(decision.swap);
    CHECK(decision.reason == SwapReason::UnknownTrack);
}

TEST_CASE("sem catalogo carregado, a palavra do usuario vale")
{
    const Catalog empty_catalog;
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_QueOCatalogoNaoTem");

    const Swapper swapper{overrides, empty_catalog};
    const auto decision = swapper.decide("Boss_Sirene", "Track_Lumiere");
    CHECK(decision.swap);
    CHECK(decision.reason == SwapReason::Applied);
}

TEST_CASE("encontro fora do catalogo ainda pode receber override")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_DoPatchNovo", "Track_UneVie");

    const Swapper swapper{overrides, catalog};
    const auto decision = swapper.decide("Boss_DoPatchNovo", "Track_Lumiere");
    CHECK(decision.swap);
}

TEST_CASE("historico guarda motivo e nome legivel, e tem teto")
{
    const test::TempDir dir;
    const auto catalog = catalog_with_tracks(dir);
    OverrideTable overrides;
    overrides.set("Boss_Sirene", "Track_UneVie");
    Swapper swapper{overrides, catalog};

    swapper.decide_and_record("Boss_Sirene", "Track_Lumiere");
    REQUIRE(swapper.history().size() == 1);
    CHECK(swapper.history().front().encounter_name == "Sirène");
    CHECK(swapper.history().front().reason == SwapReason::Applied);
    CHECK(swapper.swaps_applied() == 1);

    swapper.decide_and_record("Boss_Outro", "Track_Lumiere");
    CHECK(swapper.history().front().encounter_id == "Boss_Outro"); // mais recente primeiro
    CHECK(swapper.swaps_applied() == 1);                           // nao trocou

    for (std::size_t i = 0; i < Swapper::kHistoryLimit + 10; ++i)
    {
        swapper.decide_and_record("Boss_Sirene", "Track_Lumiere");
    }
    CHECK(swapper.history().size() == Swapper::kHistoryLimit);

    swapper.clear_history();
    CHECK(swapper.history().empty());
    CHECK(swapper.swaps_applied() == 0);
}

TEST_CASE("todo motivo tem texto proprio, para nunca aparecer 'desconhecido' na UI")
{
    for (const auto reason : {SwapReason::GloballyDisabled, SwapReason::NoOverride,
                              SwapReason::UnknownTrack, SwapReason::AlreadyTheTrack,
                              SwapReason::Applied})
    {
        CHECK_FALSE(describe(reason).empty());
        CHECK(describe(reason) != "motivo desconhecido");
    }
}
