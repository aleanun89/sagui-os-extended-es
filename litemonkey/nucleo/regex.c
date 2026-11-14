// SPDX-License-Identifier: GPL-2.0-or-later
/* regex.c - expresiones regulares sencillas para $regex.
 *
 * Lo que entiende: ^ $ . * + ? clases [a-z] [^0-9], \d \w \s (y en
 * mayúsculas, negadas), escapes (\. \* \n \t...) y las opciones i
 * (mayúsculas = minúsculas), m (^ y $ en cada línea) y s (. también casa
 * con \n). No hay grupos, alternativas (|) ni repeticiones {n,m}: dan error.
 * El punto y las clases negadas consumen un carácter UTF-8 entero.
 * Búsqueda con vuelta atrás, sin recursión en la longitud del texto. */
#include "lmk.h"

enum { RX_CHAR = 1, RX_ANY, RX_CLASE, RX_INI, RX_FIN };

static inline void bit_poner(uint8_t *c, unsigned v) { c[v >> 3] |= (uint8_t)(1u << (v & 7)); }
static inline bool bit_ver(const uint8_t *c, unsigned v) { return (c[v >> 3] >> (v & 7)) & 1; }

static uint8_t minus(uint8_t c) { return c >= 'A' && c <= 'Z' ? (uint8_t)(c + 32) : c; }

static void clase_atajo(uint8_t *cl, char letra)
{
    uint8_t t[32];
    memset(t, 0, sizeof t);
    char l = (char)(letra | 32);
    for (unsigned v = 0; v < 128; v++) {
        bool si = (l == 'd' && v >= '0' && v <= '9') ||
                  (l == 'w' && ((v >= '0' && v <= '9') || (v >= 'a' && v <= 'z') || (v >= 'A' && v <= 'Z') || v == '_')) ||
                  (l == 's' && (v == ' ' || (v >= 9 && v <= 13)));
        if (si) bit_poner(t, v);
    }
    if (letra >= 'A' && letra <= 'Z')
        for (int i = 0; i < 32; i++) t[i] = (uint8_t)~t[i];
    for (int i = 0; i < 32; i++) cl[i] |= t[i];
}

static int escape_simple(char c)
{
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'f': return '\f';
    case 'v': return '\v';
    case '0': return 0;
    default: return (unsigned char)c;
    }
}

static bool es_atajo(char c) { return c == 'd' || c == 'w' || c == 's' || c == 'D' || c == 'W' || c == 'S'; }

bool regex_compilar(lmk_regex *r, const char *pat, uint32_t len, const char *opciones, lmk_error *err)
{
    memset(r, 0, sizeof *r);
    bool punto_todo = false;
    for (const char *o = opciones; o && *o; o++) {
        if (*o == 'i') r->ignorar = true;
        else if (*o == 'm') r->multilinea = true;
        else if (*o == 's') punto_todo = true;
        else if (*o != 'x' && *o != 'u') return lmk_fallo(err, ERR_VALOR, "opción de $regex desconocida");
    }
    uint32_t i = 0;
    while (i < len) {
        if (r->n >= RX_MAX) return lmk_fallo(err, ERR_VALOR, "$regex demasiado larga");
        lmk_rx_item *it = &r->it[r->n];
        memset(it, 0, sizeof *it);
        char c = pat[i++];
        if (c == '^') it->op = RX_INI;
        else if (c == '$') it->op = RX_FIN;
        else if (c == '.') {
            it->op = RX_ANY;
            it->c = punto_todo;
        } else if (c == '[') {
            it->op = RX_CLASE;
            bool negar = i < len && pat[i] == '^';
            if (negar) i++;
            bool primero = true;
            while (i < len && (pat[i] != ']' || primero)) {
                primero = false;
                int a;
                if (pat[i] == '\\' && i + 1 < len) {
                    if (es_atajo(pat[i + 1])) { clase_atajo(it->clase, pat[i + 1]); i += 2; continue; }
                    a = escape_simple(pat[i + 1]);
                    i += 2;
                } else {
                    a = (unsigned char)pat[i++];
                }
                int b = a;
                if (i + 1 < len && pat[i] == '-' && pat[i + 1] != ']') {
                    i++;
                    if (pat[i] == '\\' && i + 1 < len) { b = escape_simple(pat[i + 1]); i += 2; }
                    else b = (unsigned char)pat[i++];
                    if (b < a) return lmk_fallo(err, ERR_VALOR, "$regex: rango al revés en una clase");
                }
                for (int v = a; v <= b; v++) {
                    bit_poner(it->clase, (unsigned)v);
                    if (r->ignorar) {
                        if (v >= 'a' && v <= 'z') bit_poner(it->clase, (unsigned)v - 32);
                        if (v >= 'A' && v <= 'Z') bit_poner(it->clase, (unsigned)v + 32);
                    }
                }
            }
            if (i >= len) return lmk_fallo(err, ERR_VALOR, "$regex: falta el ] de una clase");
            i++;
            if (negar) {
                for (int k = 0; k < 32; k++) it->clase[k] = (uint8_t)~it->clase[k];
                it->c = 1;   /* negada: consume un carácter UTF-8 entero */
            }
        } else if (c == '\\') {
            if (i >= len) return lmk_fallo(err, ERR_VALOR, "$regex: \\ al final");
            char e = pat[i++];
            if (es_atajo(e)) {
                it->op = RX_CLASE;
                clase_atajo(it->clase, e);
                it->c = e >= 'A' && e <= 'Z';
            } else if (e == 'b' || e == 'B' || (e >= '1' && e <= '9')) {
                return lmk_fallo(err, ERR_VALOR, "$regex: \\b y las referencias no están soportadas");
            } else {
                it->op = RX_CHAR;
                it->c = (uint8_t)escape_simple(e);
            }
        } else if (c == '(' || c == ')' || c == '|' || c == '{') {
            return lmk_fallo(err, ERR_VALOR, "$regex: grupos, | y {n,m} no están soportados en lite-monkey");
        } else if (c == '*' || c == '+' || c == '?') {
            return lmk_fallo(err, ERR_VALOR, "$regex: cuantificador sin nada antes");
        } else {
            it->op = RX_CHAR;
            it->c = (uint8_t)c;
        }
        if (i < len && (pat[i] == '*' || pat[i] == '+' || pat[i] == '?')) {
            if (it->op == RX_INI || it->op == RX_FIN) return lmk_fallo(err, ERR_VALOR, "$regex: cuantificador tras ^ o $");
            it->cuant = (uint8_t)pat[i++];
            if (i < len && pat[i] == '?') i++;   /* perezoso: para "buscar" da lo mismo */
            if (i < len && (pat[i] == '*' || pat[i] == '+' || pat[i] == '?'))
                return lmk_fallo(err, ERR_VALOR, "$regex: dos cuantificadores seguidos");
        }
        r->n++;
    }
    r->ancla_ini = r->n > 0 && r->it[0].op == RX_INI && !r->multilinea;
    return true;
}

static inline uint32_t utf8_ancho(const char *s, uint32_t pos, uint32_t len)
{
    uint8_t c = (uint8_t)s[pos];
    uint32_t w = c < 0xC0 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    return pos + w <= len ? w : len - pos;
}

/* Cuántos bytes consume el item en pos (0 = no casa). */
static inline uint32_t casa(const lmk_regex *r, const lmk_rx_item *it, const char *s, uint32_t pos, uint32_t len)
{
    if (pos >= len) return 0;
    uint8_t c = (uint8_t)s[pos];
    switch (it->op) {
    case RX_CHAR:
        if (c == it->c || (r->ignorar && minus(c) == minus(it->c))) return 1;
        return 0;
    case RX_ANY:
        if (c == '\n' && !it->c) return 0;
        return utf8_ancho(s, pos, len);
    case RX_CLASE:
        if (!bit_ver(it->clase, c)) return 0;
        return it->c && c >= 0x80 ? utf8_ancho(s, pos, len) : 1;
    default:
        return 0;
    }
}

static bool aqui(const lmk_regex *r, int i, const char *s, uint32_t pos, uint32_t len)
{
    while (i < r->n) {
        const lmk_rx_item *it = &r->it[i];
        if (it->op == RX_INI) {
            if (pos != 0 && !(r->multilinea && s[pos - 1] == '\n')) return false;
            i++;
            continue;
        }
        if (it->op == RX_FIN) {
            if (pos != len && !(r->multilinea && s[pos] == '\n') && !(pos + 1 == len && s[pos] == '\n')) return false;
            i++;
            continue;
        }
        if (it->cuant == 0) {
            uint32_t w = casa(r, it, s, pos, len);
            if (!w) return false;
            pos += w;
            i++;
            continue;
        }
        if (it->cuant == '?') {
            uint32_t w = casa(r, it, s, pos, len);
            if (w && aqui(r, i + 1, s, pos + w, len)) return true;
            i++;
            continue;
        }
        /* * y +: avanza lo más posible y retrocede carácter a carácter. */
        uint32_t fin = pos, cuenta = 0, w;
        while ((w = casa(r, it, s, fin, len)) != 0) { fin += w; cuenta++; }
        uint32_t minimo = it->cuant == '+' ? 1 : 0;
        if (cuenta < minimo) return false;
        if (i + 1 == r->n) return true;
        for (;;) {
            if (aqui(r, i + 1, s, fin, len)) return true;
            if (cuenta == minimo) return false;
            fin--;
            if (it->op == RX_ANY || (it->op == RX_CLASE && it->c))
                while (fin > pos && ((uint8_t)s[fin] & 0xC0) == 0x80) fin--;
            cuenta--;
        }
    }
    return true;
}

bool regex_buscar(const lmk_regex *r, const char *s, uint32_t len)
{
    if (r->ancla_ini) return aqui(r, 0, s, 0, len);
    /* Atajo: si empieza por un carácter fijo, saltar hasta él. */
    const lmk_rx_item *p = r->n ? &r->it[0] : NULL;
    bool fijo = p && p->op == RX_CHAR && p->cuant != '*' && p->cuant != '?' && !r->ignorar;
    for (uint32_t pos = 0; pos <= len; pos++) {
        if (fijo) {
            const char *q = memchr(s + pos, p->c, len - pos);
            if (!q) return false;
            pos = (uint32_t)(q - s);
        }
        if (aqui(r, 0, s, pos, len)) return true;
    }
    return false;
}
