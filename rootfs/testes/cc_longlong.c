// SPDX-License-Identifier: GPL-2.0-or-later
/* cc_longlong.c - prueba do long long no compilador de C (cc) do SaguiOS.
 *
 *     cc /testes/cc_longlong.c -o /tmp/ll
 *     /tmp/ll
 *
 * O mesmo fonte compila com o GCC e da as mesmas linhas: a prova b8 (?) compara
 * as duas saidas, dentro do SaguiOS. */
#include <stdio.h>

struct ponto { long long x; long long y; };

static long long soma(long long a, long long b) { return a + b; }
static unsigned long long usoma(unsigned long long a, unsigned long long b) { return a + b; }
static long long dobro(long long a) { return a * 2; }

int main(void)
{
    long long a = 1234567890123LL;
    long long b = 1000000007LL;
    long long negativo = -1234567890123LL;
    unsigned long long u = 18446744073709551615ULL;
    unsigned long long u2 = 12345678901234567890ULL;

    printf("a = %lld\n", a);
    printf("b = %lld\n", b);
    printf("negativo = %lld\n", negativo);
    printf("u = %llu\n", u);
    printf("a+b = %lld\n", a + b);
    printf("a-b = %lld\n", a - b);
    printf("a*b = %lld\n", a * b);
    printf("a/b = %lld\n", a / b);
    printf("a%%b = %lld\n", a % b);
    printf("neg/b = %lld\n", negativo / b);
    printf("neg%%b = %lld\n", negativo % b);
    printf("a<<10 = %lld\n", a << 10);
    printf("a>>3 = %lld\n", a >> 3);
    printf("neg>>3 = %lld\n", negativo >> 3);
    printf("u>>3 = %llu\n", u >> 3);
    printf("a&b = %lld\n", a & b);
    printf("a|b = %lld\n", a | b);
    printf("a^b = %lld\n", a ^ b);
    printf("a>b = %d\n", a > b);
    printf("a<b = %d\n", a < b);
    printf("a==b = %d\n", a == b);
    printf("a!=b = %d\n", a != b);
    printf("neg<b = %d\n", negativo < b);
    printf("u>0 = %d\n", u > 0);
    printf("sizeof = %d\n", (int)sizeof(long long));

    /* Sem sinal: divisao, resto e comparacoes. */
    printf("u2 = %llu\n", u2);
    printf("u2/3 = %llu\n", u2 / 3);
    printf("u2%%3 = %llu\n", u2 % 3);
    printf("u2>>10 = %llu\n", u2 >> 10);
    printf("u2<<1 = %llu\n", u2 << 1);
    printf("u>u2 = %d\n", u > u2);
    printf("u<u2 = %d\n", u < u2);
    printf("usoma = %llu\n", usoma(u2, 1000ULL));

    /* O sinal do quociente vai com o dividendo (como em C89). */
    printf("b%%neg = %lld\n", b % negativo);
    printf("b/neg = %lld\n", b / negativo);
    printf("7/-2 = %lld\n", 7LL / -2LL);
    printf("7%%-2 = %lld\n", 7LL % -2LL);
    printf("-7/2 = %lld\n", -7LL / 2LL);
    printf("-7%%2 = %lld\n", -7LL % 2LL);

    /* Conversoes com double. */
    printf("double = %f\n", (double)a);
    printf("de = %lld\n", (long long)1234.75);
    printf("trunc = %d\n", (int)a);
    printf("deu = %llu\n", (unsigned long long)a);

    /* Condicional e operadores logicos com 64 bits. */
    printf("max = %lld\n", (a > b) ? a : b);
    printf("and = %d %d\n", a && b, !a);

    /* Estruturas. */
    struct ponto p;
    p.x = a;
    p.y = b;
    printf("p = %lld %lld\n", p.x, p.y);

    /* Compostos com 64 bits. */
    long long c = a;
    c += 1000;
    printf("c = %lld\n", c);
    c -= 7;
    printf("c = %lld\n", c);
    c *= 3;
    printf("c = %lld\n", c);
    c /= 2;
    printf("c = %lld\n", c);
    c++;
    printf("c = %lld\n", c);
    --c;
    printf("c = %lld\n", c);
    c = -c;
    printf("c = %lld\n", c);
    c = ~c;
    printf("c = %lld\n", c);

    long long d2 = 10;
    d2 += a;
    d2 -= b;
    d2 *= 2;
    d2 /= 3;
    d2 %= 1000003;
    printf("d2 = %lld\n", d2);
    unsigned long long ud = 10;
    ud += u2;
    ud /= 7;
    printf("ud = %llu\n", ud);

    /* Incrementos. */
    long long i = 5;
    i++;
    ++i;
    i--;
    --i;
    printf("i = %lld\n", i);
    unsigned long long ui = 5;
    ui++;
    printf("ui = %llu\n", ui);

    /* Deslocamentos com conta variavel. */
    int k = 7;
    printf("a<<k = %lld\n", a << k);
    printf("a>>k = %lld\n", a >> k);
    printf("neg>>k = %lld\n", negativo >> k);
    printf("a<<40 = %lld\n", a << 40);
    printf("a<<63 = %lld\n", a << 63);

    /* Deslocamentos compostos. */
    long long s = a;
    s <<= 3;
    printf("s = %lld\n", s);
    s >>= 2;
    printf("s = %lld\n", s);
    unsigned long long t = 4000000000ULL;
    t <<= 5;
    printf("t = %llu\n", t);
    printf("hex = %llx oct = %llo\n", t, t);

    /* Conversao implicita de double para long long. */
    long long x = 1.5;
    printf("x = %lld\n", x);

    /* Contador de 64 bits. */
    for (long long i = 0; i < 3; i++) printf("i = %lld\n", i);

    /* Numeros grandes escritos a mao. */
    printf("max = %lld\n", 9223372036854775807LL);
    printf("min = %lld\n", -9223372036854775807LL - 1);
    printf("umax = %llu\n", 18446744073709551615ULL);

    /* Funcoes. */
    printf("soma = %lld\n", soma(a, b));
    printf("dobro = %lld\n", dobro(a));

    /* Vetores. */
    long long v[4];
    v[0] = a;
    v[1] = v[0] + 1;
    v[2] = v[1] * 2;
    v[3] = v[2] - v[0];
    printf("v = %lld %lld %lld %lld\n", v[0], v[1], v[2], v[3]);

    /* Um int se estende. */
    int j = 5;
    long long d = j;
    printf("d = %lld\n", d);
    d = a + j;
    printf("d = %lld\n", d);
    printf("int = %d\n", (int)a);
    return 0;
}
