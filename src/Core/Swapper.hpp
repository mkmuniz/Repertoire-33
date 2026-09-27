#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>

#include "Config/Overrides.hpp"
#include "Data/Catalog.hpp"

namespace e33
{
// Por que a troca aconteceu ou não. Existe para a linha de log e para o painel
// de diagnóstico: "não funcionou no boss X" tem de ter uma resposta exata.
enum class SwapReason
{
    GloballyDisabled,   // toggle global desligado
    NoOverride,         // sem entrada para o encontro: caminho vanilla
    UnknownTrack,       // override aponta para faixa que o catálogo não tem
    AlreadyTheTrack,    // override é a própria faixa original
    Applied,
};

[[nodiscard]] std::string_view describe(SwapReason reason);

struct SwapDecision
{
    bool swap{false};
    std::string track_id{};
    SwapReason reason{SwapReason::NoOverride};
};

struct SwapLogEntry
{
    std::string encounter_id{};
    std::string encounter_name{};
    std::string original_track{};
    std::string chosen_track{};
    SwapReason reason{SwapReason::NoOverride};
};

// Decide a troca. Pura: não conhece Unreal, não conhece ImGui, não toca disco.
// É aqui que mora a garantia central do mod — nenhum caminho devolve swap=true
// sem uma entrada explícita que o usuário criou.
class Swapper
{
public:
    Swapper(const OverrideTable& overrides, const Catalog& catalog)
        : m_overrides{overrides}
        , m_catalog{catalog}
    {
    }

    [[nodiscard]] SwapDecision decide(std::string_view encounter_id,
                                      std::string_view original_track_id) const;

    // Mesma decisão, mais o registro no histórico e o log verboso. É o que o
    // hook chama.
    SwapDecision decide_and_record(std::string_view encounter_id,
                                   std::string_view original_track_id);

    [[nodiscard]] const std::deque<SwapLogEntry>& history() const { return m_history; }
    [[nodiscard]] std::size_t swaps_applied() const { return m_swaps_applied; }
    void clear_history();

    static constexpr std::size_t kHistoryLimit = 32;

private:
    const OverrideTable& m_overrides;
    const Catalog& m_catalog;
    std::deque<SwapLogEntry> m_history{};
    std::size_t m_swaps_applied{0};
};
} // namespace e33
