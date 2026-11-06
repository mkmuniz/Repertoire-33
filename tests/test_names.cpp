#include <doctest/doctest.h>

#include <array>

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

TEST_CASE("find_entry acha o indice pelo texto, sem criar nome")
{
    test::FakeNamePool fake;
    fake.add("MUS_Battle_A");
    const auto package = fake.add("/Game/Audio/Music/MUS_Boss_Simon");
    const auto asset = fake.add("MUS_Boss_Simon");
    fake.add_wide(u"MUS_Boss_Simon"); // wide com o mesmo texto nao e o mesmo nome

    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    CHECK(pool->find_entry("/Game/Audio/Music/MUS_Boss_Simon") == package);
    CHECK(pool->find_entry("MUS_Boss_Simon") == asset);
    CHECK(pool->find_entry("None") == 0u);
    CHECK_FALSE(pool->find_entry("MUS_Boss_Sim").has_value());
    CHECK_FALSE(pool->find_entry("Inexistente").has_value());
}

TEST_CASE("find_entry ignora maiusculas, como o Unreal compara FName")
{
    test::FakeNamePool fake;
    const auto package = fake.add("/Game/Audio/MetaSound/Music/Tracks/Goblu/MUS_Track_Goblu_BT_Boss");
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    CHECK(pool->find_entry("/Game/Audio/MetaSound/MUSIC/Tracks/Goblu/MUS_Track_Goblu_BT_Boss")
          == package);
}

TEST_CASE("find_entries resolve varios textos numa varredura so")
{
    test::FakeNamePool fake;
    const auto a = fake.add("MUS_Track_A");
    const auto b = fake.add("MUS_Track_Bb");
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());

    const std::array<std::string_view, 4> texts{"mus_track_bb", "Inexistente", "MUS_TRACK_A", ""};
    const auto found = pool->find_entries(texts);
    REQUIRE(found.size() == 4);
    CHECK(found[0] == b);
    CHECK_FALSE(found[1].has_value());
    CHECK(found[2] == a);
    CHECK_FALSE(found[3].has_value());
}

TEST_CASE("split_name_number segue o ParseNumber do UE5")
{
    using unreal::split_name_number;
    const auto phase = split_name_number("MUS_Track_Simon_BT_WeLost_Phase2_3");
    CHECK(phase.base == "MUS_Track_Simon_BT_WeLost_Phase2");
    CHECK(phase.number == 4);

    CHECK(split_name_number("Rocket_0").number == 1);
    CHECK(split_name_number("Rocket_0").base == "Rocket");
    CHECK(split_name_number("Rocket_04").number == 0);     // zero a esquerda nao divide
    CHECK(split_name_number("Rocket_04").base == "Rocket_04");
    CHECK(split_name_number("Phase2").number == 0);        // sem '_' antes dos digitos
    CHECK(split_name_number("_5").base == "");
    CHECK(split_name_number("123").number == 0);           // so digitos
    CHECK(split_name_number("A_99999999999").number == 0); // nao cabe em int32
    CHECK(split_name_number("").number == 0);
}

TEST_CASE("find_name monta o FName que o Unreal guardaria para o texto")
{
    test::FakeNamePool fake;
    const auto base = fake.add("MUS_Track_Simon_BT_WeLost_Phase2");
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());

    const auto name = pool->find_name("MUS_Track_Simon_BT_WeLost_Phase2_3");
    REQUIRE(name.has_value());
    CHECK(*name == FName{base, 4});
    CHECK(pool->to_string(*name) == "MUS_Track_Simon_BT_WeLost_Phase2_3");
    CHECK_FALSE(pool->find_name("MUS_Track_Inexistente").has_value());
}

TEST_CASE("find_entry para no cursor do bloco atual")
{
    test::FakeNamePool fake;
    const auto pool = NamePool::at(fake.address());
    REQUIRE(pool.has_value());
    // Nada alem de None/ByteProperty: o resto do bloco nao e lido.
    CHECK_FALSE(pool->find_entry("").has_value());
    CHECK(pool->find_entry("ByteProperty") == 3u);
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
