// SPDX-License-Identifier: GPL-2.0-or-later
/* ioapic.h - el IOAPIC: el controlador de interrupciones de los dispositivos
 * que sustituye al par de 8259 (D166).
 *
 * El IOAPIC tiene una "tabla de redirección" con una entrada por línea de
 * entrada (pin). Cada entrada dice a qué vector, a qué CPU, con qué
 * polaridad y con qué disparo (flanco o nivel) se entrega la interrupción, y
 * si está enmascarada. Las líneas se numeran globalmente (GSI): el primer
 * IOAPIC empieza normalmente en la 0.
 *
 * Las IRQ ISA (0-15) van con identidad a los GSI 0-15, salvo lo que diga la
 * MADT del ACPI con las "Interrupt Source Override" (en casi todos los PC,
 * la IRQ 0 del PIT llega por el GSI 2).
 *
 * Es opcional (mkimagem.py --ioapic, OPC_BOOT_IOAPIC): sin la opción, o si
 * no hay MADT o IOAPIC, el sistema se queda con el PIC de siempre. Los
 * vectores siguen siendo 0x20-0x2F para las IRQ 0-15, así que los drivers
 * no notan nada: irq_registrar(), irq_compartilhar() e irq_registrar_extra()
 * funcionan igual. */
#ifndef SAGUI_IOAPIC_H
#define SAGUI_IOAPIC_H
#include "tipos.h"

/* Dirección virtual del primer IOAPIC: justo después del APIC local, en la
 * misma tabla de páginas del final del espacio (0xFFC00000, ver lapic.h).
 * Cada IOAPIC tiene su página: 0xFFC01000, 0xFFC02000... */
#define IOAPIC_VIRT 0xFFC01000u
#define IOAPIC_MAX  4

/* Lee de la MADT los IOAPIC (entrada de tipo 1), las "Interrupt Source
 * Override" (tipo 2) y las fuentes NMI (tipos 3 y 4); de la FADT, la IRQ de
 * la SCI; y de la tabla MP, si hace falta el IMCR. Antes de la paginación
 * (como cpu_lapic_capturar): las tablas pueden estar fuera del mapa 1:1. */
void ioapic_capturar(void);

/* Cuántos IOAPIC declaró el firmware y la dirección física de cada uno (la
 * usa la paginación para mapearlos). */
uint32_t ioapic_quantidade(void);
uint32_t ioapic_fisico(uint32_t i);

/* Pasa del PIC al IOAPIC: enmascara el 8259, programa la tabla de
 * redirección (todas las IRQ a la CPU 0) con lo que ya estaba habilitado en
 * el PIC, y comprueba que el tic del PIT llega. Si no llega, vuelve al PIC y
 * devuelve false. Necesita la paginación (el MMIO) y el TSC medido. */
bool ioapic_ativar(void);

/* ¿Las IRQ de los dispositivos van por el IOAPIC? */
bool ioapic_ativo(void);

/* Enmascara o desenmascara la línea de la IRQ ISA "irq" (0-15). */
void ioapic_mascarar(uint8_t irq, bool mascarada);

/* Cambia la CPU que recibe la IRQ "irq" (por su identificador de APIC
 * local). Lista para repartir las IRQ cuando el kernel pueda correr en
 * varias CPU (hoy todas van a la CPU 0, ver la D166). Devuelve false si la
 * IRQ no tiene línea en el IOAPIC. */
bool ioapic_destino(uint8_t irq, uint8_t apic_id);

/* D168: reparte las IRQ 1-15 por turnos entre las CPU secundarias ya
 * arrancadas (la 0, el PIT, se queda en la CPU 0). Devuelve cuántas movió. */
uint32_t ioapic_repartir(void);

/* ---- D211: rutas del _PRT, modo mixto con el 8259 y autoprueba ----
 *
 * Una línea (la IRQ n que ven los drivers, vector 0x20 + n) puede llegar por
 * su entrada ISA (la de la D166), por las GSI que el _PRT da a sus funciones
 * PCI (rutas) y, además, por el 8259 (LINT0 de la CPU 0 en ExtINT). Quien
 * decide qué se usa es irq_rotas.c; esto solo lo escribe en el hardware. */
#define IOAPIC_SEM_GSI   0xFFFFFFFFu
#define IOAPIC_ROTAS_MAX 24
#define IOAPIC_ROTA_ISA  (-2)   /* la GSI es la de la entrada ISA de la línea */
#define IOAPIC_ROTA_ERRO (-1)

uint32_t ioapic_gsi_isa(uint8_t irq);          /* la GSI de la entrada ISA (IOAPIC_SEM_GSI si no tiene) */
uint32_t ioapic_sci(void);                     /* la IRQ de la SCI (de la FADT) */
uint8_t  ioapic_destino_atual(uint8_t irq);    /* el APIC que recibe la línea */
uint8_t  ioapic_apic_cpu0(void);
/* Una ruta GSI -> línea, creada enmascarada (sin ligar). Devuelve su índice,
 * IOAPIC_ROTA_ISA si la GSI es la de la entrada ISA de la línea, o
 * IOAPIC_ROTA_ERRO (GSI fuera de los IOAPIC o de otra línea). */
int      ioapic_rota_criar(uint8_t irq, uint32_t gsi, bool nivel, bool baixo);
void     ioapic_rota_ligar(int r, bool ligada);
bool     ioapic_rota_info(int r, uint32_t *gsi, uint8_t *linha, bool *ligada);
/* La entrada ISA de la línea siempre enmascarada (sus dispositivos llegan
 * por otra GSI, o no llegan). */
void     ioapic_isa_desviar(uint8_t irq, bool desviada);
bool     ioapic_isa_desviada(uint8_t irq);
/* La línea entra también por el 8259 (modo mixto). Solo desde la CPU 0. */
bool     ioapic_via_pic(uint8_t irq, bool pic);
uint16_t ioapic_linhas_pic(void);
/* Un pin >= 16 sin ruta y fuera de "evitar" (la prueba de fallo). */
bool     ioapic_gsi_livre(uint32_t *gsi, const uint32_t *evitar, int n_evitar);

typedef struct {
    uint16_t isa_desviada, via_pic;
    uint32_t rotas_ligadas;
} ioapic_estado_t;
void ioapic_estado_salvar(ioapic_estado_t *e);
void ioapic_estado_restaurar(const ioapic_estado_t *e);   /* solo desde la CPU 0 */

#endif
