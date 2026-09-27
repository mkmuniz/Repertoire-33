#include <doctest/doctest.h>

#include <memory>

#include "Core/ModController.hpp"
#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

namespace
{
// Monta uma pasta de mod como a que o usuario instala, com catalogo preenchido.
void write_mod_dir(const test::TempDir& dir, std::string_view config = {})
{
    REQUIRE(write_text_file_atomic(dir.file("data/bosses.json"), R"({"bosses": [
        {"id": "Boss_Sirene", "name": "Sirène", "group": "Act I"},
        {"id": "Boss_Goblu", "name": "Goblu", "group": "Optional"}
    ]})"));
    REQUIRE(write_text_file_atomic(dir.file("data/tracks.json"), R"({"tracks": [
        {"id": "Track_Lumiere", "name": "Lumière"},
        {"id": "Track_UneVie", "name": "Une Vie à T'aimer"}
    ]})"));
    if (!config.empty())
    {
        REQUIRE(write_text_file_atomic(dir.file("config.json"), config));
    }
}

struct Harness
{
    explicit Harness(const test::TempDir& dir)
        : backend{new RecordingAudioBackend{}}
        , controller{std::unique_ptr<IAudioBackend>{backend}}
    {
        controller.initialize(dir.path());
    }

    RecordingAudioBackend* backend; // propriedade do controller
    ModController controller;
};
} // namespace

TEST_CASE("instalacao limpa nao troca nada no primeiro combate")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    h.controller.watcher().simulate({"Boss_Sirene", "Track_Lumiere"});

    CHECK(h.backend->played().empty());
    REQUIRE(h.controller.swapper().history().size() == 1);
    CHECK(h.controller.swapper().history().front().reason == SwapReason::NoOverride);
}

TEST_CASE("pipeline completo: override configurado troca a faixa no combate")
{
    const test::TempDir dir;
    write_mod_dir(dir, R"({"enabled": true, "overrides": {"Boss_Sirene": "Track_UneVie"}})");
    Harness h{dir};

    h.controller.watcher().simulate({"Boss_Sirene", "Track_Lumiere"});

    REQUIRE(h.backend->played().size() == 1);
    CHECK(h.backend->played().front() == "Track_UneVie");
    CHECK(h.controller.swapper().swaps_applied() == 1);
}

TEST_CASE("override criado pela UI vale no combate seguinte, sem reiniciar o jogo")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    h.controller.watcher().simulate({"Boss_Goblu", "Track_Lumiere"});
    CHECK(h.backend->played().empty());

    h.controller.set_override("Boss_Goblu", "Track_UneVie");
    h.controller.watcher().simulate({"Boss_Goblu", "Track_Lumiere"});
    REQUIRE(h.backend->played().size() == 1);
    CHECK(h.backend->played().front() == "Track_UneVie");
}

TEST_CASE("toggle global desliga tudo sem perder a configuracao")
{
    const test::TempDir dir;
    write_mod_dir(dir, R"({"enabled": true, "overrides": {"Boss_Sirene": "Track_UneVie"}})");
    Harness h{dir};

    h.controller.set_enabled(false);
    h.controller.watcher().simulate({"Boss_Sirene", "Track_Lumiere"});
    CHECK(h.backend->played().empty());

    h.controller.set_enabled(true);
    h.controller.watcher().simulate({"Boss_Sirene", "Track_Lumiere"});
    CHECK(h.backend->played().size() == 1);
    CHECK(h.controller.overrides().size() == 1);
}

TEST_CASE("encontro fora do catalogo entra na lista de vistos em jogo")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    h.controller.watcher().simulate({"Boss_DoPatchNovo", "Track_Lumiere"});
    REQUIRE(h.controller.catalog().unknown_encounters().size() == 1);
    CHECK(h.controller.catalog().unknown_encounters().front() == "Boss_DoPatchNovo");
}

TEST_CASE("autosave grava depois do atraso, nao a cada clique")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    h.controller.set_override("Boss_Sirene", "Track_UneVie");
    h.controller.tick(0.0);
    CHECK_FALSE(std::filesystem::exists(dir.file("config.json")));

    h.controller.tick(0.5);
    CHECK_FALSE(std::filesystem::exists(dir.file("config.json")));

    h.controller.tick(ModController::kAutoSaveDelaySeconds + 0.1);
    REQUIRE(std::filesystem::exists(dir.file("config.json")));
    CHECK_FALSE(h.controller.overrides().dirty());
}

TEST_CASE("hot reload pega edicao externa no proximo poll")
{
    const test::TempDir dir;
    write_mod_dir(dir, R"({"enabled": true, "overrides": {}})");
    Harness h{dir};
    CHECK(h.controller.overrides().size() == 0);

    REQUIRE(write_text_file_atomic(dir.file("config.json"),
                                   R"({"enabled": true, "overrides": {"Boss_Goblu": "Track_UneVie"}})"));

    h.controller.tick(0.1); // antes do poll
    h.controller.tick(ModController::kReloadPollSeconds + 0.1);
    CHECK(h.controller.overrides().size() == 1);

    h.controller.watcher().simulate({"Boss_Goblu", "Track_Lumiere"});
    CHECK(h.backend->played().size() == 1);
}

TEST_CASE("reload nao descarta mudanca da UI ainda nao gravada")
{
    const test::TempDir dir;
    write_mod_dir(dir, R"({"enabled": true, "overrides": {}})");
    Harness h{dir};

    h.controller.set_override("Boss_Sirene", "Track_UneVie"); // dirty
    REQUIRE(write_text_file_atomic(dir.file("config.json"),
                                   R"({"enabled": true, "overrides": {"Boss_Goblu": "Track_Lumiere"}})"));

    h.controller.tick(ModController::kReloadPollSeconds + 0.1);
    CHECK(h.controller.overrides().raw_track_for("Boss_Sirene") == "Track_UneVie");
}

TEST_CASE("preview toca fora de combate e para quando pedido")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    CHECK(h.controller.preview("Track_UneVie"));
    CHECK(h.backend->previewing() == "Track_UneVie");
    CHECK_FALSE(h.controller.preview(""));

    h.controller.stop_preview();
    CHECK(h.backend->previewing().empty());
    CHECK(h.backend->played().empty()); // preview nao conta como troca
}

TEST_CASE("preset exportado e reimportado reproduz a configuracao")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};

    h.controller.set_override("Boss_Sirene", "Track_UneVie");
    h.controller.set_override("Boss_Goblu", "Track_Lumiere");
    REQUIRE(h.controller.export_preset(dir.file("preset.json")));

    h.controller.clear_all_overrides();
    CHECK(h.controller.overrides().size() == 0);

    REQUIRE(h.controller.import_preset(dir.file("preset.json"), /*replace=*/true));
    CHECK(h.controller.overrides().size() == 2);
    CHECK(h.controller.overrides().raw_track_for("Boss_Sirene") == "Track_UneVie");
}

TEST_CASE("preset ausente nao altera o estado e explica na status line")
{
    const test::TempDir dir;
    write_mod_dir(dir);
    Harness h{dir};
    h.controller.set_override("Boss_Sirene", "Track_UneVie");

    CHECK_FALSE(h.controller.import_preset(dir.file("nao-existe.json"), true));
    CHECK(h.controller.overrides().size() == 1);
    CHECK_FALSE(h.controller.status_line().empty());
}

TEST_CASE("falha do backend nao fica silenciosa")
{
    struct FailingBackend final : IAudioBackend
    {
        bool play_instead(std::string_view) override { return false; }
        bool preview(std::string_view) override { return false; }
        void stop_preview() override {}
    };

    const test::TempDir dir;
    write_mod_dir(dir, R"({"enabled": true, "overrides": {"Boss_Sirene": "Track_UneVie"}})");

    ModController controller{std::make_unique<FailingBackend>()};
    controller.initialize(dir.path());
    controller.watcher().simulate({"Boss_Sirene", "Track_Lumiere"});

    CHECK(controller.status_line().find("falhou") != std::string::npos);
}
