# SPDX-License-Identifier: MIT
# Makefile de Capibara para Linux y otros sistemas con la SDL 2 (D452). Para compilarlo dentro de SaguiOS se usa
# capibara.mk (lo incluye el Makefile principal). Necesita: gcc, SDL2 y SDL2_image (con pkg-config) y Lua 5.4.
#
#   make                       compila build/capibara con Lua 5.4 compilado desde fuentes
#   make LUA_SRC=/ruta/src     carpeta src de Lua 5.4 (por defecto ../terceros/lua/fuentes; se baja con tools/lua_fuentes.sh)
#   make LUA=sistema           usa el Lua 5.4 del sistema (pkg-config lua5.4)
#   make prueba                compila y pasa las pruebas del núcleo y de los ejemplos (sin ventana)
CC      ?= gcc
LUA_SRC ?= ../terceros/lua/fuentes
B       := build
CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation $(shell pkg-config --cflags sdl2 SDL2_image)
MOTOR   := nodo texto props recursos escena entrada fisica guion audio dibujo mundo escritor
SDLCAPA := dibujo_sdl plataforma_sdl principal_pc

ifeq ($(LUA),sistema)
LUA_CF  := $(shell pkg-config --cflags lua5.4)
LUA_LIB := $(shell pkg-config --libs lua5.4)
LUA_OBJ :=
else
LUA_NUCLEO := lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm \
              lundump lvm lzio lauxlib lbaselib lcorolib ldblib liolib lmathlib loadlib loslib lstrlib ltablib lutf8lib linit
LUA_OBJ := $(addprefix $(B)/lua/,$(addsuffix .o,$(LUA_NUCLEO)))
LUA_CF  := -I$(LUA_SRC)
LUA_LIB :=
$(B)/lua/%.o: $(LUA_SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -w -DLUA_USE_POSIX -c -o $@ $<
endif

OBJ  := $(addprefix $(B)/,$(addsuffix .o,$(MOTOR) $(SDLCAPA)))
HDR  := motor/capi.h motor/dibujo_sdl.h

all: $(B)/capibara
$(B)/%.o: motor/%.c $(HDR)
	@mkdir -p $(B)
	$(CC) $(CFLAGS) $(LUA_CF) -Imotor -c -o $@ $<
$(B)/capibara: $(OBJ) $(LUA_OBJ)
	$(CC) -o $@ $^ $(shell pkg-config --libs sdl2 SDL2_image) $(LUA_LIB) -lm
# el núcleo sin SDL (pruebas): todo menos las capas de la SDL
$(B)/prueba_nucleo: pruebas/prueba_nucleo.c $(addprefix $(B)/,$(addsuffix .o,$(MOTOR))) $(LUA_OBJ)
	$(CC) $(CFLAGS) $(LUA_CF) -Imotor -o $@ $^ $(LUA_LIB) -lm
prueba: all $(B)/prueba_nucleo
	$(B)/prueba_nucleo
	SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy sh pruebas/ejemplos.sh
clean:
	rm -rf $(B)
.PHONY: all prueba clean
