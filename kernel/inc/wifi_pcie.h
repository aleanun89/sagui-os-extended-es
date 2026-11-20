// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_pcie.h - más Wi-Fi integradas PCIe de portátiles (D410): reconocimiento por PCI de la
 * familia Atheros ath9k (AR9285, AR9287, AR9380...), Broadcom BCM4313 y Ralink RT3090.
 * EXPERIMENTAL: solo identifica y registra (klog); el driver del AR9485 sigue en ar9485.c y
 * las demás aún no tienen driver (docs/wifi-pcie.md). No toca ningún registro de la tarjeta. */
#ifndef SAGUI_WIFI_PCIE_H
#define SAGUI_WIFI_PCIE_H
#include "tipos.h"

/* Recorre el PCI y registra cada Wi-Fi integrada conocida que no sea la AR9485 activa. Devuelve cuántas
 * encontró. No cambia el estado de ninguna tarjeta. */
int wifi_pcie_identificar(void);
#endif
