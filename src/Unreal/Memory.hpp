#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace e33::unreal
{
struct Section
{
    std::uintptr_t start{0};
    std::size_t size{0};

    [[nodiscard]] bool contains(std::uintptr_t address) const
    {
        return address >= start && address < start + size;
    }
    [[nodiscard]] std::span<const std::uint8_t> bytes() const
    {
        return {reinterpret_cast<const std::uint8_t*>(start), size};
    }
};

// O executável do jogo. Num build Shipping do Unreal tudo é linkado nele —
// engine, plugins, Wwise — então é o único módulo onde procurar.
struct Image
{
    std::uintptr_t base{0};
    std::size_t size{0};
    Section text{};  // código
    Section rdata{}; // constantes: vtables, strings
    Section data{};  // globais: GUObjectArray, FNamePool

    [[nodiscard]] bool contains(std::uintptr_t address) const
    {
        return address >= base && address < base + size;
    }
};

// Lê os cabeçalhos PE do executável do processo. nullopt fora do Windows.
[[nodiscard]] std::optional<Image> main_image();

// true se [address, address+size) está mapeado e legível.
[[nodiscard]] bool is_readable(const void* address, std::size_t size);

// Copia `size` bytes do jogo para `out`; false se algum byte não é legível.
// Ler memória do jogo sem proteção derruba o processo no primeiro ponteiro
// inválido — e um mod de música não pode, em nenhum caso, fechar o jogo.
//
// No MSVC é um __try (SEH): sem syscall, então serve para varrer centenas de
// milhares de objetos. Nos outros compiladores cai no is_readable.
[[nodiscard]] bool copy_memory(void* out, std::uintptr_t address, std::size_t size);

template <typename T>
[[nodiscard]] std::optional<T> read(std::uintptr_t address)
{
    T value{};
    if (address == 0 || !copy_memory(&value, address, sizeof(T)))
    {
        return std::nullopt;
    }
    return value;
}
} // namespace e33::unreal
