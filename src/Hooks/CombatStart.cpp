#include "Hooks/CombatStart.hpp"

#include "Support/Log.hpp"

namespace e33
{
void CombatWatcher::set_callback(CombatStartCallback callback)
{
    m_callback = std::move(callback);
}

void CombatWatcher::set_status(bool installed, std::string detail)
{
    m_installed = installed;
    m_detail = std::move(detail);
}

void CombatWatcher::notify(const CombatStartEvent& event) const
{
    if (!m_callback)
    {
        log::warn("inicio de combate sem callback registrado");
        return;
    }
    m_callback(event);
}

void CombatWatcher::simulate(const CombatStartEvent& event) const
{
    log::debug("simulando combate: encounter={} faixa_original={}", event.encounter_id,
               event.original_track_id);
    notify(event);
}
} // namespace e33
