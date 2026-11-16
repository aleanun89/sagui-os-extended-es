# shine (codificador MP3 de aritmética entera) en SaguiOS (D339)

- **Origen:** https://github.com/toots/shine (el fork de savonet; la biblioteca de Gabriel Bouvigne, Pete Everett y colaboradores
  sobre el codificador de ISO dist10), commit `ab5e3526b64af1a2eaa43aa6f441a7312e013519` (2023-01-01).
- **Licencia:** LGPL-2.0 o posterior (el texto, en `COPYING`: GNU Library General Public License v2). Compatible con la GPL v3 del
  proyecto (la LGPL permite combinarla con GPL v3 al enlazar; el aviso y el texto van en `/apps/shine/`).
- **Qué se copió:** solo `src/lib/` del upstream (los .c y .h de la biblioteca) en `src/`, **sin modificar ningún fichero**, más
  `COPYING` y el `README.md` del upstream. No se copian el programa de línea de órdenes (`src/bin/`), los scripts de Autotools ni el
  puerto de JavaScript.
- **Uso:** solo lo enlaza `convertir-audio` (el Conversor de audio, `userland/programas/convertir-audio.c`) con `shine.mk`. Usa
  `double` y la libm en la inicialización (tablas), por eso se compila con la x87 (`-mfpmath=387`) y la libc común del sistema; el
  resto del sistema no lo toca. No entra en la variante sin GPL (`SEM_GPL=1`), donde el conversor sale sin MP3.
- **Cómo se comprueba:** `tests/h341_conversor_audio.py` (el MP3 resultante lo decodifica ffprobe/ffmpeg y el decodificador propio).
