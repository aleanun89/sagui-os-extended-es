#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# convertir-audio.sh - prepara audio para SaguiOS con ffmpeg (mejora 053).
#
# Uso: tools/convertir-audio.sh ENTRADA FORMATO [SALIDA]
#   FORMATO:
#     mp3     MP3 VBR de calidad ~4 (~165 kbit/s), 44,1 kHz estéreo: lo más pequeño y compatible
#     mp3-bajo  MP3 de 96 kbit/s a 22,05 kHz estéreo (MPEG-2): para un pendrive lento o poca RAM
#     flac    FLAC sin pérdidas, 16 bits (si la entrada es de 24 bits, se baja a 16 al tocar)
#     adpcm   WAV IMA ADPCM, 22,05 kHz mono (x4 menor que el WAV, casi sin CPU: Pentium antiguos)
#     wav     WAV PCM de 16 bits, 44,1 kHz estéreo (lo que toca reproducir sin decodificar)
#
# SaguiOS decodifica con código propio MP3 (MPEG-1/2 Layer III, no MPEG-2.5 de 8-12 kHz), FLAC (hasta 2
# canales y 24 bits) y WAV PCM/IMA ADPCM; Ogg Vorbis está pendiente. Se conservan título, artista y álbum
# (ID3v2.3 en MP3, VORBIS_COMMENT en FLAC); se quita la carátula (-vn) porque solo estorba.
# Opciones por variables de entorno: TASA (Hz) y CANALES (1 o 2) para forzar otra frecuencia o mono.
set -e
[ $# -ge 2 ] || { sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
ent=$1
fmt=$2
base=${3:-${ent%.*}}
case $fmt in
  mp3)      ext=mp3;  opc="-c:a libmp3lame -q:a 4 -ar ${TASA:-44100} -ac ${CANALES:-2} -id3v2_version 3" ;;
  mp3-bajo) ext=mp3;  opc="-c:a libmp3lame -b:a 96k -ar ${TASA:-22050} -ac ${CANALES:-2} -id3v2_version 3" ;;
  flac)     ext=flac; opc="-c:a flac -compression_level 8 -sample_fmt s16 ${TASA:+-ar $TASA} ${CANALES:+-ac $CANALES}" ;;
  adpcm)    ext=wav;  opc="-c:a adpcm_ima_wav -ar ${TASA:-22050} -ac ${CANALES:-1}" ;;
  wav)      ext=wav;  opc="-c:a pcm_s16le -ar ${TASA:-44100} -ac ${CANALES:-2}" ;;
  *) echo "formato desconocido: $fmt (mp3, mp3-bajo, flac, adpcm, wav)" >&2; exit 2 ;;
esac
# si la salida no lleva extensión se la pone; con extensión igual a la de entrada se añade el formato
case $base in *."$ext") salida=$base ;; *) salida=$base.$ext ;; esac
[ "$salida" != "$ent" ] || salida=${base%.*}-$fmt.$ext
# shellcheck disable=SC2086
ffmpeg -hide_banner -y -i "$ent" -vn -map_metadata 0 $opc "$salida"
echo "listo: $salida (en SaguiOS: reproducir $salida)"
