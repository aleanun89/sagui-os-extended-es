#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# git_fuentes.sh - trae libgit2 a userland/terceros/git/fuentes (D400, docs/git.md).
#
# libgit2 1.9.1 (https://github.com/libgit2/libgit2): GPL-2.0 con excepción de enlace (COPYING), que
# permite enlazarla con nuestra libc/libsagui (GPL-3.0). NO se porta el git original (GPL-2.0-only,
# no enlazable con código GPL-3.0). Las fuentes NO van a git: tarball de la etiqueta (SHA-256 fijo).
#
#   tools/git_fuentes.sh [--tarball FICHERO]
#
# Se conservan src/libgit2, src/util, include/ y de deps/ solo llhttp (MIT), xdiff (LGPL-2.1+) y zlib
# (zlib); sin tests, ejemplos, pcre, ntlmclient ni winhttp. Al final queda fuentes/.preparado.
set -euo pipefail
VERSION=1.9.1
URL="https://github.com/libgit2/libgit2/archive/refs/tags/v${VERSION}.tar.gz"
SHA256=14cab3014b2b7ad75970ff4548e83615f74d719afe00aa479b4a889c1e13fc00
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/git"
DEST="${GIT_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "git_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/libgit2.tar.gz"
    echo "git_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "git_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST/deps" "$DEST/src"
cp -r "$TMP/x/include" "$DEST/include"
cp -r "$TMP/x/src/libgit2" "$TMP/x/src/util" "$DEST/src/"
for d in llhttp xdiff zlib; do cp -r "$TMP/x/deps/$d" "$DEST/deps/$d"; done
cp "$TMP/x/COPYING" "$TMP/x/AUTHORS" "$DEST/"
echo "$VERSION" > "$DEST/VERSION"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "git_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "git_fuentes.sh: listo en $DEST"
