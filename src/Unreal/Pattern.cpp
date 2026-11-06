#include "Unreal/Pattern.hpp"

#include <charconv>
#include <cstring>

namespace e33::unreal
{
std::optional<Pattern> parse_pattern(std::string_view text)
{
    Pattern pattern;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == ' ')
        {
            ++i;
            continue;
        }

        auto end = text.find(' ', i);
        if (end == std::string_view::npos)
        {
            end = text.size();
        }
        const auto token = text.substr(i, end - i);
        i = end;

        if (token == "?" || token == "??")
        {
            pattern.bytes.push_back(0);
            pattern.exact.push_back(false);
            continue;
        }

        unsigned value = 0;
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
        if (token.size() != 2 || ec != std::errc{} || ptr != token.data() + token.size())
        {
            return std::nullopt;
        }
        pattern.bytes.push_back(static_cast<std::uint8_t>(value));
        pattern.exact.push_back(true);
    }

    if (pattern.bytes.empty())
    {
        return std::nullopt;
    }
    return pattern;
}

std::optional<std::size_t> find_pattern(std::span<const std::uint8_t> haystack,
                                        const Pattern& pattern, std::size_t from)
{
    const auto size = pattern.bytes.size();
    if (size == 0 || haystack.size() < size)
    {
        return std::nullopt;
    }

    for (std::size_t start = from; start + size <= haystack.size(); ++start)
    {
        bool match = true;
        for (std::size_t k = 0; k < size; ++k)
        {
            if (pattern.exact[k] && haystack[start + k] != pattern.bytes[k])
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return start;
        }
    }
    return std::nullopt;
}

std::uintptr_t resolve_rip(std::uintptr_t instruction, std::size_t disp_offset, std::size_t length)
{
    std::int32_t disp = 0;
    std::memcpy(&disp, reinterpret_cast<const void*>(instruction + disp_offset), sizeof(disp));
    return instruction + length + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(disp));
}
} // namespace e33::unreal
