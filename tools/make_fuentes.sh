#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# make_fuentes.sh - trae GNU make 4.4.1 a userland/terceros/make/fuentes (D413, docs/make.md).
#
# GNU make (https://www.gnu.org/software/make/, GPL-3.0-or-later, compatible con la GPL v3 de SaguiOS).
# Las fuentes NO van a git: tarball oficial de ftp.gnu.org con SHA-256 fijo + parches de
# userland/terceros/make/parches/*.patch (si los hay).
#
#   tools/make_fuentes.sh [--tarball FICHERO]
#
# Se conservan src/ y lib/ (sin doc, po, tests, ni los puertos a otros sistemas). Al final queda fuentes/.preparado.
set -euo pipefail
VERSION=4.4.1
URL="https://ftp.gnu.org/gnu/make/make-${VERSION}.tar.gz"
SHA256=dd16fb1d67bfab79a72f5e8390735c49e3e8e70b4945a15ab1f81ddb78658fb3
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/make"
DEST="${MAKE_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "make_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/make.tar.gz"
    echo "make_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "make_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xzf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST"
cp -r "$TMP/x/src" "$TMP/x/lib" "$TMP/x/COPYING" "$TMP/x/AUTHORS" "$DEST/"
echo "$VERSION" > "$DEST/VERSION"
# posixos.c: sin bloqueos de fichero (fcntl F_SETLKW): --output-sync no los necesita en SaguiOS
sed -i 's/fcntl (osync_handle, F_SETLKW, &fl)/fcntl_bloqueo (osync_handle, F_SETLKW, \&fl)/' "$DEST/src/posixos.c"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "make_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "make_fuentes.sh: listo en $DEST"
