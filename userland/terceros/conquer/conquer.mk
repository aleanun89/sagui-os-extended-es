# conquer.mk - cómo se compila /apps/bin/conquer, Command & Conquer con Vanilla Conquer (D230).
# Lo incluye el Makefile principal fuera de la variante sin GPL (D109) y si
# hay compilador de C++ (userland/libcxx/libcxx.mk).
#
# Vanilla Conquer (https://github.com/TheAssemblyArmada/Vanilla-Conquer,
# GPL-3.0 con los términos adicionales de EA) es C++: el motor de Tiberian
# Dawn (jogo/tiberiandawn) y la biblioteca de Westwood (jogo/common) son el
# código del upstream, sin tocar (tools/conquer_fuentes.sh --comprobar). Lo
# nuestro: el runtime de C++ (userland/libcxx), la SDL 2 (userland/libsdl2),
# la OpenAL (userland/libopenal) y la plataforma (plataforma/: las rutas, la
# versión y el lanzador).
#
# Se compila como el «VanillaTD» del CMake del upstream con SDL2 y OpenAL:
# common (COMMON_SRC, con FIXIT_FAST_LOAD), commonv (COMMONV_SRC con
# video_sdl2, wwkeyboard_sdl2 y el sonido de OpenAL) y TIBDAWN_SRC (con
# MEGAMAPS), sin NETWORKING (sin juego en red) y sin el editor de mapas.
# Cambios de la lista del upstream: common/paths_posix.cpp (busca el
# ejecutable en /proc y guarda en $XDG_CONFIG_HOME) lo sustituye
# plataforma/paths_sagui.cpp, y el gitinfo.cpp que genera el CMake,
# plataforma/gitinfo_sagui.cpp.

CNC_DIR := userland/terceros/conquer
CNC_J   := $(CNC_DIR)/jogo
# «commit fecha revisión sha256» del tarball (tools/conquer_fuentes.sh).
CNC_VERSAO := $(shell cat $(CNC_J)/VERSAO 2>/dev/null)
CNC_COMMIT := $(word 1,$(CNC_VERSAO))
CNC_FECHA  := $(word 2,$(CNC_VERSAO))
CNC_REV    := $(word 3,$(CNC_VERSAO))

# Las listas de common/CMakeLists.txt y tiberiandawn/CMakeLists.txt.
CNC_COMMON := $(addprefix $(CNC_J)/common/, \
  _diptabl.cpp alloc.cpp auduncmp.cpp b64pipe.cpp b64straw.cpp base64.cpp bfiofile.cpp \
  blowfish.cpp blowpipe.cpp blwstraw.cpp buff.cpp buffer.cpp buffglbl.cpp ccfile.cpp cdfile.cpp \
  cliprect.cpp combuf.cpp control.cpp crc.cpp crew.cpp cstraw.cpp debugstring.cpp delay.cpp \
  dipthong.cpp drawbuff.cpp drawline.cpp drawmisc.cpp face.cpp fading.cpp field.cpp file.cpp \
  filepcx.cpp fixed.cpp font.cpp gadget.cpp getshape.cpp graphicsviewport.cpp hsv.cpp iff.cpp \
  ini.cpp int.cpp internet.cpp irandom.cpp keybuff.cpp keyframe.cpp lcw.cpp lcwpipe.cpp \
  lcwstraw.cpp linear.cpp link.cpp load.cpp memrev.cpp misc.cpp mixfile.cpp mp.cpp newdel.cpp \
  packet.cpp palette.cpp palettec.cpp paths.cpp pipe.cpp pk.cpp pkpipe.cpp pkstraw.cpp \
  ramfile.cpp random.cpp rawfile.cpp readline.cpp rect.cpp rgb.cpp rndstraw.cpp settings.cpp \
  sha.cpp shape.cpp shapipe.cpp shastraw.cpp soscodec.cpp stamp.cpp straw.cpp timer.cpp \
  timerdwn.cpp tobuff.cpp toggle.cpp vector.cpp winasm.cpp windows.cpp winhide.cpp winstub.cpp \
  wsa.cpp xordelta.cpp xpipe.cpp xstraw.cpp file_posix.cpp)
CNC_COMMONV := $(addprefix $(CNC_J)/common/, \
  framelimit.cpp gbuffer.cpp interpal.cpp unvqbuff.cpp vqaconfig.cpp vqadrawer.cpp vqaloader.cpp \
  vqapalette.cpp vqatask.cpp vqaver.cpp wwkeyboard.cpp wwmouse.cpp \
  soundio_common.cpp soundio_openal.cpp vqaaudio_openal.cpp video_sdl2.cpp wwkeyboard_sdl2.cpp)
CNC_TD := $(addprefix $(CNC_J)/tiberiandawn/, \
  aadata.cpp abstract.cpp adata.cpp aircraft.cpp anim.cpp audio.cpp base.cpp bbdata.cpp bdata.cpp \
  building.cpp bullet.cpp cargo.cpp ccini.cpp cdata.cpp cell.cpp checkbox.cpp cheklist.cpp \
  colrlist.cpp combat.cpp confdlg.cpp conquer.cpp const.cpp coord.cpp credits.cpp debug.cpp \
  dial8.cpp dialog.cpp display.cpp door.cpp drive.cpp drop.cpp edit.cpp ending.cpp event.cpp \
  expand.cpp facing.cpp factory.cpp findpath.cpp flasher.cpp fly.cpp foot.cpp fuse.cpp \
  gamedlg.cpp gauge.cpp globals.cpp goptions.cpp gscreen.cpp hdata.cpp heap.cpp help.cpp \
  house.cpp iconset.cpp idata.cpp infantry.cpp init.cpp intro.cpp iomap.cpp ioobj.cpp jshell.cpp \
  layer.cpp lib.cpp list.cpp loaddlg.cpp logic.cpp map.cpp mapeddlg.cpp mapedit.cpp mapedplc.cpp \
  mapedsel.cpp mapedtm.cpp mapsel.cpp menus.cpp miscasm.cpp mission.cpp monoc.cpp mouse.cpp \
  mplayer.cpp msgbox.cpp msglist.cpp nulldlg.cpp object.cpp odata.cpp options.cpp overlay.cpp \
  power.cpp profile.cpp queue.cpp radar.cpp radio.cpp reinf.cpp rules.cpp saveload.cpp \
  scenario.cpp scenarioini.cpp score.cpp scroll.cpp sdata.cpp shapebtn.cpp sidebar.cpp slider.cpp \
  smudge.cpp sounddlg.cpp special.cpp startup.cpp stats.cpp super.cpp tab.cpp tarcom.cpp \
  target.cpp tdata.cpp team.cpp teamtype.cpp techno.cpp template.cpp terrain.cpp textbtn.cpp \
  theme.cpp trigger.cpp turret.cpp txtlabel.cpp udata.cpp unit.cpp visudlg.cpp winstub.cpp)
CNC_PLAT := $(CNC_DIR)/plataforma/paths_sagui.cpp $(CNC_DIR)/plataforma/gitinfo_sagui.cpp

CNC_OBJ_COMMON := $(patsubst %.cpp,$(B)/%.o,$(CNC_COMMON))
CNC_OBJ_V      := $(patsubst %.cpp,$(B)/%.o,$(CNC_COMMONV) $(CNC_PLAT))
CNC_OBJ_TD     := $(patsubst %.cpp,$(B)/%.o,$(CNC_TD))
CNC_HDR := $(wildcard $(CNC_J)/common/*.h $(CNC_J)/tiberiandawn/*.h) \
  $(wildcard userland/libsdl2/*.h userland/libopenal/*.h)

# Las del CMake del upstream (Compilers.cmake: -fcheck-new, -fsigned-char,
# -fno-lifetime-dse) y las definiciones de VanillaTD con SDL2 y OpenAL.
CNC_DEFS := -DTRUE_FALSE_DEFINED -DENGLISH -DSDL_BUILD -DSDL2_BUILD -DNEW_VIDEO_BUILD -DOPENAL_BUILD -DNDEBUG \
  -DCNC_COMMIT='"$(CNC_COMMIT)"' -DCNC_FECHA='"$(CNC_FECHA)"' -DCNC_REVISAO=$(if $(CNC_REV),$(CNC_REV),0)
# Las carpetas de cabeceras, en el orden del CMake: tiberiandawn/ antes que
# common/ en el juego (las dos tienen un miscasm.h distinto).
CNC_FLAGS := $(CXXFLAGS_SAGUI) -fcheck-new -fsigned-char -fno-lifetime-dse \
  -Iuserland/libsdl2 -Iuserland/libopenal -I$(CNC_J) $(CNC_DEFS) -w

$(CNC_OBJ_COMMON): $(B)/%.o: %.cpp $(CNC_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(CNC_DIR)/conquer.mk
	@mkdir -p $(dir $@)
	$(CXX) $(CNC_FLAGS) -I$(CNC_J)/common -DFIXIT_FAST_LOAD -c -o $@ $<

$(CNC_OBJ_V): $(B)/%.o: %.cpp $(CNC_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(CNC_DIR)/conquer.mk
	@mkdir -p $(dir $@)
	$(CXX) $(CNC_FLAGS) -I$(CNC_J)/common -c -o $@ $<

$(CNC_OBJ_TD): $(B)/%.o: %.cpp $(CNC_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(CNC_DIR)/conquer.mk
	@mkdir -p $(dir $@)
	$(CXX) $(CNC_FLAGS) -I$(CNC_J)/tiberiandawn -I$(CNC_J)/common -DMEGAMAPS -c -o $@ $<

# La SDL 2, la OpenAL y el lanzador: C, con la x87 y los textos en Latin-1.
SDL2_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -Iuserland/libopenal -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
SDL2_OBJ := $(B)/userland/libsdl2/sdl2_sagui.o
OPENAL_OBJ := $(B)/userland/libopenal/openal_sagui.o
CNC_LANC_OBJ := $(B)/$(CNC_DIR)/plataforma/principal_cnc.o

$(SDL2_OBJ): userland/libsdl2/sdl2_sagui.c userland/libsdl2/sdl2gpu.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(OPENAL_OBJ): userland/libopenal/openal_sagui.c userland/libopenal/al.h userland/libopenal/alc.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(CNC_LANC_OBJ): $(CNC_DIR)/plataforma/principal_cnc.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

# Enlazado con programa_cxx.ld (las tablas de las excepciones) y la libm x87.
$(B)/userland/bin/conquer: $(CNC_OBJ_COMMON) $(CNC_OBJ_V) $(CNC_OBJ_TD) $(CNC_LANC_OBJ) $(SDL2_OBJ) $(OPENAL_OBJ) \
                           $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/conquer: la licencia (GPL-3.0 con los términos de EA) y el leeme de los datos.
CNC_RECURSOS := $(B)/conquer-res/.feito
$(CNC_RECURSOS): $(CNC_J)/License.txt $(CNC_J)/README.md $(CNC_DIR)/datos-leeme.txt
	@mkdir -p $(B)/conquer-res
	cp $(CNC_J)/License.txt $(B)/conquer-res/LICENCIA.txt
	cp $(CNC_J)/README.md $(B)/conquer-res/README.md
	cp $(CNC_DIR)/datos-leeme.txt $(B)/conquer-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/conquer.
APPS_BINS += $(B)/userland/bin/conquer
APPS_RECURSOS += conquer:$(B)/conquer-res

# ---- Red Alert (D230): solo si se bajaron sus fuentes ----
# Las fuentes de Red Alert (redalert/ del mismo tarball, 11 MB) no van en git:
# tools/conquer_fuentes.sh --redalert las copia a userland/terceros/conquer/redalert/
# (ignorada por git) y entonces se compila también /apps/bin/redalert, el
# «VanillaRA» del upstream, con los mismos common/ y commonv (los mismos
# objetos), la misma plataforma y el lanzador con -DSAGUI_RED_ALERT (datos en
# /jogos/RA, configuración en /home/redalert). Sin entrada en el menú Inicio
# mientras no haya datos de Red Alert con que probarlo.
CNC_RA := $(CNC_DIR)/redalert
ifneq ($(wildcard $(CNC_RA)/VERSAO),)
CNC_RA_SRC := $(addprefix $(CNC_RA)/, $(shell sed -n '/^set(REDALERT_SRC/,/^)/p' $(CNC_RA)/CMakeLists.txt | grep -v 'set(\|)'))
CNC_OBJ_RA := $(patsubst %.cpp,$(B)/%.o,$(CNC_RA_SRC))
CNC_RA_HDR := $(wildcard $(CNC_RA)/*.h)
# tiberiandawn/ antes que common/ (aquí redalert/: tiene su propio face.h).
$(CNC_OBJ_RA): $(B)/%.o: %.cpp $(CNC_HDR) $(CNC_RA_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(CNC_DIR)/conquer.mk
	@mkdir -p $(dir $@)
	$(CXX) $(CNC_FLAGS) -I$(CNC_RA) -I$(CNC_J)/common -c -o $@ $<

CNC_LANC_RA_OBJ := $(B)/$(CNC_DIR)/plataforma/principal_ra.o
$(CNC_LANC_RA_OBJ): $(CNC_DIR)/plataforma/principal_cnc.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -DSAGUI_RED_ALERT -c -o $@ $<

# common/face.cpp: Red Alert trae el suyo con las mismas funciones. En el CMake del
# upstream common es una biblioteca y gana el del juego; aquí se enlazan objetos
# sueltos, así que se quita el de common/ (si no, «multiple definition»). El
# winstub.cpp de los dos no se quita: no repiten funciones y el de common trae
# Load_Title_Screen, que Red Alert usa.
CNC_OBJ_COMMON_RA := $(filter-out %/common/face.o,$(CNC_OBJ_COMMON))
$(B)/userland/bin/redalert: $(CNC_OBJ_COMMON_RA) $(CNC_OBJ_V) $(CNC_OBJ_RA) $(CNC_LANC_RA_OBJ) $(SDL2_OBJ) $(OPENAL_OBJ) \
                            $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/redalert
endif
