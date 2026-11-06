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

// Um UObject: só o cabeçalho de UObjectBase, mais espaço livre para os testes
// que precisam de campos de UStruct/UFunction além dele.
struct FakeObject
{
    alignas(8) std::uint8_t bytes[0x100]{};

    [[nodiscard]] std::uintptr_t address() const { return reinterpret_cast<std::uintptr_t>(bytes); }

    template <typename T>
    void put(std::size_t offset, T value)
    {
        std::memcpy(bytes + offset, &value, sizeof(T));
    }
};

// TUObjectArray com chunks de 64K itens de `item_size` bytes, como o UE5.
class FakeObjectWorld
{
public:
    explicit FakeObjectWorld(std::size_t item_size = 0x18)
        : m_item_size{item_size}
        , m_chunk(std::make_unique<std::uint8_t[]>(item_size * 64 * 1024))
    {
        m_chunks[0] = reinterpret_cast<std::uintptr_t>(m_chunk.get());
        put_array(0x00, reinterpret_cast<std::uintptr_t>(m_chunks));
        put_array(0x10, std::int32_t{64 * 1024}); // MaxElements
        put_array(0x18, std::int32_t{1});         // MaxChunks
        put_array(0x1C, std::int32_t{1});         // NumChunks
    }

    // Cria um objeto `name` de classe `klass` (0 = ele mesmo é classe raiz)
    // dentro de `outer`. Devolve o endereço.
    std::uintptr_t add(std::string_view name, std::uintptr_t klass = 0, std::uintptr_t outer = 0)
    {
        auto object = std::make_unique<FakeObject>();
        const auto index = static_cast<std::int32_t>(m_objects.size());
        object->put(0x0C, index);
        object->put(0x10, klass == 0 ? object->address() : klass);
        object->put(0x18, unreal::FName{names.add(name), 0});
        object->put(0x20, outer);

        const auto address = object->address();
        std::memcpy(m_chunk.get() + static_cast<std::size_t>(index) * m_item_size, &address,
                    sizeof(address));
        m_objects.push_back(std::move(object));
        put_array(0x14, static_cast<std::int32_t>(m_objects.size())); // NumElements
        return address;
    }

    FakeObject& object(std::uintptr_t address)
    {
        for (auto& object : m_objects)
        {
            if (object->address() == address)
            {
                return *object;
            }
        }
        return *m_objects.front();
    }

    [[nodiscard]] std::uintptr_t array_address() const
    {
        return reinterpret_cast<std::uintptr_t>(m_array);
    }

    FakeNamePool names{};

private:
    template <typename T>
    void put_array(std::size_t offset, T value)
    {
        std::memcpy(m_array + offset, &value, sizeof(T));
    }

    std::size_t m_item_size;
    std::unique_ptr<std::uint8_t[]> m_chunk;
    std::uintptr_t m_chunks[1]{};
    alignas(8) std::uint8_t m_array[0x20]{};
    std::vector<std::unique_ptr<FakeObject>> m_objects{};
};
} // namespace e33::test
