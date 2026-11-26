// SPDX-License-Identifier: GPL-3.0-only
/* sgl_ri.c - utilidades de la representación intermedia de SaguiGL 2.0 (D475): saltos del control de
 * flujo, validación, volcado en texto y las optimizaciones comunes a todos los backends (copias, código
 * muerto y compactación de temporales). */
#include "sgl_ri.h"
#include "sgl2_base.h"

const char *const ri_nomes_ops[RI_N_OPS] = {
    "NOP", "MOV", "ADD", "MUL", "MAD", "MIN", "MAX", "FLR", "FRC", "TRUNC", "SLT", "SGE", "SEQ", "SNE", "CMP",
    "DP2", "DP3", "DP4", "RCP", "RSQ", "EX2", "LG2", "SIN", "COS", "POW", "DDX", "DDY", "ARL", "TEX", "TXB", "TXL",
    "IF", "ELSE", "ENDIF", "BGNLOOP", "ENDLOOP", "BRK", "CONT", "KILL", "FIM"
};

/* número de fuentes de cada operación */
static int ri_nfontes(int op)
{
    switch (op) {
    case RI_MOV: case RI_FLR: case RI_FRC: case RI_TRUNC: case RI_RCP: case RI_RSQ: case RI_EX2: case RI_LG2:
    case RI_SIN: case RI_COS: case RI_DDX: case RI_DDY: case RI_ARL: case RI_IF: return 1;
    case RI_ADD: case RI_MUL: case RI_MIN: case RI_MAX: case RI_SLT: case RI_SGE: case RI_SEQ: case RI_SNE:
    case RI_DP2: case RI_DP3: case RI_DP4: case RI_POW: case RI_TEX: case RI_TXB: case RI_TXL: return 2;
    case RI_MAD: case RI_CMP: return 3;
    default: return 0;
    }
}
static int ri_tem_dest(int op) { return op >= RI_MOV && op <= RI_TXL; }
static int ri_eh_fluxo(int op) { return op >= RI_IF && op <= RI_FIM; }

int ri_ligar(ri_prog_t *p)
{
    int pilha[64], np = 0;
    for (int i = 0; i < p->nins; i++) {
        ri_ins_t *I = &p->ins[i];
        switch (I->op) {
        case RI_IF: case RI_BGNLOOP: if (np >= 64) return 0; pilha[np++] = i; I->alvo = -1; break;
        case RI_ELSE:
            if (!np || p->ins[pilha[np - 1]].op != RI_IF) return 0;
            p->ins[pilha[np - 1]].alvo = i; pilha[np - 1] = i; break;
        case RI_ENDIF:
            if (!np || (p->ins[pilha[np - 1]].op != RI_IF && p->ins[pilha[np - 1]].op != RI_ELSE)) return 0;
            p->ins[pilha[--np]].alvo = i; I->alvo = -1; break;
        case RI_ENDLOOP: {
            if (!np || p->ins[pilha[np - 1]].op != RI_BGNLOOP) return 0;
            int b = pilha[--np];
            p->ins[b].alvo = i; I->alvo = b;
            /* los BRK y CONT de este bucle (los de bucles de dentro ya tienen alvo) */
            int prof = 0;
            for (int k = b + 1; k < i; k++) {
                int op = p->ins[k].op;
                if (op == RI_BGNLOOP) prof++;
                else if (op == RI_ENDLOOP) prof--;
                else if ((op == RI_BRK || op == RI_CONT) && prof == 0) p->ins[k].alvo = i;
            }
            break;
        }
        case RI_BRK: case RI_CONT: {
            int dentro = 0;
            for (int k = 0; k < np; k++) if (p->ins[pilha[k]].op == RI_BGNLOOP) dentro = 1;
            if (!dentro) return 0;
            I->alvo = -1;
            break;
        }
        default: I->alvo = -1; break;
        }
    }
    return np == 0;
}

static const char *ri_arq_nome(int a)
{
    static const char *const n[] = { "NULO", "TEMP", "ENT", "SAI", "UNI", "IMED", "END", "AMOST" };
    return a >= 0 && a <= RI_AMOST ? n[a] : "?";
}
static int ri_lim(const ri_prog_t *p, int arq)
{
    switch (arq) {
    case RI_TEMP: return p->ntemp; case RI_ENT: return p->nent; case RI_SAI: return p->nsai; case RI_UNI: return p->nuni;
    case RI_IMED: return p->nimed; case RI_END: return 1; case RI_AMOST: return p->namost;
    }
    return 0;
}

int ri_validar(const ri_prog_t *p, char *erro, int max)
{
    if (!p->nins || p->ins[p->nins - 1].op != RI_FIM) { formatar_texto(erro, (size_t)max, "falta FIM"); return 0; }
    int prof = 0;
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        if (I->op >= RI_N_OPS) { formatar_texto(erro, (size_t)max, "%d: operación %d", i, I->op); return 0; }
        if (I->op == RI_IF || I->op == RI_BGNLOOP) prof++;
        if (I->op == RI_ENDIF || I->op == RI_ENDLOOP) prof--;
        if (prof < 0) { formatar_texto(erro, (size_t)max, "%d: anidamiento", i); return 0; }
        if (ri_tem_dest(I->op)) {
            if (!I->d.masc || I->d.masc > 15) { formatar_texto(erro, (size_t)max, "%d: máscara vacía", i); return 0; }
            int a = I->d.arq;
            if (I->op == RI_ARL ? a != RI_END : (a != RI_TEMP && a != RI_SAI)) { formatar_texto(erro, (size_t)max, "%d: destino %s", i, ri_arq_nome(a)); return 0; }
            if (!I->d.rel && I->d.ind >= ri_lim(p, a)) { formatar_texto(erro, (size_t)max, "%d: destino %s[%d] fuera", i, ri_arq_nome(a), I->d.ind); return 0; }
        }
        for (int k = 0; k < ri_nfontes(I->op); k++) {
            const ri_fonte_t *f = &I->f[k];
            int ok = f->arq >= RI_TEMP && f->arq <= RI_AMOST && f->arq != RI_SAI && f->arq != RI_END;
            if ((I->op == RI_TEX || I->op == RI_TXB || I->op == RI_TXL) && k == 1) ok = f->arq == RI_AMOST;
            else if (f->arq == RI_AMOST) ok = 0;
            if (!ok) { formatar_texto(erro, (size_t)max, "%d: fuente %d en %s", i, k, ri_arq_nome(f->arq)); return 0; }
            if (!f->rel && f->ind >= ri_lim(p, f->arq)) { formatar_texto(erro, (size_t)max, "%d: fuente %s[%d] fuera", i, ri_arq_nome(f->arq), f->ind); return 0; }
            if (f->rel && f->arq == RI_IMED) { formatar_texto(erro, (size_t)max, "%d: inmediato indexado", i); return 0; }
        }
    }
    if (prof) { formatar_texto(erro, (size_t)max, "anidamiento sin cerrar"); return 0; }
    return 1;
}

/* ---------------------------------------------------------------- volcado */
static int ri_por(char *s, int max, int n, const char *t) { while (*t) { if (n + 1 < max) s[n] = *t; n++; t++; } if (max) s[n < max ? n : max - 1] = 0; return n; }
static int ri_fonte_txt(char *s, int max, int n, const ri_fonte_t *f)
{
    char b[48]; const char *xyzw = "xyzw";
    if (f->arq == RI_AMOST) { formatar_texto(b, sizeof b, "AMOST[%d]", f->ind); return ri_por(s, max, n, b); }
    char sw[5]; for (int c = 0; c < 4; c++) sw[c] = xyzw[RI_SW_C(f->sw, c)]; sw[4] = 0;
    formatar_texto(b, sizeof b, "%s%s%s[%s%d].%s%s", f->mod & RI_NEG ? "-" : "", f->mod & RI_ABS ? "|" : "", ri_arq_nome(f->arq),
                   f->rel ? "a0.x+" : "", f->ind, sw, f->mod & RI_ABS ? "|" : "");
    return ri_por(s, max, n, b);
}
int ri_texto(const ri_prog_t *p, char *s, int max)
{
    int n = 0; char b[200];
    formatar_texto(b, sizeof b, "; %s: %d instrucciones, %d temporales, %d uniformes, %d inmediatos, %d entradas, %d salidas, %d samplers\n",
                   p->tipo ? "fragmentos" : "vertices", p->nins, p->ntemp, p->nuni, p->nimed, p->nent, p->nsai, p->namost);
    n = ri_por(s, max, n, b);
    for (int i = 0; i < p->nimed; i++) {
        formatar_texto(b, sizeof b, "IMED[%d] = { %d/1000, %d/1000, %d/1000, %d/1000 }\n", i, (int)(p->imed[i][0] * 1000), (int)(p->imed[i][1] * 1000), (int)(p->imed[i][2] * 1000), (int)(p->imed[i][3] * 1000));
        n = ri_por(s, max, n, b);
    }
    int ind = 0;
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        if (I->op == RI_ELSE || I->op == RI_ENDIF || I->op == RI_ENDLOOP) ind--;
        formatar_texto(b, sizeof b, "%4d: ", i); n = ri_por(s, max, n, b);
        for (int k = 0; k < ind; k++) n = ri_por(s, max, n, "  ");
        n = ri_por(s, max, n, ri_nomes_ops[I->op]);
        if (I->d.sat) n = ri_por(s, max, n, "_SAT");
        int nf = ri_nfontes(I->op);
        if (ri_tem_dest(I->op)) {
            char m[5]; int k = 0; for (int c = 0; c < 4; c++) if (I->d.masc & (1 << c)) m[k++] = "xyzw"[c]; m[k] = 0;
            formatar_texto(b, sizeof b, " %s[%s%d].%s", ri_arq_nome(I->d.arq), I->d.rel ? "a0.x+" : "", I->d.ind, m);
            n = ri_por(s, max, n, b);
        }
        for (int k = 0; k < nf; k++) { n = ri_por(s, max, n, (k || ri_tem_dest(I->op)) ? ", " : " "); n = ri_fonte_txt(s, max, n, &I->f[k]); }
        if (I->op == RI_TEX || I->op == RI_TXB || I->op == RI_TXL) n = ri_por(s, max, n, I->tex == RI_TEX_CUBO ? ", CUBO" : ", 2D");
        n = ri_por(s, max, n, "\n");
        if (I->op == RI_IF || I->op == RI_ELSE || I->op == RI_BGNLOOP) ind++;
    }
    return n;
}

/* ---------------------------------------------------------------- optimizaciones */
static int ri_le_reg(const ri_ins_t *I, int arq, int reg)
{
    for (int k = 0; k < ri_nfontes(I->op); k++) if (I->f[k].arq == arq && (I->f[k].ind == reg || I->f[k].rel)) return 1;
    return 0;
}

/* «P escribe t; MOV d, t» -> «P escribe d», si t solo lo escribe P y solo lo lee el MOV */
static int ri_copias(ri_prog_t *p, int *nesc, int *nlei)
{
    int mudou = 0;
    for (int m = 0; m < p->nins; m++) {
        ri_ins_t *M = &p->ins[m];
        if (M->op != RI_MOV || M->d.rel || M->f[0].arq != RI_TEMP || M->f[0].rel || M->f[0].mod) continue;
        if (M->d.arq != RI_TEMP && M->d.arq != RI_SAI) continue;
        int t = M->f[0].ind;
        if (nesc[t] != 1 || nlei[t] != 1) continue;
        if (M->d.arq == RI_TEMP && M->d.ind == t) continue;
        int ok = 1;
        for (int c = 0; c < 4; c++) if ((M->d.masc & (1 << c)) && RI_SW_C(M->f[0].sw, c) != c) ok = 0;
        if (!ok) continue;
        int pp = -1;
        for (int k = m - 1; k >= 0; k--) {
            ri_ins_t *I = &p->ins[k];
            if (ri_eh_fluxo(I->op)) break;
            if (ri_tem_dest(I->op) && I->d.arq == RI_TEMP && I->d.ind == t && !I->d.rel) { pp = k; break; }
            /* el destino no se puede leer ni escribir entre P y el MOV */
            if (ri_le_reg(I, M->d.arq, M->d.ind)) { ok = 0; break; }
            if (ri_tem_dest(I->op) && I->d.arq == M->d.arq && (I->d.ind == M->d.ind || I->d.rel)) { ok = 0; break; }
        }
        if (!ok || pp < 0) continue;
        ri_ins_t *P = &p->ins[pp];
        if ((P->d.masc & M->d.masc) != M->d.masc) continue;
        if (P->op == RI_ARL) continue;
        P->d.arq = M->d.arq; P->d.ind = M->d.ind; P->d.masc = M->d.masc; P->d.sat |= M->d.sat;
        M->op = RI_NOP;
        nesc[t] = 0; nlei[t] = 0;
        mudou = 1;
    }
    return mudou;
}

/* componentes del registro (arq, reg) que lee la instrucción (todas con índice dinámico) */
static int ri_comps_lidas(const ri_ins_t *I, int arq, int reg)
{
    int m = 0, op = I->op;
    for (int k = 0; k < ri_nfontes(op); k++) {
        const ri_fonte_t *f = &I->f[k];
        if (f->arq != arq || (f->ind != reg && !f->rel)) continue;
        if (f->rel) return 15;
        int usa;
        if (op >= RI_DP2 && op <= RI_DP4) usa = op == RI_DP2 ? 3 : op == RI_DP3 ? 7 : 15;
        else if ((op >= RI_RCP && op <= RI_POW) || op == RI_ARL || op == RI_IF) usa = 1;
        else if (op == RI_TEX || op == RI_TXB || op == RI_TXL) usa = 15;
        else usa = I->d.masc;
        for (int c = 0; c < 4; c++) if (usa & (1 << c)) m |= 1 << RI_SW_C(f->sw, c);
    }
    return m;
}
/* escrituras muertas: un temporal escrito fuera de todo IF y bucle que se vuelve a escribir entero, también fuera
 * de todo IF y bucle, antes de que nadie lea esas componentes (p. ej. las salidas puestas a cero al principio) */
static int ri_escritas_mortas(ri_prog_t *p)
{
    int mudou = 0, prof = 0;
    for (int i = 0; i < p->nins; i++) {
        ri_ins_t *I = &p->ins[i];
        if (I->op == RI_IF || I->op == RI_BGNLOOP) prof++;
        else if (I->op == RI_ENDIF || I->op == RI_ENDLOOP) prof--;
        if (prof || !ri_tem_dest(I->op) || I->op == RI_ARL || I->d.arq != RI_TEMP || I->d.rel) continue;
        int t = I->d.ind, m = I->d.masc, pr = 0, morta = 0;
        for (int k = i + 1; k < p->nins && !morta; k++) {
            ri_ins_t *J = &p->ins[k];
            if (J->op == RI_NOP) continue;
            if (J->op == RI_FIM) break;
            if (ri_comps_lidas(J, RI_TEMP, t) & m) break;
            if (J->op == RI_IF || J->op == RI_BGNLOOP) pr++;
            else if (J->op == RI_ENDIF || J->op == RI_ENDLOOP) pr--;
            else if (!pr && ri_tem_dest(J->op) && J->op != RI_ARL && J->d.arq == RI_TEMP && J->d.ind == t && !J->d.rel) { m &= ~J->d.masc; if (!m) morta = 1; }
        }
        if (morta) { I->op = RI_NOP; mudou = 1; }
    }
    return mudou;
}

static void ri_contar(const ri_prog_t *p, int *nesc, int *nlei)
{
    memset(nesc, 0, (size_t)p->ntemp * sizeof(int));
    memset(nlei, 0, (size_t)p->ntemp * sizeof(int));
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        if (I->op == RI_NOP) continue;
        if (ri_tem_dest(I->op) && I->d.arq == RI_TEMP && !I->d.rel && I->d.ind < p->ntemp) nesc[I->d.ind]++;
        for (int k = 0; k < ri_nfontes(I->op); k++) if (I->f[k].arq == RI_TEMP && !I->f[k].rel && I->f[k].ind < p->ntemp) nlei[I->f[k].ind]++;
    }
}
static int ri_tem_temp_rel(const ri_prog_t *p)
{
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        if (ri_tem_dest(I->op) && I->d.rel && I->d.arq == RI_TEMP) return 1;
        for (int k = 0; k < ri_nfontes(I->op); k++) if (I->f[k].rel && I->f[k].arq == RI_TEMP) return 1;
    }
    return 0;
}
static void ri_tirar_nops(ri_prog_t *p)
{
    int n = 0;
    for (int i = 0; i < p->nins; i++) if (p->ins[i].op != RI_NOP) p->ins[n++] = p->ins[i];
    p->nins = n;
    ri_ligar(p);
}

/* compactación: intervalos de vida en orden de programa; lo que se usa dentro de un bucle vive todo el
 * bucle salvo los temporales de una sola escritura que se escriben antes de leerse */
static void ri_compactar(ri_prog_t *p, int *nesc, int *nlei)
{
    (void)nlei;
    int nt = p->ntemp;
    if (!nt) return;
    int *ini = aloca((size_t)nt * sizeof(int) * 4);
    if (!ini) return;
    int *fim = ini + nt, *novo = fim + nt, *livre_ate = novo + nt;
    for (int t = 0; t < nt; t++) { ini[t] = -1; fim[t] = -1; novo[t] = -1; }
    int *primeira_esc = livre_ate;   /* (reutiliza el sitio) */
    for (int t = 0; t < nt; t++) primeira_esc[t] = -1;
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        for (int k = 0; k < ri_nfontes(I->op); k++) if (I->f[k].arq == RI_TEMP) { int t = I->f[k].ind; if (ini[t] < 0) ini[t] = i; fim[t] = i; }
        if (ri_tem_dest(I->op) && I->d.arq == RI_TEMP) { int t = I->d.ind; if (ini[t] < 0) ini[t] = i; fim[t] = i; if (primeira_esc[t] < 0) primeira_esc[t] = i; }
    }
    /* bucles (de dentro afuera da igual: se repite hasta que no cambie nada) */
    for (int vez = 0, mudou = 1; mudou && vez < 32; vez++) {
        mudou = 0;
        for (int b = 0; b < p->nins; b++) {
            if (p->ins[b].op != RI_BGNLOOP) continue;
            int e = p->ins[b].alvo;
            for (int t = 0; t < nt; t++) {
                if (ini[t] < 0 || fim[t] < b || ini[t] > e) continue;
                int seguro = nesc[t] == 1 && primeira_esc[t] == ini[t] && ini[t] > b && fim[t] < e;
                if (seguro) continue;
                if (ini[t] > b) { ini[t] = b; mudou = 1; }
                if (fim[t] < e) { fim[t] = e; mudou = 1; }
            }
        }
    }
    /* reparto: por orden de inicio; un registro queda libre tras el fin de su temporal */
    int *ocup_fim = aloca((size_t)nt * sizeof(int));
    if (!ocup_fim) { libera(ini); return; }
    int nreg = 0;
    for (int i = 0; i < p->nins; i++) {
        for (int t = 0; t < nt; t++) {
            if (ini[t] != i) continue;
            int r = -1;
            for (int k = 0; k < nreg; k++) if (ocup_fim[k] < i) { r = k; break; }
            if (r < 0) r = nreg++;
            ocup_fim[r] = fim[t];
            novo[t] = r;
        }
    }
    for (int i = 0; i < p->nins; i++) {
        ri_ins_t *I = &p->ins[i];
        for (int k = 0; k < ri_nfontes(I->op); k++) if (I->f[k].arq == RI_TEMP && novo[I->f[k].ind] >= 0) I->f[k].ind = (uint16_t)novo[I->f[k].ind];
        if (ri_tem_dest(I->op) && I->d.arq == RI_TEMP && novo[I->d.ind] >= 0) I->d.ind = (uint16_t)novo[I->d.ind];
    }
    p->ntemp = nreg;
    libera(ocup_fim);
    libera(ini);
}

void ri_otimizar(ri_prog_t *p)
{
    if (!p->ntemp) return;
    int *nesc = aloca((size_t)p->ntemp * sizeof(int) * 2);
    if (!nesc) return;
    int *nlei = nesc + p->ntemp;
    int rel = ri_tem_temp_rel(p);
    for (int vez = 0; vez < 8; vez++) {
        int mudou = 0;
        ri_contar(p, nesc, nlei);
        if (!rel) {
            if (ri_escritas_mortas(p)) mudou = 1;
            /* código muerto: escribe un temporal que nadie lee */
            for (int i = 0; i < p->nins; i++) {
                ri_ins_t *I = &p->ins[i];
                if (ri_tem_dest(I->op) && I->op != RI_ARL && I->d.arq == RI_TEMP && !I->d.rel && nlei[I->d.ind] == 0) { I->op = RI_NOP; mudou = 1; }
            }
            ri_contar(p, nesc, nlei);
            if (ri_copias(p, nesc, nlei)) mudou = 1;
        }
        if (!mudou) break;
    }
    ri_tirar_nops(p);
    if (!rel) { ri_contar(p, nesc, nlei); ri_compactar(p, nesc, nlei); }
    libera(nesc);
}
