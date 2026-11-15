# SPDX-License-Identifier: GPL-2.0-or-later
# gnuboy.mk - cómo se compila /apps/bin/gnuboy (D235). Lo incluye el Makefile
# principal fuera de la variante sin GPL (D109).
#
# SDL2-GNUBoy (https://github.com/AlexOberhofer/SDL2-GNUBoy, commit c8642d1,
# GPL-2.0) es C: el emulador (jogo/src), sus interfaces con SDL 2 (jogo/sys/sdl2)
# y sus bibliotecas (jogo/lib: inflate y el descompresor XZ, de dominio público)
# son el código del upstream, sin tocar (tools/gnuboy_fuentes.sh --comprobar).
# Lo nuestro: la SDL 2 del SaguiOS (userland/libsdl2, la misma de Command &
# Conquer, D230) y el lanzador (plataforma/principal_gb.c: elegir ROM, ayuda sin
# ROM, gnuboy.rc inicial). La lista es la del CMakeLists.txt del upstream; de
# la plataforma «nix» solo nix.c (reloj y rutas sobre SDL) y el enlace en serie
# «dummy» (sin pipes ni sockets: no hay cable de enlace).

GB_DIR := userland/terceros/gnuboy
GB_J   := $(GB_DIR)/jogo

GB_SRC := $(addprefix $(GB_J)/, \
  sys/sdl2/sdl-video.c sys/sdl2/sdl-audio.c sys/sdl2/sdl-input.c \
  src/lcd.c src/refresh.c src/lcdc.c src/palette.c src/cpu.c src/mem.c src/rtc.c src/hw.c \
  src/sound.c src/events.c src/keytable.c src/loader.c src/save.c src/debug.c src/io.c \
  src/emu.c src/main.c src/rccmds.c src/rckeys.c src/rcvars.c src/rcfile.c src/exports.c \
  src/split.c src/path.c lib/gz/inflate.c lib/xz/xz_crc32.c lib/xz/xz_crc64.c \
  lib/xz/xz_dec_lzma2.c lib/xz/xz_dec_stream.c lib/xz/xz_dec_bcj.c \
  sys/nix/nix.c sys/dummy/io.c)
GB_OBJ := $(patsubst %.c,$(B)/%.o,$(GB_SRC))
GB_HDR := $(wildcard $(GB_J)/include/*.h $(GB_J)/lib/xz/*.h $(GB_J)/lib/gz/*.h)

# La SDL 2 del SaguiOS (la define conquer.mk si Command & Conquer está; si no, aquí).
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

GB_FLAGS := $(SDL2_FLAGS) -fcommon -w -I$(GB_J)/include -I$(GB_J)/lib -I$(GB_J) \
  -DIS_LITTLE_ENDIAN -D_GNU_SOURCE=1 -D_REENTRANT

$(GB_OBJ): $(B)/%.o: %.c $(GB_HDR) $(LIBC_HDR) userland/libsdl2/SDL.h userland/libsdl2/SDL2/SDL.h $(GB_DIR)/gnuboy.mk
	@mkdir -p $(dir $@)
	$(CC) $(GB_FLAGS) -c -o $@ $<

GB_LANC_OBJ := $(B)/$(GB_DIR)/plataforma/principal_gb.o
$(GB_LANC_OBJ): $(GB_DIR)/plataforma/principal_gb.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/gnuboy: $(GB_OBJ) $(GB_LANC_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/gnuboy: la licencia, los créditos y el leeme de las ROM.
GB_RECURSOS := $(B)/gnuboy-res/.feito
$(GB_RECURSOS): $(GB_J)/COPYING $(GB_J)/CREDITS $(GB_J)/rc/gnuboy.rc.sample $(GB_DIR)/roms-leeme.txt
	@mkdir -p $(B)/gnuboy-res
	cp $(GB_J)/COPYING $(B)/gnuboy-res/COPYING
	cp $(GB_J)/CREDITS $(B)/gnuboy-res/CREDITS
	cp $(GB_J)/rc/gnuboy.rc.sample $(B)/gnuboy-res/gnuboy.rc.sample
	cp $(GB_DIR)/roms-leeme.txt $(B)/gnuboy-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/gnuboy.
APPS_BINS += $(B)/userland/bin/gnuboy
APPS_RECURSOS += gnuboy:$(B)/gnuboy-res
