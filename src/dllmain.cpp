#include "Mod.hpp"

#include <Mod/CppUserModBase.hpp>

// Pontos de entrada exigidos pelo UE4SS: start_mod() ao carregar a DLL,
// uninstall_mod() ao descarregar.
#define BOSSMUSIC_API __declspec(dllexport)

extern "C" {
BOSSMUSIC_API RC::CppUserModBase* start_mod()
{
    return new e33::BossMusicMod{};
}

BOSSMUSIC_API void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
}
