/* unicode/ustring.h - el trozo de ICU que usa utility/support.c, sin ICU (D374).
 * u_strFromUTF8[Lenient], u_strToUTF8 y u_strCaseCompare sobre UTF-16, con el plegado de
 * mayúsculas de ASCII, Latin-1, Latin extendido A, griego y cirílico (lo que hace falta para
 * comparar nombres de unidades, ciudades y comandos). Solo lo incluye support.c. */
#ifndef FC_SAGUI_USTRING_H
#define FC_SAGUI_USTRING_H
#include <stdint.h>
typedef uint16_t UChar;
typedef int UErrorCode;
#define U_ZERO_ERROR 0
#define U_BUFFER_OVERFLOW_ERROR 15
#define U_INVALID_CHAR_FOUND 10

/* UTF-8 -> UTF-16. Los bytes inválidos pasan a U+FFFD (Lenient) o a error. */
static inline UChar *u_strFromUTF8(UChar *dest, int32_t cap, int32_t *dlen, const char *src, int32_t slen, UErrorCode *err)
{
    int32_t n = 0;
    const unsigned char *s = (const unsigned char *)src;
    int32_t i = 0;
    while (slen < 0 ? s[i] != 0 : i < slen) {
        uint32_t c = s[i++];
        int extra = 0;
        if (c >= 0xF0 && c < 0xF8) { c &= 7; extra = 3; }
        else if (c >= 0xE0) { c &= 15; extra = 2; }
        else if (c >= 0xC0) { c &= 31; extra = 1; }
        else if (c >= 0x80) { c = 0xFFFD; }
        while (extra-- > 0) {
            if ((s[i] & 0xC0) != 0x80) { c = 0xFFFD; break; }
            c = (c << 6) | (s[i++] & 63);
        }
        if (c >= 0x10000) {
            c -= 0x10000;
            if (n + 2 <= cap) { dest[n] = (UChar)(0xD800 + (c >> 10)); dest[n + 1] = (UChar)(0xDC00 + (c & 1023)); }
            n += 2;
        } else {
            if (n < cap) dest[n] = (UChar)c;
            n++;
        }
    }
    if (n < cap) dest[n] = 0;
    if (dlen) *dlen = n;
    if (n > cap) *err = U_BUFFER_OVERFLOW_ERROR;
    return dest;
}
#define u_strFromUTF8Lenient u_strFromUTF8

/* UTF-16 -> UTF-8: copia lo que cabe (nunca parte un carácter) y dice el largo total. */
static inline char *u_strToUTF8(char *dest, int32_t cap, int32_t *dlen, const UChar *src, int32_t slen, UErrorCode *err)
{
    int32_t n = 0, i = 0;
    int overflow = 0;
    while (slen < 0 ? src[i] != 0 : i < slen) {
        uint32_t c = src[i++];
        char b[4]; int k;
        if (c >= 0xD800 && c < 0xDC00 && i < slen && src[i] >= 0xDC00 && src[i] < 0xE000)
            c = 0x10000 + ((c - 0xD800) << 10) + (src[i++] - 0xDC00);
        if (c < 0x80) { b[0] = (char)c; k = 1; }
        else if (c < 0x800) { b[0] = (char)(0xC0 | (c >> 6)); b[1] = (char)(0x80 | (c & 63)); k = 2; }
        else if (c < 0x10000) { b[0] = (char)(0xE0 | (c >> 12)); b[1] = (char)(0x80 | ((c >> 6) & 63)); b[2] = (char)(0x80 | (c & 63)); k = 3; }
        else { b[0] = (char)(0xF0 | (c >> 18)); b[1] = (char)(0x80 | ((c >> 12) & 63)); b[2] = (char)(0x80 | ((c >> 6) & 63)); b[3] = (char)(0x80 | (c & 63)); k = 4; }
        if (!overflow && n + k <= cap) { for (int j = 0; j < k; j++) dest[n + j] = b[j]; }
        else overflow = 1;
        n += k;
    }
    if (n < cap) dest[n] = 0;
    if (dlen) *dlen = n;
    if (overflow) *err = U_BUFFER_OVERFLOW_ERROR;
    return dest;
}

static inline uint32_t fc_ustr_plegar(uint32_t c)
{
    if (c < 0x80) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    if ((c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 32;
    if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x178) {
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c & 1) ? c + 1 : c;
        return (c & 1) ? c : c + 1;
    }
    if (c == 0x178) return 0xFF;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}

/* Como strcmp pero sin distinguir mayúsculas; los largos son en unidades UChar (-1 = hasta el 0). */
static inline int u_strCaseCompare(const UChar *a, int32_t la, const UChar *b, int32_t lb, uint32_t opts, UErrorCode *err)
{
    int32_t i = 0;
    (void)opts; (void)err;
    for (;;) {
        int fa = la < 0 ? a[i] == 0 : i >= la;
        int fb = lb < 0 ? b[i] == 0 : i >= lb;
        if (fa || fb) return fa && fb ? 0 : fa ? -1 : 1;
        uint32_t x = fc_ustr_plegar(a[i]), y = fc_ustr_plegar(b[i]);
        if (x != y) return x < y ? -1 : 1;
        i++;
    }
}
#endif
