#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "Unreal/Memory.hpp"

namespace e33::unreal
{
// FName como fica dentro de um UObject num build Shipping (sem
// WITH_CASE_PRESERVING_NAME): índice no pool + número de instância.
struct FName
{
    std::uint32_t comparison_index{0};
    std::uint32_t number{0};
};

// Leitor do FNamePool do UE5, o dono de todas as strings de nome.
//
// Layout (UE 5.0 a 5.5):
//   FNamePool  +0x00 FRWLock (8 bytes)
//              +0x08 uint32 CurrentBlock
//              +0x0C uint32 CurrentByteCursor
//              +0x10 uint8* Blocks[8192]
//   handle     bloco = índice >> 16, offset = (índice & 0xFFFF) * 2
//   entrada    uint16 cabeçalho (bit 0 = wide, bits 6..15 = tamanho) + chars
class NamePool
{
public:
    static constexpr std::size_t kBlocksOffset = 0x10;
    static constexpr std::uint32_t kMaxBlocks = 8192;
    static constexpr std::size_t kStride = 2;

    // `pool` é o endereço do FNamePool. Só aceita se a entrada 0 for "None",
    // que todo Unreal registra primeiro.
    [[nodiscard]] static std::optional<NamePool> at(std::uintptr_t pool);

    // Procura o FNamePool na seção de dados do executável.
    [[nodiscard]] static std::optional<NamePool> find(const Image& image);

    // Nome base da entrada, em UTF-8. nullopt se o índice não aponta para uma
    // entrada legível.
    [[nodiscard]] std::optional<std::string> entry(std::uint32_t comparison_index) const;

    // Nome completo: "Boss_Simon", ou "Boss_Simon_2" quando number = 3 (o
    // Unreal guarda o sufixo somado de 1).
    [[nodiscard]] std::string to_string(FName name) const;

    // Busca reversa: o índice da entrada cujo texto é exatamente `text`, sem
    // criar nome novo — o mod nunca chama o construtor de FName do engine. Em
    // jogo cozinhado o asset registry já registrou o caminho de todo asset, então
    // as faixas do jogo estão aqui. Varre o pool inteiro: chame raramente e
    // guarde o resultado.
    [[nodiscard]] std::optional<std::uint32_t> find_entry(std::string_view text) const;

    [[nodiscard]] std::uintptr_t address() const { return m_pool; }

private:
    explicit NamePool(std::uintptr_t pool) : m_pool{pool} {}

    std::uintptr_t m_pool{0};
};
} // namespace e33::unreal
