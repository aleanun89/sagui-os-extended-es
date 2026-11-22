# SPDX-License-Identifier: GPL-3.0-only
# awk.mk - The One True AWK 20261106 dentro de SaguiOS: /bin/awk (D440, docs/awk-lua.md). Lo incluye el Makefile.
#
# El awk de Brian Kernighan (licencia permisiva de Lucent, ver ORIGEM.md) viene de tools/awk_fuentes.sh (no va
# en git). La gramática ya generada (awkgram.tab.c y .h, de bison) va en git, en gerado/; proctab.c lo genera en
# el PC el programa maketab (compilado con HOSTCC) a partir de awkgram.tab.h. Se compila con gcc contra la libc
# común y la x87 (hay dobles). Sin fuentes se salta con un aviso. AWK_DENTRO=0 lo desactiva.

AWK_DIR := userland/terceros/awk
AWK_F   := $(AWK_DIR)/fuentes
AWK_G   := $(AWK_DIR)/gerado
AWK_DENTRO ?= 1

ifeq ($(AWK_DENTRO),1)
ifneq ($(wildcard $(AWK_F)/.preparado),)
AWK_SRC := $(addprefix $(AWK_F)/, b.c lex.c lib.c main.c parse.c run.c tran.c math.c) $(AWK_G)/awkgram.tab.c \
           $(AWK_DIR)/plataforma/awk_compat.c $(AWK_DIR)/plataforma/principal_main.c
AWK_OBJ := $(patsubst %.c,$(B)/%.o,$(AWK_SRC)) $(B)/$(AWK_DIR)/proctab.o
AWK_HDR := $(wildcard $(AWK_F)/*.h $(AWK_G)/*.h $(AWK_DIR)/plataforma/*.h)

AWK_FLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -Os -w -fcommon -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) \
  -I$(AWK_DIR)/plataforma -I$(AWK_G) -I$(AWK_F) -include $(AWK_DIR)/plataforma/awk_sagui.h

# maketab (en el PC) saca de la gramática la tabla de órdenes proctab.c
$(B)/$(AWK_DIR)/maketab: $(AWK_F)/maketab.c $(AWK_G)/awkgram.tab.h $(AWK_F)/awk.h
	@mkdir -p $(dir $@)
	CPATH="$(HOST_CPATH)" $(HOSTCC) -O1 -w -I$(AWK_G) -I$(AWK_F) -o $@ $<
$(B)/$(AWK_DIR)/proctab.c: $(B)/$(AWK_DIR)/maketab
	$< $(AWK_G)/awkgram.tab.h > $@
$(B)/$(AWK_DIR)/proctab.o: $(B)/$(AWK_DIR)/proctab.c $(AWK_HDR) $(LIBC_HDR) $(AWK_DIR)/awk.mk
	$(CC) $(AWK_FLAGS) -c -o $@ $<

$(filter-out $(B)/$(AWK_DIR)/proctab.o,$(AWK_OBJ)): $(B)/%.o: %.c $(AWK_HDR) $(LIBC_HDR) $(AWK_DIR)/awk.mk
	@mkdir -p $(dir $@)
	$(CC) $(AWK_FLAGS) -c -o $@ $<

$(B)/userland/bin/awk: $(AWK_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# /apps/awk: la licencia y los ejemplos de uso (el árbol de /apps lo arma APPS_RECURSOS)
AWK_RAIZ := $(B)/awk-raiz
$(AWK_RAIZ)/.feito: $(AWK_F)/LICENSE $(AWK_F)/README.md $(AWK_F)/awk.1 $(AWK_DIR)/awk.mk
	rm -rf $(AWK_RAIZ) && mkdir -p $(AWK_RAIZ)
	cp $(AWK_F)/LICENSE $(AWK_RAIZ)/LICENCIA-AWK.txt
	cp $(AWK_F)/README.md $(AWK_RAIZ)/LEEME-AWK.md
	cp $(AWK_F)/awk.1 $(AWK_RAIZ)/awk.1
	@touch $@
APPS_RECURSOS += awk:$(AWK_RAIZ)
$(B)/apps/.feito: $(AWK_RAIZ)/.feito

# /bin/awk: herramienta básica de los guiones (el ramdisk la lleva con el resto de /bin).
PROGRAMAS += $(B)/userland/bin/awk
else
$(info Makefile: sin fuentes de awk: se salta awk de dentro (tools/awk_fuentes.sh, docs/awk-lua.md))
endif
endif
