#include <doctest/doctest.h>

#include "Data/Catalog.hpp"
#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

namespace
{
Catalog make_catalog(const test::TempDir& dir)
{
    REQUIRE(write_text_file_atomic(dir.file("bosses.json"), R"({"bosses": [
        {"id": "Boss_Sirene", "name": "Sirène", "group": "Act I"},
        {"id": "Boss_Goblu", "name": "Goblu", "group": "Optional"},
        {"id": "Boss_SemNome"}
    ]})"));
    REQUIRE(write_text_file_atomic(dir.file("tracks.json"), R"({"tracks": [
        {"id": "Track_UneVie", "name": "Une Vie à T'aimer", "source": "Act III"},
        {"id": "Track_Lumiere", "name": "Lumière", "source": "Act I"},
        {"id": "Track_Externa", "name": "Minha faixa", "dynamic": false}
    ]})"));

    Catalog catalog;
    REQUIRE(catalog.load_bosses(dir.file("bosses.json")));
    REQUIRE(catalog.load_tracks(dir.file("tracks.json")));
    return catalog;
}
} // namespace

TEST_CASE("fold ignora acento, caixa e pontuacao")
{
    CHECK(fold_for_search("Sirène") == "sirene");
    CHECK(fold_for_search("Une Vie à T'aimer") == "unevieataimer");
    CHECK(fold_for_search("Évêque") == "eveque");
    CHECK(fold_for_search("Château_Noir") == "chateaunoir");
}

TEST_CASE("digitar sem acento acha o nome acentuado")
{
    CHECK(matches_query("Sirène", "sirene"));
    CHECK(matches_query("Sirène", "SIRÈNE"));
    CHECK(matches_query("Une Vie à T'aimer", "une vie"));
    CHECK_FALSE(matches_query("Sirène", "goblu"));
}

TEST_CASE("termos podem vir em qualquer ordem")
{
    CHECK(matches_query("Une Vie à T'aimer", "aimer vie"));
    CHECK(matches_query("Une Vie à T'aimer", "  vie   une  "));
}

TEST_CASE("busca vazia lista tudo, em vez de nada")
{
    CHECK(matches_query("qualquer coisa", ""));
    CHECK(matches_query("qualquer coisa", "   "));
}

TEST_CASE("catalogo carrega e resolve nome legivel")
{
    const test::TempDir dir;
    const auto catalog = make_catalog(dir);

    CHECK(catalog.bosses().size() == 3);
    CHECK(catalog.tracks().size() == 3);
    CHECK(catalog.boss_name("Boss_Sirene") == "Sirène");
    CHECK(catalog.track_name("Track_Lumiere") == "Lumière");
}

TEST_CASE("entrada sem nome cai no proprio id, nao em string vazia")
{
    const test::TempDir dir;
    const auto catalog = make_catalog(dir);
    CHECK(catalog.boss_name("Boss_SemNome") == "Boss_SemNome");
}

TEST_CASE("id que o catalogo nao conhece aparece cru, sem quebrar a UI")
{
    const test::TempDir dir;
    const auto catalog = make_catalog(dir);
    CHECK(catalog.boss_name("Boss_DoPatchNovo") == "Boss_DoPatchNovo");
    CHECK_FALSE(catalog.knows_boss("Boss_DoPatchNovo"));
}

TEST_CASE("encontro desconhecido e registrado uma vez para a UI listar")
{
    const test::TempDir dir;
    auto catalog = make_catalog(dir);

    CHECK(catalog.note_seen_encounter("Boss_DoPatchNovo"));
    CHECK_FALSE(catalog.note_seen_encounter("Boss_DoPatchNovo")); // ja registrado
    CHECK_FALSE(catalog.note_seen_encounter("Boss_Sirene"));      // ja no catalogo
    CHECK_FALSE(catalog.note_seen_encounter(""));
    CHECK(catalog.unknown_encounters().size() == 1);
}

TEST_CASE("busca cobre nome, id e grupo")
{
    const test::TempDir dir;
    const auto catalog = make_catalog(dir);

    CHECK(catalog.find_bosses("sirene").size() == 1);
    CHECK(catalog.find_bosses("optional").size() == 1);
    CHECK(catalog.find_bosses("boss_").size() == 3);
    CHECK(catalog.find_tracks("act").size() == 2);
    CHECK(catalog.find_tracks("nao existe").empty());
}

TEST_CASE("faixa sem dynamic explicito conta como nativa")
{
    const test::TempDir dir;
    const auto catalog = make_catalog(dir);

    const auto tracks = catalog.find_tracks("Track_Lumiere");
    REQUIRE(tracks.size() == 1);
    CHECK(tracks.front()->dynamic);

    const auto external = catalog.find_tracks("Track_Externa");
    REQUIRE(external.size() == 1);
    CHECK_FALSE(external.front()->dynamic);
}

TEST_CASE("id de faixa e comparado sem caixa: MUSIC e Music sao o mesmo asset")
{
    CHECK(same_track_id("/Game/Audio/MUSIC/A.A", "/game/audio/Music/a.a"));
    CHECK_FALSE(same_track_id("/Game/A.A", "/Game/A.B"));
    CHECK_FALSE(same_track_id("/Game/A", "/Game/A.A"));

    const test::TempDir dir;
    const auto catalog = make_catalog(dir);
    CHECK(catalog.knows_track("TRACK_LUMIERE"));
    CHECK(catalog.track_name("track_lumiere") == "Lumière");
}

TEST_CASE("o catalogo distribuido em data/ carrega inteiro e sem id repetido")
{
    // Roda a partir da raiz do projeto (rundir do target tests).
    Catalog catalog;
    REQUIRE(catalog.load_bosses("data/bosses.json"));
    REQUIRE(catalog.load_tracks("data/tracks.json"));
    CHECK(catalog.bosses().size() > 100);
    CHECK(catalog.tracks().size() > 80);

    for (std::size_t i = 0; i < catalog.bosses().size(); ++i)
    {
        for (std::size_t j = i + 1; j < catalog.bosses().size(); ++j)
        {
            CHECK_MESSAGE(catalog.bosses()[i].id != catalog.bosses()[j].id,
                          catalog.bosses()[i].id);
        }
    }
    for (std::size_t i = 0; i < catalog.tracks().size(); ++i)
    {
        const auto& id = catalog.tracks()[i].id;
        // Caminho completo de objeto: pacote + "." + asset, que é o que o
        // soft pointer do jogo guarda.
        CHECK_MESSAGE(id.starts_with("/Game/"), id);
        CHECK_MESSAGE(id.find('.') != std::string::npos, id);
        for (std::size_t j = i + 1; j < catalog.tracks().size(); ++j)
        {
            CHECK_MESSAGE(!same_track_id(id, catalog.tracks()[j].id), id);
        }
    }
}

TEST_CASE("arquivo ausente nao e fatal: o mod segue com ids cruos")
{
    const test::TempDir dir;
    Catalog catalog;
    CHECK_FALSE(catalog.load_bosses(dir.file("nao-existe.json")));
    CHECK(catalog.bosses().empty());
    CHECK(catalog.boss_name("Boss_X") == "Boss_X");
}
