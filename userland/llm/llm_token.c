// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_token.c - tokenizador BPE byte a byte («gpt2» en GGUF), D380.
 *
 *  1. Los tokens especiales (<|im_start|>, <think>...) se separan primero: los de
 *     control solo si se pide (el texto de la plantilla), los «definidos por el usuario»
 *     siempre, como llama.cpp.
 *  2. El resto se corta en palabras con el pretokenizador de llama3 (el que usa «lfm2»):
 *       (?i:'s|'t|'re|'ve|'m|'ll|'d) | [^\r\n\p{L}\p{N}]?\p{L}+ | \p{N}{1,3}
 *       | ?[^\s\p{L}\p{N}]+[\r\n]* | \s*[\r\n]+ | \s+(?!\S) | \s+
 *     escrito a mano sobre los puntos de código (sin motor de expresiones regulares).
 *  3. Cada palabra pasa a la forma de gpt2 (cada byte es un carácter visible: el espacio
 *     es «Ġ», el salto de línea «Ċ»...). Si la palabra entera es un token, ya está
 *     (ignore_merges); si no, se parte en caracteres y se aplican las fusiones del
 *     fichero de menor a mayor rango.
 * Las tablas de letras y números salen de tools/llm_unicode.py (llm_unicode.h). */
#include "llm.h"
#include "llm_unicode.h"

/* ------------------------------------------------------------ Unicode */

static bool em_rangos(const uint32_t (*r)[2], int n, uint32_t c)
{
    int a = 0, b = n - 1;
    while (a <= b) {
        int m = (a + b) / 2;
        if (c < r[m][0]) b = m - 1;
        else if (c > r[m][1]) a = m + 1;
        else return true;
    }
    return false;
}

enum { F_LETRA = 1, F_NUMERO = 2, F_ESPACO = 4, F_OUTRO = 8 };

static int bandeiras(uint32_t c)
{
    if (c == 0xFFFFFFFFu) return 0;
    if (c == ' ' || (c >= 9 && c <= 13) || c == 0x85 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
        c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000)
        return F_ESPACO;
    if (c < 128) {
        if ((c | 32) >= 'a' && (c | 32) <= 'z') return F_LETRA;
        if (c >= '0' && c <= '9') return F_NUMERO;
        return F_OUTRO;
    }
    if (em_rangos(uni_letras, (int)(sizeof uni_letras / sizeof uni_letras[0]), c)) return F_LETRA;
    if (em_rangos(uni_numeros, (int)(sizeof uni_numeros / sizeof uni_numeros[0]), c)) return F_NUMERO;
    return F_OUTRO;
}

/* Un punto de código de UTF-8 (los bytes no válidos valen por sí mismos). -> bytes leídos */
static int utf8_ler(const uint8_t *s, size_t n, uint32_t *c)
{
    uint8_t b = s[0];
    int len = b < 0x80 ? 1 : (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
    if ((size_t)len > n) len = 1;
    if (len == 1) { *c = b; return 1; }
    uint32_t v = b & (0x7F >> len);
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) { *c = b; return 1; }
        v = v << 6 | (s[i] & 0x3F);
    }
    *c = v;
    return len;
}

static int utf8_escrever(uint32_t c, char *o)
{
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | c >> 6); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xE0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

/* gpt2: cada byte es un carácter. Los visibles (33-126, 161-172, 174-255) son ellos
 * mismos; los demás van, en orden, a 256, 257... */
static uint16_t byte_cp[256];
static int16_t cp_byte[512];
static void mapa_bytes(void)
{
    if (byte_cp['!']) return;
    int n = 0;
    for (int i = 0; i < 512; i++) cp_byte[i] = -1;
    for (int b = 0; b < 256; b++) {
        bool vis = (b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255);
        byte_cp[b] = (uint16_t)(vis ? b : 256 + n++);
        cp_byte[byte_cp[b]] = (int16_t)b;
    }
}

/* ------------------------------------------------------------ tablas */

static uint32_t hash_bytes(const char *s, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
    return h;
}

static int buscar_tok(const llm_tokens_t *t, const char *s, size_t n)
{
    uint32_t m = t->hash_tok_n - 1, i = hash_bytes(s, n) & m;
    for (;;) {
        uint32_t v = t->hash_tok[i];
        if (!v) return -1;
        const llm_texto_t *x = &t->tok[v - 1];
        if (x->n == n && llm_memcmp(x->s, s, n) == 0) return (int)v - 1;
        i = (i + 1) & m;
    }
}

int llm_token_id(const llm_tokens_t *t, const char *s) { return buscar_tok(t, s, llm_strlen(s)); }

static uint32_t hash_par(uint64_t k) { k ^= k >> 29; k *= 0xBF58476D1CE4E5B9ull; k ^= k >> 32; return (uint32_t)k; }

/* Fusión del par (a, b): rango y token resultante; false si no hay. */
static bool buscar_par(const llm_tokens_t *t, int a, int b, uint32_t *rango, int *res)
{
    uint64_t k = (uint64_t)(uint32_t)a << 32 | (uint32_t)b;
    uint32_t m = t->hash_par_n - 1, i = hash_par(k) & m;
    for (;;) {
        if (t->hash_par[i] == ~0ull) return false;
        if (t->hash_par[i] == k) { *rango = t->hash_par_val[2 * i]; *res = (int)t->hash_par_val[2 * i + 1]; return true; }
        i = (i + 1) & m;
    }
}

#define ERRO(...) do { llm_formatar(erro, max, __VA_ARGS__); llm_tokens_libera(t); return -1; } while (0)

int llm_tokens_criar(llm_tokens_t *t, const gguf_t *g, char *erro, size_t max)
{
    llm_memset(t, 0, sizeof *t);
    mapa_bytes();
    const uint8_t *fim_cab = g->cab + g->cab_n;   /* los textos no pasan de aquí (revisión D380) */
    llm_texto_t s;
    if (!gguf_texto(g, "tokenizer.ggml.model", &s) || s.n != 4 || llm_memcmp(s.s, "gpt2", 4))
        ERRO("tokenizador no admitido (este motor usa el BPE de gpt2)");
    const gguf_kv_t *kt = gguf_kv(g, "tokenizer.ggml.tokens");
    if (!kt || kt->tipo != GGUF_ARRAY || kt->arr_tipo != GGUF_STRING || kt->arr_n < 256 || kt->arr_n > 1000000)
        ERRO("el modelo no trae el vocabulario");
    t->n = (int)kt->arr_n;
    t->tok = llm_aloca((size_t)t->n * sizeof(llm_texto_t));
    t->tipo = llm_aloca((size_t)t->n);
    t->hash_tok_n = 1;
    while (t->hash_tok_n < (uint32_t)t->n * 2) t->hash_tok_n <<= 1;
    t->hash_tok = llm_aloca_zero(t->hash_tok_n * 4);
    if (!t->tok || !t->tipo || !t->hash_tok) ERRO("no hay memoria para el vocabulario");
    const uint8_t *p = kt->dados;
    for (int i = 0; i < t->n; i++) {
        p = gguf_arr_texto(p, fim_cab, &t->tok[i]);
        if (!p) ERRO("vocabulario del modelo cortado o no válido (token %d)", i);
        if (t->tok[i].n > 1024) ERRO("vocabulario no válido: el token %d mide %u bytes", i, t->tok[i].n);
        uint32_t tp = 1;
        gguf_arr_u32(g, "tokenizer.ggml.token_type", (uint64_t)i, &tp);
        t->tipo[i] = (uint8_t)tp;
        uint32_t mk = t->hash_tok_n - 1, h = hash_bytes(t->tok[i].s, t->tok[i].n) & mk;
        while (t->hash_tok[h]) {
            const llm_texto_t *x = &t->tok[t->hash_tok[h] - 1];
            if (x->n == t->tok[i].n && llm_memcmp(x->s, t->tok[i].s, x->n) == 0) break;   /* repetido: vale el primero */
            h = (h + 1) & mk;
        }
        if (!t->hash_tok[h]) t->hash_tok[h] = (uint32_t)i + 1;
        if ((tp == 3 || tp == 4) && t->tok[i].n && t->n_especiais < 256) t->especiais[t->n_especiais++] = i;
    }
    /* los especiales, del más largo al más corto (inserción: son pocos) */
    for (int i = 1; i < t->n_especiais; i++) {
        int v = t->especiais[i], j = i;
        while (j > 0 && t->tok[t->especiais[j - 1]].n < t->tok[v].n) { t->especiais[j] = t->especiais[j - 1]; j--; }
        t->especiais[j] = v;
    }

    const gguf_kv_t *km = gguf_kv(g, "tokenizer.ggml.merges");
    if (!km || km->tipo != GGUF_ARRAY || km->arr_tipo != GGUF_STRING) ERRO("el modelo no trae las fusiones de BPE");
    t->n_merges = (int)km->arr_n;
    t->hash_par_n = 1;
    while (t->hash_par_n < (uint32_t)t->n_merges * 2 + 2) t->hash_par_n <<= 1;
    t->hash_par = llm_aloca((size_t)t->hash_par_n * 8);
    t->hash_par_val = llm_aloca((size_t)t->hash_par_n * 8);
    if (!t->hash_par || !t->hash_par_val) ERRO("no hay memoria para las fusiones de BPE");
    llm_memset(t->hash_par, 0xFF, (size_t)t->hash_par_n * 8);
    p = km->dados;
    char junto[512];
    for (int i = 0; i < t->n_merges; i++) {
        llm_texto_t mg;
        p = gguf_arr_texto(p, fim_cab, &mg);
        if (!p) ERRO("fusiones de BPE cortadas o no válidas (%d)", i);
        if (mg.n > 1024) continue;
        uint32_t esp = 0;
        while (esp < mg.n && mg.s[esp] != ' ') esp++;
        if (esp == 0 || esp >= mg.n - 1 || mg.n - 1 > sizeof junto) continue;
        int a = buscar_tok(t, mg.s, esp), b = buscar_tok(t, mg.s + esp + 1, mg.n - esp - 1);
        llm_memcpy(junto, mg.s, esp);
        llm_memcpy(junto + esp, mg.s + esp + 1, mg.n - esp - 1);
        int r = buscar_tok(t, junto, mg.n - 1);
        if (a < 0 || b < 0 || r < 0) continue;
        uint64_t k = (uint64_t)(uint32_t)a << 32 | (uint32_t)b;
        uint32_t mk = t->hash_par_n - 1, h = hash_par(k) & mk;
        while (t->hash_par[h] != ~0ull && t->hash_par[h] != k) h = (h + 1) & mk;
        if (t->hash_par[h] == k) continue;   /* repetida: vale la de menor rango */
        t->hash_par[h] = k;
        t->hash_par_val[2 * h] = (uint32_t)i;
        t->hash_par_val[2 * h + 1] = (uint32_t)r;
    }

    uint32_t u;
    t->bos = gguf_u32(g, "tokenizer.ggml.bos_token_id", &u) ? (int)u : -1;
    t->eos = gguf_u32(g, "tokenizer.ggml.eos_token_id", &u) ? (int)u : -1;
    t->pad = gguf_u32(g, "tokenizer.ggml.padding_token_id", &u) ? (int)u : -1;
    t->add_bos = gguf_u32(g, "tokenizer.ggml.add_bos_token", &u) ? u != 0 : false;
    t->ignorar_merges = true;   /* llama3/lfm2 */
    t->im_start = llm_token_id(t, "<|im_start|>");
    t->im_end = llm_token_id(t, "<|im_end|>");
    t->think = llm_token_id(t, "<think>");
    t->think_fim = llm_token_id(t, "</think>");
    /* ids del GGUF fuera del vocabulario: como si no estuvieran (revisión D380) */
    if (t->bos >= t->n) t->bos = -1;
    if (t->eos >= t->n) t->eos = -1;
    if (t->pad >= t->n) t->pad = -1;
    return 0;
}

void llm_tokens_libera(llm_tokens_t *t)
{
    llm_libera(t->tok); llm_libera(t->tipo); llm_libera(t->hash_tok);
    llm_libera(t->hash_par); llm_libera(t->hash_par_val);
    llm_memset(t, 0, sizeof *t);
}

/* ------------------------------------------------------------ BPE de una palabra */

static int bpe_palavra(const llm_tokens_t *t, const uint32_t *cp, int n, int *saida, int max)
{
    /* la palabra en la forma de gpt2 */
    char buf[1024];
    int lb = 0;
    char u8[4];
    for (int i = 0; i < n; i++) {
        int k = utf8_escrever(cp[i], u8);
        for (int j = 0; j < k; j++) {
            if (lb + 2 > (int)sizeof buf) break;
            lb += utf8_escrever(byte_cp[(uint8_t)u8[j]], buf + lb);
        }
    }
    int id;
    if (t->ignorar_merges && (id = buscar_tok(t, buf, (size_t)lb)) >= 0) {
        if (max <= 0) return 0;
        saida[0] = id;
        return 1;
    }
    int sim[512], ns = 0;
    for (int i = 0; i < lb && ns < 512;) {
        uint32_t c;
        int k = utf8_ler((const uint8_t *)buf + i, (size_t)(lb - i), &c);
        int v = buscar_tok(t, buf + i, (size_t)k);
        if (v >= 0) sim[ns++] = v;
        i += k;
    }
    for (;;) {
        uint32_t melhor = 0xFFFFFFFFu; int pos = -1, res = -1;
        for (int i = 0; i + 1 < ns; i++) {
            uint32_t r; int x;
            if (buscar_par(t, sim[i], sim[i + 1], &r, &x) && r < melhor) { melhor = r; pos = i; res = x; }
        }
        if (pos < 0) break;
        sim[pos] = res;
        for (int i = pos + 1; i + 1 < ns; i++) sim[i] = sim[i + 1];
        ns--;
    }
    if (ns > max) ns = max < 0 ? 0 : max;   /* nunca más de lo que cabe (revisión D380) */
    for (int i = 0; i < ns; i++) saida[i] = sim[i];
    return ns;
}

/* El pretokenizador de llama3 sobre un trozo sin especiales. */
static int pretokenizar(const llm_tokens_t *t, const uint32_t *cp, int n, int *saida, int max)
{
    int total = 0;
/* ojo: evalúan p dos veces (nada de ++ dentro) */
#define CPT(p) ((p) < n ? cp[p] : 0xFFFFFFFFu)
#define FLG(p) ((p) < n ? bandeiras(cp[p]) : 0)
#define PALAVRA(fim) do { int _f = (fim); if (_f > ini) total += bpe_palavra(t, cp + ini, _f - ini, saida + total, max - total < 0 ? 0 : max - total); ini = _f; } while (0)
    int pos = 0, ini = 0;
    while (pos < n) {
        if (total >= max) break;
        uint32_t c = cp[pos];
        int f = bandeiras(c);
        if (c == '\'' && pos + 1 < n) {
            uint32_t c1 = CPT(pos + 1) | 32;
            if (c1 == 's' || c1 == 't' || c1 == 'm' || c1 == 'd') { pos += 2; PALAVRA(pos); continue; }
            if (pos + 2 < n) {
                uint32_t c2 = CPT(pos + 2) | 32;
                if ((c1 == 'r' && c2 == 'e') || (c1 == 'v' && c2 == 'e') || (c1 == 'l' && c2 == 'l')) {
                    pos += 3; PALAVRA(pos); continue;
                }
            }
        }
        if (!(c == '\r' || c == '\n' || (f & F_NUMERO))) {
            if ((f & F_LETRA) || (FLG(pos + 1) & F_LETRA)) {
                pos++;
                while (FLG(pos) & F_LETRA) pos++;
                PALAVRA(pos);
                continue;
            }
        }
        if (f & F_NUMERO) {
            int i0 = pos;
            while (FLG(pos) & F_NUMERO) {
                if (++pos - i0 >= 3) { PALAVRA(pos); i0 = pos; }
            }
            PALAVRA(pos);
            continue;
        }
        int f2 = c == ' ' ? FLG(pos + 1) : f;
        if (!(f2 & (F_ESPACO | F_LETRA | F_NUMERO)) && f) {
            pos += c == ' ';
            while (!(f2 & (F_ESPACO | F_LETRA | F_NUMERO)) && f2) { ++pos; f2 = FLG(pos); }
            uint32_t c2 = CPT(pos);
            while (c2 == '\r' || c2 == '\n') { ++pos; c2 = CPT(pos); }
            PALAVRA(pos);
            continue;
        }
        int nesp = 0, ult_rn = 0;
        while (FLG(pos + nesp) & F_ESPACO) {
            uint32_t c2 = CPT(pos + nesp);
            if (c2 == '\r' || c2 == '\n') ult_rn = pos + nesp + 1;
            nesp++;
        }
        if (ult_rn > 0) { pos = ult_rn; PALAVRA(pos); continue; }
        if (nesp > 1 && pos + nesp < n) { pos += nesp - 1; PALAVRA(pos); continue; }
        if (nesp > 0) { pos += nesp; PALAVRA(pos); continue; }
        PALAVRA(++pos);
    }
#undef CPT
#undef FLG
#undef PALAVRA
    return total;
}

static int trecho(const llm_tokens_t *t, const char *s, size_t n, int *saida, int max)
{
    if (!n) return 0;
    uint32_t *cp = llm_aloca(n * sizeof(uint32_t));
    if (!cp) return 0;
    int nc = 0;
    for (size_t i = 0; i < n;) {
        uint32_t c;
        i += (size_t)utf8_ler((const uint8_t *)s + i, n - i, &c);
        cp[nc++] = c;
    }
    int r = pretokenizar(t, cp, nc, saida, max);
    llm_libera(cp);
    return r;
}

int llm_tokenizar(const llm_tokens_t *t, const char *s, size_t n, bool especiais, int *saida, int max)
{
    int total = 0;
    size_t ini = 0;
    for (size_t i = 0; i < n;) {
        int achado = -1;
        if (s[i] == '<' || s[i] == '[') {
            for (int k = 0; k < t->n_especiais; k++) {
                int id = t->especiais[k];
                if (t->tipo[id] == 3 && !especiais) continue;
                const llm_texto_t *x = &t->tok[id];
                if (x->n <= n - i && llm_memcmp(s + i, x->s, x->n) == 0) { achado = id; break; }
            }
        } else if (t->n_especiais) {
            for (int k = 0; k < t->n_especiais; k++) {   /* especiales que no empiezan por < ni [ (raros) */
                int id = t->especiais[k];
                const llm_texto_t *x = &t->tok[id];
                if (x->s[0] == '<' || x->s[0] == '[' || (t->tipo[id] == 3 && !especiais)) continue;
                if (x->n <= n - i && llm_memcmp(s + i, x->s, x->n) == 0) { achado = id; break; }
            }
        }
        if (achado < 0) { i++; continue; }
        int r = trecho(t, s + ini, i - ini, saida + total, max - total);
        total += r;
        if (total < max) saida[total++] = achado;
        i += t->tok[achado].n;
        ini = i;
        if (total >= max) return max;
    }
    total += trecho(t, s + ini, n - ini, saida + total, max - total);
    return total > max ? max : total;
}

int llm_token_bytes(const llm_tokens_t *t, int id, char *buf, int max)
{
    if (id < 0 || id >= t->n) return 0;
    const llm_texto_t *x = &t->tok[id];
    int n = 0;
    if (t->tipo[id] == 3 || t->tipo[id] == 4) {
        for (uint32_t i = 0; i < x->n && n < max; i++) buf[n++] = x->s[i];
        return n;
    }
    for (uint32_t i = 0; i < x->n && n < max;) {
        uint32_t c;
        i += (uint32_t)utf8_ler((const uint8_t *)x->s + i, x->n - i, &c);
        int b = c < 512 ? cp_byte[c] : -1;
        if (b >= 0) buf[n++] = (char)b;
        else { char u8[4]; int k = utf8_escrever(c, u8); for (int j = 0; j < k && n < max; j++) buf[n++] = u8[j]; }
    }
    return n;
}
