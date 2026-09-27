#include "UI/Panels.hpp"

#include <imgui.h>

#include "UI/Theme.hpp"

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

// Sublinhado dourado sob a aba ativa. O ImGui marca aba por retângulo
// preenchido, que parece software; a marca por fio é o que a UI do jogo faz.
void underline_active_tab()
{
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2{min.x, max.y}, ImVec2{max.x, max.y},
        ImGui::ColorConvertFloat4ToU32(theme::color::kGold), 1.5f);
}

bool tab(const char* label)
{
    const bool open = ImGui::BeginTabItem(label);
    if (open)
    {
        underline_active_tab();
    }
    return open;
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
    // a posição fica no imgui.ini entre sessões.
    ImGui::SetNextWindowPos(ImVec2{20.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{780.0f, 520.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("REPERTOIRE", &state.open))
    {
        ImGui::End();
        return;
    }

    theme::window_ornaments();

    // A barra de título já é a placa gravada com o nome; repetir o nome no
    // corpo é ornamento redundante. Fica só a legenda.
    theme::push_small_font();
    theme::text_dim("substituicao de trilha em tempo de execucao");
    theme::pop_font();

    theme::rule();

    // O toggle global no topo: é o que o usuário procura com pressa, para
    // gravar vídeo ou isolar um bug sem desinstalar.
    bool enabled = mod.overrides().enabled();
    if (theme::checkbox("Overrides ativos", &enabled))
    {
        mod.set_enabled(enabled);
    }
    ImGui::SameLine();
    theme::push_small_font();
    if (enabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kBoneDim);
        ImGui::Text("%zu configurado(s)", mod.overrides().size());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kGold);
        ImGui::TextUnformatted("jogo vanilla");
        ImGui::PopStyleColor();
    }
    theme::pop_font();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 170.0f);
    if (theme::button("Gravar"))
    {
        static_cast<void>(mod.save_now());
    }
    ImGui::SameLine();
    if (theme::button("Recarregar"))
    {
        static_cast<void>(mod.reload_now());
    }

    if (!mod.watcher().installed())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::kBlood);
        ImGui::TextUnformatted(
            "Hook de combate nao instalado: nenhuma troca vai ocorrer em jogo.");
        ImGui::PopStyleColor();
    }

    theme::rule();

    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_NoTooltip))
    {
        if (tab("Encontros"))
        {
            const float available = ImGui::GetContentRegionAvail().y - 30.0f;
            const float half = ImGui::GetContentRegionAvail().x * 0.5f - 6.0f;
            if (ImGui::BeginChild("##left", ImVec2{half, available}, ImGuiChildFlags_Borders))
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
        if (tab("Diagnostico"))
        {
            draw_diagnostics_panel(mod, state);
            ImGui::EndTabItem();
        }
        if (tab("Ajustes"))
        {
            draw_settings_panel(mod, state);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    theme::rule();
    theme::push_small_font();
    theme::text_dim(mod.status_line());
    theme::pop_font();

    ImGui::End();
}
} // namespace e33::ui
