// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh_cliente_logica.h - la parte del cliente ssh (D317) que no toca el sistema:
 *
 *   - known_hosts: el nombre con que se guarda una máquina, qué líneas le
 *     corresponden (patrones con * ? y !, marcas @revoked) y la huella SHA256;
 *   - las claves privadas ed25519 en el formato de OpenSSH
 *     («openssh-key-v1», sin cifrar): leerlas y escribirlas;
 *   - los mensajes del servidor que tienen estructura (KEXINIT, ECDH_REPLY),
 *     leídos con límites;
 *   - la secuencia de escape «~.» y la traducción de teclas y de pantalla
 *     entre el Terminal de SaguiOS (Latin-1, teclas como ESC + letra) y el
 *     terminal xterm en UTF-8 que espera el otro lado.
 *
 * Se prueba en el anfitrión (tests/ssh_cliente_unidad.c), también con datos
 * malformados y al azar. */
#ifndef SSH_CLIENTE_LOGICA_H
#define SSH_CLIENTE_LOGICA_H
#include "sagui.h"
#include "cripto.h"
#include "sshd_logica.h"

/* ------------------------------------------------------------ textos */

static inline int ssh_minuscula(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* Huella «SHA256:...» (como ssh-keygen -l) de una clave ed25519. */
static inline void ssh_huella(const uint8_t pub[32], char *dst, unsigned max)
{
    uint8_t blob[SSH_BLOB_ED25519], h[32];
    char b[48];
    ssh_blob_montar(blob, pub);
    sha256(blob, sizeof blob, h);
    b64_codificar(h, 32, b, sizeof b);
    unsigned k = 0;
    while (b[k] && b[k] != '=') k++;
    b[k] = 0;
    unsigned i = 0;
    const char *p = "SHA256:";
    while (*p && i + 1 < max) dst[i++] = *p++;
    for (k = 0; b[k] && i + 1 < max; k++) dst[i++] = b[k];
    if (max) dst[i] = 0;
}

/* «ssh-ed25519 BASE64» de una clave pública (sin comentario). */
static inline void ssh_linha_publica(const uint8_t pub[32], char *dst, unsigned max)
{
    uint8_t blob[SSH_BLOB_ED25519];
    char b[80];
    ssh_blob_montar(blob, pub);
    b64_codificar(blob, sizeof blob, b, sizeof b);
    unsigned i = 0;
    const char *p = SSH_ED25519 " ";
    while (*p && i + 1 < max) dst[i++] = *p++;
    for (unsigned k = 0; b[k] && i + 1 < max; k++) dst[i++] = b[k];
    if (max) dst[i] = 0;
}

/* ------------------------------------------------------------ known_hosts */

/* Lo que se escribe en known_hosts para una máquina: el nombre tal cual se
 * escribió (en minúsculas) o, si el puerto no es el 22, «[nombre]:puerto».
 * false si el nombre tiene caracteres que no caben en un known_hosts. */
static inline bool ssh_kh_nome(const char *host, int porta, char *dst, unsigned max)
{
    unsigned k = 0;
    if (!host[0]) return false;
    for (const char *p = host; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                  c == '_' || c == ':';
        if (!ok) return false;
    }
    if (porta != 22) { if (k + 1 >= max) return false; dst[k++] = '['; }
    for (const char *p = host; *p; p++) {
        if (k + 1 >= max) return false;
        dst[k++] = (char)ssh_minuscula((unsigned char)*p);
    }
    if (porta != 22) {
        char n[8];
        int t = 0;
        unsigned v = (unsigned)porta;
        do { n[t++] = (char)('0' + v % 10); v /= 10; } while (v && t < 7);
        if (k + 2 + (unsigned)t >= max) return false;
        dst[k++] = ']';
        dst[k++] = ':';
        while (t) dst[k++] = n[--t];
    }
    dst[k] = 0;
    return true;
}

/* ¿"nome" encaja con el patrón (de p a f), con * y ?, sin distinguir mayúsculas? */
static inline bool ssh_padrao(const char *p, const char *f, const char *nome)
{
    /* sin recursión: se recuerda el último * para volver a probar desde ahí */
    const char *estrela = NULL, *volta = NULL;
    while (*nome) {
        if (p < f && *p == '*') { estrela = p++; volta = nome; continue; }
        if (p < f && (*p == '?' || ssh_minuscula((unsigned char)*p) == ssh_minuscula((unsigned char)*nome))) { p++; nome++; continue; }
        if (estrela) { p = estrela + 1; nome = ++volta; continue; }
        return false;
    }
    while (p < f && *p == '*') p++;
    return p == f;
}

/* ¿La lista de patrones (separados por comas) incluye "nome"? Un patrón con
 * «!» delante que encaje lo excluye aunque otro lo incluya (como OpenSSH). */
static inline bool ssh_kh_lista(const char *ini, const char *fim, const char *nome)
{
    bool sim = false;
    for (const char *p = ini; p < fim;) {
        const char *q = p;
        while (q < fim && *q != ',') q++;
        bool neg = q > p && *p == '!';
        if (q > p + (neg ? 1 : 0) && ssh_padrao(p + (neg ? 1 : 0), q, nome)) {
            if (neg) return false;
            sim = true;
        }
        p = q + 1;
    }
    return sim;
}

#define SSH_KH_NADA      0   /* no es de esta máquina (o es un comentario, o no se entiende) */
#define SSH_KH_ED25519   1   /* de esta máquina, con una clave ed25519 (en pub) */
#define SSH_KH_OUTRO     2   /* de esta máquina, con una clave de otro tipo */
#define SSH_KH_REVOGADA  3   /* @revoked: esa clave ed25519 (en pub) no vale para nadie */

/* Una línea de known_hosts: [@marca] patrones tipo BASE64 [comentario].
 * Las líneas con el nombre cifrado («|1|...», HashKnownHosts) no se
 * reconocen: cuentan como de otra máquina. */
static inline int ssh_kh_linha(const char *linha, const char *nome, uint8_t pub[32])
{
    const char *p = linha;
    bool revogada = false;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#' || *p == '\r' || *p == '\n') return SSH_KH_NADA;
    if (*p == '@') {
        if (ssh_comeca(p, "@revoked") && (p[8] == ' ' || p[8] == '\t')) revogada = true;
        else return SSH_KH_NADA;                 /* @cert-authority: certificados, no se usan */
        p += 8;
        while (*p == ' ' || *p == '\t') p++;
    }
    const char *h0 = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    const char *h1 = p;
    while (*p == ' ' || *p == '\t') p++;
    const char *t0 = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    const char *t1 = p;
    while (*p == ' ' || *p == '\t') p++;
    const char *b0 = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    const char *b1 = p;
    if (h1 == h0 || t1 == t0 || b1 == b0) return SSH_KH_NADA;
    if (*h0 == '|') return SSH_KH_NADA;
    bool ed = (unsigned)(t1 - t0) == sizeof SSH_ED25519 - 1 && ssh_comeca(t0, SSH_ED25519);
    if (revogada) {
        /* una clave revocada vale para cualquier nombre: no se mira el patrón */
        if (!ed) return SSH_KH_NADA;
    } else if (!ssh_kh_lista(h0, h1, nome)) return SSH_KH_NADA;
    if (!ed) return SSH_KH_OUTRO;
    uint8_t blob[SSH_BLOB_ED25519 + 8];
    int n = b64_decodificar(b0, (int)(b1 - b0), blob, sizeof blob, false);
    if (n != SSH_BLOB_ED25519 || !ssh_blob_ler(blob, (unsigned)n, pub)) return revogada ? SSH_KH_NADA : SSH_KH_OUTRO;
    return revogada ? SSH_KH_REVOGADA : SSH_KH_ED25519;
}

/* El veredicto sobre todas las líneas de un known_hosts. */
#define SSH_KH_DESCONHECIDA 0   /* no hay ninguna clave de esta máquina: se pregunta */
#define SSH_KH_CONHECIDA    1   /* la clave es la guardada */
#define SSH_KH_MUDOU        2   /* hay otra clave ed25519 guardada para esta máquina: se rechaza */
#define SSH_KH_REVOCADA     3   /* la clave está revocada: se rechaza */
typedef struct {
    int estado;          /* SSH_KH_* de arriba */
    int linha_mudou;     /* la línea (desde 1) de la clave distinta */
    bool outro_tipo;     /* hay claves de otro tipo (RSA...) para esta máquina */
    int linha;           /* líneas vistas */
} ssh_kh_t;

static inline void ssh_kh_iniciar(ssh_kh_t *v) { v->estado = SSH_KH_DESCONHECIDA; v->linha_mudou = 0; v->outro_tipo = false; v->linha = 0; }

/* Una línea más. Una revocada gana a todo; una distinta gana a una igual
 * (dos claves ed25519 distintas para la misma máquina es sospechoso: se rechaza). */
static inline void ssh_kh_somar(ssh_kh_t *v, const char *linha, const char *nome, const uint8_t pub[32])
{
    uint8_t k[32];
    v->linha++;
    int r = ssh_kh_linha(linha, nome, k);
    if (r == SSH_KH_OUTRO) v->outro_tipo = true;
    else if (r == SSH_KH_REVOGADA) { if (memcmp(k, pub, 32) == 0) v->estado = SSH_KH_REVOCADA; }
    else if (r == SSH_KH_ED25519 && v->estado != SSH_KH_REVOCADA) {
        if (memcmp(k, pub, 32) != 0) {
            if (v->estado != SSH_KH_MUDOU) v->linha_mudou = v->linha;
            v->estado = SSH_KH_MUDOU;
        } else if (v->estado == SSH_KH_DESCONHECIDA) v->estado = SSH_KH_CONHECIDA;
    }
}

/* ------------------------------------------------------------ claves privadas (formato de OpenSSH) */

#define SSH_PRIV_INICIO "-----BEGIN OPENSSH PRIVATE KEY-----"
#define SSH_PRIV_FIM    "-----END OPENSSH PRIVATE KEY-----"
#define SSH_PRIV_MAGIA  "openssh-key-v1"

#define SSH_CH_OK         0
#define SSH_CH_FORMATO   -1   /* no es una clave privada de OpenSSH */
#define SSH_CH_CIFRADA   -2   /* tiene frase de paso (no se admite todavía) */
#define SSH_CH_TIPO      -3   /* no es ed25519 */
#define SSH_CH_ROTA      -4   /* dice ser ed25519 pero está mal (comprobaciones, clave pública distinta) */

/* Escribe el texto entero (con las líneas de inicio y fin) en dst; devuelve
 * su tamaño o -1 si no cabe. "checkint" es un número al azar (OpenSSH lo usa
 * para ver si la frase de paso era la buena). */
static inline int ssh_chave_privada_escrever(const uint8_t semente[32], const uint8_t pub[32], const char *comentario,
                                             uint32_t checkint, char *dst, unsigned max)
{
    uint8_t bin[512], priv[256], blob[SSH_BLOB_ED25519], sp[64];
    ssh_escritor_t e, s;
    ssh_blob_montar(blob, pub);
    memcpy(sp, semente, 32);
    memcpy(sp + 32, pub, 32);
    se_iniciar(&s, priv, sizeof priv);
    se_u32(&s, checkint);
    se_u32(&s, checkint);
    se_cstr(&s, SSH_ED25519);
    se_texto(&s, pub, 32);
    se_texto(&s, sp, 64);
    se_cstr(&s, comentario);
    for (uint8_t i = 1; s.n % 8; i++) se_u8(&s, i);
    se_iniciar(&e, bin, sizeof bin);
    se_bytes(&e, SSH_PRIV_MAGIA, sizeof SSH_PRIV_MAGIA);   /* con el cero final */
    se_cstr(&e, "none");
    se_cstr(&e, "none");
    se_cstr(&e, "");
    se_u32(&e, 1);
    se_texto(&e, blob, sizeof blob);
    se_texto(&e, priv, s.n);
    bool erro = e.erro || s.erro;
    cripto_apagar(sp, sizeof sp);
    cripto_apagar(priv, sizeof priv);
    if (erro) { cripto_apagar(bin, sizeof bin); return -1; }
    char b64[720];
    int nb = b64_codificar(bin, (int)e.n, b64, sizeof b64);
    cripto_apagar(bin, sizeof bin);
    unsigned k = 0;
    int r = -1;
    const char *ini = SSH_PRIV_INICIO "\n", *fim = SSH_PRIV_FIM "\n";
    if (nb < 0) goto fora;
    for (const char *p = ini; *p; p++) { if (k + 1 >= max) goto fora; dst[k++] = *p; }
    for (int i = 0; i < nb; i++) {
        if (k + 2 >= max) goto fora;
        dst[k++] = b64[i];
        if (i % 70 == 69 || i == nb - 1) dst[k++] = '\n';
    }
    for (const char *p = fim; *p; p++) { if (k + 1 >= max) goto fora; dst[k++] = *p; }
    dst[k] = 0;
    r = (int)k;
fora:
    cripto_apagar(b64, sizeof b64);
    return r;
}

/* Lee una clave privada ed25519 de OpenSSH sin cifrar. Comprueba los dos
 * números de control, que la clave pública de fuera sea la de dentro y que
 * corresponda a la semilla. comentario puede ser NULL. */
static inline int ssh_chave_privada_ler(const char *texto, unsigned n, uint8_t semente[32], uint8_t pub[32],
                                        char *comentario, unsigned max_com)
{
    unsigned a = 0, ni = sizeof SSH_PRIV_INICIO - 1, nf = sizeof SSH_PRIV_FIM - 1;
    while (a + ni <= n && memcmp(texto + a, SSH_PRIV_INICIO, ni) != 0) a++;
    if (a + ni > n) return SSH_CH_FORMATO;
    a += ni;
    unsigned b = a;
    while (b + nf <= n && memcmp(texto + b, SSH_PRIV_FIM, nf) != 0) b++;
    if (b + nf > n) return SSH_CH_FORMATO;
    static uint8_t bin[2048];
    int nb = b64_decodificar(texto + a, (int)(b - a), bin, sizeof bin, false);
    int r = SSH_CH_FORMATO;
    ssh_leitor_t l, s;
    unsigned nc, nk, no, npb, nps;
    if (nb < (int)sizeof SSH_PRIV_MAGIA || memcmp(bin, SSH_PRIV_MAGIA, sizeof SSH_PRIV_MAGIA) != 0) goto fora;
    sl_iniciar(&l, bin + sizeof SSH_PRIV_MAGIA, (unsigned)nb - sizeof SSH_PRIV_MAGIA);
    const uint8_t *cifra = sl_texto(&l, &nc);
    const uint8_t *kdf = sl_texto(&l, &nk);
    sl_texto(&l, &no);
    uint32_t nchaves = sl_u32(&l);
    const uint8_t *pblob = sl_texto(&l, &npb);
    const uint8_t *sec = sl_texto(&l, &nps);
    if (l.erro || nchaves != 1) goto fora;
    if (!sl_igual(cifra, nc, "none") || !sl_igual(kdf, nk, "none")) { r = SSH_CH_CIFRADA; goto fora; }
    uint8_t pfora[32];
    if (!ssh_blob_ler(pblob, npb, pfora)) { r = SSH_CH_TIPO; goto fora; }
    r = SSH_CH_ROTA;
    if (nps % 8) goto fora;
    sl_iniciar(&s, sec, nps);
    uint32_t c1 = sl_u32(&s), c2 = sl_u32(&s);
    unsigned nt, np, nsp, ncm;
    const uint8_t *tipo = sl_texto(&s, &nt);
    const uint8_t *pd = sl_texto(&s, &np);
    const uint8_t *sp = sl_texto(&s, &nsp);
    const uint8_t *com = sl_texto(&s, &ncm);
    if (s.erro || c1 != c2) goto fora;
    if (!sl_igual(tipo, nt, SSH_ED25519)) { r = SSH_CH_TIPO; goto fora; }
    if (np != 32 || nsp != 64 || memcmp(pd, pfora, 32) != 0 || memcmp(sp + 32, pd, 32) != 0) goto fora;
    for (unsigned i = 1; s.pos < nps; i++)            /* el relleno: 1, 2, 3... */
        if (sl_u8(&s) != (i & 0xFF)) goto fora;
    uint8_t calc[32];
    ed25519_publica(calc, sp);
    if (memcmp(calc, pd, 32) != 0) goto fora;
    memcpy(semente, sp, 32);
    memcpy(pub, pd, 32);
    if (comentario && max_com) {
        unsigned k = 0;
        for (unsigned i = 0; i < ncm && k + 1 < max_com; i++) {
            uint8_t c = com[i];
            comentario[k++] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
        comentario[k] = 0;
    }
    r = SSH_CH_OK;
fora:
    cripto_apagar(bin, sizeof bin);
    return r;
}

/* ------------------------------------------------------------ mensajes del servidor */

/* KEXINIT (sin el byte del tipo ni la galleta de 16): las 10 listas de
 * nombres, first_kex_packet_follows y el reservado. */
static inline bool ssh_kexinit_ler(const uint8_t *pl, unsigned pn, const uint8_t *lst[10], unsigned nl[10], bool *segue)
{
    ssh_leitor_t l;
    if (pn < 17) return false;
    sl_iniciar(&l, pl + 17, pn - 17);
    for (int i = 0; i < 10; i++) lst[i] = sl_texto(&l, &nl[i]);
    *segue = sl_u8(&l) != 0;
    sl_u32(&l);
    return !l.erro;
}

/* ECDH_REPLY (RFC 5656, 4): string K_S (el blob de la clave del servidor),
 * string Q_S (32), string firma. Sin bytes de más. */
static inline bool ssh_ecdh_resposta_ler(const uint8_t *pl, unsigned pn, const uint8_t **ks, unsigned *nks,
                                         uint8_t pub[32], uint8_t qs[32], uint8_t assin[64])
{
    ssh_leitor_t l;
    unsigned nq, nf;
    if (pn < 1) return false;
    sl_iniciar(&l, pl + 1, pn - 1);
    *ks = sl_texto(&l, nks);
    const uint8_t *q = sl_texto(&l, &nq);
    const uint8_t *f = sl_texto(&l, &nf);
    if (l.erro || l.pos != l.n || nq != 32) return false;
    if (!ssh_blob_ler(*ks, *nks, pub)) return false;
    if (!ssh_firma_ler(f, nf, assin)) return false;
    memcpy(qs, q, 32);
    return true;
}

/* ------------------------------------------------------------ la secuencia de escape («~.») */

#define SSH_ESC_NADA    0
#define SSH_ESC_SAIR    1   /* ~.  corta la conexión */
#define SSH_ESC_AJUDA   2   /* ~?  */

typedef struct {
    bool inicio;   /* al principio de una línea (lo es al empezar) */
    bool til;      /* se tecleó «~» al principio de una línea: se espera la siguiente */
} ssh_escape_t;

static inline void ssh_escape_iniciar(ssh_escape_t *e) { e->inicio = true; e->til = false; }

/* Una tecla (en bruto, antes de traducirla). Deja en out lo que hay que mandar
 * (0, 1 o 2 bytes) y devuelve la acción. */
static inline int ssh_escape_tecla(ssh_escape_t *e, uint8_t c, uint8_t out[2], unsigned *n)
{
    *n = 0;
    if (e->til) {
        e->til = false;
        if (c == '.') return SSH_ESC_SAIR;
        if (c == '?') { e->inicio = false; return SSH_ESC_AJUDA; }
        if (c != '~') out[(*n)++] = '~';   /* «~~» manda un solo «~» */
        out[(*n)++] = c;
        e->inicio = c == '\r' || c == '\n';
        return SSH_ESC_NADA;
    }
    if (e->inicio && c == '~') { e->til = true; return SSH_ESC_NADA; }
    out[(*n)++] = c;
    e->inicio = c == '\r' || c == '\n' || c == 3 || c == 21;   /* también tras Ctrl+C y Ctrl+U */
    return SSH_ESC_NADA;
}

/* ------------------------------------------------------------ teclas: Terminal de SaguiOS -> xterm */

typedef struct { int esc; } ssh_teclado_t;   /* 1 = el byte anterior fue ESC */

/* Lo que manda el Terminal en modo bruto (Latin-1; Intro = \n; borrar = \b;
 * teclas especiales como ESC + letra, abi.h TECLA_B_*; Esc = ESC ESC) -> lo
 * que manda un xterm en UTF-8. Escribe como mucho 4*n + 4 bytes. */
static inline unsigned ssh_teclas_xterm(ssh_teclado_t *t, const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (t->esc) {
            t->esc = 0;
            const char *s = NULL;
            switch (c) {
            case 'A': s = "\033[A"; break;
            case 'B': s = "\033[B"; break;
            case 'C': s = "\033[C"; break;
            case 'D': s = "\033[D"; break;
            case 'H': s = "\033[H"; break;
            case 'F': s = "\033[F"; break;
            case 'X': s = "\033[3~"; break;
            case 'P': s = "\033[5~"; break;
            case 'N': s = "\033[6~"; break;
            case 27:  s = "\033"; break;      /* la tecla Esc */
            case 'R': s = ""; break;          /* «otro proceso escribió»: no es una tecla */
            default:  break;
            }
            if (s) { while (*s) dst[k++] = (uint8_t)*s++; continue; }
            dst[k++] = 27;                    /* un ESC suelto seguido de otra cosa */
        }
        if (c == 27) { t->esc = 1; continue; }
        if (c == '\n') { dst[k++] = '\r'; continue; }
        if (c == '\b') { dst[k++] = 0x7F; continue; }
        if (c >= 0x80) { dst[k++] = (uint8_t)(0xC0 | c >> 6); dst[k++] = (uint8_t)(0x80 | (c & 0x3F)); continue; }
        dst[k++] = (uint8_t)c;
    }
    return k;
}

/* D320: con el Terminal en modo xterm (CSI ? 7700 h) las teclas ya salen como las de un xterm (F1-F12,
 * Alt = ESC + tecla, Ctrl+flechas, modo aplicación del cursor): solo falta Intro -> CR, borrar -> DEL y
 * Latin-1 -> UTF-8. Una tecla Esc llega como un ESC solo, sin esperar a la siguiente. Hasta 2*n bytes. */
static inline unsigned ssh_teclas_xterm_cru(const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (c == '\n') dst[k++] = '\r';
        else if (c == '\b') dst[k++] = 0x7F;
        else if (c >= 0x80) { dst[k++] = (uint8_t)(0xC0 | c >> 6); dst[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
        else dst[k++] = (uint8_t)c;
    }
    return k;
}

/* ------------------------------------------------------------ pantalla: UTF-8 -> Latin-1 */

typedef struct {
    unsigned falta;    /* bytes de continuación que faltan */
    uint32_t cp;       /* el carácter que se está montando */
    unsigned total;    /* bytes del carácter (para rechazar las formas largas) */
    bool     caixas;   /* D320: el Terminal dibuja las cajas (ESC ( 0): se traducen en vez de aproximarlas con + - | */
    bool     dec;      /* y ahora está seleccionado el juego de dibujo DEC (ESC ( 0) */
} ssh_utf8_t;

/* D320: el carácter de dibujo de líneas del juego DEC (ESC ( 0) que le corresponde a un carácter Unicode de
 * cajas, o 0. Las líneas gruesas, dobles y redondeadas se dibujan como la sencilla. */
static inline uint8_t ssh_caixa_dec(uint32_t cp)
{
    if (cp >= 0x2500 && cp <= 0x257F) {
        if (cp <= 0x2501 || (cp >= 0x2504 && cp <= 0x2505) || (cp >= 0x2508 && cp <= 0x2509) || cp == 0x2550 ||
            (cp >= 0x254C && cp <= 0x254D) || cp == 0x2574 || cp == 0x2576 || cp == 0x257C || cp == 0x257E) return 'q';
        if (cp <= 0x2503 || (cp >= 0x2506 && cp <= 0x2507) || (cp >= 0x250A && cp <= 0x250B) || cp == 0x2551 ||
            (cp >= 0x254E && cp <= 0x254F) || cp == 0x2575 || cp == 0x2577 || cp == 0x257D || cp == 0x257F) return 'x';
        if ((cp >= 0x250C && cp <= 0x250F) || (cp >= 0x2552 && cp <= 0x2554) || cp == 0x256D) return 'l';
        if ((cp >= 0x2510 && cp <= 0x2513) || (cp >= 0x2555 && cp <= 0x2557) || cp == 0x256E) return 'k';
        if ((cp >= 0x2514 && cp <= 0x2517) || (cp >= 0x2558 && cp <= 0x255A) || cp == 0x2570) return 'm';
        if ((cp >= 0x2518 && cp <= 0x251B) || (cp >= 0x255B && cp <= 0x255D) || cp == 0x256F) return 'j';
        if ((cp >= 0x251C && cp <= 0x2523) || (cp >= 0x255E && cp <= 0x2560)) return 't';
        if ((cp >= 0x2524 && cp <= 0x252B) || (cp >= 0x2561 && cp <= 0x2563)) return 'u';
        if ((cp >= 0x252C && cp <= 0x2533) || (cp >= 0x2564 && cp <= 0x2566)) return 'w';
        if ((cp >= 0x2534 && cp <= 0x253B) || (cp >= 0x2567 && cp <= 0x2569)) return 'v';
        if ((cp >= 0x253C && cp <= 0x254B) || (cp >= 0x256A && cp <= 0x256C)) return 'n';
        return 0;
    }
    if (cp >= 0x2591 && cp <= 0x2593) return 'a';   /* ░ ▒ ▓ */
    if (cp == 0x2588) return '0';                    /* █ */
    return 0;
}

/* Caracteres frecuentes fuera de Latin-1 (dibujos de cajas, comillas) -> uno parecido. */
static inline uint8_t ssh_parecido(uint32_t cp)
{
    if (cp == 0x2500 || cp == 0x2501 || cp == 0x2550 || cp == 0x2013 || cp == 0x2014 || cp == 0x2212) return '-';
    if (cp == 0x2502 || cp == 0x2503 || cp == 0x2551) return '|';
    if (cp >= 0x250C && cp <= 0x254B) return '+';
    if (cp >= 0x2552 && cp <= 0x256C) return '+';
    if (cp == 0x2018 || cp == 0x2019 || cp == 0x201A || cp == 0x2032) return '\'';
    if (cp == 0x201C || cp == 0x201D || cp == 0x201E) return '"';
    if (cp == 0x2026) return '.';
    if (cp == 0x2022 || cp == 0x25CF) return 0xB7;   /* punto medio */
    if (cp == 0x20AC) return 'E';
    return '?';
}

/* Texto UTF-8 del otro lado -> Latin-1 para el Terminal, aunque un carácter
 * llegue partido entre dos paquetes. Escribe como mucho n bytes (con caixas, hasta 3*n + 3). */
static inline unsigned ssh_utf8_latin1(ssh_utf8_t *u, const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
#define SSH_FECHA_DEC() do { if (u->dec) { dst[k++] = 27; dst[k++] = '('; dst[k++] = 'B'; u->dec = false; } } while (0)
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (u->falta) {
            if ((c & 0xC0) == 0x80) {
                u->cp = u->cp << 6 | (c & 0x3F);
                if (--u->falta) continue;
                uint32_t cp = u->cp;
                bool larga = (u->total == 2 && cp < 0x80) || (u->total == 3 && cp < 0x800) || (u->total == 4 && cp < 0x10000);
                uint8_t d = u->caixas && !larga ? ssh_caixa_dec(cp) : 0;
                if (d) {
                    if (!u->dec) { dst[k++] = 27; dst[k++] = '('; dst[k++] = '0'; u->dec = true; }
                    dst[k++] = d;
                    continue;
                }
                SSH_FECHA_DEC();
                dst[k++] = larga || cp > 0x10FFFF ? '?' : cp < 0x100 ? (uint8_t)cp : ssh_parecido(cp);
                continue;
            }
            u->falta = 0;
            SSH_FECHA_DEC();
            dst[k++] = '?';            /* secuencia cortada: el byte actual se trata abajo */
        }
        if (c >= 0xC2 && c <= 0xF4) {
            /* el inicio de un carácter largo: no cierra el juego DEC todavía (puede ser una caja) */
            if (c <= 0xDF) { u->falta = 1; u->total = 2; u->cp = c & 0x1F; }
            else if (c <= 0xEF) { u->falta = 2; u->total = 3; u->cp = c & 0x0F; }
            else { u->falta = 3; u->total = 4; u->cp = c & 0x07; }
            continue;
        }
        SSH_FECHA_DEC();
        if (c < 0x80) { dst[k++] = (uint8_t)c; continue; }
        dst[k++] = '?';                /* continuación suelta o byte que no existe en UTF-8 */
    }
#undef SSH_FECHA_DEC
    return k;
}

/* En un xterm, «\b» solo mueve el cursor a la izquierda (bash lo usa para
 * moverse por la línea); en el Terminal de SaguiOS y en la consola además
 * borra. Con terminal remoto se cambia por ESC [ D, que solo mueve. Hasta 3*n. */
static inline unsigned ssh_retroceso_cursor(const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        if (o[i] == '\b') { dst[k++] = 27; dst[k++] = '['; dst[k++] = 'D'; }
        else dst[k++] = o[i];
    }
    return k;
}

/* Latin-1 -> UTF-8 (entrada sin terminal desde el Terminal). Hasta 2*n. */
static inline unsigned ssh_latin1_utf8(const uint8_t *o, unsigned n, uint8_t *dst) { return ssh_para_utf8(o, n, dst); }

/* ------------------------------------------------------------ respuesta del Terminal a ESC [ 18 t */

/* El Terminal de SaguiOS (D317) contesta «ESC [ 8 ; filas ; columnas t» a la
 * pregunta ESC [ 18 t. Busca esa respuesta en lo que llegó del teclado, la
 * quita y deja el tamaño. Devuelve los bytes que quedan. */
static inline unsigned ssh_tamanho_resposta(uint8_t *b, unsigned n, int *lins, int *cols, bool *achou)
{
    for (unsigned i = 0; i + 3 < n; i++) {
        if (b[i] != 27 || b[i + 1] != '[' || b[i + 2] != '8' || b[i + 3] != ';') continue;
        unsigned j = i + 4;
        int v[2] = { 0, 0 }, p = 0, d = 0;
        while (j < n && j < i + 20) {
            uint8_t c = b[j++];
            if (c >= '0' && c <= '9') { if (v[p] < 10000) v[p] = v[p] * 10 + (c - '0'); d++; continue; }
            if (c == ';' && p == 0 && d) { p = 1; d = 0; continue; }
            if (c == 't' && p == 1 && d) {
                if (v[0] > 0 && v[1] > 0) { *lins = v[0]; *cols = v[1]; *achou = true; }
                memmove(b + i, b + j, n - j);
                return n - (j - i);
            }
            break;
        }
    }
    return n;
}

#endif
