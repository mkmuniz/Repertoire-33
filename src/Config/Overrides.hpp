#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace e33
{
// Tabela boss -> faixa. VAZIA por padrão: sem entrada, o hook não faz nada.
class OverrideTable
{
public:
    void load_or_create(const std::filesystem::path& path);
    void save() const;
    void reload(); // hot reload: M3

    [[nodiscard]] bool enabled() const { return m_enabled; }
    void set_enabled(bool v) { m_enabled = v; } // toggle global

    [[nodiscard]] std::optional<std::wstring> track_for(const std::wstring& encounter_id) const;
    void set(const std::wstring& encounter_id, const std::wstring& track_id);
    void clear(const std::wstring& encounter_id); // "voltar ao original"

private:
    std::filesystem::path m_path{};
    bool m_enabled{true};
    std::unordered_map<std::wstring, std::wstring> m_overrides{};
};
} // namespace e33
