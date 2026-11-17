# sdl2_capas.mk - las capas SDL2_image y SDL2_ttf del SaguiOS (D374), reutilizables por otros ports.
# Lo incluye quien las necesite (hoy Freeciv, userland/terceros/freeciv/freeciv.mk): añade al enlace
# $(SDL2CAPAS_OBJ) y $(FONTES_OBJ) (SDL2_ttf va sobre la libfontes, que usa la x87) y compila con
# -Iuserland/libsdl2 (SDL_image.h, SDL_ttf.h, SDL2/SDL_image.h, SDL2/SDL_ttf.h). Necesitan la libc
# común (stdio, malloc), la SDL 2 (userland/libsdl2/sdl2_sagui.c) y la libsagui (imagem, compressao).
ifndef SDL2CAPAS_OBJ
SDL2CAPAS_FLAGS := -m32 -march=i686 -std=gnu11 -Os \
  -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
  -fno-stack-protector -fno-asynchronous-unwind-tables \
  -ffunction-sections -fdata-sections -DNDEBUG \
  -mfpmath=387 -fexcess-precision=standard \
  -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
  -Iuserland/libsdl2 -I$(LIBC_DIR) -Iuserland/inc -idirafter kernel/inc \
  -finput-charset=UTF-8 -fexec-charset=ISO-8859-1 -Wall -Wextra -Wno-unused-parameter
SDL2CAPAS_OBJ := $(B)/userland/libsdl2/sdl2_image.o $(B)/userland/libsdl2/sdl2_ttf.o $(B)/userland/libsdl2/sdl2_rwops.o \
  $(B)/userland/libsdl2/sdl2_mixer.o $(B)/userland/libsdl2/sdl2_vorbis.o
$(B)/userland/libsdl2/sdl2_image.o: userland/libsdl2/sdl2_image.c userland/libsdl2/SDL_image.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -c -o $@ $<
$(B)/userland/libsdl2/sdl2_ttf.o: userland/libsdl2/sdl2_ttf.c userland/libsdl2/SDL_ttf.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -c -o $@ $<
$(B)/userland/libsdl2/sdl2_rwops.o: userland/libsdl2/sdl2_rwops.c userland/libsdl2/SDL_rwops.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -c -o $@ $<
$(B)/userland/libsdl2/sdl2_mixer.o: userland/libsdl2/sdl2_mixer.c userland/libsdl2/SDL_mixer.h userland/libsdl2/SDL.h userland/terceros/stbvorbis/stb_vorbis.c $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -Iuserland/libsdl2 -c -o $@ $<
# El decodificador de Ogg Vorbis (stb_vorbis, dominio público o MIT): con la x87 y sin avisos del upstream.
$(B)/userland/libsdl2/sdl2_vorbis.o: userland/libsdl2/sdl2_vorbis.c userland/terceros/stbvorbis/stb_vorbis.c $(LIBC_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -w -c -o $@ $<
ifndef SDL2_OBJ
SDL2_OBJ := $(B)/userland/libsdl2/sdl2_sagui.o
$(SDL2_OBJ): userland/libsdl2/sdl2_sagui.c userland/libsdl2/sdl2gpu.h userland/libsdl2/SDL.h $(LIBC_HDR) $(USER_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(SDL2CAPAS_FLAGS) -c -o $@ $<
endif
endif
