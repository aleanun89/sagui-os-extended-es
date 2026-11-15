// SPDX-License-Identifier: GPL-2.0-or-later
/* ar9485.h - Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, familia AR9003, sin
 * firmware), D280. Una sola tarjeta, un solo hilo: solo la llama la tarea «wifi»
 * (kernel/rede/wifi.c), que la usa igual que al RTL8188EU del pincho USB. El driver
 * (kernel/rede/ar9485.c) se escribió leyendo el athn(4) de OpenBSD (ISC) y usa sus
 * registros y tablas (kernel/terceros/bsd/athn/); la parte pura está en
 * kernel/rede/ar9485_logica.h. Sin interrupciones: la tarea sondea los anillos. */
#ifndef SAGUI_AR9485_H
#define SAGUI_AR9485_H
#include "tipos.h"

#define AR9485_TX_GESTAO 0   /* trama de gestión: CCK 1 Mb/s */
#define AR9485_TX_DADOS  1   /* datos: tasa adaptada */
#define AR9485_TX_EAPOL  2   /* datos del saludo: CCK 1 Mb/s */

typedef struct {
    bool     presente;       /* hay una AR9485 y se pudo leer su identidad (no implica que haya arrancado) */
    bool     pronto;         /* el chip arrancó y recibe */
    uint8_t  mac[6];
    uint8_t  canal;
    uint32_t mac_ver, mac_rev;
    uint8_t  txcadeias, rxcadeias;
    uint8_t  taxa;           /* tasa de datos actual (el mismo índice que el RTL8188EU: 0..11 b/g) */
    uint8_t  taxa_rx;
    uint16_t tx_ok, tx_reintentos, tx_falhas;
    uint32_t rx_quadros, tx_quadros, rx_erros, rx_crc, rx_phy, tx_cheia, tx_perdidas, tx_underrun;
    bool     cifra_hw;       /* CCMP por hardware (caché de claves) en vez de por software */
} ar9485_t;

typedef void (*ar9485_rx_t)(const uint8_t *q, uint32_t n, int rssi, bool decifrada);

/* Busca la tarjeta en el PCI (sin tocar su estado): true si hay una AR9485. */
bool ar9485_procurar(void);
/* Enciende el chip: PCI, reinicio, ROM, anillos, canal 1. 0 = bien; si no, *motivo es el texto. */
int  ar9485_iniciar(const char **motivo);
void ar9485_parar(void);
const ar9485_t *ar9485_info(void);

void ar9485_canal(uint8_t canal);
void ar9485_modo_busca(bool busca);
int  ar9485_enviar(const uint8_t *q, uint32_t n, int tipo, bool cifrar_hw);
/* Procesa lo que haya llegado (sin esperar) y las confirmaciones de envío; devuelve cuántas tramas entregó. */
int  ar9485_receber(ar9485_rx_t chegou);
void ar9485_associado(const uint8_t bssid[6], uint16_t aid, bool slot_curto, bool preambulo_curto);
void ar9485_desassociado(void);
/* Claves CCMP en la caché del chip (solo con cifra_hw): entrada 4 = de par (MAC del AP), 0..3 = de grupo. */
void ar9485_chave(int entrada, const uint8_t chave[16], const uint8_t *mac);
void ar9485_limpar_chaves(void);
void ar9485_taxa_acertou(uint8_t taxa);   /* para la prueba */
/* Calibración periódica (ruido cada 4 min, IQ cada 30 s); la tarea la llama a menudo, ella decide. */
void ar9485_periodico(void);
/* Una línea de diagnóstico al registro (klog): registros clave y contadores. */
void ar9485_diagnostico(void);
void ar9485_cifra_hw(bool sim);

#endif
