# Pascal en SaguiOS

`pascal` compila programas en **Pascal** (al estilo de Turbo Pascal) dentro
del propio SaguiOS, directamente a un ejecutable, sin ensamblador ni
enlazador, igual que `cc` con C.

## Uso

    pascal programa.pas              genera ./programa en la carpeta actual
    pascal programa.pas -o nombre    genera "nombre"
    pascal -x87 programa.pas         los real con la FPU x87 (CPU sin SSE2)
    pascal -sse2 programa.pas        los real con SSE2

Sin `-x87` ni `-sse2`, el programa sale para la CPU en la que se compila.

El generador optimiza por omisión (`-O1`: operandos inmediatos, comparar y saltar,
desplazamientos en vez de `*` y `div` por potencias de 2). `-O0` lo desactiva todo, y
`-fno-mirilla`, `-fno-inmediatos`, `-fno-saltos` o `-fno-reduccion` desactivan un grupo:
sirve para comparar el tamaño y la velocidad. La salida del programa es la misma.

Prueba los ejemplos de `/ejemplos/pascal`:

    pascal /ejemplos/pascal/hola.pas -o /tmp/hola
    /tmp/hola

| Ejemplo         | Qué enseña                                          |
|-----------------|-----------------------------------------------------|
| `hola.pas`      | el programa más pequeño                             |
| `primos.pas`    | vectores y bucles (criba de Eratóstenes)            |
| `fibonacci.pas` | funciones recursivas e iterativas                   |
| `cadenas.pas`   | string, copy, pos, delete, insert, str, val         |
| `conjuntos.pas` | tipos enumerados y conjuntos (set of)               |
| `ordenar.pas`   | registros y ordenación                              |
| `lista.pas`     | punteros: new, dispose y una lista enlazada         |
| `archivos.pas`  | archivos de texto: rewrite, reset, append           |
| `colores.pas`   | unidad crt: clrscr, gotoxy, readkey                 |
| `fractal.pas`   | unidad sagui: gráficos en una ventana               |

En **Mico**, F5 en un archivo `.pas` lo compila y lo ejecuta en el terminal
de abajo; los errores salen en Problemas y un clic lleva a la línea.

## El lenguaje

- Tipos: `integer` y `longint` (32 bits), `byte`, `shortint`, `word`,
  `smallint`, `char`, `boolean`, `real` (= `double`), `string` y
  `string[n]`, enumerados, subrangos, `array` (también de varias
  dimensiones), `record` (con parte variante), `set of`, punteros (`^T`,
  `pointer`), `text` y `file of T`.
- Constantes con tipo (`const veces: integer = 0;`), que guardan su valor.
- Procedimientos y funciones anidados, parámetros `var` y `const`,
  `forward`, `result` y `exit`; funciones que devuelven string, record o set.
- `if`, `case` (con rangos y `else`), `for ... to/downto`, `while`,
  `repeat`, `with`, `break`, `continue`, `goto`.
- Rutinas: `write`/`writeln` con ancho y decimales (`x:8:2`), `read`/`readln`,
  `abs sqr sqrt sin cos arctan ln exp pi trunc round frac int`,
  `ord chr succ pred odd inc dec high low sizeof`, `length copy pos concat
  delete insert upcase lowercase str val`, `new dispose getmem freemem
  fillchar move`, `include exclude`, `random randomize`, `paramcount
  paramstr`, `halt`, `assign reset rewrite append close eof eoln ioresult`.
- Directivas: `{$R+}` (comprobar rangos), `{$I archivo}`, `{$DEFINE}`,
  `{$IFDEF}`/`{$IFNDEF}`/`{$ELSE}`/`{$ENDIF}`. Está definido `SAGUIOS`.

## Errores

Los errores de compilación dicen archivo, línea y columna:

    /tmp/prueba.pas:5:3: error: 'b' no está declarado

Con `{$R+}`, un índice o un valor fuera de rango para el programa:

    Error de ejecución 201: valor fuera de rango (línea 6)

Otros errores de ejecución: 200 (división por cero), 2 (archivo no
encontrado), 103-105 (archivo no abierto) y 203 (sin memoria).

## Unidad crt

`uses crt;` da `clrscr`, `gotoxy`, `wherex`, `wherey`, `clreol`, `readkey`,
`keypressed`, `delay`, `sound`, `nosound`, `textcolor`, `textbackground`,
`normvideo`, `textattr` y las constantes de color (`black` ... `white`).

- `readkey` lee una tecla sin esperar a Intro. Las teclas especiales dan
  `#0` y después su código, como en el PC: flechas `#72 #80 #75 #77`,
  Inicio `#71`, Fin `#79`, RePág `#73`, AvPág `#81`, Supr `#83`.
- `gotoxy` y `clrscr` usan una pantalla de 80x25: tras ellas, lo que
  escribe el programa se dibuja entero cuando el programa espera una tecla,
  hace `delay` o termina.
- **Limitaciones actuales:** el terminal aún no tiene colores, así que
  `textcolor` y `textbackground` solo guardan el valor; y `keypressed`
  solo detecta teclas cuando la entrada viene de un pipe (en el terminal
  devuelve siempre false).

## Unidad sagui: gráficos

`uses sagui;` abre una ventana y dibuja en ella:

    abrir_ventana(ancho, alto, 'título');
    color(rojo);  color(rgb(255, 128, 0));
    pixel(x, y);  linea(x1, y1, x2, y2);
    rect(x, y, an, al);  rect_lleno(x, y, an, al);
    circulo(x, y, r);  circulo_lleno(x, y, r);
    texto(x, y, 'hola');  limpiar;  actualizar;
    tecla_pulsada  esperar_tecla  pausa(ms)  cerrar_ventana

Colores: `negro blanco rojo verde azul amarillo naranja gris violeta cian
marron rosa`. Teclas: `tecla_arriba tecla_abajo tecla_izquierda
tecla_derecha tecla_esc tecla_enter`. La unidad `graph` (BGI) no existe:
usa `sagui`.

## Qué no hay (todavía)

Objetos (`object`, `class`), `unit` propias, cadenas largas
(`ansistring`), sobrecarga de funciones y `variant`.
