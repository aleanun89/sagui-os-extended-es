# El Terminal

El Terminal es la ventana donde escribes órdenes. Dentro corre **sagsh**, el shell de SaguiOS:
escribe una orden, pulsa Intro y verás lo que responde. Escribe `ayuda` para ver todas las
órdenes por secciones, y `ayuda ORDEN` para saber cómo se usa una (por ejemplo, `ayuda ls`).

## Lo primero

| Escribe | Para |
|---------|------|
| `ayuda` | ver las órdenes, por secciones |
| `ayuda ls` | saber cómo se usa una orden |
| `ayuda alias` | ver los nombres en español (`listar` = `ls`, `buscar` = `grep`...) |
| `ayuda sh` | el lenguaje del shell: variables, `if`, `for`, funciones |
| `ls` | ver los archivos de la carpeta, en columnas y con colores |
| `ls -l` | lo mismo con permisos, dueño, fecha y tamaño (`-h`: tamaños legibles) |
| `cd carpeta` | entrar en una carpeta (`cd ..` sube una) |
| `clear` | limpiar la pantalla (también Ctrl+L) |

Los nombres de antes, en portugués (`ajuda`, `limpar`, `conta`...), siguen funcionando.

## Colores de ls

Carpetas en **azul**, programas en **verde**, imágenes, sonido y vídeo en **magenta** y
comprimidos en **rojo**. En el Terminal las carpetas salen primero; en una tubería
(`ls | grep x`) o en un archivo, `ls` escribe un nombre por línea y sin colores.

## Teclas

| Tecla | Hace |
|-------|------|
| Tab | completa el nombre de una orden o de un archivo |
| Flecha arriba / abajo | recorre las órdenes anteriores |
| Ctrl+R | busca en las órdenes anteriores |
| Ctrl+C | para la orden en marcha |
| Ctrl+L | limpia la pantalla |
| Ctrl+Mayús+C / Ctrl+Mayús+V | copiar / pegar |
| Ctrl+Mayús+T / Ctrl+Mayús+W | nueva pestaña / cerrar la pestaña |
| Ctrl+Mayús+F | buscar en lo que salió |
| Mayús+RePág / Mayús+AvPág | subir y bajar por lo que salió |
| Ctrl+más / Ctrl+menos / Ctrl+0 | tamaño de la letra |
| Ctrl+, | preferencias |
| F1 | esta ayuda |

Con el ratón: arrastra para seleccionar (doble clic: una palabra), botón del medio para pegar,
la rueda o la **barra de la derecha** para ver lo que salió antes (clic en la barra: una
página; arrastrarla: a cualquier sitio). Ctrl+clic en un enlace lo abre en el navegador.

## Preferencias (Ctrl+,)

El tema de colores (Oscuro, Claro, Verde fósforo, Ámbar, Mar, Papel o los tuyos), la fuente (la
interna de 8x16 o una monoespaciada de `/apps/fuentes`) y su tamaño, el **interlineado** (el
aire entre filas, 2 píxeles por defecto), el tamaño de la ventana, las líneas de historial, el
cursor y la campana. Se guardan en `~/.config/terminal.conf`.

## Tu configuración: ~/.sagshrc

Cada vez que se abre un shell (una ventana o una pestaña nueva, o por SSH), sagsh lee primero
`/etc/sagshrc`, el de todo el sistema, y después **`~/.sagshrc`**, el tuyo. Ahí puedes poner:

```
alias ll='ls -l'                      # un nombre corto para una orden
export EDITOR=mico                    # variables para los programas
PS1='\w\$ '                           # el prompt
mkcd() { mkdir -p "$1" && cd "$1"; }  # funciones
echo "Hola"                           # órdenes al arrancar
```

La primera vez se crea uno de ejemplo, todo comentado (es una copia de `/etc/skel/.sagshrc`):
quita el `#` de lo que quieras usar. Si algo está mal escrito, el shell avisa y se abre igual.
Para probar un cambio sin cerrar el Terminal: `. ~/.sagshrc`. Para abrir un shell sin leerlo:
`sagsh --sin-rc`.

## El prompt

`usuario@saguios:/carpeta$`, con el usuario en verde y la carpeta en azul. Como administrador
(root) termina en `#` y va en rojo, y la ventana lleva la franja **ADMINISTRADOR**.

## El tamaño de la ventana

El Terminal se puede agrandar arrastrando la esquina. El shell sabe el tamaño en cada momento
(variables `COLUMNS` y `LINES`) y `ls` y `ayuda` se ajustan a él.
