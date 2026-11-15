# SPDX-License-Identifier: GPL-2.0-or-later
# gearsystem.mk - cómo se compila /apps/bin/gearsystem (D321). Lo incluye el Makefile principal fuera de
# la variante sin GPL (D109) y si hay compilador de C++.
#
# Gearsystem (https://github.com/drhelius/Gearsystem, 3.9.25, GPL-3.0) emula la Master System, la Game Gear y
# la SG-1000. jogo/src es el núcleo del upstream (sin la interfaz: platforms/ no se trae) con los parches
# de parches/sagui.patch (tools/gearsystem_fuentes.sh --comprobar). Lo nuestro: el marco común de
# emuladores (userland/terceros/emucomun), la SDL 2 del sistema (userland/libsdl2), plataforma/frontal_sms.cpp
# y un miniz.h vacío (sin ROM dentro de .zip).

GS_DIR := userland/terceros/gearsystem
GS_J   := $(GS_DIR)/jogo

GS_CPP := $(filter-out %/GearsystemCore.cpp.no, $(wildcard $(GS_J)/src/*.cpp)) $(wildcard $(GS_J)/src/audio/*.cpp) \
          $(GS_DIR)/plataforma/frontal_sms.cpp
GS_C   := $(GS_DIR)/plataforma/ym2413/ym2413_vacio.c
GS_OBJ := $(patsubst %.cpp,$(B)/%.o,$(GS_CPP)) $(patsubst %.c,$(B)/%.o,$(GS_C))
GS_HDR := $(shell find $(GS_J)/src -name '*.h') $(GS_DIR)/plataforma/miniz.h $(GS_DIR)/plataforma/ym2413/audio/emu2413/emu2413.h userland/terceros/emucomun/marco.hpp userland/libsdl2/SDL.h

GS_FLAGS := $(subst -Os,-O2,$(CXXFLAGS_SAGUI)) -fomit-frame-pointer -Iuserland/libsdl2 \
  -I$(GS_DIR)/plataforma -I$(GS_DIR)/plataforma/ym2413 -I$(GS_J)/src -I$(GS_J)/src/audio -DNDEBUG -DGS_DISABLE_DISASSEMBLER -w

$(patsubst %.cpp,$(B)/%.o,$(GS_CPP)): $(B)/%.o: %.cpp $(GS_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(GS_DIR)/gearsystem.mk
	@mkdir -p $(dir $@)
	$(CXX) $(GS_FLAGS) -c -o $@ $<

# El YM2413 vacío (FM de la Master System japonesa; ver plataforma/ym2413/audio/emu2413/emu2413.h) es C.
$(patsubst %.c,$(B)/%.o,$(GS_C)): $(B)/%.o: %.c $(GS_HDR) $(GS_DIR)/gearsystem.mk
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector \
	  -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -DNDEBUG -mfpmath=387 -fexcess-precision=standard \
	  -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
	  -I$(GS_DIR)/plataforma/ym2413 -w -c -o $@ $<

ifndef EMU_PRINCIPAL_OBJ
include userland/terceros/laines/laines.mk
endif

$(B)/userland/bin/gearsystem: $(GS_OBJ) $(EMU_PRINCIPAL_OBJ) $(SDL2_OBJ) $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

GS_RECURSOS := $(B)/gearsystem-res/.feito
$(GS_RECURSOS): $(GS_J)/LICENSE $(GS_J)/README.md $(GS_DIR)/leeme.txt
	@mkdir -p $(B)/gearsystem-res
	cp $(GS_J)/LICENSE $(B)/gearsystem-res/LICENSE
	cp $(GS_J)/README.md $(B)/gearsystem-res/README.md
	cp $(GS_DIR)/leeme.txt $(B)/gearsystem-res/leeme.txt
	@touch $@

APPS_BINS += $(B)/userland/bin/gearsystem
APPS_RECURSOS += gearsystem:$(B)/gearsystem-res
