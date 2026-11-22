/* SPDX-License-Identifier: GPL-3.0-only */
/* awk_compat.c - mbtowc y wctomb de un byte por carácter (Latin-1) para awk (D440). */
#include <stddef.h>

int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    if (!s) return 0;
    if (n == 0) return -1;
    if (pwc) *pwc = (unsigned char)*s;
    return *s ? 1 : 0;
}

int wctomb(char *s, wchar_t wc)
{
    if (!s) return 0;
    *s = (char)wc;
    return 1;
}
