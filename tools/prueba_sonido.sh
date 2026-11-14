#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# prueba_sonido.sh - arranca build/saguios.img en QEMU con una tarjeta Intel HDA (sonido por
# PulseAudio/WSLg) y un pendrive virtual con tonos de prueba generados aquí (sin material ajeno).
# En SaguiOS: "montar usb0" y "reproducir /usb/escala.wav" (o la440, izq_der, barrido).
# Para añadir un audio propio: ffmpeg -i x.mp3 -ac 2 -ar 44100 -c:a pcm_s16le y.wav
#                              mcopy -i ~/.cache/sagui-sonido/sonidos.img y.wav ::/
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="$HOME/.cache/sagui-sonido"; mkdir -p "$DIR"
[ -f "$HOME/.local/saguitools/env.sh" ] && . "$HOME/.local/saguitools/env.sh"
if [ ! -f "$DIR/sonidos.img" ]; then
    python3 - "$DIR" <<'PY'
import math, os, struct, sys, wave
d = sys.argv[1]; fr = 44100
def wav(n, m, c=1):
    with wave.open(os.path.join(d, n), "wb") as w:
        w.setnchannels(c); w.setsampwidth(2); w.setframerate(fr)
        w.writeframes(b"".join(struct.pack("<" + "h"*c, *x) for x in m))
def tono(f, s, v=0.4):
    n = int(fr*s); return [(int(32767*v*math.sin(2*math.pi*f*i/fr)*min(1, i/400, (n-i)/400)),) for i in range(n)]
wav("la440.wav", tono(440, 2))
wav("escala.wav", sum((tono(f, 0.35) for f in (261.63, 293.66, 329.63, 349.23, 392, 440, 493.88, 523.25)), []))
n = int(fr*1.5)
wav("izq_der.wav", [(int(12000*math.sin(2*math.pi*440*i/fr)), 0) for i in range(n)] +
                   [(0, int(12000*math.sin(2*math.pi*660*i/fr))) for i in range(n)], 2)
wav("barrido.wav", [(int(12000*math.sin(2*math.pi*(200*i/fr + 300*(i/fr)**2))),) for i in range(fr*3)])
PY
    truncate -s 34M "$DIR/sonidos.img"
    mkfs.vfat -F 32 -n SONIDOS "$DIR/sonidos.img" >/dev/null
    mcopy -i "$DIR/sonidos.img" "$DIR"/*.wav ::/ && rm -f "$DIR"/*.wav
fi
cp "$RAIZ/build/saguios.img" "$DIR/saguios.img"
CMD="$(command -v qemu-system-i386) -m 512 -name SaguiOS-sonido \
 -drive format=raw,file=$DIR/saguios.img,if=ide,snapshot=on \
 -audiodev pa,id=snd0 -device intel-hda -device hda-duplex,audiodev=snd0 \
 -device usb-ehci,id=ehci -drive if=none,id=pen,format=raw,file=$DIR/sonidos.img,snapshot=on \
 -device usb-storage,bus=ehci.0,drive=pen -display sdl"
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then exec $CMD -accel kvm -accel tcg
elif getent group kvm | grep -qw "$(id -un)"; then exec sg kvm -c "$CMD -accel kvm -accel tcg"
else exec $CMD; fi
