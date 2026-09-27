-- E33 Boss Music Swapper — UE4SS C++ mod
--
-- Dois targets (a DLL do mod e CMake; ver CMakeLists.txt):
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
-- Só o harness nativo precisa de ImGui + GLFW; a DLL usa o ImGui do UE4SS.
add_requires("imgui", {configs = {glfw = true, opengl3 = true}})

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

-- A DLL do mod NAO e construida aqui.
--
-- O fluxo suportado pelo UE4SS e CMake, compilando o mod junto com o RE-UE4SS
-- (add_subdirectory); nao ha import library publicada para linkar de fora. Ver
-- CMakeLists.txt na raiz. Este arquivo cuida so dos alvos nativos, que rodam
-- em qualquer sistema e nao precisam do jogo.

target("tests")
    set_kind("binary")
    add_files("tests/*.cpp")
    add_files(core_files)
    add_includedirs("src")
    add_packages("nlohmann_json", "doctest")

target("harness")
    set_kind("binary")
    set_default(false) -- ferramenta de desenvolvimento, não entra no pacote
    set_rundir("$(projectdir)") -- as fixtures sao lidas por caminho relativo
    add_files("harness/*.cpp", "src/UI/*.cpp")
    add_files(core_files)
    add_includedirs("src")
    add_packages("nlohmann_json", "imgui")
    if is_plat("macosx") then
        add_frameworks("OpenGL", "Cocoa", "IOKit", "CoreVideo")
    end
