# 024 - Visor de imágenes y biblioteca de imágenes

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

SaguiOS no podía mostrar ninguna imagen: no había lectores de formatos gráficos ni un programa
para verlas. Archivos abría cualquier archivo que no fuera Markdown en el Editor de texto.

## Propuesta (aplicada)

- **libsagui:** `userland/libsagui/imagem.c` + `userland/inc/imagem.h` (D054): lee BMP (1, 4,
  8, 16, 24 y 32 bits), PBM/PGM/PPM (P1-P6) y PNG (todos los tipos de color, 1-16 bits,
  Adam7) con un inflate propio; escribe BMP de 24 bits y PPM; escala con vecino más cercano o
  bilineal en coma fija (sin float).
- **Programa** `userland/programas/visor.c` (D055): ajustar/1:1, zoom con rueda y +/-,
  arrastre y flechas, imagen anterior/siguiente de la carpeta, F11, barra de estado, ventana
  redimensionable; `visor -i` para el terminal.
- **Integración:** icono «Visor» en el escritorio (`kernel/gui/arte.c`,
  `kernel/gui/area_trabalho.c`), Archivos abre las imágenes con el visor, `ajuda visor`.
- **Ejemplos:** `tools/mkimagens.py` genera `rootfs/imagenes/` (9 imágenes, 174 KB).

## Criterios de aceptación

- [x] BMP 24/32 bits (lectura y escritura) y BMP de 8 bits con paleta
- [x] PPM/PGM P6/P5 y P3
- [x] PNG con inflate propio
- [x] Ajustar, 1:1, zoom con rueda y +/-, arrastrar, flechas
- [x] Imagen anterior/siguiente de la carpeta
- [x] Pantalla completa (F11), barra de estado, redimensionable y maximizable
- [x] Doble clic en Archivos abre el visor; icono en el escritorio
- [x] Test automático `tests/h56_visor_imagenes.py`
- [x] Decisiones registradas en `docs/decisoes.md` (D054, D055)

## Notas

- Pendiente: JPEG y GIF (el GIF necesitaría LZW; el JPEG, DCT y Huffman propios); BMP con RLE.
- El inflate decodifica bit a bit; una tabla de búsqueda lo haría varias veces más rápido con
  PNG grandes.
- La transparencia del PNG se mezcla con blanco (no hay fondo a cuadros).
