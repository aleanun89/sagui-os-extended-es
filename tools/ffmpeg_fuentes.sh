#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# ffmpeg_fuentes.sh - trae FFmpeg 8.1.3 a userland/terceros/ffmpeg/fuentes (D458, docs/ffmpeg.md).
#
# FFmpeg (https://ffmpeg.org/, LGPL-2.1-or-later sin --enable-gpl ni --enable-nonfree). Las fuentes NO van a git:
# tarball oficial de ffmpeg.org con SHA-256 fijo (ffmpeg.org no publica sumas; esta es la del tarball bajado el
# 2026-11-21; hay una firma .asc junto al tarball si se quiere verificar a mano). Se quitan las referencias de FATE (tests/ref) y presets/;
# los cambios de userland/terceros/ffmpeg/parches/*.patch se aplican al final.
#
#   tools/ffmpeg_fuentes.sh [--tarball FICHERO]
set -euo pipefail
VERSION=8.1.3
URL="https://ffmpeg.org/releases/ffmpeg-${VERSION}.tar.xz"
SHA256=7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/ffmpeg"
DEST="${FFMPEG_DEST:-$BASE/fuentes}"
TARBALL=""
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        -h|--help|--ayuda) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "ffmpeg_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/ffmpeg.tar.xz"
    echo "ffmpeg_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || { echo "ffmpeg_fuentes.sh: SHA-256 inesperado" >&2; exit 1; }
mkdir -p "$TMP/x"
tar xJf "$TARBALL" -C "$TMP/x" --strip-components=1
rm -rf "$DEST"; mkdir -p "$DEST"
cp -a "$TMP/x"/. "$DEST/"
rm -rf "$DEST/presets" "$DEST/tests/ref"
echo "$VERSION" > "$DEST/VERSION.sagui"
for p in "$BASE"/parches/*.patch; do
    [ -e "$p" ] || continue
    echo "ffmpeg_fuentes.sh: aplicando $(basename "$p")"
    (cd "$DEST" && patch -p1 -s < "$p")
done
touch "$DEST/.preparado"
echo "ffmpeg_fuentes.sh: listo en $DEST"
