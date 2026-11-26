# teste.mk - /apps/bin/sdl2cuadro: programa sintético para medir la cadena de dibujo de la
# SDL 2 (D247, prueba h246). Usa la SDL 2 del sistema (userland/libsdl2).
# Las opciones, si ningún port las definió (sdl2_capas.mk define SDL2_OBJ pero no SDL2_FLAGS: sin GPL
# no hay mgba ni gnuboy que las pongan).
ifndef SDL2_FLAGS
SDL2_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
endif
ifndef SDL2_OBJ
SDL2_OBJ := $(B)/userland/libsdl2/sdl2_sagui.o
$(SDL2_OBJ): userland/libsdl2/sdl2_sagui.c userland/libsdl2/sdl2gpu.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<
endif

S2C_OBJ := $(B)/userland/libsdl2/sdl2cuadro.o
$(S2C_OBJ): userland/libsdl2/sdl2cuadro.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/sdl2cuadro: $(S2C_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/sdl2cuadro

# D288: prueba del renderizador por GPU (h288): las mismas escenas por software y «aceleradas».
S2G_OBJ := $(B)/userland/libsdl2/sdl2gpuprova.o
$(S2G_OBJ): userland/libsdl2/sdl2gpuprova.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/sdl2gpu: $(S2G_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/sdl2gpu

# D432: prueba y medida de «presentar sin volver a la RAM» (h432): un juego sintético a pantalla completa.
S2P_OBJ := $(B)/userland/libsdl2/sdl2presentarprova.o
$(S2P_OBJ): userland/libsdl2/sdl2presentarprova.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/sdl2presentar: $(S2P_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/sdl2presentar

# D393: prueba del audio de la SDL 2 con hilo propio (h393): un seno por la llamada de vuelta o por la cola mientras el
# principal se queda ocupado cuatrocientos milisegundos.
S2A_OBJ := $(B)/userland/libsdl2/sdl2audioprova.o
$(S2A_OBJ): userland/libsdl2/sdl2audioprova.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/sdl2audio: $(S2A_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/sdl2audio

# D476: prueba del limitador de FPS de la SDL 2 (h476): cuenta cuadros presentados con 30, 60 o sin tope y mide la CPU.
S2F_OBJ := $(B)/userland/libsdl2/sdl2fpsprova.o
$(S2F_OBJ): userland/libsdl2/sdl2fpsprova.c userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2_FLAGS) -c -o $@ $<

$(B)/userland/bin/sdl2fps: $(S2F_OBJ) $(SDL2_OBJ) $(LIBC_OBJ) $(QJS_LIBM) $(LIBSAGUI_OBJ) userland/programa.ld
	@mkdir -p $(dir $@)
	$(LD) $(ULDFLAGS) -o $@.elf $(filter %.o,$^)
	objcopy --strip-all $@.elf $@
	@$(PY) $(CHECK_ELF32) $@.elf

APPS_BINS += $(B)/userland/bin/sdl2fps
