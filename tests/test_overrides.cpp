#include <doctest/doctest.h>

#include "Config/Overrides.hpp"
#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

TEST_CASE("config ausente resulta em tabela vazia, nao em erro")
{
    const test::TempDir dir;
    OverrideTable table;
    table.load(dir.file("config.json"));

    CHECK(table.size() == 0);
    CHECK(table.enabled());
    CHECK_FALSE(table.track_for("Boss_Simon").has_value());
}

TEST_CASE("config malformada nao apaga nem inventa override")
{
    const test::TempDir dir;
    const auto path = dir.file("config.json");
    REQUIRE(write_text_file_atomic(path, "{ isso nao e json"));

    OverrideTable table;
    table.load(path);
    CHECK(table.size() == 0);
    CHECK_FALSE(table.track_for("Boss_Simon").has_value());
}

TEST_CASE("round trip de save e load preserva overrides e o toggle global")
{
    const test::TempDir dir;
    const auto path = dir.file("config.json");

    OverrideTable written;
    written.load(path);
    written.set("Boss_Simon", "Track_Renoir");
    written.set("Boss_Sirene", "Track_UneVieATaimer");
    written.set_enabled(false);
    REQUIRE(written.save());

    OverrideTable read;
    read.load(path);
    CHECK(read.size() == 2);
    CHECK_FALSE(read.enabled());
    CHECK(read.raw_track_for("Boss_Simon") == "Track_Renoir");
}

TEST_CASE("toggle global suprime os overrides sem apagar nenhum")
{
    OverrideTable table;
    table.set("Boss_Simon", "Track_Renoir");

    CHECK(table.track_for("Boss_Simon") == "Track_Renoir");

    table.set_enabled(false);
    CHECK_FALSE(table.track_for("Boss_Simon").has_value());
    CHECK(table.raw_track_for("Boss_Simon") == "Track_Renoir");
    CHECK(table.size() == 1);

    table.set_enabled(true);
    CHECK(table.track_for("Boss_Simon") == "Track_Renoir");
}

TEST_CASE("voltar ao original remove apenas a linha pedida")
{
    OverrideTable table;
    table.set("Boss_Simon", "Track_Renoir");
    table.set("Boss_Sirene", "Track_Lumiere");

    table.clear("Boss_Simon");
    CHECK_FALSE(table.track_for("Boss_Simon").has_value());
    CHECK(table.track_for("Boss_Sirene") == "Track_Lumiere");

    table.clear_all();
    CHECK(table.size() == 0);
}

TEST_CASE("dirty marca so mudanca real, para nao gravar disco a cada frame")
{
    OverrideTable table;
    CHECK_FALSE(table.dirty());

    table.set("Boss_Simon", "Track_Renoir");
    CHECK(table.dirty());

    const test::TempDir dir;
    table.load(dir.file("config.json"));
    CHECK_FALSE(table.dirty());

    table.set("Boss_Simon", "Track_Renoir");
    CHECK(table.dirty());
    REQUIRE(table.save());
    CHECK_FALSE(table.dirty());

    table.set("Boss_Simon", "Track_Renoir"); // mesmo valor
    CHECK_FALSE(table.dirty());
    table.set_enabled(true); // valor que ja era o atual
    CHECK_FALSE(table.dirty());
}

TEST_CASE("set ignora id ou faixa vazios em vez de gravar lixo")
{
    OverrideTable table;
    table.set("", "Track_Renoir");
    table.set("Boss_Simon", "");
    CHECK(table.size() == 0);
}

TEST_CASE("uma entrada torta e pulada sem derrubar o resto do arquivo")
{
    OverrideTable table;
    REQUIRE(table.replace_from_json(R"({
        "enabled": true,
        "overrides": {
            "Boss_Simon": "Track_Renoir",
            "Boss_Quebrado": 42,
            "Boss_Sirene": "Track_Lumiere"
        }
    })"));
    CHECK(table.size() == 2);
    CHECK(table.track_for("Boss_Simon") == "Track_Renoir");
    CHECK(table.track_for("Boss_Sirene") == "Track_Lumiere");
}

TEST_CASE("preset importado por merge preserva o que nao foi mencionado")
{
    OverrideTable table;
    table.set("Boss_Simon", "Track_Renoir");
    table.set("Boss_Goblu", "Track_Alicia");

    REQUIRE(table.merge_from_json(R"({"overrides": {"Boss_Simon": "Track_Lumiere"}})"));
    CHECK(table.track_for("Boss_Simon") == "Track_Lumiere"); // sobrescrito
    CHECK(table.track_for("Boss_Goblu") == "Track_Alicia");  // preservado
}

TEST_CASE("preset importado por replace descarta o estado anterior")
{
    OverrideTable table;
    table.set("Boss_Goblu", "Track_Alicia");

    REQUIRE(table.replace_from_json(R"({"overrides": {"Boss_Simon": "Track_Lumiere"}})"));
    CHECK(table.size() == 1);
    CHECK_FALSE(table.track_for("Boss_Goblu").has_value());
}

TEST_CASE("hot reload so reage a mudanca de mtime")
{
    const test::TempDir dir;
    const auto path = dir.file("config.json");

    OverrideTable table;
    table.load(path);
    table.set("Boss_Simon", "Track_Renoir");
    REQUIRE(table.save());

    CHECK_FALSE(table.reload_if_changed());

    // Edicao "por fora", como o usuario faria no editor de texto.
    REQUIRE(write_text_file_atomic(path, R"({"enabled": true, "overrides": {"Boss_Simon": "Track_Lumiere"}})"));
    CHECK(table.reload_if_changed());
    CHECK(table.track_for("Boss_Simon") == "Track_Lumiere");
}

TEST_CASE("reload de arquivo malformado mantem a tabela que ja funcionava")
{
    const test::TempDir dir;
    const auto path = dir.file("config.json");

    OverrideTable table;
    table.load(path);
    table.set("Boss_Simon", "Track_Renoir");
    REQUIRE(table.save());

    REQUIRE(write_text_file_atomic(path, "{ quebrado"));
    CHECK_FALSE(table.force_reload());
    CHECK(table.track_for("Boss_Simon") == "Track_Renoir");
}

TEST_CASE("json gerado e estavel e ordenado, para diff legivel")
{
    OverrideTable table;
    table.set("Boss_Sirene", "Track_Lumiere");
    table.set("Boss_Goblu", "Track_Alicia");

    const auto text = table.to_json_string();
    CHECK(text.find("Boss_Goblu") < text.find("Boss_Sirene"));
    CHECK(text.back() == '\n');
}
