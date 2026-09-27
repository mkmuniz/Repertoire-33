#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace e33
{
// Tabela encontro -> faixa, persistida em config.json.
//
// Nasce VAZIA e é isso que mantém o jogo vanilla: sem entrada para um
// encontro, o hook não faz nada e a faixa original toca. Nenhum caminho aqui
// inventa um override.
//
// Ids são UTF-8 (std::string). A conversão para o wide string do Unreal
// acontece em Hooks/, para que esta classe compile e seja testável sem o jogo.
class OverrideTable
{
public:
    // Carrega do disco. Arquivo ausente, ilegível ou malformado => tabela
    // vazia com enabled=true, e o mod segue sem tocar no jogo.
    void load(std::filesystem::path path);

    [[nodiscard]] bool save();
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    // Hot reload (M3): relê só se o mtime mudou. Devolve true se recarregou,
    // para a UI poder avisar "config recarregada".
    bool reload_if_changed();
    bool force_reload();

    // Toggle global: desativa todos os overrides de uma vez, sem desinstalar.
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void set_enabled(bool on);

    [[nodiscard]] std::optional<std::string> track_for(std::string_view encounter_id) const;
    [[nodiscard]] std::optional<std::string> raw_track_for(std::string_view encounter_id) const;

    void set(std::string_view encounter_id, std::string_view track_id);
    void clear(std::string_view encounter_id); // "voltar ao original"
    void clear_all();

    [[nodiscard]] const std::map<std::string, std::string>& entries() const { return m_overrides; }
    [[nodiscard]] std::size_t size() const { return m_overrides.size(); }
    [[nodiscard]] bool dirty() const { return m_dirty; }

    // Presets (M5): o mesmo formato do config.json, para compartilhar uma
    // trilha alternativa inteira como um arquivo só.
    [[nodiscard]] std::string to_json_string() const;
    bool merge_from_json(std::string_view text);
    bool replace_from_json(std::string_view text);

private:
    bool apply_json(std::string_view text, bool replace);
    void remember_mtime();

    std::filesystem::path m_path{};
    std::map<std::string, std::string> m_overrides{};
    bool m_enabled{true};
    bool m_dirty{false};
    std::optional<std::filesystem::file_time_type> m_mtime{};
};
} // namespace e33
