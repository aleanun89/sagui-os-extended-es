# dosbox.mk - cómo se compila /apps/bin/dosbox (D294). Lo incluye el Makefile principal fuera de
# la variante sin GPL (D109) y si hay compilador de C++ (userland/libcxx/libcxx.mk).
#
# DOSBox 0.74-3 (https://www.dosbox.com, GPL-2.0 o posterior; el clásico, no Staging ni DOSBox-X)
# es C++: jogo/src es el del upstream con los parches de parches/sagui.patch (las rutas del
# SaguiOS, sin exec; tools/dosbox_fuentes.sh --comprobar). Lo nuestro: el runtime de C++ con RTTI
# (userland/libcxx: dynamic_cast, typeid, <map>, flujos), la SDL 1.2 (userland/libsdl12, la de
# OpenJazz y Quake), el config.h de plataforma/ y el lanzador (plataforma/principal_dosbox.c).
#
# Núcleos de CPU: el normal (intérprete) y el recompilador dinámico de x86 (C_DYNAMIC_X86,
# core=dynamic; core=auto lo usa por defecto en modo real y cambia al normal en modo protegido).
# La FPU en ensamblador x87 (C_FPU_X86). Sin OpenGL, sin depurador, sin captura de pantalla
# (libpng), sin módem ni IPX (sin SDL_net), sin serie real.

DB_DIR  := userland/terceros/dosbox
DB_J    := $(DB_DIR)/jogo
DB_VER  := 0.74-3

DB_SRC := $(addprefix $(DB_J)/src/, \
  cpu/callback.cpp cpu/core_dyn_x86.cpp cpu/core_full.cpp cpu/core_normal.cpp \
  cpu/core_prefetch.cpp cpu/core_simple.cpp cpu/cpu.cpp cpu/flags.cpp cpu/modrm.cpp \
  cpu/paging.cpp \
  dos/cdrom.cpp dos/cdrom_image.cpp dos/dos.cpp dos/dos_classes.cpp dos/dos_devices.cpp \
  dos/dos_execute.cpp dos/dos_files.cpp dos/dos_ioctl.cpp dos/dos_keyboard_layout.cpp \
  dos/dos_memory.cpp dos/dos_misc.cpp dos/dos_mscdex.cpp dos/dos_programs.cpp \
  dos/dos_tables.cpp dos/drive_cache.cpp dos/drive_fat.cpp dos/drive_iso.cpp \
  dos/drive_local.cpp dos/drive_virtual.cpp dos/drives.cpp \
  dosbox.cpp \
  fpu/fpu.cpp \
  gui/midi.cpp gui/render.cpp gui/render_scalers.cpp gui/sdl_gui.cpp gui/sdl_mapper.cpp \
  gui/sdlmain.cpp \
  hardware/adlib.cpp hardware/cmos.cpp hardware/dbopl.cpp hardware/disney.cpp \
  hardware/dma.cpp hardware/gameblaster.cpp hardware/mame/saa1099.cpp hardware/mame/sn76496.cpp hardware/gus.cpp hardware/hardware.cpp \
  hardware/iohandler.cpp hardware/ipx.cpp hardware/joystick.cpp hardware/keyboard.cpp \
  hardware/memory.cpp hardware/mixer.cpp hardware/mpu401.cpp hardware/opl.cpp \
  hardware/pcspeaker.cpp hardware/pic.cpp hardware/sblaster.cpp hardware/tandy_sound.cpp \
  hardware/timer.cpp hardware/vga.cpp hardware/vga_attr.cpp hardware/vga_crtc.cpp \
  hardware/vga_dac.cpp hardware/vga_draw.cpp hardware/vga_gfx.cpp hardware/vga_memory.cpp \
  hardware/vga_misc.cpp hardware/vga_other.cpp hardware/vga_paradise.cpp \
  hardware/vga_s3.cpp hardware/vga_seq.cpp hardware/vga_tseng.cpp hardware/vga_xga.cpp \
  hardware/serialport/misc_util.cpp hardware/serialport/serialdummy.cpp \
  hardware/serialport/serialport.cpp \
  ints/bios.cpp ints/bios_disk.cpp ints/bios_keyboard.cpp ints/ems.cpp ints/int10.cpp \
  ints/int10_char.cpp ints/int10_memory.cpp ints/int10_misc.cpp ints/int10_modes.cpp \
  ints/int10_pal.cpp ints/int10_put_pixel.cpp ints/int10_vesa.cpp \
  ints/int10_video_state.cpp ints/int10_vptable.cpp ints/mouse.cpp ints/xms.cpp \
  libs/gui_tk/gui_tk.cpp \
  misc/cross.cpp misc/messages.cpp misc/programs.cpp misc/setup.cpp misc/support.cpp \
  shell/shell.cpp shell/shell_batch.cpp shell/shell_cmds.cpp shell/shell_misc.cpp)
DB_OBJ := $(patsubst %.cpp,$(B)/%.o,$(DB_SRC))
DB_HDR := $(shell find $(DB_J)/include $(DB_J)/src -name '*.h') $(DB_DIR)/plataforma/config.h \
          userland/libsdl12/SDL.h userland/libsdl12/SDL_thread.h

# Con RTTI (dynamic_cast, typeid): lo único del sistema que lo enciende. -O2 y sin marco: el
# intérprete y el recompilador son lo que más pesa; -fno-strict-aliasing: es código de 2002.
DB_FLAGS := $(subst -fno-rtti,-frtti,$(subst -Os,-O2,$(CXXFLAGS_SAGUI))) -fomit-frame-pointer -fno-strict-aliasing \
  -Iuserland/libsdl12 -I$(DB_DIR)/plataforma -I$(DB_J)/include -I$(DB_J)/src \
  -DHAVE_CONFIG_H -DNDEBUG -w

$(B)/$(DB_J)/%.o: $(DB_J)/%.cpp $(DB_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(DB_DIR)/dosbox.mk
	@mkdir -p $(dir $@)
	$(CXX) $(DB_FLAGS) -c -o $@ $<

# La SDL 1.2 común: la define openjazz.mk; sin OpenJazz (OPENJAZZ=0), aquí con las mismas opciones.
ifeq ($(SDL12_OBJ),)
SDL12_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl12 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
SDL12_OBJ := $(B)/userland/libsdl12/sdl_sagui.o
$(SDL12_OBJ): userland/libsdl12/sdl_sagui.c userland/libsdl12/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL12_FLAGS) -c -o $@ $<
endif

DB_PLAT_OBJ := $(B)/$(DB_DIR)/plataforma/principal_dosbox.o
$(DB_PLAT_OBJ): $(DB_DIR)/plataforma/principal_dosbox.c userland/libsdl12/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL12_FLAGS) -c -o $@ $<

# Enlazado con programa_cxx.ld (las tablas de las excepciones) y la libm x87.
$(B)/userland/bin/dosbox: $(DB_OBJ) $(DB_PLAT_OBJ) $(SDL12_OBJ) $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/dosbox: las licencias y el leeme.
DB_RECURSOS := $(B)/dosbox-res/.feito
$(DB_RECURSOS): $(DB_J)/COPYING $(DB_J)/AUTHORS $(DB_DIR)/leeme.txt
	@mkdir -p $(B)/dosbox-res
	cp $(DB_J)/COPYING $(B)/dosbox-res/COPYING
	cp $(DB_J)/AUTHORS $(B)/dosbox-res/AUTHORS
	cp $(DB_DIR)/leeme.txt $(B)/dosbox-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/dosbox.
APPS_BINS += $(B)/userland/bin/dosbox
APPS_RECURSOS += dosbox:$(B)/dosbox-res
