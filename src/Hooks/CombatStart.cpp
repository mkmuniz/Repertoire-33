#include "Hooks/CombatStart.hpp"

namespace e33::hooks
{
void install_combat_start_hook(std::function<void(const CombatStartEvent&)> on_start)
{
    // TODO(M0): descobrir a classe/funcao que dispara o combate.
    // Prototipar em Lua primeiro (recarrega sem fechar o jogo), depois portar.
    (void)on_start;
}
} // namespace e33::hooks
