/* numeros.c - conversión exacta entre reales y texto (D210, mejora 094 A).
 *
 * strtod/strtof con redondeo correcto (al par en los empates, el doble más
 * cercano siempre) y el cuerpo de %e/%f/%g de printf con las cifras exactas
 * del doble, redondeadas como la glibc. QuickJS y GNU Prolog lo necesitan:
 * un real escrito con %.17g y vuelto a leer tiene que ser el mismo, y
 * write(0.1) en Prolog debe dar 0.10000000000000001.
 *
 * Es código nuestro (no de terceros). Todo es aritmética entera con enteros
 * grandes de palabras de 32 bits; no hay cuentas en coma flotante, así que da
 * lo mismo con la x87 (precisión extendida) que con SSE2. No hay datos
 * estáticos: sirve con hilos.
 *
 *  - real -> texto: el doble es m * 2^e; su desarrollo decimal completo es
 *    m << e (e >= 0) o m * 5^-e con la coma -e posiciones a la izquierda
 *    (e < 0), como mucho 767 cifras significativas. Se redondea sobre esas
 *    cifras exactas.
 *  - texto -> real: el número leído es D * 10^q (D con todas sus cifras, hasta
 *    800; si hay más, las que sobran solo cuentan como «hay algo detrás»,
 *    que basta porque un punto medio entre dos dobles tiene como mucho 767
 *    cifras). Se escribe como (D * 5^q) * 2^q, se divide a 64 bits de
 *    cociente con el resto como bit pegajoso y se redondea en binario.
 *
 * La idea viene del numeros_gp.c del port de GNU Prolog (D189), que ya hacía
 * las cifras exactas; el strtod de aquí no usa aproximación en long double.
 * Compila igual en el PC para las pruebas (tests/h210_numeros_libc.py).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "numeros_libc.h"

/* ------------------------------------------------------------- enteros grandes */

/* 3200 bits: lo más grande es 5^1131 (2627 bits) desplazado 63 bits en strtod
 * y m * 5^1074 (2550 bits) en las cifras exactas. */
#define LIMBS 100

typedef struct {
    int n;                  /* palabras en uso (sin ceros arriba); 0 = cero */
    uint32_t d[LIMBS];      /* d[0] es la palabra baja */
} grande_t;

static void g_poe(grande_t *g, uint64_t v)
{
    g->n = 0;
    while (v) { g->d[g->n++] = (uint32_t)v; v >>= 32; }
}

/* g = g * m + soma */
static void g_mul_soma(grande_t *g, uint32_t m, uint32_t soma)
{
    uint64_t c = soma;
    for (int i = 0; i < g->n; i++) {
        c += (uint64_t)g->d[i] * m;
        g->d[i] = (uint32_t)c;
        c >>= 32;
    }
    if (c && g->n < LIMBS) g->d[g->n++] = (uint32_t)c;
}

/* g = g * 5^k (5^13 es la mayor potencia que cabe en 32 bits) */
static void g_mul_pot5(grande_t *g, int k)
{
    static const uint32_t pot5[14] = {
        1u, 5u, 25u, 125u, 625u, 3125u, 15625u, 78125u, 390625u, 1953125u,
        9765625u, 48828125u, 244140625u, 1220703125u
    };
    while (k >= 13) { g_mul_soma(g, pot5[13], 0); k -= 13; }
    if (k > 0) g_mul_soma(g, pot5[k], 0);
}

static void g_shl(grande_t *g, int bits)
{
    if (!g->n || bits <= 0) return;
    int palavras = bits / 32, r = bits % 32;
    if (r) {
        uint32_t c = 0;
        for (int i = 0; i < g->n; i++) {
            uint32_t v = g->d[i];
            g->d[i] = (v << r) | c;
            c = v >> (32 - r);
        }
        if (c && g->n < LIMBS) g->d[g->n++] = c;
    }
    if (palavras) {
        int n = g->n + palavras;
        if (n > LIMBS) n = LIMBS;
        for (int i = n - 1; i >= palavras; i--) g->d[i] = g->d[i - palavras];
        for (int i = 0; i < palavras; i++) g->d[i] = 0;
        g->n = n;
    }
}

static void g_shr1(grande_t *g)
{
    for (int i = 0; i < g->n; i++) {
        g->d[i] >>= 1;
        if (i + 1 < g->n) g->d[i] |= g->d[i + 1] << 31;
    }
    while (g->n && !g->d[g->n - 1]) g->n--;
}

static int g_bits(const grande_t *g)
{
    if (!g->n) return 0;
    return (g->n - 1) * 32 + 32 - __builtin_clz(g->d[g->n - 1]);
}

static int g_cmp(const grande_t *a, const grande_t *b)
{
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

/* a = a - b, con a >= b */
static void g_sub(grande_t *a, const grande_t *b)
{
    uint32_t emp = 0;
    for (int i = 0; i < a->n; i++) {
        uint64_t s = (uint64_t)(i < b->n ? b->d[i] : 0) + emp;
        emp = (uint64_t)a->d[i] < s;
        a->d[i] = (uint32_t)((uint64_t)a->d[i] - s);
    }
    while (a->n && !a->d[a->n - 1]) a->n--;
}

/* Divide por d (pequeño) y devuelve el resto. */
static uint32_t g_div_pequeno(grande_t *g, uint32_t d)
{
    uint64_t r = 0;
    for (int i = g->n - 1; i >= 0; i--) {
        r = (r << 32) | g->d[i];
        g->d[i] = (uint32_t)(r / d);
        r %= d;
    }
    while (g->n && !g->d[g->n - 1]) g->n--;
    return (uint32_t)r;
}

/* Cociente de num / den cuando num tiene 63 bits más que den: queda en
 * [2^62, 2^64). *resto dice si la división no fue exacta. num se pierde. */
static uint64_t g_dividir(grande_t *num, const grande_t *den, int *resto)
{
    grande_t ds = *den;
    g_shl(&ds, 63);
    uint64_t q = 0;
    for (int i = 63; i >= 0; i--) {
        if (g_cmp(num, &ds) >= 0) {
            g_sub(num, &ds);
            q |= (uint64_t)1 << i;
        }
        if (i) g_shr1(&ds);
    }
    *resto = num->n != 0;
    return q;
}

/* ------------------------------------------------------------- texto -> real */

/* V = (q + f) * 2^e2, con q != 0 y f en [0, 1) (resto: f != 0), redondeado
 * al formato de «prec» bits de mantisa (con el bit implícito) y exponentes
 * normales [emin, emax]. Devuelve los bits sin el signo. Sin resto, q se
 * puede normalizar; con resto, q ya trae 61 bits o más (lo garantiza quien
 * llama), así que siempre sobran bits para redondear. */
static uint64_t arredondar_bin(uint64_t q, int resto, int e2, int prec, int emin, int emax)
{
    if (!resto) while (!(q >> 63)) { q <<= 1; e2--; }
    int lq = 64 - __builtin_clzll(q);
    int p = lq - 1 + e2;                       /* V en [2^p, 2^(p+1)) */
    int fica = p >= emin ? prec : prec - (emin - p);
    if (fica < 0) { errno = ERANGE; return 0; }   /* menos de medio subnormal */
    int tira = lq - fica;                      /* entre 8 y 64 */
    uint64_t mant, sobra, meio;
    if (tira >= 64) { mant = 0; sobra = q; meio = (uint64_t)1 << 63; }
    else {
        mant = q >> tira;
        sobra = q & (((uint64_t)1 << tira) - 1);
        meio = (uint64_t)1 << (tira - 1);
    }
    int inexato = sobra || resto;
    if (sobra > meio || (sobra == meio && (resto || (mant & 1)))) mant++;
    if (p >= emin) {
        if (mant >> prec) { mant >>= 1; p++; }
        if (p > emax) { errno = ERANGE; return (uint64_t)(2 * emax + 1) << (prec - 1); }
        return ((uint64_t)(p + emax) << (prec - 1)) | (mant & (((uint64_t)1 << (prec - 1)) - 1));
    }
    /* Subnormal (o cero). Si el redondeo lo sube al primer normal, los bits
     * ya son los suyos (exponente 1, mantisa 0). ERANGE si no es exacto y es
     * «diminuto tras redondear», como la glibc (y la x87): redondeado a «prec»
     * bits sin límite de exponente sigue por debajo del primer normal. */
    if (inexato) {
        int diminuto = 1;
        if (p == emin - 1) {
            int t2 = lq - prec;
            uint64_t m2 = q >> t2, s2 = q & (((uint64_t)1 << t2) - 1), h2 = (uint64_t)1 << (t2 - 1);
            if (s2 > h2 || (s2 == h2 && (resto || (m2 & 1)))) m2++;
            if (m2 >> prec) diminuto = 0;
        }
        if (diminuto) errno = ERANGE;
    }
    return mant;
}

static int e_espaco(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
static int e_digito(int c) { return c >= '0' && c <= '9'; }

static int valor_hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c |= 0x20;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* ¿Empieza p por t (t en minúsculas), sin mirar mayúsculas? */
static int comeca(const char *p, const char *t)
{
    for (; *t; p++, t++) if ((*p | 0x20) != *t) return 0;
    return 1;
}

/* Lee el exponente (tras la 'e' o la 'p'); si no hay cifras no lo toca. */
static const char *ler_expoente(const char *p, int *valor)
{
    const char *e = p + 1;
    int neg = 0, v = 0, tem = 0;
    if (*e == '+' || *e == '-') { neg = *e == '-'; e++; }
    for (; e_digito(*e); e++) { tem = 1; if (v < 100000) v = v * 10 + (*e - '0'); }
    if (!tem) return p;
    *valor = neg ? -v : v;
    return e;
}

#define MAX_SIG 800   /* cifras significativas que se guardan; las demás solo «pegan» */

/* Lee un real de «total» bits (64 o 32) y devuelve sus bits. */
static uint64_t ler_real(const char *s, char **fim, int prec, int emin, int emax, int total)
{
    const char *p = s;
    while (e_espaco((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    uint64_t sinal = neg ? (uint64_t)1 << (total - 1) : 0;
    uint64_t inf = (uint64_t)(2 * emax + 1) << (prec - 1);
    if (comeca(p, "inf")) {
        p += 3;
        if (comeca(p, "inity")) p += 5;
        if (fim) *fim = (char *)p;
        return sinal | inf;
    }
    if (comeca(p, "nan")) {
        p += 3;
        if (*p == '(') {   /* nan(cifras-letras-_) */
            const char *q = p + 1;
            while (e_digito(*q) || ((*q | 0x20) >= 'a' && (*q | 0x20) <= 'z') || *q == '_') q++;
            if (*q == ')') p = q + 1;
        }
        if (fim) *fim = (char *)p;
        return sinal | inf | (uint64_t)1 << (prec - 2);
    }

    /* Hexadecimal: 0x1.8p3. Si tras «0x» no hay cifras, vale el «0». */
    if (p[0] == '0' && (p[1] | 0x20) == 'x') {
        const char *h = p + 2;
        uint64_t q = 0;
        int nsig = 0, resto = 0, e2 = 0, viu = 0, ponto = 0;
        for (;; h++) {
            if (*h == '.' && !ponto) { ponto = 1; continue; }
            int c = valor_hex((unsigned char)*h);
            if (c < 0) break;
            viu = 1;
            if (nsig == 0 && c == 0) { if (ponto) e2 -= 4; continue; }
            if (nsig < 16) { q = q << 4 | (uint64_t)c; nsig++; if (ponto) e2 -= 4; }
            else { if (c) resto = 1; if (!ponto) e2 += 4; }
        }
        if (viu) {
            int ep = 0;
            if ((*h | 0x20) == 'p') h = ler_expoente(h, &ep);
            if (fim) *fim = (char *)h;
            if (!q) return sinal;
            return sinal | arredondar_bin(q, resto, e2 + ep, prec, emin, emax);
        }
    }

    char dig[MAX_SIG + 1];
    int n = 0, q10 = 0, pega = 0, viu = 0;
    for (; e_digito(*p); p++) {
        viu = 1;
        if (n == 0 && *p == '0') continue;
        if (n < MAX_SIG) dig[n++] = *p;
        else { q10++; if (*p != '0') pega = 1; }
    }
    if (*p == '.') {
        const char *ponto = p++;
        int viu_frac = 0;
        for (; e_digito(*p); p++) {
            viu_frac = 1;
            if (n == 0 && *p == '0') { q10--; continue; }
            if (n < MAX_SIG) { dig[n++] = *p; q10--; }
            else if (*p != '0') pega = 1;
        }
        if (!viu && !viu_frac) p = ponto;
        viu |= viu_frac;
    }
    if (!viu) { if (fim) *fim = (char *)s; return 0; }
    if ((*p | 0x20) == 'e') {
        int ev = 0;
        p = ler_expoente(p, &ev);
        q10 += ev;
    }
    if (fim) *fim = (char *)p;
    if (pega) { dig[n++] = '1'; q10--; }   /* lo que sobraba: un poco más */
    while (n > 0 && dig[n - 1] == '0') { n--; q10++; }
    if (n == 0) return sinal;
    int ordem = n + q10;                    /* el número está en [10^(ordem-1), 10^ordem) */
    if (ordem > 310) { errno = ERANGE; return sinal | inf; }
    if (ordem < -330) { errno = ERANGE; return sinal; }

    /* D en un entero grande, de nueve en nueve cifras */
    grande_t num, den;
    g_poe(&num, 0);
    for (int i = 0; i < n; ) {
        uint32_t bloco = 0, mult = 1;
        for (int k = 0; k < 9 && i < n; k++, i++) { bloco = bloco * 10 + (uint32_t)(dig[i] - '0'); mult *= 10; }
        g_mul_soma(&num, mult, bloco);
    }
    /* D * 10^q = (D * 5^q) * 2^q: num / den por 2^q */
    g_poe(&den, 1);
    if (q10 >= 0) g_mul_pot5(&num, q10);
    else g_mul_pot5(&den, -q10);
    int desl = g_bits(&den) + 63 - g_bits(&num);
    if (desl > 0) g_shl(&num, desl);
    else if (desl < 0) g_shl(&den, -desl);
    int resto;
    uint64_t q = g_dividir(&num, &den, &resto);
    return sinal | arredondar_bin(q, resto, q10 - desl, prec, emin, emax);
}

double strtod(const char *s, char **fim)
{
    uint64_t b = ler_real(s, fim, 53, -1022, 1023, 64);
    double d;
    memcpy(&d, &b, sizeof d);
    return d;
}

float strtof(const char *s, char **fim)
{
    uint32_t b = (uint32_t)ler_real(s, fim, 24, -126, 127, 32);
    float f;
    memcpy(&f, &b, sizeof f);
    return f;
}

double atof(const char *s) { return strtod(s, NULL); }

/* ------------------------------------------------------------- real -> texto */

#define DIG_TAM 800   /* m * 5^1074 tiene como mucho 767 cifras: 86 grupos de 9 */

/* Cifras exactas del doble positivo y finito de bits u (no cero):
 * v = 0.d1d2...dn * 10^x, sin ceros al final. Devuelve n. */
static int digitos_exatos(uint64_t u, char *dig, int *x)
{
    int eb = (int)((u >> 52) & 0x7FF);
    uint64_t m = u & (((uint64_t)1 << 52) - 1);
    int e;
    if (eb) { m |= (uint64_t)1 << 52; e = eb - 1075; }
    else e = -1074;
    while (!(m & 1) && e < 0) { m >>= 1; e++; }
    grande_t g;
    g_poe(&g, m);
    int p10 = 0;
    if (e > 0) g_shl(&g, e);
    else if (e < 0) { g_mul_pot5(&g, -e); p10 = e; }   /* m*2^e = m*5^-e / 10^-e */
    int ini = DIG_TAM;
    while (g.n) {
        uint32_t r = g_div_pequeno(&g, 1000000000u);
        for (int i = 0; i < 9; i++) { dig[--ini] = (char)('0' + r % 10); r /= 10; }
    }
    while (dig[ini] == '0') ini++;
    int n = DIG_TAM - ini;
    memmove(dig, dig + ini, (size_t)n);
    int ceros = 0;
    while (n > 1 && dig[n - 1] == '0') { n--; ceros++; }
    *x = n + ceros + p10;
    return n;
}

/* Redondea dig[0..n) a «quero» cifras (al par en el empate) y deja en *pn las
 * que quedan (las que faltan detrás son ceros). Devuelve 1 si sale una cifra
 * más delante (999 -> 1000: la coma se mueve). quero <= 0: el número es menor
 * que la unidad de esa posición (con quero == 0 puede subir a 1). */
static int arredondar(char *dig, int *pn, int quero)
{
    int n = *pn;
    if (quero >= n) return 0;
    if (quero < 0) { *pn = 0; return 0; }
    char c = dig[quero];
    int sobe;
    if (c > '5') sobe = 1;
    else if (c < '5') sobe = 0;
    else {
        sobe = 0;
        for (int i = quero + 1; i < n; i++) if (dig[i] != '0') { sobe = 1; break; }
        if (!sobe) sobe = quero > 0 && ((dig[quero - 1] - '0') & 1);   /* empate: al par */
    }
    *pn = quero;
    if (!sobe) return 0;
    for (int i = quero - 1; i >= 0; i--) {
        if (dig[i] < '9') { dig[i]++; *pn = i + 1; return 0; }
    }
    dig[0] = '1';   /* eran todo nueves (o quero == 0) */
    *pn = 1;
    return 1;
}

int __libc_real_texto(char *buf, double v, char conv, int prec, int alt)
{
    uint64_t u;
    memcpy(&u, &v, sizeof u);
    int maius = conv == 'E' || conv == 'F' || conv == 'G';
    char lc = (char)(conv | 0x20);
    u &= ~((uint64_t)1 << 63);   /* el signo lo pone quien llama */
    if ((u >> 52) == 0x7FF) {
        const char *t = (u << 12) ? (maius ? "NAN" : "nan") : (maius ? "INF" : "inf");
        memcpy(buf, t, 4);
        return 3;
    }
    if (prec < 0) prec = 6;
    if (prec > LIBC_REAL_PREC_MAX) prec = LIBC_REAL_PREC_MAX;
    char dig[DIG_TAM];
    int n = 0, x = 1;
    if (u) n = digitos_exatos(u, dig, &x);
#define CIFRA(i) ((i) >= 0 && (i) < n ? dig[i] : '0')
    int tira_zeros = 0;
    if (lc == 'g') {
        int P = prec ? prec : 1;
        int ex = 0;   /* exponente de la notación científica tras redondear a P cifras */
        if (n) { if (arredondar(dig, &n, P)) x++; ex = x - 1; }
        if (P > ex && ex >= -4) { lc = 'f'; prec = P - 1 - ex; }
        else { lc = 'e'; prec = P - 1; }
        tira_zeros = !alt;
    }
    int k = 0;
    if (lc == 'e') {
        int ex = 0;
        if (n) { if (arredondar(dig, &n, prec + 1)) x++; ex = x - 1; }
        buf[k++] = CIFRA(0);
        int frac = prec;
        if (tira_zeros) while (frac > 0 && CIFRA(frac) == '0') frac--;
        if (frac > 0 || alt) buf[k++] = '.';
        for (int i = 1; i <= frac; i++) buf[k++] = CIFRA(i);
        buf[k++] = maius ? 'E' : 'e';
        buf[k++] = ex < 0 ? '-' : '+';
        int ae = ex < 0 ? -ex : ex;
        if (ae >= 100) buf[k++] = (char)('0' + ae / 100);
        buf[k++] = (char)('0' + ae / 10 % 10);
        buf[k++] = (char)('0' + ae % 10);
    } else {   /* 'f' */
        if (n && arredondar(dig, &n, x + prec)) x++;
        int ent = n && x > 0 ? x : 0;   /* cifras enteras */
        if (!ent) buf[k++] = '0';
        for (int i = 0; i < ent; i++) buf[k++] = CIFRA(i);
        int frac = prec;
        if (tira_zeros) while (frac > 0 && (!n || CIFRA(x + frac - 1) == '0')) frac--;
        if (frac > 0 || alt) buf[k++] = '.';
        for (int i = 0; i < frac; i++) buf[k++] = n ? CIFRA(x + i) : '0';
    }
#undef CIFRA
    buf[k] = 0;
    return k;
}
