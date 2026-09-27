#include "UI/Panels.hpp"

#include <imgui.h>

#include "UI/Theme.hpp"

namespace e33::ui
{
void draw_diagnostics_panel(ModController& mod, OverlayState& state)
{
    // Este painel existe por um motivo especifico: quando alguem disser "nao
    // funcionou no boss X", a resposta tem de estar aqui, sem pedir log.
    bool verbose = mod.settings().verbose_log;
    if (theme::checkbox("Log verboso", &verbose))
    {
        mod.set_verbose(verbose);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu troca(s) aplicada(s) nesta sessao",
                        mod.swapper().swaps_applied());

    ImGui::TextUnformatted(mod.watcher().installed()
                               ? "Hook de combate: instalado"
                               : "Hook de combate: NAO instalado (M0 pendente)");

    ImGui::Separator();
    theme::heading("Combates recentes");

    if (mod.swapper().history().empty())
    {
        ImGui::TextDisabled("Nenhum combate observado ainda.");
    }
    else if (ImGui::BeginTable("##history", 4,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                                   | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Encontro");
        ImGui::TableSetupColumn("Original");
        ImGui::TableSetupColumn("Tocou");
        ImGui::TableSetupColumn("Motivo");
        ImGui::TableHeadersRow();

        for (const auto& entry : mod.swapper().history())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.encounter_name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.original_track.c_str());
            ImGui::TableNextColumn();
            const bool swapped = entry.reason == SwapReason::Applied;
            ImGui::TextColored(swapped ? theme::color::kVerdigris
                                       : theme::color::kBoneDim,
                               "%s", swapped ? entry.chosen_track.c_str()
                                             : entry.original_track.c_str());
            ImGui::TableNextColumn();
            const auto reason = describe(entry.reason);
            ImGui::TextUnformatted(reason.data(), reason.data() + reason.size());
        }
        ImGui::EndTable();
    }

    static_cast<void>(state);
}
} // namespace e33::ui
