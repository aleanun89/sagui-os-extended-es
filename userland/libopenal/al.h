/* al.h - la OpenAL 1.1 del SaguiOS (un subconjunto) (D230).
 *
 * Esta cabecera es nuestra: los nombres, los tipos y los valores de las
 * constantes son los de OpenAL 1.1 para que el código de los programas
 * compile sin cambios; detrás (openal_sagui.c) hay un mezclador de software
 * que suma las fuentes y las manda al flujo de sonido del sistema
 * (audio_abrir/audio_escrever de la libsagui). Lo que hay: fuentes con un
 * búfer o una cola de búferes (streaming), PCM de 8 y 16 bits, mono y
 * estéreo, a cualquier frecuencia (se remuestrea), ganancia por fuente y
 * del oyente, tono (AL_PITCH), bucle. Sin sonido 3D: la posición se guarda
 * y no se usa (todo suena centrado). Sin hilos: el mezclador avanza en cada
 * llamada que consulta el estado (alGetSource*) y en sagui_al_bombear(), que
 * llama la SDL 2 del SaguiOS en su bucle de eventos.
 */
#ifndef SAGUI_AL_H
#define SAGUI_AL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char ALboolean;
typedef char ALchar;
typedef signed char ALbyte;
typedef unsigned char ALubyte;
typedef short ALshort;
typedef unsigned short ALushort;
typedef int ALint;
typedef unsigned int ALuint;
typedef int ALsizei;
typedef int ALenum;
typedef float ALfloat;
typedef double ALdouble;
typedef void ALvoid;

#define AL_NONE  0
#define AL_FALSE 0
#define AL_TRUE  1

#define AL_SOURCE_RELATIVE    0x202
#define AL_CONE_INNER_ANGLE   0x1001
#define AL_CONE_OUTER_ANGLE   0x1002
#define AL_PITCH              0x1003
#define AL_POSITION           0x1004
#define AL_DIRECTION          0x1005
#define AL_VELOCITY           0x1006
#define AL_LOOPING            0x1007
#define AL_BUFFER             0x1009
#define AL_GAIN               0x100A
#define AL_MIN_GAIN           0x100D
#define AL_MAX_GAIN           0x100E
#define AL_ORIENTATION        0x100F
#define AL_SOURCE_STATE       0x1010
#define AL_INITIAL            0x1011
#define AL_PLAYING            0x1012
#define AL_PAUSED             0x1013
#define AL_STOPPED            0x1014
#define AL_BUFFERS_QUEUED     0x1015
#define AL_BUFFERS_PROCESSED  0x1016
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR     0x1021
#define AL_CONE_OUTER_GAIN    0x1022
#define AL_MAX_DISTANCE       0x1023
#define AL_SEC_OFFSET         0x1024
#define AL_SAMPLE_OFFSET      0x1025
#define AL_BYTE_OFFSET        0x1026
#define AL_SOURCE_TYPE        0x1027
#define AL_STATIC             0x1028
#define AL_STREAMING          0x1029
#define AL_UNDETERMINED       0x1030

#define AL_FORMAT_MONO8       0x1100
#define AL_FORMAT_MONO16      0x1101
#define AL_FORMAT_STEREO8     0x1102
#define AL_FORMAT_STEREO16    0x1103

#define AL_FREQUENCY          0x2001
#define AL_BITS               0x2002
#define AL_CHANNELS           0x2003
#define AL_SIZE               0x2004

#define AL_NO_ERROR           0
#define AL_INVALID_NAME       0xA001
#define AL_INVALID_ENUM       0xA002
#define AL_INVALID_VALUE      0xA003
#define AL_INVALID_OPERATION  0xA004
#define AL_OUT_OF_MEMORY      0xA005

#define AL_VENDOR             0xB001
#define AL_VERSION            0xB002
#define AL_RENDERER           0xB003
#define AL_EXTENSIONS         0xB004

#define AL_DOPPLER_FACTOR     0xC000
#define AL_SPEED_OF_SOUND     0xC003
#define AL_DISTANCE_MODEL     0xD000
#define AL_INVERSE_DISTANCE_CLAMPED 0xD002

ALenum alGetError(void);
const ALchar *alGetString(ALenum p);
ALboolean alIsExtensionPresent(const ALchar *nome);
void alDistanceModel(ALenum modelo);

void alGenSources(ALsizei n, ALuint *ids);
void alDeleteSources(ALsizei n, const ALuint *ids);
ALboolean alIsSource(ALuint id);
void alSourcef(ALuint id, ALenum p, ALfloat v);
void alSource3f(ALuint id, ALenum p, ALfloat a, ALfloat b, ALfloat c);
void alSourcefv(ALuint id, ALenum p, const ALfloat *v);
void alSourcei(ALuint id, ALenum p, ALint v);
void alSource3i(ALuint id, ALenum p, ALint a, ALint b, ALint c);
void alGetSourcef(ALuint id, ALenum p, ALfloat *v);
void alGetSourcei(ALuint id, ALenum p, ALint *v);
void alSourcePlay(ALuint id);
void alSourceStop(ALuint id);
void alSourcePause(ALuint id);
void alSourceRewind(ALuint id);
void alSourcePlayv(ALsizei n, const ALuint *ids);
void alSourceStopv(ALsizei n, const ALuint *ids);
void alSourcePausev(ALsizei n, const ALuint *ids);
void alSourceQueueBuffers(ALuint id, ALsizei n, const ALuint *buffers);
void alSourceUnqueueBuffers(ALuint id, ALsizei n, ALuint *buffers);

void alGenBuffers(ALsizei n, ALuint *ids);
void alDeleteBuffers(ALsizei n, const ALuint *ids);
ALboolean alIsBuffer(ALuint id);
void alBufferData(ALuint id, ALenum formato, const ALvoid *dados, ALsizei tamanho, ALsizei freq);
void alGetBufferi(ALuint id, ALenum p, ALint *v);

void alListenerf(ALenum p, ALfloat v);
void alListener3f(ALenum p, ALfloat a, ALfloat b, ALfloat c);
void alListenerfv(ALenum p, const ALfloat *v);
void alGetListenerf(ALenum p, ALfloat *v);

/* Del SaguiOS: hace avanzar el mezclador (lo llama la SDL 2 del sistema en
 * SDL_PollEvent, SDL_Delay y SDL_RenderPresent; un programa sin SDL puede
 * llamarlo en su bucle). */
void sagui_al_bombear(void);

#ifdef __cplusplus
}
#endif

#endif
