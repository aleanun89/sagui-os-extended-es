// SPDX-License-Identifier: GPL-2.0-or-later
/* sshd_logica.h - la parte del servidor SSH (D250) que no toca el sistema:
 * lectura y escritura de los campos del protocolo, listas de algoritmos,
 * authorized_keys, qué direcciones son de la red local, a dónde se puede
 * redirigir un puerto (D253), la traducción entre el
 * terminal de SaguiOS (Latin-1, D036) y el del cliente (UTF-8, xterm) y las
 * rutas del SFTP. Se prueba en el anfitrión (tests/ssh_unidad.c). */
#ifndef SSHD_LOGICA_H
#define SSHD_LOGICA_H
#include "sagui.h"
#include "base64_logica.h"

/* ------------------------------------------------------------ campos del protocolo (RFC 4251, 5) */

/* Lector con límites: tras un campo que no cabe, "erro" queda puesto y todo
 * lo que se lea después sale vacío; se comprueba una vez al final. */
typedef struct {
    const uint8_t *p;
    unsigned n, pos;
    bool erro;
} ssh_leitor_t;

static inline void sl_iniciar(ssh_leitor_t *l, const uint8_t *p, unsigned n) { l->p = p; l->n = n; l->pos = 0; l->erro = false; }

static inline unsigned sl_u8(ssh_leitor_t *l)
{
    if (l->erro || l->n - l->pos < 1) { l->erro = true; return 0; }
    return l->p[l->pos++];
}

static inline uint32_t sl_u32(ssh_leitor_t *l)
{
    if (l->erro || l->n - l->pos < 4) { l->erro = true; return 0; }
    const uint8_t *b = l->p + l->pos;
    l->pos += 4;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}

/* uint64 del que solo se admiten valores de 32 bits (desplazamientos del SFTP). */
static inline uint32_t sl_u64_baixo(ssh_leitor_t *l)
{
    uint32_t alto = sl_u32(l), baixo = sl_u32(l);
    if (alto) l->erro = true;
    return baixo;
}

/* "string": tamaño de 32 bits y los bytes (que quedan apuntando dentro del paquete). */
static inline const uint8_t *sl_texto(ssh_leitor_t *l, unsigned *n)
{
    uint32_t t = sl_u32(l);
    if (l->erro || t > l->n - l->pos) { l->erro = true; *n = 0; return (const uint8_t *)""; }
    const uint8_t *b = l->p + l->pos;
    l->pos += t;
    *n = t;
    return b;
}

/* Un "string" copiado como texto C (cortado si no cabe; los ceros de dentro lo invalidan). */
static inline void sl_cstr(ssh_leitor_t *l, char *dst, unsigned max)
{
    unsigned n;
    const uint8_t *b = sl_texto(l, &n);
    if (n >= max) { l->erro = true; n = 0; }
    for (unsigned i = 0; i < n; i++) {
        if (!b[i]) { l->erro = true; n = 0; break; }
        dst[i] = (char)b[i];
    }
    dst[n] = 0;
}

static inline bool sl_igual(const uint8_t *b, unsigned n, const char *texto)
{
    unsigned t = 0;
    while (texto[t]) t++;
    return t == n && memcmp(b, texto, n) == 0;
}

/* Escritor: si no cabe, "erro". */
typedef struct {
    uint8_t *b;
    unsigned n, max;
    bool erro;
} ssh_escritor_t;

static inline void se_iniciar(ssh_escritor_t *e, uint8_t *b, unsigned max) { e->b = b; e->n = 0; e->max = max; e->erro = false; }

static inline void se_bytes(ssh_escritor_t *e, const void *p, unsigned n)
{
    if (e->erro || n > e->max - e->n) { e->erro = true; return; }
    memcpy(e->b + e->n, p, n);
    e->n += n;
}

static inline void se_u8(ssh_escritor_t *e, unsigned v) { uint8_t b = (uint8_t)v; se_bytes(e, &b, 1); }

static inline void se_u32(ssh_escritor_t *e, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    se_bytes(e, b, 4);
}

static inline void se_u64(ssh_escritor_t *e, uint32_t v) { se_u32(e, 0); se_u32(e, v); }
static inline void se_texto(ssh_escritor_t *e, const void *p, unsigned n) { se_u32(e, n); se_bytes(e, p, n); }

static inline void se_cstr(ssh_escritor_t *e, const char *s)
{
    unsigned n = 0;
    while (s[n]) n++;
    se_texto(e, s, n);
}

/* "mpint" de un número sin signo big-endian: sin ceros a la izquierda y con
 * un cero delante si el primer bit está puesto (para que no sea negativo). */
static inline void se_mpint(ssh_escritor_t *e, const uint8_t *v, unsigned n)
{
    while (n && !*v) { v++; n--; }
    bool cero = n && (v[0] & 0x80);
    se_u32(e, n + (cero ? 1 : 0));
    if (cero) se_u8(e, 0);
    se_bytes(e, v, n);
}

/* ------------------------------------------------------------ listas de nombres */

/* ¿Está "nome" en la lista (nombres separados por comas)? */
static inline bool ssh_lista_tem(const uint8_t *lista, unsigned n, const char *nome)
{
    unsigned t = 0;
    while (nome[t]) t++;
    for (unsigned i = 0; i <= n;) {
        unsigned j = i;
        while (j < n && lista[j] != ',') j++;
        if (j - i == t && memcmp(lista + i, nome, t) == 0) return true;
        i = j + 1;
    }
    return false;
}

/* El primer nombre de la lista del cliente que también está en la del
 * servidor (RFC 4253, 7.1). Lo copia en "saida"; false si no hay ninguno. */
static inline bool ssh_escolher(const uint8_t *cliente, unsigned n, const char *servidor, char *saida, unsigned max)
{
    unsigned ts = 0;
    while (servidor[ts]) ts++;
    for (unsigned i = 0; i < n;) {
        unsigned j = i;
        while (j < n && cliente[j] != ',') j++;
        unsigned t = j - i;
        if (t && t < max) {
            memcpy(saida, cliente + i, t);
            saida[t] = 0;
            if (ssh_lista_tem((const uint8_t *)servidor, ts, saida)) return true;
        }
        i = j + 1;
    }
    saida[0] = 0;
    return false;
}

/* ¿El primer nombre de la lista es este? (para first_kex_packet_follows) */
static inline bool ssh_primeiro_eh(const uint8_t *lista, unsigned n, const char *nome)
{
    unsigned t = 0;
    while (nome[t]) t++;
    return n >= t && memcmp(lista, nome, t) == 0 && (n == t || lista[t] == ',');
}

/* ------------------------------------------------------------ claves públicas */

#define SSH_ED25519 "ssh-ed25519"
#define SSH_BLOB_ED25519 51   /* 4 + 11 + 4 + 32 */

/* El "blob" de una clave ssh-ed25519: string "ssh-ed25519", string clave(32). */
static inline void ssh_blob_montar(uint8_t blob[SSH_BLOB_ED25519], const uint8_t pub[32])
{
    ssh_escritor_t e;
    se_iniciar(&e, blob, SSH_BLOB_ED25519);
    se_cstr(&e, SSH_ED25519);
    se_texto(&e, pub, 32);
}

static inline bool ssh_blob_ler(const uint8_t *blob, unsigned n, uint8_t pub[32])
{
    ssh_leitor_t l;
    unsigned t, tp;
    sl_iniciar(&l, blob, n);
    const uint8_t *tipo = sl_texto(&l, &t);
    const uint8_t *p = sl_texto(&l, &tp);
    if (l.erro || l.pos != n || !sl_igual(tipo, t, SSH_ED25519) || tp != 32) return false;
    memcpy(pub, p, 32);
    return true;
}

static inline bool ssh_comeca(const char *s, const char *prefixo)
{
    while (*prefixo) if (*s++ != *prefixo++) return false;
    return true;
}

/* Una línea de authorized_keys: [opciones] ssh-ed25519 BASE64 [comentario].
 * Devuelve 1 y la clave si la línea tiene una ssh-ed25519 bien formada; 0 si
 * está vacía, es un comentario o es de otro tipo (RSA, ECDSA: no se admiten);
 * -1 si dice ser ssh-ed25519 y está mal. Las opciones de delante (from=...,
 * command=...) NO se aplican: una línea con opciones se rechaza (-1), para no
 * dar más permisos de los que su dueño creía haber dado. */
static inline int ssh_linha_autorizada(const char *linha, uint8_t pub[32], char *comentario, unsigned max)
{
    const char *p = linha;
    while (*p == ' ' || *p == '\t') p++;
    if (comentario && max) comentario[0] = 0;
    if (!*p || *p == '#' || *p == '\r' || *p == '\n') return 0;
    unsigned t = sizeof SSH_ED25519 - 1;
    if (!ssh_comeca(p, SSH_ED25519) || (p[t] != ' ' && p[t] != '\t')) {
        /* ¿otro tipo de clave, u opciones delante de una ed25519? */
        for (const char *q = p; *q && *q != '\n'; q++)
            if (ssh_comeca(q, " " SSH_ED25519 " ")) return -1;
        return 0;
    }
    p += t;
    while (*p == ' ' || *p == '\t') p++;
    const char *b = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
    uint8_t blob[SSH_BLOB_ED25519 + 8];
    int n = b64_decodificar(b, (int)(p - b), blob, sizeof blob, false);
    if (n != SSH_BLOB_ED25519 || !ssh_blob_ler(blob, (unsigned)n, pub)) return -1;
    while (*p == ' ' || *p == '\t') p++;
    if (comentario && max) {
        unsigned k = 0;
        while (*p && *p != '\r' && *p != '\n' && k + 1 < max) {
            unsigned char c = (unsigned char)*p++;
            comentario[k++] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
        }
        comentario[k] = 0;
    }
    return 1;
}

/* ------------------------------------------------------------ red local */

/* ¿Se atiende a esta dirección con "solo_red_local"? Las privadas de la RFC
 * 1918, la de enlace local, la propia máquina y la subred de la tarjeta. */
static inline bool ssh_ip_local(uint32_t ip, uint32_t meu_ip, uint32_t mascara)
{
    if ((ip >> 24) == 10) return true;                    /* 10.0.0.0/8 */
    if ((ip >> 20) == (172u << 4 | 1)) return true;       /* 172.16.0.0/12 */
    if ((ip >> 16) == (192u << 8 | 168)) return true;     /* 192.168.0.0/16 */
    if ((ip >> 16) == (169u << 8 | 254)) return true;     /* 169.254.0.0/16 */
    if ((ip >> 24) == 127) return true;
    if (meu_ip && mascara && mascara != 0xFFFFFFFFu && (ip & mascara) == (meu_ip & mascara)) return true;
    return false;
}

/* ------------------------------------------------------------ redirección de puertos (D253) */

/* Modos de la opción «redirigir» de sshd.conf. */
#define SSH_RED_NO     0   /* ningún canal direct-tcpip */
#define SSH_RED_LOCAL  1   /* solo hacia esta máquina (127.x, localhost): de serie */
#define SSH_RED_REDE   2   /* también hacia direcciones de la red local (red-local) */

/* «a.b.c.d» en decimal, sin nada más (ni ceros de más, ni hexadecimal, ni
 * espacios): lo que inet_aton aceptaría de otras formas aquí no vale, para
 * que la dirección anotada en el registro sea la que se usa. */
static inline bool ssh_ip_texto(const char *s, uint32_t *ip)
{
    uint32_t v = 0;
    for (int parte = 0; parte < 4; parte++) {
        unsigned n = 0, d = 0;
        if (*s < '0' || *s > '9') return false;
        if (s[0] == '0' && s[1] >= '0' && s[1] <= '9') return false;
        while (*s >= '0' && *s <= '9') {
            n = n * 10 + (unsigned)(*s++ - '0');
            if (++d > 3 || n > 255) return false;
        }
        v = v << 8 | n;
        if (parte < 3) { if (*s++ != '.') return false; }
    }
    if (*s) return false;
    *ip = v;
    return true;
}

/* ¿Se puede abrir un túnel (ssh -L) hacia este destino? 0 = sí (y su
 * dirección en *ip); -1 = la configuración no lo permite; -2 = no es
 * «localhost» ni una dirección IPv4 escrita con números (los nombres no se
 * resuelven: lo que se autoriza es una dirección, no lo que diga un DNS). */
static inline int ssh_destino_tunel(const char *host, int modo, uint32_t meu_ip, uint32_t mascara, uint32_t *ip)
{
    static const char local[] = "localhost";
    uint32_t v;
    unsigned i = 0;
    while (local[i] && (host[i] == local[i] || host[i] == local[i] - 32)) i++;
    if (!local[i] && !host[i]) v = 0x7F000001u;
    else if (!ssh_ip_texto(host, &v)) return -2;
    if (modo == SSH_RED_NO) return -1;
    if (v == 0 || v == 0xFFFFFFFFu || (v >> 28) >= 14) return -1;   /* 0.0.0.0, difusión, multidifusión */
    if ((v >> 24) != 127) {
        if (modo != SSH_RED_REDE || !ssh_ip_local(v, meu_ip, mascara)) return -1;
        if (mascara && mascara != 0xFFFFFFFFu && (v & mascara) == (meu_ip & mascara) && (v | mascara) == 0xFFFFFFFFu)
            return -1;                                              /* la difusión de la subred */
    }
    *ip = v;
    return 0;
}

/* El valor de «redirigir» en sshd.conf; -1 si no es ninguno de los tres. */
static inline int ssh_modo_redirigir(const char *v)
{
    static const char *const nomes[] = { "no", "local", "red-local" };
    for (int i = 0; i < 3; i++) {
        unsigned k = 0;
        while (nomes[i][k] && v[k] == nomes[i][k]) k++;
        if (!nomes[i][k] && !v[k]) return i;
    }
    return -1;
}

/* ------------------------------------------------------------ terminal: cliente <-> PTY de SaguiOS */

/* Lo que teclea el cliente (UTF-8, teclas de xterm) -> lo que espera la PTY
 * (Latin-1; teclas especiales como ESC + letra, abi.h TECLA_B_*; Intro = \n;
 * borrar = \b). Con estado, porque una secuencia puede llegar partida. */
typedef struct {
    int estado;        /* 0 normal, 1 tras ESC, 2 dentro de ESC [ o ESC O, 3 falta el 2.º byte de UTF-8 */
    unsigned utf;      /* primer byte de un carácter UTF-8 de dos */
    int param;         /* número de ESC [ n ~ */
    bool sem_cr;       /* el anterior fue \r: un \n inmediato no cuenta (CR LF) */
} ssh_tecla_t;

/* Convierte n bytes; escribe como mucho 2*n + 2 en dst y devuelve cuántos. */
static inline unsigned ssh_teclas(ssh_tecla_t *t, const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        bool era_cr = t->sem_cr;
        t->sem_cr = false;
        switch (t->estado) {
        case 1:
            if (c == '[' || c == 'O') { t->estado = 2; t->param = 0; continue; }
            t->estado = 0;
            dst[k++] = 27;             /* ESC suelto seguido de otra tecla */
            break;                     /* y la tecla se trata abajo */
        case 2:
            if (c >= '0' && c <= '9') { t->param = t->param * 10 + (int)(c - '0'); continue; }
            if (c == ';') { t->param = 0; continue; }
            t->estado = 0;
            {
                int tecla = 0;
                if (c >= 'A' && c <= 'D') tecla = (int)c;          /* flechas: las mismas letras */
                else if (c == 'H') tecla = 'H';
                else if (c == 'F') tecla = 'F';
                else if (c == '~') {
                    if (t->param == 1 || t->param == 7) tecla = 'H';
                    else if (t->param == 4 || t->param == 8) tecla = 'F';
                    else if (t->param == 3) tecla = 'X';
                    else if (t->param == 5) tecla = 'P';
                    else if (t->param == 6) tecla = 'N';
                }
                if (tecla) { dst[k++] = 27; dst[k++] = (uint8_t)tecla; }
            }
            continue;
        case 3:
            t->estado = 0;
            if ((c & 0xC0) == 0x80) {
                unsigned v = (t->utf & 0x1F) << 6 | (c & 0x3F);
                dst[k++] = (uint8_t)(v < 256 ? v : '?');
                continue;
            }
            dst[k++] = '?';            /* secuencia rota: la tecla actual se trata abajo */
            break;
        default:
            break;
        }
        if (c == 27) { t->estado = 1; continue; }
        if (c == '\r') { dst[k++] = '\n'; t->sem_cr = true; continue; }
        if (c == '\n') { if (!era_cr) dst[k++] = '\n'; continue; }
        if (c == 0x7F) { dst[k++] = '\b'; continue; }
        if (c >= 0xC2 && c <= 0xDF) { t->estado = 3; t->utf = c; continue; }
        if (c >= 0x80) { if (c >= 0xE0) dst[k++] = '?'; continue; }   /* fuera de Latin-1: las continuaciones se tiran */
        dst[k++] = (uint8_t)c;
    }
    return k;
}

/* Lo que la PTY manda a la pantalla -> lo que entiende el terminal del
 * cliente: Latin-1 -> UTF-8; "\n" -> "\r\n" (en SaguiOS el salto de línea
 * también vuelve a la columna 0) y "\b" -> "\b \b" (aquí el retroceso borra).
 * Escribe como mucho 3*n en dst. */
static inline unsigned ssh_tela(const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (c == '\n') { dst[k++] = '\r'; dst[k++] = '\n'; }
        else if (c == '\b') { dst[k++] = '\b'; dst[k++] = ' '; dst[k++] = '\b'; }
        else if (c >= 0x80) { dst[k++] = (uint8_t)(0xC0 | c >> 6); dst[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
        else dst[k++] = (uint8_t)c;
    }
    return k;
}

/* Solo Latin-1 -> UTF-8 (nombres de archivo del SFTP, salida con exec_utf8). Hasta 2*n. */
static inline unsigned ssh_para_utf8(const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (c >= 0x80) { dst[k++] = (uint8_t)(0xC0 | c >> 6); dst[k++] = (uint8_t)(0x80 | (c & 0x3F)); }
        else dst[k++] = (uint8_t)c;
    }
    return k;
}

/* UTF-8 -> Latin-1 de un texto entero (rutas y órdenes que manda el cliente).
 * Lo que no existe en Latin-1 queda como '?'. Devuelve el tamaño (<= n). */
static inline unsigned ssh_de_utf8(const uint8_t *o, unsigned n, uint8_t *dst)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned c = o[i];
        if (c < 0x80) dst[k++] = (uint8_t)c;
        else if (c >= 0xC2 && c <= 0xC3 && i + 1 < n && (o[i + 1] & 0xC0) == 0x80) {
            dst[k++] = (uint8_t)((c & 0x1F) << 6 | (o[i + 1] & 0x3F));
            i++;
        } else {
            dst[k++] = '?';
            while (i + 1 < n && (o[i + 1] & 0xC0) == 0x80) i++;
        }
    }
    return k;
}

/* ------------------------------------------------------------ rutas del SFTP */

/* Ruta del cliente -> ruta absoluta simplificada (sin "." ni "..", sin barras
 * dobles). Las relativas salen de "base" (la carpeta de entrada, /home).
 * false si no cabe. */
static inline bool ssh_caminho(const char *base, const char *rel, char *dst, unsigned max)
{
    char tmp[512];
    unsigned k = 0;
    if (rel[0] != '/') {
        for (const char *p = base; *p && k < sizeof tmp - 2; p++) tmp[k++] = *p;
        tmp[k++] = '/';
    }
    for (const char *p = rel; *p; p++) {
        if (k >= sizeof tmp - 1) return false;
        tmp[k++] = *p;
    }
    tmp[k] = 0;
    unsigned o = 0;
    if (max < 2) return false;
    for (unsigned i = 0; tmp[i];) {
        while (tmp[i] == '/') i++;
        unsigned j = i;
        while (tmp[j] && tmp[j] != '/') j++;
        unsigned t = j - i;
        if (t == 0) break;
        if (t == 1 && tmp[i] == '.') { i = j; continue; }
        if (t == 2 && tmp[i] == '.' && tmp[i + 1] == '.') {
            while (o && dst[o - 1] != '/') o--;
            if (o) o--;
            i = j;
            continue;
        }
        if (o + 1 + t + 1 > max) return false;
        dst[o++] = '/';
        memcpy(dst + o, tmp + i, t);
        o += t;
        i = j;
    }
    if (!o) dst[o++] = '/';
    dst[o] = 0;
    return true;
}

#endif
