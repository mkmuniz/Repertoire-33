#pragma once

#include <filesystem>
#include <memory>

#include "Core/ModController.hpp"
#include "UI/OverlayState.hpp"

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

    std::unique_ptr<ModController> m_mod;
    ui::OverlayState m_overlay{};
    std::filesystem::path m_mod_dir{};
    bool m_started{false};
};
} // namespace e33
