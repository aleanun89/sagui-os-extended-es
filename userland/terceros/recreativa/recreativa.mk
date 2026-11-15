# SPDX-License-Identifier: GPL-2.0-or-later
# recreativa.mk - cómo se compila /apps/bin/recreativa (D294, D321). Lo incluye el Makefile principal
# fuera de la variante sin GPL.
#
# Recreativa (mejora 119) es el lanzador de juegos multisistema (DOS con DOSBox, NES, Master System,
# Game Gear, Game Boy...); código propio (GPL-2.0 o posterior), en C, con los componentes de la libsagui.
# Los sistemas y los emuladores se registran con ficheros (rootfs/etc/recreativa; docs/recreativa.md).
# Va a /apps/bin junto con los emuladores.
RC_DIR := userland/terceros/recreativa
RC_OBJ := $(B)/$(RC_DIR)/recreativa.o
$(RC_OBJ): $(RC_DIR)/recreativa.c $(USER_HDR) $(wildcard userland/inc/icones.h userland/inc/imagem.h)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/bin/recreativa: $(RC_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/recreativa
