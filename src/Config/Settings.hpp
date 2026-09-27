#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace e33
{
// Preferências do mod: hotkey, escala de fonte, verbosidade.
//
// Mora em settings.json, separado do config.json de propósito: config.json é
// compartilhável como preset, e um preset não deve carregar a hotkey nem a
// escala de fonte de quem o montou.
struct Settings
{
    std::string hotkey{"F9"};    // padrão do plano; J é do Gramophone Everywhere
    float font_scale{1.0f};      // essencial em 4K
    bool verbose_log{false};     // M5
    bool start_open{false};
    bool pause_overrides_in_combat{false};

    static constexpr float kMinFontScale = 0.5f;
    static constexpr float kMaxFontScale = 3.0f;

    void load(std::filesystem::path path);
    [[nodiscard]] bool save() const;

    [[nodiscard]] std::string to_json_string() const;
    bool apply_json(std::string_view text);

    // Virtual-key code da hotkey configurada, ou nullopt se o nome não é
    // reconhecido (aí a UI mostra o erro em vez de o mod ficar sem hotkey).
    [[nodiscard]] std::optional<int> hotkey_virtual_key() const;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    void clamp_values();

    std::filesystem::path m_path{};
};

// Nome de tecla -> virtual-key code do Windows. Exposto para a UI validar o
// que o usuário digitou sem esperar o próximo frame do jogo.
[[nodiscard]] std::optional<int> virtual_key_from_name(std::string_view name);
[[nodiscard]] std::string_view name_from_virtual_key(int vk);
} // namespace e33
