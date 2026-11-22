# SPDX-License-Identifier: GPL-2.0-or-later
# freeciv.mk - cómo se compilan /apps/bin/freeciv-server y /apps/bin/freeciv-sdl2 (D374).
# Lo incluye el Makefile principal fuera de la variante sin GPL (D109) y solo si hay fuentes
# preparadas (tools/freeciv_fuentes.sh: Freeciv 3.1.6, GPL-2.0 o posterior, y zlib 1.3.1).
#
# Freeciv es código del upstream en fuentes/ (fuera de git) con un parche pequeño
# (parches/sagui.patch, que tools/freeciv_fuentes.sh aplica) y lo nuestro en plataforma/: la
# configuración (inc/freeciv_config.h, inc/fc_config.h: sin hilos, sin ICU, sin curl, con zlib),
# un libintl que lee .mo, un sustituto de netfile.c (el upstream usa libcurl) y los lanzadores.
# Se compila en C con la x87, la libc común (userland/libc, sin su zlib.c: aquí va la zlib real de
# userland/terceros/zlib), la SDL 2 del sistema y las capas SDL2_image/SDL2_ttf de userland/libsdl2.

FC_DIR := userland/terceros/freeciv
FC_S   := $(FC_DIR)/fuentes
FC_PL  := $(FC_DIR)/plataforma
ZL_S   := userland/terceros/zlib/fuentes

FC_CC := env -u CPATH -u C_INCLUDE_PATH $(CC)

FC_UTIL   := $(filter-out $(FC_S)/utility/netfile.c,$(wildcard $(FC_S)/utility/*.c))
FC_COMMON := $(wildcard $(FC_S)/common/*.c $(FC_S)/common/aicore/*.c $(FC_S)/common/networking/*.c $(FC_S)/common/scriptcore/*.c)
FC_LUA    := $(filter-out %/lua.c %/luac.c,$(wildcard $(FC_S)/dependencies/lua-5.4/src/*.c)) \
             $(wildcard $(FC_S)/dependencies/tolua-5.2/src/lib/*.c) $(FC_S)/dependencies/cvercmp/cvercmp.c
FC_PLAT_C := $(FC_PL)/netfile_sagui.c $(FC_PL)/libintl.c
FC_AI     := $(wildcard $(FC_S)/ai/*.c $(FC_S)/ai/default/*.c $(FC_S)/ai/classic/*.c)
FC_SRV    := $(filter-out $(FC_S)/server/civserver.c,$(wildcard $(FC_S)/server/*.c $(FC_S)/server/advisors/*.c \
               $(FC_S)/server/generator/*.c $(FC_S)/server/savegame/*.c $(FC_S)/server/scripting/*.c))
FC_CLI    := $(filter-out %/dummycxx.cpp %/gui_interface.c %/gui_cbsetter.c,$(wildcard $(FC_S)/client/*.c $(FC_S)/client/agents/*.c \
               $(FC_S)/client/luascript/*.c $(FC_S)/client/gui-sdl2/*.c)) $(FC_S)/dependencies/SDL2_gfx/SDL2_rotozoom.c
FC_ZLIB   := $(addprefix $(ZL_S)/,adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c \
               infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)

# Las banderas de los ports en C (como NS_BASE) y las de Freeciv: las cabeceras de plataforma
# y de zlib antes que las de la libc (la libc trae un zlib.h de NetSurf).
FC_BASE := -m32 -march=i686 -std=gnu99 -Os -include $(LIBC_DIR)/libgen.h -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -Wno-error=int-conversion \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -I$(FC_PL)/inc -I$(ZL_S) -I$(LIBC_DIR) -isystem $(shell $(CC) -print-file-name=include) \
  -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
  -DHAVE_CONFIG_H -DAUDIO_SDL -DFREECIV_NDEBUG -DSAGUI_FREECIV -DSAGUI_SIN_STRUCT_OPTION -w
FC_INC := -I$(FC_S)/utility -I$(FC_S)/common -I$(FC_S)/common/aicore -I$(FC_S)/common/networking \
  -I$(FC_S)/common/scriptcore -I$(FC_S)/ai -I$(FC_S)/ai/default -I$(FC_S)/ai/classic -I$(FC_S)/server \
  -I$(FC_S)/server/advisors -I$(FC_S)/server/generator -I$(FC_S)/server/savegame -I$(FC_S)/server/scripting \
  -I$(FC_S)/client -I$(FC_S)/client/include -I$(FC_S)/client/luascript -I$(FC_S)/client/agents \
  -I$(FC_S)/client/gui-sdl2 -I$(FC_S)/dependencies -I$(FC_S)/dependencies/SDL2_gfx -I$(FC_S)/dependencies -Iuserland/libsdl2 \
  -I$(FC_S)/dependencies/lua-5.4/src -I$(FC_S)/dependencies/tolua-5.2/include \
  -I$(FC_S)/dependencies/cvercmp -I$(FC_S)/dependencies/tinycthread

FC_O := $(B)/$(FC_DIR)
FC_HDR := $(LIBC_HDR) $(wildcard $(FC_PL)/inc/*.h $(FC_PL)/inc/*/*.h) $(FC_DIR)/freeciv.mk

$(FC_O)/obj/%.o: %.c $(FC_S)/.preparado $(FC_HDR)
	@mkdir -p $(dir $@)
	$(FC_CC) $(FC_BASE) $(FC_INC) -c -o $@ $<

FC_OBJ_COMUN := $(addprefix $(FC_O)/obj/,$(FC_UTIL:.c=.o) $(FC_COMMON:.c=.o) $(FC_LUA:.c=.o) $(FC_PLAT_C:.c=.o) $(FC_ZLIB:.c=.o))
FC_OBJ_SRV   := $(addprefix $(FC_O)/obj/,$(FC_AI:.c=.o) $(FC_SRV:.c=.o) $(FC_S)/server/civserver.o)
FC_OBJ_CLI   := $(addprefix $(FC_O)/obj/,$(FC_CLI:.c=.o) $(FC_AI:.c=.o))
FC_LIBC_OBJ  := $(filter-out %/libc/zlib.o,$(LIBC_OBJ))

FC_LANC_SRV := $(FC_O)/principal_srv.o
$(FC_LANC_SRV): $(FC_PL)/principal_fc.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(FC_BASE) -Iuserland/inc -idirafter kernel/inc -DFC_SERVIDOR -c -o $@ $<

$(B)/userland/bin/freeciv-server: $(FC_OBJ_COMUN) $(FC_OBJ_SRV) $(FC_LANC_SRV) $(FC_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# El cliente SDL2: el mismo código común, el cliente (client/ con gui-sdl2) y las capas SDL2_image y
# SDL2_ttf del sistema (userland/libsdl2/sdl2_capas.mk) sobre la SDL 2 y la libfontes.
include userland/libsdl2/sdl2_capas.mk
FC_LANC_CLI := $(FC_O)/principal_cli.o
$(FC_LANC_CLI): $(FC_PL)/principal_fc.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(FC_BASE) -Iuserland/inc -idirafter kernel/inc -c -o $@ $<

$(B)/userland/bin/freeciv-sdl2: $(FC_OBJ_COMUN) $(FC_OBJ_CLI) $(FC_LANC_CLI) $(SDL2_OBJ) $(SDL2CAPAS_OBJ) $(FONTES_OBJ) $(FC_LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# ---- lo que va a /apps/freeciv: datos (reglas, tilesets, tema), catálogos y licencia ----
# tools/freeciv_datos.sh recorta los datos del upstream (D374) y compila los .po a .mo.
FC_RECURSOS := $(B)/freeciv-res/.feito
$(FC_RECURSOS): $(FC_S)/.preparado tools/freeciv_datos.sh tools/freeciv_mo.py $(FC_DIR)/leeme.txt $(FC_DIR)/traducciones/es-sagui.po
	rm -rf $(B)/freeciv-res.tmp
	sh tools/freeciv_datos.sh $(FC_S) $(B)/freeciv-res.tmp $(FC_DIR)/leeme.txt
	rm -rf $(B)/freeciv-res
	mv $(B)/freeciv-res.tmp $(B)/freeciv-res
	@touch $@

APPS_BINS += $(B)/userland/bin/freeciv-server $(B)/userland/bin/freeciv-sdl2
APPS_RECURSOS += freeciv:$(B)/freeciv-res
