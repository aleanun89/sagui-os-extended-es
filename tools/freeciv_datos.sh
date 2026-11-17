#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# freeciv_datos.sh - arma la carpeta que va a /apps/freeciv (D374).
#
#   tools/freeciv_datos.sh FUENTES DESTINO LEEME
#
# FUENTES es userland/terceros/freeciv/fuentes. Deja en DESTINO:
#   data/    las reglas (rulesets), los tilesets, el tema y lo demás que el cliente y el servidor
#            leen, recortado (con sonidos y música (Ogg Vorbis), sin los tilesets que no usamos);
#   locale/  los catálogos .mo (de translations/*/es.po; el resto de idiomas no se bajan);
#   COPYING, leeme.txt.
set -eu
FUENTES="${1:?falta FUENTES}"
DESTINO="${2:?falta DESTINO}"
LEEME="${3:?falta LEEME}"
D="$FUENTES/data"
mkdir -p "$DESTINO/data" "$DESTINO/locale/es/LC_MESSAGES"
( cd "$D" && tar cf - \
    --exclude=./Makefile.am --exclude='*/Makefile.am' --exclude=./cimpletoon --exclude=./cimpletoon.tilespec \
    --exclude=./alio --exclude=./alio.tilespec \
    --exclude=./toonhex.tilespec --exclude=./experimental --exclude=./experimental.serv \
    --exclude=./webperimental --exclude=./webperimental.serv --exclude=./ruledit --exclude=./icons \
    --exclude=./freeciv-client.icns --exclude=./freeciv-server.icns --exclude=./freeciv-modpack.png \
    --exclude=./freeciv-ruledit.png --exclude=./gtk3_menus.xml --exclude=./gtk3.22_menus.xml \
    --exclude=./themes/gtk3 --exclude=./themes/gtk3.22 --exclude=./themes/gtk4 --exclude=./themes/gui-qt \
    --exclude=./scenarios --exclude='*.svg' --exclude=./themes/gui-sdl2/human/fireflysung.ttf \
    --exclude=./themes/gui-sdl2/human/sazanami-gothic.ttf --exclude=./themes/gui-sdl2/human/UnDotum.ttf \
    . ) | ( cd "$DESTINO/data" && tar xf - )
python3 -I "$(dirname "$0")/freeciv_mo.py" "$FUENTES/translations/core/es.po" "$(dirname "$0")/../userland/terceros/freeciv/traducciones/es-sagui.po" "$DESTINO/locale/es/LC_MESSAGES/freeciv-core.mo"
python3 -I "$(dirname "$0")/freeciv_mo.py" "$FUENTES/translations/nations/es.po" "$DESTINO/locale/es/LC_MESSAGES/freeciv-nations.mo"
cp "$FUENTES/COPYING" "$DESTINO/COPYING"
cp "$LEEME" "$DESTINO/leeme.txt"
