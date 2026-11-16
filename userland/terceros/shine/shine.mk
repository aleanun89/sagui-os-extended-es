# SPDX-License-Identifier: GPL-2.0-or-later
# shine.mk - cómo se compila /apps/bin/convertir-audio con el codificador MP3 shine (D339). Lo incluye el Makefile
# principal fuera de la variante sin GPL (D109). Sin esto, convertir-audio se compila igual (regla por defecto) pero sin MP3.
#
# shine (LGPL-2.0+, userland/terceros/shine/src, sin modificar) es C con doubles y libm solo al iniciar: se compila con la x87
# y la libc común (como mgba), y solo lo enlaza este programa. El programa define CONVERSOR_SHINE.

SHINE_DIR := userland/terceros/shine
SHINE_SRC := $(addprefix $(SHINE_DIR)/src/, bitstream.c huffman.c l3bitstream.c l3loop.c l3mdct.c l3subband.c layer3.c reservoir.c tables.c)
SHINE_OBJ := $(patsubst %.c,$(B)/%.o,$(SHINE_SRC))
SHINE_HDR := $(wildcard $(SHINE_DIR)/src/*.h)

SHINE_FLAGS := -m32 -march=i686 -std=gnu11 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -I$(SHINE_DIR)/src -w -finput-charset=UTF-8 -fexec-charset=UTF-8

$(SHINE_OBJ): $(B)/%.o: %.c $(SHINE_HDR) $(LIBC_HDR) $(SHINE_DIR)/shine.mk
	@mkdir -p $(dir $@)
	$(CC) $(SHINE_FLAGS) -c -o $@ $<

# El programa en sí va con las banderas de siempre de los programas (sin FPU: no usa doubles) y la cabecera de shine.
$(B)/userland/programas/convertir-audio.o: UCFLAGS := $(UCFLAGS) -I$(SHINE_DIR)/src -DCONVERSOR_SHINE

$(B)/userland/bin/convertir-audio: $(B)/userland/programas/convertir-audio.o $(SHINE_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# La licencia (LGPL-2.0+) y el aviso van a /apps/shine.
SHINE_RECURSOS := $(B)/shine-res/.feito
$(SHINE_RECURSOS): $(SHINE_DIR)/COPYING $(SHINE_DIR)/ORIGEM.md
	@mkdir -p $(B)/shine-res
	cp $(SHINE_DIR)/COPYING $(B)/shine-res/COPYING
	cp $(SHINE_DIR)/ORIGEM.md $(B)/shine-res/leeme.txt
	@touch $@
APPS_RECURSOS += shine:$(B)/shine-res
