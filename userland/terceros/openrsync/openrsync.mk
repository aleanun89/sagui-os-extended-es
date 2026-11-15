# SPDX-License-Identifier: GPL-3.0-only
# openrsync.mk - cómo se compila /apps/bin/rsync (D314). Lo incluye el Makefile principal.
#
# openrsync (https://github.com/kristapsdz/openrsync, VERSION_0_5_0, licencia ISC) es el
# rsync de OpenBSD: protocolo 27, un solo proceso por lado. Lo que usa SaguiOS (el modo
# servidor: lo lanza el sshd; el emisor y el receptor) está en fuentes/ con los parches
# anotados en PARCHES.md. Lo de fork/exec/socketpair (cliente ssh, demonio) no se compila.
# Lo nuestro: compat/ (la libc que le falta), principal_rsync.c (el main) y local_rsync.c
# (la copia local, sin protocolo). tools/openrsync_fuentes.sh --comprobar verifica el origen.

OR_DIR := userland/terceros/openrsync
OR_F   := $(OR_DIR)/fuentes
OR_SRC := $(addprefix $(OR_F)/, blocks.c downloader.c flist.c hash.c ids.c io.c log.c md4.c \
            mkpath.c receiver.c sender.c server.c session.c symlinks.c uploader.c) \
          $(OR_DIR)/principal_rsync.c $(OR_DIR)/local_rsync.c
OR_OBJ := $(patsubst %.c,$(B)/%.o,$(OR_SRC))
OR_HDR := $(wildcard $(OR_F)/*.h $(OR_DIR)/compat/*.h $(OR_DIR)/*.h)

# Con la x87 (hay dobles en las estadísticas) y la libc común; con -include se les añade
# la adaptación (compat/rsync_sagui.h) y -I compat da fts.h, poll.h, search.h y grp.h.
OR_FLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -Os -w -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -I$(OR_DIR)/compat -I$(OR_F) -I$(OR_DIR) \
  -D_GNU_SOURCE -include $(OR_DIR)/compat/rsync_sagui.h

$(OR_OBJ): $(B)/%.o: %.c $(OR_HDR) $(LIBC_HDR) $(OR_DIR)/openrsync.mk
	@mkdir -p $(dir $@)
	$(CC) $(OR_FLAGS) -c -o $@ $<

# La capa de compatibilidad no lleva -include (define los rs_*at que éste renombra).
# El cliente por ssh (cliente_rsync.c, D314 v8.4.2) se compila igual: usa libsagui (executar, tuberias).
OR_COMPAT_OBJ := $(B)/$(OR_DIR)/compat/compat_sagui.o $(B)/$(OR_DIR)/cliente_rsync.o
$(OR_COMPAT_OBJ): $(B)/%.o: %.c $(OR_HDR) $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -include $(OR_DIR)/compat/rsync_sagui.h,$(OR_FLAGS)) -Iuserland/inc -c -o $@ $<

$(B)/userland/bin/rsync: $(OR_OBJ) $(OR_COMPAT_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# D194: no es de la base: /apps/bin (la partición SAGUI-APPS), que está en el PATH.
APPS_BINS += $(B)/userland/bin/rsync
