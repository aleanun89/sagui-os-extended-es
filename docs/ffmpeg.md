# FFmpeg dentro de SaguiOS (D458)

`/apps/bin/ffmpeg` y `/apps/bin/ffprobe`: FFmpeg 8.1.3 real, compilado con una configuración mínima (v8.5.6, rama
`v8.5.6-ffmpeg`). Ayuda de uso: `ayuda ffmpeg` (`rootfs/ayuda/ffmpeg.md`).

| | |
|---|---|
| Licencia | **LGPL-2.1 o posterior**: sin `--enable-gpl` ni `--enable-nonfree`, sin x264, x265, LAME ni fdk-aac (el configure lo comprueba: «License: LGPL version 2.1 or later») |
| Fuentes | `tools/ffmpeg_fuentes.sh` baja el tarball de ffmpeg.org (SHA-256 fijo) a `userland/terceros/ffmpeg/fuentes/` (no va en git; se quitan `presets/` y `tests/ref`) y aplica `userland/terceros/ffmpeg/parches/*.patch` (hoy ninguno) |
| Se compila con | `userland/terceros/ffmpeg/ffmpeg.mk`; opciones de `./configure` en `userland/terceros/ffmpeg/configurar.args` |
| Opción | `make FFMPEG=0` lo quita; con `SEM_GPL=1` tampoco entra (el programa enlaza la libsagui, GPL-3.0-only, como shine) |
| Prueba | `tests/h458_ffmpeg.py` |

## Cómo se construye

1. `make` prepara `build/ffmpeg/libsistema.a` (la libc común, la libm del MicroPython y la libsagui en una sola biblioteca)
   y `build/ffmpeg/principal_main.o`.
2. Ejecuta el `configure` de FFmpeg **fuera del árbol de fuentes** (`build/ffmpeg/obj`), cruzado: `--arch=x86_32 --cpu=i686
   --target-os=none --disable-asm`, con las banderas de la libc de SaguiOS (`-nostdinc -I userland/libc -ffreestanding
   -fbuiltin`, x87) y por «enlazador» `userland/terceros/ffmpeg/plataforma/enlazar.sh`, que convierte las órdenes de
   enlace al estilo gcc en un enlace de programa de SaguiOS (script `programa.ld`, arranque, `libsistema.a`). Así las
   pruebas de enlace del configure son reales: lo que detecta (`lrint`, `clock_gettime`, `pthread_create`...) es lo que
   hay en la libc. **No se pasan cabeceras de la glibc del anfitrión** (D191).
3. Compila con el `make` de FFmpeg (`-j` heredado) y copia `ffmpeg_g` y `ffprobe_g` sin símbolos a `build/userland/bin`.


## Qué incluye (`configurar.args`)

`--disable-everything` y luego:

- **Demuxers:** mov/mp4, matroska/webm, avi, ogg, wav, mp3, flac, aac, mpegts, mpegps, mpegvideo, h264, m4v, image2, lavfi.
- **Muxers:** mp4, mov, matroska, webm, avi, ogg, wav, mp3 (solo el contenedor, no hay codificador), flac, adts, mpegts,
  mpeg (sistema MPEG-1), mpeg1video, mpeg2video, rawvideo, image2, null, md5, framemd5, crc, pcm_*.
- **Protocolos:** `file`, `pipe`. Sin red.
- **Decodificadores:** h264, mpeg1video, mpeg2video, mpeg4, vp8, mjpeg, theora, aac, mp3, mp2, vorbis, opus, flac,
  rawvideo, ppm, bmp, pcm_*.
- **Codificadores (los nativos):** mpeg1video, mpeg2video, mpeg4, mjpeg, ppm, bmp, mp2, flac, vorbis, aac, pcm_*.
- **Filtros:** scale, crop, fps, pad, hflip, vflip, transpose, rotate, trim, setpts, format, null, setsar; aresample, volume,
  atempo, amix, amerge, pan, channelmap, aformat, atrim, anull, asplit, split; fuentes testsrc, testsrc2, color, nullsrc,
  sine, anullsrc (con el dispositivo `lavfi`).
- **Programas:** `ffmpeg` y `ffprobe`; ffplay no.

**Ampliar:** añadir el componente a la línea que toque de `configurar.args` (`--enable-decoder=...`) y volver a hacer
`make` (el `.mk` vuelve a configurar solo). `ffmpeg -codecs` / `-formats` / `-filters` dicen lo que hay.

## Lo que se añadió a la libc común (en vez de parchear FFmpeg)

No hay ningún parche en FFmpeg. Todo lo que faltaba se añadió a `userland/libc/` de forma general:

- `pthread.h`, `pthread.c`, `sched.h`: hilos POSIX sobre los hilos y el futex de la libsagui. `pthread_create/join`,
  mutex normales y recursivos, variables de condición (con `timedwait`), `rwlock` (exclusivo), `once`, claves por hilo,
  `sched_yield`. Todas las funciones son **débiles** (el port del miniJVM trae su propio `pthread_sagui.c` con su cabecera,
  que gana; `java.mk` pone su `-I` antes que el de la libc). Limitaciones: la pila de un hilo es la del kernel (960 KB) salvo
  que se pida más; `errno` sigue siendo uno por proceso; `pthread_detach` no libera la estructura.
- `matematica.c` y `math.h` (también débiles, mGBA trae `exp2f`): `exp2`, `exp2f`, `log2f`, `log10f`, `cbrtf`, `asinf`,
  `acosf`, `lrintf`, `llrint`, `llrintf`, `llround`, `llroundf`, `j1` (Bessel de orden 1, por serie y desarrollo
  asintótico, error < 1e-10: la usa `libswscale`), `sincos` y `sincosf` (que gcc genera solo).
- `errno.h`: `EDEADLK`, `ENOLCK`, `ELOOP`, `ENODATA`, `ETIME`, `EPROTO`, `EBADMSG`, `EOVERFLOW`, `ECANCELED`.

## Velocidad (tests/h458_ffmpeg.py, `ffmpeg -benchmark`, rtime)

Cuadros por segundo; el PC es un Ryzen 9 9900X (los números de un equipo antiguo serán muy inferiores). Sin ensamblador.

| Operación | QEMU con KVM | QEMU sin KVM (TCG) |
|---|---|---|
| Decodificar H.264 Main 640x360 (mandelbrot) | 781 | 112 |
| Decodificar MPEG-4 640x360 | 658 | 298 |
| Decodificar MJPEG 320x240 | 1389 | 658 |
| Convertir H.264 640x360 a MJPEG 320x180 (scale + codificar) | 403 | 53 |
| Codificar MJPEG 320x240 (con el generador testsrc) | 758 | 101 |
| Codificar MPEG-4 320x240 | 1136 | 132 |
| Codificar MPEG-1 320x240 | 1190 | 139 |

(Los clips cortos y fáciles, de décimas de segundo, dan cifras poco precisas: la primera fila, 0,02 s, es solo orientativa.) Para
comparar, el decodificador H.264 de la libsagui del Reproductor da 41 cuadros por segundo a 640x360 con KVM (D276).

**Los decodificadores de vídeo dan exactamente las mismas sumas que el ffmpeg del PC**; los de audio de coma flotante
(AAC, MP3, MP2, Vorbis, Opus) difieren en el último bit de unas pocas muestras (de 0 a 45 bytes de 44.100).

**Reproductor de SaguiOS.** El AVI MJPEG + PCM que escribe `ffmpeg` se abre y se decodifica entero (25 de 25 cuadros).
El MPEG-1 en MPEG-PS que escribe FFmpeg 8 lo decodifica el Reproductor solo hasta el cuadro 19 (de 25): el
decodificador de `userland/libsagui/mpeg1.c` pierde el final de ese tipo de flujo (la segunda secuencia va sin cabecera
de grupo); `ffprobe` y el ffmpeg del PC lo leen entero. Es un fallo del decodificador existente, no de este port.

## Pendiente (a propósito)

- **Ensamblador x86** (`--disable-asm`): FFmpeg corre todo en C con la x87. Con SSE2/SSSE3/MMX sería varias veces más rápido
  (los procesadores de los que más nos interesan, de Pentium II a Core 2, tienen MMX o SSE), pero hay que ensamblar con
  nasm/yasm dentro de la cadena y cuidar el estado de la FPU/MMX: otra versión.
- **Hilos de fotogramas**: ya hay `pthread` y el programa `ffmpeg` los usa para sus colas internas, pero la libc no es del
  todo segura entre hilos (`errno` global, stdio sin cerrojos): los decodificadores van con un solo hilo salvo `-threads N`.
- **H.264 y MP3 de salida**: x264 y LAME son GPL. Para MP3 está shine (`convertir-audio`); el AAC nativo de FFmpeg sí
  codifica.
- **zlib** (MOV con cabecera comprimida, PNG).

**Para incrustar FFmpeg en un programa:** con TCC, las cabeceras (`<libavcodec/avcodec.h>`...) y las bibliotecas estáticas
(`libavformat.a libavcodec.a libswscale.a libswresample.a libavutil.a libavfilter.a libavdevice.a`) van en `/apps/tcc/include` y
`/apps/tcc/lib` (`make install-headers install-libs` de FFmpeg, ver `ffmpeg.mk`).
