# SPDX-License-Identifier: GPL-2.0-or-later
# chocolatedoom.mk - cómo se compilan /apps/bin/chocolate-doom (y heretic, hexen, strife) (D383).
# Lo incluye el Makefile principal fuera de la variante sin GPL (D109) y solo si hay fuentes
# preparadas (tools/doom_fuentes.sh: Chocolate Doom 3.1.1, GPL-2.0 o posterior).
#
# El motor es el código del upstream en fuentes/ (fuera de git) con parches/*.patch si hacen falta;
# lo nuestro está en plataforma/: config.h (lo que genera el CMake), el lanzador (principal_cd.c: busca
# los WAD, fija la configuración en ~/.config y el audio) y lo que falte en la SDL 2 del sistema
# (userland/libsdl2) se añade ahí, de forma reutilizable. Se compila en C con la x87, la libc común
# y la SDL 2 y SDL2_mixer del sistema. Sin red (sin SDL2_net: el juego en red no conecta).

CD_DIR := userland/terceros/chocolatedoom
CD_S   := $(CD_DIR)/fuentes
CD_PL  := $(CD_DIR)/plataforma

CD_CC := env -u CPATH -u C_INCLUDE_PATH $(CC)

# Las fuentes comunes (lista del CMake de src/, sin Windows, sin mmap, sin fluidsynth/libsamplerate).
CD_COMUN_NOMBRES := aes_prng d_event d_iwad d_loop d_mode deh_str gusconf i_cdmus i_endoom i_flmusic i_glob \
  i_input i_joystick i_musicpack i_oplmusic i_pcsound i_sdlmusic i_sdlsound i_sound i_timer i_video i_videohr \
  midifallback midifile mus2mid m_bbox m_cheat m_config m_controls m_fixed net_client net_common net_dedicated \
  net_gui net_io net_loop net_packet net_petname net_query net_sdl net_server net_structrw sha1 memio tables \
  v_diskicon v_video w_checksum w_main w_wad w_file w_file_stdc w_merge z_zone i_main i_system m_argv m_misc
CD_DEH_NOMBRES := deh_io deh_main deh_mapping deh_text
CD_COMUN := $(addprefix $(CD_S)/src/,$(addsuffix .c,$(CD_COMUN_NOMBRES) $(CD_DEH_NOMBRES)))
CD_OPL   := $(addprefix $(CD_S)/opl/,opl.c opl_queue.c opl_sdl.c opl_timer.c opl3.c)
CD_PCS   := $(addprefix $(CD_S)/pcsound/,pcsound.c pcsound_sdl.c)
CD_TXT   := $(wildcard $(CD_S)/textscreen/txt_*.c)

CD_BASE := -m32 -march=i686 -std=gnu99 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -I$(CD_PL)/inc -I$(LIBC_DIR) -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -Iuserland/inc -idirafter kernel/inc \
  -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -DDISABLE_SDL2NET -DHAVE_DIRENT_H -DSAGUI_DOOM -w
CD_INC := -I$(CD_S)/src -I$(CD_S)/opl -I$(CD_S)/pcsound -I$(CD_S)/textscreen

CD_O := $(B)/$(CD_DIR)
CD_HDR := $(LIBC_HDR) $(wildcard $(CD_PL)/inc/*.h) $(CD_DIR)/chocolatedoom.mk

$(CD_O)/obj/%.o: %.c $(CD_S)/.preparado $(CD_HDR)
	@mkdir -p $(dir $@)
	$(CD_CC) $(CD_BASE) $(CD_INC) -I$(dir $<) -c -o $@ $<

CD_OBJ_COMUN := $(addprefix $(CD_O)/obj/,$(CD_COMUN:.c=.o) $(CD_OPL:.c=.o) $(CD_PCS:.c=.o) $(CD_TXT:.c=.o))

include userland/libsdl2/sdl2_capas.mk

# El lanzador: uno por juego (CD_JUEGO: 0 DOOM, 1 Heretic, 2 Hexen, 3 Strife), con los textos en Latin-1.
define CD_LANZADOR
$$(CD_O)/principal_cd$(1).o: $$(CD_PL)/principal_cd.c $$(LIBC_HDR) $$(USER_HDR) $$(CD_HDR)
	@mkdir -p $$(dir $$@)
	$$(CD_CC) $$(CD_BASE) -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -DCD_JUEGO=$(1) -c -o $$@ $$<
endef
$(foreach n,0 1 2 3,$(eval $(call CD_LANZADOR,$(n))))

# Un ejecutable por juego: $(1) = carpeta de src/ ; $(2) = nombre en /apps/bin ; $(3) = número de CD_JUEGO
define CD_JUEGO
CD_OBJ_$(1) := $$(addprefix $$(CD_O)/obj/,$$(patsubst %.c,%.o,$$(wildcard $$(CD_S)/src/$(1)/*.c)))
$$(B)/userland/bin/$(2): $$(CD_OBJ_COMUN) $$(CD_OBJ_$(1)) $$(CD_O)/principal_cd$(3).o $$(SDL2_OBJ) $$(SDL2CAPAS_OBJ) $$(FONTES_OBJ) $$(LIBC_OBJ) $$(QJS_LIBM) $$(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $$(dir $$@)
	$$(LD) $$(ULDFLAGS) -o $$@.elf $$(filter %.o,$$^)
	objcopy --strip-all $$@.elf $$@
	@$$(PY) $$(CHECK_ELF32) $$@.elf
APPS_BINS += $$(B)/userland/bin/$(2)
endef
$(eval $(call CD_JUEGO,doom,chocolate-doom,0))
$(eval $(call CD_JUEGO,heretic,chocolate-heretic,1))
$(eval $(call CD_JUEGO,hexen,chocolate-hexen,2))
$(eval $(call CD_JUEGO,strife,chocolate-strife,3))

# ---- lo que va a /apps/doom: Freedoom (BSD-3-Clause, tools/doom_datos.sh) y las licencias; sin Freedoom, solo
# las licencias (el juego avisa de que falta el WAD). Los WAD originales NUNCA van aquí (solo input_share). ----
CD_RECURSOS := $(B)/doom-res/.feito
$(CD_RECURSOS): $(CD_S)/.preparado $(CD_DIR)/leeme.txt $(wildcard $(CD_DIR)/datos/.preparado) tools/doom_datos.sh
	rm -rf $(B)/doom-res
	mkdir -p $(B)/doom-res
	cp $(CD_S)/COPYING.md $(B)/doom-res/COPYING-chocolate-doom.md
	cp $(CD_DIR)/leeme.txt $(B)/doom-res/leeme.txt
	if [ -f $(CD_DIR)/datos/.preparado ]; then cp $(CD_DIR)/datos/freedoom1.wad $(CD_DIR)/datos/freedoom2.wad $(B)/doom-res/; \
	  cp $(CD_DIR)/datos/COPYING.txt $(B)/doom-res/COPYING-freedoom.txt; cp $(CD_DIR)/datos/CREDITS.txt $(B)/doom-res/CREDITS-freedoom.txt; cp $(CD_DIR)/datos/CREDITS-MUSIC.txt $(B)/doom-res/CREDITS-MUSIC-freedoom.txt; fi
	@touch $@
APPS_RECURSOS += doom:$(B)/doom-res
