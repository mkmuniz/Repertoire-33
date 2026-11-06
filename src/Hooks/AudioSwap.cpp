#include "Hooks/AudioSwap.hpp"

#include <utility>

#include "Support/Log.hpp"

namespace e33
{
UnrealAudioBackend::UnrealAudioBackend(ApplyFn apply)
    : m_apply{std::move(apply)}
{
}

bool UnrealAudioBackend::play_instead(std::string_view track_id)
{
    return m_apply && m_apply(track_id);
}

bool UnrealAudioBackend::preview(std::string_view track_id)
{
    // Tocar fora de combate exige carregar o asset e criar um AudioComponent
    // pelo engine; a troca em batalha não precisa de nada disso.
    log::warn("preview ainda nao implementado: pediram \"{}\"", track_id);
    return false;
}

void UnrealAudioBackend::stop_preview()
{
}
} // namespace e33
