#pragma once

#include <functional>
#include <string_view>

#include "Core/AudioBackend.hpp"

namespace e33
{
// Backend real, chamado no início de cada combate com override.
//
// A troca de verdade acontece antes da luta, nos dados que o jogo lê
// (MusicPatcher). Aqui ela é confirmada nos parâmetros da batalha em curso —
// ou feita ali, tarde, quando a luta veio de uma fonte que a varredura não
// alcança. `apply` é essa troca tardia; o host a liga ao patcher.
class UnrealAudioBackend final : public IAudioBackend
{
public:
    using ApplyFn = std::function<bool(std::string_view track_id)>;

    explicit UnrealAudioBackend(ApplyFn apply);

    bool play_instead(std::string_view track_id) override;
    bool preview(std::string_view track_id) override;
    void stop_preview() override;

private:
    ApplyFn m_apply;
};
} // namespace e33
