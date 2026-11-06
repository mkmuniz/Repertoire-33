#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Hooks/CombatStart.hpp"
#include "Unreal/Objects.hpp"
#include "Unreal/Properties.hpp"

namespace e33
{
// Endereço de um asset como o TSoftObjectPtr guarda (FTopLevelAssetPath).
struct SoftPath
{
    unreal::FName package{};
    unreal::FName asset{};

    [[nodiscard]] bool empty() const { return package.comparison_index == 0; }
    friend bool operator==(const SoftPath&, const SoftPath&) = default;
};

// A troca de música do M2, feita nos DADOS que o jogo usa para escolher a
// faixa da batalha — sem interceptar chamada.
//
// StartBattleNEW e o resto do fluxo de batalha são Blueprint chamando
// Blueprint, o que não passa por ProcessEvent; interceptar isso exigiria
// assinatura de bytes do VM de script, que muda a cada patch. A faixa de cada
// batalha, porém, é um dado: um TSoftObjectPtr<USoundBase> dentro de
// S_MusicParameters, guardado
//   - no inimigo do mapa: BP_jRPG_Enemy_World_Base_Seamless_C.BattleMusicParameters,
//     com o encontro em SelectedEncounter (ou PossibleEncounters, se só houver um);
//   - na ação de roteiro das lutas de história:
//     BP_GameAction_TriggerBattle_C."Battle Start Params" (com espaços);
// e copiado para AC_jRPG_BattleManager_C.CurrentBattleStartParams quando a luta
// começa. Reescrever o caminho do soft pointer antes disso faz o próprio jogo
// carregar e tocar a faixa escolhida pelo pipeline normal — com a dinâmica de
// intensidade intacta, que era o requisito do M2.
//
// Nada aqui sabe de Windows: lê e escreve memória através de Unreal/, então os
// testes montam esse layout num buffer e conferem a troca sem o jogo. Chamar
// sempre da game thread: é ela que lê esses campos.
class MusicPatcher
{
public:
    // Faixa que deve tocar no encontro, ou nullopt para deixar a original.
    using DesiredTrack =
        std::function<std::optional<std::string>(std::string_view encounter_id,
                                                 std::string_view original_track)>;
    using BattleStarted = std::function<void(const CombatStartEvent&)>;

    struct Stats
    {
        std::size_t enemies{0};
        std::size_t actions{0};
        std::size_t managers{0};
        std::size_t patched{0}; // fontes apontando hoje para um override
        std::size_t full_passes{0};
    };

    MusicPatcher(const unreal::Objects& objects, DesiredTrack desired, BattleStarted started);

    // Varre `budget` objetos da tabela (a varredura completa leva várias
    // chamadas), reaplica a decisão em todas as fontes conhecidas e detecta o
    // início de batalha. Barato o bastante para rodar ~10 vezes por segundo.
    void tick(std::size_t budget = kSliceSize);

    // Troca tardia: grava `track_id` nos parâmetros da batalha que acabou de
    // começar. É a rede de segurança para lutas disparadas por uma fonte que a
    // varredura não cobre (diálogo, torre): o jogo ainda pré-carrega o mapa de
    // batalha antes de pedir a música. true se a batalha já aponta para a faixa.
    bool apply_to_current_battle(std::string_view track_id);

    // Resolve de uma vez, numa varredura só do pool de nomes, os caminhos que a
    // UI pode pedir — para o primeiro override não custar uma varredura no
    // meio do jogo.
    void prefetch(const std::vector<std::string>& track_ids);

    [[nodiscard]] Stats stats() const;
    // Faixas pedidas que não existem no pool de nomes do jogo (caminho errado).
    [[nodiscard]] const std::vector<std::string>& missing_tracks() const { return m_missing; }
    // Caminho do USoundBase que o battle manager carregou por último.
    [[nodiscard]] const std::string& last_loaded_track() const { return m_last_loaded; }

    static constexpr std::size_t kSliceSize = 32 * 1024;

private:
    enum class Kind : std::uint8_t
    {
        None,
        Enemy,
        Action,
        Manager,
    };

    // Offsets resolvidos por nome, uma vez por classe. -1 = ausente.
    struct ClassInfo
    {
        unreal::FName name{};
        Kind kind{Kind::None};
        std::int32_t music{-1};          // soft pointer da faixa
        std::int32_t encounter_row{-1};  // RowName do encontro (ação, manager)
        std::int32_t selected_row{-1};   // inimigo: SelectedEncounter.RowName
        std::int32_t possible{-1};       // inimigo: TArray<FDataTableRowHandle>
        std::int32_t handle_size{0};     // tamanho de um FDataTableRowHandle
        std::int32_t row_in_handle{0};   // offset de RowName dentro dele
        std::int32_t encounter_name{-1}; // manager: EncounterName, None fora de luta
        std::int32_t loaded_music{-1};   // manager: LoadedBattleMusic
    };

    struct Source
    {
        std::uintptr_t object{0};
        std::int32_t index{0};
        std::uintptr_t klass{0};
        unreal::FName class_name{};
        const ClassInfo* info{nullptr};

        // Fontes de música: o valor do jogo e o que o mod escreveu por cima.
        std::optional<SoftPath> original{};
        std::optional<SoftPath> written{};

        // Manager: estado anterior, para detectar as bordas.
        unreal::FName last_encounter{};
        std::uintptr_t last_loaded{0};
    };

    const ClassInfo& class_info(std::uintptr_t klass);
    ClassInfo describe(std::uintptr_t klass, unreal::FName name) const;
    [[nodiscard]] bool alive(const Source& source) const;

    void scan(std::size_t budget);
    void update_music(Source& source);
    void watch_battle(Source& source, std::vector<CombatStartEvent>& started);
    [[nodiscard]] std::string encounter_of(const Source& source) const;

    [[nodiscard]] std::optional<SoftPath> read_soft(std::uintptr_t at) const;
    static bool write_soft(std::uintptr_t at, const SoftPath& path);
    [[nodiscard]] std::string path_text(const SoftPath& path) const;
    std::optional<SoftPath> resolve(std::string_view track_id);

    const unreal::Objects& m_objects;
    unreal::Properties m_properties;
    DesiredTrack m_desired;
    BattleStarted m_started;

    std::unordered_map<std::uintptr_t, ClassInfo> m_classes{};
    std::unordered_map<std::uintptr_t, Source> m_sources{};
    std::int32_t m_cursor{0};
    std::size_t m_full_passes{0};

    std::unordered_map<std::string, std::optional<SoftPath>> m_resolved{}; // chave sem caixa
    std::unordered_map<std::string, std::string> m_original_by_encounter{};
    std::vector<std::string> m_missing{};
    std::uintptr_t m_current_battle{0};
    std::string m_last_loaded{};
};
} // namespace e33
