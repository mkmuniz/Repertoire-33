#include "Hooks/AudioSwap.hpp"

#include "Support/Log.hpp"

namespace e33
{
// TODO(M2 — só é possível com o jogo aberto): trocar o evento de áudio
// disparado no início do combate por outra faixa NATIVA do jogo.
//
// Faixa nativa e não arquivo externo, por três motivos que o plano registra:
// sem conversão, sem questão de licenciamento, e a trilha do E33 foi composta
// para suavizar nos períodos de recuperação e intensificar no clímax — um loop
// simples perde isso e soa pior que o original.
//
// A música de batalha não fica num lugar central: está espalhada, sobretudo
// nos mapas dos níveis. É por isso que os mods existentes editam metadados no
// .pak e ficam tudo-ou-nada; aqui a troca é no evento, em runtime.

bool UnrealAudioBackend::play_instead(std::string_view track_id)
{
    log::warn("troca de faixa nao implementada (M2): pediram \"{}\"", track_id);
    return false;
}

bool UnrealAudioBackend::preview(std::string_view track_id)
{
    log::warn("preview nao implementado (M4): pediram \"{}\"", track_id);
    return false;
}

void UnrealAudioBackend::stop_preview()
{
}
} // namespace e33
