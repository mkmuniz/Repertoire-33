#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

#include "Core/ModController.hpp"
#include "Hooks/MusicPatcher.hpp"
#include "UI/OverlayState.hpp"
#include "Unreal/Objects.hpp"

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace e33
{
// Dona do estado do mod dentro do processo do jogo.
//
// Singleton porque o ponto de entrada de uma DLL não tem onde guardar estado:
// DllMain recebe só o módulo, e os callbacks do overlay são ponteiros de
// função sem contexto.
//
// Duas threads entram aqui: a do Present (UI) e a do jogo (tick, que lê e
// escreve objetos do Unreal). m_mutex é o que as separa.
class ModHost
{
public:
    static ModHost& instance();

#if defined(_WIN32)
    void start(HMODULE module);
#endif
    void stop();

private:
    void on_first_frame();
    void on_render();
    void on_game_tick();
    bool attach_to_game(double now);
    void refresh_status();

    std::mutex m_mutex;
    std::unique_ptr<ModController> m_mod;
    std::unique_ptr<unreal::Objects> m_objects;
    std::unique_ptr<MusicPatcher> m_patcher;
    ui::OverlayState m_overlay{};
    std::filesystem::path m_mod_dir{};
    bool m_started{false};

    double m_next_attach{0.0};
    int m_layout_failures{0};
    bool m_attach_gave_up{false};
    unsigned m_ticks{0};
};
} // namespace e33
