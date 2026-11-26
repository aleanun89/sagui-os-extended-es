# SPDX-License-Identifier: GPL-3.0-only
# make.mk - GNU make 4.4.1 dentro de SaguiOS: /apps/bin/make (D413, docs/make.md). Lo incluye el Makefile.
#
# GNU make (GPL-3.0-or-later) viene de tools/make_fuentes.sh (no va en git). Se compila con gcc contra la libc
# común, con la config.h de plataforma/ (fork/exec/waitpid reales, D401; sin posix_spawn) y un glob() mínimo
# (libc/glob.c). Ejecuta las recetas con $(SHELL), por defecto /bin/sh (sagsh). Sin fuentes se salta con
# un aviso. MAKE_DENTRO=0 lo desactiva.

MK_DIR := userland/terceros/make
MK_F   := $(MK_DIR)/fuentes
MAKE_DENTRO ?= 1

ifeq ($(MAKE_DENTRO),1)
ifneq ($(wildcard $(MK_F)/.preparado),)
MK_SRC := $(addprefix $(MK_F)/src/, ar.c arscan.c commands.c default.c dir.c expand.c file.c function.c getopt.c getopt1.c \
            hash.c implicit.c job.c load.c loadapi.c main.c misc.c output.c posixos.c read.c remake.c \
            remote-stub.c rule.c shuffle.c signame.c strcache.c variable.c version.c vpath.c) \
          $(MK_F)/lib/concat-filename.c $(MK_DIR)/plataforma/principal_main.c $(MK_DIR)/plataforma/compat.c
MK_OBJ := $(patsubst %.c,$(B)/%.o,$(MK_SRC))
MK_HDR := $(wildcard $(MK_F)/src/*.h $(MK_F)/lib/*.h $(MK_DIR)/plataforma/*.h)

MK_FLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -Os -w -fcommon -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) \
  -I$(MK_DIR)/plataforma -I$(MK_F)/src -I$(MK_F)/lib -include $(MK_DIR)/plataforma/make_sagui.h -DSAGUI_SIN_STRUCT_OPTION -Dgetopt=make_getopt -Dgetopt_long=make_getopt_long \
  -Dgetopt_long_only=make_getopt_long_only -Doptarg=make_optarg -Doptind=make_optind -Dopterr=make_opterr \
  -Doptopt=make_optopt -DHAVE_CONFIG_H -DPOSIX -D_GNU_SOURCE \
  -DLIBDIR='"/apps/lib"' -DINCLUDEDIR='"/apps/include"' -DLOCALEDIR='"/apps/share/locale"'

$(MK_OBJ): $(B)/%.o: %.c $(MK_HDR) $(LIBC_HDR) $(MK_DIR)/make.mk
	@mkdir -p $(dir $@)
	$(CC) $(MK_FLAGS) -c -o $@ $<

$(B)/userland/bin/make: $(MK_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# /apps/make: ejemplos, léeme y licencia (el árbol de /apps lo arma APPS_RECURSOS)
MK_RAIZ := $(B)/make-raiz
MK_EJEMPLOS := $(wildcard $(MK_DIR)/ejemplos/proyecto/*)
$(MK_RAIZ)/.feito: $(MK_EJEMPLOS) $(MK_DIR)/leeme.txt $(MK_F)/COPYING $(MK_DIR)/make.mk
	rm -rf $(MK_RAIZ) && mkdir -p $(MK_RAIZ)/ejemplos/proyecto
	cp $(MK_EJEMPLOS) $(MK_RAIZ)/ejemplos/proyecto/
	cp $(MK_DIR)/leeme.txt $(MK_RAIZ)/leeme.txt
	cp $(MK_F)/COPYING $(MK_RAIZ)/LICENCIA-MAKE.txt
	@touch $@
APPS_RECURSOS += make:$(MK_RAIZ)
$(B)/apps/.feito: $(MK_RAIZ)/.feito

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS), que está en el PATH.
APPS_BINS += $(B)/userland/bin/make
else
$(info Makefile: sin fuentes de GNU make: se salta make de dentro (tools/make_fuentes.sh, docs/make.md))
endif
endif
