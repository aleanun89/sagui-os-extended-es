# SPDX-License-Identifier: GPL-2.0-or-later
# laines.mk - cómo se compila /apps/bin/laines (D321). Lo incluye el Makefile principal si hay
# compilador de C++ (userland/libcxx/libcxx.mk).
#
# LaiNES (https://github.com/AndreaOrru/LaiNES, BSD-2-Clause; commit 1f3d126) es C++: jogo/src y
# jogo/lib son los del upstream con parches/sagui.patch (el APU a 48 kHz; tools/laines_fuentes.sh
# --comprobar). El APU es Nes_Snd_Emu de blargg (LGPL-2.1 o posterior). No se compilan su interfaz
# (gui.cpp, menu.cpp, config.cpp, main.cpp: SDL_ttf, SDL_image, menú) ni Sound_Queue.cpp: los
# reemplaza plataforma/frontal_nes.cpp sobre el marco común (userland/terceros/emucomun) y la SDL 2
# del sistema (userland/libsdl2).

LN_DIR := userland/terceros/laines
LN_J   := $(LN_DIR)/jogo

LN_SRC := $(addprefix $(LN_J)/, \
  src/apu.cpp src/cartridge.cpp src/cpu.cpp src/joypad.cpp src/mapper.cpp src/ppu.cpp src/savestate.cpp \
  src/mappers/mapper1.cpp src/mappers/mapper2.cpp src/mappers/mapper3.cpp src/mappers/mapper4.cpp \
  src/mappers/mapper5.cpp src/mappers/mapper7.cpp src/mappers/mapper9.cpp src/mappers/mapper10.cpp \
  src/mappers/mapper11.cpp src/mappers/mapper24.cpp src/mappers/mapper34.cpp src/mappers/mapper66.cpp \
  lib/Blip_Buffer.cpp lib/Multi_Buffer.cpp lib/Nes_Apu.cpp lib/Nes_Namco.cpp lib/Nes_Oscs.cpp \
  lib/Nes_Vrc6.cpp lib/Nonlinear_Buffer.cpp lib/apu_snapshot.cpp) $(LN_DIR)/plataforma/frontal_nes.cpp
LN_OBJ := $(patsubst %.cpp,$(B)/%.o,$(LN_SRC))
LN_HDR := $(shell find $(LN_J)/src $(LN_J)/lib -name '*.h*') $(LN_DIR)/../emucomun/marco.hpp userland/libsdl2/SDL.h

# -O2: el núcleo es de ciclo exacto y es lo que más pesa en los PC antiguos.
LN_FLAGS := $(subst -Os,-O2,$(CXXFLAGS_SAGUI)) -fomit-frame-pointer -Iuserland/libsdl2 \
  -I$(LN_J)/src/include -I$(LN_J)/lib/include -DNDEBUG -w

$(LN_OBJ): $(B)/%.o: %.cpp $(LN_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(LN_DIR)/laines.mk
	@mkdir -p $(dir $@)
	$(CXX) $(LN_FLAGS) -c -o $@ $<

# El punto de entrada común de los emuladores (C): principal() -> main().
ifndef EMU_PRINCIPAL_OBJ
EMU_PRINCIPAL_OBJ := $(B)/userland/terceros/emucomun/principal_emu.o
EMU_SDL2_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
$(EMU_PRINCIPAL_OBJ): userland/terceros/emucomun/principal_emu.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(EMU_SDL2_FLAGS) -c -o $@ $<
endif

# La SDL 2 común: la define conquer.mk / gnuboy.mk; si no hay ninguna, aquí.
ifeq ($(SDL2_OBJ),)
SDL2_OBJ := $(B)/userland/libsdl2/sdl2_sagui.o
$(SDL2_OBJ): userland/libsdl2/sdl2_sagui.c userland/libsdl2/sdl2gpu.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(EMU_SDL2_FLAGS) -c -o $@ $<
endif

$(B)/userland/bin/laines: $(LN_OBJ) $(EMU_PRINCIPAL_OBJ) $(SDL2_OBJ) $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

LN_RECURSOS := $(B)/laines-res/.feito
$(LN_RECURSOS): $(LN_J)/LICENSE $(LN_J)/README.md $(LN_DIR)/leeme.txt
	@mkdir -p $(B)/laines-res
	cp $(LN_J)/LICENSE $(B)/laines-res/LICENSE
	cp $(LN_J)/README.md $(B)/laines-res/README.md
	cp $(LN_DIR)/leeme.txt $(B)/laines-res/leeme.txt
	@touch $@

APPS_BINS += $(B)/userland/bin/laines
APPS_RECURSOS += laines:$(B)/laines-res
