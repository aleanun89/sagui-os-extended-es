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
 *     siempre (hoy gpu3d no comparte buferes con el compositor: hay una copia a la RAM).
 *
 * Si no hay GPU 3D, la autoprueba fallo, el destino no cabe o la GPU rechaza algo, el
 * renderizador sigue por software (sdlg_cair) sin que el programa lo note: lo dibujado se
 * trae antes al lienzo, asi que no se pierde nada (salvo si la GPU misma fallo). El
 * renderizador por software de siempre no cambia. Solo se usa con SDL_RENDERER_ACCELERATED
 * (o SAGUI_SDL2_GPU=1 para cualquiera que no pida SOFTWARE; SAGUI_SDL2_GPU=0 lo apaga). */

#define SDLG_MAXV 1020          /* vertices por lote (multiplo de 3, el kernel admite 3072) */
#define SDLG_VAGAS GPU3D_TEXTURAS

typedef struct { float x, y, z, rhw; uint32_t cor, espec; float u, v; } sdlg_vert_t;   /* = gpu3d_vert_t */
typedef struct { float x, y, u, v; } sdlg_pt_t;

typedef struct sdlgpu {
    int l, a;                       /* el destino (el lienzo logico) */
    int sujo;                       /* hay algo dibujado en la GPU que el lienzo no tiene */
    int n;                          /* vertices pendientes */
    int b_vaga, b_mezcla;           /* estado del lote pendiente (vaga -1: sin textura) */
    int e_vaga, e_mezcla;           /* ultimo estado mandado a la GPU (-2: ninguno) */
    unsigned long long uso;         /* contador para desalojar el hueco menos usado */
    SDL_Texture *vaga[SDLG_VAGAS];
    unsigned long long vaga_uso[SDLG_VAGAS];
    sdlg_vert_t v[SDLG_MAXV];
} sdlgpu_t;

int sagui_sdl2_gpu = 0;   /* 0: automatico (solo con SDL_RENDERER_ACCELERATED), 1: siempre, -1: nunca */

static int sdlg_chamar(unsigned op, unsigned a, unsigned b, unsigned c, const void *ptr)
{
    gpu3d_pedido_t q;
    q.op = op; q.a = a; q.b = b; q.c = c; q.ptr = (unsigned)ptr; q.bytes = 0;
    return info_sistema(INFO_GPU3D_OP, &q, sizeof q);
}

/* La politica de uso: ¿este renderizador lleva la GPU? */
static int sdlg_quiere(Uint32 flags)
{
    if (flags & SDL_RENDERER_SOFTWARE) return 0;
    int modo = sagui_sdl2_gpu;
    const char *e = getenv("SAGUI_SDL2_GPU");
    if (e && e[0]) modo = e[0] == '0' ? -1 : 1;
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
    g->e_vaga = g->e_mezcla = -2;
    g->b_vaga = -1;
    for (int i = 0; i < SDLG_VAGAS; i++) g->vaga[i] = NULL;
    if (sdlg_chamar(GPU3D_OP_LIMPAR, 1, 0, 0, 0) != 0) { sdlg_chamar(GPU3D_OP_FECHAR, 0, 0, 0, 0); free(g); return NULL; }
    return g;
}

/* Manda el lote pendiente. 0 bien; <0 la GPU fallo (ya apagada); >0 no puede (sigue el software). */
static int sdlg_despejar(sdlgpu_t *g)
{
    if (!g->n) return 0;
    int r = 0;
    if (g->e_vaga != g->b_vaga || g->e_mezcla != g->b_mezcla) {
        gpu3d_estado_t e;
        e.flags = 0;
        e.func_z = G3D_Z_SEMPRE;
        e.mez_orig = G3D_F_ALFA; e.mez_dest = G3D_F_UM_MENOS_ALFA;
        e.textura = 0;
        if (g->b_vaga >= 0) { e.flags |= G3D_TEXTURA; e.textura = (unsigned char)g->b_vaga; }
        if (g->b_mezcla) e.flags |= G3D_MEZCLA;
        r = sdlg_chamar(GPU3D_OP_ESTADO, 0, 0, 0, &e);
        if (r == 0) { g->e_vaga = g->b_vaga; g->e_mezcla = g->b_mezcla; }
    }
    if (r == 0) r = sdlg_chamar(GPU3D_OP_TRIANGULOS, (unsigned)g->n, 0, 0, g->v);
    g->n = 0;
    if (r == 0) g->sujo = 1;
    return r;
}

/* Trae el cuadro de la GPU al lienzo (con alfa opaco, como pinta el software). 0 bien. */
static int sdlg_ler(sdlgpu_t *g, uint32_t *tela)
{
    int r = sdlg_despejar(g);
    if (r) return r;
    if (!g->sujo) return 0;
    r = sdlg_chamar(GPU3D_OP_LER, (unsigned)g->l, 0, 0, tela);
    if (r) return r;
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
    if (v < 0) {
        v = 0;
        for (int i = 0; i < SDLG_VAGAS; i++) {
            if (!g->vaga[i]) { v = i; break; }
            if (g->vaga_uso[i] < g->vaga_uso[v]) v = i;
        }
        if (g->vaga[v]) sdlg_cair_textura(g->vaga[v]);   /* el desalojado vuelve a subirse cuando se use */
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

/* Recorta el poligono de entrada (n <= 8 puntos) a [0,l]x[0,a] (Sutherland-Hodgman) y lo
 * pasa a triangulos en abanico; u y v se interpolan. Devuelve los puntos que quedan. */
static int sdlg_recortar(const sdlg_pt_t *in, int n, int l, int a, sdlg_pt_t *out)
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
            case 0: dp = p.x; dq = q.x; break;
            case 1: dp = (float)l - p.x; dq = (float)l - q.x; break;
            case 2: dp = p.y; dq = q.y; break;
            default: dp = (float)a - p.y; dq = (float)a - q.y; break;
            }
            if (dp >= 0) b2[k++] = p;
            if ((dp >= 0) != (dq >= 0)) {
                float s = dp / (dp - dq);
                b2[k].x = p.x + (q.x - p.x) * s; b2[k].y = p.y + (q.y - p.y) * s;
                b2[k].u = p.u + (q.u - p.u) * s; b2[k].v = p.v + (q.v - p.v) * s;
                k++;
            }
        }
        m = k;
        for (int i = 0; i < m; i++) b1[i] = b2[i];
    }
    for (int i = 0; i < m; i++) out[i] = b1[i];
    return m;
}

/* Un poligono convexo (de 3 o 4 puntos) con textura (vaga >= 0) o de color, ya con el color
 * 0xAARRGGBB que multiplica. 0 bien; <0 la GPU fallo; >0 no puede. */
static int sdlg_poligono(sdlgpu_t *g, int vaga, int mezcla, uint32_t cor, const sdlg_pt_t *p, int n)
{
    sdlg_pt_t c[16];
    int m = sdlg_recortar(p, n, g->l, g->a, c);
    if (m < 3) return 0;
    int nv = (m - 2) * 3;
    if (g->n && (g->b_vaga != vaga || g->b_mezcla != mezcla || g->n + nv > SDLG_MAXV)) {
        int r = sdlg_despejar(g);
        if (r) return r;
    }
    g->b_vaga = vaga; g->b_mezcla = mezcla;
    for (int i = 1; i + 1 < m; i++) {
        const sdlg_pt_t *t3[3] = { &c[0], &c[i], &c[i + 1] };
        for (int k = 0; k < 3; k++) {
            sdlg_vert_t *v = &g->v[g->n++];
            v->x = t3[k]->x; v->y = t3[k]->y; v->z = 0.5f; v->rhw = 1.0f;
            v->cor = cor; v->espec = 0; v->u = t3[k]->u; v->v = t3[k]->v;
        }
    }
    return 0;
}

/* Rellena un rectangulo de color. */
static int sdlg_rect(sdlgpu_t *g, int x, int y, int w, int h, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca, int mezcla)
{
    sdlg_pt_t p[4] = { { (float)x, (float)y, 0, 0 }, { (float)(x + w), (float)y, 0, 0 },
                       { (float)(x + w), (float)(y + h), 0, 0 }, { (float)x, (float)(y + h), 0, 0 } };
    uint32_t cor = ((uint32_t)ca << 24) | ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb;
    return sdlg_poligono(g, -1, mezcla, cor, p, 4);
}

/* Borra todo el destino (lo pendiente se manda antes). */
static int sdlg_limpar(sdlgpu_t *g, Uint8 cr, Uint8 cg, Uint8 cb)
{
    int r = sdlg_despejar(g);
    if (r) return r;
    r = sdlg_chamar(GPU3D_OP_LIMPAR, 1, ((uint32_t)cr << 16) | ((uint32_t)cg << 8) | cb, 0, 0);
    if (r == 0) g->sujo = 1;
    return r;
}
