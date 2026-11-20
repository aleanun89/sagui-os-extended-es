// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_visao.c - el proyector de imágenes de LFM3-VL (D380, fase 2): `llm --mmproj`, `--imagen`
 * y las imágenes del servidor.
 *
 * El «mmproj» de LFM3-VL (arquitectura «clip», proyector «lfm3vl») no es un SigLIP: es un
 * codificador híbrido de 14 bloques (la referencia es llama.cpp, tools/mtmd/models/lfm3vl.cpp):
 *
 *   imagen RGB -> tamaño múltiplo de 32 (smart_resize de Qwen2-VL, bicúbico con antialias de
 *   torchvision) -> (x/255 - 0,5)/0,5 -> parches de 16x16: dos núcleos (los dos cuadros de un
 *   vídeo; una foto llena los dos) sumados -> norma RMS
 *   por bloque: h = norma RMS(x)
 *       convolución: p = in_proj(h) = [a | m | b];  g = a*b;  g = conv 3x3 por canal (con ceros
 *                    alrededor) sobre la rejilla de parches;  h = out_proj(m*g)
 *       atención:    q, k, v (16 cabezas de 64, 8 de K/V), norma RMS de q y k por cabeza,
 *                    RoPE 2D (las 16 primeras parejas giran con la fila, las 16 siguientes con
 *                    la columna, base 1e6), atención completa;  h = attn_out(o)
 *       x = x + h;  x = x + ffn_down(silu(ffn_gate(norma(x))) * ffn_up(norma(x)))
 *   LayerNorm (con sesgo, eps 1e-6) -> cada 2x2 parches juntos (4096) -> mm.0 + sesgo -> GELU
 *   (erf) -> mm.2 + sesgo = un vector de 2048 (el embedding del modelo) por cada 2x2 parches.
 *
 * Los vectores entran en la caché entre <|vision_start|> y <|vision_end|>, en posiciones
 * seguidas (el LFM3 tiene RoPE de una dimensión). Las multiplicaciones van como las del
 * modelo: entrada en Q8_0 y productos de enteros (llm_dot_q8_0, por la mejor ruta de la
 * CPU), de modo que SaguiOS y `make llm-pc` dan los mismos números (h412). */
#include "llm_programa.h"

#define VIS_MAX_CAMADAS 32
#define VIS_TOKENS_PADRAO 144    /* ~384x384: 144 vectores, 576 parches; más es mucho más lento */

typedef struct {
    const gguf_tensor_t *ln1, *ln2, *up, *gate, *down;
    const gguf_tensor_t *conv_in, *conv_dw, *conv_out;          /* bloque de convolución */
    const gguf_tensor_t *q, *k, *v, *o, *qn, *kn;               /* bloque de atención */
} vis_camada_t;

struct llm_visao {
    gguf_t g;
    int E, F, L, H, Hkv, D, P, saida, entrada4;
    float eps, media[3], desvio[3], rope_base;
    long long min_px, max_px;
    int tokens_max;
    float *nucleo;           /* [E][3*P*P] los dos núcleos de parche en f32: [0] y [1] */
    const gguf_tensor_t *pe0, *pe1, *emb_norma, *mm_nw, *mm_nb, *mm0, *mm0b, *mm2, *mm2b;
    vis_camada_t c[VIS_MAX_CAMADAS];
};

#define ERRO(...) do { llm_formatar(erro, max, __VA_ARGS__); return -1; } while (0)

static const gguf_tensor_t *tv(const gguf_t *g, int il, const char *nome)
{
    char n[96];
    if (il < 0) llm_formatar(n, sizeof n, "%s", nome);
    else llm_formatar(n, sizeof n, "v.blk.%d.%s.weight", il, nome);
    return gguf_tensor(g, n);
}

static bool tipo_ok(const gguf_tensor_t *t)
{
    return t->tipo == T_F32 || t->tipo == T_F16 || t->tipo == T_Q8_0;
}

/* Matriz [n_in -> n_out] o vector (n_out = 0: un vector f32 de n_in). */
static int conferir(const gguf_tensor_t *t, const char *nome, int n_in, int n_out, char *erro, size_t max)
{
    if (!t) ERRO("mmproj: falta el tensor %s", nome);
    if (!n_out) {
        if (t->tipo != T_F32 || t->ne[0] != (uint64_t)n_in || t->ne[1] != 1 || t->ne[2] != 1 || t->ne[3] != 1)
            ERRO("mmproj: %s no es un vector f32 de %d", nome, n_in);
        return 0;
    }
    if (!tipo_ok(t)) ERRO("mmproj: %s es %s (este motor lee el mmproj en Q8_0, F16 o F32)", nome, gguf_nome_tipo(t->tipo));
    if (t->ne[0] != (uint64_t)n_in || t->ne[1] != (uint64_t)n_out || t->ne[2] != 1 || t->ne[3] != 1)
        ERRO("mmproj: %s mide %ux%u, se esperaba %dx%d", nome, (unsigned)t->ne[0], (unsigned)t->ne[1], n_in, n_out);
    return 0;
}

static bool arr_f32(const gguf_t *g, const char *nome, int n, float *v)
{
    const gguf_kv_t *kv = gguf_kv(g, nome);
    if (!kv || kv->tipo != GGUF_ARRAY || kv->arr_tipo != GGUF_FLOAT32 || kv->arr_n < (uint64_t)n) return false;
    for (int i = 0; i < n; i++) llm_memcpy(&v[i], kv->dados + 4 * i, 4);
    return true;
}

void llm_visao_fechar(struct llm_visao *v)
{
    if (!v) return;
    gguf_fechar(&v->g);
    llm_libera(v->nucleo);
    llm_libera(v);
}

static void progresso_mmproj(unsigned pct)
{
    if (pct == 100) llm_eprintf("\rCargando el proyector de imágenes: 100%%\n");
}

/* Abre y carga el mmproj; n_embd: el embedding del modelo de texto (la salida tiene que medir eso). */
int llm_visao_abrir(struct llm_visao **sai, const char *caminho, int n_embd, int tokens_max, bool silencio, char *erro, size_t max)
{
    *sai = NULL;
    struct llm_visao *v = llm_aloca_zero(sizeof *v);
    if (!v) ERRO("sin memoria");
    gguf_t *g = &v->g;
    if (gguf_abrir(g, caminho)) { llm_formatar(erro, max, "%s", g->erro); llm_visao_fechar(v); return -1; }
#undef ERRO
#define ERRO(...) do { llm_formatar(erro, max, __VA_ARGS__); llm_visao_fechar(v); return -1; } while (0)
    llm_texto_t t;
    if (!gguf_texto(g, "general.architecture", &t) || t.n != 4 || llm_memcmp(t.s, "clip", 4)) ERRO("%s no es un proyector (mmproj)", caminho);
    if (!gguf_texto(g, "clip.vision.projector_type", &t) || t.n != 6 || llm_memcmp(t.s, "lfm3vl", 6))
        ERRO("proyector «%.*s» no admitido (este motor lee el de LFM3-VL, lfm3vl)", t.n < 32 ? (int)t.n : 32, t.s);
    uint32_t u;
#define CHAVE(nome, var, lo, hi) do { if (!gguf_u32(g, nome, &u) || u < (lo) || u > (hi)) ERRO("mmproj: %s no válido", nome); var = (int)u; } while (0)
    CHAVE("clip.vision.embedding_length", v->E, 64, 8192);
    CHAVE("clip.vision.feed_forward_length", v->F, 64, 32768);
    CHAVE("clip.vision.block_count", v->L, 1, VIS_MAX_CAMADAS);
    CHAVE("clip.vision.attention.head_count", v->H, 1, 128);
    CHAVE("clip.vision.attention.head_count_kv", v->Hkv, 1, 128);
    CHAVE("clip.vision.patch_size", v->P, 2, 64);
    CHAVE("clip.vision.spatial_merge_size", u, 2, 2);
#undef CHAVE
    if (v->E % v->H || v->H % v->Hkv) ERRO("mmproj: cabezas no válidas");
    v->D = v->E / v->H;
    if (v->D % 32 || v->D > 256) ERRO("mmproj: cabezas de %d (se esperaba un múltiplo de 32)", v->D);
    if (!gguf_f32(g, "clip.vision.attention.layer_norm_epsilon", &v->eps)) v->eps = 1e-5f;
    if (!arr_f32(g, "clip.vision.image_mean", 3, v->media)) v->media[0] = v->media[1] = v->media[2] = 0.5f;
    if (!arr_f32(g, "clip.vision.image_std", 3, v->desvio)) v->desvio[0] = v->desvio[1] = v->desvio[2] = 0.5f;
    for (int i = 0; i < 3; i++) if (!(v->desvio[i] > 1e-6f)) ERRO("mmproj: image_std no válido");
    v->min_px = gguf_u32(g, "clip.vision.image_min_pixels", &u) ? u : 65536;
    v->max_px = gguf_u32(g, "clip.vision.image_max_pixels", &u) ? u : 16777216;
    v->rope_base = 1000000.0f;   /* vision_rope_theta de LFM3-VL (no va en el GGUF) */
    const int E = v->E, P = v->P, area = 4 * P * P;
    /* tokens: de los del modelo (min) a los pedidos (de serie 144); los píxeles, según eso */
    v->tokens_max = tokens_max > 0 ? tokens_max : VIS_TOKENS_PADRAO;
    if (v->tokens_max * (long long)area < v->min_px) v->tokens_max = (int)(v->min_px / area);
    if (v->tokens_max > 1024) v->tokens_max = 1024;
    if ((long long)v->tokens_max * area < v->max_px) v->max_px = (long long)v->tokens_max * area;
    if (v->min_px > v->max_px) v->min_px = v->max_px;

    v->pe0 = tv(g, -1, "v.patch_embd.weight");
    v->pe1 = tv(g, -1, "v.patch_embd.weight.1");
    for (int k = 0; k < 2; k++) {
        const gguf_tensor_t *p = k ? v->pe1 : v->pe0;
        if (!p) { if (k) break; ERRO("mmproj: falta v.patch_embd.weight"); }
        if ((p->tipo != T_F16 && p->tipo != T_F32) || p->ne[0] != (uint64_t)P || p->ne[1] != (uint64_t)P || p->ne[2] != 3 ||
            p->ne[3] != (uint64_t)E) ERRO("mmproj: v.patch_embd no válido");
    }
    v->emb_norma = tv(g, -1, "v.embedding_norm.weight");
    if (conferir(v->emb_norma, "v.embedding_norm", E, 0, erro, max)) { llm_visao_fechar(v); return -1; }
    for (int il = 0; il < v->L; il++) {
        vis_camada_t *c = &v->c[il];
        char n[64];
#define T(campo, nome, a, b) do { c->campo = tv(g, il, nome); llm_formatar(n, sizeof n, "v.blk.%d.%s", il, nome); \
                                  if (conferir(c->campo, n, a, b, erro, max)) { llm_visao_fechar(v); return -1; } } while (0)
        T(ln1, "ln1", E, 0); T(ln2, "ln2", E, 0);
        T(up, "ffn_up", E, v->F); T(gate, "ffn_gate", E, v->F); T(down, "ffn_down", v->F, E);
        if (tv(g, il, "conv2d_in_proj")) {
            T(conv_in, "conv2d_in_proj", E, 3 * E); T(conv_out, "conv2d_out_proj", E, E);
            c->conv_dw = tv(g, il, "conv2d_spatial");
            if (!c->conv_dw || c->conv_dw->tipo != T_F32 || c->conv_dw->ne[0] != 3 || c->conv_dw->ne[1] != 3 ||
                c->conv_dw->ne[2] != 1 || c->conv_dw->ne[3] != (uint64_t)E) ERRO("mmproj: v.blk.%d.conv2d_spatial no válido", il);
        } else {
            T(q, "attn_q", E, E); T(k, "attn_k", E, v->Hkv * v->D); T(v, "attn_v", E, v->Hkv * v->D); T(o, "attn_out", E, E);
            T(qn, "attn_q_norm", v->D, 0); T(kn, "attn_k_norm", v->D, 0);
        }
#undef T
    }
    v->mm_nw = tv(g, -1, "mm.input_norm.weight"); v->mm_nb = tv(g, -1, "mm.input_norm.bias");
    v->mm0 = tv(g, -1, "mm.0.weight"); v->mm0b = tv(g, -1, "mm.0.bias");
    v->mm2 = tv(g, -1, "mm.2.weight"); v->mm2b = tv(g, -1, "mm.2.bias");
    v->entrada4 = 4 * E;
    if (conferir(v->mm_nw, "mm.input_norm.weight", E, 0, erro, max) || conferir(v->mm_nb, "mm.input_norm.bias", E, 0, erro, max) ||
        !v->mm0 || conferir(v->mm0, "mm.0.weight", 4 * E, (int)v->mm0->ne[1], erro, max) ||
        conferir(v->mm0b, "mm.0.bias", (int)v->mm0->ne[1], 0, erro, max) ||
        !v->mm2 || conferir(v->mm2, "mm.2.weight", (int)v->mm0->ne[1], (int)v->mm2->ne[1], erro, max) ||
        conferir(v->mm2b, "mm.2.bias", (int)v->mm2->ne[1], 0, erro, max)) { llm_visao_fechar(v); return -1; }
    v->saida = (int)v->mm2->ne[1];
    if (v->saida != n_embd) ERRO("el proyector da vectores de %d y el modelo usa %d: no son del mismo modelo", v->saida, n_embd);
    unsigned long long livre = llm_memoria_livre();
    if (livre && g->dados_n + (64ull << 20) > livre) {
        char b[32];
        llm_mb(b, sizeof b, g->dados_n);
        ERRO("no queda memoria para el proyector de imágenes (%s)", b);
    }
    if (gguf_carregar(g, silencio ? NULL : progresso_mmproj)) ERRO("%s", g->erro);
    /* los dos núcleos de parche en f32 */
    const int K = 3 * P * P;
    v->nucleo = llm_aloca((size_t)2 * E * K * 4);
    if (!v->nucleo) ERRO("sin memoria");
    for (int k = 0; k < 2; k++) {
        const gguf_tensor_t *p = k ? v->pe1 : v->pe0;
        float *d = v->nucleo + (size_t)k * E * K;
        if (!p) { llm_memset(d, 0, (size_t)E * K * 4); continue; }
        for (int r = 0; r < E; r++) llm_decuant_linha(p->tipo, (const uint8_t *)p->dados + (size_t)r * gguf_tamanho_linha(p->tipo, (uint64_t)K),
                                                      d + (size_t)r * K, K);
    }
    *sai = v;
    return 0;
}
#undef ERRO

/* ------------------------------------------------------------ operaciones */

static double raiz_d(double x)
{
    if (x <= 0) return 0;
    double r = llm_sqrtf((float)x);
    for (int i = 0; i < 3; i++) r = 0.5 * (r + x / r);
    return r;
}

/* Redondeo al par más cercano (nearbyint, como Python round). */
static double arred_par(double x)
{
    double f = (double)(long long)x;
    if (x < 0 && f != x) f -= 1;   /* suelo */
    double d = x - f;
    if (d > 0.5) return f + 1;
    if (d < 0.5) return f;
    return ((long long)f % 2) ? f + 1 : f;
}
static double suelo(double x) { double f = (double)(long long)x; return (x < 0 && f != x) ? f - 1 : f; }
static double techo(double x) { double f = (double)(long long)x; return (x > 0 && f != x) ? f + 1 : f; }

/* smart_resize de Qwen2-VL: lados múltiplos de «fator» con los píxeles entre min y max. */
static void tamanho_alvo(int w, int h, int fator, long long min_px, long long max_px, int *tw, int *th)
{
    double H = h, W = w;
    long long hb = (long long)arred_par(H / fator) * fator, wb = (long long)arred_par(W / fator) * fator;
    if (hb * wb > max_px) {
        double beta = raiz_d(H * W / (double)max_px);
        hb = (long long)suelo(H / beta / fator) * fator;
        wb = (long long)suelo(W / beta / fator) * fator;
    } else if (hb * wb < min_px) {
        double beta = raiz_d((double)min_px / (H * W));
        hb = (long long)techo(H * beta / fator) * fator;
        wb = (long long)techo(W * beta / fator) * fator;
    }
    *tw = wb < fator ? fator : (int)wb;
    *th = hb < fator ? fator : (int)hb;
}

/* Filtro cúbico de Keys con a = -0,5 (el de Pillow y el de torchvision con antialias). */
static double cubico(double x)
{
    const double a = -0.5;
    if (x < 0) x = -x;
    if (x < 1) return ((a + 2) * x - (a + 3)) * x * x + 1;
    if (x < 2) return (((x - 5) * x + 8) * x - 4) * a;
    return 0;
}

#define PREC 22
/* Pesos de un eje en punto fijo: para cada salida, el primer píxel de entrada, cuántos y sus pesos
 * (redondeados a 16 bits con los bits fraccionarios que quepan, como torchvision). -> tamaño del núcleo */
static int pesos_eixo(int ent, int sai, int *ini, int *cnt, int32_t **pesos)
{
    double esc = (double)ent / sai, fesc = esc < 1 ? 1 : esc;
    double sup = 2.0 * fesc;
    int ks = (int)techo(sup) * 2 + 1;
    double *pw = llm_aloca((size_t)sai * ks * sizeof(double));
    int32_t *w = llm_aloca((size_t)sai * ks * sizeof(int32_t));
    if (!pw || !w) { llm_libera(pw); llm_libera(w); return -1; }
    double maxw = 0;
    for (int o = 0; o < sai; o++) {
        double cen = (o + 0.5) * esc, soma = 0;
        int x0 = (int)(cen - sup + 0.5), x1 = (int)(cen + sup + 0.5);
        if (x0 < 0) x0 = 0;
        if (x1 > ent) x1 = ent;
        int n = x1 - x0;
        for (int x = 0; x < n; x++) { double p = cubico((x + x0 - cen + 0.5) / fesc); pw[(size_t)o * ks + x] = p; soma += p; }
        for (int x = 0; x < n; x++) if (soma != 0) pw[(size_t)o * ks + x] /= soma;
        for (int x = n; x < ks; x++) pw[(size_t)o * ks + x] = 0;
        for (int x = 0; x < ks; x++) if (pw[(size_t)o * ks + x] > maxw) maxw = pw[(size_t)o * ks + x];
        ini[o] = x0; cnt[o] = n;
    }
    int prec = 0;
    for (; prec < PREC; prec++) if ((int)(0.5 + maxw * (double)(1 << (prec + 1))) >= (1 << 15)) break;
    double fx = (double)(1 << prec);
    for (size_t i = 0; i < (size_t)sai * ks; i++) {
        double r = pw[i] * fx + (pw[i] < 0 ? -0.5 : 0.5);
        w[i] = (int32_t)r * (1 << (PREC - prec));
    }
    llm_libera(pw);
    *pesos = w;
    return ks;
}

static uint8_t corta8(int32_t v) { v >>= PREC; return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v; }

/* Reescala RGB8 (bicúbico con antialias, primero en horizontal y después en vertical). */
static uint8_t *reescalar(const uint8_t *src, int w, int h, int tw, int th)
{
    uint8_t *tmp = NULL, *dst = NULL;
    const uint8_t *cur = src;
    int cw = w;
    if (tw != w) {
        int *ini = llm_aloca((size_t)tw * sizeof(int)), *cnt = llm_aloca((size_t)tw * sizeof(int));
        int32_t *pw = NULL;
        int ks = ini && cnt ? pesos_eixo(w, tw, ini, cnt, &pw) : -1;
        tmp = ks > 0 ? llm_aloca((size_t)tw * h * 3) : NULL;
        if (tmp)
            for (int y = 0; y < h; y++)
                for (int o = 0; o < tw; o++) {
                    int32_t s0 = 1 << (PREC - 1), s1 = s0, s2 = s0;
                    const uint8_t *p = src + ((size_t)y * w + ini[o]) * 3;
                    const int32_t *k = pw + (size_t)o * ks;
                    for (int x = 0; x < cnt[o]; x++, p += 3) { s0 += p[0] * k[x]; s1 += p[1] * k[x]; s2 += p[2] * k[x]; }
                    uint8_t *d = tmp + ((size_t)y * tw + o) * 3;
                    d[0] = corta8(s0); d[1] = corta8(s1); d[2] = corta8(s2);
                }
        llm_libera(ini); llm_libera(cnt); llm_libera(pw);
        if (!tmp) return NULL;
        cur = tmp; cw = tw;
    }
    if (th != h) {
        int *ini = llm_aloca((size_t)th * sizeof(int)), *cnt = llm_aloca((size_t)th * sizeof(int));
        int32_t *pw = NULL;
        int ks = ini && cnt ? pesos_eixo(h, th, ini, cnt, &pw) : -1;
        dst = ks > 0 ? llm_aloca((size_t)cw * th * 3) : NULL;
        if (dst)
            for (int o = 0; o < th; o++)
                for (int i = 0; i < cw * 3; i++) {
                    int32_t s = 1 << (PREC - 1);
                    const int32_t *k = pw + (size_t)o * ks;
                    for (int y = 0; y < cnt[o]; y++) s += cur[(size_t)(ini[o] + y) * cw * 3 + i] * k[y];
                    dst[(size_t)o * cw * 3 + i] = corta8(s);
                }
        llm_libera(ini); llm_libera(cnt); llm_libera(pw);
        llm_libera(tmp);
        return dst;
    }
    if (tmp) return tmp;
    dst = llm_aloca((size_t)w * h * 3);
    if (dst) llm_memcpy(dst, src, (size_t)w * h * 3);
    return dst;
}

static void rms(float *o, const float *x, const float *w, int n, float eps)
{
    double ss = 0;
    for (int i = 0; i < n; i++) ss += (double)x[i] * x[i];
    float esc = 1.0f / llm_sqrtf((float)(ss / n) + eps);
    for (int i = 0; i < n; i++) o[i] = x[i] * esc * w[i];
}

static void layernorm(float *o, const float *x, const float *w, const float *b, int n, float eps)
{
    double m = 0, v = 0;
    for (int i = 0; i < n; i++) m += x[i];
    m /= n;
    for (int i = 0; i < n; i++) { double d = x[i] - m; v += d * d; }
    v /= n;
    float esc = 1.0f / llm_sqrtf((float)v + eps);
    for (int i = 0; i < n; i++) o[i] = (float)(x[i] - m) * esc * w[i] + b[i];
}

static float silu(float x) { return x / (1.0f + llm_expf(-x)); }

/* erf con la aproximación 7.1.26 de Abramowitz y Stegun (error < 1,5e-7). */
static float erf_a(float x)
{
    float s = x < 0 ? -1.0f : 1.0f;
    if (x < 0) x = -x;
    float t = 1.0f / (1.0f + 0.3275911f * x);
    float y = 1.0f - (((((1.061405429f * t - 1.453152027f) * t) + 1.421413741f) * t - 0.284496736f) * t + 0.254829592f) * t * llm_expf(-x * x);
    return s * y;
}
static float gelu_erf(float x) { return 0.5f * x * (1.0f + erf_a(x * 0.70710678118654752f)); }

/* Y[N][n_out] = W X[N][n_in] (+ sesgo). Q8_0: entrada en Q8_0 y producto de enteros. */
typedef struct {
    const gguf_tensor_t *W;
    const float *x, *b;
    const bloco_q8_0 *xq;
    float *y;
    int N, n_in, n_out;
} vmm_t;

static void vmm_trozo(void *ctx, int ini, int fim)
{
    const vmm_t *t = ctx;
    const size_t linha = gguf_tamanho_linha(t->W->tipo, (uint64_t)t->n_in);
    const int nb = t->n_in / QK8_0;
    float linha_f[t->W->tipo == T_F16 ? t->n_in : 1];   /* en la pila: sin pedir memoria desde los hilos */
    float *wf = t->W->tipo == T_F16 ? linha_f : NULL;
    for (int r = ini; r < fim; r++) {
        const uint8_t *w = (const uint8_t *)t->W->dados + (size_t)r * linha;
        float bias = t->b ? t->b[r] : 0;
        if (t->W->tipo == T_Q8_0) {
            for (int n = 0; n < t->N; n++) t->y[(size_t)n * t->n_out + r] = llm_dot_q8_0(w, t->xq + (size_t)n * nb, t->n_in) + bias;
        } else {
            const float *f = (const float *)w;
            if (wf) { llm_decuant_linha(T_F16, w, wf, t->n_in); f = wf; }
            for (int n = 0; n < t->N; n++) {
                const float *x = t->x + (size_t)n * t->n_in;
                float s = 0;
                for (int i = 0; i < t->n_in; i++) s += f[i] * x[i];
                t->y[(size_t)n * t->n_out + r] = s + bias;
            }
        }
    }
}

static int vmm(const gguf_tensor_t *W, const gguf_tensor_t *B, const float *x, float *y, int N)
{
    vmm_t t = { W, x, B ? (const float *)B->dados : NULL, NULL, y, N, (int)W->ne[0], (int)W->ne[1] };
    bloco_q8_0 *xq = NULL;
    if (W->tipo == T_Q8_0) {
        const int nb = t.n_in / QK8_0;
        xq = llm_aloca((size_t)N * nb * sizeof(bloco_q8_0));
        if (!xq) return -1;
        for (int n = 0; n < N; n++) llm_quant_q8_0(x + (size_t)n * t.n_in, xq + (size_t)n * nb, t.n_in);
        t.xq = xq;
    }
    llm_hilos_repartir(vmm_trozo, &t, t.n_out);
    llm_libera(xq);
    return 0;
}

/* La atención completa de una capa: cabezas repartidas entre los hilos. */
typedef struct {
    const struct llm_visao *v;
    const bloco_q8_0 *qq, *kq;   /* [N][H*D/32] y [N][Hkv*D/32] */
    const float *vv;             /* [N][Hkv*D] */
    float *o;                    /* [N][H*D] */
    int N;
} vatt_t;

static void vatt_cabecas(void *ctx, int ini, int fim)
{
    const vatt_t *a = ctx;
    const struct llm_visao *v = a->v;
    const int D = v->D, N = a->N, nb = D / QK8_0, grupo = v->H / v->Hkv;
    const int nbq = v->H * nb, nbk = v->Hkv * nb, KV = v->Hkv * D;
    const float esc = 1.0f / llm_sqrtf((float)D);
    float p[N];   /* N <= 4096: 16 KB de pila; sin pedir memoria desde los hilos */
    for (int h = ini; h < fim; h++) {
        const int kh = h / grupo;
        for (int i = 0; i < N; i++) {
            const bloco_q8_0 *q = a->qq + (size_t)i * nbq + (size_t)h * nb;
            float mx = -3.4e38f, soma = 0;
            for (int j = 0; j < N; j++) {
                float s = llm_dot_q8_0(a->kq + (size_t)j * nbk + (size_t)kh * nb, q, D) * esc;
                p[j] = s;
                if (s > mx) mx = s;
            }
            for (int j = 0; j < N; j++) { p[j] = llm_expf(p[j] - mx); soma += p[j]; }
            float *o = a->o + (size_t)i * v->E + (size_t)h * D;
            llm_memset(o, 0, (size_t)D * 4);
            for (int j = 0; j < N; j++) {
                const float *vj = a->vv + (size_t)j * KV + (size_t)kh * D;
                float pj = p[j] / soma;
                for (int d = 0; d < D; d++) o[d] += pj * vj[d];
            }
        }
    }
}

/* RoPE 2D de LFM3-VL: pareja (i, i+D/2); las D/4 primeras giran con la fila, las D/4 siguientes
 * con la columna; ángulo = posición * base^(-2j/(D/2)), con multiplicaciones sucesivas (como ggml). */
static void rope2d(float *x, int D, int fila, int col, float base)
{
    const int n_dims = D / 2, sec = D / 4;
    const float esc = llm_powf(base, -2.0f / (float)n_dims);
    float tf = (float)fila, tc = (float)col;
    for (int i = 0; i < n_dims; i++) {
        if (i == sec) tc = (float)col;   /* cada sección empieza de nuevo */
        float th = i < sec ? tf : tc, s, c;
        llm_sincosf(th, &s, &c);
        float x0 = x[i], x1 = x[i + n_dims];
        x[i] = x0 * c - x1 * s;
        x[i + n_dims] = x0 * s + x1 * c;
        tf *= esc; tc *= esc;
    }
}

/* Codifica una imagen RGB8. *emb ([n][saida]) es memoria nueva. -> vectores (<0 error) */
static int codificar(struct llm_visao *v, const uint8_t *rgb, int w, int h, float **emb, char *erro, size_t max)
{
    const int P = v->P, E = v->E, F = v->F, fator = 2 * P;
    int tw, th;
    tamanho_alvo(w, h, fator, v->min_px, v->max_px, &tw, &th);
    const int px = tw / P, py = th / P, N = px * py, T = N / 4, K = 3 * P * P;
    uint8_t *img = reescalar(rgb, w, h, tw, th);
    float *x = llm_aloca((size_t)N * E * 4), *xb = llm_aloca((size_t)N * E * 4), *hb = llm_aloca((size_t)N * 3 * E * 4);
    float *f1 = llm_aloca((size_t)N * F * 4), *f2 = llm_aloca((size_t)N * F * 4), *ent = llm_aloca((size_t)K * 4);
    float *pix = llm_aloca((size_t)3 * tw * th * 4);
    if (!img || !x || !xb || !hb || !f1 || !f2 || !ent || !pix) {
        llm_libera(img); llm_libera(x); llm_libera(xb); llm_libera(hb); llm_libera(f1); llm_libera(f2); llm_libera(ent); llm_libera(pix);
        llm_formatar(erro, max, "no hay memoria para la imagen de %dx%d", tw, th);
        return -1;
    }
    /* normalizar: x/255, después (x - media)/desvío; [c][y][x], redondeado a f16 como el im2col de ggml */
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < tw * th; i++) {
            float f = (float)img[(size_t)i * 3 + c] / 255.0f;
            f = (f - v->media[c]) / v->desvio[c];
            pix[(size_t)c * tw * th + i] = llm_f16_f32(llm_f32_f16(f));
        }
    llm_libera(img);
    /* parches: núcleo [E][c][ky][kx], los dos cuadros con la misma imagen y sumados */
    for (int yy = 0; yy < py; yy++)
        for (int xx = 0; xx < px; xx++) {
            for (int c = 0; c < 3; c++)
                for (int ky = 0; ky < P; ky++)
                    for (int kx = 0; kx < P; kx++)
                        ent[(c * P + ky) * P + kx] = pix[(size_t)c * tw * th + (size_t)(yy * P + ky) * tw + xx * P + kx];
            float *o = x + (size_t)(yy * px + xx) * E;
            for (int e = 0; e < E; e++) {
                const float *k0 = v->nucleo + (size_t)e * K, *k1 = k0 + (size_t)E * K;
                float s0 = 0, s1 = 0;
                for (int i = 0; i < K; i++) { s0 += k0[i] * ent[i]; s1 += k1[i] * ent[i]; }
                o[e] = s0 + s1;
            }
        }
    llm_libera(pix); llm_libera(ent);
    for (int n = 0; n < N; n++) rms(x + (size_t)n * E, x + (size_t)n * E, v->emb_norma->dados, E, v->eps);

    int r = 0;
    for (int il = 0; il < v->L && !r; il++) {
        const vis_camada_t *c = &v->c[il];
        for (int n = 0; n < N; n++) rms(xb + (size_t)n * E, x + (size_t)n * E, c->ln1->dados, E, v->eps);
        if (c->conv_in) {
            r |= vmm(c->conv_in, NULL, xb, hb, N);   /* [a | m | b] */
            for (int n = 0; n < N; n++) {             /* g = a*b, en el primer tercio */
                float *p = hb + (size_t)n * 3 * E;
                for (int e = 0; e < E; e++) p[e] *= p[2 * E + e];
            }
            const float *k = c->conv_dw->dados;   /* [E][ky][kx] */
            for (int yy = 0; yy < py; yy++)
                for (int xx = 0; xx < px; xx++) {
                    float *o = xb + (size_t)(yy * px + xx) * E;
                    const float *m = hb + (size_t)(yy * px + xx) * 3 * E + E;
                    for (int e = 0; e < E; e++) {
                        float s = 0;
                        for (int ky = 0; ky < 3; ky++) {
                            int sy = yy + ky - 1;
                            if (sy < 0 || sy >= py) continue;
                            for (int kx = 0; kx < 3; kx++) {
                                int sx = xx + kx - 1;
                                if (sx < 0 || sx >= px) continue;
                                s += k[(size_t)e * 9 + ky * 3 + kx] * hb[(size_t)(sy * px + sx) * 3 * E + e];
                            }
                        }
                        o[e] = m[e] * s;
                    }
                }
            r |= vmm(c->conv_out, NULL, xb, hb, N);
        } else {
            const int D = v->D, KV = v->Hkv * D, nb = D / QK8_0;
            float *q = hb, *kk = hb + (size_t)N * E, *vv = hb + (size_t)N * 2 * E;
            r |= vmm(c->q, NULL, xb, q, N);
            r |= vmm(c->k, NULL, xb, kk, N);
            r |= vmm(c->v, NULL, xb, vv, N);
            bloco_q8_0 *qq = llm_aloca((size_t)N * v->H * nb * sizeof(bloco_q8_0));
            bloco_q8_0 *kq = llm_aloca((size_t)N * v->Hkv * nb * sizeof(bloco_q8_0));
            if (!qq || !kq) { llm_libera(qq); llm_libera(kq); r = -1; break; }
            for (int n = 0; n < N; n++) {
                const int fila = n / px, col = n % px;
                for (int hh = 0; hh < v->H; hh++) {
                    float *z = q + (size_t)n * E + (size_t)hh * D;
                    rms(z, z, c->qn->dados, D, v->eps);
                    rope2d(z, D, fila, col, v->rope_base);
                }
                for (int hh = 0; hh < v->Hkv; hh++) {
                    float *z = kk + (size_t)n * KV + (size_t)hh * D;
                    rms(z, z, c->kn->dados, D, v->eps);
                    rope2d(z, D, fila, col, v->rope_base);
                }
                llm_quant_q8_0(q + (size_t)n * E, qq + (size_t)n * v->H * nb, E);
                llm_quant_q8_0(kk + (size_t)n * KV, kq + (size_t)n * v->Hkv * nb, KV);
            }
            /* q y k ya están en Q8_0: la salida va a donde estaba q */
            vatt_t a = { v, qq, kq, vv, xb, N };
            llm_hilos_repartir(vatt_cabecas, &a, v->H);
            llm_libera(qq); llm_libera(kq);
            r |= vmm(c->o, NULL, xb, hb, N);
        }
        for (size_t i = 0; i < (size_t)N * E; i++) x[i] += hb[i];
        for (int n = 0; n < N; n++) rms(xb + (size_t)n * E, x + (size_t)n * E, c->ln2->dados, E, v->eps);
        r |= vmm(c->gate, NULL, xb, f1, N);
        r |= vmm(c->up, NULL, xb, f2, N);
        for (size_t i = 0; i < (size_t)N * F; i++) f1[i] = silu(f1[i]) * f2[i];
        r |= vmm(c->down, NULL, f1, hb, N);
        for (size_t i = 0; i < (size_t)N * E; i++) x[i] += hb[i];
    }
    llm_libera(f1); llm_libera(f2);
    if (r) { llm_libera(x); llm_libera(xb); llm_libera(hb); llm_formatar(erro, max, "sin memoria para la imagen"); return -1; }
    /* LayerNorm y los 2x2 parches juntos: [p(2i,2j), p(2i,2j+1), p(2i+1,2j), p(2i+1,2j+1)] */
    for (int n = 0; n < N; n++) layernorm(xb + (size_t)n * E, x + (size_t)n * E, v->mm_nw->dados, v->mm_nb->dados, E, 1e-6f);
    float *m4 = x;   /* N*E = T*4E: cabe en x */
    for (int by = 0; by < py / 2; by++)
        for (int bx = 0; bx < px / 2; bx++) {
            float *o = m4 + (size_t)(by * (px / 2) + bx) * 4 * E;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++)
                    llm_memcpy(o + (size_t)(dy * 2 + dx) * E, xb + (size_t)((2 * by + dy) * px + 2 * bx + dx) * E, (size_t)E * 4);
        }
    const int M = (int)v->mm0->ne[1];
    float *z = llm_aloca((size_t)T * M * 4), *out = llm_aloca((size_t)T * v->saida * 4);
    if (!z || !out || vmm(v->mm0, v->mm0b, m4, z, T)) {
        llm_libera(z); llm_libera(out); llm_libera(x); llm_libera(xb); llm_libera(hb);
        llm_formatar(erro, max, "sin memoria para la imagen");
        return -1;
    }
    for (size_t i = 0; i < (size_t)T * M; i++) z[i] = gelu_erf(z[i]);
    r = vmm(v->mm2, v->mm2b, z, out, T);
    llm_libera(z); llm_libera(x); llm_libera(xb); llm_libera(hb);
    if (r) { llm_libera(out); llm_formatar(erro, max, "sin memoria para la imagen"); return -1; }
    *emb = out;
    return T;
}

/* ------------------------------------------------------------ con la sesión */

bool llm_visao_ativa(const llm_sessao_t *s) { return s->visao != NULL; }

/* Una suma de control de los vectores (para comparar SaguiOS con el PC en las pruebas). */
static long long soma_emb(const float *e, size_t n)
{
    double s = 0;
    for (size_t i = 0; i < n; i++) s += (double)e[i] * (double)((i % 7) + 1);
    return (long long)(s * 1000);
}

int llm_imagem_vetores(llm_sessao_t *s, const uint8_t *dados, size_t n, float **emb, int *largura, int *altura, char *erro, size_t max)
{
    if (!s->visao) { llm_formatar(erro, max, "no hay proyector de imágenes (mmproj)"); return -1; }
    uint8_t *rgb = NULL;
    int w = 0, h = 0;
    if (llm_imagem_decodificar(dados, n, &rgb, &w, &h, erro, max)) return -1;
    unsigned t0 = llm_ms();
    int nt = codificar(s->visao, rgb, w, h, emb, erro, max);
    llm_libera(rgb);
    if (nt > 0) {
        long long sm = soma_emb(*emb, (size_t)nt * s->visao->saida);
        int neg = sm < 0;
        if (neg) sm = -sm;   /* en dos trozos: el formato de la libsagui no tiene %lld */
        llm_eprintf("[llm] imagen de %dx%d: %d vectores en %u ms; SUMA_IMAGEN: %s%d.%09d\n", w, h, nt, llm_ms() - t0,
                    neg ? "-" : "", (int)(sm / 1000000000), (int)(sm % 1000000000));
        if (largura) *largura = w;
        if (altura) *altura = h;
    }
    return nt;
}

int llm_imagem_alimentar(llm_sessao_t *s, const uint8_t *dados, size_t n, char *erro, size_t max)
{
    float *emb = NULL;
    int nt = llm_imagem_vetores(s, dados, n, &emb, NULL, NULL, erro, max);
    if (nt < 0) return -1;
    int r = llm_alimentar_emb(s, emb, nt);
    llm_libera(emb);
    if (r) { llm_formatar(erro, max, "la imagen no cabe en el contexto de la IA local"); return -1; }
    return nt;
}

int llm_alimentar_emb(llm_sessao_t *s, const float *emb, int n)
{
    if (s->e.kv.pos + n > s->e.kv.ctx) return -1;
    unsigned t0 = llm_ms();
    if (llm_avaliar_emb(&s->e, emb, n)) return -1;
    s->ms_leitura += llm_ms() - t0;
    s->n_leitura += n;
    /* en el historial, -1: no es un token (la penalización lo salta y la caché no se reutiliza) */
    for (int i = 0; i < n && s->hist_n < s->hist_max; i++) s->hist[s->hist_n++] = -1;
    return 0;
}
