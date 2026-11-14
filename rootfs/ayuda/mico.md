# Mico: editor de código

**Mico** es el editor de código de SaguiOS: explorador de la carpeta a la
izquierda, pestañas, colores para C, Python, sh, Markdown y ensamblador de
CHIP-8, buscar y reemplazar, y abajo un terminal y la lista de **problemas**
de la compilación. **F5** compila y ejecuta el archivo actual.

## Cómo abrirlo

- Desde el menú **Inicio > Accesorios > Mico (editor de código)**.
- Desde el terminal: `mico carpeta`, `mico archivo...` o `mico` solo.
- Prueba el proyecto de ejemplo: `mico /ejemplos/mico`

Para ver en el terminal qué color recibe cada letra de un archivo:

    mico -t archivo

Cada línea del archivo sale como una línea de letras: `k` palabra clave,
`t` tipo, `s` cadena, `c` comentario, `n` número, `p` preprocesador,
`f` función, `h` título, `e` énfasis, `r` registro, `l` etiqueta,
`v` variable y `.` el resto.

## Atajos de teclado

| Tecla                   | Qué hace                                      |
|-------------------------|-----------------------------------------------|
| Ctrl+N / Ctrl+O         | archivo nuevo / abrir un archivo              |
| Ctrl+S / Ctrl+Mayús+S   | guardar / guardar como                        |
| Ctrl+W / Ctrl+Q         | cerrar la pestaña / salir                     |
| Ctrl+Tab                | pestaña siguiente (con Mayús, la anterior)    |
| Ctrl+Z / Ctrl+Y         | deshacer / rehacer                            |
| Ctrl+X / Ctrl+C / Ctrl+V | cortar / copiar / pegar (sin selección: la línea) |
| Ctrl+A                  | seleccionar todo                              |
| Mayús + flechas         | seleccionar; también arrastrando con el ratón |
| doble / triple clic     | seleccionar la palabra / la línea             |
| Ctrl+/ (Ctrl+Mayús+7)   | comentar o descomentar las líneas             |
| Ctrl+D                  | duplicar la línea                             |
| Alt+Arriba / Alt+Abajo  | subir / bajar la línea                        |
| Tab / Mayús+Tab         | sangrar / quitar sangría (varias líneas)      |
| Ctrl+Espacio            | completar con palabras del documento          |
| Ctrl+F / Ctrl+H         | buscar / reemplazar                           |
| F3 / Mayús+F3           | siguiente / anterior                          |
| Ctrl+Mayús+F            | buscar en todos los archivos de la carpeta    |
| Ctrl+G                  | ir a la línea (`línea` o `línea:columna`)     |
| Ctrl+P                  | abrir un archivo de la carpeta por su nombre  |
| Ctrl+Mayús+P            | paleta: todas las órdenes por su nombre       |
| Ctrl+B / Ctrl+J         | mostrar u ocultar el explorador / el panel    |
| F5 / Mayús+F5           | ejecutar / interrumpir (Ctrl+C)               |
| F6                      | pasar del editor al terminal y volver         |
| F11                     | pantalla completa                             |
| F1                      | esta ayuda                                    |

En la barra de búsqueda: Intro busca el siguiente, Mayús+Intro el anterior,
Tab pasa al campo de reemplazo (allí Intro reemplaza uno y Mayús+Intro
todos), **Alt+M** distingue mayúsculas, **Alt+P** busca la palabra completa
y Esc la cierra.

En el terminal de abajo, Ctrl+C y Ctrl+D son del shell; para copiar y pegar
usa **Ctrl+Mayús+C** y **Ctrl+Mayús+V** o el botón derecho.

## Escribir código

- Al pulsar Intro, la línea nueva copia la sangría de la anterior, y añade un
  nivel después de `{`, `(`, `[` o, en Python, de `:`.
- Los paréntesis, corchetes, llaves y comillas se cierran solos; escribir el
  cierre que ya está a la derecha solo lo salta.
- La tabulación es de 4 espacios; cámbiala en **Editar > Sangría** o en el
  archivo `.mico`.

## Explorador

Clic en una carpeta para abrirla o cerrarla; clic en un archivo para
abrirlo en una pestaña. Con el botón derecho: Abrir, Nuevo archivo,
Nueva carpeta, Renombrar (F2), Borrar (Supr), Actualizar y Abrir terminal
aquí. Los nombres que empiezan por `.` no se muestran, salvo `.mico`.

## Ejecutar (F5) y problemas

F5 guarda el archivo y escribe la orden en el terminal de abajo:

| Extensión | Orden                                                         |
|-----------|---------------------------------------------------------------|
| `.c`      | `cc {archivo} -o /tmp/{base} && /tmp/{base}`                  |
| `.pas`    | `pascal {archivo} -o /tmp/{base} && /tmp/{base}`              |
| `.py`     | `python {archivo}`                                            |
| `.sh`     | `sh {archivo}`                                                |
| `.asm`    | `chip8asm {archivo} -o /tmp/{base}.ch8 && chip8 /tmp/{base}.ch8` |
| `.md`     | `markdown {archivo}`                                          |

Los errores con la forma `archivo:línea: mensaje` (de `cc`, `pascal` y `chip8asm`) y
las trazas de Python aparecen en la pestaña **Problemas**: un clic lleva
al archivo y a la línea.

## El archivo `.mico`

Un archivo `.mico` en la carpeta abierta configura el Mico para ese
proyecto. Una opción por línea, `nombre = valor`; `#` empieza un
comentario.

    # .mico de mi proyecto
    tab = 4                 # columnas por tabulación (1 a 16)
    espacios = sí           # Tab escribe espacios (no: un carácter tab)
    tema = oscuro           # claro u oscuro
    ejecutar.c = cc {archivo} -o /tmp/{base} && /tmp/{base} hola
    ejecutar.sh = sh {archivo} 5
    ejecutar = make         # para cualquier otra extensión

En las órdenes, `{archivo}` es la ruta completa, `{nombre}` el nombre con
extensión, `{base}` sin extensión y `{carpeta}` la carpeta del archivo. Al
guardar el `.mico` desde el propio Mico, se vuelve a leer.

## Tus datos

- **Guardar es atómico:** Mico escribe `archivo.nuevo` y luego lo renombra
  encima del original; si algo falla a medias, el archivo anterior sigue
  entero.
- **Copia de recuperación:** cada 5 segundos, lo que no está guardado se
  copia en `/home/.mico` (o en `/tmp/.mico` si no hay `/home`). Si Mico se
  cierra de golpe, al abrirlo otra vez pregunta si quieres recuperarlo.
- Si otro programa cambia un archivo abierto, Mico lo recarga (o pregunta,
  si tienes cambios sin guardar).
- Al cerrar con cambios sin guardar, Mico pregunta.

Más detalles: decisiones D074 (portapapeles) y D075 (Mico).
