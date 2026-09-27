#include "UI/Panels.hpp"

#include <imgui.h>

#include "UI/Theme.hpp"

namespace e33::ui
{
namespace
{
constexpr ImVec4 kAssigned = theme::color::kGoldBright;
constexpr ImVec4 kMuted = theme::color::kBoneDim;

// Uma linha da lista da esquerda. `assigned` vem do override bruto, não do
// resolvido: com o toggle global desligado o usuário ainda precisa ver o que
// configurou.
void draw_row(ModController& mod, OverlayState& state, const std::string& id,
              std::string_view name, std::string_view group)
{
    const auto assigned = mod.overrides().raw_track_for(id);
    if (state.only_unassigned && assigned)
    {
        return;
    }

    ImGui::PushID(id.c_str());
    const bool selected = state.selected_encounter == id;
    if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowOverlap))
    {
        state.selected_encounter = id;
    }
    ImGui::SameLine(6.0f);
    ImGui::TextUnformatted(name.data(), name.data() + name.size());

    if (!group.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "(%.*s)", static_cast<int>(group.size()), group.data());
    }

    if (assigned)
    {
        const auto track_name = mod.catalog().track_name(*assigned);
        ImGui::SameLine();
        ImGui::TextColored(kAssigned, "-> %.*s", static_cast<int>(track_name.size()),
                           track_name.data());

        // "Voltar ao original" por linha: o plano trata isso como requisito, não
        // como conveniencia.
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 26.0f);
        if (ImGui::SmallButton("x"))
        {
            mod.clear_override(id);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Voltar ao original");
        }
    }
    ImGui::PopID();
}
} // namespace

void draw_bosses_panel(ModController& mod, OverlayState& state)
{
    theme::heading("Encontros");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##boss_query", "buscar (sem acento serve)", state.boss_query,
                             sizeof(state.boss_query));
    theme::checkbox("So os sem override", &state.only_unassigned);

    ImGui::BeginChild("##boss_list");
    for (const auto* boss : mod.catalog().find_bosses(state.boss_query))
    {
        draw_row(mod, state, boss->id, boss->name, boss->group);
    }

    // Ids que o jogo reportou e o catalogo nao conhece. E assim que o M1 se
    // completa na pratica: jogue, e o que falta aparece aqui.
    const auto& unknown = mod.catalog().unknown_encounters();
    if (!unknown.empty())
    {
        ImGui::Separator();
        ImGui::TextColored(kMuted, "Vistos em jogo, fora do catalogo (%zu)", unknown.size());
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Adicione em data/bosses.json para ganhar nome legivel");
        }
        for (const auto& id : unknown)
        {
            draw_row(mod, state, id, id, "novo");
        }
    }
    ImGui::EndChild();
}
} // namespace e33::ui
