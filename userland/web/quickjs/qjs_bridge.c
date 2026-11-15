/* qjs_bridge.c - o QuickJS com um DOM mínimo, dentro do SaguiOS (D149).
 *
 * O QuickJS (bellard/quickjs, MIT) e um motor de JavaScript completo, mas
 * nao sabe nada do browser: o DOM, o console e os temporizadores sao os que
 * se montam aqui (ver qjs_bridge.h).
 *
 * Duas coisas do SO condicionam o desenho:
 *
 *  - a pilha do programa e de 64 KB e o QuickJS parseia com muita recursao,
 *    por isso a execucao corre numa pilha grande (2 MB), como ja se fazia
 *    com o interpretador antigo (js_nativo.h);
 *  - nao ha relogio de alta resolucao nem temporizadores do SO: os
 *    temporizadores sao uma fila na memoria, e quem os chama diz a hora.
 */
#include "sagui.h"
#include "quickjs.h"
#include "qjs_bridge.h"
#include "qjs_prelude.h"   /* D271: o que se monta em JavaScript por cima do DOM nativo */

#define QJS_MEM_LIMITE (48u * 1024 * 1024)   /* o teto do motor */
#define QJS_PILHA      (2u * 1024 * 1024)     /* a pilha grande */
#define QJS_TIMER_MAX  32
#define QJS_TEXTO_MAX  8192
#define QJS_DOM_MAX    512    /* nos do DOM que o ponte guarda */
#define QJS_DOM_ATR    8      /* atributos por elemento */
#define QJS_DOM_OUV    4      /* ouvintes de "click" por elemento */
#define QJS_SEL_MAX    4      /* partes de um seletor */

/* Um no do DOM: o que o script ve de um elemento da pagina. */
typedef struct {
    int id;
    int pai;                   /* o id do pai, ou -1 */
    char tag[12];
    char *id_attr;
    char *classe;
    char *texto;               /* o texto (nos de texto) */
    char *conteudo;            /* o textContent (o texto de todo o abaixo) */
    uint8_t e_texto;           /* 1 = no de texto */
    int n_atr;
    char *atr_nome[QJS_DOM_ATR];
    char *atr_valor[QJS_DOM_ATR];
    JSValue onclick;           /* o tratador posto pelo script */
    int n_ouv;                 /* os ouvintes de "click" */
    JSValue ouv[QJS_DOM_OUV];
    JSValue obj;               /* o objecto JavaScript do elemento */
    uint8_t tem_obj;
} qjs_dom_el_t;

/* Um seletor: as partes separadas por espaco (o combinador de descendente). */
typedef struct {
    char tag[12];
    char id[64];
    char classe[64];
} qjs_sel_parte_t;

typedef struct {
    int n;
    qjs_sel_parte_t p[QJS_SEL_MAX];
} qjs_seletor_t;

struct qjs_ponte {
    JSRuntime *rt;
    JSContext *ctx;
    qjs_ganchos_t g;        /* uma copia: o ponte dura mais que a chamadora */
    JSValue global;
    JSValue corpo;          /* o objeto do document.body (sem DOM) */
    char titulo[256];
    bool tem_titulo;
    char corpo_html[QJS_TEXTO_MAX];
    bool tem_corpo;
    char console_txt[QJS_TEXTO_MAX];
    char alerta_txt[QJS_TEXTO_MAX];
    char url[512];          /* a pagina que esta aberta (para o location) */
    /* os temporizadores */
    struct { JSValue fn; unsigned vence; int id; } tmp[QJS_TIMER_MAX];
    int n_tmp;
    int prox_id;
    unsigned agora;          /* a hora que o chamador diz (ms) */
    /* o DOM da pagina (D160) */
    qjs_dom_el_t dom[QJS_DOM_MAX];
    uint8_t dom_existe[QJS_DOM_MAX];
    int dom_primeiro[QJS_DOM_MAX];   /* a cadeia dos filhos */
    int dom_ultimo[QJS_DOM_MAX];
    int dom_prox[QJS_DOM_MAX];
    int dom_corpo, dom_documento;
    int evento_cancelado;    /* o preventDefault do ultimo evento */
    int n_baixados;          /* fetch/XMLHttpRequest desta pagina (D271) */
};

/* ------------------------------------------------------------ pilha grande */

/* Corre uma funcao numa pilha de 2 MB: o QuickJS recua muito (criar o
 * contexto ja percorre muitas camadas, e o parser ainda mais) e a pilha do
 * programa, de 64 KB, nao chega. A pilha grande e um bloco do heap, tomado
 * uma vez so.
 *
 * O bloco e alocado com aloca(), que devolve 8 bytes de alinhamento, por isso
 * o topo se alinha para 16: a pilha de chamada e a pilha de dados do
 * compilador esperam isso. */
static void qjs_em_pilha(void (*f)(void *), void *arg)
{
    static char *pilha;
    if (!pilha) {
        pilha = aloca(QJS_PILHA);
        if (!pilha) { f(arg); return; }
    }
    char *atual;
    __asm__ volatile("movl %%esp, %0" : "=r"(atual));
    if (atual >= pilha && atual < pilha + QJS_PILHA) { f(arg); return; }
    unsigned long base = ((unsigned long)pilha + 15ul) & ~15ul;
    char *topo = (char *)(base + QJS_PILHA - 16);
    /* Troca da pilha: guarda a antiga em ebx, monta o Argumento no sitio onde a
     * funcao o vai ler (a primeira posicao depois do endereco de retorno),
     * chama e volta a pilha antiga. O bloco de 16 bytes deixa a pilha
     * alinhada como o compilador espera. */
    __asm__ volatile("movl %%esp, %%ebx\n\t"
                     "movl %0, %%esp\n\t"
                     "subl $12, %%esp\n\t"
                     "pushl %2\n\t"
                     "call *%1\n\t"
                     "addl $16, %%esp\n\t"
                     "movl %%ebx, %%esp"
                     : : "m"(topo), "m"(f), "m"(arg)
                     : "ebx", "eax", "ecx", "edx", "memory", "cc");
}

/* ------------------------------------------------------------ utilitarios */

static void qjs_juntar(char *dst, int max, const char *txt)
{
    int n = (int)strlen(dst);
    int l = (int)strlen(txt);
    if (n + l + 1 >= max) l = max - n - 1;
    if (l <= 0) return;
    memcpy(dst + n, txt, (size_t)l);
    dst[n + l] = 0;
}

/* ------------------------------------------------------------ DOM */

static JSValue qjs_js_console_log(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    char linha[512];
    linha[0] = 0;
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (i) qjs_juntar(linha, sizeof linha, " ");
        if (s) qjs_juntar(linha, sizeof linha, s);
        JS_FreeCString(ctx, s);
    }
    qjs_juntar(p->console_txt, sizeof p->console_txt, linha);
    qjs_juntar(p->console_txt, sizeof p->console_txt, "\n");
    if (p->g.log) p->g.log(linha);
    return JS_UNDEFINED;
}

static JSValue qjs_js_alerta(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    const char *s = argc > 0 ? JS_ToCString(ctx, argv[0]) : NULL;
    copia_texto(p->alerta_txt, s ? s : "", sizeof p->alerta_txt);
    if (s) JS_FreeCString(ctx, s);
    if (p->g.alerta) p->g.alerta(p->alerta_txt);
    return JS_UNDEFINED;
}

/* document.title: ler e escrever. */
static JSValue qjs_titulo_get(JSContext *ctx, JSValueConst this_val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return JS_NewString(ctx, p->titulo);
}

static JSValue qjs_titulo_set(JSContext *ctx, JSValueConst this_val,
                               JSValueConst val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    const char *s = JS_ToCString(ctx, val);
    copia_texto(p->titulo, s ? s : "", sizeof p->titulo);
    p->tem_titulo = true;
    if (s) JS_FreeCString(ctx, s);
    if (p->g.titulo) p->g.titulo(p->titulo);
    return JS_UNDEFINED;
}

/* document.body.innerHTML e .textContent: o corpo da pagina. */
static JSValue qjs_corpo_get(JSContext *ctx, JSValueConst this_val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return JS_NewString(ctx, p->corpo_html);
}

static JSValue qjs_corpo_set(JSContext *ctx, JSValueConst this_val,
                              JSValueConst val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    const char *s = JS_ToCString(ctx, val);
    copia_texto(p->corpo_html, s ? s : "", sizeof p->corpo_html);
    p->tem_corpo = true;
    if (s) JS_FreeCString(ctx, s);
    if (p->g.corpo) p->g.corpo(p->corpo_html);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------ o DOM (D160)
 *
 * A pagina e uma arvore de elementos, e o script tem de a poder consultar
 * (getElementById, getElementsByTagName, querySelector, querySelectorAll) e
 * nela por um tratador de clique (elemento.onclick e
 * elemento.addEventListener("click", ...)).
 *
 * O navegador (o Cipo) e que tem a arvore: o motor da web (motor.c) sabe
 * como esta cada pagina, e o Cipó manda-a ao ponte como texto (o formato
 * esta em qjs_bridge.h). O ponte guarda a lista e cria um objecto
 * JavaScript por elemento.
 */

/* Compara duas cadeias sem distincao de maiusculas (e ate um maximo). */
static bool qjs_igual_n(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

static bool qjs_igual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return *a == *b;
}

/* Le um numero de um campo do texto do DOM (com o sinal). */
static int qjs_num(const char *s, int n)
{
    int v = 0, i = 0, neg = 0;
    if (n > 0 && s[0] == '-') { neg = 1; i = 1; }
    for (; i < n && s[i] >= '0' && s[i] <= '9'; i++) v = v * 10 + (s[i] - '0');
    return neg ? -v : v;
}

/* Copia um campo para memoria nova, resolvendo os escapes (\\, \t, \n, \r). */
static char *qjs_campo(const char *s, int n)
{
    char *o = aloca((size_t)n + 1);
    if (!o) return NULL;
    int k = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) {
            i++;
            c = s[i];
            if (c == 't') c = '\t';
            else if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
        }
        o[k++] = c;
    }
    o[k] = 0;
    return o;
}

/* Separa uma linha nos seus campos (separados por tabulador): devolve as
 * posicoes (desde o principio da linha) e os comprimentos. */
static int qjs_partes(const char *linha, int n, int ini[12], int len[12])
{
    int nf = 0, com = 0;
    for (int i = 0; i <= n; i++) {
        if (i == n || linha[i] == '\t') {
            if (nf < 12) { ini[nf] = com; len[nf] = i - com; }
            nf++;
            com = i + 1;
            if (i == n) break;
        }
    }
    return nf;
}

/* O texto de um no e de todos os seus descendentes, em ordem de documento.
 *
 * A recursao tem um limite de profundidade: uma arvore com um ciclo nao pode
 * fazer o navegador morrer por falta de pilha (a pilha do programa tem
 * 64 KB). Com uma arvore bem formada o limite nunca chega (D161). */
#define QJS_DOM_PROF_MAX 256

static int qjs_prof;   /* a profundidade da recursao */

static int qjs_dom_tam(qjs_ponte_t *p, int i)
{
    int t = 0;
    for (int c = p->dom_primeiro[i]; c >= 0; c = p->dom_prox[c]) {
        if (p->dom[c].e_texto) t += (int)tam_texto(p->dom[c].texto);
        else {
            if (qjs_prof >= QJS_DOM_PROF_MAX) break;
            qjs_prof++;
            t += qjs_dom_tam(p, c);
            qjs_prof--;
        }
    }
    return t;
}

static void qjs_dom_txt(qjs_ponte_t *p, int i, char *o, int *k)
{
    for (int c = p->dom_primeiro[i]; c >= 0; c = p->dom_prox[c]) {
        if (p->dom[c].e_texto) {
            int n = (int)tam_texto(p->dom[c].texto);
            memcpy(o + *k, p->dom[c].texto, (size_t)n);
            *k += n;
        } else {
            if (qjs_prof >= QJS_DOM_PROF_MAX) break;
            qjs_prof++;
            qjs_dom_txt(p, c, o, k);
            qjs_prof--;
        }
    }
}

/* Deita fora o DOM anterior (a pagina nova traz o seu). */
static void qjs_dom_limpar(qjs_ponte_t *p)
{
    for (int i = 0; i < QJS_DOM_MAX; i++) {
        if (!p->dom_existe[i]) continue;
        qjs_dom_el_t *e = &p->dom[i];
        if (e->tem_obj) { JS_FreeValue(p->ctx, e->obj); e->obj = JS_UNDEFINED; e->tem_obj = 0; }
        JS_FreeValue(p->ctx, e->onclick);
        for (int k = 0; k < e->n_ouv; k++) JS_FreeValue(p->ctx, e->ouv[k]);
        e->n_ouv = 0;
        if (e->id_attr) { libera(e->id_attr); e->id_attr = NULL; }
        if (e->classe) { libera(e->classe); e->classe = NULL; }
        if (e->texto) { libera(e->texto); e->texto = NULL; }
        if (e->conteudo) { libera(e->conteudo); e->conteudo = NULL; }
        for (int k = 0; k < e->n_atr; k++) {
            if (e->atr_nome[k]) { libera(e->atr_nome[k]); e->atr_nome[k] = NULL; }
            if (e->atr_valor[k]) { libera(e->atr_valor[k]); e->atr_valor[k] = NULL; }
        }
        e->n_atr = 0;
        p->dom_existe[i] = 0;
    }
    for (int i = 0; i < QJS_DOM_MAX; i++) {
        p->dom_primeiro[i] = -1;
        p->dom_ultimo[i] = -1;
        p->dom_prox[i] = -1;
    }
    p->dom_corpo = -1;
    p->dom_documento = -1;
}

/* O no pode ser filho de um pai que ja foi lido e que nao e um seu
 * descendente: se o pai desce ate o no, a arvore tinha um ciclo e a
 * recursao do textContent nunca mais voltava (D161). */
static bool qjs_pai_valido(qjs_ponte_t *p, int pai, int id)
{
    int a = pai;
    for (int k = 0; k < QJS_DOM_MAX && a >= 0 && a < QJS_DOM_MAX; k++) {
        if (a == id) return false;
        a = p->dom[a].pai;
    }
    return true;
}

/* Recebe a arvore da pagina (o texto que o Cipo manda). */
void qjs_definir_dom(qjs_ponte_t *p, const char *fonte, int n)
{
    if (!p) return;
    qjs_dom_limpar(p);
    if (!fonte || n <= 0) return;
    int i = 0, pular_atr = 0;
    while (i < n) {
        int fim = i;
        while (fim < n && fonte[fim] != '\n') fim++;
        int len = fim - i;
        if (len > 0 && pular_atr == 0) {
            const char *linha = fonte + i;
            int ini[12]; int lens[12];
            int nf = qjs_partes(linha, len, ini, lens);
            if (linha[ini[0]] == 'D' && nf >= 3) {
                p->dom_corpo = qjs_num(linha + ini[1], lens[1]);
                p->dom_documento = qjs_num(linha + ini[2], lens[2]);
            } else if (linha[ini[0]] == 'N' && nf >= 8) {
                int id = qjs_num(linha + ini[1], lens[1]);
                int pai = qjs_num(linha + ini[2], lens[2]);
                int na = qjs_num(linha + ini[6], lens[6]);
                if (id < 0 || id >= QJS_DOM_MAX || p->dom_existe[id]) {
                    /* o no nao cabe ou repete o identificador de outro: salta-o
                     * (e os seus atributos, que vem nas linhas seguintes) */
                    pular_atr = na > 0 ? na : 0;
                }
                else {
                    qjs_dom_el_t *e = &p->dom[id];
                    memset(e, 0, sizeof *e);
                    e->id = id;
                    e->pai = pai;
                    e->onclick = JS_NULL;
                    e->obj = JS_UNDEFINED;
                    char *tag = qjs_campo(linha + ini[3], lens[3]);
                    char *id_attr = qjs_campo(linha + ini[4], lens[4]);
                    char *classe = qjs_campo(linha + ini[5], lens[5]);
                    char *texto = qjs_campo(linha + ini[7], lens[7]);
                    copia_texto(e->tag, tag ? tag : "", sizeof e->tag);
                    e->e_texto = (e->tag[0] == '#') ? 1 : 0;
                    e->id_attr = id_attr;
                    e->classe = classe;
                    e->texto = texto;
                    if (tag) libera(tag);
                    p->dom_existe[id] = 1;
                    /* o pai ja foi lido (a lista vai em ordem de documento) */
                    if (pai >= 0 && pai < QJS_DOM_MAX && p->dom_existe[pai] &&
                        qjs_pai_valido(p, pai, id)) {
                        if (p->dom_ultimo[pai] >= 0) p->dom_prox[p->dom_ultimo[pai]] = id;
                        else p->dom_primeiro[pai] = id;
                        p->dom_ultimo[pai] = id;
                    }
                    pular_atr = 0;
                    /* os atributos vem nas linhas seguintes */
                    for (int k = 0; k < na; k++) {
                        int prox = fim + 1;
                        int f2 = prox;
                        while (f2 < n && fonte[f2] != '\n') f2++;
                        if (f2 >= n) { k = na; break; }
                        int i2[12]; int l2[12];
                        int nf2 = qjs_partes(fonte + prox, f2 - prox, i2, l2);
                        if (nf2 >= 3 && fonte[prox + i2[0]] == 'A' && e->n_atr < QJS_DOM_ATR) {
                            e->atr_nome[e->n_atr] = qjs_campo(fonte + prox + i2[1], l2[1]);
                            e->atr_valor[e->n_atr] = qjs_campo(fonte + prox + i2[2], l2[2]);
                            e->n_atr++;
                        }
                        fim = f2;
                        i = prox;
                    }
                    pular_atr = 0;   /* as linhas dos atributos ja foram lidas */
                }
            }
        } else if (pular_atr > 0) pular_atr--;
        i = fim + 1;
    }
    /* o textContent de cada elemento */
    for (int k = 0; k < QJS_DOM_MAX; k++) {
        if (!p->dom_existe[k] || p->dom[k].e_texto) continue;
        qjs_prof = 0;
        int t = qjs_dom_tam(p, k);
        char *o = aloca((size_t)t + 1);
        if (!o) continue;
        int j = 0;
        qjs_prof = 0;
        qjs_dom_txt(p, k, o, &j);
        o[j] = 0;
        p->dom[k].conteudo = o;
    }
}

/* ---- os objectos dos elementos ---- */

static JSValue qjs_dom_objeto(qjs_ponte_t *p, int idx);
static JSValue qjs_el_query(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv);
static JSValue qjs_el_query_all(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv);

/* As tarefas pendentes do motor (as continuacoes das promessas: then, await). Sem isto um
 * fetch().then() nunca corria. Com teto, para um script nao prender o navegador (D271). */
static void qjs_tarefas(qjs_ponte_t *p)
{
    JSContext *c1;
    for (int i = 0; i < 2000; i++) {
        int r = JS_ExecutePendingJob(p->rt, &c1);
        if (r <= 0) {
            if (r < 0) { JSValue exc = JS_GetException(c1); JS_FreeValue(c1, exc); }
            else break;
        }
    }
}

static char *qjs_dup(const char *s)
{
    size_t n = strlen(s);
    char *o = aloca(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* O indice do elemento de um objecto (a propriedade escondida "__dom"). */
static int qjs_dom_de(JSContext *ctx, JSValueConst este)
{
    JSValue v = JS_GetPropertyStr(ctx, este, "__dom");
    int32_t i = -1;
    if (!JS_IsException(v) && JS_IsNumber(v)) JS_ToInt32(ctx, &i, v);
    JS_FreeValue(ctx, v);
    return (int)i;
}

static JSValue qjs_el_tag(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    char mai[12];
    int k = 0;
    for (const char *q = p->dom[i].tag; *q && k < 11; q++)
        mai[k++] = (*q >= 'a' && *q <= 'z') ? (char)(*q - 32) : *q;
    mai[k] = 0;
    return JS_NewString(ctx, mai);
}

static JSValue qjs_el_id(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    return JS_NewString(ctx, p->dom[i].id_attr ? p->dom[i].id_attr : "");
}

static JSValue qjs_el_classe(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    return JS_NewString(ctx, p->dom[i].classe ? p->dom[i].classe : "");
}

static JSValue qjs_el_texto(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    return JS_NewString(ctx, p->dom[i].conteudo ? p->dom[i].conteudo : "");
}

static JSValue qjs_el_atributo(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i] || argc < 1) return JS_NULL;
    const char *nome = JS_ToCString(ctx, argv[0]);
    if (!nome) return JS_NULL;
    JSValue r = JS_NULL;
    for (int k = 0; k < p->dom[i].n_atr; k++)
        if (qjs_igual(p->dom[i].atr_nome[k], nome)) { r = JS_NewString(ctx, p->dom[i].atr_valor[k]); break; }
    JS_FreeCString(ctx, nome);
    return r;
}

static JSValue qjs_el_filhos(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    JSValue arr = JS_NewArray(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return arr;
    uint32_t k = 0;
    for (int c = p->dom_primeiro[i]; c >= 0; c = p->dom_prox[c]) {
        if (p->dom[c].e_texto) continue;
        JS_SetPropertyUint32(ctx, arr, k++, qjs_dom_objeto(p, c));
    }
    return arr;
}

static JSValue qjs_el_pai(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_NULL;
    int pai = p->dom[i].pai;
    if (pai < 0 || pai >= QJS_DOM_MAX || !p->dom_existe[pai]) return JS_NULL;
    return qjs_dom_objeto(p, pai);
}

/* ---- os seletores ---- */

static bool qjs_sel_parse(const char *s, qjs_seletor_t *sel)
{
    sel->n = 0;
    while (*s && sel->n < QJS_SEL_MAX) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        qjs_sel_parte_t *q = &sel->p[sel->n];
        memset(q, 0, sizeof *q);
        int k = 0;
        while (*s && *s != '.' && *s != '#' && *s != ' ' && k < 11) {
            char c = *s++;
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            q->tag[k++] = c;
        }
        q->tag[k] = 0;
        while (*s && *s != ' ') {
            if (*s == '.') {
                s++;
                k = 0;
                while (*s && *s != '.' && *s != '#' && *s != ' ' && k < 63) q->classe[k++] = *s++;
                q->classe[k] = 0;
            } else if (*s == '#') {
                s++;
                k = 0;
                while (*s && *s != '.' && *s != '#' && *s != ' ' && k < 63) q->id[k++] = *s++;
                q->id[k] = 0;
            } else s++;
        }
        sel->n++;
    }
    return sel->n > 0;
}

/* O elemento tem a classe pedida? (a lista de classes separada por espaco) */
static bool qjs_tem_classe(const char *lista, const char *classe)
{
    if (!lista || !classe) return false;
    int nc = (int)strlen(classe);
    const char *c = lista;
    while (*c) {
        while (*c == ' ' || *c == '\t') c++;
        const char *f = c;
        while (*c && *c != ' ' && *c != '\t') c++;
        if ((int)(c - f) == nc && qjs_igual_n(f, classe, nc)) return true;
    }
    return false;
}

/* O elemento casa com uma parte do seletor? */
static bool qjs_sel_casa(qjs_ponte_t *p, int i, const qjs_sel_parte_t *q)
{
    if (q->tag[0] && !qjs_igual(p->dom[i].tag, q->tag)) return false;
    if (q->id[0] && !qjs_igual(p->dom[i].id_attr ? p->dom[i].id_attr : "", q->id)) return false;
    if (q->classe[0] && !qjs_tem_classe(p->dom[i].classe, q->classe)) return false;
    return true;
}

/* O elemento casa com o seletor todo (com o combinador de descendente)? */
static bool qjs_sel_casa_tudo(qjs_ponte_t *p, int i, const qjs_seletor_t *sel)
{
    if (!qjs_sel_casa(p, i, &sel->p[sel->n - 1])) return false;
    int parte = sel->n - 2;
    int a = p->dom[i].pai;
    while (parte >= 0 && a >= 0 && a < QJS_DOM_MAX && p->dom_existe[a]) {
        if (qjs_sel_casa(p, a, &sel->p[parte])) parte--;
        a = p->dom[a].pai;
    }
    return parte < 0;
}

/* Procura os elementos que casam com o seletor. `raiz` = -1 para a pagina
 * toda; senao, so os descendentes desse elemento. */
static int qjs_sel_procura(qjs_ponte_t *p, int raiz, const qjs_seletor_t *sel,
                            int *achados, int max)
{
    int n = 0;
    for (int i = 0; i < QJS_DOM_MAX; i++) {
        if (!p->dom_existe[i] || p->dom[i].e_texto) continue;
        if (raiz >= 0) {
            bool dentro = false;
            int a = p->dom[i].pai;
            while (a >= 0 && a < QJS_DOM_MAX && p->dom_existe[a]) {
                if (a == raiz) { dentro = true; break; }
                a = p->dom[a].pai;
            }
            if (!dentro) continue;
        }
        if (!qjs_sel_casa_tudo(p, i, sel)) continue;
        if (achados && n < max) achados[n] = i;
        n++;
    }
    return n;
}

/* O seletor do script, com a raiz onde se procura (-1 = a pagina toda).
 * Devolve o indice do primeiro elemento (ou -1), ou o numero de elementos
 * se `todos` for verdadeiro. */
static int qjs_query(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv,
                     int raiz, bool todos)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p) return todos ? 0 : -1;
    const char *s = argc > 0 ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!s) return todos ? 0 : -1;
    qjs_seletor_t sel;
    int achados[QJS_DOM_MAX];
    int n = 0;
    if (qjs_sel_parse(s, &sel)) n = qjs_sel_procura(p, raiz, &sel, achados, QJS_DOM_MAX);
    JS_FreeCString(ctx, s);
    if (!todos) return n > 0 ? achados[0] : -1;
    return n;
}

/* ---- o document ---- */

static JSValue qjs_doc_por_id(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p || argc < 1) return JS_NULL;
    const char *id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    JSValue r = JS_NULL;
    for (int i = 0; i < QJS_DOM_MAX; i++)
        if (p->dom_existe[i] && p->dom[i].id_attr && qjs_igual(p->dom[i].id_attr, id)) {
            r = qjs_dom_objeto(p, i);
            break;
        }
    JS_FreeCString(ctx, id);
    return r;
}

static JSValue qjs_doc_por_tag(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (!p || argc < 1) return arr;
    const char *tag = JS_ToCString(ctx, argv[0]);
    if (!tag) return arr;
    uint32_t k = 0;
    for (int i = 0; i < QJS_DOM_MAX; i++) {
        if (!p->dom_existe[i] || p->dom[i].e_texto) continue;
        if (!qjs_igual(p->dom[i].tag, tag)) continue;
        JS_SetPropertyUint32(ctx, arr, k++, qjs_dom_objeto(p, i));
    }
    JS_FreeCString(ctx, tag);
    return arr;
}

static JSValue qjs_doc_query(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int achado = qjs_query(ctx, este, argc, argv, -1, false);
    if (achado < 0) return JS_NULL;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return qjs_dom_objeto(p, achado);
}

static JSValue qjs_doc_query_all(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (!p) return arr;
    const char *s = argc > 0 ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!s) return arr;
    qjs_seletor_t sel;
    int achados[QJS_DOM_MAX];
    int n = 0;
    if (qjs_sel_parse(s, &sel)) n = qjs_sel_procura(p, -1, &sel, achados, QJS_DOM_MAX);
    JS_FreeCString(ctx, s);
    for (int i = 0; i < n; i++) JS_SetPropertyUint32(ctx, arr, (uint32_t)i, qjs_dom_objeto(p, achados[i]));
    return arr;
}

/* O document.body: o elemento do corpo da pagina (ou o objecto antigo, se
 * a pagina nao tem DOM). */
static JSValue qjs_doc_body(JSContext *ctx, JSValueConst este)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p) return JS_NULL;
    if (p->dom_corpo >= 0 && p->dom_corpo < QJS_DOM_MAX && p->dom_existe[p->dom_corpo])
        return qjs_dom_objeto(p, p->dom_corpo);
    return JS_DupValue(ctx, p->corpo);
}

/* O document.documentElement: o <html> da pagina. */
static JSValue qjs_doc_documento(JSContext *ctx, JSValueConst este)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p) return JS_NULL;
    if (p->dom_documento >= 0 && p->dom_documento < QJS_DOM_MAX && p->dom_existe[p->dom_documento])
        return qjs_dom_objeto(p, p->dom_documento);
    return JS_NULL;
}

/* ---- o tratador de clique ---- */

static JSValue qjs_el_onclick_get(JSContext *ctx, JSValueConst este)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_NULL;
    return JS_DupValue(ctx, p->dom[i].onclick);
}

static JSValue qjs_el_onclick_set(JSContext *ctx, JSValueConst este, JSValueConst val)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    JS_FreeValue(ctx, p->dom[i].onclick);
    p->dom[i].onclick = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

static JSValue qjs_el_ouvinte(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int i = qjs_dom_de(ctx, este);
    if (!p || i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i] || argc < 2) return JS_UNDEFINED;
    const char *tipo = JS_ToCString(ctx, argv[0]);
    if (!tipo) return JS_UNDEFINED;
    if (qjs_igual(tipo, "click") && JS_IsFunction(ctx, argv[1])) {
        qjs_dom_el_t *e = &p->dom[i];
        if (e->n_ouv < QJS_DOM_OUV) {
            e->ouv[e->n_ouv] = JS_DupValue(ctx, argv[1]);
            e->n_ouv++;
        }
    }
    JS_FreeCString(ctx, tipo);
    return JS_UNDEFINED;
}

/* O evento: o tipo, o alvo e o preventDefault. */
static JSValue qjs_ev_prevent(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    (void)este; (void)argc; (void)argv;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (p) p->evento_cancelado = 1;
    return JS_UNDEFINED;
}

static JSValue qjs_ev_default(JSContext *ctx, JSValueConst este)
{
    (void)este;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return JS_NewBool(ctx, p && p->evento_cancelado);
}

static JSValue qjs_evento(qjs_ponte_t *p, int idx)
{
    JSContext *ctx = p->ctx;
    JSValue ev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ev, "type", JS_NewString(ctx, "click"));
    JS_SetPropertyStr(ctx, ev, "target", qjs_dom_objeto(p, idx));
    JS_SetPropertyStr(ctx, ev, "preventDefault", JS_NewCFunction(ctx, qjs_ev_prevent, "preventDefault", 0));
    JSCFunctionType ft;
    ft.getter = qjs_ev_default;
    JSValue g = JS_NewCFunction2(ctx, ft.generic, "defaultPrevented", 0, JS_CFUNC_getter, 0);
    JSAtom a = JS_NewAtom(ctx, "defaultPrevented");
    JS_DefinePropertyGetSet(ctx, ev, a, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a);
    return ev;
}

/* Cria (se preciso) o objecto JavaScript de um elemento. */
static JSValue qjs_dom_objeto(qjs_ponte_t *p, int idx)
{
    JSContext *ctx = p->ctx;
    if (idx < 0 || idx >= QJS_DOM_MAX || !p->dom_existe[idx]) return JS_NULL;
    qjs_dom_el_t *e = &p->dom[idx];
    if (e->tem_obj) return JS_DupValue(ctx, e->obj);
    JSValue obj = JS_NewObject(ctx);
    /* o identificador do no, escondido (e o que o localiza) */
    JSAtom a_dom = JS_NewAtom(ctx, "__dom");
    JS_DefinePropertyValue(ctx, obj, a_dom, JS_NewInt32(ctx, idx), JS_PROP_CONFIGURABLE);
    JS_FreeAtom(ctx, a_dom);
    /* as propriedades que sao getters */
    JSCFunctionType ft;
    ft.getter = qjs_el_tag;
    JSValue g = JS_NewCFunction2(ctx, ft.generic, "tagName", 0, JS_CFUNC_getter, 0);
    JSAtom at = JS_NewAtom(ctx, "tagName");
    JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    ft.getter = qjs_el_id;
    g = JS_NewCFunction2(ctx, ft.generic, "id", 0, JS_CFUNC_getter, 0);
    at = JS_NewAtom(ctx, "id");
    JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    ft.getter = qjs_el_classe;
    g = JS_NewCFunction2(ctx, ft.generic, "className", 0, JS_CFUNC_getter, 0);
    at = JS_NewAtom(ctx, "className");
    JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    ft.getter = qjs_el_filhos;
    g = JS_NewCFunction2(ctx, ft.generic, "children", 0, JS_CFUNC_getter, 0);
    at = JS_NewAtom(ctx, "children");
    JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    ft.getter = qjs_el_pai;
    g = JS_NewCFunction2(ctx, ft.generic, "parentElement", 0, JS_CFUNC_getter, 0);
    at = JS_NewAtom(ctx, "parentElement");
    JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    /* o onclick: getter e setter (o valor guardado e o tratador) */
    ft.getter = qjs_el_onclick_get;
    g = JS_NewCFunction2(ctx, ft.generic, "onclick", 0, JS_CFUNC_getter, 0);
    ft.setter = qjs_el_onclick_set;
    JSValue s = JS_NewCFunction2(ctx, ft.generic, "onclick", 1, JS_CFUNC_setter, 0);
    at = JS_NewAtom(ctx, "onclick");
    JS_DefinePropertyGetSet(ctx, obj, at, g, s, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, at);
    /* as funcoes */
    JS_SetPropertyStr(ctx, obj, "getAttribute", JS_NewCFunction(ctx, qjs_el_atributo, "getAttribute", 1));
    JS_SetPropertyStr(ctx, obj, "addEventListener", JS_NewCFunction(ctx, qjs_el_ouvinte, "addEventListener", 2));
    JS_SetPropertyStr(ctx, obj, "querySelector", JS_NewCFunction(ctx, qjs_el_query, "querySelector", 1));
    JS_SetPropertyStr(ctx, obj, "querySelectorAll", JS_NewCFunction(ctx, qjs_el_query_all, "querySelectorAll", 1));
    /* o corpo da pagina: o innerHTML e o textContent trocam o conteudo */
    if (idx == p->dom_corpo) {
        ft.getter = qjs_corpo_get;
        g = JS_NewCFunction2(ctx, ft.generic, "innerHTML", 0, JS_CFUNC_getter, 0);
        ft.setter = qjs_corpo_set;
        s = JS_NewCFunction2(ctx, ft.generic, "innerHTML", 1, JS_CFUNC_setter, 0);
        at = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, obj, at, g, s, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, at);
        ft.getter = qjs_corpo_get;
        g = JS_NewCFunction2(ctx, ft.generic, "textContent", 0, JS_CFUNC_getter, 0);
        ft.setter = qjs_corpo_set;
        s = JS_NewCFunction2(ctx, ft.generic, "textContent", 1, JS_CFUNC_setter, 0);
        at = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, obj, at, g, s, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, at);
    } else {
        ft.getter = qjs_el_texto;
        g = JS_NewCFunction2(ctx, ft.generic, "textContent", 0, JS_CFUNC_getter, 0);
        at = JS_NewAtom(ctx, "textContent");
        JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, at);
        ft.getter = qjs_el_texto;
        g = JS_NewCFunction2(ctx, ft.generic, "innerHTML", 0, JS_CFUNC_getter, 0);
        at = JS_NewAtom(ctx, "innerHTML");
        JS_DefinePropertyGetSet(ctx, obj, at, g, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, at);
    }
    e->obj = obj;
    e->tem_obj = 1;
    {   /* D271: o que o prelude poe em cada elemento (classList, style, dataset...) */
        JSValue gl = JS_GetGlobalObject(ctx);
        JSValue fn = JS_GetPropertyStr(ctx, gl, "__sagui_el");
        if (JS_IsFunction(ctx, fn)) {
            JSValue a[1] = { obj };
            JSValue r = JS_Call(ctx, fn, JS_UNDEFINED, 1, a);
            if (JS_IsException(r)) { JSValue exc = JS_GetException(ctx); JS_FreeValue(ctx, exc); }
            JS_FreeValue(ctx, r);
        }
        JS_FreeValue(ctx, fn);
        JS_FreeValue(ctx, gl);
    }
    return JS_DupValue(ctx, obj);
}

/* __sagui_atr(elemento, nome, valor|null): o script muda um atributo (D271). O ponte
 * guarda-o (os seletores veem a classe e o id novos) e avisa o navegador. */
static JSValue qjs_nat_atr(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p || argc < 3) return JS_UNDEFINED;
    int i = qjs_dom_de(ctx, argv[0]);
    if (i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i]) return JS_UNDEFINED;
    const char *nome = JS_ToCString(ctx, argv[1]);
    const char *valor = (JS_IsNull(argv[2]) || JS_IsUndefined(argv[2])) ? NULL : JS_ToCString(ctx, argv[2]);
    if (nome && nome[0] && strlen(nome) < 64 && (!valor || strlen(valor) < 4096)) {
        qjs_dom_el_t *e = &p->dom[i];
        if (qjs_igual(nome, "class")) { char *n2 = qjs_dup(valor ? valor : ""); if (n2) { if (e->classe) libera(e->classe); e->classe = n2; } }
        else if (qjs_igual(nome, "id")) { char *n2 = qjs_dup(valor ? valor : ""); if (n2) { if (e->id_attr) libera(e->id_attr); e->id_attr = n2; } }
        int k = 0;
        while (k < e->n_atr && !qjs_igual(e->atr_nome[k], nome)) k++;
        if (!valor) {
            if (k < e->n_atr) {
                libera(e->atr_nome[k]); libera(e->atr_valor[k]);
                for (; k + 1 < e->n_atr; k++) { e->atr_nome[k] = e->atr_nome[k + 1]; e->atr_valor[k] = e->atr_valor[k + 1]; }
                e->n_atr--;
                e->atr_nome[e->n_atr] = e->atr_valor[e->n_atr] = NULL;
            }
        } else if (k < e->n_atr) { char *n2 = qjs_dup(valor); if (n2) { libera(e->atr_valor[k]); e->atr_valor[k] = n2; } }
        else if (e->n_atr < QJS_DOM_ATR) {
            char *a = qjs_dup(nome), *b = qjs_dup(valor);
            if (a && b) { e->atr_nome[e->n_atr] = a; e->atr_valor[e->n_atr] = b; e->n_atr++; }
            else { if (a) libera(a); if (b) libera(b); }
        }
        if (p->g.atributo) p->g.atributo(e->id, nome, valor);
    }
    if (nome) JS_FreeCString(ctx, nome);
    if (valor) JS_FreeCString(ctx, valor);
    return JS_UNDEFINED;
}

/* __sagui_texto(elemento, texto): elemento.textContent = texto (D271). */
static JSValue qjs_nat_texto(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p || argc < 2) return JS_UNDEFINED;
    int i = qjs_dom_de(ctx, argv[0]);
    if (i < 0 || i >= QJS_DOM_MAX || !p->dom_existe[i] || i == p->dom_corpo || i == p->dom_documento) return JS_UNDEFINED;
    const char *txt = JS_ToCString(ctx, argv[1]);
    if (txt && strlen(txt) < 16384) {
        char *n2 = qjs_dup(txt);
        if (n2) { if (p->dom[i].conteudo) libera(p->dom[i].conteudo); p->dom[i].conteudo = n2; }
        if (p->g.texto) p->g.texto(p->dom[i].id, txt);
    }
    if (txt) JS_FreeCString(ctx, txt);
    return JS_UNDEFINED;
}

/* __sagui_baixar(url): o corpo de um recurso, como texto, ou null (fetch e XMLHttpRequest).
 * Ate 16 pedidos por pagina, para um script nao prender o navegador a descarregar. */
static JSValue qjs_nat_baixar(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (!p || argc < 1 || !p->g.baixar || p->n_baixados >= 16) return JS_NULL;
    const char *url = JS_ToCString(ctx, argv[0]);
    if (!url) return JS_NULL;
    JSValue r = JS_NULL;
    if (strlen(url) < 1024) {
        int n = 0;
        p->n_baixados++;
        char *d = p->g.baixar(url, &n);
        if (d) { r = JS_NewStringLen(ctx, d, n > 0 ? (size_t)n : 0); libera(d); }
    }
    JS_FreeCString(ctx, url);
    return r;
}

/* O querySelector e o querySelectorAll de um elemento. */
static JSValue qjs_el_query(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int raiz = qjs_dom_de(ctx, este);
    int achado = qjs_query(ctx, este, argc, argv, raiz, false);
    if (achado < 0 || !p) return JS_NULL;
    return qjs_dom_objeto(p, achado);
}

static JSValue qjs_el_query_all(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (!p) return arr;
    int raiz = qjs_dom_de(ctx, este);
    const char *s = argc > 0 ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!s) return arr;
    qjs_seletor_t sel;
    int achados[QJS_DOM_MAX];
    int n = 0;
    if (qjs_sel_parse(s, &sel)) n = qjs_sel_procura(p, raiz, &sel, achados, QJS_DOM_MAX);
    JS_FreeCString(ctx, s);
    for (int i = 0; i < n; i++) JS_SetPropertyUint32(ctx, arr, (uint32_t)i, qjs_dom_objeto(p, achados[i]));
    return arr;
}

/* O clique num elemento: chama o tratador. Devolve 1 se foi cancelado.
 * O evento sobe como no navegador: se o elemento nao tem tratador, procura
 * nos avos (e o "this" e o do avo que tem o tratador). */
int qjs_clique(qjs_ponte_t *p, int dom)
{
    if (!p || !p->ctx) return 0;
    if (dom < 0 || dom >= QJS_DOM_MAX || !p->dom_existe[dom]) return 0;
    int alvo = -1;
    for (int i = dom, guarda = 0; i >= 0 && i < QJS_DOM_MAX && p->dom_existe[i] && guarda < 64;
         i = p->dom[i].pai, guarda++) {
        qjs_dom_el_t *q = &p->dom[i];
        if (q->n_ouv > 0 || JS_IsFunction(p->ctx, q->onclick)) { alvo = i; break; }
    }
    if (alvo < 0) return 0;      /* ninguem atende a este clique */
    qjs_dom_el_t *e = &p->dom[alvo];
    p->evento_cancelado = 0;
    JSValue ev = qjs_evento(p, dom);         /* o alvo do evento: o elemento clicado */
    JSValue este = qjs_dom_objeto(p, alvo);   /* o "this": o elemento com o tratador */
    JSValue args[1];
    if (JS_IsFunction(p->ctx, e->onclick)) {
        args[0] = JS_DupValue(p->ctx, ev);
        JSValue r = JS_Call(p->ctx, e->onclick, este, 1, args);
        if (JS_IsException(r)) {
            JSValue exc = JS_GetException(p->ctx);
            const char *msg = JS_ToCString(p->ctx, exc);
            registrar("qjs: erro no tratador de clique: %s\n", msg ? msg : "?");
            if (msg) JS_FreeCString(p->ctx, msg);
            JS_FreeValue(p->ctx, exc);
        }
        JS_FreeValue(p->ctx, r);
        JS_FreeValue(p->ctx, args[0]);
    }
    for (int k = 0; k < e->n_ouv; k++) {
        args[0] = JS_DupValue(p->ctx, ev);
        JSValue r = JS_Call(p->ctx, e->ouv[k], este, 1, args);
        if (JS_IsException(r)) {
            JSValue exc = JS_GetException(p->ctx);
            const char *msg = JS_ToCString(p->ctx, exc);
            registrar("qjs: erro no ouvinte de clique: %s\n", msg ? msg : "?");
            if (msg) JS_FreeCString(p->ctx, msg);
            JS_FreeValue(p->ctx, exc);
        }
        JS_FreeValue(p->ctx, r);
        JS_FreeValue(p->ctx, args[0]);
    }
    JS_FreeValue(p->ctx, ev);
    JS_FreeValue(p->ctx, este);
    qjs_tarefas(p);
    return p->evento_cancelado ? 1 : 0;
}

/* Os elementos que tem um tratador de clique. */
int qjs_clique_ids(qjs_ponte_t *p, int *ids, int max)
{
    if (!p || !p->ctx) return 0;
    int n = 0;
    for (int i = 0; i < QJS_DOM_MAX; i++) {
        if (!p->dom_existe[i]) continue;
        qjs_dom_el_t *e = &p->dom[i];
        if (e->n_ouv <= 0 && !JS_IsFunction(p->ctx, e->onclick)) continue;
        if (ids && n < max) ids[n] = i;
        n++;
    }
    return n;
}

/* ------------------------------------------------------------ a janela */

/* O script pediu outra pagina. Quem navega e o browser (o Cipo), nao o
 * motor: a ponte so avisa, e o browser trata quando o script ja acabou
 * (navegar durante o script destruiria o motor que esta a correr). */
static void qjs_pedir_pagina(qjs_ponte_t *p, const char *url)
{
    if (!p || !url || !url[0]) return;
    registrar("qjs: a pagina pediu ir a %s\n", url);
    if (p->g.navegar) p->g.navegar(url);
}

/* location.href: ler e escrever. */
static JSValue qjs_local_href_get(JSContext *ctx, JSValueConst this_val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return JS_NewString(ctx, p->url);
}

static JSValue qjs_local_href_set(JSContext *ctx, JSValueConst this_val,
                                   JSValueConst val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    const char *s = JS_ToCString(ctx, val);
    if (s) { qjs_pedir_pagina(p, s); JS_FreeCString(ctx, s); }
    return JS_UNDEFINED;
}

/* location.assign(url) e location.replace(url): vao para a outra pagina. */
static JSValue qjs_local_ir(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    const char *s = argc > 0 ? JS_ToCString(ctx, argv[0]) : NULL;
    if (s) { qjs_pedir_pagina(p, s); JS_FreeCString(ctx, s); }
    return JS_UNDEFINED;
}

/* location.reload(): volta a carregar a mesma pagina. */
static JSValue qjs_local_recarregar(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv)
{
    (void)this_val; (void)argc; (void)argv;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    qjs_pedir_pagina(p, p->url);
    return JS_UNDEFINED;
}

/* document.URL: a pagina que esta aberta. */
static JSValue qjs_doc_url_get(JSContext *ctx, JSValueConst this_val)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    return JS_NewString(ctx, p->url);
}

/* ------------------------------------------------------------ temporizadores */

static JSValue qjs_js_set_timeout(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_UNDEFINED;
    if (p->n_tmp >= QJS_TIMER_MAX) return JS_UNDEFINED;
    int64_t ms = 0;
    if (argc > 1) JS_ToInt64(ctx, &ms, argv[1]);
    if (ms < 0) ms = 0;
    p->tmp[p->n_tmp].fn = JS_DupValue(ctx, argv[0]);
    p->tmp[p->n_tmp].vence = p->agora + (unsigned)ms;
    p->tmp[p->n_tmp].id = ++p->prox_id;
    p->n_tmp++;
    return JS_NewInt32(ctx, p->tmp[p->n_tmp - 1].id);
}

static JSValue qjs_js_clear_timeout(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
{
    (void)this_val;
    qjs_ponte_t *p = JS_GetContextOpaque(ctx);
    int32_t id = 0;
    if (argc < 1) return JS_UNDEFINED;
    JS_ToInt32(ctx, &id, argv[0]);
    for (int i = 0; i < p->n_tmp; i++) {
        if (p->tmp[i].id == id) {
            JS_FreeValue(ctx, p->tmp[i].fn);
            p->tmp[i] = p->tmp[p->n_tmp - 1];
            p->n_tmp--;
            return JS_UNDEFINED;
        }
    }
    return JS_UNDEFINED;
}

/* O que a pagina tinha antes do script rodar: o titulo e o corpo. */
void qjs_definir_titulo(qjs_ponte_t *p, const char *t)
{
    if (!p) return;
    copia_texto(p->titulo, t ? t : "", sizeof p->titulo);
}

void qjs_definir_corpo(qjs_ponte_t *p, const char *html)
{
    if (!p) return;
    copia_texto(p->corpo_html, html ? html : "", sizeof p->corpo_html);
}

void qjs_definir_url(qjs_ponte_t *p, const char *url)
{
    if (!p) return;
    copia_texto(p->url, url ? url : "", sizeof p->url);
}

/* ------------------------------------------------------------ criar e destruir */

static void qjs_preparar(qjs_ponte_t *p)
{
    JSContext *ctx = p->ctx;
    JSValue global = JS_GetGlobalObject(ctx);

    /* console.log */
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, qjs_js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, console, "info", JS_NewCFunction(ctx, qjs_js_console_log, "info", 1));
    JS_SetPropertyStr(ctx, console, "warn", JS_NewCFunction(ctx, qjs_js_console_log, "warn", 1));
    JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, qjs_js_console_log, "error", 1));
    JS_SetPropertyStr(ctx, global, "console", console);

    /* alert */
    JS_SetPropertyStr(ctx, global, "alert", JS_NewCFunction(ctx, qjs_js_alerta, "alert", 1));

    /* os temporizadores */
    JS_SetPropertyStr(ctx, global, "setTimeout", JS_NewCFunction(ctx, qjs_js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, global, "clearTimeout", JS_NewCFunction(ctx, qjs_js_clear_timeout, "clearTimeout", 1));

    /* document.title e document.body (com getter e setter). O getter e o
     * setter sao funcoes C com protocolo proprio (JS_CFUNC_getter e
     * JS_CFUNC_setter): o motor chama o getter com (ctx, this) e o setter
     * com (ctx, this, valor). */
    JSValue doc = JS_NewObject(ctx);
    JSCFunctionType ft;
    ft.getter = qjs_titulo_get;
    JSValue g_tit = JS_NewCFunction2(ctx, ft.generic, "title", 0, JS_CFUNC_getter, 0);
    ft.setter = qjs_titulo_set;
    JSValue s_tit = JS_NewCFunction2(ctx, ft.generic, "title", 1, JS_CFUNC_setter, 0);
    JSAtom a_tit = JS_NewAtom(ctx, "title");
    JS_DefinePropertyGetSet(ctx, doc, a_tit, g_tit, s_tit,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_tit);
    JSValue corpo = JS_NewObject(ctx);
    ft.getter = qjs_corpo_get;
    JSValue g_cor = JS_NewCFunction2(ctx, ft.generic, "innerHTML", 0, JS_CFUNC_getter, 0);
    ft.setter = qjs_corpo_set;
    JSValue s_cor = JS_NewCFunction2(ctx, ft.generic, "innerHTML", 1, JS_CFUNC_setter, 0);
    JSAtom a_cor = JS_NewAtom(ctx, "innerHTML");
    JS_DefinePropertyGetSet(ctx, corpo, a_cor, g_cor, s_cor,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_cor);
    ft.getter = qjs_corpo_get;
    JSValue g_txt = JS_NewCFunction2(ctx, ft.generic, "textContent", 0, JS_CFUNC_getter, 0);
    ft.setter = qjs_corpo_set;
    JSValue s_txt = JS_NewCFunction2(ctx, ft.generic, "textContent", 1, JS_CFUNC_setter, 0);
    JSAtom a_txt = JS_NewAtom(ctx, "textContent");
    JS_DefinePropertyGetSet(ctx, corpo, a_txt, g_txt, s_txt,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_txt);
    /* o document.body: o elemento do corpo da pagina, ou (se a pagina nao
     * tem DOM) o objecto de sempre (D160) */
    ft.getter = qjs_doc_body;
    JSValue g_body = JS_NewCFunction2(ctx, ft.generic, "body", 0, JS_CFUNC_getter, 0);
    JSAtom a_body = JS_NewAtom(ctx, "body");
    JS_DefinePropertyGetSet(ctx, doc, a_body, g_body, JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_body);
    /* o document.documentElement: o <html> da pagina */
    ft.getter = qjs_doc_documento;
    JSValue g_doc = JS_NewCFunction2(ctx, ft.generic, "documentElement", 0, JS_CFUNC_getter, 0);
    JSAtom a_doc = JS_NewAtom(ctx, "documentElement");
    JS_DefinePropertyGetSet(ctx, doc, a_doc, g_doc, JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_doc);
    /* as consultas ao DOM da pagina (D160) */
    JS_SetPropertyStr(ctx, doc, "getElementById", JS_NewCFunction(ctx, qjs_doc_por_id, "getElementById", 1));
    JS_SetPropertyStr(ctx, doc, "getElementsByTagName", JS_NewCFunction(ctx, qjs_doc_por_tag, "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, doc, "querySelector", JS_NewCFunction(ctx, qjs_doc_query, "querySelector", 1));
    JS_SetPropertyStr(ctx, doc, "querySelectorAll", JS_NewCFunction(ctx, qjs_doc_query_all, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, global, "document", doc);
    p->corpo = JS_DupValue(ctx, corpo);

    /* document.URL (a pagina que esta aberta) */
    ft.getter = qjs_doc_url_get;
    JSValue g_url = JS_NewCFunction2(ctx, ft.generic, "URL", 0, JS_CFUNC_getter, 0);
    JSAtom a_url = JS_NewAtom(ctx, "URL");
    JS_DefinePropertyGetSet(ctx, doc, a_url, g_url, JS_UNDEFINED,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_url);

    /* location: de onde estamos e para onde vamos. */
    JSValue loc = JS_NewObject(ctx);
    ft.getter = qjs_local_href_get;
    JSValue g_href = JS_NewCFunction2(ctx, ft.generic, "href", 0, JS_CFUNC_getter, 0);
    ft.setter = qjs_local_href_set;
    JSValue s_href = JS_NewCFunction2(ctx, ft.generic, "href", 1, JS_CFUNC_setter, 0);
    JSAtom a_href = JS_NewAtom(ctx, "href");
    JS_DefinePropertyGetSet(ctx, loc, a_href, g_href, s_href,
                            JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, a_href);
    JS_SetPropertyStr(ctx, loc, "assign", JS_NewCFunction(ctx, qjs_local_ir, "assign", 1));
    JS_SetPropertyStr(ctx, loc, "replace", JS_NewCFunction(ctx, qjs_local_ir, "replace", 1));
    JS_SetPropertyStr(ctx, loc, "reload", JS_NewCFunction(ctx, qjs_local_recarregar, "reload", 0));
    JS_SetPropertyStr(ctx, global, "location", loc);
    JS_SetPropertyStr(ctx, doc, "location", loc);

    /* A janela: window e o proprio objecto global. window.parent, window.top
     * e window.self tambem: a pagina de reenvio do DuckDuckGo faz
     * window.parent.location.replace("..."), e sem isto ficaria a dizer que
     * 'window' nao existe (a pagina ficava em branco). */
    JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "parent", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "top", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, global, "self", JS_DupValue(ctx, global));

    /* navigator.userAgent */
    JSValue nav = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, nav, "userAgent", JS_NewString(ctx, "SaguiOS/1.0 (Cipo)"));
    JS_SetPropertyStr(ctx, global, "navigator", nav);

    /* D271: os nativos que o prelude usa e o proprio prelude (qjs_prelude.h) */
    JS_SetPropertyStr(ctx, global, "__sagui_atr", JS_NewCFunction(ctx, qjs_nat_atr, "__sagui_atr", 3));
    JS_SetPropertyStr(ctx, global, "__sagui_texto", JS_NewCFunction(ctx, qjs_nat_texto, "__sagui_texto", 2));
    if (p->g.baixar) JS_SetPropertyStr(ctx, global, "__sagui_baixar", JS_NewCFunction(ctx, qjs_nat_baixar, "__sagui_baixar", 1));
    JSValue pv = JS_Eval(ctx, qjs_prelude, sizeof qjs_prelude - 1, "<prelude>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(pv)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        registrar("qjs: erro no prelude: %s\n", msg ? msg : "?");
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, pv);

    JS_FreeValue(ctx, global);
}

typedef struct { const qjs_ganchos_t *g; qjs_ponte_t *p; } criar_t;

/* Criar o contexto e o que mais consome pilha: o QuickJS monta ali todos os
 * objectos intrinsecos (Object, Function, Array, Math, JSON, Date...). Por
 * isso isto corre na pilha grande, e nao na do programa. */
static void qjs_criar_interna(void *arg)
{
    criar_t *c = (criar_t *)arg;
    qjs_ponte_t *p = (qjs_ponte_t *)aloca_zerado(sizeof(qjs_ponte_t));
    if (!p) return;
    if (c->g) p->g = *c->g;      /* copia: a struct da chamadora pode morrer */
    p->rt = JS_NewRuntime();
    if (!p->rt) { libera(p); return; }
    JS_SetMemoryLimit(p->rt, QJS_MEM_LIMITE);
    JS_SetMaxStackSize(p->rt, 1024 * 1024);   /* a pilha do motor, em bytes */
    p->ctx = JS_NewContext(p->rt);
    if (!p->ctx) { JS_FreeRuntime(p->rt); libera(p); return; }
    JS_SetContextOpaque(p->ctx, p);
    qjs_preparar(p);
    c->p = p;
}

qjs_ponte_t *qjs_criar(const qjs_ganchos_t *g)
{
    criar_t c = { g, NULL };
    qjs_em_pilha(qjs_criar_interna, &c);
    return c.p;
}

static void qjs_destruir_interna(void *arg)
{
    qjs_ponte_t *p = (qjs_ponte_t *)arg;
    for (int i = 0; i < p->n_tmp; i++) JS_FreeValue(p->ctx, p->tmp[i].fn);
    p->n_tmp = 0;
    qjs_dom_limpar(p);           /* o DOM da pagina (D160) */
    if (p->ctx) JS_FreeValue(p->ctx, p->corpo);
    if (p->ctx) { JS_FreeContext(p->ctx); p->ctx = NULL; }
    if (p->rt) { JS_FreeRuntime(p->rt); p->rt = NULL; }
}

void qjs_destruir(qjs_ponte_t *p)
{
    if (!p) return;
    qjs_em_pilha(qjs_destruir_interna, p);
    libera(p);
}

/* ------------------------------------------------------------ executar */

typedef struct { qjs_ponte_t *p; const char *codigo; int n; const char *nome; int erro;
                 char *saida; int max_saida; } exec_t;

static void qjs_executar_interna(void *arg)
{
    exec_t *e = (exec_t *)arg;
    JSContext *ctx = e->p->ctx;
    JSValue v = JS_Eval(ctx, e->codigo, e->n, e->nome, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        registrar("qjs: erro em %s: %s\n", e->nome, msg ? msg : "?");
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
        e->erro = -1;
    } else {
        if (e->saida && e->max_saida > 0) {
            const char *txt = JS_ToCString(ctx, v);
            copia_texto(e->saida, txt ? txt : "", (size_t)e->max_saida);
            if (txt) JS_FreeCString(ctx, txt);
        }
        JS_FreeValue(ctx, v);
    }
    qjs_tarefas(e->p);
}

/* O tamanho da janela, para innerWidth, innerHeight e matchMedia (D271). */
void qjs_definir_janela(qjs_ponte_t *p, int larg, int alt)
{
    if (!p || !p->ctx) return;
    JSValue g = JS_GetGlobalObject(p->ctx);
    JS_SetPropertyStr(p->ctx, g, "innerWidth", JS_NewInt32(p->ctx, larg));
    JS_SetPropertyStr(p->ctx, g, "innerHeight", JS_NewInt32(p->ctx, alt));
    JS_FreeValue(p->ctx, g);
}

/* A pagina acabou de carregar: DOMContentLoaded e load (D271). */
void qjs_carga(qjs_ponte_t *p)
{
    static const char cod[] = "if (typeof __sagui_carga === 'function') __sagui_carga();";
    qjs_executar(p, cod, (int)sizeof cod - 1, "<carga>");
}

int qjs_executar(qjs_ponte_t *p, const char *codigo, int n, const char *nome)
{
    if (!p || !p->ctx) return -1;
    exec_t e = { p, codigo, n, nome ? nome : "<script>", 0, NULL, 0 };
    qjs_em_pilha(qjs_executar_interna, &e);
    return e.erro;
}

/* Como qjs_executar, mas escreve o valor do resultado em `saida`. */
int qjs_executar_texto(qjs_ponte_t *p, const char *codigo, int n, const char *nome,
                        char *saida, int max_saida)
{
    if (!p || !p->ctx) return -1;
    if (saida && max_saida > 0) saida[0] = 0;
    exec_t e = { p, codigo, n, nome ? nome : "<script>", 0, saida, max_saida };
    qjs_em_pilha(qjs_executar_interna, &e);
    return e.erro;
}

/* Corre os temporizadores que ja venceram (ate 8 por vez, para o laço de
 * eventos nao ficar preso num script que se repetem). */
typedef struct { qjs_ponte_t *p; unsigned agora; int correram; } tique_t;

static void qjs_tique_interna(void *arg)
{
    tique_t *t = (tique_t *)arg;
    qjs_ponte_t *p = t->p;
    for (int i = 0; i < p->n_tmp && t->correram < 8; ) {
        if (p->tmp[i].vence > t->agora) { i++; continue; }
        JSValue fn = p->tmp[i].fn;
        p->tmp[i] = p->tmp[p->n_tmp - 1];
        p->n_tmp--;
        JSValue r = JS_Call(p->ctx, fn, JS_UNDEFINED, 0, NULL);
        if (JS_IsException(r)) {
            JSValue exc = JS_GetException(p->ctx);
            const char *msg = JS_ToCString(p->ctx, exc);
            registrar("qjs: erro num temporizador: %s\n", msg ? msg : "?");
            if (msg) JS_FreeCString(p->ctx, msg);
            JS_FreeValue(p->ctx, exc);
        }
        JS_FreeValue(p->ctx, r);
        JS_FreeValue(p->ctx, fn);
        t->correram++;
    }
    qjs_tarefas(p);
}

int qjs_tique(qjs_ponte_t *p, unsigned agora_ms)
{
    if (!p || !p->ctx) return 0;
    p->agora = agora_ms;
    tique_t t = { p, agora_ms, 0 };
    qjs_em_pilha(qjs_tique_interna, &t);
    return t.correram;
}

/* ------------------------------------------------------------ o que o script deixou */

const char *qjs_titulo(qjs_ponte_t *p) { return (p && p->tem_titulo) ? p->titulo : NULL; }
const char *qjs_corpo(qjs_ponte_t *p) { return (p && p->tem_corpo) ? p->corpo_html : NULL; }
const char *qjs_console(qjs_ponte_t *p) { return (p && p->console_txt[0]) ? p->console_txt : NULL; }
const char *qjs_alerta(qjs_ponte_t *p) { return (p && p->alerta_txt[0]) ? p->alerta_txt : NULL; }
