#include <doctest/doctest.h>

#include "FakeUnreal.hpp"
#include "Unreal/Properties.hpp"

using namespace e33;
using unreal::matches_member_name;
using unreal::ObjectArray;
using unreal::Objects;
using unreal::Properties;

namespace
{
// O mínimo de CoreUObject que validate_layout() consulta, mais uma classe de
// jogo com herança, como a do inimigo do E33.
struct World
{
    World()
    {
        package_class = fake.add("Package");
        core = fake.add("/Script/CoreUObject", package_class);
        class_class = fake.add("Class", 0, core);
        fake.object(package_class).put(0x10, class_class);
        struct_class = fake.add("Struct", class_class, core);
        fake.set_super(class_class, struct_class);
        script_struct = fake.add("ScriptStruct", class_class, core);
        vector = fake.add("Vector", script_struct, core);
        fake.add_field(vector, "X", "DoubleProperty", 0, 8);
        fake.add_field(vector, "Y", "DoubleProperty", 8, 8);
        fake.add_field(vector, "Z", "DoubleProperty", 16, 8);

        game = fake.add("/Game/Enemy", package_class);
        music_params = fake.add("S_MusicParameters", script_struct, game);
        fake.add_field(music_params, "Music_16_0332400D492AAEF7CE88BEBEA72A3D9B",
                       "SoftObjectProperty", 0, 0x28);
        base = fake.add("BP_Enemy_Base_C", class_class, game);
        fake.add_field(base, "SelectedEncounter", "StructProperty", 0x7D0, 0x10);
        fake.add_field(base, "BattleMusicParameters", "StructProperty", 0x830, 0x30, music_params);
        boss = fake.add("BP_Enemy_Simon_C", class_class, game);
        fake.set_super(boss, base);
    }

    Objects objects() const
    {
        return Objects{*ObjectArray::at(fake.array_address()),
                       *unreal::NamePool::at(fake.names.address())};
    }

    test::FakeObjectWorld fake{};
    std::uintptr_t package_class{}, core{}, class_class{}, struct_class{}, script_struct{};
    std::uintptr_t vector{}, game{}, music_params{}, base{}, boss{};
};
} // namespace

TEST_CASE("matches_member_name aceita o sufixo de UserDefinedStruct e nada parecido")
{
    CHECK(matches_member_name("Music", "Music"));
    CHECK(matches_member_name("Music_16_0332400D492AAEF7CE88BEBEA72A3D9B", "Music"));
    CHECK(matches_member_name("EncounterRow_25_76E166214BC95AA2A6A68DAD97A47C62", "EncounterRow"));

    CHECK_FALSE(matches_member_name("Music_FadeOut_12_16E96D27447BCE6393134092E0D1128F", "Music"));
    CHECK_FALSE(matches_member_name("MusicParameters_58_3EF84D0A4944D7C4EF7BFCB46E28D32F", "Music"));
    CHECK_FALSE(matches_member_name("Music_16_0332", "Music"));
    CHECK_FALSE(matches_member_name("Music_", "Music"));
    CHECK_FALSE(matches_member_name("Musi", "Music"));
}

TEST_CASE("find sobe pela hierarquia e devolve offset, tamanho e tipo")
{
    const World w;
    const auto objects = w.objects();
    const Properties properties{objects};

    const auto music = properties.find(w.boss, "BattleMusicParameters");
    REQUIRE(music.has_value());
    CHECK(music->offset == 0x830);
    CHECK(music->size == 0x30);
    CHECK(music->type == "StructProperty");

    const auto inner = Properties::struct_of(*music);
    CHECK(inner == w.music_params);
    const auto soft = properties.find(inner, "Music");
    REQUIRE(soft.has_value());
    CHECK(soft->offset == 0);
    CHECK(soft->type == "SoftObjectProperty");

    CHECK_FALSE(properties.find(w.boss, "Inexistente").has_value());
}

TEST_CASE("is_a reconhece subclasse e recusa o caminho inverso")
{
    const World w;
    CHECK(Properties::is_a(w.boss, w.base));
    CHECK(Properties::is_a(w.base, w.base));
    CHECK_FALSE(Properties::is_a(w.base, w.boss));
    CHECK_FALSE(Properties::is_a(0, w.base));
}

TEST_CASE("validate_layout confere os offsets contra CoreUObject")
{
    const World w;
    const auto objects = w.objects();
    CHECK(Properties{objects}.validate_layout().empty());
}

TEST_CASE("validate_layout aponta o campo que nao bate")
{
    World w;
    w.fake.set_super(w.class_class, 0);
    const auto objects = w.objects();
    const auto error = Properties{objects}.validate_layout();
    CHECK(error.find("SuperStruct") != std::string::npos);
}
