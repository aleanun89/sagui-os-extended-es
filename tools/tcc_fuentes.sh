#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# tcc_fuentes.sh - trae TCC (Tiny C Compiler) a userland/terceros/tcc/fuentes (D384, docs/tcc.md).
#
# TCC (https://repo.or.cz/tinycc.git, LGPL-2.1-or-later; su libtcc1.c, GPL-2.0-or-later con excepción
# de enlace) a un commit fijo de la rama «mob» (versión 0.9.28rc). La última etiqueta, 0.9.27, es de
# 2017; «mob» es lo que empaquetan las distribuciones. Las fuentes NO van a git:
#
#   tarball del commit (espejo de GitHub, SHA-256 fijo) + userland/terceros/tcc/parches/*.patch
#
#   tools/tcc_fuentes.sh [--tarball FICHERO]
#
# Se conservan las fuentes del compilador, include/ y lib/ (sin tests/, win32/ ni examples/).
# Los parches de SaguiOS se aplican con «patch -p1» desde fuentes/. Al final queda fuentes/.preparado.
set -euo pipefail
COMMIT=f830a1697e159056b75de40a60464bc43ee670a3
URL="https://github.com/TinyCC/tinycc/archive/${COMMIT}.tar.gz"
SHA256=7e7bffb841e3753d9f90bdb07bfcbaca88373dcfc85f4e49205ce9add942ebb2
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/tcc"
DEST="${TCC_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,14p' "$0"; exit 0 ;;
        *) echo "tcc_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/tcc.tar.gz"
    echo "tcc_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "tcc_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$TMP/x/tests" "$TMP/x/win32" "$TMP/x/examples"
rm -rf "$DEST"; mkdir -p "$DEST"
cp -r "$TMP/x/." "$DEST/"
echo "$COMMIT" > "$DEST/COMMIT"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "tcc_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "tcc_fuentes.sh: listo en $DEST"
