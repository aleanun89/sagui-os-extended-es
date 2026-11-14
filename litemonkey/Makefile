# SPDX-License-Identifier: GPL-2.0-or-later
# lite-monkey en el PC (Linux/WSL): make -C litemonkey [test|bench]
# En SaguiOS, /bin/litemonkey lo compila el Makefile principal.
CC      ?= gcc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -I../kernel/inc
B       := build
NUCLEO  := $(wildcard nucleo/*.c) ../kernel/lib/sha256.c
CAB     := $(wildcard nucleo/*.h) plataforma.h
PY      ?= python3

all: $(B)/litemonkey

$(B)/litemonkey: $(NUCLEO) principal.c plataforma_posix.c $(CAB)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -o $@ $(NUCLEO) principal.c plataforma_posix.c

$(B)/teste_nucleo: testes/teste_nucleo.c $(NUCLEO) principal.c plataforma_posix.c $(CAB)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -DLMK_SIN_MAIN -fsanitize=address,undefined -o $@ testes/teste_nucleo.c $(NUCLEO) principal.c plataforma_posix.c

# Pruebas: las del núcleo en C (con ASan/UBSan) y las del cliente Python
# contra un servidor de verdad en un puerto libre.
test: $(B)/teste_nucleo $(B)/litemonkey
	$(B)/teste_nucleo
	$(PY) testes/teste_cliente.py --servidor $(B)/litemonkey

bench: $(B)/litemonkey
	$(PY) testes/rendimiento.py --servidor $(B)/litemonkey

# Comparación con MongoDB (solo si hay mongod y pymongo)
mongo: $(B)/litemonkey
	$(PY) testes/comparar_mongo.py --servidor $(B)/litemonkey

clean:
	rm -rf $(B)

.PHONY: all test bench mongo clean
