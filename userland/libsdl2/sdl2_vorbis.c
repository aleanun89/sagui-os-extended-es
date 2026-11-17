/* sdl2_vorbis.c - el decodificador Ogg Vorbis stb_vorbis (dominio público o MIT, Sean Barrett) para
 * la SDL2_mixer del SaguiOS (D374). El código es el del upstream, sin tocar
 * (userland/terceros/stbvorbis/, SHA-256 en su VERSAO); aquí solo se elige cómo compilarlo: sin stdio
 * (se lee de memoria), sin la API de empuje. */
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../terceros/stbvorbis/stb_vorbis.c"
