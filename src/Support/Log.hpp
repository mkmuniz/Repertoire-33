#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace e33::log
{
// Log verboso opcional (M5): sem isso você depura no escuro quando alguém
// disser "não funcionou no boss X".
void set_verbose(bool on);
[[nodiscard]] bool verbose();

void write_line(std::string_view level, std::string_view message);

// Log em arquivo, dentro da pasta do mod. Sem isso você depura no escuro quando
// alguém disser "não funcionou no boss X" — e é a primeira coisa que se pede.
void open_file(const std::filesystem::path& path);
void close_file();

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args)
{
    write_line("info", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args)
{
    write_line("warn", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args)
{
    if (!verbose())
    {
        return;
    }
    write_line("debug", std::format(fmt, std::forward<Args>(args)...));
}
} // namespace e33::log
