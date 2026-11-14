#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# openjazz_fuentes.sh - trae las fuentes de OpenJazz a userland/terceros/openjazz/jogo (D228).
#
# OpenJazz (https://github.com/AlisterT/openjazz) es GPL-2.0 o posterior: la
# reimplementación libre del motor de Jazz Jackrabbit (1994). Sus fuentes son
# pequeñas (unos 1,5 MB sin comprimir, 570 KB el tarball), así que, como las de
# OpenTyrian (D156), VAN EN GIT, byte a byte como las publica el upstream: este
# guion solo sirve para comprobar que lo que hay en git es la versión fijada o
# para pasar a otra versión (cambiando VERSAO y SHA256).
#
# Del tarball se copian src/ y ext/ (el motor y sus bibliotecas: miniz,
# psmplug, argparse, scale2x y stb_rect_pack) y los textos de licencia
# (COPYING, doc/licenses.txt, README.md). Lo que es de otras plataformas
# (builds/, res/) no se copia.
#
#   tools/openjazz_fuentes.sh [--tarball FICHERO] [--comprobar]
#
#   --tarball FICHERO  usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar        no escribe nada: compara el tarball con lo que hay en git
set -euo pipefail

VERSAO=20260301
URL="https://github.com/AlisterT/openjazz/archive/refs/tags/${VERSAO}.tar.gz"
SHA256=9c117a8d9aa539c4dcb3fb5788130563a83ca1d9819e538f233721d823f7a650

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/openjazz/jogo"
TARBALL=""
COMPROBAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) COMPROBAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "openjazz_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff; do
    command -v "$h" >/dev/null 2>&1 || { echo "openjazz_fuentes.sh: falta $h" >&2; exit 1; }
done

# Todo se prepara en una carpeta temporal y solo al final se sustituye la de
# git: un corte a medias no deja un árbol mezclado.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/openjazz-${VERSAO}.tar.gz"
    echo "openjazz_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "openjazz_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/openjazz-${VERSAO}"
N="$TMP/novo"
mkdir -p "$N"
cp -a "$S/src" "$S/ext" "$N/"
cp "$S/COPYING" "$S/README.md" "$N/"
cp "$S/doc/licenses.txt" "$N/licenses.txt"
echo "$VERSAO $SHA256" > "$N/VERSAO"

if [ "$COMPROBAR" = 1 ]; then
    if diff -r "$N" "$DEST" >/dev/null; then
        echo "openjazz_fuentes.sh: $DEST es OpenJazz $VERSAO sin cambios"
        exit 0
    fi
    echo "openjazz_fuentes.sh: $DEST NO coincide con OpenJazz $VERSAO:" >&2
    diff -rq "$N" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$N" "$DEST"
echo "openjazz_fuentes.sh: OpenJazz $VERSAO en $DEST"
