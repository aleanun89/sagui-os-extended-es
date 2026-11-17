# SPDX-License-Identifier: GPL-2.0-or-later
# sdk_externo.mk - «make sdk_externo»: deja en $(B)/sdk/ lo que necesita quien compila una aplicación
# FUERA de este repositorio (D375: aplicaciones con licencia incompatible con la GPL v3, en repositorios
# aparte, p. ej. OpenTTD): los objetos de las bibliotecas propias (libc común, runtime de C++, libm,
# libsagui, SDL 2, SDL2_image/ttf, libfontes), las banderas exactas de compilación y de enlace y el
# guion de enlace. Todo con rutas absolutas, en $(B)/sdk/sdk.mk (variables de make) y sdk.sh (shell).
# Quien lo usa (la receta tools/openttd_paquete.sh) recibe la ruta por SAGUI_SDK=<copia>/build/sdk.

include userland/libsdl2/sdl2_capas.mk

SDK_DIR := $(B)/sdk
SDK_OBJS := $(CXXRT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) $(SDL2_OBJ)
SDK_CAPAS := $(SDL2CAPAS_OBJ) $(FONTES_OBJ)   # opcionales: SDL2_image y SDL2_ttf (esta pide la libfontes)
SDK_ABS = $(patsubst -I%,-I$(CURDIR)/%,$(patsubst -I$(CURDIR)/%,-I%,$1))
SDK_CXXFLAGS := $(call SDK_ABS,$(CXXFLAGS_SAGUI)) -I$(CURDIR)/userland/libsdl2
SDK_CFLAGS   := $(call SDK_ABS,$(LIBC_CFLAGS)) -I$(CURDIR)/userland/libsdl2

.PHONY: sdk_externo
sdk_externo: $(SDK_OBJS) $(SDK_CAPAS) $(CXXRT_ENVOLT)/.feito userland/programa_cxx.ld tools/elf32.py
	@mkdir -p $(SDK_DIR)
	@{ echo "# generado por «make sdk_externo» (userland/sdk_externo.mk); no editar"; \
	  echo "SAGUI_RAIZ := $(CURDIR)"; \
	  echo "SAGUI_CXXFLAGS := $(SDK_CXXFLAGS)"; \
	  echo "SAGUI_CFLAGS := $(SDK_CFLAGS)"; \
	  echo "SAGUI_LD := $(LD)"; \
	  echo "SAGUI_LDFLAGS := $(subst userland/programa_cxx.ld,$(CURDIR)/userland/programa_cxx.ld,$(ULDFLAGS_CXX))"; \
	  echo "SAGUI_OBJS := $(abspath $(SDK_OBJS))"; \
	  echo "SAGUI_OBJS_CAPAS := $(abspath $(SDK_CAPAS))"; \
	  echo "SAGUI_CHECK_ELF32 := $(CURDIR)/tools/elf32.py"; \
	} > $(SDK_DIR)/sdk.mk
	@sed -e 's/ := /=/' -e 's/^SAGUI_\([A-Z0-9_]*\)=\(.*\)$$/SAGUI_\1="\2"/' -e 's/^#.*//' $(SDK_DIR)/sdk.mk > $(SDK_DIR)/sdk.sh
	@echo "sdk_externo: $(SDK_DIR)/sdk.mk y sdk.sh listos"
