#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# convertir-video.sh - prepara video para SaguiOS con ffmpeg (mejora 048).
#
# Uso: tools/convertir-video.sh ENTRADA FORMATO [SALIDA]
#   FORMATO:
#     mjpeg   MJPEG dentro de un AVI: cada cuadro es un JPEG. Es lo que reproduce
#             hoy el reproductor de SaguiOS (video.c). Sonde y se ve bien; el
#             archivo pesa mas (unos 5-10 MB por minuto a 240p).
#     mpeg1   MPEG-1 en MPEG-PS (.mpg): el formato pequeno y rapidisimo de
#             decodificar, pero el decodificador de MPEG-1 todavia no esta
#             escrito en SaguiOS (D129): el reproductor avisa de que no lo sabe
#             leer. Se deja la opcion para tener los archivos listos.
#
# Por defecto se hace 426x240 a 30 fps (lo que recomienda la ficha de la mejora
# 048 para un Pentium III). Se cambia con las variables de entorno:
#   ALT=360  (mas alto), FPS=25, QUALIDADE=5 (mejor calidad, mas peso),
#   AUDIO=none (sin pista de audio)
#
# El audio del AVI va en PCM de 16 bits (lo que lleva sin comprimir), porque el
# reproductor de SaguiOS todavia no decodifica MP2 dentro del AVI. El MPEG-PS
# lleva MP2, que es lo que ese formato admite.
set -e
[ $# -ge 2 ] || { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
ent=$1
fmt=$2
base=${3:-${ent%.*}}
alt=${ALT:-240}
fps=${FPS:-30}
qualidade=${QUALIDADE:-5}
larg=${LARG:-$((426 * alt / 240))}
[ "$larg" -gt 0 ] 2>/dev/null || larg=426
# Un ancho par es lo que espera el MPEG-1; para el MJPEG da igual, pero se
# redondea hacia abajo para que quepa en el AVI sin rarezas.
larg=$((larg / 2 * 2))
case $fmt in
  mjpeg) ext=avi; opc="-c:v mjpeg -q:v ${qualidade} -f avi"; audio_pcm=1 ;;
  mpeg1) ext=mpg; opc="-c:v mpeg1video -b:v 600k -bf 2 -f mpeg"; audio_pcm=0 ;;
  *) echo "formato desconocido: $fmt (mjpeg, mpeg1)" >&2; exit 2 ;;
esac
case $base in *."$ext") saida=$base ;; *) saida=$base.$ext ;; esac
[ "$saida" != "$ent" ] || saida=${base%.*}-$fmt.$ext
if [ "${AUDIO:-sim}" = "none" ]; then
  opc_audio="-an"
elif [ "$audio_pcm" = "1" ]; then
  # En el AVI el audio va en PCM de 16 bits (lo que el reproductor sabe leer).
  opc_audio="-c:a pcm_s16le -ar 22050 -ac 1"
else
  # El MPEG-PS no admite PCM: lleva MP2.
  opc_audio="-c:a mp2 -b:a 96k -ar 44100 -ac 2"
fi
# shellcheck disable=SC2086
ffmpeg -hide_banner -y -i "$ent" -vf "scale=${larg}:${alt},fps=${fps}" \
       $opc_audio $opc "$saida"
echo "listo: $saida (${larg}x${alt}, ${fps} fps)"
if [ "$fmt" = "mpeg1" ]; then
  echo "aviso: SaguiOS todavia no decodifica MPEG-1 (D129); el reproductor avisara de ello"
fi
