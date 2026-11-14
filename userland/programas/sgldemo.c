// SPDX-License-Identifier: GPL-2.0-or-later
/* sgldemo - cubo de colores girando con SaguiGL (mejora 054, v5; D104).
 *   sgldemo             ventana 320x240 con el cubo y el contador de fps (Esc cierra)
 *   sgldemo -t N        dibuja N cuadros del cubo sin ventana (en memoria) y escribe
 *                       la suma del último y los fps; -z 16|32 elige el búfer Z
 *   sgldemo -c escena   escribe la suma de una escena fija (triangulo, cubo, zcruzado)
 *                       con Z de 16 y de 32 bits: la usa tests/h77_sgl.py
 * Usa SSE2: se comprueba el CPUID antes de dibujar. */
#include "sagui.h"
#include "gui.h"
#include "sgl.h"
#include "cenas.h"

#define L 320
#define A 240

static int tem_sse2(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d >> 26) & 1;
}

__attribute__((noinline)) static int fixas(const char *nome)
{
    static uint32_t px[CENA_L * CENA_A];
    for (int z = 16; z <= 32; z += 16) {
        sgl_ctx_t *c = sgl_criar_contexto_mem(px, CENA_L, CENA_A, z);
        if (!c || !cena_por_nome(nome)) { imprimir("sgldemo: escena desconocida: %s\n", nome); return 1; }
        imprimir("%s z%d %08x\n", nome, z, sgl_soma(px, CENA_L * CENA_A));
        sgl_destruir_contexto(c);
    }
    return 0;
}

__attribute__((noinline)) static int medir(int n, int zbits)
{
    uint32_t *px = aloca_zerado(L * A * 4);
    sgl_ctx_t *c = px ? sgl_criar_contexto_mem(px, L, A, zbits) : 0;
    if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
    unsigned t0 = tempo_ms();
    for (int i = 0; i < n; i++) cena_cubo_tam((float)i * 3.0f, L, A);
    unsigned ms = tempo_ms() - t0;
    if (!ms) ms = 1;
    imprimir("sgldemo: cubo, %d cuadros %dx%d, Z de %d bits, suma %08x, %u ms, %u fps\n", n, L, A, zbits,
             sgl_soma(px, L * A), ms, (unsigned)n * 1000u / ms);
    sgl_destruir_contexto(c);
    libera(px);
    return 0;
}

__attribute__((noinline)) static int janela(void)
{
    jan_t *j = jan_abrir("SaguiGL: cubo", L, A);
    if (!j) { imprimir("sgldemo: no puedo abrir la ventana\n"); return 1; }
    sgl_ctx_t *c = sgl_criar_contexto(j, 16);
    if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
    int i = 0, quadros = 0, fps = 0;
    unsigned marca = tempo_ms();
    for (;;) {
        evento_t e;
        while (proximo_evento(j->id, &e, false)) {
            if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 27)) { sgl_destruir_contexto(c); jan_fechar(j); return 0; }
        }
        cena_cubo_tam((float)i * 2.0f, j->larg, j->alt);
        i++;
        quadros++;
        unsigned agora = tempo_ms();
        if (agora - marca >= 1000) { fps = quadros * 1000 / (int)(agora - marca); quadros = 0; marca = agora; }
        char s[32];
        formatar_texto(s, sizeof s, "%d fps", fps);
        g_texto(j, 6, 4, s, 0xFFFFFF);
        sgl_trocar();
        dormir(1);
    }
}

int principal(int argc, char **argv)
{
    if (!tem_sse2()) { imprimir("sgldemo: necesita una CPU con SSE2\n"); return 1; }
    int n = 0, z = 32;
    const char *cena = 0;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-t") && i + 1 < argc) n = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-z") && i + 1 < argc) z = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-c") && i + 1 < argc) cena = argv[++i];
        else { imprimir("uso: sgldemo [-t cuadros] [-z 16|32] [-c escena]\n"); return 1; }
    }
    if (cena) return fixas(cena);
    if (n > 0) return medir(n, z);
    return janela();
}
