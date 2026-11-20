// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_pcie.c - identificación de más Wi-Fi integradas PCIe (D410), experimental.
 * Solo lee el espacio de configuración PCI (identificadores, revisión y subsistema) y lo deja
 * en el registro con la etiqueta [WPCI]; sirve para confirmar qué tarjeta lleva cada portátil
 * (la del HP dv7 era desconocida) antes de escribir su driver. Ningún acceso a los registros
 * de la tarjeta. La lógica del catálogo está en wifi_pcie_logica.h. */
#include "wifi_pcie.h"
#include "pci.h"
#include "console.h"
#include "wifi_pcie_logica.h"

static const char *fam_texto(wpci_fam_t f)
{
    switch (f) {
    case WPCI_FAM_ATH_5008: return "Atheros AR5008/AR9002 (ath9k, sin firmware)";
    case WPCI_FAM_ATH_9003: return "Atheros AR9003 (ath9k, sin firmware)";
    case WPCI_FAM_BRCM_SMAC: return "Broadcom brcmsmac (con microcódigo)";
    case WPCI_FAM_RALINK_PCI: return "Ralink rt2800 PCI (con firmware)";
    default: return "?";
    }
}

int wifi_pcie_identificar(void)
{
    pci_disp_t v[64];
    int n = pci_listar(v, 64), achadas = 0;
    for (int i = 0; i < n; i++) {
        const wpci_chip_t *c = wpci_buscar(v[i].fabricante, v[i].modelo);
        if (!c || c->estado == WPCI_ESTADO_ACTIVO) continue;
        uint32_t rev = pci_le32(&v[i], 0x08) & 0xff;
        uint32_t sub = pci_le32(&v[i], 0x2c);
        klog("[WPCI] %s (%04x:%04x rev %02x, subsistema %04x:%04x) en %02x:%02x.%x: %s, %u cadena(s)%s, ROM: %s\n",
             c->nome, c->fabricante, c->modelo, rev, sub & 0xffff, sub >> 16, v[i].barramento, v[i].dispositivo, v[i].funcao,
             fam_texto(c->fam), c->cadeias, c->ghz5 ? ", 2,4 y 5 GHz" : ", 2,4 GHz", c->rom);
        klog("[WPCI] experimental: reconocida pero sin driver todavia (firmware: %s); ver docs/wifi-pcie.md\n", c->firmware);
        achadas++;
    }
    return achadas;
}
