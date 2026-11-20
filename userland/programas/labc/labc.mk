# SPDX-License-Identifier: GPL-3.0-only
# labc.mk - el Laboratorio de C (D414): /apps/bin/labc y /apps/labc/ejemplos. Lo incluye tcc.mk (necesita
# libtcc, así que solo existe con las fuentes de TCC). Reutiliza del Mico el editor (documento, realce,
# desenho y vista). LABC=0 lo desactiva.

LABC_DIR := userland/programas/labc
LABC ?= 1

ifeq ($(LABC),1)
LABC_MICO_OBJ := $(addprefix $(B)/userland/programas/mico/,documento.o realce.o desenho.o vista.o)
LABC_HDR := $(LABC_DIR)/compilador.h userland/programas/mico/mico.h $(USER_HDR)

$(B)/$(LABC_DIR)/principal.o: $(LABC_DIR)/principal.c $(LABC_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -Iuserland/programas/mico -c -o $@ $<
# libtcc y la libc: se compila como el propio TCC (sin sagui.h)
$(B)/$(LABC_DIR)/compilador.o: $(LABC_DIR)/compilador.c $(LABC_DIR)/compilador.h $(LIBC_HDR) $(TCC_DIR)/tcc.mk
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(TCC_FLAGS) -I$(TCC_F) -c -o $@ $<

$(B)/userland/bin/labc: $(B)/$(LABC_DIR)/principal.o $(B)/$(LABC_DIR)/compilador.o $(LABC_MICO_OBJ) \
                        $(TCC_B)/libtcc.o $(TCC_B)/tcc_libc_extra.o $(TCC_B)/tcc_simbolos.o \
                        $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

LABC_RAIZ := $(B)/labc-raiz
LABC_EJEMPLOS := $(wildcard $(LABC_DIR)/ejemplos/*.c)
$(LABC_RAIZ)/.feito: $(LABC_EJEMPLOS) $(LABC_DIR)/labc.mk
	rm -rf $(LABC_RAIZ) && mkdir -p $(LABC_RAIZ)/ejemplos
	cp $(LABC_EJEMPLOS) $(LABC_RAIZ)/ejemplos/
	@touch $@
APPS_RECURSOS += labc:$(LABC_RAIZ)
$(B)/apps/.feito: $(LABC_RAIZ)/.feito
APPS_BINS += $(B)/userland/bin/labc
endif
