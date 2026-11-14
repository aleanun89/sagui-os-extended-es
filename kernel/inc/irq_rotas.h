// SPDX-License-Identifier: GPL-2.0-or-later
/* irq_rotas.h - qué camino sigue cada IRQ de los dispositivos PCI y la
 * autoprueba que lo comprueba (D211).
 *
 * Con el IOAPIC (D166) cada línea PCI llegaba por la entrada ISA de su IRQ,
 * adivinada a partir de la línea que dejó la BIOS. En muchos chipsets eso
 * no funciona (el Amilo Pi 1536: el EHCI en la IRQ 5 no recibió ninguna).
 * Aquí:
 *   - en el arranque, cada driver que sabe provocar una interrupción de su
 *     dispositivo (una "sonda") la provoca y se espera, con plazo, a que
 *     llegue; si no llega por el IOAPIC, esa línea entra por el 8259 (modo
 *     mixto, ioapic_via_pic) y se vuelve a probar;
 *   - con uACPI (--uacpi), en la tarea "acpi-aml", se lee el _PRT (prt.c)
 *     después de \_PIC(1): cada función PCI recibe su GSI real (16-23 en los
 *     Intel), se prueba otra vez y se queda lo que funcione.
 * Todo con registro [IOAPIC] y [PRT] (dispositivo -> pin -> GSI -> vector
 * -> CPU) y la tabla para infopc (INFO_IRQ_VIAS). */
#ifndef SAGUI_IRQ_ROTAS_H
#define SAGUI_IRQ_ROTAS_H
#include "tipos.h"
#include "pci.h"

/* Lo que un driver ofrece para la autoprueba. */
typedef struct {
    const char *nome;                    /* "EHCI", "e1000", "HDA" */
    pci_disp_t  pci;
    uint8_t     irq;                     /* la línea en la que el driver atiende */
    bool      (*provocar)(void *ctx);    /* pide una interrupción; false = ahora no se puede */
    uint32_t  (*contador)(void *ctx);    /* interrupciones del dispositivo atendidas por su driver */
    void      (*terminar)(void *ctx, bool chegou);   /* deja el dispositivo como estaba */
    void       *ctx;
} irq_sonda_t;

void irq_sonda_registrar(const irq_sonda_t *s);

/* En el arranque, con los drivers ya iniciados (principal.c). Con el IOAPIC
 * prueba cada sonda; sin él solo anota la tabla. */
void irq_rotas_arranque(void);

/* Desde la tarea "acpi-aml", con el espacio de nombres ya inicializado: el
 * _PRT y su autoprueba. Sin IOAPIC no hace nada. */
void irq_rotas_prt(void);

int32_t irq_rotas_info(uint32_t ptr, uint32_t n);   /* INFO_IRQ_VIAS */

/* CAB_IRQ_PRUEBA (cabecera, byte 140; mkimagem.py --irq-prueba): fallos
 * simulados para las pruebas. */
#define IRQ_PRUEBA_PRT_ERRADA 1   /* el _PRT da a los dispositivos con sonda una GSI sin nada conectado */
#define IRQ_PRUEBA_TORMENTA   4   /* D244: simula una tormenta en la IRQ 11 a los 2 s (se enmascara y se reintenta) */
#define IRQ_PRUEBA_ISA_ERRADA 2   /* la entrada ISA de sus líneas no entrega (como en el Amilo) */

#endif
