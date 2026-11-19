// SPDX-License-Identifier: GPL-2.0-or-later
/* gpu_energia.h - energía de la GPU: relojes, estados, temperatura y VRAM (D399).
 *
 * La capa común (kernel/gui/gpu_energia.c) elige el driver por el PCI, lleva el modo (bios, bajo, alto,
 * auto, manual), el gobernador «auto», la comprobación después de cada cambio y la vuelta a los relojes
 * de la BIOS si algo falla (y entonces no se reintenta en ese arranque). Los drivers solo leen sus tablas
 * y cambian relojes cuando la capa se lo pide, con el motor parado y las interrupciones cerradas:
 *   energia_radeon.c   Radeon R5xx (X1300-X1950) por la BIOS ATOM (SetEngineClock/SetMemoryClock y la
 *                      tensión por GPIO, como el «profile»/«dynpm» de Linux);
 *   energia_sim.c      de mentira, con una BIOS ATOM de juguete y el intérprete de verdad (pruebas en QEMU).
 * Para R6xx/R7xx, Evergreen e Intel la capa ya sirve: falta el driver (las tablas PPLib, el SMC...).
 * Ver docs/decisoes.md, D399. */
#ifndef SAGUI_GPU_ENERGIA_H
#define SAGUI_GPU_ENERGIA_H
#include "tipos.h"
#include "../dispositivos/gpu_energia_logica.h"

/* Lo que el driver sabe de la tarjeta (lo rellena sondar(), sin ejecutar nada de la BIOS). */
typedef struct {
    uint16_t fabricante, modelo;
    gpu_en_firmware_t fw;
    gpu_en_tabela_t   tab;
    char     bios[32];
    char     motivo[120];
    char     temp_fuente[48];
    int      temp_mili_c;           /* GPU_EN_SIN_SENSOR si no hay */
    uint32_t vram_kb;
    bool     pode_ler;              /* tiene GetEngineClock y GetMemoryClock */
    bool     pode_mudar;            /* y SetEngineClock, SetMemoryClock y al menos dos estados */
    bool     confirmada;            /* modelo probado en una máquina real (el automático lo usa) */
} gpu_en_placa_t;

typedef struct {
    const char *nome;
    bool (*sondar)(gpu_en_placa_t *p);
    /* Relojes actuales por la BIOS, en 10 kHz. */
    bool (*ler)(uint32_t *sclk, uint32_t *mclk, char *m, int nm);
    /* Pone sclk/mclk (10 kHz) y la tensión del estado "e" (NULL: la del arranque), sabiendo los de ahora.
     * Con el motor parado y las interrupciones cerradas (lo hace la capa); comprueba con ler() al final.
     * ensaio: no escribe nada en la tarjeta (lo que haría la BIOS va al registro). */
    bool (*aplicar)(const gpu_en_estado_t *e, uint32_t sclk, uint32_t mclk, uint32_t sclk_atual, uint32_t mclk_atual,
                    bool ensaio, char *m, int nm);
    /* Espera a que el 2D y el 3D terminen lo que tienen (antes de cambiar). false: no paran. */
    bool (*parar_motor)(uint32_t ms, char *m, int nm);
    int  (*ecras)(void);            /* pantallas (controladores) encendidas */
    void (*detalle)(void);          /* registros de los PLL y lo leído al registro [GPUENERGIA] */
} gpu_en_driver_t;

extern const gpu_en_driver_t gpu_en_radeon;   /* energia_radeon.c */
extern const gpu_en_driver_t gpu_en_sim;      /* energia_sim.c */

void    gpu_energia_iniciar(void);            /* al arrancar, después de acel2d y gpu3d */
int32_t gpu_energia_info(uint32_t ptr, uint32_t n);   /* INFO_GPU_ENERGIA */
int32_t gpu_energia_ctl(uint32_t pedido);             /* INFO_GPU_ENERGIA_CTL */

/* Zonas térmicas del ACPI (zonas_termicas.c; las lee la tarea acpi-aml solo si alguien mira). */
void zonas_termicas_iniciar(void);
void zonas_termicas_ciclo(void);
int  zonas_termicas_ler(int *mili_c, char (*nome)[8], int max);   /* las últimas leídas */

/* De acel2d.c y gpu3d.c (D399). */
uint32_t acel2d_ordens_total(void);
bool     acel2d_volcado_em_uso(void);
int      acel2d_energia_autoprova(char *m, int nm);
uint32_t gpu3d_lotes_total(void);
void     gpu3d_memoria_uso(uint32_t *total_kb, uint32_t *livre_kb);
int      gpu3d_energia_comprobar(char *m, int nm);
int      gpu3d_r500_ocioso(uint32_t ms);

#endif
