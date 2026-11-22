# SPDX-License-Identifier: MIT
# capibara.mk - cómo se compila /apps/bin/capibara (D452, docs/capibara.md): motor de juegos 2D en C11 con guiones en Lua 5.4
# sobre la SDL 2 de SaguiOS (userland/libsdl2) y la Lua de userland/terceros/lua (liblua.a, D440). Código MIT; gráficos de
# los ejemplos CC0 de Kenney. Entra también en la variante sin GPL. CAPIBARA=0 lo deja fuera; sin las fuentes de Lua
# (tools/lua_fuentes.sh) se salta con un aviso. Para Linux, el Makefile de userland/capibara.

CAPI_DIR := userland/capibara
CAPI_S   := $(CAPI_DIR)/motor
CAPI_O   := $(B)/$(CAPI_DIR)
CAPI_MOD := nodo texto props recursos escena entrada fisica guion audio dibujo mundo escritor dibujo_sdl plataforma_sdl
CAPIBARA ?= 1

ifeq ($(CAPIBARA),1)
ifneq ($(wildcard $(LUA_F)/.preparado),)
CAPI_FLAGS := -m32 -march=i686 -std=gnu11 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc -I$(LUA_F) -I$(CAPI_S) \
  -DCAPI_SAGUI -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wno-unused-parameter -Wno-format-truncation
CAPI_HDR := $(wildcard $(CAPI_S)/*.h) $(CAPI_DIR)/capibara.mk
CAPI_OBJ := $(addprefix $(CAPI_O)/,$(addsuffix .o,$(CAPI_MOD))) $(CAPI_O)/principal_sagui.o

include userland/libsdl2/sdl2_capas.mk

$(CAPI_O)/%.o: $(CAPI_S)/%.c $(CAPI_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(CAPI_FLAGS) -c -o $@ $<
$(CAPI_O)/principal_sagui.o: $(CAPI_DIR)/plataforma/principal_sagui.c $(CAPI_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(CAPI_FLAGS) -c -o $@ $<

$(B)/userland/bin/capibara: $(CAPI_OBJ) $(SDL2_OBJ) $(B)/userland/libsdl2/sdl2_image.o $(B)/userland/libsdl2/sdl2_rwops.o $(LUA_A) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(LUA_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
APPS_BINS += $(B)/userland/bin/capibara

# El editor (D460, /apps/bin/capibara-editor): programa gráfico de la libsagui con libc y Lua que enlaza el núcleo del motor (sin la capa
# de la SDL de ventana) y un rasterizador propio para el lienzo. Los textos de su interfaz salen en Latin-1 como los del resto de programas.
CAPI_ED    := $(CAPI_DIR)/editor
CAPI_EDFLAGS := $(subst -fexec-charset=UTF-8,-fexec-charset=ISO-8859-1,$(CAPI_FLAGS)) -I$(CAPI_ED) -Iuserland/inc
CAPI_NUCLEO := nodo texto props recursos escena entrada fisica guion audio dibujo mundo escritor
CAPI_ED_OBJ := $(addprefix $(CAPI_O)/,$(addsuffix .o,$(CAPI_NUCLEO))) $(CAPI_O)/editor/editor.o $(CAPI_O)/editor/raster.o $(CAPI_O)/editor/imagen_sdl.o $(CAPI_O)/editor/principal_editor.o
$(CAPI_O)/editor/editor.o: $(CAPI_ED)/editor.c $(CAPI_ED)/raster.h $(CAPI_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(CAPI_EDFLAGS) -c -o $@ $<
$(CAPI_O)/editor/raster.o $(CAPI_O)/editor/imagen_sdl.o: $(CAPI_O)/editor/%.o: $(CAPI_ED)/%.c $(CAPI_ED)/raster.h $(CAPI_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(CAPI_FLAGS) -I$(CAPI_ED) -c -o $@ $<
$(CAPI_O)/editor/principal_editor.o: $(CAPI_DIR)/plataforma/principal_editor.c $(CAPI_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(CAPI_FLAGS) -c -o $@ $<
$(B)/userland/bin/capibara-editor: $(CAPI_ED_OBJ) $(SDL2_OBJ) $(B)/userland/libsdl2/sdl2_image.o $(B)/userland/libsdl2/sdl2_rwops.o $(LUA_A) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(LUA_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
APPS_BINS += $(B)/userland/bin/capibara-editor

# /apps/capibara: los ejemplos (proyectos completos), la licencia y un léeme
CAPI_EJEMPLOS := $(shell find $(CAPI_DIR)/ejemplos -type f 2>/dev/null)
CAPI_RECURSOS := $(B)/capibara-res/.feito
$(CAPI_RECURSOS): $(CAPI_EJEMPLOS) $(CAPI_DIR)/LICENSE $(CAPI_DIR)/LEEME.md $(CAPI_DIR)/capibara.mk
	rm -rf $(B)/capibara-res && mkdir -p $(B)/capibara-res
	cp -r $(CAPI_DIR)/ejemplos/. $(B)/capibara-res/
	cp $(CAPI_DIR)/LICENSE $(B)/capibara-res/LICENSE
	cp $(CAPI_DIR)/LEEME.md $(B)/capibara-res/LEEME.md
	@touch $@
APPS_RECURSOS += capibara:$(B)/capibara-res
$(B)/apps/.feito: $(CAPI_RECURSOS)
else
$(info Makefile: sin fuentes de Lua: se salta Capibara (tools/lua_fuentes.sh))
endif
endif
