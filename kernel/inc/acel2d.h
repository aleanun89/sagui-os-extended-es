// SPDX-License-Identifier: GPL-2.0-or-later
/* acel2d.h - 2D por la GPU para el escritorio: copiar y rellenar rectángulos (D195).
 *
 * La capa común (kernel/gui/acel2d.c) elige al arrancar el acelerador que
 * corresponde a la tarjeta (tabla por PCI), lo pone a prueba contra la CPU y
 * solo entonces lo usa. Sin acelerador, o si falla en cualquier momento, todo
 * lo hace la CPU como siempre (video.c). Los aceleradores están en
 * kernel/dispositivos/acel_*.c. */
#ifndef SAGUI_ACEL2D_H
#define SAGUI_ACEL2D_H
#include "tipos.h"
#include "pci.h"

/* Lo que pide la cabecera de arranque (CAB_ACEL2D, byte 135; mkimagem.py). */
#define ACEL2D_MODO_MASCARA 0x07
#define ACEL2D_AUTO         0     /* elección automática por el PCI (lo normal) */
#define ACEL2D_CPU          1     /* --sin-acel2d o --acel2d=cpu: siempre la CPU */
#define ACEL2D_VMWARE       2     /* --acel2d=vmware: solo ese acelerador */
#define ACEL2D_INTEL        3     /* --acel2d=intel */
#define ACEL2D_RADEON       4     /* --acel2d=radeon (R100-R500 y, D223, R6xx/R7xx) */
#define ACEL2D_NVIDIA       6     /* --acel2d=nvidia (GeForce 6/7, D224) */
#define ACEL2D_PROVA_PIXEL  0x10  /* pruebas: la autoprueba del arranque ve un píxel distinto */
#define ACEL2D_PROVA_TEMPO  0x20  /* pruebas: la autoprueba del arranque no termina a tiempo */

/* Filas de la zona de prueba de la capa común, debajo de la pantalla visible
 * cuando el VBE anuncia memoria para ellas: un driver que necesite memoria de
 * vídeo propia (el anillo de la R6xx/R7xx, D223) la pone detrás. */
#define ACEL2D_ZONA_A       64

/* Pantalla y memoria de vídeo que ve el acelerador (la rellena acel2d.c). */
typedef struct {
    uint32_t fb_fisico;      /* framebuffer lineal del VBE (dirección física) */
    uint32_t pitch;          /* bytes por línea */
    int      larg, alt;      /* píxeles visibles */
    uint32_t memoria;        /* bytes de memoria de vídeo que anunció el VBE */
} acel_tela_t;

/* Un acelerador. Las coordenadas son píxeles del framebuffer, con el mismo pitch
 * que la pantalla; «y» puede pasar de la altura visible (zona de prueba).
 * copiar() tiene que dar el resultado de memmove con solapes en cualquier
 * dirección. Todo devuelve false si no pudo (la capa vuelve a la CPU). */
typedef struct {
    const char *nome;                 /* "vmware", "intel", "radeon", "r600dma" */
    int         modo;                 /* ACEL2D_VMWARE... (para forzarlo) */
    bool (*iniciar)(const pci_disp_t *d, int geracao, const acel_tela_t *t, char *motivo, int n);
    bool (*copiar)(int ox, int oy, int dx, int dy, int l, int a);
    bool (*preencher)(int x, int y, int l, int a, uint32_t cor);
    bool (*esperar)(void);            /* false: tiempo agotado */
    bool (*atualizar)(int x, int y, int l, int a);   /* NULL: la pantalla lee la memoria sola; false: no responde */
    void (*desligar)(void);           /* deja de usar el motor (y la pantalla como la dejó el VBE) */
    bool fora_da_tela;                /* la zona de prueba puede ir debajo de la pantalla visible
                                         (VMware en QEMU: no, solo acepta rectángulos visibles) */
    /* D246 (opcionales; a 0 / NULL en los demás aceleradores): */
    uint32_t    min_pixeis;           /* copias de menos píxeles las hace la CPU (sin apagar el acelerador) */
    bool        assincrono;           /* el motor sigue trabajando tras copiar(): hay que esperar antes de que
                                         la CPU toque la pantalla (acel_sincronizar) */
    void (*detalle)(const char *fase);   /* vuelca al log [ACEL2D] los registros clave («acel2d probar --detalle») */
} acel_driver_t;

/* Un plazo con el TSC (y, sin TSC medido, con un número de vueltas). */
typedef struct { uint64_t fim; uint32_t voltas, max_voltas; } acel_prazo_t;
void acel_prazo(acel_prazo_t *p, uint32_t ms);
void acel_prazo_longo(acel_prazo_t *p, uint32_t ms);   /* D285: sin el recorte de la sesión */
bool acel_prazo_esgotado(acel_prazo_t *p);

/* Capa común */
void        acel2d_iniciar(void);            /* tras video_iniciar: detecta, prueba y activa */
bool        acel2d_ativo(void);
/* D285: tarjeta de la pantalla y su acelerador, aunque el 2D esté en la CPU (false: ninguna de la tabla). */
bool        acel2d_placa(pci_disp_t *d, int *geracao, const acel_driver_t **driver, acel_tela_t *t, bool *suspeito);
const char *acel2d_nome(void);               /* "vmware", "intel", "radeon" o "cpu" */
const char *acel2d_motivo(void);             /* por qué la CPU, o qué tarjeta se usa */
bool        acel2d_forcado(void);            /* se pidió un acelerador concreto (--acel2d=...) */
void        acel2d_desligar(const char *motivo);   /* vuelve a la CPU para siempre */
int32_t     acel2d_apagar(void);             /* D244: «acel2d apagar» */
int32_t     acel2d_acender(void);            /* D244: «acel2d encender»: repite la autoprueba */

/* Sobre la pantalla visible. false = no se hizo: la CPU tiene que hacerlo
 * (quien llama marca la zona para video_mostrar). */
bool acel_copiar(int ox, int oy, int dx, int dy, int l, int a);
bool acel_rellenar(int x, int y, int l, int a, uint32_t cor);
bool acel_esperar(void);                     /* antes de que la CPU toque lo que la GPU escribe */
void acel_sincronizar(void);                 /* D246: espera solo si el motor es asíncrono y hay algo pendiente */
void acel_atualizar(int x, int y, int l, int a);   /* la CPU escribió en la pantalla */

/* Medidas (INFO_ACEL2D) y la orden de prueba (acel2d probar, INFO_ACEL2D_PROVA). */
void    acel2d_contar_arrasto(uint32_t quadros, uint64_t ciclos);
int32_t acel2d_info(uint32_t ptr, uint32_t n);
int32_t acel2d_prova(uint32_t ptr, uint32_t n);

/* Los aceleradores (kernel/dispositivos/acel_*.c). */
extern const acel_driver_t acel_vmware;
extern const acel_driver_t acel_intel;
extern const acel_driver_t acel_radeon;
extern const acel_driver_t acel_r600;      /* R6xx/R7xx por el motor DMA (D223) */
extern const acel_driver_t acel_nv;

#endif
