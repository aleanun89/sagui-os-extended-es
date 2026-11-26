#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# valkey_fuentes.sh - trae Valkey 8.0.2 a userland/terceros/valkey/fuentes (D477, docs/decisoes.md (D477)).
#
# Valkey (https://valkey.io/, continuación libre de Redis 7.2, licencia BSD-3-Clause) con sus dependencias
# incluidas (hiredis, Lua 5.1 con cjson/cmsgpack/struct, linenoise, hdr_histogram, fpconv). Las fuentes NO van a
# git: tarball oficial de GitHub con SHA-256 fijo. Se conservan src/, deps/ (sin jemalloc ni pruebas) y COPYING.
#
#   tools/valkey_fuentes.sh [--tarball FICHERO]
set -euo pipefail
VERSION=8.0.2
URL="https://github.com/valkey-io/valkey/archive/refs/tags/${VERSION}.tar.gz"
SHA256=e052c45b3cbe512e24fdfdc3fd337f9f5e4b8f8b8713f349ba867b829c8ff11a
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/valkey"
DEST="${VALKEY_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,9p' "$0"; exit 0 ;;
        *) echo "valkey_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/valkey.tar.gz"
    echo "valkey_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "valkey_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST/deps"
cp -r "$TMP/x/src" "$DEST/src"
for d in hiredis lua linenoise hdr_histogram fpconv; do cp -r "$TMP/x/deps/$d" "$DEST/deps/$d"; done
rm -rf "$DEST/deps/hiredis/examples" "$DEST/deps/hiredis/test.c" "$DEST/deps/lua/doc" "$DEST/deps/lua/test" \
       "$DEST/src/modules" "$DEST/src/rdma.c" 2>/dev/null || true
cp "$TMP/x/COPYING" "$DEST/COPYING"
cp "$TMP/x/valkey.conf" "$DEST/valkey.conf"
echo "$VERSION" > "$DEST/VERSION"
# release.h lo genera upstream con mkreleasehdr.sh (git, fecha): aquí fijo, para que la compilación sea reproducible
cat > "$DEST/src/release.h" <<EOT
#define REDIS_GIT_SHA1 "00000000"
#define REDIS_GIT_DIRTY "0"
#define REDIS_BUILD_ID "saguios-${VERSION}"
#include "version.h"
#define REDIS_BUILD_ID_RAW SERVER_NAME VALKEY_VERSION REDIS_BUILD_ID REDIS_GIT_DIRTY REDIS_GIT_SHA1
EOT
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "valkey_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "valkey_fuentes.sh: listo en $DEST"
