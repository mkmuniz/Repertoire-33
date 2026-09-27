#include "UI/Panels.hpp"

#include <imgui.h>

namespace e33::ui
{
namespace
{
void apply_font_scale(float scale)
{
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 19200
    ImGui::GetStyle().FontScaleMain = scale;
#else
    ImGui::GetIO().FontGlobalScale = scale;
#endif
}
} // namespace

void draw_overlay(ModController& mod, OverlayState& state)
{
    if (!state.open)
    {
        return;
    }

    apply_font_scale(mod.settings().font_scale);

    // Sem flags de posição: arrastar, redimensionar e colapsar são do ImGui, e
    // a posição fica no imgui.ini entre sessões. Não reimplementar isso.
    //
    // TODO(M4, verificar no PC): os nomes do jogo são franceses e o overlay
    // depende do glyph range da fonte que o UE4SS carrega. Se "Sirène" sair
    // como "Sirne" ou com caixas, é isso — e a correção é registrar a fonte com
    // ImGuiIO::Fonts->GetGlyphRangesDefault() trocado por um range que inclua
    // Latin-1 Supplement. O harness nativo não pega esse problema porque a
    // fonte dele é outra.
    ImGui::SetNextWindowPos(ImVec2{40.0f, 40.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{740.0f, 460.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Boss Music Swapper", &state.open))
    {
        ImGui::End();
        return;
    }

    // O toggle global fica no topo porque é o que o usuário procura com pressa:
    // gravar um vídeo ou isolar um bug sem desinstalar o mod.
    bool enabled = mod.overrides().enabled();
    if (ImGui::Checkbox("Overrides ativos", &enabled))
    {
        mod.set_enabled(enabled);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu configurado(s))", mod.overrides().size());

    if (!enabled)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4{1.0f, 0.75f, 0.3f, 1.0f}, "— jogo vanilla");
    }

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150.0f);
    if (ImGui::Button("Gravar"))
    {
        static_cast<void>(mod.save_now());
    }
    ImGui::SameLine();
    if (ImGui::Button("Recarregar"))
    {
        static_cast<void>(mod.reload_now());
    }

    if (!mod.watcher().installed())
    {
        ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.55f, 1.0f},
                           "Hook de combate nao instalado: as trocas nao vao acontecer em jogo.");
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##tabs"))
    {
        if (ImGui::BeginTabItem("Bosses"))
        {
            const float available = ImGui::GetContentRegionAvail().y - 28.0f;
            if (ImGui::BeginChild("##left", ImVec2{ImGui::GetContentRegionAvail().x * 0.5f - 4.0f,
                                                   available},
                                  ImGuiChildFlags_Borders))
            {
                draw_bosses_panel(mod, state);
            }
            ImGui::EndChild();

            ImGui::SameLine();
            if (ImGui::BeginChild("##right", ImVec2{0.0f, available}, ImGuiChildFlags_Borders))
            {
                draw_tracks_panel(mod, state);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostico"))
        {
            draw_diagnostics_panel(mod, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Ajustes"))
        {
            draw_settings_panel(mod, state);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s", mod.status_line().c_str());

    ImGui::End();
}
} // namespace e33::ui
