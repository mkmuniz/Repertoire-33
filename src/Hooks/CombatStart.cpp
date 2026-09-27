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
#if defined(_WIN32)
    // TODO(M0 — só é possível com o jogo aberto): achar a função que dispara o
    // combate e o campo com o id do encontro, e chamar m_callback ali.
    //
    // Caminho recomendado pelo plano, nesta ordem:
    //   1. Gerar o SDK com o Dumper-7 e procurar nele a classe de combate: os
    //      nomes vêm do próprio jogo, então "jRPG", "Encounter" e "Battle" são
    //      os termos por onde começar.
    //   2. Cruzar o id com DT_jRPG_Encounters (a tabela que o randomizer
    //      sobrescreve) para confirmar que é o mesmo identificador.
    //   3. Gerar o SDK do jogo com o Dumper-7 (público): ele produz os headers
    //      C++ das classes do Unreal para ESTA versão do jogo, que é o papel
    //      que o UEPseudo teria. Com o SDK em mãos, o hook sai por endereço de
    //      função, com o safetyhook ou o MinHook que já está no projeto.
    //
    // Enquanto isso não existe, install() falha alto em vez de fingir sucesso:
    // um mod que diz "instalado" e não faz nada é pior que um que diz o que
    // falta.
    log::warn("hook de inicio de combate ainda nao implementado (M0); "
              "use a simulacao para testar a logica");
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
