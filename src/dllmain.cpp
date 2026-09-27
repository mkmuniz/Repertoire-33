// Entrada da DLL.
//
// Carregada de duas formas, e as duas funcionam com o mesmo binário:
//
//   1. Proxy: renomeie para dinput8.dll e ponha ao lado do executável do jogo.
//      O Windows carrega essa DLL ao invés da do sistema, e o export abaixo
//      repassa a chamada para a verdadeira. O UE4SS usa dwmapi.dll, então os
//      dois convivem.
//   2. Injetor: qualquer injetor de DLL. DllMain faz o resto.

#include "ModHost.hpp"

#if defined(_WIN32)

#include <Windows.h>

namespace
{
HMODULE g_system_dinput8{nullptr};

DWORD WINAPI bootstrap(LPVOID module)
{
    // Fora do loader lock: DllMain não pode carregar outras DLLs nem criar
    // dispositivos D3D, e a instalação dos hooks faz as duas coisas.
    e33::ModHost::instance().start(static_cast<HMODULE>(module));
    return 0;
}
} // namespace

// Repasse do proxy. Carregado sob demanda para que a DLL do sistema só seja
// tocada se o jogo realmente pedir DirectInput.
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD version,
                                                                   REFIID riid, LPVOID* out,
                                                                   LPUNKNOWN outer)
{
    using Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

    if (g_system_dinput8 == nullptr)
    {
        wchar_t path[MAX_PATH]{};
        const auto length = GetSystemDirectoryW(path, MAX_PATH);
        if (length == 0 || length > MAX_PATH - 16)
        {
            return E_FAIL;
        }
        lstrcatW(path, L"\\dinput8.dll");
        g_system_dinput8 = LoadLibraryW(path);
    }
    if (g_system_dinput8 == nullptr)
    {
        return E_FAIL;
    }

    const auto original =
        reinterpret_cast<Fn>(GetProcAddress(g_system_dinput8, "DirectInput8Create"));
    if (original == nullptr)
    {
        return E_FAIL;
    }
    return original(inst, version, riid, out, outer);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, bootstrap, module, 0, nullptr);
            thread != nullptr)
        {
            CloseHandle(thread);
        }
        break;
    case DLL_PROCESS_DETACH:
        e33::ModHost::instance().stop();
        break;
    default:
        break;
    }
    return TRUE;
}

#endif
