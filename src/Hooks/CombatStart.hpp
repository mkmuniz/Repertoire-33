#pragma once

#include <functional>
#include <string>

namespace e33
{
struct CombatStartEvent
{
    std::string encounter_id{};       // id do encontro, como o jogo reporta
    std::string original_track_id{};  // faixa que ia tocar sem o mod
};

using CombatStartCallback = std::function<void(const CombatStartEvent&)>;

// Observa o início de combate.
//
// M0 — o primeiro milestone do projeto: logar o id do encontro. A instalação do
// hook é a única parte que precisa do jogo; `simulate()` existe para validar
// toda a lógica acima sem entrar em combate de verdade, que é o "comando de
// teste" que o plano pede no ambiente de desenvolvimento.
class CombatWatcher
{
public:
    void set_callback(CombatStartCallback callback);

    // Instala o hook de verdade. No-op fora do Windows.
    // Devolve false se não achou o alvo do hook — e aí o log diz o que faltou.
    bool install();
    [[nodiscard]] bool installed() const { return m_installed; }

    // Dispara o pipeline como se o jogo tivesse iniciado um combate.
    void simulate(const CombatStartEvent& event) const;

private:
    CombatStartCallback m_callback{};
    bool m_installed{false};
};
} // namespace e33
