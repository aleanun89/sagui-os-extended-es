#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# openrsync_fuentes.sh - comprueba (o restaura) las fuentes de openrsync de SaguiOS (D314).
#
# openrsync (https://github.com/kristapsdz/openrsync, VERSION_0_5_0, licencia ISC) es el rsync de
# OpenBSD: protocolo 27, unas 5000 líneas. Sus fuentes son pequeñas, así que VAN EN GIT en
# userland/terceros/openrsync/fuentes/, con unos pocos parches nuestros (PARCHES.md). Este guion:
#   --comprobar  baja el tarball fijado (SHA-256 comprobado) y verifica que lo que hay en git es
#                el upstream salvo los ficheros parcheados que anota PARCHES (sin sorpresas);
#   --restaurar  copia el upstream sin parchear (para pasar a otra versión: cambiar VERSAO y
#                SHA256 y rehacer los parches a mano).
#
#   tools/openrsync_fuentes.sh [--tarball FICHERO] [--comprobar|--restaurar]
set -euo pipefail

VERSAO=VERSION_0_5_0
COMMIT=22b37c9ae3cf63e5788bfe28dc4baa0ea33abb6f
URL="https://github.com/kristapsdz/openrsync/archive/refs/tags/${VERSAO}.tar.gz"
SHA256=92ac1ce5003da17c277e105c973b2288942bf0601a024ffddbd8b5a226a6f604
# lo que se copia del upstream (el resto -cliente ssh, demonio, pruebas- no se usa)
FICHEROS="blocks.c downloader.c flist.c hash.c ids.c io.c log.c md4.c md4.h mkpath.c receiver.c sender.c server.c session.c symlinks.c uploader.c extern.h LICENSE.md README.md"
# los que SaguiOS parchea (ver PARCHES.md)
PARCHEADOS="extern.h flist.c receiver.c session.c"

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/openrsync/fuentes"
TARBALL=""
MODO=comprobar

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) MODO=comprobar; shift ;;
        --restaurar) MODO=restaurar; shift ;;
        -h|--help|--ayuda) sed -n '2,16p' "$0"; exit 0 ;;
        *) echo "openrsync_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff; do
    command -v "$h" >/dev/null 2>&1 || { echo "openrsync_fuentes.sh: falta $h" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/openrsync.tar.gz"
    echo "openrsync_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "openrsync_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}
tar xzf "$TARBALL" -C "$TMP"
S="$TMP/openrsync-${VERSAO}"

if [ "$MODO" = restaurar ]; then
    mkdir -p "$DEST"
    for f in $FICHEROS; do cp "$S/$f" "$DEST/$f"; done
    echo "openrsync_fuentes.sh: upstream $VERSAO ($COMMIT) copiado a $DEST (sin parches)"
    exit 0
fi

mal=0
for f in $FICHEROS; do
    if [ ! -f "$DEST/$f" ]; then echo "falta $f" >&2; mal=1; continue; fi
    if ! diff -q "$S/$f" "$DEST/$f" >/dev/null; then
        case " $PARCHEADOS " in
            *" $f "*) echo "  $f: parcheado (esperado)";;
            *) echo "  $f: DIFIERE del upstream y no es un fichero parcheado" >&2; mal=1;;
        esac
    fi
done
[ "$mal" = 0 ] && echo "openrsync_fuentes.sh: $DEST es openrsync $VERSAO ($COMMIT) con solo los parches de PARCHES.md" || exit 1
