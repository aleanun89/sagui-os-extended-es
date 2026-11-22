/* sdl2gpu.h - renderizador de la SDL 2 por la GPU (v8.3, D288).
 *
 * Se incluye desde sdl2_sagui.c (despues de struct SDL_Renderer y struct SDL_Texture) y
 * usa la capa 3D comun del kernel (gpu3d, D285, docs/gpu3d.md) por INFO_GPU3D_OP:
 *   - cada SDL_Texture tiene su copia en la CPU (t->px, siempre valida) y, si hace falta,
 *     otra en un hueco de textura de la GPU (8 huecos: el menos usado se desaloja);
 *   - SDL_RenderCopy/CopyEx son un cuadrilatero con textura (escalado, espejo, giro,
 *     modulacion de color y alfa, mezcla); los rellenos y las lineas, cuadrilateros de color;
 *   - los vertices se acumulan y se mandan por lotes; todo se recorta antes al lienzo
 *     (el kernel solo admite 2 pixeles de margen);
 *   - SDL_RenderPresent trae el cuadro de la GPU al lienzo logico y lo presenta como
 *     siempre, salvo a pantalla completa con un driver que sepa presentar (D432, abajo).
 *
 * D432, presentar sin volver a la RAM: con la ventana a pantalla completa y un driver con
 * G3D_CAP_APRESENTAR, SDL_RenderPresent pide GPU3D_OP_APRESENTAR: la GPU copia (y escala) el
 * destino a la superficie visible de la pantalla y el compositor no pinta mientras dure. Si el
 * kernel contesta «ahora no» (un aviso, Alt+Tab, el salvapantallas...), ese cuadro vuelve por el
 * camino de siempre. Caso tipico de los ports (DOOM, AoEN, emuladores): una textura que se
 * actualiza cada cuadro y un SDL_RenderCopy que cubre todo el lienzo; esa copia se aplaza y, al
 * presentar, la GPU lleva la textura directamente a la pantalla (una sola copia, escalada).
 *   SDL_SAGUI_GPU=1            renderizador por GPU para cualquier programa (=0: nunca); es el
 *                              nombre nuevo de SAGUI_SDL2_GPU, que sigue valiendo
 *   SDL_SAGUI_GPU_PRESENTAR=0  con la GPU, presentar como antes (traer el cuadro a la RAM)
 *   SDL_SAGUI_GPU_VSYNC=1      esperar al retrazado vertical antes de copiar (menos desgarro)
 *   SDL_SAGUI_GPU_FILTRO=1     escalado bilineal (por defecto, vecino mas proximo como el software)
 *
 * Si no hay GPU 3D, la autoprueba fallo, el destino no cabe o la GPU rechaza algo, el
 * renderizador sigue por software (sdlg_cair) sin que el programa lo note: lo dibujado se
 * trae antes al lienzo, asi que no se pierde nada (salvo si la GPU misma fallo). El
 * renderizador por software de siempre no cambia. Solo se usa con SDL_RENDERER_ACCELERATED
 * (o SAGUI_SDL2_GPU=1 para cualquiera que no pida SOFTWARE; SAGUI_SDL2_GPU=0 lo apaga). */

#define SDLG_MAXV 3072          /* vertices por lote (multiplo de 3; lo que admite el kernel, GPU3D_MAX_VERT; D436) */
#define SDLG_VAGAS 32  /* D436: los 32 huecos del kernel (GPU3D_TEXTURAS, D404); con un kernel de antes de la D425, 8 */

/* D436: modos de mezcla del lote (los que caben en gpu3d v1) */
#define SDLG_M_NADA    0        /* opaco */
#define SDLG_M_ALFA    1        /* SDL_BLENDMODE_BLEND: origen * alfa + destino * (1 - alfa) */
#define SDLG_M_SOMA    2        /* SDL_BLENDMODE_ADD: origen * alfa + destino */
#define SDLG_M_PREMULT 3        /* alfa premultiplicado (SDL_ComposeCustomBlendMode ONE, ONE_MINUS_SRC_ALPHA) */

typedef struct { float x, y, z, rhw; uint32_t cor, espec; float u, v; } sdlg_vert_t;   /* = gpu3d_vert_t */
typedef struct { float x, y, u, v; uint32_t cor; } sdlg_pt_t;   /* cor: 0xAARRGGBB de ese vertice (D436) */

typedef struct sdlgpu {
    int l, a;                       /* el destino (el lienzo logico) */
    int sujo;                       /* hay algo dibujado en la GPU que el lienzo no tiene */
    int n;                          /* vertices pendientes */
    int b_vaga, b_mezcla, b_rep;    /* estado del lote pendiente (vaga -1: sin textura; mezcla SDLG_M_*; rep: repetir la textura) */
    int e_vaga, e_mezcla, e_rep;    /* ultimo estado mandado a la GPU (-2: ninguno) */
    int nvagas;                     /* D436: huecos de textura que da el kernel (32; 8 si es de antes de la D425) */
    int cx0, cy0, cx1, cy1;         /* D436: recorte (SDL_RenderSetClipRect) en pixeles del destino; sin el, todo */
    unsigned long desalojos;        /* D436: texturas que hubo que sacar de la GPU por falta de huecos */
    unsigned long desalojos_aviso;  /* lo que ya se dijo en el registro */
    unsigned long lotes, tris;      /* D436: lotes y triangulos mandados (para medir) */
    unsigned long long uso;         /* contador para desalojar el hueco menos usado */
    SDL_Texture *vaga[SDLG_VAGAS];
    unsigned long long vaga_uso[SDLG_VAGAS];
    /* D432 */
    int pres;                       /* el driver sabe presentar en la pantalla y no se pidio lo contrario */
    unsigned pres_flags;            /* G3D_APRES_* */
    int excl;                       /* el ultimo cuadro fue directo a la pantalla */
    unsigned long pres_n, pres_nao; /* cuadros directos y veces que el kernel dijo «ahora no» */
    unsigned long ler_n;            /* cuadros traidos a la RAM (GPU3D_OP_LER) */
    SDL_Texture *ad_t;              /* copia aplazada: esta textura cubre todo el lienzo (NULL: ninguna) */
    int ad_ox, ad_oy, ad_ol, ad_oa; /* su rectangulo de origen */
    sdlg_pt_t ad_p[4];              /* y el cuadrilatero, por si al final hay que dibujarla en el destino */
    sdlg_vert_t v[SDLG_MAXV];
} sdlgpu_t;

int sagui_sdl2_gpu = 0;   /* 0: automatico (solo con SDL_RENDERER_ACCELERATED), 1: siempre, -1: nunca */

/* D436 (audio), diagnostico con SDL_SAGUI_GPU_TIEMPOS=1: cuanto dura cada llamada al motor 3D del kernel. Mientras
 * dura una llamada, el kernel no deja correr a nadie mas ni atiende interrupciones (tampoco al sonido): una larga se
 * oye. Cada 5 s se escribe en el registro, por operacion, la mas larga y cuantas pasaron de 20 y de 80 ms (por el
 * TSC, calibrado con tempo_ms en los primeros 5 s). */
static int sdlg_tempos = -1;
static unsigned long long sdlg_t_max[16], sdlg_t_tsc0, sdlg_t_khz;
static unsigned long sdlg_t_n[16], sdlg_t_20[16], sdlg_t_80[16];
static unsigned sdlg_t_ms0;

static unsigned long long sdlg_tsc(void)
{
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

static void sdlg_tempos_medir(unsigned op, unsigned long long d)
{
    if (op >= 16) op = 0;
    sdlg_t_n[op]++;
    if (d > sdlg_t_max[op]) sdlg_t_max[op] = d;
    if (sdlg_t_khz) { if (d > sdlg_t_khz * 20) sdlg_t_20[op]++; if (d > sdlg_t_khz * 80) sdlg_t_80[op]++; }
    unsigned ms = tempo_ms();
    unsigned long long t = sdlg_tsc();
    if (!sdlg_t_ms0) { sdlg_t_ms0 = ms ? ms : 1; sdlg_t_tsc0 = t; return; }
    if (ms - sdlg_t_ms0 < 5000) return;
    int primeiro = !sdlg_t_khz;
    if (primeiro) sdlg_t_khz = (t - sdlg_t_tsc0) / (ms - sdlg_t_ms0);
    {
        char b[480];
        int p = 0;
        b[0] = 0;
        for (int i = 0; i < 16 && p < 420; i++)
            if (sdlg_t_n[i])
                p += snprintf(b + p, sizeof b - (size_t)p, " op%d %lu %lums %lu %lu;", i, sdlg_t_n[i],
                              (unsigned long)(sdlg_t_max[i] / sdlg_t_khz), sdlg_t_20[i], sdlg_t_80[i]);
        registrar("[SDL] gpu3d en 5 s (op llamadas max >20ms >80ms)%s:%s", primeiro ? " (las primeras)" : "", b);
    }
    for (int i = 0; i < 16; i++) { sdlg_t_max[i] = 0; sdlg_t_n[i] = sdlg_t_20[i] = sdlg_t_80[i] = 0; }
    sdlg_t_ms0 = ms; sdlg_t_tsc0 = t;
}

static int sdlg_chamar(unsigned op, unsigned a, unsigned b, unsigned c, const void *ptr)
{
    gpu3d_pedido_t q;
    q.op = op; q.a = a; q.b = b; q.c = c; q.ptr = (unsigned)ptr; q.bytes = 0;
    if (sdlg_tempos < 0) { const char *e = getenv("SDL_SAGUI_GPU_TIEMPOS"); sdlg_tempos = e && e[0] == '1'; }
    if (!sdlg_tempos) return info_sistema(INFO_GPU3D_OP, &q, sizeof q);
    unsigned long long t0 = sdlg_tsc();
    int r = info_sistema(INFO_GPU3D_OP, &q, sizeof q);
    sdlg_tempos_medir(op, sdlg_tsc() - t0);
    return r;
}

/* La politica de uso: ¿este renderizador lleva la GPU? */
static int sdlg_quiere(Uint32 flags)
{
    if (flags & SDL_RENDERER_SOFTWARE) return 0;
    int modo = sagui_sdl2_gpu;
    const char *e = getenv("SDL_SAGUI_GPU");   /* D432: el nombre nuevo; el de la D288 sigue valiendo */
    if (!e || !e[0]) e = getenv("SAGUI_SDL2_GPU");
    if (e && e[0]) modo = e[0] == '0' ? -1 : 1;
    else if (modo == 0) {
        /* D323: lo que se eligió en Configuración (Aceleración gráfica): /tmp/acel-sdl2 con "1" (sí) o "0" (no);
         * sin el archivo, automático. La variable de entorno y sagui_sdl2_gpu mandan sobre él. */
        int fa = abrir("/tmp/acel-sdl2", ABRIR_LER);
        if (fa >= 0) {
            char ch = 0;
            if (ler(fa, &ch, 1) == 1) modo = ch == '1' ? 1 : ch == '0' ? -1 : 0;
            fechar(fa);
        }
    }
    if (modo < 0) return 0;
    return modo > 0 || (flags & SDL_RENDERER_ACCELERATED);
}

/* Abre el motor para el lienzo l x a; NULL si no hay GPU o no cabe (sigue el software). */
static sdlgpu_t *sdlg_abrir(int l, int a)
{
    if (l < 8 || a < 8) return NULL;
    sdlgpu_t *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    if (sdlg_chamar(GPU3D_OP_ABRIR, (unsigned)l, (unsigned)a, 16, 0) != 0) { free(g); return NULL; }
    g->l = l; g->a = a;
    g->e_vaga = g->e_mezcla = g->e_rep = -2;
    g->b_vaga = -1;
    g->cx0 = 0; g->cy0 = 0; g->cx1 = l; g->cy1 = a;
    g->nvagas = 8;   /* un kernel de antes de la D425 no contesta CAPACIDADES y solo da 8 huecos */
    for (int i = 0; i < SDLG_VAGAS; i++) g->vaga[i] = NULL;
    if (sdlg_chamar(GPU3D_OP_LIMPAR, 1, 0, 0, 0) != 0) { sdlg_chamar(GPU3D_OP_FECHAR, 0, 0, 0, 0); free(g); return NULL; }
    /* D432: ¿el driver sabe presentar en la pantalla? (un kernel de antes contesta sin ese bit) */
    gpu3d_capas_t c;
    memset(&c, 0, sizeof c);
    const char *e = getenv("SDL_SAGUI_GPU_PRESENTAR");
    if (sdlg_chamar(GPU3D_OP_CAPACIDADES, 0, 0, 0, &c) == 0) {
        g->nvagas = SDLG_VAGAS < GPU3D_TEXTURAS ? SDLG_VAGAS : GPU3D_TEXTURAS;
        if ((c.bits & G3D_CAP_APRESENTAR) && !(e && e[0] == '0')) g->pres = 1;
    }
    e = getenv("SDL_SAGUI_GPU_VSYNC");
    if (e && e[0] == '1') g->pres_flags |= G3D_APRES_VSYNC;
    e = getenv("SDL_SAGUI_GPU_FILTRO");
    if (e && e[0] == '1') g->pres_flags |= G3D_APRES_LINEAR;
    return g;
}

/* Manda el lote pendiente. 0 bien; <0 la GPU fallo (ya apagada); >0 no puede (sigue el software). */
static int sdlg_despejar(sdlgpu_t *g)
{
    if (!g->n) return 0;
    int r = 0;
    if (g->e_vaga != g->b_vaga || g->e_mezcla != g->b_mezcla || g->e_rep != g->b_rep) {
        gpu3d_estado_t e;
        e.flags = 0;
        e.func_z = G3D_Z_SEMPRE;
        e.mez_orig = G3D_F_ALFA; e.mez_dest = G3D_F_UM_MENOS_ALFA;
        e.textura = 0;
        if (g->b_vaga >= 0) {
            e.flags |= G3D_TEXTURA; e.textura = (unsigned char)g->b_vaga;
            if (g->b_rep) e.flags |= G3D_TEX_REPETE_S | G3D_TEX_REPETE_T;
        }
        if (g->b_mezcla != SDLG_M_NADA) {
            e.flags |= G3D_MEZCLA;
            if (g->b_mezcla == SDLG_M_SOMA) e.mez_dest = G3D_F_UM;   /* D436: ADD */
            else if (g->b_mezcla == SDLG_M_PREMULT) e.mez_orig = G3D_F_UM;   /* D436: alfa premultiplicado */
        }
        r = sdlg_chamar(GPU3D_OP_ESTADO, 0, 0, 0, &e);
        if (r == 0) { g->e_vaga = g->b_vaga; g->e_mezcla = g->b_mezcla; g->e_rep = g->b_rep; }
    }
    if (r == 0) r = sdlg_chamar(GPU3D_OP_TRIANGULOS, (unsigned)g->n, 0, 0, g->v);
    if (r == 0) { g->lotes++; g->tris += (unsigned long)(g->n / 3); }
    g->n = 0;
    if (r == 0) g->sujo = 1;
    return r;
}

static int sdlg_adiado(sdlgpu_t *g);
static void sdlg_aviso_desalojo(sdlgpu_t *g);

/* Trae el cuadro de la GPU al lienzo (con alfa opaco, como pinta el software). 0 bien. */
static int sdlg_ler(sdlgpu_t *g, uint32_t *tela)
{
    int r = sdlg_adiado(g);   /* D432: la copia aplazada, ahora si, al destino */
    if (r) return r;
    r = sdlg_despejar(g);
    if (r) return r;
    if (!g->sujo) return 0;
    r = sdlg_chamar(GPU3D_OP_LER, (unsigned)g->l, 0, 0, tela);
    if (r) return r;
    g->ler_n++;
    size_t n = (size_t)g->l * (size_t)g->a;
    for (size_t i = 0; i < n; i++) tela[i] |= 0xFF000000u;
    g->sujo = 0;
    return 0;
}

/* La textura deja de estar en la GPU (se vuelve a subir cuando se use). */
static void sdlg_cair_textura(SDL_Texture *t) { t->gpu_vaga = -1; t->gpu_sujo = 1; }

/* Deja la GPU y vuelve al software; lo ya dibujado se trae al lienzo si se puede. */
static void sdlg_fechar(sdlgpu_t *g, uint32_t *tela, int preservar)
{
    if (preservar && tela) sdlg_ler(g, tela);
    sdlg_chamar(GPU3D_OP_FECHAR, 0, 0, 0, 0);
    for (int i = 0; i < SDLG_VAGAS; i++) if (g->vaga[i]) sdlg_cair_textura(g->vaga[i]);
    free(g);
}

/* Un hueco de textura para t con sus pixeles al dia (desaloja el menos usado).
 * Devuelve el hueco; -1 la GPU fallo; -2 no puede (sigue el software). */
static int sdlg_textura(sdlgpu_t *g, SDL_Texture *t)
{
    int v = t->gpu_vaga;
    if (g->ad_t && (v < 0 || t->gpu_sujo)) {   /* D432: la copia aplazada usa un hueco que puede cambiar */
        int r = sdlg_adiado(g);
        if (r) return r < 0 ? -1 : -2;
    }
    if (v < 0) {
        v = 0;
        for (int i = 0; i < g->nvagas; i++) {
            if (!g->vaga[i]) { v = i; break; }
            if (g->vaga_uso[i] < g->vaga_uso[v]) v = i;
        }
        if (g->vaga[v]) {
            /* el desalojado vuelve a subirse cuando se use; antes, lo pendiente que lo usa */
            if (g->n && g->b_vaga == v) { int r = sdlg_despejar(g); if (r) return r < 0 ? -1 : -2; }
            sdlg_cair_textura(g->vaga[v]);
            /* D436: faltan huecos; se dice en el registro (la primera vez y luego cada 256) */
            if (++g->desalojos >= g->desalojos_aviso) {
                g->desalojos_aviso = g->desalojos + 256;
                sdlg_aviso_desalojo(g);
            }
        }
        g->vaga[v] = NULL;
        t->gpu_sujo = 1;
    }
    if (t->gpu_sujo) {
        int r = sdlg_despejar(g);   /* lo pendiente usaba el contenido viejo del hueco */
        if (r) return r < 0 ? -1 : -2;
        r = sdlg_chamar(GPU3D_OP_TEXTURA, (unsigned)v, (unsigned)t->l, (unsigned)t->a, t->px);
        if (r) return r < 0 ? -1 : -2;
        t->gpu_sujo = 0;
    }
    g->vaga[v] = t;
    g->vaga_uso[v] = ++g->uso;
    t->gpu_vaga = v;
    return v;
}

/* Un color 0xAARRGGBB entre dos, con s de 0 a 1 (los puntos nuevos del recorte, D436). */
static uint32_t sdlg_cor_mistura(uint32_t a, uint32_t b, float s)
{
    if (a == b) return a;
    uint32_t o = 0;
    for (int k = 0; k < 32; k += 8) {
        float ca = (float)((a >> k) & 255u), cb = (float)((b >> k) & 255u);
        int c = (int)(ca + (cb - ca) * s + 0.5f);
        o |= (uint32_t)(c < 0 ? 0 : c > 255 ? 255 : c) << k;
    }
    return o;
}

/* Recorta el poligono de entrada (n <= 8 puntos) a [x0,x1]x[y0,y1] (Sutherland-Hodgman) y lo
 * pasa a triangulos en abanico; u, v y el color (D436) se interpolan. Devuelve los puntos que quedan. */
static int sdlg_recortar(const sdlg_pt_t *in, int n, int x0, int y0, int x1, int y1, sdlg_pt_t *out)
{
    sdlg_pt_t b1[16], b2[16];
    int m = n;
    for (int i = 0; i < n; i++) b1[i] = in[i];
    for (int borda = 0; borda < 4 && m > 0; borda++) {
        int k = 0;
        for (int i = 0; i < m; i++) {
            sdlg_pt_t p = b1[i], q = b1[(i + 1) % m];
            float dp, dq;   /* distancia con signo al borde: >= 0 dentro */
            switch (borda) {
            case 0: dp = p.x - (float)x0; dq = q.x - (float)x0; break;
            case 1: dp = (float)x1 - p.x; dq = (float)x1 - q.x; break;
            case 2: dp = p.y - (float)y0; dq = q.y - (float)y0; break;
            default: dp = (float)y1 - p.y; dq = (float)y1 - q.y; break;
            }
            if (dp >= 0) b2[k++] = p;
            if ((dp >= 0) != (dq >= 0)) {
                float s = dp / (dp - dq);
                b2[k].x = p.x + (q.x - p.x) * s; b2[k].y = p.y + (q.y - p.y) * s;
                b2[k].u = p.u + (q.u - p.u) * s; b2[k].v = p.v + (q.v - p.v) * s;
                b2[k].cor = sdlg_cor_mistura(p.cor, q.cor, s);
                k++;
            }
        }
        m = k;
        for (int i = 0; i < m; i++) b1[i] = b2[i];
    }
    for (int i = 0; i < m; i++) out[i] = b1[i];
    return m;
}

/* Un poligono convexo (de 3 o 4 puntos) con textura (vaga >= 0) o de color; cada punto lleva su
 * color 0xAARRGGBB, que multiplica (D436). mezcla: SDLG_M_*; rep: repetir la textura (lados potencia
 * de 2). Consecutivos con el mismo estado van en el mismo lote. 0 bien; <0 la GPU fallo; >0 no puede. */
static int sdlg_poligono_int(sdlgpu_t *g, int vaga, int mezcla, int rep, const sdlg_pt_t *p, int n);
static int sdlg_poligono(sdlgpu_t *g, int vaga, int mezcla, int rep, const sdlg_pt_t *p, int n)
{
    int r = sdlg_adiado(g);   /* D432: algo se dibuja encima de la copia aplazada: primero ella */
    if (r) return r;
    return sdlg_poligono_int(g, vaga, mezcla, rep, p, n);
}
static int sdlg_poligono_int(sdlgpu_t *g, int vaga, int mezcla, int rep, const sdlg_pt_t *p, int n)
{
    sdlg_pt_t c[16];
    const sdlg_pt_t *q = p;
    int m = n;
    /* D436: lo que cae dentro del recorte (casi todo) no pasa por Sutherland-Hodgman */
    for (int i = 0; i < n; i++)
        if (!(p[i].x >= (float)g->cx0 && p[i].x <= (float)g->cx1 && p[i].y >= (float)g->cy0 && p[i].y <= (float)g->cy1)) {
            m = sdlg_recortar(p, n, g->cx0, g->cy0, g->cx1, g->cy1, c);
            q = c;
            break;
        }
    if (m < 3) return 0;
    int nv = (m - 2) * 3;
    if (vaga < 0) rep = 0;
    if (g->n && (g->b_vaga != vaga || g->b_mezcla != mezcla || g->b_rep != rep || g->n + nv > SDLG_MAXV)) {
        int r = sdlg_despejar(g);
        if (r) return r;
    }
    g->b_vaga = vaga; g->b_mezcla = mezcla; g->b_rep = rep;
    for (int i = 1; i + 1 < m; i++) {
        const sdlg_pt_t *t3[3] = { &q[0], &q[i], &q[i + 1] };
        for (int k = 0; k < 3; k++) {
            sdlg_vert_t *v = &g->v[g->n++];
            v->x = t3[k]->x; v->y = t3[k]->y; v->z = 0.5f; v->rhw = 1.0f;
            v->cor = t3[k]->cor; v->espec = 0; v->u = t3[k]->u; v->v = t3[k]->v;
        }
    }
    return 0;
}

/* Rellena un rectangulo de color. */
static int sdlg_rect(sdlgpu_t *g, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, int mezcla)
{
    uint32_t cor = ((uint32_t)ca << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
    sdlg_pt_t p[4] = { { (float)x, (float)y, 0, 0, cor }, { (float)(x + w), (float)y, 0, 0, cor },
                       { (float)(x + w), (float)(y + h), 0, 0, cor }, { (float)x, (float)(y + h), 0, 0, cor } };
    return sdlg_poligono(g, -1, mezcla, 0, p, 4);
}

/* Borra todo el destino (lo pendiente se manda antes). */
static int sdlg_limpar(sdlgpu_t *g, Uint8 cr, Uint8 cg, Uint8 cb)
{
    g->ad_t = NULL;   /* D432: el borrado tapa la copia aplazada */
    int r = sdlg_despejar(g);
    if (r) return r;
    r = sdlg_chamar(GPU3D_OP_LIMPAR, 1, ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb, 0, 0);
    if (r == 0) g->sujo = 1;
    return r;
}

/* ------------------------------------------------------------------ D432: presentar en la pantalla */

/* La copia aplazada se dibuja en el destino como cualquier otra (ya no puede ir directa). */
static int sdlg_adiado(sdlgpu_t *g)
{
    if (!g->ad_t) return 0;
    SDL_Texture *t = g->ad_t;
    g->ad_t = NULL;
    if (t->gpu_vaga < 0) return 0;
    g->vaga_uso[t->gpu_vaga] = ++g->uso;
    return sdlg_poligono_int(g, t->gpu_vaga, SDLG_M_NADA, 0, g->ad_p, 4);
}

/* Aplaza una copia opaca que cubre todo el lienzo (v: el hueco de t, ya subido). */
static void sdlg_adiar(sdlgpu_t *g, SDL_Texture *t, int ox, int oy, int ol, int oa, const sdlg_pt_t *p)
{
    g->ad_t = t;
    g->ad_ox = ox; g->ad_oy = oy; g->ad_ol = ol; g->ad_oa = oa;
    for (int i = 0; i < 4; i++) g->ad_p[i] = p[i];
    g->n = 0;   /* lo pendiente queda tapado por esta copia */
}

/* Presenta en la pantalla: la textura aplazada o el destino, al rectangulo (dx, dy, dl, da) del
 * contenido de la ventana. 0: hecho; > 0: ahora no (presentar como siempre); < 0: la GPU fallo. */
static int sdlg_apresentar(sdlgpu_t *g, int janela, int dx, int dy, int dl, int da)
{
    gpu3d_apresentar_t a;
    memset(&a, 0, sizeof a);
    if (dx < 0 || dy < 0 || dl < 1 || da < 1 || dx + dl > 0xFFFF || dy + da > 0xFFFF) return 1;
    a.janela = (unsigned)janela;
    a.dx = (unsigned short)dx; a.dy = (unsigned short)dy; a.dl = (unsigned short)dl; a.da = (unsigned short)da;
    a.flags = g->pres_flags;
    if (g->ad_t && g->ad_t->gpu_vaga >= 0) {
        a.fonte = (unsigned)g->ad_t->gpu_vaga;
        a.ox = (unsigned short)g->ad_ox; a.oy = (unsigned short)g->ad_oy;
        a.ol = (unsigned short)g->ad_ol; a.oa = (unsigned short)g->ad_oa;
    } else {
        int r = sdlg_adiado(g);
        if (!r) r = sdlg_despejar(g);
        if (r) return r;
        a.fonte = GPU3D_DESTINO_PRINCIPAL;
        a.ol = (unsigned short)g->l; a.oa = (unsigned short)g->a;
    }
    int r = sdlg_chamar(GPU3D_OP_APRESENTAR, 0, 0, 0, &a);
    if (r == 0) { g->ad_t = NULL; g->pres_n++; }
    else if (r > 0) g->pres_nao++;
    return r;
}
