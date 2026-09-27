#include "Config/Overrides.hpp"

namespace e33
{
void OverrideTable::load_or_create(const std::filesystem::path& path)
{
    m_path = path;
    // TODO(M3): parse do JSON + hot reload. Ausente/ilegivel => tabela vazia,
    // nunca um erro fatal: o jogo tem que continuar vanilla.
}

void OverrideTable::save() const
{
    // TODO(M3)
}

void OverrideTable::reload()
{
    load_or_create(m_path);
}

std::optional<std::wstring> OverrideTable::track_for(const std::wstring& encounter_id) const
{
    if (!m_enabled)
    {
        return std::nullopt;
    }
    if (const auto it = m_overrides.find(encounter_id); it != m_overrides.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void OverrideTable::set(const std::wstring& encounter_id, const std::wstring& track_id)
{
    m_overrides[encounter_id] = track_id;
}

void OverrideTable::clear(const std::wstring& encounter_id)
{
    m_overrides.erase(encounter_id);
}
} // namespace e33
