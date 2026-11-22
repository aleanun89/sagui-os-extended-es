# Origen de Brumacanto

- Programa: **Brumacanto**, juego de exploración, minería y construcción en primera persona con vóxeles
  (C11, SDL 2, OpenGL ES-CM 1.1). Prueba de concepto de juego 3D del mantenedor.
- Origen: el ZIP `Brumacanto-C-SDL2-GLES11(1).zip` que entregó el mantenedor (input_share), importado el 2026-11-21.
- Licencias: código MIT («Brumacanto contributors», `LICENSE.txt`); gráficos del *Voxel pack* de Kenney, CC0
  (`LICENSE-kenney.txt`, carpeta `datos/kenney`).
- Qué se importó: `fuentes/src` (el código), `fuentes/tests` y `fuentes/tools/build_atlas.py` (como en el original),
  `datos/atlas.rgba` (el atlas que lee el juego) y `datos/kenney` (los PNG de origen del atlas).
  `Makefile.upstream` es el del original, solo de referencia.
- Qué NO se importó: `third_party/sdl2` y `third_party/khronos` (se usan la SDL 2 y las cabeceras GLES/EGL de SaguiOS),
  `brumacanto.exe`, `SDL2.dll` ni el binario de Linux.
- Cambios al código: `src/main.c` busca los gráficos en `/apps/brumacanto/` bajo `SAGUI_BRUMACANTO` (ver D444).

## Texturas de Luanti / Minetest Game (D449)
- Origen: https://github.com/luanti-org/minetest_game (commit `9e895af6e9feb98332fa1a778001442f5669bc20`), solo `mods/default` y `mods/flowers`.
- Licencia: **CC BY-SA 3.0** (todas se comprobaron una a una en el README y license.txt de su mod; ninguna con otra licencia). Van en
  `datos/luanti/` con `CREDITOS.md` (fichero, autor, licencia, URL) y `LICENCIA-CC-BY-SA-3.0.txt`. Quedan fundidas en `datos/atlas.rgba`
  (casillas 42-62, `fuentes/tools/build_atlas_luanti.py`); el atlas es por tanto CC BY-SA 3.0 y el código sigue MIT.
- Código de Luanti: **no se copia nada** (LGPL 2.1). Solo ideas (bloques, recetas, mecánicas), reescritas en C.
- Descarga: a un directorio nuevo y vacío, tratada como datos no fiables; solo se copiaron los PNG elegidos.
