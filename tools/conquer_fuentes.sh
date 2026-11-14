#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# conquer_fuentes.sh - trae las fuentes de Vanilla Conquer a userland/terceros/conquer/jogo (D230).
#
# Vanilla Conquer (https://github.com/TheAssemblyArmada/Vanilla-Conquer) es
# GPL-3.0 (con los términos adicionales de la sección 7 que puso EA al
# publicar el código en 2020): el motor de Command & Conquer (Tiberian Dawn) y
# de Red Alert, hecho portable. No publica versiones: se fija un commit de la
# rama «vanilla» y el SHA-256 de su tarball.
#
# Lo que se usa de Tiberian Dawn (common/ y tiberiandawn/) son unos 12 MB de
# fuentes sin comprimir (unos 3 MB en el paquete de git) y VA EN GIT, byte a
# byte como lo publica el upstream, como OpenJazz (D228): así el sistema se
# compila sin red y la auditoría de licencias ve el código. Red Alert
# (redalert/, otros 11 MB) NO va en git: con --redalert se copia a
# userland/terceros/conquer/redalert/ (ignorada por git) y el make compila
# también /apps/bin/redalert.
#
#   tools/conquer_fuentes.sh [--tarball FICHERO] [--comprobar] [--redalert]
#
#   --tarball FICHERO  usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar        no escribe nada: compara el tarball con lo que hay en git
#   --redalert         además, las fuentes de Red Alert a la carpeta ignorada
set -euo pipefail

COMMIT=29e68a39dbe86a48ae54976d3e9b27c6b2ab26ee
FECHA=2026-11-13
REVISION=975
URL="https://github.com/TheAssemblyArmada/Vanilla-Conquer/archive/${COMMIT}.tar.gz"
SHA256=3602f08df0b7d5c27865eb3fe6a01b27c907d02728bbc421162c0af4aa1229bf

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/conquer/jogo"
DEST_RA="$RAIZ/userland/terceros/conquer/redalert"
TARBALL=""
COMPROBAR=0
REDALERT=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) COMPROBAR=1; shift ;;
        --redalert)  REDALERT=1; shift ;;
        -h|--help|--ayuda) sed -n '2,22p' "$0"; exit 0 ;;
        *) echo "conquer_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff; do
    command -v "$h" >/dev/null 2>&1 || { echo "conquer_fuentes.sh: falta $h" >&2; exit 1; }
done

# Todo se prepara en una carpeta temporal y solo al final se sustituye la de
# git: un corte a medias no deja un árbol mezclado.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/vanilla-conquer-${COMMIT}.tar.gz"
    echo "conquer_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "conquer_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/Vanilla-Conquer-${COMMIT}"
N="$TMP/novo"
mkdir -p "$N"
cp -a "$S/common" "$S/tiberiandawn" "$N/"
cp "$S/License.txt" "$S/README.md" "$N/"
echo "$COMMIT $FECHA $REVISION $SHA256" > "$N/VERSAO"

if [ "$COMPROBAR" = 1 ]; then
    if diff -r "$N" "$DEST" >/dev/null; then
        echo "conquer_fuentes.sh: $DEST es Vanilla Conquer ${COMMIT:0:7} sin cambios"
        exit 0
    fi
    echo "conquer_fuentes.sh: $DEST NO coincide con Vanilla Conquer ${COMMIT:0:7}:" >&2
    diff -rq "$N" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$N" "$DEST"
echo "conquer_fuentes.sh: Vanilla Conquer ${COMMIT:0:7} ($FECHA) en $DEST"

if [ "$REDALERT" = 1 ]; then
    rm -rf "$DEST_RA"
    cp -a "$S/redalert" "$DEST_RA"
    echo "$COMMIT $FECHA $REVISION $SHA256" > "$DEST_RA/VERSAO"
    echo "conquer_fuentes.sh: Red Alert en $DEST_RA (fuera de git)"
fi
