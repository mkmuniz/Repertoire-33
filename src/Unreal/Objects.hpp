#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "Unreal/Memory.hpp"
#include "Unreal/Names.hpp"

namespace e33::unreal
{
// Campos de UObjectBase. Estáveis do UE 4.11 ao 5.5: é o cabeçalho que todo
// objeto tem, e que o próprio GC lê.
namespace object_layout
{
constexpr std::size_t kInternalIndex = 0x0C;
constexpr std::size_t kClass = 0x10;
constexpr std::size_t kName = 0x18;
constexpr std::size_t kOuter = 0x20;
} // namespace object_layout

// TUObjectArray do UE5: vetor de chunks de 64K FUObjectItem.
//
//   +0x00 FUObjectItem** Objects
//   +0x08 FUObjectItem*  PreAllocatedObjects
//   +0x10 int32 MaxElements    +0x14 int32 NumElements
//   +0x18 int32 MaxChunks      +0x1C int32 NumChunks
//
// O tamanho do FUObjectItem (0x18 na maioria dos builds) é medido, não
// assumido: o item em que o índice interno do objeto bate com a posição.
class ObjectArray
{
public:
    static constexpr std::int32_t kChunkSize = 64 * 1024;

    // `array` aponta para o TUObjectArray (ObjObjects, +0x10 no FUObjectArray).
    [[nodiscard]] static std::optional<ObjectArray> at(std::uintptr_t array);
    [[nodiscard]] static std::optional<ObjectArray> find(const Image& image);

    [[nodiscard]] std::int32_t count() const;

    // Objeto no índice, ou 0 para slot livre.
    [[nodiscard]] std::uintptr_t object(std::int32_t index) const;

    [[nodiscard]] std::uintptr_t address() const { return m_array; }
    [[nodiscard]] std::size_t item_size() const { return m_item_size; }

private:
    ObjectArray(std::uintptr_t array, std::size_t item_size)
        : m_array{array}
        , m_item_size{item_size}
    {
    }

    std::uintptr_t m_array{0};
    std::size_t m_item_size{0};
};

// Leitura de objetos: nome, classe, outer, e busca por nome. Junta o pool de
// nomes e a tabela de objetos, que sozinhos não respondem "quem é este objeto".
class Objects
{
public:
    Objects(ObjectArray array, NamePool names)
        : m_array{array}
        , m_names{names}
    {
    }

    [[nodiscard]] const ObjectArray& array() const { return m_array; }
    [[nodiscard]] const NamePool& names() const { return m_names; }

    [[nodiscard]] static std::uintptr_t class_of(std::uintptr_t object);
    [[nodiscard]] static std::uintptr_t outer_of(std::uintptr_t object);

    [[nodiscard]] std::string name(std::uintptr_t object) const;
    [[nodiscard]] std::string class_name(std::uintptr_t object) const;

    // "Pacote.Outer.Nome": /Game/Audio/.../MUS_Track_X.MUS_Track_X, ou
    // /Script/Engine.Actor.ReceiveTick. Ponto em todo nível, como o Dumper-7.
    [[nodiscard]] std::string path(std::uintptr_t object) const;

    // "Classe Caminho", o formato de UObject::GetFullName e do Dumper-7.
    [[nodiscard]] std::string full_name(std::uintptr_t object) const;

    // Primeiro objeto cujo full_name é exatamente `full`. Varredura linear:
    // chame na inicialização e guarde o resultado, nunca por quadro.
    [[nodiscard]] std::uintptr_t find(std::string_view full) const;

private:
    ObjectArray m_array;
    NamePool m_names;
};
} // namespace e33::unreal
