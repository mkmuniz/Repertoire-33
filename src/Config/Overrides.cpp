#include "Config/Overrides.hpp"

#include "Support/Json.hpp"
#include "Support/Log.hpp"

namespace e33
{
namespace
{
constexpr std::string_view kKeyEnabled = "enabled";
constexpr std::string_view kKeyOverrides = "overrides";
} // namespace

void OverrideTable::load(std::filesystem::path path)
{
    m_path = std::move(path);
    m_overrides.clear();
    m_enabled = true;
    m_dirty = false;

    const auto text = read_text_file(m_path);
    if (!text)
    {
        log::info("config nao encontrada em {}, comecando vazia (jogo vanilla)",
                  m_path.string());
        remember_mtime();
        return;
    }
    if (!apply_json(*text, /*replace=*/true))
    {
        log::warn("config invalida em {}, ignorando e comecando vazia", m_path.string());
    }
    remember_mtime();
}

bool OverrideTable::save()
{
    if (m_path.empty())
    {
        return false;
    }
    if (!write_text_file_atomic(m_path, to_json_string()))
    {
        log::warn("falha ao gravar {}", m_path.string());
        return false;
    }
    m_dirty = false;
    remember_mtime();
    log::debug("config gravada ({} overrides)", m_overrides.size());
    return true;
}

bool OverrideTable::reload_if_changed()
{
    if (m_path.empty())
    {
        return false;
    }
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(m_path, ec);
    if (ec)
    {
        return false;
    }
    if (m_mtime && *m_mtime == mtime)
    {
        return false;
    }
    return force_reload();
}

bool OverrideTable::force_reload()
{
    if (m_path.empty())
    {
        return false;
    }
    const auto text = read_text_file(m_path);
    if (!text)
    {
        return false;
    }
    if (!apply_json(*text, /*replace=*/true))
    {
        log::warn("reload ignorado: {} esta malformado, mantendo a tabela atual",
                  m_path.string());
        return false;
    }
    m_dirty = false;
    remember_mtime();
    log::info("config recarregada ({} overrides, enabled={})", m_overrides.size(), m_enabled);
    return true;
}

void OverrideTable::set_enabled(bool on)
{
    if (m_enabled == on)
    {
        return;
    }
    m_enabled = on;
    m_dirty = true;
}

std::optional<std::string> OverrideTable::track_for(std::string_view encounter_id) const
{
    if (!m_enabled)
    {
        return std::nullopt;
    }
    return raw_track_for(encounter_id);
}

std::optional<std::string> OverrideTable::raw_track_for(std::string_view encounter_id) const
{
    const auto it = m_overrides.find(std::string{encounter_id});
    if (it == m_overrides.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void OverrideTable::set(std::string_view encounter_id, std::string_view track_id)
{
    if (encounter_id.empty() || track_id.empty())
    {
        return;
    }
    auto& slot = m_overrides[std::string{encounter_id}];
    if (slot == track_id)
    {
        return;
    }
    slot = std::string{track_id};
    m_dirty = true;
}

void OverrideTable::clear(std::string_view encounter_id)
{
    if (m_overrides.erase(std::string{encounter_id}) > 0)
    {
        m_dirty = true;
    }
}

void OverrideTable::clear_all()
{
    if (m_overrides.empty())
    {
        return;
    }
    m_overrides.clear();
    m_dirty = true;
}

std::string OverrideTable::to_json_string() const
{
    Json root = Json::object();
    root[std::string{kKeyEnabled}] = m_enabled;
    // std::map garante ordem estável: o diff do config fica legível quando o
    // usuário versiona ou compartilha o arquivo.
    Json overrides = Json::object();
    for (const auto& [encounter, track] : m_overrides)
    {
        overrides[encounter] = track;
    }
    root[std::string{kKeyOverrides}] = std::move(overrides);
    return root.dump(2) + "\n";
}

bool OverrideTable::merge_from_json(std::string_view text)
{
    return apply_json(text, /*replace=*/false);
}

bool OverrideTable::replace_from_json(std::string_view text)
{
    return apply_json(text, /*replace=*/true);
}

bool OverrideTable::apply_json(std::string_view text, bool replace)
{
    const auto parsed = parse_json(text);
    if (!parsed || !parsed->is_object())
    {
        return false;
    }

    std::map<std::string, std::string> incoming;
    if (const auto it = parsed->find(kKeyOverrides); it != parsed->end())
    {
        if (!it->is_object())
        {
            return false;
        }
        for (const auto& [encounter, track] : it->items())
        {
            // Uma entrada torta não invalida o arquivo inteiro: pula a linha e
            // preserva o resto, senão um typo do usuário zera a config dele.
            if (!track.is_string() || encounter.empty())
            {
                log::warn("entrada ignorada em overrides: \"{}\" nao aponta para uma faixa",
                          encounter);
                continue;
            }
            const auto value = track.get<std::string>();
            if (value.empty())
            {
                continue;
            }
            incoming.emplace(encounter, value);
        }
    }

    if (replace)
    {
        m_overrides = std::move(incoming);
        m_enabled = true;
        if (const auto it = parsed->find(kKeyEnabled); it != parsed->end() && it->is_boolean())
        {
            m_enabled = it->get<bool>();
        }
    }
    else
    {
        for (auto& [encounter, track] : incoming)
        {
            m_overrides[encounter] = std::move(track);
        }
        m_dirty = true;
    }
    return true;
}

void OverrideTable::remember_mtime()
{
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(m_path, ec);
    m_mtime = ec ? std::nullopt : std::optional{mtime};
}
} // namespace e33
