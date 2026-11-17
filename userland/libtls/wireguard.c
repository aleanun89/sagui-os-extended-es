// SPDX-License-Identifier: GPL-3.0-only
/* wireguard.c - el protocolo WireGuard (D363). Ver wireguard.h.
 *
 * Escrito a partir del «whitepaper» de WireGuard (J. A. Donenfeld, «WireGuard:
 * Next Generation Kernel Network Tunnel», secciones 5.4 y 6) y de la descripción
 * del protocolo de wireguard.com; ninguna línea viene de la implementación de
 * Linux ni de otra. Las primitivas son las de la libtls (las mismas del SSH):
 * x25519.c, chacha.c (ChaCha20-Poly1305 y XChaCha20-Poly1305), blake2s.c y el
 * HMAC/HKDF de hash.c.
 *
 * Seguridad:
 *   - todo campo que viene de la red se mira contra el tamaño exacto del mensaje
 *     antes de usarlo (los mensajes del saludo tienen tamaño fijo);
 *   - las comparaciones de claves, MAC y etiquetas son de tiempo constante
 *     (cripto_igual); la clave pública del que inicia se compara con todos los
 *     pares, sin salir en el primero;
 *   - las claves efímeras, las de encadenamiento y las de transporte que caducan
 *     se borran con cripto_apagar;
 *   - el contador se comprueba antes de descifrar (barato) y la ventana solo se
 *     marca después de que la etiqueta valga (un paquete falso no la mueve). */
#include "cripto.h"
#include "wireguard.h"

static const char CONSTRUCAO[] = "Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s";
static const char IDENTIFICADOR[] = "WireGuard v1 zx2c4 Jason@zx2c4.com";
static const char ROTULO_MAC1[] = "mac1----";
static const char ROTULO_COOKIE[] = "cookie--";
static const uint8_t VAZIO[1];   /* en lugar de NULL con 0 bytes (memcpy con NULL no vale en C) */

/* ------------------------------------------------------------ utilidades */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void poe_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint64_t le64(const uint8_t *p) { return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32; }
static void poe_le64(uint8_t *p, uint64_t v) { poe_le32(p, (uint32_t)v); poe_le32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t ler_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static bool passou(uint32_t agora, uint32_t desde, uint32_t ms) { return (uint32_t)(agora - desde) >= ms; }

/* HASH(a || b) */
static void hash2(uint8_t saida[32], const void *a, unsigned na, const void *b, unsigned nb)
{
    blake2s_t c;
    blake2s_iniciar(&c, 32);
    blake2s_somar(&c, a, na);
    blake2s_somar(&c, b, nb);
    blake2s_fim(&c, saida);
}

/* MAC(chave, dados): BLAKE2s con clave y 16 bytes. La clave es de 32 bytes (mac1, cookie
 * del respondedor) o de 16 (mac2: la cookie misma). */
static void mac16(uint8_t saida[16], const uint8_t chave[32], const void *d, unsigned n) { blake2s(saida, 16, chave, 32, d, n); }
static void mac16_cookie(uint8_t saida[16], const uint8_t cookie[16], const void *d, unsigned n) { blake2s(saida, 16, cookie, 16, d, n); }

/* KDF_n(chave, entrada) del whitepaper = HKDF con HMAC-BLAKE2s, sin «info». */
static void kdf(const uint8_t chave[32], const void *entrada, unsigned n, uint8_t *t1, uint8_t *t2, uint8_t *t3)
{
    uint8_t prk[32], saida[96];
    unsigned qtd = t3 ? 3 : t2 ? 2 : 1;
    hkdf_extrair(&HASH_BLAKE2S, chave, 32, entrada, n, prk);
    hkdf_expandir(&HASH_BLAKE2S, prk, "", 0, saida, 32 * qtd);
    memcpy(t1, saida, 32);
    if (t2) memcpy(t2, saida + 32, 32);
    if (t3) memcpy(t3, saida + 64, 32);
    cripto_apagar(prk, sizeof prk);
    cripto_apagar(saida, sizeof saida);
}

static void nonce_de(uint8_t n12[12], uint64_t contador) { memset(n12, 0, 4); poe_le64(n12 + 4, contador); }

/* AEAD(chave, contador, texto, aad): sale n + 16 bytes. */
static void aead_cifrar(const uint8_t chave[32], uint64_t contador, const uint8_t *texto, unsigned n,
                        const uint8_t *aad, unsigned naad, uint8_t *saida)
{
    uint8_t n12[12];
    nonce_de(n12, contador);
    chachapoly_cifrar(chave, n12, aad, naad, texto, saida, n, saida + n);
}

static bool aead_decifrar(const uint8_t chave[32], uint64_t contador, const uint8_t *cifrado, unsigned n,
                          const uint8_t *aad, unsigned naad, uint8_t *saida)
{
    uint8_t n12[12];
    nonce_de(n12, contador);
    return chachapoly_decifrar(chave, n12, aad, naad, cifrado, saida, n, cifrado + n);
}

static bool tudo_zero(const uint8_t *p, unsigned n)
{
    uint8_t d = 0;
    for (unsigned i = 0; i < n; i++) d |= p[i];
    return d == 0;
}

/* DH; false si el resultado es cero (un punto de orden bajo: no hay secreto). */
static bool dh(uint8_t saida[32], const uint8_t privada[32], const uint8_t publica[32])
{
    x25519(saida, privada, publica);
    return !tudo_zero(saida, 32);
}

/* C y H iniciales: C = HASH(CONSTRUCAO); H = HASH(HASH(C || IDENTIFICADOR) || pública del respondedor). */
static void noise_inicio(uint8_t c[32], uint8_t h[32], const uint8_t publica_resp[32])
{
    hash2(c, CONSTRUCAO, sizeof CONSTRUCAO - 1, "", 0);
    hash2(h, c, 32, IDENTIFICADOR, sizeof IDENTIFICADOR - 1);
    hash2(h, h, 32, publica_resp, 32);
}

static void mix_hash(uint8_t h[32], const void *d, unsigned n) { hash2(h, h, 32, d, n); }

/* ------------------------------------------------------------ claves y textos */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool wg_base64_ler(const char *s, uint8_t chave[WG_CHAVE])
{
    /* 32 bytes = 43 letras y un «=» (como las escribe el wg de siempre). */
    for (int i = 0; i < 43; i++)
        if (b64_val(s[i]) < 0) return false;
    if (s[43] != '=' || s[44] != 0) return false;
    uint32_t acc = 0;
    int bits = 0, k = 0;
    for (int i = 0; i < 43; i++) {
        acc = (acc << 6) | (uint32_t)b64_val(s[i]);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (k < 32) chave[k++] = (uint8_t)(acc >> bits);
        }
    }
    /* Los 2 bits que sobran de la última letra tienen que ser cero (una sola escritura por clave). */
    return k == 32 && (acc & ((1u << bits) - 1)) == 0;
}

void wg_base64_escrever(const uint8_t chave[WG_CHAVE], char s[45])
{
    uint32_t acc = 0;
    int bits = 0, k = 0;
    for (int i = 0; i < 32; i++) {
        acc = (acc << 8) | chave[i];
        bits += 8;
        while (bits >= 6) { bits -= 6; s[k++] = B64[(acc >> bits) & 63]; }
    }
    if (bits) s[k++] = B64[(acc << (6 - bits)) & 63];
    s[k++] = '=';
    s[k] = 0;
}

void wg_gerar_privada(uint8_t privada[WG_CHAVE])
{
    cripto_aleatorio(privada, 32);
    privada[0] &= 248;
    privada[31] &= 127;
    privada[31] |= 64;
}

void wg_publica_de(const uint8_t privada[WG_CHAVE], uint8_t publica[WG_CHAVE]) { x25519_base(publica, privada); }

static bool ler_ipv4(const char **ps, uint32_t *ip)
{
    const char *s = *ps;
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return false;
        uint32_t o = 0;
        int d = 0;
        while (*s >= '0' && *s <= '9') { o = o * 10 + (uint32_t)(*s++ - '0'); if (++d > 3 || o > 255) return false; }
        v = v << 8 | o;
        if (i < 3) { if (*s != '.') return false; s++; }
    }
    *ip = v;
    *ps = s;
    return true;
}

bool wg_prefixo_ler(const char *s, wg_prefixo_t *p)
{
    uint32_t ip;
    if (!ler_ipv4(&s, &ip)) return false;
    uint32_t bits = 32;
    if (*s == '/') {
        s++;
        if (*s < '0' || *s > '9') return false;
        bits = 0;
        int d = 0;
        while (*s >= '0' && *s <= '9') { bits = bits * 10 + (uint32_t)(*s++ - '0'); if (++d > 2) return false; }
        if (bits > 32) return false;
    }
    if (*s) return false;
    uint32_t m = bits ? 0xFFFFFFFFu << (32 - bits) : 0;
    p->ip = ip & m;   /* como el wg: los bits de host se ignoran */
    p->bits = (uint8_t)bits;
    return true;
}

bool wg_prefixo_contem(const wg_prefixo_t *p, uint32_t ip)
{
    uint32_t m = p->bits ? 0xFFFFFFFFu << (32 - p->bits) : 0;
    return (ip & m) == p->ip;
}

/* ------------------------------------------------------------ pares, sesiones e índices */

void wg_iniciar(wg_t *w, const uint8_t privada[WG_CHAVE])
{
    memset(w, 0, sizeof *w);
    memcpy(w->privada, privada, 32);
    w->privada[0] &= 248; w->privada[31] &= 127; w->privada[31] |= 64;
    wg_publica_de(w->privada, w->publica);
    hash2(w->chave_mac1, ROTULO_MAC1, 8, w->publica, 32);
    hash2(w->chave_cookie, ROTULO_COOKIE, 8, w->publica, 32);
    w->mtu = WG_MTU_PADRAO;
    w->t.rekey_apos_ms = 120000;
    w->t.rejeitar_apos_ms = 180000;
    w->t.tentativa_ms = 90000;
    w->t.rekey_timeout_ms = 5000;
    w->t.keepalive_ms = 10000;
    w->t.cookie_validade_ms = 120000;
    w->t.rekey_mensagens = 1ULL << 60;
    w->t.rejeitar_mensagens = 0xFFFFFFFFFFFFFFFFULL - (1ULL << 13);   /* 2^64 - 2^13 - 1 */
    w->limite_carga = 32;
}

wg_par_t *wg_par_novo(wg_t *w, const uint8_t publica[WG_CHAVE], const uint8_t *psk)
{
    if (w->npares >= WG_PARES_MAX || cripto_igual(publica, w->publica, 32)) return NULL;
    for (int i = 0; i < w->npares; i++)
        if (cripto_igual(w->pares[i].publica, publica, 32)) return NULL;
    wg_par_t *p = &w->pares[w->npares];
    memset(p, 0, sizeof *p);
    memcpy(p->publica, publica, 32);
    if (psk) memcpy(p->psk, psk, 32);
    if (!dh(p->dh_estatico, w->privada, publica)) { cripto_apagar(p, sizeof *p); return NULL; }   /* clave de orden bajo */
    hash2(p->chave_mac1, ROTULO_MAC1, 8, publica, 32);
    hash2(p->chave_cookie, ROTULO_COOKIE, 8, publica, 32);
    w->npares++;
    return p;
}

wg_par_t *wg_par_para(wg_t *w, uint32_t destino)
{
    wg_par_t *melhor = NULL;
    int bits = -1;
    for (int i = 0; i < w->npares; i++)
        for (int j = 0; j < w->pares[i].nips; j++) {
            const wg_prefixo_t *x = &w->pares[i].ips[j];
            if ((int)x->bits > bits && wg_prefixo_contem(x, destino)) { bits = x->bits; melhor = &w->pares[i]; }
        }
    return melhor;
}

static void sessao_apagar(wg_sessao_t *s) { cripto_apagar(s, sizeof *s); }

static bool indice_usado(wg_t *w, uint32_t idx)
{
    for (int i = 0; i < w->npares; i++) {
        wg_par_t *p = &w->pares[i];
        if ((p->hs_ativo && p->hs_indice == idx) || (p->atual.valida && p->atual.indice_local == idx) ||
            (p->anterior.valida && p->anterior.indice_local == idx) || (p->proxima.valida && p->proxima.indice_local == idx))
            return true;
    }
    return false;
}

static uint32_t indice_novo(wg_t *w)
{
    uint32_t idx;
    do cripto_aleatorio(&idx, 4); while (idx == 0 || indice_usado(w, idx));
    return idx;
}

static wg_sessao_t *sessao_por_indice(wg_t *w, uint32_t idx, wg_par_t **par)
{
    for (int i = 0; i < w->npares; i++) {
        wg_par_t *p = &w->pares[i];
        wg_sessao_t *v[3] = { &p->atual, &p->anterior, &p->proxima };
        for (int k = 0; k < 3; k++)
            if (v[k]->valida && v[k]->indice_local == idx) { *par = p; return v[k]; }
    }
    return NULL;
}

static bool sessao_usavel(wg_t *w, const wg_sessao_t *s, uint32_t agora)
{
    return s->valida && s->confirmada && !passou(agora, s->criada_ms, w->t.rejeitar_apos_ms) &&
           s->contador_envio < w->t.rejeitar_mensagens;
}

/* ------------------------------------------------------------ ventana contra la repetición
 * rx_maior = el contador más alto aceptado + 1. El bit (c % WG_JANELA_BITS) dice si c
 * ya llegó, para los c de [rx_maior - WG_JANELA_BITS, rx_maior). */

static bool janela_pode(const wg_sessao_t *s, uint64_t c)
{
    if (c >= s->rx_maior) return true;
    if (s->rx_maior - 1 - c >= WG_JANELA_BITS) return false;   /* demasiado viejo */
    uint32_t b = (uint32_t)(c % WG_JANELA_BITS);
    return !(s->janela[b / 32] & (1u << (b % 32)));
}

static bool janela_marcar(wg_sessao_t *s, uint64_t c)
{
    if (!janela_pode(s, c)) return false;
    if (c >= s->rx_maior) {
        if (c - s->rx_maior >= WG_JANELA_BITS) memset(s->janela, 0, sizeof s->janela);
        else
            for (uint64_t x = s->rx_maior; x < c; x++) {   /* los que salen de la ventana dejan su sitio limpio */
                uint32_t b = (uint32_t)(x % WG_JANELA_BITS);
                s->janela[b / 32] &= ~(1u << (b % 32));
            }
        s->rx_maior = c + 1;
    }
    uint32_t b = (uint32_t)(c % WG_JANELA_BITS);
    s->janela[b / 32] |= 1u << (b % 32);
    return true;
}

/* ------------------------------------------------------------ mandar */

static void mandar(wg_t *w, wg_par_t *p, const uint8_t *d, unsigned n, uint32_t agora)
{
    if (!p->endpoint_ip) return;
    w->enviar_udp(w->ctx, p->endpoint_ip, p->endpoint_porta, d, n);
    p->tx_bytes += n;
    p->ultimo_envio_ms = agora;
}

static bool cookie_valida(wg_t *w, wg_par_t *p, uint32_t agora)
{
    if (p->tem_cookie && passou(agora, p->cookie_ms, w->t.cookie_validade_ms)) p->tem_cookie = false;
    return p->tem_cookie;
}

/* mac1 y mac2 de un mensaje del saludo de n bytes (los 32 últimos son los MAC). */
static void selar_macs(wg_t *w, wg_par_t *p, uint8_t *m, unsigned n, uint32_t agora)
{
    mac16(m + n - 32, p->chave_mac1, m, n - 32);
    memcpy(p->ultimo_mac1, m + n - 32, 16);
    p->tem_mac1 = true;
    if (cookie_valida(w, p, agora)) mac16_cookie(m + n - 16, p->cookie, m, n - 16);
    else memset(m + n - 16, 0, 16);
}

static void enviar_dados(wg_t *w, wg_par_t *p, const uint8_t *pacote, unsigned n, uint32_t agora)
{
    static uint8_t buf[WG_PACOTE_MAX + WG_SOBRECARGA + 16];
    wg_sessao_t *s = &p->atual;
    unsigned cheio = n ? (n + 15u) & ~15u : 0;   /* relleno a múltiplo de 16, sin pasar de la MTU */
    if (cheio > w->mtu && n <= w->mtu) cheio = w->mtu;
    if (cheio < n) cheio = n;
    buf[0] = WG_MSG_DADOS; buf[1] = buf[2] = buf[3] = 0;
    poe_le32(buf + 4, s->indice_remoto);
    poe_le64(buf + 8, s->contador_envio);
    memcpy(buf + 16, pacote, n);
    memset(buf + 16 + n, 0, cheio - n);
    uint8_t n12[12];
    nonce_de(n12, s->contador_envio);
    chachapoly_cifrar(s->envio, n12, VAZIO, 0, buf + 16, buf + 16, cheio, buf + 16 + cheio);
    s->contador_envio++;
    mandar(w, p, buf, 16 + cheio + 16, agora);
    p->deve_keepalive = false;
    if (n && !p->esperando_resposta) { p->esperando_resposta = true; p->dados_sem_resposta_ms = agora; }
}

static void fila_descartar(wg_par_t *p)
{
    cripto_apagar(p->fila, sizeof p->fila);
    p->fila_ini = p->fila_qtd = 0;
}

static void fila_esvaziar(wg_t *w, wg_par_t *p, uint32_t agora)
{
    while (p->fila_qtd && sessao_usavel(w, &p->atual, agora)) {
        enviar_dados(w, p, p->fila[p->fila_ini], p->fila_n[p->fila_ini], agora);
        cripto_apagar(p->fila[p->fila_ini], p->fila_n[p->fila_ini]);
        p->fila_ini = (p->fila_ini + 1) % WG_FILA_MAX;
        p->fila_qtd--;
    }
}

/* ------------------------------------------------------------ el saludo: iniciador */

static void hs_apagar(wg_par_t *p)
{
    p->hs_ativo = false;
    cripto_apagar(p->hs_efemera, 32);
    cripto_apagar(p->hs_h, 32);
    cripto_apagar(p->hs_c, 32);
}

static void mandar_iniciacao(wg_t *w, wg_par_t *p, uint32_t agora)
{
    uint8_t m[WG_TAM_INICIACAO], c[32], h[32], k[32], ss[32], epub[32], ts[12];
    hs_apagar(p);
    memset(m, 0, sizeof m);
    m[0] = WG_MSG_INICIACAO;
    uint32_t idx = indice_novo(w);
    poe_le32(m + 4, idx);
    noise_inicio(c, h, p->publica);
    wg_gerar_privada(p->hs_efemera);
    wg_publica_de(p->hs_efemera, epub);
    kdf(c, epub, 32, c, NULL, NULL);
    memcpy(m + 8, epub, 32);
    mix_hash(h, epub, 32);
    if (!dh(ss, p->hs_efemera, p->publica)) goto fim;
    kdf(c, ss, 32, c, k, NULL);
    aead_cifrar(k, 0, w->publica, 32, h, 32, m + 40);
    mix_hash(h, m + 40, 48);
    kdf(c, p->dh_estatico, 32, c, k, NULL);
    w->tai64n(w->ctx, ts);
    aead_cifrar(k, 0, ts, 12, h, 32, m + 88);
    mix_hash(h, m + 88, 28);
    selar_macs(w, p, m, sizeof m, agora);
    memcpy(p->hs_h, h, 32);
    memcpy(p->hs_c, c, 32);
    p->hs_indice = idx;
    p->hs_ativo = true;
    p->hs_enviado_ms = agora;
    mandar(w, p, m, sizeof m, agora);
fim:
    cripto_apagar(c, 32); cripto_apagar(h, 32); cripto_apagar(k, 32); cripto_apagar(ss, 32);
}

void wg_iniciar_saudacao(wg_t *w, wg_par_t *p, uint32_t agora)
{
    if (!p->endpoint_ip) return;
    /* Como mucho una iniciación cada REKEY_TIMEOUT (6.2): el reenvío lo hace wg_tique. */
    if (p->hs_tentando && p->hs_ativo && !passou(agora, p->hs_enviado_ms, w->t.rekey_timeout_ms)) return;
    if (!p->hs_tentando) { p->hs_tentando = true; p->hs_inicio_ms = agora; }
    mandar_iniciacao(w, p, agora);
}

/* Una sesión nueva pasa a ser la actual; la actual, la anterior (se recibe con ella un rato). */
static void sessao_promover(wg_par_t *p, wg_sessao_t *nova)
{
    sessao_apagar(&p->anterior);
    p->anterior = p->atual;
    p->atual = *nova;
    if (nova != &p->proxima) sessao_apagar(nova);
    else sessao_apagar(&p->proxima);
}

static void saudacao_completa(wg_t *w, wg_par_t *p, uint32_t agora)
{
    p->ultimo_hs_ms = agora;
    p->teve_hs = true;
    p->hs_completo_unix = w->hora_unix ? w->hora_unix(w->ctx) : 1;
}

static void resposta_chegou(wg_t *w, uint32_t ip, uint16_t porta, uint8_t *r, uint32_t agora)
{
    uint32_t receptor = le32(r + 8);
    wg_par_t *p = NULL;
    for (int i = 0; i < w->npares; i++)
        if (w->pares[i].hs_ativo && w->pares[i].hs_indice == receptor) p = &w->pares[i];
    if (!p) { w->descartes_outros++; return; }
    uint8_t c[32], h[32], k[32], tau[32], ss[32], vazio[1];
    memcpy(c, p->hs_c, 32);
    memcpy(h, p->hs_h, 32);
    const uint8_t *epub = r + 12;
    kdf(c, epub, 32, c, NULL, NULL);
    mix_hash(h, epub, 32);
    bool ok = dh(ss, p->hs_efemera, epub);
    if (ok) {
        kdf(c, ss, 32, c, NULL, NULL);
        ok = dh(ss, w->privada, epub);
    }
    if (ok) {
        kdf(c, ss, 32, c, NULL, NULL);
        kdf(c, p->psk, 32, c, tau, k);
        mix_hash(h, tau, 32);
        ok = aead_decifrar(k, 0, r + 44, 0, h, 32, vazio);
    }
    if (!ok) { w->descartes_mac++; goto fim; }
    mix_hash(h, r + 44, 16);
    wg_sessao_t s;
    memset(&s, 0, sizeof s);
    kdf(c, "", 0, s.envio, s.recepcao, NULL);
    s.valida = s.iniciador = s.confirmada = true;
    s.indice_local = receptor;
    s.indice_remoto = le32(r + 4);
    s.criada_ms = agora;
    sessao_promover(p, &s);
    hs_apagar(p);
    p->hs_tentando = false;
    p->rx_bytes += WG_TAM_RESPOSTA;   /* como wg show en Linux: los bytes del saludo también cuentan */
    p->endpoint_ip = ip;   /* roaming */
    p->endpoint_porta = porta;
    p->esperando_resposta = false;
    saudacao_completa(w, p, agora);
    /* Lo que esperaba sale ya; si no había nada, un keepalive confirma la sesión al otro lado. */
    if (p->fila_qtd) fila_esvaziar(w, p, agora);
    else enviar_dados(w, p, VAZIO, 0, agora);
fim:
    cripto_apagar(c, 32); cripto_apagar(h, 32); cripto_apagar(k, 32); cripto_apagar(tau, 32); cripto_apagar(ss, 32);
}

/* ------------------------------------------------------------ el saludo: respondedor */

static void cookie_calcular(wg_t *w, uint32_t ip, uint16_t porta, uint32_t agora, uint8_t cookie[16])
{
    if (!w->tem_segredo || passou(agora, w->segredo_ms, 120000)) {   /* Rm cambia cada 2 minutos */
        cripto_aleatorio(w->segredo_cookie, 32);
        w->segredo_ms = agora;
        w->tem_segredo = true;
    }
    uint8_t d[6] = { (uint8_t)(ip >> 24), (uint8_t)(ip >> 16), (uint8_t)(ip >> 8), (uint8_t)ip,
                     (uint8_t)(porta >> 8), (uint8_t)porta };
    mac16(cookie, w->segredo_cookie, d, 6);
}

static bool sob_carga(wg_t *w, uint32_t agora)
{
    if (passou(agora, w->carga_janela_ms, 1000)) { w->carga_janela_ms = agora; w->carga_qtd = 0; }
    w->carga_qtd++;
    return w->forcar_carga || w->carga_qtd > w->limite_carga;
}

/* Bajo carga: el mac2 tiene que ser el de nuestra cookie para esa dirección; si no, se le manda la cookie. */
static bool mac2_ou_cookie(wg_t *w, uint32_t ip, uint16_t porta, const uint8_t *m, unsigned n, uint32_t agora)
{
    uint8_t cookie[16], esperado[16];
    cookie_calcular(w, ip, porta, agora, cookie);
    mac16_cookie(esperado, cookie, m, n - 16);
    bool ok = cripto_igual(esperado, m + n - 16, 16);
    if (!ok) {
        uint8_t r[WG_TAM_COOKIE];
        memset(r, 0, sizeof r);
        r[0] = WG_MSG_COOKIE;
        memcpy(r + 4, m + 4, 4);   /* el índice del que mandó */
        cripto_aleatorio(r + 8, 24);
        xchachapoly_cifrar(w->chave_cookie, r + 8, m + n - 32, 16, cookie, r + 32, 16, r + 48);
        w->enviar_udp(w->ctx, ip, porta, r, sizeof r);
        w->cookies_enviadas++;
    }
    cripto_apagar(cookie, 16);
    return ok;
}

static void iniciacao_chegou(wg_t *w, uint32_t ip, uint16_t porta, uint8_t *m, uint32_t agora)
{
    uint8_t c[32], h[32], k[32], ss[32], spub[32], ts[12], tau[32];
    noise_inicio(c, h, w->publica);
    const uint8_t *epub = m + 8;
    kdf(c, epub, 32, c, NULL, NULL);
    mix_hash(h, epub, 32);
    wg_par_t *p = NULL;
    if (!dh(ss, w->privada, epub)) goto falha;
    kdf(c, ss, 32, c, k, NULL);
    if (!aead_decifrar(k, 0, m + 40, 32, h, 32, spub)) goto falha;
    mix_hash(h, m + 40, 48);
    for (int i = 0; i < w->npares; i++) {   /* sin salir en el primero: el tiempo no dice cuál */
        bool igual = cripto_igual(w->pares[i].publica, spub, 32);
        if (igual) p = &w->pares[i];
    }
    if (!p) goto falha;
    kdf(c, p->dh_estatico, 32, c, k, NULL);
    if (!aead_decifrar(k, 0, m + 88, 12, h, 32, ts)) goto falha;
    mix_hash(h, m + 88, 28);
    /* Contra la repetición: la marca de tiempo tiene que crecer; y no más de 50 por segundo. */
    if (p->recebeu_iniciacao && (memcmp(ts, p->ultimo_tai64n, 12) <= 0 || !passou(agora, p->ultima_iniciacao_ms, 20))) {
        w->descartes_repeticao++;
        goto fim;
    }
    memcpy(p->ultimo_tai64n, ts, 12);
    p->rx_bytes += WG_TAM_INICIACAO;
    p->ultima_iniciacao_ms = agora;
    p->recebeu_iniciacao = true;

    /* La respuesta */
    uint8_t r[WG_TAM_RESPOSTA], e[32], epub_r[32];
    memset(r, 0, sizeof r);
    r[0] = WG_MSG_RESPOSTA;
    wg_sessao_t s;
    memset(&s, 0, sizeof s);
    s.indice_local = indice_novo(w);
    s.indice_remoto = le32(m + 4);
    poe_le32(r + 4, s.indice_local);
    poe_le32(r + 8, s.indice_remoto);
    wg_gerar_privada(e);
    wg_publica_de(e, epub_r);
    kdf(c, epub_r, 32, c, NULL, NULL);
    memcpy(r + 12, epub_r, 32);
    mix_hash(h, epub_r, 32);
    bool ok = dh(ss, e, epub);
    if (ok) { kdf(c, ss, 32, c, NULL, NULL); ok = dh(ss, e, spub); }
    cripto_apagar(e, 32);
    if (!ok) goto falha;
    kdf(c, ss, 32, c, NULL, NULL);
    kdf(c, p->psk, 32, c, tau, k);
    mix_hash(h, tau, 32);
    aead_cifrar(k, 0, VAZIO, 0, h, 32, r + 44);
    mix_hash(h, r + 44, 16);
    kdf(c, "", 0, s.recepcao, s.envio, NULL);
    s.valida = true;
    s.criada_ms = agora;
    /* Sin confirmar hasta que llegue el primer dato con ella (5.4.7). */
    sessao_apagar(&p->proxima);
    p->proxima = s;
    sessao_apagar(&s);
    p->endpoint_ip = ip;
    p->endpoint_porta = porta;
    selar_macs(w, p, r, sizeof r, agora);
    mandar(w, p, r, sizeof r, agora);
    saudacao_completa(w, p, agora);
    goto fim;
falha:
    w->descartes_mac++;
fim:
    cripto_apagar(c, 32); cripto_apagar(h, 32); cripto_apagar(k, 32); cripto_apagar(ss, 32);
    cripto_apagar(tau, 32); cripto_apagar(ts, 12);
}

static void cookie_chegou(wg_t *w, const uint8_t *r, uint32_t agora)
{
    uint32_t receptor = le32(r + 4);
    for (int i = 0; i < w->npares; i++) {
        wg_par_t *p = &w->pares[i];
        bool meu = (p->hs_ativo && p->hs_indice == receptor) || (p->proxima.valida && p->proxima.indice_local == receptor);
        if (!meu || !p->tem_mac1) continue;
        uint8_t cookie[16];
        if (!xchachapoly_decifrar(p->chave_cookie, r + 8, p->ultimo_mac1, 16, r + 32, cookie, 16, r + 48)) {
            w->descartes_mac++;
            return;
        }
        memcpy(p->cookie, cookie, 16);
        cripto_apagar(cookie, 16);
        p->cookie_ms = agora;
        p->tem_cookie = true;
        w->cookies_recebidas++;
        /* No se reenvía ya: la próxima iniciación (wg_tique, REKEY_TIMEOUT) lleva el mac2. */
        return;
    }
    w->descartes_outros++;
}

/* ------------------------------------------------------------ datos */

static void dados_chegou(wg_t *w, uint32_t ip, uint16_t porta, uint8_t *d, unsigned n, uint32_t agora)
{
    wg_par_t *p;
    wg_sessao_t *s = sessao_por_indice(w, le32(d + 4), &p);
    uint64_t contador = le64(d + 8);
    if (!s || passou(agora, s->criada_ms, w->t.rejeitar_apos_ms) || contador >= w->t.rejeitar_mensagens) {
        w->descartes_outros++;
        return;
    }
    if (!janela_pode(s, contador)) { w->descartes_repeticao++; return; }
    unsigned nc = n - WG_SOBRECARGA;
    if (!aead_decifrar(s->recepcao, contador, d + 16, nc, VAZIO, 0, d + 16)) { w->descartes_mac++; return; }
    if (!janela_marcar(s, contador)) { w->descartes_repeticao++; return; }
    p->rx_bytes += n;
    p->endpoint_ip = ip;   /* roaming: el último paquete auténtico dice dónde está */
    p->endpoint_porta = porta;
    p->esperando_resposta = false;
    if (s == &p->proxima) {   /* el iniciador ya usa la sesión nueva: confirmada */
        p->proxima.confirmada = true;
        sessao_promover(p, &p->proxima);
        s = &p->atual;
        fila_esvaziar(w, p, agora);
    }
    /* El iniciador renueva si la sesión se acerca a REJECT_AFTER_TIME (6.2). */
    if (s == &p->atual && s->iniciador && !p->hs_tentando &&
        passou(agora, s->criada_ms, w->t.rejeitar_apos_ms - w->t.keepalive_ms - w->t.rekey_timeout_ms))
        wg_iniciar_saudacao(w, p, agora);
    if (nc == 0) return;   /* keepalive */
    const uint8_t *ipp = d + 16;
    if (nc < 20 || (ipp[0] >> 4) != 4) { w->descartes_origem++; return; }
    uint32_t total = (uint32_t)ipp[2] << 8 | ipp[3];
    if (total < 20 || total > nc) { w->descartes_origem++; return; }
    /* Encaminamiento por clave: el origen tiene que estar en los AllowedIPs de este par. */
    if (wg_par_para(w, ler_be32(ipp + 12)) != p) { w->descartes_origem++; return; }
    w->entregar(w->ctx, ipp, total);
    if (!p->deve_keepalive) { p->deve_keepalive = true; p->dados_recebidos_ms = agora; }
}

void wg_udp_chegou(wg_t *w, uint32_t ip, uint16_t porta, uint8_t *d, unsigned n, uint32_t agora)
{
    if (n < 4 || d[1] || d[2] || d[3]) { w->descartes_outros++; return; }
    switch (d[0]) {
    case WG_MSG_INICIACAO:
    case WG_MSG_RESPOSTA: {
        unsigned esperado = d[0] == WG_MSG_INICIACAO ? WG_TAM_INICIACAO : WG_TAM_RESPOSTA;
        if (n != esperado) { w->descartes_outros++; return; }
        uint8_t m1[16];
        mac16(m1, w->chave_mac1, d, n - 32);
        bool ok = cripto_igual(m1, d + n - 32, 16);
        if (!ok) { w->descartes_mac++; return; }
        if (sob_carga(w, agora) && !mac2_ou_cookie(w, ip, porta, d, n, agora)) return;
        if (d[0] == WG_MSG_INICIACAO) iniciacao_chegou(w, ip, porta, d, agora);
        else resposta_chegou(w, ip, porta, d, agora);
        return;
    }
    case WG_MSG_COOKIE:
        if (n != WG_TAM_COOKIE) { w->descartes_outros++; return; }
        cookie_chegou(w, d, agora);
        return;
    case WG_MSG_DADOS:
        if (n < WG_SOBRECARGA || n > WG_PACOTE_MAX + WG_SOBRECARGA + 16) { w->descartes_outros++; return; }
        dados_chegou(w, ip, porta, d, n, agora);
        return;
    }
    w->descartes_outros++;
}

/* ------------------------------------------------------------ de la interfaz al túnel */

/* Tras mandar con la sesión actual (datos o keepalive): renueva si toca (6.2). */
static void renovar_se_preciso(wg_t *w, wg_par_t *p, uint32_t agora)
{
    wg_sessao_t *s = &p->atual;
    if (!p->hs_tentando && s->valida && ((s->iniciador && passou(agora, s->criada_ms, w->t.rekey_apos_ms)) ||
                                         s->contador_envio >= w->t.rekey_mensagens))
        wg_iniciar_saudacao(w, p, agora);
}

void wg_enviar_pacote(wg_t *w, const uint8_t *pacote, unsigned n, uint32_t agora)
{
    if (n < 20 || (pacote[0] >> 4) != 4 || n > w->mtu) { w->descartes_outros++; return; }
    wg_par_t *p = wg_par_para(w, ler_be32(pacote + 16));
    if (!p || !p->endpoint_ip) { w->descartes_outros++; return; }
    if (sessao_usavel(w, &p->atual, agora)) {
        enviar_dados(w, p, pacote, n, agora);
        renovar_se_preciso(w, p, agora);
        return;
    }
    /* Sin sesión: espera en la fila (si está llena, se pierde el más viejo) y se saluda. */
    if (p->fila_qtd == WG_FILA_MAX) {
        cripto_apagar(p->fila[p->fila_ini], p->fila_n[p->fila_ini]);
        p->fila_ini = (p->fila_ini + 1) % WG_FILA_MAX;
        p->fila_qtd--;
    }
    int k = (p->fila_ini + p->fila_qtd) % WG_FILA_MAX;
    memcpy(p->fila[k], pacote, n);
    p->fila_n[k] = (uint16_t)n;
    p->fila_qtd++;
    /* El respondedor con una sesión recién hecha espera al primer dato del otro, no saluda encima. */
    if (p->proxima.valida && !passou(agora, p->proxima.criada_ms, w->t.rekey_timeout_ms)) return;
    wg_iniciar_saudacao(w, p, agora);
}

/* ------------------------------------------------------------ temporizadores */

uint32_t wg_tique(wg_t *w, uint32_t agora)
{
    uint32_t proximo = 1000;
    for (int i = 0; i < w->npares; i++) {
        wg_par_t *p = &w->pares[i];
        /* Reenvío de la iniciación cada REKEY_TIMEOUT (+ hasta 333 ms al azar), hasta REKEY_ATTEMPT_TIME. */
        if (p->hs_tentando) {
            proximo = 250;
            if (passou(agora, p->hs_inicio_ms, w->t.tentativa_ms)) {
                p->hs_tentando = false;
                hs_apagar(p);
                fila_descartar(p);
            } else {
                uint8_t r;
                cripto_aleatorio(&r, 1);
                if (passou(agora, p->hs_enviado_ms, w->t.rekey_timeout_ms + (uint32_t)r * 333u / 255u))
                    mandar_iniciacao(w, p, agora);
            }
        }
        /* Mandamos datos y no llegó nada en KEEPALIVE + REKEY_TIMEOUT: saludo nuevo. */
        if (p->esperando_resposta && passou(agora, p->dados_sem_resposta_ms, w->t.keepalive_ms + w->t.rekey_timeout_ms)) {
            p->esperando_resposta = false;
            wg_iniciar_saudacao(w, p, agora);
        }
        /* Keepalive pasivo: llegaron datos y no mandamos nada en KEEPALIVE_TIMEOUT. */
        if (p->deve_keepalive && passou(agora, p->dados_recebidos_ms, w->t.keepalive_ms)) {
            p->deve_keepalive = false;
            if (sessao_usavel(w, &p->atual, agora)) { enviar_dados(w, p, VAZIO, 0, agora); renovar_se_preciso(w, p, agora); }
        }
        /* Keepalive persistente. */
        if (p->keepalive_s && p->endpoint_ip && passou(agora, p->ultimo_envio_ms, p->keepalive_s * 1000u)) {
            if (sessao_usavel(w, &p->atual, agora)) { enviar_dados(w, p, VAZIO, 0, agora); renovar_se_preciso(w, p, agora); }
            else if (!p->hs_tentando) wg_iniciar_saudacao(w, p, agora);
            else p->ultimo_envio_ms = agora;
        }
        /* Las sesiones caducan a REJECT_AFTER_TIME; tras 3 veces eso sin saludo, todo se borra. */
        wg_sessao_t *v[3] = { &p->atual, &p->anterior, &p->proxima };
        for (int k = 0; k < 3; k++)
            if (v[k]->valida && passou(agora, v[k]->criada_ms, w->t.rejeitar_apos_ms)) sessao_apagar(v[k]);
        if (p->teve_hs && passou(agora, p->ultimo_hs_ms, 3 * w->t.rejeitar_apos_ms)) {
            p->teve_hs = false;
            for (int k = 0; k < 3; k++) sessao_apagar(v[k]);
            hs_apagar(p);
            p->hs_tentando = false;
        }
        if (p->tem_cookie && passou(agora, p->cookie_ms, w->t.cookie_validade_ms)) {
            p->tem_cookie = false;
            cripto_apagar(p->cookie, 16);
        }
        if (p->deve_keepalive || p->esperando_resposta) proximo = 250;
    }
    return proximo;
}

void wg_apagar(wg_t *w) { cripto_apagar(w, sizeof *w); }
