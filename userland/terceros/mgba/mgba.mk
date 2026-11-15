# SPDX-License-Identifier: GPL-2.0-or-later
# mgba.mk - cómo se compila /apps/bin/mgba (D322). Lo incluye el Makefile principal fuera de
# la variante sin GPL (D109), como DOSBox y GNUBoy.
#
# mGBA 0.10.5 (https://mgba.io, MPL-2.0; en la licencia del proyecto, GPL v3, entra por la
# «Secondary License» de la MPL, sección 3.3: docs/terceros.md) es C: jogo/ es el árbol del
# upstream reducido al núcleo (tools/mgba_fuentes.sh --comprobar). Lo nuestro: la SDL 2 del
# SaguiOS (userland/libsdl2) y el front-end propio (plataforma/principal_mgba.c) en lugar del de
# Qt/SDL de upstream; las rutas del sistema (plataforma/memoria_mgba.c, sin mmap); y la versión
# fijada (plataforma/version.c, lo que genera CMake). Sin hilos (DISABLE_THREADING), sin
# depurador, sin scripts, sin zlib/libpng/lzma/sqlite/libzip/ffmpeg: ninguna dependencia opcional.

MG_DIR := userland/terceros/mgba
MG_J   := $(MG_DIR)/jogo

MG_SRC := $(addprefix $(MG_J)/src/, \
  arm/arm.c arm/decoder-arm.c arm/decoder-thumb.c arm/decoder.c arm/isa-arm.c arm/isa-thumb.c \
  sm83/decoder.c sm83/isa-sm83.c sm83/sm83.c \
  core/bitmap-cache.c core/cache-set.c core/cheats.c core/config.c core/core.c \
  core/directories.c core/input.c core/interface.c core/library.c core/log.c core/map-cache.c \
  core/mem-search.c core/rewind.c core/serialize.c core/sync.c core/tile-cache.c core/timing.c \
  gb/audio.c gb/cheats.c gb/core.c gb/gb.c gb/input.c gb/io.c gb/mbc.c gb/memory.c \
  gb/overrides.c gb/serialize.c gb/renderers/cache-set.c gb/renderers/software.c gb/sio.c \
  gb/timer.c gb/video.c gb/sio/printer.c gb/extra/proxy.c gba/extra/proxy.c gba/extra/audio-mixer.c \
  feature/video-logger.c \
  gba/audio.c gba/bios.c gba/cart/ereader.c gba/cart/gpio.c gba/cart/matrix.c \
  gba/cart/vfame.c gba/cheats.c gba/cheats/codebreaker.c gba/cheats/gameshark.c \
  gba/cheats/parv3.c gba/core.c gba/dma.c gba/gba.c gba/hle-bios.c gba/input.c gba/io.c \
  gba/memory.c gba/overrides.c gba/renderers/cache-set.c gba/renderers/common.c \
  gba/renderers/software-bg.c gba/renderers/software-mode0.c gba/renderers/software-obj.c \
  gba/renderers/video-software.c gba/savedata.c gba/serialize.c gba/sharkport.c gba/sio.c \
  gba/sio/gbp.c gba/sio/joybus.c gba/timer.c gba/video.c \
  util/circle-buffer.c util/configuration.c util/convolve.c util/crc32.c util/export.c \
  util/formatting.c util/gbk-table.c util/hash.c util/patch.c util/patch-fast.c \
  util/patch-ips.c util/patch-ups.c util/ring-fifo.c util/string.c util/table.c \
  util/text-codec.c util/vfs.c util/vfs/vfs-file.c util/vfs/vfs-mem.c \
  third-party/blip_buf/blip_buf.c third-party/inih/ini.c)
MG_OBJ := $(patsubst %.c,$(B)/%.o,$(MG_SRC))
MG_HDR := $(shell find $(MG_J)/include $(MG_J)/src -name '*.h')

# La SDL 2 del SaguiOS (la define conquer.mk o gnuboy.mk si están; si no, aquí).
ifndef SDL2_OBJ
SDL2_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
SDL2_OBJ := $(B)/userland/libsdl2/sdl2_sagui.o
$(SDL2_OBJ): userland/libsdl2/sdl2_sagui.c userland/libsdl2/sdl2gpu.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<
endif

# -O2 (el núcleo es lo que más pesa; el resto del sistema va en -Os), sin marco de pila.
MG_DEFS := -DM_CORE_GBA -DM_CORE_GB -DDISABLE_THREADING \
  -DHAVE_STRDUP -DHAVE_LOCALTIME_R -DHAVE_STRLCPY -DHAVE_STRNDUP -DUSE_VFS_FILE -DBINARY_NAME=\"mgba\" -DMGBA_STANDALONE
MG_FLAGS := $(subst -Os,-O2,$(SDL2_FLAGS)) -fomit-frame-pointer -fno-strict-aliasing -w \
  -I$(MG_J)/include -I$(MG_J)/src -I$(MG_DIR)/plataforma -include compat_mgba.h $(MG_DEFS) \
  -finput-charset=UTF-8 -fexec-charset=UTF-8

$(MG_OBJ): $(B)/%.o: %.c $(MG_HDR) $(LIBC_HDR) $(MG_DIR)/mgba.mk
	@mkdir -p $(dir $@)
	$(CC) $(MG_FLAGS) -c -o $@ $<

MG_PLAT_SRC := $(addprefix $(MG_DIR)/plataforma/, version.c memoria_mgba.c principal_mgba.c)
MG_PLAT_OBJ := $(patsubst %.c,$(B)/%.o,$(MG_PLAT_SRC))
$(MG_PLAT_OBJ): $(B)/%.o: %.c $(MG_HDR) userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR) $(MG_DIR)/mgba.mk
	@mkdir -p $(dir $@)
	$(CC) $(MG_FLAGS) -c -o $@ $<

$(B)/userland/bin/mgba: $(MG_OBJ) $(MG_PLAT_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/mgba: la licencia MPL-2.0, la de blip_buf (LGPL-2.1) y la de inih (BSD), y el leeme.
MG_RECURSOS := $(B)/mgba-res/.feito
$(MG_RECURSOS): $(MG_J)/LICENSE $(MG_J)/src/third-party/blip_buf/license.txt \
                $(MG_J)/src/third-party/inih/LICENSE.txt $(MG_DIR)/leeme.txt
	@mkdir -p $(B)/mgba-res
	cp $(MG_J)/LICENSE $(B)/mgba-res/LICENSE
	cp $(MG_J)/src/third-party/blip_buf/license.txt $(B)/mgba-res/LICENSE-blip_buf
	cp $(MG_J)/src/third-party/inih/LICENSE.txt $(B)/mgba-res/LICENSE-inih
	cp $(MG_DIR)/leeme.txt $(B)/mgba-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/mgba.
APPS_BINS += $(B)/userland/bin/mgba
APPS_RECURSOS += mgba:$(B)/mgba-res
