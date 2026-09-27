#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace e33
{
// Fronteira com o áudio do jogo. A implementação real vive em Hooks/ e é a
// única parte que conhece o Unreal; tudo acima disso é testável sem o jogo.
class IAudioBackend
{
public:
    virtual ~IAudioBackend() = default;

    // Toca `track_id` no lugar da faixa que ia tocar no combate atual.
    virtual bool play_instead(std::string_view track_id) = 0;

    // Preview no menu (M4): toca a faixa fora de combate, para o usuário
    // ouvir antes de confirmar a escolha.
    virtual bool preview(std::string_view track_id) = 0;
    virtual void stop_preview() = 0;
};

// Backend de teste e do harness nativo: registra as chamadas em vez de tocar.
class RecordingAudioBackend final : public IAudioBackend
{
public:
    bool play_instead(std::string_view track_id) override
    {
        m_played.emplace_back(track_id);
        return true;
    }

    bool preview(std::string_view track_id) override
    {
        m_previewing = std::string{track_id};
        return true;
    }

    void stop_preview() override { m_previewing.clear(); }

    [[nodiscard]] const std::vector<std::string>& played() const { return m_played; }
    [[nodiscard]] const std::string& previewing() const { return m_previewing; }
    void clear() { m_played.clear(); m_previewing.clear(); }

private:
    std::vector<std::string> m_played{};
    std::string m_previewing{};
};
} // namespace e33
