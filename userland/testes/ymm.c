// SPDX-License-Identifier: GPL-2.0-or-later
/* ymm - prueba del estado AVX guardado por el kernel (D381, h399).
 *
 *   ymm [procesos] [segundos]   (de serie 3 y 3)
 *
 * Sin AVX utilizable (cpu_tem_avx: la CPU no lo tiene o el kernel no lo encendió) dice
 * «ymm: sin AVX» y sale con 0, sin ejecutar ninguna instrucción VEX.
 *
 * Con AVX:
 *   1. estado limpio: un proceso nuevo empieza con las mitades altas de los YMM a cero
 *      (si no, vería lo que dejó otro proceso);
 *   2. lanza "procesos" copias de sí mismo (--hijo k) y, además, dos hilos en este proceso.
 *      Cada uno carga ymm0-ymm7 con un patrón propio (las dos mitades distintas), da vueltas
 *      sin tocarlos durante unos milisegundos (el tic del planificador lo saca de la CPU y
 *      otro carga los suyos) y comprueba que siguen iguales, durante "segundos". Con un
 *      kernel que guardara solo los XMM (FXSAVE) las mitades altas de uno acabarían en otro.
 * Escribe «ymm: ok» o «ymm: FALLO ...» y el número de vueltas y de veces que vio a otro pasar
 * por medio (comprobado con el reloj: una vuelta que duró más de lo normal). */
#include "sagui.h"

#define VOLTAS_SPIN 2000000u   /* iteraciones del bucle dentro del asm (~1-10 ms) */

/* Carga ymm0-7 de "pat" (8 × 32 bytes), da "n" vueltas sin tocarlos y los guarda en "out". */
static void carregar_girar_guardar(const uint32_t *pat, uint32_t *out, uint32_t n)
{
    __asm__ volatile(
        "vmovdqu   0(%0), %%ymm0\n\t"
        "vmovdqu  32(%0), %%ymm1\n\t"
        "vmovdqu  64(%0), %%ymm2\n\t"
        "vmovdqu  96(%0), %%ymm3\n\t"
        "vmovdqu 128(%0), %%ymm4\n\t"
        "vmovdqu 160(%0), %%ymm5\n\t"
        "vmovdqu 192(%0), %%ymm6\n\t"
        "vmovdqu 224(%0), %%ymm7\n\t"
        "1: dec %2\n\t"
        "jnz 1b\n\t"
        "vmovdqu %%ymm0,   0(%1)\n\t"
        "vmovdqu %%ymm1,  32(%1)\n\t"
        "vmovdqu %%ymm2,  64(%1)\n\t"
        "vmovdqu %%ymm3,  96(%1)\n\t"
        "vmovdqu %%ymm4, 128(%1)\n\t"
        "vmovdqu %%ymm5, 160(%1)\n\t"
        "vmovdqu %%ymm6, 192(%1)\n\t"
        "vmovdqu %%ymm7, 224(%1)\n\t"
        "vzeroupper"
        : "+r"(pat), "+r"(out), "+r"(n)
        :
        : "memory");
}

/* Las mitades altas de ymm0-7 tal como están ahora (sin cargar nada antes). */
static void ler_altas(uint32_t *out)
{
    __asm__ volatile(
        "vextracti128 $1, %%ymm0, %%xmm0\n\t" "vmovdqu %%xmm0,   0(%0)\n\t"
        "vextracti128 $1, %%ymm1, %%xmm1\n\t" "vmovdqu %%xmm1,  16(%0)\n\t"
        "vextracti128 $1, %%ymm2, %%xmm2\n\t" "vmovdqu %%xmm2,  32(%0)\n\t"
        "vextracti128 $1, %%ymm3, %%xmm3\n\t" "vmovdqu %%xmm3,  48(%0)\n\t"
        "vextracti128 $1, %%ymm4, %%xmm4\n\t" "vmovdqu %%xmm4,  64(%0)\n\t"
        "vextracti128 $1, %%ymm5, %%xmm5\n\t" "vmovdqu %%xmm5,  80(%0)\n\t"
        "vextracti128 $1, %%ymm6, %%xmm6\n\t" "vmovdqu %%xmm6,  96(%0)\n\t"
        "vextracti128 $1, %%ymm7, %%xmm7\n\t" "vmovdqu %%xmm7, 112(%0)\n\t"
        "vzeroupper"
        : "+r"(out)
        :
        : "memory");
}

typedef struct {
    uint32_t semente;
    unsigned ms;
    uint32_t voltas, cortes, erros;
    uint32_t primeiro_mal[2];   /* (registro*8 + palabra, valor) del primer fallo */
} trabalho_t;

/* vextracti128 es AVX2: sin AVX2 (solo AVX, Sandy Bridge, Bulldozer) no se mira el estado limpio. */
static bool tem_avx2;

static void trabalhar(trabalho_t *t)
{
    uint32_t pat[64] __attribute__((aligned(32))), out[64] __attribute__((aligned(32)));
    for (int i = 0; i < 64; i++)   /* palabras distintas en cada registro y mitad; 0 nunca */
        pat[i] = (t->semente * 0x9E3779B1u) ^ ((uint32_t)i * 0x01010101u) ^ 0x80000000u;
    unsigned ini = tempo_ms(), normal = 0xFFFFFFFFu;
    while (tempo_ms() - ini < t->ms) {
        unsigned a = tempo_ms();
        carregar_girar_guardar(pat, out, VOLTAS_SPIN);
        unsigned d = tempo_ms() - a;
        if (d < normal) normal = d;
        if (d > normal + 15) t->cortes++;   /* más de un tic: alguien pasó por medio */
        t->voltas++;
        for (int i = 0; i < 64; i++)
            if (out[i] != pat[i]) {
                if (!t->erros) { t->primeiro_mal[0] = (uint32_t)i; t->primeiro_mal[1] = out[i]; }
                t->erros++;
            }
        /* otro patrón en la vuelta siguiente: así un estado viejo propio tampoco pasa */
        for (int i = 0; i < 64; i++) pat[i] = pat[i] * 1664525u + 1013904223u;
    }
}

static int informar(const char *quem, const trabalho_t *t)
{
    if (t->erros) {
        imprimir("ymm: FALLO %s: %u palabras distintas en %u vueltas (la primera: ymm%u palabra %u = %x)\n", quem,
                 t->erros, t->voltas, t->primeiro_mal[0] / 8, t->primeiro_mal[0] % 8, t->primeiro_mal[1]);
        return 1;
    }
    imprimir("ymm: %s bien: %u vueltas, %u con otro por medio\n", quem, t->voltas, t->cortes);
    return 0;
}

static trabalho_t hilos_t[2];
static void *hilo(void *arg) { trabalhar((trabalho_t *)arg); return NULL; }

int principal(int argc, char **argv)
{
    if (!cpu_tem_avx()) {
        imprimir("ymm: sin AVX (la CPU no lo tiene o el kernel no lo encendió)\n");
        return 0;
    }
    tem_avx2 = cpu_tem_avx2();
    if (argc >= 4 && compara_texto(argv[1], "--hijo") == 0) {
        trabalho_t t = { .semente = (uint32_t)texto_para_int(argv[2]) * 7919u + (uint32_t)meu_pid(),
                         .ms = (unsigned)texto_para_int(argv[3]) * 1000u };
        char quem[24];
        formatar_texto(quem, sizeof quem, "hijo %s", argv[2]);
        trabalhar(&t);
        return informar(quem, &t);
    }
    int procs = argc >= 2 ? texto_para_int(argv[1]) : 3, seg = argc >= 3 ? texto_para_int(argv[2]) : 3;
    if (procs < 0 || procs > 8) procs = 3;
    if (seg < 1 || seg > 60) seg = 3;
    int falhas = 0;

    /* 1. estado limpio: este proceso aún no ha usado AVX (la libsagui solo usa SSE clásico,
     * que no toca las mitades altas). */
    if (tem_avx2) {
        uint32_t altas[32] __attribute__((aligned(32)));
        ler_altas(altas);
        uint32_t o = 0;
        for (int i = 0; i < 32; i++) o |= altas[i];
        imprimir("ymm: estado limpio %s\n", o ? "MAL (mitades altas con datos de otro)" : "bien (mitades altas a cero)");
        if (o) falhas++;
    }

    /* 2. procesos hijos y dos hilos de este, todos a la vez */
    int pids[8];
    char nums[8][8], segs[8];
    formatar_texto(segs, sizeof segs, "%d", seg);
    for (int k = 0; k < procs; k++) {
        formatar_texto(nums[k], sizeof nums[k], "%d", k + 1);
        char *args[] = { "ymm", "--hijo", nums[k], segs, NULL };
        pids[k] = executar("/testes/ymm", args);
        if (pids[k] <= 0) { imprimir("ymm: FALLO no se pudo lanzar el hijo %d\n", k + 1); falhas++; }
    }
    thread_t *h[2];
    for (int i = 0; i < 2; i++) {
        hilos_t[i].semente = 0xA5A5u + (uint32_t)i * 31u + (uint32_t)meu_pid() * 131u;
        hilos_t[i].ms = (unsigned)seg * 1000u;
        h[i] = thread_create(hilo, &hilos_t[i], 0);
        if (!h[i]) { imprimir("ymm: FALLO no se pudo crear el hilo %d\n", i + 1); falhas++; }
    }
    for (int i = 0; i < 2; i++)
        if (h[i]) {
            thread_join(h[i]);
            char quem[16];
            formatar_texto(quem, sizeof quem, "hilo %d", i + 1);
            falhas += informar(quem, &hilos_t[i]);
        }
    for (int k = 0; k < procs; k++) {
        int c = -1;
        if (pids[k] > 0 && esperar(pids[k], &c) >= 0 && c != 0) falhas++;
    }
    imprimir(falhas ? "ymm: FALLO (%d)\n" : "ymm: ok (%d fallos)\n", falhas);
    return falhas ? 1 : 0;
}
