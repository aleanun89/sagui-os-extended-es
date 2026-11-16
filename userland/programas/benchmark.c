// SPDX-License-Identifier: GPL-2.0-or-later
/* benchmark.c - benchmark de monoNúcleo y multiNúcleo con puntuación (D176).
 *
 * Ocho cargas con trabajo fijo y resultado comprobado (una suma de comprobación que se
 * compara con la esperada). El tiempo se mide con el TSC (rdtsc), convertido con la
 * frecuencia que el kernel midió contra el PIT (INFO_CPU_MHZ), no con el tic de 10 ms.
 * Cada carga: un calentamiento, R repeticiones y la mediana.
 *
 *   monoNúcleo:  la carga en este proceso, sin otra carga en la máquina.
 *   multiNúcleo: la misma unidad de trabajo a la vez en N trabajadores (N = núcleos que ve
 *                el sistema), como procesos (executar) y como hilos (thread_create).
 *                Cada trabajador anota su TSC de salida y de llegada; el tiempo de la
 *                ronda va del primer inicio al último fin.
 *   Puntos: 1000 x (trabajo total / tiempo) / (trabajo de referencia / tiempo de
 *           referencia). La referencia es QEMU con KVM y una CPU (REF_US, ver D176).
 *           Total = media geométrica de las cargas.
 *
 * Uso: benchmark [--mono | --multi [N]] [--rapido] [--procesos | --hilos] [--maquina]
 *      benchmark --sumas      (diagnóstico: suma de comprobación y tiempo de cada carga)
 *
 * Se compila sin -mgeneral-regs-only (Makefile): las cargas de coma flotante llevan
 * __attribute__((target("sse2"))) y corren si el CPUID confirma SSE2; sin SSE2 usan su variante x87 (D343). */
#include "sagui.h"
#include "sha256.h"

#define MAX_N      8
#define HISTORIAL  "/home/.benchmark"
#define HISTORIAL2 "/tmp/.benchmark"

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
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

static __attribute__((noinline)) double x87_exp2(double y)
{
    double r;
    __asm__("fld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)"
            : "=t"(r) : "0"(y));
    return r;
}
static __attribute__((noinline)) double x87_log2(double x)
{
    double r;
    __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}

static unsigned media_geometrica(const unsigned *v, int n)
{
    if (n <= 0) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) s += x87_log2((double)(v[i] ? v[i] : 1));
    return (unsigned)(x87_exp2(s / n) + 0.5);
}

static unsigned tsc_khz;
static unsigned us_de(uint64_t ciclos) { return (unsigned)div64(ciclos * 1000, tsc_khz); }

static void ordenar_u64(uint64_t *v, int n)
{
    for (int i = 1; i < n; i++) {
        uint64_t x = v[i];
        int k = i - 1;
        while (k >= 0 && v[k] > x) { v[k + 1] = v[k]; k--; }
        v[k + 1] = x;
    }
}

/* ------------------------------------------------------------ las cargas */

typedef struct {   /* los datos de un trabajador (cada hilo y cada proceso tiene los suyos) */
    uint8_t  *crivo;
    uint32_t *a, *b;
    uint32_t *tab;
    uint8_t  *sha;
} ctx_t;

static volatile uint32_t semente = 0x12345678u;   /* entra por un volatile: el compilador no pliega la carga */

enum { G_ENTERO, G_FLOTANTE, G_MEMORIA, G_CIFRADO, G_SISTEMA };
static const char *const nomes_grupo[] = { "Entero", "Flotante", "Memoria", "Cifrado", "Sistema" };

/* Entero 1: cadena de multiplicaciones y xorshift (latencia de la ALU). */
#define HASH_N 72000000u
static uint32_t c_hash(ctx_t *c)
{
    (void)c;
    uint32_t x = semente, h = 2166136261u;
    for (uint32_t i = 0; i < HASH_N; i++) {
        x = x * 1664525u + 1013904223u;
        h = (h ^ x) * 16777619u;
        h ^= h >> 13;
    }
    return h;
}

/* Entero 2: criba de Eratóstenes sobre 1 MB (cabe en la caché L2). */
#define CRIBA_N 1000000u
#define CRIBA_V 100
static uint32_t c_criba(ctx_t *c)
{
    if (!c->crivo && !(c->crivo = aloca(CRIBA_N + 1))) return 0xBAD00001u;
    uint32_t total = 0;
    for (int v = 0; v < CRIBA_V; v++) {
        uint8_t *p = c->crivo;
        memset(p, 1, CRIBA_N + 1);
        p[0] = p[1] = 0;
        for (uint32_t i = 2; i * i <= CRIBA_N; i++)
            if (p[i]) for (uint32_t j = i * i; j <= CRIBA_N; j += i) p[j] = 0;
        uint32_t n = 0;
        for (uint32_t i = 0; i <= CRIBA_N; i++) n += p[i];
        total += n;
    }
    return total + (semente & 0);
}

/* MXCSR por defecto (excepciones enmascaradas). Con varias CPU el kernel puede entregar a un proceso
 * nuevo el área de FPU todavía sin guardar (ceros: MXCSR sin enmascarar, y la primera operación inexacta
 * da #XM); el programa fija su propio entorno de coma flotante antes de medir (ver D176). */
__attribute__((target("sse2")))
static void mxcsr_defecto(void)
{
    uint32_t v = 0x1F80;
    __asm__ volatile("ldmxcsr %0" : : "m"(v));
}

/* Flotante 1: Mandelbrot en double (SSE2). */
#define MANDEL_L 120
#define MANDEL_A 90
#define MANDEL_IT 400
#define MANDEL_V  45
__attribute__((target("sse2,fpmath=sse")))
static uint32_t c_mandel(ctx_t *c)
{
    (void)c;
    mxcsr_defecto();
    double dx = 3.2 / MANDEL_L, dy = 2.4 / MANDEL_A;
    uint32_t soma = 0;
    for (int v = 0; v < MANDEL_V; v++)
      for (int py = 0; py < MANDEL_A; py++)
        for (int px = 0; px < MANDEL_L; px++) {
            double off = (double)(semente & 0);   /* el volatile impide repetir el cálculo una sola vez */
            double cr = -2.2 + px * dx + off, ci = -1.2 + py * dy, zr = 0, zi = 0;
            int it = 0;
            while (it < MANDEL_IT && zr * zr + zi * zi <= 4.0) {
                double t = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = t;
                it++;
            }
            soma += (uint32_t)it;
        }
    return soma + (semente & 0);
}

/* Flotante 2: cinco cuerpos (el problema clásico de n cuerpos) con raíz cuadrada. */
typedef double v2d_t __attribute__((vector_size(16)));
#define NC_PASOS 1600000
__attribute__((target("sse2,fpmath=sse")))
static double raiz(double x)
{
    v2d_t v = { x, x };
    return __builtin_ia32_sqrtsd(v)[0];
}
__attribute__((target("sse2,fpmath=sse")))
static uint32_t c_ncuerpos(ctx_t *c)
{
    (void)c;
    mxcsr_defecto();
    double x[5] = { 0, 4.84, 8.34, 12.89, 15.37 }, y[5] = { 0, -1.16, 4.12, -15.11, -25.9 }, z[5] = { 0, -0.10, -0.4, -0.22, 0.17 };
    double vx[5] = { 0, 0.606, -1.01, 1.08, 0.979 }, vy[5] = { 0, 2.81, 1.82, 0.868, 0.594 }, vz[5] = { 0, -0.02, 0.008, -0.01, -0.034 };
    static const double m[5] = { 39.47, 0.037, 0.011, 0.0017, 0.002 };
    const double dt = 0.01;
    for (int s = 0; s < NC_PASOS + (int)(semente & 0); s++) {
        for (int i = 0; i < 5; i++)
            for (int j = i + 1; j < 5; j++) {
                double ddx = x[i] - x[j], ddy = y[i] - y[j], ddz = z[i] - z[j];
                double d2 = ddx * ddx + ddy * ddy + ddz * ddz;
                double mag = dt / (d2 * raiz(d2));
                vx[i] -= ddx * m[j] * mag; vy[i] -= ddy * m[j] * mag; vz[i] -= ddz * m[j] * mag;
                vx[j] += ddx * m[i] * mag; vy[j] += ddy * m[i] * mag; vz[j] += ddz * m[i] * mag;
            }
        for (int i = 0; i < 5; i++) { x[i] += dt * vx[i]; y[i] += dt * vy[i]; z[i] += dt * vz[i]; }
    }
    double e = 0;
    for (int i = 0; i < 5; i++) e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
    return (uint32_t)(e * 1.0e6);
}

/* Variantes x87 de las dos cargas de coma flotante (D343), para CPU sin SSE2 (Pentium II/III, Athlon antiguos).
 * Mismo cálculo que las de SSE2, compilado con la pila x87 (sin una sola instrucción SSE). Antes de medir se fija
 * la palabra de control a precisión doble (53 bits) y excepciones enmascaradas (0x27F): así los intermedios no
 * van en 80 bits y la suma de comprobación sale igual que con SSE2. Al terminar se restaura la palabra de control
 * anterior. Puntúan con la misma referencia de tiempo: salen más lentas y, por tanto, con menos puntos. */
static uint16_t x87_fijar(void)
{
    uint16_t antes, nueva = 0x027F;
    __asm__ volatile("fnstcw %0" : "=m"(antes));
    __asm__ volatile("fldcw %0" : : "m"(nueva));
    return antes;
}
static void x87_restaurar(uint16_t antes) { __asm__ volatile("fldcw %0" : : "m"(antes)); }

__attribute__((target("no-sse,fpmath=387")))
static uint32_t c_mandel_x87(ctx_t *c)
{
    (void)c;
    uint16_t cw = x87_fijar();
    double dx = 3.2 / MANDEL_L, dy = 2.4 / MANDEL_A;
    uint32_t soma = 0;
    for (int v = 0; v < MANDEL_V; v++)
      for (int py = 0; py < MANDEL_A; py++)
        for (int px = 0; px < MANDEL_L; px++) {
            double off = (double)(semente & 0);
            double cr = -2.2 + px * dx + off, ci = -1.2 + py * dy, zr = 0, zi = 0;
            int it = 0;
            while (it < MANDEL_IT && zr * zr + zi * zi <= 4.0) {
                double t = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = t;
                it++;
            }
            soma += (uint32_t)it;
        }
    x87_restaurar(cw);
    return soma + (semente & 0);
}

__attribute__((target("no-sse,fpmath=387")))
static double raiz_x87(double x)
{
    double r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}
__attribute__((target("no-sse,fpmath=387")))
static uint32_t c_ncuerpos_x87(ctx_t *c)
{
    (void)c;
    uint16_t cw = x87_fijar();
    double x[5] = { 0, 4.84, 8.34, 12.89, 15.37 }, y[5] = { 0, -1.16, 4.12, -15.11, -25.9 }, z[5] = { 0, -0.10, -0.4, -0.22, 0.17 };
    double vx[5] = { 0, 0.606, -1.01, 1.08, 0.979 }, vy[5] = { 0, 2.81, 1.82, 0.868, 0.594 }, vz[5] = { 0, -0.02, 0.008, -0.01, -0.034 };
    static const double m[5] = { 39.47, 0.037, 0.011, 0.0017, 0.002 };
    const double dt = 0.01;
    for (int s = 0; s < NC_PASOS + (int)(semente & 0); s++) {
        for (int i = 0; i < 5; i++)
            for (int j = i + 1; j < 5; j++) {
                double ddx = x[i] - x[j], ddy = y[i] - y[j], ddz = z[i] - z[j];
                double d2 = ddx * ddx + ddy * ddy + ddz * ddz;
                double mag = dt / (d2 * raiz_x87(d2));
                vx[i] -= ddx * m[j] * mag; vy[i] -= ddy * m[j] * mag; vz[i] -= ddz * m[j] * mag;
                vx[j] += ddx * m[i] * mag; vy[j] += ddy * m[i] * mag; vz[j] += ddz * m[i] * mag;
            }
        for (int i = 0; i < 5; i++) { x[i] += dt * vx[i]; y[i] += dt * vy[i]; z[i] += dt * vz[i]; }
    }
    double e = 0;
    for (int i = 0; i < 5; i++) e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
    x87_restaurar(cw);
    return (uint32_t)(e * 1.0e6);
}

/* Memoria 1: copia y lectura secuencial de dos bloques de 32 MB (no caben en la caché). */
#define MEM_BYTES (32u << 20)
#define MEM_N     (MEM_BYTES / 4)
#define MEM_V     24
static uint32_t c_copia(ctx_t *c)
{
    if (!c->a) {
        c->a = aloca(MEM_BYTES);
        c->b = aloca(MEM_BYTES);
        if (!c->a || !c->b) return 0xBAD00002u;
        for (uint32_t i = 0; i < MEM_N; i++) c->a[i] = i * 2654435761u;
        memset(c->b, 0, MEM_BYTES);
    }
    uint32_t soma = 0;
    for (int v = 0; v < MEM_V; v++) {
        memcpy(c->b, c->a, MEM_BYTES);
        uint32_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        for (uint32_t i = 0; i < MEM_N; i += 4) { s0 += c->b[i]; s1 += c->b[i + 1]; s2 += c->b[i + 2]; s3 += c->b[i + 3]; }
        soma += s0 + s1 + s2 + s3;
    }
    return soma;
}

/* Memoria 2: lecturas aleatorias encadenadas en 256 KB (caben en la caché L2). */
#define ALE_N     (1u << 16)
#define ALE_PASOS 36000000u
static uint32_t c_aleat(ctx_t *c)
{
    if (!c->tab) {
        c->tab = aloca(ALE_N * 4);
        if (!c->tab) return 0xBAD00003u;
        for (uint32_t i = 0; i < ALE_N; i++) c->tab[i] = i;
        uint32_t s = 0x9E3779B9u;
        for (uint32_t i = ALE_N - 1; i > 0; i--) {   /* Sattolo: un solo ciclo que recorre todo */
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            uint32_t j = s % i, t = c->tab[i];
            c->tab[i] = c->tab[j];
            c->tab[j] = t;
        }
    }
    uint32_t i = semente & 0;
    for (uint32_t k = 0; k < ALE_PASOS; k++) i = c->tab[i];
    return i;
}

/* Cifrado: SHA-256 (el de la libtls) sobre 1 MB, ocho veces. */
#define SHA_BYTES (1u << 20)
#define SHA_V 32
static uint32_t c_sha(ctx_t *c)
{
    if (!c->sha) {
        c->sha = aloca(SHA_BYTES);
        if (!c->sha) return 0xBAD00004u;
        for (uint32_t i = 0; i < SHA_BYTES; i++) c->sha[i] = (uint8_t)(i * 31u + (i >> 8));
    }
    uint8_t r[SHA256_TAM];
    for (int v = 0; v < SHA_V; v++) {
        sha256(c->sha, SHA_BYTES, r);
        c->sha[0] = r[0];   /* encadena una vuelta con otra */
    }
    c->sha[0] = 0;
    return (uint32_t)r[0] << 24 | (uint32_t)r[1] << 16 | (uint32_t)r[2] << 8 | r[3];
}

/* Sistema: llamadas baratas (pedir el PID) con un poco de cálculo entre ellas. */
#define LLAM_N 600000u
static uint32_t c_chamadas(ctx_t *c)
{
    (void)c;
    uint32_t x = semente, soma = 0;
    int p0 = meu_pid();   /* el PID cambia de un proceso a otro: la suma cuenta las veces que da el mismo */
    for (uint32_t i = 0; i < LLAM_N; i++) {
        soma += meu_pid() == p0;
        for (int k = 0; k < 8; k++) x = x * 1664525u + 1013904223u;
    }
    return soma + x;
}

typedef uint32_t (*fn_carga_t)(ctx_t *);
typedef struct {
    const char *nome;
    int         grupo;
    fn_carga_t  fn;
    bool        sse2;       /* necesita SSE2 (salvo que tenga variante x87) */
    fn_carga_t  fn_x87;     /* variante x87 para CPU sin SSE2 (D343), o NULL */
    uint32_t    esperado;   /* suma de comprobación correcta */
    unsigned    ref_us;     /* microsegundos por unidad en la máquina de referencia (1000 puntos) */
} carga_t;

/* REF_US: QEMU con KVM, una CPU, máquina de desarrollo (D176). */
static const carga_t cargas[] = {
    { "hash",     G_ENTERO,   c_hash,     false, NULL, 0x087f476au, 83300 },
    { "criba",    G_ENTERO,   c_criba,    false, NULL, 0x0077c748u, 83400 },
    { "mandel",   G_FLOTANTE, c_mandel,   true,  c_mandel_x87,   0x0271ec36u, 78900 },
    { "ncuerpos", G_FLOTANTE, c_ncuerpos, true,  c_ncuerpos_x87, 0x00028508u, 79400 },
    { "copia",    G_MEMORIA,  c_copia,    false, NULL, 0xda000000u, 81500 },
    { "aleat",    G_MEMORIA,  c_aleat,    false, NULL, 0x00004bb5u, 86600 },
    { "sha256",   G_CIFRADO,  c_sha,      false, NULL, 0x9dc5baa3u, 92500 },
    { "llamadas", G_SISTEMA,  c_chamadas, false, NULL, 0x36aab438u, 80200 },
};
#define QTD_CARGAS ((int)(sizeof cargas / sizeof cargas[0]))

static void ctx_liberar(ctx_t *c)
{
    if (c->crivo) libera(c->crivo);
    if (c->a) libera(c->a);
    if (c->b) libera(c->b);
    if (c->tab) libera(c->tab);
    if (c->sha) libera(c->sha);
    memset(c, 0, sizeof *c);
}

static bool tem_sse2;
static bool carga_disponivel(int i) { return !cargas[i].sse2 || tem_sse2 || cargas[i].fn_x87; }
/* Sin SSE2, las cargas con variante x87 usan ésa (D343). */
static bool usa_x87(int i) { return cargas[i].sse2 && !tem_sse2 && cargas[i].fn_x87; }
static fn_carga_t fn_de(int i) { return usa_x87(i) ? cargas[i].fn_x87 : cargas[i].fn; }
static const char *nome_de(int i)
{
    static char b[2][24];
    static int k;
    if (!usa_x87(i)) return cargas[i].nome;
    k ^= 1;
    formatar_texto(b[k], sizeof b[k], "%s (x87)", cargas[i].nome);
    return b[k];
}

/* ------------------------------------------------------------ resultados */

enum { E_ND, E_OK, E_ERRO };

typedef struct {
    int      estado;
    uint32_t obtido;
    unsigned us_mono, var_x10, pts_mono;
    unsigned us_mp, pts_mp;     /* multiNúcleo con procesos */
    unsigned us_mh, pts_mh;     /* ... con hilos */
} res_t;

static res_t res[QTD_CARGAS];
static int n_nucleos = 1;
static bool maquina;            /* --maquina: líneas para infopc */
static int reps = 5;
static int fase_feita, fase_total;

static unsigned pontos(int i, unsigned trabalhos, unsigned us)
{
    if (!us) return 0;
    uint64_t p = div64((uint64_t)1000 * cargas[i].ref_us * trabalhos, us);
    return p ? (unsigned)p : 1;
}

static void progresso(const char *fase, const char *nome)
{
    if (maquina) imprimir("P %d %d %s %s\n", fase_feita, fase_total, fase, nome);
    fase_feita++;
}

/* ------------------------------------------------------------ monoNúcleo */

static void medir_mono(int i)
{
    res_t *r = &res[i];
    ctx_t c;
    memset(&c, 0, sizeof c);
    fn_de(i)(&c);   /* calentamiento: reserva y toca la memoria, llena cachés */
    uint64_t t[16];
    int n = reps < 16 ? reps : 16;
    uint32_t obtido = 0;
    for (int k = 0; k < n; k++) {
        uint64_t a = rdtsc();
        obtido = fn_de(i)(&c);
        t[k] = rdtsc() - a;
        if (obtido != cargas[i].esperado) break;
    }
    ctx_liberar(&c);
    r->obtido = obtido;
    if (obtido != cargas[i].esperado) { r->estado = E_ERRO; return; }
    ordenar_u64(t, n);
    unsigned med = us_de(t[n / 2]);
    unsigned dif = us_de(t[n - 1] - t[0]);
    r->estado = E_OK;
    r->us_mono = med;
    r->var_x10 = med ? (unsigned)div64((uint64_t)dif * 500, med) : 0;   /* ± % x 10 = mitad del rango */
    r->pts_mono = pontos(i, 1, med);
}

/* ------------------------------------------------------------ multiNúcleo: hilos */

typedef struct {
    semaforo_t ini[MAX_N];
    semaforo_t fim;
    volatile int carga;        /* -1 = salir */
    volatile uint64_t t0[MAX_N], t1[MAX_N];
    volatile uint32_t obtido[MAX_N];
} hilos_t;
static hilos_t hl;

typedef struct { int idx; } arg_hilo_t;

static void *hilo_trabalhador(void *arg)
{
    int idx = (int)(long)arg;
    ctx_t c;
    memset(&c, 0, sizeof c);
    for (;;) {
        semaforo_tomar(&hl.ini[idx]);
        int carga = hl.carga;
        if (carga < 0) break;
        uint64_t a = rdtsc();
        uint32_t o = fn_de(carga)(&c);
        uint64_t b = rdtsc();
        hl.t0[idx] = a;
        hl.t1[idx] = b;
        hl.obtido[idx] = o;
        semaforo_sinalizar(&hl.fim);
    }
    ctx_liberar(&c);
    return 0;
}

/* Tiempo de una ronda: del primer inicio al último fin; si los TSC de las CPU no
 * estuvieran sincronizados (el rango saldría menor que el trabajador más lento), el
 * del trabajador más lento. */
static uint64_t tempo_ronda(const volatile uint64_t *t0, const volatile uint64_t *t1, int n)
{
    uint64_t min0 = t0[0], max1 = t1[0], maxd = t1[0] - t0[0];
    for (int i = 1; i < n; i++) {
        if (t0[i] < min0) min0 = t0[i];
        if (t1[i] > max1) max1 = t1[i];
        if (t1[i] - t0[i] > maxd) maxd = t1[i] - t0[i];
    }
    uint64_t d = max1 > min0 ? max1 - min0 : 0;
    return d < maxd ? maxd : d;
}

/* Un trabajador de menos no debe colgar la prueba: los que no se pudieron crear no cuentan. */
static bool multi_hilos(int n)
{
    thread_t *h[MAX_N];
    for (int i = 0; i < n; i++) semaforo_criar(&hl.ini[i], 0);
    semaforo_criar(&hl.fim, 0);
    hl.carga = 0;
    for (int i = 0; i < n; i++) {
        h[i] = thread_create(hilo_trabalhador, (void *)(long)i, 0);
        if (!h[i]) {
            hl.carga = -1;
            for (int k = 0; k < i; k++) semaforo_sinalizar(&hl.ini[k]);
            for (int k = 0; k < i; k++) thread_join(h[k]);
            return false;
        }
    }
    for (int i = 0; i < QTD_CARGAS; i++) {
        if (!carga_disponivel(i) || res[i].estado != E_OK) { fase_feita++; continue; }
        progresso("hilos", cargas[i].nome);
        uint64_t t[16];
        int rondas = reps < 16 ? reps : 16;
        bool erro = false;
        for (int k = -1; k < rondas && !erro; k++) {   /* k = -1: calentamiento */
            hl.carga = i;
            for (int w = 0; w < n; w++) semaforo_sinalizar(&hl.ini[w]);
            for (int w = 0; w < n; w++) semaforo_tomar(&hl.fim);
            for (int w = 0; w < n; w++) if (hl.obtido[w] != cargas[i].esperado) erro = true;
            if (k >= 0) t[k] = tempo_ronda(hl.t0, hl.t1, n);
        }
        if (erro) { res[i].estado = E_ERRO; continue; }
        ordenar_u64(t, rondas);
        res[i].us_mh = us_de(t[rondas / 2]);
        res[i].pts_mh = pontos(i, (unsigned)n, res[i].us_mh);
    }
    hl.carga = -1;
    for (int w = 0; w < n; w++) semaforo_sinalizar(&hl.ini[w]);
    for (int w = 0; w < n; w++) thread_join(h[w]);
    return true;
}

/* ------------------------------------------------------------ multiNúcleo: procesos */

typedef struct {
    uint32_t tipo;       /* 1 = listo, 2 = resultado */
    uint32_t idx;
    uint32_t obtido;
    uint32_t reservado;
    uint64_t t0, t1;
} msg_t;                 /* 32 bytes: cabe de una vez en un pipe */

static int fd_livre(void)
{
    for (int fd = 10; fd < 32; fd++)
        if (tipo_descritor(fd) < 0) return fd;
    return -1;
}

static bool ler_msg(int fd, msg_t *m)
{
    uint8_t *p = (uint8_t *)m;
    unsigned feito = 0;
    while (feito < sizeof *m) {
        int n = ler(fd, p + feito, (unsigned)(sizeof *m - feito));
        if (n <= 0) return false;
        feito += (unsigned)n;
    }
    return true;
}

/* El trabajador: argv = --trabajador IDX. El kernel sólo hereda los descriptores 0, 1 y 2: la entrada
 * 0 es el pipe de órdenes (un byte: el número de carga, o 0xFF para salir) y la salida 1 el de respuestas. */
static int trabalhador(int idx)
{
    const int fd_ord = ENTRADA, fd_resp = SAIDA;
    ctx_t c;
    memset(&c, 0, sizeof c);
    msg_t m = { 1, (uint32_t)idx, 0, 0, 0, 0 };
    if (escrever(fd_resp, &m, sizeof m) != (int)sizeof m) return 1;
    for (;;) {
        uint8_t ord;
        if (ler(fd_ord, &ord, 1) != 1 || ord == 0xFF) break;
        int carga = ord;
        if (carga >= QTD_CARGAS) break;
        uint64_t a = rdtsc();
        uint32_t o = fn_de(carga)(&c);
        uint64_t b = rdtsc();
        m.tipo = 2; m.obtido = o; m.t0 = a; m.t1 = b;
        if (escrever(fd_resp, &m, sizeof m) != (int)sizeof m) break;
    }
    ctx_liberar(&c);
    return 0;
}

static bool multi_procesos(int n)
{
    int pid[MAX_N], ord[MAX_N], resp[MAX_N];
    int vivos = 0;
    bool ok = true;
    for (int i = 0; i < n; i++) pid[i] = ord[i] = resp[i] = -1;
    for (int i = 0; i < n && ok; i++) {
        int po[2], pr[2];
        if (criar_pipe(po) < 0) { ok = false; break; }
        if (criar_pipe(pr) < 0) { fechar(po[0]); fechar(po[1]); ok = false; break; }
        char s_idx[8];
        formatar_texto(s_idx, sizeof s_idx, "%d", i);
        char *args[4] = { "benchmark", "--trabajador", s_idx, NULL };
        /* Poner los pipes como entrada y salida, lanzar y devolver las mías. */
        int g0 = fd_livre(), g1 = -1;
        if (g0 >= 0) { duplicar(ENTRADA, g0); g1 = fd_livre(); }
        if (g1 >= 0) duplicar(SAIDA, g1);
        duplicar(po[0], ENTRADA);
        duplicar(pr[1], SAIDA);
        pid[i] = executar("/bin/benchmark", args);
        if (g0 >= 0) { duplicar(g0, ENTRADA); fechar(g0); }
        if (g1 >= 0) { duplicar(g1, SAIDA); fechar(g1); }
        fechar(po[0]);       /* los extremos del hijo ya no me hacen falta */
        fechar(pr[1]);
        if (pid[i] < 0) { fechar(po[1]); fechar(pr[0]); ok = false; break; }
        ord[i] = po[1];
        resp[i] = pr[0];
        vivos++;
    }
    msg_t m;
    for (int i = 0; ok && i < n; i++)   /* todos listos (avisan al empezar) */
        if (!ler_msg(resp[i], &m) || m.tipo != 1) ok = false;
    for (int i = 0; ok && i < QTD_CARGAS; i++) {
        if (!carga_disponivel(i) || res[i].estado != E_OK) { fase_feita++; continue; }
        progresso("procesos", cargas[i].nome);
        uint64_t t[16];
        int rondas = reps < 16 ? reps : 16;
        bool erro = false;
        for (int k = -1; k < rondas && ok; k++) {
            uint64_t t0[MAX_N], t1[MAX_N];
            uint8_t b = (uint8_t)i;
            for (int w = 0; w < n; w++) escrever(ord[w], &b, 1);
            for (int w = 0; w < n; w++) {
                if (!ler_msg(resp[w], &m) || m.tipo != 2) { ok = false; break; }
                t0[w] = m.t0; t1[w] = m.t1;
                if (m.obtido != cargas[i].esperado) erro = true;
            }
            if (ok && k >= 0) t[k] = tempo_ronda(t0, t1, n);
        }
        if (!ok) break;
        if (erro) { res[i].estado = E_ERRO; continue; }
        ordenar_u64(t, rondas);
        res[i].us_mp = us_de(t[rondas / 2]);
        res[i].pts_mp = pontos(i, (unsigned)n, res[i].us_mp);
    }
    for (int w = 0; w < n; w++) {   /* a salir y recoger */
        if (pid[w] < 0) continue;
        uint8_t b = 0xFF;
        if (ord[w] >= 0) { escrever(ord[w], &b, 1); fechar(ord[w]); }
        if (resp[w] >= 0) fechar(resp[w]);
        int codigo;
        esperar(pid[w], &codigo);
    }
    (void)vivos;
    return ok;
}

/* ------------------------------------------------------------ resumen e historial */

typedef struct { unsigned mono, mp, mh; } totais_t;

static totais_t calcular_totais(void)
{
    unsigned a[QTD_CARGAS], b[QTD_CARGAS], c[QTD_CARGAS];
    int na = 0, nb = 0, nc = 0;
    for (int i = 0; i < QTD_CARGAS; i++) {
        if (res[i].estado != E_OK) continue;
        if (res[i].pts_mono) a[na++] = res[i].pts_mono;
        if (res[i].pts_mp) b[nb++] = res[i].pts_mp;
        if (res[i].pts_mh) c[nc++] = res[i].pts_mh;
    }
    totais_t t = { media_geometrica(a, na), media_geometrica(b, nb), media_geometrica(c, nc) };
    return t;
}

static char cpu_nome[49];

static void ler_cpu(void)
{
    uint32_t r[4];
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0x80000000u), "c"(0));
    if (r[0] >= 0x80000004u && r[0] < 0x8000FFFFu) {
        for (uint32_t i = 0; i < 3; i++) {
            __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(0x80000002u + i), "c"(0));
            memcpy(cpu_nome + 16 * i, r, 16);
        }
        cpu_nome[48] = 0;
        int k = 0;
        while (cpu_nome[k] == ' ') k++;
        if (k) memmove(cpu_nome, cpu_nome + k, 49 - (size_t)k);
    } else {
        copia_texto(cpu_nome, "CPU sin nombre", sizeof cpu_nome);
    }
    for (int i = 0; cpu_nome[i]; i++) if (cpu_nome[i] == '|') cpu_nome[i] = '/';
}

/* Una línea por ejecución: fecha|cpu|núcleos|planificador|modo|mono|procesos|hilos|puntos... */
static void guardar_historial(const char *modo, totais_t t)
{
    data_t d;
    ler_data(&d);
    char linha[320];
    int n = formatar_texto(linha, sizeof linha, "%04d-%02d-%02d %02d:%02d|%s|%d|-|%s|%u|%u|%u|",
                           d.ano, d.mes, d.dia, d.hora, d.minuto, cpu_nome, n_nucleos, modo, t.mono, t.mp, t.mh);
    for (int i = 0; i < QTD_CARGAS && n < (int)sizeof linha - 40; i++)
        n += formatar_texto(linha + n, sizeof linha - (size_t)n, "%s%u,%u,%u", i ? ";" : "",
                            res[i].pts_mono, res[i].pts_mp, res[i].pts_mh);
    n += formatar_texto(linha + n, sizeof linha - (size_t)n, "\n");
    static const char *const caminhos[] = { HISTORIAL, HISTORIAL2 };
    for (int k = 0; k < 2; k++) {
        int fd = abrir(caminhos[k], ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
        if (fd < 0) continue;
        escrever(fd, linha, (unsigned)n);
        fechar(fd);
        if (!maquina) imprimir("Historial: %s\n", caminhos[k]);
        registrar("benchmark: historial guardado en %s\n", caminhos[k]);
        return;
    }
    if (!maquina) imprimir("(no se pudo guardar el historial)\n");
}

static void mostrar(totais_t t, bool mono, bool mp, bool mh, int erros, unsigned desvio_x10)
{
    if (maquina) {
        for (int i = 0; i < QTD_CARGAS; i++)
            imprimir("R %s %d %d %u %u %u %u %d\n", cargas[i].nome, cargas[i].grupo, res[i].estado, res[i].pts_mono,
                     res[i].var_x10, res[i].pts_mp, res[i].pts_mh, usa_x87(i) ? 1 : 0);   /* último campo: 1 = variante x87 (D343) */
    } else {
        imprimir("\n%-13s %-9s", "Carga", "Grupo");
        if (mono) imprimir(" %7s %6s", "Mono", "+-%");
        if (mp) imprimir(" | %7s %6s %5s", "Procs", "Escala", "Efic");
        if (mh) imprimir(" | %7s %6s %5s", "Hilos", "Escala", "Efic");
        imprimir("\n");
        for (int i = 0; i < QTD_CARGAS; i++) {
            res_t *r = &res[i];
            imprimir("%-13s %-9s", nome_de(i), nomes_grupo[cargas[i].grupo]);
            if (r->estado != E_OK) {
                imprimir(" %s\n", r->estado == E_ERRO ? "ERROR (suma de comprobacion distinta)" : "n/d (la CPU no tiene SSE2)");
                continue;
            }
            if (mono) imprimir(" %7u %3u.%u%%", r->pts_mono, r->var_x10 / 10, r->var_x10 % 10);
            unsigned e;
            if (mp) {
                e = r->pts_mono ? r->pts_mp * 100 / r->pts_mono : 0;
                imprimir(" | %7u %3u.%02ux %3u%%", r->pts_mp, e / 100, e % 100, e / (unsigned)n_nucleos);
            }
            if (mh) {
                e = r->pts_mono ? r->pts_mh * 100 / r->pts_mono : 0;
                imprimir(" | %7u %3u.%02ux %3u%%", r->pts_mh, e / 100, e % 100, e / (unsigned)n_nucleos);
            }
            imprimir("\n");
        }
        imprimir("\n");
        for (int i = 0; i < QTD_CARGAS; i++)
            if (usa_x87(i)) { imprimir("(x87) = la CPU no tiene SSE2: esas cargas usan la FPU x87 y puntúan menos (D343).\n\n"); break; }
    }
    for (int i = 0; i < QTD_CARGAS; i++)   /* una línea por carga en el registro: las pruebas la leen */
        registrar("benchmark: carga %s estado=%d mono=%u var=%u.%u procesos=%u hilos=%u x87=%d\n", cargas[i].nome, res[i].estado,
                  res[i].pts_mono, res[i].var_x10 / 10, res[i].var_x10 % 10, res[i].pts_mp, res[i].pts_mh, usa_x87(i) ? 1 : 0);
    unsigned melhor = t.mp > t.mh ? t.mp : t.mh;
    unsigned esc = (mono && t.mono && melhor) ? melhor * 100 / t.mono : 0;   /* x100 */
    unsigned efi = esc / (unsigned)n_nucleos;
    char linha[200];
    int n = formatar_texto(linha, sizeof linha, "total mono=%u procesos=%u hilos=%u escala=%u.%02ux eficiencia=%u%% nucleos=%d errores=%d",
                           t.mono, t.mp, t.mh, esc / 100, esc % 100, efi, n_nucleos, erros);
    n += formatar_texto(linha + n, sizeof linha - (size_t)n, " tsc_desvio=%u.%u%%", desvio_x10 / 10, desvio_x10 % 10);
    if (maquina) imprimir("T %u %u %u %u %u %d %d\n", t.mono, t.mp, t.mh, esc, efi, n_nucleos, erros);
    else imprimir("%s\n", linha);
    registrar("benchmark: %s\n", linha);
}

/* ------------------------------------------------------------ principal */

static void uso(void)
{
    imprimir("Uso: benchmark [--mono | --multi [N]] [--rapido] [--procesos | --hilos]\n"
             "  sin opciones: monoNúcleo y multiNúcleo (con procesos y con hilos)\n"
             "  --mono        sólo un núcleo (sin otra carga en la máquina)\n"
             "  --multi [N]   también N trabajadores a la vez (por defecto, los núcleos que ve el sistema)\n"
             "  --rapido      3 repeticiones en vez de 5\n"
             "  --procesos / --hilos   sólo esa variante multiNúcleo\n"
             "Puntos: 1000 por carga = QEMU con KVM y una CPU (D176); el total es la media geométrica.\n"
             "El historial queda en /home/.benchmark.\n");
}

static int sumas(void)
{
    ctx_t c;
    memset(&c, 0, sizeof c);
    for (int i = 0; i < QTD_CARGAS; i++) {
        if (!carga_disponivel(i)) continue;
        fn_de(i)(&c);
        uint64_t a = rdtsc();
        uint32_t o = fn_de(i)(&c);
        unsigned us = us_de(rdtsc() - a);
        imprimir("SUMA %s 0x%08x %u us\n", nome_de(i), o, us);
        registrar("benchmark: SUMA %s 0x%08x %u us\n", cargas[i].nome, o, us);
    }
    ctx_liberar(&c);
    return 0;
}

int principal(int argc, char **argv)
{
    bool so_mono = false, rapido = false, so_procs = false, so_hilos = false, pede_sumas = false;
    int n_pedido = 0;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--mono") == 0) so_mono = true;
        else if (compara_texto(argv[i], "--multi") == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') texto_para_numero(argv[++i], &n_pedido);
        }
        else if (compara_texto(argv[i], "--rapido") == 0) rapido = true;
        else if (compara_texto(argv[i], "--procesos") == 0) so_procs = true;
        else if (compara_texto(argv[i], "--hilos") == 0) so_hilos = true;
        else if (compara_texto(argv[i], "--maquina") == 0) maquina = true;
        else if (compara_texto(argv[i], "--sumas") == 0) pede_sumas = true;
        else if (compara_texto(argv[i], "--trabajador") == 0 && i + 1 < argc) {
            info_tsc_t tsc;
            memset(&tsc, 0, sizeof tsc);
            info_sistema(INFO_CPU_MHZ, &tsc, sizeof tsc);
            tsc_khz = tsc.khz;
            int idx = 0;
            texto_para_numero(argv[i + 1], &idx);
            tem_sse2 = cpu_tem_sse2();
            return trabalhador(idx);
        }
        else { uso(); return compara_texto(argv[i], "-h") == 0 || compara_texto(argv[i], "--ayuda") == 0 ? 0 : 1; }
    }
    if (rapido) reps = 3;

    info_tsc_t tsc;
    memset(&tsc, 0, sizeof tsc);
    info_sistema(INFO_CPU_MHZ, &tsc, sizeof tsc);
    tsc_khz = tsc.khz;
    tem_sse2 = cpu_tem_sse2();
    uint32_t c1[4];
    __asm__ volatile("cpuid" : "=a"(c1[0]), "=b"(c1[1]), "=c"(c1[2]), "=d"(c1[3]) : "a"(1), "c"(0));
    if (!((c1[3] >> 4) & 1) || !tsc_khz) {
        imprimir("benchmark: la CPU no tiene TSC (o el kernel no midió su frecuencia).\n");
        return 1;
    }
    if (pede_sumas) return sumas();

    static info_nucleo_t cpus[MAX_N];
    int ncpu = info_sistema(INFO_NUCLEOS, cpus, MAX_N);
    if (ncpu < 1) ncpu = 1;
    if (ncpu > MAX_N) ncpu = MAX_N;
    n_nucleos = n_pedido > 0 ? (n_pedido > MAX_N ? MAX_N : n_pedido) : ncpu;
    ler_cpu();

    bool faz_mp = !so_mono && !so_hilos, faz_mh = !so_mono && !so_procs;
    int fases = 1 + (faz_mp ? 1 : 0) + (faz_mh ? 1 : 0);
    fase_total = fases * QTD_CARGAS;

    if (maquina) {
        imprimir("C %s\n", cpu_nome);
        imprimir("I %d %u %d\n", n_nucleos, tsc_khz, fase_total);
    } else {
        imprimir("SaguiOS benchmark (D176): %s\n", cpu_nome);
        imprimir("Núcleos: %d   TSC: %u.%03u MHz   repeticiones: %d   modo: %s\n", n_nucleos, tsc_khz / 1000, tsc_khz % 1000,
                 reps, so_mono ? "un núcleo" : "un núcleo y multinúcleo");
    }

    unsigned ms0 = tempo_ms();
    uint64_t tsc0 = rdtsc();
    for (int i = 0; i < QTD_CARGAS; i++) {
        if (!carga_disponivel(i)) { res[i].estado = E_ND; fase_feita++; continue; }
        progresso("mono", cargas[i].nome);
        medir_mono(i);
        if (!maquina)
            imprimir("  mono %-13s %s\n", nome_de(i), res[i].estado == E_OK ? "ok" : "ERROR");
    }
    if (faz_mp) {
        if (!maquina) imprimir("  multinúcleo con procesos (%d)...\n", n_nucleos);
        if (!multi_procesos(n_nucleos)) imprimir("benchmark: fallo en la variante de procesos\n");
    }
    if (faz_mh) {
        if (!maquina) imprimir("  multinúcleo con hilos (%d)...\n", n_nucleos);
        if (!multi_hilos(n_nucleos)) imprimir("benchmark: fallo en la variante de hilos\n");
    }
    unsigned ms = tempo_ms() - ms0;
    unsigned tsc_ms = us_de(rdtsc() - tsc0) / 1000;
    unsigned desvio_x10 = 0;
    if (ms) {
        unsigned dif = tsc_ms > ms ? tsc_ms - ms : ms - tsc_ms;
        desvio_x10 = (unsigned)div64((uint64_t)dif * 1000, ms);
    }

    int erros = 0;
    for (int i = 0; i < QTD_CARGAS; i++) if (res[i].estado == E_ERRO) erros++;
    totais_t t = calcular_totais();
    mostrar(t, true, faz_mp, faz_mh, erros, desvio_x10);
    if (!erros) guardar_historial(so_mono ? "mono" : so_procs ? "multi-p" : so_hilos ? "multi-h" : "todo", t);
    return erros ? 2 : 0;
}
