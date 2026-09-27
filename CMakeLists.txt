# Build da DLL do mod.
#
# O fluxo suportado pelo UE4SS é CMake, e o mod é compilado JUNTO com o
# RE-UE4SS (add_subdirectory), não contra headers avulsos — não existe pacote
# com import library publicado para linkar de fora.
#
#   git clone --recursive https://github.com/UE4SS-RE/RE-UE4SS external/RE-UE4SS
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Game__Shipping__Win64
#   cmake --build build
#
# Windows/MSVC apenas. Os alvos nativos (tests, harness) continuam no xmake e
# rodam em qualquer sistema — ver xmake.lua e docs/DEV-MACOS.md.

cmake_minimum_required(VERSION 3.22)
project(Repertoire33 CXX)

set(UE4SS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/RE-UE4SS"
    CACHE PATH "Caminho do checkout do RE-UE4SS")

if(NOT EXISTS "${UE4SS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "RE-UE4SS nao encontrado em ${UE4SS_DIR}.\n"
        "Clone com submodulos:\n"
        "  git clone --recursive https://github.com/UE4SS-RE/RE-UE4SS ${UE4SS_DIR}\n"
        "ou aponte outro caminho com -DUE4SS_DIR=<caminho>.")
endif()

add_subdirectory("${UE4SS_DIR}" RE-UE4SS)
add_subdirectory(mod)
