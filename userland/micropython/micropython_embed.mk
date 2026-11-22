# Gera a pasta micropython_embed/ a partir do repositório do MicroPython
# (sempre a versão 1.24.1, a mesma do resto; ver D034).
# Uso (só quando for atualizar o MicroPython, ou mudar o mpconfigport.h ou
# os nomes e módulos dos *_sagui.c):
#   make -f micropython_embed.mk MICROPYTHON_TOP=~/.local/src/mp
# O alvo "sagui" (o padrão) gera o pacote do embed port, apaga o
# port/embed_util.c e o port/mphalport.c (o SaguiOS tem os seus, D034) e
# copia os arquivos do extmod, lib e shared que o SaguiOS usa (D067).
MICROPYTHON_TOP ?= $(HOME)/.local/src/mp
SAGUI_TOP ?= ../..

# Hilos (D170): com MICROPY_PY_THREAD no mpconfigport.h, o py/modthread.c
# entra sozinho na coleta (o _thread e os seus nomes); o mpthreadport.h tem
# de estar ao lado do mpconfigport.h. Na D170 os nove nomes e o
# MODULE_DEF__THREAD foram acrescentados à mão nos genhdr/ (sem o
# repositório do MicroPython à mão), na ordem que o makeqstrdata.py usa.
# Os nomes (qstr) e os MP_REGISTER_MODULE dos arquivos do SaguiOS também
# precisam ser coletados (D038, D067).
SRC_QSTR += mp_sagui.c arquivos_sagui.c os_sagui.c sinais_sagui.c rede_sagui.c ssl_sagui.c hashlib_sagui.c janela_sagui.c sqlite_sagui.c
# Módulos do extmod usados (fontes copiados sem mudança).
EXTMOD_SAGUI = modjson.c modre.c modrandom.c modheapq.c modbinascii.c modselect.c modtime.c
SRC_QSTR += $(addprefix extmod/,$(EXTMOD_SAGUI))

.PHONY: sagui
sagui: micropython-embed-package
	$(RM) $(PACKAGE_DIR)/port/embed_util.c $(PACKAGE_DIR)/port/mphalport.c
	$(CP) $(addprefix $(TOP)/extmod/,$(EXTMOD_SAGUI) modtime.h) $(PACKAGE_DIR)/extmod/
	$(MKDIR) -p $(PACKAGE_DIR)/lib/re1.5 $(PACKAGE_DIR)/shared/timeutils
	$(CP) $(TOP)/lib/re1.5/*.[ch] $(PACKAGE_DIR)/lib/re1.5/
	$(CP) $(TOP)/shared/timeutils/timeutils.[ch] $(PACKAGE_DIR)/shared/timeutils/
	$(RM) -r $(BUILD)

# O abi.h e o sha256.h do SaguiOS (antes do embed.mk: o mkrules.mk copia o
# CFLAGS para a coleta dos qstr na hora em que é incluído).
CFLAGS += -I$(SAGUI_TOP)/kernel/inc -I$(SAGUI_TOP)/userland/terceros/sqlite

include $(MICROPYTHON_TOP)/ports/embed/embed.mk
