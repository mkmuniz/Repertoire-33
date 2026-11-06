#include "ModHost.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <vector>

#include "Config/Settings.hpp"
#include "Hooks/AudioSwap.hpp"
#include "Platform/Overlay.hpp"
#include "Support/Log.hpp"
#include "UI/Panels.hpp"
#include "UI/Theme.hpp"
#include "Unreal/Memory.hpp"
#include "Unreal/Properties.hpp"

namespace e33
{
namespace
{
// O jogo leva alguns segundos para popular a tabela de objetos; cada tentativa
// varre a .data do executável, então não dá para tentar a cada tick.
constexpr double kAttachRetrySeconds = 2.0;
// Layout que não confere várias vezes seguidas é patch do jogo, não pressa.
constexpr int kLayoutFailureLimit = 5;
constexpr unsigned kStatusEveryTicks = 10;
constexpr unsigned kTimingEveryTicks = 600; // um minuto a 10 ticks/s

double now_seconds()
{
#if defined(_WIN32)
    return static_cast<double>(GetTickCount64()) / 1000.0;
#else
    return 0.0;
#endif
}
} // namespace

ModHost& ModHost::instance()
{
    static ModHost host;
    return host;
}

#if defined(_WIN32)
void ModHost::start(HMODULE module)
{
    if (m_started)
    {
        return;
    }
    m_started = true;

    // A pasta da DLL, não o diretório de trabalho: o diretório de trabalho de
    // um mod injetado é o do jogo.
    wchar_t buffer[MAX_PATH]{};
    const auto dll_dir = GetModuleFileNameW(module, buffer, MAX_PATH) != 0
                             ? std::filesystem::path{buffer}.parent_path()
                             : std::filesystem::current_path();

    // A DLL mora ao lado do executavel (e o que o proxy exige), mas os
    // arquivos do mod ficam numa subpasta com o nome dele. Isso evita despejar
    // config.json e data/ dentro da pasta do jogo, e evita que dois mods
    // instalados juntos disputem os mesmos nomes de arquivo.
    m_mod_dir = dll_dir / "Repertoire33";

    log::open_file(m_mod_dir / "BossMusicSwapper.log");
    log::info("carregando de {}", m_mod_dir.string());

    {
        const std::lock_guard lock{m_mutex};
        // A troca tardia precisa do patcher, que só existe depois que o jogo
        // abrir: o backend o consulta na hora, sob o mesmo mutex do tick.
        m_mod = std::make_unique<ModController>(
            std::make_unique<UnrealAudioBackend>([this](std::string_view track) {
                return m_patcher && m_patcher->apply_to_current_battle(track);
            }));
        m_mod->initialize(m_mod_dir);
        m_mod->watcher().set_status(false, "aguardando o jogo abrir a janela");
        m_overlay.open = m_mod->settings().start_open;
    }

    platform::Config config;
    config.mod_dir = m_mod_dir;
    config.on_first_frame = [this] { on_first_frame(); };
    config.on_render = [this] { on_render(); };
    config.on_game_tick = [this] { on_game_tick(); };

    if (!platform::install(std::move(config)))
    {
        log::warn("overlay nao instalado; a troca de musica continua ativa pelo config.json");
    }
}
#endif

void ModHost::stop()
{
    if (!m_started)
    {
        return;
    }
    m_started = false;
    platform::uninstall();

    // No fim do processo o Windows já matou as outras threads — inclusive uma
    // que estivesse com o mutex. Esperar por ele travaria o jogo ao fechar.
    std::unique_lock lock{m_mutex, std::try_to_lock};
    if (!lock.owns_lock())
    {
        log::warn("encerrando sem gravar: estado ocupado por outra thread");
        log::close_file();
        return;
    }
    m_patcher.reset();
    m_objects.reset();
    if (m_mod && m_mod->overrides().dirty())
    {
        static_cast<void>(m_mod->save_now());
    }
    m_mod.reset();
    log::close_file();
}

void ModHost::on_first_frame()
{
    // Aqui o contexto do ImGui existe e o atlas ainda não foi usado, que é a
    // única janela segura para adicionar fonte.
    ui::theme::apply_style();
    if (!ui::theme::load_fonts(m_mod_dir / "assets"))
    {
        log::warn("fonte propria nao carregada; nomes acentuados podem sair errados");
    }
}

void ModHost::on_render()
{
    const std::lock_guard lock{m_mutex};
    if (m_mod == nullptr)
    {
        return;
    }

    m_mod->tick(now_seconds());

    if (const auto vk = m_mod->settings().hotkey_virtual_key();
        vk && platform::key_pressed(*vk))
    {
        m_overlay.open = !m_overlay.open;
        log::debug("overlay {}", m_overlay.open ? "aberto" : "fechado");
    }

    // O jogo só perde o input enquanto a janela está aberta.
    platform::set_wants_input(m_overlay.open);

    ui::draw_overlay(*m_mod, m_overlay);
}

void ModHost::on_game_tick()
{
    const std::lock_guard lock{m_mutex};
    if (m_mod == nullptr)
    {
        return;
    }
    if (!m_patcher && !attach_to_game(now_seconds()))
    {
        return;
    }

    // O tick divide o quadro com o jogo: o custo fica medido, para que
    // "o jogo engasga" tenha um número no log verboso.
    const auto started = std::chrono::steady_clock::now();
    m_patcher->tick();
    const long long spent = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - started)
                           .count();
    m_tick_us_total += spent;
    m_tick_us_peak = std::max(m_tick_us_peak, spent);

    if (++m_ticks % kStatusEveryTicks == 0)
    {
        refresh_status();
    }
    if (m_ticks % kTimingEveryTicks == 0)
    {
        log::debug("tick da game thread: media {} us, pico {} us em {} ticks",
                   m_tick_us_total / kTimingEveryTicks, m_tick_us_peak, kTimingEveryTicks);
        m_tick_us_total = 0;
        m_tick_us_peak = 0;
    }
}

bool ModHost::attach_to_game(double now)
{
    if (m_attach_gave_up || now < m_next_attach)
    {
        return false;
    }
    m_next_attach = now + kAttachRetrySeconds;

    const auto give_up = [this](std::string reason) {
        log::warn("integracao com o jogo desligada: {}. O jogo segue vanilla.", reason);
        m_mod->watcher().set_status(false, std::move(reason));
        m_attach_gave_up = true;
        return false;
    };

    const auto image = unreal::main_image();
    if (!image)
    {
        return give_up("executavel sem as secoes .text/.data esperadas");
    }
    const auto names = unreal::NamePool::find(*image);
    if (!names)
    {
        m_mod->watcher().set_status(false, "procurando o pool de nomes do Unreal");
        return false;
    }
    const auto array = unreal::ObjectArray::find(*image);
    if (!array)
    {
        m_mod->watcher().set_status(false, "procurando a tabela de objetos do Unreal");
        return false;
    }

    auto objects = std::make_unique<unreal::Objects>(*array, *names);
    if (auto error = unreal::Properties{*objects}.validate_layout(); !error.empty())
    {
        if (++m_layout_failures >= kLayoutFailureLimit)
        {
            return give_up("layout do Unreal diferente do esperado: " + error);
        }
        m_mod->watcher().set_status(false, "conferindo o layout do Unreal: " + error);
        return false;
    }

    // RVAs no log: comparar com outro build do jogo diz na hora se um patch
    // mexeu no executável.
    log::info("Unreal encontrado: {} objetos (item de {} bytes), GUObjectArray+0x10 em "
              "RVA 0x{:X}, FNamePool em RVA 0x{:X}",
              array->count(), array->item_size(), array->address() - image->base,
              names->address() - image->base);

    m_objects = std::move(objects);
    m_patcher = std::make_unique<MusicPatcher>(
        *m_objects,
        [this](std::string_view encounter, std::string_view original) {
            return m_mod->music_for(encounter, original);
        },
        [this](const CombatStartEvent& event) { m_mod->watcher().notify(event); });

    // Uma varredura do pool de nomes agora, em vez de uma por faixa escolhida
    // no meio do jogo.
    std::vector<std::string> tracks;
    for (const auto& track : m_mod->catalog().tracks())
    {
        tracks.push_back(track.id);
    }
    for (const auto& [encounter, track] : m_mod->overrides().entries())
    {
        tracks.push_back(track);
    }
    m_patcher->prefetch(tracks);

    m_mod->watcher().set_status(true, "ativa");
    return true;
}

void ModHost::refresh_status()
{
    const auto stats = m_patcher->stats();
    auto detail = std::format("ativa: {} inimigo(s), {} acao(oes) de roteiro, {} battle "
                              "manager; {} troca(s) armada(s)",
                              stats.enemies, stats.actions, stats.managers, stats.patched);
    if (const auto missing = m_patcher->missing_tracks().size(); missing > 0)
    {
        detail += std::format("; {} faixa(s) pedida(s) nao existem no jogo", missing);
    }
    if (const auto& last = m_patcher->last_loaded_track(); !last.empty())
    {
        const auto name = m_mod->catalog().track_name(last);
        detail += std::format("; ultima musica de batalha: {}", name);
    }
    m_mod->watcher().set_status(true, std::move(detail));
}
} // namespace e33
