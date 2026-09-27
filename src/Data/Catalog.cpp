#include "Data/Catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ranges>

#include "Support/Json.hpp"
#include "Support/Log.hpp"

namespace e33
{
namespace
{
// Latin-1 Supplement e Latin Extended-A em UTF-8 começam com 0xC3/0xC4/0xC5.
// Mapear só o que aparece em nomes franceses do jogo cobre o caso real sem
// arrastar ICU para dentro de uma DLL de mod.
char fold_two_byte(unsigned char lead, unsigned char trail)
{
    if (lead == 0xC3)
    {
        switch (trail)
        {
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: // ÀÁÂÃÄÅ
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: // àáâãäå
            return 'a';
        case 0x87: case 0xA7: // Çç
            return 'c';
        case 0x88: case 0x89: case 0x8A: case 0x8B: // ÈÉÊË
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: // èéêë
            return 'e';
        case 0x8C: case 0x8D: case 0x8E: case 0x8F: // ÌÍÎÏ
        case 0xAC: case 0xAD: case 0xAE: case 0xAF: // ìíîï
            return 'i';
        case 0x91: case 0xB1: // Ññ
            return 'n';
        case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: // ÒÓÔÕÖ
        case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: // òóôõö
            return 'o';
        case 0x99: case 0x9A: case 0x9B: case 0x9C: // ÙÚÛÜ
        case 0xB9: case 0xBA: case 0xBB: case 0xBC: // ùúûü
            return 'u';
        case 0x9D: case 0xBD: case 0xBF: // Ýýÿ
            return 'y';
        default:
            return '\0';
        }
    }
    if (lead == 0xC5 && (trail == 0x92 || trail == 0x93)) // Œœ
    {
        return 'o';
    }
    if (lead == 0xC3 && trail == 0x9F) // ß
    {
        return 's';
    }
    return '\0';
}

std::vector<std::string> split_terms(std::string_view query)
{
    std::vector<std::string> terms;
    std::string current;
    for (const char c : query)
    {
        if (c == ' ' || c == '\t')
        {
            if (!current.empty())
            {
                terms.push_back(fold_for_search(current));
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty())
    {
        terms.push_back(fold_for_search(current));
    }
    std::erase_if(terms, [](const std::string& t) { return t.empty(); });
    return terms;
}

std::string string_field(const Json& obj, std::string_view key)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}
} // namespace

std::string fold_for_search(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80)
        {
            if (std::isalnum(c) != 0)
            {
                out.push_back(static_cast<char>(std::tolower(c)));
            }
            continue;
        }
        if (i + 1 < text.size())
        {
            if (const char folded = fold_two_byte(c, static_cast<unsigned char>(text[i + 1]));
                folded != '\0')
            {
                out.push_back(folded);
                ++i;
                continue;
            }
        }
        // Byte multi-byte que não sabemos dobrar: preserva, para que uma busca
        // colando o próprio nome ainda funcione.
        out.push_back(static_cast<char>(c));
    }
    return out;
}

bool matches_query(std::string_view haystack, std::string_view query)
{
    const auto terms = split_terms(query);
    if (terms.empty())
    {
        return true; // busca vazia lista tudo
    }
    const auto folded = fold_for_search(haystack);
    return std::ranges::all_of(terms, [&folded](const std::string& term) {
        return folded.find(term) != std::string::npos;
    });
}

bool Catalog::load_bosses(const std::filesystem::path& path)
{
    const auto text = read_text_file(path);
    if (!text)
    {
        log::warn("bosses.json nao encontrado em {}; a lista vai mostrar ids cruos",
                  path.string());
        return false;
    }
    const auto parsed = parse_json(*text);
    if (!parsed || !parsed->contains("bosses") || !parsed->at("bosses").is_array())
    {
        log::warn("bosses.json malformado em {}", path.string());
        return false;
    }

    m_bosses.clear();
    for (const auto& item : parsed->at("bosses"))
    {
        if (!item.is_object())
        {
            continue;
        }
        BossEntry entry{
            .id = string_field(item, "id"),
            .name = string_field(item, "name"),
            .group = string_field(item, "group"),
        };
        if (entry.id.empty())
        {
            continue;
        }
        if (entry.name.empty())
        {
            entry.name = entry.id;
        }
        m_bosses.push_back(std::move(entry));
    }
    log::info("catalogo: {} encontros", m_bosses.size());
    return true;
}

bool Catalog::load_tracks(const std::filesystem::path& path)
{
    const auto text = read_text_file(path);
    if (!text)
    {
        log::warn("tracks.json nao encontrado em {}", path.string());
        return false;
    }
    const auto parsed = parse_json(*text);
    if (!parsed || !parsed->contains("tracks") || !parsed->at("tracks").is_array())
    {
        log::warn("tracks.json malformado em {}", path.string());
        return false;
    }

    m_tracks.clear();
    for (const auto& item : parsed->at("tracks"))
    {
        if (!item.is_object())
        {
            continue;
        }
        TrackEntry entry{
            .id = string_field(item, "id"),
            .name = string_field(item, "name"),
            .source = string_field(item, "source"),
            .dynamic = true,
        };
        if (const auto it = item.find("dynamic"); it != item.end() && it->is_boolean())
        {
            entry.dynamic = it->get<bool>();
        }
        if (entry.id.empty())
        {
            continue;
        }
        if (entry.name.empty())
        {
            entry.name = entry.id;
        }
        m_tracks.push_back(std::move(entry));
    }
    log::info("catalogo: {} faixas", m_tracks.size());
    return true;
}

std::string_view Catalog::boss_name(std::string_view id) const
{
    const auto it = std::ranges::find_if(m_bosses, [id](const BossEntry& e) { return e.id == id; });
    return it == m_bosses.end() ? id : std::string_view{it->name};
}

std::string_view Catalog::track_name(std::string_view id) const
{
    const auto it = std::ranges::find_if(m_tracks, [id](const TrackEntry& e) { return e.id == id; });
    return it == m_tracks.end() ? id : std::string_view{it->name};
}

bool Catalog::knows_boss(std::string_view id) const
{
    return std::ranges::any_of(m_bosses, [id](const BossEntry& e) { return e.id == id; });
}

bool Catalog::knows_track(std::string_view id) const
{
    return std::ranges::any_of(m_tracks, [id](const TrackEntry& e) { return e.id == id; });
}

bool Catalog::note_seen_encounter(std::string_view id)
{
    if (id.empty() || knows_boss(id))
    {
        return false;
    }
    if (std::ranges::find(m_unknown_encounters, id) != m_unknown_encounters.end())
    {
        return false;
    }
    m_unknown_encounters.emplace_back(id);
    log::info("encontro desconhecido \"{}\": adicione em data/bosses.json", id);
    return true;
}

std::vector<const BossEntry*> Catalog::find_bosses(std::string_view query) const
{
    std::vector<const BossEntry*> out;
    for (const auto& entry : m_bosses)
    {
        if (matches_query(entry.name, query) || matches_query(entry.id, query)
            || matches_query(entry.group, query))
        {
            out.push_back(&entry);
        }
    }
    return out;
}

std::vector<const TrackEntry*> Catalog::find_tracks(std::string_view query) const
{
    std::vector<const TrackEntry*> out;
    for (const auto& entry : m_tracks)
    {
        if (matches_query(entry.name, query) || matches_query(entry.id, query)
            || matches_query(entry.source, query))
        {
            out.push_back(&entry);
        }
    }
    return out;
}
} // namespace e33
