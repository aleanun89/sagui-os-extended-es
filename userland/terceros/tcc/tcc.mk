# SPDX-License-Identifier: GPL-3.0-only
# tcc.mk - TCC (Tiny C Compiler) dentro de SaguiOS (D384, docs/tcc.md). Lo incluye el Makefile.
#
# TCC (LGPL-2.1-or-later) viene de tools/tcc_fuentes.sh (no va en git). Se construyen dos compiladores
# con las mismas fuentes y la misma configuración (plataforma/config.h):
#   - $(B)/tcc/anfitrion/i386-tcc: corre en el PC; compila libtcc1.a y runmain.o (como hace TCC
#     con su propio Makefile) y sirve para probar en el PC lo que el de dentro hará igual;
#   - /apps/bin/tcc: corre en SaguiOS, compilado con gcc contra la libc común y la x87.
# Los programas que compila salen ELF32 estáticos a 0x80000000 (D384: sin intérprete ni .so):
# /apps/tcc/lib tiene crt1.o (inicio.asm de la libsagui), libc.a (la libc común, su libm y la
# libsagui entera), las SDL 2 (libSDL2.a y sus capas) y libtcc1.a; /apps/tcc/include, las cabeceras.
# Además van los fuentes de AoEN a /apps/aoen/fuentes con su compilar.sh (mejora 142).
# Sin fuentes se salta con un aviso. TCC=0 lo desactiva.

TCC_DIR := userland/terceros/tcc
TCC_F   := $(TCC_DIR)/fuentes
TCC_B   := $(B)/tcc
TCC ?= 1
TCC_RECURSOS :=

ifeq ($(TCC),1)
ifneq ($(wildcard $(TCC_F)/.preparado),)
include userland/libsdl2/sdl2_capas.mk

TCC_FONTES := $(wildcard $(TCC_F)/*.c $(TCC_F)/*.h) $(wildcard $(TCC_DIR)/plataforma/*.c $(TCC_DIR)/plataforma/*.h \
  $(TCC_DIR)/plataforma/include/*.h $(TCC_DIR)/plataforma/include/sys/*.h)
TCC_FLAGS := -m32 -march=i686 -std=gnu99 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -mfpmath=387 -fexcess-precision=standard -fcommon \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -I$(TCC_DIR)/plataforma -idirafter $(TCC_DIR)/plataforma/include -w

# ---- el compilador de SaguiOS: /apps/bin/tcc ----
$(TCC_B)/tcc.o: $(TCC_FONTES) $(LIBC_HDR) $(TCC_DIR)/tcc.mk
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(TCC_FLAGS) -DONE_SOURCE=1 -c -o $@ $(TCC_F)/tcc.c
$(TCC_B)/%.o: $(TCC_DIR)/plataforma/%.c $(LIBC_HDR) $(TCC_DIR)/tcc.mk
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(TCC_FLAGS) -c -o $@ $<
# Lo que «tcc -run» ofrece al programa (tcc_sagui_run.c): la libc, su libm y la base de la libsagui.
TCC_SIMB_OBJ := $(LIBC_OBJ) $(QJS_LIBM) $(filter-out %/inicio.asm.o,$(LIBSAGUI_BASE_OBJ))
$(TCC_B)/tcc_simbolos.c: $(TCC_SIMB_OBJ) tools/tcc_simbolos.py
	@mkdir -p $(dir $@)
	$(PY) tools/tcc_simbolos.py $@ $(TCC_SIMB_OBJ)
$(TCC_B)/tcc_simbolos.o: $(TCC_B)/tcc_simbolos.c
	$(CC) $(TCC_FLAGS) -c -o $@ $<

$(B)/userland/bin/tcc: $(TCC_B)/tcc.o $(TCC_B)/tcc_libc_extra.o $(TCC_B)/principal_main.o $(TCC_B)/tcc_simbolos.o \
                       $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# libtcc (mejora 142): la misma biblioteca (libtcc.c con ONE_SOURCE) para que un programa compile C en
# memoria; con la tabla de símbolos, lo que compila usa la libc del propio programa (tcc_sagui_run.c).
$(TCC_B)/libtcc.o: $(TCC_FONTES) $(LIBC_HDR) $(TCC_DIR)/tcc.mk
	@mkdir -p $(dir $@)
	env -u CPATH -u C_INCLUDE_PATH $(CC) $(TCC_FLAGS) -DONE_SOURCE=1 -c -o $@ $(TCC_F)/libtcc.c

# ---- el cruzado del PC: build/tcc/anfitrion/i386-tcc ----
TCC_X := $(TCC_B)/anfitrion/i386-tcc
$(TCC_X): $(TCC_FONTES) $(TCC_DIR)/tcc.mk
	@mkdir -p $(dir $@)
	CPATH="$(HOST_CPATH)" $(HOSTCC) -O2 -w -DONE_SOURCE=1 -I$(TCC_DIR)/plataforma -o $@ $(TCC_F)/tcc.c -lm

# ---- /apps/tcc: cabeceras y bibliotecas ----
TCC_RAIZ := $(TCC_B)/raiz
# Las cabeceras del kernel que piden las de userland/inc (sagui.h y compañía).
TCC_KERNEL_H := kernel/inc/abi.h kernel/inc/fuso.h kernel/inc/sha256.h kernel/inc/tipos.h
TCC_LIBTCC1 := libtcc1.c alloca.S alloca-bt.S stdatomic.c atomic.S builtin.c pic86.S dsohandle.c
# Fuera de libtcc1.a, como en TCC: -run (runmain.o), -b (bcheck.o, bcheck_run.o) y sus trazas (bt-*.o).
TCC_LIB_EXTRA := runmain.o bt-exe.o bt-log.o bcheck.o bcheck_run.o
# Los fuentes de TCC que se instalan en /apps/tcc/fuentes para que se compile a sí mismo dentro.
TCC_AUTO := tcc.c libtcc.c tccpp.c tccgen.c tccdbg.c tccelf.c tccasm.c tccrun.c tcctools.c i386-gen.c i386-link.c \
  i386-asm.c tcc.h libtcc.h tcctok.h i386-asm.h i386-tok.h x86_64-asm.h elf.h stab.h stab.def dwarf.h COPYING VERSION
TCC_EJEMPLOS := $(wildcard $(TCC_DIR)/ejemplos/*)
# La libc.a de TCC lleva la libc común, su libm y la libsagui entera (sin inicio.asm, que va en crt1.o):
# así «tcc hola.c» no necesita -l; principal_main.o da principal() a los programas con main().
TCC_LIBC_OBJ := $(LIBC_OBJ) $(QJS_LIBM) $(filter-out %/inicio.asm.o,$(LIBSAGUI_OBJ)) $(TCC_B)/principal_main.o
TCC_SDL_OBJ := $(SDL2_OBJ) $(B)/userland/libsdl2/sdl2_rwops.o
# D425: OpenGL ES 1.1 (SaguiGL) y EGL: -lGLESv1_CM -lEGL, con <GLES/gl.h> y <EGL/egl.h>.
TCC_GL_OBJ := $(B)/userland/libsgl/sgl.o $(B)/userland/libsgl/sgl_fixo.o
TCC_EGL_OBJ := $(B)/userland/libsgl/sgl_egl.o
$(TCC_RAIZ)/.feito: $(TCC_X) $(TCC_B)/libtcc.o $(TCC_B)/tcc_libc_extra.o $(TCC_B)/tcc_simbolos.c $(TCC_B)/tcc_simbolos.o \
                    $(TCC_DIR)/autocompilar.sh $(TCC_DIR)/Makefile.autocompilar $(TCC_LIBC_OBJ) $(TCC_SDL_OBJ) $(SDL2CAPAS_OBJ) $(FONTES_OBJ) $(B)/userland/libsagui/inicio.asm.o \
                    $(LIBC_HDR) $(USER_HDR) $(TCC_KERNEL_H) $(wildcard userland/libsdl2/*.h userland/libsdl2/SDL2/*.h) \
                    $(wildcard $(TCC_DIR)/plataforma/include/*.h) $(TCC_EJEMPLOS) $(TCC_DIR)/leeme.txt $(TCC_DIR)/tcc.mk \
                    $(TCC_GL_OBJ) $(TCC_EGL_OBJ) $(SDL2GL_OBJ) $(wildcard userland/inc/GLES/*.h userland/inc/EGL/*.h userland/inc/KHR/*.h) kernel/inc/esquemas.h \
                    $(LUA_A) $(FF_DEV_OK)
	rm -rf $(TCC_RAIZ) && mkdir -p $(TCC_RAIZ)/include/sagui $(TCC_RAIZ)/include/kernel $(TCC_RAIZ)/include/SDL2 \
	  $(TCC_RAIZ)/lib $(TCC_RAIZ)/ejemplos $(TCC_B)/lib-obj
	cp $(TCC_F)/include/*.h $(TCC_RAIZ)/include/
	cp $(LIBC_DIR)/*.h $(TCC_RAIZ)/include/
	cp -r $(LIBC_DIR)/arpa $(LIBC_DIR)/netinet $(LIBC_DIR)/sys $(TCC_RAIZ)/include/
	cp -r $(TCC_DIR)/plataforma/include/. $(TCC_RAIZ)/include/
	cp $(TCC_F)/libtcc.h $(TCC_RAIZ)/include/
	cp userland/inc/*.h $(TCC_RAIZ)/include/sagui/
	@# D428: gui.h incluye esquemas.h (el esquema de apariencia, compartido con el kernel)
	cp kernel/inc/esquemas.h $(TCC_RAIZ)/include/sagui/
	cp $(TCC_KERNEL_H) $(TCC_RAIZ)/include/kernel/
	cp userland/libsdl2/*.h $(TCC_RAIZ)/include/SDL2/
	cp -r userland/libsdl2/SDL2 $(TCC_RAIZ)/include/SDL2/
	cp -r userland/inc/GLES userland/inc/EGL userland/inc/KHR $(TCC_RAIZ)/include/
	@# D440: Lua 5.4 para incrustarlo: <lua.h>, <lauxlib.h>, <lualib.h>, <luaconf.h>, <lua.hpp> y -llua
	$(if $(LUA_A),cp $(addprefix $(LUA_F)/,lua.h lauxlib.h lualib.h luaconf.h lua.hpp) $(TCC_RAIZ)/include/ && cp $(LUA_A) $(TCC_RAIZ)/lib/liblua.a)
	@# D458: FFmpeg para incrustarlo: -lavformat -lavcodec -lswscale -lswresample -lavutil y <libavcodec/avcodec.h>...
	$(if $(FF_DEV_OK),cp -r $(FF_DEV)/usr/local/include/. $(TCC_RAIZ)/include/ && cp $(FF_DEV)/usr/local/lib/*.a $(TCC_RAIZ)/lib/)
	@# crt1.o: el _inicio de la libsagui (en .text: NASM deja .text.inicio sin permiso de ejecución) y _start
	objcopy --rename-section .text.inicio=.text,alloc,code,readonly,load,contents \
	  --add-symbol _start=.text:0,global,function $(B)/userland/libsagui/inicio.asm.o $(TCC_RAIZ)/lib/crt1.o
	as --32 -o $(TCC_RAIZ)/lib/crti.o /dev/null
	cp $(TCC_RAIZ)/lib/crti.o $(TCC_RAIZ)/lib/crtn.o
	ar rcs $(TCC_RAIZ)/lib/libc.a $(TCC_LIBC_OBJ)
	ar rcs $(TCC_RAIZ)/lib/libSDL2.a $(TCC_SDL_OBJ) $(SDL2GL_OBJ)
	ar rcs $(TCC_RAIZ)/lib/libSDL2_image.a $(B)/userland/libsdl2/sdl2_image.o
	ar rcs $(TCC_RAIZ)/lib/libSDL2_ttf.a $(B)/userland/libsdl2/sdl2_ttf.o $(FONTES_OBJ)
	@# las dos con todo: así da igual el orden de -lEGL y -lGLESv1_CM
	ar rcs $(TCC_RAIZ)/lib/libGLESv1_CM.a $(TCC_GL_OBJ) $(TCC_EGL_OBJ)
	ar rcs $(TCC_RAIZ)/lib/libEGL.a $(TCC_EGL_OBJ) $(TCC_GL_OBJ)
	ar rcs $(TCC_RAIZ)/lib/libSDL2_mixer.a $(B)/userland/libsdl2/sdl2_mixer.o $(B)/userland/libsdl2/sdl2_vorbis.o
	@# -lm, -lsagui y -lpthread no hacen falta (todo está en libc.a) pero se aceptan: archivos con un objeto vacío
	as --32 -o $(TCC_B)/lib-obj/vacio.o /dev/null
	for l in m sagui pthread dl; do ar rcs $(TCC_RAIZ)/lib/lib$$l.a $(TCC_B)/lib-obj/vacio.o; done
	@# libtcc1.a y runmain.o (para -run), con el propio TCC, como su lib/Makefile
	for f in $(TCC_LIBTCC1) runmain.c bt-exe.c bt-log.c; do \
	  $(TCC_X) -B$(TCC_RAIZ) -I$(TCC_DIR)/plataforma -c $(TCC_F)/lib/$$f -o $(TCC_B)/lib-obj/$${f%.*}.o || exit 1; done
	$(TCC_X) -B$(TCC_RAIZ) -I$(TCC_DIR)/plataforma -bt -c $(TCC_F)/lib/bcheck.c -o $(TCC_B)/lib-obj/bcheck.o
	$(TCC_X) -B$(TCC_RAIZ) -I$(TCC_DIR)/plataforma -bt -DBCHECK_RUN -c $(TCC_F)/lib/bcheck.c -o $(TCC_B)/lib-obj/bcheck_run.o
	$(TCC_X) -ar rcs $(TCC_RAIZ)/lib/libtcc1.a $(addprefix $(TCC_B)/lib-obj/,$(addsuffix .o,$(basename $(TCC_LIBTCC1))))
	cp $(addprefix $(TCC_B)/lib-obj/,$(TCC_LIB_EXTRA)) $(TCC_RAIZ)/lib/
	@# libtcc.a: el compilador como biblioteca (y lo que pide de la libc y la tabla de símbolos)
	ar rcs $(TCC_RAIZ)/lib/libtcc.a $(TCC_B)/libtcc.o $(TCC_B)/tcc_libc_extra.o $(TCC_B)/tcc_simbolos.o
	@# los fuentes de TCC, para compilarlo dentro (autocompilar.sh)
	mkdir -p $(TCC_RAIZ)/fuentes
	cp $(addprefix $(TCC_F)/,$(TCC_AUTO)) $(TCC_DIR)/plataforma/config.h $(TCC_DIR)/plataforma/tcc_sagui_run.c \
	  $(TCC_DIR)/plataforma/tcc_libc_extra.c $(TCC_B)/tcc_simbolos.c $(TCC_RAIZ)/fuentes/
	cp $(TCC_DIR)/autocompilar.sh $(TCC_RAIZ)/fuentes/compilar.sh
	cp $(TCC_DIR)/Makefile.autocompilar $(TCC_RAIZ)/fuentes/Makefile
	@# y las de lo que va en binario en lib/ (libtcc1.a, -run, -b y sus trazas), ya parcheadas
	mkdir -p $(TCC_RAIZ)/fuentes/lib
	cp $(addprefix $(TCC_F)/lib/,$(TCC_LIBTCC1) runmain.c bt-exe.c bt-log.c bcheck.c) $(TCC_RAIZ)/fuentes/lib/
	cp $(TCC_EJEMPLOS) $(TCC_RAIZ)/ejemplos/
	cp $(TCC_DIR)/leeme.txt $(TCC_RAIZ)/leeme.txt
	cp $(TCC_F)/COPYING $(TCC_RAIZ)/LICENCIA-TCC.txt
	@# libtcc1.c no es LGPL: su aviso (GPL-2.0-or-later con excepción de enlace) va al final
	{ printf '\n\n==== lib/libtcc1.c (libtcc1.a): GPL-2.0-or-later con excepción de enlace ====\n\n'; \
	  sed -n '1,/\*\//p' $(TCC_F)/lib/libtcc1.c; } >> $(TCC_RAIZ)/LICENCIA-TCC.txt
	@touch $@

# ---- los fuentes de AoEN, para compilarlo dentro (/apps/aoen/fuentes, mejora 142) ----
TCC_AOEN := $(B)/aoen-fuentes
$(TCC_AOEN)/.feito: $(AOEN_SRC) $(wildcard $(AOEN_DIR)/*.h) userland/terceros/stbimage/stb_image.h \
                    $(TCC_DIR)/aoen/compilar.sh $(TCC_DIR)/aoen/Makefile $(TCC_DIR)/aoen/leeme.txt $(TCC_DIR)/tcc.mk
	rm -rf $(TCC_AOEN) && mkdir -p $(TCC_AOEN)
	cp $(AOEN_SRC) $(wildcard $(AOEN_DIR)/*.h) userland/terceros/stbimage/stb_image.h $(TCC_AOEN)/
	sed 's/@FUENTES@/$(notdir $(AOEN_SRC))/' $(TCC_DIR)/aoen/compilar.sh > $(TCC_AOEN)/compilar.sh
	sed 's/@FUENTES@/$(notdir $(AOEN_SRC))/' $(TCC_DIR)/aoen/Makefile > $(TCC_AOEN)/Makefile
	cp $(TCC_DIR)/aoen/leeme.txt $(TCC_AOEN)/leeme.txt
	@touch $@

APPS_BINS += $(B)/userland/bin/tcc
APPS_RECURSOS += tcc:$(TCC_RAIZ) aoen/fuentes:$(TCC_AOEN)
TCC_RECURSOS := $(TCC_RAIZ)/.feito $(TCC_AOEN)/.feito
# El Laboratorio de C (D414) usa libtcc: va con TCC.
include userland/programas/labc/labc.mk
# (el árbol de /apps, APPS_DEP en el Makefile, se define después de incluir este fichero)
$(B)/apps/.feito: $(TCC_RECURSOS)
else
$(info Makefile: sin fuentes de TCC: se salta el compilador de C de dentro (tools/tcc_fuentes.sh, docs/tcc.md))
endif
endif
