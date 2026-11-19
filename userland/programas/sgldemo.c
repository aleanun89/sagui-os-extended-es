// SPDX-License-Identifier: GPL-2.0-or-later
/* sgldemo - cubo de colores girando con SaguiGL (mejora 054, v5; D104).
 *   sgldemo             ventana 320x240 con el cubo y el contador de fps (Esc cierra)
 *   sgldemo -n N        la ventana, N cuadros y se cierra sola: escribe ms, fps reales, el camino
 *                       (GPU o CPU) y la suma del último cuadro (sin el texto de los fps)
 *   sgldemo -s          la ventana sin ninguna pausa entre cuadros (para medir lo más que da)
 *   sgldemo -t N        dibuja N cuadros del cubo sin ventana (en memoria) y escribe
 *                       la suma del último y los fps; -z 16|32 elige el búfer Z
 *   sgldemo -c escena   escribe la suma de una escena fija (triangulo, cubo, zcruzado)
 *                       con Z de 16 y de 32 bits: la usa tests/h77_sgl.py
 *   sgldemo -g [N]      3D por la GPU (D285): dibuja las escenas fijas y una con textura,
 *                       mezcla y niebla por software y por la GPU y dice cuántos píxeles
 *                       se apartan (los bordes no coinciden bit a bit); después N cuadros
 *                       del cubo (60 si no se dice) por cada camino, con los fps
 *                       (D404: también «bilineal», «luces» y «paisaje» sin mipmaps)
 *   sgldemo -c paisaje|avion   (D404) sumas de la escena realista (cuadros 0 y 40, 160x120)
 *   sgldemo -p [N] [opciones]  (D404) el paisaje en una ventana de 640x480 durante N cuadros
 *                       (0 o nada = hasta Esc) y al final fps, triángulos/s, texturas subidas
 *                       a la GPU y su memoria. Opciones:
 *       --mem           sin ventana (en memoria; por software salvo --gpu)
 *       --gpu           con --mem, por la GPU
 *       --sw            con ventana, por software aunque haya GPU
 *       -r LxA          tamaño (por defecto 640x480; 320x240 con --mem)
 *       --texturas MB   además, MB de texturas de 256x256 distintas en cada cuadro
 *       --sin-mip       solo bilineal (sin mipmaps)
 *       --detalle N     celdas del terreno por lado (16..128, 64 por defecto)
 *       --avion         la cámara va detrás del avión
 *       --modelo F.obj  el avión es ese OBJ (+ MTL y texturas); --giro G lo orienta
 * Usa SSE2: se comprueba el CPUID antes de dibujar. */
#include "sagui.h"
#include "gui.h"
#include "sgl.h"
#include "cenas.h"
#include "paisaje.h"

#define L 320
#define A 240

static int tem_sse2(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d >> 26) & 1;
}

/* D404: las sumas del paisaje, como tests/sgl/h405_anfitriao.c («somas»). */
__attribute__((noinline)) static int fixas_paisaje(int camara)
{
    static uint32_t p[CENA_L * CENA_A];
    for (int z = 16; z <= 32; z += 16) {
        sgl_ctx_t *c = sgl_criar_contexto_mem(p, CENA_L, CENA_A, z);
        if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
        pais_opcoes_t o;
        memset(&o, 0, sizeof o);
        o.mip = 1; o.detalle = 48; o.camara = camara;
        paisaje_iniciar(&o);
        for (int q = 0; q <= 40; q += 40) {
            paisaje_cuadro(q, CENA_L, CENA_A);
            imprimir("%s z%d q%d %08x\n", camara ? "avion" : "paisaje", z, q, sgl_soma(p, CENA_L * CENA_A));
        }
        paisaje_fim();
        sgl_destruir_contexto(c);
    }
    return 0;
}

__attribute__((noinline)) static int fixas(const char *nome)
{
    static uint32_t px[CENA_L * CENA_A];
    if (!compara_texto(nome, "paisaje")) return fixas_paisaje(0);
    if (!compara_texto(nome, "avion")) return fixas_paisaje(1);
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

/* D404: una textura ampliada con bilineal y perspectiva (un suelo que se aleja). */
static void cena_bilineal(void)
{
    static unsigned char tex[4 * 4 * 3];
    for (int i = 0; i < 16; i++) {
        tex[i * 3] = (unsigned char)(((i & 3) * 80) & 255); tex[i * 3 + 1] = (unsigned char)((i >> 2) * 80);
        tex[i * 3 + 2] = (unsigned char)(((i ^ (i >> 2)) & 1) ? 220 : 30);
    }
    static const GLfloat q[] = { -2.0f, -1.0f, -2.5f,  2.0f, -1.0f, -2.5f,  2.0f, -1.0f, -9.0f,  -2.0f, -1.0f, -9.0f };
    static const GLfloat uv[] = { 0, 0,  1, 0,  1, 1,  0, 1 };
    GLuint id = 1;
    glViewport(0, 0, CENA_L, CENA_A);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustumf(-1.0f, 1.0f, -0.75f, 0.75f, 1.0f, 20.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, q); glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glColor4f(1, 1, 1, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
    glDeleteTextures(1, &id);
}

/* D404: luces por vértice (dos luces, especular, materiales) sobre una esfera. */
static void cena_luces(void)
{
    static GLfloat v[16 * 9 * 6 * 3], n[16 * 9 * 6 * 3];
    int k = 0;
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 9; j++) {
            float a0 = 6.2831853f * i / 16, a1 = 6.2831853f * (i + 1) / 16, b0 = -1.5707963f + 3.1415927f * j / 9,
                  b1 = -1.5707963f + 3.1415927f * (j + 1) / 9;
            float p[4][3] = { { sgl_cos(b0) * sgl_cos(a0), sgl_sen(b0), sgl_cos(b0) * sgl_sen(a0) },
                              { sgl_cos(b0) * sgl_cos(a1), sgl_sen(b0), sgl_cos(b0) * sgl_sen(a1) },
                              { sgl_cos(b1) * sgl_cos(a1), sgl_sen(b1), sgl_cos(b1) * sgl_sen(a1) },
                              { sgl_cos(b1) * sgl_cos(a0), sgl_sen(b1), sgl_cos(b1) * sgl_sen(a0) } };
            static const int o[6] = { 0, 2, 1, 0, 3, 2 };
            for (int t = 0; t < 6; t++, k++)
                for (int c = 0; c < 3; c++) { v[k * 3 + c] = p[o[t]][c]; n[k * 3 + c] = p[o[t]][c]; }
        }
    static const float l0[4] = { 0.5f, 0.6f, 1.0f, 0 }, l1[4] = { -1.0f, -0.2f, 0.3f, 0 }, c1[4] = { 0.2f, 0.3f, 0.9f, 1 };
    static const float dif[4] = { 0.8f, 0.6f, 0.3f, 1 }, esp[4] = { 1, 1, 1, 1 };
    glViewport(0, 0, CENA_L, CENA_A);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustumf(-0.8f, 0.8f, -0.6f, 0.6f, 1.0f, 20.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glTranslatef(0, 0, -3.2f);
    glLightfv(GL_LIGHT0, GL_POSITION, l0);
    glLightfv(GL_LIGHT1, GL_POSITION, l1); glLightfv(GL_LIGHT1, GL_DIFFUSE, c1);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, esp);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 24);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v); glNormalPointer(GL_FLOAT, 0, n);
    glDrawArrays(GL_TRIANGLES, 0, k);
    glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
}

static void cena_paisaje_g(void)
{
    pais_opcoes_t o;
    memset(&o, 0, sizeof o);
    o.mip = 0; o.detalle = 32;   /* la GPU todavía no tiene mipmaps */
    paisaje_iniciar(&o);
    paisaje_cuadro(20, CENA_L, CENA_A);
    paisaje_fim();
}

static void desenhar_cena(int k)
{
    if (k == 0) cena_triangulo();
    else if (k == 1) cena_cubo(30.0f);
    else if (k == 2) cena_z_cruzado();
    else if (k == 3) cena_textura();
    else if (k == 4) cena_bilineal();
    else if (k == 5) cena_luces();
    else cena_paisaje_g();
}

/* D285: las mismas escenas por software y por la GPU. Devuelve 0 si la GPU las
 * da parecidas, 1 si no y 2 si no hay 3D por GPU. */
__attribute__((noinline)) static int comparar_gpu(int n)
{
    static const char *const nomes[7] = { "triangulo", "cubo", "zcruzado", "textura", "bilineal", "luces", "paisaje" };
    static uint32_t sw[CENA_L * CENA_A], hw[CENA_L * CENA_A];
    int mal = 0;
    for (int k = 0; k < 7; k++) {
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
        imprimir("gpu %s: %d de %d pixeles se apartan mas de 24 (%d.%d%%), mayor diferencia %d, suma gpu %08x: %s\n", nomes[k], dif,
                 CENA_L * CENA_A, dif * 100 / (CENA_L * CENA_A), (dif * 1000 / (CENA_L * CENA_A)) % 10, maior,
                 sgl_soma(hw, CENA_L * CENA_A), bien ? "BIEN" : "MAL");
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
    /* D391: cuántos cuadros volvieron a la RAM por la GPU (sin que la CPU leyera la VRAM). */
    info_acel2d_t ac;
    memset(&ac, 0, sizeof ac);
    if (info_sistema(INFO_ACEL2D, &ac, sizeof ac) == 0)
        imprimir("sgldemo: vuelta del cuadro por la GPU: %s; %u cuadros por la GPU, %u por la CPU\n",
                 ac.retorno_estado == 1 ? "activa" : "no", ac.retorno_ops, ac.retorno_cpu_ops);
    imprimir("sgldemo -g: %s\n", mal ? "MAL" : "BIEN");
    return mal;
}

/* D404: el paisaje para medir. */
typedef struct {
    int quadros, mem, gpu, sw, larg, alt, mb, mip, detalle, camara;
    const char *modelo;
    float giro;
} opc_p_t;

static void dec1(char *s, int n, unsigned mil)   /* mil = valor * 10 */
{
    formatar_texto(s, n, "%u.%u", mil / 10, mil % 10);
}

__attribute__((noinline)) static int paisaje_medir(const opc_p_t *op)
{
    jan_t *j = 0;
    uint32_t *px = 0;
    sgl_ctx_t *c;
    if (op->mem) {
        px = aloca_zerado((size_t)op->larg * op->alt * 4);
        c = px ? sgl_criar_contexto_mem(px, op->larg, op->alt, 32) : 0;
        if (c && op->gpu && !sgl_gpu_ligar(c)) imprimir("sgldemo: sin 3D por GPU: por software\n");
    } else {
        j = jan_abrir("SaguiGL: paisaje", op->larg, op->alt);
        if (!j) { imprimir("sgldemo: no puedo abrir la ventana\n"); return 1; }
        if (op->sw) sgl_gpu_automatica(0);
        c = sgl_criar_contexto(j, 32);
    }
    if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
    static sgl_obj_t modelo;
    pais_opcoes_t o;
    memset(&o, 0, sizeof o);
    o.mip = op->mip; o.texturas_mb = op->mb; o.detalle = op->detalle; o.camara = op->camara; o.giro_aviao = op->giro;
    if (op->modelo) {
        int r = sgl_obj_cargar(op->modelo, &modelo);
        if (r < 0 && !modelo.n_vert) imprimir("sgldemo: no se pudo leer %s (%d): el avion hecho por codigo\n", op->modelo, r);
        else {
            imprimir("sgldemo: modelo %s: %d triangulos, %d materiales\n", op->modelo, modelo.n_vert / 3, modelo.n_mats);
            o.aviao = &modelo;
        }
    }
    unsigned t_ini = tempo_ms();
    paisaje_iniciar(&o);
    sgl_estat_t e;
    sgl_estatisticas(&e, 1);
    imprimir("sgldemo: paisaje preparado en %u ms: %u KB de texturas%s, %d de carga\n", tempo_ms() - t_ini, e.tex_kb,
             op->mip ? " con mipmaps" : "", paisaje_texturas_carga());
    unsigned t0 = tempo_ms(), marca = t0, fps10 = 0;
    int i = 0, nq = 0, gpu_ini = sgl_gpu_ativa();
    uint32_t tri = 0, sub = 0, kb_sub = 0, reu = 0;
    for (;;) {
        if (j) {
            evento_t ev;
            int sair = 0;
            while (proximo_evento(j->id, &ev, false))
                if (ev.tipo == EV_FECHAR || (ev.tipo == EV_TECLA && ev.tecla == 27)) sair = 1;
            if (sair) break;
        }
        paisaje_cuadro(i, op->larg, op->alt);
        glFinish();
        sgl_estatisticas(&e, 1);
        tri += e.triangulos; sub += e.tex_subidas; kb_sub += e.tex_kb_subidos; reu += e.tex_reusos;
        i++; nq++;
        unsigned agora = tempo_ms();
        if (agora - marca >= 1000) { fps10 = (unsigned)nq * 10000u / (agora - marca); nq = 0; marca = agora; }
        if (j) {
            char s[64], f[16];
            dec1(f, sizeof f, fps10);
            formatar_texto(s, sizeof s, "%s fps (%s) %u tri", f, sgl_gpu_ativa() ? "GPU" : "CPU", e.triangulos);
            g_texto(j, 6, 4, s, 0xFFFFFF);
            sgl_trocar();
            hilo_ceder();
        }
        if (op->quadros && i >= op->quadros) break;
    }
    unsigned ms = tempo_ms() - t0;
    if (!ms) ms = 1;
    char f[16], ts[16];
    dec1(f, sizeof f, (unsigned)((uint64_t)i * 10000u / ms));
    uint32_t tri_s = (uint32_t)((uint64_t)tri * 1000u / ms);
    formatar_texto(ts, sizeof ts, "%u", tri_s);
    imprimir("sgldemo paisaje: %dx%d, %d cuadros por %s: %u ms, %s fps, %u triangulos por cuadro, %s triangulos/s\n",
             op->larg, op->alt, i, sgl_gpu_ativa() ? "la GPU" : gpu_ini ? "la GPU (apagada a medias)" : "software", ms, f,
             i ? tri / (uint32_t)i : 0, ts);
    sgl_estatisticas(&e, 0);
    imprimir("sgldemo paisaje: texturas %u KB; GPU: %u huecos, %u subidas (%u KB, %u por cuadro), %u reutilizadas\n",
             e.tex_kb, e.gpu_vagas, sub, kb_sub, i ? sub / (uint32_t)i : 0, reu);
    info_gpu3d_t g;
    memset(&g, 0, sizeof g);
    if (info_sistema(INFO_GPU3D, &g, sizeof g) == 0 && g.memoria_kb)
        imprimir("sgldemo paisaje: memoria de la GPU: %u KB usados de %u KB\n", g.memoria_kb - g.livre_kb, g.memoria_kb);
    else
        imprimir("sgldemo paisaje: memoria de la GPU: sin 3D por GPU\n");
    paisaje_fim();
    if (modelo.n_vert) sgl_obj_liberar(&modelo);
    sgl_destruir_contexto(c);
    if (j) jan_fechar(j);
    if (px) libera(px);
    return 0;
}

/* Cuadros de la ventana: max_quadros 0 = hasta Esc. sem_pausa: nunca duerme. */
__attribute__((noinline)) static int janela(int max_quadros, int sem_pausa)
{
    jan_t *j = jan_abrir("SaguiGL: cubo", L, A);
    if (!j) { imprimir("sgldemo: no puedo abrir la ventana\n"); return 1; }
    sgl_ctx_t *c = sgl_criar_contexto(j, 16);
    if (!c) { imprimir("sgldemo: sin memoria\n"); return 1; }
    int i = 0, quadros = 0, fps = 0, no_tic = 0, gpu_ini = sgl_gpu_ativa();
    unsigned marca = tempo_ms(), inicio = marca, tic = marca;
    uint32_t soma = 0;
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
        if (max_quadros && i == max_quadros) soma = sgl_soma(j->px, j->larg * j->alt);
        g_texto(j, 6, 4, s, 0xFFFFFF);
        sgl_trocar();
        if (max_quadros && i >= max_quadros) {
            unsigned ms = tempo_ms() - inicio;
            if (!ms) ms = 1;
            imprimir("sgldemo: ventana %dx%d, %d cuadros por %s: %u ms, %u fps, suma %08x\n", j->larg, j->alt, i,
                     sgl_gpu_ativa() ? "la GPU" : gpu_ini ? "la GPU (apagada a medias)" : "software", ms, (unsigned)i * 1000u / ms, soma);
            sgl_destruir_contexto(c);
            jan_fechar(j);
            return 0;
        }
        /* D391: antes dormía siempre un tic (dormir(1): con el reloj a 100 Hz, el cubo no pasaba de 50 fps
         * aunque la GPU pudiera más). Ahora solo duerme si ya van 3 cuadros en el mismo tic de 10 ms (unos
         * 300 fps: más no se ve y así deja la CPU a los demás); si no, solo cede el turno. -s: nunca. */
        if (agora != tic) { tic = agora; no_tic = 0; }
        if (!sem_pausa && ++no_tic >= 3) { dormir(1); no_tic = 0; }
        else hilo_ceder();
    }
}

int principal(int argc, char **argv)
{
    if (!tem_sse2()) { imprimir("sgldemo: necesita una CPU con SSE2\n"); return 1; }
    int n = 0, z = 32, gpu = 0, nj = 0, sem_pausa = 0, pais = 0, larg = 0, alt = 0;
    const char *cena = 0;
    opc_p_t op;
    memset(&op, 0, sizeof op);
    op.mip = 1;
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-p")) {
            pais = 1;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') op.quadros = texto_para_int(argv[++i]);
            continue;
        }
        if (!compara_texto(argv[i], "--mem")) { op.mem = 1; continue; }
        if (!compara_texto(argv[i], "--gpu")) { op.gpu = 1; continue; }
        if (!compara_texto(argv[i], "--sw")) { op.sw = 1; continue; }
        if (!compara_texto(argv[i], "--sin-mip")) { op.mip = 0; continue; }
        if (!compara_texto(argv[i], "--avion")) { op.camara = 1; continue; }
        if (!compara_texto(argv[i], "--texturas") && i + 1 < argc) { op.mb = texto_para_int(argv[++i]); continue; }
        if (!compara_texto(argv[i], "--detalle") && i + 1 < argc) { op.detalle = texto_para_int(argv[++i]); continue; }
        if (!compara_texto(argv[i], "--modelo") && i + 1 < argc) { op.modelo = argv[++i]; continue; }
        if (!compara_texto(argv[i], "--giro") && i + 1 < argc) { op.giro = (float)texto_para_int(argv[++i]); continue; }
        if (!compara_texto(argv[i], "-r") && i + 1 < argc) {
            const char *r = argv[++i];
            larg = texto_para_int(r);
            while (*r && *r != 'x') r++;
            if (*r) alt = texto_para_int(r + 1);
            continue;
        }
        if (!compara_texto(argv[i], "-t") && i + 1 < argc) n = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-z") && i + 1 < argc) z = texto_para_int(argv[++i]);
        else if (!compara_texto(argv[i], "-c") && i + 1 < argc) cena = argv[++i];
        else if (!compara_texto(argv[i], "-g")) { gpu = 60; if (i + 1 < argc) gpu = texto_para_int(argv[++i]); if (gpu < 1) gpu = 1; }
        else if (!compara_texto(argv[i], "-n") && i + 1 < argc) { nj = texto_para_int(argv[++i]); if (nj < 1) nj = 1; }
        else if (!compara_texto(argv[i], "-s")) sem_pausa = 1;
        else {
            imprimir("uso: sgldemo [-t cuadros] [-z 16|32] [-c escena] [-g [cuadros]] [-n cuadros] [-s]\n"
                     "       sgldemo -p [cuadros] [--mem [--gpu]] [--sw] [-r LxA] [--texturas MB] [--sin-mip]\n"
                     "               [--detalle N] [--avion] [--modelo F.obj [--giro G]]\n");
            return 1;
        }
    }
    if (pais) {
        op.larg = larg > 0 ? larg : op.mem ? 320 : 640;
        op.alt = alt > 0 ? alt : op.mem ? 240 : 480;
        if (op.larg < 16 || op.alt < 16 || op.larg > 2048 || op.alt > 2048) { imprimir("sgldemo: tamaño fuera de 16..2048\n"); return 1; }
        if (op.mb < 0 || op.mb > PAIS_MAX_MB) { imprimir("sgldemo: --texturas de 0 a %d MB\n", PAIS_MAX_MB); return 1; }
        return paisaje_medir(&op);
    }
    if (gpu) return comparar_gpu(gpu);
    if (cena) return fixas(cena);
    if (n > 0) return medir(n, z);
    return janela(nj, sem_pausa);
}
