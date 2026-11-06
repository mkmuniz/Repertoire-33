#include <doctest/doctest.h>

#include <array>
#include <cstdint>

#include "Unreal/Pattern.hpp"

using namespace e33::unreal;

TEST_CASE("parse_pattern aceita bytes hex e os dois formatos de curinga")
{
    const auto pattern = parse_pattern("48 8B ? ?? 05");
    REQUIRE(pattern.has_value());
    CHECK(pattern->bytes.size() == 5);
    CHECK(pattern->bytes[0] == 0x48);
    CHECK(pattern->bytes[1] == 0x8B);
    CHECK_FALSE(pattern->exact[2]);
    CHECK_FALSE(pattern->exact[3]);
    CHECK(pattern->exact[4]);
}

TEST_CASE("parse_pattern rejeita lixo em vez de virar uma assinatura que casa com tudo")
{
    CHECK_FALSE(parse_pattern("").has_value());
    CHECK_FALSE(parse_pattern("   ").has_value());
    CHECK_FALSE(parse_pattern("48 ZZ").has_value());
    CHECK_FALSE(parse_pattern("488B").has_value());
    CHECK_FALSE(parse_pattern("4").has_value());
}

TEST_CASE("find_pattern respeita curingas e o ponto de partida")
{
    const std::array<std::uint8_t, 8> data{0x00, 0x48, 0x8B, 0x11, 0x48, 0x8B, 0x22, 0x90};
    const auto pattern = parse_pattern("48 8B ??");
    REQUIRE(pattern.has_value());

    CHECK(find_pattern(data, *pattern) == 1);
    CHECK(find_pattern(data, *pattern, 2) == 4);
    CHECK_FALSE(find_pattern(data, *pattern, 6).has_value());
}

TEST_CASE("find_pattern nao le alem do fim do buffer")
{
    const std::array<std::uint8_t, 3> data{0x48, 0x8B, 0x05};
    const auto pattern = parse_pattern("8B 05 00");
    REQUIRE(pattern.has_value());
    CHECK_FALSE(find_pattern(data, *pattern).has_value());
}

TEST_CASE("resolve_rip soma o deslocamento ao endereco da proxima instrucao")
{
    // mov rax, [rip+0x10] — 48 8B 05 <disp32>, 7 bytes.
    std::array<std::uint8_t, 7> code{0x48, 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00};
    const auto base = reinterpret_cast<std::uintptr_t>(code.data());
    CHECK(resolve_rip(base, 3, 7) == base + 7 + 0x10);

    // Deslocamento negativo: o alvo fica antes da instrução.
    code[3] = 0xF0;
    code[4] = 0xFF;
    code[5] = 0xFF;
    code[6] = 0xFF;
    CHECK(resolve_rip(base, 3, 7) == base + 7 - 0x10);
}
