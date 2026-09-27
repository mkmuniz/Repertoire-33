#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "Config/Overrides.hpp"
#include "Config/Settings.hpp"
#include "Core/AudioBackend.hpp"
#include "Core/Swapper.hpp"
#include "Data/Catalog.hpp"
#include "Hooks/CombatStart.hpp"

namespace e33
{
// Estado do mod e as transições entre eles. A UI lê daqui e chama daqui; não
// existe caminho da UI para o jogo que não passe por esta classe.
//
// Recebe o backend de áudio por injeção: em jogo é o UnrealAudioBackend, nos
// testes e no harness nativo é o RecordingAudioBackend. É o que permite testar
// o pipeline inteiro — evento, decisão, troca — sem abrir o jogo.
class ModController
{
public:
    explicit ModController(std::unique_ptr<IAudioBackend> audio);

    // `mod_dir` é a pasta do mod: config.json, settings.json e data/ saem dela.
    void initialize(const std::filesystem::path& mod_dir);

    // Chamar a cada frame. Faz o trabalho barato: hot reload periódico e
    // gravação preguiçosa do que a UI mudou.
    void tick(double now_seconds);

    // ---- estado, para a UI ----
    [[nodiscard]] OverrideTable& overrides() { return m_overrides; }
    [[nodiscard]] const OverrideTable& overrides() const { return m_overrides; }
    [[nodiscard]] Settings& settings() { return m_settings; }
    [[nodiscard]] const Catalog& catalog() const { return m_catalog; }
    [[nodiscard]] const Swapper& swapper() const { return m_swapper; }
    [[nodiscard]] CombatWatcher& watcher() { return m_watcher; }
    [[nodiscard]] IAudioBackend& audio() { return *m_audio; }
    [[nodiscard]] const std::string& status_line() const { return m_status; }
    [[nodiscard]] const std::filesystem::path& mod_dir() const { return m_mod_dir; }

    // ---- ações da UI ----
    void set_override(std::string_view encounter_id, std::string_view track_id);
    void clear_override(std::string_view encounter_id);
    void clear_all_overrides();
    void set_enabled(bool on);
    void set_verbose(bool on);
    bool save_now();
    bool reload_now();

    bool preview(std::string_view track_id);
    void stop_preview();

    bool export_preset(const std::filesystem::path& path) const;
    bool import_preset(const std::filesystem::path& path, bool replace);

    static constexpr double kAutoSaveDelaySeconds = 1.0;
    static constexpr double kReloadPollSeconds = 2.0;

private:
    void on_combat_start(const CombatStartEvent& event);

    std::unique_ptr<IAudioBackend> m_audio;
    OverrideTable m_overrides{};
    Settings m_settings{};
    Catalog m_catalog{};
    Swapper m_swapper;
    CombatWatcher m_watcher{};

    std::filesystem::path m_mod_dir{};
    std::string m_status{};
    double m_dirty_since{-1.0};
    double m_last_reload_poll{0.0};
};
} // namespace e33
