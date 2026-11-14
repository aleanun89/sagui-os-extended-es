# openjazz.mk - cómo se compila /apps/bin/openjazz (D228). Lo incluye el
# Makefile principal fuera de la variante sin GPL (D109) y si hay compilador de
# C++ (userland/libcxx/libcxx.mk).
#
# OpenJazz 20260301 (https://github.com/AlisterT/openjazz, GPL-2.0 o
# posterior) es C++: el motor (jogo/src) y sus bibliotecas (jogo/ext: miniz,
# psmplug, argparse y stb_rect_pack, MIT o dominio público) son el código del
# upstream, sin tocar (tools/openjazz_fuentes.sh --comprobar). Lo nuestro: el
# runtime de C++ (userland/libcxx), la SDL 1.2 (userland/libsdl12, como la
# del OpenTyrian, D156) y el lanzador (plataforma/principal_oj.c).
#
# Se compila con la SDL 1.2 (lo que el upstream llama OJ_SDL1: ni OJ_SDL2 ni
# OJ_SDL3), sin red (sin USE_SOCKETS: el juego en red se queda en el menú pero
# no conecta), sin escalado propio (sin SCALE: escala la SDL del SaguiOS),
# PORTABLE (la configuración en la carpeta actual, /home/openjazz) y solo con
# el Jazz Jackrabbit 1 (sin ENABLE_JJ2, como el Makefile del upstream).

OJ_DIR  := userland/terceros/openjazz
OJ_J    := $(OJ_DIR)/jogo
OJ_VER  := 20260301

# La lista del Makefile del upstream, sin scale2x (solo con SCALE).
OJ_SRC := $(addprefix $(OJ_J)/, \
  ext/argparse/argparse.cpp ext/miniz/miniz.cpp ext/stb/stb_rect_pack.cpp \
  ext/psmplug/fastmix.cpp ext/psmplug/load_psm.cpp ext/psmplug/psmplug.cpp \
  ext/psmplug/snd_dsp.cpp ext/psmplug/snd_flt.cpp ext/psmplug/snd_fx.cpp \
  ext/psmplug/sndfile.cpp ext/psmplug/sndmix.cpp \
  src/game/clientgame.cpp src/game/game.cpp src/game/gamemode.cpp \
  src/game/localgame.cpp src/game/servergame.cpp \
  src/io/controls.cpp src/io/file.cpp src/io/log.cpp \
  src/io/gfx/anim.cpp src/io/gfx/font.cpp src/io/gfx/paletteeffects.cpp \
  src/io/gfx/sprite.cpp src/io/gfx/video.cpp src/io/network.cpp src/io/sound.cpp \
  src/level/level.cpp src/level/movable.cpp src/main.cpp \
  src/menu/filemenu.cpp src/menu/gamemenu.cpp src/menu/mainmenu.cpp \
  src/menu/menu.cpp src/menu/plasma.cpp src/menu/setupmenu.cpp \
  src/player/player.cpp src/platforms/platform_interface.cpp \
  src/setup.cpp src/util.cpp src/version.cpp \
  src/jj1/jj1episodeutils.cpp \
  src/jj1/bonuslevel/jj1bonuslevel.cpp src/jj1/bonuslevel/jj1bonuslevelplayer.cpp \
  src/jj1/level/event/jj1bridge.cpp src/jj1/level/event/jj1event.cpp \
  src/jj1/level/event/jj1guardians.cpp src/jj1/level/event/jj1standardevent.cpp \
  src/jj1/level/jj1bird.cpp src/jj1/level/jj1bullet.cpp src/jj1/level/jj1demolevel.cpp \
  src/jj1/level/jj1level.cpp src/jj1/level/jj1levelframe.cpp src/jj1/level/jj1levelload.cpp \
  src/jj1/level/jj1levelplayer.cpp src/jj1/level/jj1levelplayerframe.cpp \
  src/jj1/planet/jj1planet.cpp src/jj1/save/jj1save.cpp \
  src/jj1/scene/jj1scene.cpp src/jj1/scene/jj1sceneload.cpp)
OJ_OBJ := $(patsubst %.cpp,$(B)/%.o,$(OJ_SRC))
OJ_HDR := $(shell find $(OJ_J)/src $(OJ_J)/ext -name '*.h') userland/libsdl12/SDL.h userland/libsdl12/SDL_audio.h

OJ_FLAGS := $(CXXFLAGS_SAGUI) -Iuserland/libsdl12 \
  -I$(OJ_J)/src -I$(OJ_J)/ext/psmplug -I$(OJ_J)/ext/miniz -I$(OJ_J)/ext/argparse -I$(OJ_J)/ext/stb \
  -DPORTABLE -DNDEBUG -DOJ_VERSION='"$(OJ_VER)"' -DOJ_DATE='"2026-03-01"' -w

$(B)/$(OJ_J)/%.o: $(OJ_J)/%.cpp $(OJ_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(OJ_DIR)/openjazz.mk
	@mkdir -p $(dir $@)
	$(CXX) $(OJ_FLAGS) -c -o $@ $<

# La SDL 1.2 del SaguiOS y el lanzador: C, con la x87 y los textos en Latin-1.
SDL12_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl12 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
SDL12_OBJ := $(B)/userland/libsdl12/sdl_sagui.o
OJ_PLAT_OBJ := $(B)/$(OJ_DIR)/plataforma/principal_oj.o

$(SDL12_OBJ): userland/libsdl12/sdl_sagui.c userland/libsdl12/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL12_FLAGS) -c -o $@ $<

$(OJ_PLAT_OBJ): $(OJ_DIR)/plataforma/principal_oj.c userland/libsdl12/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL12_FLAGS) -c -o $@ $<

# Enlazado con programa_cxx.ld (las tablas de las excepciones) y la libm x87.
$(B)/userland/bin/openjazz: $(OJ_OBJ) $(OJ_PLAT_OBJ) $(SDL12_OBJ) $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/openjazz: las licencias (la del motor y las de sus bibliotecas).
OJ_RECURSOS := $(B)/openjazz-res/.feito
$(OJ_RECURSOS): $(OJ_J)/COPYING $(OJ_J)/licenses.txt $(OJ_DIR)/datos-leeme.txt
	@mkdir -p $(B)/openjazz-res
	cp $(OJ_J)/COPYING $(B)/openjazz-res/COPYING
	cp $(OJ_J)/licenses.txt $(B)/openjazz-res/licencias.txt
	cp $(OJ_DIR)/datos-leeme.txt $(B)/openjazz-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/openjazz.
APPS_BINS += $(B)/userland/bin/openjazz
APPS_RECURSOS += openjazz:$(B)/openjazz-res
