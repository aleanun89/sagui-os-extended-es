# FFmpeg 8.1.3 en SaguiOS (D458)

- **Origen:** https://ffmpeg.org/, FFmpeg 8.1.3. Tarball `https://ffmpeg.org/releases/ffmpeg-8.1.3.tar.xz`, SHA-256
  `7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3` (ffmpeg.org no publica sumas: la fija
  `tools/ffmpeg_fuentes.sh`).
- **Licencia:** LGPL-2.1 o posterior. Sin `--enable-gpl`, `--enable-nonfree` ni `--enable-version3`. Los textos
  (`COPYING.LGPLv2.1`, `LICENSE.md`) quedan en `fuentes/`. FFmpeg se enlaza estáticamente con la libsagui GPL-3.0-only:
  el binario se distribuye como obra combinada bajo la GPL v3, y las fuentes de FFmpeg (que van tal cual) y el
  `.mk` que permite reenlazarlas están en el repositorio. Fuera de la variante `SEM_GPL=1`.
- **Qué no va a git:** `fuentes/` (lo baja `tools/ffmpeg_fuentes.sh`; quita `presets/` y `tests/ref`).
- **Cambios en FFmpeg:** ninguno (el directorio `parches/` está vacío). Lo que faltaba se añadió a la libc común.
- **Qué va a git (nuestro):** `ffmpeg.mk` (cómo se configura y compila), `configurar.args` (la lista de lo que se activa),
  `plataforma/enlazar.sh` (el «ld» del configure) y `plataforma/principal_main.c`.
- **Instalado en:** `/apps/bin/ffmpeg` y `/apps/bin/ffprobe`.
- **Cómo se comprueba:** `tests/h458_ffmpeg.py` (la suma de los cuadros decodificados es idéntica a la del ffmpeg del PC).
