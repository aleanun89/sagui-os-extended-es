# SPDX-License-Identifier: GPL-3.0-or-later
# Makefile - compila AoEN dentro de SaguiOS con make y tcc (D413; la alternativa sin make es compilar.sh).
#
#   cp -r /apps/aoen/fuentes /tmp/aoen && cd /tmp/aoen
#   make              deja ./aoen (un .o por fichero: al cambiar uno solo se recompila ese)
#   make -j2          dos compilaciones a la vez
#   make clean
FUENTES = @FUENTES@
OBJETOS = $(FUENTES:.c=.o)
CC = tcc
CFLAGS = -DSAGUI_AOEN -DAOEN_FONTE_SAGUI -DSTBI_NO_THREAD_LOCALS -DSTBI_NO_SIMD -DNDEBUG -I.

aoen: $(OBJETOS)
	$(CC) -o $@ $(OBJETOS) -lSDL2
	@echo "listo: ./aoen"

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJETOS) aoen

.PHONY: clean
