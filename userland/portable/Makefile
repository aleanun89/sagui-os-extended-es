# SPDX-License-Identifier: GPL-2.0-or-later
# Makefile - las aplicaciones de SaguiOS en Linux con SDL 2 (D459, docs/apps-portables.md).
#
#   make            compila mico, pluma, macaco, tucan, paint, planomini, gebramini y arara en build/portable/
#   make mico       solo una
#   make probar     ejecuta las pruebas de humo (SDL_VIDEODRIVER=dummy)
#
# Se compila desde la raiz del arbol de SaguiOS (o de un repo portable, que tiene la misma disposicion
# con solo lo necesario). Las fuentes de las aplicaciones y de la libsagui son las mismas que las de SaguiOS:
# solo cambia la capa de llamadas al sistema (userland/portable/).
_ESTE  := $(lastword $(MAKEFILE_LIST))
RAIZ   ?= $(abspath $(dir $(_ESTE))/../..)
B      ?= $(RAIZ)/build/portable
CC     ?= gcc
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -DSAGUI_HOST -DSAGUI_PORTABLE -fno-strict-aliasing -fcommon -w \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -D_GNU_SOURCE \
  -I$(RAIZ)/userland/inc -I$(RAIZ)/userland/portable -I$(RAIZ)/userland/libsagui -I$(RAIZ)/userland/libfontes \
  -I$(RAIZ)/userland/ia -I$(RAIZ)/userland/planomini -I$(RAIZ)/userland/gebramini -idirafter $(RAIZ)/kernel/inc $(SDL_CFLAGS)
LDLIBS := $(SDL_LIBS) -lm

# La libsagui: solo lo que usan estas aplicaciones (el archivo .a deja fuera lo que nadie pide).
LIB_NOMES ?= presentacion presentacion_io presentacion_pptx compressao deflado_continuo difer edicao_rica edicao_rica_dialogos edicao_rica_obj formatacao formatar gui \
  gui_arquivos gui_arvore gui_barras gui_cinta gui_componentes gui_desfazer gui_explorador gui_inspetor gui_moderno gui_pestanas gui_revisar gui_selcor gui_vista \
  hora icones iconos98 idioma imagem imagem_gif imagem_idct imagem_jpeg imagem_png_escribir inflado_continuo \
  pdf_escribir regex rtf texto texto_rico utilitarios wordart entrada zonas sincronia http sonidos conf_maquina contas empacotar dialogos_cf dialogos_objeto \
  gui_grafico grafico_calc
KLIB_NOMES := fuso catalogo sha256
PORT_NOMES := janelas_sdl sistema_sdl sistema_nulo rapido_c
LIB_OBJ  := $(addprefix $(B)/libsagui/,$(addsuffix .o,$(LIB_NOMES))) \
            $(addprefix $(B)/kernel/lib/,$(addsuffix .o,$(KLIB_NOMES))) \
            $(addprefix $(B)/portable/,$(addsuffix .o,$(PORT_NOMES))) \
            $(B)/fonte8x16.o $(B)/libtls/hash.o
FONTES_OBJ := $(patsubst $(RAIZ)/userland/libfontes/%.c,$(B)/libfontes/%.o,$(wildcard $(RAIZ)/userland/libfontes/*.c))
IA_OBJ     := $(patsubst $(RAIZ)/userland/ia/%.c,$(B)/ia/%.o,$(wildcard $(RAIZ)/userland/ia/*.c))
MICO_OBJ   := $(patsubst $(RAIZ)/userland/programas/mico/%.c,$(B)/programas/mico/%.o,$(wildcard $(RAIZ)/userland/programas/mico/*.c))

GBM_OBJ    := $(patsubst $(RAIZ)/userland/gebramini/%.c,$(B)/obj/gebramini/%.o,$(wildcard $(RAIZ)/userland/gebramini/*.c))
PM_OBJ     := $(patsubst $(RAIZ)/userland/planomini/%.c,$(B)/obj/planomini/%.o,$(wildcard $(RAIZ)/userland/planomini/*.c))

APPS ?= mico pluma macaco tucan paint planomini gebramini arara
all: $(addprefix $(B)/,$(APPS)) $(B)/datos
# Un repositorio portable lleva las fuentes del sistema en datos/ (las siete DejaVu); el ejecutable las busca en ./datos.
$(B)/datos:
	@mkdir -p $(B); if [ -d $(RAIZ)/datos ]; then ln -sfn $(RAIZ)/datos $@; fi

$(B)/libsagui.a: $(LIB_OBJ)
	@rm -f $@ && ar rcs $@ $^

$(B)/%.o: $(RAIZ)/userland/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/libsagui/%.o: $(RAIZ)/userland/libsagui/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/libfontes/%.o: $(RAIZ)/userland/libfontes/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/ia/%.o: $(RAIZ)/userland/ia/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/obj/gebramini/%.o: $(RAIZ)/userland/gebramini/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/obj/planomini/%.o: $(RAIZ)/userland/planomini/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/libtls/%.o: $(RAIZ)/userland/libtls/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/kernel/lib/%.o: $(RAIZ)/kernel/lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/portable/%.o: $(RAIZ)/userland/portable/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/programas/%.o: $(RAIZ)/userland/programas/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# La fuente 8x16 de la interfaz se genera del texto (tools/fonte/); un repo portable lleva el .c ya generado.
$(B)/fonte8x16.c: $(RAIZ)/tools/fonte/sagui8x16.txt $(RAIZ)/tools/mkfonte.py
	@mkdir -p $(dir $@)
	python3 $(RAIZ)/tools/mkfonte.py $< $@
$(B)/fonte8x16.o: $(B)/fonte8x16.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(B)/programas/macaco.o $(B)/programas/pluma.o $(B)/programas/tucan.o $(B)/programas/arara.o: $(wildcard $(RAIZ)/userland/programas/*.h)

$(B)/mico:   $(MICO_OBJ) $(IA_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/pluma:  $(B)/programas/pluma.o $(FONTES_OBJ) $(IA_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/macaco: $(B)/programas/macaco.o $(FONTES_OBJ) $(IA_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/tucan:  $(B)/programas/tucan.o $(FONTES_OBJ) $(IA_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/paint:  $(B)/programas/paint.o $(FONTES_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/planomini: $(B)/programas/planomini.o $(PM_OBJ) $(GBM_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/gebramini: $(B)/programas/gebramini.o $(GBM_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)
$(B)/arara: $(B)/programas/arara.o $(B)/libsagui.a
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(B)
.PHONY: all clean

# ---------------------------------------------------------------------------------------------------------
# VisualPythonS: el MicroPython de SaguiOS en Linux (D459, apartado «VisualPythonS»).
#   make python        -> build/portable/python   (con SDL 2: el modulo sagui abre las ventanas por la capa)
# El nucleo de MicroPython (micropython_embed, MIT) y los *_sagui.c son los mismos que en SaguiOS; se compilan
# con las cabeceras y la libc del PC (no con userland/micropython/libc) y la libm del PC. Lo que pide el kernel
# (procesos, hilos del kernel) llega a sistema_nulo.c; la red, los hilos y TLS tienen version para el PC
# (red_sdl.c, hilos_pc.c).
MP_DIR     := $(RAIZ)/userland/micropython
MP_EMB     := $(MP_DIR)/micropython_embed
MP_CFLAGS  := -MMD -MP -std=gnu11 -O2 -DSAGUI_HOST -DSAGUI_PORTABLE -DNDEBUG -fno-strict-aliasing -fcommon -w -D_GNU_SOURCE \
  -finput-charset=UTF-8 -I$(MP_DIR) -I$(MP_EMB) -I$(MP_EMB)/port -I$(RAIZ)/userland/terceros/sqlite -idirafter $(RAIZ)/kernel/inc
MP_PROPRIOS := mp_sagui mpthreadport_pc arquivos_sagui os_sagui rede_sagui ssl_sagui hashlib_sagui janela_sagui sqlite_sagui \
  sinais_pc grafico_sagui
MP_NUCLEO  := $(wildcard $(MP_EMB)/py/*.c) $(MP_EMB)/shared/runtime/gchelper_generic.c $(wildcard $(MP_EMB)/extmod/*.c) \
              $(MP_EMB)/shared/timeutils/timeutils.c
# ulab (D479, MIT): el «NumPy» de MicroPython, el mismo que en SaguiOS (sin numpy.load/save: no hay VFS).
ULAB_DIR   := $(RAIZ)/userland/terceros/ulab
ULAB_SRC   := $(filter-out %/numpy/io/io.c,$(shell find $(ULAB_DIR)/code -name '*.c' | sort))
ULAB_OBJ   := $(patsubst $(ULAB_DIR)/%.c,$(B)/ulab/%.o,$(ULAB_SRC))
MP_OBJ     := $(ULAB_OBJ) $(patsubst $(MP_DIR)/%.c,$(B)/mp/%.o,$(MP_NUCLEO)) $(addprefix $(B)/mp/,$(addsuffix .o,$(MP_PROPRIOS))) $(B)/mp_ponte_sagui.o $(B)/portable/inicio_python.o $(B)/portable/red_sdl.o \
              $(B)/mp/sqlite3.o $(B)/libsagui/sqlite_vfs_sagui.o
TLS_OBJ    := $(patsubst $(RAIZ)/userland/libtls/%.c,$(B)/libtls/%.o,$(filter-out %/conexao.c %/sshcifra.c %/wireguard.c %/hash.c,$(wildcard $(RAIZ)/userland/libtls/*.c)))
SQLITE_DEFS := -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL \
  -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_OMIT_DEPRECATED \
  -DHAVE_LOCALTIME_R=1 -DSQLITE_OMIT_TRACE -DSQLITE_OMIT_UTF16 -DSQLITE_OMIT_DECLTYPE -DSQLITE_OMIT_INCRBLOB \
  -DSQLITE_OMIT_AUTHORIZATION -DSQLITE_TEMP_STORE=2 -DSQLITE_OMIT_PROGRESS_CALLBACK -DSQLITE_OMIT_TCL_VARIABLE \
  -DSQLITE_OMIT_BUILTIN_TEST

$(B)/mp/%.o: $(MP_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(MP_CFLAGS) -c -o $@ $<
$(B)/ulab/%.o: $(ULAB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(MP_CFLAGS) -DMODULE_ULAB_ENABLED=1 -DULAB_CONFIG_FILE='"ulab_sagui.h"' -c -o $@ $<
# ponte_sagui.c usa tls.h e la libsagui: se compila como la propia libsagui.
$(B)/mp_ponte_sagui.o: $(MP_DIR)/ponte_sagui.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<
$(B)/libsagui/sqlite_vfs_sagui.o: CFLAGS += -I$(RAIZ)/userland/terceros/sqlite
$(B)/libtls/aesni.o: CFLAGS += -maes -msse2
$(B)/mp/sqlite3.o: $(RAIZ)/userland/terceros/sqlite/sqlite3.c
	@mkdir -p $(dir $@)
	$(CC) $(MP_CFLAGS) $(SQLITE_DEFS) -Os -c -o $@ $<
# rapido_c.o y lo demas de la capa vienen de libsagui.a
$(B)/python: $(MP_OBJ) $(TLS_OBJ) $(FONTES_OBJ) $(B)/libsagui.a
	$(CC) -o $@ $(MP_OBJ) $(TLS_OBJ) $(FONTES_OBJ) $(B)/libsagui.a $(LDLIBS) -lpthread
-include $(shell find $(B)/mp -name '*.d' 2>/dev/null)
python: $(B)/python $(B)/datos
.PHONY: python
