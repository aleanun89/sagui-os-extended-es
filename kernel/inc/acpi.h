// SPDX-License-Identifier: GPL-2.0-or-later
/* acpi.h - apagado por ACPI (S5) y botón de encendido (D113). */
#ifndef SAGUI_ACPI_H
#define SAGUI_ACPI_H
#include "tipos.h"

void acpi_capturar(void);       /* antes de la paginación: FADT y el \_S5_ de la DSDT/SSDT */
void acpi_iniciar(void);        /* con interrupciones y procesos: modo ACPI, SCI y botón */
bool acpi_apagar(void);         /* escribe SLP_TYP|SLP_EN; vuelve false si la máquina no se apagó */
bool acpi_disponivel(void);     /* las tablas son correctas, hay \_S5_ y no está desactivado */
void acpi_pedir_apagado(void);  /* SIS_DESLIGAR: la tarea "apagar" llama a maquina_apagar */
void acpi_pedir_reinicio(int metodo); /* D216/D450: la tarea "apagar" para los servicios y reinicia (metodo: ENERGIA_RESET_*) */
bool acpi_tarefa_pronta(void);   /* D450: ya existe la tarea "apagar" */
bool acpi_reset_disponivel(void); /* D450: la FADT tiene un RESET_REG utilizable (RESET_REG_SUP) */
void acpi_reset_descrever(char *b, uint32_t n);
void acpi_reset_escrever(void);  /* D450: escribe el valor de reinicio; vuelve si la máquina sigue */
bool acpi_8042_declarado(void);  /* D450: IAPC_BOOT_ARCH dice que hay un 8042 */
int32_t acpi_controle(uint32_t op);              /* SIS_ACPI */
int32_t acpi_info(uint32_t ptr, uint32_t n);     /* INFO_ENERGIA */
void acpi_botao_evento(void);    /* el botón ya visto en el SCI: cuenta y pide el apagado (IRQ) */
bool acpi_modo_ativo(void);      /* D192: SCI_EN = 1 y sin --sem-acpi (uACPI solo arranca así) */
bool acpi_botao_ativo(void);     /* D192: el botón fijo está atendido por el SCI */
void acpi_botao_rearmar(void);   /* D192: PWRBTN_EN otra vez a 1 (sin tocar los demás bits) */

/* energia.c (D450): el orden de apagado y reinicio */
#define ENERGIA_REINICIAR 0
#define ENERGIA_APAGAR    1
/* Métodos de reinicio, en el orden en que se prueban (D450). El que se pide es el primero; si no
 * reinicia, se sigue con los siguientes y después con los anteriores. */
#define ENERGIA_RESET_AUTO    0
#define ENERGIA_RESET_ACPI    1   /* RESET_REG de la FADT */
#define ENERGIA_RESET_TECLADO 2   /* 8042: 0xFE al puerto 0x64 */
#define ENERGIA_RESET_CF9     3   /* registro de reinicio del chipset (0xCF9: 2, luego 0x0E) */
#define ENERGIA_RESET_TRIPLE  4   /* IDT vacía + int3 */
#define ENERGIA_RESET_MAX     4
SEM_RETORNO void energia_executar(int modo, int metodo);   /* desde la tarea "apagar" (CPU 0) */
SEM_RETORNO void maquina_reiniciar(void);   /* sin la tarea (muy al principio): solo los métodos */

#endif
