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
void acpi_pedir_reinicio(void); /* D216: SIS_REINICIAR con servicios: la tarea "apagar" los para y reinicia */
int32_t acpi_controle(uint32_t op);              /* SIS_ACPI */
int32_t acpi_info(uint32_t ptr, uint32_t n);     /* INFO_ENERGIA */
void acpi_botao_evento(void);    /* el botón ya visto en el SCI: cuenta y pide el apagado (IRQ) */
bool acpi_modo_ativo(void);      /* D192: SCI_EN = 1 y sin --sem-acpi (uACPI solo arranca así) */
bool acpi_botao_ativo(void);     /* D192: el botón fijo está atendido por el SCI */
void acpi_botao_rearmar(void);   /* D192: PWRBTN_EN otra vez a 1 (sin tocar los demás bits) */

/* energia.c */
SEM_RETORNO void maquina_reiniciar(void);
SEM_RETORNO void maquina_apagar(void);   /* graba los discos y apaga (ACPI) o dice "Ya puedes apagar" */

#endif
