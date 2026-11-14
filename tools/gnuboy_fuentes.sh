#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# gnuboy_fuentes.sh - trae las fuentes de SDL2-GNUBoy a userland/terceros/gnuboy/jogo (D235).
#
# SDL2-GNUBoy (https://github.com/AlexOberhofer/SDL2-GNUBoy) es el emulador de
# Game Boy y Game Boy Color GNUBoy con una interfaz de SDL 2; licencia GPL-2.0
# (LICENSE.md del repositorio: la GPL versión 2).
#
# Opción A del mantenedor (D261): su código NO va en el repositorio público (es GPL
# solo v2 y se enlaza con la libsagui, GPL v3: no se puede distribuir junto), pero
# cada usuario puede compilárselo para uso propio. Por eso este guion lo DESCARGA:
# baja el commit fijado, comprueba su SHA-256 y deja en userland/terceros/gnuboy/jogo/
# lo mismo que antes iba copiado en git. Si existe userland/terceros/gnuboy/parches/
# con ficheros *.patch, los aplica en orden (hoy no hace falta ninguno: lo copiado era
# idéntico al upstream, byte a byte). Lo nuestro (gnuboy.mk, plataforma/) sí va en git.
#
# Del tarball se copian src/ (el emulador), include/, lib/ (inflate y el
# descompresor XZ, de dominio público), sys/ (las interfaces: sdl2/, dummy/...),
# rc/ (ejemplos de configuración) y los textos de licencia (LICENSE.md,
# docs/COPYING, docs/CREDITS, README.md, Version). No se copian docs/ (9 MB de
# capturas), los makefile ni los paquetes de otras plataformas.
#
#   tools/gnuboy_fuentes.sh [--tarball FICHERO] [--forzar] [--comprobar]
#
#   --tarball FICHERO  usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --forzar           rehacer jogo/ aunque ya exista
#   --comprobar        no escribe nada: compara el tarball (más los parches) con jogo/
set -euo pipefail

COMMIT=c8642d11bd2706333c5255e2b7f2b84445d0a070
FECHA=2022-03-20
URL="https://github.com/AlexOberhofer/SDL2-GNUBoy/archive/${COMMIT}.tar.gz"
SHA256=416abf5aee6af47e3ec919a4ab19322866dd17be695d3644ecb73a7eca9abdf8

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/gnuboy/jogo"
TARBALL=""
COMPROBAR=0
FORZAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) COMPROBAR=1; shift ;;
        --forzar)    FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "gnuboy_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

if [ -f "$DEST/VERSAO" ] && [ "$COMPROBAR" = 0 ] && [ "$FORZAR" = 0 ]; then
    echo "gnuboy_fuentes.sh: ya preparado en $DEST (--forzar para rehacer)"
    exit 0
fi

for h in curl sha256sum tar diff; do
    command -v "$h" >/dev/null 2>&1 || { echo "gnuboy_fuentes.sh: falta $h" >&2; exit 1; }
done

# Todo se prepara en una carpeta temporal y solo al final se sustituye la de
# git: un corte a medias no deja un árbol mezclado.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/gnuboy-${COMMIT}.tar.gz"
    echo "gnuboy_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "gnuboy_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/SDL2-GNUBoy-${COMMIT}"
N="$TMP/novo"
mkdir -p "$N"
cp -a "$S/src" "$S/include" "$S/lib" "$S/sys" "$S/rc" "$N/"
cp "$S/LICENSE.md" "$S/README.md" "$S/Version" "$N/"
cp "$S/docs/COPYING" "$N/COPYING"
cp "$S/docs/CREDITS" "$N/CREDITS"
# Parches propios (opcionales), en orden alfabético.
PARCHES="$RAIZ/userland/terceros/gnuboy/parches"
if ls "$PARCHES"/*.patch >/dev/null 2>&1; then
    command -v patch >/dev/null 2>&1 || { echo "gnuboy_fuentes.sh: falta patch" >&2; exit 1; }
    for p in "$PARCHES"/*.patch; do
        echo "gnuboy_fuentes.sh: aplicando $(basename "$p")"
        patch -p1 -d "$N" < "$p"
    done
fi
echo "$COMMIT $FECHA $SHA256" > "$N/VERSAO"

if [ "$COMPROBAR" = 1 ]; then
    if diff -r "$N" "$DEST" >/dev/null; then
        echo "gnuboy_fuentes.sh: $DEST es SDL2-GNUBoy ${COMMIT:0:7} sin cambios"
        exit 0
    fi
    echo "gnuboy_fuentes.sh: $DEST NO coincide con SDL2-GNUBoy ${COMMIT:0:7}:" >&2
    diff -rq "$N" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$N" "$DEST"
echo "gnuboy_fuentes.sh: SDL2-GNUBoy ${COMMIT:0:7} en $DEST"
