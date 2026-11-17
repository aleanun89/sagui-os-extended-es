# Freeciv en SaguiOS (D374)

Freeciv 3.1.6 (juego de estrategia por turnos libre, GPL-2.0 o posterior, <https://www.freeciv.org>) con su
cliente SDL2 y el servidor local, en español. Es la v8.5.3.1.

## Cómo jugar

1. **Inicio > Juegos > Freeciv** (ventana grande, del tamaño del escritorio; se puede redimensionar).
2. **Empezar una nueva partida**: el cliente lanza el servidor (`/apps/bin/freeciv-server`, otro proceso) y se
   conecta por TCP a 127.0.0.1. Elige nación, jugadores IA y reglas en las páginas del cliente y pulsa **Comenzar**.
3. Intro termina el turno. El servidor guarda la partida sola cada turno en `/home/.freeciv/saves/*.sav.gz`.
4. En **Archivos**, un doble clic en un `.sav` o `.sav.gz` abre Freeciv y carga esa partida
   (`freeciv-sdl2 --file RUTA`).
5. **Cargar partida** y **Unirse a un servidor LAN** (por IP, otro equipo con `freeciv-server`) también funcionan;
   el metaservidor de Internet no (sin curl).

## Qué hay dentro

| Pieza | Dónde |
|---|---|
| Servidor y cliente | `/apps/bin/freeciv-server`, `/apps/bin/freeciv-sdl2` |
| Datos (reglas civ2civ3, classic, civ1, civ2, multiplayer, sandbox...; tilesets amplio2 (por defecto), amplio, hexemplio, trident; tema *human*; banderas) | `/apps/freeciv/data` (unos 27 MB) |
| Catálogos de idioma (es) | `/apps/freeciv/locale/es/LC_MESSAGES/freeciv-core.mo` y `freeciv-nations.mo` |
| Partidas y opciones | `/home/.freeciv/` |

El idioma es el del sistema (D313; `es` si no hay uno) y se cambia con `LANG=en freeciv-sdl2` (solo el español
trae catálogo; los demás salen en inglés). Los textos del cliente SDL2 que el upstream no traduce están en
`userland/terceros/freeciv/traducciones/es-sagui.po`.

## Compilar

`tools/freeciv_fuentes.sh` baja Freeciv 3.1.6 y zlib 1.3.1 (SHA-256 fijado), genera lo que el upstream genera con
herramientas del anfitrión (`packets_gen`, `specenum_gen`, `tolua_*_gen`) y aplica `parches/sagui.patch`. Después,
`make` (la regla está en `userland/terceros/freeciv/freeciv.mk`; `FREECIV=0` la deja fuera). Sin las fuentes, el
sistema se construye igual sin el juego. Las fuentes no van en git y la variante sin GPL (`SEM_GPL=1`) no lo incluye.

## Qué se hizo en el sistema

Capas reutilizables por otros ports: `libc/sockets.c` (sockets BSD, `select`, `getaddrinfo`), `libsdl2/sdl2_image.c`
(PNG con alfa, y BMP/JPEG/GIF por la libsagui), `libsdl2/sdl2_ttf.c` (sobre libfontes), `libsdl2/sdl2_mixer.c`
(SDL2_mixer: canales de efectos y música mezclados por software sobre el audio del sistema; WAV propio, **Ogg Vorbis** con
`stb_vorbis` (dominio público o MIT, `userland/terceros/stbvorbis/`), y MP3/FLAC/MIDI como música por los decodificadores de la
libsagui) y `SDL_rwops` (`SDL_RWFromFile`, `SDL_RWFromMem`), blit con alfa y
`SDL_SetSurfaceAlphaMod` en la SDL 2, zlib 1.3.1 real. Detalles en D374 (`decisoes.md`).

## Sonido y música

Con tarjeta de sonido, Freeciv abre el audio (`[MIXER] audio abierto` en el registro), toca la fanfarria del menú y los
efectos de las unidades y la ciudad (`stdsounds`, `stdmusic`, Ogg Vorbis). Sin tarjeta, el juego va mudo. En Opciones
del cliente se elige el conjunto de sonidos y la música, o se apaga.

## Requisitos y rendimiento

- **512 MB de RAM**: partida de 6 jugadores (1 humano y 5 IA) con mapa pequeño; arranca el cliente, el servidor
  genera el mapa y se juega con fluidez razonable en QEMU.
- **256 MB**: ver la medición de abajo (rellena la prueba `H374_MEM=256M`).
- Más RAM da más margen en mapas grandes y partidas avanzadas (el servidor y el cliente son procesos aparte,
  cada uno con su propia memoria).

## Qué falta

- Sin metaservidor de música en línea; el sonido es el de `stdsounds`/`stdmusic` (Ogg Vorbis).
- Sin metaservidor ni descarga de modpacks (sin curl).
- Las formas de plural que el cliente SDL2 no traduce en el upstream siguen en inglés.
- El alfa global de superficie (`SDL_SetSurfaceAlphaMod`) se aplica en los blits, pero no hay RLE ni aceleración.
- Otros idiomas distintos del español (el catálogo se arma con `tools/freeciv_mo.py` si se añaden `.po`).

## Prueba

`tests/h374_freeciv.py` (partes `catalogos`, `licencia`, `os`): menú, partida nueva contra la IA, mapa, textos en
español, terminar un turno, guardar y cargar; capturas `docs/capturas/H374-*.png`. Sin las fuentes se salta.
