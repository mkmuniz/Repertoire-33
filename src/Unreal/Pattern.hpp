#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace e33::unreal
{
// Assinatura de bytes no formato do x64dbg/IDA: "48 8B 05 ?? ?? ?? ??", com `?`
// ou `??` como curinga.
//
// Parse e busca olham só um buffer, então compilam e são testados fora do
// Windows. Quem aponta o buffer para a memória do jogo é Unreal/Memory.
struct Pattern
{
    std::vector<std::uint8_t> bytes{};
    std::vector<bool> exact{}; // false = curinga
};

[[nodiscard]] std::optional<Pattern> parse_pattern(std::string_view text);

// Offset da primeira ocorrência de `pattern` em `haystack` a partir de `from`.
[[nodiscard]] std::optional<std::size_t> find_pattern(std::span<const std::uint8_t> haystack,
                                                      const Pattern& pattern,
                                                      std::size_t from = 0);

// Endereço alvo de um operando relativo a RIP. `instruction` é o início da
// instrução, `disp_offset` onde fica o deslocamento de 32 bits e `length` o
// tamanho da instrução: o x64 soma o deslocamento ao endereço da PRÓXIMA.
[[nodiscard]] std::uintptr_t resolve_rip(std::uintptr_t instruction, std::size_t disp_offset,
                                         std::size_t length);
} // namespace e33::unreal
