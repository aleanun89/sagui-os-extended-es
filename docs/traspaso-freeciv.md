# Traspaso: port de Freeciv (D374, rama v8.5.3.1-freeciv)

Estado a 2026-11-17. Copia de trabajo: `../miniOS_master_os_freeciv`.

## Fuentes (fuera de git)
- `tools/freeciv_fuentes.sh` baja Freeciv 3.1.6 (SHA-256 fijado) y zlib 1.3.1 a
  `userland/terceros/freeciv/fuentes/` y `userland/terceros/zlib/fuentes/` (ignoradas por git), genera
  packets_gen, specenum_gen, tolua_*_gen y recorta lo que no hace falta. Hay un tarball ya bajado en
  `/home/aanun/SO/fc_dl/fc316.tgz` (`--tarball`). `/home/aanun/SO/fc_dl/orig` es una copia sin tocar para
  sacar el futuro `parches/sagui.patch` (aun no hay parches: todo se resolvio con cabeceras de plataforma).

## Fase en la que voy
1. Capas SDL2 + zlib: HECHAS a medias. `userland/libsdl2/sdl2_image.c` (PNG con alfa propio, resto por libsagui),
   `sdl2_ttf.c` (sobre libfontes), cabeceras SDL_image.h/SDL_ttf.h y `sdl2_capas.mk`. SIN COMPILAR NI PROBAR aun.
   zlib real: sus fuentes se bajan, `freeciv.mk` las compila (FC_ZLIB); falta ponerlas como capa propia reutilizable.
2. Servidor: FUNCIONA. `freeciv-server` enlaza (2,3 MB), arranca en QEMU 512 MB, carga civ2civ3 y acepta conexiones
   en el puerto 5556. Sin ICU (shim `plataforma/inc/unicode/ustring.h`), sin hilos (`fc_tinycthread.h` falso),
   sin curl (`netfile_sagui.c`), libintl propio que lee .mo (`plataforma/libintl.c`, `tools/freeciv_mo.py`).
3. libc: `userland/libc/sockets.c` (socket/bind/listen/accept/connect/send/recv/select/getaddrinfo) sobre
   disponivel()/esperar_rede(); `select()` viejo de posix.c retirado; `getopt.h` con `SAGUI_SIN_STRUCT_OPTION`.
4. Cliente: EN CURSO. Se compila con `make -k -j3 build/userland/bin/freeciv-sdl2 INPUT_SHARE= OPENJAZZ_DATOS=0 QUAKE_DATOS=0`
   (via `SAGUI_TURNOS=6 tools/turno.sh`). Errores actuales (log en /tmp/claude-1000/w/mk5.log):
   - `SDL2/SDL2_rotozoom.h` no existe: crear `userland/libsdl2/SDL2/SDL2_rotozoom.h` que incluya
     `fuentes/dependencies/SDL2_gfx/SDL2_rotozoom.h` (o anadir -I y un wrapper).
   - `SDL_TouchFingerEvent` y `SDL_FINGER*` faltan en libsdl2/SDL.h (gui_main.h): anadir el tipo y las constantes.
   - Faltan en SDL.h: SDL_SetEventFilter, SDL_SetSurfaceAlphaMod, SDL_GetWindowDisplayMode, SDL_GetNumDisplayModes,
     SDL_GetDisplayMode, SDL_GetCurrentVideoDriver (y quiza SDL_Mix_*: se compila sin sonido, ya excluido audio_sdl.c).
   - Despues: errores de enlace (el servidor enlazo con solo 3 simbolos, esperar pocos).
   - Lanzar el servidor desde el cliente: parchear `client/connectdlg_common.c` (client_start_server usa fork/exec)
     con `executar("/apps/bin/freeciv-server", ...)` y matarlo con `matar(pid)`; conectar a 127.0.0.1:5556.
5. Espanol: catalogos `.mo` (core y nations) se generan en `tools/freeciv_datos.sh` a `/apps/freeciv/locale/es/...`.
   El lanzador `plataforma/principal_fc.c` pone LANG con el idioma del sistema (D313). Falta probarlo en el cliente.
6. Datos: `tools/freeciv_datos.sh` copia 67 MB a `/apps/freeciv` (via `APPS_RECURSOS`; el Makefile ahora usa `cp -r`).
   Falta recortar (sin SVG, flags solo -large, tema gui-sdl2 28 MB), Inicio > Juegos con icono, asociacion de
   .sav, y el formato de guardado (3.1: `.sav.zst`/bz2 si se compilan; aqui solo gzip con zlib: ajustar `compress`).
7. Prueba `tests/h374_freeciv.py`, `docs/freeciv.md`, D374 en docs/decisoes.md, fila en docs/terceros.md,
   auditar_licencias y h81: SIN HACER. `compilar_versiones.md`: pendiente de trabajador-haiku (descarga nueva).
8. Rendimiento en 256/512 MB: solo se probo el servidor (512 MB, arranca y carga reglas).

## Siguiente paso exacto
Crear el wrapper de rotozoom y los tipos/funciones SDL que faltan, volver a compilar el cliente hasta que enlace,
y arrancarlo en QEMU con `tests/qemu.py` (ver `/tmp/claude-1000/w/p1.py` como ejemplo de lanzar un comando).

## Notas
- Servidor de prueba: `echo -e "set aifill 4\nstart" > /tmp/t.serv; /apps/bin/freeciv-server -A none -r /tmp/t.serv -s /tmp`.
- No se hizo push (lo hace el coordinador).


## Actualizacion (misma rama, segunda sesion)
- HECHO: el cliente `freeciv-sdl2` compila, enlaza y arranca desde Inicio > Juegos > Freeciv (icono nuevo en
  kernel/gui/arte.c y entrada en kernel/gui/barra.c). Sale EN ESPANOL (principal_fc.c pone LANG=es si el sistema no lo da).
- La SDL 2 del sistema gano: SDL_TouchFingerEvent/SDL_FINGER*, SDL_SetEventFilter (se guarda, no se aplica),
  modos de pantalla, SDL_Set/GetSurfaceAlphaMod y SDL_SetSurfaceBlendMode reales, y blit 32->32 con mezcla alfa
  (`mezclar_px` en sdl2_sagui.c). Esto toca la SDL compartida: conviene pasar h246 y las pruebas de ports SDL2.
- "Empezar una nueva partida" lanza `/apps/bin/freeciv-server` con executar() (parche `parches/sagui.patch`, aplicado por
  `tools/freeciv_fuentes.sh`), se conecta por TCP a 127.0.0.1:5557 y llega a la pagina de inicio de partida.
- libc: fchmod, getaddrinfo("localhost"), sys/wait.h usado por el cliente.
- Capturas de prueba en /tmp/claude-1000/w/caps (no en git). Script de prueba manual: /tmp/claude-1000/w/p3.py
  (arranca por el menu, hace clic en Empezar). Todavia sin tests/h374_freeciv.py.
- Pendiente: pulsar Comenzar y ver el mapa (en curso), turno, guardar/cargar, rendimiento 256/512 MB, recorte de datos,
  asociacion de .sav, docs/freeciv.md, D374, terceros.md, auditar_licencias/h81, h374.
- HECHO tras lo anterior: "Comenzar" genera el mapa y se ve la partida (mapa hexagonal amplio2/hexemplio, unidades,
  panel, minimapa) con 6 jugadores IA en 512 MB; el servidor guardo `/home/.freeciv/saves/*.sav.gz` (zlib real funciona).
  Captura: /tmp/claude-1000/w/caps/fc-cli-6.png. Textos del servidor en espanol; algunos de la barra de estado aun en ingles
  (cadenas del cliente sin traducir en es.po o domino sin cargar: revisar).
- Siguiente paso exacto: escribir tests/h374_freeciv.py a partir de /tmp/claude-1000/w/p3.py (clic en Empezar, Comenzar,
  fin de turno con la tecla Intro, guardar/cargar), probar con 256 MB, recortar datos y escribir docs/freeciv.md y D374.


## Estado final de esta entrega (SDL2_mixer incluida)
- HECHO y probado (h374, h81 y auditar_licencias en verde; h246, h288, h321, h70 sin regresion; h322 solo fallaba por la fila de
  terceros, ya arreglada): cliente y servidor, español, ventana grande, datos 30 MB, `.sav`/`.sav.gz` en Archivos, turnos,
  guardar y cargar, y SDL2_mixer propia (`userland/libsdl2/sdl2_mixer.c`, `SDL_mixer.h`, `sdl2_rwops.c`, `sdl2_vorbis.c` con
  stb_vorbis en `userland/terceros/stbvorbis/`): WAV, Ogg Vorbis (efectos y musica) y la musica del menu suena en QEMU.
- PENDIENTE: MP3/FLAC/MIDI como musica estan escritos (por `audio_arquivo_*` de la libsagui) pero SIN probar; los efectos solo
  WAV y Ogg (MP3 como efecto no); `Mix_FadeInMusic` no hace fundido de entrada; mezcla en el hilo principal (si el bucle se para,
  el sonido se corta); rendimiento con 256 MB sin medir (con 512 MB va bien); sin prueba propia de los demas ports con
  SDL2_mixer (ninguno la usa aun); falta ejecutar la bateria completa.
