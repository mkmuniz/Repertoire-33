#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "Unreal/Objects.hpp"

namespace e33::unreal
{
// Layout de UStruct e FField/FProperty no UE 5.4 Shipping — a versão do E33.
// Conferido contra o SDK do Dumper-7 do jogo e contra um SDK nativo que lê
// esses campos no build Steam. validate_layout() reconfere em runtime antes de
// qualquer escrita: se um patch mudar isto, o mod desliga a troca em vez de
// escrever no lugar errado.
namespace struct_layout
{
constexpr std::size_t kSuperStruct = 0x40;
constexpr std::size_t kChildProperties = 0x50;
constexpr std::size_t kPropertiesSize = 0x58;
} // namespace struct_layout

namespace field_layout
{
constexpr std::size_t kClass = 0x08; // FFieldClass*, cujo primeiro campo é o FName do tipo
constexpr std::size_t kNext = 0x18;
constexpr std::size_t kName = 0x20;
constexpr std::size_t kElementSize = 0x34;
constexpr std::size_t kOffset = 0x44;
constexpr std::size_t kStruct = 0x70; // FStructProperty::Struct
} // namespace field_layout

struct Property
{
    std::uintptr_t address{0};
    std::string name{};
    std::string type{}; // "StructProperty", "SoftObjectProperty"...
    std::int32_t offset{0};
    std::int32_t size{0};
};

// Nome de membro como o Blueprint grava num UserDefinedStruct:
// "Music_16_0332400D492AAEF7CE88BEBEA72A3D9B" é o campo "Music". Aceita o nome
// exato ou esse sufixo de editor — e nada além: "Music_FadeOut_12_<guid>" não
// é "Music".
[[nodiscard]] bool matches_member_name(std::string_view actual, std::string_view wanted);

class Properties
{
public:
    explicit Properties(const Objects& objects) : m_objects{objects} {}

    // Procura em `ustruct` e nas superclasses.
    [[nodiscard]] std::optional<Property> find(std::uintptr_t ustruct, std::string_view name) const;

    // Struct de um StructProperty (para descer em FS_MusicParameters etc.).
    [[nodiscard]] static std::uintptr_t struct_of(const Property& property);

    [[nodiscard]] static std::uintptr_t super_of(std::uintptr_t ustruct);
    [[nodiscard]] static bool is_a(std::uintptr_t klass, std::uintptr_t base);

    // Confere os offsets acima contra objetos que todo Unreal tem. Devolve a
    // descrição do que não bateu, ou vazio se tudo confere.
    [[nodiscard]] std::string validate_layout() const;

private:
    const Objects& m_objects;
};
} // namespace e33::unreal
