# 003 - Compilador de C dentro de SaguiOS

- **Estado:** hecha (rama `compilador-micropython`, fusionada en `integracion`)
- **Prioridad:** media
- **Alta:** 2026-11-06

## Problema / motivación

Hoy los programas solo se compilan fuera, en Linux. Poder escribir un `.c` en el Editor y
compilarlo dentro de SaguiOS lo convierte en un entorno autosuficiente.

## Propuesta

Un compilador pequeño de un subconjunto de C (int, char, punteros, arrays, funciones,
if/while/for) que genere directamente un ELF i386 ejecutable por `sagsh`, usando libsagui y
las llamadas de sistema. Mejor escrito desde cero, por coherencia con el proyecto; si se
porta uno existente, hay que registrar su licencia en `docs/decisoes.md`.

## Criterios de aceptación

- [x] `cc /ejemplos/hola.c` genera un ejecutable que imprime el texto esperado
- [x] Test automático en QEMU (`tests/b5_compilador_c.py`)
- [x] El tamaño de la imagen sigue dentro de la meta del PRD (o se justifica)
- [x] Integrado en `sagui-os-main`

## Resultado

Compilador propio de una pasada, ~2.600 líneas, sin ensamblador ni enlazador (D033).
Uso: `cc /ejemplos/hola.c -o /tmp/hola` y luego `/tmp/hola`. Los errores se muestran como
`archivo:línea: error: ...`.

### Coma flotante (rama `cc-float`, D039)

- [x] `float` y `double` (`long double` = `double`): literales, variables, vectores,
  campos de struct, punteros, aritmética, comparaciones con NaN correctos, `!`/`&&`/`||`,
  asignaciones compuestas, `++`/`--`, conversiones y casts, varargs, paso y retorno.
- [x] SSE2 escalar por defecto; `cc -x87` usa la FPU x87 (CPUs sin SSE2). Medido: en
  QEMU el SSE2 es 6 veces más rápido que la x87 y en un PC actual 1,7 veces.
- [x] `printf` con `%f %e %g` (salida idéntica a la de GCC), `strtod`/`atof` y `<math.h>`
  (funciones con instrucción de la x87 como funciones internas `__builtin_*`).
- [x] Eliminación de las funciones no usadas de la biblioteca: `hola.c` pasa de 13,5 KB a
  5 KB. Mejoras del código generado que también aceleran los enteros (1,6 veces).
- [x] Ejemplos `/ejemplos/mandelbrot.c`, `/ejemplos/flotante.c` y
  `/ejemplos/rendimiento.c` (double, float y punto fijo cronometrados).
- [x] Prueba `tests/b7_cc_flotante.py`: salida igual a GCC dentro de SaguiOS, también con
  `-x87`, y dos programas de coma flotante en paralelo.

Rendimiento (Mandelbrot 79x23, `rendimiento.c`): en QEMU, 20 repeticiones, `double` 80 ms
con SSE2 y 480 ms con `-x87`, frente a 30 ms del mismo cálculo en punto fijo; GCC `-O0`
tarda 90 ms (SSE2) y 450 ms (x87).

### Varios ficheros de origen y más biblioteca (D133)

- [x] **Varios ficheros de origen:** `cc util.c principal.c -o programa`. Cada fichero es
  una unidad de traducción con su propio ámbito: lo declarado `static` (función,
  variable o etiqueta de `struct`) sólo se ve en su fichero, y dos ficheros pueden
  tener cada uno su `static` con el mismo nombre sin chocar. Un `static` usado desde
  otro fichero da el error de «no declarado».
- [x] **Biblioteca partida en dos:** la cabecera (tipos, macros y prototipos) se lee
  antes que el programa, como los `<...>` del C; las funciones van después, para que
  las que el programa no usa se sigan borrando (`hola.c` sigue en 5 KB).
- [x] **Biblioteca nueva:** ficheros (`fopen`, `fclose`, `fread`, `fwrite`, `fseek`,
  `ftell`, `rewind`), `qsort` con su comparador, `strtol`, `strtoul`, `atol`, `labs` y
  el reloj (`time`, `clock`, `difftime`, `localtime`, `gmtime`, `strftime`, con
  `struct tm`). El `FILE` del SaguiOS es el descriptor del fichero.
- [x] Prueba `tests/h102_cc_multifichero.py`: dos ficheros forman un programa solo, los
  `static` no se ven entre ficheros, una función repetida sin `static` da error, un
  programa de un solo fichero sigue igual, y la biblioteca nueva funciona.

## Ideas siguientes

- `goto`, struct por valor, `#`/`##` en macros.
- Optimización mayor: asignar registros (las variables siguen pasando por memoria en
  cada sentencia; es lo que hoy limita el rendimiento de los cálculos con reales).
- Integración con el Editor: compilar y ejecutar con una tecla.
- Mencionar `-x87` en `ajuda cc` (el texto está en `sagsh.c`).
