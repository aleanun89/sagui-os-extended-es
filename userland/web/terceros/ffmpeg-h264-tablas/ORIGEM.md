# FFmpeg: tablas del H.264 (CABAC y CAVLC) (D262, D276)

- **Origen:** los números de `libavcodec/h264_cabac.c`, `cabac.c` y `h264_cavlc.c` de
  [FFmpeg](https://ffmpeg.org). Los valores son los de la norma (ITU-T H.264 / ISO/IEC 14496-10:
  tablas 9-12 a 9-33 y 9-44/9-45 del CABAC; 9-4, 9-5, 9-7, 9-8 y 9-9 del CAVLC), pero la disposición
  (qué vectores, en qué orden y con qué forma) es la de FFmpeg.
- **Licencia:** **LGPL-2.1-o-posterior** (`LICENSE` es el texto de la LGPL 2.1; `LICENSE-FFmpeg.md` es el
  `LICENSE.md` de FFmpeg, que dice que la LGPL 2.1+ vale por defecto para libavcodec; ninguno de estos
  ficheros está en la lista de las partes GPL). Compatible con la GPL-3.0 del conjunto.
- **Cómo llegó aquí:** el reproductor de vídeo del Cipó de robson-apk (PR #6 de `erigobeli/sagui-os`,
  mejora 100, D276). Su `cabac_tab.h` decía «números extraídos del libavcodec/h264_cabac.c y cabac.c del
  FFmpeg» y las tablas CAVLC de `h264.c` siguen la disposición de FFmpeg; robson-apk lo reconoció el
  2026-11-14 (commit `8c3b56b` de `robson-apk/sagui-os`) y las separó en su rama `main`
  (commit `65e3b74`), de donde se tomaron tal cual.
- **Qué cambió:** nada en los números. Se añadió la línea SPDX al principio de `cabac_tab.h` y
  `cavlc_tab.h`.
- **Quién lo usa:** `userland/libsagui/h264.c` (los incluye), que usan el reproductor y el Cipó.
- **Prueba:** `tests/h276_video_h264.py` (cuadros comparados con ffmpeg).
