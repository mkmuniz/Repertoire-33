#pragma once

#include <cstddef>
#include <string>

namespace e33::ui
{
// Estado que é só da UI: o que está digitado na busca, o que está selecionado.
// Nada aqui é persistido no config — posição e tamanho da janela quem guarda é
// o imgui.ini, e a seleção não deve sobreviver a um reinício.
//
// As buscas são buffers fixos em vez de std::string para o overlay não depender
// do imgui_stdlib: menos uma tradução de ABI dentro de uma DLL injetada.
struct OverlayState
{
    static constexpr std::size_t kQueryCapacity = 96;
    static constexpr std::size_t kPathCapacity = 260;

    bool open{false};
    char boss_query[kQueryCapacity]{};
    char track_query[kQueryCapacity]{};
    char preset_path[kPathCapacity]{"preset.json"};
    std::string selected_encounter{};
    bool only_unassigned{false};
    bool confirming_clear_all{false};
};
} // namespace e33::ui
