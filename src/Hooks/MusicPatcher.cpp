#include "Hooks/MusicPatcher.hpp"

#include <algorithm>
#include <array>
#include <unordered_set>
#include <utility>

#include "Data/Catalog.hpp"
#include "Support/Log.hpp"

namespace e33
{
namespace
{
using unreal::FName;
using unreal::Objects;
using unreal::Properties;
using unreal::read;

// Classes-base do E33. Cada boss é uma subclasse (BP_EnemyWorld_Goblu_C...),
// por isso a classificação sobe pela hierarquia.
constexpr std::string_view kEnemyClass = "BP_jRPG_Enemy_World_Base_Seamless_C";
constexpr std::string_view kActionClass = "BP_GameAction_TriggerBattle_C";
constexpr std::string_view kManagerClass = "AC_jRPG_BattleManager_C";

// Objetos que não são instâncias vivas e completas: CDO, arquétipo, ainda
// carregando ou já sendo destruído. Escrever num CDO mudaria o padrão da classe
// inteira; escrever em algo carregando seria sobrescrito pelo loader.
constexpr std::size_t kObjectFlags = 0x08;
constexpr std::int32_t kSkipFlags = 0x10      // RF_ClassDefaultObject
                                    | 0x20    // RF_ArchetypeObject
                                    | 0x200   // RF_NeedInitialization
                                    | 0x400   // RF_NeedLoad
                                    | 0x1000  // RF_NeedPostLoad
                                    | 0x8000  // RF_BeginDestroyed
                                    | 0x10000; // RF_FinishDestroyed

// TSoftObjectPtr no UE 5.4: FWeakObjectPtr (8 bytes) + FSoftObjectPath
// { FTopLevelAssetPath { FName PackageName, FName AssetName }, FString SubPathString }.
constexpr std::size_t kSoftWeak = 0x00;
constexpr std::size_t kSoftPackage = 0x08;
constexpr std::size_t kSoftAsset = 0x10;
constexpr std::size_t kSoftSubPathNum = 0x20;
constexpr std::int32_t kSoftSize = 0x28;

constexpr int kChainLimit = 64;

std::string lowercase(std::string_view text)
{
    std::string out{text};
    std::ranges::transform(out, out.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return out;
}

std::string_view kind_label(int kind)
{
    constexpr std::array<std::string_view, 4> kLabels{"?", "inimigo", "acao de roteiro",
                                                      "battle manager"};
    return kLabels[static_cast<std::size_t>(kind)];
}
} // namespace

MusicPatcher::MusicPatcher(const Objects& objects, DesiredTrack desired, BattleStarted started)
    : m_objects{objects}
    , m_properties{objects}
    , m_desired{std::move(desired)}
    , m_started{std::move(started)}
{
}

void MusicPatcher::tick(std::size_t budget)
{
    scan(budget);

    std::vector<CombatStartEvent> started;
    for (auto it = m_sources.begin(); it != m_sources.end();)
    {
        auto& source = it->second;
        if (!alive(source))
        {
            if (source.object == m_current_battle)
            {
                m_current_battle = 0;
            }
            it = m_sources.erase(it);
            continue;
        }
        if (source.info->kind == Kind::Manager)
        {
            watch_battle(source, started);
        }
        else
        {
            update_music(source);
        }
        ++it;
    }

    // Fora do laço: o callback pode chamar apply_to_current_battle, que lê as
    // fontes.
    for (const auto& event : started)
    {
        m_started(event);
    }
}

void MusicPatcher::scan(std::size_t budget)
{
    const auto& array = m_objects.array();
    const auto total = array.count();
    if (total <= 0)
    {
        return;
    }

    for (std::size_t n = 0; n < budget; ++n)
    {
        if (m_cursor >= total)
        {
            m_cursor = 0;
            ++m_full_passes;
        }
        const auto index = m_cursor++;
        const auto object = array.object(index);
        if (object == 0 || m_sources.contains(object))
        {
            continue;
        }
        const auto flags = read<std::int32_t>(object + kObjectFlags);
        if (!flags || (*flags & kSkipFlags) != 0)
        {
            continue;
        }
        const auto klass = Objects::class_of(object);
        if (klass == 0)
        {
            continue;
        }
        const auto& info = class_info(klass);
        if (info.kind == Kind::None)
        {
            continue;
        }

        m_sources.emplace(object, Source{.object = object,
                                         .index = index,
                                         .klass = klass,
                                         .class_name = info.name,
                                         .info = &info});
        log::debug("fonte de musica: {} {}", kind_label(static_cast<int>(info.kind)),
                   m_objects.path(object));
    }
}

const MusicPatcher::ClassInfo& MusicPatcher::class_info(std::uintptr_t klass)
{
    // A chave é o ponteiro, mas uma classe descarregada pode ceder o endereço
    // a outra: o nome confirma que o cache ainda fala da mesma classe.
    const auto name = read<FName>(klass + unreal::object_layout::kName).value_or(FName{});
    const auto it = m_classes.find(klass);
    if (it != m_classes.end() && it->second.name == name)
    {
        return it->second;
    }
    auto& slot = m_classes[klass];
    slot = describe(klass, name);
    return slot;
}

MusicPatcher::ClassInfo MusicPatcher::describe(std::uintptr_t klass, FName name) const
{
    ClassInfo info;
    info.name = name;

    auto cursor = klass;
    for (int depth = 0; cursor != 0 && depth < kChainLimit && info.kind == Kind::None;
         ++depth, cursor = Properties::super_of(cursor))
    {
        const auto label = m_objects.name(cursor);
        if (label == kEnemyClass)
        {
            info.kind = Kind::Enemy;
        }
        else if (label == kActionClass)
        {
            info.kind = Kind::Action;
        }
        else if (label == kManagerClass)
        {
            info.kind = Kind::Manager;
        }
    }
    if (info.kind == Kind::None)
    {
        return info;
    }

    // Tudo por nome de campo, nunca por offset fixo: offsets mudam entre
    // patches, nomes de campo de Blueprint quase nunca.
    const auto expect = [&](std::optional<unreal::Property> property, std::string_view type,
                            std::int32_t size, std::string_view what) -> std::int32_t {
        if (!property || property->type != type || property->size != size)
        {
            log::warn("classe {}: campo {} nao confere ({}); fonte ignorada", m_objects.name(klass),
                      what,
                      property ? property->type + "/" + std::to_string(property->size)
                               : std::string{"ausente"});
            return -1;
        }
        return property->offset;
    };

    bool ok = true;
    const auto need = [&ok](std::int32_t offset) {
        ok = ok && offset >= 0;
        return offset;
    };

    switch (info.kind)
    {
    case Kind::Enemy:
    {
        info.music = need(expect(m_properties.find_path(klass, {"BattleMusicParameters", "Music"}),
                                 "SoftObjectProperty", kSoftSize, "BattleMusicParameters.Music"));
        const auto selected = m_properties.find(klass, "SelectedEncounter");
        info.handle_size = selected ? selected->size : 0;
        info.selected_row =
            need(expect(m_properties.find_path(klass, {"SelectedEncounter", "RowName"}),
                        "NameProperty", sizeof(FName), "SelectedEncounter.RowName"));
        info.row_in_handle = selected ? info.selected_row - selected->offset : 0;
        info.possible = need(expect(m_properties.find(klass, "PossibleEncounters"), "ArrayProperty",
                                    0x10, "PossibleEncounters"));
        break;
    }
    case Kind::Action:
        // Variável de Blueprint com espaços no nome. O SDK do Dumper-7 a mostra
        // como "Battle_Start_Params" porque troca espaço por '_'; o FName no
        // jogo é este, conferido por um SDK nativo que lê o campo ao vivo.
        info.music = need(
            expect(m_properties.find_path(klass, {"Battle Start Params", "MusicParameters", "Music"}),
                   "SoftObjectProperty", kSoftSize, "Battle Start Params.MusicParameters.Music"));
        info.encounter_row = need(expect(
            m_properties.find_path(klass, {"Battle Start Params", "EncounterRow", "RowName"}),
            "NameProperty", sizeof(FName), "Battle Start Params.EncounterRow.RowName"));
        break;
    case Kind::Manager:
        info.music = need(expect(
            m_properties.find_path(klass, {"CurrentBattleStartParams", "MusicParameters", "Music"}),
            "SoftObjectProperty", kSoftSize, "CurrentBattleStartParams.MusicParameters.Music"));
        info.encounter_row = need(expect(
            m_properties.find_path(klass, {"CurrentBattleStartParams", "EncounterRow", "RowName"}),
            "NameProperty", sizeof(FName), "CurrentBattleStartParams.EncounterRow.RowName"));
        info.encounter_name = need(expect(m_properties.find(klass, "EncounterName"), "NameProperty",
                                          sizeof(FName), "EncounterName"));
        info.loaded_music = need(expect(m_properties.find(klass, "LoadedBattleMusic"),
                                        "ObjectProperty", sizeof(std::uintptr_t),
                                        "LoadedBattleMusic"));
        break;
    case Kind::None:
        break;
    }

    if (!ok)
    {
        info.kind = Kind::None;
    }
    else
    {
        // Uma linha por subclasse (cada boss tem a sua): só no log verboso.
        log::debug("classe reconhecida: {} ({})", m_objects.name(klass),
                   kind_label(static_cast<int>(info.kind)));
    }
    return info;
}

bool MusicPatcher::alive(const Source& source) const
{
    // A tabela de objetos é a autoridade: o slot zera quando o objeto morre, e
    // a classe confirma que o endereço não foi reaproveitado por outro tipo.
    return m_objects.array().object(source.index) == source.object
           && Objects::class_of(source.object) == source.klass
           && read<FName>(source.klass + unreal::object_layout::kName) == source.class_name;
}

std::string MusicPatcher::encounter_of(const Source& source) const
{
    const auto& info = *source.info;
    const auto row_text = [this](std::uintptr_t at) -> std::string {
        const auto name = read<FName>(at);
        return name && name->comparison_index != 0 ? m_objects.names().to_string(*name)
                                                   : std::string{};
    };

    if (info.kind == Kind::Action)
    {
        return row_text(source.object + info.encounter_row);
    }

    // Inimigo: SelectedEncounter quando já foi sorteado; senão, só dá para
    // saber o encontro se houver um único possível — o caso dos bosses.
    if (auto selected = row_text(source.object + info.selected_row); !selected.empty())
    {
        return selected;
    }
    const auto data = read<std::uintptr_t>(source.object + info.possible);
    const auto count = read<std::int32_t>(source.object + info.possible + sizeof(std::uintptr_t));
    if (!data || !count || *data == 0 || *count != 1)
    {
        return {};
    }
    return row_text(*data + info.row_in_handle);
}

void MusicPatcher::update_music(Source& source)
{
    const auto encounter = encounter_of(source);
    if (encounter.empty())
    {
        return;
    }
    const auto at = source.object + source.info->music;
    const auto current = read_soft(at);
    if (!current)
    {
        return;
    }

    // O que está lá e não foi o mod que escreveu é o valor do jogo — inclusive
    // quando o jogo recarregou o objeto por cima de uma troca anterior.
    if (!source.written || *current != *source.written)
    {
        source.original = *current;
        source.written.reset();
    }
    const auto original_text = path_text(*source.original);
    m_original_by_encounter[encounter] = original_text;
    if (!source.original->empty())
    {
        // Toda faixa vista no jogo fica resolvível — inclusive uma que a busca
        // no pool tinha dado como ausente: o que o jogo usa é a prova.
        auto key = lowercase(original_text);
        if (const auto it = m_resolved.find(key); it == m_resolved.end() || !it->second)
        {
            m_resolved.insert_or_assign(std::move(key), *source.original);
            std::erase_if(m_missing, [&original_text](const std::string& missing) {
                return same_track_id(missing, original_text);
            });
        }
    }

    auto target = *source.original;
    if (const auto wanted = m_desired(encounter, original_text))
    {
        if (const auto resolved = resolve(*wanted))
        {
            target = *resolved;
        }
    }
    if (target == *current)
    {
        return;
    }
    if (!write_soft(at, target))
    {
        log::warn("{}: nao foi possivel gravar a faixa em {}", encounter, m_objects.path(source.object));
        return;
    }

    const bool restoring = target == *source.original;
    source.written = restoring ? std::nullopt : std::optional{target};
    log::info("{}: {} -> {} ({})", encounter, path_text(*current), path_text(target),
              restoring ? "original restaurada" : m_objects.name(source.object));
}

void MusicPatcher::watch_battle(Source& source, std::vector<CombatStartEvent>& started)
{
    const auto& info = *source.info;

    // EncounterName fica None fora de combate: a borda None -> X é o início.
    const auto name = read<FName>(source.object + info.encounter_name).value_or(FName{});
    if (!(name == source.last_encounter))
    {
        source.last_encounter = name;
        if (name.comparison_index != 0)
        {
            m_current_battle = source.object;

            // O id é o próprio EncounterName: é ele que acabou de mudar, então
            // é da luta atual. As fontes usam o RowName da linha do encontro,
            // que deveria ser o mesmo texto; se não for, o log diz.
            auto encounter = m_objects.names().to_string(name);
            if (const auto row = read<FName>(source.object + info.encounter_row);
                row && row->comparison_index != 0 && !(*row == name))
            {
                log::warn("EncounterName \"{}\" difere do RowName dos parametros \"{}\"",
                          encounter, m_objects.names().to_string(*row));
            }

            std::string original;
            if (const auto it = m_original_by_encounter.find(encounter);
                it != m_original_by_encounter.end())
            {
                original = it->second;
            }
            else if (const auto current = read_soft(source.object + info.music))
            {
                original = path_text(*current);
            }
            log::info("combate iniciado: {} (faixa original: {})", encounter,
                      original.empty() ? "nenhuma" : original);
            started.push_back({.encounter_id = std::move(encounter),
                               .original_track_id = std::move(original)});
        }
    }

    const auto loaded = read<std::uintptr_t>(source.object + info.loaded_music).value_or(0);
    if (loaded != source.last_loaded)
    {
        source.last_loaded = loaded;
        if (loaded != 0)
        {
            m_last_loaded = m_objects.path(loaded);
            log::info("musica da batalha carregada: {}", m_last_loaded);
        }
    }
}

bool MusicPatcher::apply_to_current_battle(std::string_view track_id)
{
    const auto it = m_sources.find(m_current_battle);
    if (it == m_sources.end() || !alive(it->second))
    {
        return false;
    }
    const auto target = resolve(track_id);
    if (!target)
    {
        return false;
    }
    const auto at = it->first + it->second.info->music;
    const auto current = read_soft(at);
    if (!current)
    {
        return false;
    }
    if (*current == *target)
    {
        return true;
    }
    if (!write_soft(at, *target))
    {
        return false;
    }
    log::info("troca tardia na batalha em curso: {} -> {}", path_text(*current), path_text(*target));
    return true;
}

std::optional<SoftPath> MusicPatcher::read_soft(std::uintptr_t at) const
{
    const auto package = read<FName>(at + kSoftPackage);
    const auto asset = read<FName>(at + kSoftAsset);
    const auto subpath = read<std::int32_t>(at + kSoftSubPathNum);
    // Subcaminho ("Pacote.Asset:Sub") não é forma de faixa de música; não mexer.
    if (!package || !asset || !subpath || *subpath != 0)
    {
        return std::nullopt;
    }
    return SoftPath{*package, *asset};
}

bool MusicPatcher::write_soft(std::uintptr_t at, const SoftPath& path)
{
    // O ponteiro fraco guarda o objeto já resolvido; zerado, o jogo resolve de
    // novo pelo caminho — que agora é o da faixa escolhida.
    constexpr std::uint64_t kNoObject = 0;
    return unreal::write(at + kSoftPackage, path.package) && unreal::write(at + kSoftAsset, path.asset)
           && unreal::write(at + kSoftWeak, kNoObject);
}

std::string MusicPatcher::path_text(const SoftPath& path) const
{
    if (path.empty())
    {
        return {};
    }
    return m_objects.names().to_string(path.package) + "." + m_objects.names().to_string(path.asset);
}

std::optional<SoftPath> MusicPatcher::resolve(std::string_view track_id)
{
    auto key = lowercase(track_id);
    if (const auto it = m_resolved.find(key); it != m_resolved.end())
    {
        return it->second;
    }
    prefetch({std::string{track_id}});
    return m_resolved[key];
}

void MusicPatcher::prefetch(const std::vector<std::string>& track_ids)
{
    struct Pending
    {
        std::string key;
        std::string id;
        unreal::SplitName package;
        unreal::SplitName asset;
    };
    std::vector<Pending> pending;
    std::vector<std::string_view> texts;
    std::unordered_set<std::string> queued;
    for (const auto& id : track_ids)
    {
        auto key = lowercase(id);
        if (m_resolved.contains(key) || !queued.insert(key).second)
        {
            continue;
        }
        const auto dot = id.rfind('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 == id.size())
        {
            m_resolved[key] = std::nullopt;
            m_missing.push_back(id);
            log::warn("faixa \"{}\" nao e um caminho de asset (Pacote.Asset)", id);
            continue;
        }
        pending.push_back({std::move(key), id, {}, {}});
    }
    // Os string_view apontam para `pending`, que não cresce mais.
    for (auto& item : pending)
    {
        const std::string_view id{item.id};
        const auto dot = id.rfind('.');
        item.package = unreal::split_name_number(id.substr(0, dot));
        item.asset = unreal::split_name_number(id.substr(dot + 1));
        texts.push_back(item.package.base);
        texts.push_back(item.asset.base);
    }
    if (pending.empty())
    {
        return;
    }

    const auto found = m_objects.names().find_entries(texts);
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        const auto& package = found[i * 2];
        const auto& asset = found[i * 2 + 1];
        if (package && asset)
        {
            m_resolved[pending[i].key] = SoftPath{FName{*package, pending[i].package.number},
                                                  FName{*asset, pending[i].asset.number}};
        }
        else
        {
            m_resolved[pending[i].key] = std::nullopt;
            m_missing.push_back(pending[i].id);
            log::warn("faixa \"{}\" nao existe no jogo (nome ausente do pool); fica a original",
                      pending[i].id);
        }
    }
}

MusicPatcher::Stats MusicPatcher::stats() const
{
    Stats stats;
    stats.full_passes = m_full_passes;
    for (const auto& [object, source] : m_sources)
    {
        switch (source.info->kind)
        {
        case Kind::Enemy:
            ++stats.enemies;
            break;
        case Kind::Action:
            ++stats.actions;
            break;
        case Kind::Manager:
            ++stats.managers;
            break;
        case Kind::None:
            break;
        }
        if (source.written)
        {
            ++stats.patched;
        }
    }
    return stats;
}
} // namespace e33
