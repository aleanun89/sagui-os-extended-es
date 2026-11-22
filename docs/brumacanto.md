# Brumacanto en SaguiOS (D444)

Juego de exploración, minería y construcción en primera persona con vóxeles, prueba de concepto 3D del mantenedor
(C11, SDL 2, OpenGL ES-CM 1.1 con búferes de vértices y pipeline fijo, unas 3.000 líneas). Código MIT, gráficos CC0
(Voxel pack de Kenney) y unas texturas CC BY-SA 3.0 de Minetest Game (D449). Es el primer juego real sobre **SaguiGL como OpenGL ES 1.1 completo** (D425) por la ruta de
SDL_GL_* sobre EGL.

- Origen y licencias: `userland/terceros/brumacanto/ORIGEM.md`, `LICENSE.txt`, `LICENSE-kenney.txt`; fila en `docs/terceros.md`.
- Se compila con `userland/terceros/brumacanto/brumacanto.mk` contra nuestra SDL 2 (`userland/libsdl2`), `sdl2_gl.c`,
  EGL y SaguiGL. No se usan `third_party/sdl2` ni `third_party/khronos` del ZIP. Entra también en `make SEM_GPL=1`.
- Ejecutable `/apps/bin/brumacanto`; atlas en `/apps/brumacanto/atlas.rgba`; partidas en `/home/brumacanto/`.
  Inicio > Juegos > Brumacanto. Ayuda: `rootfs/ayuda/brumacanto.md`.
- Opciones: `--seed S`, `--size X[xZ[xY]]`, `--view 32-104|auto` (distancia de dibujo solo para esa vez; F3 en el juego la cambia y la
  guarda; `auto` la vuelve a elegir según el equipo), `--bench CUADROS`, `--fullscreen` (pantalla completa; F11 la cambia en el juego)
  y `--escala 50|75|100` (escala de dibujo, solo para esa vez; F4 en el juego la cambia y la guarda). D451.
- **Ajustes guardados** (D444): `ajustes.txt` junto a las partidas (`/home/brumacanto/`), una clave por línea: `vista=64`,
  `escala=75`. Si no existe, la escala se lee del `escala.txt` de antes. **La primera vez** (sin `vista` guardada y sin `--view`) la
  distancia se elige según lo que anuncia el 3D de SaguiOS en `glGetString(GL_RENDERER)`: `SaguiGL (software)` (no hay 3D por la GPU)
  o `SaguiGL (GPU)` (GPU antigua, sin gpu3d v3) dan **48**; `SaguiGL (GPU v3)` da **64**, y con v3 se miden unos cuadros con la vista de 64
  (se saltan los 45 primeros, que generan terreno; se promedian los 45 siguientes): si el trabajo por cuadro es de 8 ms o menos, se
  pasa a **104**. Todo eso va dentro de `SAGUI_BRUMACANTO`; fuera de SaguiOS (Linux) la de serie es 64. En el registro:
  `BRUMA_VISTA_AJUSTE N (origen)` (lo guardado o elegido) y `BRUMA_VISTA N (origen)` (la que se usa), con `BRUMA_VISTA medida ...`
  si hubo medida. Las funciones puras están en `fuentes/src/ajustes.h` (`tests/ajustes_test.c` las prueba en el PC).

## Qué hizo falta en el sistema
- `SDL_HINT_OPENGL_ES_DRIVER` (se acepta y se ignora).
- `SDL_CreateWindow` reduce las ventanas mayores que el máximo del sistema (1000x700) conservando la proporción; el juego pide
  1152x648 y antes fallaba con «no se pudo abrir la ventana». Útil para cualquier juego SDL.
- El resto de la SDL (GetPrefPath, GetBasePath, QueueAudio, ratón relativo, StartTextInput, SaveBMP, GL_GetDrawableSize...), GL ES 1.1
  y EGL ya estaban: no hizo falta tocarlos.

## Memoria
El mundo (hasta 32768x32768x128) no se guarda entero: se genera por sectores de 16x16 al explorar, en una caché fija de
16x16 sectores (como mucho 16x16x(16x16xAltura) = 12 MB con 48 de altura y 32 MB con 128), más los sectores editados, que se
conservan, y las mallas (VBO) de los visibles. Cualquier tamaño de mundo cabe en 1-2 GB; no se cambió el tamaño de serie.

## Rendimiento (QEMU con KVM, ventana de 1000x562, 40 cuadros caminando, semilla Prueba 160x160x48)
| Vista | Software (cuadros/s) | Peor cuadro | GPU simulada (cuadros/s) |
|---|---|---|---|
| 32 | 10,3 | 140 ms | 0,7 |
| 64 | 8,2 | 150 ms | 0,6 |
| 104 | 7,1 | 220 ms | 0,7 |

Sin KVM (TCG) va a 0,3-0,5 cuadros/s (solo sirve para probar que funciona). Con 104 (que era la de serie antes de los ajustes guardados) por software son 7 cuadros/s
en QEMU; por eso la primera vez se elige 48 sin 3D por la GPU, y 32-48 con F3 o `--view` dan 9-10. El **driver simulado** de la GPU (`--gpu3d=simulado`) es unas 15 veces más
lento que el software: rasteriza en el kernel con enteros y recibe los vértices de cada dibujo; es una prueba de la ruta gpu3d v2
(h424), no un dato de rendimiento de una GPU real (X1400, HD 4500: pendiente de medir en esas máquinas).

## Por la GPU en hardware real (D451)
En el Amilo (Mobility X1400, Core Duo 1,83 GHz; vista 64): por la GPU 8,3 cuadros/s en ventana y 8,4 a pantalla completa
(antes 0,9, porque el mundo iba por software); por software 1,1. Con la escala al 50 %, 9,3 / 10,8 por la GPU y 2,9 / 2,5 por
software. A pantalla completa el cuadro va directo de la GPU a la pantalla (D432); en ventana vuelve a la RAM por la GART
(10 ms). Los detalles, las causas y el perfil (`SAGUI_GL_PERFIL=1`): D451 y `docs/gpu3d.md`.

## Prueba
`tests/h444_brumacanto.py [unidad] [fuentes] [saguios] [gpu]`: world_test del juego en el PC; en QEMU, mundo con semilla fija,
captura (`docs/capturas/H444-*`, sintética: gráficos CC0), guardar con F5, salir limpio, cargar y medir; y lo mismo con la GPU simulada,
comparando que las dos imágenes se parecen.

## Ideas de Luanti (D449)
Se tomaron **ideas** (mecánicas, recetas, tipos de bloque) de Luanti y de Minetest Game, reescritas en C: ningún código Lua ni C++ (LGPL)
entra en el juego. Las **texturas** sí se copian, solo las CC BY-SA 3.0 de `mods/default` y `mods/flowers` de Minetest Game, en
`userland/terceros/brumacanto/datos/luanti/` con `CREDITOS.md` (autor, licencia y URL de cada una). Brumacanto no recrea ningún juego
concreto: conserva su nombre, su estilo y su mundo.

| Novedad | Detalle |
|---|---|
| 10 bloques | cobre, diamante, obsidiana, pino, tablas de pino, agujas, arenisca, ladrillo de piedra, horno y lava (ids detrás de la roca madre) |
| 9 objetos | lingotes de cobre y de hierro, diamante, pico/hacha/espada de hierro y pico/hacha/espada de diamante |
| Generación | minerales con una segunda tirada de hash (los antiguos no cambian), lava en cuevas hasta y=5, arenisca bajo la arena, pinos donde la temperatura es baja, rosas sobre la hierba, un horno en cada cabaña |
| Fabricación | 13 recetas nuevas (25 en total, con lista desplazable); fundir exige llevar el horno en la mochila |
| Herramientas | niveles 1/2/3 (piedra, hierro, diamante): velocidad 3,4/5/7,5 y daño de espada 5/7/9; vida 90/260/900 usos; el diamante y la obsidiana piden pico de hierro |
| Lava | no sólida, brilla, quema 4 de daño cada 0,75 s, se puede tapar con bloques |
| Guardados | versión 3 (añade el desgaste); las versiones 1 y 2 se cargan y se renumeran los objetos |

Atlas: casillas 42-62 (`fuentes/tools/build_atlas_luanti.py`, sin dependencias; repetible). Las herramientas de hierro y de diamante son los
iconos CC0 de Kenney recoloreados. La bolsa pasa de 6 a 8 columnas para caber los 52 objetos de la mochila (38 bloques y 14 objetos).

**Rendimiento tras D449** (KVM, misma prueba, vista 64): por software 8,3-8,6 cuadros/s (antes 8,2; sin cambio apreciable: solo hay más tipos de bloque,
no más geometría) y 0,7 con la GPU simulada (igual que antes); parecido de imagen software/GPU 96 %. Capturas: `H444-1` (partida), `H444-3` (mochila con los objetos nuevos).
