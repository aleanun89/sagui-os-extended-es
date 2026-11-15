// SPDX-License-Identifier: GPL-2.0-or-later
/* bluetooth.h - Bluetooth con un controlador USB estándar (D279, mejora 059, v8.2).
 *
 * La pila (HCI, L2CAP, SDP, HIDP y el gestor) está en kernel/bluetooth/ y no sabe nada del USB; el
 * puente con el USB, la tarea «bluetooth» y la llamada SIS_BLUETOOTH están en kernel/usb/usbbt.c. Ver
 * docs/bluetooth.md. */
#ifndef SAGUI_BLUETOOTH_H
#define SAGUI_BLUETOOTH_H
#include "tipos.h"

int32_t sis_bluetooth(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

#endif
