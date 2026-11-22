#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# lua_fuentes.sh - trae Lua 5.4.9 a userland/terceros/lua/fuentes (D440, docs/awk-lua.md).
#
# Lua (PUC-Rio, https://www.lua.org/, licencia MIT). Las fuentes NO van a git: tarball oficial de lua.org con
# SHA-256 fijo. Se conserva src/ (sin Makefile) y el texto de la licencia.
#
#   tools/lua_fuentes.sh [--tarball FICHERO]
set -euo pipefail
VERSION=5.4.9
URL="https://www.lua.org/ftp/lua-${VERSION}.tar.gz"
SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/lua"
DEST="${LUA_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,8p' "$0"; exit 0 ;;
        *) echo "lua_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/lua.tar.gz"
    echo "lua_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "lua_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST"
cp "$TMP/x"/src/*.c "$TMP/x"/src/*.h "$TMP/x"/src/lua.hpp "$DEST/"
cp "$TMP/x/README" "$DEST/README"
# la licencia MIT es el final de lua.h (y del README de la web); se guarda el texto aparte
sed -n '/^\* Copyright (C) 1994/,/^\*\*\*\*/p' "$TMP/x/src/lua.h" > "$DEST/LICENCIA-LUA.txt" || true
echo "$VERSION" > "$DEST/VERSION"
# la raíz de los módulos de Lua es /apps/lua (package.path: /apps/lua/share/lua/5.4/?.lua, y ./?.lua)
sed -i 's|#define LUA_ROOT\t"/usr/local/"|#define LUA_ROOT\t"/apps/lua/"|' "$DEST/luaconf.h"
grep -q '"/apps/lua/"' "$DEST/luaconf.h" || { echo "lua_fuentes.sh: no se pudo cambiar LUA_ROOT" >&2; exit 1; }
# LUA_USE_POSIX y la entrada de línea los pone userland/terceros/lua/lua.mk con -D
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "lua_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "lua_fuentes.sh: listo en $DEST"
