#pragma once

#include <filesystem>
#include <random>
#include <string>

namespace e33::test
{
// Diretório temporário que se apaga sozinho, para os testes que tocam disco.
class TempDir
{
public:
    TempDir()
    {
        static std::mt19937 rng{std::random_device{}()};
        const auto id = std::uniform_int_distribution<unsigned>{0, 0xFFFFFFu}(rng);
        m_path = std::filesystem::temp_directory_path() / ("e33-test-" + std::to_string(id));
        std::filesystem::create_directories(m_path);
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] std::filesystem::path file(std::string_view name) const { return m_path / name; }
    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path{};
};
} // namespace e33::test
