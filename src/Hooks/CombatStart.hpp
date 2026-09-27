#pragma once

#include <functional>
#include <string>

namespace e33
{
struct CombatStartEvent
{
    std::wstring encounter_id{};
};

namespace hooks
{
// M0 — o primeiro milestone: logar o id do encontro quando um combate começa.
// Referência de como hookar as classes de combate do E33: README do mod
// AutoParryAnim (mod UE4SS em C++ para o proprio jogo).
void install_combat_start_hook(std::function<void(const CombatStartEvent&)> on_start);
} // namespace hooks
} // namespace e33
