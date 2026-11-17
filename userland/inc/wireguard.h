// SPDX-License-Identifier: GPL-3.0-only
/* wireguard.h - el protocolo WireGuard (D363), sin llamadas al sistema.
 *
 * Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s como lo describe el «whitepaper» de
 * WireGuard (secciones 5 y 6): el saludo (iniciación y respuesta, en los dos
 * papeles), las claves de transporte, el contador con su ventana contra la
 * repetición, el renuevo de claves por tiempo y por número de mensajes, los
 * keepalive (pasivo y persistente), la cookie bajo carga, la PSK y el roaming
 * del extremo.
 *
 * El módulo no conoce sockets ni la interfaz: quien lo usa le da tres funciones
 * (mandar un datagrama UDP, entregar un paquete IP descifrado a la interfaz y
 * leer el reloj TAI64N) y le pasa el tiempo en milisegundos de un reloj que solo
 * avanza. Así se prueba entero en el anfitrión (tests/h365_wireguard.py) y el
 * demonio de SaguiOS (userland/programas/wg.c) solo pone la parte del sistema.
 *
 * Solo IPv4 dentro del túnel (la pila de SaguiOS no tiene IPv6); el extremo
 * también es IPv4. */
#ifndef SAGUI_WIREGUARD_H
#define SAGUI_WIREGUARD_H
#include "sagui.h"

#define WG_CHAVE        32
#define WG_PARES_MAX    8
#define WG_IPS_MAX      16      /* AllowedIPs por par */
#define WG_FILA_MAX     16      /* paquetes que esperan el saludo, por par */
#define WG_MTU_PADRAO   1420
#define WG_PACOTE_MAX   1500    /* el paquete IP más grande que acepta (la MTU no pasa de aquí) */
#define WG_CAB_DADOS    16      /* tipo, receptor y contador */
#define WG_SOBRECARGA   32      /* cabecera + etiqueta Poly1305 */
#define WG_JANELA_BITS  2048    /* ventana contra la repetición (múltiplo de 32) */

/* Tipos de mensaje */
#define WG_MSG_INICIACAO 1
#define WG_MSG_RESPOSTA  2
#define WG_MSG_COOKIE    3
#define WG_MSG_DADOS     4
#define WG_TAM_INICIACAO 148
#define WG_TAM_RESPOSTA  92
#define WG_TAM_COOKIE    64

typedef struct { uint32_t ip; uint8_t bits; } wg_prefixo_t;

/* Una sesión: el par de claves de transporte de un saludo. */
typedef struct {
    bool     valida;
    bool     iniciador;     /* este lado mandó la iniciación de este saludo */
    bool     confirmada;    /* el respondedor solo manda después del primer dato del iniciador */
    uint32_t indice_local, indice_remoto;
    uint32_t criada_ms;
    uint8_t  envio[WG_CHAVE], recepcao[WG_CHAVE];
    uint64_t contador_envio;
    uint64_t rx_maior;      /* el contador más alto aceptado + 1 (0 = ninguno) */
    uint32_t janela[WG_JANELA_BITS / 32];
} wg_sessao_t;

typedef struct {
    uint8_t  publica[WG_CHAVE];
    uint8_t  psk[WG_CHAVE];           /* ceros si no hay PresharedKey */
    uint8_t  dh_estatico[WG_CHAVE];   /* DH(nuestra privada, su pública): fijo, se calcula una vez */
    uint8_t  chave_mac1[WG_CHAVE];    /* HASH("mac1----" || su pública): el MAC1 de lo que le mandamos */
    uint8_t  chave_cookie[WG_CHAVE];  /* HASH("cookie--" || su pública): descifra sus cookies */
    uint32_t endpoint_ip;             /* 0 = aún no se sabe (espera que el otro llame) */
    uint16_t endpoint_porta;
    wg_prefixo_t ips[WG_IPS_MAX];
    int      nips;
    uint16_t keepalive_s;             /* PersistentKeepalive; 0 = no */

    /* El saludo en curso (este lado inicia). */
    bool     hs_ativo;                /* hay una iniciación sin respuesta */
    uint32_t hs_indice;
    uint8_t  hs_efemera[WG_CHAVE];    /* la privada efímera: se borra al terminar */
    uint8_t  hs_h[32], hs_c[32];
    uint32_t hs_enviado_ms;           /* la última iniciación mandada */
    uint32_t hs_inicio_ms;            /* el primer intento de esta serie (REKEY_ATTEMPT_TIME) */
    bool     hs_tentando;             /* la serie de intentos sigue (reenvía cada REKEY_TIMEOUT) */
    uint8_t  ultimo_mac1[16];         /* el MAC1 de lo último que mandamos: el aad de la cookie */
    bool     tem_mac1;
    uint8_t  cookie[16];
    uint32_t cookie_ms;
    bool     tem_cookie;

    /* Como respondedor */
    uint8_t  ultimo_tai64n[12];       /* la marca de tiempo más alta recibida (contra la repetición) */
    uint32_t ultima_iniciacao_ms;
    bool     recebeu_iniciacao;

    wg_sessao_t atual, anterior, proxima;

    /* Temporizadores (whitepaper, 6.1-6.5) */
    uint32_t ultimo_envio_ms;         /* cualquier paquete mandado (para el keepalive) */
    uint32_t dados_sem_resposta_ms;   /* el primer dato mandado sin que llegue nada después */
    bool     esperando_resposta;
    uint32_t dados_recebidos_ms;      /* el primer dato recibido sin que mandásemos nada después */
    bool     deve_keepalive;
    uint32_t ultimo_hs_ms;            /* el último saludo completo (para borrar claves) */
    bool     teve_hs;

    /* Estado para «wg show» */
    uint32_t hs_completo_unix;        /* hora (Unix) del último saludo completo; 0 = ninguno */
    uint64_t rx_bytes, tx_bytes;

    /* Paquetes que esperan el saludo */
    uint16_t fila_n[WG_FILA_MAX];
    uint8_t  fila[WG_FILA_MAX][WG_PACOTE_MAX];
    int      fila_ini, fila_qtd;
} wg_par_t;

/* Los tiempos del protocolo (whitepaper 6.1), en milisegundos. Las pruebas los acortan. */
typedef struct {
    uint32_t rekey_apos_ms;          /* REKEY_AFTER_TIME, 120 s */
    uint32_t rejeitar_apos_ms;       /* REJECT_AFTER_TIME, 180 s */
    uint32_t tentativa_ms;           /* REKEY_ATTEMPT_TIME, 90 s */
    uint32_t rekey_timeout_ms;       /* REKEY_TIMEOUT, 5 s */
    uint32_t keepalive_ms;           /* KEEPALIVE_TIMEOUT, 10 s */
    uint32_t cookie_validade_ms;     /* lo que vale una cookie recibida, 120 s */
    uint64_t rekey_mensagens;        /* REKEY_AFTER_MESSAGES, 2^60 */
    uint64_t rejeitar_mensagens;     /* REJECT_AFTER_MESSAGES, 2^64 - 2^13 - 1 */
} wg_tempos_t;

typedef struct wg {
    uint8_t  privada[WG_CHAVE], publica[WG_CHAVE];
    uint8_t  chave_mac1[WG_CHAVE];    /* HASH("mac1----" || nuestra pública): comprueba lo que llega */
    uint8_t  chave_cookie[WG_CHAVE];  /* HASH("cookie--" || nuestra pública): cifra nuestras cookies */
    wg_par_t pares[WG_PARES_MAX];
    int      npares;
    uint16_t mtu;
    wg_tempos_t t;

    /* Cookies como respondedor (bajo carga) */
    uint8_t  segredo_cookie[WG_CHAVE];
    uint32_t segredo_ms;
    bool     tem_segredo;
    uint32_t carga_janela_ms, carga_qtd;
    bool     forcar_carga;            /* pruebas: como si estuviera siempre bajo carga */
    uint32_t limite_carga;            /* saludos por segundo a partir de los que pide cookie */

    /* Lo que pone quien lo usa */
    void   (*enviar_udp)(void *ctx, uint32_t ip, uint16_t porta, const uint8_t *d, unsigned n);
    void   (*entregar)(void *ctx, const uint8_t *pacote, unsigned n);
    void   (*tai64n)(void *ctx, uint8_t ts[12]);
    uint32_t (*hora_unix)(void *ctx);
    void    *ctx;

    /* Contadores de lo que se descartó (para «wg show» y las pruebas) */
    uint32_t descartes_repeticao, descartes_mac, descartes_origem, descartes_outros;
    uint32_t cookies_enviadas, cookies_recebidas;
} wg_t;

/* Claves en base64 (44 letras con el «=» final). */
bool wg_base64_ler(const char *s, uint8_t chave[WG_CHAVE]);
void wg_base64_escrever(const uint8_t chave[WG_CHAVE], char s[45]);
void wg_gerar_privada(uint8_t privada[WG_CHAVE]);   /* aleatoria y «clamped» */
void wg_publica_de(const uint8_t privada[WG_CHAVE], uint8_t publica[WG_CHAVE]);
/* «10.0.0.0/24» -> prefijo; false si no es IPv4 válido. */
bool wg_prefixo_ler(const char *s, wg_prefixo_t *p);
bool wg_prefixo_contem(const wg_prefixo_t *p, uint32_t ip);

void wg_iniciar(wg_t *w, const uint8_t privada[WG_CHAVE]);
/* Añade un par; psk puede ser NULL. Devuelve NULL si no cabe o es nuestra propia clave. */
wg_par_t *wg_par_novo(wg_t *w, const uint8_t publica[WG_CHAVE], const uint8_t *psk);
/* El par al que va un destino (el prefijo más largo de los AllowedIPs). */
wg_par_t *wg_par_para(wg_t *w, uint32_t destino);

/* Un datagrama que llegó al puerto del WireGuard (se puede modificar en el sitio). */
void wg_udp_chegou(wg_t *w, uint32_t ip, uint16_t porta, uint8_t *d, unsigned n, uint32_t agora_ms);
/* Un paquete IP que la interfaz quiere mandar por el túnel. */
void wg_enviar_pacote(wg_t *w, const uint8_t *pacote, unsigned n, uint32_t agora_ms);
/* Los temporizadores: llamarla a menudo (como mucho cada ~250 ms). Devuelve los ms
 * hasta la próxima vez que hace falta. */
uint32_t wg_tique(wg_t *w, uint32_t agora_ms);
/* Empieza un saludo con el par ya (wg-quick up con keepalive persistente, o las pruebas). */
void wg_iniciar_saudacao(wg_t *w, wg_par_t *p, uint32_t agora_ms);
/* Borra todas las claves (al apagar la interfaz). */
void wg_apagar(wg_t *w);

#endif
