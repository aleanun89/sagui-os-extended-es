# SPDX-License-Identifier: GPL-3.0-only
# ffmpeg.mk - FFmpeg 8.1.3 dentro de SaguiOS: /apps/bin/ffmpeg y /apps/bin/ffprobe (D458, docs/ffmpeg.md).
# Lo incluye el Makefile.
#
# FFmpeg (LGPL-2.1-or-later; sin --enable-gpl ni --enable-nonfree) viene de tools/ffmpeg_fuentes.sh (no va en git).
# Se configura con su propio ./configure, cruzado con la cadena de SaguiOS (i686, la libc común y la libsagui; sin
# ensamblador); el «ld» que ve el configure es plataforma/enlazar.sh. Luego se compila con el make de FFmpeg (fuera
# del árbol de fuentes, en $(B)/ffmpeg/obj) y se enlazan ffmpeg y ffprobe. FFMPEG=0 lo deja fuera.

FF_DIR := userland/terceros/ffmpeg
FF_F   := $(FF_DIR)/fuentes
FFMPEG ?= 1

ifeq ($(FFMPEG),1)
ifneq ($(wildcard $(FF_F)/.preparado),)
FF_B   := $(B)/ffmpeg
FF_ABS := $(abspath $(FF_B))
FF_SIS := $(FF_B)/libsistema.a
FF_CFLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra -fno-builtin -Werror=implicit-function-declaration -Iuserland/inc -idirafter kernel/inc -finput-charset=UTF-8 -fexec-charset=ISO-8859-1,$(subst -idirafter kernel/inc,,$(UCFLAGS))) \
  -ffreestanding -fbuiltin -mfpmath=387 -fexcess-precision=standard -O2 -fcommon -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -I$(abspath $(LIBC_DIR)) -D_GNU_SOURCE

$(FF_B)/principal_main.o: $(FF_DIR)/plataforma/principal_main.c
	@mkdir -p $(dir $@)
	$(CC) $(FF_CFLAGS) -c -o $@ $<

# la libc común y la libsagui juntas, en una biblioteca, para que el configure pueda enlazar sus pruebas
$(FF_SIS): $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@ && ar rcs $@ $^

# ./configure: la lista de lo que se activa está en docs/ffmpeg.md
FF_CONFIG_ARGS := $(shell grep -v '^\#' $(FF_DIR)/configurar.args | tr '\n' ' ')
$(FF_B)/obj/config.h: $(FF_DIR)/configurar.args $(FF_DIR)/ffmpeg.mk $(FF_DIR)/plataforma/enlazar.sh $(FF_SIS) $(FF_B)/principal_main.o
	rm -rf $(FF_B)/obj && mkdir -p $(FF_B)/obj
	cd $(FF_B)/obj && FF_SIS_LIB=$(FF_ABS)/libsistema.a FF_SIS_INICIO=$(abspath $(B))/userland/libsagui/inicio.asm.o \
	  FF_PRINCIPAL=$(FF_ABS)/principal_main.o FF_LDS=$(abspath userland/programa.ld) \
	  $(abspath $(FF_F))/configure $(FF_CONFIG_ARGS) --cc='$(CC)' --extra-cflags='$(FF_CFLAGS)' \
	  --ld=$(abspath $(FF_DIR)/plataforma/enlazar.sh) > configure.log 2>&1 || { tail -40 configure.log; tail -40 ffbuild/config.log; exit 1; }

# el make de FFmpeg compila las bibliotecas y enlaza ffmpeg_g y ffprobe_g (con enlazar.sh como «ld»)
FF_ENV := FF_SIS_LIB=$(FF_ABS)/libsistema.a FF_SIS_INICIO=$(abspath $(B))/userland/libsagui/inicio.asm.o \
  FF_PRINCIPAL=$(FF_ABS)/principal_main.o FF_LDS=$(abspath userland/programa.ld)
$(FF_B)/obj/.compilado: $(FF_B)/obj/config.h
	$(FF_ENV) $(MAKE) -C $(FF_B)/obj ffmpeg_g ffprobe_g
	@touch $@
$(B)/userland/bin/ffmpeg: $(FF_B)/obj/.compilado
	@mkdir -p $(dir $@)
	objcopy --strip-all $(FF_B)/obj/ffmpeg_g $@
	@$(PY) $(CHECK_ELF32) $(FF_B)/obj/ffmpeg_g
$(B)/userland/bin/ffprobe: $(FF_B)/obj/.compilado
	@mkdir -p $(dir $@)
	objcopy --strip-all $(FF_B)/obj/ffprobe_g $@
	@$(PY) $(CHECK_ELF32) $(FF_B)/obj/ffprobe_g
# las bibliotecas estáticas y las cabeceras públicas, para que TCC y otros programas usen libavcodec (tcc.mk las copia a /apps/tcc)
FF_DEV := $(FF_B)/dev
FF_DEV_OK := $(FF_DEV)/.feito
$(FF_DEV)/.feito: $(FF_B)/obj/.compilado
	rm -rf $(FF_DEV) && mkdir -p $(FF_DEV)
	$(FF_ENV) $(MAKE) -C $(FF_B)/obj install-headers install-libs DESTDIR=$(FF_ABS)/dev
	@touch $@
APPS_BINS += $(B)/userland/bin/ffmpeg $(B)/userland/bin/ffprobe
else
$(info Makefile: sin fuentes de FFmpeg: se salta ffmpeg de dentro (tools/ffmpeg_fuentes.sh, docs/ffmpeg.md))
endif
endif
