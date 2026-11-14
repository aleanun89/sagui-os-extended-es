// SPDX-License-Identifier: GPL-2.0-or-later
/* prt.h - el enrutado de las interrupciones PCI del firmware (_PRT) con
 * uACPI (D211).
 *
 * El _PRT de un puente PCI (el raíz, PNP0A03/PNP0A08, o uno PCI-PCI) dice a
 * qué entrada de interrupción llega cada pin INTA-INTD de cada ranura de su
 * bus. Cada fila es (dirección de la ranura, pin, fuente, índice):
 *   - fuente 0: el índice es directamente la GSI (el pin del IOAPIC), por
 *     nivel y activa baja (lo normal en el modo APIC de los Intel);
 *   - fuente = un "Link Device" (LNKA... o PIRQ en el modo PIC): la IRQ es la
 *     que dé su _CRS (con su polaridad y disparo); si el enlace está apagado
 *     se elige una de su _PRS y se enciende con _SRS.
 * Un dispositivo detrás de un puente sin _PRT propio usa el de arriba con el
 * pin "girado" (swizzle de la especificación PCI-PCI): (pin + ranura) % 4.
 *
 * Este módulo solo evalúa (no toca el IOAPIC ni el PCI): el que llama le da
 * la lista de funciones PCI y recibe la GSI de cada una. Lo usan el kernel
 * (irq_rotas.c, tarea "acpi-aml") y, en el PC, tools/uacpi_volcado.c sobre
 * volcados de máquinas reales. Antes hay que haber llamado a \_PIC (con
 * uacpi_set_interrupt_model) para que el firmware dé la tabla del modo
 * elegido. */
#ifndef SAGUI_PRT_H
#define SAGUI_PRT_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

typedef struct {
    /* Entrada: la función PCI. */
    uint8_t  bus, disp, fun;
    uint8_t  pino;          /* 1-4 = INTA-INTD (registro 0x3D); 0 = no interrumpe */
    uint8_t  linha;         /* la línea que dejó la BIOS (0x3C), para elegir en _PRS */
    uint8_t  secundario;    /* puente PCI-PCI: su bus secundario; 0 = no es puente */
    /* Salida. */
    bool     achado;        /* hay GSI */
    bool     nivel, baixo;  /* disparo por nivel; activa a nivel bajo */
    bool     srs;           /* el enlace estaba apagado y se encendió con _SRS */
    uint8_t  pino_tabela;   /* el pin (1-4) en la fila usada (tras el swizzle) */
    uint8_t  disp_tabela;   /* la ranura de la fila usada */
    uint32_t gsi;
    char     tabela[32];    /* el dispositivo ACPI cuyo _PRT se usó ("\_SB_.PCI0") */
    char     ligacao[32];   /* el Link Device ("\_SB_.LNKA"); vacío si es directo */
    const char *erro;       /* por qué no hay GSI */
} prt_funcao_t;

/* Resuelve las n funciones. Devuelve cuántas tienen GSI, o -1 si no hay
 * ningún puente raíz PCI en el espacio de nombres. "permitir_srs": se puede
 * encender un enlace apagado (cambia el enrutado del chipset). */
int prt_resolver(prt_funcao_t *f, int n, bool permitir_srs);

/* Cuántos _PRT se leyeron en la última llamada (para el registro). */
int prt_tabelas_lidas(void);

#endif
