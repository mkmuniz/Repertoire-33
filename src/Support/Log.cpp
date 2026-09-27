#include "Support/Log.hpp"

#include <atomic>
#include <cstdio>

#if defined(E33_WITH_UE4SS)
#include <DynamicOutput/DynamicOutput.hpp>
#endif

namespace e33::log
{
namespace
{
std::atomic<bool> g_verbose{false};
} // namespace

void set_verbose(bool on)
{
    g_verbose.store(on, std::memory_order_relaxed);
}

bool verbose()
{
    return g_verbose.load(std::memory_order_relaxed);
}

void write_line(std::string_view level, std::string_view message)
{
#if defined(E33_WITH_UE4SS)
    const auto line = std::format("[BossMusic/{}] {}\n", level, message);
    RC::Output::send(RC::to_wstring(line));
#else
    std::fprintf(stderr, "[BossMusic/%.*s] %.*s\n", static_cast<int>(level.size()), level.data(),
                 static_cast<int>(message.size()), message.data());
#endif
}
} // namespace e33::log
