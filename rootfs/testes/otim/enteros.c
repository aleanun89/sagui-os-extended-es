// SPDX-License-Identifier: GPL-2.0-or-later
/* enteros.c - prueba del generador (D108): aritmetica entera, condiciones y
 * vectores. La salida debe ser igual con cc -O0, cc -O1 y GCC. */
#ifndef __SAGUIOS__
#include <stdio.h>   /* con GCC en el ordenador de desarrollo */
#endif
int g_a[16];
int g_n = 7;
unsigned g_u = 4000000000u;
char g_c[8];
short g_s[8];
struct par { int x; int y; char c; int v[4]; };
struct par g_p;
struct par g_ps[3];

int acc;

void mostrar(int id, int v) { printf("%d:%d\n", id, v); }

int f_div(int x)
{
    int r = 0;
    r += x / 1; r += x / 2; r += x / 4; r += x / 8; r += x / 16; r += x / 1024;
    r += x % 1; r += x % 2; r += x % 4; r += x % 8; r += x % 16; r += x % 1024;
    r += x / 3; r += x % 3; r += x / 10; r += x % 10; r += x / -4; r += x % -4; r += x / -1;
    return r;
}

unsigned f_udiv(unsigned x)
{
    unsigned r = 0;
    r += x / 1; r += x / 2; r += x / 4; r += x / 256; r += x / 0x80000000u;
    r += x % 1; r += x % 2; r += x % 8; r += x % 256; r += x % 3; r += x / 7;
    return r;
}

int f_mul(int x)
{
    int r = 0;
    r += x * 0; r += x * 1; r += x * 2; r += x * 3; r += x * 4; r += x * 5; r += x * 8; r += x * 9;
    r += x * 10; r += x * 100; r += x * -1; r += x * -8; r += x * 200; r += 3 * x; r += 7 * x; r += 0 * x; r += 1 * x;
    r += x + 0; r += x - 0; r += x | 0; r += x ^ 0; r += x & -1; r += x & 0; r += x << 0; r += x >> 0;
    r += x << 3; r += x >> 2; r += x & 255; r += x | 255; r += x ^ 0x5555;
    r += x & 100000; r += x + 100000; r += x - 100000; r += x * 100000;
    return r;
}

int f_const(void)
{
    int a = 2 + 3, b = 7 * 6, c = 100 - 1 - 2, d = (1 + 2) * (3 + 4), e = 5 & 3 | 8 ^ 2;
    return a + b * 1000 + c * 100000 + d + e;
}

int f_cmp(int x, int y)
{
    int r = 0;
    if (x < y) r += 1;
    if (x <= y) r += 2;
    if (x > y) r += 4;
    if (x >= y) r += 8;
    if (x == y) r += 16;
    if (x != y) r += 32;
    if (x < 5) r += 64;
    if (5 < x) r += 128;
    if (5 >= x) r += 256;
    if (x == 7) r += 512;
    if (7 != x) r += 1024;
    if (3 > y) r += 2048;
    if (g_n < x) r += 4096;
    if (x < g_n) r += 8192;
    if (x == g_n) r += 16384;
    r += (x < y) * 100000 + (x >= g_n) * 200000;
    return r;
}

int f_ucmp(unsigned x, unsigned y)
{
    int r = 0;
    if (x < y) r += 1;
    if (x > y) r += 2;
    if (x <= 5u) r += 4;
    if (x >= g_u) r += 8;
    if (g_u > y) r += 16;
    return r;
}

int f_logico(int a, int b, int c)
{
    int r = 0;
    if (a && b) r += 1;
    if (a || b) r += 2;
    if (a && b && c) r += 4;
    if (a || b || c) r += 8;
    if (a && (b || c)) r += 16;
    if ((a || b) && c) r += 32;
    if (a && b || c) r += 64;
    if (a || b && c) r += 128;
    if (!(a && b)) r += 256;
    if (!(a || b)) r += 512;
    if ((a > 1 || b < 2) && (c != 3 || a == b)) r += 1024;
    if (a > 0 && b > 0 && c > 0 || a < 0 && b < 0) r += 2048;
    r += (a && b) * 10000 + (a || c) * 20000;
    r += (a && b) ? 100000 : 200000;
    r += (a || b) ? 1000000 : 2000000;
    int w = 0;
    while (a > 0 && b > 0) { a--; b--; w++; }
    r += w * 7;
    w = 0;
    while (a < 3 || b < 3) { a++; b++; w++; if (w > 10) break; }
    r += w * 13;
    int i;
    for (i = 0; i < 10 && c > 0; i++) c--;
    r += i;
    for (i = 0; i < 10 || c < 5; i++) { c++; if (i > 7) break; }
    r += i * 3 + c;
    do { w--; } while (w > 0 && w % 3 != 0);
    r += w;
    return r;
}

int f_vec(int n)
{
    int loc[10];
    int i, s = 0;
    for (i = 0; i < 10; i++) loc[i] = i * i + n;
    for (i = 0; i < 16; i++) g_a[i] = i * 3 - n;
    for (i = 0; i < 8; i++) { g_c[i] = (char)(i + n * 40); g_s[i] = (short)(i * 1000 + n * 5000); }
    for (i = 0; i < 10; i++) s += loc[i] * 3 + g_a[i] + g_c[i % 8] + g_s[i % 8];
    s += loc[3] + loc[9] + g_a[0] + g_a[15] + g_c[2] + g_s[7];
    loc[2] = 5; g_a[3] = 9; loc[4] += 3; g_a[5] -= 2; g_a[6]++; --loc[7]; loc[8]++; g_c[1] = 77; g_s[2] = -3;
    s += loc[2] + g_a[3] + loc[4] + g_a[5] + g_a[6] + loc[7] + loc[8] + g_c[1] + g_s[2];
    int *p = loc;
    s += p[1] + p[i - 1] + *(p + 3) + *p;
    int *q = &g_a[4];
    s += q[0] + q[2] + q[-1];
    char *cp = g_c; short *sp = g_s;
    s += cp[3] + sp[3] + cp[i - 5] + sp[i - 6];
    int m2[3][4];
    int j;
    for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) m2[i][j] = i * 10 + j + n;
    for (i = 0; i < 3; i++) for (j = 0; j < 4; j++) s += m2[i][j] * (i + 1) + m2[2][3] + m2[1][j];
    return s;
}

int f_struct(int n)
{
    struct par l, ls[2];
    int i, s = 0;
    l.x = n; l.y = n * 2; l.c = (char)n; for (i = 0; i < 4; i++) l.v[i] = i + n;
    g_p.x = l.y; g_p.y = l.x + 1; g_p.c = 5; for (i = 0; i < 4; i++) g_p.v[i] = l.v[i] * 2;
    ls[0] = l; ls[1].x = 3; ls[1].y = 4; ls[1].c = 1; ls[1].v[2] = 8;
    g_ps[1].x = 10; g_ps[2].y = 11; g_ps[0].v[3] = 12; g_ps[1].v[1] = n;
    s += l.x + l.y + l.c + l.v[2] + g_p.x + g_p.y + g_p.c + g_p.v[3];
    s += ls[0].x + ls[1].y + ls[1].v[2] + ls[0].v[3] + g_ps[1].x + g_ps[2].y + g_ps[0].v[3] + g_ps[1].v[1];
    struct par *pp = &l;
    pp->x += 5; pp->v[1] = 40; s += pp->x + pp->v[1] + pp->c;
    l.x++; l.v[0] += 2; --g_p.y; s += l.x + l.v[0] + g_p.y;
    return s;
}

int f_incr(int n)
{
    int i = n, j = n * 2, k;
    i++; ++i; j--; --j; i += 3; j -= 4; i <<= 1; j >>= 1; i *= 3; j /= 2; i %= 7; j |= 8; i &= 13; j ^= 5;
    k = i++ + ++j;
    k += j-- - --i;
    int a = 0, b = 0;
    for (a = 0; a < 5; a++) b += a;
    for (a = 5; a > 0; --a) b += a * a;
    g_n++; g_n--; ++g_n; g_n += 2; g_n -= 2; g_n--;
    char c = 120; c += 10; c++;
    short s = 32000; s += 1000; s++;
    unsigned u = 5; u -= 7; u++;
    return i + j * 100 + k * 10000 + b + c + s + (int)(u & 0xFFFF);
}

int f_switch(int x)
{
    int r = 0, i;
    for (i = 0; i < 6; i++) {
        switch (x + i) {
        case 0: r += 1; break;
        case 1: r += 2;
        case 2: r += 4; break;
        case 5: r += 8; break;
        default: r += 16;
        }
    }
    return r;
}

int f_chain(int x)
{
    int r = x;
    r = r + 1 + 2 + 3;
    r = r * 2 * 2 * 2;
    r = r - 1 - 1;
    r = (r + 3) / 4 + (r - 3) % 4;
    r = (x > 0 ? r : -r) + (x == 0 ? 100 : 200);
    r = r / 2 + r / 4 + r / 8 + r % 8;
    return r;
}

int g_calls;
int efecto(int v) { g_calls++; return v; }

int f_corto(int a)
{
    int r = 0;
    g_calls = 0;
    if (efecto(a) && efecto(a - 1)) r += 1;
    if (efecto(a) || efecto(a - 1)) r += 2;
    if (efecto(0) && efecto(1)) r += 4;
    if (efecto(1) || efecto(1)) r += 8;
    return r * 100 + g_calls;
}

int main(void)
{
    int v[] = {0, 1, -1, 2, -2, 3, -3, 7, -7, 8, -8, 9, 15, -15, 16, -16, 17, 100, -100, 1023, -1023, 1024, -1025,
               65535, -65536, 123456789, -123456789, 2147483647, -2147483647};
    unsigned uv[] = {0, 1, 2, 3, 7, 8, 255, 256, 257, 65535, 1000000, 2147483647u, 2147483648u, 4000000000u, 4294967295u};
    int i, j, n = (int)(sizeof v / sizeof v[0]), un = (int)(sizeof uv / sizeof uv[0]);
    for (i = 0; i < n; i++) {
        printf("v=%d div=%d mul=%d chain=%d switch=%d\n", v[i], f_div(v[i]), f_mul(v[i]), f_chain(v[i] % 1000), f_switch(v[i] % 8));
        printf("vec=%d struct=%d incr=%d corto=%d\n", f_vec(v[i] % 100), f_struct(v[i] % 100), f_incr(v[i] % 1000), f_corto(v[i] % 3));
    }
    for (i = 0; i < un; i++) printf("u=%u %u\n", uv[i], f_udiv(uv[i]));
    for (i = 0; i < n; i += 2)
        for (j = 0; j < n; j += 3) {
            printf("%d,%d cmp=%d lg=%d\n", v[i], v[j], f_cmp(v[i], v[j]), f_logico(v[i] % 5, v[j] % 5, (v[i] + v[j]) % 5));
        }
    for (i = 0; i < un; i++) for (j = 0; j < un; j += 2) printf("ucmp=%d\n", f_ucmp(uv[i], uv[j]));
    printf("const=%d\n", f_const());
    return 0;
}
