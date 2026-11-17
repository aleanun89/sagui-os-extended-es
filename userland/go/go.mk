# SPDX-License-Identifier: GPL-3.0-only
# go.mk - soporte de Go con TinyGo (D364). Lo incluye el Makefile principal.
#
# Nada de esto se compila si TinyGo no está instalado (tools/tinygo_entorno.sh instalar): se
# salta con un aviso, como los datos opcionales. GO=0 lo desactiva del todo.
#   - $(B)/userland/libgo.a: libsagui + libc común + userland/go/runtime/glue.c, que es contra lo
#     que enlaza tools/go-sagui (el envoltorio de tinygo con el objetivo de SaguiOS).
#   - $(B)/userland/go/bin/<ejemplo>: cada carpeta de userland/go/ejemplos/ compilada y enlazada.
#   - Los ejemplos van a /apps/go/ (no al ramdisk base: son aplicaciones, ver D194/D324).

GO_DIR := userland/go
GO_GLUE_OBJ := $(B)/$(GO_DIR)/glue.o

$(GO_GLUE_OBJ): $(GO_DIR)/runtime/glue.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(LIBC_CFLAGS) -fno-tree-loop-distribute-patterns -Wall -Wno-unused-parameter \
	  -Iuserland/inc -idirafter kernel/inc -c -o $@ $<

$(B)/userland/libgo.a: $(LIBSAGUI_OBJ) $(LIBC_OBJ) $(GO_GLUE_OBJ)
	@mkdir -p $(dir $@)
	rm -f $@
	ar rcs $@ $^

GO_TINYGO := $(wildcard $(HOME)/.local/saguitools/tinygo/bin/tinygo)
GO ?= 1
GO_EJEMPLOS := $(patsubst $(GO_DIR)/ejemplos/%/main.go,%,$(wildcard $(GO_DIR)/ejemplos/*/main.go))
GO_FUENTES := $(wildcard $(GO_DIR)/ejemplos/*/*.go) $(GO_DIR)/ejemplos/go.mod \
  $(wildcard $(GO_DIR)/syscall/*.go) $(GO_DIR)/alvos/saguios.json $(GO_DIR)/runtime/parchear_tinygo.py \
  tools/go-sagui tools/tinygo_entorno.sh userland/programa.ld

.PHONY: go-ejemplos
ifeq ($(GO),1)
ifneq ($(GO_TINYGO),)
GO_BINS := $(addprefix $(B)/userland/go/bin/,$(GO_EJEMPLOS))

# Cada ejemplo es un paquete main en userland/go/ejemplos/<nombre>/.
$(GO_BINS): $(B)/userland/go/bin/%: $(B)/userland/libgo.a $(GO_FUENTES)
	@mkdir -p $(dir $@)
	tools/go-sagui build -o $@ ./$(GO_DIR)/ejemplos/$*

go-ejemplos: $(GO_BINS)

GO_RECURSOS := $(B)/go-res/.feito
$(GO_RECURSOS): $(GO_BINS) $(GO_DIR)/leeme.txt
	@mkdir -p $(B)/go-res
	cp $(GO_BINS) $(B)/go-res/
	cp $(GO_DIR)/leeme.txt $(B)/go-res/leeme.txt
	@touch $@
APPS_RECURSOS += go:$(B)/go-res
else
$(info Makefile: TinyGo no instalado: se salta Go (tools/tinygo_entorno.sh instalar, docs/go.md))
go-ejemplos:
	@echo "TinyGo no instalado: tools/tinygo_entorno.sh instalar (docs/go.md)"
endif
else
go-ejemplos:
	@echo "GO=0: Go desactivado"
endif
