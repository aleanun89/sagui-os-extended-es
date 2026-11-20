# make: compilar proyectos dentro de SaguiOS

**make** (`/apps/bin/make`) es GNU make 4.4.1. Lee un `Makefile`, mira qué ficheros han cambiado y ejecuta solo
las órdenes necesarias. Va bien con **tcc** (ver `tcc.md`) y usa `/bin/sh` (sagsh) para las recetas.
Ejemplo completo en `/apps/make/ejemplos/proyecto`: cópialo a tu carpeta para probarlo.

## Un Makefile mínimo

    CC = tcc
    OBJETOS = principal.o calculos.o

    calc: $(OBJETOS)
    <TAB>$(CC) -o $@ $(OBJETOS)

    %.o: %.c
    <TAB>$(CC) -c -o $@ $<

    clean:
    <TAB>rm -f $(OBJETOS) calc

Las líneas de receta empiezan con un **tabulador**, no con espacios. `$@` es el objetivo, `$<` la primera
dependencia, `$^` todas. Las dependencias de cabeceras se escriben a mano: `principal.o: calculos.h`.

## Lo que se usa más

    make                    compila el primer objetivo
    make calc               un objetivo concreto
    make -n                 solo muestra lo que haría
    make -j2                dos recetas a la vez (en SaguiOS, la salida se mezcla: usa -O para ordenarla)
    make -C carpeta         trabaja en otra carpeta
    make -f otro.mk         otro fichero en vez de Makefile
    make CC=tcc CFLAGS=-b   variables desde la línea (con tcc, -b comprueba los límites de arrays)
    make -B                 recompila todo
    make -p                 las reglas y variables que trae (reglas implícitas de C, etc.)

También funcionan `include otro.mk`, `$(wildcard *.c)`, `$(patsubst ...)`, `$(shell ...)`, `ifeq`, `define`,
reglas con patrón (`%.o: %.c`), objetivos falsos (`.PHONY`) y variables de entorno.

## Compilar AoEN y TCC con make

    cp -r /apps/aoen/fuentes /tmp/aoen && cd /tmp/aoen && make -j2    deja ./aoen (un .o por fichero)
    cp -r /apps/tcc/fuentes /tmp/tcc && cd /tmp/tcc && make           deja ./tcc

Al tocar un fichero, `make` recompila solo ese. Si no se quiere make, `sh compilar.sh` sigue funcionando.

## Límites

Sin guile ni carga de módulos (`load`). La hora de los ficheros es de un segundo: si dos órdenes seguidas tocan
un fichero en el mismo segundo, make puede creerlo al día (`sleep 1` entre ellas). `-l` (límite de carga) no
limita nada. Licencia GPL-3.0-or-later, en `/apps/make/LICENCIA-MAKE.txt`.
