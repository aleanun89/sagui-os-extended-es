# CHIP-8: emulador y ensamblador

**CHIP-8** es una máquina virtual de 1977 pensada para escribir juegos
pequeños: 4 KB de memoria, 16 registros de 8 bits, una pantalla de 64x32
puntos y un teclado hexadecimal de 16 teclas. Después vinieron el
**SUPER-CHIP 1.1** (128x64, desplazamiento de pantalla, sprites de 16x16) y
el **XO-CHIP** (64 KB, dos planos de bits con 4 colores y sonido con patrón).

SaguiOS trae un emulador de las tres (`chip8`), un ensamblador y
desensamblador (`chip8asm`) y cuatro juegos propios en `/jogos/chip8`, con
su código fuente en `/jogos/chip8/fuentes`.

## Cómo abrirlo

- Icono **CHIP-8** del escritorio, o `chip8` en el terminal: muestra la
  lista de ROM de `/jogos/chip8` (doble clic o Intro abre).
- `chip8 juego.ch8` abre una ROM directamente.
- En **Archivos**, doble clic en un archivo `.ch8`, `.sc8` o `.xo8`.

## Teclas

El teclado hexadecimal del CHIP-8 está en la parte izquierda del teclado del
PC, en la misma posición:

| CHIP-8      | PC          |
|-------------|-------------|
| 1 2 3 C     | 1 2 3 4     |
| 4 5 6 D     | Q W E R     |
| 7 8 9 E     | A S D F     |
| A 0 B F     | Z X C V     |

Las teclas se leen por su **posición** (código de barrido), así que valen
igual en el teclado español y en el ABNT2. Las **flechas** equivalen a 5, 7,
8 y 9 (arriba, izquierda, abajo, derecha), como W A S D. Con `-k numerico`
también vale el teclado numérico para 0-9. Las teclas mantenidas se
detectan de verdad: `EX9E` ve la tecla mientras está pulsada.

| Tecla        | Qué hace                                         |
|--------------|--------------------------------------------------|
| P            | Pausa / continuar                                |
| N            | En pausa: ejecuta una instrucción (paso a paso)  |
| F5           | Reinicia la ROM                                  |
| F10          | Muestra u oculta el depurador                    |
| F2           | Paleta de colores siguiente                      |
| + y -        | Más o menos instrucciones por cuadro             |
| F11          | Pantalla completa                                |
| Ctrl+O       | Abrir otra ROM                                   |
| Clic derecho | Menú: abrir, reiniciar, pausa, modo, paleta, velocidad |

La ventana se puede redimensionar y maximizar; la pantalla del CHIP-8 se
amplía con un factor entero y queda centrada. La barra de abajo muestra el
modo, las instrucciones por cuadro, las instrucciones por segundo (IPS) y la
ROM.

## Modos y quirks

El modo se elige por la extensión (`.ch8` CHIP-8, `.sc8` SUPER-CHIP, `.xo8`
XO-CHIP) o con `-m chip8|schip|xochip` (y en el menú del botón derecho).
Cada modo trae los comportamientos de su máquina original, los llamados
*quirks*, que se pueden cambiar uno a uno con `-q nombre=valor`:

| Quirk       | Valores                | chip8      | schip    | xochip     |
|-------------|------------------------|------------|----------|------------|
| desplazar   | vy / vx (8XY6, 8XYE)   | vy         | vx       | vy         |
| memoria     | incrementa / fija (FX55, FX65) | incrementa | fija | incrementa |
| salto       | v0 / vx (BNNN o BXNN)  | v0         | vx       | v0         |
| borde       | recorta / envuelve     | recorta    | recorta  | envuelve   |
| logicas     | reinicia / conserva (VF en 8XY1-3) | reinicia | conserva | conserva |
| vblank      | espera / no (DXYN acaba el cuadro) | espera | no | no       |

Ejemplo: `chip8 -m schip -q memoria=incrementa juego.sc8`.

**Velocidad.** El emulador ejecuta 60 cuadros por segundo, marcados por el
reloj del sistema; en cada cuadro corre unas instrucciones (`-v N`; por
defecto 11 en CHIP-8, 30 en SUPER-CHIP y 1000 en XO-CHIP) y los dos
temporizadores bajan una unidad. `+` y `-` cambian la velocidad en marcha.

**Pantalla.** En SUPER-CHIP y XO-CHIP, `00FF` pasa a 128x64 y `00FE` vuelve
a 64x32, borrando la pantalla; el desplazamiento (`00CN`, `00DN`, `00FB`,
`00FC`) mueve píxeles del modo actual y `DXY0` dibuja 16x16 también en
64x32, como en el XO-CHIP (el SUPER-CHIP original desplazaba medio píxel y
dibujaba 8x16 en ese modo).

**Sonido.** El altavoz del PC solo hace una onda cuadrada: mientras el
temporizador de sonido es mayor que cero suena un pitido de 440 Hz. En
XO-CHIP no se puede reproducir el patrón de 128 bits (`F002`): se toca su
frecuencia fundamental, calculada con la altura (`FX3A`, 4000 x 2^((tono-64)/48)
muestras por segundo) y las veces que el patrón sube en sus 128 bits. Las
melodías se reconocen, pero el timbre no.

**Paletas.** SaguiOS (verde y amarillo), Clásica, Ámbar, Fósforo, Papel y
Atardecer. `-p número` o `-p nombre` elige la inicial. En XO-CHIP los
cuatro colores son: apagado, plano 1, plano 2 y los dos planos.

## ROM de internet: chip8-roms

El **Chip8 Community Archive** (https://github.com/JohnEarnest/chip8Archive,
de John Earnest y muchos autores) tiene más de cien programas de CHIP-8,
SUPER-CHIP y XO-CHIP, todos bajo la licencia **CC0** (dominio público).
SaguiOS no los incluye: se descargan al usarlos, con red.

    chip8-roms lista [--plataforma chip8|schip|xochip] [--buscar texto]
    chip8-roms info octojam1title
    chip8-roms bajar clave...        chip8-roms bajar --todas [--plataforma schip]
    chip8-roms jugar octojam1title

- Se guardan en `/home/chip8/roms` si `/home` es persistente y, si no, en
  `/tmp/chip8/roms` (con un aviso: se pierden al apagar). Junto a cada ROM
  va su `<clave>.json` con título, autores, opciones y la atribución
  «CC0, Chip8 Community Archive». La extensión sale de la plataforma
  (`.ch8`, `.sc8`, `.xo8`).
- `jugar` baja la ROM si falta y abre `chip8` con las opciones del índice
  traducidas: modo (`platform`), velocidad (`tickrate` = instrucciones por
  cuadro) y los quirks (`shiftQuirks`, `loadStoreQuirks`, `jumpQuirks`,
  `clipQuirks`, `logicQuirks`, `vBlankQuirks`). El emulador no tiene colores
  por programa ni `vfOrderQuirks`, rotación o tamaño máximo: se avisa.
- Las bajadas salen en el selector de `chip8` (sin argumentos), marcadas con
  `+` y con su título.
- `--url BASE` usa otro servidor (un espejo; debe tener `programs.json` y
  `roms/<clave>.ch8`) y `--carpeta DIR` otra carpeta.
- **`chip8-roms-gui`** (Inicio, Juegos, «ROM de CHIP-8 (bajar)») es lo mismo
  con ventana: lista con marca de descargada, filtro por plataforma, búsqueda,
  panel con descripción, evento, fecha, autores y opciones, y los botones
  Descargar, Descargar todas, Jugar y Actualizar índice.

## Depurador

F10 abre un panel con PC, I, los registros V0-VF, los temporizadores, la
pila y el código desensamblado alrededor del PC (la línea actual marcada
con `>`). Con la ROM en pausa (P), N ejecuta una instrucción cada vez.

## Sin ventana: chip8 -t

    chip8 -t 300 juego.ch8

ejecuta 300 cuadros sin ventana y muestra los registros, la pila, una suma
de la pantalla y la pantalla en texto (`#` encendido; en XO-CHIP `2` es solo
el plano 2 y `3` los dos). `-e Q:T` pulsa teclas en el cuadro Q (T en
hexadecimal, un bit por tecla). Así se comprueba el emulador con las ROM de
prueba de `/testes/chip8`.

## Juegos incluidos

Todos son originales, hechos para SaguiOS con `chip8asm`:

- **RAMAS** (`ramas.ch8`, CHIP-8): el sagui recoge las bananas que caen de
  las ramas y esquiva los palos secos. A y D para andar.
- **SUMAS** (`sumas.ch8`, CHIP-8): un tablero de 4x4 números; cada tecla
  marca la casilla en la misma posición. Hay que llegar justo a la META.
- **CANOA** (`canoa.sc8`, SUPER-CHIP): bajar el río esquivando rocas y
  troncos, con la pantalla desplazándose. A y D para remar.
- **AURORA** (`aurora.xo8`, XO-CHIP): demostración a 4 colores con aurora,
  estrellas y una melodía. W/S tono, A/D viento, E música, V salir.

## El ensamblador chip8asm

    chip8asm juego.asm -o juego.ch8
    chip8asm -d juego.ch8 [-o juego.asm]

Sin `-o`, `juego.asm` genera `juego.ch8`. Los errores dicen archivo y línea:

    juego.asm:12: símbolo no definido 'bucle'

### Sintaxis

Una instrucción por línea; los comentarios empiezan con `;` o `#`. Las
mayúsculas no importan en los mnemónicos ni en los registros.

    ; comentario
    VELOC = 3              ; constante (o: VELOC equ 3)
    inicio:                ; etiqueta
        ld v0, VELOC       # otro comentario
        jp inicio

- Números: `255`, `0xFF`, `0b1010`, `%1010` o `%X.X.` (en los binarios con
  `%`, `.` es 0 y `X` o `#` es 1: muy cómodo para dibujar sprites), `'A'`.
- Expresiones: `+ - * / & | ^ << >> ~` y paréntesis; `$` es la dirección de
  la línea. Ejemplo: `ld v1, fin - textos`.
- Directivas: `db` (bytes y textos entre comillas), `dw` (palabras de 16
  bits), `ds N[, valor]` (N bytes), `org dirección`, `align N`,
  `include "otro.asm"`.
- El programa empieza en `0x200`. Las constantes que usan etiquetas
  posteriores solo se pueden usar después de definidas; para eso, escribe
  la expresión directamente.

### Instrucciones

| Instrucción             | Opcode | Qué hace |
|-------------------------|--------|----------|
| cls / ret               | 00E0 / 00EE | Borra la pantalla / vuelve de una subrutina |
| jp dir / jp v0, dir     | 1NNN / BNNN | Salta (a dir + V0, o + VX con salto=vx) |
| call dir                | 2NNN   | Llama a una subrutina |
| se vx, n / sne vx, n    | 3XNN / 4XNN | Salta la siguiente si igual / distinto |
| se vx, vy / sne vx, vy  | 5XY0 / 9XY0 | Igual, entre registros |
| ld vx, n / add vx, n    | 6XNN / 7XNN | Carga / suma (sin tocar VF) |
| ld vx, vy               | 8XY0   | Copia |
| or, and, xor vx, vy     | 8XY1-3 | Lógicas |
| add vx, vy / sub / subn | 8XY4 / 8XY5 / 8XY7 | Con acarreo en VF |
| shr vx [, vy] / shl     | 8XY6 / 8XYE | Desplaza (sin vy, vy = vx: igual con los dos quirks) |
| ld i, dir               | ANNN   | I = dir (12 bits) |
| rnd vx, n               | CXNN   | Aleatorio AND n |
| drw vx, vy, n           | DXYN   | Sprite de n filas (n = 0: 16x16) |
| skp vx / sknp vx        | EX9E / EXA1 | Salta si la tecla VX está / no está pulsada |
| ld vx, dt / ld vx, k    | FX07 / FX0A | Lee el retardo / espera una tecla |
| ld dt, vx / ld st, vx   | FX15 / FX18 | Temporizadores |
| add i, vx               | FX1E   | I += VX |
| ld f, vx / ld hf, vx    | FX29 / FX30 | I = dígito pequeño / grande |
| ld b, vx                | FX33   | Decimal de VX en I, I+1, I+2 |
| ld [i], vx / ld vx, [i] | FX55 / FX65 | Guarda / carga V0..VX |
| scd n / scr / scl       | 00CN / 00FB / 00FC | SCHIP: desplaza abajo / derecha / izquierda |
| low / high / exit       | 00FE / 00FF / 00FD | SCHIP: 64x32 / 128x64 / terminar |
| ld r, vx / ld vx, r     | FX75 / FX85 | SCHIP: guarda / lee V0..VX en los "flags" |
| scu n                   | 00DN   | XO-CHIP: desplaza arriba |
| save vx - vy / load vx - vy | 5XY2 / 5XY3 | XO-CHIP: guarda / carga un rango sin tocar I |
| ld i, long dir          | F000 NNNN | XO-CHIP: I = dir de 16 bits |
| plane n                 | FN01   | XO-CHIP: planos de dibujo (1, 2 o 3) |
| audio / pitch vx        | F002 / FX3A | XO-CHIP: patrón de sonido / altura |
| sys dir                 | 0NNN   | Código máquina del COSMAC (se ignora) |

Las palabras `i`, `dt`, `st`, `k`, `f`, `hf`, `b`, `r`, `long` y `v0`-`vf`
están reservadas y no pueden ser etiquetas.

### Un programa pequeño

    ; mueve un punto con A y D
        ld v1, 30          ; x
        ld v2, 16          ; y
        ld i, punto
        drw v1, v2, 1
    bucle:
        ld v0, 7           ; tecla 7 = A
        sknp v0
        call izquierda
        ld v0, 9           ; tecla 9 = D
        sknp v0
        call derecha
        jp bucle
    izquierda:
        drw v1, v2, 1      ; borra (el dibujo es XOR)
        add v1, -1
        drw v1, v2, 1
        ret
    derecha:
        drw v1, v2, 1
        add v1, 1
        drw v1, v2, 1
        ret
    punto:
        db %X.......

Guárdalo como `/tmp/punto.asm` y ejecuta:

    chip8asm /tmp/punto.asm
    chip8 /tmp/punto.ch8

### Desensamblar

`chip8asm -d rom.ch8` escribe el programa en la misma sintaxis, con la
dirección y los bytes de cada instrucción en un comentario. Lo que no es una
instrucción sale como `dw` o `db`, así que volver a ensamblar el resultado
da exactamente la misma ROM.
