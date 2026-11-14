// SPDX-License-Identifier: GPL-2.0-or-later
/* rapido.c - memcpy, memset, memmove y píxeles rápidos para los programas (D042).
 *
 * Hasta aquí las copias eran byte a byte. Ahora:
 *   - bloques pequeños (menos de 16 bytes): byte a byte, sin coste fijo;
 *   - bloques medios, o CPU sin SSE2: "rep movsd" / "rep stosd" (cualquier
 *     i686; con fast strings/ERMSB, como el G470, la CPU copia en bloques);
 *   - bloques a partir de LIMIAR_SSE2 bytes, si CPUID dice que hay SSE2:
 *     las rutinas de kernel/lib/sse2.c (mismo código del kernel), compiladas
 *     aparte con -msse2. El resto de libsagui sigue -mgeneral-regs-only.
 *
 * Por qué un umbral: la primera instrucción SSE de un proceso genera una
 * excepción #NM y de ahí en adelante el kernel guarda sus registros cuando
 * otro proceso también usa la FPU (D032). Un programa que solo copia textos
 * pequeños nunca llega a usar SSE y no paga nada de eso.
 *
 * SSE2 no siempre gana: en QEMU sin KVM (el SSE se emula), ampliar una
 * línea con punpckldq es más lento que el bucle en C; en un PC real es al contrario.
 * Por eso, en el modo automático, las AMOSTRAS primeras llamadas grandes de
 * cada función alternan las dos versiones (el resultado es el mismo) y miden los
 * ciclos (RDTSC); desde allí queda la más rápida, por proceso. El
 * método puede forzarse con mem_metodo() (el bench y la prueba H40 comparan).
 */
#include "sagui.h"
#include "sse2.h"

#define LIMIAR_SSE2 256   /* bytes: ver las medidas en D042 */
#define AMOSTRAS 8        /* llamadas medidas de cada versión antes de elegir */

/* D208: cinco variantes por función, cada una en su fichero con su -m (sse1.c, sse2.c,
 * sse3.c, ssse3.c) y llamada solo si CPUID dice que la CPU la tiene. V_GEN es x87/enteros
 * (rep movsd, bucles en C). Cada función tiene las que le aportan algo (mascara_f). */
enum { V_GEN, V_SSE, V_SSE2, V_SSE3, V_SSSE3, QTD_V };
#define B(v) (1u << (v))

static int metodo = -1;   /* MEM_AUTO, MEM_SIMPLES, MEM_REP o un nivel forzado; -1 = todavía no decidió */
static int8_t tem[QTD_V] = { 1, -1, -1, -1, -1 };

static void ler_cpuid(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    bool fx = (d >> 24) & 1;   /* sin FXSR el kernel no enciende SSE */
    tem[V_SSE]   = fx && ((d >> 25) & 1);
    tem[V_SSE2]  = fx && ((d >> 26) & 1);
    tem[V_SSE3]  = tem[V_SSE2] && (c & 1);
    tem[V_SSSE3] = tem[V_SSE3] && ((c >> 9) & 1);
}

static inline bool cpu_v(int v)
{
    if (tem[v] < 0) ler_cpuid();
    return tem[v] == 1;
}

bool cpu_tem_sse(void)   { return cpu_v(V_SSE); }
bool cpu_tem_sse2(void)  { return cpu_v(V_SSE2); }
bool cpu_tem_sse3(void)  { return cpu_v(V_SSE3); }
bool cpu_tem_ssse3(void) { return cpu_v(V_SSSE3); }

static inline int met(void)
{
    if (metodo < 0) metodo = cpu_tem_sse() ? MEM_AUTO : MEM_REP;
    return metodo;
}

/* ------------------------------------------------------------ elección medida */

/* Cada función se mide en tres franjas de tamaño (hasta 1 KB, hasta 8 KB y
 * arriba): el coste fijo de las vueltas de alineación pesa en los bloques pequeños.
 * Con más de dos candidatas, las llamadas medidas las recorren por turnos. */
enum { F_COPIAR, F_MEMSET, F_PREENCHER, F_AMPLIAR, F_TROCAR, F_MISTURAR, QTD_F };
static const uint8_t mascara_f[QTD_F] = {
    [F_COPIAR]   = B(V_GEN) | B(V_SSE) | B(V_SSE2) | B(V_SSE3),
    [F_MEMSET]   = B(V_GEN) | B(V_SSE) | B(V_SSE2),
    [F_PREENCHER] = B(V_GEN) | B(V_SSE) | B(V_SSE2),
    [F_AMPLIAR]  = B(V_GEN) | B(V_SSE) | B(V_SSE2),
    [F_TROCAR]   = B(V_GEN) | B(V_SSE2) | B(V_SSSE3),
    [F_MISTURAR] = B(V_GEN) | B(V_SSE2),
};
#define FAIXAS 3
typedef struct {
    uint16_t amostras;   /* llamadas ya medidas */
    int8_t   escolha;    /* -1 midiendo; si no, la variante elegida (V_*) */
    uint32_t melhor[QTD_V];  /* menor coste visto de cada versión (ciclos por 256 bytes) */
} calibragem_t;
static calibragem_t cal[QTD_F][FAIXAS];
static bool cal_pronta;

static void cal_iniciar(void)
{
    for (int f = 0; f < QTD_F; f++)
        for (int k = 0; k < FAIXAS; k++) {
            cal[f][k].amostras = 0;
            cal[f][k].escolha = -1;
            for (int v = 0; v < QTD_V; v++) cal[f][k].melhor[v] = 0xFFFFFFFFu;
        }
    cal_pronta = true;
}

/* Bits a uno (sin libgcc: __builtin_popcount pediría __popcountsi2). */
static inline int bits(unsigned m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

/* Variantes de la función f que la CPU puede ejecutar. */
static inline unsigned candidatas(int f)
{
    unsigned m = 0;
    for (int v = 0; v < QTD_V; v++)
        if ((mascara_f[f] & B(v)) && cpu_v(v)) m |= B(v);
    return m;
}

static inline calibragem_t *faixa(int f, size_t n)
{
    if (!cal_pronta) cal_iniciar();
    return &cal[f][n <= 1024 ? 0 : n <= 8192 ? 1 : 2];
}

static inline uint32_t tsc(void)
{
    uint32_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return a;
}

/* Nivel máximo que se pide con mem_metodo (MEM_SSE2 = 3, MEM_SSE = 4...) -> V_*. */
static inline int nivel_forcado(int m)
{
    return m == MEM_SSE ? V_SSE : m == MEM_SSE2 ? V_SSE2 : m == MEM_SSE3 ? V_SSE3 : m == MEM_SSSE3 ? V_SSSE3 : -1;
}

/* Variante que se ejecuta ahora; *medindo = 1 si esta llamada cuenta para la medida. */
static inline int versao(int f, size_t n, int *medindo)
{
    int m = met();
    *medindo = 0;
    int forcado = nivel_forcado(m);
    if (forcado >= 0) {   /* la mayor variante que la función tiene por debajo del nivel pedido */
        unsigned c = candidatas(f);
        for (int v = forcado; v > V_GEN; v--)
            if (c & B(v)) return n >= LIMIAR_SSE2 ? v : V_GEN;
        return V_GEN;
    }
    if (m != MEM_AUTO || n < LIMIAR_SSE2) return V_GEN;
    unsigned c = candidatas(f);
    if (c == B(V_GEN)) return V_GEN;
    calibragem_t *k = faixa(f, n);
    if (k->escolha >= 0) return k->escolha;
    int qtd = bits(c), vez = k->amostras % qtd;
    for (int v = 0; v < QTD_V; v++)
        if ((c & B(v)) && vez-- == 0) { *medindo = 1; return v; }
    return V_GEN;
}

static void anotar(int f, int v, uint32_t ciclos, size_t n)
{
    calibragem_t *c = faixa(f, n);
    uint32_t custo = ciclos / (uint32_t)((n >> 8) | 1);
    if (custo < c->melhor[v]) c->melhor[v] = custo;
    unsigned cand = candidatas(f);
    if (++c->amostras >= AMOSTRAS * (unsigned)bits(cand)) {
        int e = V_GEN;
        for (int w = 1; w < QTD_V; w++)
            if ((cand & B(w)) && c->melhor[w] < c->melhor[e]) e = w;
        c->escolha = (int8_t)e;
    }
}

int mem_metodo(int m)
{
    int antes = met();
    int v = nivel_forcado(m);
    if (m == MEM_AUTO) metodo = -1;
    else if (m == MEM_SIMPLES || m == MEM_REP || (v >= 0 && cpu_v(v))) metodo = m;
    return antes;
}

/* Para el bench y el infopc: en cada franja de tamaño la variante elegida
 * (r = x87/enteros, e = SSE, s = SSE2, 3 = SSE3, x = SSSE3, ? = todavía midiendo). */
int mem_escolhas(char *s, size_t max)
{
    static const char *const nomes[QTD_F] = { "memcpy", "memset", "preencher32", "ampliar2x32", "trocar_rb32", "misturar32" };
    static const char letra[QTD_V] = { 'r', 'e', 's', '3', 'x' };
    int n = 0;
    s[0] = 0;
    for (int f = 0; f < QTD_F; f++) {
        char e[FAIXAS + 1];
        bool simd = candidatas(f) != B(V_GEN);
        if (!cal_pronta) cal_iniciar();
        for (int k = 0; k < FAIXAS; k++)
            e[k] = !simd ? 'r' : cal[f][k].escolha < 0 ? '?' : letra[cal[f][k].escolha];
        e[FAIXAS] = 0;
        n += formatar_texto(s + n, max - (size_t)n, "%s%s=%s", f ? " " : "", nomes[f], e);
    }
    return n;
}

/* Cuál es la variante que gana con bloques grandes (la franja de más de 8 KB) en una
 * función: el infopc y el arranque la muestran ("copias: SSSE3"). Fuerza la medida si
 * todavía no hay (unas llamadas sobre un búfer propio). */
static const char *const nome_v[QTD_V] = { "generica (rep/C)", "SSE", "SSE2", "SSE3", "SSSE3" };

static void calibrar_ahora(void)
{
    static bool hecho;
    if (hecho) return;
    hecho = true;
    size_t n = 16384;
    uint32_t *a = aloca(n * 2 + 64), *b;
    if (!a) return;
    b = a + n / 4 + 16;
    static const size_t tam[3] = { 512, 4096, 16384 };   /* una por franja de tamaño */
    for (int k = 0; k < AMOSTRAS * 4 + 4; k++) {   /* bastan para las cuatro candidatas como mucho */
        for (int z = 0; z < 3; z++) {
            size_t t = tam[z];
            memcpy(b, a, t);
            memset(b, 0, t);
            preencher32(b, 1, t / 4);
            ampliar2x32(b, NULL, a, t / 8);
            trocar_rb32(b, a, t / 4);
            misturar32(b, a, t / 4, 100);
        }
    }
    libera(a);
}

int mem_descrever(char *s, size_t max)
{
    calibrar_ahora();
    static const int fs[] = { F_COPIAR, F_MEMSET, F_PREENCHER, F_TROCAR, F_MISTURAR };
    static const char *const nomes[] = { "copias", "memset", "relleno", "canales", "mezcla" };
    int n = 0;
    s[0] = 0;
    for (unsigned i = 0; i < sizeof fs / sizeof fs[0]; i++) {
        calibragem_t *c = faixa(fs[i], 16384);
        int v = candidatas(fs[i]) == B(V_GEN) ? V_GEN : c->escolha < 0 ? V_GEN : c->escolha;
        n += formatar_texto(s + n, max - (size_t)n, "%s%s: %s", i ? ", " : "", nomes[i], nome_v[v]);
    }
    return n;
}

/* Ejecuta la variante elegida, midiendo mientras la elección no esté hecha. */
#define DESPACHAR(f, n, gen, sse, sse2, sse3, ssse3)                         \
    do {                                                                     \
        int med_;                                                            \
        int v_ = versao(f, n, &med_);                                        \
        uint32_t t_ = med_ ? tsc() : 0;                                      \
        switch (v_) {                                                        \
        case V_SSE:   sse; break;                                            \
        case V_SSE2:  sse2; break;                                           \
        case V_SSE3:  sse3; break;                                           \
        case V_SSSE3: ssse3; break;                                          \
        default:      gen; break;                                            \
        }                                                                    \
        if (med_) anotar(f, v_, tsc() - t_, n);                              \
    } while (0)

/* ------------------------------------------------------------ copias */

static inline void rep_copiar(void *d, const void *s, size_t n)
{
    uint32_t q = n >> 2, r = n & 3;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(q) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(r) : : "memory");
}

void *memcpy(void *d, const void *s, size_t n)
{
    if (n < 16 || met() == MEM_SIMPLES) {
        uint8_t *a = d;
        const uint8_t *b = s;
        while (n--) *a++ = *b++;
        return d;
    }
    DESPACHAR(F_COPIAR, n, rep_copiar(d, s, n), sse1_copiar(d, s, n), sse2_copiar(d, s, n), sse3_copiar(d, s, n), rep_copiar(d, s, n));
    return d;
}

/* memmove hacia atrás: la variante de memcpy si ya está elegida (SSE3 usa la de SSE2: lddqu
 * solo cambia las lecturas); mientras mide, o si la elegida es x87, rep hacia atrás. */
static inline int variante_tras(size_t n)
{
    int m = met(), f = nivel_forcado(m);
    if (f >= 0) {
        if (f >= V_SSE2 && cpu_v(V_SSE2)) return V_SSE2;
        return cpu_v(V_SSE) ? V_SSE : V_GEN;
    }
    if (m != MEM_AUTO) return V_GEN;
    int e = faixa(F_COPIAR, n)->escolha;
    return e < 0 ? V_GEN : e;
}
static inline bool simd_tras(size_t n) { return variante_tras(n) != V_GEN; }

void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *a = d;
    const uint8_t *b = s;
    if (a <= b || a >= b + n) return memcpy(d, s, n);
    int m = met();
    if (m == MEM_SIMPLES || n < 16) {
        a += n; b += n;
        while (n--) *--a = *--b;
    } else if (n >= LIMIAR_SSE2 && simd_tras(n)) {
        /* sigue la elección de memcpy (no se puede medir: solapado) */
        if (variante_tras(n) == V_SSE) sse1_copiar_tras(d, s, n); else sse2_copiar_tras(d, s, n);
    } else {
        /* De atrás hacia adelante con la flag de dirección activada solo aquí. */
        void *dp = a + n - 1;
        const void *sp = b + n - 1;
        __asm__ volatile("std; rep movsb; cld" : "+D"(dp), "+S"(sp), "+c"(n) : : "memory");
    }
    return d;
}

static inline void rep_memset(void *p, int v, size_t n)
{
    uint32_t v4 = (uint8_t)v * 0x01010101u, q = n >> 2, r = n & 3;
    __asm__ volatile("rep stosl" : "+D"(p), "+c"(q) : "a"(v4) : "memory");
    __asm__ volatile("rep stosb" : "+D"(p), "+c"(r) : "a"(v4) : "memory");
}

void *memset(void *d, int v, size_t n)
{
    if (n < 16 || met() == MEM_SIMPLES) {
        uint8_t *a = d;
        while (n--) *a++ = (uint8_t)v;
        return d;
    }
    DESPACHAR(F_MEMSET, n, rep_memset(d, v, n), sse1_memset(d, v, n), sse2_memset(d, v, n), sse2_memset(d, v, n), rep_memset(d, v, n));
    return d;
}

/* ------------------------------------------------------------ píxeles */

static inline void rep_preencher32(uint32_t *d, uint32_t cor, size_t n)
{
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(cor) : "memory");
}

void preencher32(uint32_t *d, uint32_t cor, size_t n)
{
    if (n < 8 || met() == MEM_SIMPLES) {
        while (n--) *d++ = cor;
        return;
    }
    DESPACHAR(F_PREENCHER, n * 4, rep_preencher32(d, cor, n), sse1_preencher32(d, cor, n), sse2_preencher32(d, cor, n), sse2_preencher32(d, cor, n), rep_preencher32(d, cor, n));
}

static void c_ampliar2x(uint32_t *d, uint32_t *d2, const uint32_t *o, size_t n)
{
    if (d2)
        for (; n; n--, o++) { uint32_t c = *o; d[0] = c; d[1] = c; d2[0] = c; d2[1] = c; d += 2; d2 += 2; }
    else
        for (; n; n--, o++) { uint32_t c = *o; *d++ = c; *d++ = c; }
}

void ampliar2x32(uint32_t *d, uint32_t *d2, const uint32_t *o, size_t n)
{
    if (met() == MEM_SIMPLES) { c_ampliar2x(d, d2, o, n); return; }
    DESPACHAR(F_AMPLIAR, n * 8, c_ampliar2x(d, d2, o, n), sse1_ampliar2x(d, d2, o, n), sse2_ampliar2x(d, d2, o, n), sse2_ampliar2x(d, d2, o, n), c_ampliar2x(d, d2, o, n));
}

static void c_escurecer(uint32_t *d, size_t n, int desloc, uint32_t mascara)
{
    for (; n; n--, d++) *d = (*d >> desloc) & mascara;
}

void escurecer32(uint32_t *d, size_t n, int desloc, uint32_t mascara)
{
    /* No es idempotente: medir las dos versiones daría resultados diferentes.
     * Usa la elección de preencher32 (el mismo tipo de bucle). */
    int m = met();
    if (n * 4 >= LIMIAR_SSE2 && cpu_tem_sse2() &&
        (nivel_forcado(m) >= V_SSE2 || (m == MEM_AUTO && faixa(F_PREENCHER, n * 4)->escolha >= V_SSE2)))
        sse2_escurecer(d, n, (unsigned)desloc, mascara);
    else
        c_escurecer(d, n, desloc, mascara);
}

/* ------------------------------------------------------------ canales y mezcla (D208) */

static void c_trocar(uint32_t *d, const uint32_t *o, size_t n)
{
    for (; n; n--, d++, o++) {
        uint32_t c = *o;
        *d = (c & 0xFF00FF00u) | (c >> 16 & 0xFF) | (c & 0xFF) << 16;
    }
}

/* Intercambia rojo y azul de n píxeles (0xAARRGGBB <-> 0xAABBGGRR); d y o pueden ser el mismo bloque. */
void trocar_rb32(uint32_t *d, const uint32_t *o, size_t n)
{
    if (met() == MEM_SIMPLES) { c_trocar(d, o, n); return; }
    DESPACHAR(F_TROCAR, n * 4, c_trocar(d, o, n), c_trocar(d, o, n), sse2_trocar_rb32(d, o, (unsigned)n),
              c_trocar(d, o, n), ssse3_trocar_rb32(d, o, (unsigned)n));
}

static void c_misturar(uint32_t *d, const uint32_t *o, size_t n, unsigned a)
{
    const unsigned b = 256 - a;
    for (; n; n--, d++, o++) {
        uint32_t p = *o, q = *d, r = 0;
        for (int s = 0; s < 32; s += 8) r |= ((((p >> s) & 255) * a + ((q >> s) & 255) * b) >> 8) << s;
        *d = r;
    }
}

/* Mezcla constante de n píxeles de o sobre d: cada canal d = (o*a + d*(256-a)) >> 8, con a de 0 a 256. */
void misturar32(uint32_t *d, const uint32_t *o, size_t n, unsigned a)
{
    if (a > 256) a = 256;
    if (met() == MEM_SIMPLES) { c_misturar(d, o, n, a); return; }
    DESPACHAR(F_MISTURAR, n * 4, c_misturar(d, o, n, a), c_misturar(d, o, n, a), sse2_misturar32(d, o, (unsigned)n, a),
              c_misturar(d, o, n, a), c_misturar(d, o, n, a));
}
