# quake.mk - cómo se compila /apps/bin/quake (D229). Lo incluye el Makefile
# principal fuera de la variante sin GPL (D109). QUAKE=0 lo deja fuera.
#
# El motor es el WinQuake de id Software (https://github.com/id-Software/Quake,
# GPL-2.0 o posterior), en jogo/, sin tocar (tools/quake_fuentes.sh
# --comprobar): el renderizador por software en C, sin el ensamblador
# (-U__i386__ deja id386 en 0 y el motor usa sus rutinas en C, las que
# llevaba para Alpha y SPARC), sin red (net_none.c: solo la conexión local de
# la partida de un jugador) y sin CD (cd_null.c). Lo nuestro, en plataforma/:
# el sys, el vídeo y la entrada sobre la SDL 1.2 común (userland/libsdl12,
# D228), el sonido, la superposición de ficheros de /home/quake sobre los
# datos de /jogos y el lanzador.
#
# -Dfopen=quake_fopen: las llamadas del motor a fopen() (configuración,
# partidas, demos) pasan por plataforma/arquivos_quake.c sin cambiar sus
# fuentes. -fcommon: el código de 1996 define variables globales sin
# inicializar en varios ficheros. Se compila con la x87, como los demás
# juegos (D208), y a -O2: el renderizador por software es lo que más pesa.

QK_DIR  := userland/terceros/quake
QK_J    := $(QK_DIR)/jogo

QK_SRC  := $(addprefix $(QK_J)/,$(addsuffix .c, \
  chase cl_demo cl_input cl_main cl_parse cl_tent cmd common console crc cvar \
  d_edge d_fill d_init d_modech d_part d_polyse d_scan d_sky d_sprite d_surf d_vars d_zpoint \
  draw host host_cmd keys mathlib menu model net_loop net_main net_none net_vcr nonintel \
  pr_cmds pr_edict pr_exec r_aclip r_alias r_bsp r_draw r_edge r_efrag r_light r_main \
  r_misc r_part r_sky r_sprite r_surf r_vars sbar screen snd_dma snd_mem snd_mix \
  sv_main sv_move sv_phys sv_user view wad world zone cd_null))
QK_PLAT := $(addprefix $(QK_DIR)/plataforma/,sys_sagui.c vid_sagui.c snd_sagui.c arquivos_quake.c principal_quake.c)
QK_OBJ  := $(patsubst %.c,$(B)/%.o,$(QK_SRC))
QK_PLAT_OBJ := $(patsubst %.c,$(B)/%.o,$(QK_PLAT))
QK_HDR  := $(wildcard $(QK_J)/*.h) $(QK_J)/progdefs.q1 $(QK_DIR)/plataforma/arquivos_quake.h \
           userland/libsdl12/SDL.h $(LIBC_HDR)

QK_BASE := -m32 -march=i686 -std=gnu99 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon -fno-strict-aliasing \
  -mfpmath=387 -fexcess-precision=standard -U__i386__ \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -I$(QK_J) -Iuserland/libsdl12 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc
# El motor: sus avisos son los de un código de 1996 (no se tocan).
QK_FLAGS := $(QK_BASE) -Dfopen=quake_fopen -w -Wno-error=implicit-function-declaration
# Lo nuestro: con avisos y los textos en Latin-1, como el resto del userland.
QK_PLAT_FLAGS := $(QK_BASE) -I$(QK_DIR)/plataforma -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 \
  -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

$(B)/$(QK_J)/%.o: $(QK_J)/%.c $(QK_HDR) $(QK_DIR)/quake.mk
	@mkdir -p $(dir $@)
	$(CC) $(QK_FLAGS) -c -o $@ $<

$(B)/$(QK_DIR)/plataforma/%.o: $(QK_DIR)/plataforma/%.c $(QK_HDR) $(USER_HDR) $(QK_DIR)/quake.mk
	@mkdir -p $(dir $@)
	$(CC) $(QK_PLAT_FLAGS) -c -o $@ $<

# La SDL 1.2 común: la define openjazz.mk; sin OpenJazz (OPENJAZZ=0 o sin
# compilador de C++), aquí, con las mismas opciones.
ifeq ($(SDL12_OBJ),)
SDL12_OBJ := $(B)/userland/libsdl12/sdl_sagui.o
SDL12_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl12 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
$(SDL12_OBJ): userland/libsdl12/sdl_sagui.c userland/libsdl12/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL12_FLAGS) -c -o $@ $<
endif

# Enlazado como un programa de C (programa.ld) con la libc común y la libm x87.
$(B)/userland/bin/quake: $(QK_OBJ) $(QK_PLAT_OBJ) $(SDL12_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Lo que va a /apps/quake: la licencia del motor y cómo conseguir los datos.
QK_RECURSOS := $(B)/quake-res/.feito
$(QK_RECURSOS): $(QK_J)/gnu.txt $(QK_J)/readme.txt $(QK_DIR)/datos-leeme.txt
	@mkdir -p $(B)/quake-res
	cp $(QK_J)/gnu.txt $(B)/quake-res/COPYING
	cp $(QK_J)/readme.txt $(B)/quake-res/readme-id.txt
	cp $(QK_DIR)/datos-leeme.txt $(B)/quake-res/leeme.txt
	@touch $@

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS) y /apps/quake.
APPS_BINS += $(B)/userland/bin/quake
APPS_RECURSOS += quake:$(B)/quake-res
