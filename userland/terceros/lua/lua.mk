# SPDX-License-Identifier: GPL-3.0-only
# lua.mk - Lua 5.4.9 dentro de SaguiOS (D440, docs/awk-lua.md). Lo incluye el Makefile.
#
# Lua (PUC-Rio, MIT) viene de tools/lua_fuentes.sh (no va en git). Se compila con gcc contra la libc común y la
# x87 (todo son dobles): el intérprete /apps/bin/lua, el compilador /apps/bin/luac, la biblioteca liblua.a (para
# que TCC compile programas con Lua dentro: /apps/tcc/lib/liblua.a y las cabeceras en /apps/tcc/include) y
# /apps/lua (ejemplos, léeme, licencia y la raíz de los módulos). Con LUA_USE_POSIX: io.popen y os.execute usan
# popen y system (D401), os.tmpname usa mkstemp. Sin fuentes se salta con un aviso. LUA_DENTRO=0 lo desactiva.

LUA_DIR := userland/terceros/lua
LUA_F   := $(LUA_DIR)/fuentes
LUA_DENTRO ?= 1

ifeq ($(LUA_DENTRO),1)
ifneq ($(wildcard $(LUA_F)/.preparado),)
LUA_NUCLEO := lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm \
              lundump lvm lzio lauxlib lbaselib lcorolib ldblib liolib lmathlib loadlib loslib lstrlib ltablib lutf8lib linit
LUA_LIB_OBJ := $(addprefix $(B)/$(LUA_F)/,$(addsuffix .o,$(LUA_NUCLEO))) $(B)/$(LUA_DIR)/plataforma/lsock_sagui.o   # D477: módulo socket
LUA_HDR := $(wildcard $(LUA_F)/*.h $(LUA_DIR)/plataforma/*.h)

LUA_FLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -Os -w -fcommon -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -I$(LUA_F) \
  -DLUA_USE_POSIX -include $(LUA_DIR)/plataforma/lua_sagui.h

$(LUA_LIB_OBJ) $(B)/$(LUA_F)/lua.o $(B)/$(LUA_F)/luac.o: $(B)/%.o: %.c $(LUA_HDR) $(LIBC_HDR) $(LUA_DIR)/lua.mk
	@mkdir -p $(dir $@)
	$(CC) $(LUA_FLAGS) -c -o $@ $<
$(B)/$(LUA_DIR)/principal_main.o: $(LUA_DIR)/plataforma/principal_main.c
	@mkdir -p $(dir $@)
	$(CC) $(LUA_FLAGS) -c -o $@ $<

# D477: linit.c abre utf8 por luaopen_utf8_sagui (lsock_sagui.c), que además deja el módulo «socket» en package.preload
$(B)/$(LUA_F)/linit.o $(B)/$(LUA_F)/linit.pic.o: LUA_FLAGS += -Dluaopen_utf8=luaopen_utf8_sagui

# la biblioteca, para los programas que incrustan Lua (también va a /apps/tcc/lib)
LUA_A := $(B)/lua/liblua.a
$(LUA_A): $(LUA_LIB_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@ && ar rcs $@ $^

$(B)/userland/bin/lua: $(B)/$(LUA_F)/lua.o $(B)/$(LUA_DIR)/principal_main.o $(LUA_A) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(LUA_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
$(B)/userland/bin/luac: $(B)/$(LUA_F)/luac.o $(B)/$(LUA_DIR)/principal_main.o $(LUA_A) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(LUA_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# liblua.so.5.4 (enlace dinámico D378, para programas dinámicos que incrusten Lua): la misma biblioteca compilada
# como PIC y enlazada contra libc.so.1 y libsagui.so.1 (de /lib desde D462); va a /apps/lib. /apps/lua/lua-din es el
# propio intérprete enlazado dinámicamente con ella (lo ejecuta h440).
LUA_SO_PIC := $(patsubst %.o,%.pic.o,$(LUA_LIB_OBJ))
LUA_SO := $(SO_APPS_DIR)/liblua.so.5.4
SO_APPS += $(LUA_SO)
$(LUA_SO_PIC) $(B)/$(LUA_F)/lua.pic.o: $(B)/%.pic.o: %.c $(LUA_HDR) $(LIBC_HDR) $(LUA_DIR)/lua.mk
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -fno-pic -fno-pie,$(LUA_FLAGS)) -fPIC -fno-tree-loop-distribute-patterns -c -o $@ $<
$(LUA_SO): $(LUA_SO_PIC) $(LIBC_SO) $(LIBSAGUI_SO)
	@mkdir -p $(dir $@)
	$(LD) $(SO_LDFLAGS) -soname liblua.so.5.4 -o $(B)/pic/liblua.so.5.4.elf $(LUA_SO_PIC) $(LIBC_SO) $(LIBSAGUI_SO)
	strip --strip-unneeded -o $@ $(B)/pic/liblua.so.5.4.elf
$(B)/lua-din/lua-din: $(B)/$(LUA_F)/lua.pic.o $(B)/$(LUA_DIR)/principal_main.o $(B)/userland/libsagui/inicio.asm.o $(LUA_SO)
	$(call enlazar_dinamico,$(B)/$(LUA_F)/lua.pic.o $(B)/$(LUA_DIR)/principal_main.o,$(LUA_SO) $(LIBC_SO) $(LIBSAGUI_SO))
$(B)/apps/.feito: $(LUA_SO)

# /apps/lua: ejemplos, léeme y licencia (el árbol de /apps lo arma APPS_RECURSOS)
LUA_RAIZ := $(B)/lua-raiz
LUA_EJEMPLOS := $(wildcard $(LUA_DIR)/ejemplos/*)
LUA_MODULOS := $(wildcard $(LUA_DIR)/modulos/*.lua)
$(LUA_RAIZ)/.feito: $(B)/lua-din/lua-din $(LUA_EJEMPLOS) $(LUA_MODULOS) $(LUA_DIR)/leeme.txt $(LUA_F)/LICENCIA-LUA.txt $(LUA_DIR)/lua.mk
	rm -rf $(LUA_RAIZ) && mkdir -p $(LUA_RAIZ)/ejemplos $(LUA_RAIZ)/share/lua/5.4 $(LUA_RAIZ)/lib/lua/5.4
	cp $(LUA_EJEMPLOS) $(LUA_RAIZ)/ejemplos/
	cp $(LUA_MODULOS) $(LUA_RAIZ)/share/lua/5.4/   # D477: valkey.lua
	cp $(B)/lua-din/lua-din $(LUA_RAIZ)/lua-din
	cp $(LUA_DIR)/leeme.txt $(LUA_RAIZ)/leeme.txt
	cp $(LUA_F)/LICENCIA-LUA.txt $(LUA_RAIZ)/LICENCIA-LUA.txt
	@touch $@
APPS_RECURSOS += lua:$(LUA_RAIZ)
$(B)/apps/.feito: $(LUA_RAIZ)/.feito

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS), que está en el PATH.
APPS_BINS += $(B)/userland/bin/lua $(B)/userland/bin/luac
else
$(info Makefile: sin fuentes de Lua: se salta Lua de dentro (tools/lua_fuentes.sh, docs/awk-lua.md))
endif
endif
