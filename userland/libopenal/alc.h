/* alc.h - el contexto de OpenAL 1.1 del SaguiOS (D230). Ver al.h. Hay un
 * dispositivo (el flujo de sonido del sistema; sin tarjeta de sonido, uno
 * mudo que mezcla igual para que el programa siga su curso) y un contexto. */
#ifndef SAGUI_ALC_H
#define SAGUI_ALC_H

#include "al.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
typedef char ALCboolean;
typedef char ALCchar;
typedef int ALCint;
typedef int ALCenum;
typedef int ALCsizei;
typedef unsigned int ALCuint;
typedef void ALCvoid;

#define ALC_FALSE 0
#define ALC_TRUE  1
#define ALC_FREQUENCY 0x1007
#define ALC_REFRESH   0x1008
#define ALC_SYNC      0x1009
#define ALC_MONO_SOURCES   0x1010
#define ALC_STEREO_SOURCES 0x1011
#define ALC_NO_ERROR        0
#define ALC_INVALID_DEVICE  0xA001
#define ALC_INVALID_CONTEXT 0xA002
#define ALC_INVALID_ENUM    0xA003
#define ALC_INVALID_VALUE   0xA004
#define ALC_OUT_OF_MEMORY   0xA005
#define ALC_DEFAULT_DEVICE_SPECIFIER 0x1004
#define ALC_DEVICE_SPECIFIER         0x1005
#define ALC_EXTENSIONS               0x1006
#define ALC_MAJOR_VERSION 0x1000
#define ALC_MINOR_VERSION 0x1001

ALCdevice *alcOpenDevice(const ALCchar *nome);
ALCboolean alcCloseDevice(ALCdevice *d);
ALCcontext *alcCreateContext(ALCdevice *d, const ALCint *atributos);
void alcDestroyContext(ALCcontext *c);
ALCboolean alcMakeContextCurrent(ALCcontext *c);
ALCcontext *alcGetCurrentContext(void);
ALCdevice *alcGetContextsDevice(ALCcontext *c);
void alcProcessContext(ALCcontext *c);
void alcSuspendContext(ALCcontext *c);
ALCenum alcGetError(ALCdevice *d);
const ALCchar *alcGetString(ALCdevice *d, ALCenum p);
void alcGetIntegerv(ALCdevice *d, ALCenum p, ALCsizei n, ALCint *v);
ALCboolean alcIsExtensionPresent(ALCdevice *d, const ALCchar *nome);

#ifdef __cplusplus
}
#endif

#endif
