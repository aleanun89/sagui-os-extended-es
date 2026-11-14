// SPDX-License-Identifier: GPL-2.0-or-later
/* sae.h - SAE (Simultaneous Authentication of Equals) de WPA3-Personal con
 * el grupo 19 (P-256), IEEE 802.11-2020 12.4 (D220, v7.3.2).
 *
 * El elemento de la contraseña (PWE) se puede sacar de dos formas:
 *   - cazar y picotear (12.4.4.2.2): depende de la contraseña y de las dos
 *     MAC (la nuestra y la del punto de acceso);
 *   - hash-to-element (H2E, 12.4.4.2.3): de la contraseña y el SSID sale un
 *     punto PT que no depende de las MAC; el PWE es val·PT.
 * El programa «wifi» calcula el PT y los PWE de los puntos de acceso que ve
 * (la contraseña no entra en el kernel); el kernel hace el Commit y el Confirm
 * con el PWE (o con el PT, para el H2E con cualquier punto de acceso).
 *
 * Mismo archivo en el kernel, en el programa «wifi» y en el anfitrión. */
#ifndef SAGUI_SAE_H
#define SAGUI_SAE_H
#include "p256.h"

#define SAE_GRUPO      19
#define SAE_PONTO_TAM  64    /* x || y */

typedef struct {
    p256_ponto_t pwe;
    uint8_t rand[32], mask[32];
    uint8_t escalar[32], elemento[SAE_PONTO_TAM];           /* nuestro Commit */
    uint8_t escalar_par[32], elemento_par[SAE_PONTO_TAM];   /* el del punto de acceso */
    uint8_t kck[32], pmk[32], pmkid[16];
} sae_t;

/* Cazar y picotear: false si en 200 vueltas no sale ningún punto (no pasa). */
bool sae_pwe_hp(const uint8_t *senha, uint32_t nsenha, const uint8_t mac_a[6], const uint8_t mac_b[6],
                uint8_t pwe[SAE_PONTO_TAM]);
/* H2E: el PT de una red (identificador de contraseña opcional: nid = 0). */
bool sae_pt(const uint8_t *ssid, uint32_t nssid, const uint8_t *senha, uint32_t nsenha, const uint8_t *id,
            uint32_t nid, uint8_t pt[SAE_PONTO_TAM]);
bool sae_pwe_de_pt(const uint8_t pt[SAE_PONTO_TAM], const uint8_t mac_a[6], const uint8_t mac_b[6],
                   uint8_t pwe[SAE_PONTO_TAM]);

/* Nuestro Commit con rand y mask (aleatorios en [2, n-1]): false si el
 * escalar no vale (hay que repetir con otros) o el PWE no es un punto. */
bool sae_commit(sae_t *s, const uint8_t pwe[SAE_PONTO_TAM], const uint8_t rand[32], const uint8_t mask[32]);
/* El Commit del punto de acceso: 0 = KCK, PMK y PMKID listos. */
#define SAE_ERRO_ESCALAR  -1   /* escalar fuera de (1, n) */
#define SAE_ERRO_ELEMENTO -2   /* el elemento no está en la curva */
#define SAE_ERRO_REFLEXO  -3   /* nos devuelve nuestro propio Commit */
#define SAE_ERRO_CHAVE    -4   /* el secreto compartido es el infinito */
int  sae_processar_commit(sae_t *s, const uint8_t escalar[32], const uint8_t elemento[SAE_PONTO_TAM]);
/* Confirm = HMAC-SHA256(KCK, send-confirm || escalar || elemento || escalar_par || elemento_par). */
void sae_confirmar(const sae_t *s, uint16_t enviado, uint8_t confirm[32]);
bool sae_verificar(const sae_t *s, uint16_t enviado_par, const uint8_t confirm[32]);

#endif
