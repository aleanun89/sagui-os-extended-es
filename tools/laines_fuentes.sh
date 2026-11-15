#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# laines_fuentes.sh - trae las fuentes de LaiNES a userland/terceros/laines/jogo (D321).
#
# LaiNES (https://github.com/AndreaOrru/LaiNES, BSD-2-Clause; commit fijo) es un emulador de NES
# en C++ con el APU de blargg (Nes_Snd_Emu, LGPL-2.1 o posterior). Las fuentes VAN EN GIT
# (son pequeñas y la licencia lo permite); este guion hace reproducible el árbol:
#
#   tarball del commit (SHA-256 fijo) + userland/terceros/laines/parches/*.patch
#
#   tools/laines_fuentes.sh [--tarball FICHERO] [--comprobar | --regenerar]
#
#   (sin opción)   descarga, aplica los parches y sustituye jogo/
#   --tarball F    usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar    no escribe nada: compara tarball + parches con lo que hay en git
#   --regenerar    rehace parches/sagui.patch como la diferencia entre el tarball y jogo/
#
# Se copian src/, lib/, LICENSE y README.md (no res/ -tipografía, iconos- ni el submódulo simpleini:
# la interfaz es la nuestra, userland/terceros/laines/plataforma/).
set -euo pipefail

COMMIT=1f3d126171104ec57e515641ea3b5291d383ebdf
URL="https://github.com/AndreaOrru/LaiNES/archive/${COMMIT}.tar.gz"
SHA256=17e43f5efdfc382d3ec0d146ae5073d14370a3d4ed6deb8c3042bb688f1d266a

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/laines"
DEST="$BASE/jogo"
PARCHES="$BASE/parches"
TARBALL=""
MODO=normal

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) MODO=comprobar; shift ;;
        --regenerar) MODO=regenerar; shift ;;
        -h|--help|--ayuda) sed -n '2,19p' "$0"; exit 0 ;;
        *) echo "laines_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff patch; do
    command -v "$h" >/dev/null 2>&1 || { echo "laines_fuentes.sh: falta $h" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/laines.tar.gz"
    echo "laines_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "laines_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/LaiNES-${COMMIT}"
mkdir -p "$TMP/a" "$TMP/b"
for d in "$TMP/a" "$TMP/b"; do
    cp -a "$S/src" "$S/lib" "$d/"
    cp "$S/LICENSE" "$S/README.md" "$d/"
    echo "$COMMIT $SHA256" > "$d/VERSAO"
done

if [ "$MODO" = regenerar ]; then
    mkdir -p "$PARCHES"
    ( cd "$TMP" && cp -a "$DEST" "$TMP/c" && diff -ruN a c > "$PARCHES/sagui.patch" || true )
    echo "laines_fuentes.sh: parche regenerado: $(grep -c '^diff ' "$PARCHES/sagui.patch") ficheros"
    exit 0
fi

for p in "$PARCHES"/*.patch; do
    [ -f "$p" ] || continue
    ( cd "$TMP/b" && patch -s -p1 < "$p" ) || { echo "laines_fuentes.sh: falla el parche $p" >&2; exit 1; }
done

if [ "$MODO" = comprobar ]; then
    if diff -r "$TMP/b" "$DEST" >/dev/null; then
        echo "laines_fuentes.sh: $DEST es LaiNES $COMMIT con los parches de $PARCHES"
        exit 0
    fi
    echo "laines_fuentes.sh: $DEST NO coincide con LaiNES $COMMIT + parches:" >&2
    diff -rq "$TMP/b" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mv "$TMP/b" "$DEST"
echo "laines_fuentes.sh: LaiNES $COMMIT (con parches) en $DEST"
