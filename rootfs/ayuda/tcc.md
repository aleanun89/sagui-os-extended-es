# TCC: compilar C dentro de SaguiOS

**TCC** (Tiny C Compiler, `/apps/bin/tcc`) compila programas en C en un instante, dentro del propio SaguiOS.
Los ejemplos están en `/apps/tcc/ejemplos`; cópialos a tu carpeta para cambiarlos.

## Lo básico

    tcc hola.c -o hola                      un programa
    ./hola
    tcc -run hola.c                         compilar en memoria y ejecutar, sin fichero
    tcc -o calculos principal.c calculos.c  varios ficheros
    tcc -c calculos.c                       por partes: .o y después enlazar
    tcc -o calculos principal.o calculos.o

Los programas salen **estáticos**: llevan dentro lo que usan de la libc y la libsagui. No hace falta `-lm`.
TCC no optimiza (es rápido compilando, no tanto el programa). Los textos se guardan tal como están escritos
en el fichero: el terminal usa Latin-1.

## Con la libsagui (ventanas de SaguiOS)

`#include "sagui.h"` y el programa puede empezar en `principal(argc, argv)` como los de `/bin` (o en `main`).
Ejemplo: `tcc /apps/tcc/ejemplos/ventana.c -o ventana && ./ventana` abre una ventana con un degradado.

## Con la SDL 2

    tcc sdl.c -o sdl -lSDL2                 (también -lSDL2_image, -lSDL2_ttf, -lSDL2_mixer)

`#include <SDL2/SDL.h>` o `"SDL.h"`. Ejemplo: `/apps/tcc/ejemplos/sdl.c` (un cuadrado que rebota).

## Encontrar errores de memoria con -b

`-b` añade **comprobación de límites**: si el programa escribe fuera de un array o usa memoria ya liberada,
se para y dice el fichero y la línea, en vez de fallar en silencio o mucho después:

    tcc -b limites.c -o limites && ./limites
    /apps/tcc/ejemplos/limites.c:13: at main: BCHECK: ... is outside of the region ...

Prueba `limites.c` y `liberada.c` de los ejemplos con y sin `-b`. El programa va más lento: úsalo para buscar
fallos, no para el programa final. También vale con `-run` (`tcc -b -run prog.c`).

## C desde un programa: libtcc

Un programa puede compilar C en memoria y llamarlo (un «laboratorio», complementos...):

    tcc libtcc_hola.c -o libtcc_hola -ltcc && ./libtcc_hola

Ver `/apps/tcc/ejemplos/libtcc_hola.c` (`tcc_new`, `tcc_compile_string`, `tcc_relocate`, `tcc_get_symbol`).
La cabecera es `<libtcc.h>`.

## TCC se compila a sí mismo

Los fuentes de TCC están en `/apps/tcc/fuentes`:

    cp -r /apps/tcc/fuentes /tmp/tcc
    cd /tmp/tcc
    sh compilar.sh                          deja ./tcc
    ./tcc -run /apps/tcc/ejemplos/hola.c

## Licencia de lo que compilas

Los programas llevan dentro la libc y la libsagui de SaguiOS (y la SDL 2 si la usan), que son **GPL 3**
(solo la versión 3): si **distribuyes** un programa compilado así, tiene que ir bajo la GPL 3. Un programa GPL 2
(solo la versión 2) o propietario no puede distribuirse enlazado con ellas; para uso propio no hay límite.
`libtcc1.a` (TCC) tiene una excepción de enlace y `-b` y libtcc son LGPL 2.1 o posterior: no añaden nada.
Está previsto (mejora 143) que la libsagui pase a licencia MIT.

## Más

- Modificar y recompilar el juego **AoEN**: `/ayuda/modificar-aoen.md`.
- Cabeceras en `/apps/tcc/include` (`sagui/` las de SaguiOS, `SDL2/` las de la SDL); bibliotecas en `/apps/tcc/lib`.
- TCC es software libre (LGPL 2.1 o posterior): `/apps/tcc/LICENCIA-TCC.txt`.
