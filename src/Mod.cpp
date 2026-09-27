#include "Mod.hpp"

#include <filesystem>

#include <imgui.h>

#include "Hooks/AudioSwap.hpp"
#include "Support/Log.hpp"
#include "UI/Panels.hpp"
#include "UI/Theme.hpp"

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace e33
{
namespace
{
// Pasta do mod dentro de ue4ss\Mods\BossMusicSwapper\, de onde saem config.json,
// settings.json e data/.
std::filesystem::path resolve_mod_dir()
{
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH]{};
    HMODULE self{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&resolve_mod_dir), &self)
        != 0)
    {
        if (GetModuleFileNameW(self, buffer, MAX_PATH) != 0)
        {
            // ...\BossMusicSwapper\dlls\main.dll -> ...\BossMusicSwapper
            return std::filesystem::path{buffer}.parent_path().parent_path();
        }
    }
#endif
    return std::filesystem::current_path();
}
} // namespace

BossMusicMod::BossMusicMod()
{
    ModName = STR("BossMusicSwapper");
    ModVersion = STR("0.1.0");
    ModDescription = STR("Troca a musica de boss em runtime, sem mexer nos .pak");
    ModAuthors = STR("mkmuniz");

    m_mod = std::make_unique<ModController>(std::make_unique<UnrealAudioBackend>());
    m_mod->initialize(resolve_mod_dir());
    m_overlay.open = m_mod->settings().start_open;

    // TODO(pre-requisito do plano, so verificavel no PC): confirmar na doc da
    // versao de UE4SS usada como um mod C++ registra uma janela ImGui PROPRIA
    // renderizada sobre o jogo. Isso mudou entre versoes e e configuravel.
    //
    // Duas formas, em ordem de preferencia:
    //   1. Overlay proprio: se a versao expoe um callback de render do overlay
    //      do jogo, e ele que deve chamar render(). E o comportamento que o
    //      README promete (hotkey abre uma janela sobre o jogo).
    //   2. Aba no debug window do UE4SS: register_tab() abaixo sempre funciona
    //      e serve de plano B, mas exige a janela separada do UE4SS aberta.
    //
    // Ate a resposta existir, register_tab garante que a UI e alcancavel.
    register_tab(STR("Boss Music"), [](CppUserModBase* self) {
        static_cast<BossMusicMod*>(self)->render();
    });
}

BossMusicMod::~BossMusicMod() = default;

void BossMusicMod::on_unreal_init()
{
    // O hook de combate so pode ser instalado depois que o Unreal subiu.
    if (!m_mod->watcher().install())
    {
        log::warn("mod carregado, mas sem hook de combate: nenhuma troca vai ocorrer "
                  "ate o M0 ser concluido");
    }
}

void BossMusicMod::on_update()
{
    poll_hotkey();

#if defined(_WIN32)
    m_mod->tick(static_cast<double>(GetTickCount64()) / 1000.0);
#else
    m_mod->tick(0.0);
#endif
}

void BossMusicMod::poll_hotkey()
{
#if defined(_WIN32)
    const auto vk = m_mod->settings().hotkey_virtual_key();
    if (!vk)
    {
        return;
    }
    // Borda de descida: segurar a tecla nao pode ficar abrindo e fechando a
    // janela a cada frame.
    const bool down = (GetAsyncKeyState(*vk) & 0x8000) != 0;
    if (down && !m_hotkey_was_down)
    {
        m_overlay.open = !m_overlay.open;
        log::debug("overlay {}", m_overlay.open ? "aberto" : "fechado");
    }
    m_hotkey_was_down = down;
#endif
}

void BossMusicMod::render()
{
    // A janela so captura mouse enquanto esta aberta; fechada, nao desenha nada
    // e o jogo nao fica preso.
    if (!m_overlay.open)
    {
        return;
    }

    ensure_theme();
    ui::draw_overlay(*m_mod, m_overlay);
}

void BossMusicMod::ensure_theme()
{
    if (m_theme_applied)
    {
        return;
    }
    m_theme_applied = true;

    // O estilo e barato e pode ser aplicado a qualquer momento.
    ui::theme::apply_style();

    // TODO(verificar no PC): carregar fonte adiciona ao atlas do ImGui, e o
    // atlas aqui e do UE4SS, nao nosso. Se o jogo travar ou a fonte nao
    // aparecer na primeira abertura do overlay, a chamada precisa migrar para
    // um callback do UE4SS que rode ANTES do NewFrame do quadro — nao de dentro
    // do desenho. O overlay funciona com a fonte padrao de qualquer forma; o
    // que se perde e o estilo e os acentos franceses.
    if (!ui::theme::load_fonts(m_mod->mod_dir() / "assets"))
    {
        log::warn("fonte propria nao carregada; o overlay usa a fonte padrao e "
                  "nomes como \"Sirene\" podem sair sem acento");
    }
}
} // namespace e33
