# 048 - Vídeo: códec abierto a 240p/30 fps y reproductor

- **Estado:** en curso (fase 1 hecha: MJPEG en AVI; fase 2 del MPEG-1 hecha: el MPEG-1 en
  MPEG-PS descodifica y acierta con el ffmpeg, ~48 dB en todos los cuadros, D137 y D144)
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Depende de:** 047 (JPEG, para la fase 1); 041 (sonido) para el audio
- **Relacionadas:** 049 (HMKD embebe estos vídeos)

## Objetivo

Reproducir en SaguiOS vídeos de **426x240 (240p) a 30 fps**, generados en el PC del usuario
con **ffmpeg**, con un reproductor propio. Debe ir fluido incluso en un **Pentium III**
(036). El decodificador se escribe aquí, a partir de las especificaciones.

## Elección del códec

| Opción | Complejidad del decodificador | Coste de CPU | Tamaño del archivo (240p, 1 min) | Patentes |
| --- | --- | --- | --- | --- |
| **MJPEG** (cada cuadro, un JPEG) en AVI | **mínima**: reutiliza el JPEG de la 047 | bajo-medio | grande (~15-25 MB) | libre |
| **MPEG-1 vídeo** (+ audio **MP2**) en MPEG-PS | baja: ~2.000-3.000 líneas, estándar de 1993 | **muy bajo** (pensado para CPU de 1993) | pequeño (~3-6 MB) | **caducadas**; libre de usar |
| Theora (VP3) en Ogg | media-alta | medio | pequeño | libre |
| VP8/VP9/AV1 en WebM | alta a muy alta | alto (no apto para un P3) | muy pequeño | libre, pero desproporcionado aquí |

**Elección:**
- **Fase 1, MJPEG en AVI:** casi gratis una vez hecho el JPEG. Sirve para validar el
  reproductor, la sincronización y el escalado.
- **Fase 2, MPEG-1 + MP2 en MPEG-PS (`.mpg`):** el formato «oficial» de SaguiOS. Es pequeño y
  rapidísimo de decodificar, y ffmpeg lo genera sin opciones raras.

## Generación en el PC (ffmpeg)

```bash
# Fase 1: MJPEG (+ PCM) en AVI
ffmpeg -i entrada.mp4 -vf "scale=-2:240,fps=30" -c:v mjpeg -q:v 5 \
       -c:a pcm_s16le -ar 22050 -ac 1 salida.avi

# Fase 2: MPEG-1 + MP2 en MPEG-PS (formato recomendado)
ffmpeg -i entrada.mp4 -vf "scale=-2:240,fps=30" -c:v mpeg1video -b:v 600k -bf 2 \
       -c:a mp2 -b:a 96k -ar 44100 -ac 2 -f mpeg salida.mpg
```

Un script `tools/convertir-video.sh` envolverá estas órdenes. La documentación dirá el
tamaño y los fps recomendados por tipo de PC: 240p/30 para Pentium III y 360p para Core Duo
en adelante.

## Reproductor (`reproductor`, «Reproductor de vídeo»)

- Abre `.avi` (MJPEG/PCM) y `.mpg` (MPEG-1/MP2); doble clic en Archivos.
- Controles:
  - reproducir/pausa (Espacio);
  - avanzar/retroceder (flechas: 5 s; MPEG-1 busca al GOP más cercano);
  - barra de posición con tiempo (usa la barra de progreso de la 042, si existe);
  - volumen y silencio;
  - bucle;
  - pantalla completa (F11) con **escalado** (vecino o bilineal SSE2) y proporción
    conservada.
- **Sincronización:**
  - con sonido (041), el reloj maestro es el audio;
  - sin driver de sonido, el reloj del sistema, y el vídeo se reproduce en silencio (avisa
    una vez).
  - Si la CPU no llega, salta cuadros (B primero) en vez de ir a cámara lenta.
- Conversión YUV 4:2:0 → RGB en C, con SSE2 si hay.
- Estadísticas con `I`: fps, cuadros saltados y tiempo de decodificación.
- `reproductor -t archivo` decodifica sin ventana y escribe sumas por cuadro, para las pruebas.
- **API en la libsagui** (`video_abrir`, `video_cuadro`, `video_audio`...), para que la usen
  el visor HMKD (049), Cipó, tkinter y Python.

## Criterios de aceptación

- [x] **MJPEG/AVI** a 240p/30 fps reproducidos a tiempo real (a 19,6 fps en TCG, que es
      el peor caso; con KVM, más deprisa); objetivo: Pentium III real
- [ ] **MPEG-1/MPEG-PS**: el reproductor reconoce el archivo y avisa de que todavía no
      se decodifica (D129). **PENDIENTE**: falta el decodificador de MPEG-1
- [x] Cuadros comparados con ffmpeg en el anfitrión (PSNR de 44,6 dB, con un umbral de 35)
- [x] Pausa, búsqueda, pantalla completa y salto de cuadros
- [x] Audio sincronizado cuando exista la 041 (el audio del AVI es el reloj maestro);
      en silencio sin ella (y avisa)
- [x] `tools/convertir-video.sh` y un vídeo de ejemplo propio (generado, sin material ajeno)
      en `/videos` (`sagui.avi`, 426x240, 30 fps, 2 s)
- [x] Test automático en `tests/` (`tests/h100_video_v62.py`)
- [x] Decisión registrada en `docs/decisoes.md` (D129)

## Lo que hay ahora (v6.2, D129)

- `userland/libsagui/video.c` (`video.h`): lee el AVI con vídeo MJPEG y su audio PCM,
  y entrega los cuadros uno a uno (`video_quadro`) y el audio del intervalo
  (`video_audio`).
- `userland/programas/reproductor.c`: el reproductor con ventana (pausa, búsqueda de
  5 s, salto de cuadros, bucle, volumen, barra de posición, pantalla completa,
  estadísticas) y los modos `-t` y `--volcar` para las pruebas.
- `tools/convertir-video.sh`: convierte un vídeo del PC a MJPEG/AVI (o a MPEG-1/MPEG-PS,
  que todavía no se reproduce).
- `tools/mkvideo.py`: dibuja los cuadros del vídeo de ejemplo y los codifica con
  ffmpeg; es lo que usa también la prueba.
- `/videos/sagui.avi`: el vídeo de ejemplo (426x240, 30 fps, 2 s), propio.
- `tests/h100_video_v62.py`: la prueba (anfitrión, dentro de SaguiOS, la ventana y
  el sonido).

## Lo que falta

- El decodificador de **MPEG-1 en MPEG-PS** (las tablas de Huffman de la ITU-T H.261,
  la compensación de movimiento y el reordenamiento de los cuadros B).
- El audio comprimido (MP2) dentro del AVI.
- El doble clic en un `.avi` dentro del programa Archivos.
- La API en el MicroPython y el uso desde el visor del HMKD (mejora 049).

## Notas

- Los vídeos de ejemplo deben ser **propios**: animaciones generadas por un script (fractales
  o el sagüí), nada con derechos de terceros.
- MPEG-1 incluye los cuadros B (bidireccionales): hay que reordenar la presentación.

## Estado de la fase 2 (MPEG-1 en MPEG-PS)

Hecho (D137): el demultiplexador MPEG-PS y el decodificador de vídeo MPEG-1, escritos desde
cero en `userland/libsagui/mpeg1.c` (en enteros, sin coma flotante), con la transformada
inversa de la DCT compartida con el JPEG (`imagem_idct.c`). El `reproductor` abre los `.mpg` y
muestra los cuadros.

Estado del contraste con el ffmpeg (`tests/h115_video_mpeg1.py`, `tools/mpeg1_psnr.txt`):

| parte | estado |
| --- | --- |
| cuadros I (la parte de la DCT) | bien: los coeficientes coinciden con los del ffmpeg |
| conversión YCbCr a RGB | bien: usa el alcance limitado (del estudio), como hace el ffmpeg en el MPEG-1 |
| cuadros P y B | bien: la compensación de movimiento es la de la norma (7.6.3) y las referencias de delante y de detrás son las ciertas (en el MPEG-1 el vector "de delante" apunta al cuadro **anterior**) |

El objetivo eran 35 dB en todos los cuadros y se llegó a ~48 dB de media (el peor cuadro
queda en 47 dB). La prueba `h115` ya exige los 35 dB en cada cuadro.

Lo que faltaba y se corrigió en la fase 2 (D144):

- el primer coeficiente de los bloques inter (el caso especial "1" + signo) nunca se leía, lo
  que desalineaba el flujo dos bits por bloque y estropeaba todos los cuadros P y B;
- en los cuadros B, la media con la referencia de atrás sólo se hace cuando también hay
  predicción de delante (y el macrobloque saltado hereda la dirección del anterior);
- el último cuadro del flujo nunca se cerraba (sólo se cerraba cuando venía el código de
  inicio del cuadro siguiente).
