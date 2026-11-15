// SPDX-License-Identifier: GPL-2.0-or-later
/* sgldemo - cubo de colores girando con SaguiGL (mejora 054, v5; D104).
 *   sgldemo             ventana 320x240 con el cubo y el contador de fps (Esc cierra)
 *   sgldemo -t N        dibuja N cuadros del cubo sin ventana (en memoria) y escribe
 *                       la suma del último y los fps; -z 16|32 elige el búfer Z
 *   sgldemo -c escena   escribe la suma de una escena fija (triangulo, cubo, zcruzado)
 *                       con Z de 16 y de 32 bits: la usa tests/h77_sgl.py
 *   sgldemo -g [N]      3D por la GPU (D285): dibuja las escenas fijas y una con textura,
 *                       mezcla y niebla por software y por la GPU y dice cuántos píxeles
 *                       se apartan (los bordes no coinciden bit a bit); después N cuadros
 *                       del cubo (60 si no se dice) por cada camino, con los fps
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

/* Escena con textura (8x8, repetida), mezcla y niebla: un suelo de frente y un rectángulo translúcido. */
static void cena_textura(void)
{
    static unsigned char tex[8 * 8 * 4];
    for (int i = 0; i < 64; i++) {
        int claro = ((i & 7) ^ (i >> 3)) & 1;
        tex[i * 4] = (unsigned char)(claro ? 230 : 40);
        tex[i * 4 + 1] = (unsigned char)(60 + (i >> 3) * 24);
        tex[i * 4 + 2] = (unsigned char)(claro ? 60 : 200);
        tex[i * 4 + 3] = 255;
    }
    static const GLfloat q[] = { -1.5f, -1.0f, -4.0f,  1.5f, -1.0f, -4.0f,  1.5f, 1.0f, -4.0f,  -1.5f, 1.0f, -4.0f };
    static const GLfloat uv[] = { 0, 0,  2, 0,  2, 2,  0, 2 };
    static const GLfloat fog[] = { 0.5f, 0.5f, 0.6f, 1.0f };
    GLuint id = 1;
    glViewport(0, 0, CENA_L, CENA_A);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustumf(-1.0f, 1.0f, -0.75f, 0.75f, 2.0f, 10.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_FOG);
    glFogf(GL_FOG_START, 0.0f); glFogf(GL_FOG_END, 2.0f); glFogfv(GL_FOG_COLOR, fog);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, q); glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glColor4f(1, 1, 1, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisable(GL_TEXTURE_2D); glDisable(GL_FOG);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    /* Un rectángulo rojo translúcido delante. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPushMatrix();
    glTranslatef(0.4f, 0.2f, 1.0f); glScalef(0.5f, 0.5f, 1.0f);
    glColor4f(1.0f, 0.1f, 0.1f, 0.5f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glPopMatrix();
    glDisable(GL_BLEND);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_DEPTH_TEST);
    glDeleteTextures(1, &id);
}

static void desenhar_cena(int k)
{
    if (k == 0) cena_triangulo();
    else if (k == 1) cena_cubo(30.0f);
    else if (k == 2) cena_z_cruzado();
    else cena_textura();
}

/* D285: las mismas escenas por software y por la GPU. Devuelve 0 si la GPU las
 * da parecidas, 1 si no y 2 si no hay 3D por GPU. */
__attribute__((noinline)) static int comparar_gpu(int n)
{
    static const char *const nomes[4] = { "triangulo", "cubo", "zcruzado", "textura" };
    static uint32_t sw[CENA_L * CENA_A], hw[CENA_L * CENA_A];
    int mal = 0;
    for (int k = 0; k < 4; k++) {
        sgl_ctx_t *c = sgl_criar_contexto_mem(sw, CENA_L, CENA_A, 32);
        if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
        desenhar_cena(k);
        sgl_destruir_contexto(c);
        c = sgl_criar_contexto_mem(hw, CENA_L, CENA_A, 32);
        if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
        if (!sgl_gpu_ligar(c)) {
            info_gpu3d_t g;
            memset(&g, 0, sizeof g);
            info_sistema(INFO_GPU3D, &g, sizeof g);
            imprimir("sgldemo: sin 3D por GPU, SaguiGL por software: %s\n", g.motivo);
            sgl_destruir_contexto(c);
            return 2;
        }
        desenhar_cena(k);
        glFinish();
        int caiu = !sgl_gpu_ativa();
        sgl_destruir_contexto(c);
        if (caiu) { imprimir("gpu %s: la GPU se apago a medias (ver «gpu3d»): MAL\n", nomes[k]); return 1; }
        int dif = 0, maior = 0, pintados = 0;
        for (int i = 0; i < CENA_L * CENA_A; i++) {
            int d = 0;
            for (int sft = 0; sft < 24; sft += 8) {
                int x = (int)((sw[i] >> sft) & 0xFF) - (int)((hw[i] >> sft) & 0xFF);
                if (x < 0) x = -x;
                if (x > d) d = x;
            }
            if (d > 24) dif++;
            if (d > maior) maior = d;
            if (sw[i] != sw[0]) pintados++;
        }
        /* Los bordes de los triángulos y las líneas no caen en los mismos píxeles: hasta un 4 %. */
        int bien = dif * 100 <= CENA_L * CENA_A * 4 && pintados > 0;
        imprimir("gpu %s: %d de %d pixeles se apartan mas de 24 (%d.%d%%), mayor diferencia %d: %s\n", nomes[k], dif,
                 CENA_L * CENA_A, dif * 100 / (CENA_L * CENA_A), (dif * 1000 / (CENA_L * CENA_A)) % 10, maior, bien ? "BIEN" : "MAL");
        if (!bien) mal = 1;
    }
    /* Velocidad: el cubo por cada camino, con el cuadro traído a la RAM cada vez. */
    uint32_t *px = aloca_zerado(L * A * 4);
    if (!px) return mal;
    for (int gpu = 0; gpu < 2; gpu++) {
        sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
        if (!c) break;
        if (gpu && !sgl_gpu_ligar(c)) { imprimir("sgldemo: la GPU no acepta %dx%d\n", L, A); sgl_destruir_contexto(c); break; }
        unsigned t0 = tempo_ms();
        for (int i = 0; i < n; i++) { cena_cubo_tam((float)i * 3.0f, L, A); glFinish(); }
        unsigned ms = tempo_ms() - t0;
        if (!ms) ms = 1;
        imprimir("sgldemo: cubo %dx%d, %d cuadros por %s: %u ms, %u fps\n", L, A, n,
                 gpu ? (sgl_gpu_ativa() ? "la GPU" : "la GPU (apagada a medias)") : "software", ms, (unsigned)n * 1000u / ms);
        sgl_destruir_contexto(c);
    }
    libera(px);
    imprimir("sgldemo -g: %s\n", mal ? "MAL" : "BIEN");
    return mal;
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
        formatar_texto(s, sizeof s, "%d fps (%s)", fps, sgl_gpu_ativa() ? "GPU" : "CPU");
        glFinish();   /* D285: con la GPU, el cuadro llega a la ventana antes de escribir encima */
        g_texto(j, 6, 4, s, 0xFFFFFF);
        sgl_trocar();
        dormir(1);
    }
}

int principal(int argc, char **argv)
{
    if (!tem_sse2()) { imprimir("sgldemo: necesita una CPU con SSE2\n"); return 1; }
    int n = 0, z = 32, gpu = 0;
    const char *cena = 0;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-t") && i + 1 < argc) n = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-z") && i + 1 < argc) z = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-c") && i + 1 < argc) cena = argv[++i];
        else if (!compara_texto(argv[i], "-g")) { gpu = 60; if (i + 1 < argc) gpu = texto_para_int(argv[++i]); if (gpu < 1) gpu = 1; }
        else { imprimir("uso: sgldemo [-t cuadros] [-z 16|32] [-c escena] [-g [cuadros]]\n"); return 1; }
    }
    if (gpu) return comparar_gpu(gpu);
    if (cena) return fixas(cena);
    if (n > 0) return medir(n, z);
    return janela();
}
