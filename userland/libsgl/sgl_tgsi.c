// SPDX-License-Identifier: GPL-3.0-only
/* sgl_tgsi.c - RI -> TGSI en texto (SaguiGL 2.0, fase 2: backend VirGL, D480).
 *
 * La RI (sgl_ri.h) se diseñó para traducirse casi 1:1: cada instrucción es una línea de TGSI. Lo único que no es
 * directo:
 *   - los inmediatos y los reales se escriben con 9 cifras significativas (ida y vuelta exacta de un float);
 *   - ARL escribe ADDR[0].x con la componente de la máscara;
 *   - gl_FrontFacing: TGSI da +1 / -1 y la RI 1.0 / 0.0, así que el sombreador empieza convirtiéndolo en un
 *     temporal extra y las lecturas de esa entrada leen ese temporal;
 *   - si algún temporal se direcciona con ADDR (arrays locales con índice dinámico), todos los temporales se declaran
 *     como un solo array de TGSI (DCL TEMP[0..n], ARRAY(1)): sin eso la GPU no los puede direccionar;
 *   - «continue»: el TGSI tiene CONT, pero con la GPU del anfitrión de las pruebas (Mesa d3d12 de WSLg detrás de
 *     virglrenderer) un CONT no salta nada, así que los bucles con «continue» se traducen sin él: el cuerpo va dentro
 *     de un bucle interior que se ejecuta una vez (CONT = salir de él) y un BRK del bucle original pone una bandera
 *     y sale, y después del bucle interior se mira la bandera;
 *   - gl_PointCoord no se traduce (solo existe al dibujar puntos, que va por la CPU).
 * Sin libm, sin memoria dinámica y sin coma flotante más allá de leer los inmediatos: sirve igual en el anfitrión
 * (las pruebas lo vuelcan sin QEMU) y en el SaguiOS. */
#include "sgl_tgsi.h"
#include "sgl2_base.h"

typedef struct { char *s; int max, n; int ruim; } sai_t;

static void pc(sai_t *o, char c) { if (o->n + 1 < o->max) o->s[o->n] = c; o->n++; }
static void ps(sai_t *o, const char *t) { while (*t) pc(o, *t++); }
static void pd(sai_t *o, int v)
{
    char b[16]; int k = 0;
    if (v < 0) { pc(o, '-'); v = -v; }
    do { b[k++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (k) pc(o, b[--k]);
}
static void fim(sai_t *o) { if (o->max > 0) o->s[o->n < o->max ? o->n : o->max - 1] = 0; }

/* un float en decimal con 9 cifras significativas: «d.dddddddde+XX» (tgsi_text lo lee con atof) */
static void pf(sai_t *o, float v)
{
    double d = v;
    if (!(d == d) || d > 3.4e38 || d < -3.4e38) d = 0;
    if (d < 0) { pc(o, '-'); d = -d; }
    if (d == 0) { ps(o, "0.0"); return; }
    int e = 0;
    while (d >= 10.0) { d /= 10.0; e++; }
    while (d < 1.0) { d *= 10.0; e--; }
    long long q = (long long)(d * 1e8 + 0.5);
    if (q >= 1000000000LL) { q /= 10; e++; }
    pd(o, (int)(q / 100000000LL));
    pc(o, '.');
    long long r = q % 100000000LL;
    for (long long m = 10000000LL; m; m /= 10) pc(o, (char)('0' + (r / m) % 10));
    pc(o, 'e'); pc(o, e < 0 ? '-' : '+'); if (e < 0) e = -e;
    if (e < 10) pc(o, '0');
    pd(o, e);
}

/* «DCL TEMP[0..3]» o, con un solo registro, «DCL TEMP[3]» */
static void dcl_rango(sai_t *o, const char *arq, int lo, int hi, const char *resto)
{
    ps(o, "DCL "); ps(o, arq); pc(o, '['); pd(o, lo);
    if (hi > lo) { ps(o, ".."); pd(o, hi); }
    ps(o, "]"); ps(o, resto); pc(o, '\n');
}

static const char *arq_tgsi(int a)
{
    switch (a) {
    case RI_TEMP: return "TEMP"; case RI_ENT: return "IN"; case RI_SAI: return "OUT"; case RI_UNI: return "CONST";
    case RI_IMED: return "IMM"; case RI_END: return "ADDR"; case RI_AMOST: return "SAMP";
    }
    return "?";
}

typedef struct { sai_t *o; const ri_prog_t *p; int face_ent, face_tmp; } ctx_t;

static void swz(sai_t *o, uint8_t sw)
{
    pc(o, '.');
    for (int c = 0; c < 4; c++) pc(o, "xyzw"[RI_SW_C(sw, c)]);
}
/* el registro: «TEMP[3]», «CONST[ADDR[0].x+3]» */
static void reg(ctx_t *x, int arq, int ind, int rel)
{
    sai_t *o = x->o;
    if (arq == RI_ENT && x->face_tmp >= 0 && ind == x->face_ent && !rel) { arq = RI_TEMP; ind = x->face_tmp; }
    ps(o, arq_tgsi(arq)); pc(o, '[');
    if (rel) { ps(o, "ADDR[0].x+"); }
    pd(o, ind); pc(o, ']');
}
static void fonte(ctx_t *x, const ri_fonte_t *f, int so_comp)
{
    sai_t *o = x->o;
    if (f->mod & RI_NEG) pc(o, '-');
    if (f->mod & RI_ABS) pc(o, '|');
    reg(x, f->arq, f->ind, f->rel);
    if (so_comp >= 0) { pc(o, '.'); for (int c = 0; c < 4; c++) pc(o, "xyzw"[so_comp]); }
    else swz(o, f->sw);
    if (f->mod & RI_ABS) pc(o, '|');
}
static void destino(ctx_t *x, const ri_dest_t *d)
{
    sai_t *o = x->o;
    reg(x, d->arq, d->ind, d->rel);
    if (d->masc != 15) { pc(o, '.'); for (int c = 0; c < 4; c++) if (d->masc & (1 << c)) pc(o, "xyzw"[c]); }
}

static const char *nome_op(int op)
{
    switch (op) {
    case RI_MOV: return "MOV"; case RI_ADD: return "ADD"; case RI_MUL: return "MUL"; case RI_MAD: return "MAD";
    case RI_MIN: return "MIN"; case RI_MAX: return "MAX"; case RI_FLR: return "FLR"; case RI_FRC: return "FRC";
    case RI_TRUNC: return "TRUNC"; case RI_SLT: return "SLT"; case RI_SGE: return "SGE"; case RI_SEQ: return "SEQ";
    case RI_SNE: return "SNE"; case RI_CMP: return "CMP"; case RI_DP2: return "DP2"; case RI_DP3: return "DP3";
    case RI_DP4: return "DP4"; case RI_RCP: return "RCP"; case RI_RSQ: return "RSQ"; case RI_EX2: return "EX2";
    case RI_LG2: return "LG2"; case RI_SIN: return "SIN"; case RI_COS: return "COS"; case RI_POW: return "POW";
    case RI_DDX: return "DDX"; case RI_DDY: return "DDY"; case RI_ARL: return "ARL"; case RI_TEX: return "TEX";
    case RI_TXB: return "TXB"; case RI_TXL: return "TXL";
    }
    return NULL;
}
static int n_fontes(int op)
{
    switch (op) {
    case RI_MOV: case RI_FLR: case RI_FRC: case RI_TRUNC: case RI_RCP: case RI_RSQ: case RI_EX2: case RI_LG2:
    case RI_SIN: case RI_COS: case RI_DDX: case RI_DDY: case RI_ARL: case RI_IF: return 1;
    case RI_MAD: case RI_CMP: return 3;
    case RI_ADD: case RI_MUL: case RI_MIN: case RI_MAX: case RI_SLT: case RI_SGE: case RI_SEQ: case RI_SNE:
    case RI_DP2: case RI_DP3: case RI_DP4: case RI_POW: case RI_TEX: case RI_TXB: case RI_TXL: return 2;
    }
    return 0;
}

int sgl_tgsi_gerar(const ri_prog_t *p, const int *gen, char *saida, int max, char *erro, int max_erro)
{
    sai_t O = { saida, max, 0, 0 };
    sai_t *o = &O;
    ctx_t x = { o, p, -1, -1 };
    int fs = p->tipo == 1;
    #define FALHA(m) do { if (erro && max_erro > 0) copia_texto(erro, (m), (size_t)max_erro); return -1; } while (0)
    /* gl_FrontFacing y gl_PointCoord */
    int ntemp = p->ntemp;
    /* bucles con «continue» (directo: el CONT apunta al ENDLOOP del bucle): cada uno lleva una bandera en un temporal */
    int ncont = 0, tcont[64], fcont[64];
    for (int i = 0; i < p->nins; i++) {
        if (p->ins[i].op != RI_BGNLOOP) continue;
        int e = p->ins[i].alvo, tiene = 0;
        for (int k = i + 1; k < e && !tiene; k++) if (p->ins[k].op == RI_CONT && p->ins[k].alvo == e) tiene = 1;
        if (tiene && ncont < 64) { tcont[ncont] = e; fcont[ncont] = ntemp++; ncont++; }
        else if (tiene) FALHA("demasiados bucles con continue");
    }
    for (int r = 0; r < p->nent; r++) {
        if (p->ent[r].sem == RI_SEM_PONTO_COORD) FALHA("gl_PointCoord no se traduce (solo se usa al dibujar puntos)");
        if (fs && p->ent[r].sem == RI_SEM_FACE) { x.face_ent = r; x.face_tmp = ntemp++; }
    }
    ps(o, fs ? "FRAG\n" : "VERT\n");
    if (fs) ps(o, "PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\n");
    /* entradas */
    int orden = 0;
    for (int r = 0; r < p->nent; r++) {
        ps(o, "DCL IN["); pd(o, r); pc(o, ']');
        if (!fs) { pc(o, '\n'); continue; }
        switch (p->ent[r].sem) {
        case RI_SEM_POSICAO: ps(o, ", POSITION, LINEAR"); break;
        case RI_SEM_FACE: ps(o, ", FACE, CONSTANT"); break;
        case RI_SEM_GENERICO: ps(o, ", GENERIC["); pd(o, gen && gen[r] >= 0 ? gen[r] : 32 + orden); orden++; ps(o, "], PERSPECTIVE"); break;
        default: FALHA("entrada de fragmentos desconocida");
        }
        pc(o, '\n');
    }
    /* salidas */
    orden = 0;
    for (int r = 0; r < p->nsai; r++) {
        ps(o, "DCL OUT["); pd(o, r); ps(o, "], ");
        switch (p->sai[r].sem) {
        case RI_SEM_POSICAO: ps(o, "POSITION"); break;
        case RI_SEM_PONTO_TAM: ps(o, "PSIZE"); break;
        case RI_SEM_COR: ps(o, "COLOR"); break;
        case RI_SEM_GENERICO: ps(o, "GENERIC["); pd(o, gen && gen[r] >= 0 ? gen[r] : 32 + orden); orden++; pc(o, ']'); break;
        default: FALHA("salida desconocida");
        }
        pc(o, '\n');
    }
    for (int i = 0; i < p->namost; i++) {
        int cubo = p->amost_tipo[i] == RI_TEX_CUBO;
        ps(o, "DCL SAMP["); pd(o, i); ps(o, "]\nDCL SVIEW["); pd(o, i); ps(o, cubo ? "], CUBE, FLOAT\n" : "], 2D, FLOAT\n");
    }
    if (p->nuni > 0) dcl_rango(o, "CONST", 0, p->nuni - 1, "");
    if (ntemp > 0) {
        /* con un temporal indexado por ADDR (los arrays locales con índice dinámico) la RI no dice qué registros forman el
         * array: todos los temporales se declaran como UN array de TGSI, que la GPU trata con índices constantes igual de bien */
        int rel = 0;
        for (int i = 0; i < p->nins && !rel; i++) {
            const ri_ins_t *I = &p->ins[i];
            if (n_fontes(I->op) || nome_op(I->op)) {
                if (nome_op(I->op) && I->op != RI_ARL && I->d.arq == RI_TEMP && I->d.rel) rel = 1;
                for (int k = 0; k < n_fontes(I->op); k++) if (I->f[k].arq == RI_TEMP && I->f[k].rel) rel = 1;
            }
        }
        dcl_rango(o, "TEMP", 0, ntemp - 1, rel ? ", ARRAY(1)" : "");
    }
    if (p->usa_a0) ps(o, "DCL ADDR[0]\n");
    for (int i = 0; i < p->nimed; i++) {
        ps(o, "IMM["); pd(o, i); ps(o, "] FLT32 { ");
        for (int c = 0; c < 4; c++) { if (c) ps(o, ", "); pf(o, p->imed[i][c]); }
        ps(o, " }\n");
    }
    int imm_cero = p->nimed, imm_uno = p->nimed + 1;
    if (x.face_tmp >= 0 || ncont) { ps(o, "IMM["); pd(o, imm_cero); ps(o, "] FLT32 { 0.0, 0.0, 0.0, 0.0 }\n"); }
    if (ncont) { ps(o, "IMM["); pd(o, imm_uno); ps(o, "] FLT32 { 1.0, 1.0, 1.0, 1.0 }\n"); }
    int n = 0;
    #define NUM() do { char b_[12]; int k_ = 0, v_ = n++; do { b_[k_++] = (char)('0' + v_ % 10); v_ /= 10; } while (v_); \
        for (int s_ = 0; s_ < 4 - k_; s_++) { pc(o, ' '); } \
        while (k_) { pc(o, b_[--k_]); } \
        ps(o, ": "); } while (0)
    if (x.face_tmp >= 0) {
        /* temp = (FACE.x >= 0) ? 1.0 : 0.0, contra un inmediato extra con ceros */
        NUM(); ps(o, "SGE TEMP["); pd(o, x.face_tmp); ps(o, "], IN["); pd(o, x.face_ent); ps(o, "].xxxx, IMM["); pd(o, imm_cero); ps(o, "].xxxx\n");
    }
    int prof = 0;
    for (int i = 0; i < p->nins; i++) {
        const ri_ins_t *I = &p->ins[i];
        switch (I->op) {
        case RI_NOP: continue;
        case RI_FIM: NUM(); ps(o, "END\n"); continue;
        case RI_IF: NUM(); ps(o, "IF "); fonte(&x, &I->f[0], -1); pc(o, '\n'); prof++; continue;
        case RI_ELSE: NUM(); ps(o, "ELSE\n"); continue;
        case RI_ENDIF: NUM(); ps(o, "ENDIF\n"); prof--; continue;
        case RI_BGNLOOP: {
            NUM(); ps(o, "BGNLOOP\n"); prof++;
            for (int q = 0; q < ncont; q++) if (tcont[q] == I->alvo) {
                NUM(); ps(o, "MOV TEMP["); pd(o, fcont[q]); ps(o, "].x, IMM["); pd(o, imm_cero); ps(o, "].xxxx\n");
                NUM(); ps(o, "BGNLOOP\n");
            }
            continue;
        }
        case RI_ENDLOOP: {
            int q;
            for (q = 0; q < ncont && tcont[q] != i; q++) { }
            if (q < ncont) {
                NUM(); ps(o, "BRK\n");
                NUM(); ps(o, "ENDLOOP\n");
                NUM(); ps(o, "IF TEMP["); pd(o, fcont[q]); ps(o, "].xxxx\n");
                NUM(); ps(o, "BRK\n");
                NUM(); ps(o, "ENDIF\n");
            }
            NUM(); ps(o, "ENDLOOP\n"); prof--;
            continue;
        }
        case RI_BRK: case RI_CONT: {
            int q;
            for (q = 0; q < ncont && tcont[q] != I->alvo; q++) { }
            if (q < ncont && I->op == RI_BRK) { NUM(); ps(o, "MOV TEMP["); pd(o, fcont[q]); ps(o, "].x, IMM["); pd(o, imm_uno); ps(o, "].xxxx\n"); }
            NUM(); ps(o, q < ncont || I->op == RI_BRK ? "BRK\n" : "CONT\n");
            continue;
        }
        case RI_KILL: NUM(); ps(o, "KILL\n"); continue;
        default: break;
        }
        const char *nm = nome_op(I->op);
        if (!nm) FALHA("operación desconocida");
        NUM(); ps(o, nm);
        if (I->d.sat) ps(o, "_SAT");
        pc(o, ' ');
        if (I->op == RI_ARL) {
            int c = 0; while (c < 3 && !(I->d.masc & (1 << c))) c++;
            ps(o, "ADDR[0].x, "); fonte(&x, &I->f[0], RI_SW_C(I->f[0].sw, c)); pc(o, '\n');
            continue;
        }
        destino(&x, &I->d);
        int nf = n_fontes(I->op);
        for (int k = 0; k < nf; k++) {
            ps(o, ", ");
            if (k == 1 && (I->op == RI_TEX || I->op == RI_TXB || I->op == RI_TXL)) {
                ps(o, "SAMP["); pd(o, I->f[1].ind); ps(o, "], "); ps(o, I->tex == RI_TEX_CUBO ? "CUBE" : "2D");
                break;
            }
            fonte(&x, &I->f[k], -1);
        }
        pc(o, '\n');
    }
    fim(o);
    if (o->n >= o->max) FALHA("el TGSI no cabe en el búfer");
    #undef FALHA
    #undef NUM
    return o->n;
}
