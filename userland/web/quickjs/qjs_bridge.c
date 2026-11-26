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
#include "qjs_dom.h"       /* D488: o DOM em JavaScript (gerado de qjs_dom.js por tools/js2h.py) */
/* D487: la red del JavaScript (qjs_rede.c); solo el Cipó la liga, por eso son débiles */
void qjs_rede_url(const char *url) __attribute__((weak));
void qjs_rede_preparar(JSContext *ctx) __attribute__((weak));

#define QJS_MEM_LIMITE (64u * 1024 * 1024)   /* o teto do motor */
#define QJS_PILHA      (2u * 1024 * 1024)     /* a pilha grande */
#define QJS_TIMER_MAX  4096                   /* temporizadores e quadros vivos de uma pagina */
#define QJS_TEXTO_MAX  8192

/* Um temporizador (setTimeout/setInterval) e um quadro (requestAnimationFrame). */
typedef struct { JSValue fn, args; unsigned vence, ms, seq; int id; uint8_t repete; } qjs_tempo_t;
typedef struct { JSValue fn; int id; } qjs_quadro_t;

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
    char url[2048];         /* a pagina que esta aberta (para o location) */
    /* o bucle de eventos (D488) */
    qjs_tempo_t *tmp; int n_tmp, cap_tmp, prox_id; unsigned seq;
    qjs_quadro_t *quadros; int n_quadros, cap_quadros, prox_quadro; unsigned ult_quadro, t0;
    unsigned inicio; int lento;     /* o script que esta a correr (para parar os que nao acabam) */
    int quer;                       /* QJS_QUER_*: ouvintes de rato ou de teclado */
    int larg, alt;                  /* a janela, se o navegador nao tem o gancho janela */
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
#ifdef QJS_ANFITRIAO
    f(arg);      /* a prova no PC (tests/web/qjs_anfitriao.c): a pilha do anfitriao chega */
    return;
#endif
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

/* ------------------------------------------------------------ o DOM (D160, D488)
 *
 * Desde a D488 a copia do DOM que o script ve vive em JavaScript (qjs_dom.js):
 * nos, eventos com captura e borbulha, seletores, medidas, history. O C so
 * da os nativos de __sagui_nat, que passam cada pedido ao navegador pelos
 * ganchos, e o bucle de eventos (temporizadores, quadros, microtarefas).
 */

/* As tarefas pendentes do motor (as continuacoes das promessas: then, await). Sem isto um
 * fetch().then() nunca corria. Com teto, para um script nao prender o navegador (D271). */
static void qjs_tarefas(qjs_ponte_t *p)
{
    JSContext *c1;
    for (int i = 0; i < 2000; i++) {
        int r = JS_ExecutePendingJob(p->rt, &c1);
        if (r <= 0) {
            if (r < 0) {
                JSValue exc = JS_GetException(c1);
                const char *msg = JS_ToCString(c1, exc);
                registrar("qjs: erro numa promessa: %s\n", msg ? msg : "?");
                if (msg) JS_FreeCString(c1, msg);
                JS_FreeValue(c1, exc);
            }
            else break;
        }
    }
}

static void qjs_excecao(qjs_ponte_t *p, const char *onde)
{
    JSValue exc = JS_GetException(p->ctx);
    const char *msg = JS_ToCString(p->ctx, exc);
    registrar("qjs: erro %s: %s\n", onde, msg ? msg : "?");
    if (msg) JS_FreeCString(p->ctx, msg);
    JS_FreeValue(p->ctx, exc);
}

/* Um texto do script (UTF-8) para o gancho; NULL se nao e texto. */
static const char *qjs_txt(JSContext *ctx, JSValueConst v)
{
    if (JS_IsNull(v) || JS_IsUndefined(v)) return NULL;
    return JS_ToCString(ctx, v);
}

static int qjs_int(JSContext *ctx, JSValueConst v, int padrao)
{
    int32_t i;
    if (JS_IsUndefined(v) || JS_ToInt32(ctx, &i, v) < 0) return padrao;
    return (int)i;
}

#define QJS_NAT(nome) static JSValue nome(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
#define QJS_P qjs_ponte_t *p = JS_GetContextOpaque(ctx); (void)este; if (!p) return JS_UNDEFINED
#define ARG(i) (argc > (i) ? argv[i] : JS_UNDEFINED)

/* criar(tag, texto) -> id do no novo (solto), ou -1 */
QJS_NAT(qjs_n_criar)
{
    QJS_P;
    if (!p->g.dom_criar) return JS_NewInt32(ctx, -1);
    const char *tag = qjs_txt(ctx, ARG(0)), *txt = qjs_txt(ctx, ARG(1));
    int id = tag ? p->g.dom_criar(tag, txt) : -1;
    if (tag) JS_FreeCString(ctx, tag);
    if (txt) JS_FreeCString(ctx, txt);
    return JS_NewInt32(ctx, id);
}

QJS_NAT(qjs_n_inserir)
{
    QJS_P;
    int r = p->g.dom_inserir ? p->g.dom_inserir(qjs_int(ctx, ARG(0), -1), qjs_int(ctx, ARG(1), -1), qjs_int(ctx, ARG(2), -1)) : 0;
    return JS_NewBool(ctx, r);
}

QJS_NAT(qjs_n_tirar)
{
    QJS_P;
    int r = p->g.dom_tirar ? p->g.dom_tirar(qjs_int(ctx, ARG(0), -1)) : 0;
    return JS_NewBool(ctx, r);
}

QJS_NAT(qjs_n_dado)
{
    QJS_P;
    const char *t = qjs_txt(ctx, ARG(1));
    if (t && p->g.dom_dado) p->g.dom_dado(qjs_int(ctx, ARG(0), -1), t);
    if (t) JS_FreeCString(ctx, t);
    return JS_UNDEFINED;
}

/* html(no, html) -> a lista dos nos novos (texto no formato do DOM), ou null */
QJS_NAT(qjs_n_html)
{
    QJS_P;
    const char *h = qjs_txt(ctx, ARG(1));
    JSValue r = JS_NULL;
    if (h && p->g.dom_html) {
        int n = 0;
        char *l = p->g.dom_html(qjs_int(ctx, ARG(0), -1), h, &n);
        if (l) { r = JS_NewStringLen(ctx, l, n > 0 ? (size_t)n : 0); libera(l); }
    }
    if (h) JS_FreeCString(ctx, h);
    return r;
}

/* atr(no, nome, valor|null): um atributo */
QJS_NAT(qjs_n_atr)
{
    QJS_P;
    const char *nome = qjs_txt(ctx, ARG(1)), *valor = qjs_txt(ctx, ARG(2));
    if (nome && nome[0] && strlen(nome) < 64 && (!valor || strlen(valor) < 65536) && p->g.atributo)
        p->g.atributo(qjs_int(ctx, ARG(0), -1), nome, valor);
    if (nome) JS_FreeCString(ctx, nome);
    if (valor) JS_FreeCString(ctx, valor);
    return JS_UNDEFINED;
}

static JSValue qjs_vetor(JSContext *ctx, const int *v, int n)
{
    JSValue a = JS_NewArray(ctx);
    for (int i = 0; i < n; i++) JS_SetPropertyUint32(ctx, a, (uint32_t)i, JS_NewInt32(ctx, v[i]));
    return a;
}

/* caixa(no) -> [16] (o navegador maqueta antes, se o DOM mudou), ou null */
QJS_NAT(qjs_n_caixa)
{
    QJS_P;
    int o[16];
    if (!p->g.caixa || !p->g.caixa(qjs_int(ctx, ARG(0), -1), o)) return JS_NULL;
    return qjs_vetor(ctx, o, 16);
}

QJS_NAT(qjs_n_estilo)
{
    QJS_P;
    const char *prop = qjs_txt(ctx, ARG(1));
    char o[256];
    o[0] = 0;
    bool ok = prop && p->g.estilo && p->g.estilo(qjs_int(ctx, ARG(0), -1), prop, o, sizeof o);
    if (prop) JS_FreeCString(ctx, prop);
    return ok ? JS_NewString(ctx, o) : JS_NULL;
}

QJS_NAT(qjs_n_em)
{
    QJS_P;
    return JS_NewInt32(ctx, p->g.elemento_em ? p->g.elemento_em(qjs_int(ctx, ARG(0), 0), qjs_int(ctx, ARG(1), 0)) : -1);
}

/* janela() -> [innerWidth, innerHeight, scrollX, scrollY, largura e altura da pagina] */
QJS_NAT(qjs_n_janela)
{
    QJS_P;
    int o[6] = { p->larg, p->alt, 0, 0, p->larg, p->alt };
    if (p->g.janela) p->g.janela(o);
    return qjs_vetor(ctx, o, 6);
}

QJS_NAT(qjs_n_rolar)
{
    QJS_P;
    if (p->g.rolar) p->g.rolar(qjs_int(ctx, ARG(0), 0), qjs_int(ctx, ARG(1), 0));
    return JS_UNDEFINED;
}

QJS_NAT(qjs_n_historia)
{
    QJS_P;
    const char *u = qjs_txt(ctx, ARG(0));
    if (u && strlen(u) < 2048) {
        copia_texto(p->url, u, sizeof p->url);
        if (p->g.historia) p->g.historia(u, qjs_int(ctx, ARG(1), 0));
    }
    if (u) JS_FreeCString(ctx, u);
    return JS_UNDEFINED;
}

QJS_NAT(qjs_n_ir)
{
    QJS_P;
    if (p->g.ir) p->g.ir(qjs_int(ctx, ARG(0), 0));
    return JS_UNDEFINED;
}

static void qjs_pedir_pagina(qjs_ponte_t *p, const char *url);

QJS_NAT(qjs_n_navegar)
{
    QJS_P;
    const char *u = qjs_txt(ctx, ARG(0));
    if (u) { qjs_pedir_pagina(p, u); JS_FreeCString(ctx, u); }
    return JS_UNDEFINED;
}

/* quer('rato' | 'tecla'): a pagina tem ouvintes do movimento do rato ou do teclado */
QJS_NAT(qjs_n_quer)
{
    QJS_P;
    const char *t = qjs_txt(ctx, ARG(0));
    if (t) {
        if (!strcmp(t, "rato")) p->quer |= QJS_QUER_RATO;
        else if (!strcmp(t, "tecla")) p->quer |= QJS_QUER_TECLA;
        JS_FreeCString(ctx, t);
    }
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

/* firma() e incremental(sim): so para as provas (h488) */
QJS_NAT(qjs_n_firma)
{
    QJS_P;
    return JS_NewInt64(ctx, p->g.firma ? (int64_t)p->g.firma() : 0);
}

QJS_NAT(qjs_n_incremental)
{
    QJS_P;
    if (p->g.incremental) p->g.incremental(qjs_int(ctx, ARG(0), 1));
    return JS_UNDEFINED;
}

/* rolagem(no) -> [x, y, max x, max y] de um bloco com overflow; rolar_caixa(no, x, y) */
QJS_NAT(qjs_n_rolagem)
{
    QJS_P;
    int o[4];
    if (!p->g.rolagem || !p->g.rolagem(qjs_int(ctx, ARG(0), -1), o)) return JS_NULL;
    return qjs_vetor(ctx, o, 4);
}

QJS_NAT(qjs_n_rolar_caixa)
{
    QJS_P;
    if (p->g.rolar_caixa) p->g.rolar_caixa(qjs_int(ctx, ARG(0), -1), qjs_int(ctx, ARG(1), 0), qjs_int(ctx, ARG(2), 0));
    return JS_UNDEFINED;
}

/* ---- o clique (D160): agora e um evento como os outros ---- */

int qjs_clique(qjs_ponte_t *p, int dom)
{
    int r = qjs_despachar(p, dom, "click", "{\"detalhe\":1}");
    return r > 0 ? (r & 1) : 0;
}

/* Desde a D488 o navegador acha o elemento clicado sem marcar nada no layout. */
int qjs_clique_ids(qjs_ponte_t *p, int *ids, int max)
{
    (void)p; (void)ids; (void)max;
    return 0;
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

/* ------------------------------------------------------------ o bucle de eventos (D488)
 *
 * As tarefas sao os temporizadores (setTimeout, setInterval) e os eventos que o
 * navegador manda; depois de cada uma correm as microtarefas (as promessas). Os
 * quadros (requestAnimationFrame) correm todos juntos, uma vez por volta, no
 * maximo a ~60 por segundo e antes de o navegador pintar. Cada volta tem um
 * orcamento (QJS_VOLTA_MS): o resto fica para a seguinte, e a interface nao para.
 * O setInterval volta a contar depois do callback (como em Serenity e nos
 * navegadores): um callback lento nao se empilha. */

#define QJS_VOLTA_MS   12
#define QJS_LENTO_MS   20000   /* um script que corre mais do que isto seguido e parado */

static void qjs_tempo_tirar(qjs_ponte_t *p, int k)
{
    JS_FreeValue(p->ctx, p->tmp[k].fn);
    JS_FreeValue(p->ctx, p->tmp[k].args);
    p->tmp[k] = p->tmp[p->n_tmp - 1];
    p->n_tmp--;
}

/* temporizador(fn, ms, repete, [args]) -> id */
QJS_NAT(qjs_n_temporizador)
{
    QJS_P;
    if (!JS_IsFunction(ctx, ARG(0))) return JS_NewInt32(ctx, 0);
    if (p->n_tmp >= QJS_TIMER_MAX) return JS_NewInt32(ctx, 0);
    if (p->n_tmp >= p->cap_tmp) {
        int cap = p->cap_tmp ? p->cap_tmp * 2 : 32;
        qjs_tempo_t *t = realoca(p->tmp, (size_t)cap * sizeof *t);
        if (!t) return JS_NewInt32(ctx, 0);
        p->tmp = t; p->cap_tmp = cap;
    }
    int ms = qjs_int(ctx, ARG(1), 0);
    if (ms < 0) ms = 0;
    int rep = qjs_int(ctx, ARG(2), 0);
    if (rep && ms < 4) ms = 4;
    qjs_tempo_t *t = &p->tmp[p->n_tmp++];
    t->fn = JS_DupValue(ctx, ARG(0));
    t->args = JS_IsArray(ctx, ARG(3)) ? JS_DupValue(ctx, ARG(3)) : JS_UNDEFINED;
    t->ms = (unsigned)ms;
    t->vence = tempo_ms() + (unsigned)ms;
    t->repete = rep ? 1 : 0;
    t->id = ++p->prox_id;
    t->seq = ++p->seq;
    return JS_NewInt32(ctx, t->id);
}

QJS_NAT(qjs_n_parar)
{
    QJS_P;
    int id = qjs_int(ctx, ARG(0), 0);
    for (int i = 0; i < p->n_tmp; i++) if (p->tmp[i].id == id) { qjs_tempo_tirar(p, i); break; }
    return JS_UNDEFINED;
}

/* quadro(fn) -> id (requestAnimationFrame) */
QJS_NAT(qjs_n_quadro)
{
    QJS_P;
    if (!JS_IsFunction(ctx, ARG(0)) || p->n_quadros >= QJS_TIMER_MAX) return JS_NewInt32(ctx, 0);
    if (p->n_quadros >= p->cap_quadros) {
        int cap = p->cap_quadros ? p->cap_quadros * 2 : 16;
        qjs_quadro_t *q = realoca(p->quadros, (size_t)cap * sizeof *q);
        if (!q) return JS_NewInt32(ctx, 0);
        p->quadros = q; p->cap_quadros = cap;
    }
    qjs_quadro_t *q = &p->quadros[p->n_quadros++];
    q->fn = JS_DupValue(ctx, ARG(0));
    q->id = ++p->prox_quadro;
    return JS_NewInt32(ctx, q->id);
}

QJS_NAT(qjs_n_sem_quadro)
{
    QJS_P;
    int id = qjs_int(ctx, ARG(0), 0);
    for (int i = 0; i < p->n_quadros; i++)
        if (p->quadros[i].id == id) {
            JS_FreeValue(ctx, p->quadros[i].fn);
            memmove(p->quadros + i, p->quadros + i + 1, (size_t)(p->n_quadros - i - 1) * sizeof *p->quadros);
            p->n_quadros--;
            break;
        }
    return JS_UNDEFINED;
}

/* Um script preso num laco: o QuickJS pergunta de vez em quando se deve parar. */
static int qjs_interromper(JSRuntime *rt, void *opaco)
{
    (void)rt;
    qjs_ponte_t *p = opaco;
    if (!p || !p->inicio) return 0;
    if (tempo_ms() - p->inicio < QJS_LENTO_MS) return 0;
    if (!p->lento) registrar("qjs: o script demora demasiado; para-se\n");
    p->lento = 1;
    return 1;
}

static void qjs_comecar(qjs_ponte_t *p) { p->inicio = tempo_ms(); if (!p->inicio) p->inicio = 1; p->lento = 0; }
static void qjs_acabar(qjs_ponte_t *p) { p->inicio = 0; }

/* Chama f(args...) como uma tarefa: com as microtarefas depois. */
static void qjs_tarefa(qjs_ponte_t *p, JSValue f, JSValue args, const char *onde)
{
    JSContext *ctx = p->ctx;
    JSValue argv[8];
    int argc = 0;
    if (JS_IsArray(ctx, args)) {
        JSValue l = JS_GetPropertyStr(ctx, args, "length");
        int32_t n = 0;
        JS_ToInt32(ctx, &n, l);
        JS_FreeValue(ctx, l);
        for (int i = 0; i < n && i < 8; i++) argv[argc++] = JS_GetPropertyUint32(ctx, args, (uint32_t)i);
    }
    qjs_comecar(p);
    JSValue r = JS_Call(ctx, f, JS_UNDEFINED, argc, argv);
    if (JS_IsException(r)) qjs_excecao(p, onde);
    JS_FreeValue(ctx, r);
    for (int i = 0; i < argc; i++) JS_FreeValue(ctx, argv[i]);
    qjs_tarefas(p);
    qjs_acabar(p);
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
    if (qjs_rede_url) qjs_rede_url(p->url);      /* D487 */
    char *j = aloca(strlen(p->url) * 2 + 8);     /* o location e o history do qjs_dom.js */
    if (!j) return;
    int k = 0;
    j[k++] = '[';
    j[k++] = '"';
    for (const char *q = p->url; *q; q++) {
        if (*q == '"' || *q == '\\') j[k++] = '\\';
        if ((unsigned char)*q >= 32) j[k++] = *q;
    }
    j[k++] = '"';
    j[k++] = ']';
    j[k] = 0;
    qjs_chamar(p, "__sagui_url", j);
    libera(j);
}

/* A arvore da pagina (o texto que o navegador manda): o qjs_dom.js monta a copia. */
typedef struct { qjs_ponte_t *p; const char *fonte; int n; } dom_t;

static void qjs_definir_dom_interna(void *arg)
{
    dom_t *d = (dom_t *)arg;
    JSContext *ctx = d->p->ctx;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue f = JS_GetPropertyStr(ctx, g, "__sagui_montar");
    if (JS_IsFunction(ctx, f)) {
        JSValue t = JS_NewStringLen(ctx, d->fonte, (size_t)d->n);
        qjs_comecar(d->p);
        JSValue r = JS_Call(ctx, f, g, 1, &t);
        if (JS_IsException(r)) qjs_excecao(d->p, "ao montar o DOM");
        JS_FreeValue(ctx, r);
        JS_FreeValue(ctx, t);
        qjs_acabar(d->p);
    }
    JS_FreeValue(ctx, f);
    JS_FreeValue(ctx, g);
}

void qjs_definir_dom(qjs_ponte_t *p, const char *fonte, int n)
{
    if (!p || !p->ctx || !fonte || n <= 0) return;
    dom_t d = { p, fonte, n };
    qjs_em_pilha(qjs_definir_dom_interna, &d);
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

    /* os nativos do DOM e do bucle de eventos (D488); qjs_dom.js poe por cima o setTimeout & cia. */
    static const struct { const char *nome; JSCFunction *f; int n; } nat[] = {
        { "criar", qjs_n_criar, 2 }, { "inserir", qjs_n_inserir, 3 }, { "tirar", qjs_n_tirar, 1 }, { "dado", qjs_n_dado, 2 },
        { "html", qjs_n_html, 2 }, { "atr", qjs_n_atr, 3 }, { "caixa", qjs_n_caixa, 1 }, { "estilo", qjs_n_estilo, 2 },
        { "em", qjs_n_em, 2 }, { "janela", qjs_n_janela, 0 }, { "rolar", qjs_n_rolar, 2 }, { "historia", qjs_n_historia, 2 },
        { "ir", qjs_n_ir, 1 }, { "navegar", qjs_n_navegar, 1 }, { "quer", qjs_n_quer, 1 },
        { "rolagem", qjs_n_rolagem, 1 }, { "rolar_caixa", qjs_n_rolar_caixa, 3 },
        { "firma", qjs_n_firma, 0 }, { "incremental", qjs_n_incremental, 1 },
        { "temporizador", qjs_n_temporizador, 4 }, { "parar", qjs_n_parar, 1 }, { "quadro", qjs_n_quadro, 1 }, { "sem_quadro", qjs_n_sem_quadro, 1 },
    };
    JSValue nv = JS_NewObject(ctx);
    for (unsigned i = 0; i < sizeof nat / sizeof nat[0]; i++)
        JS_SetPropertyStr(ctx, nv, nat[i].nome, JS_NewCFunction(ctx, nat[i].f, nat[i].nome, nat[i].n));
    JS_SetPropertyStr(ctx, global, "__sagui_nat", nv);
    JS_SetPropertyStr(ctx, global, "setTimeout", JS_NewCFunction(ctx, qjs_n_temporizador, "setTimeout", 2));   /* ate o qjs_dom.js */

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
    /* o document.body sem DOM (o objecto de sempre, D149): o qjs_dom.js usa-o se a pagina nao tem arvore */
    JS_SetPropertyStr(ctx, global, "__sagui_corpo", JS_DupValue(ctx, corpo));
    JS_SetPropertyStr(ctx, global, "document", doc);
    p->corpo = corpo;      /* D488: a referencia de JS_NewObject (antes duplicava-se e ficava perdida) */

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
    JS_SetPropertyStr(ctx, doc, "location", JS_DupValue(ctx, loc));   /* D488: sin el Dup, el location se liberaba dos veces */

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
    /* D488: o DOM em JavaScript (qjs_dom.js) */
    pv = JS_Eval(ctx, qjs_dom_js, sizeof qjs_dom_js - 1, "<dom>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(pv)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        registrar("qjs: erro no DOM: %s\n", msg ? msg : "?");
        JSValue pil = JS_GetPropertyStr(ctx, exc, "stack");
        const char *ps = JS_IsString(pil) ? JS_ToCString(ctx, pil) : NULL;
        if (ps) { registrar("qjs: pilha: %s\n", ps); JS_FreeCString(ctx, ps); }
        JS_FreeValue(ctx, pil);
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, pv);
    if (qjs_rede_preparar) qjs_rede_preparar(ctx);   /* D487: fetch, XHR, cookies, almacenamiento y WebSocket completos (despues do DOM: usa o document) */

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
    JS_SetInterruptHandler(p->rt, qjs_interromper, p);
    p->larg = 800; p->alt = 600; p->t0 = tempo_ms();
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
    while (p->n_tmp) qjs_tempo_tirar(p, p->n_tmp - 1);
    for (int i = 0; i < p->n_quadros; i++) JS_FreeValue(p->ctx, p->quadros[i].fn);
    p->n_quadros = 0;
    if (p->tmp) { libera(p->tmp); p->tmp = NULL; }
    if (p->quadros) { libera(p->quadros); p->quadros = NULL; }
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
    qjs_comecar(e->p);
    JSValue v = JS_Eval(ctx, e->codigo, e->n, e->nome, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        registrar("qjs: erro em %s: %s\n", e->nome, msg ? msg : "?");
        /* a linha do erro, para saber que sintaxe falhou */
        JSValue pil = JS_GetPropertyStr(ctx, exc, "stack");
        const char *ps = JS_IsString(pil) ? JS_ToCString(ctx, pil) : NULL;
        if (ps) {
            const char *dp = strrchr(ps, ':');
            int ln = 0;
            if (dp) { dp++; while (*dp >= '0' && *dp <= '9') ln = ln * 10 + (*dp++ - '0'); }
            registrar("qjs: pilha: %s\n", ps);
            if (ln > 0) {
                const char *q = e->codigo, *fim = e->codigo + e->n;
                for (int l = 1; l < ln && q < fim; q++) if (*q == '\n') l++;
                for (int d = 0; d < 2; d++) {   /* a linha e a seguinte */
                    char tr[100]; int k = 0;
                    while (q < fim && *q != '\n' && k < 99) { char ch = *q++; tr[k++] = (ch < 32 || ch > 126) ? '?' : ch; }
                    tr[k] = 0; if (q < fim) q++;
                    registrar("qjs: linha %d: %s\n", ln + d, tr);
                }
            }
            JS_FreeCString(ctx, ps);
        }
        JS_FreeValue(ctx, pil);
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
    qjs_acabar(e->p);
}

/* O tamanho da janela, para innerWidth, innerHeight e matchMedia (D271). */
void qjs_definir_janela(qjs_ponte_t *p, int larg, int alt)
{
    if (!p || !p->ctx) return;
    p->larg = larg; p->alt = alt;   /* desde a D488 o innerWidth le-se do gancho janela (ou daqui) */
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

/* Uma volta do bucle de eventos: as tarefas vencidas (cada uma com as suas
 * microtarefas) ate ao orcamento, e depois os quadros, se ja toca. */
typedef struct { qjs_ponte_t *p; int correram; } tique_t;

static void qjs_tique_interna(void *arg)
{
    tique_t *t = (tique_t *)arg;
    qjs_ponte_t *p = t->p;
    unsigned ini = tempo_ms();
    for (;;) {
        unsigned agora = tempo_ms();
        int k = -1;
        for (int i = 0; i < p->n_tmp; i++) {
            if ((int)(p->tmp[i].vence - agora) > 0) continue;
            if (k < 0 || (int)(p->tmp[i].vence - p->tmp[k].vence) < 0 ||
                (p->tmp[i].vence == p->tmp[k].vence && (int)(p->tmp[i].seq - p->tmp[k].seq) < 0)) k = i;
        }
        if (k < 0) break;
        JSValue fn = JS_DupValue(p->ctx, p->tmp[k].fn), args = JS_DupValue(p->ctx, p->tmp[k].args);
        int rep = p->tmp[k].repete ? p->tmp[k].id : -1;
        if (rep >= 0) p->tmp[k].vence = agora + 0x40000000u;   /* nao volta nesta volta: conta outra vez ao acabar */
        else qjs_tempo_tirar(p, k);
        qjs_tarefa(p, fn, args, "num temporizador");
        if (rep >= 0)
            for (int i = 0; i < p->n_tmp; i++) if (p->tmp[i].id == rep) { p->tmp[i].vence = tempo_ms() + p->tmp[i].ms; break; }
        JS_FreeValue(p->ctx, fn);
        JS_FreeValue(p->ctx, args);
        t->correram++;
        if (t->correram >= 64 || tempo_ms() - ini >= QJS_VOLTA_MS) break;
    }
    /* os quadros: os que estavam pedidos ao comecar a volta (os novos ficam para a seguinte) */
    unsigned agora = tempo_ms();
    if (p->n_quadros && agora - p->ult_quadro >= 15) {
        p->ult_quadro = agora;
        int n = p->n_quadros;
        qjs_quadro_t *q = aloca((size_t)n * sizeof *q);
        if (q) {
            memcpy(q, p->quadros, (size_t)n * sizeof *q);
            p->n_quadros = 0;
            JSValue arg = JS_NewFloat64(p->ctx, (double)(agora - p->t0));
            for (int i = 0; i < n; i++) {
                JSValue a1 = JS_NewArray(p->ctx);
                JS_SetPropertyUint32(p->ctx, a1, 0, JS_DupValue(p->ctx, arg));
                qjs_tarefa(p, q[i].fn, a1, "num quadro");
                JS_FreeValue(p->ctx, a1);
                JS_FreeValue(p->ctx, q[i].fn);
                t->correram++;
            }
            JS_FreeValue(p->ctx, arg);
            libera(q);
        }
    }
}

int qjs_tique(qjs_ponte_t *p, unsigned agora_ms)
{
    (void)agora_ms;
    if (!p || !p->ctx) return 0;
    tique_t t = { p, 0 };
    qjs_em_pilha(qjs_tique_interna, &t);
    return t.correram;
}

/* ms ate a proxima coisa a fazer (temporizador ou quadro); -1 = nada. */
int qjs_espera(qjs_ponte_t *p)
{
    if (!p || !p->ctx) return -1;
    unsigned agora = tempo_ms();
    int m = -1;
    for (int i = 0; i < p->n_tmp; i++) {
        int d = (int)(p->tmp[i].vence - agora);
        if (d < 0) d = 0;
        if (m < 0 || d < m) m = d;
    }
    if (p->n_quadros) { int d = 15 - (int)(agora - p->ult_quadro); if (d < 0) d = 0; if (m < 0 || d < m) m = d; }
    return m;
}

int qjs_animando(qjs_ponte_t *p) { return p && p->n_quadros > 0; }
int qjs_quer(qjs_ponte_t *p) { return p ? p->quer : 0; }

/* ---- os eventos do navegador e as chamadas de fora (D488) ---- */

typedef struct { qjs_ponte_t *p; const char *funcao; int dom; const char *tipo; const char *json; int r; } chamada_t;

static void qjs_chamar_interna(void *arg)
{
    chamada_t *c = (chamada_t *)arg;
    qjs_ponte_t *p = c->p;
    JSContext *ctx = p->ctx;
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue f = JS_GetPropertyStr(ctx, g, c->funcao);
    c->r = -1;
    if (JS_IsFunction(ctx, f)) {
        JSValue argv[3];
        int argc = 0;
        if (c->tipo) {
            argv[0] = JS_NewInt32(ctx, c->dom);
            argv[1] = JS_NewString(ctx, c->tipo);
            argv[2] = JS_NewString(ctx, c->json ? c->json : "");
            argc = 3;
        } else if (c->json && c->json[0]) {
            JSValue v = JS_ParseJSON(ctx, c->json, strlen(c->json), "<chamada>");
            if (JS_IsException(v)) { qjs_excecao(p, "no JSON de uma chamada"); v = JS_UNDEFINED; }
            if (JS_IsArray(ctx, v)) {
                JSValue l = JS_GetPropertyStr(ctx, v, "length");
                int32_t n = 0; JS_ToInt32(ctx, &n, l); JS_FreeValue(ctx, l);
                for (int i = 0; i < n && i < 3; i++) argv[argc++] = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
                JS_FreeValue(ctx, v);
            } else argv[argc++] = v;
        }
        qjs_comecar(p);
        JSValue r = JS_Call(ctx, f, g, argc, argv);
        if (JS_IsException(r)) qjs_excecao(p, c->funcao);
        else { int32_t v = 0; if (JS_IsNumber(r)) JS_ToInt32(ctx, &v, r); c->r = v; }
        JS_FreeValue(ctx, r);
        for (int i = 0; i < argc; i++) JS_FreeValue(ctx, argv[i]);
        qjs_tarefas(p);
        qjs_acabar(p);
    }
    JS_FreeValue(ctx, f);
    JS_FreeValue(ctx, g);
}

int qjs_despachar(qjs_ponte_t *p, int dom, const char *tipo, const char *json)
{
    if (!p || !p->ctx || !tipo) return -1;
    chamada_t c = { p, "__sagui_evento", dom, tipo, json ? json : "{}", -1 };
    qjs_em_pilha(qjs_chamar_interna, &c);
    return c.r;
}

int qjs_chamar(qjs_ponte_t *p, const char *funcao, const char *json)
{
    if (!p || !p->ctx || !funcao) return -1;
    chamada_t c = { p, funcao, -1, NULL, json, -1 };
    qjs_em_pilha(qjs_chamar_interna, &c);
    return c.r;
}

/* ------------------------------------------------------------ o que o script deixou */

const char *qjs_titulo(qjs_ponte_t *p) { return (p && p->tem_titulo) ? p->titulo : NULL; }
const char *qjs_corpo(qjs_ponte_t *p) { return (p && p->tem_corpo) ? p->corpo_html : NULL; }
const char *qjs_console(qjs_ponte_t *p) { return (p && p->console_txt[0]) ? p->console_txt : NULL; }
const char *qjs_alerta(qjs_ponte_t *p) { return (p && p->alerta_txt[0]) ? p->alerta_txt : NULL; }
