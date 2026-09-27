set(TARGET BossMusicSwapper)

# A DLL carrega tudo menos o que é só do harness nativo. A divisão é a mesma do
# xmake.lua: Calc-like puro, Hooks/ e UI/ por cima.
file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp")

add_library(${TARGET} SHARED ${MOD_SOURCES})

# O UE4SS carrega <pasta do mod>/dlls/main.dll, então o nome do arquivo é fixo.
set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "main"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON)

target_include_directories(${TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_compile_definitions(${TARGET} PRIVATE E33_WITH_UE4SS NOMINMAX WIN32_LEAN_AND_MEAN)

# nlohmann/json: header-only, buscado na configuração. O ImGui vem do próprio
# UE4SS — o mod desenha no contexto dele, então linkar outro seria um segundo
# ImGui no mesmo processo.
include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

target_link_libraries(${TARGET} PUBLIC UE4SS PRIVATE nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(${TARGET} PRIVATE /utf-8 /EHsc)
endif()

# ---- pacote distribuível ----
#
# Monta exatamente a árvore que o usuário extrai em ue4ss\Mods\. Empacotar por
# install/CPack em vez de script solto mantém a lista de arquivos num lugar só:
# esquecer data/ ou a fonte gera um mod que abre e não funciona.
set(PACKAGE_ROOT "${CMAKE_BINARY_DIR}/package/BossMusicSwapper")

install(TARGETS ${TARGET} RUNTIME DESTINATION "BossMusicSwapper/dlls")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../data/"
        DESTINATION "BossMusicSwapper/data")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../assets/"
        DESTINATION "BossMusicSwapper/assets")
install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../config.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../enabled.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/../README.md"
        DESTINATION "BossMusicSwapper")

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Repertoire33")
set(CPACK_PACKAGE_FILE_NAME "Repertoire33")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
include(CPack)
