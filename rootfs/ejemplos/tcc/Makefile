# SPDX-License-Identifier: GPL-3.0-only
# Compila los cinco ejemplos con el tcc de SaguiOS:  make SALIDA=/tmp
CC = tcc
SALIDA = .

all: $(SALIDA)/huella $(SALIDA)/visorppm $(SALIDA)/snake $(SALIDA)/visorlogs $(SALIDA)/lumen

$(SALIDA)/huella: huella.c
	$(CC) $(CFLAGS) -o $@ huella.c

$(SALIDA)/visorppm: visorppm.c
	$(CC) $(CFLAGS) -o $@ visorppm.c

$(SALIDA)/snake: snake.c
	$(CC) $(CFLAGS) -o $@ snake.c

$(SALIDA)/visorlogs: visorlogs.c
	$(CC) $(CFLAGS) -o $@ visorlogs.c

$(SALIDA)/lumen: lumen.c
	$(CC) $(CFLAGS) -o $@ lumen.c

.PHONY: all
