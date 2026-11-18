#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# doom_fuentes.sh - trae Chocolate Doom a userland/terceros/chocolatedoom/fuentes (D383, docs/doom.md).
#
# Chocolate Doom 3.1.1 (https://github.com/chocolate-doom/chocolate-doom, GPL-2.0 o posterior;
# sha1.c es de GnuPG, GPL-2.0 o posterior; las fuentes de textscreen son de DOSBox, GPL-2.0 o
# posterior). Las fuentes NO van a git: tarball de la etiqueta (SHA-256 fijo) más
# userland/terceros/chocolatedoom/parches/*.patch, aplicados con «patch -p1» desde fuentes/.
# Se conservan src, opl, pcsound, textscreen y los documentos de licencia (no cmake, win32, pkg...).
#
#   tools/doom_fuentes.sh [--tarball FICHERO] [--forzar]
set -euo pipefail
VERSAO=3.1.1
URL="https://github.com/chocolate-doom/chocolate-doom/archive/refs/tags/chocolate-doom-${VERSAO}.tar.gz"
SHA256=1edcc41254bdc194beb0d33e267fae306556c4d24110a1d3d3f865717f25da23
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/chocolatedoom"
DEST="${DOOM_DEST:-$BASE/fuentes}"
TARBALL=""; FORZAR=0
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,13p' "$0"; exit 0 ;;
        *) echo "doom_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
if [ -f "$DEST/.preparado" ] && [ "$FORZAR" = 0 ]; then
    echo "doom_fuentes.sh: ya preparado en $DEST (--forzar para rehacer)"; exit 0
fi
for h in curl sha256sum tar patch; do
    command -v "$h" >/dev/null 2>&1 || { echo "doom_fuentes.sh: falta $h" >&2; exit 1; }
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/cd.tar.gz"
    echo "doom_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "doom_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST"
for d in src opl pcsound textscreen; do cp -r "$TMP/x/$d" "$DEST/"; done
cp "$TMP/x/COPYING.md" "$TMP/x/AUTHORS" "$TMP/x/README.md" "$TMP/x/NEWS.md" "$DEST/"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "doom_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
echo "$VERSAO" > "$DEST/.preparado"
echo "doom_fuentes.sh: listo en $DEST"
