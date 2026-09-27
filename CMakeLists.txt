# Build da DLL do mod.
#
# Sem UE4SS: compilar um mod C++ dele exige o repositório PRIVADO
# Re-UE4SS/UEPseudo (ver docs/BUILD-BLOCKER.md). Toda a stack aqui é pública,
# então qualquer pessoa — e o CI — consegue compilar:
#
#   ImGui     (MIT)      desenho do overlay
#   MinHook   (BSD-2)    hook das vtables de D3D
#   nlohmann  (MIT)      JSON
#
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   cmake --build build --target package
#
# Windows/MSVC apenas. Os alvos nativos (tests, harness) continuam no xmake e
# rodam em qualquer sistema — ver xmake.lua e docs/DEV-MACOS.md.

cmake_minimum_required(VERSION 3.22)
project(Repertoire33 CXX)

if(NOT WIN32)
    message(FATAL_ERROR "A DLL do mod so compila no Windows. Para os testes e o "
                        "harness, use o xmake: xmake build tests && xmake run tests")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)

FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE)

# ImGui não traz CMakeLists próprio; vira uma biblioteca nossa logo abaixo.
# Mesma versao que o xmake entrega ao harness. Divergir faria o overlay ser
# julgado no harness sobre uma API diferente da que a DLL compila — e nomes de
# flag mudaram entre versoes recentes (ImGuiChildFlags_Border -> Borders).
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.92.9b-docking
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(nlohmann_json minhook imgui)

add_library(imgui_backend STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp")
target_include_directories(imgui_backend PUBLIC "${imgui_SOURCE_DIR}")
target_link_libraries(imgui_backend PUBLIC d3d11 d3d12 dxgi dxguid)

set(TARGET BossMusicSwapper)

file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_library(${TARGET} SHARED ${MOD_SOURCES})

# Nome do arquivo: dinput8 para o modo proxy. Com injetor, o nome é irrelevante.
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "dinput8")

target_include_directories(${TARGET} PRIVATE src)
target_compile_definitions(${TARGET} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(${TARGET} PRIVATE
    imgui_backend
    minhook
    nlohmann_json::nlohmann_json
    d3d11 d3d12 dxgi dxguid)

if(MSVC)
    target_compile_options(${TARGET} PRIVATE /utf-8 /EHsc /W4)
endif()

# ---- pacote distribuível ----
#
# Monta a árvore que o usuário extrai. Empacotar por install/CPack em vez de
# script solto mantém a lista num lugar só: esquecer data/ ou a fonte gera um
# mod que abre e não funciona.
# A DLL na raiz do zip e o resto na subpasta: extrair tudo em Win64/ deixa a
# DLL ao lado do executavel e os arquivos do mod separados.
install(TARGETS ${TARGET} RUNTIME DESTINATION ".")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION "Repertoire33/data")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION "Repertoire33/assets")
install(FILES
        "${CMAKE_SOURCE_DIR}/config.json"
        "${CMAKE_SOURCE_DIR}/LICENSE"
        "${CMAKE_SOURCE_DIR}/README.md"
        DESTINATION "Repertoire33")

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Repertoire33")
set(CPACK_PACKAGE_FILE_NAME "Repertoire33")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
include(CPack)
