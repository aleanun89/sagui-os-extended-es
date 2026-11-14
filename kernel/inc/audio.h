// SPDX-License-Identifier: GPL-2.0-or-later
/* audio.h - tarjetas de sonido (AC'97 e Intel HDA) y el mezclador del kernel (D088-D090). */
#ifndef SAGUI_AUDIO_H
#define SAGUI_AUDIO_H
#include "tipos.h"
#include "abi.h"
#include "pci.h"

/* Lo que el mezclador necesita de un driver. La tarjeta reproduce sin parar un anillo
 * de cuadros (16 bits, estéreo) en la memoria; el mezclador escribe delante
 * de la posición que la tarjeta está leyendo. */
typedef struct {
    const char *driver;           /* "ac97" o "hda" */
    char        placa[48];
    char        codec[64];
    pci_disp_t  pci;
    int         irq;              /* línea del PIC (0 = sin interrupción) */
    uint32_t    taxa;             /* Hz de la salida */
    uint32_t    quadros;          /* tamaño del anillo, en cuadros */
    int16_t    *anel;             /* el anillo visto por la CPU (alias sin caché) */
    void      (*ligar)(void);     /* empieza a reproducir desde el inicio del anillo */
    void      (*parar)(void);
    uint32_t  (*posicao)(void);   /* cuadro del anillo que la tarjeta está reproduciendo */
    bool      (*atender)(void);   /* en la IRQ: ¿la causa era de la tarjeta? (y la limpia) */
    /* D211: autoprueba de la IRQ: pide una interrupción y, después, deja la
     * tarjeta como estaba. NULL = no sabe (AC'97). */
    bool      (*provocar_irq)(void);
    void      (*fim_prova_irq)(void);
} placa_som_t;

/* Ayudas para los drivers (audio.c): memoria para DMA por debajo de 4 GB,
 * contigua, vista por la CPU sin caché. Devuelve la dirección física y pone en
 * *cpu el alias. */
uint32_t audio_dma_alocar(uint32_t paginas, void **cpu);
void     audio_esperar_us(uint32_t us);

bool ac97_procurar(placa_som_t *p);
bool hda_procurar(placa_som_t *p);
int32_t hda_chamada(uint32_t op, uint32_t a, uint32_t b);   /* SIS_AUDIO_HDA (D107) */

void    audio_iniciar(void);
void    audio_tique(void);                 /* del reloj: el mezclador también funciona por sondeo */
void    audio_linha_boot(char *s, int max, bool *aviso);
int32_t audio_info(uint32_t ptr, uint32_t n);
int     audio_mestre(int *vol, int *mudo);          /* -> AUDIO_SEM_PLACA/FUNCIONANDO/SEM_DRIVER (bandeja, D114) */
void    audio_mestre_definir(int vol, int mudo);    /* < 0 = no cambia */

/* Flujos (los descriptores de audio, ver kernel/fs/arquivos.c). */
struct fluxo;
int32_t audio_fluxo_escrever(struct fluxo *f, const uint8_t *buf, uint32_t n, bool sem_bloquear);
void    audio_fluxo_fechar(struct fluxo *f);

int32_t sis_audio_abrir(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_audio_escrever(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_audio_controle(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_audio_fechar(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
int32_t sis_audio_hda(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* kernel/fs/arquivos.c */
int          arquivos_novo_audio(struct fluxo *f);   /* -> fd */
struct fluxo *arquivos_fluxo(uint32_t fd);           /* NULL = no es un flujo de audio */

#endif
