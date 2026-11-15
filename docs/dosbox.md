# DOSBox y Recreativa: juegos de DOS en SaguiOS

> Desde D321 Recreativa es un lanzador **multisistema** (DOS, NES, Master System, Game Gear...): ver [`recreativa.md`](recreativa.md).

*(D294, mejora 119, v8.3.1)*

SaguiOS lleva **DOSBox 0.74-3** (el clásico, GPL v2 o posterior) y un lanzador de juegos,
**Recreativa**. Los dos son aplicaciones no esenciales: viven en la partición `SAGUI-APPS`
(`/apps/bin/dosbox`, `/apps/bin/recreativa`) y salen en **Inicio > Juegos**:

- **Recreativa (juegos y consolas)**: la lista de tus juegos, con cuadrícula o lista, propiedades por juego,
  CD, núcleo, ciclos y sonido.
- **DOSBox** y **DOSBox (pantalla completa)**: el emulador solo, con el indicador `Z:\>` y `C:` ya montado.
- Un **.exe, .com o .bat** se abre con DOSBox desde **Archivos** (doble clic): se monta su carpeta como C:,
  se ejecuta y DOSBox se cierra al terminar el programa.

Los juegos **no vienen con el sistema** (no son libres) y **nunca** van a git ni a las imágenes publicadas.

## Meter tus juegos

1. Copia cada juego **en su propia carpeta** dentro de `/home/dos` (es el C: de DOSBox):
   `/home/dos/DOOM/DOOM.EXE`, `/home/dos/PRINCE/PRINCE.EXE`...
   - Con un pendrive: móntalo (Discos), copia la carpeta con Archivos y listo.
   - Con `input_share/games/<carpeta>` al compilar (docs/input_share.md): acaba en `/jogos/<carpeta>`.
2. Abre **Recreativa**. Las carpetas de `/home/dos` y de `/jogos` que tienen un `.exe`, `.com` o `.bat`
   en su raíz **aparecen solas** (menos las del sistema: `chip8`, `sagui3d`, `jclic`, `opentyrian`,
   `openjazz`, `quake`, `conquer`, `gnuboy`). El nombre sale de la carpeta (`DOOM_II` -> «Doom Ii») y el
   programa que se lanza es el primero que no parece un instalador (`setup`, `install`, `config`...).
3. Para un juego en otro sitio: **Añadir** (o **Insert**): elige la carpeta con el diálogo común, Recreativa
   detecta los programas y propone el nombre; cámbialo en **Propiedades** y acepta.
4. **Jugar** (Intro o doble clic). **Configurar** lanza el programa de setup del juego (para elegir la
   tarjeta de sonido en juegos que lo piden: Sound Blaster 16, puerto 220, IRQ 7, DMA 1, HDMA 5).
   **Quitar** solo lo quita de la lista: los archivos no se tocan. **Carpeta** abre Archivos en ella.

La lista se guarda en `/home/dos/juegos.conf` (texto: una sección `[Nombre]` por juego). Si `/home` no es una
partición de datos (sistema sin disco), vive en la RAM y se pierde al apagar.

### Portadas

Si hay una imagen `portada.png`, `portada.jpg`, `portada.bmp` o `portada.gif` en la carpeta del juego, o
`/home/dos/portadas/<nombre del juego>.png`, la cuadrícula la muestra (a 72x54). Sin ella sale un icono.

## Propiedades de cada juego

| Campo | Qué hace |
| --- | --- |
| Programa | El .exe/.com/.bat que se ejecuta (relativo a la carpeta del juego) |
| Núcleo de la CPU | `auto` (por defecto), `normal` (intérprete) o `dynamic` (recompilador dinámico, ver abajo) |
| Ciclos | `auto`, `max` o un número fijo (1000, 3000, 8000, 12000, 20000, 30000, 50000) |
| Memoria | 4, 8, 16, 32 o 64 MB de memoria extendida |
| Sonido | Sound Blaster 16, Pro, 2.0 o 1.0, AdLib, Gravis UltraSound o ninguno (solo el altavoz del PC) |
| CD (D:) | Una carpeta (se monta como CD-ROM) o una imagen `.iso` / `.cue` |
| Órdenes previas | Una orden de DOS antes del programa (por ejemplo `set blaster=a220 i7 d1`) |
| Configurar | El programa de setup del juego |

Recreativa escribe lo anterior en `/home/.config/recreativa-juego.conf` y arranca:

    dosbox -conf /home/.config/dosbox.conf -conf /home/.config/recreativa-juego.conf -noautoexec \
           -c "mount c <carpeta>" -c "c:" [-c "imgmount d <cd> -t iso"] [-c <previa>] -c <programa> -c exit

## Ajustar la velocidad

- **Ciclos** (cuántas instrucciones de DOS por milisegundo): un juego de 1992 en `auto` va bien; uno que va
  a trompicones, prueba `max`; uno que va demasiado rápido, un número fijo (3000 ~ un 286 a 12 MHz,
  8000 ~ 386, 20000 ~ 486 a 66 MHz, 50000 ~ Pentium).
- **Durante el juego**: `Ctrl+F11` y `Ctrl+F12` bajan y suben los ciclos (el título de la ventana los
  muestra); `Ctrl+F8` / `Ctrl+F7` el salto de imágenes.
- **Núcleo**:
  - `normal`: el intérprete, sirve para todo.
  - `dynamic`: el recompilador dinámico de x86. Va **8-9 veces más rápido** en un bucle de 32 bits (medido en
    QEMU con KVM, ver abajo). Su memoria de código es ejecutable en SaguiOS (i686 sin PAE).
  - `auto` (por defecto): intérprete en modo real y recompilador al pasar a modo protegido (los juegos de DOS4GW,
    Doom, Descent...). Es el que conviene casi siempre.

  Si un juego se cuelga con `dynamic`, ponlo a `normal` en sus Propiedades.

## Teclado, ratón y pantalla

- **Teclado**: las teclas llegan a DOSBox como en un PC (las flechas, el teclado numérico, F1-F12, Ctrl,
  Alt y Mayús; Ctrl+F1 abre el asignador de teclas, que guarda en `/home/.config/dosbox-mapper.txt`).
  Para una distribución distinta de la americana, `keyboardlayout=` en `dosbox.conf` (por ejemplo `sp`).
- **Ratón**: al hacer clic en la ventana, DOSBox lo **captura en modo relativo** (el puntero se oculta y los
  movimientos son los del ratón, sin chocar con el borde). **Ctrl+Alt** lo suelta (o cambiar de ventana).
  `autolock=false` en `[sdl]` lo desactiva.
- **Ventana y pantalla completa**: la ventana se puede redimensionar y la imagen se escala; **F11** (o
  *DOSBox (pantalla completa)* del menú, o `dosbox --pantalla-completa`) la pasa a pantalla completa, con
  bandas negras para mantener el 4:3. El escalado de DOSBox (`scaler=normal2x` por defecto; prueba `normal3x`,
  `hq2x`, `advmame2x`) se cambia en `[render]` de `dosbox.conf`.
- **Sonido**: Sound Blaster 16 (puerto 220, IRQ 7, DMA 1), AdLib (OPL3), altavoz del PC, Disney Sound Source
  y Tandy; Gravis UltraSound si lo eliges. **MIDI no**: el sintetizador del sistema (`audio_midi`) solo toca
  archivos `.mid`, no recibe notas en directo; los juegos con música MIDI usan la AdLib / Sound Blaster
  (casi todos la traen), o el sintetizador FM de DOSBox.

## La configuración: `/home/.config/dosbox.conf`

Se crea sola la primera vez con los valores del sistema (solo lo que difiere de DOSBox: SVGA S3, `output=surface`,
`core=auto`, `cycles=auto`, Sound Blaster 16 y `[autoexec]` con `mount c /home/dos`). Edítalo con el Editor;
las opciones son las de DOSBox 0.74 (`config -writeconf /home/.config/otro.conf` desde su prompt escribe todas
con sus comentarios). `Ctrl+F10` abre el asistente de configuración de DOSBox con el ratón.

Dentro de DOSBox puedes montar otras carpetas y CD:

    mount d /jogos/otro -t cdrom          (carpeta como CD)
    imgmount d /home/dos/disco.iso -t iso (imagen de CD)
    mount a /home/dos/discos -t floppy
    imgmount a /home/dos/disquete.img -t floppy

## Cuánto cuesta (medido)

QEMU con KVM (un Ryzen), BENCH.COM de `tests/dosbox_com.py` (bucle de 32 bits con suma, giro, xor, imul),
`cycles=max`, 3 s de tiempo de DOS:

| Núcleo | Vueltas de 16384 iteraciones | Instrucciones/s (aprox.) |
| --- | --- | --- |
| `normal` (intérprete) | ~4 050 | ~150 millones |
| `dynamic` (recompilador) | ~36 300 | ~1 300 millones |

(Ver `tests/h294_dosbox.py nucleos`, que repite la medida y exige que `dynamic` supere 2 veces al intérprete.)

## Límites conocidos

- Sin red de DOSBox (módem, IPX, SDL_net): no hay sockets de ese tipo en la SDL del sistema.
- Sin MIDI en directo (ver arriba) ni CD de audio por la unidad real: los CD se montan como carpeta o imagen
  `.iso`/`.cue` (las pistas de audio de un `.cue` suenan por el mezclador de DOSBox).
- Sin captura de pantalla de DOSBox (`Ctrl+F5`: pide libpng); usa **Captura** del sistema.
- Joystick (D307): con `joysticktype=auto` (lo que escribe el `dosbox.conf` nuevo desde la v8.3.3) DOSBox emula el
  joystick del PC (puerto 0x201) con el primer mando del sistema: USB, Xbox 360 con cable o el del puerto de juegos
  real ([mandos.md](mandos.md)). Un `dosbox.conf` anterior tiene `joysticktype=none`: hay que cambiar esa línea a
  `auto` (o borrar el fichero para que se escriba otra vez). En Recreativa, cada juego tiene su «Joystick» (auto,
  2axis, 4axis, fcs, ch o none) en Propiedades.
- El renderizador de la SDL 2 por la GPU (D288) **no se usa**: DOSBox 0.74 es SDL 1.2, que dibuja la superficie
  por software y la copia escalada a la ventana (rápido en 640x480; si la ventana es muy grande el escalado
  cuesta). Pasarlo a SDL 2 es trabajo de un port nuevo (DOSBox Staging, que no se ha usado).
