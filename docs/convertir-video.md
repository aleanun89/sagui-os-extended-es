# Convertir vídeos para SaguiOS desde el PC

`tools/conversor_video_pc.py` es un programa con ventana (Python 3 + tkinter) que se ejecuta **en el PC**
(Windows, Linux o macOS) y prepara vídeos para el Reproductor de SaguiOS usando **ffmpeg** por debajo.

## Instalar

- **Windows**:
  1. Python 3 de python.org (trae tkinter).
  2. ffmpeg de https://ffmpeg.org/download.html. Copia `ffmpeg.exe` y `ffprobe.exe` junto a `conversor_video_pc.py`, o déjalos en el PATH.
  3. Doble clic en el `.py`, o en una consola: `py conversor_video_pc.py`.
- **Linux / WSL**: `sudo apt install python3-tk ffmpeg` y `python3 tools/conversor_video_pc.py`.
- Si ffmpeg está en otra carpeta: `--ffmpeg RUTA\ffmpeg.exe`.

## Uso

1. «Añadir...» para elegir uno o varios vídeos (mp4, mkv, avi, mov, webm, wmv…).
2. Elige el formato, la calidad, el tamaño (ancho; el alto se calcula y nunca se agranda) y los cuadros
   por segundo.
3. Carpeta de destino (vacío = junto a cada vídeo). Los ficheros salen como `nombre-sagui.mp4`.
4. **Audio dual**: en «Pista de audio» elige la primera, la segunda, la tercera, o un idioma (español,
   inglés, portugués…, según la etiqueta de idioma de la pista). «Ver pistas...» enseña las que trae cada
   vídeo y cuál se usará; si un vídeo no tiene la pedida, se usa la primera. SaguiOS reproduce una sola pista.
5. «Convertir». La barra muestra el progreso; «Cancelar» para y borra el fichero a medias.

## Formatos

| Formato | Sonido | Ocupa | Decodificar en SaguiOS | Para qué |
|---|---|---|---|---|
| MP4 H.264 Main + AAC | sí | muy poco | medio (41 c/s a 640x360 en QEMU+KVM) | **el recomendado** |
| MP4 H.264 Baseline + AAC | sí | poco | algo más ligero (sin CABAC ni cuadros B) | PC antiguos |
| AVI MJPEG + PCM | sí | mucho (unas 15 veces más) | 14,5 c/s a 640x360 | cuando sobra disco y falta CPU |
| MPEG-1 | **no** | poco | el más rápido (119 c/s) | solo imagen: el Reproductor aún no decodifica su audio |

Las cifras son de un vídeo de prueba de 3 s convertido con «Media», 640 de ancho y 25 c/s y
reproducido con `reproductor -t` en QEMU con KVM (2026-11-16).

**Consejos para PC de la época** (Pentium II/III): 480 o 640 de ancho, 20-25 c/s y calidad «Media». Si se
salta cuadros, prueba el Baseline o baja el tamaño.
