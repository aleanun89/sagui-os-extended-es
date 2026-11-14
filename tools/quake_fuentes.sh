#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# quake_fuentes.sh - trae las fuentes del motor de Quake a userland/terceros/quake/jogo (D229).
#
# El motor es el WinQuake que id Software publicó con licencia GPL-2.0 (o
# posterior) en 1999 (https://github.com/id-Software/Quake, carpeta WinQuake),
# fijado en el último commit del repositorio. Como las de OpenJazz (D228) y
# OpenTyrian (D156), las fuentes VAN EN GIT, byte a byte como las publica id:
# este guion solo sirve para comprobar que lo que hay en git es esa versión o
# para pasar a otra (cambiando COMMIT y SHA256).
#
# De WinQuake/ se copian solo los .c de la versión por software sin red (la
# lista de abajo, la del «quake.x11» de Makefile.linuxi386 sin el ensamblador,
# X11, OSS ni los sockets) y todas sus cabeceras (con progdefs.q1 y .q2), más los textos de licencia
# del repositorio (gnu.txt y readme.txt). Lo de otras plataformas (DOS,
# Windows, Solaris, OpenGL), los .s y los binarios (cwsdpmi.exe, dxsdk,
# scitech) no se copian. Ningún fichero se cambia: la plataforma del SaguiOS
# está aparte, en userland/terceros/quake/plataforma/.
#
#   tools/quake_fuentes.sh [--tarball FICHERO] [--comprobar]
#
#   --tarball FICHERO  usar un tarball ya descargado (se comprueba igual el SHA-256)
#   --comprobar        no escribe nada: compara el tarball con lo que hay en git
set -euo pipefail

COMMIT=bf4ac424ce754894ac8f1dae6a3981954bc9852d
URL="https://github.com/id-Software/Quake/archive/${COMMIT}.tar.gz"
SHA256=fb98e8c99740123f3fac830c1387584ec1f684db8908cbf42b7187d06d399013

# Los .c del motor (WinQuake/), sin los de plataforma.
FONTES="chase cl_demo cl_input cl_main cl_parse cl_tent cmd common console crc cvar
d_edge d_fill d_init d_modech d_part d_polyse d_scan d_sky d_sprite d_surf d_vars d_zpoint
draw host host_cmd keys mathlib menu model net_loop net_main net_none net_vcr nonintel
pr_cmds pr_edict pr_exec r_aclip r_alias r_bsp r_draw r_edge r_efrag r_light r_main
r_misc r_part r_sky r_sprite r_surf r_vars sbar screen snd_dma snd_mem snd_mix
sv_main sv_move sv_phys sv_user view wad world zone cd_null"

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/quake/jogo"
TARBALL=""
COMPROBAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tarball)   TARBALL="$2"; shift 2 ;;
        --comprobar) COMPROBAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,22p' "$0"; exit 0 ;;
        *) echo "quake_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

for h in curl sha256sum tar diff; do
    command -v "$h" >/dev/null 2>&1 || { echo "quake_fuentes.sh: falta $h" >&2; exit 1; }
done

# Todo se prepara en una carpeta temporal y solo al final se sustituye la de
# git: un corte a medias no deja un árbol mezclado.
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$TARBALL" ]; then
    TARBALL="$TMP/quake-${COMMIT}.tar.gz"
    echo "quake_fuentes.sh: descargando $URL"
    curl -fsSL -o "$TARBALL" "$URL"
fi
echo "$SHA256  $TARBALL" | sha256sum -c --quiet - || {
    echo "quake_fuentes.sh: el SHA-256 de $TARBALL no es el esperado" >&2
    exit 1
}

tar xzf "$TARBALL" -C "$TMP"
S="$TMP/Quake-${COMMIT}"
N="$TMP/novo"
mkdir -p "$N"
for f in $FONTES; do
    cp "$S/WinQuake/$f.c" "$N/"
done
cp "$S"/WinQuake/*.h "$N/"
# progdefs.h incluye uno de estos dos según QUAKE2.
cp "$S/WinQuake/progdefs.q1" "$S/WinQuake/progdefs.q2" "$N/"
cp "$S/gnu.txt" "$S/readme.txt" "$N/"
echo "$COMMIT $SHA256" > "$N/VERSAO"

if [ "$COMPROBAR" = 1 ]; then
    if diff -r "$N" "$DEST" >/dev/null; then
        echo "quake_fuentes.sh: $DEST es WinQuake ${COMMIT:0:12} sin cambios"
        exit 0
    fi
    echo "quake_fuentes.sh: $DEST NO coincide con WinQuake ${COMMIT:0:12}:" >&2
    diff -rq "$N" "$DEST" >&2 || true
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$N" "$DEST"
echo "quake_fuentes.sh: WinQuake ${COMMIT:0:12} en $DEST"
