#include "Support/Log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>

namespace e33::log
{
namespace
{
std::atomic<bool> g_verbose{false};
std::mutex g_mutex;
std::FILE* g_file{nullptr};

const char* prefix()
{
    return "[BossMusic]";
}
} // namespace

void set_verbose(bool on)
{
    g_verbose.store(on, std::memory_order_relaxed);
}

bool verbose()
{
    return g_verbose.load(std::memory_order_relaxed);
}

void open_file(const std::filesystem::path& path)
{
    const std::lock_guard lock{g_mutex};
    if (g_file != nullptr)
    {
        std::fclose(g_file);
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    // "w": o log é de uma sessão. Acumular entre execuções transforma o arquivo
    // que se pede ao usuário em algo que ninguém consegue ler.
    g_file = std::fopen(path.string().c_str(), "w");
}

void close_file()
{
    const std::lock_guard lock{g_mutex};
    if (g_file != nullptr)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void write_line(std::string_view level, std::string_view message)
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count()
                      % 1000;

    const std::lock_guard lock{g_mutex};
    // stderr sempre: com o console do jogo aberto, aparece ali. O arquivo é o
    // que se pede quando alguém reporta problema.
    std::fprintf(stderr, "%s[%s] %.*s\n", prefix(), std::string{level}.c_str(),
                 static_cast<int>(message.size()), message.data());
    if (g_file != nullptr)
    {
        std::fprintf(g_file, "%s[%s] %03lld %.*s\n", prefix(), std::string{level}.c_str(),
                     static_cast<long long>(secs), static_cast<int>(message.size()),
                     message.data());
        std::fflush(g_file);
    }
}
} // namespace e33::log
