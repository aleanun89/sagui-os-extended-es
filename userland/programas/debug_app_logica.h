// SPDX-License-Identifier: GPL-2.0-or-later
/* debug_app_logica.h - la parte de DEBUG-APP (D362) que no toca el sistema:
 * el código que se dicta o se copia al otro equipo, la clave de un solo uso
 * que sale de su secreto y las direcciones a las que se deja entrar.
 *
 * El código (versión 1): 25 bytes en base32 de Crockford (sin I, L, O ni U;
 * al leer, I y L valen 1 y O vale 0, y se ignoran mayúsculas, guiones y
 * espacios), 40 letras en 8 grupos de 5:
 *
 *   versión (1) | IP (4) | puerto (2) | secreto (8) | huella (8) | suma (2)
 *
 * - IP y puerto, en orden de red: dónde escucha la sesión;
 * - secreto: 64 bits del generador aleatorio del kernel. Nunca viaja por la
 *   red: de él sale la semilla de una clave Ed25519 de cliente
 *   (SHA-256(DAPP_ROTULO | secreto)), y el servidor solo conoce la
 *   pública. Se quema en la primera entrada (y con DAPP_FALLOS_MAX intentos
 *   fallidos), así que un atacante tiene muy pocas pruebas en 2^64;
 * - huella: 8 bytes que atan la clave del servidor al secreto
 *   (dapp_huella: SHA-256(DAPP_ROTULO_HUELLA | secreto | SHA-256(clave))).
 *   El cliente la recalcula con la clave que le enseña el servidor y la
 *   compara antes de entregar nada, contra un intermediario en la red. Con
 *   llave (v2) y no el principio de «SHA256:...» (v1): la clave del servidor
 *   es la de siempre y cualquiera la lee con ssh-keyscan, así que con v1 un
 *   atacante podía pasar meses fabricando una clave con los mismos 64 bits y
 *   usarla contra todas las sesiones; con v2 no sabe qué valor buscar sin el
 *   secreto, y cada intento suyo es una conexión que el cliente aborta;
 * - suma: los 2 primeros bytes del SHA-256 de lo anterior. Una letra mal
 *   copiada se detecta (falla 1 de cada 65536 veces) antes de conectar.
 *
 * tools/depurar-remoto.py implementa lo mismo en Python; tests/h364 compara
 * los dos (tests/debug_app_unidad.c en el anfitrión). */
#ifndef DEBUG_APP_LOGICA_H
#define DEBUG_APP_LOGICA_H
#include "sagui.h"
#include "sha256.h"

#define DAPP_VERSAO      2          /* v2: huella con llave (dapp_huella) */
#define DAPP_SEGREDO     8
#define DAPP_HUELLA      8
#define DAPP_CARGA       (1 + 4 + 2 + DAPP_SEGREDO + DAPP_HUELLA)   /* 23 */
#define DAPP_BYTES       (DAPP_CARGA + 2)                           /* 25 = 200 bits = 40 letras */
#define DAPP_LETRAS      40
#define DAPP_GRUPO       5
#define DAPP_CODIGO_MAX  (DAPP_LETRAS + DAPP_LETRAS / DAPP_GRUPO)   /* con los guiones y el cero */
#define DAPP_ROTULO      "SaguiOS DEBUG-APP v1"          /* la semilla (no cambió en v2) */
#define DAPP_ROTULO_HUELLA "SaguiOS DEBUG-APP huella v2"
#define DAPP_FALLOS_MAX  3          /* conexiones con una clave mala antes de anular el código */
#define DAPP_PUERTO      2222       /* de serie; si está ocupado, los 9 siguientes */
#define DAPP_MIN_MIN     15         /* duración: de 15 minutos... */
#define DAPP_MAX_MIN     480        /* ...a 8 horas */
#define DAPP_DEF_MIN     120

/* Errores de dapp_decodificar */
#define DAPP_MAL_LETRA   (-1)       /* una letra que no es del alfabeto, o no son 40 */
#define DAPP_MAL_SUMA    (-2)       /* alguna letra está mal copiada */
#define DAPP_MAL_VERSAO  (-3)       /* de otra versión de DEBUG-APP */

typedef struct {
    uint32_t ip;
    unsigned porta;
    uint8_t  segredo[DAPP_SEGREDO];
    uint8_t  huella[DAPP_HUELLA];
} dapp_codigo_t;

static const char DAPP_ALFABETO[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static inline int dapp_valor_letra(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c == 'I' || c == 'L') return 1;
    if (c == 'O') return 0;
    for (int i = 0; i < 32; i++)
        if (DAPP_ALFABETO[i] == c) return i;
    return -1;
}

static inline void dapp_suma(const uint8_t carga[DAPP_CARGA], uint8_t suma[2])
{
    unsigned char h[32];
    sha256(carga, DAPP_CARGA, h);
    suma[0] = h[0];
    suma[1] = h[1];
}

static inline void dapp_empacotar(const dapp_codigo_t *c, uint8_t b[DAPP_BYTES])
{
    b[0] = DAPP_VERSAO;
    b[1] = (uint8_t)(c->ip >> 24); b[2] = (uint8_t)(c->ip >> 16); b[3] = (uint8_t)(c->ip >> 8); b[4] = (uint8_t)c->ip;
    b[5] = (uint8_t)(c->porta >> 8); b[6] = (uint8_t)c->porta;
    memcpy(b + 7, c->segredo, DAPP_SEGREDO);
    memcpy(b + 7 + DAPP_SEGREDO, c->huella, DAPP_HUELLA);
    dapp_suma(b, b + DAPP_CARGA);
}

/* «XXXXX-XXXXX-...» (47 letras y el cero). false si no cabe. */
static inline bool dapp_codificar(const dapp_codigo_t *c, char *dst, unsigned max)
{
    if (max < DAPP_CODIGO_MAX) return false;
    uint8_t b[DAPP_BYTES];
    dapp_empacotar(c, b);
    unsigned acc = 0, nbits = 0, k = 0, letras = 0;
    for (unsigned i = 0; i < DAPP_BYTES; i++) {
        acc = (acc << 8) | b[i];
        nbits += 8;
        while (nbits >= 5) {
            if (letras && letras % DAPP_GRUPO == 0) dst[k++] = '-';
            dst[k++] = DAPP_ALFABETO[(acc >> (nbits - 5)) & 31];
            nbits -= 5;
            letras++;
        }
    }
    dst[k] = 0;
    memset(b, 0, sizeof b);   /* lleva el secreto */
    return true;
}

/* 0 o DAPP_MAL_*. Admite el código con o sin guiones y espacios. */
static inline int dapp_decodificar(const char *t, dapp_codigo_t *c)
{
    uint8_t b[DAPP_BYTES];
    unsigned acc = 0, nbits = 0, n = 0, letras = 0;
    for (; *t; t++) {
        if (*t == '-' || *t == ' ' || *t == '\t' || *t == '\r' || *t == '\n') continue;
        int v = dapp_valor_letra(*t);
        if (v < 0 || letras >= DAPP_LETRAS) return DAPP_MAL_LETRA;
        letras++;
        acc = (acc << 5) | (unsigned)v;
        nbits += 5;
        if (nbits >= 8) {
            b[n++] = (uint8_t)(acc >> (nbits - 8));
            nbits -= 8;
        }
    }
    if (letras != DAPP_LETRAS) return DAPP_MAL_LETRA;
    uint8_t s[2];
    dapp_suma(b, s);
    if (s[0] != b[DAPP_CARGA] || s[1] != b[DAPP_CARGA + 1]) return DAPP_MAL_SUMA;
    if (b[0] != DAPP_VERSAO) return DAPP_MAL_VERSAO;
    c->ip = (uint32_t)b[1] << 24 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 8 | b[4];
    c->porta = (unsigned)b[5] << 8 | b[6];
    memcpy(c->segredo, b + 7, DAPP_SEGREDO);
    memcpy(c->huella, b + 7 + DAPP_SEGREDO, DAPP_HUELLA);
    memset(b, 0, sizeof b);
    return 0;
}

/* La semilla Ed25519 de la clave del cliente: SHA-256(DAPP_ROTULO | secreto).
 * Solo del secreto (no de la huella): así debug-app puede dar la clave pública
 * al sshd antes de que este cargue la clave del servidor y diga su huella. */
static inline void dapp_semente(const uint8_t segredo[DAPP_SEGREDO], uint8_t semente[32])
{
    uint8_t d[sizeof DAPP_ROTULO - 1 + DAPP_SEGREDO];
    memcpy(d, DAPP_ROTULO, sizeof DAPP_ROTULO - 1);
    memcpy(d + sizeof DAPP_ROTULO - 1, segredo, DAPP_SEGREDO);
    sha256(d, sizeof d, semente);
    memset(d, 0, sizeof d);
}

/* La huella del código: SHA-256(DAPP_ROTULO_HUELLA | secreto | hash)[0..8),
 * con hash = SHA-256 del blob de la clave del servidor (lo que dice «sshd
 * --depuracion» en «@fp»). La calcula debug-app, que tiene el secreto; el
 * sshd no lo conoce. */
static inline void dapp_huella(const uint8_t segredo[DAPP_SEGREDO], const uint8_t hash_chave[32], uint8_t huella[DAPP_HUELLA])
{
    uint8_t d[sizeof DAPP_ROTULO_HUELLA - 1 + DAPP_SEGREDO + 32], h[32];
    memcpy(d, DAPP_ROTULO_HUELLA, sizeof DAPP_ROTULO_HUELLA - 1);
    memcpy(d + sizeof DAPP_ROTULO_HUELLA - 1, segredo, DAPP_SEGREDO);
    memcpy(d + sizeof DAPP_ROTULO_HUELLA - 1 + DAPP_SEGREDO, hash_chave, 32);
    sha256(d, sizeof d, h);
    memcpy(huella, h, DAPP_HUELLA);
    memset(d, 0, sizeof d);
    memset(h, 0, sizeof h);
}

/* ------------------------------------------------------------ direcciones permitidas */

/* Privadas: RFC 1918 y enlace local (169.254/16). Ni 127/8 (nadie de fuera
 * llega desde ahí) ni 100.64/10 (CGNAT: pueden ser otros clientes del
 * proveedor); para esas, «publicas». */
static inline bool dapp_ip_privada(uint32_t ip)
{
    return (ip >> 24) == 10 || (ip >> 20) == (172u << 4 | 1) || (ip >> 16) == (192u << 8 | 168) ||
           (ip >> 16) == (169u << 8 | 254);
}

/* ¿Toda la red rede/prefixo cae dentro de un bloque privado? */
static inline bool dapp_rede_privada(uint32_t rede, int prefixo)
{
    if (prefixo >= 16 && ((rede >> 16) == (192u << 8 | 168) || (rede >> 16) == (169u << 8 | 254))) return true;
    if (prefixo >= 12 && (rede >> 20) == (172u << 4 | 1)) return true;
    if (prefixo >= 8 && (rede >> 24) == 10) return true;
    return false;
}

#define DAPP_REGRAS_MAX 8
typedef struct {
    bool     subred;           /* la subred de la tarjeta, la que tenga en ese momento */
    uint32_t rede, mascara;
} dapp_regra_t;

typedef struct {
    dapp_regra_t r[DAPP_REGRAS_MAX];
    int  n;
    bool publicas;             /* dejar también direcciones que no son privadas (con aviso) */
} dapp_permitir_t;

static inline uint32_t dapp_mascara(int prefixo)
{
    return prefixo <= 0 ? 0 : prefixo >= 32 ? 0xFFFFFFFFu : ~((1u << (32 - prefixo)) - 1);
}

/* «a.b.c.d» estricto (sin ceros de más). */
static inline bool dapp_ip_texto(const char *s, unsigned n, uint32_t *ip)
{
    uint32_t v = 0;
    unsigned i = 0;
    for (int parte = 0; parte < 4; parte++) {
        unsigned x = 0, d = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            if (d && x == 0) return false;
            x = x * 10 + (unsigned)(s[i] - '0');
            if (++d > 3 || x > 255) return false;
            i++;
        }
        if (!d) return false;
        v = v << 8 | x;
        if (parte < 3) {
            if (i >= n || s[i] != '.') return false;
            i++;
        }
    }
    if (i != n) return false;
    *ip = v;
    return true;
}

/* Lee «subred, 192.168.5.0/24, 10.8.0.7, publicas» (comas o espacios). Una
 * entrada que no se entiende, o una dirección pública sin «publicas», es un
 * error (se dice cuál en erro) y no se abre nada: mejor que abrir de más.
 * Lista vacía = «subred». */
static inline int dapp_permitir_ler(const char *t, dapp_permitir_t *p, char *erro, unsigned max)
{
    memset(p, 0, sizeof *p);
    if (erro && max) erro[0] = 0;
    const char *s = t;
    /* primero «publicas», que vale esté donde esté */
    for (;;) {
        while (*s == ',' || *s == ' ' || *s == '\t') s++;
        if (!*s) break;
        const char *f = s;
        while (*f && *f != ',' && *f != ' ' && *f != '\t') f++;
        if (f - s == 8 && compara_texto_n(s, "publicas", 8) == 0) p->publicas = true;
        s = f;
    }
    for (s = t;;) {
        while (*s == ',' || *s == ' ' || *s == '\t') s++;
        if (!*s) break;
        const char *f = s;
        while (*f && *f != ',' && *f != ' ' && *f != '\t') f++;
        unsigned n = (unsigned)(f - s);
        if (n == 8 && compara_texto_n(s, "publicas", 8) == 0) { s = f; continue; }
        if (p->n >= DAPP_REGRAS_MAX) {
            if (erro) formatar_texto(erro, max, "demasiadas entradas (como mucho %d)", DAPP_REGRAS_MAX);
            return -1;
        }
        dapp_regra_t *r = &p->r[p->n];
        char ent[40];   /* la entrada sola, para el mensaje de error */
        unsigned ne = n < sizeof ent - 1 ? n : sizeof ent - 1;
        memcpy(ent, s, ne);
        ent[ne] = 0;
        if (n == 6 && compara_texto_n(s, "subred", 6) == 0) {
            r->subred = true;
        } else {
            unsigned nip = n;
            int prefixo = 32;
            for (unsigned i = 0; i < n; i++)
                if (s[i] == '/') {
                    nip = i;
                    prefixo = 0;
                    unsigned d = 0;
                    for (unsigned k = i + 1; k < n; k++) {
                        if (s[k] < '0' || s[k] > '9' || ++d > 2) { prefixo = -1; break; }
                        prefixo = prefixo * 10 + (s[k] - '0');
                    }
                    if (!d) prefixo = -1;
                    break;
                }
            uint32_t ip;
            if (!dapp_ip_texto(s, nip, &ip) || prefixo < 8 || prefixo > 32) {
                if (erro) formatar_texto(erro, max, "no entiendo «%s» (subred, a.b.c.d o a.b.c.d/N con N de 8 a 32)", ent);
                return -1;
            }
            r->mascara = dapp_mascara(prefixo);
            r->rede = ip & r->mascara;
            if (!p->publicas && !dapp_rede_privada(r->rede, prefixo)) {
                if (erro) formatar_texto(erro, max, "«%s» no es una red privada (añade «publicas» si de verdad hace falta)", ent);
                return -1;
            }
        }
        p->n++;
        s = f;
    }
    if (!p->n) { p->r[0].subred = true; p->n = 1; }
    return 0;
}

/* ¿Puede entrar ip? meu_ip/mascara: los de la tarjeta ahora mismo. */
static inline bool dapp_permitido(const dapp_permitir_t *p, uint32_t ip, uint32_t meu_ip, uint32_t mascara)
{
    if (!ip || ip == 0xFFFFFFFFu || ip == meu_ip) return false;
    if (!p->publicas && !dapp_ip_privada(ip)) return false;
    for (int i = 0; i < p->n; i++) {
        const dapp_regra_t *r = &p->r[i];
        if (r->subred) {
            if (!meu_ip || !mascara || mascara == 0xFFFFFFFFu) continue;
            if ((ip & mascara) != (meu_ip & mascara)) continue;
            if ((ip | mascara) == 0xFFFFFFFFu || (ip & ~mascara) == 0) continue;   /* difusión y red */
            return true;
        }
        if ((ip & r->mascara) == r->rede) return true;
    }
    return false;
}

/* ------------------------------------------------------------ hexadecimal */

static inline void dapp_hex(const uint8_t *b, unsigned n, char *dst)
{
    static const char h[] = "0123456789abcdef";
    for (unsigned i = 0; i < n; i++) { dst[2 * i] = h[b[i] >> 4]; dst[2 * i + 1] = h[b[i] & 15]; }
    dst[2 * n] = 0;
}

static inline bool dapp_de_hex(const char *t, uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < 2 * n; i++) {
        char c = t[i];
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) return false;
        if (i & 1) b[i / 2] = (uint8_t)(b[i / 2] << 4 | v);
        else b[i / 2] = (uint8_t)v;
    }
    return t[2 * n] == 0;
}

#endif
