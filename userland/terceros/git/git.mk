# SPDX-License-Identifier: GPL-3.0-only
# git.mk - la orden git de SaguiOS (/apps/bin/git, D400, docs/git.md). Lo incluye el Makefile.
#
# libgit2 1.9.1 (GPL-2.0 con excepción de enlace) viene de tools/git_fuentes.sh (no va en git). Se
# compila con la libc común (sin su zlib.c: va la zlib de deps/zlib de libgit2), la x87 y la
# libsagui, sin hilos, con regex de la libc, SHA-1 con detección de colisiones, llhttp, xdiff y:
#   plataforma/proceso.c + ponte.c   git_process sobre executar()/tuberías (transporte SSH «exec»)
#   plataforma/flujo_sagui.c         flujos TCP y TLS (libtls) para HTTP(S)
#   plataforma/git2_features.h       la configuración
# git.c es la orden (init clone fetch pull push status add commit log diff branch checkout remote config).
# Sin fuentes se salta con un aviso. GIT=0 lo desactiva.

GIT_DIR_ := userland/terceros/git
GIT_F    := $(GIT_DIR_)/fuentes
GIT_PL   := $(GIT_DIR_)/plataforma
GIT_B    := $(B)/git
GIT ?= 1

ifeq ($(GIT),1)
ifneq ($(wildcard $(GIT_F)/.preparado),)

# Como el CMake de libgit2: todo src/libgit2, streams/ y transports/ (los de otras plataformas y
# bibliotecas están protegidos por #ifdef y quedan vacíos con git2_features.h).
GIT_SRC_LG := $(wildcard $(GIT_F)/src/libgit2/*.c $(GIT_F)/src/libgit2/streams/*.c $(GIT_F)/src/libgit2/transports/*.c)
GIT_SRC_UT := $(wildcard $(GIT_F)/src/util/*.c) $(filter-out %/win32_leakcheck.c,$(wildcard $(GIT_F)/src/util/allocators/*.c)) \
  $(GIT_F)/src/util/hash/builtin.c $(wildcard $(GIT_F)/src/util/hash/rfc6234/*.c) \
  $(GIT_F)/src/util/hash/collisiondetect.c $(wildcard $(GIT_F)/src/util/hash/sha1dc/*.c) \
  $(GIT_F)/src/util/unix/map.c $(GIT_F)/src/util/unix/realpath.c
GIT_SRC_DEP := $(wildcard $(GIT_F)/deps/xdiff/*.c) $(wildcard $(GIT_F)/deps/llhttp/*.c) \
  $(filter-out %/gzclose.c,$(wildcard $(GIT_F)/deps/zlib/*.c))
GIT_SRC := $(GIT_SRC_LG) $(GIT_SRC_UT) $(GIT_SRC_DEP)
GIT_PLAT := $(GIT_PL)/proceso.c $(GIT_PL)/flujo_sagui.c $(GIT_PL)/ponte.c

GIT_CC := env -u CPATH -u C_INCLUDE_PATH $(CC)
GIT_BASE := -m32 -march=i686 -std=gnu99 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -I$(GIT_PL) -I$(GIT_F)/src/libgit2 -I$(GIT_F)/src/util -I$(GIT_F)/include -I$(GIT_F)/deps/llhttp \
  -I$(GIT_F)/deps/xdiff -I$(GIT_F)/deps/zlib -I$(LIBC_DIR) -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/inc -idirafter kernel/inc \
  -D_GNU_SOURCE -DGIT_DEPRECATE_HARD -DLLHTTP_STRICT_MODE=0 -DNO_VIZ -DSTDC -DNO_GZIP \
  -DSHA1DC_NO_STANDARD_INCLUDES=1 '-DSHA1DC_CUSTOM_INCLUDE_SHA1_C="git2_util.h"' \
  '-DSHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C="git2_util.h"' -w

GIT_HDR := $(LIBC_HDR) $(wildcard $(GIT_PL)/*.h) $(GIT_DIR_)/git.mk

# git.c es nuestro: sus textos (UTF-8 en el fuente) salen en Latin-1, como los demás programas de SaguiOS.
$(GIT_B)/obj/$(GIT_DIR_)/git.o: $(GIT_DIR_)/git.c $(GIT_F)/.preparado $(GIT_HDR)
	@mkdir -p $(dir $@)
	$(GIT_CC) $(GIT_BASE) -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -c -o $@ $<

$(GIT_B)/obj/%.o: %.c $(GIT_F)/.preparado $(GIT_HDR)
	@mkdir -p $(dir $@)
	$(GIT_CC) $(GIT_BASE) -c -o $@ $<

GIT_OBJ := $(addprefix $(GIT_B)/obj/,$(GIT_SRC:.c=.o) $(GIT_PLAT:.c=.o) $(GIT_DIR_)/git.o)
GIT_LIBC_OBJ := $(filter-out %/libc/zlib.o,$(LIBC_OBJ))

$(B)/userland/bin/git: $(GIT_OBJ) $(GIT_LIBC_OBJ) $(QJS_LIBM) $(TLS_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/git
else
$(info Makefile: sin fuentes de libgit2: se salta la orden git (tools/git_fuentes.sh, docs/git.md))
endif
endif
