#pragma once

// Memória sintética com o layout do UE5, para testar os leitores de Unreal/
// sem o jogo. Cada estrutura é montada byte a byte como o engine monta.

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
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
        sync_cursor();
        return index;
    }

    std::uint32_t add_wide(std::u16string_view name)
    {
        const auto index = static_cast<std::uint32_t>(m_cursor / unreal::NamePool::kStride);
        const auto header = static_cast<std::uint16_t>((name.size() << 6) | 0x1);
        std::memcpy(m_block.get() + m_cursor, &header, sizeof(header));
        std::memcpy(m_block.get() + m_cursor + 2, name.data(), name.size() * 2);
        m_cursor += 2 + name.size() * 2;
        sync_cursor();
        return index;
    }

    [[nodiscard]] std::uintptr_t address() const
    {
        return reinterpret_cast<std::uintptr_t>(m_pool.data());
    }

private:
    // CurrentByteCursor (+0x0C): até onde o bloco atual está preenchido.
    void sync_cursor()
    {
        const auto cursor = static_cast<std::uint32_t>(m_cursor);
        std::memcpy(m_pool.data() + 0x0C, &cursor, sizeof(cursor));
    }

    static constexpr std::size_t kBlockSize = 64 * 1024;

    std::vector<std::uint8_t> m_pool;
    std::unique_ptr<std::uint8_t[]> m_block;
    std::size_t m_cursor{0};
};

// Um UObject: o cabeçalho de UObjectBase, mais espaço para os campos de
// Blueprint que os testes usam (o battle manager do E33 vai até 0x9C0).
struct FakeObject
{
    alignas(8) std::uint8_t bytes[0xC00]{};

    [[nodiscard]] std::uintptr_t address() const { return reinterpret_cast<std::uintptr_t>(bytes); }

    template <typename T>
    void put(std::size_t offset, T value)
    {
        std::memcpy(bytes + offset, &value, sizeof(T));
    }

    template <typename T>
    [[nodiscard]] T get(std::size_t offset) const
    {
        T value{};
        std::memcpy(&value, bytes + offset, sizeof(T));
        return value;
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

    // Acrescenta um FProperty ao fim de ChildProperties de `owner` (um UStruct).
    std::uintptr_t add_field(std::uintptr_t owner, std::string_view name, std::string_view type,
                             std::int32_t offset, std::int32_t size, std::uintptr_t inner = 0)
    {
        auto field = std::make_unique<FakeObject>();
        field->put(0x08, field_class(type));
        field->put(0x20, unreal::FName{names.add(name), 0});
        field->put(0x34, size);
        field->put(0x44, offset);
        field->put(0x70, inner);
        const auto address = field->address();

        // Encadeia no fim da lista, como o Unreal mantém a ordem de declaração.
        std::uintptr_t* link = reinterpret_cast<std::uintptr_t*>(object(owner).bytes + 0x50);
        while (*link != 0)
        {
            link = reinterpret_cast<std::uintptr_t*>(*link + 0x18);
        }
        *link = address;
        m_fields.push_back(std::move(field));
        return address;
    }

    void set_super(std::uintptr_t ustruct, std::uintptr_t super)
    {
        object(ustruct).put(0x40, super);
    }

    // O objeto morre: o Unreal zera o slot na tabela (a memória pode continuar lá).
    void kill(std::uintptr_t address)
    {
        const auto index = object(address).get<std::int32_t>(0x0C);
        std::memset(m_chunk.get() + static_cast<std::size_t>(index) * m_item_size, 0,
                    sizeof(std::uintptr_t));
    }

    FakeNamePool names{};

private:
    std::uintptr_t field_class(std::string_view type)
    {
        for (const auto& [name, klass] : m_field_classes)
        {
            if (name == type)
            {
                return klass->address();
            }
        }
        auto klass = std::make_unique<FakeObject>();
        klass->put(0x00, unreal::FName{names.add(type), 0});
        const auto address = klass->address();
        m_field_classes.emplace_back(std::string{type}, std::move(klass));
        return address;
    }

    std::vector<std::unique_ptr<FakeObject>> m_fields{};
    std::vector<std::pair<std::string, std::unique_ptr<FakeObject>>> m_field_classes{};

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
