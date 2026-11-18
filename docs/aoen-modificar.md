# AoEN compilable dentro de SaguiOS (D384, mejora 142)

AoEN (D382, `userland/juegos/aoen`) es un motor sencillo y educativo: desde la v8.5.4.9 sus fuentes van en la imagen
y se recompila dentro con TCC ([`tcc.md`](tcc.md)). La guía para el usuario, con los ejercicios, es
[`rootfs/ayuda/modificar-aoen.md`](../rootfs/ayuda/modificar-aoen.md) (`/ayuda/modificar-aoen.md`).

## Qué se instala

`/apps/aoen/fuentes` (lo monta `userland/terceros/tcc/tcc.mk`, objetivo `build/aoen-fuentes`):
- los `.c` del juego (`AOEN_SRC` de `aoen.mk`: todos menos `aoen_config.c`) y todos los `.h`;
- `stb_image.h` (de `userland/terceros/stbimage`, dominio público o MIT);
- `compilar.sh` (de `userland/terceros/tcc/aoen/compilar.sh`, con la lista de ficheros puesta por `make`): las mismas
  definiciones que `aoen.mk` (`SAGUI_AOEN`, `AOEN_FONTE_SAGUI`, `STBI_NO_THREAD_LOCALS`, `NDEBUG`) más
  `STBI_NO_SIMD` (TCC no tiene los intrínsecos SSE2 de stb_image) y `-lSDL2`; lo que se le pase va a tcc (`-b`, `-g`);
- `leeme.txt` con dónde está cada cosa.

Si se añade un `.c` a AoEN, entra solo (la lista sale de `AOEN_SRC`). **No hizo falta tocar AoEN**: TCC compila
todos sus fuentes tal como están (las rutas SSE2 de `rapido.c` son de la libsagui, ya compilada con gcc, y se eligen
por CPUID igual que en `/apps/bin/aoen`). El AoEN de TCC no está optimizado: dibuja más despacio que el de gcc.

## Comprobado (h392)

- `compilar.sh` dentro de SaguiOS: 0,03-0,04 s con KVM y 0,4 s con TCG; el `./aoen` arranca con los DRS
  sintéticos (`tools/aoen_drs_sintetico.py`), dibuja el menú (captura `H392-3`) y juega una escaramuza de 4
  jugadores (`H392-4`).
- `compilar.sh -b`: la escaramuza corre con comprobación de límites sin ningún aviso.
- Los cuatro ejercicios de la guía (coste y vida del aldeano, la tecnología «Sandalias», la IA normal más agresiva,
  los botones azules) compilan con TCC (comprobado con el cruzado del PC sobre los fuentes de la v8.5.4.7).

Si cambian las tablas que nombra la guía (`unit_def`, `tech_def`, `niveles` de `ia.c`, `COL_RED` de `interfaz.h`) o la
función `walk_step`, hay que revisar la guía.
