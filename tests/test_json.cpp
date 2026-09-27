#include <doctest/doctest.h>

#include "Support/Json.hpp"
#include "TempDir.hpp"

using namespace e33;

TEST_CASE("read_text_file devolve nullopt em vez de lancar quando o arquivo nao existe")
{
    const test::TempDir dir;
    CHECK_FALSE(read_text_file(dir.file("nao-existe.json")).has_value());
}

TEST_CASE("escrita atomica cria diretorio pai e faz round trip")
{
    const test::TempDir dir;
    const auto path = dir.file("sub/dir/config.json");

    REQUIRE(write_text_file_atomic(path, "{\"a\":1}"));
    const auto text = read_text_file(path);
    REQUIRE(text.has_value());
    CHECK(*text == "{\"a\":1}");
}

TEST_CASE("escrita atomica sobrescreve arquivo existente e nao deixa .tmp para tras")
{
    const test::TempDir dir;
    const auto path = dir.file("config.json");

    REQUIRE(write_text_file_atomic(path, "primeiro"));
    REQUIRE(write_text_file_atomic(path, "segundo"));
    CHECK(*read_text_file(path) == "segundo");

    std::size_t leftovers = 0;
    for (const auto& entry : std::filesystem::directory_iterator{dir.path()})
    {
        if (entry.path().filename().string().find(".tmp-") != std::string::npos)
        {
            ++leftovers;
        }
    }
    CHECK(leftovers == 0);
}

TEST_CASE("parse_json devolve nullopt em JSON invalido")
{
    CHECK_FALSE(parse_json("{ nao e json").has_value());
    CHECK_FALSE(parse_json("").has_value());
}

TEST_CASE("parse_json aceita comentarios, porque o usuario vai editar o config a mao")
{
    const auto parsed = parse_json(R"({ // comentario
        "enabled": true
    })");
    REQUIRE(parsed.has_value());
    CHECK(parsed->at("enabled").get<bool>() == true);
}
