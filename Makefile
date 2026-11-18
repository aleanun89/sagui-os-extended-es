# SPDX-License-Identifier: GPL-3.0-only
# Makefile do SaguiOS
#
# Alvos principais:
#   make            -> gera build/saguios.img (imagem para pendrive)
#   make vdi        -> gera também build/saguios.vdi (VirtualBox)
#   make imagem-persistente [DATOS_MB=64]
#                   -> build/saguios-persistente.img: a mesma imagem com uma
#                      partição ext2 de dados que o SaguiOS monta em /home (D040)
#   make imagem-particoes [HOME_MB=2048 TMP_MB=256 LOGS_MB=128 PART_FS=fat32]
#                   -> build/saguios-particiones.img: disco para Rufus/dd con /home, /tmp e /logs (D120)
#   Tamaños por variable de entorno o de make (D181): HOME_MB TMP_MB LOGS_MB PART_FS
#   PART_EXT DATOS_MB DATOS_FAT32_MB JOGOS_MB (800; 0 = el justo) JOGOS_IMG_MB (saguios.img: 0)
#   JOGOS=0 (sin juegos) INPUT_SHARE (input_share; vacío = sin él, D228) OPENJAZZ_DATOS=0 OPENJAZZ=0
#   DOSBOX=0 (sin DOSBox ni el lanzador Recreativa, D294)
#   MGBA=0 (sin mGBA, el emulador de GBA, GB y GBC, D322; MPL-2.0, entra con PUBLICABLE=1)
#   GNUBOY=0 (sin el emulador de Game Boy, D235) CONQUER=0 (sin Command & Conquer, D230) CNC_VIDEOS=1 (con las películas de C&C: ~880 MB más en
#   /jogos; súbase JOGOS_MB, p. ej. JOGOS_MB=2000)
#   QUAKE_DATOS=0 QUAKE=0 (D229)
#   RAMDISK=base|completo (D324; completo = apps dentro del ramdisk, como APPS=0, y el formato de siempre)
#   RAIZ_DISCO=1 (por defecto, D379 M1): initramfs mínimo + raíz ext2 SAGUI-RAIZ (live desde el medio; el CD,
#                toram); RAIZ_DISCO=0: el ramdisk de siempre con todo el sistema (como hasta la v8.5.3)
#   PERSIST_MB (256): la capa persistente SAGUI-PERSIST de las imágenes persistentes (D379)
#   APPS_MB (0 = lo justo más margen) APPS=0 (las apps de /apps dentro del ramdisk, D194)
#   make variantes [DESTINO=entrega/v7.0b] -> todas las imágenes en una carpeta (gerar_versao.sh --todas)
#   make imagem-smp -> build/saguios-smp.img: la misma imagen con --smp (D164; sin IOAPIC)
#   make imagem-persistente-smp -> las dos persistentes (ext2 y FAT32) con --smp
#   make imagem-uacpi -> build/saguios-uacpi.img: la misma imagen con --uacpi (D192: intérprete
#                      de AML e indicador de batería, desactivados por defecto)
#   make test       -> roda todos os testes automáticos no QEMU
#   make rodar      -> abre o sistema no QEMU (precisa de janela; na VPS use os testes)
#   make entrega FASE=N -> copia .img, .vdi e SHA-256 para entrega/faseN/
#   make vazio      -> alvo vazio (critério da H01)
#   make limpar     -> apaga build/

# El objetivo por defecto es siempre «all». Sin esto, en un arbol sin las fuentes de NetSurf (el
# repositorio publico) el primero que se define en un .mk incluido (p. ej. un .o de gprolog) pasaba
# a ser el objetivo por defecto y «make» no generaba la imagen (D261).
.DEFAULT_GOAL := all

# O GCC do sistema em modo 32 bits e freestanding faz o papel do compilador
# cruzado (ver docs/decisoes.md, D001).
CC      := gcc
AS      := nasm
LD      := ld
PY      := python3

# Comprobacao: o kernel e os programas de utilizador sao ELF de 32 bits
# (i386). Se verifica a classe do ELF depois de ligar, para que um gcc que
# perdesse o -m32 nao pase despercebido.
CHECK_ELF32 := tools/elf32.py

# Las cabeceras del anfitrión no deben colarse en el kernel ni en el userland:
# GCC añade CPATH y C_INCLUDE_PATH incluso con -nostdinc, y el env.sh del
# entorno local las define (D184: NetSurf compilaba contra la glibc sin aviso).
# Lo de dentro del SaguiOS no ve esas rutas; las herramientas que corren en el
# PC (HOSTCC) las reciben explícitamente en HOST_CPATH (D191).
HOST_CPATH := $(CPATH)
unexport CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH
override CPATH :=
override C_INCLUDE_PATH :=

# Nivel de otimizacao. Por defecto -O2; se pode mudar na linha de comando
# (make OTIMIZACAO=-Os) ou pelo guion gerar_versao.sh (--otimizacao). O
# MicroPython e o litemonkey usam os seus proprios niveis (-Os), porque o
# codigo de terceiros nao se mexe.
OTIMIZACAO ?= -O2

CFLAGS  := -m32 -march=i686 -std=gnu11 $(OTIMIZACAO) -g \
           -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
           -fno-stack-protector -fno-asynchronous-unwind-tables \
           -mgeneral-regs-only -fno-omit-frame-pointer \
           -Wall -Wextra -Werror=implicit-function-declaration \
           -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 \
           -DSAGUI_KERNEL -Ikernel/inc
LDFLAGS := -m elf_i386 -nostdlib --no-dynamic-linker -z noexecstack --no-warn-rwx-segments

# Variantes de licencia (mejora 057, D109; D258): SEM_GPL=1 deja fuera todo el
# codigo de terceros con licencia GPL (los ports OpenTyrian, NetSurf, gprolog,
# OpenJazz, Vanilla Conquer, Quake y GNUBoy, y kernel/terceros/linux/ mientras exista;
# el codigo propio es GPL y entra siempre) y compila en su propia carpeta (build-sem-gpl/) para no mezclar objetos con
# los de la imagen normal. SEM_FIRMWARE=1 hace lo mismo con el firmware
# binario (firmware/); hoy no hay ninguno, pero la opción ya existe para
# que las piezas futuras la respeten.
SEM_GPL      ?= 0
SEM_FIRMWARE ?= 0
ifeq ($(SEM_GPL),1)
B       ?= build-sem-gpl
VARIANTE_DEFS := -DSAGUI_SEM_GPL
else ifeq ($(SEM_FIRMWARE),1)
B       ?= build-sem-firmware
VARIANTE_DEFS :=
else
B       ?= build
VARIANTE_DEFS :=
endif
ifeq ($(SEM_FIRMWARE),1)
VARIANTE_DEFS += -DSAGUI_SEM_FIRMWARE
endif
# D215: el firmware del pincho Wi-Fi RTL8188EU (linux-firmware, redistribuible,
# no libre) va en /lib/firmware del ramdisk con su licencia al lado. Lo baja y
# comprueba tools/firmware_rtl8188eu.sh; con SEM_FIRMWARE=1 no entra y el
# driver dice que falta.
ifeq ($(SEM_FIRMWARE),1)
FIRMWARE_RAMDISK :=
FIRMWARE_RADEON :=
else
FIRMWARE_RAMDISK := firmware/realtek/rtl8188eufw.bin firmware/realtek/LICENCE.rtlwifi_firmware.txt
# D284/D287: microcódigo del procesador de órdenes (CP) de las Radeon (R7xx y R520_cp.bin de las R5xx),
# de linux-firmware (tools/firmware_radeon.sh y tools/firmware_r5xx.sh), en /lib/firmware/radeon.
FIRMWARE_RADEON := $(wildcard firmware/radeon/*.bin) firmware/radeon/LICENSE.radeon.txt
endif
# D259: PUBLICABLE=1 es lo que pasa gerar_versao.sh sin --con-input-share (lo que se
# publica). Deja fuera lo que no puede ir en un binario GPL v3: NetSurf y GNUBoy (GPL-2.0
# solo la v2, userland/terceros/) y el r8169 derivado de Linux (GPL-2.0-only,
# kernel/terceros/linux/r8169/: el driver usa el inicio clasico propio, r8169_sem_gpl.h).
# En lo privado (make all de desarrollo, priv_variantes) entra todo.
PUBLICABLE ?= 0
ifeq ($(PUBLICABLE),1)
VARIANTE_DEFS += -DSAGUI_PUBLICABLE
NETSURF := 0
GNUBOY  := 0
endif
CFLAGS  += $(VARIANTE_DEFS)

# uACPI (D192): intérprete de AML de terceros (MIT) en kernel/terceros/uacpi/,
# sin cambios. Las mismas definiciones para uACPI y para la capa propia que lo
# usa (kernel/dispositivos/uacpi_sagui.c, ec.c, bateria.c): con direcciones
# físicas de 32 bits cambian los tipos de la interfaz. Los tipos salen de
# kernel/inc/uacpi_types.h (UACPI_OVERRIDE_TYPES), no de <stdint.h>.
UACPI_DEFS := -Ikernel/terceros/uacpi/include -DUACPI_PHYS_ADDR_IS_32BITS -DUACPI_OVERRIDE_TYPES
CFLAGS  += $(UACPI_DEFS)

KERNEL_C   := $(shell find kernel -name '*.c' | sort)
KERNEL_ASM := $(shell find kernel -name '*.asm' | sort)
ifeq ($(SEM_GPL),1)
KERNEL_C   := $(filter-out kernel/terceros/linux/%,$(KERNEL_C))
KERNEL_ASM := $(filter-out kernel/terceros/linux/%,$(KERNEL_ASM))
endif
KERNEL_OBJ := $(patsubst %.c,$(B)/%.o,$(KERNEL_C)) $(patsubst %.asm,$(B)/%.asm.o,$(KERNEL_ASM)) \
              $(B)/gerado/fonte8x16.o
KERNEL_HDR := $(shell find kernel -name '*.h')
ROOTFS     := $(shell find rootfs -type f | sort)

# Programas de usuário: libsagui + um .c por programa em userland/programas/
# (vão para /bin) e em userland/testes/ (vão para /testes).
# D378: con PIC=1 (solo lo usa la submake que compila las bibliotecas compartidas en $(B)/pic) el
# código sale con -fPIC; si no, como siempre.
CODIGO_PIC := $(if $(filter 1,$(PIC)),-fPIC,-fno-pic -fno-pie)
UCFLAGS := -m32 -march=i686 -std=gnu11 $(OTIMIZACAO) \
           -ffreestanding -fno-builtin -nostdlib $(CODIGO_PIC) \
           -fno-stack-protector -fno-asynchronous-unwind-tables -mgeneral-regs-only \
           -ffunction-sections -fdata-sections \
           -Wall -Wextra -Werror=implicit-function-declaration \
           -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 \
           -Iuserland/inc -idirafter kernel/inc $(VARIANTE_DEFS)
ULDFLAGS := -m elf_i386 -nostdlib -T userland/programa.ld --gc-sections -z max-page-size=4096 \
            -z noexecstack --no-warn-rwx-segments
LIBSAGUI_OBJ := $(B)/userland/fonte8x16.o $(B)/userland/libsagui/sse2.o $(B)/userland/libsagui/sse1.o $(B)/userland/libsagui/sse3.o $(B)/userland/libsagui/ssse3.o $(B)/userland/libsagui/fuso.o $(B)/userland/libsagui/catalogo.o \
                $(patsubst userland/libsagui/%.c,$(B)/userland/libsagui/%.o,$(filter-out userland/libsagui/sqlite_% $(if $(filter 1,$(SEM_GPL)),userland/libsagui/h264.c userland/libsagui/aac.c),$(wildcard userland/libsagui/*.c))) \
                $(patsubst userland/libsagui/%.asm,$(B)/userland/libsagui/%.asm.o,$(wildcard userland/libsagui/*.asm))
# D378: la libsagui se parte en dos para el enlace dinámico (libsagui.so.1 y libsagui_medios.so.1): la base
# es lo que usan muchos programas (arranque, llamadas, texto, memoria, ventanas, imágenes, red, compresión) y no
# depende de la otra; «medios» es el resto (sonido y vídeo, documentos, particiones, saguimark...). Los programas
# estáticos siguen enlazando LIBSAGUI_OBJ entero, como siempre.
LIBSAGUI_BASE_NOMES := ambiente catalogo chamadas compressao conf_maquina contas div64 empacotar entrada formatar fuso gui \
  gui_arquivos gui_barras gui_revisar hilos hora http icones idioma imagem imagem_gif imagem_idct imagem_jpeg \
  inicio.asm mandos memoria rapido rede regex servicos sincronia sse1 sse2 sse3 ssse3 texto utilitarios zonas
LIBSAGUI_BASE_OBJ := $(B)/userland/fonte8x16.o $(addprefix $(B)/userland/libsagui/,$(addsuffix .o,$(LIBSAGUI_BASE_NOMES)))
LIBSAGUI_MEDIOS_OBJ := $(filter-out $(LIBSAGUI_BASE_OBJ),$(LIBSAGUI_OBJ))
USER_HDR  := $(wildcard userland/inc/*.h) kernel/inc/abi.h kernel/inc/sha256.h kernel/inc/fuso.h kernel/inc/versao.h

# La libc común (D210, mejora 094 A): stdio con búfer, printf completo, strtod,
# errno, qsort, stat, opendir, fechas, getenv, atexit, realpath, mmap... sobre
# la libsagui. Nació como la libc del port de NetSurf (D184) y es nuestra (no
# es de terceros). Con la x87, como los ports que la usan; los números de
# doble precisión salen de la libm del MicroPython (QJS_LIBM). Cada port la
# enlaza en vez de su libc propia: sus cabeceras van en -nostdinc -I$(LIBC_DIR).
LIBC_DIR := userland/libc
LIBC_SRC := $(wildcard $(LIBC_DIR)/*.c)
LIBC_OBJ := $(patsubst $(LIBC_DIR)/%.c,$(B)/$(LIBC_DIR)/%.o,$(LIBC_SRC))
LIBC_HDR := $(wildcard $(LIBC_DIR)/*.h $(LIBC_DIR)/*/*.h)
LIBC_CFLAGS := -m32 -march=i686 -std=gnu99 -Os \
  -ffreestanding -fno-builtin -nostdlib $(CODIGO_PIC) \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -I$(LIBC_DIR) -isystem $(shell $(CC) -print-file-name=include) \
  -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
$(B)/$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(LIBC_CFLAGS) -fno-tree-loop-distribute-patterns -Wall -Wno-unused-parameter \
	  -Iuserland/inc -idirafter kernel/inc -c -o $@ $<

# Os dados da compilação (data, hora, commit) entram no cabeçalho gerado.
COMPILACAO_H := $(B)/gerado/compilacao.h
# libtls (D060-D063): criptografia, X.509 e TLS, só nos programas que usam rede cifrada.
TLS_OBJ   := $(patsubst userland/libtls/%.c,$(B)/userland/libtls/%.o,$(wildcard userland/libtls/*.c)) \
             $(B)/userland/libtls/sha256.o
TLS_PROGS := sshd ssh ssh-keygen wg curl wget certificados cipo sha256sum ia chatia configuracion correo jclicbajar sintonia pluma macaco tucan traductor debug-app
# O cliente de IA (D134): a biblioteca da API compativel com a da OpenAI, que
# o `ia` do terminal e o assistente do Mico partilham. Fica numa pasta propria
# (como a libtls) porque so os programas que falam com a IA a ligam.
IA_OBJ   := $(patsubst userland/ia/%.c,$(B)/userland/ia/%.o,$(filter-out userland/ia/agente_panel.c,$(wildcard userland/ia/*.c)))
IA_GUI_OBJ := $(B)/userland/ia/agente_panel.o
# O cliente de correio (082): os protocolos (SMTP, POP3, IMAP) e o MIME, numa
# pasta propria (como a libtls), porque so o programa `correo` os liga.
CORREO_OBJ := $(patsubst userland/mail/%.c,$(B)/userland/mail/%.o,$(wildcard userland/mail/*.c))
PROGRAMAS := $(patsubst userland/programas/%.c,$(B)/userland/bin/%,$(wildcard userland/programas/*.c))
# O Sagui 3D (B4) tem vários arquivos, numa pasta própria.
SAGUI3D_OBJ := $(patsubst userland/sagui3d/%.c,$(B)/userland/sagui3d/%.o,$(wildcard userland/sagui3d/*.c))
PROGRAMAS += $(B)/userland/bin/sagui3d
PROGRAMAS += $(B)/userland/bin/llm
# O Mico (mejora 037, D075), editor de código, também tem vários arquivos.
MICO_OBJ := $(patsubst userland/programas/mico/%.c,$(B)/userland/programas/mico/%.o,$(wildcard userland/programas/mico/*.c))
PROGRAMAS += $(B)/userland/bin/mico
# O motor de PDF (melhora 059, D138): fica em userland/pdf/ e usa a FPU x87 (dobres),
# por isso não entra na libsagui comum (que vai sem -mgeneral-regs-only, D076). Só
# o programa visorpdf liga com ele, como o sagui3d e o cc fazem com os seus.
PDF_DIR  := userland/pdf
PDF_OBJ  := $(patsubst $(PDF_DIR)/%.c,$(B)/$(PDF_DIR)/%.o,$(wildcard $(PDF_DIR)/*.c))
PDF_HDR  := $(wildcard $(PDF_DIR)/*.h)
PDF_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -I$(PDF_DIR)
# O motor da web do Cipó (melhoria 081): vem do repositório robson-apk/sagui-os
# (ramo cipo-web; ver userland/web/ORIGEM.md). Fica em userland/web/ e usa a FPU
# x87 (dobles), por isso não entra na libsagui comum (que vai sem
# -mgeneral-regs-only, D076). Só o programa cipo liga com ele, como o visorpdf.
WEB_DIR  := userland/web
WEB_OBJ  := $(patsubst $(WEB_DIR)/%.c,$(B)/$(WEB_DIR)/%.o,$(wildcard $(WEB_DIR)/*.c))
WEB_HDR  := $(wildcard $(WEB_DIR)/*.h)
WEB_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -I$(WEB_DIR) -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
  -Wno-sign-compare -Wno-missing-field-initializers
# O QuickJS (D149): motor de JavaScript de terceiros (bellard/quickjs, licença
# MIT; ver userland/web/quickjs/ORIGEM.md). Vem escrito para um sistema com
# libc: desde a D210 usa a libc comum (LIBC_OBJ, com as suas cabeceras), que
# já tem o printf e o strtod exatos. Compila com a x87, porque os números são
# de dupla precisão. QJS_OBJ leva os objetos da libc (o cipo e o qjs ligam-nos).
QJS_DIR  := userland/web/quickjs
QJS_SRC  := $(QJS_DIR)/quickjs.c $(QJS_DIR)/cutils.c $(QJS_DIR)/libregexp.c \
            $(QJS_DIR)/libunicode.c $(QJS_DIR)/dtoa.c $(QJS_DIR)/qjs_bridge.c
QJS_OBJ  := $(patsubst $(QJS_DIR)/%.c,$(B)/$(QJS_DIR)/%.o,$(QJS_SRC)) $(LIBC_OBJ)
QJS_HDR  := $(wildcard $(QJS_DIR)/*.h) $(LIBC_HDR)
# A libm de dupla precisão, na variante x87 (o ABI dos argumentos em xmm não
# serve para quem chama com a x87): a mesma que usa o python-x87.
QJS_LIBM = $(patsubst $(MP_DIR)/libm_dbl/%.c,$(B)/x87/$(MP_DIR)/libm_dbl/%.o,$(wildcard $(MP_DIR)/libm_dbl/*.c)) \
           $(B)/x87/$(MP_DIR)/libm_sagui.o
QJSFLAGS := -m32 -march=i686 -std=gnu11 -Os -ffreestanding -fno-builtin -nostdlib \
  -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -DCONFIG_VERSION='"$(shell cat $(QJS_DIR)/VERSION)"' \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -I$(QJS_DIR) -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -D_DEFAULT_SOURCE \
  -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
  -Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-function
# O sagsh (D037) também tem vários arquivos; /bin/sh é uma cópia dele.
SAGSH_OBJ := $(patsubst userland/sagsh/%.c,$(B)/userland/sagsh/%.o,$(wildcard userland/sagsh/*.c))
PROGRAMAS += $(B)/userland/bin/sagsh $(B)/userland/bin/sh
# A prova da rota v6 do SaguiGL (texturas, mistura, neblina e luzes, D130): o
# programa está em tests/sgl/ porque o mesmo código compila no anfitrião.
PROGRAMAS += $(B)/userland/bin/sglv6
# SQLite 3 (mejora 064, D116): la amalgamación de dominio público en userland/terceros/sqlite/,
# con el VFS propio (sqlite_vfs_sagui.c) y la libc común (D210; hasta entonces, sqlite_libc.c y
# unas cabeceras mínimas). Esos objetos no entran en la libsagui común: los ligan solo el
# programa sqlite3 y el python. Se compilan con la x87 (sin -mgeneral-regs-only ni SSE2), que
# sirve igual al python con SSE2 y al python-x87. El programa sqlite3 es propio y pequeño
# (userland/programas/sqlite3.c).
SQ_DIR := userland/terceros/sqlite
SQLITE_OBJ := $(B)/$(SQ_DIR)/sqlite3.o $(B)/userland/libsagui/sqlite_vfs_sagui.o
SQLITE_DEFS := -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL \
               -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_MAX_MMAP_SIZE=0 -DSQLITE_OMIT_SHARED_CACHE \
               -DSQLITE_OMIT_DEPRECATED -DHAVE_LOCALTIME_R=1 -DSQLITE_OMIT_TRACE -DSQLITE_OMIT_UTF16 \
               -DSQLITE_OMIT_DECLTYPE -DSQLITE_OMIT_INCRBLOB -DSQLITE_OMIT_AUTHORIZATION \
               -DSQLITE_TEMP_STORE=2 -DSQLITE_OMIT_PROGRESS_CALLBACK -DSQLITE_OMIT_TCL_VARIABLE \
               -DSQLITE_OMIT_BUILTIN_TEST -DNDEBUG
SQFLAGS := $(filter-out -mgeneral-regs-only -Wall -Wextra,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -Os
# MicroPython (/bin/python): o núcleo vem gerado em userland/micropython/
# micropython_embed (código de terceiros, licença MIT, ver D034). Para uma
# imagem sem ele: make MICROPYTHON=0. O ponto flutuante (D038) usa o SSE2:
# o MicroPython é compilado sem -mgeneral-regs-only e com a libm_dbl (musl/
# FreeBSD, também de terceiros). A libsagui ligada a ele segue sem SSE.
MICROPYTHON ?= 1
MP_DIR := userland/micropython
# A biblioteca padrão (D067-D069): módulos do extmod copiados sem mudança e
# o pegamento próprio (*_sagui.c): arquivos, os, socket, ssl e hashlib. A
# ponte_sagui.c liga o ssl e o hashlib à libtls (compilada como programa).
MP_PROPRIOS := mp_sagui mpthreadport arquivos_sagui os_sagui rede_sagui ssl_sagui hashlib_sagui janela_sagui sqlite_sagui
MP_SRC := $(wildcard $(MP_DIR)/micropython_embed/py/*.c) $(MP_DIR)/micropython_embed/shared/runtime/gchelper_generic.c \
          $(wildcard $(MP_DIR)/micropython_embed/extmod/*.c) $(MP_DIR)/micropython_embed/shared/timeutils/timeutils.c \
          $(wildcard $(MP_DIR)/libm_dbl/*.c) $(MP_DIR)/libm_sagui.c $(patsubst %,$(MP_DIR)/%.c,$(MP_PROPRIOS))
# D210: la libc es la común (LIBC_OBJ, con la x87; sirve igual a las dos variantes).
MP_OBJ := $(patsubst %.c,$(B)/%.o,$(MP_SRC)) $(SQLITE_OBJ) $(B)/$(MP_DIR)/inicio_python.o \
          $(B)/$(MP_DIR)/ponte_sagui.o
MP_HDR := $(wildcard $(MP_DIR)/*.h $(MP_DIR)/libc/*.h $(MP_DIR)/micropython_embed/*/*.h) kernel/inc/abi.h
MPCFLAGS := -m32 -march=i686 -std=gnu99 -Os \
            -ffreestanding -fno-builtin -nostdlib $(CODIGO_PIC) \
            -fno-stack-protector -fno-asynchronous-unwind-tables -msse -msse2 -mfpmath=sse \
            -ffunction-sections -fdata-sections -DNDEBUG \
            -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
            -I$(MP_DIR)/libc -I$(MP_DIR) -I$(MP_DIR)/micropython_embed -I$(MP_DIR)/micropython_embed/port \
            -idirafter kernel/inc -Iuserland/terceros/sqlite
# Variante sem SSE2 (D082): os mesmos fontes compilados com a FPU x87
# (-mfpmath=387) em $(B)/x87/, para o Pentium III, o Athlon XP e afins.
MPCFLAGS_X87 := $(filter-out -msse -msse2 -mfpmath=sse,$(MPCFLAGS)) -mfpmath=387 -DSAGUI_X87
MP_OBJ_X87 := $(patsubst $(B)/%,$(B)/x87/%,$(filter-out $(B)/$(MP_DIR)/ponte_sagui.o $(SQLITE_OBJ),$(MP_OBJ))) \
              $(B)/$(MP_DIR)/ponte_sagui.o $(SQLITE_OBJ) $(LIBC_OBJ)
MP_OBJ += $(LIBC_OBJ)
ifeq ($(MICROPYTHON),1)
PROGRAMAS += $(B)/userland/bin/python $(B)/userland/bin/python-x87
APPS_MOVER += python python-x87
endif
# CHIP-8 (D070): o emulador e o montador (regras mais abaixo).
PROGRAMAS += $(B)/userland/bin/chip8 $(B)/userland/bin/chip8asm
# lite-monkey (mejora 067, D117): servidor de documentos en el puerto 7979. El
# núcleo es el mismo C99 de litemonkey/ que se compila en el PC; aquí, con la
# capa plataforma_sagui.c. Usa la FPU x87 (dobles), así que sin -mgeneral-regs-only,
# y sus textos van en UTF-8 (los mensajes de error viajan en el BSON; el registro
# en pantalla se pasa a Latin-1 en plataforma_sagui.c).
LMK_SRC := $(wildcard litemonkey/nucleo/*.c) litemonkey/principal.c litemonkey/plataforma_sagui.c
LMK_OBJ := $(patsubst litemonkey/%.c,$(B)/litemonkey/%.o,$(LMK_SRC))
LMK_HDR := $(wildcard litemonkey/nucleo/*.h) litemonkey/plataforma.h
LMK_PY  := $(wildcard litemonkey/python/litemonkey/*.py) litemonkey/python/lmk
PROGRAMAS += $(B)/userland/bin/litemonkey
# D378: las pruebas del enlace dinámico (dlprueba, dinamico y sus .so) van a /apps/testes, no al ramdisk.
DL_PROG_TESTE := $(B)/userland/testes/dlprueba $(B)/userland/testes/dinamico $(B)/userland/testes/compartidas
PROG_TESTE := $(filter-out $(DL_PROG_TESTE),$(patsubst userland/testes/%.c,$(B)/userland/testes/%,$(wildcard userland/testes/*.c)))
# O OpenTyrian (D156): um jogo de terceiros (opentyrian/opentyrian), com
# licença GPL-2.0, por isso vive isolado em userland/terceros/opentyrian/ e
# NÃO entra na variante SEM_GPL=1 (que é sem nada de GPL, D109). O motor é o
# código do upstream, sem mudanças; o que é nosso está em plataforma/ (a SDL
# 1.2 do SaguiOS) e em libc/ (a libc mínima que o jogo precisa).
#
# O jogo usa números reais, então compila com a x87 (sem -mgeneral-regs-only),
# como o motor de PDF e o motor da web. A libm é a mesma do MicroPython, na
# variante x87 (a que usa o QuickJS).
OT_DIR      := userland/terceros/opentyrian
OT_JOGO_SRC := $(wildcard $(OT_DIR)/jogo/*.c)
OT_PLAT_SRC := $(OT_DIR)/plataforma/sdl_sagui.c $(OT_DIR)/plataforma/principal_ot.c
# D210: la libc es la común (userland/libc); antes traía la suya en libc/.
OT_OBJ := $(patsubst %.c,$(B)/%.o,$(OT_JOGO_SRC)) \
          $(patsubst %.c,$(B)/%.o,$(OT_PLAT_SRC)) \
          $(LIBC_OBJ)
OT_HDR := $(wildcard $(OT_DIR)/jogo/*.h) $(wildcard $(OT_DIR)/plataforma/*.h) \
          $(LIBC_HDR)
OT_LIBM := $(QJS_LIBM)
OT_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -fcommon \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -I$(OT_DIR)/jogo -I$(OT_DIR)/plataforma -I$(LIBC_DIR) \
  -Iuserland/inc -idirafter kernel/inc \
  -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
  -Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-function \
  -Wno-old-style-definition -Wno-int-conversion
# D181, D228: la partición de juegos SAGUI-JOGOS (ext2, lectura y escritura, montada en
# /jogos con un juego por carpeta). JOGOS_MB es su tamaño en las imágenes persistentes,
# la de particiones y lo que crea el instalador (800 MB: sitio para meter desde el
# SaguiOS la versión completa de un juego); JOGOS_IMG_MB, el de saguios.img y sus
# hermanas (-smp, -uacpi), que van a git en entrega/vX y las usan las pruebas (h151
# mide la meta de 38 MB): 0 = el justo para lo que lleva. JOGOS=0, sin ella.
JOGOS_MB ?= 800
JOGOS_IMG_MB ?= 0
JOGOS ?= 1
# D228: lo que pone en la imagen quien compila (docs/input_share.md): input_share/games/<carpeta>
# va a /jogos/<carpeta> y input_share/apps/ a /apps. Nunca va a git (.gitignore).
# INPUT_SHARE= (vacío) lo deja fuera: es lo que hace gerar_versao.sh con las entregas.
INPUT_SHARE ?= input_share

ifeq ($(SEM_GPL),1)
# A imagem sem GPL não leva o jogo (D109 e D156), nem a partição dos seus dados (D181).
JOGOS_DEP :=
JOGOS_OPC :=
JOGOS_OPC_IMG :=
else
PROGRAMAS += $(B)/userland/bin/opentyrian
APPS_MOVER += opentyrian
# D181: os dados do jogo (o shareware, 6,9 MB) não vão no ramdisk (que se copia
# inteiro para a RAM): vão numa partição ext2 da própria imagem, SAGUI-JOGOS. D228: el
# /etc/fstab la monta en /jogos (lectura y escritura), con los datos del OpenTyrian en
# /jogos/opentyrian (salen de la imagen versionada del disco aparte, D156, con el
# mtools), los de OpenJazz en /jogos/openjazz (si se bajaron) y lo de input_share/games.
JOGOS_DEP := $(B)/jogos/.feito
JOGOS_OPC := --jogos $(B)/jogos/raiz --jogos-mb $(JOGOS_MB)
JOGOS_OPC_IMG := --jogos $(B)/jogos/raiz --jogos-mb $(JOGOS_IMG_MB)
endif
ifeq ($(JOGOS),0)
JOGOS_DEP :=
JOGOS_OPC :=
JOGOS_OPC_IMG :=
endif

# GNU Prolog 1.5.0 (D189): /apps/bin/gprolog (el intérprete) y /apps/bin/pl2wam (lo que
# consult/1 lanza para compilar un .pl). Su licencia es LGPL-3 o GPL-2 a
# elegir: vive aislado en userland/terceros/gprolog/ y no entra en la
# variante SEM_GPL=1 (D109). GPROLOG=0 lo deja fuera de la imagen.
GPROLOG ?= 1
GP_APPS :=
ifneq ($(SEM_GPL),1)
ifeq ($(GPROLOG),1)
include userland/terceros/gprolog/gprolog.mk
PROGRAMAS += $(B)/userland/bin/gprolog $(B)/userland/bin/pl2wam
GP_APPS := gprolog pl2wam
endif
endif

# D194: las aplicaciones que no son de la base no van en el ramdisk (que se
# copia entero a la RAM al arrancar): van a la partición SAGUI-APPS de la
# imagen (ext2, lectura y escritura), que el /etc/fstab monta en /apps. Cada
# app deja su ejecutable en /apps/bin (que está en el PATH del shell) y sus
# datos en /apps/<app>/. Con APPS=0 el mismo árbol va dentro del ramdisk, en
# /apps: las rutas son las mismas y la imagen es de un solo bloque.
#   APPS_PROGRAMAS: programas de userland/programas/ que van a /apps/bin.
#   APPS_BINS:      todos los ejecutables de /apps/bin (los de terceros se añaden
#                   en su .mk, como NetSurf).
#   APPS_RECURSOS:  "app:carpeta" -> se copia la carpeta a /apps/<app>/.
# D324: perfil del ramdisk. «base» (por defecto): solo lo fundamental (las aplicaciones van a la
# partición SAGUI-APPS). «completo»: las mismas aplicaciones dentro del ramdisk, en /apps, sin
# partición (equivale a APPS=0). La ISO (el kernel no lee el CD, D077) es siempre «completo»:
# su ramdisk (ramdisk-cd.sgfs) lleva la base mas /apps.
RAMDISK ?= base
ifeq ($(RAMDISK),completo)
APPS := 0
endif
APPS ?= 1
APPS_MB ?= 0
# gprolog y pl2wam solo si se compilan (no con SEM_GPL=1 ni con GPROLOG=0).
# sintonia (D225): el reproductor de música propio; la radio por HTTPS le pide la libtls (TLS_PROGS).
# D324: lo que se mueve del ramdisk a /apps para adelgazarlo (el ramdisk entero se copia a la
# RAM al arrancar). APPS_MOVER acumula, ademas, lo que solo existe segun la configuracion
# (opentyrian, python). La lista y los MB de cada cosa estan en docs/decisoes.md (D324).
APPS_MOVER_FIJO := cipo pluma macaco tucan mico correo planomini gebramini visorpdf chatia ia \
  reproductor musica camara grabadora paint pascal cc spider sagui3d buscaminas banana sgldemo \
  chip8 chip8asm jclic jclicbajar traductor convertir-audio jabuti llm
APPS_PROGRAMAS := sqlite3 qjs sintonia $(GP_APPS) $(APPS_MOVER_FIJO) $(APPS_MOVER)
APPS_BINS := $(addprefix $(B)/userland/bin/,$(APPS_PROGRAMAS))
APPS_RECURSOS := videos:userland/apps/videos

# La libfontes (D196): el motor de fuentes vectoriales (TrueType/OpenType-CFF/WOFF,
# el del Cipó sacado de userland/web/) y su API sencilla. Usa la FPU x87, como el
# motor de PDF y el de la web: no entra en la libsagui comun; la enlazan los
# programas que la usan (Cipó, Configuración, Editor, Paint y la prueba).
FONTES_DIR := userland/libfontes
FONTES_OBJ := $(patsubst $(FONTES_DIR)/%.c,$(B)/$(FONTES_DIR)/%.o,$(wildcard $(FONTES_DIR)/*.c))
FONTES_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard \
  -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
  -Wno-sign-compare -Wno-missing-field-initializers
# Las fuentes de Typodermic (CC0, D196): la selección va a /apps/fuentes. Salen del zip
# de NEW_FONTS/ (tools/fuentes_typodermic.py; FUENTES_RED=1 lo baja de la web si falta);
# sin el zip, el sistema se construye igual, sin ellas.
FUENTES_ZIP := $(wildcard NEW_FONTS/typodermic-public-domain-*.zip)
FUENTES_RES :=
ifneq ($(FUENTES_ZIP),)
FUENTES_RES := $(B)/fuentes-res/.feito
APPS_RECURSOS += fuentes:$(B)/fuentes-res
endif

# Java con miniJVM (D376): /apps/bin/java, /apps/bin/javac y /apps/java/. JAVA=0 lo deja fuera.
# Va aquí (tras FONTES_OBJ) porque el programa java enlaza libfontes para sagui.gui.
include userland/terceros/minijvm/java.mk
ifneq ($(JV_PROGRAMAS),)
APPS_PROGRAMAS += java javac
APPS_BINS += $(JV_PROGRAMAS)
endif
ifneq ($(JV_RECURSOS),)
APPS_RECURSOS += java:$(JV_RES)
endif

# El navegador NetSurf (D184): GPL-2.0 (solo la v2), aislado en userland/terceros/netsurf/
# como el OpenTyrian, y fuera de la variante sin GPL. Sus fuentes no van en
# git: las baja y prepara tools/netsurf_fuentes.sh; sin ellas el sistema se
# construye igual, sin el navegador. D258: la GPL-2.0 sola no es compatible con la
# libsagui (GPL-3.0), asi que NetSurf es SOLO para uso privado: gerar_versao.sh
# pasa NETSURF=0 en lo publicable (make variantes) y lo deja en priv_variantes.
NETSURF ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(NETSURF),1)
# D261: en el árbol público no están las fuentes (se descargan) y puede faltar el pegamento:
# solo se incluye si hay las dos cosas.
ifneq ($(wildcard userland/terceros/netsurf/fuentes/.preparado),)
ifneq ($(wildcard userland/terceros/netsurf/netsurf.mk),)
include userland/terceros/netsurf/netsurf.mk
endif
endif
endif
endif

# El runtime de C++ (D228) y OpenJazz (D228): GPL-2.0, aislado en
# userland/terceros/openjazz/ como el OpenTyrian, fuera de la variante sin GPL y
# solo si hay compilador de C++. OPENJAZZ=0 lo deja fuera.
OPENJAZZ ?= 1
include userland/libcxx/libcxx.mk
include userland/sdk_externo.mk   # D375: «make sdk_externo» para compilar aplicaciones en repositorios aparte
ifneq ($(SEM_GPL),1)
ifeq ($(OPENJAZZ)$(CXX_OK),11)
include userland/terceros/openjazz/openjazz.mk
endif
endif

# Command & Conquer con Vanilla Conquer (D230): GPL-3.0, aislado en
# userland/terceros/conquer/ como OpenJazz, fuera de la variante sin GPL y solo
# si hay compilador de C++; con la SDL 2 (userland/libsdl2) y la OpenAL
# (userland/libopenal) del sistema. CONQUER=0 lo deja fuera.
CONQUER ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(CONQUER)$(CXX_OK),11)
include userland/terceros/conquer/conquer.mk
endif
endif

# DOSBox 0.74-3 (D294): el emulador de MS-DOS (GPL-2.0 o posterior), aislado en
# userland/terceros/dosbox/ como OpenJazz, fuera de la variante sin GPL y solo si hay
# compilador de C++ (con RTTI en el runtime). DOSBOX=0 lo deja fuera. Se incluye detrás de
# Quake: comparten la SDL 1.2 (SDL12_OBJ).
DOSBOX ?= 1

# Quake (D229): el motor de id Software (GPL-2.0), aislado en userland/terceros/quake/
# como OpenJazz, fuera de la variante sin GPL. Es C: no necesita el compilador de C++.
# QUAKE=0 lo deja fuera.
QUAKE ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(QUAKE),1)
include userland/terceros/quake/quake.mk
endif
endif

ifneq ($(SEM_GPL),1)
ifeq ($(DOSBOX)$(CXX_OK),11)
include userland/terceros/dosbox/dosbox.mk
endif
endif

# rsync (D314, mejora 120): openrsync de OpenBSD (ISC), aislado en userland/terceros/openrsync/.
# El modo servidor lo lanza el sshd; la copia local es propia. Sin la variante GPL no hace falta
# distinguirlo: la licencia ISC es compatible. RSYNC=0 lo deja fuera.
RSYNC ?= 1
ifeq ($(RSYNC),1)
include userland/terceros/openrsync/openrsync.mk
endif

# Recreativa (D294, D321): el lanzador multisistema; también sin DOSBOX.
ifneq ($(SEM_GPL),1)
include userland/terceros/recreativa/recreativa.mk
endif

# Emuladores de consolas (D321): LaiNES (NES, BSD-2-Clause) y Gearsystem (Master System, Game Gear
# y SG-1000; GPL-3.0), C++ con la SDL 2 del sistema y un marco común (userland/terceros/emucomun).
# LAINES=0 / GEARSYSTEM=0 los dejan fuera; con SEM_GPL=1 (D109) tampoco entran (el APU de LaiNES es LGPL).
LAINES ?= 1
GEARSYSTEM ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(LAINES)$(CXX_OK),11)
include userland/terceros/laines/laines.mk
endif
ifeq ($(GEARSYSTEM)$(CXX_OK),11)
ifneq ($(wildcard userland/terceros/gearsystem/gearsystem.mk),)
include userland/terceros/gearsystem/gearsystem.mk
endif
endif
endif

# Game Boy con SDL2-GNUBoy (D235): GPL-2.0, aislado en userland/terceros/gnuboy/ como
# los anteriores y fuera de la variante sin GPL; es C: no pide g++. Usa la SDL 2
# (userland/libsdl2) del sistema. GNUBOY=0 lo deja fuera (D259: PUBLICABLE=1 lo pasa; GPL-2.0 solo
# la v2). D261: su codigo (jogo/) no va en el repositorio publico: lo baja tools/gnuboy_fuentes.sh
# (make priv_descargas); sin el, como en el arbol publico, no entra y el sistema se construye igual.
GNUBOY ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(GNUBOY),1)
ifneq ($(wildcard userland/terceros/gnuboy/gnuboy.mk),)
ifneq ($(wildcard userland/terceros/gnuboy/jogo/VERSAO),)
include userland/terceros/gnuboy/gnuboy.mk
endif
endif
endif
endif

# Game Boy Advance, Game Boy y Game Boy Color con mGBA (D322): MPL-2.0, compatible con la GPL v3
# del proyecto (docs/terceros.md); aislado en userland/terceros/mgba/. Es C y usa la SDL 2 del
# sistema. MGBA=0 lo deja fuera. Sus fuentes (jogo/) van en git (tools/mgba_fuentes.sh --comprobar)
# y entra con PUBLICABLE=1: ninguna ROM ni BIOS van con el sistema.
MGBA ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(MGBA),1)
ifneq ($(wildcard userland/terceros/mgba/jogo/VERSAO),)
include userland/terceros/mgba/mgba.mk
endif
endif
endif

# Freeciv (D374): GPL-2.0 o posterior, aislado en userland/terceros/freeciv/ como OpenJazz; solo
# si se bajaron las fuentes (tools/freeciv_fuentes.sh) y fuera de la variante sin GPL. FREECIV=0 lo deja fuera.
FREECIV ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(FREECIV),1)
ifneq ($(wildcard userland/terceros/freeciv/fuentes/.preparado),)
include userland/terceros/freeciv/freeciv.mk
endif
endif
endif

# Chocolate Doom (D383): GPL-2.0 o posterior, aislado en userland/terceros/chocolatedoom/ como Freeciv; solo
# si se bajaron las fuentes (tools/doom_fuentes.sh) y fuera de la variante sin GPL. DOOM=0 lo deja fuera.
DOOM ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(DOOM),1)
ifneq ($(wildcard userland/terceros/chocolatedoom/fuentes/.preparado),)
include userland/terceros/chocolatedoom/chocolatedoom.mk
endif
endif
endif

# Conversor de audio (D339): el MP3 lo codifica shine (LGPL-2.0+), aislado en userland/terceros/shine/. SHINE=0 o SEM_GPL=1
# lo dejan fuera (el programa sale igual, sin la salida MP3).
SHINE ?= 1
ifneq ($(SEM_GPL),1)
ifeq ($(SHINE),1)
ifneq ($(wildcard userland/terceros/shine/src/layer3.c),)
include userland/terceros/shine/shine.mk
endif
endif
endif

# Programa sintético de la SDL 2 para la prueba h246 (D247).
include userland/libsdl2/teste.mk
# AoEN (D382): RTS propio estilo Age of Empires (GPL-3.0-or-later); los datos del juego original no se distribuyen.
include userland/juegos/aoen/aoen.mk
# TCC (D384): el compilador de C dentro de SaguiOS (/apps/bin/tcc) y los fuentes de AoEN para recompilarlo dentro.
# Solo si se bajaron sus fuentes (tools/tcc_fuentes.sh); TCC=0 lo deja fuera.
# Fuera de la variante sin GPL (lo que compila lleva la libsagui GPL-3.0-only y libtcc1.c es GPL-2.0+), como Freeciv.
ifneq ($(SEM_GPL),1)
include userland/terceros/tcc/tcc.mk
endif

# Soporte de Go con TinyGo (D364): solo si TinyGo está instalado (tools/tinygo_entorno.sh).
include userland/go/go.mk

# D194: lo que va a /apps sale de /bin. APPS_DEP es el árbol de /apps ya
# montado en $(B)/apps/raiz; APPS_OPC, la partición en las imágenes; el CD
# (que no lee particiones, D077) lleva las apps en su propio ramdisk.
PROGRAMAS := $(filter-out $(APPS_BINS),$(PROGRAMAS))
APPS_DEP := $(B)/apps/.feito
# Si cambia APPS entre dos compilaciones en la misma carpeta, el ramdisk se rehace
# (con APPS=0 lleva /apps dentro; con APPS=1, no): una marca por valor.
APPS_MARCA := $(B)/apps/opcao-APPS-$(APPS)
$(shell mkdir -p $(B)/apps && { [ -e $(APPS_MARCA) ] || { rm -f $(B)/apps/opcao-APPS-*; touch $(APPS_MARCA); }; })
ifeq ($(APPS),0)
APPS_OPC :=
RAMDISK_APPS_DEP := $(APPS_DEP)
CD_RAMDISK := $(B)/ramdisk.sgfs
else
APPS_OPC := --apps $(B)/apps/raiz --apps-mb $(APPS_MB)
RAMDISK_APPS_DEP :=
CD_RAMDISK := $(B)/ramdisk-cd.sgfs
APPS_IMG_DEP := $(APPS_DEP)
endif

# D379 (M1): las imágenes por defecto. El cargador lee solo el núcleo y un initramfs pequeño
# ($(INITRAMFS): shell, utilidades estáticas y /etc/raiz.conf); la raíz del sistema es la partición
# SAGUI-RAIZ (ext2, de build/rootfs con /apps dentro, P2 A), que el núcleo monta en «/»:
#   - imágenes de disco y de USB (y la VDI): live desde el medio, la raíz de solo lectura con una capa
#     escribible en la RAM, o en SAGUI-PERSIST en las persistentes (P3 A); SAGUI-CONF como siempre;
#   - la ISO: la raíz dentro de la imagen El Torito, copiada a la RAM al arrancar (toram, P4/P7 A; si no
#     cabe, live desde el CD);
#   - el instalador copia el initramfs y la raíz por ficheros a SAGUI-RAIZ, con escritura (R2).
# Con RAMDISK=completo (o RAIZ_DISCO=0) todo vuelve a ser como hasta la v8.5.3: el ramdisk SGFS con todo
# el sistema es la raíz (saguios-completo.img, para los PC cuyo disco de arranque el núcleo no ve).
# El SGFS completo ($(B)/ramdisk.sgfs) se sigue haciendo: de él sale build/rootfs y lo usan
# saguios-completo.img, las pruebas de compatibilidad y RAIZ_DISCO=0.
RAIZ_DISCO ?= 1
ifeq ($(RAMDISK),completo)
override RAIZ_DISCO := 0
endif
INITRAMFS := $(B)/initramfs.sgfs
PERSIST_MB ?= 256
ifeq ($(RAIZ_DISCO),1)
IMG_RAMDISK := $(INITRAMFS)
IMG_RAMDISK_DEP := $(INITRAMFS) $(B)/ramdisk.sgfs
RAIZ_OPC := --raiz $(B)/rootfs
PERSIST_OPC := --persist-mb $(PERSIST_MB)
CD_DEP := $(INITRAMFS) $(B)/ramdisk.sgfs $(APPS_IMG_DEP)
CD_OPC := --ramdisk $(INITRAMFS) --raiz $(B)/rootfs $(APPS_OPC)
else
IMG_RAMDISK := $(B)/ramdisk.sgfs
IMG_RAMDISK_DEP := $(B)/ramdisk.sgfs
RAIZ_OPC :=
PERSIST_OPC :=
CD_DEP := $(CD_RAMDISK)
CD_OPC := --ramdisk $(CD_RAMDISK)
endif

.PHONY: help uefi variantes priv_variantes priv_descargas imagem-smp imagem-persistente-smp imagem-persistente-smp-uacpi imagem-uacpi all vazio vdi iso test rodar entrega limpar dashboard imagem-persistente imagem-persistente-fat32 imagem-particoes imagem-particoes-xz

all: $(B)/saguios.img $(B)/host/chip8

vazio:
	@echo "SaguiOS: alvo vazio ok"

# ---------- carregador de boot ----------
$(B)/boot/mbr.bin: boot/mbr.asm boot/cabecalho.inc
	@mkdir -p $(dir $@)
	$(AS) -f bin -I boot/ -o $@ $<

$(B)/boot/estagio2.bin: boot/estagio2.asm $(wildcard boot/*.inc)
	@mkdir -p $(dir $@)
	$(AS) -f bin -I boot/ -o $@ $<

# ---------- cargador UEFI (D264): BOOTX64.EFI y BOOTIA32.EFI ----------
# C freestanding con el gcc del sistema (x86-64 y -m32), enlazado como ELF independiente
# de la posición y convertido a PE32+/PE32 por tools/mkefi.py (sin gnu-efi ni EDK2).
# El trampolín a 32 bits es un binario plano de NASM que el cargador lleva dentro.
UEFI_CFLAGS  := -std=gnu11 -O2 -ffreestanding -fno-builtin -nostdlib -fpie -fvisibility=hidden \
                -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
                -fno-tree-loop-distribute-patterns -mgeneral-regs-only -fshort-wchar \
                -Wall -Wextra -finput-charset=UTF-8 -Iboot/uefi
UEFI_LDFLAGS := -pie --no-dynamic-linker -Bsymbolic -z norelro -z noexecstack -z max-page-size=0x1000 \
                --no-warn-rwx-segments -T boot/uefi/ligacao.ld
UEFI_EFI     := $(B)/boot/uefi/BOOTX64.EFI $(B)/boot/uefi/BOOTIA32.EFI

$(B)/boot/uefi/trampolim.bin: boot/uefi/trampolim.asm
	@mkdir -p $(dir $@)
	$(AS) -f bin -o $@ $<

$(B)/boot/uefi/x64/blob.o: boot/uefi/trampolim_blob.asm $(B)/boot/uefi/trampolim.bin
	@mkdir -p $(dir $@)
	$(AS) -f elf64 -DTRAMPOLIM_BIN='"$(B)/boot/uefi/trampolim.bin"' -o $@ $<

$(B)/boot/uefi/ia32/blob.o: boot/uefi/trampolim_blob.asm $(B)/boot/uefi/trampolim.bin
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -DTRAMPOLIM_BIN='"$(B)/boot/uefi/trampolim.bin"' -o $@ $<

$(B)/boot/uefi/x64/cargador.o: boot/uefi/cargador.c boot/uefi/uefi.h
	@mkdir -p $(dir $@)
	$(CC) -m64 -march=x86-64 -mno-red-zone $(UEFI_CFLAGS) -c -o $@ $<

# D310: el cargador IA32 se compila para i586, para que pueda avisar (sin CMOV de por medio)
# de que la CPU no es i686 antes de cargar un kernel que sí lo es.
$(B)/boot/uefi/ia32/cargador.o: boot/uefi/cargador.c boot/uefi/uefi.h
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i586 $(UEFI_CFLAGS) -c -o $@ $<

$(B)/boot/uefi/BOOTX64.EFI: $(B)/boot/uefi/x64/cargador.o $(B)/boot/uefi/x64/blob.o boot/uefi/ligacao.ld tools/mkefi.py
	$(LD) -m elf_x86_64 $(UEFI_LDFLAGS) -o $(B)/boot/uefi/x64/cargador.elf $(B)/boot/uefi/x64/cargador.o $(B)/boot/uefi/x64/blob.o
	$(PY) tools/mkefi.py $(B)/boot/uefi/x64/cargador.elf $@

$(B)/boot/uefi/BOOTIA32.EFI: $(B)/boot/uefi/ia32/cargador.o $(B)/boot/uefi/ia32/blob.o boot/uefi/ligacao.ld tools/mkefi.py
	$(LD) -m elf_i386 $(UEFI_LDFLAGS) -o $(B)/boot/uefi/ia32/cargador.elf $(B)/boot/uefi/ia32/cargador.o $(B)/boot/uefi/ia32/blob.o
	$(PY) tools/mkefi.py $(B)/boot/uefi/ia32/cargador.elf $@

uefi: $(UEFI_EFI)

# ---------- kernel ----------
$(B)/kernel/%.o: kernel/%.c $(KERNEL_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# kernel/lib/sse2.c (D042) é o único arquivo do kernel que pode usar XMM:
# o GCC gera SSE2 em qualquer função dele, e todas só rodam entre
# fpu_kernel_inicio() e fpu_kernel_fim(). O mesmo arquivo entra na libsagui.
SSE2_CFLAGS = $(filter-out -mgeneral-regs-only,$(1)) -msse -msse2
$(B)/kernel/lib/sse2.o: CFLAGS := $(call SSE2_CFLAGS,$(CFLAGS))
# D208: una variante por fichero, cada una con su -m: SSE sin SSE2 (sse1.c), SSE3 (sse3.c) y
# SSSE3 (ssse3.c). Solo se llaman si CPUID dice que la CPU las tiene. -mstackrealign: la pila del kernel
# no está alineada a 16 bytes y GCC puede guardar un vector en un temporal alineado de la pila.
SSE1_CFLAGS  = $(filter-out -mgeneral-regs-only,$(1)) -msse -mno-sse2 -mstackrealign
SSE3_CFLAGS  = $(filter-out -mgeneral-regs-only,$(1)) -msse -msse2 -msse3 -mstackrealign
SSSE3_CFLAGS = $(filter-out -mgeneral-regs-only,$(1)) -msse -msse2 -msse3 -mssse3 -mstackrealign
$(B)/kernel/lib/sse1.o: CFLAGS := $(call SSE1_CFLAGS,$(CFLAGS))
$(B)/kernel/lib/sse3.o: CFLAGS := $(call SSE3_CFLAGS,$(CFLAGS))
$(B)/kernel/lib/ssse3.o: CFLAGS := $(call SSSE3_CFLAGS,$(CFLAGS))

$(B)/kernel/%.asm.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -I kernel/arch/ -o $@ $<

# A fonte é desenhada em texto (tools/fonte/) e vira C no build.
$(B)/gerado/fonte8x16.c: tools/fonte/sagui8x16.txt tools/mkfonte.py
	@mkdir -p $(dir $@)
	$(PY) tools/mkfonte.py $< $@

$(B)/gerado/fonte8x16.o: $(B)/gerado/fonte8x16.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Dados da compilação (data, hora, commit) para o "Acerca de" e o `sobre`.
# A data e a hora podem se fixar com SAGUI_DATA_COMPILACAO e
# SAGUI_HORA_COMPILACAO para fazer uma compilação reproduzível.
$(B)/gerado/compilacao.h: tools/mkcompilacao.py
	@mkdir -p $(dir $@)
	$(PY) tools/mkcompilacao.py $@

$(B)/kernel.elf: $(KERNEL_OBJ) kernel/ligacao.ld
	$(LD) $(LDFLAGS) -T kernel/ligacao.ld -o $@ $(KERNEL_OBJ)
	@$(PY) $(CHECK_ELF32) $@

$(B)/kernel.bin: $(B)/kernel.elf
	objcopy -O binary $< $@

# ---------- programas de usuário ----------
$(B)/userland/%.o: userland/%.c $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

# O "Acerca de" (sobre-o-sistema) e o `sobre` mostram os dados da compilação
# (data, hora e commit), que estão no cabeçalho gerado.
$(B)/userland/programas/sobre.o $(B)/userland/programas/sobre-o-sistema.o: $(COMPILACAO_H)
$(B)/userland/programas/sobre.o $(B)/userland/programas/sobre-o-sistema.o: UCFLAGS += -I$(B)/gerado

# O simd (D032) usa FPU, MMX, SSE e SSE2 de propósito: é compilado sem
# -mgeneral-regs-only. O kernel salva o estado de cada processo. Sem -msse2 no
# arquivo todo (D076): cada função de teste liga o seu recurso com
# __attribute__((target)) e só roda depois de o CPUID confirmar.
$(B)/userland/programas/simd.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS))

# O motor do SaguiMark (D093) usa a FPU x87 e, em funções marcadas com
# __attribute__((target("sse2"))), SSE2; como o simd, sem -msse2 no arquivo
# todo. Usa o SHA-256 do kernel, ligado só nos programas que têm o motor.
$(B)/userland/libsagui/saguimark.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS))
# El ecualizador (D201) calcula los biquads con la x87 (el kernel solo recibe enteros).
$(B)/userland/libsagui/ecualizador.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387
# (con SEM_GPL=1 h264.c y aac.c no se compilan: incluyen las tablas de FFmpeg y FAAD2, que son LGPL/GPL de terceros)
# AAC-LC (mejora 100, D276): coma flotante con la x87, como el ecualizador (vale en cualquier CPU).
$(B)/userland/libsagui/aac.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -fno-math-errno
$(B)/userland/bin/saguimark $(B)/userland/bin/infopc: $(B)/userland/libtls/sha256.o
# D215: el programa wifi calcula la PMK con el mismo kernel/lib/wpa.c del kernel.
# D220: y el PT y los PWE de SAE (WPA3) con wpa_cifra.c, p256.c y sae.c (y el SHA-256).
WIFI_LIB_H := kernel/inc/wpa.h kernel/inc/wpa_cifra.h kernel/inc/p256.h kernel/inc/sae.h kernel/inc/sha256.h
$(B)/userland/wpa.o: kernel/lib/wpa.c $(WIFI_LIB_H)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/wpa_cifra.o: kernel/lib/wpa_cifra.c $(WIFI_LIB_H)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/p256.o: kernel/lib/p256.c $(WIFI_LIB_H)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/sae.o: kernel/lib/sae.c $(WIFI_LIB_H)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/bin/wifi: $(B)/userland/wpa.o $(B)/userland/wpa_cifra.o $(B)/userland/p256.o $(B)/userland/sae.o \
                        $(B)/userland/libtls/sha256.o
# D354: la ventana de redes va en wifi_ventana.h (incluida por wifi.c)
$(B)/userland/programas/wifi.o: userland/programas/wifi_ventana.h
# D363: la ventana de la VPN va en wg_ventana.h (incluida por wg.c)
$(B)/userland/programas/wg.o: userland/programas/wg_ventana.h userland/inc/wireguard.h

# El benchmark (D176) tiene cargas de coma flotante SSE2 (con __attribute__((target)), tras el
# CPUID) y usa la x87 para la media geométrica: sin -mgeneral-regs-only, como el simd. Lleva SHA-256.
$(B)/userland/programas/benchmark.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS))
$(B)/userland/bin/benchmark: $(B)/userland/libtls/sha256.o
# D255: el gestor de servicios comprueba las contraseñas (PBKDF2-HMAC-SHA256, userland/inc/pbkdf2.h).
$(B)/userland/bin/servicios $(B)/userland/bin/login $(B)/userland/testes/usuarios: $(B)/userland/libtls/sha256.o
# D318: el instalador escribe las contraseñas elegidas en la sombra de SAGUI-CONF (PBKDF2) y su
# asistente va en instalar_asistente.h.
$(B)/userland/bin/instalar: $(B)/userland/libtls/sha256.o
$(B)/userland/programas/instalar.o: userland/programas/instalar_asistente.h

# As rotinas SSE2 da libsagui são o mesmo kernel/lib/sse2.c, compilado sem
# -mgeneral-regs-only; texto.c só as chama se o CPUID disser que há SSE2.
$(B)/userland/libsagui/sse2.o: kernel/lib/sse2.c kernel/inc/sse2.h
	@mkdir -p $(dir $@)
	$(CC) $(call SSE2_CFLAGS,$(UCFLAGS)) -c -o $@ $<
$(B)/userland/libsagui/sse1.o: kernel/lib/sse1.c kernel/inc/sse2.h
	@mkdir -p $(dir $@)
	$(CC) $(call SSE1_CFLAGS,$(UCFLAGS)) -c -o $@ $<
$(B)/userland/libsagui/sse3.o: kernel/lib/sse3.c kernel/inc/sse2.h
	@mkdir -p $(dir $@)
	$(CC) $(call SSE3_CFLAGS,$(UCFLAGS)) -c -o $@ $<
$(B)/userland/libsagui/ssse3.o: kernel/lib/ssse3.c kernel/inc/sse2.h
	@mkdir -p $(dir $@)
	$(CC) $(call SSSE3_CFLAGS,$(UCFLAGS)) -c -o $@ $<

# O intérprete de regras POSIX TZ (D080) é o mesmo kernel/lib/fuso.c.
$(B)/userland/libsagui/fuso.o: kernel/lib/fuso.c kernel/inc/fuso.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

# D313: el lector de catálogos de traducción es el mismo kernel/lib/catalogo.c.
$(B)/userland/libsagui/catalogo.o: kernel/lib/catalogo.c kernel/inc/catalogo.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

$(B)/userland/fonte8x16.o: $(B)/gerado/fonte8x16.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

$(B)/userland/libsagui/%.asm.o: userland/libsagui/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 -o $@ $<

$(B)/userland/bin/%: $(B)/userland/programas/%.o $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# Os programas com HTTPS ligam também a libtls (o --gc-sections tira o que não usam).
$(addprefix $(B)/userland/bin/,$(TLS_PROGS)) $(B)/userland/testes/pruebacripto: $(TLS_OBJ)

# O `ia`, o Mico e o chatia ligam a biblioteca do cliente de IA (D134).
$(B)/userland/bin/ia: $(IA_OBJ)
$(B)/userland/bin/chatia: $(IA_OBJ) $(IA_GUI_OBJ)
# Pluma, Macaco y el correo integran el panel de agentes de IA (D298).
$(B)/userland/bin/pluma $(B)/userland/bin/macaco $(B)/userland/bin/tucan $(B)/userland/bin/correo: $(IA_OBJ) $(IA_GUI_OBJ)
# A janela de Configuração também fala com o servidor de IA para pedir os modelos (D145).
$(B)/userland/bin/configuracion: $(IA_OBJ)
# El Traductor (D335) pide propuestas al agente de IA del sistema.
$(B)/userland/bin/traductor: $(IA_OBJ)
# O `correo` liga a biblioteca dos protocolos de correio e a libtls (D082).
$(B)/userland/bin/correo: $(CORREO_OBJ)
$(B)/userland/mail/%.o: userland/mail/%.c $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/ia/%.o: userland/ia/%.c $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

# libsgl (D104, mejora 054): SaguiGL por software. Usa SSE2 (el sgldemo comprueba
# el CPUID), sin FMA y sin errno en la raíz, para que el anfitrión dé los mismos bits.
SGL_FLAGS := -msse -msse2 -mfpmath=sse -ffp-contract=off -fno-math-errno -mstackrealign -Iuserland/libsgl
SGL_OBJ   := $(B)/userland/libsgl/sgl.o $(B)/userland/libsgl/cenas.o
SGL_HDR   := $(wildcard userland/libsgl/*.h)
# A prova da rota v6 (texturas, mistura, neblina e luzes, D130): o mesmo programa
# compila-se no anfitrião (tests/h101_sagui_gl_v6.py) e dentro do SaguiOS.
SGL_PROG_OBJ := $(B)/userland/programas/sglv6.o
$(SGL_OBJ) $(SGL_PROG_OBJ) $(B)/userland/programas/sgldemo.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) $(SGL_FLAGS)
$(SGL_OBJ) $(SGL_PROG_OBJ) $(B)/userland/programas/sgldemo.o: $(SGL_HDR)
$(B)/userland/bin/sgldemo: $(SGL_OBJ)
$(SGL_PROG_OBJ): tests/sgl/sglv6.c $(SGL_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/bin/sglv6: $(SGL_OBJ) $(SGL_PROG_OBJ)

# O armazón do driver 3D das GPU da AMD (melhora 054, parte B2, D158): a tabela
# de familias, a identificação (com o mapeo do BAR que faz o kernel) e o
# vigilante da GPU. Fica numa pasta própria, como a libtls, porque só o
# programa `amd` a liga.
AMD_DIR := userland/libamd
AMD_OBJ := $(patsubst $(AMD_DIR)/%.c,$(B)/$(AMD_DIR)/%.o,$(wildcard $(AMD_DIR)/*.c))
AMD_HDR := $(wildcard $(AMD_DIR)/*.h)
$(B)/$(AMD_DIR)/%.o: $(AMD_DIR)/%.c $(AMD_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -I$(AMD_DIR) -c -o $@ $<
$(B)/userland/bin/amd: $(AMD_OBJ)
$(B)/userland/programas/amd.o: UCFLAGS += -I$(AMD_DIR)
$(B)/userland/programas/amd.o: $(AMD_HDR)

$(B)/userland/libtls/%.o: userland/libtls/%.c userland/libtls/bignum.h userland/libtls/campo25519.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

# O protocolo e o X.509 não são caminho quente: -Os (metade do tamanho). As
# primitivas (AES, ChaCha20, curvas, bignum, hashes) ficam em -O2.
$(B)/userland/libtls/tls.o $(B)/userland/libtls/x509.o $(B)/userland/libtls/almacen.o $(B)/userland/libtls/conexao.o: UCFLAGS += -Os

# AES-NI (D060): único arquivo da libtls com XMM; só é chamado se o CPUID tiver AES e SSE2.
$(B)/userland/libtls/aesni.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -msse -msse2 -maes

# O SHA-256 é o mesmo arquivo do kernel (gerador aleatório, D061).
$(B)/userland/libtls/sha256.o: kernel/lib/sha256.c kernel/inc/sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

$(B)/userland/sagui3d/%.o: userland/sagui3d/%.c userland/sagui3d/jogo.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

$(B)/userland/bin/sagui3d: $(SAGUI3D_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(SAGUI3D_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

# D380 (mejora 139): el motor de modelos de lenguaje locales y el programa `llm`, en su
# carpeta. Con la x87 (como el motor de PDF); los productos punto de llm_quant_sse2.c y
# llm_quant_ssse3.c, con SSE2/SSSE3, solo se llaman si CPUID lo dice. Su aritmética de float
# también va por la x87: así todas las rutas dan exactamente los mismos logits. Va a /apps/bin (APPS_MOVER_FIJO).
# `make llm-pc` compila el mismo motor para el PC: build/host/llm-pc (tools/llm_comparar.py, tests/h388).
LLM_DIR   := userland/llm
LLM_HDR   := $(wildcard $(LLM_DIR)/*.h)
LLM_OBJ   := $(patsubst $(LLM_DIR)/%.c,$(B)/$(LLM_DIR)/%.o,$(wildcard $(LLM_DIR)/*.c))
LLM_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard
$(B)/$(LLM_DIR)/%.o: $(LLM_DIR)/%.c $(LLM_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(LLM_FLAGS) -c -o $@ $<
$(B)/$(LLM_DIR)/llm_quant_sse2.o: LLM_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -msse -msse2 -mfpmath=387 -fexcess-precision=standard -mstackrealign
$(B)/$(LLM_DIR)/llm_quant_ssse3.o: LLM_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -msse -msse2 -mssse3 -mfpmath=387 -fexcess-precision=standard -mstackrealign
$(B)/userland/bin/llm: $(LLM_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(LLM_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf
# En el PC, la aritmética de float también con la x87 (como en SaguiOS): así los dos dan
# exactamente los mismos tokens (con SSE, los redondeos distintos cambian alguno, h388).
LLM_PC_FLAGS := -O2 -g -D_GNU_SOURCE -DLLM_ANFITRIAO -DLLM_MAT_X87
$(B)/host/llm/%.o: $(LLM_DIR)/%.c $(LLM_HDR)
	@mkdir -p $(dir $@)
	CPATH="$(HOST_CPATH)" $(HOSTCC) $(LLM_PC_FLAGS) $(if $(filter llm_quant_ss%,$*),-msse2 -mssse3) -mfpmath=387 -fexcess-precision=standard -c -o $@ $<
$(B)/host/llm-pc: $(patsubst $(LLM_DIR)/%.c,$(B)/host/llm/%.o,$(wildcard $(LLM_DIR)/*.c))
	$(HOSTCC) -o $@ $^ -lm -lpthread
.PHONY: llm-pc
llm-pc: $(B)/host/llm-pc

# O motor de PDF (D138): compila com a x87 (sem -mgeneral-regs-only), que é o
# mesmo que usam o cc e o python-x87. O visorpdf liga com ele.
$(B)/$(PDF_DIR)/%.o: $(PDF_DIR)/%.c $(PDF_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(PDF_FLAGS) -c -o $@ $<

$(B)/userland/programas/visorpdf.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) \
  -mfpmath=387 -fexcess-precision=standard -Iuserland/pdf

$(B)/userland/bin/visorpdf: $(B)/userland/programas/visorpdf.o $(PDF_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# GebraMini (D309, mejora 128): el motor de geometría dinámica y de expresiones con dobles de la
# FPU x87 (fsin, fpatan...; sin libm), como el motor de PDF. Solo el programa gebramini lo liga.
GBM_DIR  := userland/gebramini
GBM_OBJ  := $(patsubst $(GBM_DIR)/%.c,$(B)/$(GBM_DIR)/%.o,$(wildcard $(GBM_DIR)/*.c))
GBM_HDR  := $(wildcard $(GBM_DIR)/*.h)
GBM_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -I$(GBM_DIR)
$(B)/$(GBM_DIR)/%.o: $(GBM_DIR)/%.c $(GBM_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(GBM_FLAGS) -c -o $@ $<
$(B)/userland/programas/gebramini.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -I$(GBM_DIR)
$(B)/userland/programas/gebramini.o: $(GBM_HDR)
# El catálogo de funciones de Macaco (D311) es una cabecera propia: al cambiarla hay que recompilar macaco.o.
$(B)/userland/programas/macaco.o: userland/programas/macaco_funciones.h userland/programas/macaco_formatos.h
$(B)/userland/programas/pluma.o: userland/programas/pluma_cinta.h
$(B)/userland/programas/tucan.o: userland/programas/tucan_cinta.h userland/programas/tucan_modos.h userland/programas/tucan_pres.h userland/programas/tucan_agente.h
$(B)/userland/programas/macaco.o: userland/programas/macaco_cinta.h
$(B)/userland/bin/gebramini: $(GBM_OBJ)

# Jabuti (D377, mejora 137): programación con bloques para niños; el intérprete y el modelo van en userland/jabuti/
# (enteros, sin FPU, para un Pentium II). Solo el programa jabuti lo liga.
JB_DIR  := userland/jabuti
JB_OBJ  := $(patsubst $(JB_DIR)/%.c,$(B)/$(JB_DIR)/%.o,$(wildcard $(JB_DIR)/*.c))
JB_HDR  := $(wildcard $(JB_DIR)/*.h) userland/programas/jabuti_ops.h
$(B)/$(JB_DIR)/%.o: $(JB_DIR)/%.c $(JB_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -I$(JB_DIR) -c -o $@ $<
$(B)/userland/programas/jabuti.o: UCFLAGS := $(UCFLAGS) -I$(JB_DIR)
$(B)/userland/programas/jabuti.o: $(JB_HDR)
$(B)/userland/bin/jabuti: $(JB_OBJ)

# PlanoMini (D312, mejora 129): el motor del CAD 2D (órdenes, geometría, .pmini, DXF) con la x87 como GebraMini
# (gbm_mat.h, sin libm). Solo el programa planomini lo liga.
PM_DIR  := userland/planomini
PM_OBJ  := $(patsubst $(PM_DIR)/%.c,$(B)/$(PM_DIR)/%.o,$(wildcard $(PM_DIR)/*.c))
PM_HDR  := $(wildcard $(PM_DIR)/*.h)
PM_FLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -I$(PM_DIR) -I$(GBM_DIR)
$(B)/$(PM_DIR)/%.o: $(PM_DIR)/%.c $(PM_HDR) $(GBM_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(PM_FLAGS) -c -o $@ $<
$(B)/userland/programas/planomini.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard -I$(PM_DIR) -I$(GBM_DIR)
$(B)/userland/programas/planomini.o: $(PM_HDR) $(GBM_HDR)
$(B)/userland/bin/planomini: $(PM_OBJ)

# O motor da web do Cipó (081): compila com a x87 (sem -mgeneral-regs-only), como o
# motor de PDF. O Cipó liga com ele.
$(B)/$(WEB_DIR)/%.o: $(WEB_DIR)/%.c $(WEB_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(WEB_FLAGS) -c -o $@ $<

# O QuickJS: compila sem libc (-nostdinc) e com as cabeceras de reemplazo.
$(B)/$(QJS_DIR)/%.o: $(QJS_DIR)/%.c $(QJS_HDR)
	@mkdir -p $(dir $@)
	@# CPATH= para que o QuickJS so veja as cabeceras de reemplazo (inc/) e as
	@# do proprio gcc: o CPATH do ambiente apontaria para a libc do anfitriao.
	CPATH= $(CC) $(QJSFLAGS) -c -o $@ $<

$(B)/userland/programas/cipo.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) \
  -I$(QJS_DIR) \
  -mfpmath=387 -fexcess-precision=standard -I$(WEB_DIR)

$(B)/userland/bin/cipo: $(B)/userland/programas/cipo.o $(WEB_OBJ) $(FONTES_OBJ) $(QJS_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# O teste do motor (081): liga com o motor da web, como o cipo.
$(B)/userland/testes/cipo-motor.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) \
  -mfpmath=387 -fexcess-precision=standard -I$(WEB_DIR)

$(B)/userland/testes/cipo-motor: $(B)/userland/testes/cipo-motor.o $(WEB_OBJ) $(FONTES_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@

# O qjs (D188): o QuickJS na terminal. Liga os mesmos objetos do motor que o
# cipo (QJS_OBJ, menos o qjs_bridge.o, que e o DOM do navegador). O prelude em
# JavaScript (console, inspect) vai embutido como string C (tools/js2h.py).
QJS_CLI_OBJ := $(filter-out %/qjs_bridge.o,$(QJS_OBJ))
$(B)/userland/qjs/qjs_prelude.h: userland/programas/qjs_prelude.js tools/js2h.py
	@mkdir -p $(dir $@)
	$(PY) tools/js2h.py $< $@ qjs_prelude

$(B)/userland/programas/qjs.o: userland/programas/qjs.c $(B)/userland/qjs/qjs_prelude.h $(QJS_HDR)
	@mkdir -p $(dir $@)
	CPATH= $(CC) $(QJSFLAGS) -I$(B)/userland/qjs -c -o $@ $<

$(B)/userland/bin/qjs: $(B)/userland/programas/qjs.o $(QJS_CLI_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# O programa de prova do QuickJS (D149): liga com o motor (e a libm x87).
$(B)/userland/testes/quickjs.o: userland/testes/quickjs.c $(QJS_DIR)/qjs_bridge.h $(QJS_HDR)
	@mkdir -p $(dir $@)
	CPATH= $(CC) $(QJSFLAGS) -c -o $@ $<

$(B)/userland/testes/quickjs: $(B)/userland/testes/quickjs.o $(QJS_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@

$(B)/litemonkey/%.o: litemonkey/%.c $(LMK_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only -fexec-charset=ISO-8859-1,$(UCFLAGS)) -fexec-charset=UTF-8 -mfpmath=387 \
	  -Wno-unused-parameter -DLMK_SAGUI -Ilitemonkey -c -o $@ $<

$(B)/userland/bin/litemonkey: $(LMK_OBJ) $(B)/userland/libtls/sha256.o $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(LMK_OBJ) $(B)/userland/libtls/sha256.o $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

# O OpenTyrian (D156): o motor (jogo/) e a plataforma (plataforma/) compilam
# com os mesmos flags (x87, sem libc do anfitriao); a libc é a común (D210).
$(B)/$(OT_DIR)/jogo/%.o: $(OT_DIR)/jogo/%.c $(OT_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(OT_FLAGS) -c -o $@ $<

$(B)/$(OT_DIR)/plataforma/%.o: $(OT_DIR)/plataforma/%.c $(OT_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(OT_FLAGS) -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -c -o $@ $<

# --wrap=fopen: lo de «./» va a /home (D204, plataforma/principal_ot.c).
$(B)/userland/bin/opentyrian: $(OT_OBJ) $(OT_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) --wrap=fopen -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

$(B)/userland/programas/mico/%.o: userland/programas/mico/%.c userland/programas/mico/mico.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<

$(B)/userland/bin/mico: $(MICO_OBJ) $(IA_OBJ) $(IA_GUI_OBJ) $(TLS_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(MICO_OBJ) $(IA_OBJ) $(IA_GUI_OBJ) $(TLS_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

# -Os: o sagsh vai duas vezes para a imagem (/bin/sagsh e /bin/sh).
$(B)/userland/sagsh/%.o: userland/sagsh/%.c userland/sagsh/sagsh.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -Os -c -o $@ $<

$(B)/userland/bin/sagsh: $(SAGSH_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(SAGSH_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

$(B)/userland/bin/sh: $(B)/userland/bin/sagsh
	cp $< $@

# CHIP-8 (D070-D072): emulador (chip8) e montador (chip8asm) numa pasta
# própria. O montador e o modo de texto do emulador também compilam para o
# host (-DSAGUI_HOST, com a libc): o build monta as ROMs dos jogos com o
# montador do host, e as provas comparam as saídas do host e do SaguiOS.
C8_DIR     := userland/chip8
C8_HDR     := $(wildcard $(C8_DIR)/*.h)
C8_EMU_OBJ := $(addprefix $(B)/$(C8_DIR)/,nucleo.o desmontador.o relatorio.o plataforma.o chip8.o)
C8_ASM_OBJ := $(addprefix $(B)/$(C8_DIR)/,nucleo.o desmontador.o montador.o plataforma.o chip8asm.o)
HOSTCC     := gcc
HOST_CFLAGS := -O2 -Wall -Wextra -DSAGUI_HOST
C8_HOST_ASM := $(B)/host/chip8asm
C8_HOST_EMU := $(B)/host/chip8
# Jogos (rootfs/jogos/chip8/fuentes/NOME.asm -> /jogos/chip8/NOME.ext) e
# ROMs de prova (rootfs/testes/chip8/NOME.asm -> /testes/chip8/NOME.ext).
C8_JOGOS   := ramas.ch8 sumas.ch8 canoa.sc8 aurora.xo8
C8_PROVAS  := prueba-chip8.ch8 prueba-schip.sc8 prueba-xochip.xo8
C8_FONTES_J := $(wildcard rootfs/jogos/chip8/fuentes/*.asm)
C8_FONTES_P := $(wildcard rootfs/testes/chip8/*.asm)
C8_ROMS    := $(addprefix $(B)/chip8/jogos/,$(C8_JOGOS)) $(addprefix $(B)/chip8/testes/,$(C8_PROVAS))

$(C8_EMU_OBJ) $(C8_ASM_OBJ): $(C8_HDR)

$(B)/userland/bin/chip8: $(C8_EMU_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(C8_EMU_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

$(B)/userland/bin/chip8asm: $(C8_ASM_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(C8_ASM_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

$(C8_HOST_ASM): $(addprefix $(C8_DIR)/,nucleo.c desmontador.c montador.c plataforma.c chip8asm.c host.c) $(C8_HDR)
	@mkdir -p $(dir $@)
	CPATH="$(HOST_CPATH)" $(HOSTCC) $(HOST_CFLAGS) -DPRINCIPAL=principal -o $@ $(filter %.c,$^)

$(C8_HOST_EMU): $(addprefix $(C8_DIR)/,nucleo.c desmontador.c relatorio.c plataforma.c host.c) $(C8_HDR)
	@mkdir -p $(dir $@)
	CPATH="$(HOST_CPATH)" $(HOSTCC) $(HOST_CFLAGS) -DPRINCIPAL=c8_principal_texto -o $@ $(filter %.c,$^)

define C8_ROM_REGRA
$(B)/chip8/jogos/%.$(1): rootfs/jogos/chip8/fuentes/%.asm $(C8_FONTES_J) $(C8_HOST_ASM)
	@mkdir -p $$(dir $$@)
	$(C8_HOST_ASM) $$< -o $$@
$(B)/chip8/testes/%.$(1): rootfs/testes/chip8/%.asm $(C8_FONTES_P) $(C8_HOST_ASM)
	@mkdir -p $$(dir $$@)
	$(C8_HOST_ASM) $$< -o $$@
endef
$(foreach e,ch8 sc8 xo8,$(eval $(call C8_ROM_REGRA,$(e))))

# O cc (compilador de C) embute a sua biblioteca padrão de um cabeçalho.
# Ele mesmo usa a x87 (D039) para ler os números reais e calcular as
# constantes float/double: sem -mgeneral-regs-only, com o arredondamento
# do C padrão (cada atribuição/conversão arredonda para double).
$(B)/userland/programas/cc.o: userland/programas/cc_biblioteca.h userland/programas/gerador_x86.h userland/programas/elf_saida.h
$(B)/userland/testes/pruebacripto.o: userland/testes/pruebacripto_rsa.h
$(B)/userland/programas/cc.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard
# O compilador de Pascal (D085-D087) usa o mesmo gerador do cc e embute a sua
# biblioteca, escrita em Pascal; como o cc, calcula constantes reais com a x87.
$(B)/userland/programas/pascal.o: userland/programas/pascal_biblioteca.h userland/programas/gerador_x86.h userland/programas/elf_saida.h
$(B)/userland/programas/pascal.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -fexcess-precision=standard

# SQLite (D116): amalgamación con las cabeceras de la libc común (D210), y el VFS
# propio con el sagui.h normal.
$(B)/$(SQ_DIR)/sqlite3.o: $(SQ_DIR)/sqlite3.c $(SQ_DIR)/sqlite3.h $(LIBC_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SQFLAGS) -w -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) $(SQLITE_DEFS) -c -o $@ $<
$(B)/userland/libsagui/sqlite_vfs_sagui.o: $(B)/userland/libsagui/%.o: userland/libsagui/%.c $(SQ_DIR)/sqlite3.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SQFLAGS) -Wall -I$(SQ_DIR) -c -o $@ $<
$(B)/userland/bin/sqlite3: $(SQLITE_OBJ) $(LIBC_OBJ) $(QJS_LIBM)
$(B)/userland/programas/sqlite3.o: UCFLAGS := $(SQFLAGS) -Wall -I$(SQ_DIR)

# Regras do MicroPython (as gerais de userland/ valem para libc_sagui.c,
# que usa a libsagui; o resto usa os cabeçalhos de libc/ e não o sagui.h).
$(B)/$(MP_DIR)/micropython_embed/%.o: $(MP_DIR)/micropython_embed/%.c $(MP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS) -c -o $@ $<

# O pegamento do lado do MicroPython: textos em UTF-8 (o str do Python é
# UTF-8; a conversão para o Latin-1 do terminal é feita na saída, D068).
$(patsubst %,$(B)/$(MP_DIR)/%.o,$(MP_PROPRIOS)): $(B)/$(MP_DIR)/%.o: $(MP_DIR)/%.c $(MP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS) -Wall -finput-charset=UTF-8 -c -o $@ $<

# A libm_dbl é código de terceiros (musl/FreeBSD): sem os avisos dela.
$(B)/$(MP_DIR)/libm_dbl/%.o: $(MP_DIR)/libm_dbl/%.c $(MP_DIR)/libm_dbl/libm.h $(MP_DIR)/libc/math.h $(MP_DIR)/libc/float.h
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS) -w -c -o $@ $<

$(B)/$(MP_DIR)/libm_sagui.o: $(MP_DIR)/libm_sagui.c $(MP_DIR)/libc/math.h
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS) -Wall -c -o $@ $<

# O libc_sagui.c usa a libsagui (regra geral de userland/), mas faz parte do
# python, que já exige SSE2: sem -mgeneral-regs-only, como o resto dele. Só o
# inicio_python.c (regra geral, com -mgeneral-regs-only) roda antes de saber
# se há SSE2: ele confere o CPUID e recusa a CPU sem SSE2 com uma mensagem.
$(B)/$(MP_DIR)/libc_sagui.o: UCFLAGS := $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -msse -msse2 -mfpmath=sse

$(B)/userland/bin/python: $(MP_OBJ) $(TLS_OBJ) $(FONTES_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(MP_OBJ) $(TLS_OBJ) $(FONTES_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

# A variante x87 (D082): as mesmas regras, com MPCFLAGS_X87.
$(B)/x87/$(MP_DIR)/micropython_embed/%.o: $(MP_DIR)/micropython_embed/%.c $(MP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS_X87) -c -o $@ $<

$(patsubst %,$(B)/x87/$(MP_DIR)/%.o,$(MP_PROPRIOS)): $(B)/x87/$(MP_DIR)/%.o: $(MP_DIR)/%.c $(MP_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS_X87) -Wall -finput-charset=UTF-8 -c -o $@ $<

$(B)/x87/$(MP_DIR)/libm_dbl/%.o: $(MP_DIR)/libm_dbl/%.c $(MP_DIR)/libm_dbl/libm.h $(MP_DIR)/libc/math.h $(MP_DIR)/libc/float.h
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS_X87) -w -c -o $@ $<

$(B)/x87/$(MP_DIR)/libm_sagui.o: $(MP_DIR)/libm_sagui.c $(MP_DIR)/libc/math.h
	@mkdir -p $(dir $@)
	$(CC) $(MPCFLAGS_X87) -Wall -c -o $@ $<

$(B)/x87/$(MP_DIR)/libc_sagui.o: $(MP_DIR)/libc_sagui.c $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(filter-out -mgeneral-regs-only,$(UCFLAGS)) -mfpmath=387 -c -o $@ $<

$(B)/x87/$(MP_DIR)/inicio_python.o: $(MP_DIR)/inicio_python.c $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -DSAGUI_X87 -c -o $@ $<

$(B)/userland/bin/python-x87: $(MP_OBJ_X87) $(TLS_OBJ) $(FONTES_OBJ) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(MP_OBJ_X87) $(TLS_OBJ) $(FONTES_OBJ) $(LIBSAGUI_OBJ)
	objcopy --strip-all $@.elf $@

$(B)/userland/testes/%: $(B)/userland/testes/%.o $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@

# D378 (fase 1 del enlace dinámico, docs/enlace-dinamico.md): el cargador de
# bibliotecas compartidas en espacio de usuario (userland/libdl, dlopen/dlsym)
# y las bibliotecas .so de prueba de /testes/lib (h385). Las .so se compilan
# con -fPIC y se enlazan con -shared, enlace inmediato (-z now) y RELRO; una
# usa solo DT_GNU_HASH y otra solo DT_HASH. libdlmal.so va sin -fPIC a
# propósito (relocaciones de texto): el cargador tiene que rechazarla.
DL_OBJ     := $(B)/userland/libdl/ligador.o
DL_SO_DIR  := $(B)/userland/testes/lib
DL_TESTE_SO := $(addprefix $(DL_SO_DIR)/,libdlbase.so libdldep.so libdlsha.so libdlsha_simb.so libdlmal.so libcomp_a.so libcomp_b.so)
DL_PIC_CFLAGS := $(filter-out -fno-pic -fno-pie,$(UCFLAGS)) -fPIC -fno-tree-loop-distribute-patterns
DL_SO_LDFLAGS := -m elf_i386 -shared -nostdlib -z now -z relro -z noseparate-code -z max-page-size=4096 -z noexecstack
$(B)/userland/libdl/%.o: userland/libdl/%.c userland/inc/ligador.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c -o $@ $<
$(B)/userland/testes/dl/%.pic.o: userland/testes/dl/%.c kernel/lib/sha256.c kernel/inc/sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(DL_PIC_CFLAGS) -c -o $@ $<
$(B)/userland/testes/dl/%.nopic.o: userland/testes/dl/%.c kernel/lib/sha256.c kernel/inc/sha256.h
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -fno-tree-loop-distribute-patterns -c -o $@ $<
$(DL_SO_DIR)/libdlbase.so: $(B)/userland/testes/dl/base.pic.o
	@mkdir -p $(dir $@)
	$(LD) $(DL_SO_LDFLAGS) --hash-style=gnu -soname libdlbase.so -o $@ $<
$(DL_SO_DIR)/libdldep.so: $(B)/userland/testes/dl/dep.pic.o $(DL_SO_DIR)/libdlbase.so
	$(LD) $(DL_SO_LDFLAGS) --hash-style=sysv -soname libdldep.so -rpath '$$ORIGIN' --enable-new-dtags \
	  -o $@ $< -L$(DL_SO_DIR) -ldlbase
$(DL_SO_DIR)/libdlsha.so: $(B)/userland/testes/dl/sha.pic.o
	@mkdir -p $(dir $@)
	$(LD) $(DL_SO_LDFLAGS) --hash-style=both -soname libdlsha.so -o $@ $<
$(DL_SO_DIR)/libdlsha_simb.so: $(B)/userland/testes/dl/sha.pic.o
	@mkdir -p $(dir $@)
	$(LD) $(DL_SO_LDFLAGS) --hash-style=both -Bsymbolic-functions -soname libdlsha_simb.so -o $@ $<
$(DL_SO_DIR)/libdlmal.so: $(B)/userland/testes/dl/base.nopic.o
	@mkdir -p $(dir $@)
	$(LD) $(DL_SO_LDFLAGS) -z notext --hash-style=both -soname libdlmal.so -o $@ $<
$(B)/userland/testes/dlprueba: $(DL_OBJ) $(B)/userland/testes/dl/sha.nopic.o $(DL_TESTE_SO)
# D378 (fase 3, h390): libcomp.so en dos versiones del mismo tamaño (cambia una letra de un
# texto), enlazadas como las del sistema (código y datos de solo lectura en páginas propias).
$(B)/userland/testes/dl/comp_%.pic.o: userland/testes/dl/comp.c
	@mkdir -p $(dir $@)
	$(CC) $(DL_PIC_CFLAGS) -DVERSAO='"$(shell echo $* | tr a-z A-Z)"' -c -o $@ $<
$(DL_SO_DIR)/libcomp_%.so: $(B)/userland/testes/dl/comp_%.pic.o
	@mkdir -p $(dir $@)
	$(LD) $(SO_LDFLAGS) -soname libcomp.so -o $@ $<

# D378 fase 2: las bibliotecas del sistema como .so y el intérprete /lib/ld-sagui.so.1.
#  - Los objetos PIC los compila una submake con B=$(B)/pic y PIC=1 (mismas reglas y banderas
#    que los estáticos, más -fPIC). Los programas estáticos no cambian.
#  - libsagui.so.1 (la base de LIBSAGUI_BASE_OBJ sin inicio.asm, más dl_ponte.c), libc.so.1 y
#    libsagui_medios.so.1 (el resto de la libsagui) van a /apps/lib mientras solo haya programas
#    dinámicos en /apps (decisión 4a; el ramdisk tiene un límite, h324). Solo el intérprete va a /lib.
#  - Un programa dinámico enlaza inicio.asm.o (su _inicio) y las .so con el guion por defecto
#    del ld, a 0x80000000, con PT_INTERP = /lib/ld-sagui.so.1, -z now y RELRO.
#  - ld-sagui.so.1 es un ELF estático a 0xD8000000 (INTERP_BASE): ligador.c con LD_INTERPRETE.
PIC_B    := $(B)/pic
SO_DIR   := $(B)/lib
SO_APPS_DIR := $(B)/lib-apps
LIBSAGUI_SO := $(SO_APPS_DIR)/libsagui.so.1
LIBC_SO  := $(SO_APPS_DIR)/libc.so.1
LD_SAGUI := $(SO_DIR)/ld-sagui.so.1
MEDIOS_SO := $(SO_APPS_DIR)/libsagui_medios.so.1
SO_SISTEMA := $(LD_SAGUI)
pic_de = $(patsubst $(B)/%,$(PIC_B)/%,$(1))
LIBSAGUI_BASE_PIC := $(call pic_de,$(filter-out %/inicio.asm.o,$(LIBSAGUI_BASE_OBJ)) $(B)/userland/libdl/dl_ponte.o)
LIBSAGUI_MEDIOS_PIC := $(call pic_de,$(LIBSAGUI_MEDIOS_OBJ) $(B)/userland/libtls/sha256.o)   # saguimark usa el SHA-256
LIBC_PIC := $(call pic_de,$(LIBC_OBJ) $(QJS_LIBM))   # con la libm de doble precisión (la del MicroPython, x87)
PIC_FUENTES := $(wildcard userland/libsagui/*.c userland/libsagui/*.asm userland/libc/*.c userland/libc/*.h userland/libc/*/*.h \
  $(MP_DIR)/libm_dbl/*.c $(MP_DIR)/libm_sagui.c \
  userland/inc/*.h kernel/lib/*.c kernel/inc/*.h userland/libdl/*.c)
$(PIC_B)/.objetos: $(PIC_FUENTES) Makefile
	+$(MAKE) --no-print-directory B=$(PIC_B) PIC=1 $(LIBSAGUI_BASE_PIC) $(LIBSAGUI_MEDIOS_PIC) $(LIBC_PIC)
	@touch $@
SO_LDFLAGS := -m elf_i386 -shared -nostdlib -z now -z relro -z text -z noexecstack -z max-page-size=4096 \
  --hash-style=gnu -Bsymbolic-functions
$(LIBSAGUI_SO): $(PIC_B)/.objetos
	@mkdir -p $(dir $@)
	$(LD) $(SO_LDFLAGS) -soname libsagui.so.1 -o $(PIC_B)/$(notdir $@).elf $(LIBSAGUI_BASE_PIC)
	strip --strip-unneeded -o $@ $(PIC_B)/$(notdir $@).elf
$(MEDIOS_SO): $(PIC_B)/.objetos $(LIBSAGUI_SO)
	@mkdir -p $(dir $@)
	$(LD) $(SO_LDFLAGS) -soname libsagui_medios.so.1 -o $(PIC_B)/$(notdir $@).elf $(LIBSAGUI_MEDIOS_PIC) $(LIBSAGUI_SO)
	strip --strip-unneeded -o $@ $(PIC_B)/$(notdir $@).elf
$(LIBC_SO): $(PIC_B)/.objetos $(LIBSAGUI_SO)
	@mkdir -p $(dir $@)
	$(LD) $(SO_LDFLAGS) -soname libc.so.1 -o $(PIC_B)/$(notdir $@).elf $(LIBC_PIC) $(LIBSAGUI_SO)
	strip --strip-unneeded -o $@ $(PIC_B)/$(notdir $@).elf
$(B)/userland/libdl/ligador_ld.o: userland/libdl/ligador.c userland/inc/ligador.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -DLD_INTERPRETE -c -o $@ $<
$(LD_SAGUI): $(B)/userland/libdl/ld_sagui.o $(B)/userland/libdl/ligador_ld.o $(LIBSAGUI_OBJ) userland/libdl/ld-sagui.ld
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -nostdlib -T userland/libdl/ld-sagui.ld --gc-sections -z max-page-size=4096 -z noexecstack \
	  -o $@.elf $(B)/userland/libdl/ld_sagui.o $(B)/userland/libdl/ligador_ld.o $(filter-out %/inicio.asm.o,$(LIBSAGUI_OBJ))
	objcopy --strip-all $@.elf $@
# Enlace de un programa dinámico: $(call enlazar_dinamico,objetos,.so[,opciones del ld])
ULDFLAGS_DIN := -m elf_i386 -nostdlib -Ttext-segment=0x80000000 -e _inicio --gc-sections -z max-page-size=4096 \
  -z noexecstack -z now -z relro -z text --hash-style=gnu --dynamic-linker /lib/ld-sagui.so.1
define enlazar_dinamico
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS_DIN) $(3) -o $@.elf $(B)/userland/libsagui/inicio.asm.o $(1) $(2)
	objcopy --strip-all $@.elf $@
endef
# Programas de /apps que van dinámicos (D378, decisión 4a: /apps primero).
$(B)/userland/bin/buscaminas: $(B)/userland/programas/buscaminas.o $(B)/userland/libsagui/inicio.asm.o $(LIBSAGUI_SO) $(MEDIOS_SO)
	$(call enlazar_dinamico,$(B)/userland/programas/buscaminas.o,$(LIBSAGUI_SO) $(MEDIOS_SO))
# La prueba de la fase 2 (h386): un programa dinámico con libc.so y dlopen por el intérprete.
$(B)/userland/testes/dinamico: $(B)/userland/testes/dinamico.o $(B)/userland/libsagui/inicio.asm.o $(LIBSAGUI_SO) $(LIBC_SO)
	$(call enlazar_dinamico,$(B)/userland/testes/dinamico.o,$(LIBC_SO) $(LIBSAGUI_SO),--export-dynamic-symbol=anfitriao_registrar)
# D378 (fase 3, h390): las páginas compartidas, con las tres bibliotecas del sistema.
$(B)/userland/testes/compartidas: $(B)/userland/testes/compartidas.o $(B)/userland/libsagui/inicio.asm.o $(LIBSAGUI_SO) $(LIBC_SO) $(MEDIOS_SO)
	$(call enlazar_dinamico,$(B)/userland/testes/compartidas.o,$(LIBC_SO) $(LIBSAGUI_SO) $(MEDIOS_SO))
APPS_RECURSOS += lib:$(SO_APPS_DIR) testes:$(B)/apps-testes
$(APPS_DEP): $(MEDIOS_SO) $(LIBSAGUI_SO) $(LIBC_SO) $(B)/apps-testes/.feito
$(B)/apps-testes/.feito: $(DL_PROG_TESTE) $(DL_TESTE_SO)
	@rm -rf $(B)/apps-testes && mkdir -p $(B)/apps-testes/lib
	cp $(DL_PROG_TESTE) $(B)/apps-testes/
	cp $(DL_TESTE_SO) $(B)/apps-testes/lib/
	@touch $@

# ---------- ramdisk (SGFS) ----------
# O ramdisk é montado a partir de uma cópia do rootfs/ com os programas em
# /bin e os programas de teste em /testes. Sem MicroPython, sai também o
# /lib/python (requests e companhia, D067).
#
# D147: a cópia vai para uma pasta temporária só sua e o SGFS escreve-se
# aparte, com renomeado no fim. Assim uma construção interrompida (ou duas
# construções ao mesmo tempo, uma a apagar a pasta da outra) não deixa um
# ramdisk pela metade: antes, um SGFS sem entradas chegava à imagem e o
# kernel arrancava sem nenhum programa, sem que nenhuma aplicação abrisse.
# D270: /sistema/efi/EFI/BOOT lleva el cargador UEFI (BOOTX64.EFI y BOOTIA32.EFI, unos 60 KB): el
# instalador copia /sistema/efi a la partición EFI del disco de destino; /sistema/arranque/mbr.bin
# es el MBR de SaguiOS, para instalar con MBR desde un disco GPT (cuyo LBA 0 es el MBR protector).
# D172: al final la copia queda como $(B)/rootfs (renombrada, no copiada de nuevo):
# las pruebas (h08, h16, h24, h59...) leen de ahí el contenido exacto de la imagen.
# D313: los catálogos de traducción (.cat) salen de los .po de idiomas/ y de las marcas _()/N_()
# del código (el pseudo-idioma «xx» se genera de las fuentes). Van a /sistema/idiomas del ramdisk.
IDIOMAS_FUENTES := $(wildcard idiomas/*/*.po) $(wildcard kernel/gui/*.c kernel/dispositivos/acpi.c kernel/nucleo/supervisor.c kernel/nucleo/energia.c userland/programas/*.c userland/libsagui/*.c userland/inc/*.h)
$(B)/idiomas/.feito: tools/extraer_textos.py $(IDIOMAS_FUENTES)
	@rm -rf $(B)/idiomas && mkdir -p $(B)/idiomas
	$(PY) tools/extraer_textos.py compilar $(B)/idiomas
	@touch $@

# D374/D379: el SGFS admite rutas de hasta 55 caracteres; Freeciv y OpenTTD tienen más largas, así que no van en un
# ramdisk con /apps dentro («completo» y el CD con RAIZ_DISCO=0). Con la raíz en ext2 (M1, lo de por defecto:
# imágenes de disco y la ISO) sí van: el SGFS queda solo para el initramfs y esos formatos de compatibilidad.
SGFS_MAX_RUTA := 55
$(B)/ramdisk.sgfs: $(B)/idiomas/.feito $(ROOTFS) $(PROGRAMAS) $(PROG_TESTE) $(SO_SISTEMA) $(C8_ROMS) $(LMK_PY) tools/mksgfs.py \
                  userland/libsgl/sgl.c userland/libsgl/sgl.h $(RAMDISK_APPS_DEP) $(APPS_MARCA) $(FIRMWARE_RAMDISK) $(FIRMWARE_RADEON) \
                  $(UEFI_EFI) $(B)/boot/mbr.bin
	@set -e; \
	d="$$(mktemp -d $(B)/rootfs.XXXXXX)"; \
	trap 'rm -rf "$$d"' EXIT; \
	chmod 755 "$$d"; \
	cp -r rootfs/. "$$d/"; \
	mkdir -p "$$d/jogos/chip8" "$$d/testes/chip8" "$$d/ejemplos/sagui_gl"; \
	cp userland/libsgl/sgl.c userland/libsgl/sgl.h "$$d/ejemplos/sagui_gl/"; \
	cp $(B)/chip8/jogos/* "$$d/jogos/chip8/"; \
	cp $(B)/chip8/testes/* "$$d/testes/chip8/"; \
	if [ -n "$(PROGRAMAS)" ]; then cp $(PROGRAMAS) "$$d/bin/"; fi; \
	if [ -n "$(PROG_TESTE)" ]; then cp $(PROG_TESTE) "$$d/testes/"; fi; \
	cp $(SO_SISTEMA) "$$d/lib/"; \
	if [ -n "$(RAMDISK_APPS_DEP)" ]; then cp -r $(B)/apps/raiz "$$d/apps"; \
	  for x in freeciv openttd; do \
	    if [ -e "$$d/apps/$$x" ]; then \
	      echo "Makefile: $$x fuera del ramdisk completo (rutas de más de $(SGFS_MAX_RUTA) caracteres para el SGFS; D379)"; \
	      rm -rf "$$d/apps/$$x" "$$d/apps/bin/$$x" "$$d/apps/bin/$$x"-*; fi; \
	  done; \
	  if [ -e "$$d/apps/doom" ]; then \
	    echo "Makefile: DOOM (Freedoom, unos 57 MB) fuera del ramdisk completo (D383; las imágenes con /apps en partición lo conservan)"; \
	    rm -rf "$$d/apps/doom" "$$d/apps/bin"/chocolate-*; fi; \
	else : > "$$d/etc/apps-fuera"; fi; \
	mkdir -p "$$d/lib/python"; \
	cp -r litemonkey/python/litemonkey "$$d/lib/python/"; \
	find "$$d" -type d -name __pycache__ -prune -exec rm -rf {} +; \
	cp litemonkey/python/lmk "$$d/bin/lmk"; \
	if [ "$(MICROPYTHON)" = "0" ]; then rm -rf "$$d/lib/python" "$$d/bin/lmk"; fi; \
	if [ -n "$(FIRMWARE_RAMDISK)" ]; then mkdir -p "$$d/lib/firmware"; cp $(FIRMWARE_RAMDISK) "$$d/lib/firmware/"; fi; \
	if [ -n "$(FIRMWARE_RADEON)" ]; then mkdir -p "$$d/lib/firmware/radeon"; cp $(FIRMWARE_RADEON) "$$d/lib/firmware/radeon/"; fi; \
	mkdir -p "$$d/sistema/efi/EFI/BOOT" "$$d/sistema/arranque" "$$d/sistema/idiomas"; \
	cp -r $(B)/idiomas/* "$$d/sistema/idiomas/"; \
	cp $(UEFI_EFI) "$$d/sistema/efi/EFI/BOOT/"; \
	cp $(B)/boot/mbr.bin "$$d/sistema/arranque/mbr.bin"; \
	$(PY) tools/mksgfs.py "$$d" "$@.novo"; \
	mv -f "$@.novo" "$@"; \
	if [ -e $(B)/rootfs ]; then rm -rf $(B)/rootfs.antigo; mv $(B)/rootfs $(B)/rootfs.antigo; fi; \
	mv "$$d" $(B)/rootfs; \
	rm -rf $(B)/rootfs.antigo

# D379 (M1): el initramfs de las imágenes (live; el CD lo arranca en toram y el instalador marca la
# cabecera del disco instalado para la raíz con escritura). Sale de build/rootfs, que hace el ramdisk.
$(INITRAMFS): $(B)/ramdisk.sgfs tools/mkinitramfs.py tools/mksgfs.py
	$(PY) tools/mkinitramfs.py $(B)/rootfs $@.novo --modo live --capa auto
	mv -f $@.novo $@

# ---------- las fuentes de Typodermic (D196) ----------
$(B)/fuentes-res/.feito: tools/fuentes_typodermic.py $(FUENTES_ZIP)
	@rm -rf $(B)/fuentes-res && mkdir -p $(B)/fuentes-res
	$(PY) tools/fuentes_typodermic.py seleccion $(B)/fuentes-res $(if $(FUENTES_RED),,--sin-red)
	@touch $@

$(B)/$(FONTES_DIR)/%.o: $(FONTES_DIR)/%.c userland/inc/fontes.h $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(FONTES_FLAGS) -c -o $@ $<

# Los programas que usan fuentes vectoriales enlazan la libfontes.
$(B)/userland/bin/pluma $(B)/userland/bin/tucan $(B)/userland/bin/fuentes $(B)/userland/bin/macaco $(B)/userland/bin/configuracion $(B)/userland/bin/editor $(B)/userland/bin/paint $(B)/userland/bin/terminal $(B)/userland/testes/pruebafuentes: $(FONTES_OBJ)

# ---------- /apps (D194) ----------
# D228: lo de $(INPUT_SHARE)/apps (docs/input_share.md) va encima, tal cual; la marca
# (tools/marca_carpeta.py) cambia cuando cambia lo que hay allí.
APPS_MARCA_ENTRADA := $(B)/apps/entradas.txt
$(shell mkdir -p $(B)/apps && $(PY) tools/marca_carpeta.py $(APPS_MARCA_ENTRADA) "INPUT_SHARE=$(INPUT_SHARE)" "$(INPUT_SHARE)/apps")
# El árbol de la partición SAGUI-APPS: /bin con los ejecutables de las apps y
# una carpeta por app con sus datos, más el leeme.txt. Se monta en una carpeta
# temporal que solo al final se renombra (como el ramdisk, D147).
# D324: guiones de Python que van a /apps/bin junto a python (chip8-roms).
APPS_GUIONES := $(if $(filter 1,$(MICROPYTHON)),$(wildcard userland/apps/bin/*))
$(APPS_DEP): $(APPS_BINS) $(APPS_GUIONES) $(wildcard userland/apps/videos/*) $(NS_RECURSOS) $(FUENTES_RES) $(OJ_RECURSOS) $(CNC_RECURSOS) $(QK_RECURSOS) $(GB_RECURSOS) $(DB_RECURSOS) $(LN_RECURSOS) $(GS_RECURSOS) $(MG_RECURSOS) $(SHINE_RECURSOS) $(GO_RECURSOS) $(FC_RECURSOS) $(AOEN_RES) $(CD_RECURSOS) $(JV_RECURSOS) $(APPS_MARCA_ENTRADA) userland/apps/leeme.txt
	@set -e; \
	mkdir -p $(B)/apps; \
	t="$$(mktemp -d $(B)/apps/tmp.XXXXXX)"; \
	trap 'rm -rf "$$t"' EXIT; \
	chmod 755 "$$t"; \
	mkdir "$$t/bin"; \
	if [ -n "$(APPS_BINS)" ]; then cp $(APPS_BINS) "$$t/bin/"; fi; \
	if [ -n "$(APPS_GUIONES)" ]; then cp $(APPS_GUIONES) "$$t/bin/"; fi; \
	for r in $(APPS_RECURSOS); do mkdir -p "$$t/$${r%%:*}"; cp -r "$${r#*:}"/* "$$t/$${r%%:*}/"; done; \
	cp userland/apps/leeme.txt "$$t/leeme.txt"; \
	if [ -n "$(INPUT_SHARE)" ] && [ -d "$(INPUT_SHARE)/apps" ]; then \
	  echo "Makefile: $(INPUT_SHARE)/apps -> /apps"; cp -r "$(INPUT_SHARE)/apps/." "$$t/"; fi; \
	rm -rf $(B)/apps/raiz; \
	mv "$$t" $(B)/apps/raiz; \
	touch $@

# El ramdisk del CD con RAIZ_DISCO=0 (el formato de siempre): el mismo, con /apps dentro.
$(B)/ramdisk-cd.sgfs: $(B)/ramdisk.sgfs $(APPS_DEP) tools/mksgfs.py
	@set -e; \
	d="$$(mktemp -d $(B)/rootfs-cd.XXXXXX)"; \
	trap 'rm -rf "$$d"' EXIT; \
	chmod 755 "$$d"; \
	cp -r $(B)/rootfs/. "$$d/"; \
	cp -r $(B)/apps/raiz "$$d/apps"; \
	for x in freeciv openttd; do \
	  if [ -e "$$d/apps/$$x" ]; then \
	    echo "Makefile: $$x fuera del CD con RAIZ_DISCO=0 (rutas de más de $(SGFS_MAX_RUTA) caracteres para el SGFS; con la raíz en ext2 sí va, D379)"; \
	    rm -rf "$$d/apps/$$x" "$$d/apps/bin/$$x" "$$d/apps/bin/$$x"-*; fi; \
	done; \
	if [ -e "$$d/apps/doom" ]; then \
	  echo "Makefile: DOOM (Freedoom) fuera del CD con RAIZ_DISCO=0 (D383; con la raíz en ext2 sí va)"; \
	  rm -rf "$$d/apps/doom" "$$d/apps/bin"/chocolate-*; fi; \
	$(PY) tools/mksgfs.py "$$d" "$@.novo"; \
	mv -f "$@.novo" "$@"

# ---------- partição de jogos (D181, D228) ----------
# La raíz de SAGUI-JOGOS, que se monta en /jogos (lectura y escritura, D228):
#   - lo que el ramdisk tiene en /jogos (chip8, jclic, sagui3d): el montaje lo
#     tapa, así que va también aquí (el ramdisk lo sigue llevando para las imágenes
#     sin la partición: el CD y la variante sin GPL);
#   - opentyrian/: los datos del OpenTyrian, de la imagen FAT32 del disco aparte (D156);
#   - openjazz/: el shareware de Jazz Jackrabbit si se bajó (tools/openjazz_datos.sh;
#     OPENJAZZ_DATOS=0 lo deja fuera) y un leeme que dice cómo conseguir los datos;
#   - quake/: el shareware de Quake si se bajó (tools/quake_datos.sh, D229;
#     QUAKE_DATOS=0 lo deja fuera) y un leeme que dice cómo conseguir los datos;
#   - lo que haya en $(INPUT_SHARE)/games/<carpeta> (docs/input_share.md); de
#     $(INPUT_SHARE)/games/CnC, si tiene las ISO de C&C Gold, lo que necesita Vanilla
#     Conquer (tools/cnc_extraer.py, D230; con las películas si CNC_VIDEOS=1).
# Se arma en una carpeta temporal que solo al final se renombra: una extracción cortada
# a medias no deja una carpeta incompleta que parezca buena. La marca
# (tools/marca_carpeta.py) cambia cuando cambia lo que hay en input_share/games o en los
# datos de OpenJazz o de Quake (o INPUT_SHARE, OPENJAZZ_DATOS y QUAKE_DATOS), y entonces
# se rehace.
OPENJAZZ_DATOS ?= 1
CNC_VIDEOS ?= 0
OJ_DATOS_DIR := userland/terceros/openjazz/datos
QUAKE_DATOS ?= 1
QK_DATOS_DIR := userland/terceros/quake/datos
JOGOS_MARCA := $(B)/jogos/entradas.txt
$(shell mkdir -p $(B)/jogos && $(PY) tools/marca_carpeta.py $(JOGOS_MARCA) "OPENJAZZ_DATOS=$(OPENJAZZ_DATOS) QUAKE_DATOS=$(QUAKE_DATOS) INPUT_SHARE=$(INPUT_SHARE) CNC_VIDEOS=$(CNC_VIDEOS)" "$(INPUT_SHARE)/games" $(OJ_DATOS_DIR) $(QK_DATOS_DIR))
JOGOS_BASE := $(shell find rootfs/jogos -type f 2>/dev/null | grep -v ' ')
$(B)/jogos/.feito: $(OT_DIR)/dados.img.xz $(OT_DIR)/dados-leeme.txt $(JOGOS_MARCA) $(C8_ROMS) $(JOGOS_BASE) \
                   userland/terceros/openjazz/datos-leeme.txt userland/terceros/quake/datos-leeme.txt userland/terceros/gnuboy/roms-leeme.txt \
                   tools/copiar_jogos.sh tools/cnc_extraer.py
	@CNC_VIDEOS="$(CNC_VIDEOS)" sh tools/copiar_jogos.sh "$(B)/jogos" "$(OT_DIR)" "$(B)/chip8/jogos" "$(OJ_DATOS_DIR)" "$(OPENJAZZ_DATOS)" "$(INPUT_SHARE)" "$(QK_DATOS_DIR)" "$(QUAKE_DATOS)"
	touch $@

# ---------- imagem final ----------
# D264: la imagen normal es híbrida: arranca por BIOS (MBR) y por UEFI (la ESP SAGUI-EFI).
$(B)/saguios.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi $(JOGOS_OPC_IMG) $(APPS_OPC)
	cd $(B) && sha256sum saguios.img > saguios.img.sha256

# Todas las variantes en una carpeta, con SHA256SUMS.txt y VERSION.txt
# (gerar_versao.sh --todas). Los tamaños se cambian con las variables de
# siempre (HOME_MB, JOGOS_MB, DATOS_FAT32_MB...; ver gerar_versao.sh --help).
#   make variantes [DESTINO=entrega/v7.0b] [GERAR_OPC="--probar"]
DESTINO ?= entrega/latest
GERAR_OPC ?=
variantes:
	./gerar_versao.sh --todas --destino $(DESTINO) $(GERAR_OPC)

# Variantes privadas (solo para quien compila, NUNCA publicar, D228): todas las de
# «variantes» con los juegos de input_share/ y los datos que no van en git. Antes
# baja (si faltan) lo que hace falta: las fuentes del navegador NetSurf (D184), las del
# emulador GNUBoy (D235, D261), los
# datos shareware de Quake (D229) y OpenJazz (D228) y las fuentes de Typodermic
# (D196). Los guiones no repiten la descarga si ya está hecha.
#   make priv_variantes [PRIV_DESTINO=entrega/latest] [GERAR_OPC="--probar"]
PRIV_DESTINO ?= entrega/latest
TYPODERMIC_ZIP := typodermic-public-domain-2024-12.zip
priv_descargas:
	@[ -f userland/terceros/netsurf/fuentes/.preparado ] || bash tools/netsurf_fuentes.sh
	@[ -f userland/terceros/gnuboy/jogo/VERSAO ] || bash tools/gnuboy_fuentes.sh
	@# D374: Freeciv (GPL-2.0 o posterior) también va en las públicas si sus fuentes están preparadas
	@# (antes de «make variantes», ejecutar tools/freeciv_fuentes.sh); aquí se preparan para las privadas.
	bash tools/freeciv_fuentes.sh
	@# D384: TCC (LGPL-2.1 o posterior), el compilador de C de dentro y AoEN compilable dentro (públicas y privadas).
	@[ -f userland/terceros/tcc/fuentes/.preparado ] || bash tools/tcc_fuentes.sh
	@# D383: Chocolate Doom (GPL-2.0 o posterior) y Freedoom (BSD-3-Clause) también van en las públicas
	bash tools/doom_fuentes.sh
	bash tools/doom_datos.sh
	bash tools/quake_datos.sh
	bash tools/openjazz_datos.sh
	@$(PY) -c "import sys; sys.path.insert(0, 'tools'); import fuentes_typodermic as f; sys.exit(0 if f.buscar_zip() else 3)" || \
	    echo "priv_descargas: sin las fuentes de Typodermic (opcionales; se sigue sin ellas): baja a mano $(TYPODERMIC_ZIP) de typodermicfonts.com/public-domain/ y ponlo en NEW_FONTS/"
	@[ -d input_share ] || echo "priv_descargas: AVISO: no hay input_share/ (los juegos propios no entrarán; ver docs/input_share.md)"
	@# D375: OpenTTD (GPL-2.0 solo) va SOLO en las imágenes privadas: se compila como paquete desde su repositorio
	@# aparte y se instala en input_share/apps, que el make vuelca en /apps. Las públicas (INPUT_SHARE=) no lo llevan.
	@if [ -d "$(OPENTTD_REPO_DIR)/.git" ] || [ -n "$(OPENTTD_URL)" ]; then \
	    mkdir -p input_share/apps && bash tools/openttd_paquete.sh && bash tools/openttd_paquete.sh instalar; \
	else echo "priv_descargas: sin el repositorio de OpenTTD ($(OPENTTD_REPO_DIR) u OPENTTD_URL): la imagen privada no lo llevará"; fi

OPENTTD_REPO_DIR ?= $(if $(OPENTTD_REPO),$(OPENTTD_REPO),../openttd-saguios)
OPENTTD_URL ?=

priv_variantes: priv_descargas
	./gerar_versao.sh --todas --con-input-share --destino $(PRIV_DESTINO) $(GERAR_OPC)
	@echo "priv_variantes: en $(PRIV_DESTINO): son para uso propio, NO publicar (D228)"

# Ayuda: objetivos y variables principales (el detalle, en compilar_versiones.md).
help:
	@echo "SaguiOS: objetivos principales (detalle en compilar_versiones.md)"
	@echo "  make                  build/saguios.img: la imagen base (sin NetSurf ni GNUBoy si no estan sus fuentes)"
	@echo "  make vdi              ademas build/saguios.vdi (VirtualBox)"
	@echo "  make imagem-persistente | imagem-persistente-fat32 | imagem-particoes | imagem-smp | imagem-uacpi"
	@echo "  make variantes [DESTINO=entrega/latest]    todas las imagenes publicables (sin NetSurf, GNUBoy ni input_share)"
	@echo "  make priv_descargas   baja lo que no va en git: NetSurf, GNUBoy, datos shareware de Quake y OpenJazz, fuentes Typodermic"
	@echo "  make priv_variantes [PRIV_DESTINO=entrega/latest]   igual que variantes pero con todo lo descargado y los juegos de"
	@echo "                        input_share/: para uso PROPIO, no se distribuyen (NetSurf y GNUBoy son GPL v2)"
	@echo "  make test | rodar | limpar | entrega FASE=N | help"
	@echo "Variables: B (carpeta de compilacion) INPUT_SHARE (vacio = sin juegos propios) JOGOS=0 GNUBOY=0 NETSURF=0 CONQUER=0 QUAKE=0"
	@echo "  OPENJAZZ=0 DOSBOX=0 MGBA=0 APPS=0 HOME_MB TMP_MB LOGS_MB JOGOS_MB DATOS_MB PART_FS FUENTES_RED=1 (baja Typodermic si falta) SEM_GPL=1"
	@echo "  GERAR_OPC=\"--probar\" (opciones de gerar_versao.sh)"
	@echo "Entorno: . ~/.local/saguitools/env.sh antes de make (ver compilar_versiones.md)"

# D270: todas las imágenes de disco (multinúcleo, uACPI, persistentes, particiones) son híbridas
# BIOS+UEFI como saguios.img (--uefi, D264); solo la ISO sigue siendo solo BIOS.
# Imagen multinúcleo (D164): la misma, con los procesadores secundarios
# encendidos. Sin el IOAPIC (D166): en el Amilo (945) las IRQ PCI del USB no
# llegaban por él y todo iba lentísimo; hasta que el IOAPIC compruebe al
# arrancar que llegan las de los dispositivos, va aparte (--ioapic a mano).
imagem-smp: $(B)/saguios-smp.img
$(B)/saguios-smp.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --smp $(JOGOS_OPC_IMG) $(APPS_OPC)

# Imagen con el intérprete de AML (uACPI) y el indicador de batería (D192):
# la misma imagen con --uacpi. Por defecto el arranque no ejecuta AML; esta es
# la que se prueba en los portátiles. Va aparte (como imagem-smp) para que
# build/saguios.img no cambie según una variable.
imagem-uacpi: $(B)/saguios-uacpi.img
$(B)/saguios-uacpi.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --uacpi $(JOGOS_OPC_IMG) $(APPS_OPC)
	cd $(B) && sha256sum saguios-uacpi.img > saguios-uacpi.img.sha256

# D077: CD arrancável (El Torito com emulação de disco rígido). A imagem de
# dentro é a de sempre, com OPC_CD no cabeçalho (o disco de arranque é só
# leitura) e completada até um cilindro inteiro; a ISO sai do tools/mkiso.py,
# sem xorriso nem genisoimage.
iso: $(B)/saguios.iso

# D379 (M1): el CD lleva la raíz (con /apps dentro, también Freeciv) en la imagen El Torito y arranca en
# toram; con RAIZ_DISCO=0, las apps de /apps dentro de su ramdisk (ramdisk-cd.sgfs, D194).
$(B)/saguios-cd.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(CD_DEP) tools/mkimagem.py
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf $(CD_OPC) \
	    --saida $@ --vesa --cd

# D270: la ISO arranca también por UEFI (EFI.IMG con el cargador; el kernel y el ramdisk, de la imagen de dentro).
$(B)/saguios.iso: $(B)/saguios-cd.img tools/mkiso.py $(UEFI_EFI)
	$(PY) tools/mkiso.py --imagem $< --saida $@ --efi $(B)/boot/uefi
	cd $(B) && sha256sum saguios.iso > saguios.iso.sha256

# D040: imagem com partição de dados persistente (precisa do mke2fs do host).
DATOS_MB ?= 64
imagem-persistente: $(B)/saguios-persistente.img

$(B)/saguios-persistente.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --dados-mb $(DATOS_MB) $(JOGOS_OPC) $(APPS_OPC) $(PERSIST_OPC)

# La misma imagen con una partición FAT32 de datos (/home), legible también
# desde Windows. Sale esparsa: ocupa poco en disco aunque mida DATOS_FAT32_MB.
DATOS_FAT32_MB ?= 2048
imagem-persistente-fat32: $(B)/saguios-persistente-fat32.img

# Las persistentes con todos los procesadores (--smp, D164; sin IOAPIC, ver
# imagem-smp): las mismas particiones, con el multinúcleo encendido.
imagem-persistente-smp: $(B)/saguios-persistente-smp.img $(B)/saguios-persistente-fat32-smp.img
# Persistente ext2 multinúcleo con uACPI (--uacpi, D192): la de los portátiles, con el
# indicador de batería; la genera también «make variantes» (gerar_versao.sh --uacpi).
imagem-persistente-smp-uacpi: $(B)/saguios-persistente-smp-uacpi.img
$(B)/saguios-persistente-smp-uacpi.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --smp --uacpi --dados-mb $(DATOS_MB) $(JOGOS_OPC) $(APPS_OPC) $(PERSIST_OPC)
$(B)/saguios-persistente-smp.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --smp --dados-mb $(DATOS_MB) $(JOGOS_OPC) $(APPS_OPC) $(PERSIST_OPC)

$(B)/saguios-persistente-fat32-smp.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --smp --dados-mb $(DATOS_FAT32_MB) --dados-fs fat32 $(JOGOS_OPC) $(APPS_OPC) $(PERSIST_OPC)

$(B)/saguios-persistente-fat32.img: $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
	$(PY) tools/mkimagem.py --mbr $(B)/boot/mbr.bin --estagio2 $(B)/boot/estagio2.bin \
	    --kernel $(B)/kernel.bin --kernel-elf $(B)/kernel.elf --ramdisk $(IMG_RAMDISK) $(RAIZ_OPC) \
	    --saida $@ --vesa --uefi $(B)/boot/uefi --dados-mb $(DATOS_FAT32_MB) --dados-fs fat32 $(JOGOS_OPC) $(APPS_OPC) $(PERSIST_OPC)

# D120: imagen con varias particiones de datos (/home, /tmp y /logs) para grabar con Rufus o dd.
# Tamaños en MB; 0 = sin esa partición. Sale dispersa. Ejemplo:
#   make imagem-particoes HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32
#   make imagem-particoes-xz   (además .img.xz)     python3 tools/crear-disco.py -i   (asistente)
HOME_MB ?= 2048
TMP_MB ?= 256
LOGS_MB ?= 128
PART_FS ?= fat32
PART_EXT ?=            # D213: todas van ya como lógicas en una extendida; PART_EXT=--primarias, el formato de antes
PART_OPC = --home-mb $(HOME_MB) --tmp-mb $(TMP_MB) --logs-mb $(LOGS_MB) --fs $(PART_FS) $(PART_EXT) --salida $(B)/saguios-particiones.img $(JOGOS_OPC) \
           $(if $(APPS_OPC),--apps $(B)/apps/raiz --apps-mb $(APPS_MB),--sin-apps) $(if $(filter 1,$(RAIZ_DISCO)),--initramfs $(INITRAMFS),--clasica)
PART_DEPS = $(B)/boot/mbr.bin $(B)/boot/estagio2.bin $(B)/kernel.bin $(IMG_RAMDISK_DEP) tools/mkimagem.py tools/crear-disco.py $(JOGOS_DEP) $(APPS_IMG_DEP) $(UEFI_EFI)
imagem-particoes: $(PART_DEPS)
	$(PY) tools/crear-disco.py $(PART_OPC)

imagem-particoes-xz: $(PART_DEPS)
	$(PY) tools/crear-disco.py $(PART_OPC) --xz

$(B)/saguios.vdi: $(B)/saguios.img
	rm -f $@ $(B)/vdi.raw
	cp $< $(B)/vdi.raw
	# disco virtual com tamanho múltiplo de 1 MB (ver D004)
	truncate -s %1M $(B)/vdi.raw
	qemu-img convert -f raw -O vdi $(B)/vdi.raw $@
	rm -f $(B)/vdi.raw
	cd $(B) && sha256sum saguios.vdi > saguios.vdi.sha256

vdi: $(B)/saguios.vdi

test: $(B)/saguios.img
	$(PY) tests/rodar_todos.py

rodar: $(B)/saguios.img
	qemu-system-i386 -m 512M -drive format=raw,file=$< -serial stdio

entrega: $(B)/saguios.img $(B)/saguios.vdi
	@test -n "$(FASE)" || (echo "use: make entrega FASE=N" && false)
	mkdir -p entrega/fase$(FASE)
	cp $(B)/saguios.img $(B)/saguios.vdi entrega/fase$(FASE)/
	cd entrega/fase$(FASE) && sha256sum saguios.img saguios.vdi > SHA256SUMS.txt
	@cat entrega/fase$(FASE)/SHA256SUMS.txt

# D109: la imagen sin nada de licencia GPL (compilada en build-sem-gpl/) copiada
# como build-sem-gpl/saguios-sin-gpl.img. Antes pasa la auditoría de licencias.
.PHONY: entrega-sin-gpl auditar-licencias
auditar-licencias:
	$(PY) tools/auditar_licencias.py

entrega-sin-gpl: auditar-licencias
	$(MAKE) SEM_GPL=1 build-sem-gpl/saguios.img
	cp build-sem-gpl/saguios.img build-sem-gpl/saguios-sin-gpl.img
	cd build-sem-gpl && sha256sum saguios-sin-gpl.img > saguios-sin-gpl.img.sha256
	@cat build-sem-gpl/saguios-sin-gpl.img.sha256

# A versão completa (imagens, ISO, VDI e a entrega em entrega/latest) monta o
# guion gerar_versao.sh, que também parametriza a compilação.
.PHONY: versao
versao:
	./gerar_versao.sh

dashboard:
	$(PY) tools/status.py atualizar

limpar:
	rm -rf $(B)
