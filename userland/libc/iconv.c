/* iconv.c - iconv mínimo para NetSurf (D184).
 *
 * Carencia: el SaguiOS no tiene iconv (la libsagui solo sabe pasar UTF-8 a
 * Latin-1, utf8_para_latin1). NetSurf lo usa para convertir texto entre su
 * UTF-8 interno y otras codificaciones (formularios, nombres de fichero).
 * Las páginas no pasan por aquí: libparserutils trae sus propios
 * decodificadores (UTF-8, UTF-16, ISO-8859-*, Windows-125x).
 *
 * Codificaciones: UTF-8, ISO-8859-1 (LATIN1), WINDOWS-1252 (CP1252),
 * US-ASCII, UTF-16 / UTF-16LE / UTF-16BE, UTF-32 / UCS-4 (LE/BE). Con
 * "//TRANSLIT" lo que no cabe sale como '?'; con "//IGNORE" se salta.
 */
#include "sagui.h"

#include "ctype.h"
#include "errno.h"
#include "iconv.h"
#include "stdlib.h"
#include "string.h"

enum { C_UTF8, C_LATIN1, C_CP1252, C_ASCII, C_UTF16LE, C_UTF16BE, C_UTF32LE, C_UTF32BE };

typedef struct {
    int de, para;
    int translit, ignorar;
} conv_t;

/* Windows-1252 de 0x80 a 0x9F (0 = sin asignar). */
static const uint16_t cp1252[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
};

static bool igual_nome(const char *a, const char *b, size_t n)
{
    /* Sin mayúsculas y sin '-' ni '_' ("utf8" = "UTF-8"). */
    size_t i = 0;
    while (i < n && *b) {
        char ca = a[i], cb = *b;
        if (ca == '-' || ca == '_') { i++; continue; }
        if (cb == '-' || cb == '_') { b++; continue; }
        if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) return false;
        i++; b++;
    }
    while (i < n && (a[i] == '-' || a[i] == '_')) i++;
    while (*b == '-' || *b == '_') b++;
    return i == n && !*b;
}

static int codificacao(const char *nome, size_t n)
{
    static const struct { const char *nome; int c; } t[] = {
        { "UTF-8", C_UTF8 }, { "UTF8", C_UTF8 },
        { "ISO-8859-1", C_LATIN1 }, { "ISO8859-1", C_LATIN1 }, { "LATIN1", C_LATIN1 },
        { "L1", C_LATIN1 }, { "ISO_8859-1", C_LATIN1 },
        { "WINDOWS-1252", C_CP1252 }, { "CP1252", C_CP1252 },
        { "US-ASCII", C_ASCII }, { "ASCII", C_ASCII }, { "ANSI_X3.4-1968", C_ASCII },
        { "UTF-16", C_UTF16LE }, { "UTF-16LE", C_UTF16LE }, { "UTF-16BE", C_UTF16BE },
        { "UCS-2", C_UTF16LE }, { "UCS-2LE", C_UTF16LE }, { "UCS-2BE", C_UTF16BE },
        { "UTF-32", C_UTF32LE }, { "UTF-32LE", C_UTF32LE }, { "UTF-32BE", C_UTF32BE },
        { "UCS-4", C_UTF32BE }, { "UCS-4LE", C_UTF32LE }, { "UCS-4BE", C_UTF32BE },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++)
        if (igual_nome(nome, t[i].nome, n)) return t[i].c;
    return -1;
}

iconv_t iconv_open(const char *para, const char *de)
{
    const char *bp = strstr(para, "//");
    const char *bd = strstr(de, "//");
    int cp = codificacao(para, bp ? (size_t)(bp - para) : strlen(para));
    int cd = codificacao(de, bd ? (size_t)(bd - de) : strlen(de));
    if (cp < 0 || cd < 0) { errno = EINVAL; return (iconv_t)-1; }
    conv_t *c = malloc(sizeof *c);
    if (!c) { errno = ENOMEM; return (iconv_t)-1; }
    c->de = cd;
    c->para = cp;
    c->translit = bp && strstr(bp, "TRANSLIT");
    c->ignorar = bp && strstr(bp, "IGNORE");
    return c;
}

int iconv_close(iconv_t cd)
{
    if (cd == (iconv_t)-1 || !cd) { errno = EBADF; return -1; }
    free(cd);
    return 0;
}

/* Lee un carácter: devuelve los bytes usados, 0 si la entrada está
 * incompleta, -1 si no es válida. */
static int ler_car(int c, const unsigned char *p, size_t n, uint32_t *u)
{
    switch (c) {
    case C_UTF8: {
        if (!n) return 0;
        unsigned char b = p[0];
        int k;
        if (b < 0x80) { *u = b; return 1; }
        else if ((b & 0xE0) == 0xC0) { k = 2; *u = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { k = 3; *u = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0) { k = 4; *u = b & 0x07; }
        else return -1;
        if (n < (size_t)k) return 0;
        for (int i = 1; i < k; i++) {
            if ((p[i] & 0xC0) != 0x80) return -1;
            *u = (*u << 6) | (p[i] & 0x3F);
        }
        if ((k == 2 && *u < 0x80) || (k == 3 && *u < 0x800) || (k == 4 && *u < 0x10000) ||
            *u > 0x10FFFF || (*u >= 0xD800 && *u < 0xE000)) return -1;
        return k;
    }
    case C_LATIN1: if (!n) return 0; *u = p[0]; return 1;
    case C_ASCII: if (!n) return 0; if (p[0] >= 0x80) return -1; *u = p[0]; return 1;
    case C_CP1252:
        if (!n) return 0;
        if (p[0] >= 0x80 && p[0] < 0xA0) {
            if (!cp1252[p[0] - 0x80]) return -1;
            *u = cp1252[p[0] - 0x80];
        } else *u = p[0];
        return 1;
    case C_UTF16LE: case C_UTF16BE: {
        if (n < 2) return 0;
        uint32_t a = c == C_UTF16LE ? (uint32_t)(p[0] | p[1] << 8) : (uint32_t)(p[0] << 8 | p[1]);
        if (a >= 0xD800 && a < 0xDC00) {
            if (n < 4) return 0;
            uint32_t b = c == C_UTF16LE ? (uint32_t)(p[2] | p[3] << 8) : (uint32_t)(p[2] << 8 | p[3]);
            if (b < 0xDC00 || b >= 0xE000) return -1;
            *u = 0x10000 + ((a - 0xD800) << 10) + (b - 0xDC00);
            return 4;
        }
        if (a >= 0xDC00 && a < 0xE000) return -1;
        *u = a;
        return 2;
    }
    default: {
        if (n < 4) return 0;
        *u = c == C_UTF32LE ? (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24)
                            : (uint32_t)((uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]);
        return *u > 0x10FFFF ? -1 : 4;
    }
    }
}

/* Escribe un carácter: bytes escritos, 0 si no cabe, -1 si no existe en
 * la codificación de destino. */
static int escrever_car(int c, uint32_t u, unsigned char *p, size_t n)
{
    switch (c) {
    case C_UTF8:
        if (u < 0x80) { if (n < 1) return 0; p[0] = (unsigned char)u; return 1; }
        if (u < 0x800) { if (n < 2) return 0; p[0] = (unsigned char)(0xC0 | u >> 6); p[1] = (unsigned char)(0x80 | (u & 0x3F)); return 2; }
        if (u < 0x10000) { if (n < 3) return 0; p[0] = (unsigned char)(0xE0 | u >> 12); p[1] = (unsigned char)(0x80 | ((u >> 6) & 0x3F)); p[2] = (unsigned char)(0x80 | (u & 0x3F)); return 3; }
        if (n < 4) return 0;
        p[0] = (unsigned char)(0xF0 | u >> 18); p[1] = (unsigned char)(0x80 | ((u >> 12) & 0x3F));
        p[2] = (unsigned char)(0x80 | ((u >> 6) & 0x3F)); p[3] = (unsigned char)(0x80 | (u & 0x3F));
        return 4;
    case C_LATIN1: if (u > 0xFF) return -1; if (!n) return 0; p[0] = (unsigned char)u; return 1;
    case C_ASCII: if (u > 0x7F) return -1; if (!n) return 0; p[0] = (unsigned char)u; return 1;
    case C_CP1252: {
        unsigned char b = 0;
        if (u < 0x80 || (u >= 0xA0 && u <= 0xFF)) b = (unsigned char)u;
        else {
            for (int i = 0; i < 32; i++) if (cp1252[i] == u) { b = (unsigned char)(0x80 + i); break; }
            if (!b) return -1;
        }
        if (!n) return 0;
        p[0] = b;
        return 1;
    }
    case C_UTF16LE: case C_UTF16BE: {
        uint16_t w[2];
        int k = 1;
        if (u >= 0x10000) { w[0] = (uint16_t)(0xD800 + ((u - 0x10000) >> 10)); w[1] = (uint16_t)(0xDC00 + ((u - 0x10000) & 0x3FF)); k = 2; }
        else w[0] = (uint16_t)u;
        if (n < (size_t)(2 * k)) return 0;
        for (int i = 0; i < k; i++) {
            if (c == C_UTF16LE) { p[2 * i] = (unsigned char)w[i]; p[2 * i + 1] = (unsigned char)(w[i] >> 8); }
            else { p[2 * i] = (unsigned char)(w[i] >> 8); p[2 * i + 1] = (unsigned char)w[i]; }
        }
        return 2 * k;
    }
    default:
        if (n < 4) return 0;
        if (c == C_UTF32LE) { p[0] = (unsigned char)u; p[1] = (unsigned char)(u >> 8); p[2] = (unsigned char)(u >> 16); p[3] = (unsigned char)(u >> 24); }
        else { p[0] = (unsigned char)(u >> 24); p[1] = (unsigned char)(u >> 16); p[2] = (unsigned char)(u >> 8); p[3] = (unsigned char)u; }
        return 4;
    }
}

size_t iconv(iconv_t cd, char **ent, size_t *nent, char **sai, size_t *nsai)
{
    conv_t *c = cd;
    if (cd == (iconv_t)-1 || !c) { errno = EBADF; return (size_t)-1; }
    if (!ent || !*ent) return 0;   /* volver al estado inicial: no hay estado */
    size_t irreversiveis = 0;
    while (*nent > 0) {
        uint32_t u;
        int r = ler_car(c->de, (const unsigned char *)*ent, *nent, &u);
        if (r == 0) { errno = EINVAL; return (size_t)-1; }
        if (r < 0) {
            if (!c->ignorar && !c->translit) { errno = EILSEQ; return (size_t)-1; }
            (*ent)++; (*nent)--;
            continue;
        }
        int w = escrever_car(c->para, u, (unsigned char *)*sai, *nsai);
        if (w < 0) {
            if (c->ignorar) { *ent += r; *nent -= (size_t)r; irreversiveis++; continue; }
            if (!c->translit) { errno = EILSEQ; return (size_t)-1; }
            w = escrever_car(c->para, '?', (unsigned char *)*sai, *nsai);
            irreversiveis++;
        }
        if (w == 0) { errno = E2BIG; return (size_t)-1; }
        *ent += r; *nent -= (size_t)r;
        *sai += w; *nsai -= (size_t)w;
    }
    return irreversiveis;
}
