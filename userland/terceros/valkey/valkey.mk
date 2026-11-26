# SPDX-License-Identifier: GPL-3.0-only
# valkey.mk - Valkey 8.0.2 dentro de SaguiOS (D477, docs/decisoes.md (D477)). Lo incluye el Makefile.
#
# Valkey (continuación libre de Redis, BSD-3) viene de tools/valkey_fuentes.sh (no va en git). Se compila con gcc
# contra la libc común y la x87 (puntuaciones de zset, INCRBYFLOAT: dobles): el servidor /apps/bin/valkey-server,
# el cliente /apps/bin/valkey-cli, la biblioteca hiredis (libhiredis.a y cabeceras para tcc en /apps/tcc) y
# /apps/valkey (valkey.conf de SaguiOS, ejemplos de clientes C, MicroPython y Lua, léeme y licencias).
# Se compila SIN __linux__ (-U__linux__): Valkey cae al bucle con select() (ae_select.c) y a las rutas
# genéricas (sin epoll, sin /proc, sin backtrace de glibc). Malloc: el de la libc. Sin TLS ni módulos (dlopen
# existe, pero un módulo necesitaría las cabeceras del servidor; MODULE LOAD da error). Los trabajos en segundo
# plano (bio.c) usan la capa pthread (D458); RDB y AOF en segundo plano usan fork (D401). Sin fuentes se salta.
# VALKEY_DENTRO=0 lo desactiva.

VK_DIR := userland/terceros/valkey
VK_F   := $(VK_DIR)/fuentes
VALKEY_DENTRO ?= 1

ifeq ($(VALKEY_DENTRO),1)
ifneq ($(wildcard $(VK_F)/.preparado),)

VK_SRV_NOM := threads_mngr adlist quicklist ae anet dict kvstore server sds zmalloc lzf_c lzf_d pqsort zipmap sha1 ziplist \
  release memory_prefetch io_threads networking util object db replication rdb t_string t_list t_set t_zset t_hash config aof \
  pubsub multi debug sort intset syncio cluster cluster_legacy cluster_slot_stats crc16 endianconv slowlog eval bio rio rand \
  memtest syscheck crcspeed crccombine crc64 bitops sentinel notify setproctitle blocked hyperloglog latency sparkline \
  valkey-check-rdb valkey-check-aof geo lazyfree module evict expire geohash geohash_helper childinfo defrag siphash rax \
  t_stream listpack localtime lolwut lolwut5 lolwut6 acl tracking socket tls sha256 timeout setcpuaffinity monotonic \
  mt19937-64 resp_parser call_reply script_lua script functions function_lua commands strl connection unix logreqres
VK_CLI_NOM := anet adlist dict valkey-cli zmalloc release ae serverassert crcspeed crccombine crc64 siphash crc16 monotonic \
  cli_common mt19937-64 strl cli_commands
VK_SRV_OBJ := $(addprefix $(B)/$(VK_F)/src/,$(addsuffix .o,$(VK_SRV_NOM)))
VK_CLI_OBJ := $(addprefix $(B)/$(VK_F)/src/,$(addsuffix .o,$(VK_CLI_NOM)))

VK_HIREDIS_NOM := alloc net hiredis sds async read sockcompat
VK_HIREDIS_OBJ := $(addprefix $(B)/$(VK_F)/deps/hiredis/,$(addsuffix .o,$(VK_HIREDIS_NOM)))
VK_LUA_NOM := lapi lcode ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio \
  strbuf fpconv lauxlib lbaselib ldblib liolib lmathlib loslib ltablib lstrlib loadlib linit lua_cjson lua_struct lua_cmsgpack lua_bit
VK_LUA_OBJ := $(addprefix $(B)/$(VK_F)/deps/lua/src/,$(addsuffix .o,$(VK_LUA_NOM)))
VK_DEPS_OBJ := $(B)/$(VK_F)/deps/hdr_histogram/hdr_histogram.o $(B)/$(VK_F)/deps/fpconv/fpconv_dtoa.o $(B)/$(VK_F)/deps/linenoise/linenoise.o
VK_ALL_OBJ := $(sort $(VK_SRV_OBJ) $(VK_CLI_OBJ) $(VK_HIREDIS_OBJ) $(VK_LUA_OBJ) $(VK_DEPS_OBJ))
VK_HDR := $(wildcard $(VK_F)/src/*.h $(VK_F)/deps/*/*.h $(VK_F)/deps/lua/src/*.h $(VK_DIR)/plataforma/*.h)

VK_FLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -O2 -w -fcommon -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -U__linux__ -U__linux -Ulinux \
  -include $(VK_DIR)/plataforma/valkey_sagui.h -DNDEBUG
VK_INC := -I$(VK_F)/src -I$(VK_F)/deps/hiredis -I$(VK_F)/deps/lua/src -I$(VK_F)/deps/linenoise -I$(VK_F)/deps/hdr_histogram -I$(VK_F)/deps/fpconv
# el cliente toma <sds.h> y <sdscompat.h> de hiredis (así hay un solo juego de funciones sds en el binario)
VK_INC_CLI := -I$(VK_F)/deps/hiredis -I$(VK_F)/src -I$(VK_F)/deps/linenoise -I$(VK_F)/deps/hdr_histogram -I$(VK_F)/deps/fpconv

$(VK_ALL_OBJ): $(B)/%.o: %.c $(VK_HDR) $(LIBC_HDR) $(VK_DIR)/valkey.mk
	@mkdir -p $(dir $@)
	$(CC) $(VK_FLAGS) $(VK_INC) -c -o $@ $<
$(addprefix $(B)/$(VK_F)/src/,valkey-cli.o cli_common.o cli_commands.o): $(B)/%.o: %.c $(VK_HDR) $(LIBC_HDR) $(VK_DIR)/valkey.mk
	@mkdir -p $(dir $@)
	$(CC) $(VK_FLAGS) $(VK_INC_CLI) -c -o $@ $<
# Lua 5.1 de Valkey (cjson, cmsgpack, struct, bit incluidos)
$(VK_LUA_OBJ): VK_FLAGS += -DLUA_ANSI -DENABLE_CJSON_GLOBAL -DREDIS_STATIC='' -DLUA_USE_MKSTEMP
$(B)/$(VK_F)/deps/hdr_histogram/hdr_histogram.o: VK_FLAGS += -DHDR_MALLOC_INCLUDE='"hdr_redis_malloc.h"'
$(B)/$(VK_DIR)/principal_main.o: $(VK_DIR)/plataforma/principal_main.c
	@mkdir -p $(dir $@)
	$(CC) $(VK_FLAGS) -c -o $@ $<

# hiredis: la biblioteca para los programas de C (también va a /apps/tcc/lib)
VK_HIREDIS_A := $(B)/valkey/libhiredis.a
$(VK_HIREDIS_A): $(VK_HIREDIS_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@ && ar rcs $@ $^

VK_LD_COMUN := $(B)/$(VK_DIR)/principal_main.o $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
$(B)/userland/bin/valkey-server: $(VK_SRV_OBJ) $(VK_LUA_OBJ) $(VK_DEPS_OBJ) $(VK_HIREDIS_A) $(VK_LD_COMUN)
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(VK_HIREDIS_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
$(B)/userland/bin/valkey-cli: $(VK_CLI_OBJ) $(B)/$(VK_F)/deps/linenoise/linenoise.o $(B)/$(VK_F)/deps/hdr_histogram/hdr_histogram.o $(VK_HIREDIS_A) $(VK_LD_COMUN)
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^) $(VK_HIREDIS_A)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# /apps/valkey: configuración, ejemplos y licencias (el árbol de /apps lo arma APPS_RECURSOS)
VK_RAIZ := $(B)/valkey-raiz
VK_EJEMPLOS := $(wildcard $(VK_DIR)/ejemplos/*)
$(VK_RAIZ)/.feito: $(VK_EJEMPLOS) $(VK_DIR)/leeme.txt $(VK_DIR)/valkey-sagui.conf $(VK_F)/COPYING $(VK_DIR)/valkey.mk
	rm -rf $(VK_RAIZ) && mkdir -p $(VK_RAIZ)/ejemplos $(VK_RAIZ)/datos
	cp $(VK_EJEMPLOS) $(VK_RAIZ)/ejemplos/
	cp $(VK_DIR)/leeme.txt $(VK_RAIZ)/leeme.txt
	cp $(VK_DIR)/valkey-sagui.conf $(VK_RAIZ)/valkey.conf
	cp $(VK_F)/COPYING $(VK_RAIZ)/LICENCIA-VALKEY.txt
	cp $(VK_F)/deps/hiredis/COPYING $(VK_RAIZ)/LICENCIA-HIREDIS.txt
	@touch $@
APPS_RECURSOS += valkey:$(VK_RAIZ)
$(B)/apps/.feito: $(VK_RAIZ)/.feito

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS), que está en el PATH.
APPS_BINS += $(B)/userland/bin/valkey-server $(B)/userland/bin/valkey-cli
VALKEY_HIREDIS_A := $(VK_HIREDIS_A)
endif
endif
