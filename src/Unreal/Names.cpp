#include "Unreal/Names.hpp"

#include <format>
#include <string_view>

namespace e33::unreal
{
namespace
{
constexpr std::uint16_t kWideBit = 0x1;
constexpr unsigned kLengthShift = 6;
constexpr std::uint16_t kMaxLength = 1024; // 10 bits

// Wide no pool é UTF-16. Nomes de asset do E33 são ASCII na prática, mas um
// nome acentuado não pode virar lixo no log.
void append_utf8(std::string& out, std::uint32_t cp)
{
    if (cp < 0x80)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::optional<std::string> read_entry(std::uintptr_t blocks, std::uint32_t index)
{
    const auto block = index >> 16;
    const auto offset = static_cast<std::uintptr_t>(index & 0xFFFF) * NamePool::kStride;
    if (block >= NamePool::kMaxBlocks)
    {
        return std::nullopt;
    }

    const auto base = read<std::uintptr_t>(blocks + block * sizeof(std::uintptr_t));
    if (!base || *base == 0)
    {
        return std::nullopt;
    }

    const auto header = read<std::uint16_t>(*base + offset);
    if (!header)
    {
        return std::nullopt;
    }
    const bool wide = (*header & kWideBit) != 0;
    const std::uint16_t length = *header >> kLengthShift;
    if (length == 0 || length >= kMaxLength)
    {
        return std::nullopt;
    }

    const auto chars = *base + offset + sizeof(std::uint16_t);
    const std::size_t bytes = static_cast<std::size_t>(length) * (wide ? 2 : 1);
    if (!is_readable(reinterpret_cast<const void*>(chars), bytes))
    {
        return std::nullopt;
    }

    std::string out;
    out.reserve(length);
    if (wide)
    {
        const auto* utf16 = reinterpret_cast<const char16_t*>(chars);
        for (std::uint16_t i = 0; i < length; ++i)
        {
            append_utf8(out, utf16[i]);
        }
    }
    else
    {
        // Narrow é Latin-1, não UTF-8: "è" ocupa um byte.
        const auto* latin1 = reinterpret_cast<const unsigned char*>(chars);
        for (std::uint16_t i = 0; i < length; ++i)
        {
            append_utf8(out, latin1[i]);
        }
    }
    return out;
}
} // namespace

std::optional<NamePool> NamePool::at(std::uintptr_t pool)
{
    if (pool == 0)
    {
        return std::nullopt;
    }
    const auto none = read_entry(pool + kBlocksOffset, 0);
    if (!none || *none != "None")
    {
        return std::nullopt;
    }
    return NamePool{pool};
}

std::optional<NamePool> NamePool::find(const Image& image)
{
    // Varre a .data procurando um bloco 0 cuja primeira entrada seja "None".
    // É a assinatura estrutural que o Dumper-7 também usa: independe de
    // compilador e de patch, porque o layout do pool não muda entre eles.
    const auto start = image.data.start;
    const auto end = image.data.start + image.data.size;
    for (auto cursor = start; cursor + kBlocksOffset + sizeof(std::uintptr_t) <= end;
         cursor += sizeof(std::uintptr_t))
    {
        // Filtro barato antes de seguir ponteiro: CurrentBlock é pequeno.
        const auto current_block = *reinterpret_cast<const std::uint32_t*>(cursor + 0x8);
        if (current_block >= kMaxBlocks)
        {
            continue;
        }
        const auto block0 = *reinterpret_cast<const std::uintptr_t*>(cursor + kBlocksOffset);
        if (block0 == 0 || image.contains(block0))
        {
            continue; // os blocos são alocados no heap, nunca dentro do módulo
        }
        if (auto pool = at(cursor))
        {
            // Segunda prova: a entrada seguinte do Unreal é sempre "ByteProperty",
            // logo depois de "None" (2 bytes de cabeçalho + 4 chars).
            constexpr std::uint32_t kSecondEntry = (2 + 4) / kStride;
            if (pool->entry(kSecondEntry) == "ByteProperty")
            {
                return pool;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> NamePool::entry(std::uint32_t comparison_index) const
{
    return read_entry(m_pool + kBlocksOffset, comparison_index);
}

std::string NamePool::to_string(FName name) const
{
    auto base = entry(name.comparison_index);
    if (!base)
    {
        return std::format("<FName {}>", name.comparison_index);
    }
    if (name.number == 0)
    {
        return std::move(*base);
    }
    return std::format("{}_{}", *base, name.number - 1);
}
} // namespace e33::unreal
