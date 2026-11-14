// SPDX-License-Identifier: GPL-2.0-or-later
/* lapic.h - el APIC local (LAPIC) de cada procesador (D157).
 *
 * El LAPIC es el controlador de interrupciones que lleva dentro la propia
 * CPU. Es el que permite:
 *   - encender los procesadores secundarios (con la secuencia INIT-SIPI);
 *   - mandarles una interrupción (IPI) a los demás procesadores;
 *   - tener un temporizador propio, con su propio tic.
 *
 * El registro está en la memoria, en 0xFEE00000 (el "espacio de aperture"
 * del APIC). Los desplazamientos de abajo son los de los registros: el
 * índice del registro es (dirección >> 4) & 0xFF.
 *
 * Cuando la máquina trae un solo procesador, el APIC sigue ahí (inactivo)
 * y nada de esto se usa: el sistema funciona exactamente igual que antes.
 */
#ifndef SAGUI_LAPIC_H
#define SAGUI_LAPIC_H
#include "tipos.h"

/* La dirección virtual donde el kernel mapea el APIC local. Está al final
 * del espacio del programa (0xFFC00000), donde ningún programa llega: el
 * mapa se copia para todos los espacios, porque el kernel usa el APIC
 * incluso cuando corre en el contexto de un programa. */
#define LAPIC_BASE 0xFFC00000u

/* La base física por defecto del APIC local (la de la MADT, si hay). */
#define LAPIC_FISICO 0xFEE00000u

/* Registros (desplazamiento desde la base). */
#define LAPIC_ID       0x020u   /* identificador de esta CPU (bits 24-31) */
#define LAPIC_TPR      0x080u   /* prioridad de tarea */
#define LAPIC_EOI      0x0B0u   /* fin de interrupción */
#define LAPIC_SVR      0x0F0u   /* vector espurio y enable (bit 8) */
#define LAPIC_ICR      0x300u   /* comando de interrupción (delivery) */
#define LAPIC_ICR_ALTO 0x310u   /* destino del comando (bits 24-31) */
#define LAPIC_LVT      0x320u   /* vectores locales (timer en 0x320) */
#define LAPIC_LVT_TIMER 0x320u
#define LAPIC_LINT0    0x350u   /* línea externa 0 (el PIC, ExtINT, en el principal) */
#define LAPIC_LINT1    0x360u   /* línea externa 1 (NMI) */
#define LAPIC_TIMER_INI 0x380u  /* cuenta inicial del temporizador */
#define LAPIC_TIMER_CNT 0x390u  /* cuenta actual del temporizador */
#define LAPIC_DIV      0x3E0u   /* divisor del temporizador */

/* Modos de entrega del ICR (bits 8-10 do registo). O QEMU usa o valor 5
 * para o INIT e o 6 para a SIPI (a Intel põe 4 e 5, mas o firmware e o
 * QEMU combinam aqui): são esses os que valem. */
#define LAPIC_MODO_FIXO 0x000u
#define LAPIC_MODO_INIT 0x500u
#define LAPIC_MODO_SIPI 0x600u

/* El vector espurio: un número que no usa nadie (el 0xFF). */
#define LAPIC_ESPURIO 0xFFu

/* O LVT do temporizador: o QEMU poe o modo periodico no bit 17 (e o vector
 * nos bits 7-0); a Intel põe o modo periodico nos bits 28-31 (01) e o vector
 * nos bits 11-8. Como o QEMU e' o que se usa, escreve-se o valor do QEMU (e
 * o bit 16 e' a mascara: nunca se poe, para o temporizador tocar). */
#define LAPIC_LVT_PERIODICO 0x20000u

/* O divisor do temporizador: no QEMU o registo guarda so os bits 0, 1 e 3.
 * Com 0x0B (esses tres bits a 1) o divisor e' 1 (o mais fino). */
#define LAPIC_DIVISOR_1 0x0Bu

uint32_t lapic_ler(uint32_t deslocamento);
void     lapic_escrever(uint32_t deslocamento, uint32_t valor);
uint32_t lapic_id(void);              /* el identificador de esta CPU */

/* Enciende el APIC local de esta CPU (SVR) y enmascara las dos líneas
 * externas (LINT0 y LINT1): los dispositivos de entrada y salida los lleva
 * el procesador principal, con el PIC de siempre. */
void lapic_ativar(void);

/* Envía un INIT a un procesador (lo deja esperando la SIPI). */
void lapic_init(uint32_t destino);

/* Envía una SIPI: el procesador empieza en la página del vector, en modo
 * real (aquí, la 0x7000, donde está el trampolín). */
void lapic_sipi(uint32_t destino, uint8_t vetor);

/* Envía una interrupción fija al procesador con ese APIC (D164). */
void lapic_ipi(uint32_t destino, uint8_t vetor);

/* El principal acepta IPI: activa su APIC si la BIOS lo dejó apagado, sin
 * quitar el PIC de LINT0. Devuelve true si ya estaba activo (D164). */
bool lapic_ativar_principal(void);

/* Envía una interrupción fija a todos los procesadores menos éste. */
void lapic_ipi_todos(uint8_t vetor);

/* Acaba a interrupcao: escreve o EOI (fim de interrupcao) no APIC local.
 * Sem isto o bit "em servico" fica levantado e o APIC nao entrega mais o
 * mesmo vector: era o que fazia o tic de cada processador soar uma vez so
 * (D157). */
void lapic_fim(void);
/* D211: ¿el vector está en servicio (ISR) en el APIC local de esta CPU? */
bool lapic_em_servico(uint8_t vetor);

/* Prepara el temporizador local para que suene cada 10 ms. Devuelve false
 * si no se ha podido (entonces el procesador se queda con el tic del
 * principal). */
bool lapic_temporizador_iniciar(uint8_t vetor);

#endif
