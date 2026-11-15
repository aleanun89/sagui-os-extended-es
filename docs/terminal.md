# Terminal y sagsh: colores y aviso de administrador (D263)

## Colores

El Terminal entiende las secuencias ANSI SGR: 30-37 y 90-97 (letra), 40-47 y 100-107 (fondo), 1 (negrita,
que aclara los 8 colores básicos), 7 (inverso), 0/22/27/39/49 (quitar) y, llevados al color más parecido de
los 16 de la paleta del tema, `38;5;n`, `48;5;n` y `38;2;r;g;b`. La paleta es la del tema elegido en
Preferencias (Ctrl+,), así que los colores se ven bien tanto en los fondos oscuros como en los claros.

Quién manda colores es la variable **`TERM`**: la ponen el Terminal (`xterm-256color` desde la D320; antes `xterm-16color`) y `sshd` (lo que
pida el cliente). La consola de texto y el puerto serie no la tienen y no reciben ninguna secuencia.
`NO_COLOR` (cualquier valor) los quita siempre. Por SSH con `ssh -t` los bytes pasan tal cual: el
terminal del anfitrión los pinta.

| Orden | Colores |
|-------|---------|
| `ls` | carpetas azul, programas verde, imágenes/sonido/vídeo magenta, comprimidos rojo |
| `grep` | coincidencia en rojo, nombre de archivo en magenta, número de línea en verde |
| sagsh | mensajes de error en rojo |
| `ajuda` | títulos en cian |

Todos aceptan `--color[=always|never|auto]`. Con `auto` (por defecto) solo pintan si la salida es un
terminal con `TERM`; en tuberías (`ls | grep x`) y en archivos (`ls > f`) nunca.

## El prompt

`PS1` admite los escapes de bash: `\u` usuario, `\h` máquina, `\w` carpeta, `\W` su último componente,
`\$` (`$`, o `#` si el shell es root y se ve), `\e`/`\033` (ESC), `\a`, `\n`, `\[` y `\]`. El de
`/etc/sagshrc` es `sagui@saguios:/carpeta$` (usuario verde, carpeta azul) y, para root,
`root@saguios:/carpeta#` en rojo. Sin colores se queda `sagui:/carpeta$`.

## Root siempre visible

Antes de cada prompt, sagsh manda el título de la ventana (OSC 0): `[root] root@saguios:/carpeta` si es
root, `sagui@saguios:/carpeta` si no. Al verlo, el Terminal marca la pestaña: la barra de pestañas se
muestra aunque haya una sola, teñida de rojo, con «[root]» en el título de la pestaña, la franja
**ADMINISTRADOR** a la derecha y una línea roja bajo la barra. Al salir de `su` el shell de abajo vuelve a
mandar el título normal y la marca desaparece. En el modo rescate (consola) el prompt es `root:/#`.

## Historial

Con `TERM` (Terminal y SSH), sagsh guarda cada línea en `/home/.sagsh_historial` y la lee al arrancar
(las últimas 200). `history -c` también vacía el archivo. Tab completa órdenes y rutas, y Ctrl+R busca.

## Aplicaciones de pantalla completa: vim, less, htop, nano, mc (D320)

El emulador del Terminal vive en `userland/programas/terminal_vt.h` (lógica pura, probada en el PC con
`tests/terminal_vt_unidad.c`) y entiende un subconjunto de xterm/VT220 suficiente para las aplicaciones de pantalla
completa que se usan por `ssh` (D317). El emulador de la consola del kernel (`kernel/dispositivos/terminal_grafico.c`) es
otro, mínimo (D317: solo `ESC [ D C J K` y salta el resto); no se ha tocado y no recibe estas aplicaciones.

`TERM=xterm-256color`. Se eligió porque lo que las aplicaciones comprueban en terminfo (regiones de desplazamiento,
`bce`, pantalla alternativa, `ech`, `il`/`dl`, `rep`, colores 256) está implementado; los 256 colores y el truecolor se
aproximan a los 16 de la paleta del tema (el atributo de una celda son 16 bits).

**Modo xterm.** Con el sagsh local el Terminal conserva su comportamiento de siempre (LF vuelve a la columna 0, BS
borra, las teclas especiales salen como `ESC` + letra). El cliente `ssh` enciende el modo xterm con
`CSI ? 7700 h` (modo privado de SaguiOS) y lo apaga con `CSI ? 7700 l` al salir; el sagsh lo apaga también en cada prompt,
por si un `ssh` murió sin avisar. En modo xterm: LF no vuelve a la columna 0, BS solo mueve el cursor y las teclas
salen como las de un xterm.

| Grupo | Secuencias |
|-------|-----------|
| Controles | BEL, BS, HT, LF/VT/FF, CR, SO/SI |
| Cursor | CUP/HVP `H f`, CUU/CUD/CUF/CUB `A B C D`, CNL/CPL `E F`, CHA/HPA `G \``, VPA `d`/`e`, CHT/CBT `I Z`, DECSC/DECRC `ESC 7`/`ESC 8` y `CSI s`/`CSI u`, IND `ESC D`, NEL `ESC E`, RI `ESC M` (con desplazamiento inverso) |
| Borrado | ED 0/1/2/3 `J`, EL 0/1/2 `K`, ECH `X`, ICH `@`, DCH `P`, IL `L`, DL `M`, REP `b`; todo borra con el fondo actual (bce) |
| Regiones | DECSTBM `CSI t;b r` con LF, RI, IL/DL y SU/SD (`CSI S`, `CSI T`) dentro de ella; DECOM `?6`; DECAWM `?7` con salto de línea pendiente como xterm; IRM `CSI 4 h` |
| Pantalla alternativa | `?47`, `?1047`, `?1048`, `?1049` (guarda el cursor, limpia al entrar y restaura al salir); sin historial mientras está |
| Modos | `?25` cursor, `?1` DECCKM, `ESC =`/`ESC >` DECKPAM/DECKPNM, `?2004` pegado entre corchetes (el Terminal rodea el pegado con `ESC [ 200 ~` / `201 ~`), `CSI 20 h/l` LNM, `CSI ! p` DECSTR, `ESC c` RIS, `ESC # 8` DECALN |
| Se aceptan sin efecto | ratón (`?1000`, `?1002`, `?1003`, `?1006`, `?1015`), foco `?1004`, parpadeo `?12`, `?5`, `?2026`, DCS/APC/PM/SOS, títulos con `CSI 22/23 t`, DECSCUSR |
| SGR | 0, 1 negrita, 2 tenue, 4 subrayado, 5/6 parpadeo (se ignora), 7 inverso, 8 oculto y sus anulaciones (22 24 25 27 28), 30-37 90-97, 40-47 100-107, 39/49, `38;5;n`/`48;5;n` (también con `:`), `38;2;r;g;b`/`48;2;...` |
| Juegos de caracteres | `ESC ( 0`, `ESC ) 0` (dibujo de líneas DEC) y `ESC ( B`. Las cajas `j k l m n q t u v w x` se trazan con rectángulos (se ven en cualquier fuente), `a` y `0` como trama y bloque, el resto con una letra Latin-1 parecida |
| Respuestas | DSR 5 y 6 (`CSI 6n` da la posición, con DECOM relativa a la región), DA primario `CSI c` (`ESC [ ? 62 ; 22 c`), DA secundario `CSI > c`, tamaño `CSI 18 t` (D317) |

Las cajas Unicode (`U+2500`..`U+257F`, sombreados y bloque) que mandan htop y mc en UTF-8 las convierte `ssh` en el juego DEC
antes de pasarlas al Terminal; las gruesas, dobles y redondeadas se dibujan como la línea sencilla.

**Teclas en modo xterm.** F1-F4 `ESC O P`..`S`, F5-F12 `ESC [ 15~`..`24~`, flechas `ESC [ A` (o `ESC O A` con DECCKM),
Inicio/Fin `ESC [ H`/`F`, Insert/Supr/RePág/AvPág `ESC [ 2~ 3~ 5~ 6~`, con Mayús/Alt/Ctrl `ESC [ 1 ; m A` y
`ESC [ n ; m ~` (m = 1 + Mayús 1 + Alt 2 + Ctrl 4), Alt+tecla = `ESC` + tecla, y Esc es un `ESC` solo. F11 y F12 los
puede coger el escritorio o el programa que tenga el foco.

**Qué no hay:** ratón para las aplicaciones (se ignora), caracteres de más de una celda de ancho (CJK, emojis), colores
reales de 256 / 24 bits (se aproximan a 16), tabulaciones configurables (`HTS`/`TBC`: son fijas, cada 8), parpadeo, DECSCNM,
DECSLRM, sixel. Copiar texto de un recuadro DEC copia las letras del juego (`qqqq`).

Probado con vim, less, htop y nano reales (flujos grabados en un pty de 80x24, comparados fila a fila con tmux),
y por `ssh` contra el servidor de prueba de h317 (`tests/h320_terminal_vt.py`, capturas `H320-*.png`).
