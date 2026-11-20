# Ejemplo de proyecto con make dentro de SaguiOS (D413): tres ficheros .c, una cabecera y dependencias.
#
#   cp -r /apps/make/ejemplos/proyecto /tmp/proyecto && cd /tmp/proyecto
#   make              compila lo que falta
#   make              (otra vez) no hace nada: todo está al día
#   touch calculos.c && make     recompila solo calculos.c y enlaza
#   make clean        borra lo generado
#
# make usa /bin/sh (sagsh) para las recetas y tcc como compilador de C.
CC = tcc
CFLAGS = -Wall -O
include opciones.mk
OBJETOS = principal.o calculos.o util.o

calc: $(OBJETOS)
	$(CC) -o $@ $(OBJETOS)
	@echo "enlazado: $@"

# regla implícita propia (también valdría la de make: %.o: %.c)
%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

principal.o: calculos.h util.h
calculos.o: calculos.h
util.o: util.h

.PHONY: clean probar
probar: calc
	./calc 6

clean:
	rm -f $(OBJETOS) calc
