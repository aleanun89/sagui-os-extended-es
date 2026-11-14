// SPDX-License-Identifier: GPL-3.0-only
/* desanalise.c - árbol -> texto.
 *
 * Se usa para mostrar un trabajo ("[1]+ Hecho  contador 4"), para el "type"
 * de una función y, sobre todo, para pasar funciones y bloques a un sagsh
 * hijo (etapa de pipe interna, bloque en segundo plano). Las palabras quedan
 * crudas en el árbol, así que el texto generado se analiza de nuevo igual.
 * En el modo compacto todo va en una línea (sin los cuerpos de los here-docs);
 * en el normal los separadores son saltos de línea y los cuerpos de los
 * here-docs entran justo después de la línea que los pide. */
#include "sagsh.h"

typedef struct {
    texto_t *t;
    bool     compacto;
    redir_t *pend[32];
    int      npend;
} ds_t;

static void no_texto(ds_t *d, no_t *n);

static void separar(ds_t *d)
{
    if (d->compacto) { tx_junta(d->t, "; "); return; }
    tx_car(d->t, '\n');
    for (int i = 0; i < d->npend; i++) {
        redir_t *r = d->pend[i];
        tx_junta(d->t, r->corpo ? r->corpo : "");
        for (const char *c = r->alvo; *c; c++) {
            if (*c == '\'' || *c == '"') continue;
            if (*c == '\\' && c[1]) c++;
            tx_car(d->t, *c);
        }
        tx_car(d->t, '\n');
    }
    d->npend = 0;
}

static bool termina_em_fundo(no_t *n)
{
    return n && n->tipo == N_LISTA && n->nfilhos > 0 && n->fundo[n->nfilhos - 1];
}

/* Una lista seguida del separador correcto antes de una palabra reservada. */
static void corpo(ds_t *d, no_t *n)
{
    if (!n) { tx_junta(d->t, ":"); separar(d); return; }
    if (n->tipo == N_LISTA && n->nfilhos == 0) return;   /* elemento de case vacío */
    no_texto(d, n);
    if (termina_em_fundo(n)) {
        if (d->compacto) tx_car(d->t, ' ');
        else separar(d);
    } else {
        separar(d);
    }
}

static void redirs(ds_t *d, redir_t *r)
{
    static const char *op[] = { "<", ">", ">>", ">|", "<>", "<&", ">&", "<<", "<<<", "&>", "&>>" };
    for (; r; r = r->prox) {
        tx_car(d->t, ' ');
        if (r->fd >= 0) tx_formatar(d->t, "%d", r->fd);
        tx_junta(d->t, r->tipo == R_HEREDOC && r->tira_tab ? "<<-" : op[r->tipo]);
        tx_junta(d->t, r->alvo);
        if (r->tipo == R_HEREDOC && !d->compacto && d->npend < 32) d->pend[d->npend++] = r;
    }
}

static void no_texto(ds_t *d, no_t *n)
{
    texto_t *t = d->t;
    switch (n->tipo) {
    case N_SIMPLES: {
        bool primeiro = true;
        for (int i = 0; i < n->natr; i++, primeiro = false) {
            if (!primeiro) tx_car(t, ' ');
            tx_junta(t, n->atribs[i]);
        }
        for (int i = 0; i < n->npal; i++, primeiro = false) {
            if (!primeiro) tx_car(t, ' ');
            tx_junta(t, n->palavras[i]);
        }
        redirs(d, n->redir);
        return;
    }
    case N_PIPE:
        if (n->negar) tx_junta(t, "! ");
        for (int i = 0; i < n->nfilhos; i++) {
            if (i) tx_junta(t, " | ");
            no_texto(d, n->filhos[i]);
        }
        return;
    case N_E:
    case N_OU:
        no_texto(d, n->a);
        tx_junta(t, n->tipo == N_E ? " && " : " || ");
        no_texto(d, n->b);
        return;
    case N_LISTA:
        for (int i = 0; i < n->nfilhos; i++) {
            no_texto(d, n->filhos[i]);
            if (n->fundo[i]) tx_junta(t, " &");
            if (i < n->nfilhos - 1) {
                if (n->fundo[i] && d->compacto) tx_car(t, ' ');
                else separar(d);
            }
        }
        return;
    case N_SUBSHELL:
        tx_junta(t, "( ");
        corpo(d, n->a);
        tx_car(t, ')');
        break;
    case N_GRUPO:
        tx_junta(t, "{ ");
        corpo(d, n->a);
        tx_car(t, '}');
        break;
    case N_SE:
        tx_junta(t, "if ");
        corpo(d, n->a);
        tx_junta(t, "then ");
        corpo(d, n->b);
        if (n->c) { tx_junta(t, "else "); corpo(d, n->c); }
        tx_junta(t, "fi");
        break;
    case N_ENQUANTO:
    case N_ATE:
        tx_junta(t, n->tipo == N_ENQUANTO ? "while " : "until ");
        corpo(d, n->a);
        tx_junta(t, "do ");
        corpo(d, n->b);
        tx_junta(t, "done");
        break;
    case N_PARA:
        tx_junta(t, "for ");
        tx_junta(t, n->nome);
        if (n->com_lista) {
            tx_junta(t, " in");
            for (int i = 0; i < n->npal; i++) { tx_car(t, ' '); tx_junta(t, n->palavras[i]); }
        }
        separar(d);
        tx_junta(t, "do ");
        corpo(d, n->b);
        tx_junta(t, "done");
        break;
    case N_CASO:
        tx_junta(t, "case ");
        tx_junta(t, n->nome);
        tx_junta(t, " in");
        separar(d);
        for (item_caso_t *it = n->itens; it; it = it->prox) {
            for (int i = 0; i < it->npadroes; i++) {
                if (i) tx_car(t, '|');
                tx_junta(t, it->padroes[i]);
            }
            tx_junta(t, ") ");
            if (it->corpo) corpo(d, it->corpo);
            tx_junta(t, ";;");
            separar(d);
        }
        tx_junta(t, "esac");
        break;
    case N_FUNCAO:
        tx_junta(t, n->nome);
        tx_junta(t, "() ");
        no_texto(d, n->a);
        return;
    }
    redirs(d, n->redir);
}

void desanalisar(no_t *n, texto_t *t, bool compacto)
{
    ds_t d;
    memset(&d, 0, sizeof d);
    d.t = t;
    d.compacto = compacto;
    no_texto(&d, n);
    if (d.npend) separar(&d);
}

char *texto_do_no(no_t *n, bool compacto)
{
    texto_t t = {0};
    desanalisar(n, &t, compacto);
    return tx_texto(&t);
}
