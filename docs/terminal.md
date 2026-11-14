# Terminal y sagsh: colores y aviso de administrador (D263)

## Colores

El Terminal entiende las secuencias ANSI SGR: 30-37 y 90-97 (letra), 40-47 y 100-107 (fondo), 1 (negrita,
que aclara los 8 colores básicos), 7 (inverso), 0/22/27/39/49 (quitar) y, llevados al color más parecido de
los 16 de la paleta del tema, `38;5;n`, `48;5;n` y `38;2;r;g;b`. La paleta es la del tema elegido en
Preferencias (Ctrl+,), así que los colores se ven bien tanto en los fondos oscuros como en los claros.

Quién manda colores es la variable **`TERM`**: la ponen el Terminal (`xterm-16color`) y `sshd` (lo que
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
