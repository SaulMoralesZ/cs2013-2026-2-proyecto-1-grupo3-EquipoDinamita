#!/usr/bin/env bash
# Genera el instalador (.exe NSIS) y el portable (.zip) de Windows DESDE LINUX.
#   Requisitos: sudo apt install mingw-w64 nsis     (wine es opcional: ejecuta las pruebas)
#   Uso:        installer/build_windows.sh
#   Resultado:  dist/escape-circuit-<version>-windows-x86_64.exe
#               dist/escape-circuit-<version>-windows-x86_64.zip
# Alternativa sin instalar nada: el workflow .github/workflows/build.yml compila
# en un Windows real de GitHub Actions y deja los mismos archivos en "Artifacts".
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-windows"
DIST="$ROOT/dist"

for tool in x86_64-w64-mingw32-g++ makensis; do
    if ! command -v "$tool" > /dev/null; then
        echo "Falta $tool. Instale: sudo apt install mingw-w64 nsis" >&2
        exit 1
    fi
done

# Con wine, ctest puede ejecutar las pruebas .exe compiladas para Windows.
EMULATOR=()
if command -v wine > /dev/null; then EMULATOR=(-DCMAKE_CROSSCOMPILING_EMULATOR=wine); fi

echo "==> Configurando (MinGW-w64, Release, estatico)"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCIRCUIT_ESCAPE_STATIC=ON \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/installer/windows/mingw-w64-x86_64.cmake" "${EMULATOR[@]}"

echo "==> Compilando"
cmake --build "$BUILD" -j"$(nproc)"

if command -v wine > /dev/null; then
    echo "==> Pruebas (con wine)"
    ctest --test-dir "$BUILD" --output-on-failure || echo "Aviso: algunas pruebas fallaron bajo wine." >&2
else
    echo "==> Pruebas omitidas (instale wine para ejecutarlas)"
fi

echo "==> Empaquetando"
(cd "$BUILD" && cpack -G "NSIS;ZIP")

mkdir -p "$DIST"
cp "$BUILD"/escape-circuit-*windows*.exe "$BUILD"/escape-circuit-*windows*.zip "$DIST/"
echo
echo "Paquetes listos en $DIST:"
ls -1 "$DIST"
