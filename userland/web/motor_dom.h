// SPDX-License-Identifier: GPL-3.0-only
/* motor_dom.h - el DOM mutable del motor de páginas del Cipó (D488).
 *
 * Solo lo incluye motor.c, al final (usa sus tipos internos: no_m, motor,
 * estilizar...). Va aparte para que los cambios del DOM no se mezclen con los
 * de CSS y maquetación, que se tocan a la vez en otras ramas.
 *
 * Tres piezas:
 *
 *  1. La tabla id -> nodo: el JavaScript habla de nodos por su número (D160) y
 *     cada operación los busca en O(1) en lugar de recorrer la página.
 *  2. Las operaciones del DOM: crear un nodo suelto, meterlo (o moverlo) en un
 *     padre, quitarlo, cambiar el texto de un nodo de texto, cambiar los hijos
 *     por un HTML (innerHTML) y exportar un subárbol en el formato del puente.
 *  3. Las marcas de sucio para el remaquetado incremental: cada cambio marca
 *     solo los nodos afectados (el nodo, sus hermanos si cambió la forma del
 *     árbol, sus antepasados como «hay algo debajo»). En el layout siguiente la
 *     cascada corre solo por lo marcado, las medidas intrínsecas de lo que no
 *     cambió se conservan y se calcula la franja de la página que hay que volver
 *     a pintar. Si la hoja tiene :has() (el estilo de un antepasado depende de
 *     sus hijos), la cascada vuelve a ser completa: es lo seguro.
 */

static int dom_avisos;     /* diagnóstico: estilos que cambian al reestilizar */

/* ------------------------------------------------------------ tabla id -> nodo */

static void dom_registrar(motor_t *m, no_m *n)
{
    if (n->dom < 0) return;
    if (n->dom >= m->dom_cap) {
        int cap = m->dom_cap ? m->dom_cap : 1024;
        while (cap <= n->dom) cap *= 2;
        no_m **t = m_novo(m, cap * (int)sizeof(no_m *));
        if (!t) return;                      /* sin memoria: dom_no() recorre la página */
        memset(t, 0, (size_t)cap * sizeof(no_m *));
        if (m->dom_tab) memcpy(t, m->dom_tab, (size_t)m->dom_cap * sizeof(no_m *));
        m->dom_tab = t; m->dom_cap = cap;
    }
    m->dom_tab[n->dom] = n;
}

/* El nodo de un identificador del DOM (elemento o texto), o NULL. */
static no_m *dom_no(motor_t *m, int dom)
{
    if (!m || dom < 0) return NULL;
    if (dom < m->dom_cap && m->dom_tab && m->dom_tab[dom] && m->dom_tab[dom]->dom == dom) return m->dom_tab[dom];
    for (no_m *n = m->raiz; n; n = proximo_no(n)) if (n->dom == dom && !n->pseudo) return n;
    return NULL;
}

/* ¿a está dentro de b (o es b)? */
static bool dom_dentro(no_m *a, no_m *b)
{
    for (int k = 0; a && k < 4096; a = a->pai, k++) if (a == b) return true;
    return false;
}

/* ¿el nodo está colgado de la página (y no suelto)? */
static bool dom_na_pagina(motor_t *m, no_m *n) { return m->raiz && dom_dentro(n, m->raiz); }

/* ------------------------------------------------------------ marcas de sucio */

/* La hoja de la página: ¿tiene :has() o combinadores de hermanos? (se mira una
 * vez por cascada completa) */
static bool sub_tem(const subsel_t *ss, bool *irmao)
{
    for (; ss; ss = ss->prox) {
        if (ss->tipo == SUB_HAS) return true;
        for (int k = 0; k < ss->n; k++)
            for (int c = 0; c < ss->s[k].n; c++) {
                if (ss->s[k].c[c].comb == '+' || ss->s[k].c[c].comb == '~') *irmao = true;
                if (ss->s[k].c[c].subs && sub_tem(ss->s[k].c[c].subs, irmao)) return true;
            }
    }
    return false;
}

static void css_analisar(motor_t *m)
{
    m->css_has = false; m->css_irmao = false;
    for (int r = 0; r < m->n_regras; r++) {
        const seletor_t *s = &m->regras[r].sel;
        for (int c = 0; c < s->n; c++) {
            if (s->c[c].comb == '+' || s->c[c].comb == '~') m->css_irmao = true;
            if (s->c[c].subs && sub_tem(s->c[c].subs, &m->css_irmao)) m->css_has = true;
        }
    }
    static bool ja;
    if (!ja || m->css_has || m->css_irmao) registrar("cipo: dom: hoja con %d reglas, :has %d, hermanos %d\n", m->n_regras, m->css_has, m->css_irmao);
    ja = true;
    dom_avisos = 0;
}

/* Guarda la caja de antes del cambio (la del último layout) del nodo y de sus
 * antepasados: después del layout, el primer antepasado con la misma caja dice
 * dónde acaba lo que hay que volver a pintar. */
static void dom_raiz_suja(motor_t *m, no_m *n)
{
    if (!n) return;
    if (m->n_sujos >= DOM_SUJOS_MAX) { m->faixa_toda = true; return; }
    for (int i = 0; i < m->n_sujos; i++) if (m->sujos[i].no == n) return;
    dom_sujo_t *s = &m->sujos[m->n_sujos++];
    s->no = n;
    s->n = 0;
    for (no_m *a = n; a && s->n < DOM_CADEIA; a = a->pai) {
        s->c[s->n].no = a;
        s->c[s->n].x = a->ax; s->c[s->n].y = a->ay; s->c[s->n].l = a->l; s->c[s->n].a = a->a;
        s->c[s->n].pint = a->pintado;
        s->n++;
    }
    /* lo que el nodo y lo de dentro ocupaban (puede salirse de la caja: sombras, absolutos) */
    int dir = n->ax + n->l, baixo = n->ay + n->a;
    if (n->pintado && !n->texto) extensao(n, &dir, &baixo);
    s->y0 = n->pintado ? n->ay : INF; s->y1 = n->pintado ? baixo : -INF;
}

/* Marca un nodo: SJ_PROPRIO (su estilo; baja a los hijos si cambia),
 * SJ_ARVORE (él y todo lo de dentro). Los antepasados quedan con SJ_DESC. */
static void dom_marcar(motor_t *m, no_m *n, uint8_t como)
{
    if (!n) return;
    n->sujo |= como;
    for (no_m *a = n->pai; a && !(a->sujo & SJ_DESC); a = a->pai) a->sujo |= SJ_DESC;
    m->recascata = true;
    if (dom_na_pagina(m, n)) dom_raiz_suja(m, n->texto && n->pai ? n->pai : n);
}

/* La forma de los hijos de p cambió (entró o salió un nodo): p y sus hijos se
 * vuelven a estilizar (:first-child, :nth-child, :empty...); con + o ~ en la
 * hoja, cada hermano con todo lo de dentro. */
static void dom_marcar_filhos(motor_t *m, no_m *p)
{
    if (!p) return;
    dom_marcar(m, p, SJ_PROPRIO);
    for (no_m *c = p->filho; c; c = c->prox) {
        if (c->pseudo) continue;
        c->sujo |= m->css_irmao ? SJ_ARVORE : SJ_PROPRIO;
    }
    p->sujo |= SJ_DESC;
}

/* Estilo de un nodo, como en estilizar_tudo(). */
static void dom_estilizar_um(motor_t *m, no_m *n)
{
    if (n->pseudo) { n->bg_img = url_img(m, n->e.bg_url); n->mask_img = url_img(m, n->e.mask_url); return; }
    estilizar(m, n);
    n->bg_img = -1; n->mask_img = -1;
    if (!n->texto) {
        n->bg_img = url_img(m, n->e.bg_url); n->mask_img = url_img(m, n->e.mask_url);
        if (n->bg_img >= 0) m->imgs[n->bg_img].prio = 1;
    }
}

/* Todo el subárbol: lo mismo que la cascada completa, pero de un nodo. */
static void dom_estilizar_arvore(motor_t *m, no_m *n)
{
    no_m *prox = n->prox;
    n->prox = NULL;                 /* estilizar_tudo recorre los hermanos: solo este */
    estilizar_tudo(m, n);
    normalizar(m, n);
    estilizar_tudo(m, n);
    n->prox = prox;
}

static void dom_limpar_marcas(no_m *n)
{
    n->sujo = (uint8_t)((n->sujo & ~(SJ_PROPRIO | SJ_ARVORE | SJ_DESC)) | SJ_LAYOUT);
    for (no_m *c = n->filho; c; c = c->prox) dom_limpar_marcas(c);
}

static int dom_inc_nos;   /* nodos estilizados en la última cascada incremental (para medir) */

static int dom_contar(no_m *n)
{
    int k = 1;
    for (no_m *c = n->filho; c; c = c->prox) k += dom_contar(c);
    return k;
}

static void dom_inc_desce(motor_t *m, no_m *n)
{
    for (no_m *c = n->filho; c; c = c->prox) {
        uint8_t s = c->sujo;
        if (!(s & (SJ_PROPRIO | SJ_ARVORE | SJ_DESC))) continue;
        c->sujo = (uint8_t)((s & ~(SJ_PROPRIO | SJ_ARVORE | SJ_DESC)) | SJ_LAYOUT);
        if (s & SJ_ARVORE) {
            dom_estilizar_arvore(m, c);
            dom_limpar_marcas(c);
            dom_inc_nos += dom_contar(c);
            continue;
        }
        if (s & SJ_PROPRIO) {
            static estilo_t antes;
            antes = c->e;
            uint8_t disp = c->e.display;
            dom_estilizar_um(m, c);
            dom_inc_nos++;
            if (memcmp(&antes, &c->e, sizeof antes) != 0 && dom_avisos < 3) {
                const uint8_t *x = (const uint8_t *)&antes, *y = (const uint8_t *)&c->e;
                unsigned k = 0;
                while (k < sizeof antes && x[k] == y[k]) k++;
                registrar("cipo: dom: el estilo de <%s> cambia en el byte %u de %u\n", c->tag, k, (unsigned)sizeof antes);
                dom_avisos++;
            }
            if (memcmp(&antes, &c->e, sizeof antes) != 0 || (disp == D_NONE) != (c->e.display == D_NONE)) {
                /* cambió: lo de dentro hereda otra cosa */
                if (c->filho && c->e.display != D_NONE) {
                    for (no_m *g = c->filho; g; g = g->prox) if (!g->pseudo) g->sujo |= SJ_ARVORE;
                    c->sujo |= SJ_DESC;
                }
            }
            normalizar(m, c);
        }
        if (c->e.display != D_NONE && (c->sujo & SJ_DESC)) { c->sujo &= (uint8_t)~SJ_DESC; dom_inc_desce(m, c); }
        else if (s & SJ_DESC) dom_inc_desce(m, c);
    }
}

/* La cascada solo por lo marcado. false = no se puede (la hace completa quien llama). */
static bool dom_inc_estilo(motor_t *m)
{
    dom_inc_nos = 0;
    if (!m->inc_ok || m->css_has || !(m->raiz->sujo & (SJ_DESC | SJ_PROPRIO | SJ_ARVORE))) return false;
    if (m->raiz->sujo & (SJ_PROPRIO | SJ_ARVORE)) return false;
    m->raiz->sujo = (uint8_t)((m->raiz->sujo & ~SJ_DESC) | SJ_LAYOUT);
    has_resto = 200000;
    dom_inc_desce(m, m->raiz);
    return true;
}

/* Antes del layout: las medidas intrínsecas (medir_max/min) de lo que no cambió
 * siguen valiendo si la anchura y los recursos son los mismos. */
static void dom_limpar_layout(motor_t *m, bool incremental)
{
    for (no_m *q = m->raiz; q; q = proximo_no(q)) {
        if (!incremental || (q->sujo & SJ_LAYOUT)) q->max_c = q->min_c = -1;
        else if (q->max_c >= 0) m->inc_medidas++;
        q->sujo &= (uint8_t)~SJ_LAYOUT;
        q->colapso = 0;
        q->alvo = -1;
        q->x = q->y = q->l = q->a = 0;
        q->frags = q->frags_fim = NULL;
    }
}

/* Después del layout: la franja de la página que cambió. */
static void dom_faixa_calcular(motor_t *m)
{
    m->faixa_y0 = INF; m->faixa_y1 = -INF;
    if (m->faixa_toda || !m->n_sujos) { if (m->faixa_toda) { m->faixa_y0 = 0; m->faixa_y1 = INF; } goto fim; }
    for (int i = 0; i < m->n_sujos; i++) {
        dom_sujo_t *s = &m->sujos[i];
        int y0 = s->y0, y1 = s->y1;
        no_m *n = s->no;
        if (n->pintado && dom_na_pagina(m, n)) {
            int dir = n->ax + n->l, baixo = n->ay + n->a;
            if (!n->texto) extensao(n, &dir, &baixo);
            if (n->ay < y0) y0 = n->ay;
            if (baixo > y1) y1 = baixo;
        }
        int k;
        for (k = 1; k < s->n; k++) {   /* el primer antepasado que no se movió ni cambió de tamaño */
            no_m *a = s->c[k].no;
            if (a == m->raiz) { k = s->n; break; }
            if (a->pintado && s->c[k].pint && a->ax == s->c[k].x && a->ay == s->c[k].y && a->l == s->c[k].l && a->a == s->c[k].a) break;
        }
        if (k >= s->n) { m->faixa_y0 = 0; m->faixa_y1 = INF; goto fim; }   /* cambió la página entera */
        no_m *a = s->c[k].no;
        if (a->ay < y0) y0 = a->ay;
        if (a->ay + a->a > y1) y1 = a->ay + a->a;
        if (y0 < m->faixa_y0) m->faixa_y0 = y0;
        if (y1 > m->faixa_y1) m->faixa_y1 = y1;
    }
fim:
    m->n_sujos = 0;
    m->faixa_toda = false;
}

/* ------------------------------------------------------------ las operaciones */

static void dom_desligar(motor_t *m, no_m *n)
{
    no_m *p = n->pai;
    if (!p) return;
    if (m->foco && dom_dentro(m->foco, n)) m->foco = NULL;
    if (n->ant) n->ant->prox = n->prox; else p->filho = n->prox;
    if (n->prox) n->prox->ant = n->ant; else p->ult = n->ant;
    n->pai = n->ant = n->prox = NULL;
    /* un bloco anónimo que se queda vacío desaparece */
    if (p->dom < 0 && !p->pseudo && !p->filho && p->pai && igual(p->tag, "#anon")) dom_desligar(m, p);
}

/* Las imágenes de un subárbol nuevo: lo mismo que motor_ler() hace al leer la página. */
static void dom_imagens(motor_t *m, no_m *n)
{
    if (n->texto) return;
    if (igual(n->tag, "img") && n->img < 0) {
        const char *src = atributo_m(n, "src");
        if (!src || !src[0]) { const char *ds = atributo_m(n, "data-src"); if (ds) src = ds; }
        if (src && src[0]) n->img = url_img(m, src);
    }
    for (no_m *c = n->filho; c; c = c->prox) dom_imagens(m, c);
}

int motor_dom_criar(motor_t *m, const char *tag, const char *texto)
{
    if (!m || !tag || !tag[0]) return -1;
    mo = m;
    no_m *n = novo_no(m, NULL);
    if (!n) return -1;
    if (igual(tag, "#texto")) {
        int nt = texto ? (int)tam_texto(texto) : 0;
        n->texto = 1;
        copia_texto(n->tag, "#texto", sizeof n->tag);
        n->t = m_dup(m, texto ? texto : "", nt);
        n->nt = n->t ? nt : 0;
    } else {
        int k = 0;
        for (const char *p = tag; *p && k < 11; p++) n->tag[k++] = minusc(*p);
        n->tag[k] = 0;
    }
    n->bg_img = -1; n->mask_img = -1;
    n->sujo = SJ_ARVORE;
    return n->dom;
}

bool motor_dom_inserir(motor_t *m, int pai, int dom, int antes)
{
    no_m *p = dom_no(m, pai), *n = dom_no(m, dom), *a = antes >= 0 ? dom_no(m, antes) : NULL;
    if (!p || !n || p->texto || n == m->raiz || dom_dentro(p, n)) return false;
    if (antes >= 0 && (!a || a == n)) return false;
    mo = m;
    if (n->pai) {
        no_m *velho = n->pai;
        while (velho && velho->dom < 0) velho = velho->pai;
        if (dom_na_pagina(m, n)) { dom_raiz_suja(m, n); dom_marcar_filhos(m, velho); }
        dom_desligar(m, n);
    }
    no_m *alvo_pai = p;
    if (a) {
        alvo_pai = a->pai;
        if (!alvo_pai || (alvo_pai != p && !(alvo_pai->dom < 0 && dom_dentro(alvo_pai, p)))) return false;
    } else if (p->ps[1] && p->ps[1]->pai == p) a = p->ps[1];   /* antes del ::after */
    n->pai = alvo_pai;
    if (a) {
        n->prox = a; n->ant = a->ant;
        if (a->ant) a->ant->prox = n; else alvo_pai->filho = n;
        a->ant = n;
    } else {
        n->ant = alvo_pai->ult; n->prox = NULL;
        if (alvo_pai->ult) alvo_pai->ult->prox = n; else alvo_pai->filho = n;
        alvo_pai->ult = n;
    }
    dom_imagens(m, n);
    if (dom_na_pagina(m, n)) {
        dom_marcar_filhos(m, p);
        if (alvo_pai != p) dom_marcar_filhos(m, alvo_pai);
        n->sujo |= SJ_ARVORE;
        dom_marcar(m, n, SJ_ARVORE);
    }
    return true;
}

bool motor_dom_tirar(motor_t *m, int dom)
{
    no_m *n = dom_no(m, dom);
    if (!n || n == m->raiz || !n->pai) return false;
    no_m *p = n->pai;
    while (p && p->dom < 0) p = p->pai;
    bool vivo = dom_na_pagina(m, n);
    if (vivo) dom_raiz_suja(m, n);
    dom_desligar(m, n);
    if (vivo && p) dom_marcar_filhos(m, p);
    return true;
}

bool motor_dom_dado(motor_t *m, int dom, const char *txt)
{
    no_m *n = dom_no(m, dom);
    if (!n || !n->texto || !txt) return false;
    int nt = (int)tam_texto(txt);
    char *t = m_dup(m, txt, nt);
    if (!t) return false;
    n->t = t; n->nt = nt;
    if (n->texto == 2) n->texto = 1;   /* el espacio que normalizar() había quitado */
    if (dom_na_pagina(m, n)) { dom_marcar(m, n, SJ_PROPRIO); if (n->pai) dom_marcar(m, n->pai, SJ_PROPRIO); }
    return true;
}

/* innerHTML = html: los hijos de dom son los del HTML (Latin-1). */
bool motor_dom_html(motor_t *m, int dom, const char *html, int n)
{
    no_m *e = dom_no(m, dom);
    if (!e || e->texto || !html) return false;
    mo = m;
    bool vivo = dom_na_pagina(m, e);
    if (vivo) dom_raiz_suja(m, e);
    if (m->foco && dom_dentro(m->foco, e) && m->foco != e) m->foco = NULL;
    /* se quedan solo los generados ::before/::after (se rehacen en la cascada) */
    e->filho = e->ult = NULL; e->ps[0] = e->ps[1] = NULL; e->frags = e->frags_fim = NULL;
    lendo_fragmento = true;
    ler_html_em(m, html, n, e);
    lendo_fragmento = false;
    for (no_m *c = e->filho; c; c = c->prox) dom_imagens(m, c);
    if (vivo) dom_marcar(m, e, SJ_ARVORE);
    else e->sujo |= SJ_ARVORE;
    return true;
}

/* ---- exportar un subárbol en el formato del puente (qjs_bridge.h), en UTF-8 ---- */

typedef struct { char *d; int n, cap; bool falta; } dom_buf_t;

static void db_por(dom_buf_t *b, const char *s, int n)
{
    if (b->falta) return;
    if (b->n + n + 1 > b->cap) {
        int cap = b->cap ? b->cap * 2 : 8192;
        while (cap < b->n + n + 1) cap *= 2;
        char *d = aloca((size_t)cap);
        if (!d) { b->falta = true; return; }
        if (b->d) { memcpy(d, b->d, (size_t)b->n); libera(b->d); }
        b->d = d; b->cap = cap;
    }
    memcpy(b->d + b->n, s, (size_t)n);
    b->n += n;
    b->d[b->n] = 0;
}

static void db_txt(dom_buf_t *b, const char *s) { db_por(b, s, (int)tam_texto(s)); }

static void db_num(dom_buf_t *b, int v)
{
    char t[16];
    formatar_texto(t, sizeof t, "%d", v);
    db_txt(b, t);
}

/* Un campo: Latin-1 -> UTF-8, con \\, \t, \n y \r escapados. */
static void db_campo(dom_buf_t *b, const char *s, int n)
{
    if (!s) return;
    if (n < 0) n = (int)tam_texto(s);
    char t[256]; int k = 0;
    for (int i = 0; i < n; i++) {
        uint8_t c = (uint8_t)s[i];
        if (k > 240) { db_por(b, t, k); k = 0; }
        if (c == '\\') { t[k++] = '\\'; t[k++] = '\\'; }
        else if (c == '\t') { t[k++] = '\\'; t[k++] = 't'; }
        else if (c == '\n') { t[k++] = '\\'; t[k++] = 'n'; }
        else if (c == '\r') { t[k++] = '\\'; t[k++] = 'r'; }
        else if (c < 0x80) t[k++] = (char)c;
        else { t[k++] = (char)(0xC0 | (c >> 6)); t[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    db_por(b, t, k);
}

static void db_no(dom_buf_t *b, no_m *n, int pai)
{
    db_txt(b, "N\t"); db_num(b, n->dom); db_txt(b, "\t"); db_num(b, pai); db_txt(b, "\t");
    int na = 0;
    if (n->texto) { db_txt(b, "#texto\t\t\t0\t"); db_campo(b, n->t, n->nt); db_txt(b, "\n"); return; }
    for (atr_t *a = n->atr; a && na < 64; a = a->prox) na++;
    db_campo(b, n->tag, -1); db_txt(b, "\t");
    db_campo(b, n->id ? n->id : "", -1); db_txt(b, "\t");
    db_campo(b, n->classe ? n->classe : "", -1); db_txt(b, "\t");
    db_num(b, na); db_txt(b, "\t\n");
    int k = 0;
    for (atr_t *a = n->atr; a && k < na; a = a->prox, k++) {
        db_txt(b, "A\t"); db_campo(b, a->nome, -1); db_txt(b, "\t"); db_campo(b, a->valor ? a->valor : "", -1); db_txt(b, "\n");
    }
}

/* Los nodos de dentro de n (sin n), en orden de documento; salta los generados. */
static void db_filhos(dom_buf_t *b, no_m *n, int pai, int prof)
{
    if (prof > 512) return;
    for (no_m *c = n->filho; c; c = c->prox) {
        if (c->pseudo) continue;
        if (c->dom < 0) { db_filhos(b, c, pai, prof + 1); continue; }   /* bloco anónimo: sus hijos son de pai */
        if (c->texto == 2 && 0) continue;
        db_no(b, c, pai);
        if (!c->texto) db_filhos(b, c, c->dom, prof + 1);
    }
}

/* dom < 0: la página entera (con la cabecera D); si no, los hijos de dom.
 * Devuelve un bloque de aloca() (lo libera quien llama) y su tamaño. */
char *motor_dom_exportar(motor_t *m, int dom, int *n)
{
    dom_buf_t b = { 0 };
    *n = 0;
    if (!m || !m->raiz) return NULL;
    if (dom < 0) {
        db_txt(&b, "D\t"); db_num(&b, motor_dom_corpo(m)); db_txt(&b, "\t"); db_num(&b, motor_dom_documento(m)); db_txt(&b, "\n");
        db_no(&b, m->raiz, -1);
        db_filhos(&b, m->raiz, m->raiz->dom, 0);
    } else {
        no_m *e = dom_no(m, dom);
        if (!e) return NULL;
        db_txt(&b, "D\t-1\t-1\n");
        db_filhos(&b, e, e->dom, 0);
    }
    if (b.falta) { if (b.d) libera(b.d); return NULL; }
    *n = b.n;
    return b.d;
}

/* ------------------------------------------------------------ medidas y punto */

/* La caja de un elemento en línea: la unión de los fragmentos de su texto en el
 * bloque que lo contiene. */
static bool dom_caixa_inline(no_m *n, int *x0, int *y0, int *x1, int *y1)
{
    no_m *b = n->pai;
    while (b && !b->frags) b = b->pai;
    if (!b || !b->pintado) return false;
    bool achou = false;
    *x0 = *y0 = INF; *x1 = *y1 = -INF;
    for (struct frag *f = b->frags; f; f = f->prox) {
        no_m *q = f->estilo_de;
        if (!q || !dom_dentro(q, n)) continue;
        int fx = b->ax + f->x, fy = b->ay + f->y;
        if (fx < *x0) *x0 = fx;
        if (fy < *y0) *y0 = fy;
        if (fx + f->l > *x1) *x1 = fx + f->l;
        if (fy + f->a > *y1) *y1 = fy + f->a;
        achou = true;
    }
    return achou;
}

/* 16 enteros, como motor_caixas(): x, y, anchura, altura (caja del borde, página),
 * bordes (4), rellenos (4), anchura y altura del contenido desplazable, mostrado
 * (0/1), 1 si es la caja de un elemento en línea. */
bool motor_dom_caixa(motor_t *m, int dom, int *o)
{
    memset(o, 0, 16 * sizeof(int));
    no_m *n = dom_no(m, dom);
    if (!n || !dom_na_pagina(m, n)) return false;
    if (n->texto) n = n->pai;
    if (!n) return false;
    for (no_m *a = n; a; a = a->pai) if (a->e.display == D_NONE) return true;   /* sin caja */
    if (n->pintado && (n->l > 0 || n->a > 0 || !n->frags)) {
        o[0] = n->ax; o[1] = n->ay; o[2] = n->l; o[3] = n->a;
        for (int j = 0; j < 4; j++) { o[4 + j] = n->bl[j]; o[8 + j] = n->pl[j]; }
        int dir = n->ax + n->l - n->bl[1], baixo = n->ay + n->a - n->bl[2];
        extensao(n, &dir, &baixo);
        o[12] = dir - n->ax - n->bl[3] + n->pl[1];
        o[13] = baixo - n->ay - n->bl[0] + n->pl[2];
        if (o[12] < n->l - n->bl[1] - n->bl[3]) o[12] = n->l - n->bl[1] - n->bl[3];
        if (o[13] < n->a - n->bl[0] - n->bl[2]) o[13] = n->a - n->bl[0] - n->bl[2];
        o[14] = 1;
        return true;
    }
    int x0, y0, x1, y1;
    if (dom_caixa_inline(n, &x0, &y0, &x1, &y1)) {
        o[0] = x0; o[1] = y0; o[2] = x1 - x0; o[3] = y1 - y0;
        o[12] = o[2]; o[13] = o[3]; o[14] = 1; o[15] = 1;
    }
    return true;
}

/* El elemento que está en el punto (x, y) de la página: el más profundo que lo
 * contiene (y, dentro de un bloque, el elemento en línea del fragmento). */
static no_m *dom_em(no_m *n, int x, int y, no_m *melhor)
{
    for (; n; n = n->prox) {
        if (n->texto || n->e.display == D_NONE || !n->pintado) continue;
        if (n->e.visivel) { if (n->filho) melhor = dom_em(n->filho, x, y, melhor); continue; }   /* visibility: hidden */
        bool dentro = x >= n->ax && x < n->ax + n->l && y >= n->ay && y < n->ay + n->a;
        if (dentro && n->dom >= 0) melhor = n;
        if (n->filho && (dentro || !n->e.corta)) melhor = dom_em(n->filho, x, y, melhor);
    }
    return melhor;
}

int motor_dom_em(motor_t *m, int x, int y)
{
    if (!m || !m->raiz) return -1;
    no_m *b = dom_em(m->raiz, x, y, NULL);
    if (!b) return motor_dom_corpo(m);
    /* el texto del bloque (y de sus anónimos): ¿qué elemento en línea hay en el punto? */
    no_m *blocos[2] = { b, NULL };
    for (int k = 0; k < 1; k++) {
        for (no_m *q = blocos[k]; q; q = proximo_no(q)) {
            if (q != b && !dom_dentro(q, b)) break;
            if (!q->frags || !q->pintado) continue;
            for (struct frag *f = q->frags; f; f = f->prox) {
                int fx = q->ax + f->x, fy = q->ay + f->y;
                if (x < fx || x >= fx + f->l || y < fy || y >= fy + f->a) continue;
                no_m *e = f->estilo_de;
                while (e && (e->texto || e->dom < 0)) e = e->pai;
                if (e && dom_dentro(e, b) && e != b) b = e;
            }
        }
    }
    while (b && b->dom < 0) b = b->pai;
    return b ? b->dom : -1;
}

/* getComputedStyle de un elemento por su identificador del DOM. */
static bool estilo_no_m(motor_t *m, no_m *n, const char *prop, char *o, int max);
bool motor_dom_estilo(motor_t *m, int dom, const char *prop, char *o, int max)
{
    no_m *n = dom_no(m, dom);
    o[0] = 0;
    if (!n || n->texto) return false;
    return estilo_no_m(m, n, prop, o, max);
}

/* Lo que costó el último layout (para la prueba de D488). */
void motor_dom_estadistica(motor_t *m, int *estilizados, int *medidas, int *y0, int *y1)
{
    *estilizados = m->inc_ult_nos; *medidas = m->inc_ult_medidas;
    *y0 = m->faixa_y0; *y1 = m->faixa_y1;
}

void motor_dom_incremental(motor_t *m, bool sim)
{
    if (!m) return;
    m->inc_desligado = !sim;
    if (!sim) m->recascata = true;     /* el layout siguiente, completo (para comparar) */
}

/* scrollTop/scrollLeft de un bloque con overflow (D489): [x, y, máximo x, máximo y]. */
bool motor_dom_rolagem(motor_t *m, int dom, int *o)
{
    no_m *n = dom_no(m, dom);
    o[0] = o[1] = o[2] = o[3] = 0;
    if (!n || n->texto) return false;
    o[0] = n->rol_x; o[1] = n->rol_y; o[2] = n->rmax_x; o[3] = n->rmax_y;
    return true;
}

/* scrollTo de un bloque: se recorta a [0, máximo] y se rehace la lista de pintura. */
bool motor_dom_rolar(motor_t *m, int dom, int x, int y)
{
    no_m *n = dom_no(m, dom);
    if (!n || n->texto) return false;
    if (x > n->rmax_x) x = n->rmax_x;
    if (y > n->rmax_y) y = n->rmax_y;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x == n->rol_x && y == n->rol_y) return false;
    n->rol_x = x; n->rol_y = y;
    mo = m;
    rol_refazer(m);
    return true;
}

/* El elemento del DOM del campo con foco, o -1. */
int motor_dom_foco(motor_t *m) { return m && m->foco ? m->foco->dom : -1; }

/* Una firma de la lista de pintura (posiciones, tamaños, colores y textos): dos
 * layouts que pintan lo mismo dan la misma firma. La prueba compara el layout
 * incremental con el completo. */
unsigned motor_dom_assinatura(motor_t *m)
{
    unsigned h = 2166136261u;
#define MIX(v) (h = (h ^ (unsigned)(v)) * 16777619u)
    for (int i = 0; m && i < m->n_cmds; i++) {
        cmd_t *c = &m->cmds[i];
        MIX(c->tipo); MIX(c->x); MIX(c->y); MIX(c->l); MIX(c->a); MIX(c->cor);
        if (c->tipo == C_TEXTO && c->t) for (int k = 0; k < c->nt; k++) MIX((uint8_t)c->t[k]);
    }
#undef MIX
    return h;
}
