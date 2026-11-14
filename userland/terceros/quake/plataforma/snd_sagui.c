/* snd_sagui.c - el sonido de Quake en el SaguiOS (D229).
 *
 * Quake mezcla en un búfer circular que imagina como el de la DMA de una
 * Sound Blaster: pregunta por dónde va leyendo la tarjeta
 * (SNDDMA_GetDMAPos) y escribe por delante. Aquí la «tarjeta» es el
 * callback de audio de la SDL común (userland/libsdl12, D228): cada vez que
 * el flujo de sonido del sistema tiene sitio, copia el trozo siguiente del
 * búfer circular y avanza la posición. Es el modelo del snd_sdl.c de
 * «sdlquake», escrito de nuevo para el SaguiOS.
 *
 * 22050 Hz, estéreo, 16 bits (los sonidos de Quake son de 11025 Hz: el motor
 * los pasa a la frecuencia de salida al cargarlos). «-sndspeed 11025» y
 * «-sndmono» lo cambian; «-nosound» lo apaga (lo mira el motor).
 */
#include "quakedef.h"
#include "arquivos_quake.h"
#include "SDL.h"

#define AMOSTRAS_ANEL 16384   /* muestras (de un canal) del búfer circular: potencia de 2 */

static unsigned char *anel = NULL;
static int bytes_anel = 0;

/* El callback de la SDL: copia «len» bytes del búfer circular y avanza. */
static void encher(void *dado, Uint8 *saida, int len)
{
    (void)dado;
    if (!shm || !anel) return;
    int por_amostra = shm->samplebits / 8;
    int pos = (shm->samplepos * por_amostra) % bytes_anel;
    int resta = len;
    while (resta > 0) {
        int n = bytes_anel - pos;
        if (n > resta) n = resta;
        memcpy(saida, anel + pos, (size_t)n);
        saida += n;
        resta -= n;
        pos = (pos + n) % bytes_anel;
    }
    shm->samplepos = (shm->samplepos + len / por_amostra) % shm->samples;
}

qboolean SNDDMA_Init(void)
{
    SDL_AudioSpec pedido, obtido;
    int i;

    memset(&pedido, 0, sizeof pedido);
    pedido.freq = 22050;
    if ((i = COM_CheckParm("-sndspeed")) && i + 1 < com_argc) pedido.freq = Q_atoi(com_argv[i + 1]);
    if (pedido.freq != 11025 && pedido.freq != 22050 && pedido.freq != 44100) pedido.freq = 22050;
    pedido.format = AUDIO_S16SYS;
    pedido.channels = COM_CheckParm("-sndmono") ? 1 : 2;
    pedido.samples = pedido.freq <= 11025 ? 256 : 512;   /* unos 23 ms por bloque */
    pedido.callback = encher;
    if (SDL_OpenAudio(&pedido, &obtido) < 0) {
        Con_Printf("SNDDMA_Init: %s\n", SDL_GetError());
        return false;
    }

    shm = &sn;
    shm->splitbuffer = 0;
    shm->samplebits = (obtido.format & 0xFF) == 8 ? 8 : 16;
    shm->speed = obtido.freq;
    shm->channels = obtido.channels;
    shm->samples = AMOSTRAS_ANEL;
    shm->submission_chunk = 1;
    shm->samplepos = 0;
    bytes_anel = AMOSTRAS_ANEL * (shm->samplebits / 8);
    anel = calloc(1, (size_t)bytes_anel);
    if (!anel) {
        SDL_CloseAudio();
        shm = NULL;
        return false;
    }
    if (shm->samplebits == 8) memset(anel, 0x80, (size_t)bytes_anel);
    shm->buffer = anel;
    registrar("[QUAKE] sonido %d Hz, %d canales, %d bits", shm->speed, shm->channels, shm->samplebits);
    SDL_PauseAudio(0);
    return true;
}

int SNDDMA_GetDMAPos(void)
{
    return shm ? shm->samplepos : 0;
}

void SNDDMA_Shutdown(void)
{
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    free(anel);
    anel = NULL;
    shm = NULL;
}

void SNDDMA_Submit(void)
{
    /* El callback lee directamente del búfer circular. */
}
