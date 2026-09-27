#include "Mod.hpp"

#include <DynamicOutput/DynamicOutput.hpp>

#include "Hooks/CombatStart.hpp"

using namespace RC;

namespace e33
{
BossMusicMod::BossMusicMod()
{
    ModName = STR("BossMusicSwapper");
    ModVersion = STR("0.1.0");
    ModDescription = STR("Troca a musica de boss em runtime, sem mexer nos .pak");
    ModAuthors = STR("mkmuniz");

    // A config nasce vazia: sem override, o jogo continua 100% vanilla.
    m_overrides.load_or_create("config.json");
}

BossMusicMod::~BossMusicMod() = default;

void BossMusicMod::on_unreal_init()
{
    hooks::install_combat_start_hook([this](const CombatStartEvent& ev) {
        Output::send<LogLevel::Verbose>(STR("[BossMusic] combate iniciado: {}\n"), ev.encounter_id);
        // M2: resolver o override e trocar o evento de audio aqui.
    });
}

void BossMusicMod::on_update()
{
    // M4: hotkey (F9 por padrao) abre/fecha o overlay; captura o mouse só
    // enquanto a janela está aberta.
    if (m_overlay_open)
    {
        render_overlay();
    }
}

void BossMusicMod::render_overlay()
{
    // M4: bosses a esquerda, faixas a direita, busca, preview,
    // "voltar ao original" por linha e o toggle global no topo.
}
} // namespace e33
