# ffmpeg y ffprobe: convertir y examinar audio y vídeo

**ffmpeg** (`/apps/bin/ffmpeg`) y **ffprobe** (`/apps/bin/ffprobe`) son FFmpeg 8.1.3, el de verdad, compilado dentro de
SaguiOS con una configuración mínima (licencia LGPL-2.1 o posterior). Va en C, sin ensamblador, y con los hilos de
SaguiOS: en un PC actual con KVM decodifica H.264 de 640x360 a unos 780 cuadros por segundo y en el emulador sin KVM a
unos 110; en un equipo antiguo irá bastante más lento (mide con `ffmpeg -benchmark -i x.mp4 -f null -`). Para ver qué trae: `ffmpeg -formats`, `ffmpeg -codecs`, `ffmpeg -filters`.

## Qué sabe hacer

- **Formatos:** MP4/MOV, Matroska/WebM, AVI, Ogg, WAV, MP3, FLAC, AAC (ADTS), MPEG-TS y MPEG-PS, vídeo MPEG
  elemental y H.264 en bruto, secuencias de imágenes (PPM, BMP). Protocolos `file` y `pipe`.
- **Decodifica:** H.264, MPEG-1, MPEG-2, MPEG-4, VP8, MJPEG, Theora; AAC, MP3, MP2, Vorbis, Opus, FLAC, PCM.
- **Codifica:** MPEG-1, MPEG-2, MPEG-4 parte 2, MJPEG, PPM, BMP; MP2, FLAC, Vorbis, AAC, PCM. **No hay H.264 ni MP3
  de salida** (x264 y LAME son GPL).
- **Filtros:** `scale`, `crop`, `fps`, `pad`, `hflip`, `vflip`, `transpose`, `rotate`, `trim`, `setpts`, `format`;
  de audio `aresample`, `volume`, `atempo`, `amix`, `amerge`, `pan`, `atrim`; y los generadores `testsrc`, `color`,
  `sine`... (`-f lavfi -i testsrc=size=320x240:rate=25`).

## Ejemplos

    ffprobe /usb/pelicula.mp4                      qué trae (flujos, duración, códecs)
    ffprobe -v error -show_streams -of json x.mkv  lo mismo, en JSON
    ffmpeg -i entrada.mkv -f md5 -                 decodifica todo y da su suma (comprobar que se lee bien)
    ffmpeg -benchmark -i entrada.mp4 -f null -     cuánto tarda en decodificar (línea «bench: ... rtime=»)

**Convertir para el Reproductor de SaguiOS** (como `tools/conversor_video_pc.py`, sin H.264 de salida):

    # AVI con MJPEG y sonido PCM: el más ligero de decodificar
    ffmpeg -i peli.mp4 -vf "scale='min(640,iw)':-2,fps=20" -c:v mjpeg -pix_fmt yuvj420p -q:v 5 \
           -c:a pcm_s16le -ac 2 -ar 22050 -f avi peli-sagui.avi
    # MPEG-1 (solo imagen): el más rápido
    ffmpeg -i peli.mp4 -vf "scale=640:-2,fps=25" -c:v mpeg1video -q:v 5 -an -f mpeg peli-sagui.mpg
    reproductor peli-sagui.avi

**Audio:**

    ffmpeg -i peli.mp4 -vn -c:a flac sonido.flac           extraer el audio sin pérdida
    ffmpeg -i cancion.ogg -c:a pcm_s16le cancion.wav        a WAV
    ffmpeg -i x.wav -af "volume=0.5,aresample=44100" y.wav  bajar el volumen y cambiar la frecuencia

**Imagen:**

    ffmpeg -i peli.mp4 -vf scale=320:-2 -frames:v 1 portada.ppm   un cuadro
    ffmpeg -i peli.mp4 -vf "crop=320:240,hflip" corto.avi

## Notas

- Un vídeo a tamaño de DVD tarda mucho: baja el ancho con `scale` y los cuadros con `fps` antes de codificar.
- La salida va a donde se indique; `/tmp` está en memoria. Usa `-y` para sobrescribir.
- Los decodificadores usan un solo hilo salvo que se pida `-threads N`; con una sola CPU no acelera nada.
- La configuración (qué códecs y filtros hay) está en `userland/terceros/ffmpeg/configurar.args`; ver `docs/ffmpeg.md`.
