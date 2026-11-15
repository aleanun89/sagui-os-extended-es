# FAAD2: libros de Huffman del AAC (D262, D276)

**Code from FAAD2 is copyright (c) Nero AG, www.nero.com**

- **Origen:** las tablas de decodificación de Huffman de [FAAD2](https://github.com/knik0/faad2)
  (M. Bakker, Ahead Software AG / Nero AG). Los códigos son los libros de Huffman de la norma del AAC
  (ISO/IEC 14496-3, anexo 4.A), pero los números se sacaron de las tablas de FAAD2.
- **Licencia:** GPL-2.0 **o (a elección) cualquier versión posterior**: compatible con la GPL-3.0 del
  conjunto. `COPYING` es el texto con el preámbulo de FAAD2, que sigue valiendo:
  - «Any non-GPL usage of this software or parts of this software is strictly forbidden.» (SaguiOS es
    GPL: el uso es GPL);
  - el aviso de copyright apropiado (sección 2c de la GPL 2) es la frase en negrita de arriba, que
    también está en `aac_huff.h` y en `userland/libsagui/aac.c`.
- **Cómo llegó aquí:** el reproductor de vídeo del Cipó de robson-apk (PR #6 de `erigobeli/sagui-os`,
  mejora 100, D276), como `libsagui/aac_huff.h` ya con el crédito a FAAD2; separado por robson-apk en
  su rama `main` (commit `65e3b74`), de donde se tomó tal cual.
- **Qué cambió:** nada en los números (formato `(tamaño << 24) | código` por libro, en el orden de la
  norma). Se añadió la línea SPDX.
- **Quién lo usa:** `userland/libsagui/aac.c`, que usan el reproductor y el Cipó.
- **Prueba:** `tests/h276_video_h264.py` (tono de 440 Hz recuperado del audio decodificado).
