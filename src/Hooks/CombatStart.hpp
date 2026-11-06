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
// Em jogo quem detecta é o MusicPatcher — a borda None -> encontro em
// AC_jRPG_BattleManager_C.EncounterName — e ele chama notify(). simulate() é o
// mesmo caminho, para validar toda a lógica acima sem entrar em combate: é o
// "comando de teste" que o plano pede no ambiente de desenvolvimento.
class CombatWatcher
{
public:
    void set_callback(CombatStartCallback callback);

    // Estado da integração com o jogo, para a UI. `detail` diz o que está
    // ativo ou o que falta — "não funcionou" tem de ter resposta sem pedir log.
    void set_status(bool installed, std::string detail);
    [[nodiscard]] bool installed() const { return m_installed; }
    [[nodiscard]] const std::string& detail() const { return m_detail; }

    void notify(const CombatStartEvent& event) const;

    // Dispara o pipeline como se o jogo tivesse iniciado um combate.
    void simulate(const CombatStartEvent& event) const;

private:
    CombatStartCallback m_callback{};
    bool m_installed{false};
    std::string m_detail{"integracao com o jogo nao iniciada"};
};
} // namespace e33
