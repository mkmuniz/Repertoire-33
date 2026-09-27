#include "Config/Settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "Support/Json.hpp"
#include "Support/Log.hpp"

namespace e33
{
namespace
{
struct KeyName
{
    std::string_view name;
    int vk;
};

// Subconjunto dos virtual-key codes do Windows que faz sentido como hotkey de
// overlay. Valores literais para não arrastar windows.h para os testes nativos.
constexpr std::array kKeys{
    KeyName{"F1", 0x70},  KeyName{"F2", 0x71},  KeyName{"F3", 0x72},  KeyName{"F4", 0x73},
    KeyName{"F5", 0x74},  KeyName{"F6", 0x75},  KeyName{"F7", 0x76},  KeyName{"F8", 0x77},
    KeyName{"F9", 0x78},  KeyName{"F10", 0x79}, KeyName{"F11", 0x7A}, KeyName{"F12", 0x7B},
    KeyName{"INSERT", 0x2D}, KeyName{"DELETE", 0x2E}, KeyName{"HOME", 0x24},
    KeyName{"END", 0x23}, KeyName{"PAGEUP", 0x21}, KeyName{"PAGEDOWN", 0x22},
    KeyName{"SCROLLLOCK", 0x91}, KeyName{"PAUSE", 0x13}, KeyName{"TAB", 0x09},
};

std::string upper(std::string_view text)
{
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    std::erase(out, ' ');
    std::erase(out, '_');
    return out;
}
} // namespace

std::optional<int> virtual_key_from_name(std::string_view name)
{
    const auto needle = upper(name);
    for (const auto& key : kKeys)
    {
        if (key.name == needle)
        {
            return key.vk;
        }
    }
    // Uma letra ou dígito: os virtual-key codes coincidem com o ASCII maiúsculo.
    if (needle.size() == 1)
    {
        const auto c = static_cast<unsigned char>(needle.front());
        if (std::isalnum(c) != 0)
        {
            return static_cast<int>(c);
        }
    }
    return std::nullopt;
}

std::string_view name_from_virtual_key(int vk)
{
    for (const auto& key : kKeys)
    {
        if (key.vk == vk)
        {
            return key.name;
        }
    }
    return {};
}

void Settings::load(std::filesystem::path path)
{
    m_path = std::move(path);
    const auto text = read_text_file(m_path);
    if (!text)
    {
        log::debug("settings nao encontrado em {}, usando os padroes", m_path.string());
        return;
    }
    if (!apply_json(*text))
    {
        log::warn("settings invalido em {}, usando os padroes", m_path.string());
        // Reset dos valores, preservando o caminho: quem chamou ainda precisa
        // dele para gravar os padroes de volta.
        auto kept = std::move(m_path);
        *this = Settings{};
        m_path = std::move(kept);
    }
    log::set_verbose(verbose_log);
}

bool Settings::save() const
{
    if (m_path.empty())
    {
        return false;
    }
    return write_text_file_atomic(m_path, to_json_string());
}

std::string Settings::to_json_string() const
{
    Json root = Json::object();
    root["hotkey"] = hotkey;
    root["font_scale"] = font_scale;
    root["verbose_log"] = verbose_log;
    root["start_open"] = start_open;
    root["pause_overrides_in_combat"] = pause_overrides_in_combat;
    return root.dump(2) + "\n";
}

bool Settings::apply_json(std::string_view text)
{
    const auto parsed = parse_json(text);
    if (!parsed || !parsed->is_object())
    {
        return false;
    }

    // Cada campo é opcional: um settings.json escrito por uma versão anterior
    // do mod continua válido, e os campos novos assumem o padrão.
    if (const auto it = parsed->find("hotkey"); it != parsed->end() && it->is_string())
    {
        const auto name = it->get<std::string>();
        if (virtual_key_from_name(name))
        {
            hotkey = name;
        }
        else
        {
            log::warn("hotkey \"{}\" nao reconhecida, mantendo {}", name, hotkey);
        }
    }
    if (const auto it = parsed->find("font_scale"); it != parsed->end() && it->is_number())
    {
        font_scale = it->get<float>();
    }
    if (const auto it = parsed->find("verbose_log"); it != parsed->end() && it->is_boolean())
    {
        verbose_log = it->get<bool>();
    }
    if (const auto it = parsed->find("start_open"); it != parsed->end() && it->is_boolean())
    {
        start_open = it->get<bool>();
    }
    if (const auto it = parsed->find("pause_overrides_in_combat");
        it != parsed->end() && it->is_boolean())
    {
        pause_overrides_in_combat = it->get<bool>();
    }

    clamp_values();
    return true;
}

std::optional<int> Settings::hotkey_virtual_key() const
{
    return virtual_key_from_name(hotkey);
}

void Settings::clamp_values()
{
    font_scale = std::clamp(font_scale, kMinFontScale, kMaxFontScale);
}
} // namespace e33
