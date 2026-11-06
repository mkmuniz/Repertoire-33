#include "Unreal/Names.hpp"

#include <algorithm>
#include <array>
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
    std::array<std::uint8_t, kMaxLength * 2> raw{};
    if (!copy_memory(raw.data(), chars, static_cast<std::size_t>(length) * (wide ? 2 : 1)))
    {
        return std::nullopt;
    }

    std::string out;
    out.reserve(length);
    for (std::uint16_t i = 0; i < length; ++i)
    {
        // Narrow é Latin-1, não UTF-8: "è" ocupa um byte.
        const std::uint32_t cp = wide ? static_cast<std::uint32_t>(raw[i * 2])
                                            | (static_cast<std::uint32_t>(raw[i * 2 + 1]) << 8)
                                      : raw[i];
        append_utf8(out, cp);
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
        // Filtro barato antes de seguir ponteiro: CurrentBlock é pequeno. Lido
        // pelo caminho protegido: nem toda página da .data é garantidamente
        // legível num executável com protetor.
        const auto current_block = read<std::uint32_t>(cursor + 0x8);
        if (!current_block || *current_block >= kMaxBlocks)
        {
            continue;
        }
        const auto block0 = read<std::uintptr_t>(cursor + kBlocksOffset).value_or(0);
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

SplitName split_name_number(std::string_view text)
{
    std::size_t digits = 0;
    while (digits < text.size() && text[text.size() - 1 - digits] >= '0'
           && text[text.size() - 1 - digits] <= '9')
    {
        ++digits;
    }

    constexpr std::size_t kMaxDigits = 10;
    const auto first_digit = text.size() - digits;
    if (digits == 0 || digits >= text.size() || digits > kMaxDigits
        || text[first_digit - 1] != '_' || (digits > 1 && text[first_digit] == '0'))
    {
        return {text, 0};
    }

    std::uint64_t value = 0;
    for (std::size_t i = first_digit; i < text.size(); ++i)
    {
        value = value * 10 + static_cast<std::uint64_t>(text[i] - '0');
    }
    if (value >= 0x7FFFFFFF)
    {
        return {text, 0};
    }
    return {text.substr(0, first_digit - 1), static_cast<std::uint32_t>(value + 1)};
}

std::optional<std::uint32_t> NamePool::find_entry(std::string_view text) const
{
    return find_entries(std::span{&text, 1}).front();
}

std::vector<std::optional<std::uint32_t>>
NamePool::find_entries(std::span<const std::string_view> texts) const
{
    std::vector<std::optional<std::uint32_t>> found(texts.size());
    const auto lower = [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    };

    // Baldes por tamanho: uma entrada do pool só é lida se algum texto pedido
    // tem o mesmo tamanho, e aí só é comparada com esses.
    std::vector<std::string> wanted(texts.size());
    std::vector<std::vector<std::size_t>> by_length(kMaxLength);
    std::size_t remaining = 0;
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        if (texts[i].empty() || texts[i].size() >= kMaxLength)
        {
            continue;
        }
        wanted[i].resize(texts[i].size());
        std::ranges::transform(texts[i], wanted[i].begin(), lower);
        by_length[texts[i].size()].push_back(i);
        ++remaining;
    }
    std::array<char, kMaxLength> chars{};

    // Blocos são preenchidos em ordem; ao abrir um bloco novo o Unreal grava um
    // cabeçalho de tamanho 0 no fim do anterior, que é onde a varredura para.
    constexpr std::size_t kBlockBytes = 0x10000 * kStride;
    const auto current_block = read<std::uint32_t>(m_pool + 0x08).value_or(0);
    const auto current_cursor = read<std::uint32_t>(m_pool + 0x0C).value_or(0);

    for (std::uint32_t block = 0; block <= current_block && block < kMaxBlocks && remaining > 0;
         ++block)
    {
        const auto base = read<std::uintptr_t>(m_pool + kBlocksOffset + block * sizeof(std::uintptr_t));
        if (!base || *base == 0)
        {
            break;
        }
        const std::size_t limit = block == current_block ? current_cursor : kBlockBytes;
        std::size_t offset = 0;
        while (offset + sizeof(std::uint16_t) <= limit && remaining > 0)
        {
            const auto header = read<std::uint16_t>(*base + offset);
            if (!header)
            {
                break;
            }
            const bool wide = (*header & kWideBit) != 0;
            const std::uint16_t length = *header >> kLengthShift;
            if (length == 0)
            {
                break;
            }
            const auto index = (block << 16) | static_cast<std::uint32_t>(offset / kStride);

            // Só nomes narrow: caminhos de asset são ASCII.
            if (!wide && !by_length[length].empty()
                && copy_memory(chars.data(), *base + offset + sizeof(std::uint16_t), length))
            {
                std::ranges::transform(chars.begin(), chars.begin() + length, chars.begin(), lower);
                const std::string_view text{chars.data(), length};
                for (const auto i : by_length[length])
                {
                    if (!found[i] && text == wanted[i])
                    {
                        found[i] = index;
                        --remaining;
                    }
                }
            }
            offset += sizeof(std::uint16_t) + static_cast<std::size_t>(length) * (wide ? 2 : 1);
            offset += offset % kStride;
        }
    }
    return found;
}

std::optional<FName> NamePool::find_name(std::string_view text) const
{
    const auto split = split_name_number(text);
    const auto index = find_entry(split.base);
    if (!index)
    {
        return std::nullopt;
    }
    return FName{*index, split.number};
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
