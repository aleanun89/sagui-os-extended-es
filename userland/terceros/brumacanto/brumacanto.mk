# SPDX-License-Identifier: MIT
# brumacanto.mk - cómo se compila /apps/bin/brumacanto (D444): juego de exploración, minería y construcción
# con vóxeles en primera persona (MIT, "Brumacanto contributors"; gráficos CC0 de Kenney y texturas CC BY-SA 3.0 de Minetest Game, D449). Es C11, SDL 2 y
# OpenGL ES-CM 1.1: se enlaza con la SDL 2 del sistema (userland/libsdl2), SDL_GL_* sobre EGL (sdl2_gl.c) y SaguiGL
# (libsgl, por software o por la GPU con gpu3d v2). NO se usan las cabeceras third_party/sdl2 ni khronos del ZIP original.
# Entra también en la variante sin GPL (código MIT; los gráficos son datos CC0 y CC BY-SA, no código).

BR_DIR := userland/terceros/brumacanto
BR_S   := $(BR_DIR)/fuentes/src
BR_O   := $(B)/$(BR_DIR)
BR_SRC := $(addprefix $(BR_S)/,main.c draw.c world.c sim.c save.c) $(BR_DIR)/plataforma/principal_bruma.c
BR_FLAGS := -m32 -march=i686 -std=gnu11 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -DSAGUI_BRUMACANTO -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wno-unused-parameter
BR_HDR := $(wildcard $(BR_S)/*.h) $(BR_DIR)/brumacanto.mk
BR_OBJ := $(patsubst $(BR_S)/%.c,$(BR_O)/%.o,$(filter $(BR_S)/%,$(BR_SRC))) $(BR_O)/principal_bruma.o

include userland/libsdl2/sdl2_capas.mk

$(BR_O)/%.o: $(BR_S)/%.c $(BR_HDR) $(LIBC_HDR) $(USER_HDR) $(wildcard userland/inc/GLES/*.h userland/inc/EGL/*.h)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(BR_FLAGS) -c -o $@ $<

$(BR_O)/principal_bruma.o: $(BR_DIR)/plataforma/principal_bruma.c $(BR_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(BR_FLAGS) -c -o $@ $<

# SaguiGL va compilado con SSE2 (SGL_FLAGS en el Makefile): el juego, con la x87, lo llama por la pila (cdecl).
$(B)/userland/bin/brumacanto: $(BR_OBJ) $(SDL2_OBJ) $(B)/userland/libsdl2/sdl2_rwops.o $(SDL2GL_OBJ) $(B)/userland/libsgl/sgl.o $(B)/userland/libsgl/sgl_fixo.o $(B)/userland/libsgl/sgl_egl.o $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
APPS_BINS += $(B)/userland/bin/brumacanto

# Los gráficos: solo el atlas ya montado (assets/atlas.rgba) y la licencia; los PNG de Kenney quedan en el árbol.
BR_RECURSOS := $(B)/brumacanto-res/.feito
$(BR_RECURSOS): $(BR_DIR)/datos/atlas.rgba $(BR_DIR)/LICENSE.txt $(BR_DIR)/LICENSE-kenney.txt $(BR_DIR)/ORIGEM.md $(BR_DIR)/datos/luanti/CREDITOS.md $(BR_DIR)/datos/luanti/LICENCIA-CC-BY-SA-3.0.txt
	rm -rf $(B)/brumacanto-res && mkdir -p $(B)/brumacanto-res
	cp $(BR_DIR)/datos/atlas.rgba $(B)/brumacanto-res/
	cp $(BR_DIR)/LICENSE.txt $(B)/brumacanto-res/LICENSE.txt
	cp $(BR_DIR)/LICENSE-kenney.txt $(B)/brumacanto-res/LICENSE-kenney.txt
	cp $(BR_DIR)/datos/luanti/CREDITOS.md $(B)/brumacanto-res/CREDITOS-luanti.md
	cp $(BR_DIR)/datos/luanti/LICENCIA-CC-BY-SA-3.0.txt $(B)/brumacanto-res/LICENCIA-CC-BY-SA-3.0.txt
	@touch $@
APPS_RECURSOS += brumacanto:$(B)/brumacanto-res
