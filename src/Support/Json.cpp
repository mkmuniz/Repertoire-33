#include "Support/Json.hpp"

#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace e33
{
std::optional<std::string> read_text_file(const std::filesystem::path& path)
{
    std::ifstream in{path, std::ios::binary};
    if (!in)
    {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad())
    {
        return std::nullopt;
    }
    return buffer.str();
}

namespace
{
std::filesystem::path temp_sibling_of(const std::filesystem::path& path)
{
    static std::mt19937 rng{std::random_device{}()};
    const auto suffix = std::uniform_int_distribution<unsigned>{0, 0xFFFFFFu}(rng);
    auto temp = path;
    temp += ".tmp-" + std::to_string(suffix);
    return temp;
}
} // namespace

bool write_text_file_atomic(const std::filesystem::path& path, std::string_view contents)
{
    std::error_code ec;
    if (const auto parent = path.parent_path(); !parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
    }

    const auto temp = temp_sibling_of(path);
    {
        std::ofstream out{temp, std::ios::binary | std::ios::trunc};
        if (!out)
        {
            return false;
        }
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        out.flush();
        if (!out)
        {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }

    std::filesystem::rename(temp, path, ec);
    if (ec)
    {
        // Windows recusa rename sobre arquivo existente em alguns casos.
        std::filesystem::remove(path, ec);
        std::filesystem::rename(temp, path, ec);
    }
    if (ec)
    {
        std::error_code cleanup;
        std::filesystem::remove(temp, cleanup);
        return false;
    }
    return true;
}

std::optional<Json> parse_json(std::string_view text)
{
    auto parsed = Json::parse(text, nullptr, /*allow_exceptions=*/false, /*ignore_comments=*/true);
    if (parsed.is_discarded())
    {
        return std::nullopt;
    }
    return parsed;
}
} // namespace e33
