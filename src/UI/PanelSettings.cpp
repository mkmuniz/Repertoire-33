#include "UI/Panels.hpp"

#include <cstring>
#include <filesystem>

#include <imgui.h>

namespace e33::ui
{
void draw_settings_panel(ModController& mod, OverlayState& state)
{
    auto& settings = mod.settings();

    // Hotkey editavel desde o M1, porque conflito com outro mod e o problema
    // mais chato de diagnosticar: o overlay simplesmente nao abre.
    char hotkey[32]{};
    std::snprintf(hotkey, sizeof(hotkey), "%s", settings.hotkey.c_str());
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputText("Hotkey", hotkey, sizeof(hotkey)))
    {
        if (virtual_key_from_name(hotkey))
        {
            settings.hotkey = hotkey;
            static_cast<void>(settings.save());
        }
    }
    if (!settings.hotkey_virtual_key())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.55f, 1.0f}, "tecla nao reconhecida");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(evite J: e do Gramophone Everywhere)");

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderFloat("Escala da fonte", &settings.font_scale, Settings::kMinFontScale,
                           Settings::kMaxFontScale, "%.2fx"))
    {
        static_cast<void>(settings.save());
    }

    if (ImGui::Checkbox("Abrir o overlay ao iniciar", &settings.start_open))
    {
        static_cast<void>(settings.save());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Presets");
    ImGui::TextWrapped("O preset tem o mesmo formato do config.json, so com os overrides: "
                       "da para compartilhar uma trilha alternativa inteira como um arquivo.");

    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputText("##preset_path", state.preset_path, sizeof(state.preset_path));
    ImGui::SameLine();
    if (ImGui::Button("Exportar"))
    {
        static_cast<void>(mod.export_preset(std::filesystem::path{state.preset_path}));
    }
    ImGui::SameLine();
    if (ImGui::Button("Importar"))
    {
        // Merge, nao replace: importar um preset parcial nao deve apagar o que o
        // usuario ja tinha configurado por fora dele.
        static_cast<void>(mod.import_preset(std::filesystem::path{state.preset_path},
                                            /*replace=*/false));
    }

    ImGui::Separator();
    if (!state.confirming_clear_all)
    {
        if (ImGui::Button("Remover todos os overrides"))
        {
            state.confirming_clear_all = true;
        }
    }
    else
    {
        ImGui::TextColored(ImVec4{1.0f, 0.75f, 0.3f, 1.0f}, "Remover %zu override(s)?",
                           mod.overrides().size());
        ImGui::SameLine();
        if (ImGui::Button("Sim, remover"))
        {
            mod.clear_all_overrides();
            state.confirming_clear_all = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar"))
        {
            state.confirming_clear_all = false;
        }
    }
}
} // namespace e33::ui
