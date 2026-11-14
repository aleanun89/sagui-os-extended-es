// SPDX-License-Identifier: GPL-2.0-or-later
/* tls.c - cliente TLS 1.3 (RFC 8446) e TLS 1.2 com ECDHE + AEAD (RFC 5246,
 * 5289, 7627, 7905, 8422). D062.
 *
 * Um handshake por conexão, sem retomada de sessão, sem renegociação, sem
 * compressão, sem RC4/CBC/SSL3: só suítes AEAD (AES-GCM e ChaCha20-Poly1305)
 * com troca de chaves efêmera (X25519, P-256 ou P-384). O cliente oferece
 * TLS 1.3 e 1.2 no mesmo ClientHello; o servidor escolhe.
 *
 * Camadas:
 *   - registros: lê cabeçalho + corpo, decifra com a chave do momento;
 *   - mensagens de handshake: junta os pedaços de vários registros (uma
 *     mensagem pode vir partida, ou várias num registro só);
 *   - o handshake em si, em duas funções (1.3 e 1.2) que leem as
 *     mensagens na ordem que o protocolo exige: fora de ordem = erro.
 * Qualquer erro manda o alerta adequado e encerra a conexão: não há
 * recuperação no meio do handshake. */
#include "tls.h"

#define REG_MAX   (16384 + 2048)   /* corpo cifrado máximo (TLS 1.2 permite 2048 de expansão) */
#define HS_MAX    (96 * 1024)      /* maior mensagem de handshake aceita (cadeias grandes) */
#define CERTS_MAX 10

enum { CT_CCS = 20, CT_ALERTA = 21, CT_HS = 22, CT_DADOS = 23 };
enum { HS_CH = 1, HS_SH = 2, HS_TICKET = 4, HS_EE = 8, HS_CERT = 11, HS_SKE = 12, HS_CREQ = 13,
       HS_SHD = 14, HS_CV = 15, HS_CKE = 16, HS_FIN = 20, HS_KEY_UPDATE = 24, HS_MSG_HASH = 254 };
enum { AEAD_GCM = 1, AEAD_CHACHA = 2 };
enum { GRUPO_X25519 = 29 };

typedef struct {
    uint16_t id;
    const char *nome;
    int aead;
    unsigned chave;
    const hash_alg_t *h;
    bool tls13;
} suite_t;

/* Na ordem de preferência: ChaCha20 primeiro, porque sem AES-NI ele é
 * várias vezes mais rápido que o nosso AES sem tabelas. */
static const suite_t SUITES[] = {
    { 0x1303, "TLS_CHACHA20_POLY1305_SHA256", AEAD_CHACHA, 32, &HASH_SHA256, true },
    { 0x1301, "TLS_AES_128_GCM_SHA256", AEAD_GCM, 16, &HASH_SHA256, true },
    { 0x1302, "TLS_AES_256_GCM_SHA384", AEAD_GCM, 32, &HASH_SHA384, true },
    { 0xcca9, "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256", AEAD_CHACHA, 32, &HASH_SHA256, false },
    { 0xcca8, "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256", AEAD_CHACHA, 32, &HASH_SHA256, false },
    { 0xc02b, "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", AEAD_GCM, 16, &HASH_SHA256, false },
    { 0xc02f, "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", AEAD_GCM, 16, &HASH_SHA256, false },
    { 0xc02c, "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", AEAD_GCM, 32, &HASH_SHA384, false },
    { 0xc030, "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", AEAD_GCM, 32, &HASH_SHA384, false },
};
#define NSUITES (int)(sizeof SUITES / sizeof SUITES[0])

/* SignatureScheme do TLS -> ASSIN_* */
static int esquema_tls(uint16_t s)
{
    switch (s) {
    case 0x0401: return ASSIN_RSA_SHA256;
    case 0x0501: return ASSIN_RSA_SHA384;
    case 0x0601: return ASSIN_RSA_SHA512;
    case 0x0403: return ASSIN_ECDSA_SHA256;
    case 0x0503: return ASSIN_ECDSA_SHA384;
    case 0x0603: return ASSIN_ECDSA_SHA512;
    case 0x0804: return ASSIN_PSS_SHA256;
    case 0x0805: return ASSIN_PSS_SHA384;
    case 0x0806: return ASSIN_PSS_SHA512;
    default: return 0;
    }
}

typedef struct {
    int      aead;
    gcm_t    gcm;
    uint8_t  chave[32];
    uint8_t  iv[12];        /* TLS 1.3 e ChaCha: 12 bytes; TLS 1.2 GCM: só os 4 primeiros (o "sal") */
    uint64_t seq;
} cifra_t;

struct tls {
    tls_config_t cfg;
    char     host[256];
    int      fd;
    int      versao;
    const suite_t *suite;
    int      grupo;
    uint8_t  cli_random[32], srv_random[32], sessao[32];
    uint8_t  x_priv[32], x_pub[32];
    uint8_t  ec_priv[48], ec_pub[97];
    int      ec_grupo;          /* curva da chave em ec_priv (0 = nenhuma) */
    sha256_t t256;
    sha512_t t384;
    cifra_t  le, escreve, le_prox;   /* le_prox: TLS 1.2, entra em vigor no ChangeCipherSpec */
    bool     tem_le_prox;
    uint8_t  reg[5 + REG_MAX + 64];
    uint8_t  saida[5 + 16384 + 64];
    uint8_t *dados;            /* texto claro do último registro de aplicação */
    unsigned ndados, pos;
    uint8_t *hs;               /* bytes de handshake ainda não consumidos */
    unsigned nhs, ini_hs, cap_hs;
    uint8_t  s_hs[48], c_hs[48], s_ap[48], c_ap[48];
    uint8_t  mestre12[48];
    bool     ems;
    uint8_t *certs_der;
    cert_t   certs[CERTS_MAX];
    int      ncerts;
    int      validacao, raiz;
    char     alpn[32];
    char     erro[200];
    bool     fim, conectado, pediu_cert;
    unsigned ms;
};

/* ------------------------------------------------------------ utilidades */

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t be24(const uint8_t *p) { return (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]; }

static int falhar(tls_t *t, int codigo, const char *fmt, ...)
{
    if (!t->erro[0]) {
        va_list ap;
        va_start(ap, fmt);
        formatar_lista(t->erro, sizeof t->erro, fmt, ap);
        va_end(ap);
    }
    t->fim = true;
    return codigo;
}

/* Leitor com limites: toda leitura confere o que sobra. */
typedef struct { const uint8_t *p, *fim; bool ruim; } leitor_t;

static leitor_t leitor(const uint8_t *p, unsigned n) { leitor_t l = { p, p + n, false }; return l; }
static bool tem(leitor_t *l, unsigned n) { if ((unsigned)(l->fim - l->p) < n) l->ruim = true; return !l->ruim; }
static unsigned u8(leitor_t *l) { if (!tem(l, 1)) return 0; return *l->p++; }
static unsigned u16(leitor_t *l) { if (!tem(l, 2)) return 0; unsigned v = be16(l->p); l->p += 2; return v; }
static unsigned u24(leitor_t *l) { if (!tem(l, 3)) return 0; unsigned v = be24(l->p); l->p += 3; return v; }
static const uint8_t *bytes(leitor_t *l, unsigned n) { if (!tem(l, n)) return NULL; const uint8_t *p = l->p; l->p += n; return p; }
static leitor_t sub(leitor_t *l, unsigned n) { const uint8_t *p = bytes(l, n); leitor_t s = { p, p ? p + n : NULL, !p }; return s; }
static bool vazio(const leitor_t *l) { return l->p == l->fim; }

/* Escritor para montar mensagens. */
typedef struct { uint8_t *p; unsigned n, max; } escritor_t;

static void w8(escritor_t *e, unsigned v) { if (e->n < e->max) e->p[e->n] = (uint8_t)v; e->n++; }
static void w16(escritor_t *e, unsigned v) { w8(e, v >> 8); w8(e, v); }
static void wbytes(escritor_t *e, const void *d, unsigned n) { const uint8_t *b = d; for (unsigned i = 0; i < n; i++) w8(e, b[i]); }
/* Reserva o campo de tamanho (2 ou 3 bytes) e devolve onde ele está. */
static unsigned abre(escritor_t *e, int bytes_tam) { unsigned pos = e->n; for (int i = 0; i < bytes_tam; i++) w8(e, 0); return pos; }
static void fecha(escritor_t *e, unsigned pos, int bytes_tam)
{
    unsigned n = e->n - pos - (unsigned)bytes_tam;
    for (int i = 0; i < bytes_tam && pos + (unsigned)i < e->max; i++) e->p[pos + i] = (uint8_t)(n >> (8 * (bytes_tam - 1 - i)));
}

/* ------------------------------------------------------------ transcrição */

static void transcrito_somar(tls_t *t, const uint8_t *m, unsigned n)
{
    sha256_somar(&t->t256, m, n);
    sha512_somar(&t->t384, m, n);
}

static void transcrito_hash(const tls_t *t, uint8_t *saida)
{
    if (t->suite->h == &HASH_SHA384) {
        sha512_t c = t->t384;
        sha512_fim(&c, saida);
    } else {
        sha256_t c = t->t256;
        sha256_fim(&c, saida);
    }
}

/* ------------------------------------------------------------ alertas */

static const char *alerta_texto(unsigned d)
{
    switch (d) {
    case 0: return "cierre";
    case 10: return "mensaje inesperado";
    case 20: return "registro alterado (bad_record_mac)";
    case 40: return "negociación fallida: sin algoritmos en común";
    case 42: return "certificado incorrecto";
    case 43: return "certificado no soportado";
    case 44: return "certificado revocado";
    case 45: return "certificado caducado";
    case 46: return "certificado desconocido";
    case 47: return "parámetro ilegal";
    case 48: return "autoridad de certificación desconocida";
    case 50: return "error de formato";
    case 51: return "error de descifrado";
    case 70: return "versión de protocolo no soportada";
    case 71: return "seguridad insuficiente";
    case 80: return "error interno";
    case 86: return "retroceso de versión no permitido";
    case 109: return "falta una extensión";
    case 110: return "extensión no soportada";
    case 112: return "nombre de servidor no reconocido";
    case 116: return "el servidor exige certificado de cliente";
    case 120: return "sin protocolo de aplicación en común";
    default: return "alerta desconocida";
    }
}

static int enviar_registro(tls_t *t, int tipo, const uint8_t *d, unsigned n);

static void mandar_alerta(tls_t *t, unsigned descricao)
{
    uint8_t a[2] = { descricao == 0 ? 1 : 2, (uint8_t)descricao };
    enviar_registro(t, CT_ALERTA, a, 2);
}

static int erro_proto(tls_t *t, unsigned alerta, const char *msg)
{
    if (!t->fim) mandar_alerta(t, alerta);
    return falhar(t, TLS_ERRO_PROTOCOLO, "%s", msg);
}

/* ------------------------------------------------------------ registros */

static int ler_exato(tls_t *t, uint8_t *b, unsigned n)
{
    while (n) {
        int r = ler(t->fd, b, n);
        if (r <= 0) return r < 0 ? r : 0;
        b += r;
        n -= (unsigned)r;
    }
    return 1;
}

/* Nonce = iv XOR número de sequência (TLS 1.3 e ChaCha20 no 1.2). */
static void nonce_xor(const cifra_t *c, uint8_t n[12])
{
    memcpy(n, c->iv, 12);
    for (int i = 0; i < 8; i++) n[11 - i] ^= (uint8_t)(c->seq >> (8 * i));
}

static bool aead_abrir(const cifra_t *c, const uint8_t nonce[12], const uint8_t *aad, unsigned naad,
                       uint8_t *d, unsigned n, const uint8_t *tag)
{
    if (c->aead == AEAD_GCM) return gcm_decifrar(&c->gcm, nonce, aad, naad, d, d, n, tag);
    return chachapoly_decifrar(c->chave, nonce, aad, naad, d, d, n, tag);
}

static void aead_fechar(const cifra_t *c, const uint8_t nonce[12], const uint8_t *aad, unsigned naad,
                        uint8_t *d, unsigned n, uint8_t *tag)
{
    if (c->aead == AEAD_GCM) gcm_cifrar(&c->gcm, nonce, aad, naad, d, d, n, tag);
    else chachapoly_cifrar(c->chave, nonce, aad, naad, d, d, n, tag);
}

/* Lê um registro e decifra. *tipo é o tipo real (no 1.3, o de dentro). */
static int ler_registro(tls_t *t, int *tipo, uint8_t **dados, unsigned *n)
{
    if (t->fim) return falhar(t, TLS_ERRO_REDE, "conexión cerrada");
    uint8_t *h = t->reg;
    int r = ler_exato(t, h, 5);
    if (r <= 0) return r == 0 ? 0 : falhar(t, TLS_ERRO_REDE, "la conexión se cortó (%s)", nome_erro(r));
    unsigned tam = be16(h + 3);
    *tipo = h[0];
    if (h[1] != 3 || tam > REG_MAX || (*tipo < CT_CCS || *tipo > CT_DADOS))
        return erro_proto(t, 50, "el servidor no habla TLS (¿es un puerto HTTP sin cifrar?)");
    r = ler_exato(t, h + 5, tam);
    if (r <= 0) return falhar(t, TLS_ERRO_REDE, "la conexión se cortó en medio de un registro");
    uint8_t *d = h + 5;
    if (!t->le.aead || *tipo == CT_CCS) {   /* sem cifra (ou o CCS, que nunca é cifrado) */
        if (tam > 16384) return erro_proto(t, 22, "registro demasiado grande");
        *dados = d;
        *n = tam;
        return 1;
    }
    cifra_t *c = &t->le;
    uint8_t nonce[12];
    if (t->versao == TLS13) {
        if (*tipo != CT_DADOS || tam < 17) return erro_proto(t, 10, "registro sin cifrar fuera de lugar");
        nonce_xor(c, nonce);
        if (!aead_abrir(c, nonce, h, 5, d, tam - 16, d + tam - 16))
            return erro_proto(t, 20, "registro alterado o clave incorrecta (bad_record_mac)");
        unsigned k = tam - 16;
        while (k && d[k - 1] == 0) k--;   /* tira o preenchimento e acha o tipo verdadeiro */
        if (!k) return erro_proto(t, 10, "registro cifrado vacío");
        *tipo = d[--k];
        if (k > 16384) return erro_proto(t, 22, "registro demasiado grande");
        *dados = d;
        *n = k;
    } else {
        uint8_t aad[13];
        unsigned extra = c->aead == AEAD_GCM ? 8 : 0;   /* nonce explícito do GCM */
        if (tam < extra + 16) return erro_proto(t, 50, "registro cifrado demasiado corto");
        unsigned k = tam - extra - 16;
        if (c->aead == AEAD_GCM) {
            memcpy(nonce, c->iv, 4);
            memcpy(nonce + 4, d, 8);
        } else nonce_xor(c, nonce);
        for (int i = 0; i < 8; i++) aad[i] = (uint8_t)(c->seq >> (56 - 8 * i));
        aad[8] = (uint8_t)*tipo;
        aad[9] = 3; aad[10] = 3;
        aad[11] = (uint8_t)(k >> 8); aad[12] = (uint8_t)k;
        if (!aead_abrir(c, nonce, aad, 13, d + extra, k, d + extra + k))
            return erro_proto(t, 20, "registro alterado o clave incorrecta (bad_record_mac)");
        if (k > 16384) return erro_proto(t, 22, "registro demasiado grande");
        *dados = d + extra;
        *n = k;
    }
    c->seq++;
    return 1;
}

static int enviar_registro(tls_t *t, int tipo, const uint8_t *d, unsigned n)
{
    do {
        unsigned k = n > 16384 ? 16384 : n;
        uint8_t *h = t->saida, *corpo = h + 5;
        cifra_t *c = &t->escreve;
        unsigned tam;
        h[0] = (uint8_t)tipo;
        h[1] = 3;
        h[2] = t->versao == TLS13 || t->versao == TLS12 ? 3 : 1;   /* o primeiro ClientHello vai como 3.1 */
        if (!c->aead || tipo == CT_CCS) {
            memcpy(corpo, d, k);
            tam = k;
        } else if (t->versao == TLS13) {
            memcpy(corpo, d, k);
            corpo[k] = (uint8_t)tipo;
            tam = k + 1 + 16;
            h[0] = CT_DADOS;
            h[3] = (uint8_t)(tam >> 8); h[4] = (uint8_t)tam;
            uint8_t nonce[12];
            nonce_xor(c, nonce);
            aead_fechar(c, nonce, h, 5, corpo, k + 1, corpo + k + 1);
            c->seq++;
        } else {
            unsigned extra = c->aead == AEAD_GCM ? 8 : 0;
            uint8_t nonce[12], aad[13];
            for (int i = 0; i < 8; i++) aad[i] = (uint8_t)(c->seq >> (56 - 8 * i));
            aad[8] = (uint8_t)tipo; aad[9] = 3; aad[10] = 3;
            aad[11] = (uint8_t)(k >> 8); aad[12] = (uint8_t)k;
            if (c->aead == AEAD_GCM) {
                memcpy(nonce, c->iv, 4);
                memcpy(nonce + 4, aad, 8);   /* nonce explícito = número de sequência: nunca se repete */
                memcpy(corpo, aad, 8);
            } else nonce_xor(c, nonce);
            memcpy(corpo + extra, d, k);
            aead_fechar(c, nonce, aad, 13, corpo + extra, k, corpo + extra + k);
            tam = extra + k + 16;
            c->seq++;
        }
        h[3] = (uint8_t)(tam >> 8);
        h[4] = (uint8_t)tam;
        if (escrever_tudo(t->fd, h, 5 + tam) < 0) return falhar(t, TLS_ERRO_REDE, "no se pudo enviar (conexión cortada)");
        d += k;
        n -= k;
    } while (n);
    return 0;
}

/* ------------------------------------------------------------ mensagens de handshake */

static int tratar_alerta(tls_t *t, const uint8_t *d, unsigned n)
{
    if (n != 2) return erro_proto(t, 50, "alerta mal formada");
    if (d[1] == 0) { t->fim = true; return 0; }   /* close_notify */
    t->fim = true;
    return falhar(t, TLS_ERRO_ALERTA, "el servidor cortó la conexión: %s (alerta %u)", alerta_texto(d[1]), d[1]);
}

/* Devolve a próxima mensagem inteira (cabeçalho de 4 bytes incluído). */
static int proxima_mensagem(tls_t *t, uint8_t **msg, unsigned *n)
{
    for (;;) {
        unsigned disp = t->nhs - t->ini_hs;
        if (disp >= 4) {
            unsigned tam = be24(t->hs + t->ini_hs + 1);
            if (tam > HS_MAX) return erro_proto(t, 50, "mensaje de negociación demasiado grande");
            if (disp >= 4 + tam) {
                *msg = t->hs + t->ini_hs;
                *n = 4 + tam;
                t->ini_hs += 4 + tam;
                return 1;
            }
            if (4 + tam > t->cap_hs) {
                uint8_t *novo = realoca(t->hs, 4 + tam);
                if (!novo) return falhar(t, TLS_ERRO_MEMORIA, "sin memoria");
                t->hs = novo;
                t->cap_hs = 4 + tam;
            }
        }
        /* chega o que sobrou para o começo e lê mais um registro */
        if (t->ini_hs) {
            memmove(t->hs, t->hs + t->ini_hs, disp);
            t->nhs = disp;
            t->ini_hs = 0;
        }
        int tipo;
        uint8_t *d;
        unsigned k;
        int r = ler_registro(t, &tipo, &d, &k);
        if (r <= 0) return r == 0 ? falhar(t, TLS_ERRO_REDE, "el servidor cerró la conexión durante la negociación") : r;
        if (tipo == CT_ALERTA) {
            r = tratar_alerta(t, d, k);
            return r < 0 ? r : falhar(t, TLS_ERRO_REDE, "el servidor cerró la conexión durante la negociación");
        }
        if (tipo == CT_CCS) {
            if (k != 1 || d[0] != 1 || disp) return erro_proto(t, 10, "ChangeCipherSpec incorrecto");
            if (t->versao == TLS12) {
                if (!t->tem_le_prox) return erro_proto(t, 10, "ChangeCipherSpec antes de tiempo");
                t->le = t->le_prox;
                t->tem_le_prox = false;
            }
            continue;   /* no 1.3 é só compatibilidade com "caixas do meio" e não muda nada */
        }
        if (tipo != CT_HS) return erro_proto(t, 10, "datos de aplicación durante la negociación");
        if (!k) return erro_proto(t, 10, "registro de negociación vacío");
        if (t->nhs + k > t->cap_hs) {
            unsigned cap = t->cap_hs * 2 < t->nhs + k ? t->nhs + k : t->cap_hs * 2;
            uint8_t *novo = realoca(t->hs, cap);
            if (!novo) return falhar(t, TLS_ERRO_MEMORIA, "sin memoria");
            t->hs = novo;
            t->cap_hs = cap;
        }
        memcpy(t->hs + t->nhs, d, k);
        t->nhs += k;
    }
}

/* Espera uma mensagem de um tipo; soma à transcrição. Se vier outra,
 * devolve OUTRA_MSG sem consumi-la. */
#define OUTRA_MSG (-1000)
static int esperar_msg(tls_t *t, int tipo, leitor_t *corpo, uint8_t **inteira, unsigned *ninteira)
{
    uint8_t *m;
    unsigned n;
    int r = proxima_mensagem(t, &m, &n);
    if (r <= 0) return r;
    if (m[0] != tipo) {   /* fica para a próxima leitura; o chamador decide (mensagens opcionais) */
        t->ini_hs -= n;
        return OUTRA_MSG;
    }
    transcrito_somar(t, m, n);
    *corpo = leitor(m + 4, n - 4);
    if (inteira) { *inteira = m; *ninteira = n; }
    return 1;
}


static int enviar_hs(tls_t *t, const uint8_t *m, unsigned n)
{
    transcrito_somar(t, m, n);
    return enviar_registro(t, CT_HS, m, n);
}

/* ------------------------------------------------------------ ClientHello */

static bool eh_ip(const char *h)
{
    int pontos = 0;
    for (; *h; h++) {
        if (*h == '.') pontos++;
        else if (*h < '0' || *h > '9') return false;
    }
    return pontos == 3;
}

static bool oferece13(const tls_t *t) { return t->cfg.versao_max >= TLS13; }
static bool oferece12(const tls_t *t) { return t->cfg.versao_min <= TLS12; }

static unsigned montar_client_hello(tls_t *t, uint8_t *buf, unsigned max, const uint8_t *cookie, unsigned ncookie)
{
    escritor_t e = { buf, 0, max };
    w8(&e, HS_CH);
    unsigned p_msg = abre(&e, 3);
    w16(&e, 0x0303);
    wbytes(&e, t->cli_random, 32);
    w8(&e, 32);
    wbytes(&e, t->sessao, 32);   /* "legacy_session_id" aleatório: compatibilidade (RFC 8446, D.4) */
    unsigned p = abre(&e, 2);
    for (int i = 0; i < NSUITES; i++)
        if ((SUITES[i].tls13 && oferece13(t)) || (!SUITES[i].tls13 && oferece12(t))) w16(&e, SUITES[i].id);
    fecha(&e, p, 2);
    w8(&e, 1);
    w8(&e, 0);   /* sem compressão */
    unsigned p_ext = abre(&e, 2);
    if (t->host[0] && !eh_ip(t->host)) {   /* server_name (RFC 6066): não vale para IPs */
        w16(&e, 0);
        unsigned a = abre(&e, 2), b = abre(&e, 2);
        w8(&e, 0);
        unsigned c = abre(&e, 2);
        wbytes(&e, t->host, (unsigned)tam_texto(t->host));
        fecha(&e, c, 2); fecha(&e, b, 2); fecha(&e, a, 2);
    }
    w16(&e, 10);   /* supported_groups */
    w16(&e, 8); w16(&e, 6);
    w16(&e, GRUPO_X25519); w16(&e, CURVA_P256); w16(&e, CURVA_P384);
    w16(&e, 11);   /* ec_point_formats: só não comprimido */
    w16(&e, 2); w8(&e, 1); w8(&e, 0);
    w16(&e, 13);   /* signature_algorithms */
    {
        static const uint16_t esq[] = { 0x0403, 0x0503, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601 };
        unsigned a = abre(&e, 2), b = abre(&e, 2);
        for (unsigned i = 0; i < sizeof esq / sizeof esq[0]; i++) w16(&e, esq[i]);
        fecha(&e, b, 2); fecha(&e, a, 2);
    }
    if (t->cfg.alpn) {   /* ALPN (RFC 7301) */
        w16(&e, 16);
        unsigned a = abre(&e, 2), b = abre(&e, 2);
        w8(&e, (unsigned)tam_texto(t->cfg.alpn));
        wbytes(&e, t->cfg.alpn, (unsigned)tam_texto(t->cfg.alpn));
        fecha(&e, b, 2); fecha(&e, a, 2);
    }
    if (oferece12(t)) {
        w16(&e, 23); w16(&e, 0);                  /* extended_master_secret (RFC 7627) */
        w16(&e, 0xff01); w16(&e, 1); w8(&e, 0);   /* renegotiation_info vazio: "não renegocio" (RFC 5746) */
    }
    if (oferece13(t)) {
        w16(&e, 43);   /* supported_versions */
        unsigned a = abre(&e, 2);
        w8(&e, oferece12(t) ? 4 : 2);
        w16(&e, TLS13);
        if (oferece12(t)) w16(&e, TLS12);
        fecha(&e, a, 2);
        if (cookie) {
            w16(&e, 44);
            w16(&e, ncookie);
            wbytes(&e, cookie, ncookie);
        }
        w16(&e, 51);   /* key_share: uma chave só (a do grupo pedido no HelloRetryRequest, se houve) */
        unsigned a2 = abre(&e, 2), b2 = abre(&e, 2);
        if (t->ec_grupo) {
            unsigned np = 1 + 2 * ec_tam(t->ec_grupo);
            w16(&e, (unsigned)t->ec_grupo); w16(&e, np); wbytes(&e, t->ec_pub, np);
        } else {
            w16(&e, GRUPO_X25519); w16(&e, 32); wbytes(&e, t->x_pub, 32);
        }
        fecha(&e, b2, 2); fecha(&e, a2, 2);
    }
    fecha(&e, p_ext, 2);
    fecha(&e, p_msg, 3);
    return e.n <= max ? e.n : 0;
}

/* ------------------------------------------------------------ chaves do TLS 1.3 */

/* HKDF-Expand-Label(segredo, rótulo, contexto, n) (RFC 8446, 7.1) */
static void expandir_rotulo(const hash_alg_t *h, const uint8_t *seg, const char *rotulo,
                            const uint8_t *ctx, unsigned nctx, uint8_t *saida, unsigned n)
{
    uint8_t info[2 + 1 + 6 + 32 + 1 + HASH_MAX];
    unsigned nr = (unsigned)tam_texto(rotulo), k = 0;
    info[k++] = (uint8_t)(n >> 8);
    info[k++] = (uint8_t)n;
    info[k++] = (uint8_t)(6 + nr);
    memcpy(info + k, "tls13 ", 6); k += 6;
    memcpy(info + k, rotulo, nr); k += nr;
    info[k++] = (uint8_t)nctx;
    memcpy(info + k, ctx, nctx); k += nctx;
    hkdf_expandir(h, seg, info, k, saida, n);
}

static void derivar(const hash_alg_t *h, const uint8_t *seg, const char *rotulo, const uint8_t *hash_ctx, uint8_t *saida)
{
    expandir_rotulo(h, seg, rotulo, hash_ctx, h->tam, saida, h->tam);
}

static void instalar13(const tls_t *t, cifra_t *c, const uint8_t *seg)
{
    const suite_t *s = t->suite;
    memset(c, 0, sizeof *c);
    c->aead = s->aead;
    expandir_rotulo(s->h, seg, "key", NULL, 0, c->chave, s->chave);
    expandir_rotulo(s->h, seg, "iv", NULL, 0, c->iv, 12);
    if (c->aead == AEAD_GCM) gcm_chave(&c->gcm, c->chave, s->chave);
}

static void finished13(const tls_t *t, const uint8_t *base, const uint8_t *hash_ctx, uint8_t *saida)
{
    const hash_alg_t *h = t->suite->h;
    uint8_t chave[HASH_MAX];
    expandir_rotulo(h, base, "finished", NULL, 0, chave, h->tam);
    hmac(h, chave, h->tam, hash_ctx, h->tam, saida);
    cripto_apagar(chave, sizeof chave);
}

/* ------------------------------------------------------------ certificados */

static int ler_certificados(tls_t *t, leitor_t *l, bool formato13)
{
    if (formato13) {
        unsigned nctx = u8(l);
        bytes(l, nctx);
    }
    leitor_t lista = sub(l, u24(l));
    if (l->ruim || lista.ruim || !vazio(l)) return erro_proto(t, 50, "mensaje Certificate mal formado");
    unsigned total = (unsigned)(lista.fim - lista.p);
    libera(t->certs_der);
    t->certs_der = aloca(total ? total : 1);
    if (!t->certs_der) return falhar(t, TLS_ERRO_MEMORIA, "sin memoria");
    unsigned usado = 0;
    t->ncerts = 0;
    while (!vazio(&lista)) {
        unsigned n = u24(&lista);
        const uint8_t *d = bytes(&lista, n);
        if (formato13) bytes(&lista, u16(&lista));   /* extensões do certificado (OCSP, SCT): ignoradas */
        if (lista.ruim || !d || !n) return erro_proto(t, 50, "mensaje Certificate mal formado");
        if (t->ncerts == CERTS_MAX) continue;   /* cadeia longa: o resto não é usado */
        memcpy(t->certs_der + usado, d, n);
        int r = x509_ler(&t->certs[t->ncerts], t->certs_der + usado, n);
        if (r != X509_OK && t->ncerts == 0) {
            t->validacao = r;
            if (!t->cfg.inseguro || r == X509_FORMATO) {
                mandar_alerta(t, 42);
                return falhar(t, TLS_ERRO_CERT, "certificado del servidor: %s", x509_erro_texto(r));
            }
        }
        if (r == X509_OK || r == X509_CRITICA) t->ncerts++;
        usado += n;
    }
    if (!t->ncerts) return erro_proto(t, 50, "el servidor no mandó certificado");
    int raiz = -1;
    int v = x509_validar(t->certs, t->ncerts, t->cfg.raizes, t->host, data_agora(), &raiz);
    if (t->validacao == X509_OK) t->validacao = v;
    t->raiz = raiz;
    if (t->validacao != X509_OK && !t->cfg.inseguro) {
        unsigned a = t->validacao == X509_CADUCADO ? 45 : t->validacao == X509_EMISSOR ? 48 : 42;
        mandar_alerta(t, a);
        return falhar(t, TLS_ERRO_CERT, "%s", x509_erro_texto(t->validacao));
    }
    return 1;
}

/* ------------------------------------------------------------ troca de chaves */

static bool segredo_ecdhe(tls_t *t, int grupo, const uint8_t *pub, unsigned n, uint8_t *seg, unsigned *nseg)
{
    if (grupo == GRUPO_X25519) {
        if (n != 32) return false;
        x25519(seg, t->x_priv, pub);
        uint8_t zero = 0;
        for (int i = 0; i < 32; i++) zero |= seg[i];
        *nseg = 32;
        return zero != 0;   /* ponto de ordem pequena: segredo nulo (RFC 7748, 6.1) */
    }
    if (grupo != t->ec_grupo) return false;
    *nseg = ec_tam(grupo);
    return ecdh_segredo(grupo, t->ec_priv, pub, n, seg);
}

/* ------------------------------------------------------------ TLS 1.3 */

static int handshake13(tls_t *t, const uint8_t *chave_srv, unsigned nchave)
{
    const hash_alg_t *h = t->suite->h;
    unsigned hl = h->tam;
    uint8_t seg[48], zeros[HASH_MAX], prk[HASH_MAX], deriv[HASH_MAX], th[HASH_MAX], vazio_h[HASH_MAX];
    unsigned nseg;
    if (!segredo_ecdhe(t, t->grupo, chave_srv, nchave, seg, &nseg)) return erro_proto(t, 47, "clave efímera del servidor no válida");
    memset(zeros, 0, sizeof zeros);
    hash_calcular(h, "", 0, vazio_h);
    /* early_secret = Extract(0, 0); handshake_secret = Extract(Derive(early, "derived"), ECDHE) */
    hkdf_extrair(h, NULL, 0, zeros, hl, prk);
    derivar(h, prk, "derived", vazio_h, deriv);
    hkdf_extrair(h, deriv, hl, seg, nseg, prk);
    cripto_apagar(seg, sizeof seg);
    transcrito_hash(t, th);
    derivar(h, prk, "c hs traffic", th, t->c_hs);
    derivar(h, prk, "s hs traffic", th, t->s_hs);
    derivar(h, prk, "derived", vazio_h, deriv);
    uint8_t mestre[HASH_MAX];
    hkdf_extrair(h, deriv, hl, zeros, hl, mestre);
    cripto_apagar(prk, sizeof prk);
    instalar13(t, &t->le, t->s_hs);

    leitor_t l;
    int r = esperar_msg(t, HS_EE, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba EncryptedExtensions") : r;
    leitor_t exts = sub(&l, u16(&l));
    while (!exts.ruim && !vazio(&exts)) {
        unsigned tipo = u16(&exts);
        leitor_t v = sub(&exts, u16(&exts));
        if (tipo == 16) {   /* ALPN escolhido */
            leitor_t lista = sub(&v, u16(&v));
            unsigned n = u8(&lista);
            const uint8_t *p = bytes(&lista, n);
            if (p && n < sizeof t->alpn) { memcpy(t->alpn, p, n); t->alpn[n] = 0; }
        }
    }
    if (exts.ruim || l.ruim) return erro_proto(t, 50, "EncryptedExtensions mal formado");

    r = esperar_msg(t, HS_CREQ, &l, NULL, NULL);
    if (r == 1) t->pediu_cert = true;
    else if (r != OUTRA_MSG) return r;
    r = esperar_msg(t, HS_CERT, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba el certificado del servidor") : r;
    r = ler_certificados(t, &l, true);
    if (r <= 0) return r;

    /* CertificateVerify: assinatura sobre a transcrição até o Certificate */
    transcrito_hash(t, th);
    r = esperar_msg(t, HS_CV, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba CertificateVerify") : r;
    unsigned esq = u16(&l);
    unsigned na = u16(&l);
    const uint8_t *assin = bytes(&l, na);
    if (!assin || !vazio(&l)) return erro_proto(t, 50, "CertificateVerify mal formado");
    static uint8_t conteudo[64 + 34 + HASH_MAX];
    memset(conteudo, 0x20, 64);
    memcpy(conteudo + 64, "TLS 1.3, server CertificateVerify", 34);   /* com o zero final */
    memcpy(conteudo + 98, th, hl);
    int alg = esquema_tls((uint16_t)esq);
    /* No 1.3 o PKCS#1 v1.5 não vale para o handshake (4.2.3) */
    if (!alg || alg == ASSIN_RSA_SHA256 || alg == ASSIN_RSA_SHA384 || alg == ASSIN_RSA_SHA512)
        return erro_proto(t, 47, "esquema de firma no permitido en TLS 1.3");
    if (!x509_verificar(&t->certs[0], alg, conteudo, 98 + hl, assin, na)) {
        mandar_alerta(t, 51);
        return falhar(t, TLS_ERRO_CRIPTO, "la firma del servidor (CertificateVerify) no es válida");
    }

    /* Finished do servidor */
    uint8_t esperado[HASH_MAX];
    transcrito_hash(t, th);
    finished13(t, t->s_hs, th, esperado);
    r = esperar_msg(t, HS_FIN, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba Finished") : r;
    const uint8_t *vd = bytes(&l, hl);
    if (!vd || !vazio(&l) || !cripto_igual(vd, esperado, hl)) {
        mandar_alerta(t, 51);
        return falhar(t, TLS_ERRO_CRIPTO, "el Finished del servidor no coincide");
    }
    /* segredos de aplicação: transcrição até o Finished do servidor */
    transcrito_hash(t, th);
    derivar(h, mestre, "c ap traffic", th, t->c_ap);
    derivar(h, mestre, "s ap traffic", th, t->s_ap);
    cripto_apagar(mestre, sizeof mestre);

    /* nossa parte: CCS de compatibilidade, [Certificate vazio], Finished */
    static const uint8_t ccs = 1;
    enviar_registro(t, CT_CCS, &ccs, 1);
    instalar13(t, &t->escreve, t->c_hs);
    if (t->pediu_cert) {
        uint8_t cv[8] = { HS_CERT, 0, 0, 4, 0, 0, 0, 0 };   /* contexto vazio, lista vazia */
        if (enviar_hs(t, cv, 8) < 0) return TLS_ERRO_REDE;
    }
    uint8_t fin[4 + HASH_MAX];
    transcrito_hash(t, th);
    fin[0] = HS_FIN; fin[1] = 0; fin[2] = 0; fin[3] = (uint8_t)hl;
    finished13(t, t->c_hs, th, fin + 4);
    if (enviar_hs(t, fin, 4 + hl) < 0) return TLS_ERRO_REDE;
    instalar13(t, &t->le, t->s_ap);
    instalar13(t, &t->escreve, t->c_ap);
    return 1;
}

/* ------------------------------------------------------------ TLS 1.2 */

static void instalar12(tls_t *t)
{
    const suite_t *s = t->suite;
    unsigned nk = s->chave, niv = s->aead == AEAD_GCM ? 4 : 12;
    uint8_t bloco[2 * 32 + 2 * 12], semente[64];
    memcpy(semente, t->srv_random, 32);
    memcpy(semente + 32, t->cli_random, 32);
    tls12_prf(s->h, t->mestre12, 48, "key expansion", semente, 64, bloco, 2 * nk + 2 * niv);
    cifra_t *c = &t->escreve, *sv = &t->le_prox;
    memset(c, 0, sizeof *c);
    memset(sv, 0, sizeof *sv);
    c->aead = sv->aead = s->aead;
    memcpy(c->chave, bloco, nk);
    memcpy(sv->chave, bloco + nk, nk);
    memcpy(c->iv, bloco + 2 * nk, niv);
    memcpy(sv->iv, bloco + 2 * nk + niv, niv);
    if (s->aead == AEAD_GCM) {
        gcm_chave(&c->gcm, c->chave, nk);
        gcm_chave(&sv->gcm, sv->chave, nk);
    }
    t->tem_le_prox = true;
    cripto_apagar(bloco, sizeof bloco);
}

static int handshake12(tls_t *t)
{
    leitor_t l;
    int r = esperar_msg(t, HS_CERT, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba el certificado del servidor") : r;
    r = ler_certificados(t, &l, false);
    if (r <= 0) return r;

    /* ServerKeyExchange: curva, chave efêmera e assinatura sobre os dois aleatórios + parâmetros */
    r = esperar_msg(t, HS_SKE, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba ServerKeyExchange (solo ECDHE)") : r;
    const uint8_t *params = l.p;
    if (u8(&l) != 3) return erro_proto(t, 47, "solo se aceptan curvas con nombre");
    t->grupo = (int)u16(&l);
    unsigned npub = u8(&l);
    const uint8_t *pub = bytes(&l, npub);
    unsigned nparams = (unsigned)(l.p - params);
    unsigned esq = u16(&l), na = u16(&l);
    const uint8_t *assin = bytes(&l, na);
    if (!pub || !assin || !vazio(&l)) return erro_proto(t, 50, "ServerKeyExchange mal formado");
    int alg = esquema_tls((uint16_t)esq);
    if (!alg) return erro_proto(t, 47, "esquema de firma no soportado");
    static uint8_t assinado[64 + 4 + 256];
    if (nparams > sizeof assinado - 64) return erro_proto(t, 50, "ServerKeyExchange demasiado grande");
    memcpy(assinado, t->cli_random, 32);
    memcpy(assinado + 32, t->srv_random, 32);
    memcpy(assinado + 64, params, nparams);
    /* a suíte diz o tipo de chave: ECDSA para ...ECDSA..., RSA para ...RSA... */
    bool suite_ecdsa = t->suite->id == 0xcca9 || t->suite->id == 0xc02b || t->suite->id == 0xc02c;
    bool alg_ecdsa = alg >= ASSIN_ECDSA_SHA256 && alg <= ASSIN_ECDSA_SHA512;
    if (suite_ecdsa != alg_ecdsa) return erro_proto(t, 47, "firma de tipo distinto del de la suite");
    if (!x509_verificar(&t->certs[0], alg, assinado, 64 + nparams, assin, na)) {
        mandar_alerta(t, 51);
        return falhar(t, TLS_ERRO_CRIPTO, "la firma del servidor (ServerKeyExchange) no es válida");
    }
    /* nossa chave efêmera para o grupo escolhido */
    uint8_t cke[4 + 1 + 97];
    unsigned ncke;
    if (t->grupo == GRUPO_X25519) {
        memcpy(cke + 5, t->x_pub, 32);
        ncke = 32;
    } else if (t->grupo == CURVA_P256 || t->grupo == CURVA_P384) {
        if (t->ec_grupo != t->grupo) {
            t->ec_grupo = t->grupo;
            if (!ecdh_gerar(t->grupo, t->ec_priv, t->ec_pub)) return falhar(t, TLS_ERRO_CRIPTO, "error al generar la clave");
        }
        ncke = 1 + 2 * ec_tam(t->grupo);
        memcpy(cke + 5, t->ec_pub, ncke);
    } else return erro_proto(t, 47, "curva no soportada");
    uint8_t pms[48];
    unsigned npms;
    if (!segredo_ecdhe(t, t->grupo, pub, npub, pms, &npms)) return erro_proto(t, 47, "clave efímera del servidor no válida");

    r = esperar_msg(t, HS_CREQ, &l, NULL, NULL);
    if (r == 1) t->pediu_cert = true;
    else if (r != OUTRA_MSG) return r;
    r = esperar_msg(t, HS_SHD, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba ServerHelloDone") : r;

    if (t->pediu_cert) {   /* sem certificado de cliente: lista vazia */
        uint8_t cv[7] = { HS_CERT, 0, 0, 3, 0, 0, 0 };
        if (enviar_hs(t, cv, 7) < 0) return TLS_ERRO_REDE;
    }
    cke[0] = HS_CKE; cke[1] = 0; cke[2] = 0; cke[3] = (uint8_t)(ncke + 1);
    cke[4] = (uint8_t)ncke;
    if (enviar_hs(t, cke, 5 + ncke) < 0) return TLS_ERRO_REDE;

    /* segredo mestre: estendido (hash da sessão) se o servidor aceitou o RFC 7627 */
    const hash_alg_t *h = t->suite->h;
    uint8_t th[HASH_MAX];
    if (t->ems) {
        transcrito_hash(t, th);
        tls12_prf(h, pms, npms, "extended master secret", th, h->tam, t->mestre12, 48);
    } else {
        uint8_t semente[64];
        memcpy(semente, t->cli_random, 32);
        memcpy(semente + 32, t->srv_random, 32);
        tls12_prf(h, pms, npms, "master secret", semente, 64, t->mestre12, 48);
    }
    cripto_apagar(pms, sizeof pms);
    instalar12(t);
    static const uint8_t ccs = 1;
    enviar_registro(t, CT_CCS, &ccs, 1);   /* o CCS nunca é cifrado; o Finished já vai com a chave nova */
    uint8_t fin[4 + 12];
    transcrito_hash(t, th);
    fin[0] = HS_FIN; fin[1] = 0; fin[2] = 0; fin[3] = 12;
    tls12_prf(h, t->mestre12, 48, "client finished", th, h->tam, fin + 4, 12);
    if (enviar_hs(t, fin, 16) < 0) return TLS_ERRO_REDE;

    /* CCS + Finished do servidor (proxima_mensagem troca a chave no CCS) */
    uint8_t esperado[12];
    transcrito_hash(t, th);
    tls12_prf(h, t->mestre12, 48, "server finished", th, h->tam, esperado, 12);
    r = esperar_msg(t, HS_FIN, &l, NULL, NULL);
    if (r != 1) return r == OUTRA_MSG ? erro_proto(t, 10, "se esperaba Finished") : r;
    if (t->tem_le_prox) return erro_proto(t, 10, "Finished sin ChangeCipherSpec");
    const uint8_t *vd = bytes(&l, 12);
    if (!vd || !vazio(&l) || !cripto_igual(vd, esperado, 12)) {
        mandar_alerta(t, 51);
        return falhar(t, TLS_ERRO_CRIPTO, "el Finished del servidor no coincide");
    }
    return 1;
}

/* ------------------------------------------------------------ início */

static const uint8_t HRR_RANDOM[32] = {   /* SHA-256("HelloRetryRequest") (RFC 8446, 4.1.3) */
    0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
    0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C };

tls_t *tls_novo(const tls_config_t *cfg, int fd)
{
    tls_t *t = aloca_zerado(sizeof(tls_t));
    if (!t) return NULL;
    t->cfg = *cfg;
    if (!t->cfg.versao_min) t->cfg.versao_min = TLS12;
    if (!t->cfg.versao_max) t->cfg.versao_max = TLS13;
    copia_texto(t->host, cfg->host ? cfg->host : "", sizeof t->host);
    t->fd = fd;
    t->raiz = -1;
    t->cap_hs = 16384;
    t->hs = aloca(t->cap_hs);
    if (!t->hs) { libera(t); return NULL; }
    return t;
}

int tls_conectar(tls_t *t)
{
    unsigned inicio = tempo_ms();
    cripto_aleatorio(t->cli_random, 32);
    cripto_aleatorio(t->sessao, 32);
    cripto_aleatorio(t->x_priv, 32);
    x25519_base(t->x_pub, t->x_priv);
    sha256_iniciar(&t->t256);
    sha384_iniciar(&t->t384);
    static uint8_t ch[1024];
    unsigned nch = montar_client_hello(t, ch, sizeof ch, NULL, 0);
    if (!nch) return falhar(t, TLS_ERRO_PROTOCOLO, "ClientHello demasiado grande");
    if (enviar_hs(t, ch, nch) < 0) return TLS_ERRO_REDE;
    bool houve_hrr = false;
    const suite_t *suite_hrr = NULL;
    for (;;) {
        uint8_t *m;
        unsigned nm;
        int r = proxima_mensagem(t, &m, &nm);
        if (r <= 0) return r;
        if (m[0] != HS_SH) return erro_proto(t, 10, "se esperaba ServerHello");
        leitor_t l = leitor(m + 4, nm - 4);
        unsigned versao = u16(&l);
        const uint8_t *rnd = bytes(&l, 32);
        unsigned nsess = u8(&l);
        const uint8_t *sess = bytes(&l, nsess);
        unsigned suite = u16(&l), comp = u8(&l);
        leitor_t exts = { NULL, NULL, false };
        if (!vazio(&l)) exts = sub(&l, u16(&l));
        if (l.ruim || !vazio(&l) || !rnd || comp != 0) return erro_proto(t, 50, "ServerHello mal formado");
        bool hrr = memcmp(rnd, HRR_RANDOM, 32) == 0;
        unsigned ver_sel = 0, grupo = 0, nchave = 0, ncookie = 0;
        const uint8_t *chave = NULL, *cookie = NULL;
        bool ri = false, ems = false;
        while (exts.p && !vazio(&exts) && !exts.ruim) {
            unsigned tipo = u16(&exts);
            leitor_t v = sub(&exts, u16(&exts));
            if (tipo == 43) ver_sel = u16(&v);
            else if (tipo == 51) {
                grupo = u16(&v);
                if (!hrr) { nchave = u16(&v); chave = bytes(&v, nchave); }
            } else if (tipo == 44) { ncookie = u16(&v); cookie = bytes(&v, ncookie); }
            else if (tipo == 23) ems = true;
            else if (tipo == 0xff01) { ri = true; if (u8(&v) != 0) return erro_proto(t, 40, "renegociación no soportada"); }
            else if (tipo == 16) {
                leitor_t lista = sub(&v, u16(&v));
                unsigned n = u8(&lista);
                const uint8_t *p = bytes(&lista, n);
                if (p && n < sizeof t->alpn) { memcpy(t->alpn, p, n); t->alpn[n] = 0; }
            }
            if (v.ruim) return erro_proto(t, 50, "extensión del ServerHello mal formada");
        }
        if (exts.ruim) return erro_proto(t, 50, "ServerHello mal formado");
        t->versao = ver_sel ? (int)ver_sel : (int)versao;
        if (t->versao != TLS13 && t->versao != TLS12)
            return erro_proto(t, 70, "el servidor solo habla versiones antiguas de TLS/SSL (no soportadas)");
        if (t->versao < t->cfg.versao_min || t->versao > t->cfg.versao_max)
            return erro_proto(t, 70, "el servidor eligió una versión de TLS que no se ofreció");
        t->suite = NULL;
        for (int i = 0; i < NSUITES; i++)
            if (SUITES[i].id == suite && SUITES[i].tls13 == (t->versao == TLS13)) t->suite = &SUITES[i];
        if (!t->suite) return erro_proto(t, 47, "el servidor eligió una suite no ofrecida");
        if (hrr) {
            /* HelloRetryRequest: outro grupo (P-256/P-384); a transcrição
             * começa de novo com o hash do primeiro ClientHello (4.4.1) */
            if (houve_hrr || t->versao != TLS13 || (grupo != CURVA_P256 && grupo != CURVA_P384))
                return erro_proto(t, 47, "HelloRetryRequest no válido");
            houve_hrr = true;
            suite_hrr = t->suite;
            uint8_t h1[HASH_MAX], mh[4 + HASH_MAX];
            const hash_alg_t *h = t->suite->h;
            transcrito_hash(t, h1);
            mh[0] = HS_MSG_HASH; mh[1] = 0; mh[2] = 0; mh[3] = (uint8_t)h->tam;
            memcpy(mh + 4, h1, h->tam);
            sha256_iniciar(&t->t256);
            sha384_iniciar(&t->t384);
            transcrito_somar(t, mh, 4 + h->tam);
            transcrito_somar(t, m, nm);
            t->ec_grupo = (int)grupo;
            if (!ecdh_gerar(t->ec_grupo, t->ec_priv, t->ec_pub)) return falhar(t, TLS_ERRO_CRIPTO, "error al generar la clave");
            static uint8_t cookie_copia[512];
            if (ncookie > sizeof cookie_copia) return erro_proto(t, 50, "cookie demasiado grande");
            if (cookie) memcpy(cookie_copia, cookie, ncookie);
            nch = montar_client_hello(t, ch, sizeof ch, cookie ? cookie_copia : NULL, ncookie);
            if (!nch) return falhar(t, TLS_ERRO_PROTOCOLO, "ClientHello demasiado grande");
            if (enviar_hs(t, ch, nch) < 0) return TLS_ERRO_REDE;
            continue;
        }
        if (suite_hrr && t->suite != suite_hrr) return erro_proto(t, 47, "la suite cambió después del HelloRetryRequest");
        transcrito_somar(t, m, nm);
        memcpy(t->srv_random, rnd, 32);
        if (t->versao == TLS13) {
            if (nsess != 32 || memcmp(sess, t->sessao, 32) != 0) return erro_proto(t, 47, "legacy_session_id distinto");
            if (!chave) return erro_proto(t, 109, "el servidor no mandó su clave (key_share)");
            t->grupo = (int)grupo;
            if (grupo != GRUPO_X25519 && grupo != (unsigned)t->ec_grupo) return erro_proto(t, 47, "grupo no ofrecido");
            static uint8_t chave_copia[100];
            if (nchave > sizeof chave_copia) return erro_proto(t, 47, "clave del servidor demasiado grande");
            memcpy(chave_copia, chave, nchave);
            r = handshake13(t, chave_copia, nchave);
        } else {
            /* Proteção contra rebaixamento: um servidor TLS 1.3 que responde
             * 1.2 a quem ofereceu 1.3 marca o aleatório (4.1.3). */
            static const uint8_t DOWNGRD[7] = { 0x44, 0x4F, 0x57, 0x4E, 0x47, 0x52, 0x44 };
            if (oferece13(t) && memcmp(rnd + 24, DOWNGRD, 7) == 0 && (rnd[31] == 1 || rnd[31] == 0))
                return erro_proto(t, 47, "intento de rebajar la versión de TLS detectado");
            t->ems = ems;
            (void)ri;
            r = handshake12(t);
        }
        if (r <= 0) return r;
        break;
    }
    t->conectado = true;
    t->ms = tempo_ms() - inicio;
    cripto_apagar(t->x_priv, sizeof t->x_priv);
    cripto_apagar(t->ec_priv, sizeof t->ec_priv);
    return 0;
}

/* ------------------------------------------------------------ dados */

/* Mensagens de handshake depois da conexão (TLS 1.3): tickets e KeyUpdate. */
static int pos_handshake(tls_t *t)
{
    while (t->nhs - t->ini_hs >= 4) {
        uint8_t *m = t->hs + t->ini_hs;
        unsigned tam = be24(m + 1);
        if (t->nhs - t->ini_hs < 4 + tam) break;
        t->ini_hs += 4 + tam;
        if (t->versao != TLS13) {
            if (m[0] == 0 && tam == 0) continue;   /* HelloRequest: não renegociamos, só ignoramos (RFC 5246, 7.4.1.1) */
            return erro_proto(t, 10, "renegociación no soportada");
        }
        if (m[0] == HS_TICKET) continue;   /* sem retomada de sessão: ignora */
        if (m[0] == HS_KEY_UPDATE && tam == 1) {
            const hash_alg_t *h = t->suite->h;
            uint8_t novo[HASH_MAX];
            expandir_rotulo(h, t->s_ap, "traffic upd", NULL, 0, novo, h->tam);
            memcpy(t->s_ap, novo, h->tam);
            instalar13(t, &t->le, t->s_ap);
            if (m[4] == 1) {   /* o servidor pede que troquemos também */
                uint8_t ku[5] = { HS_KEY_UPDATE, 0, 0, 1, 0 };
                if (enviar_registro(t, CT_HS, ku, 5) < 0) return TLS_ERRO_REDE;
                expandir_rotulo(h, t->c_ap, "traffic upd", NULL, 0, novo, h->tam);
                memcpy(t->c_ap, novo, h->tam);
                instalar13(t, &t->escreve, t->c_ap);
            }
            continue;
        }
        return erro_proto(t, 10, "mensaje de negociación inesperado");
    }
    if (t->ini_hs == t->nhs) t->ini_hs = t->nhs = 0;
    return 1;
}

int tls_ler(tls_t *t, void *buf, unsigned n)
{
    if (!t->conectado) return falhar(t, TLS_ERRO_PROTOCOLO, "sin conexión TLS");
    while (t->pos >= t->ndados) {
        if (t->fim) return t->erro[0] ? TLS_ERRO_REDE : 0;
        int tipo;
        uint8_t *d;
        unsigned k;
        int r = ler_registro(t, &tipo, &d, &k);
        if (r == 0) { t->fim = true; return 0; }   /* TCP fechou sem close_notify: o HTTP sabe se faltou algo */
        if (r < 0) return r;
        if (tipo == CT_DADOS) { t->dados = d; t->ndados = k; t->pos = 0; continue; }
        if (tipo == CT_ALERTA) { r = tratar_alerta(t, d, k); if (r < 0) return r; return 0; }
        if (tipo == CT_HS) {
            if (t->nhs + k > t->cap_hs) {
                uint8_t *novo = realoca(t->hs, t->nhs + k);
                if (!novo) return falhar(t, TLS_ERRO_MEMORIA, "sin memoria");
                t->hs = novo;
                t->cap_hs = t->nhs + k;
            }
            memcpy(t->hs + t->nhs, d, k);
            t->nhs += k;
            r = pos_handshake(t);
            if (r < 0) return r;
            continue;
        }
        return erro_proto(t, 10, "registro inesperado");
    }
    unsigned k = t->ndados - t->pos;
    if (k > n) k = n;
    memcpy(buf, t->dados + t->pos, k);
    t->pos += k;
    return (int)k;
}

int tls_escrever(tls_t *t, const void *buf, unsigned n)
{
    if (!t->conectado || t->fim) return falhar(t, TLS_ERRO_REDE, "conexión cerrada");
    int r = enviar_registro(t, CT_DADOS, buf, n);
    return r < 0 ? r : (int)n;
}

void tls_fechar(tls_t *t)
{
    if (!t) return;
    if (t->conectado && !t->fim) mandar_alerta(t, 0);
    libera(t->hs);
    libera(t->certs_der);
    cripto_apagar(t, sizeof *t);
    libera(t);
}

const char *tls_erro(const tls_t *t) { return t->erro[0] ? t->erro : "error desconocido"; }
const char *tls_versao_texto(const tls_t *t) { return t->versao == TLS13 ? "TLSv1.3" : t->versao == TLS12 ? "TLSv1.2" : "?"; }
const char *tls_suite_texto(const tls_t *t) { return t->suite ? t->suite->nome : "?"; }
const char *tls_grupo_texto(const tls_t *t)
{
    return t->grupo == GRUPO_X25519 ? "X25519" : t->grupo == CURVA_P256 ? "P-256" : t->grupo == CURVA_P384 ? "P-384" : "?";
}
const char *tls_alpn(const tls_t *t) { return t->alpn; }
int tls_cadeia(const tls_t *t, const cert_t **certs) { *certs = t->certs; return t->ncerts; }
int tls_validacao(const tls_t *t) { return t->validacao; }
int tls_raiz(const tls_t *t) { return t->raiz; }
unsigned tls_ms_handshake(const tls_t *t) { return t->ms; }
/* Texto claro já decifrado que tls_ler entrega sem ler o socket (o select
 * do python precisa saber disso, D069). */
int tls_pendente(const tls_t *t) { return t->pos < t->ndados ? (int)(t->ndados - t->pos) : 0; }
