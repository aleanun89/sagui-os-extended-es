#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# dosbox_fuentes.sh - trae las fuentes de DOSBox 0.74-3 a userland/terceros/dosbox/jogo (D294).
#
# DOSBox 0.74-3 (https://www.dosbox.com, GPL-2.0 o posterior) es el emulador de PC/DOS
# clásico (no DOSBox Staging ni DOSBox-X). Como OpenJazz (D228), las fuentes VAN EN GIT
# ya con nuestros parches aplicados; este guion es lo que hace reproducible el árbol:
#
#   tarball oficial (versión y SHA-256 fijos) + userland/terceros/dosbox/parches/*.patch
#
#   tools/dosbox_fuentes.sh [--tarball FICHERO] [--comprobar | --regenerar]
#
#   (sin opción)   descarga, aplica los parches y sustituye jogo/
#   --tarball F    usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar    no escribe nada: compara tarball + parches con lo que hay en git
#   --regenerar    rehace parches/sagui.patch como la diferencia entre el tarball y
#                  jogo/ (se edita jogo/ a mano y se regenera el parche)
#
# Del tarball se copian src/, include/ y los textos (COPYING, AUTHORS, README, THANKS).
# Lo que es de otras plataformas (visualc_net, scripts de autotools, docs) no se copia.
set -euo pipefail

VERSAO=0.74-3
URL="https://sourceforge.net/projects/dosbox/files/dosbox/${VERSAO}/dosbox-${VERSAO}.tar.gz/download"
SHA256=c0d13dd7ed2ed363b68de615475781e891cd582e8162b5c3669137502222260a

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/dosbox"
DEST="$BASE/jogo"
PARCHES="$BASE/parches"
TARBALL=""
MODO=normal

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) MODO=comprobar; shift ;;
        --regenerar) MODO=regenerar; shift ;;
        -h|--help|--ayuda) sed -n '2,22p' "$0"; exit 0 ;;
        *) echo "dosbox_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff patch; do
    command -v "$h" >/dev/null 2>&1 || { echo "dosbox_fuentes.sh: falta $h" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/dosbox-${VERSAO}.tar.gz"
    echo "dosbox_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "dosbox_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/dosbox-${VERSAO}"
# El árbol sin tocar (a/) y el que lleva los parches (b/).
mkdir -p "$TMP/a" "$TMP/b"
for d in "$TMP/a" "$TMP/b"; do
    cp -a "$S/src" "$S/include" "$d/"
    cp "$S/COPYING" "$S/AUTHORS" "$S/README" "$S/THANKS" "$d/"
    echo "$VERSAO $SHA256" > "$d/VERSAO"
done

if [ "$MODO" = regenerar ]; then
    mkdir -p "$PARCHES"
    ( cd "$TMP" && cp -a "$DEST" "$TMP/c" && diff -ruN a c > "$PARCHES/sagui.patch" || true )
    echo "dosbox_fuentes.sh: parche regenerado: $(grep -c '^diff ' "$PARCHES/sagui.patch") ficheros"
    exit 0
fi

for p in "$PARCHES"/*.patch; do
    [ -f "$p" ] || continue
    ( cd "$TMP/b" && patch -s -p1 < "$p" ) || { echo "dosbox_fuentes.sh: falla el parche $p" >&2; exit 1; }
done

if [ "$MODO" = comprobar ]; then
    if diff -r "$TMP/b" "$DEST" >/dev/null; then
        echo "dosbox_fuentes.sh: $DEST es DOSBox $VERSAO con los parches de $PARCHES"
        exit 0
    fi
    echo "dosbox_fuentes.sh: $DEST NO coincide con DOSBox $VERSAO + parches:" >&2
    diff -rq "$TMP/b" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mv "$TMP/b" "$DEST"
echo "dosbox_fuentes.sh: DOSBox $VERSAO (con parches) en $DEST"
