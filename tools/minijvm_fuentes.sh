#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# minijvm_fuentes.sh - trae miniJVM a userland/terceros/minijvm/fuentes (D376, docs/java.md).
#
# miniJVM (https://github.com/digitalgust/miniJVM, MIT; su biblioteca de clases lleva ficheros de
# OpenJDK bajo GPL-2.0-only con Classpath-exception) a un commit fijo. Las fuentes NO van a git:
#
#   tarball del commit (SHA-256 fijo) + userland/terceros/minijvm/parches/*.patch
#
#   tools/minijvm_fuentes.sh [--tarball FICHERO]
#
# Se conservan minijvm/c (la VM, sin mimalloc, sljit ni mbedtls: no se usan en SaguiOS), minijvm/java (la biblioteca de clases) y las licencias.
# Los parches de SaguiOS se aplican con «patch -p1» desde fuentes/.
set -euo pipefail
COMMIT=76f904ea4008a8058db5155bf5ee2cfe94d19e60
URL="https://github.com/digitalgust/miniJVM/archive/${COMMIT}.tar.gz"
SHA256=ffddb6c587898d17d53b12327243106641ff81380797ebd72113de7716e7806e
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/minijvm"
DEST="$BASE/fuentes"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,13p' "$0"; exit 0 ;;
        *) echo "minijvm_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/minijvm.tar.gz"
    echo "minijvm_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "minijvm_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
tar xzf "$TARBALL" -C "$TMP" --strip-components=1 \
    --wildcards '*/minijvm/c' '*/minijvm/java' '*/LICENSE' '*/README.md'
rm -rf "$DEST"; mkdir -p "$DEST"
rm -rf "$TMP/minijvm/c/utils/mimalloc" "$TMP/minijvm/c/utils/sljit" "$TMP/minijvm/c/utils/https"
cp -r "$TMP/minijvm" "$TMP/LICENSE" "$TMP/README.md" "$DEST/"
cp -r "$BASE/plataforma/sustitutos/utils" "$DEST/minijvm/c/"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "minijvm_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
echo "minijvm_fuentes.sh: listo en $DEST"
