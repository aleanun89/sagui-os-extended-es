/* desenrolar.c - el desenrollador de pila de las excepciones de C++ (D228).
 *
 * Cuando un programa de C++ lanza una excepción, hay que subir por la pila
 * cuadro a cuadro hasta el «catch» que la recoge, ejecutando por el camino
 * los destructores de los objetos locales. g++ no genera código para subir:
 * describe cada función en la sección .eh_frame (las tablas CFI de DWARF:
 * dónde está la dirección de retorno, dónde guardó cada registro) y deja los
 * «catch» y los destructores en tablas (la LSDA, .gcc_except_table) que lee
 * la personalidad de C++ (excecoes.cpp). Este fichero es el que sube.
 *
 * Es el modelo de dos fases del ABI de Itanium:
 *
 *   1. búsqueda: se sube sin tocar nada, preguntando a la personalidad de
 *      cada cuadro si tiene un manejador para la excepción. Si no hay
 *      ninguno, no se ha destruido nada y __cxa_throw llama a terminate.
 *   2. limpieza: se vuelve a subir desde el principio, y en cada cuadro con
 *      algo que hacer (destructores o el propio catch) se «instala» el
 *      contexto: se restauran los registros de ese cuadro y se salta a su
 *      «landing pad». Si es un destructor, el código del cuadro termina
 *      llamando a _Unwind_Resume, que sigue subiendo desde allí.
 *
 * Solo i386, solo lo que genera g++ (CIE versión 1 y 3, las codificaciones de
 * punteros habituales, las instrucciones CFA de DWARF 4 y las expresiones
 * sencillas que g++ usa en el main que realinea la pila). Todo el código es
 * nuestro; no hay nada del libgcc.
 *
 * Las .eh_frame del programa las delimita el guion de enlace
 * (userland/programa_cxx.ld) con __eh_frame_inicio y __eh_frame_fim; la
 * búsqueda es lineal (las excepciones son raras: solo cuestan cuando se
 * lanzan).
 *
 * No es para varios hilos: el estado global de las excepciones (excecoes.cpp)
 * es uno solo. Los ports de C++ que hay son de un hilo.
 */
#include <stddef.h>
#include <stdint.h>

#include "desenrolar.h"

extern const uint8_t __eh_frame_inicio[] __attribute__((weak));
extern const uint8_t __eh_frame_fim[] __attribute__((weak));

/* Los registros de DWARF en i386. */
enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI, R_EIP, N_REGS };

/* Cómo se recupera un registro del cuadro que llamó. */
enum {
    REGRA_IGUAL = 0,    /* no lo tocó: el mismo valor */
    REGRA_INDEFINIDO,   /* no se puede recuperar (en el EIP: fin de la pila) */
    REGRA_OFFSET,       /* guardado en la memoria CFA + n */
    REGRA_VAL_OFFSET,   /* el valor es CFA + n */
    REGRA_REGISTO,      /* está en otro registro */
    REGRA_EXPR,         /* guardado en la dirección que da una expresión */
    REGRA_VAL_EXPR      /* el valor lo da una expresión */
};

typedef struct {
    uint8_t como;
    int32_t n;
    const uint8_t *expr;   /* la expresión (con su longitud delante) */
} regra_t;

/* Una fila de la tabla CFI: el CFA y las reglas de los registros. */
typedef struct {
    uint8_t cfa_e_expr;
    uint8_t cfa_reg;
    int32_t cfa_off;
    const uint8_t *cfa_expr;
    regra_t regras[N_REGS];
} linha_t;

#define MAX_LEMBRAR 8

typedef struct {
    linha_t linha;
    linha_t inicial;                 /* la de las instrucciones del CIE (DW_CFA_restore) */
    linha_t pilha[MAX_LEMBRAR];      /* DW_CFA_remember_state / restore_state */
    int n_pilha;
    uint32_t args_size;
} estado_t;

/* Lo que se saca de un CIE. */
typedef struct {
    const uint8_t *cie;
    uint32_t alin_codigo;
    int32_t alin_dados;
    uint32_t reg_retorno;
    uint8_t cod_fde, cod_lsda, tem_z, sinal;
    uintptr_t personalidade;
    const uint8_t *instr, *instr_fim;
} cie_t;

struct _Unwind_Context {
    uint32_t r[N_REGS];   /* los registros de este cuadro; r[R_EIP] es la dirección de retorno */
    uint32_t cfa;         /* el CFA de este cuadro (lo que valía el ESP antes de llamarle) */
    uint32_t args_size;   /* argumentos empujados en el punto de la llamada (DW_CFA_GNU_args_size) */
    uintptr_t lsda;
    uintptr_t inicio_func;
    _Unwind_Personality_Fn personalidade;
    estado_t estado;      /* la fila de este cuadro, para subir al siguiente */
};

/* ------------------------------------------------------------ lectura */

uintptr_t dw_ler_uleb(const uint8_t **p)
{
    uintptr_t v = 0;
    unsigned desl = 0;
    uint8_t b;
    do {
        b = *(*p)++;
        if (desl < 32) v |= (uintptr_t)(b & 0x7F) << desl;
        desl += 7;
    } while (b & 0x80);
    return v;
}

intptr_t dw_ler_sleb(const uint8_t **p)
{
    uintptr_t v = 0;
    unsigned desl = 0;
    uint8_t b;
    do {
        b = *(*p)++;
        if (desl < 32) v |= (uintptr_t)(b & 0x7F) << desl;
        desl += 7;
    } while (b & 0x80);
    if (desl < 32 && (b & 0x40)) v |= ~(uintptr_t)0 << desl;
    return (intptr_t)v;
}

static uint16_t ler16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t ler32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

unsigned dw_tamanho_codificacao(uint8_t cod)
{
    if (cod == DW_EH_PE_omit) return 0;
    switch (cod & 0x0F) {
    case DW_EH_PE_absptr: return 4;
    case DW_EH_PE_udata2: case DW_EH_PE_sdata2: return 2;
    case DW_EH_PE_udata4: case DW_EH_PE_sdata4: return 4;
    case DW_EH_PE_udata8: case DW_EH_PE_sdata8: return 8;
    default: return 0;   /* las de longitud variable no tienen tamaño fijo */
    }
}

int dw_ler_ponteiro(const uint8_t **pp, uint8_t cod, uintptr_t base_func, uintptr_t *saida)
{
    const uint8_t *p = *pp;
    uintptr_t v;
    if (cod == DW_EH_PE_omit) { *saida = 0; return 1; }
    if ((cod & 0x70) == DW_EH_PE_aligned) {
        p = (const uint8_t *)(((uintptr_t)p + 3) & ~(uintptr_t)3);
        v = ler32(p);
        p += 4;
        *pp = p;
        *saida = v;
        return 1;
    }
    const uint8_t *campo = p;
    switch (cod & 0x0F) {
    case DW_EH_PE_absptr:
    case DW_EH_PE_udata4: v = ler32(p); p += 4; break;
    case DW_EH_PE_sdata4: v = (uintptr_t)(int32_t)ler32(p); p += 4; break;
    case DW_EH_PE_udata2: v = ler16(p); p += 2; break;
    case DW_EH_PE_sdata2: v = (uintptr_t)(int32_t)(int16_t)ler16(p); p += 2; break;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8: v = ler32(p); p += 8; break;   /* 32 bits: la parte baja */
    case DW_EH_PE_uleb128: v = dw_ler_uleb(&p); break;
    case DW_EH_PE_sleb128: v = (uintptr_t)dw_ler_sleb(&p); break;
    default: return 0;
    }
    if (v != 0) {   /* un cero es «nulo» (una LSDA o una personalidad que no hay) */
        switch (cod & 0x70) {
        case DW_EH_PE_absptr: break;
        case DW_EH_PE_pcrel:   v += (uintptr_t)campo; break;
        case DW_EH_PE_funcrel: v += base_func; break;
        default: return 0;   /* textrel/datarel: no los genera g++ sin PIC en i386 */
        }
        if (cod & DW_EH_PE_indirect) v = *(const uintptr_t *)v;
    }
    *pp = p;
    *saida = v;
    return 1;
}

/* ------------------------------------------------------------ CIE y FDE */

static int ler_cie(const uint8_t *cie, cie_t *c)
{
    const uint8_t *p = cie;
    uint32_t len = ler32(p);
    if (len == 0 || len == 0xFFFFFFFFu) return 0;
    const uint8_t *fim = p + 4 + len;
    p += 4;
    if (ler32(p) != 0) return 0;   /* no es un CIE */
    p += 4;
    uint8_t versao = *p++;
    if (versao != 1 && versao != 3) return 0;
    const char *aug = (const char *)p;
    while (*p) p++;
    p++;
    c->cie = cie;
    c->cod_fde = DW_EH_PE_absptr;
    c->cod_lsda = DW_EH_PE_omit;
    c->personalidade = 0;
    c->tem_z = 0;
    c->sinal = 0;
    c->alin_codigo = (uint32_t)dw_ler_uleb(&p);
    c->alin_dados = (int32_t)dw_ler_sleb(&p);
    c->reg_retorno = versao == 1 ? *p++ : (uint32_t)dw_ler_uleb(&p);
    const uint8_t *fim_aug = NULL;
    if (aug[0] == 'z') {
        uintptr_t n = dw_ler_uleb(&p);
        fim_aug = p + n;
        c->tem_z = 1;
        for (const char *a = aug + 1; *a; a++) {
            if (*a == 'R') c->cod_fde = *p++;
            else if (*a == 'L') c->cod_lsda = *p++;
            else if (*a == 'P') {
                uint8_t cod = *p++;
                if (!dw_ler_ponteiro(&p, cod, 0, &c->personalidade)) return 0;
            } else if (*a == 'S') c->sinal = 1;
            else break;   /* lo que no se conoce: el tamaño de 'z' permite saltarlo */
        }
        p = fim_aug;
    } else if (aug[0] != 0) {
        return 0;   /* aumentos antiguos ("eh"): no los genera g++ moderno */
    }
    c->instr = p;
    c->instr_fim = fim;
    return 1;
}

/* Busca el FDE que cubre «pc»; devuelve 1 con el CIE leído, el inicio de la
 * función, la LSDA y las instrucciones del FDE. */
static int achar_fde(uintptr_t pc, cie_t *c, uintptr_t *inicio, uintptr_t *lsda,
                     const uint8_t **instr, const uint8_t **instr_fim)
{
    const uint8_t *p = __eh_frame_inicio, *fim = __eh_frame_fim;
    const uint8_t *cie_lido = NULL;
    if (!p || !fim) return 0;
    while (p + 8 <= fim) {
        uint32_t len = ler32(p);
        if (len == 0) break;   /* el terminador */
        if (len == 0xFFFFFFFFu) return 0;
        const uint8_t *reg = p + 4, *prox = reg + len;
        uint32_t id = ler32(reg);
        if (id != 0) {
            const uint8_t *cie = reg - id;
            if (cie != cie_lido) {
                if (!ler_cie(cie, c)) { p = prox; continue; }
                cie_lido = cie;
            }
            const uint8_t *q = reg + 4;
            uintptr_t ini, tam;
            if (!dw_ler_ponteiro(&q, c->cod_fde, 0, &ini)) { p = prox; continue; }
            if (!dw_ler_ponteiro(&q, c->cod_fde & 0x0F, 0, &tam)) { p = prox; continue; }
            if (ini != 0 && pc >= ini && pc - ini < tam) {
                *inicio = ini;
                *lsda = 0;
                if (c->tem_z) {
                    uintptr_t n = dw_ler_uleb(&q);
                    const uint8_t *fim_aug = q + n;
                    if (c->cod_lsda != DW_EH_PE_omit && !dw_ler_ponteiro(&q, c->cod_lsda, ini, lsda))
                        return 0;
                    q = fim_aug;
                }
                *instr = q;
                *instr_fim = prox;
                return 1;
            }
        }
        p = prox;
    }
    return 0;
}

/* ------------------------------------------------------------ CFA */

static void regra_por(linha_t *l, uintptr_t reg, uint8_t como, int32_t n, const uint8_t *expr)
{
    if (reg >= N_REGS) return;   /* x87, SSE...: no hacen falta para subir */
    l->regras[reg].como = como;
    l->regras[reg].n = n;
    l->regras[reg].expr = expr;
}

/* Ejecuta las instrucciones CFA hasta la fila que cubre «alvo» (sin incluirla
 * si empieza justo en alvo). 0 si hay algo que no se entiende. */
static int executar_cfa(const uint8_t *p, const uint8_t *fim, const cie_t *c, uintptr_t loc,
                        uintptr_t alvo, estado_t *e)
{
    linha_t *l = &e->linha;
    while (p < fim && loc < alvo) {
        uint8_t op = *p++;
        uint8_t baixo = op & 0x3F;
        uintptr_t reg, n;
        switch (op & 0xC0) {
        case 0x40: loc += baixo * c->alin_codigo; continue;                                     /* advance_loc */
        case 0x80: regra_por(l, baixo, REGRA_OFFSET, (int32_t)dw_ler_uleb(&p) * c->alin_dados, NULL); continue;
        case 0xC0: if (baixo < N_REGS) l->regras[baixo] = e->inicial.regras[baixo]; continue;  /* restore */
        default: break;
        }
        switch (op) {
        case 0x00: break;                                                         /* nop */
        case 0x01: if (!dw_ler_ponteiro(&p, c->cod_fde, 0, &loc)) return 0; break;   /* set_loc */
        case 0x02: loc += *p * c->alin_codigo; p += 1; break;                     /* advance_loc1 */
        case 0x03: loc += ler16(p) * c->alin_codigo; p += 2; break;               /* advance_loc2 */
        case 0x04: loc += ler32(p) * c->alin_codigo; p += 4; break;               /* advance_loc4 */
        case 0x05: reg = dw_ler_uleb(&p);                                         /* offset_extended */
                   regra_por(l, reg, REGRA_OFFSET, (int32_t)dw_ler_uleb(&p) * c->alin_dados, NULL); break;
        case 0x06: reg = dw_ler_uleb(&p);                                         /* restore_extended */
                   if (reg < N_REGS) { l->regras[reg] = e->inicial.regras[reg]; }
                   break;
        case 0x07: regra_por(l, dw_ler_uleb(&p), REGRA_INDEFINIDO, 0, NULL); break;  /* undefined */
        case 0x08: regra_por(l, dw_ler_uleb(&p), REGRA_IGUAL, 0, NULL); break;       /* same_value */
        case 0x09: reg = dw_ler_uleb(&p);                                         /* register */
                   regra_por(l, reg, REGRA_REGISTO, (int32_t)dw_ler_uleb(&p), NULL); break;
        case 0x0A: if (e->n_pilha >= MAX_LEMBRAR) return 0;                       /* remember_state */
                   e->pilha[e->n_pilha++] = *l; break;
        case 0x0B: if (e->n_pilha <= 0) return 0;                                 /* restore_state */
                   *l = e->pilha[--e->n_pilha]; break;
        case 0x0C: l->cfa_e_expr = 0;                                             /* def_cfa */
                   l->cfa_reg = (uint8_t)dw_ler_uleb(&p);
                   l->cfa_off = (int32_t)dw_ler_uleb(&p); break;
        case 0x0D: l->cfa_e_expr = 0; l->cfa_reg = (uint8_t)dw_ler_uleb(&p); break;  /* def_cfa_register */
        case 0x0E: l->cfa_e_expr = 0; l->cfa_off = (int32_t)dw_ler_uleb(&p); break;  /* def_cfa_offset */
        case 0x0F: l->cfa_e_expr = 1; l->cfa_expr = p;                             /* def_cfa_expression */
                   n = dw_ler_uleb(&p); p += n; break;
        case 0x10: reg = dw_ler_uleb(&p);                                          /* expression */
                   regra_por(l, reg, REGRA_EXPR, 0, p);
                   n = dw_ler_uleb(&p); p += n; break;
        case 0x11: reg = dw_ler_uleb(&p);                                          /* offset_extended_sf */
                   regra_por(l, reg, REGRA_OFFSET, (int32_t)dw_ler_sleb(&p) * c->alin_dados, NULL); break;
        case 0x12: l->cfa_e_expr = 0;                                              /* def_cfa_sf */
                   l->cfa_reg = (uint8_t)dw_ler_uleb(&p);
                   l->cfa_off = (int32_t)dw_ler_sleb(&p) * c->alin_dados; break;
        case 0x13: l->cfa_e_expr = 0; l->cfa_off = (int32_t)dw_ler_sleb(&p) * c->alin_dados; break;
        case 0x14: reg = dw_ler_uleb(&p);                                          /* val_offset */
                   regra_por(l, reg, REGRA_VAL_OFFSET, (int32_t)dw_ler_uleb(&p) * c->alin_dados, NULL); break;
        case 0x15: reg = dw_ler_uleb(&p);                                          /* val_offset_sf */
                   regra_por(l, reg, REGRA_VAL_OFFSET, (int32_t)dw_ler_sleb(&p) * c->alin_dados, NULL); break;
        case 0x16: reg = dw_ler_uleb(&p);                                          /* val_expression */
                   regra_por(l, reg, REGRA_VAL_EXPR, 0, p);
                   n = dw_ler_uleb(&p); p += n; break;
        case 0x2E: e->args_size = (uint32_t)dw_ler_uleb(&p); break;               /* GNU_args_size */
        case 0x2F: reg = dw_ler_uleb(&p);                                          /* GNU_negative_offset_extended */
                   regra_por(l, reg, REGRA_OFFSET, -(int32_t)dw_ler_uleb(&p) * c->alin_dados, NULL); break;
        default: return 0;
        }
    }
    return 1;
}

/* Evalúa una expresión DWARF (las sencillas que salen en las CFI de g++).
 * «expr» apunta a la longitud; si «empurrar», se empieza con el CFA en la
 * pila (DW_CFA_expression). 0 si hay algo que no se entiende. */
static int avaliar(const uint8_t *expr, const uint32_t *r, int empurrar, uint32_t cfa, uint32_t *saida)
{
    uint32_t pilha[16];
    int n = 0;
    const uint8_t *p = expr;
    uintptr_t len = dw_ler_uleb(&p);
    const uint8_t *fim = p + len;
    if (empurrar) pilha[n++] = cfa;
    while (p < fim) {
        uint8_t op = *p++;
        uint32_t a;
        if (n >= 15) return 0;
        if (op >= 0x30 && op <= 0x4F) { pilha[n++] = op - 0x30; continue; }                 /* lit0..31 */
        if (op >= 0x50 && op <= 0x6F) {                                                     /* reg0..31 */
            if (op - 0x50 >= N_REGS) return 0;
            pilha[n++] = r[op - 0x50]; continue;
        }
        if (op >= 0x70 && op <= 0x8F) {                                                     /* breg0..31 */
            if (op - 0x70 >= N_REGS) return 0;
            pilha[n++] = r[op - 0x70] + (uint32_t)dw_ler_sleb(&p); continue;
        }
        switch (op) {
        case 0x03: pilha[n++] = ler32(p); p += 4; break;                                    /* addr */
        case 0x06: if (n < 1) return 0; pilha[n - 1] = *(const uint32_t *)pilha[n - 1]; break;  /* deref */
        case 0x08: pilha[n++] = *p++; break;                                                /* const1u */
        case 0x09: pilha[n++] = (uint32_t)(int32_t)(int8_t)*p++; break;                     /* const1s */
        case 0x0A: pilha[n++] = ler16(p); p += 2; break;                                    /* const2u */
        case 0x0B: pilha[n++] = (uint32_t)(int32_t)(int16_t)ler16(p); p += 2; break;        /* const2s */
        case 0x0C: case 0x0D: pilha[n++] = ler32(p); p += 4; break;                         /* const4u/s */
        case 0x10: pilha[n++] = (uint32_t)dw_ler_uleb(&p); break;                           /* constu */
        case 0x11: pilha[n++] = (uint32_t)dw_ler_sleb(&p); break;                           /* consts */
        case 0x12: if (n < 1) return 0; pilha[n] = pilha[n - 1]; n++; break;                /* dup */
        case 0x13: if (n < 1) return 0; n--; break;                                         /* drop */
        case 0x14: if (n < 2) return 0; pilha[n] = pilha[n - 2]; n++; break;                /* over */
        case 0x16: if (n < 2) return 0; a = pilha[n - 1]; pilha[n - 1] = pilha[n - 2]; pilha[n - 2] = a; break;
        case 0x1A: if (n < 2) return 0; pilha[n - 2] &= pilha[n - 1]; n--; break;           /* and */
        case 0x1C: if (n < 2) return 0; pilha[n - 2] -= pilha[n - 1]; n--; break;           /* minus */
        case 0x22: if (n < 2) return 0; pilha[n - 2] += pilha[n - 1]; n--; break;           /* plus */
        case 0x23: if (n < 1) return 0; pilha[n - 1] += (uint32_t)dw_ler_uleb(&p); break;   /* plus_uconst */
        case 0x92: { uintptr_t rg = dw_ler_uleb(&p);                                        /* bregx */
                     if (rg >= N_REGS) return 0;
                     pilha[n++] = r[rg] + (uint32_t)dw_ler_sleb(&p); break; }
        case 0x96: break;                                                                   /* nop */
        default: return 0;
        }
    }
    if (n < 1) return 0;
    *saida = pilha[n - 1];
    return 1;
}

/* Prepara el contexto del cuadro cuyo IP está en ctx->r[R_EIP]: busca su FDE,
 * calcula su fila, su CFA, la LSDA y la personalidad. 0 = no hay FDE (fin de
 * lo que se puede subir) o no se entiende. */
static int analisar_quadro(struct _Unwind_Context *ctx)
{
    cie_t c;
    uintptr_t inicio, lsda;
    const uint8_t *instr, *instr_fim;
    /* El IP es la dirección de retorno: la instrucción de la llamada es la
     * anterior (y si la llamada es lo último de la función, el retorno ya es
     * de la siguiente). */
    uintptr_t pc = ctx->r[R_EIP] - 1;
    if (ctx->r[R_EIP] == 0) return 0;
    if (!achar_fde(pc, &c, &inicio, &lsda, &instr, &instr_fim)) return 0;
    if (c.reg_retorno != R_EIP) return 0;

    estado_t *e = &ctx->estado;
    for (int i = 0; i < N_REGS; i++) { e->linha.regras[i].como = REGRA_IGUAL; e->linha.regras[i].n = 0; e->linha.regras[i].expr = NULL; }
    e->linha.cfa_e_expr = 0;
    e->linha.cfa_reg = R_ESP;
    e->linha.cfa_off = 4;
    e->linha.cfa_expr = NULL;
    e->n_pilha = 0;
    e->args_size = 0;
    if (!executar_cfa(c.instr, c.instr_fim, &c, 0, ~(uintptr_t)0, e)) return 0;
    e->inicial = e->linha;
    e->n_pilha = 0;
    if (!executar_cfa(instr, instr_fim, &c, inicio, pc + 1, e)) return 0;

    if (e->linha.cfa_e_expr) {
        if (!avaliar(e->linha.cfa_expr, ctx->r, 0, 0, &ctx->cfa)) return 0;
    } else {
        if (e->linha.cfa_reg >= N_REGS) return 0;
        ctx->cfa = ctx->r[e->linha.cfa_reg] + (uint32_t)e->linha.cfa_off;
    }
    ctx->args_size = e->args_size;
    ctx->lsda = lsda;
    ctx->inicio_func = inicio;
    ctx->personalidade = (_Unwind_Personality_Fn)c.personalidade;
    return 1;
}

/* Pasa al cuadro que llamó al de ctx (ya analizado). 0 = no hay más pila. */
static int subir(struct _Unwind_Context *ctx)
{
    uint32_t novo[N_REGS];
    const linha_t *l = &ctx->estado.linha;
    for (int i = 0; i < N_REGS; i++) novo[i] = ctx->r[i];
    novo[R_ESP] = ctx->cfa;   /* sin regla, el ESP de quien llamó es el CFA */
    for (int i = 0; i < N_REGS; i++) {
        const regra_t *g = &l->regras[i];
        uint32_t v;
        switch (g->como) {
        case REGRA_IGUAL: break;
        case REGRA_INDEFINIDO: if (i == R_EIP) return 0; novo[i] = 0; break;
        case REGRA_OFFSET: novo[i] = *(const uint32_t *)(uintptr_t)(ctx->cfa + (uint32_t)g->n); break;
        case REGRA_VAL_OFFSET: novo[i] = ctx->cfa + (uint32_t)g->n; break;
        case REGRA_REGISTO: if (g->n < 0 || g->n >= N_REGS) return 0; novo[i] = ctx->r[g->n]; break;
        case REGRA_EXPR:
            if (!avaliar(g->expr, ctx->r, 1, ctx->cfa, &v)) return 0;
            novo[i] = *(const uint32_t *)(uintptr_t)v;
            break;
        case REGRA_VAL_EXPR:
            if (!avaliar(g->expr, ctx->r, 1, ctx->cfa, &v)) return 0;
            novo[i] = v;
            break;
        default: return 0;
        }
    }
    for (int i = 0; i < N_REGS; i++) ctx->r[i] = novo[i];
    return ctx->r[R_EIP] != 0;
}

/* Restaura los registros del cuadro y salta a su landing pad (en ensamblador,
 * más abajo). El ESP es el del punto de la llamada menos los argumentos que
 * estaban empujados, que el landing pad da por quitados. */
void desenrolar_instalar(const uint32_t *r) __attribute__((noreturn));

static void instalar(struct _Unwind_Context *ctx) __attribute__((noreturn));
static void instalar(struct _Unwind_Context *ctx)
{
    uint32_t r[N_REGS];
    for (int i = 0; i < N_REGS; i++) r[i] = ctx->r[i];
    r[R_ESP] += ctx->args_size;
    desenrolar_instalar(r);
}

/* ------------------------------------------------------------ las dos fases */

static _Unwind_Reason_Code fase2(struct _Unwind_Exception *exc, struct _Unwind_Context *ctx)
{
    for (;;) {
        if (!analisar_quadro(ctx)) return _URC_FATAL_PHASE2_ERROR;
        int e_manejador = (ctx->cfa == exc->private_2);
        if (ctx->personalidade) {
            _Unwind_Reason_Code r = ctx->personalidade(1, _UA_CLEANUP_PHASE | (e_manejador ? _UA_HANDLER_FRAME : 0),
                                                       exc->exception_class, exc, ctx);
            if (r == _URC_INSTALL_CONTEXT) instalar(ctx);
            if (r != _URC_CONTINUE_UNWIND) return _URC_FATAL_PHASE2_ERROR;
        }
        if (e_manejador) return _URC_FATAL_PHASE2_ERROR;   /* el manejador tenía que instalarse */
        if (!subir(ctx)) return _URC_FATAL_PHASE2_ERROR;
    }
}

/* Lo llama _Unwind_RaiseException (ensamblador) con los registros de quien
 * la llamó. Si encuentra manejador no vuelve; si no, devuelve el motivo. */
_Unwind_Reason_Code desenrolar_lancar_c(struct _Unwind_Exception *exc, const uint32_t *regs)
{
    struct _Unwind_Context ctx;
    for (int i = 0; i < N_REGS; i++) ctx.r[i] = regs[i];
    /* Fase 1: buscar el manejador sin tocar nada. */
    for (;;) {
        if (!analisar_quadro(&ctx)) return _URC_END_OF_STACK;
        if (ctx.personalidade) {
            _Unwind_Reason_Code r = ctx.personalidade(1, _UA_SEARCH_PHASE, exc->exception_class, exc, &ctx);
            if (r == _URC_HANDLER_FOUND) break;
            if (r != _URC_CONTINUE_UNWIND) return _URC_FATAL_PHASE1_ERROR;
        }
        if (!subir(&ctx)) return _URC_END_OF_STACK;
    }
    exc->private_1 = 0;
    exc->private_2 = ctx.cfa;
    /* Fase 2: desde el principio, limpiando hasta el manejador. */
    for (int i = 0; i < N_REGS; i++) ctx.r[i] = regs[i];
    return fase2(exc, &ctx);
}

/* Lo llama _Unwind_Resume al acabar los destructores de un cuadro: se sigue
 * la fase 2 desde ese cuadro. */
void desenrolar_retomar_c(struct _Unwind_Exception *exc, const uint32_t *regs) __attribute__((noreturn));
void desenrolar_retomar_c(struct _Unwind_Exception *exc, const uint32_t *regs)
{
    struct _Unwind_Context ctx;
    for (int i = 0; i < N_REGS; i++) ctx.r[i] = regs[i];
    fase2(exc, &ctx);
    cxxrt_aviso("C++: _Unwind_Resume no encontró dónde seguir (tablas de desenrollado dañadas)");
    __builtin_trap();
}

/* Los puntos de entrada en ensamblador: guardan los registros de quien llama
 * tal como estarán cuando la llamada vuelva (EIP = la dirección de retorno,
 * ESP = el de después del «ret») y llaman a la parte en C. EAX, ECX y EDX no
 * se guardan: quien llama no espera que se conserven. La pila queda alineada
 * a 16 en la llamada (con la de entrada a 12 mod 16, como en el ABI). */
__asm__(
    ".text\n"
    ".globl _Unwind_RaiseException\n"
    ".type _Unwind_RaiseException, @function\n"
    "_Unwind_RaiseException:\n"
    ".globl _Unwind_Resume_or_Rethrow\n"
    ".type _Unwind_Resume_or_Rethrow, @function\n"
    "_Unwind_Resume_or_Rethrow:\n"
    "    movl 4(%esp), %edx\n"
    "    leal 4(%esp), %eax\n"
    "    movl (%esp), %ecx\n"
    "    subl $44, %esp\n"
    "    movl $0, 0(%esp)\n"
    "    movl $0, 4(%esp)\n"
    "    movl $0, 8(%esp)\n"
    "    movl %ebx, 12(%esp)\n"
    "    movl %eax, 16(%esp)\n"
    "    movl %ebp, 20(%esp)\n"
    "    movl %esi, 24(%esp)\n"
    "    movl %edi, 28(%esp)\n"
    "    movl %ecx, 32(%esp)\n"
    "    movl %esp, %eax\n"
    "    subl $8, %esp\n"
    "    pushl %eax\n"
    "    pushl %edx\n"
    "    call desenrolar_lancar_c\n"
    "    addl $60, %esp\n"
    "    ret\n"
    ".size _Unwind_RaiseException, .-_Unwind_RaiseException\n"
    "\n"
    ".globl _Unwind_Resume\n"
    ".type _Unwind_Resume, @function\n"
    "_Unwind_Resume:\n"
    "    movl 4(%esp), %edx\n"
    "    leal 4(%esp), %eax\n"
    "    movl (%esp), %ecx\n"
    "    subl $44, %esp\n"
    "    movl $0, 0(%esp)\n"
    "    movl $0, 4(%esp)\n"
    "    movl $0, 8(%esp)\n"
    "    movl %ebx, 12(%esp)\n"
    "    movl %eax, 16(%esp)\n"
    "    movl %ebp, 20(%esp)\n"
    "    movl %esi, 24(%esp)\n"
    "    movl %edi, 28(%esp)\n"
    "    movl %ecx, 32(%esp)\n"
    "    movl %esp, %eax\n"
    "    subl $8, %esp\n"
    "    pushl %eax\n"
    "    pushl %edx\n"
    "    call desenrolar_retomar_c\n"
    "    ud2\n"
    ".size _Unwind_Resume, .-_Unwind_Resume\n"
    "\n"
    /* desenrolar_instalar(r): r[0..8] = eax ecx edx ebx esp ebp esi edi eip.
     * La dirección de destino se deja justo debajo del ESP nuevo (esa zona
     * ya no es de nadie: son los cuadros que se han desenrollado) y se salta
     * con un «ret». */
    ".globl desenrolar_instalar\n"
    ".type desenrolar_instalar, @function\n"
    "desenrolar_instalar:\n"
    "    movl 4(%esp), %eax\n"
    "    movl 16(%eax), %ecx\n"
    "    subl $4, %ecx\n"
    "    movl 32(%eax), %edx\n"
    "    movl %edx, (%ecx)\n"
    "    movl 12(%eax), %ebx\n"
    "    movl 20(%eax), %ebp\n"
    "    movl 24(%eax), %esi\n"
    "    movl 28(%eax), %edi\n"
    "    movl 8(%eax), %edx\n"
    "    movl %ecx, %esp\n"
    "    movl 0(%eax), %eax\n"
    "    ret\n"
    ".size desenrolar_instalar, .-desenrolar_instalar\n"
);

/* ------------------------------------------------------------ el resto del ABI */

uintptr_t _Unwind_GetGR(struct _Unwind_Context *ctx, int reg)
{
    return (reg >= 0 && reg < N_REGS) ? ctx->r[reg] : 0;
}

void _Unwind_SetGR(struct _Unwind_Context *ctx, int reg, uintptr_t valor)
{
    if (reg >= 0 && reg < N_REGS) ctx->r[reg] = (uint32_t)valor;
}

uintptr_t _Unwind_GetIP(struct _Unwind_Context *ctx) { return ctx->r[R_EIP]; }

uintptr_t _Unwind_GetIPInfo(struct _Unwind_Context *ctx, int *antes_da_instrucao)
{
    if (antes_da_instrucao) *antes_da_instrucao = 0;   /* sin cuadros de señal */
    return ctx->r[R_EIP];
}

void _Unwind_SetIP(struct _Unwind_Context *ctx, uintptr_t valor) { ctx->r[R_EIP] = (uint32_t)valor; }
uintptr_t _Unwind_GetLanguageSpecificData(struct _Unwind_Context *ctx) { return ctx->lsda; }
uintptr_t _Unwind_GetRegionStart(struct _Unwind_Context *ctx) { return ctx->inicio_func; }
uintptr_t _Unwind_GetCFA(struct _Unwind_Context *ctx) { return ctx->cfa; }

void _Unwind_DeleteException(struct _Unwind_Exception *exc)
{
    if (exc && exc->exception_cleanup) exc->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, exc);
}
