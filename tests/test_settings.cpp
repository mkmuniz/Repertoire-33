#include <doctest/doctest.h>

#include "Config/Settings.hpp"
#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

TEST_CASE("padroes: F9 e escala 1.0, como o README promete")
{
    const Settings s;
    CHECK(s.hotkey == "F9");
    CHECK(s.font_scale == doctest::Approx(1.0f));
    CHECK_FALSE(s.verbose_log);
    CHECK(s.hotkey_virtual_key() == 0x78);
}

TEST_CASE("nome de tecla aceita variacao de caixa e separador")
{
    CHECK(virtual_key_from_name("f8") == 0x77);
    CHECK(virtual_key_from_name("Page Up") == 0x21);
    CHECK(virtual_key_from_name("page_down") == 0x22);
    CHECK(virtual_key_from_name("K") == 'K');
    CHECK(virtual_key_from_name("7") == '7');
    CHECK_FALSE(virtual_key_from_name("Ctrl+Shift+Q").has_value());
    CHECK_FALSE(virtual_key_from_name("").has_value());
}

TEST_CASE("hotkey invalida no arquivo e rejeitada, e o padrao continua valendo")
{
    Settings s;
    REQUIRE(s.apply_json(R"({"hotkey": "NaoExiste"})"));
    CHECK(s.hotkey == "F9");
    CHECK(s.hotkey_virtual_key().has_value());
}

TEST_CASE("escala de fonte e limitada a faixa utilizavel")
{
    Settings s;
    REQUIRE(s.apply_json(R"({"font_scale": 99.0})"));
    CHECK(s.font_scale == doctest::Approx(Settings::kMaxFontScale));

    REQUIRE(s.apply_json(R"({"font_scale": -3.0})"));
    CHECK(s.font_scale == doctest::Approx(Settings::kMinFontScale));
}

TEST_CASE("settings de uma versao anterior continua carregando")
{
    Settings s;
    REQUIRE(s.apply_json(R"({"hotkey": "F7"})"));
    CHECK(s.hotkey == "F7");
    CHECK(s.font_scale == doctest::Approx(1.0f)); // campo ausente usa o padrao
    CHECK_FALSE(s.start_open);
}

TEST_CASE("round trip em disco preserva os campos")
{
    const test::TempDir dir;
    const auto path = dir.file("settings.json");

    Settings written;
    written.load(path);
    written.hotkey = "F11";
    written.font_scale = 1.75f;
    written.verbose_log = true;
    REQUIRE(written.save());

    Settings read;
    read.load(path);
    CHECK(read.hotkey == "F11");
    CHECK(read.font_scale == doctest::Approx(1.75f));
    CHECK(read.verbose_log);
}

TEST_CASE("settings malformado cai nos padroes sem perder o caminho do arquivo")
{
    const test::TempDir dir;
    const auto path = dir.file("settings.json");
    REQUIRE(write_text_file_atomic(path, "{ quebrado"));

    Settings s;
    s.load(path);
    CHECK(s.hotkey == "F9");
    CHECK(s.path() == path);
}
