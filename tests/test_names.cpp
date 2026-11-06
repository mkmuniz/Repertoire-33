#include <doctest/doctest.h>

#include "FakeUnreal.hpp"
#include "Unreal/Names.hpp"

using namespace e33;
using unreal::FName;
using unreal::NamePool;

TEST_CASE("NamePool so aceita um pool cuja entrada 0 e None")
{
    const test::FakeNamePool fake;
    CHECK(NamePool::at(fake.address()).has_value());
    CHECK_FALSE(NamePool::at(0).has_value());

    std::vector<std::uint8_t> garbage(0x100, 0);
    CHECK_FALSE(NamePool::at(reinterpret_cast<std::uintptr_t>(garbage.data())).has_value());
}

TEST_CASE("NamePool le entradas narrow pelo indice do Unreal")
{
    test::FakeNamePool fake;
    const auto simon = fake.add("Boss_Simon");
    const auto odd = fake.add("Abc"); // tamanho impar: a proxima entrada e alinhada
    const auto after = fake.add("Play_Music");

    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    CHECK(pool->entry(0) == "None");
    CHECK(pool->entry(3) == "ByteProperty");
    CHECK(pool->entry(simon) == "Boss_Simon");
    CHECK(pool->entry(odd) == "Abc");
    CHECK(pool->entry(after) == "Play_Music");
}

TEST_CASE("NamePool converte nomes wide e Latin-1 para UTF-8")
{
    test::FakeNamePool fake;
    const auto wide = fake.add_wide(u"Sirène");
    const auto latin1 = fake.add("Sir\xe8ne");

    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    CHECK(pool->entry(wide) == "Sir\xc3\xa8ne");
    CHECK(pool->entry(latin1) == "Sir\xc3\xa8ne");
}

TEST_CASE("to_string aplica o sufixo de instancia como o Unreal: number - 1")
{
    test::FakeNamePool fake;
    const auto index = fake.add("AkComponent");
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());

    CHECK(pool->to_string(FName{index, 0}) == "AkComponent");
    CHECK(pool->to_string(FName{index, 1}) == "AkComponent_0");
    CHECK(pool->to_string(FName{index, 3}) == "AkComponent_2");
}

TEST_CASE("indice fora do pool nao le memoria alheia")
{
    const test::FakeNamePool fake;
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    // Bloco 2 nao foi alocado.
    CHECK_FALSE(pool->entry(2u << 16).has_value());
    CHECK(pool->to_string(FName{2u << 16, 0}).starts_with("<FName"));
}
