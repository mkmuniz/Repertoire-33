#include "Unreal/Objects.hpp"

#include <array>
#include <vector>

namespace e33::unreal
{
namespace
{
constexpr std::size_t kObjects = 0x00;
constexpr std::size_t kMaxElements = 0x10;
constexpr std::size_t kNumElements = 0x14;
constexpr std::size_t kMaxChunks = 0x18;
constexpr std::size_t kNumChunks = 0x1C;

constexpr std::int32_t chunks_for(std::int32_t elements)
{
    return (elements + ObjectArray::kChunkSize - 1) / ObjectArray::kChunkSize;
}

std::uintptr_t item_object(std::uintptr_t array, std::size_t item_size, std::int32_t index)
{
    const auto chunks = read<std::uintptr_t>(array + kObjects);
    if (!chunks || *chunks == 0)
    {
        return 0;
    }
    const auto chunk_index = static_cast<std::uintptr_t>(index / ObjectArray::kChunkSize);
    const auto chunk = read<std::uintptr_t>(*chunks + chunk_index * sizeof(std::uintptr_t));
    if (!chunk || *chunk == 0)
    {
        return 0;
    }
    const auto within = static_cast<std::uintptr_t>(index % ObjectArray::kChunkSize);
    return read<std::uintptr_t>(*chunk + within * item_size).value_or(0);
}

// Quantos dos primeiros objetos têm o índice interno igual à posição. Com o
// tamanho de item errado, a partir do segundo nenhum bate.
int matching_indices(std::uintptr_t array, std::size_t item_size, std::int32_t sample)
{
    int matches = 0;
    for (std::int32_t i = 0; i < sample; ++i)
    {
        const auto object = item_object(array, item_size, i);
        if (object == 0)
        {
            continue;
        }
        if (read<std::int32_t>(object + object_layout::kInternalIndex) == i)
        {
            ++matches;
        }
    }
    return matches;
}
} // namespace

std::optional<ObjectArray> ObjectArray::at(std::uintptr_t array)
{
    const auto max_elements = read<std::int32_t>(array + kMaxElements);
    const auto num_elements = read<std::int32_t>(array + kNumElements);
    const auto max_chunks = read<std::int32_t>(array + kMaxChunks);
    const auto num_chunks = read<std::int32_t>(array + kNumChunks);
    if (!max_elements || !num_elements || !max_chunks || !num_chunks)
    {
        return std::nullopt;
    }
    if (*num_elements <= 0 || *num_elements > *max_elements || *max_elements > 0x4000000)
    {
        return std::nullopt;
    }
    if (*num_chunks != chunks_for(*num_elements) || *max_chunks != chunks_for(*max_elements))
    {
        return std::nullopt;
    }

    constexpr std::array<std::size_t, 3> kItemSizes{0x18, 0x10, 0x20};
    const std::int32_t sample = *num_elements < 16 ? *num_elements : 16;
    for (const auto size : kItemSizes)
    {
        if (matching_indices(array, size, sample) * 4 >= sample * 3)
        {
            return ObjectArray{array, size};
        }
    }
    return std::nullopt;
}

std::optional<ObjectArray> ObjectArray::find(const Image& image)
{
    const auto end = image.data.start + image.data.size;
    for (auto cursor = image.data.start; cursor + kNumChunks + 4 <= end; cursor += 8)
    {
        // Filtros baratos, só com leituras dentro da própria .data — mesmo
        // assim protegidas: nem toda página dela é garantidamente legível.
        const auto chunks = read<std::uintptr_t>(cursor + kObjects).value_or(0);
        if (chunks == 0 || image.contains(chunks))
        {
            continue;
        }
        const auto num = read<std::int32_t>(cursor + kNumElements).value_or(0);
        const auto num_chunks = read<std::int32_t>(cursor + kNumChunks).value_or(-1);
        if (num <= 0 || num_chunks != chunks_for(num))
        {
            continue;
        }
        if (auto found = at(cursor))
        {
            return found;
        }
    }
    return std::nullopt;
}

std::int32_t ObjectArray::count() const
{
    return read<std::int32_t>(m_array + kNumElements).value_or(0);
}

std::uintptr_t ObjectArray::object(std::int32_t index) const
{
    if (index < 0 || index >= count())
    {
        return 0;
    }
    return item_object(m_array, m_item_size, index);
}

std::uintptr_t Objects::class_of(std::uintptr_t object)
{
    return object == 0 ? 0 : read<std::uintptr_t>(object + object_layout::kClass).value_or(0);
}

std::uintptr_t Objects::outer_of(std::uintptr_t object)
{
    return object == 0 ? 0 : read<std::uintptr_t>(object + object_layout::kOuter).value_or(0);
}

std::string Objects::name(std::uintptr_t object) const
{
    if (object == 0)
    {
        return "None";
    }
    const auto name = read<FName>(object + object_layout::kName);
    return name ? m_names.to_string(*name) : std::string{"<ilegivel>"};
}

std::string Objects::class_name(std::uintptr_t object) const
{
    return name(class_of(object));
}

std::string Objects::path(std::uintptr_t object) const
{
    // Outers do mais externo (o pacote) para dentro. O limite evita laço
    // infinito se um ponteiro de outer estiver corrompido.
    std::vector<std::uintptr_t> chain;
    for (auto cursor = object; cursor != 0 && chain.size() < 32; cursor = outer_of(cursor))
    {
        chain.push_back(cursor);
    }

    std::string out;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        if (!out.empty())
        {
            out.push_back('.');
        }
        out += name(*it);
    }
    return out;
}

std::string Objects::full_name(std::uintptr_t object) const
{
    return class_name(object) + " " + path(object);
}

std::uintptr_t Objects::find(std::string_view full) const
{
    // A comparação barata vem antes: o nome curto é o último segmento do
    // caminho, então só monta o caminho inteiro de quem já bate no nome. Um
    // pacote não tem ponto ("Package /Script/CoreUObject"): aí o corte é o
    // espaço depois da classe.
    const auto cut = full.find_last_of(". ");
    if (cut == std::string_view::npos || cut + 1 >= full.size())
    {
        return 0;
    }
    const auto short_name = full.substr(cut + 1);

    const auto total = m_array.count();
    for (std::int32_t i = 0; i < total; ++i)
    {
        const auto object = m_array.object(i);
        if (object == 0 || name(object) != short_name)
        {
            continue;
        }
        if (full_name(object) == full)
        {
            return object;
        }
    }
    return 0;
}
} // namespace e33::unreal
