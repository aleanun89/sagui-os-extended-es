# Tzar en SaguiOS (D417)

Port del RTS **Tzar: The Burden of the Crown** (Haemimont, 1999; edición FX en castellano) a SaguiOS en **C99 + la SDL2
de SaguiOS**, con dibujo por software, tomando como referencia **ReTzar** (<https://github.com/ColCarlo/ReTzar>), el
remake de fans en Rust + Bevy + wgpu, licencia **GPL-3.0-or-later**. Traducir su código a C es obra derivada: cada
fichero traducido lleva su origen (fichero de ReTzar) y la licencia, y ReTzar figura en `docs/terceros.md`.

Los **datos del juego no se distribuyen**: son del juego comercial. No van a git, ni a las imágenes públicas, ni a las
pruebas de la batería, ni a capturas públicas. Las pruebas usan datos **sintéticos** (`tools/tzar_sintetico.py`).

## Fase 1: estudio de ReTzar (2026-11-19, commit `cf2af88` de ReTzar)

### Tamaño

ReTzar tiene **119 500 líneas de Rust** en `src/` (94 200 de código y 25 300 de pruebas) más 4 300 de pruebas de
integración en `tests/` y 1,6 MB de una copia parcheada de `bevy_render` en `vendor/`. Por módulo (líneas sin pruebas):

| Módulo | Líneas | Qué hace | ¿Hace falta en SaguiOS? |
|---|---:|---|---|
| `gameplay.rs` + `gameplay/` | 38 500 | simulación: órdenes, producción, combate, magia, recogida, guarniciones, puertas, barcos, guiones de misión | sí (núcleo del juego) |
| ...de ello `gameplay/ai/` | 10 400 | IA original: compilador e intérprete de los guiones `DATA\AI\*.ais/*.ai` del Tzar.exe, economía, guerra, flota, hechizos | sí (fase 6) |
| ...de ello `pathing`, `movement`, `combat`, `islands` | 4 750 | caminos (A* por casillas y por islas), pasos, choques, combate | sí |
| ...de ello `scripting`, `campaign_pilot` | 4 400 | sucesos/acciones/reglas de las misiones y paso entre misiones | sí |
| `frontend/` | 13 700 | Bevy: escena, vista, entrada, efectos (luces, CRT, niebla suave, rayos) y 5 sombreadores WGSL | se rehace (dibujo por software) |
| `menu.rs` + `menu/` | 7 500 | menús originales (pantallas `S_*`), opciones | sí, simplificado |
| `hud.rs` + `hud/` | 6 200 | barra de la partida, órdenes, minimapa, panel de trucos | sí |
| `world.rs` + `world/` | 4 100 | objetos del mapa (`.objects`, `.objext`, `.kingdominfo`), biblioteca de dibujos (`IMAGES.INI`), árboles, acantilados, orden de pintado | sí (**fase 2**) |
| `random_map.rs` | 2 500 | mapas aleatorios (plasma, inicios, colocación) | sí (fase 7) |
| `audio.rs` + `audio_unix.rs` + `audio/` | 2 850 | WAVE PCM, MIDI con SoundFont (rustysynth), ambiente | se adapta a la SDL de SaguiOS |
| `lang.rs` + `lang/` | 2 100 | 8 idiomas y textos superpuestos | **no**: la edición FX ya está en castellano |
| `script.rs` + `script/` | 1 700 | lector de `.events/.actions/.rules` y expresiones | sí (fase 5) |
| `simulation.rs` | 1 200 | atributos de objetos, reloj determinista, visibilidad | sí |
| `font.rs` | 1 000 | fuentes `.FNT` de Haemimont (métricas + BMP de cobertura) | sí (fase 4) |
| `media.rs` + `ddv.rs` | 1 630 | vídeos (Media Foundation/AVFoundation; DDV «Masher» con tablas del Tzar.exe) | **no**: FX trae `intro.MPG` (MPEG-1, ya lo lee la libsagui) |
| `archive.rs` | 610 | paquetes `*.wdt` (HMMSYS PackFile + LZSS por trozos) | sí (**fase 2**) |
| `terrain.rs` | 580 | biomas, losetas de 32x32, transiciones por máscara, minimapa | sí (**fase 2**) |
| `save.rs` | 620 | partidas guardadas (JSON cifrado) | sí, en formato propio |
| `pack`, `packing`, `vfs`, `flac`, `original`, `picker`, `first_start`, `hd`, `paths` | 4 150 | copia portátil cifrada de ReTzar, paquetes HD, selector de carpeta | **no** |
| `hpfs.rs` | 390 | contenedores HPFS de Haemimont (mapas `.wmp`, campañas `.wcm` con mapas anidados) | sí (**fase 2**) |
| `scenario.rs`, `map.rs` | 490 | catálogo de mapas y campañas; cabecera `2mzt` y casillas | sí (**fase 2**) |
| `bitmap.rs`, `sprite.rs`, `animation.rs` | 790 | BMP (8/16/24/32 bits), sprites RLE ` elr` (paleta, RGB555 y sombras), tiempos de animación | sí (**fase 2**) |
| resto (`selection`, `keys`, `dialogue`, `dialogs`, `bolt`, `flight`, `scene`, `game`, `main`, `bin/`) | 3 900 | selección, teclas, conversaciones, proyectiles, pájaros, herramientas | parte |

Lo que hace falta traducir son unas **72 000 líneas de Rust** (sin el frontend de Bevy, los idiomas, los vídeos ni la
copia portátil), que en C serán del orden de **55 000-70 000 líneas** (C es más largo en tablas y en manejo de memoria,
y más corto donde Rust usa genéricos y serde). Para comparar: AoEN son 13 500 líneas de C. **Tzar es unas cinco veces
AoEN.** Es viable, pero no de una vez: va por fases que se pueden publicar por separado (primero un visor, después la
simulación sin IA, después la IA y las campañas).

### Formatos de datos (comprobados con la edición FX)

La instalación FX trae `Packs/*.wdt`, `Maps/*.wmp`, `Campaign/*.wcm`, `Tzar.exe`, `Tzar.ini`, `intro.MPG` y `Hero Icons/`.

- **Paquetes `*.wdt`** (`archive.rs`): fichero entero comprimido con **LZSS por trozos** (`LZSS`, tamaño, trozo de
  32 KiB, algoritmo `0xc4` = distancias de 12 bits y longitudes de 4, tabla de desplazamientos; cada trozo con su
  diccionario, un trozo del tamaño exacto va sin comprimir). Dentro, un **HMMSYS PackFile**: número de entradas en
  0x20 y desde 0x28 nombres con prefijo compartido con el anterior (longitud total, prefijo, sufijo), desplazamiento y
  tamaño. `sounds.wdt` va sin LZSS. Orden de carga (los últimos ganan): `data`, `data2`, `images`, `images2`,
  `terrains`, `local`, `help`, `v101`; las carpetas sueltas `DATA`, `IMAGES`, `LOCAL`, `HELP` completan lo que falte.
  Descomprimidos: `images.wdt` 107 MB, `terrains.wdt` 60 MB; se leen **trozo a trozo bajo demanda**, nunca enteros.
- **Imágenes** (`bitmap.rs`, `sprite.rs`): BMP BI_RGB de 8, 16 (RGB555), 24 y 32 bits (verde puro = transparente en
  sprites) y **sprites RLE** (` elr` ... `RLE\0`, columnas x filas de cuadros, formato 1 = indexado con paleta BGRA,
  3 = RGB555, 7 = solo sombra; cada fila es una lista de (salto, cuenta)). En los indexados, los grises 240-255 se
  cambian por la rampa del color del reino (112 + 16·color). Las filas de los RLE van por dirección y se reordenan.
- **`IMAGES.INI`** y `IMAGES/TERRAIN/<bioma>.IMAGES.INI`: secciones de dibujos con `image`, `cols`, `rows`, `object`
  (esquina y tamaño en casillas), `selection`, `hotspot`, `startrow`, `fps`; `<sección>_s` = sombra, `image_c` =
  estandartes del color del reino. `DATA/OBJECTS/*.INI`: cada clase con sus dibujos por civilización (`euro`, `arab`,
  `asia`, `comm`).
- **Mapas `.wmp` y campañas `.wcm`** (`hpfs.rs`): contenedor **HPFS** de bloques de 512 bytes con inodos (tamaño,
  profundidad de indirección, 126 punteros) y directorios; una campaña contiene sus mapas como ficheros HPFS anidados.
  Cada mapa: `.header` (`2mzt`, lado en 6, objetos en 8, bioma en 0xd6, jugadores en 0x4c), `.tiles` (lado² palabras:
  terreno a/b en bits 7-9/4-6, forma 0-3, acantilado bit 10, árbol bits 11-15), `.objects` (registros de longitud
  variable: grupo/clase, banderas, x, y, dueño, civilización, variante, vida, imagen propia), `.objext` (héroes),
  `.kingdominfo` (8 x 152: nombre, color, equipo, recursos, IA), `.diplomacy`, `.events/.actions/.rules` (guiones),
  `.conv` (conversaciones), `.sounds`, listas de prohibidos e investigaciones iniciales.
- **Terreno** (`terrain.rs`): `IMAGES/TERRAIN/<bioma>.TERRAIN.INI` con atlas por pareja de terrenos (`[terrainfiles]`
  `ab=fichero`, `[terrainsize]` columnas,filas[,cuadros de costa]); las transiciones son máscaras con **magenta**
  (terreno a) y **verde** (terreno b) que se rellenan con las texturas de los dos; agua animada por páginas; árboles en
  un atlas de 31 columnas con su sombra; acantilados en atlas de 6 x 14.
- **Sonido** (`sounds.wdt`): 438 WAVE PCM y 14 MIDI estándar (la música). **Fuentes** `LOCAL/FONTS/*.FNT`.
  **Textos** `LOCAL/STRINGS*.INI` en Windows-1252 (castellano en esta edición). **IA**: `DATA/AI/*.ais` y `*.ai`
  (lenguaje propio que compila el Tzar.exe; ReTzar lo traduce). **Vídeo**: `intro.MPG` (MPEG-1). La edición FX no trae
  DDV, así que **no hace falta leer tablas del Tzar.exe**.

### Bucle de juego e IA en ReTzar

- Simulación **determinista por tics** (`simulation.rs`: reloj fijo, atributos de `DATA/OBJECTS/*.INI` y tablas de
  tecnología) separada del dibujo (Bevy solo pinta). Cada tic: guiones de misión (`scripting.rs`), órdenes de cada
  objeto (`gameplay.rs`: andar, atacar, construir, recoger, hechizos, guarniciones...), movimiento (`movement.rs` +
  `pathing.rs`, A* por casillas con islas precalculadas) y la IA.
- **IA**: la del original, que se programa en los guiones `DATA/AI/*.ais`. ReTzar traduce el compilador del Tzar.exe
  (`ai/script.rs`) y el intérprete de reglas (`ai/rules.rs`, `think.rs`) más los «gestores» nativos: construir
  (`build`), recoger (`harvest`), guerra y defensa (`war`, `defence`, `breach`), flota y transportes (`navy`,
  `transport`), hechizos (`spells`), lugares de la base (`places`).
- Dibujo: orden del pintor por (prioridad de la clase, fila de la casilla, columna de derecha a izquierda, capa
  sombra < cuerpo < estandarte); el terreno, los parches de suelo y los acantilados van debajo de todo.

### Comparación con AoEN (`userland/juegos/aoen/`, `docs/aoen.md`)

| Pieza | AoEN | Tzar | Reaprovechar |
|---|---|---|---|
| Ventana, bucle, entrada | SDL2, superficie de la ventana, escala entera | igual | **sí**: el esquema de `main.c` (bucle, `*_SHOT`, `*_FRAMES`, registro `[TZAR]`) |
| Hilos | `hilos.c` (reparto del dibujo y de la simulación por franjas) | igual | **sí**, tal cual (es del mantenedor) |
| Dibujo | isométrico en rombos, SLP, rutas SSE2 (`rapido.c`) | **casillas cuadradas** de 32x32 vistas en oblicuo, RLE propio | las ideas (copiar por filas, sombra al 50 %, recorte), no el código del rombo |
| Terreno | losetas + transiciones + HD en punto fijo | atlas por pareja de terrenos con máscaras | no (formato distinto) |
| Caminos | A* por casillas | A* por casillas con islas y reglas del original | la estructura (montículo binario, rejilla); la lógica, de ReTzar |
| IA | propia (`ia.c`) | la original, guionizada | no (Tzar necesita la suya para que las misiones funcionen) |
| Interfaz | propia (`ui_*.c`), fuente de 8x16 | la original (pantallas `S_*`, fuentes `.FNT`) | los controles básicos; el aspecto, de los datos |
| Sonido y música | mezclador SDL + sintetizador MIDI propio (`music.c`) | WAVE + MIDI | **sí**: `audio.c` y `music.c` casi enteros |
| Datos | `/jogos/AOE`, `AOEN_DATA`, `aoen.cfg` | `/jogos/TZAR`, `TZAR_DATA` | el mismo esquema de búsqueda |
| TCC | fuentes de AoEN para recompilar dentro | | más adelante, igual |

### Plan por fases

| Fase | Contenido | C estimado | Dificultad |
|---|---|---:|---|
| 1 | Estudio, plan, decisión D417 | - | baja |
| **2** | Lectores (wdt+LZSS, HPFS, INI, BMP, RLE, mapa, objetos), terreno, árboles, acantilados y **visor** `tzar` con desplazamiento y minimapa; prueba de unidad con datos sintéticos y prueba en QEMU (h417) | 3 500 | media |
| 3 | Animación (tiempos de `animation.rs`, agua, fuegos), selección y fichas de objetos, textos de `STRINGS.INI` | 2 500 | media |
| 4 | Interfaz original: fuentes `.FNT`, barra de la partida, órdenes, minimapa, menús principales | 9 000 | media-alta |
| 5 | Simulación sin IA: atributos, órdenes, movimiento y caminos, combate, construcción, recogida, producción, guiones de misión; partida jugable contra reinos quietos; guardar/cargar | 25 000 | alta |
| 6 | IA original (compilador e intérprete de `*.ais`, gestores) | 12 000 | alta |
| 7 | Campañas (paso entre misiones, conversaciones), mapas aleatorios, sonido y música, vídeo de la introducción | 8 000 | media |
| 8 | Rendimiento (hilos, rutas SSE2), equipos antiguos, multijugador (si se decide) | 3 000 | media |

Cada fase se prueba en el PC (SDL sin pantalla) con datos sintéticos en la batería y, a mano, con los datos reales en
capturas **privadas** (`/home/aanun/SO/private/tzar/capturas/`).

## Fase 2: lectores y visor

Código en `userland/juegos/tzar/` (C99, GPL-3.0-or-later):

| Fichero | Traducido de ReTzar | Qué hace |
|---|---|---|
| `wdt.c` | `archive.rs` | paquetes `*.wdt`: LZSS por trozos bajo demanda, directorio HMMSYS, carpetas sueltas |
| `hpfs.c` | `hpfs.rs` | contenedores HPFS y anidados (sin copiar la campaña a memoria) |
| `recursos.c` | `assets.rs` | los paquetes de una instalación en su orden, índice por nombre, INI |
| `imagen.c` | `bitmap.rs`, `sprite.rs` | BMP y sprites RLE a ARGB, color del reino, sombras |
| `mapa.c` | `map.rs`, `world.rs` (lectura) | cabecera, casillas, objetos, reinos |
| `terreno.c` | `terrain.rs`, `world.rs` (árboles, acantilados) | losetas y transiciones, árboles, acantilados, minimapa |
| `escena.c` | `world.rs` (biblioteca y pintor) | qué dibujo lleva cada objeto, dónde y en qué orden |
| `visor.c` | (propio) | el programa `tzar`: ventana SDL, desplazamiento, minimapa, capturas |

Dónde poner los datos: la carpeta de la instalación (con `Packs/` y `Maps/`) en **`/jogos/TZAR`** (en las imágenes
privadas, `input_share/games/TZAR/`), o donde diga `TZAR_DATA` o el primer argumento. Sin datos, el visor lo explica en
su ventana.

### Uso del visor

`/apps/bin/tzar [carpeta [mapa]]`. El mapa es `Maps/MP1.wmp` o `Campaign/<campaña>.wcm|MAPS/<mapa>.wmp` (también en
`TZAR_MAPA`); sin él, el primero de `Maps/`. Flechas o WASD (con Mayús, más deprisa) o el ratón en los bordes para moverse,
clic o arrastre en el minimapa para saltar, **N/P** (o AvPág/RePág) para el mapa siguiente o anterior (sueltos y de las
campañas), Inicio para volver a la esquina, Esc para salir. Pruebas: `TZAR_SHOT=f.bmp TZAR_FRAMES=n` (guarda el cuadro n
y sale), `TZAR_POS=x,y`, `TZAR_W`/`TZAR_H`. Registro: `[TZAR] ...` (mapa, dibujos, fallos, tiempo de carga y de dibujo).

### Resultado (2026-11-19)

- **Con la instalación FX real** (comprobación privada `h417 privado`, en el PC con ASan/UBSan): las **2 421 entradas**
  de los 8 paquetes, **590 BMP**, **961 sprites RLE**, 187 INI y los **71 mapas** (6 sueltos y 65 de las 6 campañas,
  hasta 512x512 y 73 000 dibujos) se leen **sin ningún fallo**, con todas las losetas, el minimapa y la escena (ningún
  objeto sin dibujo). Unos 15 s para todo con ASan. Abrir un mapa de 256x256 cuesta 85-120 ms en el PC.
- **Visor**: terreno con transiciones, acantilados, árboles con sombra, edificios con los estandartes del color del reino,
  unidades, recursos, objetos del escenario, puertas y puentes, minimapa. Dibujo de un cuadro de 1024x720 en el PC: 0-2 ms.
  Capturas privadas en `/home/aanun/SO/private/tzar/capturas/` (nunca en `docs/capturas/`).
- **SaguiOS** (QEMU, datos sintéticos): `/apps/bin/tzar` (120 KB) abre el mapa de `/jogos/TZAR` (H417-1) y sin datos
  explica dónde ponerlos (H417-2).
- **Pruebas**: `tests/h417_tzar.py` (`git`, `lectores`: 41 comprobaciones con ASan/UBSan y sin fugas, `pc`, `saguios`;
  `privado` a mano). Datos sintéticos: `tools/tzar_sintetico.py` (LZSS, HMMSYS, BMP, RLE, HPFS hechos desde cero).

### Pendiente de la fase 2 (queda para otras fases)

Entrada en Inicio > Juegos, probar con datos reales dentro de SaguiOS (imagen privada) y medir la memoria y el tiempo de
carga en equipos antiguos.

## Fase 3: animación, selección y textos (rama `tzar-anim`)

Ficheros nuevos de `userland/juegos/tzar/` (C99, GPL-3.0-or-later; cada uno cita su origen en ReTzar):

| Fichero | Traducido de ReTzar | Qué hace |
|---|---|---|
| `animacao.c` | `animation.rs`, `flight.rs` | `TzTempo` (líneas de tiempo: `directional`, `stepped`, `uniform`, `frame`), pájaros en vuelo (`TzVoo`), `tz_animar(escena, ms)` |
| `dibujo.c` | `font.rs` | lienzo de 32 bits (`TzLienzo`, `tz_pintar`, rectángulos) y fuentes `.FNT` de Haemimont (`TzFonte`) |
| `textos.c` | `assets.rs` (`Ini::locale`), `world.rs` (`class_label`) | `LOCAL/STRINGS*.INI` fusionados, nombre de clase y ficha, Windows-1252 a Latin-1/UTF-8 |
| `selecao.c` | `frontend/mission.rs`, `selection.rs` | marco de selección (esquinas de 6 píxeles), barra de vida y maná, objeto bajo el ratón |
| `escena.c` (ampliado) | `world.rs` (`timeline`, `object`, `gate`, `fish`, `flight`, `GATE_FLAGS`, `site_picture`, `runtime_image`) | animaciones por visual, peces, aves, banderas de puertas, objetos con pose y efectos sueltos |
| `terreno.c` (ampliado) | `terrain.rs`, `frontend/scene.rs` | agua y costas animadas (WATERFPS = 4), transiciones que siguen el ciclo común |

### Qué se anima

- **Agua y costas**: la pareja `00` pasa páginas verticales y las costas horizontales a 4 por segundo (`tic = ms / 250`);
  una transición compuesta repite el ciclo común (mínimo común múltiplo) de sus tres losetas y cada cuadro compuesto se
  guarda en la caché.
- **Unidades paradas**: la hoja de reposo (`image_idle`) con su registro de animación (`anim_time`, `delay`, relleno de
  1 ms en 1 ms) desde un punto del ciclo según el `id`; las hojas de una columna, a su `fps`. (El reloj de reposo con
  sorteo de ±3 s de ReTzar, `IdleClock`, no se traduce: es de la simulación.)
- **Animaciones del mapa** (`animation`, `ground_object`, `ground_patch`, `item_flag`): a su `fps` o al de la palabra 0x58
  del registro; fuegos, humo, fuentes, molinos de los datos.
- **Estandartes y banderas** de edificios (`image_c`): a su `fps` o a 10 por segundo, en el color del reino.
- **Puertas**: las dos torres y el puente levadizo (fila elegida por quien llame) y las dos banderas del color del reino
  de `gates c` con sus posiciones (`GATE_FLAGS`).
- **Peces**: uno de los cinco saltos `fish_N` al azar (`object_noise`), a un cuadro cada 100 ms desde un cuadro al azar.
- **Pájaros** (gaviotas y águilas): vuelo con las vueltas del original (plantillas A y B de 22 puntos), un píxel cada
  `(32000 / move_len) >> 5` ms, columnas de giro y aleteo con sorteo. Sin buscador de caminos de vuelo: una casilla son
  32 pasos.
- **Obras y ruinas**: `tz_dibujar_objeto` con `em_obra` elige `build<w>x<h>` (o la de puertas, puentes, muros, puerto) y la
  fila por `vida / vida_max`; los escombros ya muestran el cuadro de su variante.

### API para la simulación (`tzar.h`)

```c
/* escena montada del mapa (visor) o vacía (simulación): las hojas se leen bajo demanda y se guardan */
int  tz_escena_preparar(TzEscena *e, TzBiblioteca *b, TzRecursos *r, const TzMapa *m);
void tz_animar(TzEscena *e, uint64_t ms);                 /* cuadro de cada visual y pájaros en el instante ms */
const char *tz_seccao_pose(b, o, estado, buf, n);          /* qué sección de IMAGES.INI corresponde a la pose */
int  tz_dibujar_objeto(e, lienzo, o, estado, px, py, camx, camy, ms);   /* sombra, cuerpo y estandarte */
int  tz_dibujar_efecto(e, lienzo, "fire_small", x, y, camx, camy, desde_ms, ms, fila_fija);  /* flechas: fila = dirección de 24 */
void tz_marco_selecao(lienzo, x, y, w, h, color);  void tz_barra_vida(lienzo, x, y, casillas, vida, vida_max, mana, mana_max);
int  tz_selecao_apontar(e, mapa, wx, wy);   int tz_selecao_caixa(e, obj, &x, &y, &w, &h);
int  tz_nome_objeto(textos, o, buf, n);     int tz_ficha_objeto(textos, o, buf, n);   /* Latin-1 */
```

`TzEstado` lleva pose (`TZ_POSE_PARADO/ANDAR/ATACAR/CONSTRUIR/TRABALHAR/FEITICO/MORRER`), dirección 0-7 (columna =
`dir * columnas / 8`), ataque 0-2 (`image_attack`, `2`, `3`; el que no existe vuelve al primero), tarea (`image_chop`,
`capture`, `milk`, `dig`, `mine`), fila del puente, obra y `desde_ms`. El dibujo de un objeto no deja nada en la
escena (solo las imágenes, en una caché); el orden del pintor es `tz_profundidade(x, y, lado)` (como el del visor).
Quien llame pone `px, py` = esquina de la casilla del objeto más su avance de paso.

### Visor (`tzar`)

El mundo se anima solo; un clic izquierdo selecciona (marco, barra de vida y ficha con el nombre de `STRINGS.INI` abajo
a la izquierda, con `ARIAL12B_AA.FNT` del juego); **H** muestra las barras de vida de todos. Pruebas: `TZAR_MS=n` (reloj
fijo) y `TZAR_CLICK=x,y`; registro `[TZAR] seleccion: objeto N clase`.

### Pruebas y resultado (2026-11-20)

- `h417 lectores` (datos sintéticos, ASan/UBSan, sin fugas): **95 comprobaciones** (eran 41), con los casos de las pruebas de
  `animation.rs` (reposo que aguanta 8000 ms y repite sin deriva, 994 ms por defecto, atrasos que crecen, coma final,
  `fps`), la fuente `.FNT` (avances y cobertura), `STRINGS` fusionados con variantes por civilización, agua y transición
  con dos páginas, el mapa `prueba/anim.wmp` (caballero, llama, peces, águila, puerta, casa), águila que se mueve sin
  salirse, selección, marco, barra de vida, `tz_dibujar_objeto` en cada pose (andar, atacar, morir, obra, puerta) y
  `tz_dibujar_efecto`.
- `h417 pc`: el agua de la captura cambia de página a los 250 ms, un clic en el peón lo selecciona y su marco aparece.
- `h417 privado` (a mano, instalación FX real): las **465 hojas animadas de cada bioma** (31 200 cuadros) se leen sin
  fallos; 8 secciones (`trees`, `trees shadow`...) no traen fichero en esta edición (usa el atlas del bioma);
  7 fuentes `.FNT` (`FIXSHAD.FNT` es de color y ReTzar tampoco la usa para texto); capturas privadas `anim-*.png` en
  `/home/aanun/SO/private/tzar/capturas/`.

### Pendiente

Reloj de reposo con sorteo (`IdleClock`) y movimiento entre casillas (`motion_offset`) son de la simulación; la barra de
vida de un edificio usa el ancho de la caja de selección en casillas (ReTzar: el de la huella); nombres propios de
héroes (`.objext`), redes de los barcos pesqueros, relámpagos (`bolt.rs`) y luces/niebla suave quedan para fases
siguientes; la hoja de aves no tiene buscador de caminos de vuelo.

## Fase 5: simulación (rama `tzar-sim`)

| Fichero | Traducido de ReTzar | Qué hace |
|---|---|---|
| `mundo.h` | (contrato) | modelo del mundo, comandos, eventos, ganchos, consultas para la interfaz, la IA y los guiones |
| `catalogo.c` | `simulation.rs` AttributeCatalog, `gameplay.rs` Catalog, `animation.rs` | atributos por clase y civilización (tablas TXT), costes, tiempos de los dibujos, huellas, tecnologías, órdenes, menús, BONUS.INI, RES.INI |
| `mundo.c` | `simulation.rs`, `gameplay.rs` (relaciones, población), `scripting.rs` (victoria) | creación desde el mapa, el tic, comandos, eventos, niebla, población, victoria, consultas, acciones de guion, puente con la animación (`tz_mundo_pose`) |
| `acoes.c` | `gameplay.rs` (programas de las órdenes), `movement.rs`, `combat.rs` | andar, atacar, disparar, recoger (minas, campos, vacas, pesca, montones), talar, entregar, construir, reparar, capturar, guarniciones, torres, producción, investigación y mejoras, comandos |
| `caminho.c` | `pathing.rs` | A* por casillas (16/22), capas de tierra, agua y aire, puertas, puentes, islas |
| `gravar.c` | (idea de `save.rs`) | partidas `TZSV` y suma del estado con el mismo recorrido |
| `demo.c`, `partida.c` | (propios) | jugador automático de prueba y partida mínima jugable en el visor |

**Jugar en el visor**: `J` (o `TZAR_JOGAR=1`) convierte el mapa en partida. Clic o arrastre para seleccionar, clic
derecho para la orden por defecto (Mayús encola), 1-9 para entrenar o investigar en un edificio o para elegir el
edificio que construyen los peones (se coloca con un clic), Supr para parar, Espacio para la pausa, + y - para la
velocidad, F5 y F9 para guardar y cargar (`tzar.sav`). `TZAR_DEMO=1` pone el jugador de prueba en todos los reinos.

**Pruebas**: `tests/tzar_simulacao.c` (sin cabeza: `tzar_simulacao <instalación> <mapa> <tics> [semilla] [--gravar f]`,
o `--listar`) y las partes `simulacao`, `pc` y `saguios` de h417 (el mismo mapa sintético da la misma suma en el PC y
en SaguiOS).

### Resultado de la fase 5 (2026-11-20)

- **Con la instalación real** (privado, en el PC con ASan/UBSan): los **71 mapas** (6 sueltos y 65 de campaña, hasta
  512x512 y 4 000 objetos) juegan 3 000 tics con el jugador de prueba en todos los reinos sin un solo fallo de memoria,
  dan **las mismas sumas dos veces** y **guardar a mitad y cargar** da la misma suma final; 38 llegan a un final. El
  peor mapa cuesta 12 ms por tic con ASan (unos 2 ms sin él); los mapas normales, 0,1-0,5 ms.
- **SaguiOS** (QEMU, datos sintéticos): la misma partida da **la misma suma que en el PC** (h417 `saguios`).
- Arreglos que salieron con los datos reales: el A* ya no recorre toda la isla cuando el destino está en otra (va a lo
  más cercano), no evita las casillas del propio destino, usa la heurística doble lejos (de 12 a 0,1 ms por tic en MP6 y
  de 7 a 1,9 en los mapas de 512); los punteros a `TzObj` se vuelven a pedir después de crear objetos (`m->obj` puede
  moverse).

## El juego completo (integración, `jogo.c`)

`/apps/bin/tzar` (Inicio > Juegos > Tzar; ayuda en `/ayuda/tzar.md`) es el juego: la introducción (`intro.MPG`, se salta con
una tecla), el **menú principal original** (`ui_menu.c`), la **escaramuza** en un mapa de la instalación o en uno
**aleatorio** (`aleatorio.c`) con la partida libre del original (castillo y tres peones por reino,
`tz_mundo_inicio_livre`) y la **IA original** (`ia/`) en los reinos del ordenador, las **campañas** (la misión alcanzada
según el progreso, su guion con historias, ventanas y conversaciones, `tz_guiao_pedido`, el resultado y la misión
siguiente con los héroes y variables que pasan), **guardar y cargar** (menú de la partida, F5/F9), el **sonido y la
música** con su volumen. La interfaz es la de la fase 4 (`ui_*.c`) sobre el mundo vivo (`tz_desenhar_mundo`).
`tzar --visor [carpeta [mapa]]` es el visor de las fases 2-5. Las imágenes privadas copian `input_share/Tzar Anthology`
a `/jogos/TZAR` (`tools/copiar_jogos.sh`, solo con `INPUT_SHARE`; hace falta `JOGOS_MB` de unos 1 500).

Pruebas: h417 `pc` (el juego arranca en el menú y juega una escaramuza con los datos sintéticos) y, a mano,
`h417_tzar.py privado_saguios` (el juego dentro de SaguiOS con los datos reales: menú, escaramuza contra la IA y una misión
de campaña; capturas privadas `saguios-*.png` y cuadros por segundo).

### Lo que falta o difiere

- Simulación (tras la v8.5.5.2, ver «Simulación completa»): las piedras del alma y los objetos de la fábrica mágica y las joyas del mercado asiático,
  los pergaminos no abren historia, los héroes de la taberna no reciben nombre de HEROES.INI, los precios de la taberna no llevan las rebajas de
  investigaciones, la elección de blanco no cuenta los atacantes que ya tiene cada candidato, y el juego de dados («gamble») no está.
- Juego: la escaramuza y las campañas de un jugador (sin red).
- Interfaz: la diplomacia no tiene tributos ni rendición; el chat solo sirve para la orden «ai ...» de la IA y para leer
  lo que dice; los cursores no se animan (primer cuadro de cada forma); la taberna, el comercio, los objetos, los préstamos,
  el intercambio y la cruzada de la simulación completa aún no tienen ventanas propias (se mandan por las órdenes del menú).

## Simulación completa (v8.5.5.2, rama `v8.5.5.2-tzar-sim`, D430)

Todo traducido de `gameplay.rs`/`combat.rs`/`random_map.rs` de ReTzar (cada fichero cita su origen). Pruebas: `tests/tzar_extras.c`
(h417 `extras`, 123 comprobaciones con ASan/UBSan y datos sintéticos) y las ampliaciones de `campanhas`.

| Fichero | Qué hace |
|---|---|
| `comercio.c` | rutas de mercaderes entre mercados (`TZ_O_COMERCIAR`: carga 500/300/100, ganancia `carga*distancia*(40+bonus)/(lado*100)`, un mercader por mercado, Trade taxes y Banking), `TZ_O_VENDER`, intercambio de la taberna (`TZ_C_TROCAR`), préstamos, ofertas y contratos de mercenarios y héroes, cruzadas de la catedral, monje a sacerdote y peón a guerrero de la yihad |
| `itens.c` | recoger, llevar (hasta `max_obj_in_holder`), soltar, dar, usar: pociones (suben máximos, ataque y defensa para siempre, experiencia), equipo (suma mientras se lleva), libros (aprender o lanzar sin maná con cargas), venta en el mercado, soltar al morir |
| `magia.c` | además: maldición, hipnosis, invisibilidad, remolino, alzar muertos, enseñar, espías (suplantar, revelar) y el golpe mortal del ninja |
| `mundo.c` | experiencia y niveles como el original (tope 0 / Learning +4 / academias +10 / Heroism sin tope; 100/70/50/35 %; Body Building, Nobility, Tournaments; héroe al nivel 12), unidades grandes 2x2, recursos compartidos, rescate de prisioneros del reino 9 |
| `acoes.c` | elección de blanco con las tablas de prioridad de `combat.rs` (humano, IA y catapultas), invisibles fuera de la vista enemiga |
| `aleatorio.c` | mapas aleatorios que se pueden guardar y cargar (`tz_aleatorio_texto`/`_de_texto`, una línea en el `.txt` de la partida), puertas mágicas y objetos al azar; las águilas eran la clase 135 (bandera) y ahora la 119 |

**Cambios en `mundo.h` (solo se añade):** clases `TZC_*` nuevas; `TzItem` y `TzObj.item/pocao_ataque/pocao_defesa`; campos nuevos al final de
`TzAtor` (ruta, intercambio, ofertas, efectos, espía, libro, remolino) y de `TzReinoEstado` (préstamo, cruzada, torneo); `TzMundo.sem_resgate`;
órdenes `TZ_O_VENDER/ASESINAR/SUPLANTAR`; comandos `TZ_C_EMPRESTAR/DEVOLVER/TROCAR/CONTRATAR/CRUZADA/CONVERTIR/VENDER`; eventos
`TZ_EV_COMERCIO/ITEM/CONTRATO/EMPRESTIMO/CRUZADA/TROCA` (`TZ_EV_TRABALHO`, un evento por golpe de trabajo, ya existía: el sonido no necesita
sondear); consultas `tz_mundo_ofertas/preco_oferta/lucro_rota/capacidade_comercio/itens/capacidade_itens/criar_item/valor_item/conhece_feitico/
regras_xp/experiencia/pode_resgatar`, `tz_feitico_ordem`, `tz_catalogo_custo_ini`. `TZSV` pasa a la versión 5. Convenciones: comercio con
`TZ_ARG_COMERCIO(r0, r1, cantidad)`, origen en `(x2, y2)`, destino en `alvo`; objetos: `alvo` = el objeto, `arg = 1` con objetivo en `(x, y)`;
revelarse el espía: `TZ_C_CONVERTIR` con `arg = TZC_SPY` sobre el anfitrión. **Unidades grandes** (mercader, catapultas, ballesta, barcos,
dragón): la casilla `cx,cy` y `px,py` son la de arriba a la izquierda y ocupan 2x2; la escena no cambia.

## Pantalla del mapa aleatorio (v8.5.5.2, rama `v8.5.5.2-tzar-aleat`, D435)

Inicio > Un solo jugador > Mapa aleatorio abre `TZ_MN_ALEAT` (`ui_menu.c`), la `S_SINGLERND` del original, con el mismo arte y rectángulos que
`Screen::Random` de ReTzar. Lo que tiene el original (y ReTzar) y cómo está aquí:

| Opción | Estado |
|---|---|
| Tamaño 64, 96, 128, 192, 256, 384, 512 | sí; el número de reinos y los tipos permitidos dependen del tamaño como en el original |
| Tipo de mapa (al azar, archipiélagos, islas, continente, lagos, tierra) | sí; en 64 no hay archipiélagos, islas ni lagos; en 96 ni archipiélagos ni lagos; en 128 ni archipiélagos |
| Terreno (al azar, hierba, verano, nieve, desierto) | sí; «al azar» sale de la semilla |
| Recursos del mapa (pocos, normales, muchos) | sí (cantidad de las minas) |
| Torres de guardia, animales, objetos mágicos, puertas mágicas | sí |
| Montañas | **apagada** (la casilla sale en gris): ReTzar no las genera y falta la traducción |
| Número de reinos (2 a 8), civilización y equipo de cada uno | sí; los del mismo equipo son aliados (nuevo) |
| Tipo de partida, victoria, población, dificultad, recursos iniciales (bajo, medio, alto), sin niebla, explorar | sí |
| Comportamiento de la IA de cada reino, rendición de la IA, límite de la victoria por tiempo o puntos | no (la IA usa su guion estándar; el límite queda en 30) |
| Jugador humano en otra fila, unidades iniciales, edad de inicio | el original no deja elegirlas (el humano es el reino 1; castillo y tres peones) |
| Semilla | sí, «Otro mapa» la cambia y se ve bajo la vista previa |

La **vista previa** (96 píxeles; un clic la amplía a 420) genera el mapa con las opciones de la pantalla y la reutiliza el minimapa de la
escaramuza (`previa_desenhar`, `jogo.c`); se regenera solo cuando cambia la línea de opciones, y el catálogo del bioma queda guardado entre
cambios. Las **últimas opciones** se guardan en `tzar.cfg` (clave `aleatorio=`, una línea `ALEAT2 ...` con todo) al salir de la pantalla o al
jugar, y cada partida nueva avanza la semilla. La **partida guardada** lleva la línea `ALEAT` de siempre (tamaño, reinos, tipo, recursos,
torres, animales, terreno, semilla, equipos, civilizaciones, puertas, objetos), así que cargar regenera el mismo mapa; «reiniciar» y
«repetir» tras perder o ganar repiten el mismo mapa (antes salía otro).

Variables de prueba: `TZAR_MENU=13` abre la pantalla, `TZAR_ALEAT='ALEAT2 ...'` pone las opciones, `TZAR_AMPLIAR=1` amplía la vista previa,
`TZAR_AUTO=aleatorio` juega con ellas y `TZAR_PRUEBA_GUARDAR=1` guarda, carga y compara. Pruebas: h417 `campanhas`, `ui` y `pc`; a mano,
`h417_tzar.py privado_aleat` (SaguiOS con los datos reales; capturas privadas `aleat-*.png`, nunca en git).

**Mapas aleatorios aún sin traducir:** montañas (opción apagada en ReTzar), ruinas del rey de la colina (solo en ese modo) y parches/objetos de
suelo (la edición FX no trae dibujos `gp_`/`go_`). **Guiones:** recursos compartidos, «explorar» temporal, rescate del reino 9 y
`@kingdomcanrescue` ya funcionan; `@build` termina la obra como cualquiera (ya lo hacía).

**Posibles causas de cuelgue** que se vieron leyendo el código (no reproducidas): `tz_mover_para` devuelve 0 mientras `falhas` es 1..4 y el
destino sigue sin camino, y `tz_caminho` sigue limitado a 40 000 nodos, sin bucle sin fin en lo revisado; el bucle de `repartir` en `guiao.c`
solo acaba si cada regla acaba o espera: una regla «mantener» sin espera con una acción que no avanza el reloj lo repetiría cada periodo, no
dentro del mismo tic.
## Arquitectura del juego (fases 3-8)

El contrato entre los módulos está en **`userland/juegos/tzar/mundo.h`**; esta sección explica cómo encaja. Lo
escribió el agente de la simulación (rama `tzar-sim`) para que la interfaz, la IA, las campañas y el sonido trabajen a la
vez sobre él.

### Capas

```
 datos (fase 2: wdt, hpfs, recursos, imagem, mapa, terreno, escena)
        |
 catálogo (catalogo.c): atributos por clase y civilización, tecnologías, órdenes, menús, BONUS.INI  -- solo lectura
        |
 mundo (mundo.c, movimento.c, combate.c, economia.c, producao.c, gravar.c...)                      -- fase 5
   ^ comandos (tz_mundo_comando)          | eventos (tz_mundo_eventos, cada lector con su cursor)
   |                                      v
 interfaz (fase 4) · IA (fase 6, gancho por reino) · guiones de misión (fase 7, gancho) · sonido (fase 7, solo eventos)
        |
 dibujo: la escena lee el mundo (tz_mundo_objeto_mapa / TzObj: estado, trabajo, dirección, px/py, anim_tic)
```

### Reglas

1. **Determinista**: solo enteros, un generador (`TzRng`, el `NativeRng` del original), orden fijo (objetos por índice,
   reinos 1..8). La misma semilla y los mismos comandos dan la misma `tz_mundo_suma()` en el PC y en SaguiOS. Si se usan
   los hilos de AoEN (`aoen/hilos.c`), solo para cálculos que no dependan del orden (niebla por franjas, caminos pedidos
   en el tic y aplicados después en orden de índice).
2. **Nadie escribe en `TzMundo` desde fuera.** La interfaz, la IA y la red mandan `TzComando` (ordenar, construir,
   producir, investigar, cancelar, reunión, modo, derribar, diplomacia, rendirse) que se aplican al principio del tic
   siguiente. La IA y los guiones son **ganchos** (`TzGancho`) que corren dentro del tic; lo que manden se aplica en el
   mismo tic. Los guiones tienen además acciones directas `tz_mundo_guiao_*` (crear, quitar, matar, cambiar de dueño,
   recursos, diplomacia, revelar, investigación, prohibir, orden sin dueño, mensaje, sonido, final).
3. **Eventos** (`TzEvento`, cola circular de 4096): nacer, morir, golpear, disparar, impactar, órdenes, obras,
   entrenamiento, investigación, recursos, sin recursos/población, aviso de ataque, captura, nivel, puertas, hechizos,
   derrota, final, mensajes y sonidos de guion. **El sonido es solo un lector de eventos**; la interfaz los usa para
   avisos; los guiones, para sus sucesos.
4. **Índices**: un objeto es su índice en `m->obj` (`TzId`); no se reutiliza en toda la partida (los muertos quedan con
   `TZ_F_MORTO`). Los punteros solo valen dentro del tic. `TzMundo` es grande (cola de eventos dentro): va en memoria
   dinámica o estática, nunca en la pila.
5. **Unidades**: casillas de 32 px; posición fina en 1/256 de casilla; tic de 50 ms (20 por segundo, `TICK_MS` de
   ReTzar); 8 direcciones (0 = norte, horario); recursos en el orden del motor (oro, piedra, comida, madera); reino 0 =
   naturaleza, 1..8 los del mapa, 10 = discordia.

### Qué es de cada agente

| Módulo | Ficheros | Usa de `mundo.h` |
|---|---|---|
| Simulación (fase 5, `tzar-sim`) | `catalogo.c`, `mundo.c`, `movimento.c`, `combate.c`, `economia.c`, `producao.c`, `gravar.c` | todo; es el único que escribe en el mundo |
| Animación y escena (fase 3, `tzar-anim`) | `escena.c`, `animacao.c` | `TzObj` (estado, trabajo, dirección, `px/py`, `anim_tic`), `tz_mundo_objeto_mapa`, `tz_mundo_nevoa` |
| Interfaz (fase 4) | `ui_*.c` | consultas (`tz_mundo_area`, `tz_mundo_em`, `tz_mundo_pode`, `tz_catalogo_menu`, costes, `tz_mundo_cabe`, `tz_mundo_ordem_padrao`), comandos, eventos (avisos) |
| IA (fase 6) | `ia/ia*.c` | un gancho por reino; **solo** ve lo que `tz_mundo_ve`/`tz_mundo_nevoa` le dejan (como un jugador) y solo actúa con comandos |
| Campañas y guiones (fase 7) | `guiao*.c`, `campanha.c` | un gancho; eventos; `tz_mundo_guiao_*`; `TzRegras.campanha` y `sem_vitoria_auto` |
| Sonido y música (fase 7) | `som.c`, `musica.c` | solo eventos (con su cursor) y la posición de la cámara |

### Guardar y cargar

`tz_mundo_gravar`/`tz_mundo_carregar`: formato propio `TZSV` (versión, semilla, tic y bloques con etiqueta: objetos,
actores, reinos, casillas que cambian, proyectiles, niebla, y un bloque por gancho con su `etiqueta`). Se carga sobre el
mismo mapa; una partida cargada sigue con la misma suma que la original.

## Fase 4: interfaz original (rama `tzar-ui`)

Módulos `ui_*.c` (`ui.h`), traducidos de `font.rs`, `hud.rs`, `menu.rs` y `frontend/gameplay.rs` de ReTzar. No usan la SDL: dibujan en
un lienzo ARGB (`TzTela`) y reciben la entrada como `TzUiEntrada`; el programa del juego traduce la SDL. Solo hablan con el mundo por
`mundo.h` (lecturas, `tz_mundo_comando`, eventos con cursor propio `ev_cursor`).

| Fichero | Qué hace |
|---|---|
| `ui_fonte.c` | fuentes `.FNT` del juego (arial10/12b/14b, cgb_22b, small): cobertura 0-7, avances por carácter, medir, recortar con «...», partir en líneas |
| `ui_skin.c` | arte bajo demanda (caché), `STRINGS.INI` 1-3 en el idioma de la instalación, iconos (`ICONS.INI` y `DATA/OBJECTS/<clase>.INI`), colores de reino de `COLOR BARS` y `PALETTE.BMP`; skin vacía con la letra del sistema |
| `ui_hud.c` | marco de piedra de 800x600 a 1920x1080 (franjas en espejo), contadores, vista de selección (retrato, vida, estadísticas, varias unidades), rejilla 2x4 de órdenes con coste y motivo, flechas de página/submenú, minimapa con niebla y rectángulo de vista, línea de ayuda, avisos |
| `ui_selecao.c` | clic, Mayús, recuadro (unidades antes que edificios), doble clic (misma clase en pantalla), grupos Ctrl+0-9, clic derecho con `tz_mundo_ordem_padrao` (Mayús encola, edificios fijan la reunión, más de 64 unidades en varios comandos) |
| `ui_jogo.c` | ratón y teclado de la partida, cámara (flechas, bordes, minimapa), colocación de edificios (`tz_mundo_cabe`, huella centrada), órdenes con objetivo, atajos de `MENU.INI`, avisos desde `TZ_EV_SEM_RECURSOS`, `SEM_POPULACAO`, `ATAQUE_AVISO`, `MENSAGEM` y `FIM` |
| `ui_mapa.c` | terreno y escena provisionales (como el visor) con la niebla del jugador; la animación de la fase 3 lo sustituirá |
| `ui_menu.c` | principal, un jugador, campañas (las 6), misiones, escaramuza con opciones (jugadores, tipo, victoria, población, dificultad, recursos, razas, sin niebla, explorar), cargar, guardar (nombre editable), opciones, créditos, salir con confirmación y menú de partida. Devuelve la elección en `TzMenu.acao` |

Convenciones que la simulación debe respetar (o decir si cambian):
- `tz_catalogo_menu` solo da el menú raíz de la clase; la interfaz abre los submenús leyendo `cat->menus` (secciones de `MENU.INI`, en orden) y
  resuelve cada clave con `tz_catalogo_ordem`. La variante `<menu>_b` se usa si el edificio tiene cola de producción.
- Una orden con `submenu` abre ese menú; `build X` entra en colocación; `create X` manda `TZ_C_PRODUZIR` (Mayús x5); `technology/upgrade` manda
  `TZ_C_PESQUISAR`; `stop`, `stand_ground`, `hold_steady`, `holdfire`, `die`, `explore` van directas; el resto con objetivo manda `TZ_C_ORDEM`
  tras un clic izquierdo, y en los hechizos `TzOrdem.arg` es el índice de la orden en `cat->ord`.
- La interfaz recorre `m->obj` (solo lectura) en lugar de `tz_mundo_area` porque el contrato no dice qué «dono» significa «cualquiera».
- Eventos que usa: `TZ_EV_SELECIONADO` (lo emite ella al elegir), `SEM_RECURSOS` (`classe` = recurso), `SEM_POPULACAO`, `ATAQUE_AVISO`, `MENSAGEM`, `FIM`.

Pruebas: `python3 tests/h417_tzar.py ui` (121 comprobaciones desde la v8.5.5.2; eran 93 con ASan/UBSan, mundo de prueba `tests/tzar_ui_stub.c`, sin datos del juego) y,
a mano con los datos reales, `h417_tzar.py privado` (capturas `ui-*.png` en `/home/aanun/SO/private/tzar/capturas/`).
Hecho en la v8.5.5.2 (ver «Interfaz, pantalla y sonido (v8.5.5.2)»): panel a la derecha, diplomacia y chat, instrucciones, créditos
animados, lista de misiones, previsualización del mapa, fin de partida (`D_ENDGAME`) y cursores del juego. Pendiente: las pistas de
`LOCAL/HELP.INI` sobre los controles de los menús (las instrucciones salen de `HELP/HK.TXT`, como en ReTzar) y conectar con `visor.c`.

## Fase 7: campañas, guiones, mapas aleatorios, sonido, música e introducción

Rama `tzar-campanas`. Todo se programa **solo contra `mundo.h`** (más `recursos.c`, `hpfs.c` y `mapa.c` de la fase 2).

| Fichero | Traducido de ReTzar | Qué hace |
|---|---|---|
| `guiao_ler.c` | `script.rs`, `dialogue.rs`, `world.rs` (`.objext`, `.diplomacy`) | lee `.events/.actions/.rules/.defeat.messages/.conv/.objext/.diplomacy` de un mapa; un guion dañado no impide abrirlo |
| `guiao_expr.c` | `script/expr.rs` | el lenguaje de expresiones con las reglas del nativo (`8-4-2` = 6, `&` más fuerte que `+`...) |
| `guiao.c` | `gameplay/scripting.rs` | reglas, sucesos, 28 acciones, temporizadores, conversaciones, final de la partida (victoria, derrota, límites) |
| `guiao_gravar.c` | (propio) | el bloque `GUIA` de la partida guardada |
| `campanha.c` | `scenario.rs`, `menu_ui.rs` | catálogo de mapas y campañas, orden de la historia, progreso (`TZPROG1`), héroes y variables `#` entre misiones |
| `aleatorio.c` | `random_map.rs` | mapa aleatorio: plasma, terreno por capas, bosques, formas de inicio con castillo, torres, minas, aldeanos, árboles, peces y animales |
| `som.c` | `audio.rs`, `frontend/gameplay.rs` | mezcladora de 30 huecos con las prioridades del original, atenuación por distancia, eventos a sonidos, habla, película |
| `musica.c` | `audio.rs` | lista de misión, pausa, intro, victoria; el sintetizador MIDI es el de la libsagui (`audio_midi.c`) |
| `intro.c` | (propio) | `intro.MPG` con el decodificador MPEG-1 de la libsagui; en el PC se salta |

### Cómo se engancha el guion (para la interfaz y quien arranque la partida)

```c
TzMissaoAberta ab;  tz_missao_abrir(&mapas, &mision, &ab);          // mapa + guion
tz_mundo_criar(&m, cat, &ab.mapa, &regras, semente);
tz_guiao_registrar(&m, &ab.guiao, 1 /*campaña*/);                   // gancho «GUIA»; pone m.jogador y sem_vitoria_auto
tz_levar_para(&m, &ab.guiao, &levar);                               // héroes y «#» de la misión anterior
```

Cada tic la interfaz vacía `tz_guiao_pedido()`: historia (`TZ_PD_HISTORIA`, con voz, música y duración), aviso (`TZ_PD_AVISO_JANELA`), línea o
elección de conversación (`TZ_PD_FALA`; `tz_guiao_fala`, `tz_guiao_mensagem`), cámara (`TRAVAR`, `ROLAR`, `DESTRAVAR`), selección y notas
(`TZ_PD_NOTA` con la clave de `[viewmsg]`). Mientras `tz_guiao_modal()` la partida espera: la interfaz contesta con `tz_guiao_fechar` o
`tz_guiao_responder(escolha)`. Las líneas de texto y los sonidos por nombre salen por los eventos normales (`TZ_EV_MENSAGEM`, `TZ_EV_SOM`). Al
acabar, `tz_guiao_acabou` y `tz_guiao_proximo_mapa` dicen si ganó y a qué misión sigue; `tz_levar_de` recoge lo que pasa a la siguiente y
`tz_progresso_marcar` anota la misión alcanzada.

El guion escribe directamente en `TzObj` lo que `mundo.h` no cubre con una acción `tz_mundo_guiao_*` (poner en juego un objeto oculto, salud,
nivel, maná, bloqueo, «dios»): están en `guiao.c` (`gm_*` y los `case`), para cambiarlos por llamadas del mundo si la simulación las ofrece. La IA
(`@setai`, `@setnoattacktime`, `@kingdomcanrescue`) se conecta con `tz_guiao_servicos`.

### Qué falta o difiere de ReTzar

- Guiones y mapa aleatorio: resuelto en la v8.5.5.2 salvo lo que dice «Simulación completa».
- Sonido: el trabajo (talar, minar, construir) suena mirando el estado de los objetos (el mundo ya emite `TZ_EV_TRABALHO` por golpe:
  pendiente usarlo); sin sonidos de la naturaleza por hábitat y hora (`audio/ambience.rs`). Las voces que siguen a un objeto, los
  sonidos del mapa (`.sounds`) y la lluvia están desde la v8.5.5.2.

### Resultado (2026-11-20)

- **Con la instalación real** (privado): los guiones de las **71 misiones** (6 campañas y 6 mapas sueltos: 1 063 reglas, 3 245 acciones, 270
  conversaciones con 907 frases) se leen sin fallos y corren sin romper nada, todas sus reglas nombran acciones que existen, y con la **simulación real**
  las 65 misiones de campaña corren 143 557 tics (se disparan reglas, se abren ventanas, 18 misiones llegan a un final); los **438 WAV** de
  `sounds.wdt` se decodifican, los **14 MIDI** se sintetizan enteros y los 70 grupos de `SOUNDS.INI` nombran clips que existen (salvo 24 de su
  sección «OLD STUFF»); se generan mapas aleatorios de los 4 biomas y se abren en la simulación real. Capturas privadas:
  `/home/aanun/SO/private/tzar/capturas/camp-*.png`.
- **Pruebas**: `tests/h417_tzar.py campanhas` (134 comprobaciones con datos sintéticos, ASan/UBSan: expresiones, reglas, ventanas, temporizadores,
  conversaciones, final, héroes, guardar y cargar con 1 800 bloques dañados, 606 bytes cambiados al azar, catálogo, progreso, mapas aleatorios,
  mezcladora y MIDI) y `tests/tzar_campanas.c privado|real` a mano.
- El desbordamiento de `userland/libsagui/audio_midi.c` (`tmp[i] * 22000`) ya está arreglado (cuenta en 64 bits).

## La IA (fase 6, rama `tzar-ia`)

Traducción a C de la IA original de ReTzar (`src/gameplay/ai.rs` y `src/gameplay/ai/`), en `userland/juegos/tzar/ia/`
(unas 3 800 líneas): cada fichero dice de qué fichero de ReTzar y de qué rutina del Tzar.exe sale cada función.

| Fichero | Qué hace | De ReTzar |
|---|---|---|
| `ia.h` | interfaz pública (biblioteca, compilador, crear el jugador, orden «AI», EXTERN) | `ai.rs` |
| `ia_int.h` | el jugador por dentro (listas, grupos de campesinos, reservas, lugares) | `ai.rs` Player, `think.rs` Lists |
| `ia_guiao.c` | biblioteca de `DATA\AI\*` y tipos; compilador de `*.ais`/`*.ai` (INCLUDE, grupos `{}`, CRITICAL, WHEN/HAVE/TRIGGER/FORGET/SAY, NOT, %, nombres pendientes) | `script.rs` |
| `ia_regras.c` | condiciones, evaluación de reglas, listas de vigilancia, fase de construir, metas, costes y reservas por partes | `rules.rs` |
| `ia_jogador.c` | gancho del reino, periodo por dificultad, casa, listas, recuentos, cinco fases, ociosos, guardar/cargar, orden «AI», EXTERN | `think.rs`, `ai.rs` |
| `ia_obras.c` | árbol de la civilización (quién produce, qué está), dónde poner un edificio, quién lo construye | `tree.rs`, `build.rs` |
| `ia_economia.c` | recolectores queridos por recurso, minas, campos, vacas, bosque, almacenes, campesino ocioso | `harvest.rs`, `idle.rs` |
| `ia_guerra.c` | lugares y su registro (prioridad, mínimo, fuerza querida), grupos, milicia, torres y defensas, unidades ociosas, exploración | `places.rs`, `records.rs`, `war.rs`, `defence.rs`, `idle.rs` |

**Cómo corre.** `tz_ia_criar` (o `tz_ia_criar_todas`, uno por reino con `TzReinoEstado.ia`) compila el guion del reino
(`random.ais` si no tiene) y registra un `TzGancho` por reino. El gancho corre cada tic: lee los eventos (pérdidas y
amenazas) y, cada periodo (2000/1500/1000 ms en fácil/normal/difícil), mira: rehace sus listas con lo que tiene, vuelve a
evaluar las reglas que leen lo que cambió, rehace los lugares que conoce y hace una de las cinco fases por turno
(construir, recolectar, almacenes, guerra, especial), luego explora y atiende a sus unidades ociosas. Solo ve lo suyo y lo
que `tz_mundo_ve`/`tz_mundo_nevoa` le dejan, y solo actúa con `tz_mundo_comando` (ordenar, construir, producir,
investigar). Es determinista: enteros, su propio `TzRng` (semilla de la partida y reino), orden por índice; su estado va
en la partida guardada (bloque `IA<k>`; al guardar lee antes los eventos pendientes, porque la cola no se guarda).

**Dificultad.** Como el original: periodo, 30 minutos sin atacar casas en fácil (y 15 de pausa tras cada ataque a una
casa), las condiciones `DIFFICULTY` de los guiones, la protección de minas lejanas, las torres de casa (5 o 10), torres
de piedra más allá de fácil, puestos y talleres en difícil, y las unidades ociosas que avanzan más allá de fácil.

**Diferencias con el original** (decididas o pendientes):
- **Niebla:** el gestor del original lo sabe todo; aquí la IA solo cuenta y ataca lo que ve (incluidos `ENEMY`/`ALLY`
  de los guiones y los lugares), por eso **explora** (nuestro): una unidad de combate, o un campesino si aún no hay
  ejército, va al sector sin ver más cercano.
- Las listas y los lugares se rehacen en cada mirada en vez de llevarse al día con ganchos del motor; de los enemigos
  no se sabe adónde van (sin «fuerzas que llegan» hostiles).
- Los grupos van con «mover» al punto del lugar (la simulación los hace pelear con lo que ven) en vez de Approach.
- La busca de un sitio mira hasta 4096 casillas en la misma mirada (el original, 512 y luego 200 cada 100 ms); la obra
  lejana la pone la simulación al aplicar el comando (no hay «sitio virtual»).
- Sin hacer: flota y transporte (`navy.rs`, `transport.rs`, `reach.rs`), hechizos (`spells.rs`), brechas en muros
  (`breach.rs`), invocaciones del mago, yihad y cruzada (`goals.rs`), `CONVERT` (la taberna; no hay comando),
  `RESIGN` (las reglas de la partida no dejan rendirse a la IA) y captura con campesinos.
- La casa no se pone en puentes, muros, puertas ni campos con dueño (en MP1 hay puentes del reino 1 lejos de todo).
- `EXTERN`: un evaluador propio pequeño (números, `$aik`, `@gold..@wood(k)` del propio reino, aritmética, comparaciones,
  `&&`, `||`, `!`); los guiones de misión (fase 7) pueden poner el suyo en `tz_ia_externo`.
- Las frases `SAY` (con «talk on») quedan en el jugador (`tz_ia_fala`) para que la interfaz las enseñe; `SAY "HMAI ..."`
  es la orden «AI» (talk on/off, none, eco, war, full).

**Visor.** `/apps/bin/tzar` crea la IA para los reinos del ordenador al empezar una partida (`TZAR_SEM_IA=1` la quita;
con `TZAR_DEMO=1` sigue el jugador de prueba `demo.c`).

**Pruebas.** `tests/h417_tzar.py ia` (batería, sin datos reales): `tests/tzar_ia_prova.c` (99 comprobaciones del
compilador, casos de `script_tests.rs`) y `tests/tzar_ia_partida.c` sobre la instalación sintética jugable de
`tools/tzar_ia_sintetico.py` (catálogo, mapa 48x48 con dos reinos, minas y bosques, guiones con grupos, INCLUDE, NOT, %,
TIME, RANDOM, CIV, DIFFICULTY): 6000 tics con ASan/UBSan, dos partidas con las mismas sumas, guardar/cargar a mitad
igual, y las dos IA construyen, entrenan, investigan, mandan grupos y pelean. `h417_tzar.py privado_ia` (a mano, nunca
en la batería): los 12 `*.ais` reales compilan y partidas IA contra IA en MP2, MP3 y MP5 con `--castelo` (la
preparación de una partida libre del original pone castillo y campesinos; la simulación aún no).

**Guion de cada reino.** `mapa.c` lee del registro del reino el guion (bytes 89-120, `TzReino.ia_guiao`) y los bits de
IA (byte 121, `ia_modo`; 0 si el byte 122 es 2), como `world.rs`; `tz_ia_criar_todas` los usa (sin bits, IA completa).

**Resultado con los datos reales** (privado, 2026-11-20): los 12 guiones compilan, y los que nombran los 150 reinos con
IA de los 71 mapas reales (sueltos y de campañas) también (random.ais: 1 147 reglas, 838
condiciones, 35 gatillos). MP1-MP6 con castillo, 24 000 tics (20 minutos de juego): deterministas, guardar/cargar igual
en MP2-MP6 (y en Barcelona de «La Reconquista» con los guiones de la campaña, 12 000 tics), economía, casas, investigaciones, ejército y ataques (MP1, 30 minutos: 171 y 177 unidades
creadas, 92 y 119 bajas). MP6 con 8 IA: 1,3 ms por tic en el PC.

## Interfaz, pantalla y sonido (v8.5.5.2, rama `v8.5.5.2-tzar`)

### El «cuelgue» de las campañas

Síntoma (el mantenedor): «el Tzar se bloquea repitiendo el audio»: la imagen se queda quieta y suena lo mismo.

Causa: en una conversación del guion (`.conv`), una frase sin elecciones se pasaba con `tz_guiao_fechar`, que solo cierra
historias y avisos (`guiao.c`: con una conversación activa no hace nada). La ventana seguía abierta para siempre, el mundo no
avanzaba (la partida espera mientras `tz_guiao_modal`), la escena dejaba de animarse y solo seguía la música. Pasa en la primera
misión de las campañas (en QEMU con los datos reales, «El Imperio de Gengis Khan»: tic 4 para siempre). Arreglo: `modal_seguir()`
en `jogo.c` llama a `tz_guiao_responder` si hay frase (dice la siguiente o acaba la conversación) y a `tz_guiao_fechar` si es una
historia o un aviso.

Otras comprobaciones que no dieron cuelgue (QEMU con KVM, datos reales, 10-12 minutos cada una): escaramuza con el jugador de
prueba, con clics y teclas al azar y con 768 MB; el menú; las partidas en el PC con el sintetizador MIDI. El mezclador del kernel
pone en silencio lo ya tocado, así que un programa parado se queda callado: si alguna vez se oye repetir el último trozo con la
imagen quieta, es el sistema entero el que está parado (el anillo de la tarjeta se repite), no el juego.

**Vigilante** (`vigia.c`): un hilo mira el latido del bucle principal; si no avanza en `TZAR_VIGIA` segundos (8; 0 lo apaga), pausa la
tarjeta y escribe `[TZAR] vigilante: el juego no avanza desde hace N ms (en: dibujo > ...; dato T)` con la pila de módulos que marca
el bucle (eventos, menú, carga, mundo con el tic, guion e interfaz, sonido, dibujo, presentar, introducción, previa del mapa); al
volver, `[TZAR] vigilante: el juego vuelve tras N ms`. Además, la introducción ya no se queda en un cuadro que no se puede leer.

Otros arreglos: los volúmenes iban de 0 a 256 a una función de 0 a 100 (siempre al máximo); los cursores de eventos del sonido y de
la interfaz se ponen al de la partida al empezar o cargar (una partida cargada con menos eventos los dejaba sordos).

### Modos de pantalla (`opcoes.c`, `tzar.cfg`)

El juego dibuja en un lienzo (al menos 800x600, lo que necesita la interfaz original) y lo escala él mismo a la ventana o a la
pantalla completa, con bandas negras:

- **Ventana o pantalla completa**: F11 (en SaguiOS lo atiende el sistema y el juego lo nota), Alt+Intro u Opciones > Modo. A pantalla
  completa pide «no molestar» (`no_molestar(1)`, D422) y lo suelta al salir.
- **Resolución**: «Pantalla» (la de la pantalla) o las del original, 640x480, 800x600 y 1024x768 (640x480 se dibuja a 800x600 y se
  reduce, como ReTzar).
- **Escalado**: entero (por defecto; si no cabe x1, ajustado) o ajustado a la pantalla manteniendo la proporción.
- **Filtro**: nítido (vecino más próximo, por defecto) o suave (bilineal).
- **Interfaz**: con la resolución de la pantalla, automática (el mayor factor hasta x3 que deja al menos 800x600, `hud_layout` de
  ReTzar: 2560x1440 se dibuja a 1280x720 y se amplía x2) o x1/x2/x3.
- **Panel** a la izquierda o a la derecha (LAYOUT800R) y **cursor** del juego o del sistema.

Se guarda en `tzar.cfg` (en la carpeta de las partidas, `$HOME/.tzar`), con los volúmenes. Pruebas: `TZAR_FULL=1`, `TZAR_W`/`TZAR_H`
(fijan el lienzo).

### Interfaz

- **Panel derecho** (`ui_hud.c`, `tz_ui_rect`): el arte `SCREEN 800/1024 RIGHT`, los botones `... RIGHT` y las posiciones de
  `hud.rs Layout` (lo que está desde x = 480 sigue al borde derecho).
- **Diplomacia y chat** (`ui_janelas.c`): botones de la barra; la diplomacia cambia la actitud con cada reino (aliado con vista,
  neutral, enemigo: `TZ_C_DIPLOMACIA`) y enseña la suya; el chat (también con Intro) manda «ai talk on», «ai war»... a la IA y
  enseña sus frases `SAY` sobre el mapa.
- **Cursores del juego** (`ui_cursor.c`): `CURSORS/S_DEF` y las formas `cs_*` de `DATA/IMAGES.INI` según la orden (atacar, recoger,
  reparar, capturar, mover...); sin datos, una flecha propia.
- **Fin de partida** (`D_ENDGAME`): repetir, siguiente misión o cargar, salir y ocultar (ver el mapa), con el dibujo `WIN`/`LOSE` y las
  cuentas de cada reino.
- **Campañas**: la lista de misiones (las no alcanzadas en gris) con la descripción de la campaña y de la misión; se puede jugar
  cualquiera de las alcanzadas.
- **Escaramuza**: previsualización del mapa (minimapa del terreno con los objetos de cada reino) y su descripción.
- **Instrucciones** (Menú > Instrucciones o F1): `HELP/HK.TXT`, o un texto propio con las teclas.
- **Créditos animados**: `LOCAL/CREDITS.TXT` sube por la página derecha del libro con los estilos `@N`.

### Sonido

- Las voces que siguen a un objeto se recolocan cuando el objeto anda o la vista se mueve.
- Los sonidos puestos en el mapa (`<mapa>.sounds`, `world.rs SoundSource`): una vez, cada tanto o en bucle donde el jugador ve
  (ningún mapa original trae, pero el editor los pone).
- La lluvia (`audio/ambience.rs`): clips `SOUND/RAIN/<nivel>/RAIN START|1-6|END` y gotas en la vista; el clima sigue a
  `simulation.rs update_weather` (ocho pares de 480-720 s despejado y 60-120 s de lluvia, en los biomas con `rain=1`; nieve en el de
  nieve). Solo se ve y se oye: no cambia la simulación. `TZAR_CHUVA=1` la pone ya, `TZAR_SEM_CLIMA=1` la quita.

### Pruebas

`h417_tzar.py cuelgue` (batería, PC): la campaña sintética abre una historia y una conversación al empezar; cerradas como un jugador
(`TZAR_AUTO_FECHAR`), el mundo sigue (con el código de antes: tic 4 para siempre); el vigilante avisa de una parada provocada
(`TZAR_PRUEBA_PARADA`) con el módulo y apunta la vuelta; sin paradas calla. `saguios` hace lo mismo dentro de SaguiOS.
`campanhas`: la voz que sigue al objeto, los sonidos del mapa y la lluvia (140 comprobaciones).

A mano, con los datos reales en QEMU (KVM, 2 GB, tarjeta HDA, guion privado `/home/aanun/SO/private/tzar/repro_cuelgue.py` que mueve
el ratón, hace clics, lleva la vista a los bordes y pulsa teclas al azar): campaña 1 en ventana, escaramuza y mapa aleatorio a pantalla
completa (1024x768, entero) y escaramuza con 768 MB, unos 10 minutos cada una: sin cuelgues ni avisos del vigilante, 50 cuadros por
segundo (el tope del bucle). Capturas privadas `repro-*.png` y `v8552-menu-*.png` en `/home/aanun/SO/private/tzar/capturas/`.

Visto de paso: `intro.MPG` de la edición FX no la abre el lector de vídeo de la libsagui («no es un vídeo que se lea»): la
introducción se salta (ahora lo dice el registro). Pendiente: las ventanas de la taberna, el comercio, los objetos, los préstamos, el
intercambio y la cruzada de la simulación completa (D430), y usar `TZ_EV_TRABALHO` para el sonido del trabajo.
