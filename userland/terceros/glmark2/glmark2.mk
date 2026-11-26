# SPDX-License-Identifier: GPL-3.0-only
# glmark2.mk - cómo se compila /apps/bin/glmark2-es2 (D481). Lo incluye el Makefile principal si hay compilador
# de C++ (userland/libcxx/libcxx.mk) y se bajaron las fuentes (tools/glmark2_fuentes.sh).
#
# glmark2 2023.01 (https://github.com/glmark2/glmark2, GPL-3.0-or-later) es el banco de pruebas de OpenGL ES 2.0.
# fuentes/src y fuentes/data son los del upstream con parches/*.patch. Sin meson: este .mk compila la «flavor»
# glesv2 con EGL y un estado nativo propio de SaguiOS (plataforma/native-state-sagui.cpp: la ventana es una jan_t
# de la libsagui, la superficie EGL de la SaguiGL 2.0). Cambios respecto a la flavor X11:
#   - plataforma/image-reader.cpp lee PNG y JPEG con stb_image (userland/terceros/stbimage) en vez de libpng y libjpeg;
#   - plataforma/shared-library.cpp no usa dlopen: los símbolos de EGL y GLES2 salen de eglGetProcAddress de la libsgl;
#   - main.cpp elige NativeStateSagui (parches/main-sagui.patch).
# Los datos (modelos, sombreadores, texturas: 10 MB) van a la partición de apps, /apps/glmark2/data.

GM_DIR := userland/terceros/glmark2
GM_J   := $(GM_DIR)/fuentes
GM_S   := $(GM_J)/src

GM_CPP := benchmark-collection benchmark canvas-generic gl-headers gl-visual-config main-loop mesh model options \
  results-file scene-buffer scene-build scene-bump scene-clear scene-conditionals scene scene-default-options \
  scene-desktop scene-effect-2d scene-function scene-grid scene-ideas scene-jellyfish scene-loop scene-pulsar \
  scene-refract scene-shading scene-shadow scene-terrain scene-texture text-renderer texture gl-state-egl main
GM_CPP_SUB := scene-terrain/base-renderer scene-terrain/blur-renderer scene-terrain/copy-renderer \
  scene-terrain/luminance-renderer scene-terrain/normal-from-height-renderer scene-terrain/overlay-renderer \
  scene-terrain/renderer-chain scene-terrain/simplex-noise-renderer scene-terrain/terrain-renderer \
  scene-terrain/texture-renderer
GM_CC := libmatrix/log libmatrix/mat libmatrix/program libmatrix/shader-source libmatrix/util \
  scene-ideas/a scene-ideas/d scene-ideas/e scene-ideas/i scene-ideas/lamp scene-ideas/logo scene-ideas/m \
  scene-ideas/n scene-ideas/o scene-ideas/s scene-ideas/splines scene-ideas/table scene-ideas/t
GM_C := glad/src/egl glad/src/gles2

GM_OBJ := $(addprefix $(B)/$(GM_S)/,$(addsuffix .o,$(GM_CPP) $(GM_CPP_SUB) $(GM_CC) $(GM_C))) \
  $(B)/$(GM_DIR)/plataforma/native-state-sagui.o $(B)/$(GM_DIR)/plataforma/image-reader.o \
  $(B)/$(GM_DIR)/plataforma/shared-library.o $(B)/$(GM_DIR)/plataforma/janela.o
GM_HDR := $(shell find $(GM_S) -name '*.h') $(GM_DIR)/plataforma/native-state-sagui.h

GM_DEFS := -DGLMARK2_USE_GLESv2 -DGLMARK2_USE_EGL -DGLMARK2_USE_SAGUI -DUSE_EXCEPTIONS -DUSE_OZONE \
  '-DGLMARK_VERSION="2023.01"' '-DGLMARK_DATA_PATH="/apps/glmark2/data"' '-DGLMARK2_EXECUTABLE="glmark2-es2"'
GM_INC := -I$(GM_S)/glad/include -I$(GM_S)/libmatrix -I$(GM_S)/scene-ideas -I$(GM_S)/scene-terrain -I$(GM_S) \
  -I$(GM_DIR)/plataforma -Iuserland/inc -idirafter kernel/inc -Iuserland/terceros/stbimage
GM_FLAGS := $(subst -Os,-O2,$(CXXFLAGS_SAGUI)) -fomit-frame-pointer $(GM_DEFS) $(GM_INC) -DNDEBUG -w \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1

$(B)/$(GM_S)/%.o: $(GM_S)/%.cpp $(GM_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(GM_DIR)/glmark2.mk
	@mkdir -p $(dir $@)
	$(CXX) $(GM_FLAGS) -c -o $@ $<
$(B)/$(GM_S)/%.o: $(GM_S)/%.cc $(GM_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(GM_DIR)/glmark2.mk
	@mkdir -p $(dir $@)
	$(CXX) $(GM_FLAGS) -c -o $@ $<
$(B)/$(GM_DIR)/plataforma/%.o: $(GM_DIR)/plataforma/%.cpp $(GM_HDR) $(CXXRT_HDR) $(CXXRT_ENVOLT)/.feito $(GM_DIR)/glmark2.mk
	@mkdir -p $(dir $@)
	$(CXX) $(GM_FLAGS) -c -o $@ $<
# glad es C (carga de EGL y GLES2 por eglGetProcAddress)
GM_CFLAGS := -m32 -march=i686 -std=gnu11 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -DNDEBUG -w \
  -mfpmath=387 -fexcess-precision=standard -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -DUSE_OZONE $(GM_INC) -I$(LIBC_DIR) -idirafter kernel/inc
$(B)/$(GM_S)/glad/src/%.o: $(GM_S)/glad/src/%.c $(GM_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GM_CFLAGS) -c -o $@ $<

$(B)/$(GM_DIR)/plataforma/janela.o: $(GM_DIR)/plataforma/janela.c $(GM_DIR)/plataforma/janela.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GM_CFLAGS) -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -c -o $@ $<

# El punto de entrada: principal() -> main() (el de glmark2). Sin más: no pide ROM.
GM_PRINCIPAL_OBJ := $(B)/$(GM_DIR)/plataforma/principal.o
$(GM_PRINCIPAL_OBJ): $(GM_DIR)/plataforma/principal.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GM_CFLAGS) -c -o $@ $<

$(B)/userland/bin/glmark2-es2: $(GM_OBJ) $(GM_PRINCIPAL_OBJ) $(SGL2_OBJ) $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa_cxx.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_CXX) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

GM_RECURSOS := $(B)/glmark2-res/.feito
$(GM_RECURSOS): $(GM_J)/COPYING $(GM_J)/.preparado $(GM_DIR)/leeme.txt $(GM_DIR)/benchmarks.txt
	@rm -rf $(B)/glmark2-res; mkdir -p $(B)/glmark2-res
	cp -r $(GM_J)/data $(B)/glmark2-res/data
	cp $(GM_J)/COPYING $(B)/glmark2-res/COPYING
	cp $(GM_DIR)/leeme.txt $(B)/glmark2-res/leeme.txt
	cp $(GM_DIR)/benchmarks.txt $(B)/glmark2-res/benchmarks.txt
	@touch $@

APPS_BINS += $(B)/userland/bin/glmark2-es2
APPS_RECURSOS += glmark2:$(B)/glmark2-res
