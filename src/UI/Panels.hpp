#pragma once

#include "Core/ModController.hpp"
#include "UI/OverlayState.hpp"

namespace e33::ui
{
// Janela principal: chrome, toggle global, status e abas.
void draw_overlay(ModController& mod, OverlayState& state);

// Cada painel desenha dentro do que o chamador já abriu.
void draw_bosses_panel(ModController& mod, OverlayState& state);
void draw_tracks_panel(ModController& mod, OverlayState& state);
void draw_diagnostics_panel(ModController& mod, OverlayState& state);
void draw_settings_panel(ModController& mod, OverlayState& state);
} // namespace e33::ui
