#include "Core/Swapper.hpp"

#include "Support/Log.hpp"

namespace e33
{
std::string_view describe(SwapReason reason)
{
    switch (reason)
    {
    case SwapReason::GloballyDisabled:
        return "overrides desativados no toggle global";
    case SwapReason::NoOverride:
        return "sem override para este encontro (faixa original)";
    case SwapReason::UnknownTrack:
        return "a faixa do override nao existe no catalogo";
    case SwapReason::AlreadyTheTrack:
        return "o override aponta para a propria faixa original";
    case SwapReason::Applied:
        return "override aplicado";
    }
    return "motivo desconhecido";
}

SwapDecision Swapper::decide(std::string_view encounter_id,
                             std::string_view original_track_id) const
{
    if (!m_overrides.enabled())
    {
        return {.swap = false, .track_id = {}, .reason = SwapReason::GloballyDisabled};
    }

    const auto target = m_overrides.track_for(encounter_id);
    if (!target)
    {
        return {.swap = false, .track_id = {}, .reason = SwapReason::NoOverride};
    }

    if (*target == original_track_id)
    {
        // Disparar a mesma faixa reiniciaria a música do zero no meio da
        // transição; não fazer nada soa melhor e é o que o usuário espera.
        return {.swap = false, .track_id = *target, .reason = SwapReason::AlreadyTheTrack};
    }

    // Catálogo carregado e sem essa faixa: trocar levaria a silêncio, que é
    // pior que vanilla. Catálogo vazio (não carregado) significa que não temos
    // base para julgar, e aí a palavra do usuário vale.
    if (!m_catalog.tracks().empty() && !m_catalog.knows_track(*target))
    {
        return {.swap = false, .track_id = *target, .reason = SwapReason::UnknownTrack};
    }

    return {.swap = true, .track_id = *target, .reason = SwapReason::Applied};
}

SwapDecision Swapper::decide_and_record(std::string_view encounter_id,
                                        std::string_view original_track_id)
{
    auto decision = decide(encounter_id, original_track_id);

    m_history.push_front(SwapLogEntry{
        .encounter_id = std::string{encounter_id},
        .encounter_name = std::string{m_catalog.boss_name(encounter_id)},
        .original_track = std::string{original_track_id},
        .chosen_track = decision.track_id,
        .reason = decision.reason,
    });
    while (m_history.size() > kHistoryLimit)
    {
        m_history.pop_back();
    }

    if (decision.swap)
    {
        ++m_swaps_applied;
        log::info("{}: {} -> {}", m_catalog.boss_name(encounter_id), original_track_id,
                  decision.track_id);
    }
    else
    {
        log::debug("{}: sem troca ({})", m_catalog.boss_name(encounter_id),
                   describe(decision.reason));
    }

    if (decision.reason == SwapReason::UnknownTrack)
    {
        log::warn("faixa \"{}\" configurada para \"{}\" nao existe no catalogo; "
                  "mantendo a original",
                  decision.track_id, encounter_id);
    }
    return decision;
}

void Swapper::clear_history()
{
    m_history.clear();
    m_swaps_applied = 0;
}
} // namespace e33
