// SPDX-License-Identifier: GPL-2.0-or-later
/* saguimark.c - el motor de SaguiMark (D093, mejora 051).
 *
 * Pruebas de dibujo propias, con carga fija y resultado conocido: cada una
 * devuelve un número (cuenta, suma o resumen) que se compara con el
 * esperado; si no coincide, la prueba sale como "ERROR" y no gana puntos (una
 * CPU o FPU que calcula mal no puede tener nota). El tiempo se mide con
 * RDTSC y se convierte con la frecuencia del TSC que el kernel midió contra el PIT
 * en el arranque (INFO_CPU_MHZ); el reloj del sistema confirma el total.
 *
 * Cada prueba: un calentamiento, N repeticiones (3 en modo rápido, 9 en el preciso),
 * la mediana y la variación (± %). Puntos = tiempo_de_referencia / tiempo x 1000;
 * grupo = media geométrica de sus pruebas; total = media geométrica de los
 * grupos Enteros, FPU x87, SIMD, Memoria y Gráficos 2D (el grupo Sistema mide el propio
 * SaguiOS y queda fuera del total). La referencia (1000 puntos) es, por ahora, una
 * máquina virtual QEMU/KVM (ver tempos_ref y D093); la definitiva será el HP
 * dv7.
 *
 * Compilado sin -mgeneral-regs-only (Makefile): la FPU x87 se usa aquí
 * a voluntad (el kernel guarda el estado de cada proceso, D032). El SSE2 queda sólo
 * en las funciones marcadas con __attribute__((target("sse2"))), llamadas después
 * del CPUID: en un Pentium III el resto corre y el grupo SIMD sale "n/d". */
#include "saguimark.h"
#include "gui.h"
#include "sha256.h"

/* ------------------------------------------------------------ utilidades */

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void cpuid(uint32_t f, uint32_t r[4])
{
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(f), "c"(0));
}

/* División de 64 por 32 bits: no hay libgcc en los programas. */
static uint64_t div64(uint64_t n, uint32_t d)
{
    if (!d) return 0;
    uint64_t q = 0, r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    return q;
}

static uint32_t semente;
static inline uint32_t aleat(void)   /* xorshift32: la misma secuencia en toda máquina */
{
    uint32_t x = semente;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return semente = x;
}

/* x87: precisión de 53 bits (como el double del SSE2), redondeo al más
 * próximo, excepciones enmascaradas. Así el Mandelbrot y las matrices dan el mismo
 * resultado en la FPU y en el SSE2, en cualquier CPU. */
static uint16_t cw_antes;
static void x87_inicio(void)
{
    uint16_t cw = 0x027F;
    __asm__ volatile("fnstcw %0" : "=m"(cw_antes));
    __asm__ volatile("fldcw %0" : : "m"(cw));
}
static void x87_fim(void) { __asm__ volatile("fldcw %0" : : "m"(cw_antes)); }

static inline double x87_sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static inline double x87_raiz(double x) { double r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
/* 2^y = 2^(y - n) * 2^n, con n = y redondeado (el f2xm1 sólo acepta |y - n| <= 1).
 * noinline: el asm apila hasta tres valores en la FPU, y el GCC no lo sabe;
 * en una llamada la pila x87 empieza vacía, así que no desborda. */
static __attribute__((noinline)) double x87_exp2(double y)
{
    double r;
    __asm__("fld %%st(0)\n\t"
            "frndint\n\t"
            "fxch\n\t"
            "fsub %%st(1), %%st\n\t"
            "f2xm1\n\t"
            "fld1\n\t"
            "faddp\n\t"
            "fscale\n\t"
            "fstp %%st(1)"
            : "=t"(r) : "0"(y));
    return r;
}
static inline double x87_exp(double x) { return x87_exp2(x * 1.4426950408889634); }
static __attribute__((noinline)) double x87_log2(double x)
{
    double r;
    __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}

/* ------------------------------------------------------------ dados */

static uint8_t *crivo;                 /* 4 MB */
static uint32_t *vet, *vet_orig;       /* 400 mil enteros */
static uint8_t *texto, *comprimido, *descomprimido;
static uint8_t *bloco_sha;             /* 2 MB */
static uint32_t *tabela;               /* 256 mil entradas */
static double *ma, *mb, *mc;           /* 96 x 96 */
static float *sinal, *saida_fir;
static uint32_t *mem_a, *mem_b;        /* 8 MB cada */
static uint32_t *nos;                  /* 16 MB: 256 mil nodos de 64 bytes */
static void *blocos[16];
static int qtd_blocos;

static void *bloco(size_t n)
{
    void *p = aloca(n + 64);
    if (!p) return NULL;
    blocos[qtd_blocos++] = p;
    return (void *)(((uint32_t)p + 63) & ~63u);
}

static void liberar_tudo(void)
{
    for (int i = 0; i < qtd_blocos; i++) libera(blocos[i]);
    qtd_blocos = 0;
}

/* ------------------------------------------------------------ Enteros */

#define CRIVO_N 4000000u
#define CRIVO_VOLTAS 4
static uint32_t prova_crivo(void)
{
    uint32_t n = 0;
    for (int v = 0; v < CRIVO_VOLTAS; v++) {
        memset(crivo, 0, CRIVO_N);
        n = 0;
        for (uint32_t i = 2; i < CRIVO_N; i++) {
            if (crivo[i]) continue;
            n++;
            if (i < 2000)   /* i * i < CRIVO_N */
                for (uint32_t k = i * i; k < CRIVO_N; k += i) crivo[k] = 1;
        }
    }
    return n;
}

#define VET_N 400000
static void prep_ordenar(void) { memcpy(vet, vet_orig, VET_N * 4); }

static void ordenar(uint32_t *a, int n)
{
    int pilha[64], sp = 0, ini = 0, fim = n - 1;
    for (;;) {
        while (fim - ini > 16) {
            int m = ini + (fim - ini) / 2;
            /* mediana de tres en el medio */
            if (a[m] < a[ini]) { uint32_t t = a[m]; a[m] = a[ini]; a[ini] = t; }
            if (a[fim] < a[ini]) { uint32_t t = a[fim]; a[fim] = a[ini]; a[ini] = t; }
            if (a[fim] < a[m]) { uint32_t t = a[fim]; a[fim] = a[m]; a[m] = t; }
            uint32_t piv = a[m];
            int i = ini, j = fim;
            while (i <= j) {
                while (a[i] < piv) i++;
                while (a[j] > piv) j--;
                if (i <= j) { uint32_t t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
            }
            /* el lado menor va a la pila: ella nunca pasa de log2(n) */
            if (j - ini < fim - i) { pilha[sp++] = i; pilha[sp++] = fim; fim = j; }
            else { pilha[sp++] = ini; pilha[sp++] = j; ini = i; }
        }
        for (int i = ini + 1; i <= fim; i++) {
            uint32_t v = a[i];
            int k = i - 1;
            while (k >= ini && a[k] > v) { a[k + 1] = a[k]; k--; }
            a[k + 1] = v;
        }
        if (!sp) break;
        fim = pilha[--sp];
        ini = pilha[--sp];
    }
}

static uint32_t prova_ordenar(void)
{
    ordenar(vet, VET_N);
    uint32_t s = 0;
    for (int i = 0; i < VET_N; i++) {
        if (i && vet[i - 1] > vet[i]) return 0xBAD00000u | (uint32_t)i;
        s += vet[i] * (uint32_t)(i | 1);
    }
    return s;
}

/* LZ propio: un byte de control; < 0x80 = (c + 1) bytes literales a seguir;
 * >= 0x80 = copia de (c & 0x7F) + 4 bytes, de una distancia de 16 bits. */
#define TEXTO_N 262144
static int lz_comprimir(const uint8_t *s, int n, uint8_t *d)
{
    static uint32_t h[4096];
    memset(h, 0xFF, sizeof h);
    int o = 0, lit = 0, i = 0;
    while (i < n) {
        int len = 0;
        uint32_t cand = 0xFFFFFFFFu;
        if (i + 4 <= n) {
            uint32_t k = ((uint32_t)s[i] | (uint32_t)s[i + 1] << 8 | (uint32_t)s[i + 2] << 16) * 2654435761u >> 20;
            cand = h[k];
            h[k] = (uint32_t)i;
            if (cand != 0xFFFFFFFFu && i - (int)cand < 65536) {
                while (i + len < n && len < 131 && s[cand + len] == s[i + len]) len++;
            }
        }
        if (len >= 4) {
            if (lit) { d[o - lit - 1] = (uint8_t)(lit - 1); lit = 0; }
            int dist = i - (int)cand;
            d[o++] = (uint8_t)(0x80 | (len - 4));
            d[o++] = (uint8_t)dist;
            d[o++] = (uint8_t)(dist >> 8);
            i += len;
        } else {
            if (!lit) o++;   /* lugar del control */
            d[o++] = s[i++];
            if (++lit == 128) { d[o - lit - 1] = 127; lit = 0; }
        }
    }
    if (lit) d[o - lit - 1] = (uint8_t)(lit - 1);
    return o;
}

static int lz_descomprimir(const uint8_t *s, int n, uint8_t *d, int max)
{
    int i = 0, o = 0;
    while (i < n) {
        int c = s[i++];
        if (c < 0x80) {
            if (o + c + 1 > max || i + c + 1 > n) return -1;
            for (int k = 0; k <= c; k++) d[o++] = s[i++];
        } else {
            int len = (c & 0x7F) + 4, dist = s[i] | s[i + 1] << 8;
            i += 2;
            if (dist == 0 || dist > o || o + len > max) return -1;
            for (int k = 0; k < len; k++, o++) d[o] = d[o - dist];
        }
    }
    return o;
}

#define LZ_VOLTAS 24
static uint32_t prova_lz(void)
{
    int c = 0;
    for (int v = 0; v < LZ_VOLTAS; v++) {
        c = lz_comprimir(texto, TEXTO_N, comprimido);
        int d = lz_descomprimir(comprimido, c, descomprimido, TEXTO_N);
        if (d != TEXTO_N || memcmp(texto, descomprimido, TEXTO_N) != 0) return 0xBAD00000u;
    }
    return (uint32_t)c;
}

#define SHA_N (2u << 20)
#define SHA_VOLTAS 5
static uint32_t prova_sha(void)
{
    uint8_t r[SHA256_TAM];
    for (int v = 0; v < SHA_VOLTAS; v++) sha256(bloco_sha, SHA_N, r);
    return (uint32_t)r[0] << 24 | (uint32_t)r[1] << 16 | (uint32_t)r[2] << 8 | r[3];
}

#define TAB_BITS 18
#define TAB_N    (1u << TAB_BITS)
#define TAB_VOLTAS 8

static uint32_t prova_tabela(void)
{
    /* Direccionamiento abierto: 128 mil claves en una tabla de 256 mil, después
     * 256 mil búsquedas (la mitad de las claves existe); ocho veces. */
    uint32_t achou = 0;
    for (int v = 0; v < TAB_VOLTAS; v++) {
    memset(tabela, 0, TAB_N * 4);
    semente = 0x51A6u;
    for (uint32_t i = 0; i < TAB_N / 2; i++) {
        uint32_t k = aleat() | 1, p = (k * 2654435761u) >> (32 - TAB_BITS);
        while (tabela[p] && tabela[p] != k) p = (p + 1) & (TAB_N - 1);
        tabela[p] = k;
    }
    uint32_t s1 = 0x51A6u, s2 = 0x7E57u;
    achou = 0;
    for (uint32_t i = 0; i < TAB_N; i++) {
        semente = (i & 1) ? s2 : s1;
        uint32_t k = aleat() | 1;
        if (i & 1) s2 = semente; else s1 = semente;
        uint32_t p = (k * 2654435761u) >> (32 - TAB_BITS);
        while (tabela[p] && tabela[p] != k) p = (p + 1) & (TAB_N - 1);
        if (tabela[p] == k) achou++;
    }
    }
    return achou;
}

/* ------------------------------------------------------------ FPU x87 */

#define MAND_L 320
#define MAND_A 240
#define MAND_IT 200
static const double mand_x0 = -2.2, mand_y0 = -1.2, mand_dx = 3.2 / MAND_L, mand_dy = 2.4 / MAND_A;

#define MAND_VOLTAS_X87  2
#define MAND_VOLTAS_SSE2 8
static uint32_t prova_mandel_x87(void)
{
    x87_inicio();
    uint32_t total = 0;
    for (int v = 0; v < MAND_VOLTAS_X87; v++) {
    total = 0;
    for (int y = 0; y < MAND_A; y++) {
        double ci = mand_y0 + y * mand_dy;
        for (int x = 0; x < MAND_L; x++) {
            double cr = mand_x0 + x * mand_dx, zr = 0, zi = 0;
            int it = 0;
            for (; it < MAND_IT; it++) {
                double zr2 = zr * zr, zi2 = zi * zi;
                if (zr2 + zi2 > 4.0) break;
                zi = (2.0 * zr) * zi + ci;
                zr = (zr2 - zi2) + cr;
            }
            total += (uint32_t)it;
        }
    }
    }
    x87_fim();
    return total;
}

/* Simpson de sin(x) e^(-x/4) + raíz(x) en [0, 10], con la FPU haciendo seno,
 * exponencial (f2xm1/fscale) y raíz. Resultado en millonésimas. */
#define INTEG_N 200000
#define INTEG_VOLTAS 4
static double integrando(double x) { return x87_sin(x) * x87_exp(-0.25 * x) + x87_raiz(x); }

static uint32_t prova_integral(void)
{
    x87_inicio();
    uint32_t v = 0;
    for (int k = 0; k < INTEG_VOLTAS; k++) {
        double h = 10.0 / INTEG_N, s = integrando(0) + integrando(10.0);
        for (int i = 1; i < INTEG_N; i++) s += (i & 1 ? 4.0 : 2.0) * integrando(i * h);
        double r = s * h / 3.0;
        v = (uint32_t)(r * 1e6 + 0.5);
    }
    x87_fim();
    return v;
}

/* Matrices 96 x 96 con valores enteros pequeños: las sumas son exactas en
 * double, y la FPU y el SSE2 tienen que dar el mismo número. */
#define MAT_N 96
#define MAT_VOLTAS_X87  64
#define MAT_VOLTAS_SSE2 256
static void prep_matrizes(void)
{
    for (int i = 0; i < MAT_N; i++)
        for (int j = 0; j < MAT_N; j++) {
            ma[i * MAT_N + j] = (double)((i * 7 + j * 3) % 11 - 5);
            mb[i * MAT_N + j] = (double)((i * 5 + j * 2) % 13 - 6);
        }
}

static uint32_t soma_matriz(void)
{
    int s = 0;
    for (int i = 0; i < MAT_N; i++)
        for (int j = 0; j < MAT_N; j++) s += (int)mc[i * MAT_N + j] * ((i + j) % 7 + 1);
    return (uint32_t)s;
}

static uint32_t prova_matrizes_x87(void)
{
    x87_inicio();
    for (int v = 0; v < MAT_VOLTAS_X87; v++)
        for (int i = 0; i < MAT_N; i++) {
            double *c = mc + i * MAT_N;
            for (int j = 0; j < MAT_N; j++) c[j] = 0;
            for (int k = 0; k < MAT_N; k++) {
                double a = ma[i * MAT_N + k];
                const double *b = mb + k * MAT_N;
                for (int j = 0; j < MAT_N; j++) c[j] += a * b[j];
            }
        }
    uint32_t s = soma_matriz();
    x87_fim();
    return s;
}

/* ------------------------------------------------------------ SIMD (SSE2) */

typedef double v2d __attribute__((vector_size(16)));
typedef long long v2l __attribute__((vector_size(16)));
typedef float v4f __attribute__((vector_size(16)));
typedef float v4f_u __attribute__((vector_size(16), aligned(4)));

/* Dos píxeles por instrucción: las franjas que ya escaparon dejan de contar
 * (la máscara de la comparación vale 0 en ellas). */
__attribute__((target("sse2,fpmath=sse"))) static uint32_t prova_mandel_sse2(void)
{
    uint32_t total = 0;
    const v2d quatro = { 4.0, 4.0 }, dois = { 2.0, 2.0 };
    for (int v = 0; v < MAND_VOLTAS_SSE2; v++) {
    total = 0;
    for (int y = 0; y < MAND_A; y++) {
        double ciy = mand_y0 + y * mand_dy;
        v2d ci = { ciy, ciy };
        for (int x = 0; x < MAND_L; x += 2) {
            v2d cr = { mand_x0 + x * mand_dx, mand_x0 + (x + 1) * mand_dx }, zr = { 0, 0 }, zi = { 0, 0 };
            v2l n = { 0, 0 };
            for (int it = 0; it < MAND_IT; it++) {
                v2d zr2 = zr * zr, zi2 = zi * zi;
                v2l dentro = (zr2 + zi2) <= quatro;
                if (!(dentro[0] | dentro[1])) break;
                n -= dentro;   /* -1 en las franjas que siguen dentro */
                zi = (dois * zr) * zi + ci;
                zr = (zr2 - zi2) + cr;
            }
            total += (uint32_t)(n[0] + n[1]);
        }
    }
    }
    return total;
}

__attribute__((target("sse2,fpmath=sse"))) static uint32_t prova_matrizes_sse2(void)
{
    for (int v = 0; v < MAT_VOLTAS_SSE2; v++)
        for (int i = 0; i < MAT_N; i++) {
            v2d *c = (v2d *)(mc + i * MAT_N);
            for (int j = 0; j < MAT_N / 2; j++) c[j] = (v2d){ 0, 0 };
            for (int k = 0; k < MAT_N; k++) {
                double a1 = ma[i * MAT_N + k];
                v2d a = { a1, a1 };
                const v2d *b = (const v2d *)(mb + k * MAT_N);
                for (int j = 0; j < MAT_N / 2; j++) c[j] += a * b[j];
            }
        }
    return soma_matriz();
}

/* Filtro FIR de 32 coeficientes, cuatro muestras por instrucción. Valores
 * enteros pequeños: las sumas en float son exactas. */
#define FIR_N 65536
#define FIR_K 32
#define FIR_VOLTAS 160
static float fir_h[FIR_K] __attribute__((aligned(16)));

__attribute__((target("sse2,fpmath=sse"))) static uint32_t prova_fir_sse2(void)
{
    for (int v = 0; v < FIR_VOLTAS; v++)
        for (int n = 0; n < FIR_N; n += 4) {
            v4f acc = { 0, 0, 0, 0 };
            for (int k = 0; k < FIR_K; k++) {
                v4f h = { fir_h[k], fir_h[k], fir_h[k], fir_h[k] };
                acc += h * *(const v4f_u *)(sinal + n + k);
            }
            *(v4f *)(saida_fir + n) = acc;
        }
    int s = 0;
    for (int n = 0; n < FIR_N; n++) s += (int)saida_fir[n] * ((n & 3) + 1);
    return (uint32_t)s;
}

/* ------------------------------------------------------------ Memoria */

#define MEM_PAL (2u << 20)   /* palabras de 32 bits: 8 MB */
#define MEM_VOLTAS_COPIA   64
#define MEM_VOLTAS_ESCRITA 64
#define MEM_VOLTAS_LEITURA 128

static uint32_t prova_copia(void)
{
    for (int v = 0; v < MEM_VOLTAS_COPIA; v++) {
        uint32_t *d = mem_b, *o = mem_a, n = MEM_PAL;
        __asm__ volatile("cld; rep movsl" : "+D"(d), "+S"(o), "+c"(n) : : "memory");
    }
    uint32_t s = 0;
    for (uint32_t i = 0; i < MEM_PAL; i += 1024) s += mem_b[i] ^ i;
    return s;
}

static uint32_t prova_escrita(void)
{
    for (int v = 0; v < MEM_VOLTAS_ESCRITA; v++) {
        uint32_t *d = mem_b, n = MEM_PAL, val = 0x5A5A0000u + (uint32_t)v;
        __asm__ volatile("cld; rep stosl" : "+D"(d), "+c"(n) : "a"(val) : "memory");
    }
    for (uint32_t i = 0; i < MEM_PAL; i += 4093)
        if (mem_b[i] != 0x5A5A0000u + MEM_VOLTAS_ESCRITA - 1) return 0xBAD00000u;
    return 1;
}

static uint32_t prova_leitura(void)
{
    uint32_t s = 0;
    for (int v = 0; v < MEM_VOLTAS_LEITURA; v++) {
        uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        for (uint32_t i = 0; i < MEM_PAL; i += 4) {
            s0 += mem_a[i]; s1 += mem_a[i + 1]; s2 += mem_a[i + 2]; s3 += mem_a[i + 3];
        }
        s += s0 + s1 + s2 + s3;
    }
    return s;
}

/* Latencia: un solo ciclo aleatorio (Sattolo) por 256 mil nodos de 64 bytes
 * (16 MB). Ocho vueltas completas: el recorrido tiene que terminar en el nodo 0. */
#define NOS_N (1u << 18)
#define NOS_PASSO 16   /* palabras por nodo */
#define NOS_VOLTAS 8
static uint32_t prova_latencia(void)
{
    uint32_t i = 0;
    for (uint32_t k = 0; k < NOS_VOLTAS * NOS_N; k++) i = nos[i * NOS_PASSO];
    return i;
}

/* ------------------------------------------------------------ Sistema */

static uint32_t prova_chamada(void)
{
    int p = meu_pid();
    for (int i = 0; i < 200000; i++)
        if (meu_pid() != p) return 0;
    return 1;
}

static int tubo[2] = { -1, -1 };
static uint32_t prova_tubo(void)
{
    char b[64], r[64];
    for (int i = 0; i < 64; i++) b[i] = (char)i;
    for (int i = 0; i < 80000; i++) {
        if (escrever(tubo[1], b, 64) != 64 || ler(tubo[0], r, 64) != 64 || r[63] != 63) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------ Gráficos 2D (D102) */

/* Una ventana de 128x128 en una esquina de la pantalla. Las pruebas dibujan en ella con las
 * mismas funciones de la libsagui que usan los programas (g_linha, g_retangulo,
 * g_copiar_area); dos mandan el resultado a la pantalla (jan_mostrar), lo que mide la
 * tubería completa: llamada al sistema, gestor de ventanas, copia al
 * framebuffer (write-combining, D057) y conversión a 16 bits (D078). El
 * contenido se comprueba con una suma FNV de los 16384 píxeles (unos 30 us, dentro del
 * tiempo de la prueba). Los esperados vienen de una implementación independiente en
 * Python (tests/saguimark_ref2d.py, que el h72 comprueba). */
#define J2D 128
static jan_t *jan2d;

static uint32_t soma_janela(void)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < J2D * J2D; i++) h = (h ^ jan2d->px[i]) * 16777619u;
    return h;
}

static void prep_limpar(void) { g_retangulo(jan2d, 0, 0, J2D, J2D, 0); }

static void prep_padrao(void)
{
    for (uint32_t i = 0; i < J2D * J2D; i++) jan2d->px[i] = i * 2654435761u;
}

/* Líneas pseudoaleatorias que salen un poco de la ventana (recorte incluido). */
static void linhas_aleatorias(int n, uint32_t sem, int mostrar_cada)
{
    semente = sem;
    for (int i = 0; i < n; i++) {
        int x0 = (int)(aleat() % 160) - 16;
        int y0 = (int)(aleat() % 160) - 16;
        int x1 = (int)(aleat() % 160) - 16;
        int y1 = (int)(aleat() % 160) - 16;
        g_linha(jan2d, x0, y0, x1, y1, aleat());
        if (mostrar_cada && (i + 1) % mostrar_cada == 0) jan_mostrar(jan2d);
    }
}

#define LIN_N 200000
static uint32_t prova_linhas(void)
{
    linhas_aleatorias(LIN_N, 31337, 0);
    return soma_janela();
}

#define PAN_N 12000
#define PAN_CADA 10
static uint32_t prova_linhas_tela(void)
{
    linhas_aleatorias(PAN_N, 2718, PAN_CADA);
    return soma_janela();
}

#define RET_N 60000
#define RET_PIXELS 96334725u    /* píxeles pintados (ya recortados), cuenta del anfitrión */
static uint32_t prova_retangulos(void)
{
    semente = 4711;
    for (int i = 0; i < RET_N; i++) {
        int x = (int)(aleat() % 160) - 16;
        int y = (int)(aleat() % 160) - 16;
        int l = (int)(aleat() % 128) + 1;
        int a = (int)(aleat() % 128) + 1;
        g_retangulo(jan2d, x, y, l, a, aleat());
    }
    return soma_janela();
}

#define BLT_N 20000
#define BLT_PIXELS 80489397u
static uint32_t prova_blocos(void)
{
    semente = 1618;
    for (int i = 0; i < BLT_N; i++) {
        int l = (int)(aleat() % 64) + 32;
        int a = (int)(aleat() % 64) + 32;
        int sx = (int)(aleat() % (uint32_t)(J2D - l + 1));
        int sy = (int)(aleat() % (uint32_t)(J2D - a + 1));
        int dx = (int)(aleat() % (uint32_t)(J2D - l + 1));
        int dy = (int)(aleat() % (uint32_t)(J2D - a + 1));
        g_copiar_area(jan2d, sx, sy, l, a, dx, dy);
    }
    return soma_janela();
}

/* Abre la ventana (si hay escritorio) y la lleva a la esquina de abajo a la izquierda,
 * lejos de la ventana del infopc. Se queda unos 1,5 s parada con una figura para que
 * se vea (y se capture) antes de las medidas. */
static void abrir_2d(void)
{
    info_video_t v;
    memset(&v, 0, sizeof v);
    info_sistema(INFO_VIDEO, &v, sizeof v);
    if (!v.escritorio) return;
    jan2d = jan_abrir("SaguiMark 2D", J2D, J2D);
    if (!jan2d) return;
    mover_janela(jan2d->id, 0, -1);
    prep_limpar();
    semente = 5;
    for (int i = 0; i < 40; i++) {
        int x0 = (int)(aleat() % J2D), y0 = (int)(aleat() % J2D), x1 = (int)(aleat() % J2D), y1 = (int)(aleat() % J2D);
        g_linha(jan2d, x0, y0, x1, y1, 0x0B7A3E | (aleat() & 0x3F3F3F));
    }
    jan_mostrar(jan2d);
    registrar("saguimark: ventana 2D abierta");
    dormir(1500);
}

static void fechar_2d(void)
{
    if (jan2d) jan_fechar(jan2d);
    jan2d = NULL;
}

/* ------------------------------------------------------------ tabla de pruebas */

typedef struct {
    const char *nome;
    int         grupo;
    uint32_t  (*f)(void);
    void      (*prep)(void);   /* antes de cada repetición, fuera del tiempo */
    uint32_t    esperado;
    uint32_t    tolerancia;
    unsigned    ref_us;        /* tiempo de la máquina de referencia (1000 puntos) */
    unsigned    n1;            /* pruebas 2D: carga por ejecución (D102) */
    const char *u1;
    unsigned    n2;
    const char *u2;
} prova_def_t;

/* Referencia PROVISIONAL (D093), en microsegundos: la mediana de tres
 * ejecuciones del modo preciso en una máquina virtual QEMU 8.2 con KVM (-cpu
 * qemu32, 512 MB) en el PC de desarrollo (WSL2, TSC de 4,4 GHz), versión 1
 * de las pruebas, el 2026-11-07 (las cuatro pruebas del grupo Gráficos 2D, nuevas en la
 * versión 2, el 2026-11-08, en la misma máquina; la ventana es 128x128 en la esquina de
 * abajo a la derecha de la pantalla de 1024x768 de QEMU). Cuando se mida el HP dv7, sus tiempos
 * entran aquí y pasa a valer 1000 (y los esperados no cambian). */
static const prova_def_t provas[] = {
    { "Criba de primos",       SM_ENTEROS, prova_crivo,          NULL,          283146u,     0, 19088 , 0, NULL, 0, NULL },
    { "Ordenación (quicksort)", SM_ENTEROS, prova_ordenar,       prep_ordenar,  0xD89378D2u,           0, 17674 , 0, NULL, 0, NULL },
    { "Compresión LZ",         SM_ENTEROS, prova_lz,             NULL,          0x20825u,           0, 19499 , 0, NULL, 0, NULL },
    { "SHA-256",               SM_ENTEROS, prova_sha,            NULL,          0xFFDEC0BEu,           0, 21488 , 0, NULL, 0, NULL },
    { "Tabla hash",            SM_ENTEROS, prova_tabela,         NULL,          0x20007u,           0, 18085 , 0, NULL, 0, NULL },
    { "Mandelbrot",            SM_FPU,     prova_mandel_x87,     NULL,          0x33DF16u,           0, 24433 , 0, NULL, 0, NULL },
    { "Integral (sin/exp/raíz)", SM_FPU,   prova_integral,       NULL,          22098359u,           3, 25107 , 0, NULL, 0, NULL },
    { "Matrices 96x96",        SM_FPU,     prova_matrizes_x87,   prep_matrizes, 0x6BEu,           0, 20651 , 0, NULL, 0, NULL },
    { "Mandelbrot (2 px)",     SM_SIMD,    prova_mandel_sse2,    NULL,          0,           0, 24354 , 0, NULL, 0, NULL },
    { "Matrices 96x96",        SM_SIMD,    prova_matrizes_sse2,  prep_matrizes, 0,           0, 21845 , 0, NULL, 0, NULL },
    { "Filtro FIR (4 muestras)", SM_SIMD,  prova_fir_sse2,       NULL,          0xFFED8EB9u,           0, 24332 , 0, NULL, 0, NULL },
    { "Copia (rep movsd)",     SM_MEMORIA, prova_copia,          NULL,          0,           0, 20595 , 0, NULL, 0, NULL },
    { "Escritura (rep stosd)", SM_MEMORIA, prova_escrita,        NULL,          1,           0, 20002 , 0, NULL, 0, NULL },
    { "Lectura",               SM_MEMORIA, prova_leitura,        NULL,          0,           0, 17530 , 0, NULL, 0, NULL },
    { "Latencia (lista)",      SM_MEMORIA, prova_latencia,       NULL,          0,           0, 21609 , 0, NULL, 0, NULL },
    { "Líneas (búfer)",        SM_2D,      prova_linhas,         prep_limpar,   0x7EADA991u,     0, 23000, LIN_N, "líneas", 0, NULL },
    { "Líneas a la pantalla",  SM_2D,      prova_linhas_tela,    prep_limpar,   0x8961084Bu,     0, 40000, PAN_N, "líneas", PAN_N / PAN_CADA, "cuadros" },
    { "Rectángulos (relleno)", SM_2D,      prova_retangulos,     prep_limpar,   0x2B1AC807u,     0, 16250, RET_N, "rectáng.", RET_PIXELS, "píxeles" },
    { "Copia de bloques",      SM_2D,      prova_blocos,         prep_padrao,   0x3108AB00u,     0, 13700, BLT_N, "copias", BLT_PIXELS, "píxeles" },
    { "Llamada al sistema",    SM_SISTEMA, prova_chamada,        NULL,          1,           0, 21165 , 0, NULL, 0, NULL },
    { "Tubería (pipe)",        SM_SISTEMA, prova_tubo,           NULL,          1,           0, 24549 , 0, NULL, 0, NULL },
};
#define QTD_PROVAS (int)(sizeof provas / sizeof provas[0])

static const char *const nomes_grupo[SM_GRUPOS] = { "Enteros", "FPU x87", "SIMD (SSE2)", "Memoria", "Gráficos 2D", "Sistema" };
static const char *const chaves_grupo[SM_GRUPOS] = { "enteros", "fpu", "simd", "memoria", "2d", "sistema" };

const char *sm_nome_grupo(int g) { return g >= 0 && g < SM_GRUPOS ? nomes_grupo[g] : "?"; }

int sm_grupo_por_nome(const char *n)
{
    for (int g = 0; g < SM_GRUPOS; g++)
        if (compara_texto(n, chaves_grupo[g]) == 0) return g;
    return -1;
}

/* ------------------------------------------------------------ preparación */

static bool preparar(unsigned grupos, bool *mem_ok)
{
    static const char *const palavras[] = {
        "sagui", "sistema", "memoria", "procesador", "ventana", "archivo", "tarea", "red", "de", "la", "el",
        "con", "para", "una", "que", "en", "los", "programa", "teclado", "pantalla", "disco", "reloj",
        "puerto", "bus", "caché", "núcleo", "kernel", "puntero", "pila", "cola", "árbol", "lista",
    };
    qtd_blocos = 0;
    *mem_ok = true;
    if (grupos & (1u << SM_ENTEROS)) {
        crivo = bloco(CRIVO_N);
        vet = bloco(VET_N * 4); vet_orig = bloco(VET_N * 4);
        texto = bloco(TEXTO_N); comprimido = bloco(TEXTO_N + TEXTO_N / 64 + 16); descomprimido = bloco(TEXTO_N);
        bloco_sha = bloco(SHA_N);
        tabela = bloco(TAB_N * 4);
        if (!crivo || !vet || !vet_orig || !texto || !comprimido || !descomprimido || !bloco_sha || !tabela) return false;
        semente = 12345;
        for (int i = 0; i < VET_N; i++) vet_orig[i] = aleat();
        semente = 777;
        int o = 0;
        while (o < TEXTO_N) {
            const char *p = palavras[aleat() % (sizeof palavras / sizeof palavras[0])];
            while (*p && o < TEXTO_N) texto[o++] = (uint8_t)*p++;
            uint32_t r = aleat() % 16;
            if (o < TEXTO_N) texto[o++] = r == 0 ? '\n' : r == 1 ? '.' : ' ';
        }
        semente = 99;
        for (uint32_t i = 0; i < SHA_N / 4; i++) ((uint32_t *)bloco_sha)[i] = aleat();
    }
    if (grupos & ((1u << SM_FPU) | (1u << SM_SIMD))) {
        ma = bloco(MAT_N * MAT_N * 8); mb = bloco(MAT_N * MAT_N * 8); mc = bloco(MAT_N * MAT_N * 8);
        sinal = bloco((FIR_N + FIR_K) * 4); saida_fir = bloco(FIR_N * 4);
        if (!ma || !mb || !mc || !sinal || !saida_fir) return false;
        semente = 2024;
        for (int i = 0; i < FIR_N + FIR_K; i++) sinal[i] = (float)(aleat() % 16);
        for (int k = 0; k < FIR_K; k++) fir_h[k] = (float)((int)(aleat() % 7) - 3);
    }
    if (grupos & (1u << SM_MEMORIA)) {
        mem_a = bloco(MEM_PAL * 4); mem_b = bloco(MEM_PAL * 4); nos = bloco(NOS_N * NOS_PASSO * 4);
        if (!mem_a || !mem_b || !nos) { *mem_ok = false; return true; }
        for (uint32_t i = 0; i < MEM_PAL; i++) mem_a[i] = i * 2654435761u;
        memset(mem_b, 0, MEM_PAL * 4);
        /* Sattolo: permutación con un único ciclo. */
        uint32_t *perm = aloca(NOS_N * sizeof *perm);   /* D378: 1 MB, fuera de la BSS */
        if (!perm) { *mem_ok = false; return true; }
        for (uint32_t i = 0; i < NOS_N; i++) perm[i] = i;
        semente = 4242;
        for (uint32_t i = NOS_N - 1; i > 0; i--) {
            uint32_t j = aleat() % i, t = perm[i];
            perm[i] = perm[j];
            perm[j] = t;
        }
        for (uint32_t i = 0; i < NOS_N; i++) nos[i * NOS_PASSO] = perm[i];
        libera(perm);
    }
    return true;
}

/* Esperados que dependen de fórmulas y no de constantes medidas. */
static uint32_t esperado_de(int i)
{
    if (provas[i].f == prova_leitura) {
        /* suma de i * 2654435761 para i < MEM_PAL, en 32 bits, por las vueltas */
        uint32_t n = MEM_PAL, tri = (n / 2) * (n - 1);
        return tri * 2654435761u * MEM_VOLTAS_LEITURA;
    }
    if (provas[i].f == prova_copia) {
        uint32_t s = 0;
        for (uint32_t k = 0; k < MEM_PAL; k += 1024) s += (k * 2654435761u) ^ k;
        return s;
    }
    if (provas[i].f == prova_latencia) return 0;
    if (provas[i].f == prova_mandel_sse2) return provas[5].esperado;   /* el mismo que el del Mandelbrot x87 */
    if (provas[i].f == prova_matrizes_sse2) return provas[7].esperado;
    return provas[i].esperado;
}

/* ------------------------------------------------------------ ejecución */

static unsigned media_geometrica(const unsigned *v, int n)
{
    if (n <= 0) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) s += x87_log2((double)(v[i] ? v[i] : 1));
    return (unsigned)(x87_exp2(s / n) + 0.5);
}

static void ordenar_u64(uint64_t *v, int n)
{
    for (int i = 1; i < n; i++) {
        uint64_t x = v[i];
        int k = i - 1;
        while (k >= 0 && v[k] > x) { v[k + 1] = v[k]; k--; }
        v[k + 1] = x;
    }
}

static unsigned ciclos_us(uint64_t c, unsigned khz) { return (unsigned)div64(c * 1000, khz); }

bool sm_executar(sm_resultado_t *r, int modo, unsigned grupos, sm_progresso_t cb, void *ctx)
{
    memset(r, 0, sizeof *r);
    r->modo = modo;
    r->repeticoes = modo == SM_PRECISO ? 9 : 3;
    r->grupos = grupos;
    ler_data(&r->data);
    info_tsc_t tsc;
    memset(&tsc, 0, sizeof tsc);
    info_sistema(INFO_CPU_MHZ, &tsc, sizeof tsc);
    r->tsc_khz = tsc.khz;

    uint32_t c[4];
    cpuid(0, c);
    uint32_t maxx;
    uint32_t r1[4] = { 0 };
    if (c[0] >= 1) cpuid(1, r1);
    cpuid(0x80000000u, c);
    maxx = c[0];
    if (maxx >= 0x80000004u && maxx < 0x8000FFFFu) {
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002u + i, c); memcpy(r->cpu + 16 * i, c, 16); }
        r->cpu[48] = 0;
        int k = 0;
        while (r->cpu[k] == ' ') k++;
        if (k) memmove(r->cpu, r->cpu + k, 49 - (size_t)k);
    } else {
        copia_texto(r->cpu, "(la CPU no informa el nombre)", sizeof r->cpu);
    }
    if ((r1[2] >> 31) & 1) {
        cpuid(0x40000000u, c);
        memcpy(r->hipervisor, &c[1], 4); memcpy(r->hipervisor + 4, &c[2], 4); memcpy(r->hipervisor + 8, &c[3], 4);
        r->hipervisor[12] = 0;
        for (int i = 0; i < 12; i++) if (r->hipervisor[i] && (r->hipervisor[i] < 0x20 || r->hipervisor[i] > 0x7E)) r->hipervisor[i] = '?';
    }
    bool tem_fpu = r1[3] & 1, tem_sse2 = cpu_tem_sse2() && tem_fpu, tem_tsc = (r1[3] >> 4) & 1;

    bool mem_ok;
    if (!tem_tsc || !r->tsc_khz || !preparar(grupos, &mem_ok)) {
        liberar_tudo();
        return false;
    }
    if (grupos & (1u << SM_SISTEMA)) criar_pipe(tubo);
    if (grupos & (1u << SM_2D)) abrir_2d();

    unsigned ms_ini = tempo_ms();
    uint64_t tsc_ini = rdtsc();
    int total = 0;
    for (int i = 0; i < QTD_PROVAS; i++)
        if (grupos & (1u << provas[i].grupo)) total++;
    int feito = 0;
    for (int i = 0; i < QTD_PROVAS && r->qtd < SM_MAX_PROVAS; i++) {
        const prova_def_t *d = &provas[i];
        if (!(grupos & (1u << d->grupo))) continue;
        sm_prova_t *p = &r->prova[r->qtd++];
        p->nome = d->nome;
        p->grupo = d->grupo;
        p->esperado = esperado_de(i);
        if (cb) cb(ctx, feito, total, p);
        feito++;
        if ((d->grupo == SM_FPU && !tem_fpu) || (d->grupo == SM_SIMD && !tem_sse2) ||
            (d->grupo == SM_MEMORIA && !mem_ok) || (d->grupo == SM_SISTEMA && tubo[0] < 0) ||
            (d->grupo == SM_2D && !jan2d)) {
            p->estado = SM_ND;
            p->motivo = d->grupo == SM_SIMD ? "la CPU no tiene SSE2" : d->grupo == SM_FPU ? "la CPU no tiene FPU"
                        : d->grupo == SM_MEMORIA ? "sin memoria para 32 MB" : d->grupo == SM_2D ? "sin escritorio (no hay ventanas)"
                        : "no se pudo crear la tubería";
            continue;
        }
        uint64_t t[9];
        p->estado = SM_OK;
        for (int k = -1; k < r->repeticoes; k++) {   /* k = -1: calentamiento */
            if (d->prep) d->prep();
            uint64_t a = rdtsc();
            uint32_t v = d->f();
            uint64_t b = rdtsc();
            if (k >= 0) t[k] = b - a;
            p->obtido = v;
            uint32_t dif = v > p->esperado ? v - p->esperado : p->esperado - v;
            if (dif > d->tolerancia) {
                p->estado = SM_ERRO;
                p->motivo = "resultado incorrecto";
                r->erro = true;
                break;
            }
        }
        if (p->estado != SM_OK) continue;
        ordenar_u64(t, r->repeticoes);
        uint64_t med = t[r->repeticoes / 2];
        p->tempo_us = ciclos_us(med, r->tsc_khz);
        if (!p->tempo_us) p->tempo_us = 1;
        uint64_t amp = t[r->repeticoes - 1] - t[0], base = med;
        while (base >> 32) { base >>= 1; amp >>= 1; }
        p->var_x10 = base ? (unsigned)div64(amp * 500, (uint32_t)base) : 0;
        if (d->n1) {   /* pruebas 2D: tasas por segundo */
            p->unidade1 = d->u1;
            p->taxa1 = (unsigned)div64((uint64_t)d->n1 * 1000000u, p->tempo_us);
            if (d->n2) { p->unidade2 = d->u2; p->taxa2 = (unsigned)div64((uint64_t)d->n2 * 1000000u, p->tempo_us); }
        }
        if (d->ref_us) p->pontos = (unsigned)div64((uint64_t)d->ref_us * 1000, p->tempo_us);
    }
    uint64_t tsc_fim = rdtsc();
    r->duracao_ms = tempo_ms() - ms_ini;
    if (cb) cb(ctx, total, total, NULL);
    fechar_2d();
    if (tubo[0] >= 0) { fechar(tubo[0]); fechar(tubo[1]); tubo[0] = tubo[1] = -1; }
    liberar_tudo();

    /* El TSC tiene que concordar con el reloj del sistema (10 ms de resolución):
     * si no concuerda, la frecuencia cambió en medio (ahorro de energía). */
    unsigned ms_tsc = ciclos_us(tsc_fim - tsc_ini, r->tsc_khz) / 1000;
    if (r->duracao_ms >= 200) {
        unsigned d = ms_tsc > r->duracao_ms ? ms_tsc - r->duracao_ms : r->duracao_ms - ms_tsc;
        r->tsc_desvio_x10 = d * 1000 / r->duracao_ms;
    }

    /* Grupos y total. */
    unsigned vg[SM_GRUPOS];
    int ng = 0;
    for (int g = 0; g < SM_GRUPOS; g++) {
        unsigned v[SM_MAX_PROVAS];
        int n = 0, nd = 0, err = 0, sem_ref = 0;
        for (int i = 0; i < r->qtd; i++) {
            if (r->prova[i].grupo != g) continue;
            if (r->prova[i].estado == SM_ERRO) err++;
            else if (r->prova[i].estado == SM_ND) nd++;
            else if (!r->prova[i].pontos) sem_ref++;
            else v[n++] = r->prova[i].pontos;
        }
        if (err) r->estado_grupo[g] = SM_ERRO;
        else if (n) { r->estado_grupo[g] = SM_OK; r->pontos_grupo[g] = media_geometrica(v, n); }
        else r->estado_grupo[g] = SM_ND;
        (void)nd; (void)sem_ref;
        if (g < SM_GRUPOS_TOTAL && (grupos & (1u << g))) {
            if (r->estado_grupo[g] == SM_OK) vg[ng++] = r->pontos_grupo[g];
            else r->total_parcial = true;
        }
        if (g < SM_GRUPOS_TOTAL && !(grupos & (1u << g))) r->total_parcial = true;
    }
    r->total = r->erro ? 0 : media_geometrica(vg, ng);
    return true;
}

/* ------------------------------------------------------------ textos */

typedef struct { char *p; int n, max; } saida_t;

static void ad(saida_t *o, const char *fmt, ...)
{
    if (o->n >= o->max - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int k = formatar_lista(o->p + o->n, (size_t)(o->max - o->n), fmt, ap);
    va_end(ap);
    if (k > 0) o->n += k;
    if (o->n > o->max - 1) o->n = o->max - 1;
}

const char *sm_aviso_maquina(const sm_resultado_t *r)
{
    if (comeca_com(r->hipervisor, "TCGTCGTCGTCG"))
        return "QEMU sin KVM (TCG, emulación): la puntuación no representa ningún PC real";
    if (r->hipervisor[0]) return "máquina virtual: la puntuación depende del anfitrión";
    return NULL;
}

static void fmt_pontos(char *b, int max, int estado, unsigned pontos)
{
    if (estado == SM_ERRO) copia_texto(b, "ERROR", (size_t)max);
    else if (estado == SM_ND) copia_texto(b, "n/d", (size_t)max);
    else if (!pontos) copia_texto(b, "-", (size_t)max);
    else formatar_texto(b, (size_t)max, "%u", pontos);
}

int sm_texto(const sm_resultado_t *r, char *buf, int max)
{
    saida_t o = { buf, 0, max };
    char a[24];
    if (max > 0) buf[0] = 0;
    ad(&o, "SaguiMark %u, modo %s (%d repeticiones, mediana)\n", SM_VERSAO, r->modo == SM_PRECISO ? "preciso" : "rápido",
       r->repeticoes);
    ad(&o, "Referencia: 1000 = QEMU/KVM (provisional, D093; la definitiva será el HP dv7)\n");
    ad(&o, "Las puntuaciones de versiones distintas del benchmark NO se comparan (esta es la %u)\n", SM_VERSAO);
    ad(&o, "CPU: %s, TSC %u,%02u MHz\n", r->cpu, r->tsc_khz / 1000, r->tsc_khz % 1000 / 10);
    ad(&o, "Fecha: %02u/%02u/%04u %02u:%02u, duración %u,%u s\n", r->data.dia, r->data.mes, r->data.ano, r->data.hora,
       r->data.minuto, r->duracao_ms / 1000, r->duracao_ms % 1000 / 100);
    const char *av = sm_aviso_maquina(r);
    if (av) ad(&o, "Aviso: %s\n", av);
    if (r->tsc_desvio_x10 > 100)
        ad(&o, "Aviso: el TSC y el reloj del sistema difieren un %u,%u %% (¿frecuencia variable?)\n",
           r->tsc_desvio_x10 / 10, r->tsc_desvio_x10 % 10);
    ad(&o, "\n");
    if (r->erro) ad(&o, "TOTAL:      ERROR (alguna prueba ha calculado mal: ¿fallo de hardware u overclock?)\n");
    else if (r->total) ad(&o, "TOTAL:      %u%s\n", r->total, r->total_parcial ? "  (parcial: falta algún grupo)" : "");
    else ad(&o, "TOTAL:      n/d\n");
    for (int g = 0; g < SM_GRUPOS; g++) {
        if (!(r->grupos & (1u << g))) continue;
        fmt_pontos(a, sizeof a, r->estado_grupo[g], r->pontos_grupo[g]);
        ad(&o, "  %-12s %s%s\n", nomes_grupo[g], a, g == SM_SISTEMA ? "  (mide SaguiOS; fuera del total)" : "");
    }
    ad(&o, "\nGrupo        Prueba                     Tiempo       ±%%     Puntos  Verificación\n");
    for (int i = 0; i < r->qtd; i++) {
        const sm_prova_t *p = &r->prova[i];
        char tempo[24], var[16], ver[48];
        if (p->estado == SM_OK) {
            formatar_texto(tempo, sizeof tempo, "%u,%02u ms", p->tempo_us / 1000, p->tempo_us % 1000 / 10);
            formatar_texto(var, sizeof var, "%u,%u", p->var_x10 / 10, p->var_x10 % 10);
            formatar_texto(ver, sizeof ver, "ok (%x)", p->obtido);
        } else {
            copia_texto(tempo, "-", sizeof tempo);
            copia_texto(var, "-", sizeof var);
            if (p->estado == SM_ERRO) formatar_texto(ver, sizeof ver, "ERROR: %x, esperado %x", p->obtido, p->esperado);
            else formatar_texto(ver, sizeof ver, "n/d: %s", p->motivo ? p->motivo : "");
        }
        fmt_pontos(a, sizeof a, p->estado, p->pontos);
        ad(&o, "%-12s %-26s %10s %6s %8s  %s\n", nomes_grupo[p->grupo], p->nome, tempo, var, a, ver);
    }
    bool cab = false;
    for (int i = 0; i < r->qtd; i++) {
        const sm_prova_t *p = &r->prova[i];
        if (p->estado != SM_OK || !p->taxa1) continue;
        if (!cab) ad(&o, "\nGráficos 2D (ventana de 128x128, velocidad real):\n"), cab = true;
        ad(&o, "  %-24s %u %s/s", p->nome, p->taxa1, p->unidade1);
        if (p->taxa2) ad(&o, ", %u %s/s", p->taxa2, p->unidade2);
        ad(&o, "\n");
    }
    return o.n;
}

/* Comparación con las máquinas de referencia (SM_REFERENCIAS), en barras de
 * texto. Formato del archivo: "versão;máquina;total;enteros;fpu;simd;memoria;2d",
 * '#' empieza un comentario; sólo entran las líneas de la misma versión. */
int sm_comparar(const sm_resultado_t *r, char *buf, int max)
{
    saida_t o = { buf, 0, max };
    if (max > 0) buf[0] = 0;
    static char arq[4096];
    int fd = abrir(SM_REFERENCIAS, ABRIR_LER), n = 0;
    if (fd >= 0) { n = ler(fd, arq, sizeof arq - 1); fechar(fd); }
    if (n < 0) n = 0;
    arq[n] = 0;
    struct { char nome[41]; unsigned total; } m[16];   /* 40 columnas: el nombre se corta */
    int q = 0;
    copia_texto(m[q].nome, "Esta máquina", sizeof m[q].nome);
    m[q++].total = r->erro ? 0 : r->total;
    char *l = arq;
    while (*l && q < 16) {
        char *f = acha_char(l, '\n');
        if (f) *f = 0;
        if (*l && *l != '#') {
            char *c1 = acha_char(l, ';'), *c2 = c1 ? acha_char(c1 + 1, ';') : NULL;
            if (c1 && c2 && texto_para_int(l) == SM_VERSAO) {
                *c2 = 0;
                copia_texto(m[q].nome, c1 + 1, sizeof m[q].nome);
                m[q++].total = (unsigned)texto_para_int(c2 + 1);
            }
        }
        if (!f) break;
        l = f + 1;
    }
    unsigned maior = 1;
    for (int i = 0; i < q; i++) if (m[i].total > maior) maior = m[i].total;
    ad(&o, "Comparación (SaguiMark %u, total; referencias en %s):\n", SM_VERSAO, SM_REFERENCIAS);
    for (int i = 0; i < q; i++) {
        char barra[48];
        int k = (int)(m[i].total * 40 / maior);
        for (int j = 0; j < k && j < 40; j++) barra[j] = '#';
        barra[k > 40 ? 40 : k] = 0;
        char v[16];
        fmt_pontos(v, sizeof v, i == 0 && r->erro ? SM_ERRO : SM_OK, m[i].total);
        ad(&o, "  %-40s %6s  %s\n", m[i].nome, v, barra);
    }
    if (q == 1) ad(&o, "  (no se han encontrado referencias de la versión %u)\n", SM_VERSAO);
    return o.n;
}

/* /home si está montado para escritura; si no /tmp (en la RAM). */
int sm_salvar(const sm_resultado_t *r, char *caminho, int max)
{
    static info_montagem_t mt[10];
    const char *pasta = "/tmp";
    int n = info_sistema(INFO_MONTAGENS, mt, 10);
    for (int i = 0; i < n; i++)
        if (!(mt[i].opcoes & MONTAR_SO_LEITURA) && compara_texto(mt[i].ponto, "/home") == 0) pasta = "/home";
    formatar_texto(caminho, (size_t)max, "%s/saguimark-%04u%02u%02u-%02u%02u.txt", pasta, r->data.ano, r->data.mes,
                   r->data.dia, r->data.hora, r->data.minuto);
    static char t[8192];
    int k = sm_texto(r, t, sizeof t);
    k += sm_comparar(r, t + k, (int)sizeof t - k);
    if (k < (int)sizeof t - 200) {
        int g[SM_GRUPOS_TOTAL];
        for (int i = 0; i < SM_GRUPOS_TOTAL; i++) g[i] = r->estado_grupo[i] == SM_OK ? (int)r->pontos_grupo[i] : 0;
        k += formatar_texto(t + k, sizeof t - (size_t)k,
                            "\nLínea para %s (añádela con el nombre del PC):\n%u;%s;%u;%d;%d;%d;%d;%d\n", SM_REFERENCIAS,
                            SM_VERSAO, r->cpu, r->total, g[0], g[1], g[2], g[3], g[4]);
    }
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int e = escrever_tudo(fd, t, (unsigned)k);
    fechar(fd);
    return e < 0 ? e : k;
}
