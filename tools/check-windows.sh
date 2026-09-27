#!/usr/bin/env bash
# Checagem de sintaxe do código Windows, a partir do macOS ou do Linux.
#
# Existe porque a DLL só compila no Windows, e mandar código Windows para o CI
# sem nenhuma verificação local gasta uma rodada de CI por erro de digitação.
# Não substitui o build de verdade: o compilador aqui é o GCC/mingw e o do CI é
# o MSVC, que diverge em conformidade. Mas pega a maioria dos erros.
#
#   brew install mingw-w64      # ou: apt install g++-mingw-w64-x86-64
#   ./tools/check-windows.sh
set -euo pipefail

CXX=x86_64-w64-mingw32-g++
if ! command -v "$CXX" >/dev/null; then
    echo "erro: $CXX nao encontrado. Instale o mingw-w64." >&2
    exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${TMPDIR:-/tmp}/e33-wincheck"
mkdir -p "$CACHE"

# Mesma versao do ImGui que o CMakeLists fixa.
IMGUI_TAG="$(grep -A2 'FetchContent_Declare(imgui' "$ROOT/CMakeLists.txt" | grep GIT_TAG | awk '{print $2}')"
if [ ! -d "$CACHE/imgui" ]; then
    git clone -q --depth 1 --branch "$IMGUI_TAG" https://github.com/ocornut/imgui "$CACHE/imgui"
fi
if [ ! -f "$CACHE/MinHook.h" ]; then
    curl -sfL -o "$CACHE/MinHook.h" \
        https://raw.githubusercontent.com/TsudaKageyu/minhook/master/include/MinHook.h
fi

JSON_INC="$(dirname "$(dirname "$(find "$HOME/.xmake/packages" -name json.hpp -path '*nlohmann*' 2>/dev/null | head -1)")")"
if [ -z "$JSON_INC" ] || [ ! -d "$JSON_INC" ]; then
    echo "erro: headers do nlohmann_json nao encontrados. Rode 'xmake f -y' antes." >&2
    exit 1
fi

failures=0
count=0
while IFS= read -r file; do
    count=$((count + 1))
    if ! output=$("$CXX" -std=c++23 -fsyntax-only \
        -DNOMINMAX -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE \
        -I "$ROOT/src" -I "$CACHE" -I "$CACHE/imgui" -I "$JSON_INC" \
        "$file" 2>&1); then
        failures=$((failures + 1))
        echo "--- $file"
        echo "$output" | head -20
    fi
done < <(find "$ROOT/src" -name '*.cpp' | sort)

if [ "$failures" -eq 0 ]; then
    echo "$count arquivo(s) OK"
else
    echo "$failures de $count arquivo(s) com erro" >&2
    exit 1
fi
