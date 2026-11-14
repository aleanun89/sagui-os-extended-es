// SPDX-License-Identifier: GPL-2.0-or-later
/* aleatorio.c - números impredecibles para el TLS, el TCP y el DNS (D061).
 *
 * Un depósito ("pool") SHA-256 recibe todo lo que es difícil de predecir:
 *   - RDSEED/RDRAND, si el CPUID dice que existen (Intel desde 2012, AMD
 *     desde 2015). Algunas AMD devuelven siempre 0xFFFFFFFF después de dormir:
 *     ese caso se detecta y la instrucción se descarta;
 *   - el TSC en el instante de cada interrupción (reloj, teclado, ratón, disco,
 *     red) y el EIP interrumpido: el "jitter" que sobra incluso sin RDRAND;
 *   - en el arranque, el reloj CMOS y un bucle que mide la variación del propio TSC.
 *
 * La salida no sale directo del depósito: cada pedido deriva una clave nueva
 * (SHA-256 del depósito + clave anterior) y genera bloques SHA-256(clave ||
 * contador); al final cambia la clave otra vez, para que quien lea la memoria
 * después no reconstruya lo que ya fue entregado. Es el diseño de un "hash
 * DRBG" simplificado; los límites (sin estimar la entropía, máquina virtual
 * sin RDRAND con poco jitter) están en D061.
 *
 * Todo corre con las interrupciones desactivadas (llamadas al sistema e IRQ): no
 * hay bloqueos. */
#include "aleatorio.h"
#include "abi.h"
#include "relogio.h"
#include "sha256.h"
#include "sistema.h"
#include "texto.h"
#include "console.h"

#define ANEL 64

static sha256_t reservatorio;
static uint8_t chave[SHA256_TAM];
static uint64_t contador;
static uint32_t anel[ANEL];       /* TSC de las últimas interrupciones, todavía no mezclado */
static uint32_t anel_pos;
static uint32_t fontes, eventos, pedidos;

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cpuid(uint32_t folha, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(folha), "c"(0));
}

/* Las instrucciones devuelven CF = 1 cuando el valor es bueno; lo intentamos algunas veces. */
static bool rdrand32(uint32_t *v)
{
    for (int i = 0; i < 10; i++) {
        uint8_t ok;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok) : : "cc");
        if (ok) return true;
    }
    return false;
}

static bool rdseed32(uint32_t *v)
{
    for (int i = 0; i < 100; i++) {
        uint8_t ok;
        __asm__ volatile("rdseed %0; setc %1" : "=r"(*v), "=qm"(ok) : : "cc");
        if (ok) return true;
        __asm__ volatile("pause");
    }
    return false;
}

/* 8 palabras del hardware (RDSEED de preferencia: viene directo de la fuente física). */
static void misturar_hardware(void)
{
    uint32_t v[8];
    int n = 0;
    for (int i = 0; i < 8; i++) {
        bool ok = false;
        if (fontes & ALEA_RDSEED) ok = rdseed32(&v[n]);
        if (!ok && (fontes & ALEA_RDRAND)) ok = rdrand32(&v[n]);
        if (ok) n++;
    }
    if (n) sha256_somar(&reservatorio, v, (unsigned)n * 4);
    memset(v, 0, sizeof v);
}

void aleatorio_evento(uint32_t dado)
{
    uint32_t t = (uint32_t)tsc();
    /* Gira en vez de sobrescribir: si nadie pide bytes por mucho tiempo,
     * las interrupciones viejas siguen contando en el XOR. */
    anel[anel_pos % ANEL] ^= t ^ (dado << 16 | dado >> 16);
    anel_pos++;
    eventos++;
}

void aleatorio_misturar(const void *dados, uint32_t n)
{
    sha256_somar(&reservatorio, dados, n);
}

/* Une el anillo y el TSC al depósito y deriva la clave del próximo pedido. */
static void renovar_chave(void)
{
    uint64_t t = tsc();
    sha256_somar(&reservatorio, anel, sizeof anel);
    sha256_somar(&reservatorio, &t, sizeof t);
    sha256_somar(&reservatorio, &anel_pos, sizeof anel_pos);
    misturar_hardware();
    sha256_t c = reservatorio;   /* el depósito sigue acumulando; la copia se convierte en la clave */
    sha256_somar(&c, chave, sizeof chave);
    sha256_somar(&c, &contador, sizeof contador);
    sha256_fim(&c, chave);
    contador++;
}

void aleatorio_bytes(void *destino, uint32_t n)
{
    uint8_t *d = destino;
    renovar_chave();
    while (n) {
        uint8_t bloco[SHA256_TAM];
        sha256_t c;
        sha256_iniciar(&c);
        sha256_somar(&c, chave, sizeof chave);
        sha256_somar(&c, &contador, sizeof contador);
        sha256_fim(&c, bloco);
        contador++;
        uint32_t k = n < SHA256_TAM ? n : SHA256_TAM;
        memcpy(d, bloco, k);
        d += k;
        n -= k;
    }
    /* Clave nueva después de entregar: lo que salió no puede recalcularse. */
    static const uint8_t marca = 0xA5;
    sha256_t c;
    sha256_iniciar(&c);
    sha256_somar(&c, chave, sizeof chave);
    sha256_somar(&c, &marca, 1);
    sha256_somar(&c, &contador, sizeof contador);
    sha256_fim(&c, chave);
    contador++;
    pedidos++;
}

uint32_t aleatorio_u32(void)
{
    uint32_t v;
    aleatorio_bytes(&v, sizeof v);
    return v;
}

void aleatorio_iniciar(void)
{
    uint32_t a, b, c, d;
    sha256_iniciar(&reservatorio);
    fontes = ALEA_TSC;
    cpuid(0, &a, &b, &c, &d);
    uint32_t max = a;
    if (max >= 1) {
        cpuid(1, &a, &b, &c, &d);
        if (c & (1u << 30)) fontes |= ALEA_RDRAND;
    }
    if (max >= 7) {
        cpuid(7, &a, &b, &c, &d);
        if (b & (1u << 18)) fontes |= ALEA_RDSEED;
    }
    /* RDRAND roto (siempre el mismo valor, típico 0xFFFFFFFF): no se fía. */
    if (fontes & ALEA_RDRAND) {
        uint32_t x = 0, y = 0;
        if (!rdrand32(&x) || !rdrand32(&y) || x == y) fontes &= ~(uint32_t)(ALEA_RDRAND | ALEA_RDSEED);
    }
    data_hora_t h;
    cmos_ler_data(&h);
    sha256_somar(&reservatorio, &h, sizeof h);
    /* Jitter: cuánto anda el TSC en 256 vueltas de un trabajo fijo varía con
     * cachés, bus y (en una VM) con el anfitrión. Poca entropía por
     * medida, pero muchas medidas. */
    uint32_t medidas[256];
    volatile uint32_t lixo = 0;
    for (int i = 0; i < 256; i++) {
        uint64_t t0 = tsc();
        for (int k = 0; k < 64 + (i & 15); k++) lixo = lixo * 1664525u + 1013904223u + (uint32_t)k;
        medidas[i] = (uint32_t)(tsc() - t0) ^ lixo;
    }
    sha256_somar(&reservatorio, medidas, sizeof medidas);
    misturar_hardware();
    renovar_chave();
    klog("[SAGUI] aleatorio: rdrand=%u rdseed=%u\n", (fontes & ALEA_RDRAND) ? 1u : 0u, (fontes & ALEA_RDSEED) ? 1u : 0u);
}

/* ------------------------------------------------------------ llamadas */

int32_t sis_aleatorio(uint32_t buf, uint32_t n, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (n > 4096) n = 4096;
    if (!usuario_ok((void *)buf, n, true)) return ERRO_ENDERECO;
    aleatorio_bytes((void *)buf, n);
    return (int32_t)n;
}

int32_t sis_info_aleatorio(uint32_t ptr, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    (void)b; (void)c; (void)d; (void)e;
    info_aleatorio_t *i = (info_aleatorio_t *)ptr;
    if (!usuario_ok(i, sizeof *i, true)) return ERRO_ENDERECO;
    i->fontes = fontes;
    i->eventos = eventos;
    i->pedidos = pedidos;
    i->reservado = 0;
    return 0;
}
