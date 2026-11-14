// SPDX-License-Identifier: GPL-3.0-only
/* relogio.h - tiempo desde el arranque (PIT) y fecha/hora (CMOS). */
#ifndef SAGUI_RELOGIO_H
#define SAGUI_RELOGIO_H
#include "tipos.h"
#include "cpu.h"   /* quadro_t */

#define TIQUES_POR_SEGUNDO 100

void     relogio_iniciar(void);
uint32_t relogio_tiques(void);
uint32_t relogio_segundos(void);
void     relogio_dormir_ms(uint32_t ms);
quadro_t *relogio_tic_cpu(quadro_t *q);   /* o tic de um processador secundario (D157) */

typedef struct {
    uint16_t ano;
    uint8_t  mes, dia, hora, minuto, segundo;
} data_hora_t;

void cmos_ler_data(data_hora_t *d);
void cmos_gravar_data(const data_hora_t *d);     /* D080: en el formato (BCD/binario, 12/24 h) que ya usa el RTC */
int  dia_da_semana(int ano, int mes, int dia);   /* 0 = domingo */

/* Hora del sistema (D080, kernel/nucleo/hora.c): el kernel guarda UTC; el RTC
 * puede estar en UTC o en hora local; la zona es una regla POSIX TZ. */
#include "fuso.h"
void     hora_iniciar(void);                /* lee el RTC (antes de haber /etc/zona: zona UTC) */
void     hora_carregar_config(void);        /* después del fstab: /home/.zona o /etc/zona */
uint32_t hora_utc(void);                    /* segundos desde 1970 */
void     hora_local(fuso_data_t *d);        /* ahora, en la zona del sistema */
void     hora_local_de(uint32_t utc, fuso_data_t *d);
uint32_t hora_local_para_utc(uint32_t local);   /* FAT32 guarda la hora local */
bool     hora_24h(void);
uint32_t hora_ajustes(void);                /* cambia con cada cambio de zona o de hora */

#endif
