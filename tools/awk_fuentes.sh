#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# awk_fuentes.sh - trae The One True AWK 20261106 a userland/terceros/awk/fuentes (D440, docs/awk-lua.md).
#
# The One True AWK (Brian Kernighan, https://github.com/onetrueawk/awk; licencia permisiva tipo MIT/ISC de
# Lucent, en LICENSE). Las fuentes NO van a git: tarball de la etiqueta con SHA-256 fijo. La gramática ya
# generada (awkgram.tab.c y awkgram.tab.h, de bison 3.8.2) va en git, en userland/terceros/awk/gerado/, porque
# el anfitrión no tiene por qué traer bison; este guion comprueba que awkgram.y sea el mismo del que salieron.
#
#   tools/awk_fuentes.sh [--tarball FICHERO]
set -euo pipefail
VERSION=20261106
URL="https://github.com/onetrueawk/awk/archive/refs/tags/${VERSION}.tar.gz"
SHA256=1b354fe6fa0089a959ef912085e7a064236d15a7d3c9e6ce86456c3b87d3f464
SHA256_GRAMATICA=452c57bb2c797ab4acb6972048e883db47e9c52268dc17c984374823ed73e079
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/awk"
DEST="${AWK_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,10p' "$0"; exit 0 ;;
        *) echo "awk_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/awk.tar.gz"
    echo "awk_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "awk_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
echo "$SHA256_GRAMATICA  $TMP/x/awkgram.y" | sha256sum -c --quiet - || { echo "awk_fuentes.sh: awkgram.y cambió: regenerar userland/terceros/awk/gerado/" >&2; exit 1; }
rm -rf "$DEST"; mkdir -p "$DEST"
# solo el intérprete (sin las pruebas, el makefile ni las páginas de manual extra)
cp "$TMP/x"/{awk.h,proto.h,b.c,lex.c,lib.c,main.c,maketab.c,parse.c,run.c,tran.c,math.c,awkgram.y,LICENSE,README.md,awk.1} "$DEST/"
echo "$VERSION" > "$DEST/VERSION"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "awk_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "awk_fuentes.sh: listo en $DEST"
