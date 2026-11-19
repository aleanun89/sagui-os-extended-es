// SPDX-License-Identifier: GPL-2.0-or-later
/* mandos.h - la capa común de mandos y joysticks (D307): USB HID, Xbox 360 con cable y el
 * puerto de juegos. Los drivers registran cada mando y le pasan su estado ya normalizado;
 * los programas lo leen por sondeo (INFO_MANDOS, abi.h). */
#ifndef SAGUI_MANDOS_H
#define SAGUI_MANDOS_H
#include "tipos.h"
#include "abi.h"
#include "hid_informe.h"

/* CAB_MANDOS_PRUEBA (bootinfo.h, byte 144; mkimagem.py --mandos-prueba): bit 0 = registrar los
 * dos mandos de prueba (un Dual Action por el analizador HID y un Xbox 360) y aceptar
 * MANDOS_INYECTAR. Solo para las pruebas. */

void mandos_iniciar(void);                 /* al arrancar: mandos de prueba y puerto de juegos */
/* Registra un mando; devuelve el manejador (>= 0) o -1 si la tabla está llena. */
int  mando_registrar(uint8_t tipo, const char *nome, const char *via, uint16_t fab, uint16_t prod,
                     uint8_t eixos, uint8_t botoes, uint8_t hats, uint8_t mapa);
void mando_atualizar(int h, const hid_estado_t *e);   /* nuevo estado (desde la tarea del driver) */
void mando_retirar(int h);
void mando_informe_crudo(int h, const uint8_t *b, int n);   /* el último informe sin decodificar */
void mando_descriptor(int h, const uint8_t *d, int n);      /* el descriptor de informe (HID) */
int32_t mandos_info(uint32_t ptr, uint32_t n);       /* INFO_MANDOS */

/* ---- portajogos.c: el joystick analógico del puerto de juegos (0x201) ---- */
bool pj_detectar(void);                    /* busca el joystick; lo registra o lo retira */
int  pj_manejador(void);                   /* el manejador de su mando (-1 = no hay) */
void pj_ler(estado_mando_t *e, uint16_t crudo[4]);   /* mide ahora (como mucho cada 20 ms) */
void pj_calibracao(uint16_t cal[4][3], bool *manual);
void pj_calibrar(const uint16_t cal[4][3]);           /* NULL = volver a la automática */

#endif
