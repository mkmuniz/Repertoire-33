#include <doctest/doctest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "FakeUnreal.hpp"
#include "Hooks/MusicPatcher.hpp"

using namespace e33;
using unreal::FName;

namespace
{
constexpr const char* kGoblu =
    "/Game/Audio/MetaSound/Music/Tracks/Goblu/MUS_Track_Goblu_BT_Boss.MUS_Track_Goblu_BT_Boss";
constexpr const char* kRenoir = "/Game/Audio/MetaSound/MUSIC/Tracks/Sea_Cliff/"
                                "MUS_Track_SeaCliff_BT_Renoir.MUS_Track_SeaCliff_BT_Renoir";

// O pedaço do E33 que o patcher lê, com os nomes de campo e offsets do SDK do
// Dumper-7 do jogo (UE 5.4.4). Campos de UserDefinedStruct levam o sufixo de
// GUID do editor, como no jogo.
struct E33World
{
    E33World()
    {
        package_class = fake.add("Package");
        core = fake.add("/Script/CoreUObject", package_class);
        class_class = fake.add("Class", 0, core);
        fake.object(package_class).put(0x10, class_class);
        script_struct = fake.add("ScriptStruct", class_class, core);

        engine = fake.add("/Script/Engine", package_class);
        row_handle = fake.add("DataTableRowHandle", script_struct, engine);
        fake.add_field(row_handle, "DataTable", "ObjectProperty", 0, 8);
        fake.add_field(row_handle, "RowName", "NameProperty", 8, 8);

        game = fake.add("/Game/Gameplay", package_class);
        music_params = fake.add("S_MusicParameters", script_struct, game);
        fake.add_field(music_params, "Music_16_0332400D492AAEF7CE88BEBEA72A3D9B",
                       "SoftObjectProperty", 0, 0x28);
        fake.add_field(music_params,
                       "ResumeExplorationMusicAfterBattle_15_68A8BF90470E6D62C6B3C4880831B126",
                       "BoolProperty", 0x28, 1);
        start_params = fake.add("FBattleStartParams", script_struct, game);
        fake.add_field(start_params, "EncounterRow_25_76E166214BC95AA2A6A68DAD97A47C62",
                       "StructProperty", 0, 0x10, row_handle);
        fake.add_field(start_params, "MusicParameters_58_3EF84D0A4944D7C4EF7BFCB46E28D32F",
                       "StructProperty", 0x68, 0x30, music_params);

        enemy_base = fake.add("BP_jRPG_Enemy_World_Base_Seamless_C", class_class, game);
        fake.add_field(enemy_base, "PossibleEncounters", "ArrayProperty", 0x7C0, 0x10);
        fake.add_field(enemy_base, "SelectedEncounter", "StructProperty", 0x7D0, 0x10, row_handle);
        fake.add_field(enemy_base, "BattleMusicParameters", "StructProperty", 0x830, 0x30,
                       music_params);
        goblu_class = fake.add("BP_EnemyWorld_Goblu_C", class_class, game);
        fake.set_super(goblu_class, enemy_base);

        action_class = fake.add("BP_GameAction_TriggerBattle_C", class_class, game);
        fake.add_field(action_class, "Battle Start Params", "StructProperty", 0x30, 0x158,
                       start_params);

        manager_class = fake.add("AC_jRPG_BattleManager_C", class_class, game);
        fake.add_field(manager_class, "EncounterName", "NameProperty", 0x190, 8);
        fake.add_field(manager_class, "CurrentBattleStartParams", "StructProperty", 0x4C8, 0x158,
                       start_params);
        fake.add_field(manager_class, "LoadedBattleMusic", "ObjectProperty", 0x9B8, 8);

        // Nomes que o asset registry teria registrado.
        goblu_track = path_names(kGoblu);
        renoir_track = path_names(kRenoir);
    }

    SoftPath path_names(std::string_view path)
    {
        const auto dot = path.rfind('.');
        return {FName{fake.names.add(path.substr(0, dot)), 0},
                FName{fake.names.add(path.substr(dot + 1)), 0}};
    }

    FName name(std::string_view text) { return FName{fake.names.add(text), 0}; }

    static void put_music(test::FakeObject& object, std::size_t at, const SoftPath& path)
    {
        object.put(at + 0x00, std::uint64_t{0x0000000100000007}); // ponteiro fraco resolvido
        object.put(at + 0x08, path.package);
        object.put(at + 0x10, path.asset);
    }

    static SoftPath music_at(const test::FakeObject& object, std::size_t at)
    {
        return {object.get<FName>(at + 0x08), object.get<FName>(at + 0x10)};
    }

    std::uintptr_t add_enemy(std::string_view encounter, const SoftPath& music)
    {
        const auto enemy = fake.add("BP_EnemyWorld_Goblu_C_0", goblu_class, game);
        fake.object(enemy).put(0x7D0 + 8, name(encounter));
        put_music(fake.object(enemy), 0x830, music);
        return enemy;
    }

    test::FakeObjectWorld fake{};
    std::uintptr_t package_class{}, core{}, class_class{}, script_struct{}, engine{}, game{};
    std::uintptr_t row_handle{}, music_params{}, start_params{};
    std::uintptr_t enemy_base{}, goblu_class{}, action_class{}, manager_class{};
    SoftPath goblu_track{}, renoir_track{};
};

// O pedaço do ModController que o patcher consulta: overrides por encontro.
struct Harness
{
    explicit Harness(E33World& world)
        : objects{*unreal::ObjectArray::at(world.fake.array_address()),
                  *unreal::NamePool::at(world.fake.names.address())}
        , patcher{objects,
                  [this](std::string_view encounter, std::string_view original) {
                      seen_originals[std::string{encounter}] = std::string{original};
                      const auto it = overrides.find(std::string{encounter});
                      return it == overrides.end() ? std::nullopt
                                                   : std::optional<std::string>{it->second};
                  },
                  [this](const CombatStartEvent& event) { battles.push_back(event); }}
    {
    }

    void run() { patcher.tick(1 << 20); }

    unreal::Objects objects;
    MusicPatcher patcher;
    std::map<std::string, std::string> overrides{};
    std::map<std::string, std::string> seen_originals{};
    std::vector<CombatStartEvent> battles{};
};
} // namespace

TEST_CASE("sem override o jogo fica intocado")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};

    h.run();

    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
    CHECK(w.fake.object(enemy).get<std::uint64_t>(0x830) == 0x0000000100000007);
    CHECK(h.seen_originals["GO_Goblu"] == kGoblu);
    CHECK(h.patcher.stats().enemies == 1);
    CHECK(h.patcher.stats().patched == 0);
}

TEST_CASE("override troca o caminho da musica do inimigo e solta o objeto resolvido")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;

    h.run();

    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.renoir_track);
    // Ponteiro fraco zerado: o jogo resolve de novo pelo caminho novo.
    CHECK(w.fake.object(enemy).get<std::uint64_t>(0x830) == 0);
    CHECK(h.patcher.stats().patched == 1);
}

TEST_CASE("caminho do catalogo com outra caixa resolve para o mesmo nome do jogo")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.renoir_track);
    Harness h{w};
    // Catalogo diz MUSIC, o pool registrou Music.
    h.overrides["GO_Goblu"] = "/Game/Audio/MetaSound/MUSIC/Tracks/Goblu/"
                              "MUS_Track_Goblu_BT_Boss.MUS_Track_Goblu_BT_Boss";
    h.run();
    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
}

TEST_CASE("tirar o override devolve a faixa original")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;
    h.run();
    REQUIRE(E33World::music_at(w.fake.object(enemy), 0x830) == w.renoir_track);

    h.overrides.clear();
    h.run();

    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
    CHECK(h.patcher.stats().patched == 0);
    // A original continua sendo a do jogo, nao a que o mod escreveu.
    CHECK(h.seen_originals["GO_Goblu"] == kGoblu);
}

TEST_CASE("objeto recarregado pelo jogo vira a nova original")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;
    h.run();

    // O nivel recarrega o ator por cima da troca.
    E33World::put_music(w.fake.object(enemy), 0x830, w.goblu_track);
    h.run();

    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.renoir_track);
    h.overrides.clear();
    h.run();
    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
}

TEST_CASE("inimigo sem encontro sorteado usa o unico possivel, e nao chuta entre varios")
{
    E33World w;
    const auto lone = w.fake.add("BP_EnemyWorld_Goblu_C_1", w.goblu_class, w.game);
    E33World::put_music(w.fake.object(lone), 0x830, w.goblu_track);
    std::vector<std::uint8_t> one(0x10, 0);
    const auto row = w.name("GO_Goblu");
    std::memcpy(one.data() + 8, &row, sizeof(row));
    w.fake.object(lone).put(0x7C0, reinterpret_cast<std::uintptr_t>(one.data()));
    w.fake.object(lone).put(0x7C8, std::int32_t{1});

    const auto crowd = w.fake.add("BP_EnemyWorld_Goblu_C_2", w.goblu_class, w.game);
    E33World::put_music(w.fake.object(crowd), 0x830, w.goblu_track);
    std::vector<std::uint8_t> two(0x20, 0);
    std::memcpy(two.data() + 8, &row, sizeof(row));
    std::memcpy(two.data() + 0x18, &row, sizeof(row));
    w.fake.object(crowd).put(0x7C0, reinterpret_cast<std::uintptr_t>(two.data()));
    w.fake.object(crowd).put(0x7C8, std::int32_t{2});

    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;
    h.run();

    CHECK(E33World::music_at(w.fake.object(lone), 0x830) == w.renoir_track);
    CHECK(E33World::music_at(w.fake.object(crowd), 0x830) == w.goblu_track);
}

TEST_CASE("acao de roteiro de luta de historia tambem e trocada")
{
    E33World w;
    const auto action = w.fake.add("BP_GameAction_TriggerBattle_C_0", w.action_class, w.game);
    w.fake.object(action).put(0x30 + 8, w.name("SI_Axon_Sirene"));
    E33World::put_music(w.fake.object(action), 0x30 + 0x68, w.goblu_track);
    Harness h{w};
    h.overrides["SI_Axon_Sirene"] = kRenoir;

    h.run();

    CHECK(E33World::music_at(w.fake.object(action), 0x98) == w.renoir_track);
    CHECK(h.patcher.stats().actions == 1);
}

TEST_CASE("CDO e arquetipo nunca sao alterados")
{
    E33World w;
    const auto cdo = w.add_enemy("GO_Goblu", w.goblu_track);
    w.fake.object(cdo).put(0x08, std::int32_t{0x10}); // RF_ClassDefaultObject
    const auto archetype = w.add_enemy("GO_Goblu", w.goblu_track);
    w.fake.object(archetype).put(0x08, std::int32_t{0x20}); // RF_ArchetypeObject
    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;

    h.run();

    CHECK(E33World::music_at(w.fake.object(cdo), 0x830) == w.goblu_track);
    CHECK(E33World::music_at(w.fake.object(archetype), 0x830) == w.goblu_track);
    CHECK(h.patcher.stats().enemies == 0);
}

TEST_CASE("faixa que nao existe no jogo deixa a original e e reportada")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    h.overrides["GO_Goblu"] = "/Game/Audio/Nada/MUS_Inexistente.MUS_Inexistente";

    h.run();

    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
    REQUIRE(h.patcher.missing_tracks().size() == 1);
    CHECK(h.patcher.missing_tracks().front() == "/Game/Audio/Nada/MUS_Inexistente.MUS_Inexistente");
}

TEST_CASE("objeto que morreu sai da lista e nao e mais escrito")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    h.run();
    REQUIRE(h.patcher.stats().enemies == 1);

    w.fake.kill(enemy);
    h.overrides["GO_Goblu"] = kRenoir;
    h.run();

    CHECK(h.patcher.stats().enemies == 0);
    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);
}

TEST_CASE("borda None -> encontro no battle manager e o inicio do combate")
{
    E33World w;
    w.add_enemy("GO_Goblu", w.goblu_track);
    const auto manager = w.fake.add("AC_jRPG_BattleManager_C_0", w.manager_class, w.game);
    Harness h{w};
    h.run();
    CHECK(h.battles.empty());

    // A luta comeca: o jogo copia os parametros e preenche EncounterName.
    auto& bm = w.fake.object(manager);
    bm.put(0x4C8 + 8, w.name("GO_Goblu"));
    E33World::put_music(bm, 0x4C8 + 0x68, w.goblu_track);
    bm.put(0x190, w.name("GO_Goblu"));
    h.run();

    REQUIRE(h.battles.size() == 1);
    CHECK(h.battles.front().encounter_id == "GO_Goblu");
    CHECK(h.battles.front().original_track_id == kGoblu);

    // Mesma luta, outro tick: nada novo. Fim (None) e nova luta: outro evento.
    h.run();
    CHECK(h.battles.size() == 1);
    bm.put(0x190, FName{});
    h.run();
    bm.put(0x190, w.name("GO_Goblu"));
    h.run();
    CHECK(h.battles.size() == 2);
}

TEST_CASE("troca tardia grava a faixa nos parametros da batalha em curso")
{
    E33World w;
    const auto manager = w.fake.add("AC_jRPG_BattleManager_C_0", w.manager_class, w.game);
    Harness h{w};
    h.run();
    CHECK_FALSE(h.patcher.apply_to_current_battle(kRenoir)); // nenhuma batalha ainda

    auto& bm = w.fake.object(manager);
    bm.put(0x4C8 + 8, w.name("WM_Sprong"));
    E33World::put_music(bm, 0x4C8 + 0x68, w.goblu_track);
    bm.put(0x190, w.name("WM_Sprong"));
    h.run();
    REQUIRE(h.battles.size() == 1);
    // A original vem dos proprios parametros: nenhuma fonte conhecida tinha esse encontro.
    CHECK(h.battles.front().original_track_id == kGoblu);

    CHECK(h.patcher.apply_to_current_battle(kRenoir));
    CHECK(E33World::music_at(bm, 0x4C8 + 0x68) == w.renoir_track);
    CHECK(bm.get<std::uint64_t>(0x4C8 + 0x68) == 0);
    CHECK(h.patcher.apply_to_current_battle(kRenoir)); // ja aponta: nada a fazer
}

TEST_CASE("classe com campo de tipo inesperado e ignorada em vez de escrita as cegas")
{
    E33World w;
    const auto odd = w.fake.add("BP_jRPG_Enemy_World_Base_Seamless_C", w.class_class, w.core);
    // Mesmo nome de classe, mas o campo de musica nao e um soft pointer.
    w.fake.add_field(odd, "BattleMusicParameters", "IntProperty", 0x830, 4);
    const auto enemy = w.fake.add("Impostor_0", odd, w.game);
    w.fake.object(enemy).put(0x830 + 8, w.goblu_track.package);
    Harness h{w};
    h.overrides["GO_Goblu"] = kRenoir;

    h.run();

    CHECK(h.patcher.stats().enemies == 0);
}

TEST_CASE("faixa dada como ausente volta a valer quando o jogo a usa em algum objeto")
{
    E33World w;
    const auto enemy = w.add_enemy("GO_Goblu", w.goblu_track);
    Harness h{w};
    constexpr const char* kLater = "/Game/Audio/MetaSound/Music/Tracks/Clea_Workshop/"
                                   "MUS_Track_CleaWorkshop_BT_Clea.MUS_Track_CleaWorkshop_BT_Clea";
    h.overrides["GO_Goblu"] = kLater;
    h.run();
    REQUIRE(h.patcher.missing_tracks().size() == 1);
    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == w.goblu_track);

    // Um nivel carrega um inimigo que usa essa faixa: agora ela existe.
    const auto clea = w.path_names(kLater);
    w.add_enemy("CFH_Boss_Clea", clea);
    // A fonte que revela a faixa pode ser visitada depois da que a pede.
    h.run();
    h.run();

    CHECK(h.patcher.missing_tracks().empty());
    CHECK(E33World::music_at(w.fake.object(enemy), 0x830) == clea);
}

TEST_CASE("o id do combate e o EncounterName, mesmo com parametros da luta anterior")
{
    E33World w;
    const auto manager = w.fake.add("AC_jRPG_BattleManager_C_0", w.manager_class, w.game);
    Harness h{w};
    h.run();

    auto& bm = w.fake.object(manager);
    bm.put(0x4C8 + 8, w.name("WM_Sprong")); // sobra da luta anterior
    bm.put(0x190, w.name("CFH_Boss_Clea"));
    h.run();

    REQUIRE(h.battles.size() == 1);
    CHECK(h.battles.front().encounter_id == "CFH_Boss_Clea");
}
