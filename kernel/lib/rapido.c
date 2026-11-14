// SPDX-License-Identifier: GPL-2.0-or-later
/* rapido.c - copias y rellenos grandes de la interfaz gráfica (D042).
 *
 * El compositor de ventanas y la copia del búfer de atrás a la placa mueven
 * varios MB por segundo. Hay tres formas de hacerlo en un i686:
 *   - "rep movsd" / "rep stosd": funciona en cualquier CPU, sin SSE; en las que
 *     tienen fast strings/ERMSB (el G470, Sandy Bridge) la propia CPU copia en
 *     bloques grandes cuando la memoria es normal (write-back);
 *   - SSE2 (movdqu/movdqa): 16 bytes por instrucción; en la placa de vídeo, que
 *     suele ser memoria sin caché (UC) o write-combining (WC), el
 *     rep movsd se convierte en 4 bytes por escritura y el SSE2 manda 16;
 *   - SSE2 con escritura no temporal (movntdq + sfence): no lee la línea de
 *     caché antes de escribir ni la deja en caché; bueno para la placa y para
 *     bloques mayores que la caché.
 * Cuál es mejor depende de la máquina (en QEMU sin KVM el SSE se emula), así que
 * rapido_iniciar() mide cada uno en el arranque (unos milisegundos, con el
 * reloj de la CPU, RDTSC) y elige. El resultado va al log ([RAPIDO]).
 *
 * El SSE2 solo corre entre rapido_inicio() y rapido_fim(), que abren y cierran
 * fpu_kernel_inicio/fim (guarda los XMM del proceso dueño). Fuera de eso, o en
 * bloques pequeños (por debajo de LIMIAR bytes), usa rep movsd.
 *
 * Placa en 16 o 15 bits (D078): la copia a la pantalla convierte cada píxel
 * (rapido_converter16). En C, dos píxeles por escritura de 32 bits; con SSE2,
 * ocho por vuelta. La medida del arranque elige también entre los dos. */
#include "rapido.h"
#include "console.h"
#include "fpu.h"
#include "memoria.h"
#include "sse2.h"
#include "texto.h"
#include "video.h"

#define LIMIAR 64   /* bytes; por debajo de esto las vueltas de alineación no compensan */

/* D208: una variante por juego de instrucciones. SSE (movups/movntps + prefetch, para el
 * Pentium III y el Athlon XP), SSE2 (movdqu/movntdq) y SSE3 (lddqu para leer sin alinear).
 * Cada una, normal y no temporal. Solo se miden las que la CPU tiene (CPUID). */
enum { REP, SSE, SSE_NT, SSE2, SSE2_NT, SSE3, SSE3_NT, QTD_METODOS };
static const char *const nome_metodo[QTD_METODOS] = { "rep", "sse", "sse-nt", "sse2", "sse2-nt", "sse3", "sse3-nt" };

static bool metodo_existe(int m)
{
    uint32_t r = fpu_recursos();
    switch (m) {
    case REP: return true;
    case SSE: case SSE_NT: return (r & CPU_SSE) != 0;
    case SSE2: case SSE2_NT: return (r & CPU_SSE2) != 0;
    case SSE3: case SSE3_NT: return (r & (CPU_SSE2 | CPU_SSE3)) == (CPU_SSE2 | CPU_SSE3);
    }
    return false;
}
static int met_copia = REP, met_tela = REP, met_preencher = REP, met_conv = REP;   /* met_conv: REP = en C */
static bool usar_sse;   /* algún método elegido usa XMM */

static inline void rep_copiar(void *d, const void *o, size_t n)
{
    uint32_t q = n >> 2, r = n & 3;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(o), "+c"(q) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(o), "+c"(r) : : "memory");
}

static inline void rep_preencher32(uint32_t *d, uint32_t cor, size_t n)
{
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(cor) : "memory");
}

static void copiar_com(int m, void *d, const void *o, size_t n)
{
    if (m == REP || n < LIMIAR || !fpu_kernel_ativo()) rep_copiar(d, o, n);
    else switch (m) {
    case SSE:     sse1_copiar(d, o, n); break;
    case SSE_NT:  sse1_copiar_nt(d, o, n); break;
    case SSE2:    sse2_copiar(d, o, n); break;
    case SSE2_NT: sse2_copiar_nt(d, o, n); break;
    case SSE3:    sse3_copiar(d, o, n); break;
    default:      sse3_copiar_nt(d, o, n); break;
    }
}

void rapido_copiar(void *d, const void *o, size_t n)      { copiar_com(met_copia, d, o, n); }
void rapido_copiar_tela(void *d, const void *o, size_t n) { copiar_com(met_tela, d, o, n); }

void rapido_preencher32(uint32_t *d, uint32_t cor, size_t n)
{
    if (met_preencher == REP || n * 4 < LIMIAR || !fpu_kernel_ativo()) rep_preencher32(d, cor, n);
    else if (met_preencher == SSE) sse1_preencher32(d, cor, n);
    else sse2_preencher32(d, cor, n);
}

/* 0x00RRGGBB -> RGB565 o RGB555 (el C de siempre; la misma cuenta de sse2.c). */
static inline uint32_t pixel16(uint32_t c, int r555)
{
    return r555 ? ((c >> 9) & 0x7C00) | ((c >> 6) & 0x03E0) | ((c >> 3) & 0x1F)
                : ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F);
}

static void converter_c(uint16_t *d, const uint32_t *o, size_t n, int r555)
{
    /* En la placa sin write-combining cada escritura se convierte en un viaje al bus:
     * dos píxeles por escritura de 32 bits, con el destino alineado. */
    if (n && ((uint32_t)d & 2)) { *d++ = (uint16_t)pixel16(*o++, r555); n--; }
    for (; n >= 2; n -= 2, d += 2, o += 2)
        *(uint32_t *)d = pixel16(o[0], r555) | pixel16(o[1], r555) << 16;
    if (n) *d = (uint16_t)pixel16(*o, r555);
}

static void converter_com(int m, uint16_t *d, const uint32_t *o, size_t n, int r555)
{
    if (m == REP || n * 4 < LIMIAR || !fpu_kernel_ativo()) converter_c(d, o, n, r555);
    else sse2_converter16(d, o, n, r555);
}

void rapido_converter16(uint16_t *d, const uint32_t *o, size_t n, bool r555)
{
    converter_com(met_conv, d, o, n, r555);
}

bool rapido_inicio(void) { return usar_sse && fpu_kernel_inicio(); }
void rapido_fim(bool aberto) { if (aberto) fpu_kernel_fim(); }

/* ------------------------------------------------------------ medida en el arranque */

static inline uint64_t ciclos(void)
{
    uint32_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return ((uint64_t)d << 32) | a;
}

/* Menor tiempo de 3 vueltas, en ciclos/1024 (cabe en 32 bits). El SSE2 corre
 * dentro de un fpu_kernel_inicio/fim, como en el uso real. */
static uint32_t medir_copia(int m, void *d, const void *o, size_t n)
{
    uint32_t melhor = 0xFFFFFFFFu;
    for (int v = 0; v < 3; v++) {
        bool aberto = m != REP && fpu_kernel_inicio();
        uint64_t t = ciclos();
        copiar_com(m, d, o, n);
        t = ciclos() - t;
        if (aberto) fpu_kernel_fim();
        uint32_t k = (uint32_t)(t >> 10) + 1;
        if (k < melhor) melhor = k;
    }
    return melhor;
}

static uint32_t medir_preencher(int m, uint32_t *d, size_t n)
{
    uint32_t melhor = 0xFFFFFFFFu;
    for (int v = 0; v < 3; v++) {
        bool aberto = m != REP && fpu_kernel_inicio();
        int antes = met_preencher;
        met_preencher = m;
        uint64_t t = ciclos();
        rapido_preencher32(d, 0x00123456, n);
        t = ciclos() - t;
        met_preencher = antes;
        if (aberto) fpu_kernel_fim();
        uint32_t k = (uint32_t)(t >> 10) + 1;
        if (k < melhor) melhor = k;
    }
    return melhor;
}

/* El SSE2 solo gana si es al menos un 10% más rápido (ruido de la medida). */
static uint32_t medir_conversao(int m, uint16_t *d, const uint32_t *o, size_t n, int r555)
{
    uint32_t melhor = 0xFFFFFFFFu;
    for (int v = 0; v < 3; v++) {
        bool aberto = m != REP && fpu_kernel_inicio();
        uint64_t t = ciclos();
        converter_com(m, d, o, n, r555);
        t = ciclos() - t;
        if (aberto) fpu_kernel_fim();
        uint32_t k = (uint32_t)(t >> 10) + 1;
        if (k < melhor) melhor = k;
    }
    return melhor;
}

static int escolher(const uint32_t *t, int qtd)
{
    int m = REP;
    for (int k = 1; k < qtd; k++)
        if (t[k] != 0xFFFFFFFFu && t[k] * 10 < t[REP] * 9 && t[k] < t[m]) m = k;
    return m;
}

/* RAM -> placa: las primeras 256 líneas de la pantalla (1 MB a 1024x768), con
 * lo que ya está en el búfer de atrás: la imagen no cambia. En la placa de verdad,
 * si la memoria de vídeo es sin caché (UC), cada vuelta tarda algunos ms; en
 * write-combining (D057) el rep movsd ya junta las escrituras y la diferencia
 * con el SSE2 disminuye. Por eso la medida viene después de wc_mapear. */
static void medir_tela(uint32_t *tt)
{
    uint32_t *tras = video_buffer();
    uint8_t *placa = video_placa();
    if (video_ativo() && placa && video_formato_direto() && video_pitch() == video_largura() * 4) {
        uint32_t linhas = video_altura() < 256 ? video_altura() : 256;
        size_t bytes = (size_t)video_largura() * linhas * 4;
        for (int m = 0; m < QTD_METODOS; m++) tt[m] = metodo_existe(m) ? medir_copia(m, placa, tras, bytes) : 0xFFFFFFFFu;
        met_tela = escolher(tt, QTD_METODOS);
    } else if (video_ativo() && placa && video_bpp() <= 16 && video_pitch() == video_largura() * 2) {
        /* 16 o 15 bits: C o SSE2 en la conversión (D078); tt[2] queda en 0. */
        uint32_t linhas = video_altura() < 256 ? video_altura() : 256;
        size_t px = (size_t)video_largura() * linhas;
        tt[REP] = medir_conversao(REP, (uint16_t *)placa, tras, px, video_bpp() == 15);
        tt[SSE] = metodo_existe(SSE2) ? medir_conversao(SSE2, (uint16_t *)placa, tras, px, video_bpp() == 15) : 0xFFFFFFFFu;
        met_conv = escolher(tt, 2) ? SSE2 : REP;
    }
    usar_sse = met_copia != REP || met_tela != REP || met_preencher != REP || met_conv != REP;
}

void rapido_iniciar(void)
{
    if (!(fpu_recursos() & CPU_SSE)) {
        klog("[RAPIDO] sem SSE: copias e preenchimentos com rep movsd/stosd\n");
        return;
    }
    /* RAM -> RAM: 256 KB (una ventana grande compuesta), en un bloque temporal. */
    const size_t n = 256 * 1024;
    uint8_t *a = kmalloc(2 * n + 64);
    if (!a) return;
    uint8_t *o = (uint8_t *)(((uint32_t)a + 15) & ~15u), *d = o + n + 16;   /* destino alineado, origen no */
    memset(o, 0x5A, n);
    uint32_t tc[QTD_METODOS], tp[3], tt[QTD_METODOS];
    static const int pm[3] = { REP, SSE, SSE2 };   /* los rellenos: sin variante SSE3 ni no temporal */
    for (int m = 0; m < QTD_METODOS; m++) tt[m] = 0xFFFFFFFFu;
    for (int m = 0; m < QTD_METODOS; m++) tc[m] = metodo_existe(m) ? medir_copia(m, d, o + 4, n - 16) : 0xFFFFFFFFu;
    for (int k = 0; k < 3; k++) tp[k] = metodo_existe(pm[k]) ? medir_preencher(pm[k], (uint32_t *)d, n / 4) : 0xFFFFFFFFu;
    met_copia = escolher(tc, QTD_METODOS);
    met_preencher = pm[escolher(tp, 3)];
    kfree(a);
    medir_tela(tt);
    char l[240];
    int k = snformatar(l, sizeof l, "copia");
    for (int m = 0; m < QTD_METODOS; m++) if (tc[m] != 0xFFFFFFFFu) k += snformatar(l + k, sizeof l - (size_t)k, " %s %u", nome_metodo[m], tc[m]);
    k += snformatar(l + k, sizeof l - (size_t)k, "; preencher");
    for (int j = 0; j < 3; j++) if (tp[j] != 0xFFFFFFFFu) k += snformatar(l + k, sizeof l - (size_t)k, " %s %u", nome_metodo[pm[j]], tp[j]);
    k += snformatar(l + k, sizeof l - (size_t)k, "; tela");
    for (int m = 0; m < QTD_METODOS; m++) if (tt[m] != 0xFFFFFFFFu) k += snformatar(l + k, sizeof l - (size_t)k, " %s %u", nome_metodo[m], tt[m]);
    klog("[RAPIDO] ciclos/1024: %s\n", l);
    char s[96];
    rapido_descrever(s, sizeof s);
    klog("[RAPIDO] escolhido: %s\n", s);
}

/* Después de cambiar de modo (D059) el pitch y el tamaño cambian: mide de nuevo
 * solo la copia a la placa. */
void rapido_medir_tela(void)
{
    if (!(fpu_recursos() & CPU_SSE)) return;
    uint32_t tt[QTD_METODOS];
    for (int m = 0; m < QTD_METODOS; m++) tt[m] = 0xFFFFFFFFu;
    medir_tela(tt);
    char s[96];
    rapido_descrever(s, sizeof s);
    klog("[RAPIDO] tela %ux%ux%u -> %s\n", video_largura(), video_altura(), video_bpp(), s);
}

int rapido_descrever(char *s, int max)
{
    return snformatar(s, (uint32_t)max, "copia %s, preencher %s, tela %s",
                      nome_metodo[met_copia], nome_metodo[met_preencher],
                      video_ativo() && video_bpp() <= 16 ? (met_conv == REP ? "c16" : "sse2-16") : nome_metodo[met_tela]);
}
