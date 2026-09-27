-- E33 Boss Music Swapper — UE4SS C++ mod
--
-- Três targets:
--   BossMusicSwapper  DLL do mod. Windows/MSVC, precisa do checkout do UE4SS:
--                       xmake f --ue4ss=C:/path/to/RE-UE4SS && xmake
--   tests             Testes da lógica pura. Roda em qualquer SO, sem o jogo:
--                       xmake build tests && xmake run tests
--   harness           Preview nativo do overlay em ImGui, sem o jogo:
--                       xmake build harness && xmake run harness
--
-- Só a DLL depende de Windows. Ver docs/DEV-MACOS.md.

set_xmakever("2.8.0")
set_languages("c++23")
set_allowedmodes("debug", "release")
add_rules("mode.debug", "mode.release")

add_requires("nlohmann_json")
add_requires("doctest")

option("ue4ss")
    set_default("")
    set_showmenu(true)
    set_description("Caminho para o checkout do RE-UE4SS")
option_end()

-- Lógica que não conhece nem o Unreal nem o ImGui. Compartilhada pelos três
-- targets, e é o que os testes cobrem.
local core_files = {
    "src/Support/*.cpp",
    "src/Config/*.cpp",
    "src/Data/*.cpp",
    "src/Core/*.cpp",
    "src/Hooks/CombatStart.cpp", -- compila nativo de proposito: simulate() e o
                                 -- caminho de teste sem o jogo
}

target("BossMusicSwapper")
    set_kind("shared")
    set_basename("main")
    set_default(false) -- precisa de Windows + UE4SS; não entra no build padrão
    set_enabled(is_plat("windows"))

    add_files("src/dllmain.cpp", "src/Mod.cpp", "src/UI/*.cpp")
    add_files("src/Hooks/AudioSwap.cpp") -- unica parte que fala com o audio do jogo
    add_files(core_files)
    add_includedirs("src")
    add_packages("nlohmann_json")
    add_defines("E33_WITH_UE4SS")

    on_load(function (target)
        local sdk = get_config("ue4ss")
        if not sdk or sdk == "" then
            return
        end
        target:add("includedirs", path.join(sdk, "UE4SS/include"))
        target:add("includedirs", path.join(sdk, "deps/first/File/include"))
        target:add("includedirs", path.join(sdk, "deps/first/DynamicOutput/include"))
        target:add("includedirs", path.join(sdk, "deps/third/imgui"))
    end)

    if is_plat("windows") then
        add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", "/EHsc")
    end

target("tests")
    set_kind("binary")
    add_files("tests/*.cpp")
    add_files(core_files)
    add_includedirs("src")
    add_packages("nlohmann_json", "doctest")
