#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace e33
{
struct BossEntry
{
    std::string id{};       // id do encontro, como o jogo reporta
    std::string name{};     // nome legível: "Sirène"
    std::string group{};    // agrupamento na UI: ato, região, "opcional"
};

struct TrackEntry
{
    std::string id{};
    std::string name{};
    std::string source{};   // de onde a faixa vem, para dar contexto na lista
    bool dynamic{true};     // faixa nativa preserva a dinâmica de intensidade
};

// Catálogo lido de data/bosses.json e data/tracks.json (M1).
//
// O catálogo é conveniência de UI, nunca autoridade: um encontro que o jogo
// reporta e não está no arquivo continua utilizável, aparece com o id crú e
// entra na lista de "vistos em jogo" — que é justamente como se descobrem os
// ids que faltam.
class Catalog
{
public:
    bool load_bosses(const std::filesystem::path& path);
    bool load_tracks(const std::filesystem::path& path);

    [[nodiscard]] const std::vector<BossEntry>& bosses() const { return m_bosses; }
    [[nodiscard]] const std::vector<TrackEntry>& tracks() const { return m_tracks; }

    [[nodiscard]] std::string_view boss_name(std::string_view id) const;
    [[nodiscard]] std::string_view track_name(std::string_view id) const;

    [[nodiscard]] bool knows_boss(std::string_view id) const;
    [[nodiscard]] bool knows_track(std::string_view id) const;

    // Chamado pelo hook a cada combate: registra ids que o catálogo não
    // conhece para a UI listar. É o caminho prático do M1 — jogue, e os ids
    // que faltam aparecem no overlay em vez de só no log.
    bool note_seen_encounter(std::string_view id);
    [[nodiscard]] const std::vector<std::string>& unknown_encounters() const
    {
        return m_unknown_encounters;
    }

    [[nodiscard]] std::vector<const BossEntry*> find_bosses(std::string_view query) const;
    [[nodiscard]] std::vector<const TrackEntry*> find_tracks(std::string_view query) const;

private:
    std::vector<BossEntry> m_bosses{};
    std::vector<TrackEntry> m_tracks{};
    std::vector<std::string> m_unknown_encounters{};
};

// Normaliza para busca: minúsculas, sem acento, sem espaço nem pontuação.
// "Sirène" e "sirene" têm de bater, e "une vie" tem de achar "Une Vie à T'aimer".
[[nodiscard]] std::string fold_for_search(std::string_view text);

// true se todos os termos de `query` aparecem em `haystack` (em qualquer ordem).
[[nodiscard]] bool matches_query(std::string_view haystack, std::string_view query);
} // namespace e33
