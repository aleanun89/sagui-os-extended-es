#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# mgba_fuentes.sh - trae las fuentes de mGBA 0.10.5 a userland/terceros/mgba/jogo (D322).
#
# mGBA (https://mgba.io, MPL-2.0) es el emulador de Game Boy Advance, Game Boy y Game Boy Color.
# Como DOSBox (D294), las fuentes VAN EN GIT; este guion hace reproducible el árbol:
#
#   tarball oficial (versión y SHA-256 fijos) + userland/terceros/mgba/parches/*.patch (si hay)
#
#   tools/mgba_fuentes.sh [--tarball FICHERO] [--comprobar | --regenerar]
#
#   (sin opción)   descarga, aplica los parches y sustituye jogo/
#   --tarball F    usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar    no escribe nada: compara tarball + parches con lo que hay en git
#   --regenerar    rehace parches/sagui.patch como diferencia entre el tarball y jogo/
#
# Del tarball solo se copia el núcleo: include/, src/{arm,core,feature,gb,gba,sm83,util} (sin pruebas ni
# lo que es de otras plataformas), src/third-party/{blip_buf,inih} y los textos (LICENSE,
# README.md, CHANGES). NO se copian ni Qt, ni SDL de upstream, ni libpng, zlib, lzma, sqlite,
# discord-rpc ni lua/ffmpeg: ninguna dependencia opcional.
set -euo pipefail

VERSAO=0.10.5
URL="https://github.com/mgba-emu/mgba/archive/refs/tags/${VERSAO}.tar.gz"
SHA256=91d6fbd32abcbdf030d58d3f562de25ebbc9d56040d513ff8e5c19bee9dacf14

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/mgba"
DEST="$BASE/jogo"
PARCHES="$BASE/parches"
TARBALL=""
MODO=normal

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) MODO=comprobar; shift ;;
        --regenerar) MODO=regenerar; shift ;;
        -h|--help|--ayuda) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "mgba_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff patch; do
    command -v "$h" >/dev/null 2>&1 || { echo "mgba_fuentes.sh: falta $h" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/mgba-${VERSAO}.tar.gz"
    echo "mgba_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "mgba_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/mgba-${VERSAO}"
mkdir -p "$TMP/a" "$TMP/b"
for d in "$TMP/a" "$TMP/b"; do
    cp -a "$S/include" "$d/"
    mkdir -p "$d/src/third-party"
    for m in arm core gb gba sm83 util feature; do cp -a "$S/src/$m" "$d/src/"; done
    cp -a "$S/src/third-party/blip_buf" "$S/src/third-party/inih" "$d/src/third-party/"
    # Fuera: pruebas, depuradores, interfaz gráfica propia y CMake.
    find "$d/src" -type d \( -name test -o -name debugger -o -name gui \) -prune -exec rm -rf {} +
    find "$d/src" -name CMakeLists.txt -delete
    cp "$S/LICENSE" "$S/README.md" "$S/CHANGES" "$d/"
    echo "$VERSAO $SHA256" > "$d/VERSAO"
done

if [ "$MODO" = regenerar ]; then
    mkdir -p "$PARCHES"
    ( cd "$TMP" && cp -a "$DEST" "$TMP/c" && diff -ruN a c > "$PARCHES/sagui.patch" || true )
    echo "mgba_fuentes.sh: parche regenerado: $(grep -c '^diff ' "$PARCHES/sagui.patch") ficheros"
    exit 0
fi

for p in "$PARCHES"/*.patch; do
    [ -f "$p" ] || continue
    ( cd "$TMP/b" && patch -s -p1 < "$p" ) || { echo "mgba_fuentes.sh: falla el parche $p" >&2; exit 1; }
done

if [ "$MODO" = comprobar ]; then
    if diff -r "$TMP/b" "$DEST" >/dev/null; then
        echo "mgba_fuentes.sh: $DEST es mGBA $VERSAO con los parches de $PARCHES"
        exit 0
    fi
    echo "mgba_fuentes.sh: $DEST NO coincide con mGBA $VERSAO + parches:" >&2
    diff -rq "$TMP/b" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mv "$TMP/b" "$DEST"
echo "mgba_fuentes.sh: mGBA $VERSAO (con parches) en $DEST"
