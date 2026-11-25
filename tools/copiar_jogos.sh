#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# copiar_jogos.sh - arma la raíz de la partición de juegos SAGUI-JOGOS (D181, D228).
#
#   [CNC_VIDEOS=1] tools/copiar_jogos.sh DESTINO DIR_OPENTYRIAN DIR_ROMS_CHIP8 DIR_DATOS_OPENJAZZ OPENJAZZ_DATOS INPUT_SHARE \
#                         [DIR_DATOS_QUAKE QUAKE_DATOS]
#
# Lo llama el Makefile ($(B)/jogos/.feito). Deja en DESTINO/raiz lo que va en la
# partición, que el /etc/fstab monta en /jogos:
#   - rootfs/jogos/ (jclic, sagui3d, las fuentes de chip8) y las ROM de chip8: lo
#     mismo que el ramdisk tiene en /jogos, que el montaje tapa;
#   - opentyrian/: los datos de la imagen versionada del disco aparte (D156);
#   - openjazz/: el shareware si se bajó (tools/openjazz_datos.sh) y OPENJAZZ_DATOS
#     no es 0, más el leeme del SaguiOS;
#   - quake/: el shareware de Quake si se bajó (tools/quake_datos.sh, D229) y
#     QUAKE_DATOS no es 0, más el leeme del SaguiOS;
#   - gb/: el leeme de las ROM de Game Boy (D235; las ROM, nunca: INPUT_SHARE/games/gb las trae);
#   - INPUT_SHARE/games/<carpeta> en <carpeta> (docs/input_share.md); INPUT_SHARE
#     vacío = nada. Salvo INPUT_SHARE/games/CnC con las ISO de C&C Gold (D230): de
#     ahí no se copian las ISO sino lo que saca tools/cnc_extraer.py (con las
#     películas si la variable de entorno CNC_VIDEOS no es 0), que se guarda en DESTINO/cnc para no
#     descomprimirlo cada vez.
# Todo se prepara en DESTINO/tmp.* y solo al final se renombra a DESTINO/raiz.
set -eu

DEST="${1:?falta DESTINO}"
OT_DIR="${2:?falta la carpeta del OpenTyrian}"
ROMS="${3:?falta la carpeta de las ROM de chip8}"
OJ_DATOS="${4:?falta la carpeta de datos de OpenJazz}"
OJ_INCLUIR="${5:-1}"
ENTRADA="${6:-}"
CNC_VIDEOS="${CNC_VIDEOS:-0}"
QK_DATOS="${7:-}"
QK_INCLUIR="${8:-1}"

command -v mcopy >/dev/null || { echo "copiar_jogos: falta el mcopy (mtools); cargue ~/.local/saguitools/env.sh" >&2; exit 1; }

mkdir -p "$DEST"
TMP="$(mktemp -d "$DEST/tmp.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
R="$TMP/raiz"
mkdir "$R"

# Lo del ramdisk.
cp -r rootfs/jogos/. "$R/"
mkdir -p "$R/chip8"
cp "$ROMS"/* "$R/chip8/"

# OpenTyrian (D156, D181).
xz -dc "$OT_DIR/dados.img.xz" > "$TMP/dados.img"
mkdir "$R/opentyrian"
MTOOLS_SKIP_CHECK=1 mcopy -s -n -m -i "$TMP/dados.img" "::/jogos/opentyrian/*" "$R/opentyrian/"
test -f "$R/opentyrian/tyrian1.lvl"
cp "$OT_DIR/dados-leeme.txt" "$R/opentyrian/leeme.txt"
rm -f "$TMP/dados.img"

# OpenJazz (D228): el shareware, sin quitar ni cambiar nada (lo pide su LICENSE.DOC).
mkdir "$R/openjazz"
if [ "$OJ_INCLUIR" != 0 ] && [ -f "$OJ_DATOS/.preparado" ]; then
    cp -r "$OJ_DATOS/jazz/." "$R/openjazz/"
    echo "copiar_jogos: shareware de Jazz Jackrabbit -> /jogos/openjazz"
fi
cp userland/terceros/openjazz/datos-leeme.txt "$R/openjazz/leeme-saguios.txt"

# Quake (D229): el shareware entero y sin cambios (lo pide su licencia, SLICNSE.TXT).
mkdir "$R/quake"
if [ -n "$QK_DATOS" ] && [ "$QK_INCLUIR" != 0 ] && [ -f "$QK_DATOS/.preparado" ]; then
    cp -r "$QK_DATOS/quake/." "$R/quake/"
    echo "copiar_jogos: shareware de Quake -> /jogos/quake"
fi
cp userland/terceros/quake/datos-leeme.txt "$R/quake/leeme-saguios.txt"

# Game Boy (D235): la carpeta de las ROM (que no vienen con el sistema) con un leeme; lo que
# haya en input_share/games/gb se mezcla debajo, con el resto de input_share/games.
mkdir "$R/gb"
cp userland/terceros/gnuboy/roms-leeme.txt "$R/gb/leeme-saguios.txt"

# input_share/games (D228).
if [ -n "$ENTRADA" ] && [ -d "$ENTRADA/games" ]; then
    for g in "$ENTRADA"/games/*/; do
        [ -d "$g" ] || continue
        n="$(basename "$g")"
        iso=0
        for i in "$g"*.iso "$g"*.ISO; do [ -f "$i" ] && iso=1; done
        if [ "$n" = "CnC" ] && [ "$iso" = 1 ]; then
            # Command & Conquer (D230): de las ISO, solo lo que usa el motor.
            if [ "$CNC_VIDEOS" != 0 ]; then
                python3 tools/cnc_extraer.py --videos --buscar "$g" "$DEST/cnc"
            else
                python3 tools/cnc_extraer.py --buscar "$g" "$DEST/cnc"
            fi
            mkdir -p "$R/CnC"
            # Enlaces duros (la misma partición del PC): las películas pesan 880 MB.
            cp -rl "$DEST/cnc/." "$R/CnC/" 2>/dev/null || cp -r "$DEST/cnc/." "$R/CnC/"
            echo "copiar_jogos: $ENTRADA/games/CnC (ISO de C&C) -> /jogos/CnC"
            continue
        fi
        # Tzar (D417) va aparte, más abajo: solo lo que lee el port.
        if [ "$n" = "TZAR" ] && [ -d "$g/Packs" ]; then
            continue
        fi
        # AoE (D382): si es el CD entero (con GAME/DATA), solo la carpeta GAME sin los vídeos
        # (AVI, 127 MB): el instalador, DirectX y las demos del CD no los usa AoEN, que
        # encuentra /jogos/AOE/GAME/DATA solo (userland/juegos/aoen/config.c).
        if [ "$n" = "AOE" ] && [ -d "$g/GAME/DATA" ]; then
            mkdir -p "$R/AOE"
            dest="$(cd "$R/AOE" && pwd)"
            ( cd "$g" && find GAME -path GAME/AVI -prune -o -type d -exec mkdir -p "$dest/{}" \; && \
              find GAME -path GAME/AVI -prune -o -type f ! -name '*:Zone.Identifier' -exec cp {} "$dest/{}" \; )
            echo "copiar_jogos: $ENTRADA/games/AOE (CD de AoE) -> /jogos/AOE/GAME (sin AVI)"
            continue
        fi
        # Las imágenes de disco (.iso, .bin, .img) no se copian tal cual: son los discos
        # originales (p. ej. las ISO de C&C, 1,2 GB); de ellas solo se extrae lo que el
        # juego necesita, con su herramienta (D230). Sin herramienta, no entran.
        mkdir -p "$R/$n"
        # Destino en ruta absoluta: dentro del «cd» una ruta relativa apuntaría a la
        # propia carpeta de origen (input_share) y find se recorrería a sí mismo.
        dest="$(cd "$R/$n" && pwd)"
        ( cd "$g" && find . -type d -exec mkdir -p "$dest/{}" \; && \
          find . -type f ! -iname '*.iso' ! -iname '*.bin' ! -iname '*.img' ! -name '*:Zone.Identifier' \
               -exec cp {} "$dest/{}" \; )
        echo "copiar_jogos: $ENTRADA/games/$n -> /jogos/$n (sin imágenes de disco)"
    done
fi
# Tzar (D417): la instalación del mantenedor, «INPUT_SHARE/Tzar Anthology», va a /jogos/TZAR con solo lo que
# lee el port (paquetes, mapas, campañas, la introducción y Tzar.ini); nada del Tzar.exe ni de sus DLL. Solo con
# INPUT_SHARE (imágenes privadas): las entregas lo dejan vacío. Unos 500 MB: hace falta JOGOS_MB grande.
# También en INPUT_SHARE/games/TZAR (la otra forma de guardarlo; antes entraba entera por el bucle general,
# con el .exe y las DLL).
t=""
if [ -n "$ENTRADA" ] && [ -d "$ENTRADA/Tzar Anthology/Packs" ]; then
    t="$ENTRADA/Tzar Anthology"
elif [ -n "$ENTRADA" ] && [ -d "$ENTRADA/games/TZAR/Packs" ]; then
    t="$ENTRADA/games/TZAR"
fi
if [ -n "$t" ]; then
    mkdir -p "$R/TZAR"
    for x in Packs Maps Campaign; do [ -d "$t/$x" ] && cp -r "$t/$x" "$R/TZAR/"; done
    for x in intro.MPG Tzar.ini; do [ -f "$t/$x" ] && cp "$t/$x" "$R/TZAR/"; done
    echo "copiar_jogos: $t -> /jogos/TZAR (paquetes, mapas, campañas e introducción)"
fi
# Las marcas de descarga de Windows no son del juego.
find "$R" -name '*:Zone.Identifier' -exec rm -f {} +

rm -rf "$DEST/raiz" "$DEST/opentyrian"
mv "$R" "$DEST/raiz"
echo "copiar_jogos: $(find "$DEST/raiz" -type f | wc -l) ficheros en $DEST/raiz"
