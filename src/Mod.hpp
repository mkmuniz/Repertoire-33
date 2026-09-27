#pragma once

#include <Mod/CppUserModBase.hpp>

#include "Config/Overrides.hpp"

namespace e33
{
// Janela ImGui do mod.
//
// TODO(M0, pré-requisito do plano): confirmar na doc da versão de UE4SS usada
// como um mod C++ registra uma janela ImGui PRÓPRIA renderizada sobre o jogo,
// em vez de uma aba na janela de debug do UE4SS (register_tab). Isso é
// configurável e mudou entre versões — todo o resto depende dessa resposta.
class BossMusicMod final : public RC::CppUserModBase
{
public:
    BossMusicMod();
    ~BossMusicMod() override;

    void on_unreal_init() override;
    void on_update() override;

private:
    void render_overlay();

    OverrideTable m_overrides{};
    bool m_overlay_open{false};
};
} // namespace e33
