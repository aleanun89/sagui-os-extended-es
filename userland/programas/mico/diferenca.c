// SPDX-License-Identifier: GPL-2.0-or-later
/* diferenca.c - propuestas de cambio de la IA como diferencias (mejora 043, D240).
 *
 * El panel «Asistente» (painel.c) le pide al modelo el código completo del
 * fragmento y entrega aquí la respuesta. Se extrae el primer bloque ```...```, se
 * compara por líneas con el texto actual del documento (libsagui/difer.c) y se
 * enseña como en un «diff»: líneas quitadas en rojo con «-», añadidas en verde con
 * «+» y tres líneas de contexto. Nada cambia hasta pulsar «Aplicar» (todos los
 * bloques) o «Aplicar este bloque»; cada aplicación es un solo paso de deshacer.
 *
 * El fragmento se identifica por líneas (p_l0, p_n) y no por posición en bytes, y
 * tras cada aplicación se vuelve a calcular la diferencia contra el texto que
 * queda: así los bloques que sobran siguen siendo coherentes. Si el documento
 * cambió desde la petición (doc->versao), se avisa y se recalcula sin aplicar. */
#include "mico.h"
#include "difer.h"

#define CONTEXTO 3

typedef struct { int op0, op1; } bloco_t;   /* operaciones [op0, op1) */

static doc_t *p_doc;
static int p_l0, p_n, p_versao, p_topo, p_bloco;
static bool p_todo;
static char *p_nov;                  /* la propuesta (Latin-1) */
static difer_linha_t *p_nl;
static int p_nnl;
static char *p_ori;                  /* el texto actual del fragmento */
static difer_linha_t *p_ol;
static int p_nol;
static difer_op_t *p_ops;
static int p_nops;
static bloco_t *p_blocos;
static int p_nblocos, p_mais, p_menos;
static int *p_filas, p_nfilas;       /* lo que se dibuja: índice de operación o -1 (separador) */
static int *p_fila_bloco;            /* primera fila de cada bloque */
static ret_t b_todos, b_este, b_desc, b_ant, b_sig;

static void libera_calculo(void)
{
    if (p_ori) libera(p_ori);
    if (p_ol) libera(p_ol);
    if (p_ops) libera(p_ops);
    if (p_blocos) libera(p_blocos);
    if (p_filas) libera(p_filas);
    if (p_fila_bloco) libera(p_fila_bloco);
    p_ori = NULL; p_ol = NULL; p_ops = NULL; p_blocos = NULL; p_filas = NULL; p_fila_bloco = NULL;
    p_nol = p_nops = p_nblocos = p_nfilas = 0;
}

void prop_descartar(void)
{
    libera_calculo();
    if (p_nov) libera(p_nov);
    if (p_nl) libera(p_nl);
    p_nov = NULL; p_nl = NULL; p_nnl = 0; p_doc = NULL;
    p_bloco = p_topo = 0;
}

bool prop_ativa(void) { return p_doc != NULL; }

static bool doc_vivo(doc_t *d)
{
    for (int i = 0; i < qtd_abas; i++) if (abas[i] == d) return true;
    return false;
}

/* Líneas del documento sin contar la vacía que deja el último '\n'. */
static int linhas_efetivas(doc_t *d)
{
    int n = d->nli;
    if (n > 0 && doc_tam_linha(d, n - 1) == 0) n--;
    return n;
}

static void montar_filas(void)
{
    int *dist = aloca((size_t)(p_nops + 1) * sizeof(int));
    p_filas = aloca((size_t)(2 * p_nops + 2) * sizeof(int));
    p_fila_bloco = aloca((size_t)(p_nblocos + 1) * sizeof(int));
    if (!dist || !p_filas || !p_fila_bloco) { if (dist) libera(dist); p_nfilas = 0; return; }
    int d = 1 << 20;
    for (int i = 0; i < p_nops; i++) { d = p_ops[i].tipo == DIFER_IGUAL ? d + 1 : 0; dist[i] = d; }
    d = 1 << 20;
    for (int i = p_nops - 1; i >= 0; i--) {
        d = p_ops[i].tipo == DIFER_IGUAL ? d + 1 : 0;
        if (d < dist[i]) dist[i] = d;
    }
    int b = 0;
    bool oculto = false;
    for (int i = 0; i < p_nops; i++) {
        bool ver = p_ops[i].tipo != DIFER_IGUAL || dist[i] <= CONTEXTO;
        if (!ver) { oculto = true; continue; }
        if (oculto && p_nfilas) p_filas[p_nfilas++] = -1;
        oculto = false;
        while (b < p_nblocos && p_blocos[b].op0 == i) p_fila_bloco[b++] = p_nfilas;
        p_filas[p_nfilas++] = i;
    }
    libera(dist);
}

/* Compara el fragmento actual del documento con la propuesta. */
static void recalcular(void)
{
    libera_calculo();
    doc_t *d = p_doc;
    if (p_todo) { p_l0 = 0; p_n = linhas_efetivas(d); }
    if (p_l0 > d->nli) p_l0 = d->nli;
    if (p_l0 + p_n > d->nli) p_n = d->nli - p_l0;
    int ini = p_l0 < d->nli ? doc_ini(d, p_l0) : d->tam;
    int fim = p_l0 + p_n < d->nli ? doc_ini(d, p_l0 + p_n) : d->tam;
    p_ori = aloca((size_t)(fim - ini) + 2);
    if (!p_ori) { prop_descartar(); return; }
    doc_pedaco(d, ini, fim - ini, p_ori);
    p_ori[fim - ini] = 0;
    int nl = 1;
    for (int i = 0; i < fim - ini; i++) if (p_ori[i] == '\n') nl++;
    p_ol = aloca((size_t)nl * sizeof *p_ol);
    p_nol = p_ol ? difer_partir(p_ori, fim - ini, p_ol, nl) : 0;
    int cap = p_nol + p_nnl + 1;
    p_ops = aloca((size_t)cap * sizeof *p_ops);
    if (!p_ops) { prop_descartar(); return; }
    p_nops = difer_linhas(p_ol, p_nol, p_nl, p_nnl, p_ops, cap);
    if (p_nops < 0) {   /* demasiado distinto: todo fuera y todo dentro */
        p_nops = 0;
        for (int i = 0; i < p_nol; i++) { p_ops[p_nops].tipo = DIFER_QUITA; p_ops[p_nops].ia = i; p_ops[p_nops].ib = 0; p_nops++; }
        for (int j = 0; j < p_nnl; j++) { p_ops[p_nops].tipo = DIFER_ANADE; p_ops[p_nops].ia = p_nol; p_ops[p_nops].ib = j; p_nops++; }
    }
    p_blocos = aloca((size_t)(p_nops + 1) * sizeof *p_blocos);
    p_nblocos = p_mais = p_menos = 0;
    for (int i = 0; i < p_nops && p_blocos; ) {
        if (p_ops[i].tipo == DIFER_IGUAL) { i++; continue; }
        int k = i;
        while (k < p_nops && p_ops[k].tipo != DIFER_IGUAL) {
            if (p_ops[k].tipo == DIFER_QUITA) p_menos++; else p_mais++;
            k++;
        }
        p_blocos[p_nblocos].op0 = i; p_blocos[p_nblocos].op1 = k; p_nblocos++;
        i = k;
    }
    p_versao = d->versao;
    if (p_bloco >= p_nblocos) p_bloco = p_nblocos ? p_nblocos - 1 : 0;
    montar_filas();
}

static void registrar_doc(const char *que)
{
    doc_t *d = p_doc ? p_doc : DOC;
    const char *t = doc_texto(d);
    registrar("mico: propuesta %s, documento %d bytes hash %08x", que, d->tam, (unsigned)hash_texto(t, d->tam));
}

/* Crea la propuesta con la respuesta del modelo: el primer bloque de código. Devuelve
 * el número de bloques de diferencias (0 = sin código o sin cambios). */
int prop_criar(doc_t *d, int l0, int n, bool todo, const char *resp, int rn)
{
    prop_descartar();
    int a = -1, b = rn;
    for (int i = 0; i + 2 < rn; i++) {
        if (resp[i] == '`' && resp[i + 1] == '`' && resp[i + 2] == '`') {
            if (a < 0) {
                i += 3;
                while (i < rn && resp[i] != '\n') i++;   /* la etiqueta del lenguaje */
                a = i + 1;
            } else { b = i; break; }
        }
    }
    if (a < 0 || a >= b) return 0;
    p_nov = aloca((size_t)(b - a) + 2);
    if (!p_nov) return 0;
    int k = 0;
    for (int i = a; i < b; i++) if (resp[i] != '\r') p_nov[k++] = resp[i];
    p_nov[k] = 0;
    k = (int)utf8_para_latin1(p_nov, (size_t)k, NULL);
    p_nov[k] = 0;
    int nl = 1;
    for (int i = 0; i < k; i++) if (p_nov[i] == '\n') nl++;
    p_nl = aloca((size_t)nl * sizeof *p_nl);
    if (!p_nl) { prop_descartar(); return 0; }
    p_nnl = difer_partir(p_nov, k, p_nl, nl);
    p_doc = d; p_l0 = l0; p_n = n; p_todo = todo; p_bloco = 0; p_topo = 0;
    recalcular();
    if (!p_doc) return 0;
    if (!p_nblocos) { prop_descartar(); return 0; }
    registrar("mico: propuesta %d bloques (+%d -%d)", p_nblocos, p_mais, p_menos);
    return p_nblocos;
}

/* Aplica un bloque al documento (sin agrupar ni recalcular). Devuelve la posición del cambio. */
static int aplicar_uno(bloco_t *bl)
{
    doc_t *d = p_doc;
    int ia0 = -1, nq = 0, ib0 = -1, na = 0;
    for (int i = bl->op0; i < bl->op1; i++) {
        if (p_ops[i].tipo == DIFER_QUITA) { if (ia0 < 0) ia0 = p_ops[i].ia; nq++; }
        else { if (ib0 < 0) ib0 = p_ops[i].ib; na++; if (ia0 < 0) ia0 = p_ops[i].ia; }
    }
    int la = p_l0 + ia0;
    char *t;
    int tam = 0, cap = 2;
    for (int j = 0; j < na; j++) cap += p_nl[ib0 + j].n + 1;
    t = aloca((size_t)cap);
    if (!t) return -1;
    int ini, fim;
    bool no_fim = la + nq >= d->nli;      /* llega al final del archivo, sin '\n' detrás */
    if (la >= d->nli) { ini = d->tam; fim = d->tam; if (na) t[tam++] = '\n'; }
    else { ini = doc_ini(d, la); fim = no_fim ? d->tam : doc_ini(d, la + nq); }
    for (int j = 0; j < na; j++) {
        memcpy(t + tam, p_nl[ib0 + j].p, (size_t)p_nl[ib0 + j].n);
        tam += p_nl[ib0 + j].n;
        if (j + 1 < na || !no_fim) t[tam++] = '\n';
    }
    if (fim > ini) doc_apagar(d, ini, fim - ini);
    if (tam) doc_inserir(d, ini, t, tam);
    libera(t);
    p_n += na - nq;
    return ini;
}

static void depois_de_editar(int pos)
{
    doc_t *d = p_doc;
    d->ed_so_linha = false;
    vista_ir(pos, false);
    vista_apos_edicao();
    vista_sujar_tudo();
    sujo |= R_EDITOR | R_ESTADO | R_ABAS | R_PAINEL;
}

/* Comprueba que se puede aplicar: mismo documento y sin cambios desde que se calculó. */
static bool pode_aplicar(void)
{
    if (!p_doc) return false;
    if (!doc_vivo(p_doc)) { prop_descartar(); avisar("El archivo de la propuesta se cerró"); return false; }
    if (p_doc != DOC) { avisar("La propuesta es de otro archivo: ábrelo para aplicarla"); return false; }
    if (p_doc->versao != p_versao) {
        recalcular();
        registrar("mico: propuesta recalculada, el archivo cambió");
        avisar("El archivo cambió desde la petición: diferencias recalculadas, revísalas");
        if (p_doc && !p_nblocos) { prop_descartar(); avisar("Ya no queda nada que aplicar"); }
        sujo |= R_PAINEL;
        return false;
    }
    return true;
}

static void terminar_aplicacao(int pos, const char *que)
{
    depois_de_editar(pos);
    registrar_doc(que);
    recalcular();
    if (p_doc && !p_nblocos) { prop_descartar(); avisar("Propuesta aplicada entera (Ctrl+Z la deshace)"); }
    else if (p_doc) {
        registrar("mico: propuesta quedan %d bloques", p_nblocos);
        avisar("Bloque aplicado: quedan %d (Ctrl+Z lo deshace)", p_nblocos);
    }
}

void prop_aplicar_tudo(void)
{
    if (!pode_aplicar()) return;
    doc_t *d = p_doc;
    int pos = 0;
    doc_grupo(d, true);
    for (int b = p_nblocos - 1; b >= 0; b--) pos = aplicar_uno(&p_blocos[b]);
    doc_grupo(d, false);
    terminar_aplicacao(pos < 0 ? 0 : pos, "aplicada entera");
}

void prop_aplicar_bloco(void)
{
    if (!pode_aplicar() || !p_nblocos) return;
    doc_t *d = p_doc;
    doc_grupo(d, true);
    int pos = aplicar_uno(&p_blocos[p_bloco]);
    doc_grupo(d, false);
    terminar_aplicacao(pos < 0 ? 0 : pos, "bloque aplicado");
}

void prop_descartar_acao(void)
{
    if (!p_doc) return;
    prop_descartar();
    registrar("mico: propuesta descartada");
    avisar("Propuesta descartada");
    sujo |= R_PAINEL;
}

static void mostrar_bloco(void)
{
    if (!p_nblocos || !p_fila_bloco) return;
    int f = p_fila_bloco[p_bloco] - 2;
    p_topo = f < 0 ? 0 : f;
}

void prop_mover(int dir)
{
    if (!p_doc || !p_nblocos) return;
    p_bloco = (p_bloco + dir + p_nblocos) % p_nblocos;
    mostrar_bloco();
    registrar("mico: propuesta bloque %d de %d", p_bloco + 1, p_nblocos);
    sujo |= R_PAINEL;
}

static int filas_visiveis(int area_a) { int v = (area_a - LETRA_A - 10) / LETRA_A; return v < 1 ? 1 : v; }
static int area_diff = 100;

void prop_rodar(int n)
{
    int max = p_nfilas - filas_visiveis(area_diff);
    p_topo += n * 3;
    if (p_topo > max) p_topo = max;
    if (p_topo < 0) p_topo = 0;
    sujo |= R_PAINEL;
}

static void botao(ret_t *rb, int x, int y, const char *s, uint32_t cor)
{
    int n = (int)tam_texto(s);
    *rb = (ret_t){ x, y, n * LETRA_L + 16, 22 };
    d_ret(rb->x, rb->y, rb->l, rb->a, cor);
    d_texto(rb->x + 8, rb->y + 3, s, n, 0xFFFFFF);
}

/* Dibuja la vista de diferencias en r (área del panel); deja libres las dos filas de abajo. */
void prop_desenhar(ret_t r)
{
    bool claro = tema == &tema_claro;
    uint32_t cor_q = claro ? 0xFFDDDD : 0x5A2626, cor_a = claro ? 0xDDF5DD : 0x1F4A2A;
    uint32_t txt_q = claro ? 0x9B1C1C : 0xFF9A9A, txt_a = claro ? 0x146C2E : 0x8FE3A0;
    area_diff = r.a - 62;
    char cab[200];
    formatar_texto(cab, sizeof cab, "Propuesta de la IA: bloque %d de %d   (+%d -%d)   Re Pág / Av Pág cambian de bloque",
                   p_bloco + 1, p_nblocos, p_mais, p_menos);
    d_texto_ate(r.x + 12, r.y + 4, cab, r.l - 24, tema->destaque);
    int vis = filas_visiveis(area_diff);
    if (p_topo > p_nfilas - vis) p_topo = p_nfilas - vis;
    if (p_topo < 0) p_topo = 0;
    int bloco_op0 = p_nblocos ? p_blocos[p_bloco].op0 : 0, bloco_op1 = p_nblocos ? p_blocos[p_bloco].op1 : 0;
    for (int k = 0; k < vis && p_topo + k < p_nfilas; k++) {
        int op = p_filas[p_topo + k];
        int y = r.y + 8 + LETRA_A + k * LETRA_A;
        if (op < 0) { d_texto(r.x + 12, y, "   ...", 6, tema->margem_texto); continue; }
        difer_op_t *o = &p_ops[op];
        const difer_linha_t *l = o->tipo == DIFER_ANADE ? &p_nl[o->ib] : &p_ol[o->ia];
        uint32_t cor = tema->painel_texto;
        char marca = ' ';
        if (o->tipo == DIFER_QUITA) { d_ret(r.x + 8, y, r.l - 16, LETRA_A, cor_q); cor = txt_q; marca = '-'; }
        else if (o->tipo == DIFER_ANADE) { d_ret(r.x + 8, y, r.l - 16, LETRA_A, cor_a); cor = txt_a; marca = '+'; }
        if (op >= bloco_op0 && op < bloco_op1) d_ret(r.x + 4, y, 3, LETRA_A, tema->destaque);
        char buf[400];
        int n = l->n < (int)sizeof buf - 1 ? l->n : (int)sizeof buf - 1;
        memcpy(buf, l->p, (size_t)n);
        buf[n] = 0;
        for (int i = 0; i < n; i++) if (buf[i] == '\t') buf[i] = ' ';
        d_letra(r.x + 12, y, (unsigned char)marca, cor);
        d_texto_ate(r.x + 12 + 2 * LETRA_L, y, buf, r.l - 40, cor);
    }
    int y = r.y + r.a - 28, x = r.x + 8;
    botao(&b_todos, x, y, "Aplicar", tema->destaque);
    botao(&b_este, b_todos.x + b_todos.l + 6, y, "Aplicar este bloque", tema->destaque);
    botao(&b_desc, b_este.x + b_este.l + 6, y, "Descartar", tema->barra_borda);
    botao(&b_ant, b_desc.x + b_desc.l + 6, y, "< Anterior", tema->barra_borda);
    botao(&b_sig, b_ant.x + b_ant.l + 6, y, "Siguiente >", tema->barra_borda);
}

/* Clic en los botones de la vista. true = lo tratamos. */
bool prop_clique(int x, int y)
{
    if (!p_doc) return false;
    if (dentro(x, y, b_todos)) prop_aplicar_tudo();
    else if (dentro(x, y, b_este)) prop_aplicar_bloco();
    else if (dentro(x, y, b_desc)) prop_descartar_acao();
    else if (dentro(x, y, b_ant)) prop_mover(-1);
    else if (dentro(x, y, b_sig)) prop_mover(1);
    else return false;
    sujo |= R_PAINEL;
    return true;
}

bool prop_tecla(unsigned t)
{
    if (!p_doc) return false;
    if (t == TECLA_PGDN) { prop_mover(1); return true; }
    if (t == TECLA_PGUP) { prop_mover(-1); return true; }
    return false;
}
