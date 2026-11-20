# SPDX-License-Identifier: GPL-3.0-or-later
# tzar.mk - cómo se compila /apps/bin/tzar, el visor de mapas de Tzar (D417, docs/tzar.md).
# Lectores traducidos a C de ReTzar (GPL-3.0-or-later) y visor propio, con la libc común, la SDL 2 del sistema
# (userland/libsdl2) y la libsagui (registro y fuente de 8x16). Los datos del juego NO se distribuyen: el usuario
# pone su instalación en /jogos/TZAR (en las imágenes privadas, input_share/games/TZAR).

TZAR_DIR := userland/juegos/tzar
TZAR_O   := $(B)/$(TZAR_DIR)
TZAR_SRC := $(wildcard $(TZAR_DIR)/*.c) $(wildcard $(TZAR_DIR)/ia/*.c)
TZAR_FLAGS := -m32 -march=i686 -std=gnu99 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -DSAGUI_TZAR -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-format-truncation

TZAR_HDR := $(wildcard $(TZAR_DIR)/*.h) $(wildcard $(TZAR_DIR)/ia/*.h) $(TZAR_DIR)/tzar.mk
TZAR_OBJ := $(patsubst $(TZAR_DIR)/%.c,$(TZAR_O)/%.o,$(TZAR_SRC))

$(TZAR_O)/%.o: $(TZAR_DIR)/%.c $(TZAR_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(TZAR_FLAGS) -c -o $@ $<

$(B)/userland/bin/tzar: $(TZAR_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/tzar
