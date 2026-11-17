# SPDX-License-Identifier: GPL-3.0-only
# java.mk - miniJVM (D376): /apps/bin/java y la biblioteca de clases en /apps/java/lib. Lo incluye el Makefile.
#
# miniJVM (MIT; la biblioteca de clases lleva partes de OpenJDK GPL-2.0-only con Classpath-exception) viene de
# tools/minijvm_fuentes.sh (no va en git). La VM se compila con la libc común y la x87; el JIT está
# apagado (miniJVM lo apaga en 32 bits). La biblioteca de clases se compila con el JDK del anfitrión
# (tools/java_entorno.sh instalar), con --release 8. Sin fuentes se salta con un aviso. JAVA=0 lo desactiva.

JV_DIR  := userland/terceros/minijvm
JV_C    := $(JV_DIR)/fuentes/minijvm/c
JV_B    := $(B)/$(JV_DIR)
JAVA ?= 1
# La biblioteca de clases lleva partes de OpenJDK (GPL-2.0 con Classpath-exception): fuera de la variante sin GPL (D109).
ifeq ($(SEM_GPL),1)
JAVA := 0
endif
JV_PROGRAMAS :=

ifeq ($(JAVA),1)
ifneq ($(wildcard $(JV_C)/jvm/jvm.c),)
JV_SRC := $(filter-out $(JV_C)/jvm/os_posix.c $(JV_C)/jvm/os_win.c $(JV_C)/jvm/jit.c $(JV_C)/jvm/jdwp.c, \
  $(wildcard $(JV_C)/jvm/*.c) $(wildcard $(JV_C)/utils/*.c)) $(JV_DIR)/plataforma/os_sagui.c \
  $(JV_DIR)/plataforma/pthread_sagui.c $(JV_DIR)/plataforma/minijvm_sustitutos.c $(JV_DIR)/plataforma/depuracion_sagui.c $(JV_DIR)/plataforma/consola_sagui.c $(JV_DIR)/plataforma/registro_sagui.c $(JV_DIR)/plataforma/gui_nativos.c $(JV_DIR)/plataforma/java_main.c
JV_OBJ := $(foreach f,$(JV_SRC),$(JV_B)/$(subst /,_,$(f:.c=.o)))

JV_FLAGS := -m32 -march=i686 -std=gnu99 -O2 \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG -mfpmath=387 -fexcess-precision=standard -fcommon \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -I$(JV_DIR)/plataforma/inc -I$(JV_C) -I$(JV_C)/jvm -I$(JV_C)/utils -w -finput-charset=UTF-8 -fexec-charset=UTF-8 \
  -D__saguios__ -DMADV_DONTNEED=4 -include sagui_red.h -I$(JV_C)/utils/https

# d_type.c: su jvm_printf se sustituye por el de registro_sagui.c
$(JV_B)/$(subst /,_,$(JV_C)/utils/d_type.o): JV_EXTRA := -include jvm_printf_renombrado.h

# jni_std.c: la consola convierte entre UTF-8 (Java) y Latin-1 (terminal), ver consola_latin1.h
$(JV_B)/$(subst /,_,$(JV_C)/jvm/jni_std.o): JV_EXTRA := -include consola_latin1.h

define JV_REGLA
$(JV_B)/$(subst /,_,$(1:.c=.o)): $(1) $(LIBC_HDR) $(JV_DIR)/java.mk $(wildcard $(JV_DIR)/plataforma/inc/*.h)
	@mkdir -p $$(dir $$@)
	$$(CC) $$(JV_FLAGS) $$(JV_EXTRA) -c -o $$@ $$<
endef
$(foreach f,$(JV_SRC),$(eval $(call JV_REGLA,$(f))))

$(B)/userland/bin/java: $(JV_OBJ) $(FONTES_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

# javac (Janino sobre esta misma VM): una copia de java que se reconoce por su nombre (java_main.c)
$(B)/userland/bin/javac: $(B)/userland/bin/java
	cp $< $@

JV_PROGRAMAS := $(B)/userland/bin/java $(B)/userland/bin/javac
else
$(info Makefile: sin fuentes de miniJVM: se salta Java (tools/minijvm_fuentes.sh, docs/java.md))
endif
endif

# ---- biblioteca de clases (minijvm_rt.jar) y ejemplos: con el JDK del anfitrión (tools/java_entorno.sh) ----
JV_JDK := $(wildcard $(HOME)/.local/saguitools/jdk/bin/javac)
JV_FUENTES_JAVA := $(JV_DIR)/fuentes/minijvm/java
JV_RES := $(B)/java-res
JV_RT := $(JV_RES)/lib/minijvm_rt.jar
JV_EJEMPLOS_SRC := $(wildcard userland/java/ejemplos/*.java)

.PHONY: java-clases
ifneq ($(JV_PROGRAMAS),)
ifneq ($(JV_JDK),)
$(JV_RT): $(JV_C)/jvm/jvm.c $(JV_DIR)/java.mk
	@mkdir -p $(dir $@)
	rm -rf $(B)/java-obj/rt
	mkdir -p $(B)/java-obj/rt/META-INF
	find $(JV_FUENTES_JAVA)/src/main/java -name '*.java' > $(B)/java-obj/rt.txt
	$(dir $(JV_JDK))javac --release 8 -nowarn -encoding utf-8 -d $(B)/java-obj/rt @$(B)/java-obj/rt.txt
	cp -r $(JV_FUENTES_JAVA)/src/main/resource/. $(B)/java-obj/rt/
	cp $(JV_FUENTES_JAVA)/MIT_LICENSE $(JV_FUENTES_JAVA)/LICENSE $(JV_FUENTES_JAVA)/CLASSPATH_EXCEPTION $(B)/java-obj/rt/META-INF/
	$(dir $(JV_JDK))jar cf $@.novo -C $(B)/java-obj/rt .
	mv -f $@.novo $@

# La biblioteca gráfica sagui.gui (userland/java/sagui): paquete propio sobre los métodos nativos de gui_nativos.c
JV_SAGUI_SRC := $(wildcard userland/java/sagui/gui/*.java)
$(JV_RES)/lib/sagui.jar: $(JV_SAGUI_SRC) $(JV_RT) tools/javac-sagui
	@mkdir -p $(B)/java-obj/sagui
	rm -rf $(B)/java-obj/sagui
	mkdir -p $(B)/java-obj/sagui
	tools/javac-sagui -d $(B)/java-obj/sagui $(JV_SAGUI_SRC)
	$(dir $(JV_JDK))jar cf $@ -C $(B)/java-obj/sagui .

# Los ejemplos de userland/java/ejemplos (tools/javac-sagui los compila contra la biblioteca de miniJVM).
$(JV_RES)/ejemplos/.feito: $(JV_RT) $(JV_RES)/lib/sagui.jar $(JV_EJEMPLOS_SRC) tools/javac-sagui
	@mkdir -p $(JV_RES)/ejemplos
	tools/javac-sagui -d $(JV_RES)/ejemplos $(JV_EJEMPLOS_SRC)
	@touch $@

# Janino (BSD-3-Clause, tools/java_entorno.sh janino): el compilador de Java que corre sobre miniJVM
JV_JANINO := $(wildcard $(HOME)/.local/saguitools/janino/janino-*.jar)
$(JV_RES)/lib/janino.jar: $(JV_JANINO)
	@mkdir -p $(dir $@)
	cp $(wildcard $(HOME)/.local/saguitools/janino/janino-*.jar) $@
	cp $(wildcard $(HOME)/.local/saguitools/janino/commons-compiler-*.jar) $(dir $@)commons-compiler.jar

$(JV_RES)/lib/sagui-javac.jar: userland/java/javac/SaguiJavac.java $(JV_RES)/lib/janino.jar $(JV_RT) tools/javac-sagui
	@mkdir -p $(B)/java-obj/javac
	tools/javac-sagui -cp $(JV_RES)/lib/janino.jar:$(JV_RES)/lib/commons-compiler.jar -d $(B)/java-obj/javac userland/java/javac/SaguiJavac.java
	$(dir $(JV_JDK))jar cf $@ -C $(B)/java-obj/javac .

$(JV_RES)/leeme.txt: userland/java/leeme.txt userland/java/LICENCIA-TERCEROS.txt
	@mkdir -p $(JV_RES)
	cp userland/java/leeme.txt $@
	cp userland/java/LICENCIA-TERCEROS.txt $(JV_RES)/LICENCIA-TERCEROS.txt

java-clases: $(JV_RES)/lib/sagui.jar $(if $(JV_JANINO),$(JV_RES)/lib/sagui-javac.jar) $(JV_RT) $(JV_RES)/ejemplos/.feito $(JV_RES)/leeme.txt
JV_RECURSOS := $(JV_RES)/.feito
$(JV_RECURSOS): $(JV_RES)/lib/sagui.jar $(if $(JV_JANINO),$(JV_RES)/lib/sagui-javac.jar) $(JV_RT) $(JV_RES)/ejemplos/.feito $(JV_RES)/leeme.txt
	@touch $@
else
$(info Makefile: sin JDK: la biblioteca de clases de Java no se compila (tools/java_entorno.sh instalar, docs/java.md))
java-clases:
	@echo "Sin JDK: tools/java_entorno.sh instalar"
endif
endif
