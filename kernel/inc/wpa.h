// SPDX-License-Identifier: GPL-2.0-or-later
/* wpa.h - criptografía y formatos del Wi-Fi cliente (D215): SHA-1, HMAC-SHA1,
 * PBKDF2 (la PMK), el PRF de la 802.11i, AES-128 con su envoltura de claves
 * (RFC 3394), los elementos de información de las balizas y el saludo de
 * cuatro pasos de WPA2-PSK (EAPOL-Key).
 *
 * El mismo archivo (kernel/lib/wpa.c) se compila en el kernel (la pila
 * 802.11 de kernel/rede/wifi.c), en el programa «wifi» (la PMK se calcula en
 * el programa: la contraseña nunca entra en el kernel) y en el anfitrión
 * (tests/h215_wifi.py, con -DSAGUI_HOST y los vectores de las normas). No
 * usa memcpy ni nada de libc para servir a los tres lados sin adaptación. */
#ifndef SAGUI_WPA_H
#define SAGUI_WPA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdint.h>
#include <stdbool.h>
#endif

/* ------------------------------------------------------------ SHA-1 y HMAC */

#define SHA1_TAM   20
#define SHA1_BLOCO 64

typedef struct {
    uint32_t h[5];
    uint8_t  bloco[SHA1_BLOCO];
    uint32_t usados;
    uint64_t total;
} sha1_t;

void sha1_iniciar(sha1_t *c);
void sha1_somar(sha1_t *c, const void *dados, uint32_t n);
void sha1_fim(sha1_t *c, uint8_t saida[SHA1_TAM]);
void sha1(const void *dados, uint32_t n, uint8_t saida[SHA1_TAM]);

/* HMAC-SHA1 (RFC 2104) de varias partes seguidas (sin juntarlas en memoria). */
void hmac_sha1_partes(const uint8_t *chave, uint32_t nchave, int partes, const uint8_t *const *p,
                      const uint32_t *n, uint8_t saida[SHA1_TAM]);
void hmac_sha1(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[SHA1_TAM]);

/* PBKDF2-HMAC-SHA1 (RFC 2898). La PMK de WPA2-PSK es
 * pbkdf2_sha1(contraseña, SSID, 4096, 32 bytes) (IEEE 802.11-2016, J.4). */
void pbkdf2_sha1(const uint8_t *senha, uint32_t nsenha, const uint8_t *sal, uint32_t nsal, uint32_t iteracoes,
                 uint8_t *saida, uint32_t nsaida);
#define WPA_PMK_TAM 32
/* La PMK de una red: false si la contraseña no tiene de 8 a 63 caracteres o el SSID de 1 a 32. */
bool wpa_pmk(const char *senha, const uint8_t *ssid, uint32_t nssid, uint8_t pmk[WPA_PMK_TAM]);

/* El PRF de la 802.11i (IEEE 802.11-2016, 12.7.1.2): PRF-n(K, A, B). */
void wpa_prf(const uint8_t *chave, uint32_t nchave, const char *rotulo, const uint8_t *b, uint32_t nb,
             uint8_t *saida, uint32_t nsaida);

/* ------------------------------------------------------------ AES-128 */

typedef struct { uint8_t rk[11][16]; } aes128_t;

void aes128_chave(aes128_t *a, const uint8_t chave[16]);
void aes128_cifrar(const aes128_t *a, const uint8_t entrada[16], uint8_t saida[16]);
void aes128_decifrar(const aes128_t *a, const uint8_t entrada[16], uint8_t saida[16]);
/* La caja S del AES (FIPS 197, 5.1.1): la usa también la mezcla de TKIP (D220). */
uint8_t aes_caixa_s(uint8_t x);
/* Envoltura de claves AES (RFC 3394). n = bytes del texto en claro (múltiplo de
 * 8, >= 16); la envuelta ocupa n + 8. desenvolver: false si la comprobación falla. */
void aes_envolver(const uint8_t kek[16], const uint8_t *claro, uint32_t n, uint8_t *envolto);
bool aes_desenvolver(const uint8_t kek[16], const uint8_t *envolto, uint32_t n_envolto, uint8_t *claro);

/* Comparación en tiempo constante y borrado que el compilador no quita. */
bool wpa_igual(const void *a, const void *b, uint32_t n);
void wpa_apagar(void *p, uint32_t n);

/* ------------------------------------------------------------ balizas */

#define WPA_SEG_ABERTA    0
#define WPA_SEG_WEP       1   /* bit «privacy» sin RSN ni WPA */
#define WPA_SEG_WPA1      2   /* solo WPA (el elemento de Microsoft, normalmente TKIP) */
#define WPA_SEG_WPA2      3   /* RSN con PSK */
#define WPA_SEG_OUTRA     4   /* RSN sin PSK ni SAE (Enterprise, OWE...): no se admite */
#define WPA_SEG_WPA3      5   /* RSN solo con SAE (D220) */
#define WPA_SEG_WPA2_WPA3 6   /* RSN con PSK y SAE: modo de transición (D220) */
#define WPA_SEG_WPA1_WPA2 7   /* WPA y RSN con PSK: modo mixto (D220) */

#define WPA_CIFRA_WEP40  1
#define WPA_CIFRA_TKIP   2
#define WPA_CIFRA_CCMP   4
#define WPA_CIFRA_WEP104 5
#define WPA_CIFRA_BIP    6    /* BIP-CMAC-128: el grupo de gestión por defecto */

/* AKM del RSN (máscara en wpa_bss_t.akms) */
#define WPA_AKM_PSK    0x01   /* 00-0F-AC:2 */
#define WPA_AKM_PSK256 0x02   /* 00-0F-AC:6, PSK con SHA-256 */
#define WPA_AKM_SAE    0x04   /* 00-0F-AC:8, WPA3-Personal */
#define WPA_AKM_8021X  0x08   /* Enterprise (no se admite) */
#define WPA_AKM_OUTRA  0x10

typedef struct {
    uint8_t  ssid[32];
    uint8_t  nssid;
    uint8_t  canal;           /* del elemento DS (0 = no venía) */
    uint8_t  seguranca;       /* WPA_SEG_* */
    uint8_t  cifra_grupo;     /* WPA_CIFRA_* del RSN */
    uint8_t  cifras_par;      /* máscara de bits (1 << WPA_CIFRA_*) */
    bool     psk;             /* AKM 00-0F-AC:2 o :6 */
    bool     ht;              /* elemento HT Capabilities (802.11n) */
    uint8_t  tasas[16];       /* en unidades de 500 kb/s, con el bit 0x80 de «básica» */
    uint8_t  ntasas;
    uint8_t  rsn[64];         /* el elemento RSN tal cual (se compara en el mensaje 3) */
    uint8_t  nrsn;
    /* D220 */
    uint8_t  akms;            /* WPA_AKM_* del RSN */
    bool     mfpc, mfpr;      /* 802.11w: capaz / obligatoria */
    uint8_t  cifra_gestao;    /* grupo de gestión (WPA_CIFRA_BIP si no se dice) */
    bool     h2e;             /* RSNXE con «SAE hash-to-element» */
    bool     so_h2e;          /* selector 123 en las tasas: solo H2E */
    uint8_t  rsnxe[8];        /* el elemento RSNXE tal cual (se compara en el mensaje 3) */
    uint8_t  nrsnxe;
    bool     wpa1;            /* hay elemento WPA (Microsoft) */
    uint8_t  wpa1_grupo;      /* WPA_CIFRA_* */
    uint8_t  wpa1_pares;      /* máscara (1 << WPA_CIFRA_*) */
    bool     wpa1_psk;
    uint8_t  wpa1_ie[48];     /* el elemento WPA tal cual */
    uint8_t  nwpa1_ie;
} wpa_bss_t;

/* Analiza los elementos de información de una baliza o respuesta de sondeo
 * (después de los 12 bytes fijos). privacidade = bit 4 de «capability». */
void wpa_analisar_ies(const uint8_t *ie, uint32_t n, bool privacidade, wpa_bss_t *o);

/* ------------------------------------------------------------ lo que se negocia (D220) */

typedef struct {
    uint8_t wpa1;          /* 1 = WPA (elemento de Microsoft, descriptor EAPOL 254) */
    uint8_t akm;           /* 2 (PSK), 6 (PSK-SHA256) u 8 (SAE): sufijo de 00-0F-AC */
    uint8_t cifra_par;     /* WPA_CIFRA_CCMP o WPA_CIFRA_TKIP */
    uint8_t cifra_grupo;
    uint8_t pmf;           /* 0 = sin 802.11w, 1 = capaz (MFPC), 2 = obligatoria (MFPC y MFPR) */
    uint8_t h2e;           /* SAE con hash-to-element: RSNXE en la asociación y en el mensaje 2 */
} wpa_par_t;

/* Lo que se elige con una red: false si no hay nada admitido. sae = tenemos
 * con qué hacer SAE (PT o PWE); psk = tenemos la PMK. */
bool wpa_escolher(const wpa_bss_t *b, bool sae, bool psk, wpa_par_t *p);
/* Nuestro elemento RSN (o WPA) y, con H2E, el RSNXE detrás: lo que va en la
 * asociación y en el mensaje 2. Devuelve el tamaño (como mucho WPA_IE_MAX). */
#define WPA_IE_MAX 48
uint32_t wpa_montar_ie(const wpa_par_t *p, uint8_t *s);
/* Nuestro elemento RSN de siempre: versión 1, grupo CCMP, par CCMP, AKM PSK. */
#define WPA_RSN_IE_TAM 22
void wpa_rsn_ie(uint8_t saida[WPA_RSN_IE_TAM]);

/* ------------------------------------------------------------ saludo de cuatro pasos */

#define WPA_PTK_MAX   64    /* KCK (16) | KEK (16) | TK (16 CCMP, 32 TKIP: clave y las dos de Michael) */
#define WPA_PTK_TAM   48    /* la de CCMP */
#define WPA_NONCE_TAM 32

typedef struct {
    uint8_t  pmk[WPA_PMK_TAM];
    uint8_t  aa[6], spa[6];             /* MAC del punto de acceso y la nuestra */
    uint8_t  anonce[WPA_NONCE_TAM], snonce[WPA_NONCE_TAM];
    uint8_t  ptk[WPA_PTK_MAX];
    uint8_t  nptk;
    bool     tem_anonce, ptk_pronta;
    uint8_t  replay[8];
    bool     tem_replay;
    uint8_t  gtk[32];
    uint8_t  ngtk, gtk_id;
    uint8_t  ie[WPA_IE_MAX];            /* el RSN (o WPA) que mandamos en la asociación */
    uint8_t  nie;
    uint8_t  ap_rsn[64];                /* el RSN (o WPA) de la baliza del punto de acceso */
    uint8_t  nap_rsn;
    uint8_t  rsc[8];                    /* contador de la GTK (mensaje 3 o de grupo) */
    /* D220 */
    wpa_par_t par;
    uint8_t  versao;                    /* versión del descriptor: 1 MD5/RC4, 2 SHA1/AES, 3 o 0 CMAC/AES */
    uint8_t  versao_8021x;
    uint8_t  ap_rsnxe[8];
    uint8_t  nap_rsnxe;
    uint8_t  igtk[16];                  /* la clave de BIP (802.11w) */
    uint16_t igtk_id;
    uint8_t  ipn[6];
    bool     tem_igtk;
    uint8_t  pmkid[16];                 /* el del SAE: se compara con el del mensaje 1 */
    bool     tem_pmkid, pmkid_visto, pmkid_ok;
    uint8_t  pedido[8];                 /* contador de nuestras peticiones (fallo de Michael) */
} wpa_sup_t;

/* Resultados de wpa_processar */
#define WPA_IGNORAR     0    /* no era para nosotros o no cambia nada */
#define WPA_RESPONDER   1    /* mandar «resp» (mensaje 2) */
#define WPA_CHAVES      2    /* mandar «resp» (mensaje 4) e instalar la PTK y la GTK */
#define WPA_GTK_NOVA    3    /* mandar «resp» (grupo, mensaje 2) e instalar la GTK nueva */
#define WPA_ERRO_MIC   -1    /* MIC que no cuadra (contraseña equivocada en el mensaje 3) */
#define WPA_ERRO       -2    /* trama mal formada, contador repetido, RSN distinto... */

#define WPA_EAPOL_MAX 256

/* Prepara el estado para una asociación WPA2-PSK con CCMP (snonce: 32 bytes aleatorios). */
void wpa_sup_iniciar(wpa_sup_t *s, const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6], const uint8_t spa[6],
                     const uint8_t *ap_rsn, uint32_t nap_rsn, const uint8_t snonce[WPA_NONCE_TAM]);
/* Lo mismo con lo negociado (D220): ap_ie = el RSN o el WPA de la baliza;
 * ap_rsnxe, el RSNXE (o nada); pmkid del SAE (o nada). */
void wpa_sup_preparar(wpa_sup_t *s, const wpa_par_t *par, const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6],
                      const uint8_t spa[6], const uint8_t *ap_ie, uint32_t nap_ie, const uint8_t *ap_rsnxe,
                      uint32_t nap_rsnxe, const uint8_t *pmkid, const uint8_t snonce[WPA_NONCE_TAM]);
/* Procesa una trama EAPOL (desde el byte de versión de 802.1X) y prepara la
 * respuesta en resp (WPA_EAPOL_MAX bytes) con su tamaño en *nresp. */
int wpa_processar(wpa_sup_t *s, const uint8_t *eapol, uint32_t n, uint8_t *resp, uint32_t *nresp);
/* El aviso de un fallo de Michael (TKIP, 12.8.2): EAPOL-Key con «error» y «petición». */
uint32_t wpa_falha_mic(wpa_sup_t *s, bool par, uint8_t *resp);

/* Calcula la PTK (para las pruebas): la de CCMP con PRF-SHA1. */
void wpa_ptk(const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6], const uint8_t spa[6],
             const uint8_t anonce[WPA_NONCE_TAM], const uint8_t snonce[WPA_NONCE_TAM], uint8_t ptk[WPA_PTK_TAM]);

#endif
