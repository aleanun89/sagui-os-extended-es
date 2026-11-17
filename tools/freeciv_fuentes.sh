#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# freeciv_fuentes.sh - descarga y prepara las fuentes de Freeciv y de zlib (D374).
#
# Freeciv (https://github.com/freeciv/freeciv, GPL-2.0 o posterior) 3.1.6: el servidor y el cliente
# SDL2. Las fuentes (unas 90 MB sin los gráficos de origen) NO van en git: este guion baja el tarball
# de la etiqueta, comprueba su SHA-256, copia a userland/terceros/freeciv/fuentes/ (ignorada por git)
# lo que hace falta (sin gtk, qt, ni los .blend de origen ni las traducciones que no son es)
# y genera lo que el sistema de construcción de Freeciv genera en el anfitrión:
#   - common/packets_gen.[ch], client/packhand_gen.[ch], server/hand_gen.[ch] (python3)
#   - utility/specenum_gen.h (python3)
# También baja zlib 1.3.1 (licencia zlib) a userland/terceros/zlib/fuentes/ (capa reutilizable).
#
#   tools/freeciv_fuentes.sh [--tarball FICHERO] [--forzar] [--zlib-tarball FICHERO]
#
# Al final deja userland/terceros/freeciv/fuentes/.preparado: el Makefile solo compila Freeciv si existe.
set -euo pipefail

VERSAO=3.1.6
URL="https://github.com/freeciv/freeciv/archive/refs/tags/R3_1_6.tar.gz"
SHA256=acf6d8027ddec4682287d2cbd38cc1479dbad6fe7676f3f6e7213c6bc2beb640
ZVER=1.3.1
ZURL="https://github.com/madler/zlib/releases/download/v${ZVER}/zlib-${ZVER}.tar.gz"
ZSHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${FREECIV_DEST:-$RAIZ/userland/terceros/freeciv/fuentes}"   # FREECIV_DEST / ZLIB_DEST: para probar en una carpeta temporal
ZDEST="${ZLIB_DEST:-$RAIZ/userland/terceros/zlib/fuentes}"
TARBALL=""; ZTARBALL=""; FORZAR=0
while [ $# -gt 0 ]; do
    case "$1" in
        --tarball) TARBALL="$2"; shift 2 ;;
        --zlib-tarball) ZTARBALL="$2"; shift 2 ;;
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,19p' "$0"; exit 0 ;;
        *) echo "freeciv_fuentes.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done
for h in curl sha256sum tar python3; do
    command -v "$h" >/dev/null 2>&1 || { echo "freeciv_fuentes.sh: falta $h" >&2; exit 1; }
done
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

obtener() {   # URL SHA256 FICHERO_OPCIONAL SALIDA
    if [ -z "$3" ]; then
        echo "freeciv_fuentes.sh: descargando $1"
        curl -fsSL -o "$4" "$1"
    else
        cp "$3" "$4"
    fi
    echo "$2  $4" | sha256sum -c --quiet - || { echo "freeciv_fuentes.sh: SHA-256 incorrecto en $1" >&2; exit 1; }
}

# ---- zlib ----
if [ "$FORZAR" = 1 ] || [ ! -f "$ZDEST/.preparado" ]; then
    obtener "$ZURL" "$ZSHA256" "$ZTARBALL" "$TMP/zlib.tar.gz"
    rm -rf "$ZDEST"; mkdir -p "$ZDEST"
    tar xzf "$TMP/zlib.tar.gz" -C "$ZDEST" --strip-components=1
    echo "$ZVER" > "$ZDEST/.preparado"
fi

# ---- Freeciv ----
if [ "$FORZAR" = 0 ] && [ -f "$DEST/.preparado" ]; then
    echo "freeciv_fuentes.sh: ya preparado ($(cat "$DEST/.preparado"))"; exit 0
fi
obtener "$URL" "$SHA256" "$TARBALL" "$TMP/freeciv.tar.gz"
mkdir "$TMP/x"
tar xzf "$TMP/freeciv.tar.gz" -C "$TMP/x" --strip-components=1 \
    --exclude='*/data/graphics' --exclude='*/windows' --exclude='*/platforms' --exclude='*/flatpak' \
    --exclude='*/ChangeLog*' --exclude='*/client/gui-gtk*' --exclude='*/client/gui-qt' --exclude='*/doc' \
    --exclude='*/tools' --exclude='*/tests' --exclude='*/data/scenarios'
# traducciones: solo el español (D374)
mkdir -p "$TMP/tr"
tar xzf "$TMP/freeciv.tar.gz" -C "$TMP/tr" --strip-components=1 \
    --wildcards '*/translations/core/es.po' '*/translations/nations/es.po' '*/translations/ruledit/es.po' 2>/dev/null || true
rm -rf "$TMP/x/translations"
mkdir -p "$TMP/x/translations"
[ -d "$TMP/tr/translations" ] && cp -r "$TMP/tr/translations/." "$TMP/x/translations/"
( cd "$TMP/x/common" && ./generate_packets.py packets_gen.h packets_gen.c ../client/packhand_gen.h ../client/packhand_gen.c ../server/hand_gen.h ../server/hand_gen.c )
( cd "$TMP/x/utility" && ./generate_specenum.py specenum_gen.h )
# tolua: el generador de las enlaces de Lua (el upstream lo compila con el cc del anfitrión y lo usa
# al construir; los tarballs de GitHub no traen los *_gen.[ch], los de las publicaciones sí)
HCC="${HOSTCC:-cc}"
mkdir "$TMP/tolua"
echo '#define HAVE_GMTIME_R 1' > "$TMP/tolua/liblua_config.h"
( cd "$TMP/x/dependencies" && env -u CPATH -u C_INCLUDE_PATH $HCC -O1 -w -I"$TMP/tolua" -Ilua-5.4/src -Itolua-5.2/include -DLUA_USE_POSIX \
    $(ls lua-5.4/src/*.c | grep -v '/lua\.c$\|/luac\.c$') tolua-5.2/src/lib/*.c tolua-5.2/src/bin/tolua.c tolua-5.2/src/bin/toluabind.c \
    -lm -o "$TMP/tolua/tolua" )
gerar_tolua() {   # carpeta nombre pkg
    ( cd "$TMP/x/$1" && "$TMP/tolua/tolua" -n "$2" -o "tolua_$2_gen.c" -H "tolua_$2_gen.h" "tolua_$2.pkg" )
}
gerar_tolua common/scriptcore common_a; gerar_tolua common/scriptcore common_z
gerar_tolua common/scriptcore game; gerar_tolua common/scriptcore signal
gerar_tolua client/luascript client
gerar_tolua server/scripting fcdb; gerar_tolua server/scripting server
sh "$TMP/x/gen_headers/generate_version_header.sh" "$TMP/x/version_gen.h"
# el parche de SaguiOS (D374): el cliente lanza el servidor con executar() en vez de fork()
patch -p1 -d "$TMP/x" < "$RAIZ/userland/terceros/freeciv/parches/sagui.patch" > "$TMP/patch.log" 2>&1 || { cat "$TMP/patch.log" >&2; echo "freeciv_fuentes.sh: el parche de SaguiOS no se aplica a $VERSAO: no se deja nada preparado" >&2; exit 1; }
rm -rf "$DEST"; mkdir -p "$(dirname "$DEST")"
mv "$TMP/x" "$DEST"
echo "$VERSAO" > "$DEST/.preparado"
echo "freeciv_fuentes.sh: listo en $DEST"
