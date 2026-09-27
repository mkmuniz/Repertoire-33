#pragma once

#include "Core/AudioBackend.hpp"

namespace e33
{
// Backend real: a única classe do mod que fala com o áudio do Unreal.
// Quando um patch do jogo quebrar a troca, o estrago está aqui.
class UnrealAudioBackend final : public IAudioBackend
{
public:
    bool play_instead(std::string_view track_id) override;
    bool preview(std::string_view track_id) override;
    void stop_preview() override;
};
} // namespace e33
