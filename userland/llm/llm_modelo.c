// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_modelo.c - la red LFM3 (D380): carga, caché K/V en Q8_0 y paso hacia delante.
 *
 * Un bloque LFM3 (la referencia es llama.cpp, src/models/lfm3.cpp y lfm-base.cpp):
 *
 *   h = norma(x)                                  (RMS, «operator_norm»)
 *   si la capa es de convolución corta:
 *       b, c, x' = in_proj(h)                     (tres trozos de n_embd)
 *       y = c * conv1d_causal(b * x')             (núcleo de l_conv = 7 por canal)
 *       h = out_proj(y)
 *   si es de atención (GQA, 16 cabezas de 128, 4 de K/V):
 *       q, k, v = wq h, wk h, wv h;  q y k con norma RMS por cabeza;  RoPE NEOX
 *       (base 1e6 con los factores de rope_freqs en las capas completas, 1e4 en las de
 *       ventana de 256 posiciones);  o = atención(q, k, v);
 *       o *= sigmoide(wgate h) por cabeza;  h = wo o
 *   h = norma(h)  (post_attention_norm, si está)
 *   x = x + h
 *   h = ffn_down(silu(ffn_gate(norma(x))) * ffn_up(norma(x)));  h = norma(h) (post_ffw_norm)
 *   x = x + h
 * y al final logits = salida(norma(x)), con la salida atada al embedding si no hay otra.
 *
 * Los tokens se pasan por lotes (LLM_LOTE): cada fila de pesos se lee una vez por lote
 * y se multiplica por todas las entradas, que es lo que más acelera la lectura del texto
 * en las CPU con poca caché. La atención y la convolución van token a token dentro del
 * lote, en orden (son causales). */
#include "llm.h"

#define ERRO(...) do { llm_formatar(erro, max, __VA_ARGS__); return -1; } while (0)

static bool tipo_matriz(uint32_t t)
{
    return t == T_F32 || t == T_F16 || t == T_Q8_0 || t == T_Q4_0 || t == T_Q4_K || t == T_Q6_K;
}

static const gguf_tensor_t *tensor_camada(const gguf_t *g, int il, const char *nome)
{
    char n[96];
    llm_formatar(n, sizeof n, "blk.%d.%s.weight", il, nome);
    return gguf_tensor(g, n);
}

/* Comprueba un tensor: que exista (si hace falta), sus dimensiones y su tipo. */
static int conferir(const gguf_tensor_t *t, const char *nome, int il, uint64_t ne0, uint64_t ne1, bool matriz,
                    char *erro, size_t max)
{
    if (!t) ERRO("falta el tensor blk.%d.%s", il, nome);
    if (t->ne[2] != 1 || t->ne[3] != 1) ERRO("tensor blk.%d.%s: más de dos dimensiones", il, nome);
    if (t->ne[0] != ne0 || t->ne[1] != ne1)
        ERRO("tensor blk.%d.%s: %ux%u, se esperaba %ux%u", il, nome, (unsigned)t->ne[0], (unsigned)t->ne[1],
             (unsigned)ne0, (unsigned)ne1);
    if (matriz ? !tipo_matriz(t->tipo) : t->tipo != T_F32)
        ERRO("tensor blk.%d.%s: tipo %s no admitido", il, nome, gguf_nome_tipo(t->tipo));
    if ((ne0 % (t->tipo == T_Q4_K || t->tipo == T_Q6_K ? QK_K : QK8_0)) && t->tipo != T_F32 && t->tipo != T_F16)
        ERRO("tensor blk.%d.%s: filas que no son múltiplo del bloque", il, nome);
    return 0;
}

static bool chave(const gguf_t *g, const char *arq, const char *nome, uint32_t *v)
{
    char n[96];
    llm_formatar(n, sizeof n, "%s.%s", arq, nome);
    return gguf_u32(g, n, v);
}

static bool chave_f(const gguf_t *g, const char *arq, const char *nome, float *v)
{
    char n[96];
    llm_formatar(n, sizeof n, "%s.%s", arq, nome);
    return gguf_f32(g, n, v);
}

/* Un valor por capa: un vector o un número para todas. */
static bool por_camada(const gguf_t *g, const char *arq, const char *nome, int il, uint32_t *v)
{
    char n[96];
    llm_formatar(n, sizeof n, "%s.%s", arq, nome);
    const gguf_kv_t *kv = gguf_kv(g, n);
    if (!kv) return false;
    if (kv->tipo == GGUF_ARRAY) return gguf_arr_u32(g, n, (uint64_t)il, v);
    return gguf_u32(g, n, v);
}

int llm_modelo_abrir(llm_modelo_t *m, const char *caminho, char *erro, size_t max)
{
    llm_memset(m, 0, sizeof *m);
    gguf_t *g = &m->g;
    if (gguf_abrir(g, caminho)) {
        llm_formatar(erro, max, "%s", g->erro);
        return -1;
    }
    llm_texto_t t;
    if (!gguf_texto(g, "general.architecture", &t) || t.n >= sizeof m->arquitetura) ERRO("el modelo no dice su arquitectura");
    llm_memcpy(m->arquitetura, t.s, t.n);
    if (gguf_texto(g, "general.name", &t)) {
        unsigned n = t.n < sizeof m->nome - 1 ? t.n : sizeof m->nome - 1;
        llm_memcpy(m->nome, t.s, n);
        for (unsigned i = 0; i < n; i++)   /* el nombre sale en el terminal: sin caracteres de control */
            if ((uint8_t)m->nome[i] < 0x20 || ((uint8_t)m->nome[i] >= 0x7F && (uint8_t)m->nome[i] < 0xA0)) m->nome[i] = '?';
    }
    gguf_u32(g, "general.file_type", &m->tipo_ficheiro);
    if (llm_strcmp(m->arquitetura, "lfm3"))
        ERRO("arquitectura «%s» no admitida (este motor ejecuta lfm3)", m->arquitetura);
    const char *a = m->arquitetura;
    uint32_t u;
    if (!chave(g, a, "block_count", &u) || u < 1 || u > LLM_MAX_CAMADAS) ERRO("block_count no válido");
    m->n_camadas = (int)u;
    if (!chave(g, a, "embedding_length", &u) || u < 32 || u > 16384) ERRO("embedding_length no válido");
    m->n_embd = (int)u;
    if (!chave(g, a, "feed_forward_length", &u) || u < 32 || u > 65536) ERRO("feed_forward_length no válido");
    m->n_ff = (int)u;
    if (!chave(g, a, "attention.head_count", &u) || u < 1 || u > 256) ERRO("attention.head_count no válido");
    m->n_cab = (int)u;
    if (!chave(g, a, "attention.key_length", &u)) u = (uint32_t)(m->n_embd / m->n_cab);
    if (u < 32 || u > 512 || u % 32) ERRO("key_length no válido (%u)", u);
    m->dim_cab = (int)u;
    uint32_t vl;
    if (chave(g, a, "attention.value_length", &vl) && vl != u) ERRO("value_length distinto de key_length");
    if (chave(g, a, "rope.dimension_count", &vl) && (int)vl != m->dim_cab) ERRO("RoPE parcial no admitido");
    if (!chave(g, a, "shortconv.l_cache", &u) || u < 2 || u > 16) ERRO("shortconv.l_cache no válido");
    m->l_conv = (int)u;
    if (!chave(g, a, "attention.sliding_window", &u)) u = 0;
    if (u > (1u << 20)) ERRO("sliding_window no válido (%u)", u);
    m->n_janela = (int)u;
    if (!chave(g, a, "context_length", &u)) u = 4096;
    if (u < 1 || u > (1u << 20)) ERRO("context_length no válido (%u)", u);
    m->ctx_treino = (int)u;
    if (!chave_f(g, a, "attention.layer_norm_rms_epsilon", &m->eps)) m->eps = 1e-5f;
    if (!chave_f(g, a, "rope.freq_base", &m->rope_base)) m->rope_base = 10000.0f;
    if (!chave_f(g, a, "rope.freq_base_swa", &m->rope_base_janela)) m->rope_base_janela = m->rope_base;

    m->emb = gguf_tensor(g, "token_embd.weight");
    if (!m->emb || m->emb->ne[0] != (uint64_t)m->n_embd || !tipo_matriz(m->emb->tipo)) ERRO("token_embd no válido");
    if (m->emb->ne[1] < 1 || m->emb->ne[1] > (1u << 21)) ERRO("vocabulario del embedding no válido");
    if (m->emb->ne[2] != 1 || m->emb->ne[3] != 1) ERRO("token_embd con más de dos dimensiones");
    m->n_vocab = (int)m->emb->ne[1];
    m->saida = gguf_tensor(g, "output.weight");
    if (!m->saida) m->saida = m->emb;
    if (m->saida->ne[0] != (uint64_t)m->n_embd || m->saida->ne[1] != (uint64_t)m->n_vocab) ERRO("output no válido");
    m->norma_saida = gguf_tensor(g, "output_norm.weight");
    if (!m->norma_saida || m->norma_saida->ne[0] != (uint64_t)m->n_embd || m->norma_saida->tipo != T_F32)
        ERRO("output_norm no válido");
    m->rope_freqs = gguf_tensor(g, "rope_freqs.weight");
    if (m->rope_freqs && (m->rope_freqs->ne[0] != (uint64_t)(m->dim_cab / 2) || m->rope_freqs->tipo != T_F32))
        ERRO("rope_freqs no válido");

    const int E = m->n_embd, F = m->n_ff, D = m->dim_cab;
    bool algum_janela = false;
    for (int il = 0; il < m->n_camadas; il++) {
        llm_camada_t *c = &m->c[il];
        uint32_t nkv = 0, jan = 0;
        if (!por_camada(g, a, "attention.head_count_kv", il, &nkv)) ERRO("falta attention.head_count_kv");
        if (nkv > (uint32_t)m->n_cab) ERRO("capa %d: %u cabezas K/V (más que las %d de Q)", il, nkv, m->n_cab);
        por_camada(g, a, "attention.sliding_window_pattern", il, &jan);
        c->n_kv = (int)nkv;
        c->atencao = nkv > 0;
        c->janela = c->atencao && jan;
        if (c->janela) algum_janela = true;
        if (c->atencao && (m->n_cab % c->n_kv)) ERRO("capa %d: %d cabezas K/V no dividen %d", il, c->n_kv, m->n_cab);
        c->norma_op = tensor_camada(g, il, "attn_norm");
        c->norma_ffn = tensor_camada(g, il, "ffn_norm");
        c->norma_op_pos = tensor_camada(g, il, "post_attention_norm");
        c->norma_ffn_pos = tensor_camada(g, il, "post_ffw_norm");
        if (conferir(c->norma_op, "attn_norm", il, E, 1, false, erro, max)) return -1;
        if (conferir(c->norma_ffn, "ffn_norm", il, E, 1, false, erro, max)) return -1;
        if ((c->norma_op_pos == NULL) != (c->norma_ffn_pos == NULL)) ERRO("capa %d: falta una de las normas posteriores", il);
        if (c->norma_op_pos && conferir(c->norma_op_pos, "post_attention_norm", il, E, 1, false, erro, max)) return -1;
        if (c->norma_ffn_pos && conferir(c->norma_ffn_pos, "post_ffw_norm", il, E, 1, false, erro, max)) return -1;
        c->ffn_gate = tensor_camada(g, il, "ffn_gate");
        c->ffn_up = tensor_camada(g, il, "ffn_up");
        c->ffn_down = tensor_camada(g, il, "ffn_down");
        if (conferir(c->ffn_gate, "ffn_gate", il, E, F, true, erro, max)) return -1;
        if (conferir(c->ffn_up, "ffn_up", il, E, F, true, erro, max)) return -1;
        if (conferir(c->ffn_down, "ffn_down", il, F, E, true, erro, max)) return -1;
        if (c->atencao) {
            const int Q = m->n_cab * D, KV = c->n_kv * D;
            c->wq = tensor_camada(g, il, "attn_q"); c->wk = tensor_camada(g, il, "attn_k");
            c->wv = tensor_camada(g, il, "attn_v"); c->wo = tensor_camada(g, il, "attn_output");
            c->wgate = tensor_camada(g, il, "attn_gate");
            c->q_norma = tensor_camada(g, il, "attn_q_norm"); c->k_norma = tensor_camada(g, il, "attn_k_norm");
            if (!c->wq && tensor_camada(g, il, "attn_qkv")) ERRO("capa %d: Q/K/V juntos (attn_qkv) aún no admitidos", il);
            if (conferir(c->wq, "attn_q", il, E, Q, true, erro, max)) return -1;
            if (conferir(c->wk, "attn_k", il, E, KV, true, erro, max)) return -1;
            if (conferir(c->wv, "attn_v", il, E, KV, true, erro, max)) return -1;
            if (conferir(c->wo, "attn_output", il, Q, E, true, erro, max)) return -1;
            if (conferir(c->wgate, "attn_gate", il, E, m->n_cab, true, erro, max)) return -1;
            if (conferir(c->q_norma, "attn_q_norm", il, D, 1, false, erro, max)) return -1;
            if (conferir(c->k_norma, "attn_k_norm", il, D, 1, false, erro, max)) return -1;
            c->idx_cache = m->n_atencao++;
            if (c->janela) m->n_janelas++; else m->n_completas++;
        } else {
            c->conv = tensor_camada(g, il, "shortconv.conv");
            c->conv_in = tensor_camada(g, il, "shortconv.in_proj");
            c->conv_out = tensor_camada(g, il, "shortconv.out_proj");
            if (conferir(c->conv, "shortconv.conv", il, (uint64_t)m->l_conv, E, false, erro, max)) return -1;
            if (conferir(c->conv_in, "shortconv.in_proj", il, E, 3ull * E, true, erro, max)) return -1;
            if (conferir(c->conv_out, "shortconv.out_proj", il, E, E, true, erro, max)) return -1;
            c->idx_conv = m->n_conv++;
        }
    }
    if (!algum_janela) ERRO("LFM3 necesita capas de ventana deslizante");
    if (m->n_janelas && m->n_janela < 1) ERRO("falta attention.sliding_window");
    return 0;
}

int llm_modelo_carregar(llm_modelo_t *m, void (*progresso)(unsigned pct))
{
    return gguf_carregar(&m->g, progresso);
}

void llm_modelo_fechar(llm_modelo_t *m) { gguf_fechar(&m->g); }

/* ------------------------------------------------------------ memoria */

static size_t bytes_kv_posicao(const llm_modelo_t *m, int n_kv) { return (size_t)n_kv * m->dim_cab / QK8_0 * sizeof(bloco_q8_0); }

unsigned long long llm_memoria_pesos(const llm_modelo_t *m) { return m->g.dados_n + m->g.cab_n; }

unsigned long long llm_memoria_cache(const llm_modelo_t *m, int ctx)
{
    unsigned long long t = 0;
    for (int il = 0; il < m->n_camadas; il++) {
        const llm_camada_t *c = &m->c[il];
        if (!c->atencao) { t += (unsigned long long)(m->l_conv - 1) * m->n_embd * 4; continue; }
        int pos = c->janela ? m->n_janela + LLM_LOTE : ctx;
        if (pos > ctx) pos = ctx;
        t += 2ull * (unsigned long long)pos * bytes_kv_posicao(m, c->n_kv);
    }
    return t;
}

/* La entrada cuantizada de una multiplicación: la mayor entrada (E, F o Q = n_cab * dim_cab), redondeada a
 * bloques de 256 y en el peor formato (Q8_K: 292 bytes por 256, más que Q8_0), por cada token del lote. */
static size_t xq_maior(const llm_modelo_t *m)
{
    size_t n = (size_t)m->n_embd;
    if ((size_t)m->n_ff > n) n = (size_t)m->n_ff;
    if ((size_t)m->n_cab * (size_t)m->dim_cab > n) n = (size_t)m->n_cab * (size_t)m->dim_cab;
    return (n + QK_K - 1) / QK_K * QK_K;
}
static size_t xq_bytes(const llm_modelo_t *m) { return (size_t)LLM_LOTE * xq_maior(m) / QK_K * sizeof(bloco_q8_K); }

/* Trabajo: los vectores del lote y los logits, la puntuación de la atención (hasta ctx). */
static unsigned long long trabalho(const llm_modelo_t *m, int ctx)
{
    const unsigned long long B = LLM_LOTE, E = (unsigned)m->n_embd, F = (unsigned)m->n_ff;
    const unsigned long long Q = (unsigned long long)m->n_cab * m->dim_cab;
    unsigned long long t = B * (3 * E + 2 * F + 2 * Q + 3 * E + 2 * Q + (unsigned)m->n_cab) * 4;
    t += (unsigned long long)m->n_vocab * 4 + (unsigned long long)m->n_cab * (ctx + 16) * 4;
    t += xq_bytes(m);
    t += B * (unsigned long long)m->dim_cab * 2 * 4 * 2;
    return t;
}

unsigned long long llm_memoria_trabalho(const llm_modelo_t *m, int ctx) { return trabalho(m, ctx); }

/* ------------------------------------------------------------ estado */

int llm_estado_criar(llm_estado_t *e, llm_modelo_t *m, int ctx)
{
    llm_memset(e, 0, sizeof *e);
    e->m = m;
    e->lote_max = LLM_LOTE;
    llm_cache_t *kv = &e->kv;
    kv->ctx = ctx;
    kv->mem_n = (size_t)llm_memoria_cache(m, ctx);
    kv->mem = llm_mapa(kv->mem_n);
    kv->k = llm_aloca_zero(sizeof(void *) * (size_t)(m->n_atencao + 1));
    kv->v = llm_aloca_zero(sizeof(void *) * (size_t)(m->n_atencao + 1));
    kv->posicoes = llm_aloca_zero(sizeof(int) * (size_t)(m->n_atencao + 1));
    if (!kv->mem || !kv->k || !kv->v || !kv->posicoes) { llm_estado_libera(e); return -1; }
    uint8_t *p = kv->mem;
    for (int il = 0; il < m->n_camadas; il++) {
        const llm_camada_t *c = &m->c[il];
        if (!c->atencao) continue;
        int pos = c->janela ? m->n_janela + LLM_LOTE : ctx;
        if (pos > ctx) pos = ctx;
        size_t n = (size_t)pos * bytes_kv_posicao(m, c->n_kv);
        kv->posicoes[c->idx_cache] = pos;
        kv->k[c->idx_cache] = (bloco_q8_0 *)p; p += n;
        kv->v[c->idx_cache] = (bloco_q8_0 *)p; p += n;
        if (c->n_kv > kv->n_kv_max) kv->n_kv_max = c->n_kv;
    }
    kv->conv_estado = (float *)p;

    const int B = LLM_LOTE, E = m->n_embd, F = m->n_ff, Q = m->n_cab * m->dim_cab, D = m->dim_cab;
    e->x = llm_aloca((size_t)B * E * 4);
    e->xb = llm_aloca((size_t)B * E * 4);
    e->xb2 = llm_aloca((size_t)B * E * 4);
    e->hb = llm_aloca((size_t)B * F * 4);
    e->hb2 = llm_aloca((size_t)B * F * 4);
    e->bcx = llm_aloca((size_t)B * 3 * E * 4);
    e->q = llm_aloca((size_t)B * Q * 4);
    e->k = llm_aloca((size_t)B * Q * 4);
    e->v = llm_aloca((size_t)B * Q * 4);
    e->att = llm_aloca((size_t)B * Q * 4);
    e->gate = llm_aloca((size_t)B * m->n_cab * 4);
    e->logits = llm_aloca((size_t)m->n_vocab * 4);
    e->pont = llm_aloca((size_t)m->n_cab * (ctx + 16) * 4);
    e->xq = llm_aloca(xq_bytes(m));
    e->qq = llm_aloca((size_t)D / QK8_0 * sizeof(bloco_q8_0) * (size_t)m->n_cab);
    e->rope_cos = llm_aloca((size_t)B * D * 4);   /* [b][completa: D/2 | ventana: D/2] */
    e->rope_sin = llm_aloca((size_t)B * D * 4);
    e->inv_freq_comp = llm_aloca((size_t)D * 4);
    e->inv_freq_jan = llm_aloca((size_t)D * 4);
    if (!e->x || !e->xb || !e->xb2 || !e->hb || !e->hb2 || !e->bcx || !e->q || !e->k || !e->v || !e->att || !e->gate ||
        !e->logits || !e->pont || !e->xq || !e->qq || !e->rope_cos || !e->rope_sin || !e->inv_freq_comp ||
        !e->inv_freq_jan) {
        llm_estado_libera(e);
        return -1;
    }
    e->bytes = kv->mem_n + trabalho(m, ctx);
    llm_estado_reiniciar(e);
    return 0;
}

void llm_estado_libera(llm_estado_t *e)
{
    if (e->kv.mem) llm_mapa_libera(e->kv.mem, e->kv.mem_n);
    llm_libera(e->kv.k); llm_libera(e->kv.v); llm_libera(e->kv.posicoes);
    void *v[] = { e->x, e->xb, e->xb2, e->hb, e->hb2, e->bcx, e->q, e->k, e->v, e->att, e->gate, e->logits,
                  e->pont, e->xq, e->qq, e->rope_cos, e->rope_sin, e->inv_freq_comp, e->inv_freq_jan };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) llm_libera(v[i]);
    llm_memset(e, 0, sizeof *e);
}

void llm_estado_reiniciar(llm_estado_t *e)
{
    const llm_modelo_t *m = e->m;
    e->kv.pos = 0;
    llm_memset(e->kv.conv_estado, 0, (size_t)m->n_conv * (m->l_conv - 1) * m->n_embd * 4);
}

/* ------------------------------------------------------------ operaciones */

static void rmsnorm(float *o, const float *x, const float *w, int n, float eps)
{
    double ss = 0;
    for (int i = 0; i < n; i++) ss += (double)x[i] * x[i];
    float esc = 1.0f / llm_sqrtf((float)(ss / n) + eps);
    for (int i = 0; i < n; i++) o[i] = x[i] * esc * w[i];
}

static inline float silu(float x) { return x / (1.0f + llm_expf(-x)); }
static inline float sigmoide(float x) { return 1.0f / (1.0f + llm_expf(-x)); }

typedef struct {
    const uint8_t *w;
    size_t linha;          /* bytes por fila */
    uint32_t tipo;
    int n_in, n_out, B;
    const void *xq; size_t xq_passo;   /* entrada cuantizada de cada token */
    const float *xf;                    /* entrada en float (pesos F32/F16) */
    float *y;
} matmul_t;

static float dot_f32(const float *a, const float *b, int n)
{
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

static void matmul_trozo(void *ctx, int ini, int fim)
{
    const matmul_t *t = ctx;
    llm_dot_t dot = t->tipo == T_Q4_K ? llm_dot_q4_K : t->tipo == T_Q6_K ? llm_dot_q6_K :
                    t->tipo == T_Q8_0 ? llm_dot_q8_0 : t->tipo == T_Q4_0 ? llm_dot_q4_0 : NULL;
    float linha_f[t->tipo == T_F16 ? t->n_in : 1];
    for (int r = ini; r < fim; r++) {
        const uint8_t *w = t->w + (size_t)r * t->linha;
        if (dot) {
            for (int b = 0; b < t->B; b++)
                t->y[(size_t)b * t->n_out + r] = dot(w, (const uint8_t *)t->xq + (size_t)b * t->xq_passo, t->n_in);
        } else {
            const float *wf = (const float *)w;
            if (t->tipo == T_F16) { llm_decuant_linha(T_F16, w, linha_f, t->n_in); wf = linha_f; }
            for (int b = 0; b < t->B; b++)
                t->y[(size_t)b * t->n_out + r] = dot_f32(wf, t->xf + (size_t)b * t->n_in, t->n_in);
        }
    }
}

/* y[B][n_out] = W x[B][n_in] */
static void matmul(llm_estado_t *e, const gguf_tensor_t *W, const float *x, float *y, int B)
{
    matmul_t t;
    t.w = W->dados; t.tipo = W->tipo; t.n_in = (int)W->ne[0]; t.n_out = (int)W->ne[1]; t.B = B;
    t.linha = gguf_tamanho_linha(W->tipo, W->ne[0]);
    t.xf = x; t.y = y; t.xq = e->xq; t.xq_passo = 0;
    /* La entrada cuantizada cabe en e->xq (revisión D380): llm_modelo_abrir ya comprobó las dimensiones. */
    if ((size_t)t.n_in > xq_maior(e->m) || B > LLM_LOTE) { llm_eprintf("llm: matmul fuera de los límites\n"); return; }
    if (W->tipo == T_Q4_K || W->tipo == T_Q6_K) {
        t.xq_passo = (size_t)t.n_in / QK_K * sizeof(bloco_q8_K);
        for (int b = 0; b < B; b++) llm_quant_q8_K(x + (size_t)b * t.n_in, (bloco_q8_K *)((uint8_t *)e->xq + b * t.xq_passo), t.n_in);
    } else if (W->tipo == T_Q8_0 || W->tipo == T_Q4_0) {
        t.xq_passo = (size_t)t.n_in / QK8_0 * sizeof(bloco_q8_0);
        for (int b = 0; b < B; b++) llm_quant_q8_0(x + (size_t)b * t.n_in, (bloco_q8_0 *)((uint8_t *)e->xq + b * t.xq_passo), t.n_in);
    }
    llm_hilos_repartir(matmul_trozo, &t, t.n_out);
}

/* Tablas de RoPE del lote: ángulo = pos * base^(-2i/D) / factor_i, en float y por
 * multiplicaciones sucesivas, como ggml (así los ángulos grandes redondean igual). */
static void rope_tabelas(llm_estado_t *e, int pos0, int n)
{
    const llm_modelo_t *m = e->m;
    const int D = m->dim_cab, H = D / 2;
    const float *ff = m->rope_freqs ? (const float *)m->rope_freqs->dados : NULL;
    const float esc_c = llm_powf(m->rope_base, -2.0f / D), esc_j = llm_powf(m->rope_base_janela, -2.0f / D);
    for (int b = 0; b < n; b++) {
        float tc = (float)(pos0 + b), tj = tc;
        float *cc = e->rope_cos + (size_t)b * D, *sc = e->rope_sin + (size_t)b * D;
        for (int i = 0; i < H; i++) {
            llm_sincosf(ff ? tc / ff[i] : tc, &sc[i], &cc[i]);
            llm_sincosf(tj, &sc[H + i], &cc[H + i]);
            tc *= esc_c; tj *= esc_j;
        }
    }
}

static void rope(float *v, const float *c, const float *s, int D)
{
    const int H = D / 2;
    for (int i = 0; i < H; i++) {
        float x0 = v[i], x1 = v[i + H];
        v[i] = x0 * c[i] - x1 * s[i];
        v[i + H] = x0 * s[i] + x1 * c[i];
    }
}

static void bloco_conv(llm_estado_t *e, const llm_camada_t *c, int n)
{
    const llm_modelo_t *m = e->m;
    const int E = m->n_embd, L = m->l_conv, Lm = L - 1;
    matmul(e, c->conv_in, e->xb, e->bcx, n);
    float *est = e->kv.conv_estado + (size_t)c->idx_conv * Lm * E;   /* [canal][Lm], el más viejo primero */
    const float *w = c->conv->dados;
    for (int b = 0; b < n; b++) {
        const float *B_ = e->bcx + (size_t)b * 3 * E, *C_ = B_ + E, *X_ = C_ + E;
        float *y = e->xb2 + (size_t)b * E;
        for (int ch = 0; ch < E; ch++) {
            float *s = est + (size_t)ch * Lm;
            const float *wc = w + (size_t)ch * L;
            float bx = B_[ch] * X_[ch], soma = 0;
            for (int k = 0; k < Lm; k++) soma += s[k] * wc[k];
            soma += bx * wc[Lm];
            for (int k = 0; k + 1 < Lm; k++) s[k] = s[k + 1];
            s[Lm - 1] = bx;
            y[ch] = C_[ch] * soma;
        }
    }
    matmul(e, c->conv_out, e->xb2, e->xb, n);
}

typedef struct { llm_estado_t *e; const llm_camada_t *c; int b, pos; } atencao_t;

/* Las cabezas [ini, fim) de un token. */
static void atencao_cabecas(void *ctx, int ini, int fim)
{
    const atencao_t *a = ctx;
    llm_estado_t *e = a->e;
    const llm_modelo_t *m = e->m;
    const llm_camada_t *c = a->c;
    const int D = m->dim_cab, Q = m->n_cab * D, grupo = m->n_cab / c->n_kv, nb = D / QK8_0, nbkv = c->n_kv * nb;
    const int slots = e->kv.posicoes[c->idx_cache];
    const int ini_t = c->janela && a->pos - m->n_janela + 1 > 0 ? a->pos - m->n_janela + 1 : 0;
    const int nt = a->pos - ini_t + 1;
    const bloco_q8_0 *K = e->kv.k[c->idx_cache], *V = e->kv.v[c->idx_cache];
    const float esc = 1.0f / llm_sqrtf((float)D);
    for (int h = ini; h < fim; h++) {
        float *pont = e->pont + (size_t)h * (e->kv.ctx + 16);   /* una tira por cabeza: se reparten entre hilos */
        const int kh = h / grupo;
        bloco_q8_0 *qq = e->qq + (size_t)h * nb;
        llm_quant_q8_0(e->q + (size_t)a->b * Q + (size_t)h * D, qq, D);
        float maxp = -3.4e38f;
        for (int t = 0; t < nt; t++) {
            int slot = (ini_t + t) % slots;
            float p = llm_dot_q8_0(K + (size_t)slot * nbkv + (size_t)kh * nb, qq, D) * esc;
            pont[t] = p;
            if (p > maxp) maxp = p;
        }
        float soma = 0;
        for (int t = 0; t < nt; t++) { pont[t] = llm_expf(pont[t] - maxp); soma += pont[t]; }
        float *o = e->att + (size_t)a->b * Q + (size_t)h * D;
        llm_memset(o, 0, (size_t)D * 4);
        for (int t = 0; t < nt; t++) {
            int slot = (ini_t + t) % slots;
            const bloco_q8_0 *vb = V + (size_t)slot * nbkv + (size_t)kh * nb;
            float p = pont[t] / soma;
            for (int j = 0; j < nb; j++) {
                float d = llm_f16_f32(vb[j].d) * p;
                float *oj = o + j * QK8_0;
                for (int i = 0; i < QK8_0; i++) oj[i] += d * vb[j].qs[i];
            }
        }
        float gt = sigmoide(e->gate[(size_t)a->b * m->n_cab + h]);
        for (int i = 0; i < D; i++) o[i] *= gt;
    }
}

static void bloco_atencao(llm_estado_t *e, const llm_camada_t *c, int pos0, int n)
{
    const llm_modelo_t *m = e->m;
    const int D = m->dim_cab, Q = m->n_cab * D, KV = c->n_kv * D, nb = D / QK8_0;
    matmul(e, c->wq, e->xb, e->q, n);
    matmul(e, c->wk, e->xb, e->k, n);
    matmul(e, c->wv, e->xb, e->v, n);
    matmul(e, c->wgate, e->xb, e->gate, n);
    const float *qn = c->q_norma->dados, *kn = c->k_norma->dados;
    const int off = c->janela ? D / 2 : 0;
    const int slots = e->kv.posicoes[c->idx_cache];
    for (int b = 0; b < n; b++) {
        const float *cc = e->rope_cos + (size_t)b * D, *ss = e->rope_sin + (size_t)b * D;
        float tmp_c[256], tmp_s[256];
        for (int i = 0; i < D / 2; i++) { tmp_c[i] = cc[off + i]; tmp_s[i] = ss[off + i]; }
        for (int h = 0; h < m->n_cab; h++) {
            float *q = e->q + (size_t)b * Q + (size_t)h * D;
            rmsnorm(q, q, qn, D, m->eps);
            rope(q, tmp_c, tmp_s, D);
        }
        for (int h = 0; h < c->n_kv; h++) {
            float *k = e->k + (size_t)b * KV + (size_t)h * D;
            rmsnorm(k, k, kn, D, m->eps);
            rope(k, tmp_c, tmp_s, D);
        }
        int slot = (pos0 + b) % slots;
        llm_quant_q8_0(e->k + (size_t)b * KV, e->kv.k[c->idx_cache] + (size_t)slot * c->n_kv * nb, KV);
        llm_quant_q8_0(e->v + (size_t)b * KV, e->kv.v[c->idx_cache] + (size_t)slot * c->n_kv * nb, KV);
    }
    for (int b = 0; b < n; b++) {
        atencao_t a = { e, c, b, pos0 + b };
        llm_hilos_repartir(atencao_cabecas, &a, m->n_cab);
    }
    matmul(e, c->wo, e->att, e->xb, n);
}

static void somar_norma(llm_estado_t *e, const gguf_tensor_t *pos_norma, int n)
{
    const llm_modelo_t *m = e->m;
    const int E = m->n_embd;
    for (int b = 0; b < n; b++) {
        float *h = e->xb + (size_t)b * E, *x = e->x + (size_t)b * E;
        if (pos_norma) rmsnorm(h, h, pos_norma->dados, E, m->eps);
        for (int i = 0; i < E; i++) x[i] += h[i];
    }
}

static int avaliar_lote(llm_estado_t *e, const int *toks, int n, float *logits_todos)
{
    llm_modelo_t *m = e->m;
    const int E = m->n_embd, F = m->n_ff, pos0 = e->kv.pos;
    if (pos0 + n > e->kv.ctx) return -1;
    const size_t linha_emb = gguf_tamanho_linha(m->emb->tipo, (uint64_t)E);
    for (int b = 0; b < n; b++) {
        int t = toks[b];
        if (t < 0 || t >= m->n_vocab) t = 0;
        llm_decuant_linha(m->emb->tipo, (const uint8_t *)m->emb->dados + (size_t)t * linha_emb, e->x + (size_t)b * E, E);
    }
    rope_tabelas(e, pos0, n);
    for (int il = 0; il < m->n_camadas; il++) {
        const llm_camada_t *c = &m->c[il];
        for (int b = 0; b < n; b++)
            rmsnorm(e->xb + (size_t)b * E, e->x + (size_t)b * E, c->norma_op->dados, E, m->eps);
        if (c->atencao) bloco_atencao(e, c, pos0, n);
        else bloco_conv(e, c, n);
        somar_norma(e, c->norma_op_pos, n);
        for (int b = 0; b < n; b++)
            rmsnorm(e->xb + (size_t)b * E, e->x + (size_t)b * E, c->norma_ffn->dados, E, m->eps);
        matmul(e, c->ffn_gate, e->xb, e->hb, n);
        matmul(e, c->ffn_up, e->xb, e->hb2, n);
        for (size_t i = 0; i < (size_t)n * F; i++) e->hb[i] = silu(e->hb[i]) * e->hb2[i];
        matmul(e, c->ffn_down, e->hb, e->xb, n);
        somar_norma(e, c->norma_ffn_pos, n);
    }
    e->kv.pos += n;
    /* Logits: del último token (o de todos, para comparar). */
    int b0 = logits_todos ? 0 : n - 1;
    for (int b = b0; b < n; b++) {
        rmsnorm(e->xb, e->x + (size_t)b * E, m->norma_saida->dados, E, m->eps);
        matmul(e, m->saida, e->xb, e->logits, 1);
        if (logits_todos) llm_memcpy(logits_todos + (size_t)b * m->n_vocab, e->logits, (size_t)m->n_vocab * 4);
    }
    return 0;
}

int llm_avaliar(llm_estado_t *e, const int *toks, int n, float *logits_todos)
{
    const int V = e->m->n_vocab;
    while (n > 0) {
        int k = n > e->lote_max ? e->lote_max : n;
        if (avaliar_lote(e, toks, k, logits_todos)) return -1;
        toks += k; n -= k;
        if (logits_todos) logits_todos += (size_t)k * V;
    }
    return 0;
}
