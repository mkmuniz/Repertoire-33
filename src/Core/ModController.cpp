#include "Core/ModController.hpp"

#include <format>
#include <utility>

#include "Support/Json.hpp"
#include "Support/Log.hpp"

namespace e33
{
ModController::ModController(std::unique_ptr<IAudioBackend> audio)
    : m_audio{std::move(audio)}
    , m_swapper{m_overrides, m_catalog}
{
}

void ModController::initialize(const std::filesystem::path& mod_dir)
{
    m_mod_dir = mod_dir;

    // Settings primeiro: é ele que liga o log verboso, e sem isso a carga do
    // resto acontece em silêncio justamente quando você quer ver o que houve.
    m_settings.load(mod_dir / "settings.json");
    log::set_verbose(m_settings.verbose_log);

    m_catalog.load_bosses(mod_dir / "data" / "bosses.json");
    m_catalog.load_tracks(mod_dir / "data" / "tracks.json");
    m_overrides.load(mod_dir / "config.json");

    m_watcher.set_callback([this](const CombatStartEvent& event) { on_combat_start(event); });
    m_watcher.install();

    m_status = std::format("{} override(s), {} encontro(s) e {} faixa(s) no catalogo",
                           m_overrides.size(), m_catalog.bosses().size(),
                           m_catalog.tracks().size());
    log::info("{}", m_status);
}

void ModController::tick(double now_seconds)
{
    // Gravação preguiçosa: a UI mexe no estado a cada clique, e gravar no disco
    // por clique é I/O dentro do frame do jogo.
    if (m_overrides.dirty())
    {
        if (m_dirty_since < 0.0)
        {
            m_dirty_since = now_seconds;
        }
        else if (now_seconds - m_dirty_since >= kAutoSaveDelaySeconds)
        {
            save_now();
        }
    }
    else
    {
        m_dirty_since = -1.0;
    }

    // Hot reload: só verifica mtime de vez em quando, não a cada frame.
    if (now_seconds - m_last_reload_poll >= kReloadPollSeconds)
    {
        m_last_reload_poll = now_seconds;
        if (!m_overrides.dirty() && m_overrides.reload_if_changed())
        {
            m_status = std::format("config recarregada do disco: {} override(s)",
                                   m_overrides.size());
        }
    }
}

void ModController::set_override(std::string_view encounter_id, std::string_view track_id)
{
    m_overrides.set(encounter_id, track_id);
}

void ModController::clear_override(std::string_view encounter_id)
{
    m_overrides.clear(encounter_id);
}

void ModController::clear_all_overrides()
{
    m_overrides.clear_all();
}

void ModController::set_enabled(bool on)
{
    m_overrides.set_enabled(on);
    m_status = on ? "overrides ativos" : "overrides desativados (jogo vanilla)";
}

void ModController::set_verbose(bool on)
{
    m_settings.verbose_log = on;
    log::set_verbose(on);
    static_cast<void>(m_settings.save());
}

bool ModController::save_now()
{
    const bool ok = m_overrides.save();
    m_dirty_since = -1.0;
    m_status = ok ? std::format("config gravada ({} override(s))", m_overrides.size())
                  : "falha ao gravar config.json";
    return ok;
}

bool ModController::reload_now()
{
    const bool ok = m_overrides.force_reload();
    m_status = ok ? std::format("config recarregada ({} override(s))", m_overrides.size())
                  : "nada para recarregar (arquivo ausente ou malformado)";
    return ok;
}

bool ModController::preview(std::string_view track_id)
{
    if (track_id.empty())
    {
        return false;
    }
    return m_audio->preview(track_id);
}

void ModController::stop_preview()
{
    m_audio->stop_preview();
}

bool ModController::export_preset(const std::filesystem::path& path) const
{
    if (!write_text_file_atomic(path, m_overrides.to_json_string()))
    {
        return false;
    }
    log::info("preset exportado para {}", path.string());
    return true;
}

bool ModController::import_preset(const std::filesystem::path& path, bool replace)
{
    const auto text = read_text_file(path);
    if (!text)
    {
        m_status = std::format("preset nao encontrado: {}", path.string());
        return false;
    }
    const bool ok = replace ? m_overrides.replace_from_json(*text)
                            : m_overrides.merge_from_json(*text);
    m_status = ok ? std::format("preset importado: {} override(s)", m_overrides.size())
                  : "preset invalido, nada foi alterado";
    return ok;
}

void ModController::on_combat_start(const CombatStartEvent& event)
{
    m_catalog.note_seen_encounter(event.encounter_id);

    const auto decision = m_swapper.decide_and_record(event.encounter_id,
                                                      event.original_track_id);
    if (!decision.swap)
    {
        return;
    }
    if (!m_audio->play_instead(decision.track_id))
    {
        m_status = std::format("a troca para \"{}\" falhou; a faixa original continua",
                               decision.track_id);
        log::warn("{}", m_status);
    }
}
} // namespace e33
