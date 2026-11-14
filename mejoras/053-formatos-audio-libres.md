# 053 - Formatos de audio libres: IMA ADPCM, MP3, FLAC y Ogg Vorbis

- **Estado:** fases 1-3 hechas (IMA ADPCM, MP3 y FLAC; D098-D100); **Ogg Vorbis (fase 4) pendiente**
- **Prioridad:** media
- **Alta:** 2026-11-07
- **Depende de:** 041 (sonido; `reproducir` en streaming)
- **Relacionadas:** 048 (vídeo), 034 (CHIP-8), 045 (USB/EHCI rápido)

## Problema / motivación

`reproducir` solo entiende WAV PCM. 30 s de música en calidad CD ocupan 5,3 MB: mucho para
un pendrive leído a ~50 KB/s (antes de la 045) y para la memoria de un PC antiguo. Hacen falta
formatos **libres** y comprimidos, decodificados con código **propio**.

## Propuesta (por fases)

| Fase | Formato | Compresión | Decodificador | Generar en el PC (ffmpeg) |
| --- | --- | --- | --- | --- |
| 1 | **IMA ADPCM** en WAV (`.wav`, formato 0x11) | ×4, con pérdida leve | ~150 líneas; casi sin CPU | `ffmpeg -i x -c:a adpcm_ima_wav -ar 22050 -ac 1 salida.wav` |
| 2 | **MP3** (MPEG-1/2 Layer III, `.mp3`) | ×10, con pérdida | ~2.500 líneas, escrito a partir de la norma ISO/IEC 11172-3 (y 13818-3 para 16/22/24 kHz): cabeceras y sincronía, *side info*, depósito de bits (*bit reservoir*), factores de escala, Huffman (tablas de la norma), recuantización, estéreo M/S e intensidad, reordenación, antialias, IMDCT con ventanas larga/corta/mixta, banco de filtros polifásico. Etiquetas ID3v1/ID3v2 (título y artista), VBR (cabecera Xing/Info) para la duración | `ffmpeg -i x -c:a libmp3lame -q:a 4 salida.mp3` |
| 3 | **FLAC** (`.flac`) | ×1,7-2, **sin pérdida** | ~1.000 líneas: tramas, subtramas (constante, verbatim, fija, LPC), residuo Rice, decorrelación estéreo, CRC | `ffmpeg -i x -c:a flac salida.flac` |
| 4 | **Ogg Vorbis** (`.ogg`) | ×10, con pérdida | ~3.000-4.000 líneas: contenedor Ogg, cabeceras, *floor* 1 (y 0 si se puede), *residue*, *codebooks* VQ, MDCT, ventanas | `ffmpeg -i x -c:a libvorbis -q:a 3 salida.ogg` |

- **MP3:** sus patentes caducaron (la última en 2017), así que es libre de implementar y
  usar. Es el formato que más archivos de audio ya tiene la gente: por eso va en la fase 2.
  Decodificador **propio**, sin copiar libmad, minimp3 ni otros; las tablas numéricas de la
  norma (Huffman, bandas de factores de escala, ventanas) se generan o transcriben de la norma.
  Rendimiento: tiempo real en un Pentium III con coma fija o con float (SSE si hay).
- Biblioteca común en la libsagui (`audio_formato.c`) con detección por contenido, no por
  extensión: `audio_abrir_arquivo()` → flujo de PCM de 16 bits que `reproducir`, el reproductor
  de vídeo (048), los juegos, CHIP-8 y Python (`sagui.sonido`) consumen por bloques.
- `reproducir` en **streaming** con todos los formatos (empieza a sonar enseguida).
- Etiquetas: título y artista (ID3 en MP3, comentarios Vorbis y FLAC `VORBIS_COMMENT`), mostradas por
  `reproducir`.
- Opcional, más adelante: un **reproductor gráfico** con lista, barra de progreso (042) y
  volumen, y `reproducir` sobre HMKD (049) para audio embebido (`::audio[...]`).
- Doble clic en Archivos sobre `.wav`, `.mp3`, `.flac` y `.ogg`.

## Pruebas

- Archivos de prueba **generados** en el anfitrión (tonos, barridos y ruido, sin material con
  derechos), codificados con ffmpeg en cada formato. SaguiOS los decodifica y se compara el PCM
  con el decodificado por ffmpeg:
  - FLAC: **idéntico bit a bit**;
  - MP3: PSNR frente a ffmpeg por encima de un umbral (un decodificador de MP3 conforme no es
    idéntico bit a bit, pero sí muy cercano); también MPEG-2 (22 kHz) y VBR;
  - ADPCM: idéntico al de ffmpeg (es determinista);
  - Vorbis: diferencia máxima y PSNR por encima de un umbral.
- Rendimiento: tiempo de decodificación por segundo de audio en QEMU/KVM y en TCG.
  Objetivo: Vorbis en tiempo real en un Pentium III.
- Grabación con `-audiodev wav` en QEMU para comprobar que suena (como h70).

## Criterios de aceptación

- [x] IMA ADPCM, MP3 y FLAC en la libsagui, con detección por contenido (`audio_formato.h`); Ogg Vorbis **pendiente**
- [x] `reproducir` en streaming con ADPCM, MP3 y FLAC y etiquetas (ID3 y VORBIS_COMMENT); doble clic en Archivos
- [x] Comparación con ffmpeg: bit a bit en FLAC y ADPCM; MP3 con SNR ≥ 95 dB (umbral 80) y la misma duración
- [x] Test automático en `tests/h75_codecs_audio.py`
- [x] Decisiones D098-D100 en `docs/decisoes.md`
- [ ] Ogg Vorbis (fase 4), MPEG-2.5, CHIP-8 y Python sobre la biblioteca, reproductor gráfico

## Resultado (fases 1-3)

- **Formatos:** WAV PCM (8/16/24/32 bits), WAV IMA ADPCM (0x11, bloques de hasta 4096 bytes,
  mono y estéreo), MP3 (MPEG-1 Layer III a 32/44,1/48 kHz y MPEG-2 a 16/22,05/24 kHz; CBR y VBR;
  mono, estéreo, conjunto y M/S; ID3v1/v2; Xing/Info y LAME para la duración y el recorte sin
  silencios) y FLAC (4-24 bits, 1-2 canales, niveles 0-12, STREAMINFO y VORBIS_COMMENT).
- **Rendimiento (QEMU/KVM):** MP3 estéreo 44,1 kHz 128 kbit/s: 18 Mciclos por segundo de audio
  (≈7 % de un Pentium III a 500 MHz); FLAC 4,8; ADPCM 0,9. En TCG, 23 veces el tiempo real para
  el MP3. Coma fija de enteros, sin FPU.
- **Preparar audio:** `tools/convertir-audio.sh entrada mp3|mp3-bajo|flac|adpcm|wav`.
- **Reproductor gráfico** (D115, `musica`, prueba h86): salto en los cuatro decodificadores
  (`audio_arquivo_buscar`), barra de progreso, lista y etiquetas; ver `docs/decisoes.md`.
- **Pendiente:** Vorbis (con la misma biblioteca: añadir `audio_vorbis.c` y un `case` en
  `audio_arquivo_abrir`), MPEG-2.5, y que CHIP-8/Python abran archivos con la biblioteca.
