// SPDX-License-Identifier: GPL-2.0-or-later
/* bench.c - mide la velocidad de la memoria y de la interfaz gráfica (D042).
 *
 * Uso: bench          todo
 *      bench mem      memcpy, memset y rellenar píxeles, en MB/s
 *      bench gui      cuadros por segundo de una ventana 640x480
 *
 * Cada medida repite la operación hasta pasar al menos MEDIDA_MS y divide los
 * bytes por el tiempo del reloj del sistema (10 ms de resolución). Los números del
 * QEMU sin KVM no representan la PC de verdad: sirven para comparar las
 * versiones entre sí. En la G470, ejecute "bench" y el "sagui3d --medir". */
#include "sagui.h"
#include "gui.h"

#define MEDIDA_MS 300

static uint8_t *buf_a, *buf_b;

typedef void (*operacao_t)(uint8_t *d, const uint8_t *o, size_t n);

static void op_memcpy(uint8_t *d, const uint8_t *o, size_t n) { memcpy(d, o, n); }
static void op_memset(uint8_t *d, const uint8_t *o, size_t n) { (void)o; memset(d, 0x5A, n); }
static void op_memmove(uint8_t *d, const uint8_t *o, size_t n) { (void)o; memmove(d + 64, d, n - 64); }
static void op_pixels(uint8_t *d, const uint8_t *o, size_t n) { (void)o; preencher32((uint32_t *)d, 0x00336699, n / 4); }
static void op_trocar(uint8_t *d, const uint8_t *o, size_t n) { trocar_rb32((uint32_t *)d, (const uint32_t *)o, n / 4); }
static void op_misturar(uint8_t *d, const uint8_t *o, size_t n) { misturar32((uint32_t *)d, (const uint32_t *)o, n / 4, 100); }
static void op_ampliar(uint8_t *d, const uint8_t *o, size_t n) { ampliar2x32((uint32_t *)d, NULL, (const uint32_t *)o, n / 8); }

/* MB/s * 10 (una cifra decimal) */
static unsigned medir(operacao_t op, size_t n, int desalinho)
{
    unsigned voltas = 0, ini = tempo_ms(), agora;
    uint64_t bytes = 0;
    do {
        int vezes = n >= 65536 ? 1 : 16;
        for (int k = 0; k < vezes; k++) op(buf_b + desalinho, buf_a + desalinho, n);
        voltas += (unsigned)vezes;
        bytes += (uint64_t)n * (unsigned)vezes;
        agora = tempo_ms();
    } while (agora - ini < MEDIDA_MS);
    unsigned ms = agora - ini;
    /* bytes / ms = KB/s aproximado (1000 bytes); en MB/s con una decimal: */
    return (unsigned)((uint32_t)(bytes >> 10) * 10u / ms * 1000u / 1024u);
}

static const char *const nome_metodo[] = { "auto", "C", "rep", "sse2", "sse", "sse3", "ssse3" };


static void linha_mem(const char *nome, operacao_t op, int metodo)
{
    static const size_t tamanhos[] = { 64, 256, 1024, 4096, 65536, 1048576 };
    int antes = mem_metodo(metodo);
    char t[24];
    formatar_texto(t, sizeof t, "%s %s", nome, nome_metodo[metodo]);
    imprimir("%-14s", t);
    for (unsigned i = 0; i < sizeof tamanhos / sizeof tamanhos[0]; i++) {
        unsigned v = medir(op, tamanhos[i], 0);
        imprimir(" %6u", v / 10);
    }
    imprimir("\n");
    mem_metodo(antes);
}

static void bench_mem(void)
{
    static const struct { const char *nome; operacao_t op; } ops[] = {
        { "memcpy", op_memcpy }, { "memset", op_memset }, { "memmove", op_memmove },
        { "preencher32", op_pixels }, { "ampliar2x", op_ampliar },
        { "trocar_rb32", op_trocar }, { "misturar32", op_misturar },
    };
    imprimir("Memoria (MB/s)    64 B  256 B   1 KB   4 KB  64 KB   1 MB  [SSE2: %s]\n",
             cpu_tem_sse2() ? "si" : "no");
    /* D208: una fila por variante que la CPU tenga (SSE, SSE2, SSE3, SSSE3). */
    static const int ordem[] = { MEM_AUTO, MEM_SIMPLES, MEM_REP, MEM_SSE, MEM_SSE2, MEM_SSE3, MEM_SSSE3 };
    for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++)
        for (unsigned j = 0; j < sizeof ordem / sizeof ordem[0]; j++) {
            int m = ordem[j];
            bool tem = m == MEM_SSE ? cpu_tem_sse() : m == MEM_SSE2 ? cpu_tem_sse2() :
                       m == MEM_SSE3 ? cpu_tem_sse3() : m == MEM_SSSE3 ? cpu_tem_ssse3() : true;
            if (tem) linha_mem(ops[k].nome, ops[k].op, m);
        }
    char t[160];
    mem_escolhas(t, sizeof t);
    imprimir("Modo auto: %s\n", t);
    registrar("bench: auto %s", t);
}

static void bench_gui(void)
{
    jan_t *j = jan_abrir("bench", 640, 480);
    if (!j) { imprimir("bench: no he podido abrir la ventana\n"); return; }
    /* Redibujar todo: fondo, 24 rectángulos, 20 líneas de texto y mostrar. */
    unsigned q = 0, ini = tempo_ms(), agora;
    do {
        g_retangulo(j, 0, 0, 640, 480, (q & 1) ? COR_FUNDO : 0xE0E8F0);
        for (int k = 0; k < 24; k++) g_retangulo(j, 10 + (k % 6) * 100, 40 + (k / 6) * 100, 90, 90, 0x103050 * (uint32_t)(k + 1));
        for (int k = 0; k < 20; k++) g_texto(j, 8, 4 + k * 22, "SaguiOS bench: g_retangulo, g_texto y jan_mostrar", COR_TEXTO);
        jan_mostrar(j);
        q++;
        agora = tempo_ms();
    } while (agora - ini < 2 * MEDIDA_MS || q < 4);
    unsigned redesenhar = q * 10000 / (agora - ini);
    /* Solo mostrar: el coste del kernel (componer y copiar a la tarjeta). */
    q = 0; ini = tempo_ms();
    do {
        jan_mostrar(j);
        q++;
        agora = tempo_ms();
    } while (agora - ini < 2 * MEDIDA_MS || q < 4);
    unsigned mostrar = q * 10000 / (agora - ini);
    jan_fechar(j);
    imprimir("GUI 640x480: redibujar y mostrar %u.%u cuadros/s, solo mostrar %u.%u cuadros/s\n",
             redesenhar / 10, redesenhar % 10, mostrar / 10, mostrar % 10);
    registrar("bench: gui redibujar %u.%u mostrar %u.%u", redesenhar / 10, redesenhar % 10, mostrar / 10, mostrar % 10);
}

int principal(int argc, char **argv)
{
    bool mem = argc < 2 || compara_texto(argv[1], "mem") == 0;
    bool gui = argc < 2 || compara_texto(argv[1], "gui") == 0;
    if (!mem && !gui) {
        imprimir_em(ERROS, "uso: bench [mem|gui]\n");
        return 1;
    }
    if (mem) {
        buf_a = aloca(1048576 + 4096);
        buf_b = aloca(1048576 + 4096);
        if (!buf_a || !buf_b) { imprimir_em(ERROS, "bench: sin memoria\n"); return 1; }
        buf_a = (uint8_t *)(((uint32_t)buf_a + 63) & ~63u);
        buf_b = (uint8_t *)(((uint32_t)buf_b + 63) & ~63u);
        memset(buf_a, 7, 1048576);
        bench_mem();
    }
    if (gui) bench_gui();
    return 0;
}
