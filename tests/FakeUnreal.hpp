#pragma once

// Memória sintética com o layout do UE5, para testar os leitores de Unreal/
// sem o jogo. Cada estrutura é montada byte a byte como o engine monta.

#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

#include "Unreal/Names.hpp"

namespace e33::test
{
// Um FNamePool com um bloco só. `add` devolve o índice que o Unreal daria.
class FakeNamePool
{
public:
    FakeNamePool()
        : m_pool(unreal::NamePool::kBlocksOffset + sizeof(std::uintptr_t) * 4, 0)
        , m_block(std::make_unique<std::uint8_t[]>(kBlockSize))
    {
        const auto block = reinterpret_cast<std::uintptr_t>(m_block.get());
        std::memcpy(m_pool.data() + unreal::NamePool::kBlocksOffset, &block, sizeof(block));
        add("None");
        add("ByteProperty");
    }

    std::uint32_t add(std::string_view name)
    {
        const auto index = static_cast<std::uint32_t>(m_cursor / unreal::NamePool::kStride);
        const auto header = static_cast<std::uint16_t>(name.size() << 6);
        std::memcpy(m_block.get() + m_cursor, &header, sizeof(header));
        std::memcpy(m_block.get() + m_cursor + 2, name.data(), name.size());
        m_cursor += 2 + name.size();
        m_cursor += m_cursor % unreal::NamePool::kStride;
        return index;
    }

    std::uint32_t add_wide(std::u16string_view name)
    {
        const auto index = static_cast<std::uint32_t>(m_cursor / unreal::NamePool::kStride);
        const auto header = static_cast<std::uint16_t>((name.size() << 6) | 0x1);
        std::memcpy(m_block.get() + m_cursor, &header, sizeof(header));
        std::memcpy(m_block.get() + m_cursor + 2, name.data(), name.size() * 2);
        m_cursor += 2 + name.size() * 2;
        return index;
    }

    [[nodiscard]] std::uintptr_t address() const
    {
        return reinterpret_cast<std::uintptr_t>(m_pool.data());
    }

private:
    static constexpr std::size_t kBlockSize = 64 * 1024;

    std::vector<std::uint8_t> m_pool;
    std::unique_ptr<std::uint8_t[]> m_block;
    std::size_t m_cursor{0};
};
} // namespace e33::test
