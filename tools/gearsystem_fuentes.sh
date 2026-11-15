#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# gearsystem_fuentes.sh - trae las fuentes de Gearsystem a userland/terceros/gearsystem/jogo (D321).
#
# Gearsystem (https://github.com/drhelius/Gearsystem, GPL-3.0; versión fija) es un emulador de
# Master System, Game Gear y SG-1000 en C++ (Ignacio Sanchez). Las fuentes VAN EN GIT
# (la licencia lo permite); este guion hace reproducible el árbol:
#
#   tarball de la versión (SHA-256 fijo) + userland/terceros/gearsystem/parches/*.patch
#
#   tools/gearsystem_fuentes.sh [--tarball FICHERO] [--comprobar | --regenerar]
#
#   (sin opción)   descarga, aplica los parches y sustituye jogo/
#   --tarball F    usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar    no escribe nada: compara tarball + parches con lo que hay en git
#   --regenerar    rehace parches/sagui.patch como la diferencia entre el tarball y jogo/
#
# Se copian src/ (el núcleo, sin interfaz), LICENSE y README.md (no platforms/: la interfaz es la nuestra,
# userland/terceros/gearsystem/plataforma/) y SIN src/audio/emu2413 (FM de Genesis Plus GX, licencia no
# comercial: se sustituye por plataforma/ym2413/, vacío).
set -euo pipefail

COMMIT=3.9.25
URL="https://github.com/drhelius/Gearsystem/archive/${COMMIT}.tar.gz"
SHA256=1ad0baec0904384aa19cde98ce93db97907530d4b313f1798ec460c9ccb8c417

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$RAIZ/userland/terceros/gearsystem"
DEST="$BASE/jogo"
PARCHES="$BASE/parches"
TARBALL=""
MODO=normal

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) MODO=comprobar; shift ;;
        --regenerar) MODO=regenerar; shift ;;
        -h|--help|--ayuda) sed -n '2,19p' "$0"; exit 0 ;;
        *) echo "gearsystem_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff patch; do
    command -v "$h" >/dev/null 2>&1 || { echo "gearsystem_fuentes.sh: falta $h" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/gearsystem.tar.gz"
    echo "gearsystem_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "gearsystem_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/Gearsystem-${COMMIT}"
mkdir -p "$TMP/a" "$TMP/b"
for d in "$TMP/a" "$TMP/b"; do
    cp -a "$S/src" "$d/"
    cp "$S/LICENSE" "$S/README.md" "$d/"
    # El YM2413 (src/audio/emu2413) es de Genesis Plus GX (licencia no comercial): no se trae (D321).
    rm -rf "$d/src/audio/emu2413"
    echo "$COMMIT $SHA256" > "$d/VERSAO"
done

if [ "$MODO" = regenerar ]; then
    mkdir -p "$PARCHES"
    ( cd "$TMP" && cp -a "$DEST" "$TMP/c" && diff -ruN a c > "$PARCHES/sagui.patch" || true )
    echo "gearsystem_fuentes.sh: parche regenerado: $(grep -c '^diff ' "$PARCHES/sagui.patch") ficheros"
    exit 0
fi

for p in "$PARCHES"/*.patch; do
    [ -f "$p" ] || continue
    ( cd "$TMP/b" && patch -s -p1 < "$p" ) || { echo "gearsystem_fuentes.sh: falla el parche $p" >&2; exit 1; }
done

if [ "$MODO" = comprobar ]; then
    if diff -r "$TMP/b" "$DEST" >/dev/null; then
        echo "gearsystem_fuentes.sh: $DEST es Gearsystem $COMMIT con los parches de $PARCHES"
        exit 0
    fi
    echo "gearsystem_fuentes.sh: $DEST NO coincide con Gearsystem $COMMIT + parches:" >&2
    diff -rq "$TMP/b" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mv "$TMP/b" "$DEST"
echo "gearsystem_fuentes.sh: Gearsystem $COMMIT (con parches) en $DEST"
