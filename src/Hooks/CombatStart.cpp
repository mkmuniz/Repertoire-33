#include "Hooks/CombatStart.hpp"

#include "Support/Log.hpp"

namespace e33
{
void CombatWatcher::set_callback(CombatStartCallback callback)
{
    m_callback = std::move(callback);
}

bool CombatWatcher::install()
{
#if defined(E33_WITH_UE4SS)
    // TODO(M0 — só é possível com o jogo aberto): achar a função que dispara o
    // combate e o campo com o id do encontro, e chamar m_callback ali.
    //
    // Caminho recomendado pelo plano, nesta ordem:
    //   1. Protótipo em Lua (recarrega sem fechar o jogo) para achar a classe:
    //      RegisterHook em candidatos de BP_/jRPG_ e imprimir o nome da classe
    //      e as propriedades do objeto no console do UE4SS.
    //   2. Cruzar o id com DT_jRPG_Encounters (a tabela que o randomizer
    //      sobrescreve) para confirmar que é o mesmo identificador.
    //   3. Referência de um hook de combate já funcionando neste jogo: o README
    //      do mod AutoParryAnim, que é UE4SS em C++ para o proprio E33.
    //
    // Enquanto isso não existe, install() falha alto em vez de fingir sucesso:
    // um mod que diz "instalado" e não faz nada é pior que um que diz o que
    // falta.
    log::warn("hook de inicio de combate ainda nao implementado (M0); "
              "use o comando de simulacao para testar a logica");
    m_installed = false;
    return false;
#else
    // Build nativo (testes e harness): não há jogo para hookar, e simulate()
    // é o caminho normal.
    m_installed = false;
    return false;
#endif
}

void CombatWatcher::simulate(const CombatStartEvent& event) const
{
    if (!m_callback)
    {
        log::warn("simulate() sem callback registrado");
        return;
    }
    log::debug("simulando combate: encounter={} faixa_original={}", event.encounter_id,
               event.original_track_id);
    m_callback(event);
}
} // namespace e33
