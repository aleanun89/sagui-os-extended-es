// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_ferramentas.c - plantilla de chat de LFM3 con herramientas e imágenes, y lectura de las
 * llamadas a herramientas que genera el modelo (D380, fase 2). Ver llm_ferramentas.h.
 *
 * La referencia es la plantilla jinja que trae el GGUF (tokenizer.chat_template de LFM3 y
 * LFM3-VL, que es la misma); tests/h412_ia_fase2.py la compara en el PC con jinja2. */
#include "llm_ferramentas.h"
/* El lector de JSON de los agentes (userland/ia/json.c, D295) entra aquí tal cual: así `llm`
 * no enlaza toda la biblioteca de IA (TLS incluido) solo por él. */
#include "../ia/json.c"

/* ------------------------------------------------------------ búfer */

void llm_buf_add(llm_buf_t *b, const char *s, int n)
{
    if (n <= 0 || b->sem_memoria) return;
    if (b->n + n + 1 > b->cap) {
        int nc = b->cap ? b->cap : 256;
        while (nc < b->n + n + 1) { if (nc > (1 << 28)) { b->sem_memoria = true; return; } nc *= 2; }
        char *p = llm_aloca((size_t)nc);
        if (!p) { b->sem_memoria = true; return; }
        if (b->n) llm_memcpy(p, b->p, (size_t)b->n);
        llm_libera(b->p);
        b->p = p;
        b->cap = nc;
    }
    llm_memcpy(b->p + b->n, s, (size_t)n);
    b->n += n;
    b->p[b->n] = 0;
}

void llm_buf_txt(llm_buf_t *b, const char *s) { llm_buf_add(b, s, (int)llm_strlen(s)); }

void llm_buf_libera(llm_buf_t *b) { llm_libera(b->p); llm_memset(b, 0, sizeof *b); }

/* ------------------------------------------------------------ JSON como Python */

void llm_json_cadeia(llm_buf_t *o, const char *s, int n)
{
    static const char hexa[] = "0123456789abcdef";
    llm_buf_add(o, "\"", 1);
    int ini = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        const char *e = NULL;
        char u[7];
        if (c == '"') e = "\\\"";
        else if (c == '\\') e = "\\\\";
        else if (c == '\n') e = "\\n";
        else if (c == '\r') e = "\\r";
        else if (c == '\t') e = "\\t";
        else if (c == '\b') e = "\\b";
        else if (c == '\f') e = "\\f";
        else if (c < 0x20) { u[0] = '\\'; u[1] = 'u'; u[2] = '0'; u[3] = '0'; u[4] = hexa[c >> 4]; u[5] = hexa[c & 15]; u[6] = 0; e = u; }
        if (!e) continue;
        llm_buf_add(o, s + ini, i - ini);
        llm_buf_txt(o, e);
        ini = i + 1;
    }
    llm_buf_add(o, s + ini, n - ini);
    llm_buf_add(o, "\"", 1);
}

/* La cadena del token i, sin escapes, en memoria nueva (NULL si no hay memoria). */
static char *texto_de(const char *t, const json_tok_t *tk, int i, int *n)
{
    int max = tk[i].fim - tk[i].ini + 8;
    char *d = llm_aloca((size_t)max);
    if (!d) { *n = 0; return NULL; }
    *n = json_texto(t, tk, i, d, max);
    if (*n < 0) { *n = 0; d[0] = 0; }
    return d;
}

static void reemitir_cadeia(const char *t, const json_tok_t *tk, int i, llm_buf_t *o)
{
    int n;
    char *d = texto_de(t, tk, i, &n);
    if (!d) { o->sem_memoria = true; return; }
    llm_json_cadeia(o, d, n);
    llm_libera(d);
}

int llm_json_reemitir(const char *t, const json_tok_t *tk, int i, llm_buf_t *o)
{
    switch (tk[i].tipo) {
    case JSON_STR: reemitir_cadeia(t, tk, i, o); return i + 1;
    case JSON_NUM: case JSON_LIT: llm_buf_add(o, t + tk[i].ini, tk[i].fim - tk[i].ini); return i + 1;
    case JSON_ARR: {
        llm_buf_add(o, "[", 1);
        int j = i + 1;
        for (int k = 0; k < tk[i].n; k++) {
            if (k) llm_buf_add(o, ", ", 2);
            j = llm_json_reemitir(t, tk, j, o);
        }
        llm_buf_add(o, "]", 1);
        return j;
    }
    case JSON_OBJ: {
        llm_buf_add(o, "{", 1);
        int j = i + 1;
        for (int k = 0; k < tk[i].n; k++) {
            if (k) llm_buf_add(o, ", ", 2);
            reemitir_cadeia(t, tk, j, o);
            llm_buf_add(o, ": ", 2);
            j = llm_json_reemitir(t, tk, j + 1, o);
        }
        llm_buf_add(o, "}", 1);
        return j;
    }
    }
    return i + 1;
}

/* ------------------------------------------------------------ plantilla */

static bool campo_igual(const char *t, const json_tok_t *tk, int i, const char *s)
{
    if (i < 0 || tk[i].tipo != JSON_STR) return false;
    int n = (int)llm_strlen(s);
    return tk[i].fim - tk[i].ini == n && !llm_memcmp(t + tk[i].ini, s, (size_t)n);
}

static bool contem(const char *s, int n, const char *x)
{
    int k = (int)llm_strlen(x);
    for (int i = 0; i + k <= n; i++) if (!llm_memcmp(s + i, x, (size_t)k)) return true;
    return false;
}

static void seg(llm_segs_t *s, int tipo, const char *t, int n) { if (n > 0 || tipo == SEG_IMAGEM) s->f(s->ctx, tipo, t, n); }
static void seg_txt(llm_segs_t *s, int tipo, const char *t) { seg(s, tipo, t, (int)llm_strlen(t)); }

/* El texto de un «content»: cadena, o lista de partes ({"type":"text","text":...} y las imágenes,
 * «image_url» de OpenAI o «image»). Con «s», las imágenes salen como trozos aparte, entre
 * <|vision_start|> y <|vision_end|>; sin él, solo el texto (va todo en «o»). */
static void conteudo(const char *t, const json_tok_t *tk, int ct, llm_buf_t *o, llm_segs_t *s, int *n_img)
{
    if (ct < 0) return;
    if (tk[ct].tipo == JSON_STR) {
        int n;
        char *d = texto_de(t, tk, ct, &n);
        if (d) llm_buf_add(o, d, n); else o->sem_memoria = true;
        llm_libera(d);
        return;
    }
    if (tk[ct].tipo == JSON_OBJ) {   /* una sola parte */
        int tp = json_campo(t, tk, ct, "type");
        if (campo_igual(t, tk, tp, "text")) conteudo(t, tk, json_campo(t, tk, ct, "text"), o, s, n_img);
        return;
    }
    if (tk[ct].tipo != JSON_ARR) return;
    for (int q = 0; q < tk[ct].n; q++) {
        int it = json_elem(tk, ct, q);
        if (tk[it].tipo == JSON_STR) { conteudo(t, tk, it, o, s, n_img); continue; }
        int tp = json_campo(t, tk, it, "type");
        if (campo_igual(t, tk, tp, "text")) conteudo(t, tk, json_campo(t, tk, it, "text"), o, s, n_img);
        else if (campo_igual(t, tk, tp, "image_url") || campo_igual(t, tk, tp, "image")) {
            if (!s) continue;
            int u = json_campo(t, tk, it, "image_url");
            if (u >= 0 && tk[u].tipo == JSON_OBJ) u = json_campo(t, tk, u, "url");
            if (u < 0) u = json_campo(t, tk, it, "image");
            if (u < 0) u = json_campo(t, tk, it, "url");
            seg(s, SEG_TEXTO, o->p, o->n);   /* lo de antes de la imagen */
            o->n = 0;
            seg_txt(s, SEG_CONTROLE, "<|vision_start|>");
            int un = 0;
            char *ud = u >= 0 && tk[u].tipo == JSON_STR ? texto_de(t, tk, u, &un) : NULL;
            seg(s, SEG_IMAGEM, ud ? ud : "", un);
            llm_libera(ud);
            seg_txt(s, SEG_CONTROLE, "<|vision_end|>");
            (*n_img)++;
        }
    }
}

/* Un valor de un argumento, como render_value de la plantilla. */
static void valor_arg(const char *t, const json_tok_t *tk, int i, llm_buf_t *o)
{
    if (tk[i].tipo == JSON_STR) { conteudo(t, tk, i, o, NULL, NULL); return; }
    llm_json_reemitir(t, tk, i, o);
}

#define ARGS_TOKS 4096

/* Las llamadas de un mensaje del asistente, como render_tool_calls. 0 = ok */
static int chamadas_hist(const char *t, const json_tok_t *tk, int tc, llm_buf_t *o, char *erro, int max)
{
    json_tok_t *at = NULL;
    for (int k = 0; k < tk[tc].n; k++) {
        int c = json_elem(tk, tc, k);
        if (tk[c].tipo != JSON_OBJ) continue;
        int fn = json_campo(t, tk, c, "function");
        if (fn < 0) fn = c;
        int nm = json_campo(t, tk, fn, "name");
        if (nm < 0 || tk[nm].tipo != JSON_STR) { llm_formatar(erro, (size_t)max, "tool_calls sin nombre"); llm_libera(at); return -1; }
        if (k) llm_buf_txt(o, "\n\n");
        llm_buf_txt(o, "<tool_call>\n<function=");
        conteudo(t, tk, nm, o, NULL, NULL);
        llm_buf_txt(o, ">\n");
        int ar = json_campo(t, tk, fn, "arguments");
        if (ar >= 0 && tk[ar].tipo == JSON_STR) {
            /* OpenAI manda los argumentos como texto JSON: se leen (la plantilla quiere el objeto) */
            int n;
            char *d = texto_de(t, tk, ar, &n);
            if (!d) { llm_libera(at); llm_formatar(erro, (size_t)max, "sin memoria"); return -1; }
            if (!at) at = llm_aloca(sizeof(json_tok_t) * ARGS_TOKS);
            int na = at ? json_analisar(d, n, at, ARGS_TOKS) : -2;
            if (na >= 1 && at[0].tipo == JSON_OBJ) {
                int j = 1;
                for (int p = 0; p < at[0].n; p++) {
                    llm_buf_txt(o, "<parameter=");
                    conteudo(d, at, j, o, NULL, NULL);
                    llm_buf_txt(o, ">\n");
                    valor_arg(d, at, j + 1, o);
                    llm_buf_txt(o, "\n</parameter>\n");
                    j = json_depois(at, j + 1);
                }
            } else {
                /* vacío, {} o null valen; lo demás no es un objeto */
                int z = 0;
                while (z < n && (d[z] == ' ' || d[z] == '\n' || d[z] == '\t' || d[z] == '\r')) z++;
                if (z < n && !(n - z == 4 && !llm_memcmp(d + z, "null", 4)) && !(na >= 1 && at && at[0].tipo == JSON_OBJ)) {
                    llm_libera(d); llm_libera(at);
                    llm_formatar(erro, (size_t)max, "los argumentos de una llamada no son un objeto JSON");
                    return -1;
                }
            }
            llm_libera(d);
        } else if (ar >= 0 && tk[ar].tipo == JSON_OBJ) {
            int j = ar + 1;
            for (int p = 0; p < tk[ar].n; p++) {
                llm_buf_txt(o, "<parameter=");
                conteudo(t, tk, j, o, NULL, NULL);
                llm_buf_txt(o, ">\n");
                valor_arg(t, tk, j + 1, o);
                llm_buf_txt(o, "\n</parameter>\n");
                j = json_depois(tk, j + 1);
            }
        }
        llm_buf_txt(o, "</function>\n</tool_call>");
    }
    llm_libera(at);
    return 0;
}

static bool diferido(const char *t, const json_tok_t *tk, int i)
{
    int d = json_campo(t, tk, i, "defer_loading");
    return d >= 0 && json_verdadero(t, tk, d);
}

/* tool_to_json: el objeto de la función sin «defer_loading». */
static void ferramenta_json(const char *t, const json_tok_t *tk, int fn, llm_buf_t *o)
{
    llm_buf_add(o, "{", 1);
    int j = fn + 1;
    bool primeiro = true;
    for (int k = 0; k < tk[fn].n; k++) {
        int v = j + 1;
        if (!campo_igual(t, tk, j, "defer_loading")) {
            if (!primeiro) llm_buf_add(o, ", ", 2);
            primeiro = false;
            reemitir_cadeia(t, tk, j, o);
            llm_buf_add(o, ": ", 2);
            llm_json_reemitir(t, tk, v, o);
        }
        j = json_depois(tk, v);
    }
    llm_buf_add(o, "}", 1);
}

int llm_plantilla(const char *t, const json_tok_t *tk, const char *sistema, llm_segs_t *s,
                  llm_plantilla_t *info, char *erro, int max)
{
    llm_memset(info, 0, sizeof *info);
    info->tools = -1;
    int msgs = json_campo(t, tk, 0, "messages");
    if (msgs < 0 || tk[msgs].tipo != JSON_ARR) { llm_formatar(erro, (size_t)max, "faltan los messages"); return -1; }
    int tools = json_campo(t, tk, 0, "tools");
    int escolha = json_campo(t, tk, 0, "tool_choice");
    if (tools >= 0 && (tk[tools].tipo != JSON_ARR || tk[tools].n == 0)) tools = -1;
    if (campo_igual(t, tk, escolha, "none")) tools = -1;
    info->tools = tools;
    if (tools >= 0) {
        if (campo_igual(t, tk, escolha, "required")) llm_formatar(info->prefixo, sizeof info->prefixo, "<tool_call>\n<function=");
        else if (escolha >= 0 && tk[escolha].tipo == JSON_OBJ) {   /* {"type":"function","function":{"name":X}} */
            int fn = json_campo(t, tk, escolha, "function");
            int nm = json_campo(t, tk, fn >= 0 ? fn : escolha, "name");
            char nome[128];
            if (nm >= 0 && json_texto(t, tk, nm, nome, sizeof nome) > 0 && !contem(nome, (int)llm_strlen(nome), ">")
                && !contem(nome, (int)llm_strlen(nome), "\n"))
                llm_formatar(info->prefixo, sizeof info->prefixo, "<tool_call>\n<function=%s>\n", nome);
        }
    }

    llm_buf_t b = { 0 };
    int ini = 0, n_img = 0;
    seg_txt(s, SEG_CONTROLE, "<|startoftext|>");
    /* el sistema: el primer mensaje si es «system» (si no, el de --sistema) y las herramientas */
    int m0 = json_elem(tk, msgs, 0);
    if (m0 >= 0 && campo_igual(t, tk, json_campo(t, tk, m0, "role"), "system")) {
        conteudo(t, tk, json_campo(t, tk, m0, "content"), &b, NULL, &n_img);
        ini = 1;
    } else if (sistema && *sistema) llm_buf_txt(&b, sistema);
    if (tools >= 0) {
        bool algum = false;
        for (int k = 0; k < tk[tools].n; k++) {
            int tl = json_elem(tk, tools, k);
            if (tk[tl].tipo != JSON_OBJ) continue;
            int fn = json_campo(t, tk, tl, "function");
            if (fn < 0 || tk[fn].tipo != JSON_OBJ) fn = tl;
            if (diferido(t, tk, fn) || diferido(t, tk, tl)) continue;
            if (!algum) { if (b.n) llm_buf_txt(&b, "\n\n"); llm_buf_txt(&b, "<tools>"); algum = true; }
            llm_buf_txt(&b, "\n");
            ferramenta_json(t, tk, fn, &b);
        }
        if (algum) llm_buf_txt(&b, "\n</tools>");
    }
    if (b.n) {
        seg_txt(s, SEG_CONTROLE, "<|im_start|>system\n");
        seg(s, SEG_TEXTO, b.p, b.n);
        seg_txt(s, SEG_CONTROLE, "<|im_end|>\n");
    }
    for (int k = ini; k < tk[msgs].n; k++) {
        int m = json_elem(tk, msgs, k);
        if (tk[m].tipo != JSON_OBJ) continue;
        int pr = json_campo(t, tk, m, "role"), ct = json_campo(t, tk, m, "content");
        const char *papel = campo_igual(t, tk, pr, "assistant") ? "assistant" : campo_igual(t, tk, pr, "tool") ? "tool" :
                            campo_igual(t, tk, pr, "system") ? "system" : "user";   /* nada de tokens metidos en «role» */
        char cab[48];
        llm_formatar(cab, sizeof cab, "<|im_start|>%s\n", papel);
        seg_txt(s, SEG_CONTROLE, cab);
        b.n = 0;
        if (!llm_strcmp(papel, "assistant")) {
            /* lo pensado no vuelve a la conversación: <think></think> y lo de después */
            conteudo(t, tk, ct, &b, NULL, &n_img);
            int f = 0;
            for (int p = 0; p + 8 <= b.n; p++) if (!llm_memcmp(b.p + p, "</think>", 8)) f = p + 8;
            int fim = b.n;
            if (f) {   /* trim, como la plantilla */
                while (f < fim && (b.p[f] == ' ' || b.p[f] == '\n' || b.p[f] == '\t' || b.p[f] == '\r')) f++;
                while (fim > f && (b.p[fim - 1] == ' ' || b.p[fim - 1] == '\n' || b.p[fim - 1] == '\t' || b.p[fim - 1] == '\r')) fim--;
            }
            seg_txt(s, SEG_CONTROLE, "<think></think>");
            llm_buf_t c = { 0 };
            if (b.p) llm_buf_add(&c, b.p + f, fim - f);
            int tc = json_campo(t, tk, m, "tool_calls");
            if (tc >= 0 && tk[tc].tipo == JSON_ARR && chamadas_hist(t, tk, tc, &c, erro, max)) { llm_buf_libera(&c); llm_buf_libera(&b); return -1; }
            if (c.sem_memoria) { llm_buf_libera(&c); llm_buf_libera(&b); llm_formatar(erro, (size_t)max, "sin memoria"); return -1; }
            seg(s, SEG_TEXTO, c.p, c.n);
            llm_buf_libera(&c);
        } else if (!llm_strcmp(papel, "tool")) {
            conteudo(t, tk, ct, &b, NULL, &n_img);
            if (b.p && contem(b.p, b.n, "<tool_response>")) seg(s, SEG_TEXTO, b.p, b.n);
            else {
                llm_buf_t c = { 0 };
                llm_buf_txt(&c, "<tool_response>\n");
                if (b.p) llm_buf_add(&c, b.p, b.n);
                llm_buf_txt(&c, "\n</tool_response>");
                seg(s, SEG_TEXTO, c.p, c.n);
                llm_buf_libera(&c);
            }
        } else {
            conteudo(t, tk, ct, &b, s, &n_img);
            seg(s, SEG_TEXTO, b.p, b.n);
        }
        if (b.sem_memoria) { llm_buf_libera(&b); llm_formatar(erro, (size_t)max, "sin memoria"); return -1; }
        seg_txt(s, SEG_CONTROLE, "<|im_end|>\n");
    }
    llm_buf_libera(&b);
    seg_txt(s, SEG_CONTROLE, "<|im_start|>assistant\n<think></think>");
    if (info->prefixo[0]) seg_txt(s, SEG_TEXTO, info->prefixo);
    info->n_imagens = n_img;
    return 0;
}

/* ------------------------------------------------------------ respuesta del modelo */

static const char MARCA[] = "<tool_call>";

static int achar(const char *s, int n, int de, const char *x)
{
    int k = (int)llm_strlen(x);
    for (int i = de; i + k <= n; i++) if (!llm_memcmp(s + i, x, (size_t)k)) return i;
    return -1;
}

int llm_ferramentas_marca(const char *txt, int n, int *guardar)
{
    int p = achar(txt, n, 0, MARCA);
    *guardar = 0;
    if (p >= 0) return p;
    const int k = (int)sizeof MARCA - 1;
    for (int g = k - 1; g > 0; g--)
        if (g <= n && !llm_memcmp(txt + n - g, MARCA, (size_t)g)) { *guardar = g; break; }
    return -1;
}

/* El tipo que el esquema da al parámetro «nome» de la herramienta «fnome» («string», «integer»...). */
static bool parametro_texto(const char *t, const json_tok_t *tk, int tools, const char *fnome, int fn_n, const char *pnome, int pn)
{
    if (tools < 0) return false;
    for (int k = 0; k < tk[tools].n; k++) {
        int tl = json_elem(tk, tools, k);
        int fn = json_campo(t, tk, tl, "function");
        if (fn < 0) fn = tl;
        int nm = json_campo(t, tk, fn, "name");
        if (nm < 0 || tk[nm].tipo != JSON_STR || tk[nm].fim - tk[nm].ini != fn_n || llm_memcmp(t + tk[nm].ini, fnome, (size_t)fn_n)) continue;
        int pa = json_campo(t, tk, fn, "parameters");
        int pr = json_campo(t, tk, pa, "properties");
        if (pr < 0 || tk[pr].tipo != JSON_OBJ) return false;
        int j = pr + 1;
        for (int q = 0; q < tk[pr].n; q++) {
            if (tk[j].fim - tk[j].ini == pn && !llm_memcmp(t + tk[j].ini, pnome, (size_t)pn)) {
                int tp = json_campo(t, tk, j + 1, "type");
                return campo_igual(t, tk, tp, "string");
            }
            j = json_depois(tk, j + 1);
        }
        return false;
    }
    return false;
}

/* ¿Es el texto un único valor JSON (número, true, objeto...)? */
static bool eh_json(const char *s, int n)
{
    static json_tok_t tk[1024];
    return n > 0 && json_analisar(s, n, tk, 1024) >= 1;
}

int llm_ferramentas_ler(const char *corpo, const json_tok_t *tk, const llm_plantilla_t *info,
                        const char *txt, int n, llm_buf_t *conteudo_o, llm_buf_t *chamadas, bool delta)
{
    static unsigned id_seq;
    int p = info->tools >= 0 ? achar(txt, n, 0, MARCA) : -1;
    int fim_c = p >= 0 ? p : n;
    if (p >= 0) while (fim_c > 0 && (txt[fim_c - 1] == ' ' || txt[fim_c - 1] == '\n' || txt[fim_c - 1] == '\t' || txt[fim_c - 1] == '\r')) fim_c--;
    llm_buf_add(conteudo_o, txt, fim_c);
    if (p < 0) return 0;
    int qtd = 0;
    while (p >= 0) {
        int f = achar(txt, n, p, "<function=");
        if (f < 0) break;
        int prox = achar(txt, n, p + (int)sizeof MARCA - 1, MARCA);
        if (prox >= 0 && f > prox) { p = prox; continue; }
        int nf = f + 10, fe = nf;
        while (fe < n && txt[fe] != '>' && txt[fe] != '\n') fe++;
        if (fe == nf) break;
        const char *nome = txt + nf;
        int nome_n = fe - nf;
        int fim_bloco = achar(txt, n, fe, "</function>");
        if (fim_bloco < 0 || (prox >= 0 && fim_bloco > prox)) fim_bloco = prox >= 0 ? prox : n;   /* cortada: lo que haya */
        llm_buf_t args = { 0 };
        llm_buf_add(&args, "{", 1);
        int q = fe, np = 0;
        for (;;) {
            int pp = achar(txt, fim_bloco, q, "<parameter=");
            if (pp < 0) break;
            int kn = pp + 11, ke = kn;
            while (ke < fim_bloco && txt[ke] != '>' && txt[ke] != '\n') ke++;
            if (ke >= fim_bloco) break;
            int v0 = ke + 1;
            if (v0 < fim_bloco && txt[v0] == '\n') v0++;
            int v1 = achar(txt, fim_bloco, v0, "</parameter>");
            int outro = achar(txt, fim_bloco, v0, "<parameter=");
            if (v1 < 0 || (outro >= 0 && outro < v1)) v1 = outro >= 0 ? outro : fim_bloco;
            q = v1;
            if (v1 > v0 && txt[v1 - 1] == '\n') v1--;
            if (np++) llm_buf_add(&args, ", ", 2);
            llm_json_cadeia(&args, txt + kn, ke - kn);
            llm_buf_add(&args, ": ", 2);
            if (!parametro_texto(corpo, tk, info->tools, nome, nome_n, txt + kn, ke - kn) && eh_json(txt + v0, v1 - v0))
                llm_buf_add(&args, txt + v0, v1 - v0);
            else llm_json_cadeia(&args, txt + v0, v1 - v0);
        }
        llm_buf_add(&args, "}", 1);
        char cab[96];
        if (!qtd) llm_buf_add(chamadas, "[", 1); else llm_buf_add(chamadas, ",", 1);
        if (delta) { llm_formatar(cab, sizeof cab, "{\"index\":%d,", qtd); llm_buf_txt(chamadas, cab); }
        else llm_buf_add(chamadas, "{", 1);
        llm_formatar(cab, sizeof cab, "\"id\":\"call_%u\",\"type\":\"function\",\"function\":{\"name\":", ++id_seq);
        llm_buf_txt(chamadas, cab);
        llm_json_cadeia(chamadas, nome, nome_n);
        llm_buf_txt(chamadas, ",\"arguments\":");
        llm_json_cadeia(chamadas, args.p ? args.p : "{}", args.p ? args.n : 2);
        llm_buf_txt(chamadas, "}}");
        llm_buf_libera(&args);
        qtd++;
        p = prox;
    }
    if (qtd) llm_buf_add(chamadas, "]", 1);
    return qtd;
}
