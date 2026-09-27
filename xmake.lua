-- E33 Boss Music Swapper — UE4SS C++ mod
--
-- PRE-REQUISITO: defina UE4SS_SDK apontando para o checkout do UE4SS
-- (com RE-UE4SS/include e as libs já compiladas):
--   xmake f --ue4ss=C:/path/to/RE-UE4SS
--
-- Só compila no Windows/MSVC: o target é uma DLL carregada pelo UE4SS.

set_xmakever("2.8.0")
set_languages("c++23")
set_arch("x64")

option("ue4ss")
    set_default("")
    set_showmenu(true)
    set_description("Caminho para o checkout do RE-UE4SS")
option_end()

target("BossMusicSwapper")
    set_kind("shared")
    set_basename("main")

    add_files("src/**.cpp")
    add_includedirs("src")

    -- TODO(M1): trocar por add_deps("UE4SS") se o mod for compilado dentro
    -- da árvore do UE4SS. Standalone precisa dos includes + import lib.
    on_load(function (target)
        local sdk = get_config("ue4ss")
        if sdk and sdk ~= "" then
            target:add("includedirs", path.join(sdk, "UE4SS/include"))
            target:add("includedirs", path.join(sdk, "deps/first/File/include"))
            target:add("includedirs", path.join(sdk, "deps/first/DynamicOutput/include"))
            target:add("includedirs", path.join(sdk, "deps/third/imgui"))
        end
    end)

    if is_plat("windows") then
        add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
        add_cxflags("/utf-8", "/EHsc")
    end
