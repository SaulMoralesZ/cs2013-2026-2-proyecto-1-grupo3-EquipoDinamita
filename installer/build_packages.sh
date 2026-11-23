#!/usr/bin/env bash
# Genera el instalador (.deb) y la version portable (.tar.gz) de Circuito de Escape.
#   Uso: installer/build_packages.sh
#   Resultado: dist/escape-circuit_<version>_amd64.deb
#              dist/escape-circuit-<version>-linux-x86_64.tar.gz
# Compila en Release con ejecutables estaticos (no dependen de las bibliotecas
# del sistema), ejecuta todas las pruebas y empaqueta con CPack.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist"

for level in facil medio dificil; do
    if [[ ! -f "$ROOT/assets/maps/mapa_$level.txt" ]]; then
        echo "Falta assets/maps/mapa_$level.txt: genere el lote con map_generator antes de empaquetar." >&2
        exit 1
    fi
done

echo "==> Configurando (Release, estatico)"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCIRCUIT_ESCAPE_STATIC=ON

echo "==> Compilando"
cmake --build "$BUILD" -j"$(nproc)"

echo "==> Pruebas"
ctest --test-dir "$BUILD" --output-on-failure

echo "==> Empaquetando"
(cd "$BUILD" && cpack)

mkdir -p "$DIST"
cp "$BUILD"/escape-circuit*.deb "$BUILD"/escape-circuit*.tar.gz "$DIST/"
echo
echo "Paquetes listos en $DIST:"
ls -1 "$DIST"
