// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi.h - Wi-Fi cliente con el pincho USB Realtek RTL8188EU (D215).
 *
 * Dos capas:
 *   - kernel/rede/rtl8188eu.c: el chip (registros por control USB, eFuse,
 *     firmware, inicio de MAC/BB/RF, canal, descriptores de TX y RX, claves en
 *     la CAM para el CCMP por hardware). Solo lo llama la tarea «wifi».
 *   - kernel/rede/wifi.c: la pila 802.11 de cliente (búsqueda, autenticación
 *     abierta, asociación, WPA2-PSK con kernel/lib/wpa.c), la interfaz wlan0
 *     de la pila IP y la llamada al sistema SIS_WIFI. */
#ifndef SAGUI_WIFI_H
#define SAGUI_WIFI_H
#include "tipos.h"
#include "usb.h"
#include "wifi_n.h"
#include "../rede/wifi_usb_logica.h"   /* D409: las familias de pincho (WUSB_*) */

/* ---- usb.c llama a esto al enumerar y al retirar ---- */
bool wifi_usb_reconhecer(usb_disp_t *d);   /* true = era el pincho (o su CD virtual) y ya está atendido */
void wifi_usb_retirado(usb_disp_t *d);
/* D280: busca la Wi-Fi integrada Atheros AR9485 (PCIe) y, si está, la enciende la tarea «wifi». */
void wifi_pci_iniciar(void);

/* ---- rtl8188eu.c ---- */
#define R88E_TX_GESTAO 0   /* trama de gestión: cola de gestión, CCK 1 Mb/s */
#define R88E_TX_DADOS  1   /* datos: tasa adaptada */
#define R88E_TX_EAPOL  2   /* datos del saludo: tasa baja y fija */

typedef struct {
    usb_disp_t *d;
    bool     presente;            /* el dispositivo sigue conectado */
    uint8_t  mac[6];
    uint8_t  rom[512];            /* imagen de la eFuse */
    uint8_t  xtal;
    bool     cck_alta_potencia;
    uint32_t rf_chnlbw;
    uint8_t  canal;
    int      rx_h;                /* recepción continua (usb_rx_*) */
    uint32_t rx_buf;              /* buffer físico de recepción (16 KB) */
    uint32_t sys_cfg;             /* REG_SYS_CFG: versión del chip */
    bool     etv;                 /* RTL8188ETV (comparte el ID con el EU) */
    char     fw_versao[24];
    uint32_t erros_usb;
    /* control de la tasa de datos (una AMRR sencilla con los informes de TX) */
    uint8_t  taxa;                /* índice de tasa del chip (0..11 b/g, 12..19 MCS 0-7, D238) */
    uint16_t tx_ok, tx_reintentos, tx_falhas;
    uint32_t informes;
    /* D238: rendimiento */
    bool     usar_filas;          /* intentar las colas USB (se apaga tras un fallo o con el ajuste «filas 0») */
    uint8_t  rx_fila, tx_fila;    /* transferencias de cada cola; 0 = una cada vez (D215) */
    uint32_t rx_bufs, tx_bufs;    /* buffers físicos de las colas (se guardan para siempre) */
    bool     ht, sgi;             /* conexión 802.11n: tasas MCS y GI corto */
    uint8_t  mcs_ap;              /* los MCS 0-7 que el punto de acceso recibe */
    uint8_t  taxa_rx;             /* la tasa de la última trama de datos recibida */
    uint32_t rx_transf, rx_bytes_usb;    /* transferencias de recepción leídas y sus bytes */
    uint32_t tx_transf, tx_bytes_usb;    /* tramas mandadas al USB y sus bytes */
    uint32_t tx_cheia, tx_perdidas;      /* la cola de envío estaba llena; tramas que no salieron */
    uint32_t tx_erros_seguidos;
    wn_rx_conta_t rx;             /* el desglose de las agregaciones */
    /* D409: la familia del pincho y lo propio de cada una (experimentales, sin probar en hardware). */
    uint8_t  familia;             /* WUSB_RTL8188EU (0 = sin fijar: se trata como el EU), WUSB_RTL8192CU, WUSB_RT2870 */
    uint16_t vid, pid;            /* los del dispositivo */
    /* RTL8188CU/8192CU (rtl8188eu.c con familia C, rtl8192cu_logica.h) */
    bool     c_c2t2r, c_c1t2r, c_umc, c_acut, c_ligado;
    uint8_t  c_ntx, c_nrx, c_placa, c_regulacao, c_pa;
    uint32_t rf_chnlbw1;          /* RF_CHNLBW cacheado de la cadena 1 */
    /* RT2870/RT3070/RT5370 (rt2870.c) */
    uint16_t rt_mac_ver, rt_rev;  /* ASIC_VER_ID */
    uint16_t rt_rf;               /* RT2860_RF_* / RT3070_RF_* / RT5390_RF_* */
    uint8_t  rt_ntx, rt_nrx;
    uint8_t  rt_wcid_seq;
} r88e_t;

/* 0 = bien; si no, *motivo dice por qué (texto para el usuario). */
int  r88e_ler_efuse(r88e_t *r, const char **motivo);
int  r88e_iniciar(r88e_t *r, const uint8_t *fw, uint32_t nfw, const char **motivo);
void r88e_parar(r88e_t *r);
void r88e_canal(r88e_t *r, uint8_t canal);
int  r88e_enviar(r88e_t *r, const uint8_t *q, uint32_t n, int tipo, bool cifrar);
/* Recibe lo que haya (sin esperar); por cada trama 802.11 llama a «chegou». */
typedef void (*r88e_rx_t)(const uint8_t *q, uint32_t n, int rssi, bool decifrada);
int  r88e_receber(r88e_t *r, r88e_rx_t chegou);
void r88e_modo_busca(r88e_t *r, bool busca);   /* filtros abiertos y ganancia de búsqueda */
void r88e_associado(r88e_t *r, const uint8_t bssid[6], uint16_t intervalo, bool slot_curto, bool preambulo_curto);
void r88e_desassociado(r88e_t *r);
/* CCMP en la CAM: entrada 4 = clave de par (mac del AP), 0..3 = de grupo (keyid). */
void r88e_chave(r88e_t *r, int entrada, const uint8_t chave[16], const uint8_t *mac, uint8_t keyid);
void r88e_limpar_chaves(r88e_t *r);
/* D238: tasas 802.11n (ht) con GI corto (sgi) y los MCS del punto de acceso;
 * vuelve a la tasa inicial. Sin HT, la escalera b/g de siempre. */
void r88e_ht(r88e_t *r, bool ht, bool sgi, uint8_t mcs_ap);
/* D238: deja las colas USB y vuelve a lo de la D215 (una lectura cada vez y
 * usb_bulk para enviar) hasta que se vuelva a enchufar el pincho. */
void r88e_rx_simples(r88e_t *r);
/* D238: transferencias en vuelo de cada cola (0 sin cola). */
void r88e_em_voo(r88e_t *r, uint32_t *rx, uint32_t *tx);

/* Nombre del archivo de firmware de /lib/firmware según la familia y el chip (tras leer la eFuse). */
const char *r88e_nome_firmware(const r88e_t *r);

/* ---- rt2870.c (D409): Ralink RT2870/RT3070/RT5370, misma interfaz que arriba ---- */
int  rt28_ler_efuse(r88e_t *r, const char **motivo);
const char *rt28_nome_firmware(const r88e_t *r);
int  rt28_iniciar(r88e_t *r, const uint8_t *fw, uint32_t nfw, const char **motivo);
void rt28_canal(r88e_t *r, uint8_t canal);
int  rt28_enviar(r88e_t *r, const uint8_t *q, uint32_t n, int tipo, bool cifrar);
int  rt28_receber(r88e_t *r, r88e_rx_t chegou);
void rt28_modo_busca(r88e_t *r, bool busca);
void rt28_associado(r88e_t *r, const uint8_t bssid[6], uint16_t intervalo, bool slot_curto, bool preambulo_curto);
void rt28_desassociado(r88e_t *r);
void rt28_chave(r88e_t *r, int entrada, const uint8_t chave[16], const uint8_t *mac, uint8_t keyid);
void rt28_limpar_chaves(r88e_t *r);
void rt28_ht(r88e_t *r, bool ht, bool sgi, uint8_t mcs_ap);

/* ---- wifi.c ---- */
int32_t sis_wifi(uint32_t op, uint32_t ptr, uint32_t n, uint32_t d, uint32_t e);
/* Para la bandeja e infopc sin pasar por la llamada al sistema. */
struct info_wifi;
void wifi_info(struct info_wifi *v);

#endif
