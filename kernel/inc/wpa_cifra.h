// SPDX-License-Identifier: GPL-2.0-or-later
/* wpa_cifra.h - las primitivas criptográficas del Wi-Fi que no son las de
 * WPA2-CCMP (D220, v7.3.2): MD5 y HMAC-MD5 (descriptor 1 de EAPOL-Key), RC4,
 * CRC-32, Michael y la mezcla de clave de TKIP, el encapsulado TKIP y WEP de
 * una trama 802.11, HMAC-SHA256 con el KDF de la 802.11 y el HKDF (RFC 5869),
 * AES-CMAC (RFC 4493), el CCMP por software (las tramas de gestión protegidas
 * de la 802.11w) y BIP-CMAC-128.
 *
 * Como wpa.c, se compila en el kernel, en el programa «wifi» y en el
 * anfitrión (tests/h215_wifi.py): no usa nada de libc. */
#ifndef SAGUI_WPA_CIFRA_H
#define SAGUI_WPA_CIFRA_H
#include "wpa.h"

/* ------------------------------------------------------------ MD5 (RFC 1321) */

#define MD5_TAM 16

typedef struct {
    uint32_t h[4];
    uint8_t  bloco[64];
    uint32_t usados;
    uint64_t total;
} md5_t;

void md5_iniciar(md5_t *c);
void md5_somar(md5_t *c, const void *dados, uint32_t n);
void md5_fim(md5_t *c, uint8_t saida[MD5_TAM]);
void md5(const void *dados, uint32_t n, uint8_t saida[MD5_TAM]);
void hmac_md5(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[MD5_TAM]);

/* ------------------------------------------------------------ HMAC-SHA256, KDF y HKDF */

#define SHA256_SAIDA 32

void hmac_sha256_partes(const uint8_t *chave, uint32_t nchave, int partes, const uint8_t *const *p,
                        const uint32_t *n, uint8_t saida[SHA256_SAIDA]);
void hmac_sha256(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[SHA256_SAIDA]);
/* KDF-Hash-Length de la 802.11 (IEEE 802.11-2020, 12.7.1.6.2) con SHA-256:
 * HMAC(K, i || rótulo || contexto || Length), i y Length de 16 bits en
 * little-endian, i desde 1; Length = 8 * nsaida bits. */
void kdf_sha256(const uint8_t *chave, uint32_t nchave, const char *rotulo, const uint8_t *contexto,
                uint32_t ncontexto, uint8_t *saida, uint32_t nsaida);
/* HKDF (RFC 5869) con SHA-256. */
void hkdf_extrair(const uint8_t *sal, uint32_t nsal, const uint8_t *ikm, uint32_t nikm, uint8_t prk[SHA256_SAIDA]);
void hkdf_expandir(const uint8_t prk[SHA256_SAIDA], const uint8_t *info, uint32_t ninfo, uint8_t *saida, uint32_t n);

/* ------------------------------------------------------------ RC4 y CRC-32 */

typedef struct { uint8_t s[256]; uint8_t i, j; } rc4_t;

void rc4_chave(rc4_t *r, const uint8_t *chave, uint32_t n);
void rc4_aplicar(rc4_t *r, uint8_t *dados, uint32_t n);   /* cifra y descifra (en el sitio) */
void rc4_pular(rc4_t *r, uint32_t n);                    /* descarta n bytes del flujo */
uint32_t wpa_crc32(const uint8_t *p, uint32_t n);        /* el CRC-32 de la IEEE 802.3 */

/* ------------------------------------------------------------ Michael y TKIP */

/* Michael (IEEE 802.11-2020, 12.5.2.3) sobre un mensaje ya montado. */
void michael(const uint8_t chave[8], const uint8_t *msg, uint32_t n, uint8_t mic[8]);
/* La mezcla de clave de TKIP (12.5.2.5): fase 1 con los 32 bits altos del
 * TSC y la dirección del transmisor; fase 2 con los 16 bajos -> clave RC4. */
void tkip_fase1(uint16_t ttak[5], const uint8_t tk[16], const uint8_t ta[6], uint32_t iv32);
void tkip_fase2(uint8_t rc4[16], const uint16_t ttak[5], const uint8_t tk[16], uint16_t iv16);

/* Tramas 802.11 de datos. «q» lleva la cabecera (cab bytes, con la de QoS) y
 * luego los datos en claro: n es el tamaño total. El buffer tiene que tener
 * sitio para 20 bytes más (TKIP: IV 8 + MIC 8 + ICV 4) u 8 (WEP: IV 4 + ICV 4).
 * Devuelven el tamaño nuevo de la trama, ya con el bit «protegida». */
uint32_t tkip_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], const uint8_t chave_mic[8],
                     uint64_t tsc, uint8_t keyid);
uint32_t wep_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t *chave, uint32_t nchave,
                    uint32_t iv, uint8_t keyid);
/* Descifran en el sitio: los datos en claro quedan en q + cab + 8 (TKIP) o
 * q + cab + 4 (WEP), con *nclaro bytes (sin MIC ni ICV). */
#define CIFRA_OK         0
#define CIFRA_ERRO_ICV  -1    /* clave equivocada o trama rota: se descarta sin más */
#define CIFRA_ERRO_MIC  -2    /* Michael no cuadra con el ICV bien: contramedidas */
#define CIFRA_ERRO      -3    /* trama mal formada */
int tkip_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], const uint8_t chave_mic[8],
                  uint32_t *nclaro);
int wep_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t *chave, uint32_t nchave, uint32_t *nclaro);
/* El TSC (TKIP) o el PN (CCMP) de la cabecera de seguridad, en claro. */
uint64_t tkip_tsc(const uint8_t *iv);
uint64_t ccmp_pn(const uint8_t *iv);

/* ------------------------------------------------------------ AES-CMAC, CCMP y BIP */

void aes_cmac(const uint8_t chave[16], const uint8_t *m, uint32_t n, uint8_t t[16]);
/* CCMP-128 por software (12.5.3) para tramas de datos y de gestión (la
 * 802.11w: el bit «gestión» del nonce). La cabecera queda en claro; detrás
 * la cabecera CCMP (8) y al final el MIC (8). */
uint32_t ccmp_cifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], uint64_t pn, uint8_t keyid);
/* *pn = el PN de la trama (para la repetición, que comprueba quien llama). */
int ccmp_decifrar(uint8_t *q, uint32_t cab, uint32_t n, const uint8_t tk[16], uint64_t *pn, uint32_t *nclaro);
/* BIP-CMAC-128 (12.5.4): el MMIE (elemento 76, 18 bytes) va al final del cuerpo
 * de una trama de gestión de grupo. */
#define BIP_MMIE_TAM 18
uint32_t bip_assinar(uint8_t *q, uint32_t n, const uint8_t igtk[16], uint16_t keyid, uint64_t ipn);
/* true si el MMIE está, es de keyid y su MIC cuadra; *ipn = su IPN. */
bool bip_verificar(const uint8_t *q, uint32_t n, const uint8_t igtk[16], uint16_t keyid, uint64_t *ipn);

#endif
