# SPDX-License-Identifier: GPL-3.0-or-later
# aoen.mk - cómo se compilan /apps/bin/aoen y /apps/bin/aoen-config (D382, mejora 140).
# AoEN es un RTS estilo Age of Empires del mantenedor (C99 + SDL2, dibujo por software). Se compila
# con la libc común, la SDL 2 del sistema (userland/libsdl2) y la libsagui (de la que toma la fuente de 8x16 de la
# interfaz, AOEN_FONTE_SAGUI), con la x87 como el resto
# de los ports en C; las rutas rápidas SSE2 las elige el propio juego por CPUID (rapido.c).
# Los datos del juego original, las texturas y la música NO se distribuyen (docs/aoen.md): las
# texturas y la música, si existen en $(INPUT_SHARE)/aoen, se copian solo a las imágenes privadas.

AOEN_DIR := userland/juegos/aoen
AOEN_O   := $(B)/$(AOEN_DIR)
AOEN_SRC := $(filter-out $(AOEN_DIR)/aoen_config.c,$(wildcard $(AOEN_DIR)/*.c))
AOEN_CFG_SRC := $(AOEN_DIR)/aoen_config.c $(AOEN_DIR)/config.c
AOEN_FLAGS := -m32 -march=i686 -std=gnu99 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/terceros/stbimage -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -DSAGUI_AOEN -DAOEN_FONTE_SAGUI -DSTBI_NO_THREAD_LOCALS -finput-charset=UTF-8 -fexec-charset=UTF-8 -Wall -Wno-unused-function -Wno-unused-parameter

AOEN_HDR := $(wildcard $(AOEN_DIR)/*.h) userland/terceros/stbimage/stb_image.h $(AOEN_DIR)/aoen.mk
AOEN_OBJ := $(patsubst $(AOEN_DIR)/%.c,$(AOEN_O)/%.o,$(AOEN_SRC))
AOEN_CFG_OBJ := $(AOEN_O)/cfg_aoen_config.o $(AOEN_O)/cfg_config.o

$(AOEN_O)/%.o: $(AOEN_DIR)/%.c $(AOEN_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(AOEN_FLAGS) -c -o $@ $<
$(AOEN_O)/cfg_%.o: $(AOEN_DIR)/%.c $(AOEN_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(AOEN_FLAGS) -c -o $@ $<

$(B)/userland/bin/aoen: $(AOEN_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

$(B)/userland/bin/aoen-config: $(AOEN_CFG_OBJ) $(AOEN_O)/puente.o $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/aoen $(B)/userland/bin/aoen-config

# Texturas HD y música MIDI de terceros: solo si el mantenedor las tiene en $(INPUT_SHARE)/aoen
# (nunca en git ni en las imágenes públicas, que se hacen con INPUT_SHARE=). Van a /apps/aoen.
AOEN_TERCEROS := $(if $(and $(INPUT_SHARE),$(if $(filter 1,$(PUBLICABLE) $(SEM_GPL)),,x)),$(wildcard $(INPUT_SHARE)/aoen/texturas $(INPUT_SHARE)/aoen/musica))
$(shell mkdir -p $(B) && $(PY) tools/marca_carpeta.py $(B)/aoen-res.marca "INPUT_SHARE=$(INPUT_SHARE)" "$(INPUT_SHARE)/aoen")
AOEN_RES := $(B)/aoen-res/.feito
$(AOEN_RES): $(B)/aoen-res.marca $(AOEN_DIR)/aoen.mk
	rm -rf $(B)/aoen-res && mkdir -p $(B)/aoen-res
	@for d in $(AOEN_TERCEROS); do cp -r "$$d" $(B)/aoen-res/; done; \
	find $(B)/aoen-res -name '*:Zone.Identifier' -delete; \
	cp $(AOEN_DIR)/leeme.txt $(B)/aoen-res/leeme.txt
	@touch $@
APPS_RECURSOS += aoen:$(B)/aoen-res
APPS_RECURSOS_DEP += $(AOEN_RES)
