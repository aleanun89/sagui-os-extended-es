// SPDX-License-Identifier: GPL-2.0-or-later
/* ecualizador.c - coeficientes del ecualizador de 5 bandas (D201).
 *
 * El kernel no usa la FPU: aquí, con la x87 (como saguimark), se calculan los biquads
 * del "Audio EQ Cookbook" de R. Bristow-Johnson y se pasan ya en enteros Q28 por
 * AUDIO_EQ. Bandas: 60 Hz (low-shelf), 250 Hz, 1 kHz, 4 kHz (peaking, Q = 1) y 12 kHz
 * (high-shelf), cada una de -12 a +12 dB. Se compila sin -mgeneral-regs-only (Makefile).
 * Con EQ_ANFITRION se compila en el PC para la prueba de unidad. */
#ifdef EQ_ANFITRION
#include "abi.h"
#else
#include "sagui.h"
#endif

static const int eq_freq[EQ_BANDAS] = { 60, 250, 1000, 4000, 12000 };
static const char *const eq_preajustes[][1] = { { "plano" }, { "graves" }, { "voz" }, { "agudos" } };
static const signed char eq_preajuste_db[4][EQ_BANDAS] = {
    { 0, 0, 0, 0, 0 },        /* plano */
    { 6, 4, 0, 0, 0 },        /* graves */
    { -3, -1, 3, 3, -2 },     /* voz */
    { 0, 0, 0, 4, 6 },        /* agudos */
};

int eq_freq_banda(int i) { return eq_freq[i]; }

/* 0 = encontrado (rellena db), -1 = no existe. */
int eq_preajuste(const char *nome, int *db)
{
    for (int p = 0; p < 4; p++) {
        const char *a = eq_preajustes[p][0], *b = nome;
        while (*a && *a == *b) { a++; b++; }
        if (*a || *b) continue;
        for (int i = 0; i < EQ_BANDAS; i++) db[i] = eq_preajuste_db[p][i];
        return 0;
    }
    return -1;
}

/* Nombre del preajuste que coincide con db, o NULL. */
const char *eq_nome_preajuste(const int *db)
{
    for (int p = 0; p < 4; p++) {
        int ig = 1;
        for (int i = 0; i < EQ_BANDAS; i++) ig &= db[i] == eq_preajuste_db[p][i];
        if (ig) return eq_preajustes[p][0];
    }
    return 0;
}

/* ---- x87 ---- */
static double x_sin(double x) { double r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
static double x_cos(double x) { double r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
static double x_raiz(double x) { double r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
static __attribute__((noinline)) double x_exp2(double y)
{
    double r;
    __asm__("fld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)"
            : "=t"(r) : "0"(y));
    return r;
}
static __attribute__((noinline)) double x_log2(double x)
{
    double r;
    __asm__("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}
/* 10^(dB/k) */
static double dez_a(double db, double k) { return x_exp2(db / k * 3.321928094887362); }

#define PI 3.14159265358979323846

typedef struct { double b0, b1, b2, a1, a2; } bq_t;   /* ya divididos por a0 */

static bq_t biquad(int banda, double db, double fs)
{
    double A = dez_a(db, 40.0), w = 2 * PI * eq_freq[banda] / fs, cs = x_cos(w), sn = x_sin(w);
    double b0, b1, b2, a0, a1, a2;
    if (banda == 0 || banda == EQ_BANDAS - 1) {
        double al = sn / 2 * 1.4142135623730951;          /* S = 1 */
        double r = 2 * x_raiz(A) * al;
        if (banda == 0) {
            b0 = A * ((A + 1) - (A - 1) * cs + r);
            b1 = 2 * A * ((A - 1) - (A + 1) * cs);
            b2 = A * ((A + 1) - (A - 1) * cs - r);
            a0 = (A + 1) + (A - 1) * cs + r;
            a1 = -2 * ((A - 1) + (A + 1) * cs);
            a2 = (A + 1) + (A - 1) * cs - r;
        } else {
            b0 = A * ((A + 1) + (A - 1) * cs + r);
            b1 = -2 * A * ((A - 1) + (A + 1) * cs);
            b2 = A * ((A + 1) + (A - 1) * cs - r);
            a0 = (A + 1) - (A - 1) * cs + r;
            a1 = 2 * ((A - 1) - (A + 1) * cs);
            a2 = (A + 1) - (A - 1) * cs - r;
        }
    } else {
        double al = sn / 2;                               /* Q = 1 */
        b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A;
        a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A;
    }
    bq_t q = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    return q;
}

/* |H(e^jw)|^2 de un biquad. */
static double mag2(const bq_t *q, double w)
{
    double c1 = x_cos(w), s1 = x_sin(w), c2 = x_cos(2 * w), s2 = x_sin(2 * w);
    double nr = q->b0 + q->b1 * c1 + q->b2 * c2, ni = -(q->b1 * s1 + q->b2 * s2);
    double dr = 1 + q->a1 * c1 + q->a2 * c2, di = -(q->a1 * s1 + q->a2 * s2);
    return (nr * nr + ni * ni) / (dr * dr + di * di);
}

static int redondea(double x) { return (int)(x < 0 ? x - 0.5 : x + 0.5); }

int eq_calcular_m(const int *db, unsigned taxa, eq_audio_t *e, int margen);

/* Calcula la cascada para realces db[5] (dB enteros, -12..+12) a la tasa dada. Las bandas
 * en 0 dB no gastan etapa; si todas están en 0, etapas = 0 (derivación total: el sonido sale
 * bit a bit igual). Margen automático: la ganancia previa es el pico de la respuesta total
 * (si es positivo), metida en los b de la primera etapa. 0 = bien, -1 = parámetro no válido. */
int eq_calcular(const int *db, unsigned taxa, eq_audio_t *e) { return eq_calcular_m(db, taxa, e, 1); }

/* margen = 0: sin ganancia previa (solo para las pruebas de la respuesta de cada banda). */
int eq_calcular_m(const int *db, unsigned taxa, eq_audio_t *e, int margen)
{
    bq_t q[EQ_BANDAS];
    int n = 0;
    for (unsigned i = 0; i < sizeof *e; i++) ((char *)e)[i] = 0;
    if (taxa < 8000) taxa = 48000;
    double fs = taxa;
    for (int i = 0; i < EQ_BANDAS; i++) {
        if (db[i] < -12 || db[i] > 12) return -1;
        e->db[i] = (signed char)db[i];
        if (db[i] == 0 || eq_freq[i] > fs * 0.45) continue;   /* sin efecto o por encima de Nyquist */
        q[n++] = biquad(i, db[i], fs);
    }
    if (!n) return 0;
    /* pico de la respuesta, 20 Hz a 0,45 fs, 96 puntos logarítmicos */
    double pico = 0, fmax = fs * 0.45, r = fmax / 20.0, lr = x_log2(r);
    for (int k = 0; k < 96; k++) {
        double f = 20.0 * x_exp2(lr * k / 95.0), w = 2 * PI * f / fs, m = 1;
        for (int j = 0; j < n; j++) m *= mag2(&q[j], w);
        if (m > pico) pico = m;
    }
    double g = margen && pico > 1.0 ? 1.0 / x_raiz(pico) : 1.0;
    q[0].b0 *= g; q[0].b1 *= g; q[0].b2 *= g;
    for (int j = 0; j < n; j++) {
        double v[5] = { q[j].b0, q[j].b1, q[j].b2, q[j].a1, q[j].a2 };
        for (int c = 0; c < 5; c++) {
            if (v[c] > 7.9 || v[c] < -7.9) return -1;
            e->coef[j][c] = redondea(v[c] * 268435456.0);
        }
    }
    e->etapas = (unsigned)n;
    return 0;
}

#ifndef EQ_ANFITRION
#define EQ_DIR_CONF "/home/.config"
#define EQ_ARQ_CONF "/home/.config/sagui.conf"

/* "+3", "-2", "0" (el printf de la libsagui no tiene %+d). d: 4 bytes como mínimo. */
char *eq_db_texto(char *d, int v)
{
    formatar_texto(d, 4, v > 0 ? "+%d" : "%d", v);
    return d;
}

static bool home_persistente(void)
{
    info_montagem_t m[10];
    int n = info_sistema(INFO_MONTAGENS, m, 10);
    for (int i = 0; i < n && i < 10; i++)
        if (compara_texto(m[i].ponto, "/home") == 0 && !(m[i].opcoes & MONTAR_SO_LEITURA)) return true;
    return false;
}

/* "0,+3,-2,0,12" -> db[5]. 0 = bien. */
int eq_texto_ler(const char *v, int *db)
{
    for (int i = 0; i < EQ_BANDAS; i++) {
        int neg = 0, n = 0, dig = 0;
        if (*v == '+' || *v == '-') neg = *v++ == '-';
        for (; *v >= '0' && *v <= '9'; v++, dig++) { n = n * 10 + (*v - '0'); if (n > 99) return -1; }
        if (!dig || n > 12) return -1;
        db[i] = neg ? -n : n;
        if (i < EQ_BANDAS - 1) { if (*v++ != ',') return -1; }
    }
    return *v == 0 || *v == '\r' || *v == '\n' ? 0 : -1;
}

/* Lee la clave ecualizador= de sagui.conf. 0 = encontrada y válida. */
int eq_conf_ler(int *db)
{
    static char t[2048];
    int fd = abrir(EQ_ARQ_CONF, ABRIR_LER);
    if (fd < 0) return -1;
    int n = ler(fd, t, sizeof t - 1);
    fechar(fd);
    if (n <= 0) return -1;
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        if (comeca_com(p, "ecualizador=")) return eq_texto_ler(p + 12, db);
        if (!fim) break;
        p = fim + 1;
    }
    return -1;
}

/* Pone (o cambia) la clave ecualizador= de sagui.conf sin tocar el resto. 1 = /home no es un
 * disco persistente (no se guarda); <0 = error. */
int eq_conf_gravar(const int *db)
{
    static char t[2048], o[2200];
    int n = 0, no = 0;
    if (!home_persistente()) return 1;
    int fd = abrir(EQ_ARQ_CONF, ABRIR_LER);
    if (fd >= 0) { n = ler(fd, t, sizeof t - 1); fechar(fd); if (n < 0) n = 0; }
    t[n] = 0;
    for (char *p = t; *p;) {
        char *fim = acha_char(p, '\n');
        if (fim) *fim = 0;
        if (*p && !comeca_com(p, "ecualizador=")) {
            no += formatar_texto(o + no, sizeof o - (unsigned)no, "%s\n", p);
        }
        if (!fim) break;
        p = fim + 1;
    }
    no += formatar_texto(o + no, sizeof o - (unsigned)no, "ecualizador=%d,%d,%d,%d,%d\n", db[0], db[1], db[2], db[3], db[4]);
    criar_pasta(EQ_DIR_CONF);
    fd = abrir(EQ_ARQ_CONF, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    int r = escrever_tudo(fd, o, (unsigned)no);
    fechar(fd);
    return r < 0 ? r : 0;
}

/* Calcula y manda al mezclador. Devuelve 0, o <0 si no hay tarjeta o el kernel lo rechaza. */
int eq_aplicar(const int *db)
{
    info_audio_t ia;
    unsigned taxa = 48000;
    if (info_sistema(INFO_AUDIO, &ia, sizeof ia) == 0 && ia.taxa) taxa = ia.taxa;
    static eq_audio_t e;
    if (eq_calcular(db, taxa, &e) < 0) return -1;
    int r = audio_controle(-1, AUDIO_EQ, (int)&e);
    return r < 0 ? r : 0;
}
#endif
