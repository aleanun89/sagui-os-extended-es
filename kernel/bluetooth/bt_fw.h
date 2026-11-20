// SPDX-License-Identifier: GPL-2.0-or-later
/* bt_fw.h - carga de firmware y parches de los controladores Bluetooth que no arrancan listos (D411).
 *
 * Experimental, sin probar en hardware real. Protocolo escrito a partir de las especificaciones
 * públicas y de lo que se sabe del comportamiento de btusb/btbcm/btrtl/ath3k de Linux (solo como
 * referencia de protocolo: no se copió código):
 *   Broadcom  parche .hcd por HCI (Download_Minidriver 0xFC2E, registros del archivo, Reset). Los .hcd NO
 *             vienen en linux-firmware: se cargan si el usuario los pone en /lib/firmware/brcm/; sin
 *             parche se sigue con lo que el controlador tenga en ROM.
 *   Realtek   rtl_bt/NOMBRE_fw.bin («Realtech», parches por versión de ROM) y NOMBRE_config.bin, orden 0xFC20 en
 *             fragmentos de 252 bytes.
 *   Atheros   AR3011 (ath3k-1.fw) y AR3012 (ar3k/NOMBRE.dfu): peticiones de control de fabricante y bulk
 *             (no HCI), antes de iniciar el HCI.
 * La parte pura (análisis de archivos, tablas, fragmentos) se prueba en el PC (tests/bt_fw_unidad.c). */
#ifndef SAGUI_BT_FW_H
#define SAGUI_BT_FW_H
#include "bt.h"

#define BTF_FIN    0   /* acabó (con o sin parche): el inicio sigue */
#define BTF_SIGUE  1   /* espera la respuesta de una orden o un plazo */

/* Números de compañía del Bluetooth SIG (Assigned Numbers) */
#define BT_FAB_BROADCOM 15
#define BT_FAB_REALTEK  93

/* Lo que se sabe del controlador al llegar al paso de firmware (tras Read_Local_Version). */
typedef struct {
    uint16_t vid, pid;               /* USB */
    uint8_t  hci_ver;
    uint16_t hci_rev, lmp_sub, fabricante;
} bt_fw_ident_t;

/* ---- Broadcom: archivo .hcd (registros: opcode(2) longitud(1) datos) ---- */
typedef struct { uint16_t op; const uint8_t *par; int n; } bt_hcd_reg_t;
int  bt_hcd_validar(const uint8_t *d, int n);                         /* número de registros o <0 */
int  bt_hcd_siguiente(const uint8_t *d, int n, int *pos, bt_hcd_reg_t *r);   /* 1 = registro, 0 = fin, <0 = corrupto */
/* Nombre del archivo: variante 0 «brcm/BCM20702A1-0a5c-21e8.hcd», 1 «brcm/BCM20702A1.hcd» (nombre local
 * del chip recortado a letras y cifras). Devuelve 0, o <0 si el nombre local no sirve. */
int  bt_bcm_nombre(const uint8_t *nome_local, int n, uint16_t vid, uint16_t pid, int variante, char *out, int max);

/* ---- Realtek ---- */
typedef struct {
    const char *chip, *fw, *cfg;
    uint16_t lmp_sub, hci_rev;
    uint8_t  hci_ver;
} bt_rtl_chip_t;
const bt_rtl_chip_t *bt_rtl_buscar(uint16_t lmp_sub, uint16_t hci_rev, uint8_t hci_ver);
typedef struct { const uint8_t *ptr; int n; uint32_t fw_version; int project_id; } bt_rtl_parche_t;
#define BT_RTL_E_FIRMA   (-1)   /* no es «Realtech» */
#define BT_RTL_E_V2      (-2)   /* «RTBTCore»: formato nuevo, no soportado */
#define BT_RTL_E_SIN_PARCHE (-3)/* ningún parche para esta versión de ROM */
#define BT_RTL_E_CORRUPTO (-4)
#define BT_RTL_E_EPILOGO (-5)   /* falta la firma final 51 04 fd 77 */
int  bt_rtl_parche(const uint8_t *fw, int n, uint8_t rom_version, bt_rtl_parche_t *p);
/* Flujo a descargar = parche (con los 4 últimos bytes cambiados por fw_version) + configuración. */
int  bt_rtl_n_frag(int total);                                        /* total/252 + 1 (el último puede ir vacío) */
int  bt_rtl_fragmento(const bt_rtl_parche_t *p, const uint8_t *cfg, int ncfg, int i, uint8_t *out);   /* bytes de parámetros */

/* ---- Atheros ---- */
#define BT_ATH_NINGUNO 0
#define BT_ATH_3011    1
#define BT_ATH_3012    2
int  bt_ath_tipo(uint16_t vid, uint16_t pid);
int  bt_ath_nombre_parche(uint32_t rom, char *out, int max);                  /* ar3k/AthrBT_0x%08x.dfu */
int  bt_ath_nombre_cfg(uint32_t rom, uint8_t ref_clock, char *out, int max);  /* ar3k/ramps_0x%08x_%d.dfu */
/* Carga antes de iniciar el HCI. Devuelve 0 = nada que hacer (seguir con el HCI), 1 = cargado y el
 * dispositivo se reenumera (no seguir: aparecerá otro), <0 = error (se sigue sin parche). */
int  bt_ath_cargar(const bt_plat_t *p);

/* ---- Máquina de estados del inicio (la llama bt_hci.c) ---- */
void bt_fw_reiniciar(void);                                  /* arranque en frío: se puede volver a intentar */
int  bt_fw_empezar(const bt_fw_ident_t *id);                 /* BTF_SIGUE si mandó una orden */
bool bt_fw_espera(uint16_t op);                              /* ¿es la respuesta que espera el motor? */
int  bt_fw_respuesta(uint16_t op, uint8_t st, const uint8_t *r, int n);
int  bt_fw_tic(void);                                        /* plazos (Broadcom espera 50 y 250 ms) */
int  bt_fw_abortar(void);                                    /* una orden nunca contestó: se sigue sin parche */
bool bt_fw_cargado(void);                                    /* se cargó un parche: hay que releer la versión */
const char *bt_fw_estado(void);                              /* texto corto para el registro ("" = nada) */
#endif
