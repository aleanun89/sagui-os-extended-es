#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# glmark2_fuentes.sh - trae glmark2 2023.01 a userland/terceros/glmark2/fuentes (D481, docs/decisoes.md (D481)).
#
# glmark2 (https://github.com/glmark2/glmark2, GPL-3.0-or-later) es el banco de pruebas de OpenGL / OpenGL ES 2.0 de
# Linaro. Las fuentes NO van a git: tarball oficial de GitHub con SHA-256 fijo. Se conservan src/ (sin las
# bibliotecas internas de libpng, libjpeg-turbo y zlib, que solo usa Android: aquí las imágenes las lee stb_image),
# data/ (modelos, sombreadores y texturas) y las licencias. Después se aplican userland/terceros/glmark2/parches/*.patch.
#
#   tools/glmark2_fuentes.sh [--tarball FICHERO]
set -euo pipefail
VERSION=2023.01
URL="https://github.com/glmark2/glmark2/archive/refs/tags/${VERSION}.tar.gz"
SHA256=8fece3fc323b643644a525be163dc4931a4189971eda1de8ad4c1712c5db3d67
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/glmark2"
DEST="${GLMARK2_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,10p' "$0"; exit 0 ;;
        *) echo "glmark2_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/glmark2.tar.gz"
    echo "glmark2_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "glmark2_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST"
cp -r "$TMP/x/src" "$DEST/src"
cp -r "$TMP/x/data" "$DEST/data"
cp "$TMP/x/COPYING" "$TMP/x/COPYING.SGI" "$TMP/x/README" "$TMP/x/NEWS" "$DEST/"
rm -rf "$DEST/src/libjpeg-turbo" "$DEST/src/libpng" "$DEST/src/zlib" "$DEST/data/meson.build" "$DEST/data/wscript_build"
echo "$VERSION" > "$DEST/VERSION"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "glmark2_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "glmark2_fuentes.sh: listo en $DEST"
