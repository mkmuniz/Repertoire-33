#include <doctest/doctest.h>

#include "FakeUnreal.hpp"
#include "Unreal/Objects.hpp"

using namespace e33;
using unreal::ObjectArray;
using unreal::Objects;

namespace
{
// Um mundo mínimo com a hierarquia que todo Unreal tem no começo da tabela.
struct World
{
    World()
    {
        package_class = fake.add("Package");
        core = fake.add("/Script/CoreUObject", package_class);
        class_class = fake.add("Class", 0, core);
        fake.object(package_class).put(0x10, class_class);
        fake.object(package_class).put(0x20, core);
        function_class = fake.add("Function", class_class, core);

        ak = fake.add("/Script/AkAudio", package_class);
        statics = fake.add("AkGameplayStatics", class_class, ak);
        post_event = fake.add("PostEvent", function_class, statics);
        component = fake.add("AkComponent", class_class, ak);
        component_post = fake.add("PostEvent", function_class, component);
    }

    test::FakeObjectWorld fake{};
    std::uintptr_t package_class{}, core{}, class_class{}, function_class{};
    std::uintptr_t ak{}, statics{}, post_event{}, component{}, component_post{};
};
} // namespace

TEST_CASE("ObjectArray mede o tamanho do item pelo indice interno de cada objeto")
{
    for (const std::size_t size : {std::size_t{0x18}, std::size_t{0x10}, std::size_t{0x20}})
    {
        test::FakeObjectWorld fake{size};
        for (int i = 0; i < 8; ++i)
        {
            fake.add("Obj");
        }
        const auto array = ObjectArray::at(fake.array_address());
        REQUIRE(array.has_value());
        CHECK(array->item_size() == size);
        CHECK(array->count() == 8);
    }
}

TEST_CASE("ObjectArray recusa contadores incoerentes")
{
    test::FakeObjectWorld fake;
    fake.add("Obj");
    auto* raw = reinterpret_cast<std::int32_t*>(fake.array_address());
    raw[7] = 5; // NumChunks nao bate com NumElements
    CHECK_FALSE(ObjectArray::at(fake.array_address()).has_value());
}

TEST_CASE("ObjectArray devolve 0 fora dos limites em vez de ler alem da tabela")
{
    test::FakeObjectWorld fake;
    fake.add("Obj");
    const auto array = ObjectArray::at(fake.array_address());
    REQUIRE(array.has_value());
    CHECK(array->object(-1) == 0);
    CHECK(array->object(1) == 0);
    CHECK(array->object(0) != 0);
}

TEST_CASE("Objects monta nome, classe e caminho como o Dumper-7")
{
    World w;
    const auto array = ObjectArray::at(w.fake.array_address());
    const auto names = unreal::NamePool::at(w.fake.names.address());
    REQUIRE(array.has_value());
    REQUIRE(names.has_value());
    const Objects objects{*array, *names};

    CHECK(objects.name(w.post_event) == "PostEvent");
    CHECK(objects.class_name(w.post_event) == "Function");
    CHECK(objects.path(w.post_event) == "/Script/AkAudio.AkGameplayStatics.PostEvent");
    CHECK(objects.full_name(w.post_event)
          == "Function /Script/AkAudio.AkGameplayStatics.PostEvent");
    CHECK(objects.full_name(w.core) == "Package /Script/CoreUObject");
}

TEST_CASE("find distingue funcoes de mesmo nome pelo caminho completo")
{
    World w;
    const Objects objects{*ObjectArray::at(w.fake.array_address()),
                          *unreal::NamePool::at(w.fake.names.address())};

    CHECK(objects.find("Function /Script/AkAudio.AkGameplayStatics.PostEvent") == w.post_event);
    CHECK(objects.find("Function /Script/AkAudio.AkComponent.PostEvent") == w.component_post);
    CHECK(objects.find("Package /Script/CoreUObject") == w.core);
    CHECK(objects.find("Function /Script/AkAudio.AkComponent.Nada") == 0);
    CHECK(objects.find("sem-espaco") == 0);
}
