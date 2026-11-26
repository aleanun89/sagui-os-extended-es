// SPDX-License-Identifier: GPL-3.0-only
/* sgl_glsl_ri.h - generación de la representación intermedia (sgl_ri.h) desde el árbol de GLSL ES 1.00
 * (incluido al final de sgl_glsl.c; D475).
 *
 * Cada valor vive en registros vec4: escalar y vector en uno (con su swizzle), matN en N columnas, cada
 * elemento de array y cada campo de struct desde un registro propio. Las expresiones devuelven un «val»
 * (fichero, registro y swizzle) sin copiar cuando no hace falta (variables, swizzles, inmediatos); las
 * funciones embutidas que no son operaciones de la RI se bajan aquí a operaciones de la RI. */

typedef struct { uint8_t arq; int reg; uint8_t sw; tipo t; } val;

#define SW_REP(k) RI_SW((k), (k), (k), (k))

/* contexto de la función que se expande (return, bucles propios) */
typedef struct fctx {
    struct fctx *pai;
    fn   *f;
    val   ret;             /* donde va el valor de return */
    int   envolve;         /* el cuerpo va dentro de BGNLOOP ... BRK ENDLOOP (hay return antes del final) */
    int   ret_aninhado;    /* hay return dentro de un bucle de la función: bandera */
    int   bandeira;        /* temporal de la bandera */
    int   lacos;           /* bucles abiertos dentro de esta función */
    cmd  *ret_final;       /* el return que es la última sentencia (no hace falta salto) */
} fctx;

/* ---------------------------------------------------------------- tamaños */
static int nregs(const tipo *t)
{
    int k;
    if (t->base == T_STRUCT) { k = 0; for (int i = 0; i < t->s->nc; i++) k += nregs(&t->s->c[i].t); }
    else k = t->col > 1 ? t->col : 1;
    return k * (t->arr ? t->arr : 1);
}
static int nregs_elem(const tipo *t) { tipo e = *t; e.arr = 0; return nregs(&e); }
static int campo_reg(const st *s, int i) { int k = 0; for (int j = 0; j < i; j++) k += nregs(&s->c[j].t); return k; }
/* máscara de escritura de cada registro del tipo (vecN: N componentes) */
static int mascaras(const tipo *t, uint8_t *m)
{
    int n = 0, rep = t->arr ? t->arr : 1;
    for (int r = 0; r < rep; r++) {
        if (t->base == T_STRUCT) { for (int i = 0; i < t->s->nc; i++) n += mascaras(&t->s->c[i].t, m + n); }
        else { int cols = t->col > 1 ? t->col : 1; uint8_t mk = (uint8_t)((1 << (t->lin ? t->lin : 1)) - 1); for (int k = 0; k < cols; k++) m[n++] = mk; }
    }
    return n;
}
static uint32_t tipo_gl(const tipo *t)
{
    if (t->base == T_SAMPLER2D) return 0x8B5E;
    if (t->base == T_SAMPLERCUBE) return 0x8B60;
    if (t->col > 1) return 0x8B5A + (uint32_t)(t->col - 2);
    if (t->base == T_FLOAT) return t->lin == 1 ? 0x1406 : 0x8B50 + (uint32_t)(t->lin - 2);
    if (t->base == T_INT) return t->lin == 1 ? 0x1404 : 0x8B53 + (uint32_t)(t->lin - 2);
    if (t->base == T_BOOL) return t->lin == 1 ? 0x8B56 : 0x8B57 + (uint32_t)(t->lin - 2);
    return 0;
}

/* ---------------------------------------------------------------- emisión */
static ri_ins_t *emitir(comp *c, int op)
{
    ri_prog_t *p = c->ri;
    if (p->nins == p->cins) {
        int nc = p->cins ? p->cins * 2 : 256;
        ri_ins_t *ni = aloca((size_t)nc * sizeof(ri_ins_t));
        if (!ni) { falha(c, "sin memoria", NULL); static ri_ins_t z; memset(&z, 0, sizeof z); return &z; }
        if (p->nins) memcpy(ni, p->ins, (size_t)p->nins * sizeof(ri_ins_t));
        if (p->ins) libera(p->ins);
        p->ins = ni; p->cins = nc;
    }
    ri_ins_t *i = &p->ins[p->nins++];
    memset(i, 0, sizeof *i);
    i->op = (uint8_t)op; i->linha = (uint16_t)c->linha;
    return i;
}
static ri_dest_t D(int arq, int reg, int masc) { ri_dest_t d; memset(&d, 0, sizeof d); d.arq = (uint8_t)arq; d.ind = (uint16_t)reg; d.masc = (uint8_t)masc; return d; }
static ri_fonte_t F(val v) { ri_fonte_t f; memset(&f, 0, sizeof f); f.arq = v.arq; f.ind = (uint16_t)v.reg; f.sw = v.sw; return f; }
static ri_fonte_t Fneg(val v) { ri_fonte_t f = F(v); f.mod = RI_NEG; return f; }
static val V(int arq, int reg, uint8_t sw, tipo t) { val v; v.arq = (uint8_t)arq; v.reg = reg; v.sw = sw; v.t = t; return v; }
static val coluna(val v, int k) { tipo t = t_vec(v.t.base, v.t.lin); return V(v.arq, v.reg + k, RI_SW_XYZW, t); }
static val comp_de(val v, int i) { tipo t = t_esc(v.t.base); return V(v.arq, v.reg, SW_REP(RI_SW_C(v.sw, i)), t); }
static int escalar_t(const tipo *t) { return !t->arr && t->base != T_STRUCT && t->lin <= 1 && t->col <= 1; }
static val novo(comp *c, tipo t) { int r = c->ri->ntemp; c->ri->ntemp += nregs(&t); return V(RI_TEMP, r, escalar_t(&t) ? SW_REP(0) : RI_SW_XYZW, t); }
static val tmpf(comp *c, int n) { return novo(c, n == 1 ? t_esc(T_FLOAT) : t_vec(T_FLOAT, n)); }
static int masc_n(int n) { return (1 << n) - 1; }

static void ins1(comp *c, int op, ri_dest_t d, ri_fonte_t a) { ri_ins_t *i = emitir(c, op); i->d = d; i->f[0] = a; }
static void ins2(comp *c, int op, ri_dest_t d, ri_fonte_t a, ri_fonte_t b) { ri_ins_t *i = emitir(c, op); i->d = d; i->f[0] = a; i->f[1] = b; }
static void ins3(comp *c, int op, ri_dest_t d, ri_fonte_t a, ri_fonte_t b, ri_fonte_t x) { ri_ins_t *i = emitir(c, op); i->d = d; i->f[0] = a; i->f[1] = b; i->f[2] = x; }

/* d = op(a[, b[, x]]) por componente, n componentes, resultado nuevo */
static val op1(comp *c, int op, val a, int n) { val r = tmpf(c, n); ins1(c, op, D(RI_TEMP, r.reg, masc_n(n)), F(a)); return r; }
static val op2(comp *c, int op, val a, val b, int n) { val r = tmpf(c, n); ins2(c, op, D(RI_TEMP, r.reg, masc_n(n)), F(a), F(b)); return r; }
static val op3(comp *c, int op, val a, val b, val x, int n) { val r = tmpf(c, n); ins3(c, op, D(RI_TEMP, r.reg, masc_n(n)), F(a), F(b), F(x)); return r; }
/* operación escalar de la RI aplicada componente a componente */
static val escalar_n(comp *c, int op, val a, val b, int n)
{
    val r = tmpf(c, n);
    for (int i = 0; i < n; i++) {
        ri_ins_t *I = emitir(c, op);
        I->d = D(RI_TEMP, r.reg, 1 << i);
        I->f[0] = F(comp_de(a, a.t.lin > 1 || i == 0 ? i : 0));
        if (op == RI_POW) I->f[1] = F(comp_de(b, b.t.lin > 1 ? i : 0));
    }
    return r;
}

/* ---------------------------------------------------------------- inmediatos */
static int mesmo_float(float a, float b) { uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y; }
static int imed_reg_novo(comp *c)
{
    ri_prog_t *p = c->ri;
    if (p->nimed == p->cimed) {
        int nc = p->cimed ? p->cimed * 2 : 32;
        float (*ni)[4] = aloca((size_t)nc * sizeof *ni);
        uint8_t *nu = aloca((size_t)nc);
        if (!ni || !nu) { falha(c, "sin memoria", NULL); return 0; }
        if (p->nimed) { memcpy(ni, p->imed, (size_t)p->nimed * sizeof *ni); memcpy(nu, c->imed_uso, (size_t)p->nimed); }
        if (p->imed) libera(p->imed);
        if (c->imed_uso) libera(c->imed_uso);
        p->imed = ni; c->imed_uso = nu; p->cimed = nc;
    }
    int r = p->nimed++;
    p->imed[r][0] = p->imed[r][1] = p->imed[r][2] = p->imed[r][3] = 0;
    c->imed_uso[r] = 0;
    return r;
}
static val imed(comp *c, const float *v, int n, int base)
{
    ri_prog_t *p = c->ri;
    tipo t = n == 1 ? t_esc(base) : t_vec(base, n);
    if (n == 1) {
        for (int r = 0; r < p->nimed; r++) for (int k = 0; k < c->imed_uso[r]; k++) if (mesmo_float(p->imed[r][k], v[0])) return V(RI_IMED, r, SW_REP(k), t);
        for (int r = 0; r < p->nimed; r++) if (c->imed_uso[r] < 4) { int k = c->imed_uso[r]++; p->imed[r][k] = v[0]; return V(RI_IMED, r, SW_REP(k), t); }
        int r = imed_reg_novo(c); if (c->falhou) return V(RI_IMED, 0, 0, t);
        p->imed[r][0] = v[0]; c->imed_uso[r] = 1;
        return V(RI_IMED, r, SW_REP(0), t);
    }
    for (int r = 0; r < p->nimed; r++) {
        if (c->imed_uso[r] < n) continue;
        int ig = 1; for (int i = 0; i < n; i++) if (!mesmo_float(p->imed[r][i], v[i])) ig = 0;
        if (ig) return V(RI_IMED, r, RI_SW_XYZW, t);
    }
    int r = imed_reg_novo(c); if (c->falhou) return V(RI_IMED, 0, 0, t);
    for (int i = 0; i < n; i++) p->imed[r][i] = v[i];
    c->imed_uso[r] = (uint8_t)n;
    return V(RI_IMED, r, RI_SW_XYZW, t);
}
static val k1(comp *c, float x) { return imed(c, &x, 1, T_FLOAT); }
static val imed_tipo(comp *c, const double *v, const tipo *t)
{
    float f[4]; int n = t->lin ? t->lin : 1;
    for (int i = 0; i < n && i < 4; i++) f[i] = (float)v[i];
    return imed(c, f, n, t->base);
}

/* ---------------------------------------------------------------- salidas y entradas */
static int sai_nova(comp *c, int sem, int idx, int sombra)
{
    ri_prog_t *p = c->ri;
    if (p->nsai >= RI_MAX_SAI || c->nsombra >= RI_MAX_SAI) { falha(c, "demasiadas salidas", NULL); return 0; }
    int r = p->nsai++;
    p->sai[r].sem = (uint8_t)sem; p->sai[r].idx = (uint8_t)idx;
    c->sombra_tmp[c->nsombra] = sombra; c->sombra_sai[c->nsombra] = r; c->nsombra++;
    return r;
}
static int ent_nova(comp *c, int sem, int idx)
{
    ri_prog_t *p = c->ri;
    if (p->nent >= RI_MAX_ENT) { falha(c, "demasiadas entradas", NULL); return 0; }
    int r = p->nent++;
    p->ent[r].sem = (uint8_t)sem; p->ent[r].idx = (uint8_t)idx;
    return r;
}
static void var_registrar(comp *c, ri_var_t **lista, int *n, const char *nome, const tipo *t, int reg)
{
    ri_var_t *nv = aloca((size_t)(*n + 1) * sizeof(ri_var_t));
    if (!nv) { falha(c, "sin memoria", NULL); return; }
    if (*n) { memcpy(nv, *lista, (size_t)*n * sizeof(ri_var_t)); libera(*lista); }
    ri_var_t *v = &nv[*n];
    memset(v, 0, sizeof *v);
    copia_texto(v->nome, nome, sizeof v->nome);
    v->tipo_gl = tipo_gl(t); v->arr = t->arr; v->reg = reg; v->nreg = nregs_elem(t); v->comps = t->lin ? t->lin : 1;
    *lista = nv; (*n)++;
}
/* uniformes de struct: cada campo es «nome.campo» y «nome[i].campo», como los ve GL */
static void unif_registrar(comp *c, const char *nome, const tipo *t, int reg)
{
    ri_prog_t *p = c->ri;
    if (t->base == T_STRUCT) {
        int k = t->arr ? t->arr : 1, passo = nregs_elem(t);
        for (int i = 0; i < k; i++)
            for (int f = 0; f < t->s->nc; f++) {
                char nn[96];
                if (t->arr) formatar_texto(nn, sizeof nn, "%s[%d].%s", nome, i, t->s->c[f].nome);
                else formatar_texto(nn, sizeof nn, "%s.%s", nome, t->s->c[f].nome);
                unif_registrar(c, nn, &t->s->c[f].t, reg + i * passo + campo_reg(t->s, f));
            }
        return;
    }
    var_registrar(c, &p->unif, &p->nunif, nome, t, reg);
}
/* las variables embutidas reciben su sitio al usarse por primera vez */
static void embutida_sitio(comp *c, sim *s)
{
    ri_prog_t *p = c->ri;
    const char *n = s->nome;
    if (!compara_texto(n, "gl_Position") || !compara_texto(n, "gl_PointSize")) {
        val t = novo(c, s->t);
        s->arq = RI_TEMP; s->reg = t.reg;
        sai_nova(c, n[3] == 'P' && n[4] == 'o' && n[5] == 's' ? RI_SEM_POSICAO : RI_SEM_PONTO_TAM, 0, t.reg);
    } else if (!compara_texto(n, "gl_FragColor") || !compara_texto(n, "gl_FragData")) {
        if (c->reg_cor < 0) { tipo v4 = t_vec(T_FLOAT, 4); c->reg_cor = novo(c, v4).reg; sai_nova(c, RI_SEM_COR, 0, c->reg_cor); }
        s->arq = RI_TEMP; s->reg = c->reg_cor;
    } else if (!compara_texto(n, "gl_FragCoord")) { s->arq = RI_ENT; s->reg = ent_nova(c, RI_SEM_POSICAO, 0); p->usa_fragcoord = 1; }
    else if (!compara_texto(n, "gl_FrontFacing")) { s->arq = RI_ENT; s->reg = ent_nova(c, RI_SEM_FACE, 0); p->usa_face = 1; }
    else if (!compara_texto(n, "gl_PointCoord")) { s->arq = RI_ENT; s->reg = ent_nova(c, RI_SEM_PONTO_COORD, 0); p->usa_ponto_coord = 1; }
    else falha(c, "variable embutida sin sitio:", n);
    s->sw = RI_SW_XYZW;
}
static val val_sim(comp *c, sim *s)
{
    if (s->reg < 0) {
        if (s->classe == 5) embutida_sitio(c, s);
        else { falha(c, "variable usada antes de tener sitio:", s->nome); return V(RI_TEMP, 0, RI_SW_XYZW, s->t); }
    }
    uint8_t sw = (uint8_t)s->sw;
    if (escalar_t(&s->t)) sw = SW_REP(RI_SW_C(sw, 0));   /* un escalar se lee siempre replicado */
    return V(s->arq, s->reg, sw, s->t);
}

/* ---------------------------------------------------------------- utilidades del árbol */
static int puro(no *n)
{
    if (!n) return 1;
    if (n->k == N_ATR || n->k == N_PRE || n->k == N_POS) return 0;
    if (n->k == N_CHAMA && n->f) return 0;
    if (!puro(n->a) || !puro(n->b) || !puro(n->c)) return 0;
    for (int i = 0; i < n->nargs; i++) if (!puro(n->args[i])) return 0;
    return 1;
}
/* la variable raíz de una expresión que se escribe */
static sim *raiz_sim(no *n) { while (n && (n->k == N_CAMPO || n->k == N_IDX)) n = n->a; return n && n->k == N_VAR ? n->s : NULL; }
static int cmd_escreve_p(cmd *m, sim *s, int prof);
/* ¿escribe la expresión en s? (también dentro de las funciones que llama: una global) */
static int expr_escreve(no *n, sim *s, int prof)
{
    if (!n) return 0;
    if (prof > 40) return 1;
    if ((n->k == N_ATR || n->k == N_PRE || n->k == N_POS) && raiz_sim(n->a) == s) return 1;
    if (n->k == N_CHAMA && n->f) {
        for (int i = 0; i < n->nargs && i < n->f->np; i++) if (n->f->qual[i] && raiz_sim(n->args[i]) == s) return 1;
        if (n->f->corpo && cmd_escreve_p(n->f->corpo, s, prof + 1)) return 1;
    }
    if (expr_escreve(n->a, s, prof) || expr_escreve(n->b, s, prof) || expr_escreve(n->c, s, prof)) return 1;
    for (int i = 0; i < n->nargs; i++) if (expr_escreve(n->args[i], s, prof)) return 1;
    return 0;
}
static int cmd_escreve_p(cmd *m, sim *s, int prof)
{
    for (; m; m = m->prox) {
        if (expr_escreve(m->e, s, prof) || expr_escreve(m->e2, s, prof) || expr_escreve(m->e3, s, prof) || expr_escreve(m->init, s, prof)) return 1;
        if (cmd_escreve_p(m->a, s, prof) || cmd_escreve_p(m->b, s, prof) || cmd_escreve_p(m->filhos, s, prof)) return 1;
    }
    return 0;
}
static int cmd_escreve(cmd *m, sim *s) { return cmd_escreve_p(m, s, 0); }
/* ¿hay return (fuera de «excepto»)? dentro_laco: ¿alguno dentro de un bucle? */
static void cmd_returns(cmd *m, cmd *excepto, int laco, int *ret, int *ret_laco)
{
    for (; m; m = m->prox) {
        switch (m->k) {
        case 7: if (m != excepto) { (*ret)++; if (laco) (*ret_laco)++; } break;
        case 2: cmd_returns(m->filhos, excepto, laco, ret, ret_laco); break;
        case 3: cmd_returns(m->a, excepto, laco, ret, ret_laco); cmd_returns(m->b, excepto, laco, ret, ret_laco); break;
        case 4: cmd_returns(m->a, excepto, laco, ret, ret_laco); cmd_returns(m->b, excepto, 1, ret, ret_laco); break;
        case 5: case 6: cmd_returns(m->b, excepto, 1, ret, ret_laco); break;
        }
    }
}
/* continue que pertenece a este bucle (no a uno anidado) */
static int cmd_continue(cmd *m)
{
    for (; m; m = m->prox) {
        if (m->k == 9) return 1;
        if (m->k == 4 || m->k == 5 || m->k == 6) continue;
        if (cmd_continue(m->a) || cmd_continue(m->b) || cmd_continue(m->filhos)) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- lvalues */
typedef struct {
    uint8_t arq; int reg; uint8_t bsw; tipo t;
    int nsw; uint8_t sw[4];          /* swizzle de componentes (sobre bsw) */
    int tem_idx; val idx;            /* registro = reg + idx (índice dinámico, ya multiplicado) */
    int tem_comp; val comp;          /* componente dinámica de un vector */
} lval;

static val gexpr(comp *c, no *n);
static val carregar(comp *c, lval *l);
static void atribuir(comp *c, lval *l, val v);

static lval lval_de_val(val v) { lval l; memset(&l, 0, sizeof l); l.arq = v.arq; l.reg = v.reg; l.bsw = v.sw; l.t = v.t; return l; }
static void arl(comp *c, val idx) { ins1(c, RI_ARL, D(RI_END, 0, 1), F(idx)); c->ri->usa_a0 = 1; }

static lval glval(comp *c, no *n)
{
    if (!c->falhou && !n->ok) { if (checar(c, n)) n->ok = 1; }
    if (c->falhou) { lval z; memset(&z, 0, sizeof z); z.arq = RI_TEMP; z.t = t_esc(T_FLOAT); return z; }
    if (n->k == N_VAR) return lval_de_val(val_sim(c, n->s));
    if (n->k == N_CAMPO) {
        lval b = glval(c, n->a);
        if (n->nsw < 0) {   /* campo de struct */
            const st *s = b.t.s; int f = 0;
            for (int i = 0; i < s->nc; i++) if (!compara_texto(s->c[i].nome, n->campo)) f = i;
            b.reg += campo_reg(s, f); b.t = n->t;
            return b;
        }
        if (b.tem_comp) { val v = carregar(c, &b); b = lval_de_val(v); }
        uint8_t nsw[4];
        for (int i = 0; i < n->nsw; i++) nsw[i] = b.nsw ? b.sw[n->sw[i]] : (uint8_t)n->sw[i];
        b.nsw = n->nsw; memcpy(b.sw, nsw, 4); b.t = n->t;
        return b;
    }
    if (n->k == N_IDX) {
        lval b = glval(c, n->a);
        if (b.nsw || b.tem_comp) { val v = carregar(c, &b); b = lval_de_val(v); }
        tipo t = b.t;
        double k[16];
        int constante = avaliar_const(c, n->b, k);
        if (t.arr || t.col > 1) {   /* elemento de array o columna de matriz: registros */
            int passo = t.arr ? nregs_elem(&t) : 1, cont = t.arr ? t.arr : t.col;
            if (constante) {
                int i = (int)k[0];
                if (i < 0 || i >= cont) { falha(c, "índice constante fuera del array", NULL); return b; }
                b.reg += i * passo; b.t = n->t;
                return b;
            }
            if (b.arq == RI_AMOST) { falha(c, "los samplers solo se indexan con constantes", NULL); return b; }
            val iv = gexpr(c, n->b);
            if (iv.t.base != T_INT || !eh_escalar(&iv.t)) { falha(c, "el índice tiene que ser un int", NULL); return b; }
            /* registro = reg + i*passo (+ el índice que ya tuviera) */
            val total = iv;
            if (passo != 1) total = op2(c, RI_MUL, iv, k1(c, (float)passo), 1);
            if (b.tem_idx) total = op2(c, RI_ADD, total, b.idx, 1);
            b.tem_idx = 1; b.idx = total; b.t = n->t;
            if (b.arq == RI_TEMP && c->ri->tarr_n == 0) { /* se marca abajo, en la declaración */ }
            return b;
        }
        /* componente de un vector */
        if (constante) {
            int i = (int)k[0];
            if (i < 0 || i >= t.lin) { falha(c, "índice constante fuera del vector", NULL); return b; }
            b.nsw = 1; b.sw[0] = (uint8_t)i; b.t = n->t;
            return b;
        }
        val iv = gexpr(c, n->b);
        if (iv.t.base != T_INT || !eh_escalar(&iv.t)) { falha(c, "el índice tiene que ser un int", NULL); return b; }
        b.tem_comp = 1; b.comp = iv; b.t = n->t;   /* el tipo del elemento; el del vector se recupera abajo */
        b.sw[3] = (uint8_t)t.lin;                  /* (componentes del vector) */
        return b;
    }
    return lval_de_val(gexpr(c, n));
}

/* lee el registro base (con el índice dinámico, si lo hay) */
static val carregar_base(comp *c, lval *l, tipo t)
{
    if (!l->tem_idx) return V(l->arq, l->reg, l->bsw, t);
    val r = novo(c, t);
    int k = nregs(&t);
    arl(c, l->idx);
    for (int i = 0; i < k; i++) {
        ri_fonte_t f; memset(&f, 0, sizeof f); f.arq = l->arq; f.ind = (uint16_t)(l->reg + i); f.sw = k > 1 ? RI_SW_XYZW : l->bsw; f.rel = 1;
        ins1(c, RI_MOV, D(RI_TEMP, r.reg + i, 15), f);
    }
    return r;
}
static val carregar(comp *c, lval *l)
{
    if (l->tem_comp) {
        int nv = l->sw[3];
        val vec = carregar_base(c, l, t_vec(l->t.base, nv));
        /* r = (i == k) ? v.k : r, para cada k */
        val r = tmpf(c, 1); r.t = l->t;
        ins1(c, RI_MOV, D(RI_TEMP, r.reg, 1), F(comp_de(vec, 0)));
        for (int k = 1; k < nv; k++) {
            val e = op2(c, RI_SEQ, l->comp, k1(c, (float)k), 1);
            ins3(c, RI_CMP, D(RI_TEMP, r.reg, 1), Fneg(e), F(comp_de(vec, k)), F(r));
        }
        return r;
    }
    tipo tb = l->t;
    if (l->nsw) tb = t_vec(l->t.base, 4);
    val v = carregar_base(c, l, l->nsw ? tb : l->t);
    if (l->nsw) {
        uint8_t sw = 0;
        for (int i = 0; i < 4; i++) sw |= (uint8_t)(RI_SW_C(v.sw, l->sw[i < l->nsw ? i : l->nsw - 1]) << (2 * i));
        v.sw = sw; v.t = l->t;
    }
    return v;
}
static void mov_regs(comp *c, int arq, int reg, int rel, val v, const tipo *t)
{
    int k = nregs(t);
    uint8_t mm[256], *m = k <= 256 ? mm : anovo(&c->ar, k);
    if (!m) return;
    mascaras(t, m);
    for (int i = 0; i < k; i++) {
        ri_dest_t d = D(arq, reg + i, m[i]); d.rel = (uint8_t)rel;
        ri_fonte_t f = F(k > 1 ? V(v.arq, v.reg + i, RI_SW_XYZW, v.t) : v);
        ins1(c, RI_MOV, d, f);
    }
}
static void atribuir(comp *c, lval *l, val v)
{
    if (c->falhou) return;
    if (l->arq != RI_TEMP && l->arq != RI_SAI) { falha(c, "no se puede escribir ahí", NULL); return; }
    if (l->tem_comp) {
        /* carga el vector, cambia la componente elegida y lo vuelve a guardar */
        int nv = l->sw[3];
        tipo tv = t_vec(l->t.base, nv);
        lval base = *l; base.tem_comp = 0; base.t = tv; base.sw[3] = 0;
        val vec = carregar_base(c, &base, tv);
        val e2 = novo(c, tv);
        ins1(c, RI_MOV, D(RI_TEMP, e2.reg, masc_n(nv)), F(vec));
        for (int k = 0; k < nv; k++) {
            val e = op2(c, RI_SEQ, l->comp, k1(c, (float)k), 1);
            ins3(c, RI_CMP, D(RI_TEMP, e2.reg, 1 << k), Fneg(e), F(comp_de(v, 0)), F(comp_de(e2, k)));
        }
        base.bsw = RI_SW_XYZW;
        atribuir(c, &base, e2);
        return;
    }
    if (l->tem_idx) arl(c, l->idx);
    if (l->nsw) {
        int masc = 0; uint8_t sw = 0;
        for (int i = 0; i < l->nsw; i++) { masc |= 1 << l->sw[i]; sw |= (uint8_t)(RI_SW_C(v.sw, v.t.lin > 1 ? i : 0) << (2 * l->sw[i])); }
        ri_dest_t d = D(l->arq, l->reg, masc); d.rel = (uint8_t)l->tem_idx;
        ri_fonte_t f = F(v); f.sw = sw;
        ins1(c, RI_MOV, d, f);
        return;
    }
    mov_regs(c, l->arq, l->reg, l->tem_idx, v, &l->t);
}
static void atribuir_sim(comp *c, sim *s, val v) { lval l = lval_de_val(val_sim(c, s)); atribuir(c, &l, v); }

/* ---------------------------------------------------------------- aritmética */
static val recip(comp *c, val b, int n)
{
    if (b.t.lin <= 1 || n == 1) return escalar_n(c, RI_RCP, b, b, 1);
    return escalar_n(c, RI_RCP, b, b, n);
}
static val mat_vec(comp *c, val m, val v)
{
    int N = m.t.col;
    val r = tmpf(c, N);
    ins2(c, RI_MUL, D(RI_TEMP, r.reg, masc_n(N)), F(coluna(m, 0)), F(comp_de(v, 0)));
    for (int k = 1; k < N; k++) ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(N)), F(coluna(m, k)), F(comp_de(v, k)), F(r));
    return r;
}
static val vec_mat(comp *c, val v, val m)
{
    int N = m.t.col;
    val r = tmpf(c, N);
    for (int k = 0; k < N; k++) ins2(c, N == 2 ? RI_DP2 : N == 3 ? RI_DP3 : RI_DP4, D(RI_TEMP, r.reg, 1 << k), F(v), F(coluna(m, k)));
    return r;
}
static val mat_mat(comp *c, val a, val b)
{
    int N = a.t.col;
    val r = novo(c, a.t);
    for (int j = 0; j < N; j++) {
        val bj = coluna(b, j);
        ins2(c, RI_MUL, D(RI_TEMP, r.reg + j, masc_n(N)), F(coluna(a, 0)), F(comp_de(bj, 0)));
        for (int k = 1; k < N; k++) ins3(c, RI_MAD, D(RI_TEMP, r.reg + j, masc_n(N)), F(coluna(a, k)), F(comp_de(bj, k)), F(V(RI_TEMP, r.reg + j, RI_SW_XYZW, bj.t)));
    }
    return r;
}
/* división entera (trunca hacia cero), n componentes */
static val div_int(comp *c, val a, val b, tipo rt)
{
    int n = rt.lin;
    val A = tmpf(c, n), B = tmpf(c, n);
    ri_fonte_t fa = F(a); fa.mod = RI_ABS; ri_fonte_t fb = F(b); fb.mod = RI_ABS;
    ins1(c, RI_MOV, D(RI_TEMP, A.reg, masc_n(n)), fa);
    ins1(c, RI_MOV, D(RI_TEMP, B.reg, masc_n(n)), fb);
    A.t = B.t = rt; A.t.base = B.t.base = T_FLOAT;
    if (b.t.lin == 1 && n > 1) { B.sw = SW_REP(0); B.t = t_esc(T_FLOAT); }
    val rb = recip(c, B, n);
    val q = op2(c, RI_MUL, A, rb, n);
    ins1(c, RI_FLR, D(RI_TEMP, q.reg, masc_n(n)), F(q));
    val r = tmpf(c, n);
    ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(n)), Fneg(q), F(B), F(A));        /* r = A - q*B */
    val ajuste = op2(c, RI_SGE, r, B, n);
    ins2(c, RI_ADD, D(RI_TEMP, q.reg, masc_n(n)), F(q), F(ajuste));
    val baixo = op2(c, RI_SLT, r, k1(c, 0), n);
    ins2(c, RI_ADD, D(RI_TEMP, q.reg, masc_n(n)), F(q), Fneg(baixo));
    val s = op2(c, RI_MUL, a, b, n);
    val res = tmpf(c, n);
    ins3(c, RI_CMP, D(RI_TEMP, res.reg, masc_n(n)), F(s), Fneg(q), F(q));
    res.t = rt;
    return res;
}
static val aritmetica(comp *c, int op, val a, val b, tipo rt)
{
    int ma = eh_matriz(&a.t), mb = eh_matriz(&b.t);
    if (op == '*' && ma && mb) return mat_mat(c, a, b);
    if (op == '*' && ma && eh_vetor(&b.t)) { val r = mat_vec(c, a, b); r.t = rt; return r; }
    if (op == '*' && eh_vetor(&a.t) && mb) { val r = vec_mat(c, a, b); r.t = rt; return r; }
    if (op == '/' && rt.base == T_INT) return div_int(c, a, b, rt);
    int regs = nregs(&rt), n = rt.lin;
    val r = novo(c, rt);
    val rb; int tem_rb = 0;
    if (op == '/' && !mb) { rb = recip(c, b, b.t.lin > 1 ? n : 1); tem_rb = 1; }
    for (int k = 0; k < regs; k++) {
        val ak = ma ? coluna(a, k) : a, bk = mb ? coluna(b, k) : b;
        ri_dest_t d = D(RI_TEMP, r.reg + k, masc_n(n));
        switch (op) {
        case '+': ins2(c, RI_ADD, d, F(ak), F(bk)); break;
        case '-': ins2(c, RI_ADD, d, F(ak), Fneg(bk)); break;
        case '*': ins2(c, RI_MUL, d, F(ak), F(bk)); break;
        default: {
            val inv = tem_rb ? rb : recip(c, bk, n);
            ins2(c, RI_MUL, d, F(ak), F(inv));
        }
        }
    }
    return r;
}

/* ---------------------------------------------------------------- construtores */
typedef struct { val v; int comp; } peca;   /* una componente de un argumento */
static void conv_comp(comp *c, int base_dst, int base_src, ri_dest_t d, ri_fonte_t f)
{
    if (base_dst == T_INT && base_src == T_FLOAT) ins1(c, RI_TRUNC, d, f);
    else if (base_dst == T_BOOL && base_src != T_BOOL) { val z = k1(c, 0); ins2(c, RI_SNE, d, f, F(z)); }
    else ins1(c, RI_MOV, d, f);
}
static val construtor(comp *c, no *n)
{
    tipo t = n->t;
    int na = n->nargs;
    if (t.arr) { falha(c, "GLSL ES 1.00 no tiene constructores de arrays", NULL); return novo(c, t_esc(T_FLOAT)); }
    val args[20];
    for (int i = 0; i < na && i < 20; i++) args[i] = gexpr(c, n->args[i]);
    if (c->falhou) return novo(c, t);
    val r = novo(c, t);
    if (t.base == T_STRUCT) {
        if (na != t.s->nc) { falha(c, "el constructor de la struct necesita un argumento por campo", NULL); return r; }
        for (int i = 0; i < na; i++) {
            if (!tipo_igual(&args[i].t, &t.s->c[i].t)) { falha(c, "tipo de un campo en el constructor de la struct:", t.s->c[i].nome); return r; }
            lval l = lval_de_val(r); l.reg += campo_reg(t.s, i); l.t = t.s->c[i].t; l.bsw = RI_SW_XYZW;
            atribuir(c, &l, args[i]);
        }
        return r;
    }
    if (na == 0) { falha(c, "constructor sin argumentos", NULL); return r; }
    for (int i = 0; i < na; i++) if (args[i].t.base == T_STRUCT || args[i].t.arr || args[i].t.base >= T_SAMPLER2D) { falha(c, "argumento no válido en un constructor", NULL); return r; }
    int N = t.lin, cols = t.col > 1 ? t.col : 1;
    if (na == 1 && eh_escalar(&args[0].t)) {
        if (cols > 1) {   /* matriz con la diagonal */
            val z = k1(c, 0);
            for (int k = 0; k < cols; k++) {
                ins1(c, RI_MOV, D(RI_TEMP, r.reg + k, masc_n(N)), F(z));
                conv_comp(c, t.base, args[0].t.base, D(RI_TEMP, r.reg + k, 1 << k), F(args[0]));
            }
        } else conv_comp(c, t.base, args[0].t.base, D(RI_TEMP, r.reg, masc_n(N)), F(args[0]));
        return r;
    }
    if (na == 1 && cols > 1 && eh_matriz(&args[0].t)) {   /* de otra matriz: la esquina, el resto de la identidad */
        int M = args[0].t.col;
        val z = k1(c, 0), u = k1(c, 1);
        for (int k = 0; k < cols; k++) {
            if (N > M || k >= M) ins1(c, RI_MOV, D(RI_TEMP, r.reg + k, masc_n(N)), F(z));
            if (k < M) ins1(c, RI_MOV, D(RI_TEMP, r.reg + k, masc_n(N < M ? N : M)), F(coluna(args[0], k)));
            else ins1(c, RI_MOV, D(RI_TEMP, r.reg + k, 1 << k), F(u));
        }
        return r;
    }
    if (na == 1 && cols == 1 && eh_matriz(&args[0].t)) { falha(c, "un vector no se construye de una matriz", NULL); return r; }
    /* el caso general: las componentes de los argumentos, en orden, columna a columna */
    int total = N * cols, pos = 0;
    for (int a = 0; a < na; a++) {
        if (pos >= total) { falha(c, "demasiados argumentos en el constructor", NULL); return r; }
        val v = args[a];
        int vcols = v.t.col > 1 ? v.t.col : 1, vn = v.t.lin;
        for (int vc = 0; vc < vcols && pos < total; vc++) {
            val src = vcols > 1 ? coluna(v, vc) : v;
            int i = 0;
            while (i < vn && pos < total) {
                /* un trozo que cae en el mismo registro de destino */
                int dreg = pos / N, dcomp = pos % N;
                int k = 0, masc = 0; uint8_t sw = 0;
                while (i + k < vn && dcomp + k < N && pos + k < total) {
                    masc |= 1 << (dcomp + k);
                    sw |= (uint8_t)(RI_SW_C(src.sw, i + k) << (2 * (dcomp + k)));
                    k++;
                }
                ri_fonte_t f = F(src); f.sw = sw;
                conv_comp(c, t.base, v.t.base, D(RI_TEMP, r.reg + dreg, masc), f);
                i += k; pos += k;
            }
        }
    }
    if (pos < total) falha(c, "faltan argumentos en el constructor", NULL);
    return r;
}

/* ---------------------------------------------------------------- funciones embutidas */
static val atan_t(comp *c, val t, int n)   /* atan(t) para t en [0, 1]: t * P(t^2), error < 2e-6 */
{
    static const float k[6] = { 0.9999772310f, -0.3326228559f, 0.1935404092f, -0.1164262742f, 0.0526469573f, -0.0117189428f };
    val t2 = op2(c, RI_MUL, t, t, n);
    val r = tmpf(c, n);
    ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(n)), F(t2), F(k1(c, k[5])), F(k1(c, k[4])));
    for (int i = 3; i >= 0; i--) ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(n)), F(r), F(t2), F(k1(c, k[i])));
    ins2(c, RI_MUL, D(RI_TEMP, r.reg, masc_n(n)), F(r), F(t));
    return r;
}
static val atan2_v(comp *c, val y, val x, int n)
{
    val ay = tmpf(c, n), ax = tmpf(c, n);
    ri_fonte_t fy = F(y); fy.mod = RI_ABS; ri_fonte_t fx = F(x); fx.mod = RI_ABS;
    ins1(c, RI_MOV, D(RI_TEMP, ay.reg, masc_n(n)), fy);
    ins1(c, RI_MOV, D(RI_TEMP, ax.reg, masc_n(n)), fx);
    val mn = op2(c, RI_MIN, ay, ax, n), mx = op2(c, RI_MAX, ay, ax, n);
    ins2(c, RI_MAX, D(RI_TEMP, mx.reg, masc_n(n)), F(mx), F(k1(c, 1e-30f)));
    val t = op2(c, RI_MUL, mn, recip(c, mx, n), n);
    val a = atan_t(c, t, n);
    /* |y| > |x|: pi/2 - a */
    val troca = op2(c, RI_SLT, ax, ay, n);
    val b = tmpf(c, n);
    ins3(c, RI_MAD, D(RI_TEMP, b.reg, masc_n(n)), F(a), F(k1(c, -2)), F(k1(c, 1.5707963268f)));   /* pi/2 - 2a */
    ins3(c, RI_MAD, D(RI_TEMP, a.reg, masc_n(n)), F(troca), F(b), F(a));                          /* a + troca*(pi/2 - 2a) */
    /* x < 0: pi - a */
    val e = tmpf(c, n);
    ins2(c, RI_ADD, D(RI_TEMP, e.reg, masc_n(n)), F(k1(c, 3.1415926536f)), Fneg(a));
    ins3(c, RI_CMP, D(RI_TEMP, a.reg, masc_n(n)), F(x), F(e), F(a));
    /* y < 0: -a */
    ins3(c, RI_CMP, D(RI_TEMP, a.reg, masc_n(n)), F(y), Fneg(a), F(a));
    return a;
}
static val sqrt_v(comp *c, val x, int n) { val r = escalar_n(c, RI_RSQ, x, x, n); return escalar_n(c, RI_RCP, r, r, n); }
static val dot_v(comp *c, val a, val b, int n)
{
    if (n == 1) return op2(c, RI_MUL, a, b, 1);
    return op2(c, n == 2 ? RI_DP2 : n == 3 ? RI_DP3 : RI_DP4, a, b, 1);
}
static val textura(comp *c, no *n, val *a, int na);

static val embutida(comp *c, no *n)
{
    val a[4]; int na = n->nargs;
    for (int i = 0; i < na && i < 4; i++) a[i] = gexpr(c, n->args[i]);
    if (c->falhou) return novo(c, t_esc(T_FLOAT));
    int k = n->t.lin ? n->t.lin : 1, n0 = na ? (a[0].t.lin ? a[0].t.lin : 1) : 1;
    val r;
    switch (n->embutida) {
    case E_RADIANS: r = op2(c, RI_MUL, a[0], k1(c, 0.017453292519943295f), k); break;
    case E_DEGREES: r = op2(c, RI_MUL, a[0], k1(c, 57.29577951308232f), k); break;
    case E_SIN: r = escalar_n(c, RI_SIN, a[0], a[0], k); break;
    case E_COS: r = escalar_n(c, RI_COS, a[0], a[0], k); break;
    case E_TAN: { val s = escalar_n(c, RI_SIN, a[0], a[0], k), co = escalar_n(c, RI_COS, a[0], a[0], k); r = op2(c, RI_MUL, s, recip(c, co, k), k); break; }
    case E_ASIN: case E_ACOS: {
        val um = tmpf(c, k);
        ins3(c, RI_MAD, D(RI_TEMP, um.reg, masc_n(k)), Fneg(a[0]), F(a[0]), F(k1(c, 1)));   /* 1 - x^2 */
        ins2(c, RI_MAX, D(RI_TEMP, um.reg, masc_n(k)), F(um), F(k1(c, 0)));
        val s = sqrt_v(c, um, k);
        r = n->embutida == E_ASIN ? atan2_v(c, a[0], s, k) : atan2_v(c, s, a[0], k);
        break;
    }
    case E_ATAN: {   /* atan(x) = sign(x) * atan(|x|) con atan(|x|) = pi/2 - atan(1/|x|) si |x| > 1 */
        val ax = tmpf(c, k); ri_fonte_t f = F(a[0]); f.mod = RI_ABS;
        ins1(c, RI_MOV, D(RI_TEMP, ax.reg, masc_n(k)), f);
        val t = op2(c, RI_MIN, ax, recip(c, ax, k), k);
        val at = atan_t(c, t, k);
        val grande = op2(c, RI_SLT, k1(c, 1), ax, k);
        val b = tmpf(c, k);
        ins3(c, RI_MAD, D(RI_TEMP, b.reg, masc_n(k)), F(at), F(k1(c, -2)), F(k1(c, 1.5707963268f)));
        ins3(c, RI_MAD, D(RI_TEMP, at.reg, masc_n(k)), F(grande), F(b), F(at));
        ins3(c, RI_CMP, D(RI_TEMP, at.reg, masc_n(k)), F(a[0]), Fneg(at), F(at));
        r = at; break;
    }
    case E_ATAN2: r = atan2_v(c, a[0], a[1], k); break;
    case E_POW: r = escalar_n(c, RI_POW, a[0], a[1], k); break;
    case E_EXP: { val m = op2(c, RI_MUL, a[0], k1(c, 1.4426950408889634f), k); r = escalar_n(c, RI_EX2, m, m, k); break; }
    case E_LOG: { val l = escalar_n(c, RI_LG2, a[0], a[0], k); r = op2(c, RI_MUL, l, k1(c, 0.6931471805599453f), k); break; }
    case E_EXP2: r = escalar_n(c, RI_EX2, a[0], a[0], k); break;
    case E_LOG2: r = escalar_n(c, RI_LG2, a[0], a[0], k); break;
    case E_SQRT: r = sqrt_v(c, a[0], k); break;
    case E_INVSQRT: r = escalar_n(c, RI_RSQ, a[0], a[0], k); break;
    case E_ABS: { r = tmpf(c, k); ri_fonte_t f = F(a[0]); f.mod = RI_ABS; ins1(c, RI_MOV, D(RI_TEMP, r.reg, masc_n(k)), f); break; }
    case E_SIGN: { val z = k1(c, 0); val p = op2(c, RI_SLT, z, a[0], k), m = op2(c, RI_SLT, a[0], z, k); r = tmpf(c, k); ins2(c, RI_ADD, D(RI_TEMP, r.reg, masc_n(k)), F(p), Fneg(m)); break; }
    case E_FLOOR: r = op1(c, RI_FLR, a[0], k); break;
    case E_CEIL: { r = tmpf(c, k); ins1(c, RI_FLR, D(RI_TEMP, r.reg, masc_n(k)), Fneg(a[0])); ins1(c, RI_MOV, D(RI_TEMP, r.reg, masc_n(k)), Fneg(r)); break; }
    case E_FRACT: r = op1(c, RI_FRC, a[0], k); break;
    case E_MOD: {
        val q = op2(c, RI_MUL, a[0], recip(c, a[1], a[1].t.lin > 1 ? k : 1), k);
        ins1(c, RI_FLR, D(RI_TEMP, q.reg, masc_n(k)), F(q));
        r = tmpf(c, k);
        ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(k)), Fneg(a[1]), F(q), F(a[0]));
        break;
    }
    case E_MIN: r = op2(c, RI_MIN, a[0], a[1], k); break;
    case E_MAX: r = op2(c, RI_MAX, a[0], a[1], k); break;
    case E_CLAMP: r = op2(c, RI_MAX, a[0], a[1], k); ins2(c, RI_MIN, D(RI_TEMP, r.reg, masc_n(k)), F(r), F(a[2])); break;
    case E_MIX:
        if (a[2].t.base == T_BOOL) { r = tmpf(c, k); ins3(c, RI_CMP, D(RI_TEMP, r.reg, masc_n(k)), Fneg(a[2]), F(a[1]), F(a[0])); break; }
        { val d = tmpf(c, k); ins2(c, RI_ADD, D(RI_TEMP, d.reg, masc_n(k)), F(a[1]), Fneg(a[0])); r = op3(c, RI_MAD, d, a[2], a[0], k); }
        break;
    case E_STEP: r = op2(c, RI_SGE, a[1], a[0], k); break;
    case E_SMOOTH: {
        val den = tmpf(c, k > 1 && a[1].t.lin > 1 ? k : 1);
        int kd = k > 1 && a[1].t.lin > 1 ? k : 1;
        ins2(c, RI_ADD, D(RI_TEMP, den.reg, masc_n(kd)), F(a[1]), Fneg(a[0]));
        val t = tmpf(c, k);
        ins2(c, RI_ADD, D(RI_TEMP, t.reg, masc_n(k)), F(a[2]), Fneg(a[0]));
        val inv = recip(c, den, kd);
        ri_ins_t *I = emitir(c, RI_MUL); I->d = D(RI_TEMP, t.reg, masc_n(k)); I->d.sat = 1; I->f[0] = F(t); I->f[1] = F(inv);
        val p = tmpf(c, k);
        ins3(c, RI_MAD, D(RI_TEMP, p.reg, masc_n(k)), F(t), F(k1(c, -2)), F(k1(c, 3)));
        ins2(c, RI_MUL, D(RI_TEMP, p.reg, masc_n(k)), F(p), F(t));
        r = op2(c, RI_MUL, p, t, k);
        break;
    }
    case E_LENGTH: { val d = dot_v(c, a[0], a[0], n0); r = sqrt_v(c, d, 1); break; }
    case E_DISTANCE: { val d = tmpf(c, n0); ins2(c, RI_ADD, D(RI_TEMP, d.reg, masc_n(n0)), F(a[0]), Fneg(a[1])); val q = dot_v(c, d, d, n0); r = sqrt_v(c, q, 1); break; }
    case E_DOT: r = dot_v(c, a[0], a[1], n0); break;
    case E_CROSS: {
        val t = tmpf(c, 3);
        ri_fonte_t ay = F(a[0]), bz = F(a[1]), az = F(a[0]), by = F(a[1]);
        #define SWC(v, i, j, k2) (uint8_t)(RI_SW_C((v).sw, i) | (RI_SW_C((v).sw, j) << 2) | (RI_SW_C((v).sw, k2) << 4) | (RI_SW_C((v).sw, k2) << 6))
        ay.sw = SWC(a[0], 1, 2, 0); bz.sw = SWC(a[1], 2, 0, 1); az.sw = SWC(a[0], 2, 0, 1); by.sw = SWC(a[1], 1, 2, 0);
        #undef SWC
        ins2(c, RI_MUL, D(RI_TEMP, t.reg, 7), az, by);
        r = tmpf(c, 3);
        ins3(c, RI_MAD, D(RI_TEMP, r.reg, 7), ay, bz, Fneg(t));
        break;
    }
    case E_NORMALIZE: { val d = dot_v(c, a[0], a[0], k); val q = escalar_n(c, RI_RSQ, d, d, 1); r = op2(c, RI_MUL, a[0], q, k); break; }
    case E_FACEFWD: { val d = dot_v(c, a[2], a[1], k); r = tmpf(c, k); ins3(c, RI_CMP, D(RI_TEMP, r.reg, masc_n(k)), F(d), F(a[0]), Fneg(a[0])); break; }
    case E_REFLECT: {
        val d = dot_v(c, a[1], a[0], k);
        val d2 = op2(c, RI_ADD, d, d, 1);
        r = tmpf(c, k); ins3(c, RI_MAD, D(RI_TEMP, r.reg, masc_n(k)), Fneg(d2), F(a[1]), F(a[0]));
        break;
    }
    case E_REFRACT: {
        val d = dot_v(c, a[1], a[0], k);
        val t = tmpf(c, 1); ins3(c, RI_MAD, D(RI_TEMP, t.reg, 1), Fneg(d), F(d), F(k1(c, 1)));           /* 1 - d^2 */
        val e2 = op2(c, RI_MUL, a[2], a[2], 1);
        val kk = tmpf(c, 1); ins3(c, RI_MAD, D(RI_TEMP, kk.reg, 1), Fneg(e2), F(t), F(k1(c, 1)));      /* k */
        val km = op2(c, RI_MAX, kk, k1(c, 0), 1);
        val sk = sqrt_v(c, km, 1);
        val q = op3(c, RI_MAD, a[2], d, sk, 1);                                                         /* eta*d + sqrt(k) */
        val e = op2(c, RI_MUL, a[2], a[0], k);
        ins3(c, RI_MAD, D(RI_TEMP, e.reg, masc_n(k)), Fneg(q), F(a[1]), F(e));
        r = tmpf(c, k); ins3(c, RI_CMP, D(RI_TEMP, r.reg, masc_n(k)), F(kk), F(k1(c, 0)), F(e));
        break;
    }
    case E_MATCOMP: { r = novo(c, a[0].t); for (int j = 0; j < a[0].t.col; j++) ins2(c, RI_MUL, D(RI_TEMP, r.reg + j, masc_n(a[0].t.lin)), F(coluna(a[0], j)), F(coluna(a[1], j))); r.t = n->t; return r; }
    case E_LT: r = op2(c, RI_SLT, a[0], a[1], k); break;
    case E_LE: r = op2(c, RI_SGE, a[1], a[0], k); break;
    case E_GT: r = op2(c, RI_SLT, a[1], a[0], k); break;
    case E_GE: r = op2(c, RI_SGE, a[0], a[1], k); break;
    case E_EQ: r = op2(c, RI_SEQ, a[0], a[1], k); break;
    case E_NE: r = op2(c, RI_SNE, a[0], a[1], k); break;
    case E_ANY: { val u = k1(c, 1); val d = dot_v(c, a[0], u, n0); r = op2(c, RI_SNE, d, k1(c, 0), 1); break; }
    case E_ALL: { val u = k1(c, 1); val d = dot_v(c, a[0], u, n0); r = op2(c, RI_SEQ, d, k1(c, (float)n0), 1); break; }
    case E_NOT: r = op2(c, RI_SEQ, a[0], k1(c, 0), k); break;
    case E_DFDX: case E_DFDY:
        if (c->tipo_shader != 1) { falha(c, "las derivadas solo existen en los sombreadores de fragmentos", NULL); return novo(c, n->t); }
        r = op1(c, n->embutida == E_DFDX ? RI_DDX : RI_DDY, a[0], k); c->ri->usa_ddx = 1; break;
    case E_FWIDTH: {
        if (c->tipo_shader != 1) { falha(c, "las derivadas solo existen en los sombreadores de fragmentos", NULL); return novo(c, n->t); }
        val x = op1(c, RI_DDX, a[0], k), y = op1(c, RI_DDY, a[0], k);
        ri_fonte_t fx = F(x), fy = F(y); fx.mod = fy.mod = RI_ABS;
        r = tmpf(c, k); ins2(c, RI_ADD, D(RI_TEMP, r.reg, masc_n(k)), fx, fy);
        c->ri->usa_ddx = 1;
        break;
    }
    default: return textura(c, n, a, na);
    }
    r.t = n->t;
    return r;
}

static val textura(comp *c, no *n, val *a, int na)
{
    (void)na;
    int e = n->embutida;
    int cubo = e == E_TEXCUBE || e == E_TEXCUBEB || e == E_TEXCUBELOD;
    int proj = e == E_TEX2DPROJ || e == E_TEX2DPROJB || e == E_TEX2DPROJLOD;
    int lod = e == E_TEX2DLOD || e == E_TEX2DPROJLOD || e == E_TEXCUBELOD;
    int bias = e == E_TEX2DB || e == E_TEX2DPROJB || e == E_TEXCUBEB;
    val r = novo(c, t_vec(T_FLOAT, 4));
    if (a[0].arq != RI_AMOST) { falha(c, "el primer argumento tiene que ser un sampler", NULL); return r; }
    if (bias && c->tipo_shader == 0) { falha(c, "el sesgo de textura solo existe en fragmentos", NULL); return r; }
    if (lod && c->tipo_shader == 1) { falha(c, "texture*Lod solo existe en vértices", NULL); return r; }
    int nc = cubo ? 3 : 2, ncoord = a[1].t.lin;
    if (a[1].t.base != T_FLOAT || (!proj && ncoord != nc) || (proj && ncoord != 3 && ncoord != 4)) { falha(c, "coordenadas de textura de tamaño no válido", NULL); return r; }
    val coord = a[1];
    if (proj || bias || lod) {
        val t = tmpf(c, 4);
        if (proj) {
            val q = comp_de(a[1], ncoord - 1);
            val iq = escalar_n(c, RI_RCP, q, q, 1);
            ins2(c, RI_MUL, D(RI_TEMP, t.reg, 3), F(a[1]), F(iq));
        } else ins1(c, RI_MOV, D(RI_TEMP, t.reg, masc_n(nc)), F(a[1]));
        if (bias || lod) ins1(c, RI_MOV, D(RI_TEMP, t.reg, 8), F(comp_de(a[2], 0)));
        coord = t;
    }
    ri_ins_t *I = emitir(c, bias ? RI_TXB : lod ? RI_TXL : RI_TEX);
    I->d = D(RI_TEMP, r.reg, 15); I->f[0] = F(coord); I->f[1] = F(a[0]); I->tex = (uint8_t)(cubo ? RI_TEX_CUBO : RI_TEX_2D);
    int s = a[0].reg;
    if (s >= 0 && s < RI_MAX_AMOST) c->ri->amost_tipo[s] = (uint8_t)(cubo ? RI_TEX_CUBO : RI_TEX_2D);
    return r;
}

/* ---------------------------------------------------------------- expresiones */
static val chamar_usuario(comp *c, no *n);

static val cond_bool(comp *c, no *e, const char *onde)
{
    val v = gexpr(c, e);
    if (!c->falhou && (v.t.base != T_BOOL || !eh_escalar(&v.t))) falha(c, onde, NULL);
    return v;
}

static val gexpr(comp *c, no *n)
{
    if (!c->falhou && n && !n->ok) { if (checar(c, n)) n->ok = 1; }
    if (c->falhou || !n) return novo(c, t_esc(T_FLOAT));
    if (n->linha) c->linha = n->linha;
    /* constantes que se conocen al compilar */
    if ((n->k == N_BIN || n->k == N_UN || n->k == N_CONSTRUTOR) && !eh_matriz(&n->t) && !n->t.arr && n->t.base != T_STRUCT && n->t.lin <= 4) {
        double v[16];
        if (puro(n) && avaliar_const(c, n, v)) return imed_tipo(c, v, &n->t);
    }
    switch (n->k) {
    case N_NUM: return imed_tipo(c, n->v, &n->t);
    case N_VAR: return val_sim(c, n->s);
    case N_SEQ: gexpr(c, n->a); return gexpr(c, n->b);
    case N_CONSTRUTOR: return construtor(c, n);
    case N_CHAMA: return n->f ? chamar_usuario(c, n) : embutida(c, n);
    case N_CAMPO: case N_IDX: { lval l = glval(c, n); return carregar(c, &l); }
    case N_UN: {
        val a = gexpr(c, n->a);
        if (n->op == '-') { val r = novo(c, a.t); for (int k = 0; k < nregs(&a.t); k++) ins1(c, RI_MOV, D(RI_TEMP, r.reg + k, masc_n(a.t.lin)), Fneg(nregs(&a.t) > 1 ? coluna(a, k) : a)); return r; }
        if (n->op == '!') { val r = op2(c, RI_SEQ, a, k1(c, 0), 1); r.t = a.t; return r; }
        return a;
    }
    case N_PRE: case N_POS: {
        lval l = glval(c, n->a);
        val v = carregar(c, &l);
        val antes = v;
        if (n->k == N_POS) { antes = novo(c, v.t); mov_regs(c, RI_TEMP, antes.reg, 0, v, &v.t); }
        val um = imed(c, (float[]){ 1.0f }, 1, v.t.base);
        val nv = aritmetica(c, n->op == K_INC ? '+' : '-', v, um, v.t);
        atribuir(c, &l, nv);
        return n->k == N_PRE ? nv : antes;
    }
    case N_ATR: {
        lval l = glval(c, n->a);
        val b = gexpr(c, n->b);
        if (n->op != '=') {
            val a = carregar(c, &l);
            int op = n->op == K_ADDA ? '+' : n->op == K_SUBA ? '-' : n->op == K_MULA ? '*' : '/';
            if (op == '*' && eh_vetor(&a.t) && eh_matriz(&b.t)) b = vec_mat(c, a, b), b.t = a.t;
            else if (op == '*' && eh_matriz(&a.t) && eh_vetor(&b.t)) { falha(c, "mat *= vec no es válido", NULL); return b; }
            else b = aritmetica(c, op, a, b, a.t);
        }
        if (!tipo_igual(&b.t, &l.t)) { falha(c, "asignación con tipos distintos", NULL); return b; }
        atribuir(c, &l, b);
        return b;
    }
    case N_COND: {
        val cnd = cond_bool(c, n->a, "la condición de «?:» tiene que ser un bool");
        if (puro(n->b) && puro(n->c) && n->t.base != T_SAMPLER2D && n->t.base != T_SAMPLERCUBE) {
            val x = gexpr(c, n->b), y = gexpr(c, n->c);
            val r = novo(c, n->t);
            int k = nregs(&n->t);
            uint8_t mm[64]; if (k > 64) { falha(c, "tipo demasiado grande en «?:»", NULL); return r; }
            mascaras(&n->t, mm);
            for (int i = 0; i < k; i++)
                ins3(c, RI_CMP, D(RI_TEMP, r.reg + i, mm[i]), Fneg(cnd), F(k > 1 ? V(x.arq, x.reg + i, RI_SW_XYZW, x.t) : x), F(k > 1 ? V(y.arq, y.reg + i, RI_SW_XYZW, y.t) : y));
            return r;
        }
        val r = novo(c, n->t);
        ins1(c, RI_IF, D(RI_NULO, 0, 0), F(cnd));
        val x = gexpr(c, n->b); mov_regs(c, RI_TEMP, r.reg, 0, x, &n->t);
        ins1(c, RI_ELSE, D(RI_NULO, 0, 0), F(cnd));
        val y = gexpr(c, n->c); mov_regs(c, RI_TEMP, r.reg, 0, y, &n->t);
        emitir(c, RI_ENDIF);
        return r;
    }
    case N_BIN: {
        int op = n->op;
        if (op == K_AND || op == K_OR) {
            val a = gexpr(c, n->a);
            if (puro(n->b)) { val b = gexpr(c, n->b); val r = op2(c, op == K_AND ? RI_MUL : RI_MAX, a, b, 1); r.t = t_esc(T_BOOL); return r; }
            val r = novo(c, t_esc(T_BOOL));
            ins1(c, RI_MOV, D(RI_TEMP, r.reg, 1), F(a));
            if (op == K_AND) ins1(c, RI_IF, D(RI_NULO, 0, 0), F(a));
            else { val na2 = op2(c, RI_SEQ, a, k1(c, 0), 1); ins1(c, RI_IF, D(RI_NULO, 0, 0), F(na2)); }
            val b = gexpr(c, n->b);
            ins1(c, RI_MOV, D(RI_TEMP, r.reg, 1), F(b));
            emitir(c, RI_ENDIF);
            return r;
        }
        val a = gexpr(c, n->a), b = gexpr(c, n->b);
        val r;
        switch (op) {
        case K_XOR: r = op2(c, RI_SNE, a, b, 1); break;
        case '<': r = op2(c, RI_SLT, a, b, 1); break;
        case '>': r = op2(c, RI_SLT, b, a, 1); break;
        case K_LE: r = op2(c, RI_SGE, b, a, 1); break;
        case K_GE: r = op2(c, RI_SGE, a, b, 1); break;
        case K_EQ: case K_NE: {
            int k = nregs(&a.t);
            if (k == 1 && a.t.lin == 1) { r = op2(c, op == K_EQ ? RI_SEQ : RI_SNE, a, b, 1); break; }
            uint8_t mm[64]; if (k > 64) { falha(c, "tipo demasiado grande en ==", NULL); return a; }
            mascaras(&a.t, mm);
            val acc = k1(c, 0);
            for (int i = 0; i < k; i++) {
                int nc = mm[i] == 1 ? 1 : mm[i] == 3 ? 2 : mm[i] == 7 ? 3 : 4;
                val ai = k > 1 ? V(a.arq, a.reg + i, RI_SW_XYZW, a.t) : a, bi = k > 1 ? V(b.arq, b.reg + i, RI_SW_XYZW, b.t) : b;
                val dif = op2(c, RI_SNE, ai, bi, nc); dif.t = nc == 1 ? t_esc(T_FLOAT) : t_vec(T_FLOAT, nc);
                val s = dot_v(c, dif, k1(c, 1), nc);
                acc = op2(c, RI_ADD, acc, s, 1);
            }
            r = op2(c, op == K_EQ ? RI_SEQ : RI_SNE, acc, k1(c, 0), 1);
            break;
        }
        default: return aritmetica(c, op, a, b, n->t);
        }
        r.t = t_esc(T_BOOL);
        return r;
    }
    }
    return novo(c, t_esc(T_FLOAT));
}

/* ---------------------------------------------------------------- sentencias */
static void gcmd(comp *c, cmd *s);

static void decl_sitio(comp *c, sim *s)
{
    if (s->classe == 4 && s->tem_cval && !s->t.arr && s->t.col <= 1 && s->t.base != T_STRUCT) {
        val v = imed_tipo(c, s->cval, &s->t);
        s->arq = v.arq; s->reg = v.reg; s->sw = v.sw;
        return;
    }
    val v = novo(c, s->t);
    s->arq = RI_TEMP; s->reg = v.reg; s->sw = RI_SW_XYZW;
}
static void gdecl(comp *c, cmd *d)
{
    sim *s = d->decl;
    if (!s) return;
    if (s->t.base == T_SAMPLER2D || s->t.base == T_SAMPLERCUBE) { falha(c, "los samplers solo pueden ser uniformes o parámetros:", s->nome); return; }
    decl_sitio(c, s);
    if (s->classe == 4 && !d->init) { falha(c, "una constante necesita valor:", s->nome); return; }
    if (d->init && s->arq == RI_TEMP) {
        val v = gexpr(c, d->init);
        if (!c->falhou && !tipo_igual(&v.t, &s->t)) { falha(c, "inicialización con un tipo distinto:", s->nome); return; }
        atribuir_sim(c, s, v);
    }
}
static void apos_laco(comp *c)
{
    /* un return dentro del bucle (con la bandera puesta) sigue saliendo hacia fuera */
    if (c->fn && c->fn->ret_aninhado) {
        val b = V(RI_TEMP, c->fn->bandeira, SW_REP(0), t_esc(T_FLOAT));
        ins1(c, RI_IF, D(RI_NULO, 0, 0), F(b));
        emitir(c, RI_BRK);
        emitir(c, RI_ENDIF);
    }
}
static void sair_se_falso(comp *c, no *e, const char *onde)
{
    val cnd = cond_bool(c, e, onde);
    val nao = op2(c, RI_SEQ, cnd, k1(c, 0), 1);
    ins1(c, RI_IF, D(RI_NULO, 0, 0), F(nao));
    emitir(c, RI_BRK);
    emitir(c, RI_ENDIF);
}
static void gcmd(comp *c, cmd *s)
{
    for (; s && !c->falhou; s = s->prox) {
        if (s->e && s->e->linha) c->linha = s->e->linha;
        switch (s->k) {
        case 0: gexpr(c, s->e); break;
        case 1: gdecl(c, s); break;
        case 2: gcmd(c, s->filhos); break;
        case 3: {
            val cnd = cond_bool(c, s->e, "la condición del if tiene que ser un bool");
            ins1(c, RI_IF, D(RI_NULO, 0, 0), F(cnd));
            gcmd(c, s->a);
            if (s->b) { ins1(c, RI_ELSE, D(RI_NULO, 0, 0), F(cnd)); gcmd(c, s->b); }
            emitir(c, RI_ENDIF);
            break;
        }
        case 4: case 5: case 6: {
            if (s->k == 4 && s->a) gcmd(c, s->a);   /* inicialización del for */
            int com_cont = cmd_continue(s->b) && ((s->k == 4 && s->e3) || s->k == 6);
            if (c->fn) c->fn->lacos++;
            c->prof_laco++;
            if (com_cont) {
                /* el incremento (o la condición del do-while) va arriba, saltándose la primera vuelta */
                val pri = novo(c, t_esc(T_FLOAT));
                ins1(c, RI_MOV, D(RI_TEMP, pri.reg, 1), F(k1(c, 1)));
                emitir(c, RI_BGNLOOP);
                val nao = op2(c, RI_SEQ, V(RI_TEMP, pri.reg, SW_REP(0), t_esc(T_FLOAT)), k1(c, 0), 1);
                ins1(c, RI_IF, D(RI_NULO, 0, 0), F(nao));
                if (s->k == 4) gexpr(c, s->e3); else sair_se_falso(c, s->e, "la condición del bucle tiene que ser un bool");
                emitir(c, RI_ENDIF);
                ins1(c, RI_MOV, D(RI_TEMP, pri.reg, 1), F(k1(c, 0)));
                if (s->k == 4 && s->e) sair_se_falso(c, s->e, "la condición del bucle tiene que ser un bool");
                gcmd(c, s->b);
            } else {
                emitir(c, RI_BGNLOOP);
                if (s->k != 6 && s->e) sair_se_falso(c, s->e, "la condición del bucle tiene que ser un bool");
                gcmd(c, s->b);
                if (s->k == 4 && s->e3) gexpr(c, s->e3);
                if (s->k == 6 && s->e) sair_se_falso(c, s->e, "la condición del bucle tiene que ser un bool");
            }
            emitir(c, RI_ENDLOOP);
            c->prof_laco--;
            if (c->fn) c->fn->lacos--;
            apos_laco(c);
            break;
        }
        case 7: {
            fctx *f = c->fn;
            if (!f) { falha(c, "return fuera de una función", NULL); break; }
            if (s->e) {
                if (f->f->ret.base == T_VOID) { falha(c, "return con valor en una función void", NULL); break; }
                val v = gexpr(c, s->e);
                if (!tipo_igual(&v.t, &f->f->ret)) { falha(c, "return con un tipo distinto del de la función", NULL); break; }
                lval l = lval_de_val(f->ret); atribuir(c, &l, v);
            } else if (f->f->ret.base != T_VOID) { falha(c, "return sin valor en una función que devuelve algo", NULL); break; }
            if (s == f->ret_final) break;
            if (f->lacos) { ins1(c, RI_MOV, D(RI_TEMP, f->bandeira, 1), F(k1(c, 1))); }
            emitir(c, RI_BRK);
            break;
        }
        case 8: if (!c->prof_laco || (c->fn && !c->fn->lacos)) { falha(c, "break fuera de un bucle", NULL); break; } emitir(c, RI_BRK); break;
        case 9: if (!c->prof_laco || (c->fn && !c->fn->lacos)) { falha(c, "continue fuera de un bucle", NULL); break; } emitir(c, RI_CONT); break;
        case 10:
            if (c->tipo_shader != 1) { falha(c, "discard solo existe en los sombreadores de fragmentos", NULL); break; }
            emitir(c, RI_KILL); c->ri->usa_kill = 1; break;
        }
    }
}

/* ---------------------------------------------------------------- funciones del usuario (expandidas) */
static val chamar_usuario(comp *c, no *n)
{
    fn *f = n->f;
    val r = novo(c, f->ret.base != T_VOID ? f->ret : t_esc(T_FLOAT));
    r.t = f->ret;
    if (!f->corpo) { falha(c, "función sin definir:", f->nome); return r; }
    if (++c->prof_inline > 32) { falha(c, "llamadas anidadas demasiado hondas (GLSL ES no tiene recursión):", f->nome); c->prof_inline--; return r; }
    for (fctx *g = c->fn; g; g = g->pai) if (g->f == f) { falha(c, "GLSL ES no tiene recursión:", f->nome); c->prof_inline--; return r; }
    /* argumentos: los «in» que el cuerpo no toca no se copian (el parámetro es el mismo valor) */
    lval saidas[16]; val args[16]; int qual[16];
    sim antes[16];
    for (int i = 0; i < f->np; i++) {
        qual[i] = f->qual[i];
        antes[i] = *f->params[i];
        if (qual[i]) {
            if (!raiz_sim(n->args[i])) { falha(c, "un argumento out o inout tiene que ser una variable", NULL); c->prof_inline--; return r; }
            if (!checar(c, n->args[i]) || !checar_escrita(c, n->args[i])) { c->prof_inline--; return r; }
            saidas[i] = glval(c, n->args[i]);
        }
        if (qual[i] != 1) args[i] = qual[i] == 2 ? carregar(c, &saidas[i]) : gexpr(c, n->args[i]);
    }
    for (int i = 0; i < f->np && !c->falhou; i++) {
        sim *p = f->params[i];
        int sampler = p->t.base == T_SAMPLER2D || p->t.base == T_SAMPLERCUBE;
        sim *rs = raiz_sim(n->args[i]);
        if (qual[i] == 0 && (sampler || (!cmd_escreve(f->corpo, p) && !(rs && cmd_escreve(f->corpo, rs))))) { p->arq = args[i].arq; p->reg = args[i].reg; p->sw = args[i].sw; continue; }
        if (sampler) { falha(c, "un sampler no puede ser out ni inout", NULL); break; }
        val pv = novo(c, p->t);
        p->arq = RI_TEMP; p->reg = pv.reg; p->sw = RI_SW_XYZW;
        if (qual[i] != 1) { lval l = lval_de_val(pv); atribuir(c, &l, args[i]); }
    }
    /* el cuerpo */
    fctx fx; memset(&fx, 0, sizeof fx);
    fx.f = f; fx.ret = r; fx.pai = c->fn;
    cmd *ult = NULL; for (cmd *m = f->corpo->filhos; m; m = m->prox) ult = m;
    fx.ret_final = ult && ult->k == 7 ? ult : NULL;
    int nret = 0, nret_laco = 0;
    cmd_returns(f->corpo->filhos, fx.ret_final, 0, &nret, &nret_laco);
    fx.envolve = nret > 0; fx.ret_aninhado = nret_laco > 0;
    if (fx.ret_aninhado) { fx.bandeira = novo(c, t_esc(T_FLOAT)).reg; ins1(c, RI_MOV, D(RI_TEMP, fx.bandeira, 1), F(k1(c, 0))); }
    fctx *antes_fn = c->fn;
    c->fn = &fx;
    if (fx.envolve) { emitir(c, RI_BGNLOOP); c->prof_laco++; }
    gcmd(c, f->corpo->filhos);
    if (fx.envolve) { emitir(c, RI_BRK); emitir(c, RI_ENDLOOP); c->prof_laco--; }
    c->fn = antes_fn;
    /* out / inout de vuelta */
    for (int i = 0; i < f->np && !c->falhou; i++) if (qual[i]) atribuir(c, &saidas[i], val_sim(c, f->params[i]));
    for (int i = 0; i < f->np; i++) { f->params[i]->arq = antes[i].arq; f->params[i]->reg = antes[i].reg; f->params[i]->sw = antes[i].sw; }
    c->prof_inline--;
    return r;
}

/* ---------------------------------------------------------------- el programa */
static void insertar_inicio(comp *c, ri_ins_t *v, int n)
{
    ri_prog_t *p = c->ri;
    for (int i = 0; i < n; i++) emitir(c, RI_NOP);
    if (c->falhou) return;
    memmove(p->ins + n, p->ins, (size_t)(p->nins - n) * sizeof(ri_ins_t));
    memcpy(p->ins, v, (size_t)n * sizeof(ri_ins_t));
}
static void gerar(comp *c)
{
    ri_prog_t *p = c->ri;
    c->reg_cor = -1;
    int vs = c->tipo_shader == 0;
    /* las globales, en orden de declaración */
    for (cmd *g = c->globais; g && !c->falhou; g = g->prox) {
        sim *s = g->decl;
        if (!s) continue;
        int samp = s->t.base == T_SAMPLER2D || s->t.base == T_SAMPLERCUBE;
        s->sw = RI_SW_XYZW;
        if (s->classe == 1) {
            if (g->init) { falha(c, "un uniforme no puede tener valor inicial:", s->nome); return; }
            if (samp) {
                s->arq = RI_AMOST; s->reg = p->namost; p->namost += s->t.arr ? s->t.arr : 1;
                if (p->namost > RI_MAX_AMOST) { falha(c, "demasiados samplers", NULL); return; }
                for (int k = s->reg; k < p->namost; k++) p->amost_tipo[k] = (uint8_t)(s->t.base == T_SAMPLERCUBE ? RI_TEX_CUBO : RI_TEX_2D);
                var_registrar(c, &p->unif, &p->nunif, s->nome, &s->t, s->reg);
            } else {
                s->arq = RI_UNI; s->reg = p->nuni; p->nuni += nregs(&s->t);
                unif_registrar(c, s->nome, &s->t, s->reg);
            }
        } else if (samp) { falha(c, "los samplers solo pueden ser uniformes:", s->nome); return; }
        else if (s->classe == 2) {
            if (!vs) { falha(c, "attribute solo existe en los sombreadores de vértices:", s->nome); return; }
            if (s->t.base != T_FLOAT || s->t.arr || s->t.base == T_STRUCT) { falha(c, "un attribute tiene que ser float, vecN o matN:", s->nome); return; }
            if (g->init) { falha(c, "un attribute no puede tener valor inicial:", s->nome); return; }
            s->arq = RI_ENT; s->reg = p->nent;
            for (int k = 0; k < nregs(&s->t); k++) ent_nova(c, RI_SEM_ATRIB, p->nent);
            var_registrar(c, &p->attr, &p->nattr, s->nome, &s->t, s->reg);
        } else if (s->classe == 3) {
            if (s->t.base != T_FLOAT || s->t.base == T_STRUCT) { falha(c, "un varying tiene que ser float, vecN, matN o un array de ellos:", s->nome); return; }
            if (g->init) { falha(c, "un varying no puede tener valor inicial:", s->nome); return; }
            int k = nregs(&s->t);
            if (vs) {
                val t = novo(c, s->t);
                s->arq = RI_TEMP; s->reg = t.reg;
                int r0 = p->nsai;
                for (int i = 0; i < k; i++) sai_nova(c, RI_SEM_GENERICO, c->nvary_ord++, t.reg + i);
                var_registrar(c, &p->vary, &p->nvary, s->nome, &s->t, r0);
            } else {
                s->arq = RI_ENT; s->reg = p->nent;
                for (int i = 0; i < k; i++) ent_nova(c, RI_SEM_GENERICO, c->nvary_ord++);
                var_registrar(c, &p->vary, &p->nvary, s->nome, &s->t, s->reg);
            }
        } else {
            decl_sitio(c, s);
            if (s->classe == 4 && !g->init) { falha(c, "una constante necesita valor:", s->nome); return; }
        }
    }
    if (c->falhou) return;
    /* los arrays de temporales que se pueden indexar quedan fuera de la compactación: todas las globales y
     * locales que son arrays (se marca el tramo de temporales usado hasta aquí y lo que venga se revisa
     * en ri_otimizar por los accesos con rel) */
    fn *m = NULL;
    for (fn *f = c->fns; f; f = f->prox) if (!compara_texto(f->nome, "main") && f->definida) m = f;
    if (!m) { falha(c, "falta la función main()", NULL); return; }
    if (m->np || m->ret.base != T_VOID) { falha(c, "main tiene que ser «void main()»", NULL); return; }
    /* inicializadores de las globales, después el main */
    for (cmd *g = c->globais; g && !c->falhou; g = g->prox) {
        sim *s = g->decl;
        if (!s || !g->init || s->arq != RI_TEMP || s->classe == 3) continue;
        if (g->init->linha) c->linha = g->init->linha;
        val v = gexpr(c, g->init);
        if (!c->falhou && !tipo_igual(&v.t, &s->t)) { falha(c, "inicialización con un tipo distinto:", s->nome); return; }
        atribuir_sim(c, s, v);
    }
    no chamada; memset(&chamada, 0, sizeof chamada);
    chamada.k = N_CHAMA; chamada.f = m; chamada.t = m->ret; chamada.ok = 1;
    c->prof_inline = 0;
    chamar_usuario(c, &chamada);
    if (c->falhou) return;
    if (vs) {
        int tem_pos = 0;
        for (int i = 0; i < p->nsai; i++) if (p->sai[i].sem == RI_SEM_POSICAO) tem_pos = 1;
        if (!tem_pos) { for (sim *s = c->escopo[0]; s; s = s->prox) if (!compara_texto(s->nome, "gl_Position") && s->reg < 0) embutida_sitio(c, s); }
    }
    /* las salidas: el temporal de cada una a su registro SAI */
    for (int i = 0; i < c->nsombra; i++) ins1(c, RI_MOV, D(RI_SAI, c->sombra_sai[i], 15), F(V(RI_TEMP, c->sombra_tmp[i], RI_SW_XYZW, t_vec(T_FLOAT, 4))));
    emitir(c, RI_FIM);
    /* y al principio, las salidas a cero (lo que el sombreador no escriba sale definido) */
    if (c->nsombra) {
        ri_ins_t ini[RI_MAX_SAI];
        val z = k1(c, 0);
        for (int i = 0; i < c->nsombra; i++) {
            memset(&ini[i], 0, sizeof ini[i]);
            ini[i].op = RI_MOV; ini[i].d = D(RI_TEMP, c->sombra_tmp[i], 15); ini[i].f[0] = F(z);
            if (p->sai[c->sombra_sai[i]].sem == RI_SEM_PONTO_TAM) ini[i].f[0] = F(k1(c, 1));
        }
        insertar_inicio(c, ini, c->nsombra);
    }
}

/* ================================================================ API */
void ri_liberar(ri_prog_t *p)
{
    if (!p) return;
    if (p->ins) libera(p->ins);
    if (p->imed) libera(p->imed);
    if (p->unif) libera(p->unif);
    if (p->attr) libera(p->attr);
    if (p->vary) libera(p->vary);
    libera(p);
}

ri_prog_t *sgl_glsl_compilar(const char *fonte, int n, int tipo_shader, char *erro, int max_erro)
{
    comp *c = aloca_zerado(sizeof(comp));
    if (!c) return NULL;
    c->fonte = fonte ? fonte : ""; c->nfonte = fonte ? n : 0; c->tipo_shader = tipo_shader; c->linha = 1;
    ri_prog_t *p = aloca_zerado(sizeof(ri_prog_t));
    if (!p) { libera(c); return NULL; }
    c->ri = p; p->tipo = tipo_shader;
    if (!preprocessar(c)) goto fim;
    c->p = c->pp; c->linha = 1;
    c->escopo[c->nescopo++] = NULL;
    embutidas_globais(c);
    lex(c);
    topo(c);
    if (!c->falhou) gerar(c);
    if (!c->falhou && !ri_ligar(p)) falha(c, "control de flujo mal anidado (error interno)", NULL);
    if (!c->falhou) {
        ri_otimizar(p);
        char e2[200];
        if (!ri_validar(p, e2, sizeof e2)) { c->linha = 0; falha(c, "error interno:", e2); }
    }
fim:
    if (c->falhou) {
        if (erro && max_erro > 0) copia_texto(erro, c->erro, (size_t)max_erro);
        ri_liberar(p); p = NULL;
    } else if (erro && max_erro > 0) erro[0] = 0;
    if (c->imed_uso) libera(c->imed_uso);
    alivre(&c->ar);
    libera(c);
    return p;
}
