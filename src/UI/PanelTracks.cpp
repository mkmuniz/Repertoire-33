#include "UI/Panels.hpp"

#include <imgui.h>

#include "UI/Theme.hpp"

namespace e33::ui
{
void draw_tracks_panel(ModController& mod, OverlayState& state)
{
    if (state.selected_encounter.empty())
    {
        theme::heading("Faixas");
        ImGui::TextWrapped("Escolha um encontro a esquerda para atribuir uma faixa.");
        return;
    }

    const auto boss_name = mod.catalog().boss_name(state.selected_encounter);
    theme::heading("Faixa para");
    ImGui::SameLine();
    ImGui::TextUnformatted(boss_name.data(), boss_name.data() + boss_name.size());

    const auto current = mod.overrides().raw_track_for(state.selected_encounter);
    if (current)
    {
        const auto name = mod.catalog().track_name(*current);
        ImGui::TextColored(theme::color::kVerdigris, "atual: %.*s",
                           static_cast<int>(name.size()), name.data());
        ImGui::SameLine();
        if (ImGui::SmallButton("Voltar ao original"))
        {
            mod.clear_override(state.selected_encounter);
        }
    }
    else
    {
        ImGui::TextDisabled("atual: faixa original do jogo");
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##track_query", "buscar faixa", state.track_query,
                             sizeof(state.track_query));

    ImGui::BeginChild("##track_list");
    for (const auto* track : mod.catalog().find_tracks(state.track_query))
    {
        ImGui::PushID(track->id.c_str());
        const bool is_current = current && *current == track->id;
        if (ImGui::Selectable("##track", is_current, ImGuiSelectableFlags_AllowOverlap))
        {
            mod.set_override(state.selected_encounter, track->id);
        }
        ImGui::SameLine(6.0f);
        ImGui::TextUnformatted(track->name.c_str());

        // Nome longo nao pode empurrar o botao nem passar por baixo dele. Em vez
        // de quebrar a linha, os marcadores secundarios so aparecem se couberem:
        // eles sao contexto, o nome e o botao sao o essencial.
        const float button_x = ImGui::GetContentRegionMax().x - 62.0f;
        const auto cursor_after = [] {
            return ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
        };

        if (!track->dynamic)
        {
            // A trilha do E33 suaviza na recuperacao e intensifica no climax.
            // Uma faixa sem essa estrutura precisa vir marcada, senao o usuario
            // acha que o mod piorou o audio.
            constexpr const char* kFlat = "[loop simples]";
            if (cursor_after() + ImGui::CalcTextSize(kFlat).x + 16.0f < button_x)
            {
                ImGui::SameLine();
                ImGui::TextColored(theme::color::kGold, "%s", kFlat);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Sem a dinamica de intensidade da trilha original");
                }
            }
        }
        if (!track->source.empty())
        {
            const auto width = ImGui::CalcTextSize(track->source.c_str()).x;
            if (cursor_after() + width + 24.0f < button_x)
            {
                ImGui::SameLine();
                ImGui::TextColored(theme::color::kBoneDim, "(%s)", track->source.c_str());
            }
        }

        ImGui::SameLine(button_x);
        if (ImGui::SmallButton("Ouvir"))
        {
            static_cast<void>(mod.preview(track->id));
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}
} // namespace e33::ui
