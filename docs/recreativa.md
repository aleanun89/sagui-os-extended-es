# Recreativa: lanzador de juegos multisistema

*(D321, v8.4.3; amplía D294)*

Recreativa (`/apps/bin/recreativa`, en **Inicio > Juegos > Recreativa (juegos y consolas)**) lista y lanza juegos
de **varios sistemas**: DOS (con DOSBox), NES (LaiNES), Master System y Game Gear (Gearsystem), y los que se
vayan registrando (Game Boy, GBA...). Cada sistema y cada emulador se **registran con ficheros de texto**, sin
tocar el código: para añadir un sistema basta un `.conf` y el binario del emulador.

**Las ROM nunca van a git ni a las imágenes publicadas** (`docs/input_share.md`). Las pone quien usa el sistema.

## Cómo se usa

- A la izquierda, la **lista de sistemas**: «Todos», «DOS», «NES», «Master System», «Game Gear»... (solo los que tienen
  algún emulador instalado). Un clic filtra; **F6** pasa al siguiente.
- Los juegos aparecen **solos por extensión** en las carpetas de cada sistema (hasta dos niveles de subcarpetas):

  | Sistema | Extensiones | Carpetas |
  |---|---|---|
  | DOS | carpetas con `.exe`, `.com` o `.bat` | `/home/dos`, `/jogos` (D294) |
  | NES | `.nes` | `/home/juegos/nes`, `/jogos/nes` |
  | Master System | `.sms`, `.sg` | `/home/juegos/sms`, `/jogos/sms` |
  | Game Gear | `.gg` | `/home/juegos/gg`, `/jogos/gg` |

  `/jogos/<sistema>` es lo que se pone en `input_share/games/<sistema>/` al compilar (`docs/input_share.md`).
- **Añadir** (o Insert): en «Todos» o «DOS» elige una carpeta de un juego de DOS con el diálogo común (D272); si la
  carpeta no tiene programas de DOS pero sí ROM de consolas, se añade como **carpeta de ROM**. Con un sistema de
  consolas elegido, **Añadir** pide una ROM (con filtro de las extensiones del sistema). En el menú **Juego**:
  «Añadir ROM...» (el sistema se deduce de la extensión) y «Añadir carpeta de ROM...».
- **Jugar** (Intro o doble clic), **Jugar a pantalla completa**, **Propiedades** (Alt+Intro): nombre, **emulador**
  (de los instalados para ese sistema) y pantalla completa; los de DOS tienen las propiedades de DOSBox de siempre.
  **Quitar** solo quita de la lista (los archivos no se tocan). **Carpeta** abre Archivos.
- **Portada** opcional: una imagen (`png`, `jpg`, `bmp`, `gif`) **con el mismo nombre que la ROM**, en su carpeta
  (`Juego.nes` -> `Juego.png`), o `/home/juegos/portadas/<nombre del juego>.png`. Los de DOS: `portada.png` en la
  carpeta o `/home/dos/portadas/<nombre>.png`.
- **Archivos > Abrir** (doble clic) y **Abrir con...** abren `.nes`, `.sms`, `.sg` y `.gg` con su emulador, y la
  lista de «Abrir con» incluye los emuladores instalados.

Lo que se guarda:

| Fichero | Contenido |
|---|---|
| `/home/dos/juegos.conf` | los juegos de DOS (como en D294) |
| `/home/.config/recreativa/roms.conf` | ROM de consolas añadidas, cambiadas (nombre, emulador, pantalla completa) u ocultas, y las carpetas de ROM añadidas (`[@/ruta]` con `sistema=`) |
| `/home/.config/<emulador>/` | SRAM de los cartuchos con batería (`.sav`) y estados de guardado (los gestiona cada emulador) |

## Dónde están los registros

| Ruta | Qué define |
|---|---|
| `/etc/recreativa/sistemas/<sistema>.conf` | un sistema: nombre, extensiones, carpetas, icono y sus emuladores |
| `/etc/recreativa/emuladores/<emulador>.conf` | un emulador que se **suma** a sistemas ya definidos en otro fichero (alternativas) |
| `/home/.config/recreativa/sistemas/*.conf` y `.../emuladores/*.conf` | lo mismo, del usuario (el de un id ya existente lo reemplaza) |

En el repositorio: `rootfs/etc/recreativa/...`. Texto Latin-1; líneas `clave=valor`; `#` abre un comentario
(también tras el valor, separado por un espacio); claves en minúsculas; los espacios de los extremos se ignoran.

## Formato de `sistemas/<sistema>.conf`

El **id del sistema** es el nombre del fichero sin `.conf` (`nes`, `sms`, `gg`, `gb`, `gbc`, `gba`, `dos`).

```
# SPDX-License-Identifier: GPL-2.0-or-later
nombre=NES                     # lo que sale en la lista de la izquierda (máximo ~14 letras)
orden=20                       # posición en la lista (menor, antes); «Todos» siempre va la primera
extensiones=nes                # extensiones de ROM, sin punto, separadas por comas; sin distinguir mayúsculas
carpetas=/home/juegos/nes,/jogos/nes   # (hasta 4) donde se buscan ROM solas; las que no existen se saltan
icono=jogo                     # nombre del icono (userland/inc/icones.h); si no existe, el genérico de juego
frecuencia=60                  # solo informativa (Hz)

[emulador laines]              # uno o más bloques; «laines» es el id del emulador
nombre=LaiNES
binario=/apps/bin/laines       # si NO existe, este emulador se ignora; si no queda ninguno, el sistema no se muestra
args={rom}                     # argumentos (ver abajo)
args_completa=--pantalla-completa {rom}   # opcional: con pantalla completa (si falta se usa args)
predeterminado=si              # opcional: el que se usa por defecto (si ninguno: el de menor «prioridad»)
prioridad=50                   # opcional (por defecto 50): menor = antes
tipo=dosbox                    # opcional: caso especial de DOSBox (ver más abajo)
```

- Un sistema puede tener **varios emuladores** (varios bloques). Recreativa usa el predeterminado y en
  **Propiedades** se elige otro entre los instalados; la elección se guarda por juego en `roms.conf`.
- **Un emulador cuyo `binario` no existe no se muestra** ni se puede elegir: se pueden registrar siempre
  emuladores opcionales (GNUBoy solo existe en la imagen privada) sin condicionar nada más. Un sistema sin
  ningún emulador instalado desaparece de la lista.
- **Argumentos:** la plantilla se parte por espacios y **cada palabra es un argumento** (una ruta con espacios no
  se parte). Se sustituyen `{rom}` (ruta completa), `{nombre}` (el del juego), `{carpeta}` (la de la ROM) y
  `{estados}` (`/home/.config/<id del emulador>`). `argv[0]` es el nombre del binario.
- **DOSBox** (`tipo=dosbox`, en `dos.conf`): los juegos de DOS no son ROM sueltas sino carpetas con un `.exe`, `.com` o
  `.bat`; Recreativa los busca en `/home/dos` y `/jogos`, los guarda en `/home/dos/juegos.conf` y arranca DOSBox con
  sus propiedades (núcleo, ciclos, sonido, joystick, CD...). `args` y `extensiones` no se usan.

## Formato de `emuladores/<emulador>.conf`

Un emulador adicional para sistemas definidos en otro sitio (así quien añade el segundo emulador no edita el `.conf`
del sistema). No lleva bloques: son campos planos con la lista de sistemas.

```
# emuladores/gnuboy.conf
id=gnuboy
nombre=GNUBoy
binario=/apps/bin/gnuboy
sistemas=gb,gbc               # ids de sistema donde se ofrece (si el sistema no existe, se ignora)
args={rom}
prioridad=60                  # detrás del predeterminado
solo_privado=si               # informativo: GPL-2.0-only (D235), solo en la imagen privada
```

Si el mismo `sistemas/<id>.conf` ya trae un bloque `[emulador]` con el mismo id, gana el del sistema. Los sistemas
se leen primero (`/etc` y luego `/home`) y los emuladores sueltos después.

## Cómo añadir un sistema o un emulador (ejemplo para mGBA)

1. El binario del emulador va a `/apps/bin/` (`APPS_BINS` en su `.mk`; se lanza como
   `<binario> [--pantalla-completa] ROM`).
2. Un `rootfs/etc/recreativa/sistemas/gb.conf` (y `gbc.conf`, o uno solo con `extensiones=gb,gbc`):

   ```
   nombre=Game Boy
   orden=40
   extensiones=gb
   carpetas=/home/juegos/gb,/jogos/gb
   [emulador mgba]
   nombre=mGBA
   binario=/apps/bin/mgba
   args={rom}
   args_completa=--pantalla-completa {rom}
   predeterminado=si
   ```
3. GNUBoy (`emuladores/gnuboy.conf`) se suma solo a `gb` y `gbc` si `/apps/bin/gnuboy` existe (imagen privada).
4. Para abrir la extensión desde **Archivos** hay que añadirla en `userland/programas/arquivos.c`
   (`abrir_item`, `programa_padrao` y la lista de «Abrir con»), como `.nes`, `.sms`, `.sg` y `.gg` (D321).

## Los emuladores de D321

| Emulador | Sistemas | Licencia | Binario |
|---|---|---|---|
| **LaiNES** | NES (mappers 0-5, 7, 9-11, 24, 26, 34, 66) | BSD-2-Clause (APU de blargg: LGPL 2.1+) | `/apps/bin/laines` |
| **Gearsystem 3.9.25** | Master System, Game Gear, SG-1000 | GPL-3.0 | `/apps/bin/gearsystem` |

SMS Plus (Charles MacDonald) **no se usa**: su Z80 y su YM2413 son del proyecto MAME bajo su licencia no comercial de
entonces, incompatible con la GPL v3. Gearsystem no lleva el YM2413 (el de Genesis Plus GX, no comercial): la música FM de
la Master System japonesa calla, el PSG suena. No se leen ROM dentro de un `.zip`.

Comunes a los dos (`userland/terceros/emucomun/marco.hpp`, sobre la SDL 2 del sistema, D288):

- **Vídeo:** una ventana (por la GPU si la hay) escalada con la proporción de la imagen; **F11** o Alt+Intro, o
  `--pantalla-completa`, para la pantalla completa.
- **Sonido** por la SDL (cola con tope de 150 ms).
- **Teclado:** flechas, **X** = A / botón 1, **Z** = B / botón 2, Intro = Start, Espacio o Mayús der. = Select,
  **F5** guardar estado, **F7** cargar, **F9** reiniciar, **Tab** avance rápido, **Esc** salir.
- **Mando (D307):** `SDL_GameController`: cruceta y palanca izquierda; B (derecha) o Y = A; A (abajo) o X = B;
  Start; Back = Select; el segundo mando es el jugador 2.
- **Velocidad:** 60 Hz NTSC y 50 Hz PAL (la de la ROM en Gearsystem); si la CPU no llega, se **saltan los dibujos**
  (no la emulación) hasta recuperar. Cada 5 s anotan en el registro los cuadros por segundo, los saltados y el
  porcentaje de CPU (`[LAINES] 60.0 cuadros/s, CPU 12%, 0 saltados`).
- **SRAM** de los cartuchos con batería y **estados de guardado** en `/home/.config/laines/` y
  `/home/.config/gearsystem/`.

Medidas (D321, ROM de prueba): con KVM LaiNES 7 % de CPU y Gearsystem 4 % a 60 cuadros/s; sin KVM (TCG, referencia de un PC antiguo)
LaiNES 99 % (se sostiene la velocidad saltando dibujos) y Gearsystem 34 %. LaiNES es de ciclo exacto y el más pesado.

Prueba: `tests/h321_emuladores.py` (ROM de prueba **generadas**, ninguna comercial; `tools/nes_rom_teste.py` y
`tools/sms_rom_teste.py`).
