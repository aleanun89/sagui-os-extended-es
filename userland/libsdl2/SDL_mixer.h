/* SDL_mixer.h - SDL2_mixer del SaguiOS (D374): mezcla por software sobre la SDL 2 del sistema.
 * Canales de efectos (Mix_Chunk) y una música (Mix_Music). Formatos: WAV (PCM 8/16/24/32 bits),
 * Ogg Vorbis (stb_vorbis, dominio público/MIT) y, para la música, también MP3, FLAC y MIDI por los
 * decodificadores de la libsagui. Implementación: sdl2_mixer.c. Sin hilos: mezcla en la llamada de
 * vuelta de audio, que la SDL del sistema atiende desde SDL_PollEvent, SDL_Delay y SDL_RenderPresent. */
#ifndef SDL_MIXER_H_SAGUI
#define SDL_MIXER_H_SAGUI
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MIX_MAX_VOLUME 128
#define MIX_DEFAULT_FREQUENCY 44100
#define MIX_DEFAULT_FORMAT AUDIO_S16SYS
#define MIX_DEFAULT_CHANNELS 2
#define MIX_CHANNELS 8
#define MIX_CHANNEL_POST (-2)
typedef enum { MIX_INIT_FLAC = 1, MIX_INIT_MOD = 2, MIX_INIT_MP3 = 8, MIX_INIT_OGG = 16, MIX_INIT_MID = 32 } MIX_InitFlags;
typedef struct Mix_Chunk { int allocated; Uint8 *abuf; Uint32 alen; Uint8 volume; } Mix_Chunk;
typedef enum { MIX_NO_FADING, MIX_FADING_OUT, MIX_FADING_IN } Mix_Fading;
typedef enum { MUS_NONE, MUS_CMD, MUS_WAV, MUS_MOD, MUS_MID, MUS_OGG, MUS_MP3, MUS_MP3_MAD_UNUSED, MUS_FLAC } Mix_MusicType;
typedef struct _Mix_Music Mix_Music;

int  Mix_Init(int flags);
void Mix_Quit(void);
int  Mix_OpenAudio(int frecuencia, Uint16 formato, int canales, int tam_bloque);
void Mix_CloseAudio(void);
int  Mix_QuerySpec(int *frecuencia, Uint16 *formato, int *canales);
int  Mix_AllocateChannels(int n);
#define Mix_GetError SDL_GetError
#define Mix_SetError SDL_SetError

Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int liberar);
#define Mix_LoadWAV(f) Mix_LoadWAV_RW(SDL_RWFromFile((f), "rb"), 1)
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *mem, Uint32 n);
void Mix_FreeChunk(Mix_Chunk *c);
Mix_Music *Mix_LoadMUS(const char *ruta);
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *src, int liberar);
void Mix_FreeMusic(Mix_Music *m);
Mix_MusicType Mix_GetMusicType(const Mix_Music *m);

int  Mix_PlayChannelTimed(int canal, Mix_Chunk *c, int veces, int ms);
#define Mix_PlayChannel(canal, c, veces) Mix_PlayChannelTimed((canal), (c), (veces), -1)
int  Mix_FadeInChannelTimed(int canal, Mix_Chunk *c, int veces, int ms_fundido, int ms);
#define Mix_FadeInChannel(canal, c, veces, ms) Mix_FadeInChannelTimed((canal), (c), (veces), (ms), -1)
int  Mix_Volume(int canal, int volumen);
int  Mix_VolumeChunk(Mix_Chunk *c, int volumen);
int  Mix_HaltChannel(int canal);
int  Mix_ExpireChannel(int canal, int ms);
int  Mix_FadeOutChannel(int canal, int ms);
void Mix_Pause(int canal);
void Mix_Resume(int canal);
int  Mix_Paused(int canal);
int  Mix_Playing(int canal);
void Mix_ChannelFinished(void (*fin)(int canal));
Mix_Chunk *Mix_GetChunk(int canal);
int  Mix_ReserveChannels(int n);
/* Añadidos para Chocolate Doom (D383) */
typedef void (*Mix_EffectFunc_t)(int canal, void *flujo, int len, void *datos);
typedef void (*Mix_EffectDone_t)(int canal, void *datos);
int  Mix_OpenAudioDevice(int frecuencia, Uint16 formato, int canales, int tam_bloque, const char *dispositivo, int cambios);
int  Mix_SetPanning(int canal, Uint8 izq, Uint8 der);
void Mix_HookMusic(void (*f)(void *datos, Uint8 *flujo, int len), void *datos);
int  Mix_RegisterEffect(int canal, Mix_EffectFunc_t f, Mix_EffectDone_t fin, void *datos);
int  Mix_SetMusicCMD(const char *cmd);
int  Mix_SetMusicPosition(double segundos);

int  Mix_PlayMusic(Mix_Music *m, int veces);
int  Mix_FadeInMusic(Mix_Music *m, int veces, int ms);
int  Mix_VolumeMusic(int volumen);
int  Mix_HaltMusic(void);
int  Mix_FadeOutMusic(int ms);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);
void Mix_RewindMusic(void);
int  Mix_PausedMusic(void);
int  Mix_PlayingMusic(void);
Mix_Fading Mix_FadingMusic(void);
void Mix_HookMusicFinished(void (*fin)(void));
#ifdef __cplusplus
}
#endif
#endif
