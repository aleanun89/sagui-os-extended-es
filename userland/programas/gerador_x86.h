// SPDX-License-Identifier: GPL-2.0-or-later
/* gerador_x86.h - la parte común de los compiladores de SaguiOS (cc y pascal):
 * el búfer de código, la emisión de bytes, los saltos con parche, las
 * primitivas de punto flotante (SSE2 o x87, D039/D076) y las rutinas del
 * principio y del fin del programa generado.
 *
 * Es una cabecera con funciones "static", incluida una vez por compilador (como
 * cc_biblioteca.h): cada .c de userland/programas se convierte en un programa, así que
 * un módulo común en .c sería un ejecutable más en /bin. Los nombres son los
 * que el cc siempre usó, para que el cc siga igual línea por línea (D085).
 *
 * Quien incluye define antes:
 *   GX_NOME      el nombre del compilador, para "<nombre>: sin memoria";
 *   GX_MSG_SSE2  el aviso del programa generado en una CPU sin SSE2;
 * y define después invalidar_ultimas(), que olvida las anotaciones de su
 * optimizador de "última instrucción" (el salto condicional borra código). */

/* Direcciones del programa generado (ver userland/programa.ld y processo.h). */
#define ARQ_BASE   0x80000000u   /* el segmento de código empieza al inicio del archivo (D049) */
#define COD_DESLOC 0x100u        /* cabeceras ELF antes del código */
#define COD_BASE   (ARQ_BASE + COD_DESLOC)
#define DADOS_BASE 0x88000000u
#define BSS_BASE   0x8C000000u

/* ------------------------------------------------------------ utilidades */

static void *memoria(size_t n)
{
    void *p = aloca_zerado(n);
    if (!p) { imprimir_em(ERROS, "%s: sin memoria\n", GX_NOME); sair(1); }
    return p;
}

/* Vector que crece: *buf con *cap elementos de tam bytes. */
static void garantir(void **buf, int *cap, int precisa, int tam)
{
    if (precisa <= *cap) return;
    int novo = *cap ? *cap : 64;
    while (novo < precisa) novo *= 2;
    void *n = memoria((size_t)novo * tam);
    if (*buf) { memcpy(n, *buf, (size_t)*cap * tam); libera(*buf); }
    *buf = n;
    *cap = novo;
}

static char *duplicar_texto(const char *s, int n)
{
    char *d = memoria(n + 1);
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

/* ------------------------------------------------------------ código */

static uint8_t *cod;
static int ncod, capcod;
static uint8_t *dados;
static int ndados, capdados;
static int nbss;
static bool descartando;    /* el código se descartará (sizeof de C, función de la biblioteca no usada) */
static bool compilando_biblioteca = true;
static bool modo_linux;     /* -linux: programa para Linux (pruebas fuera de SaguiOS) */

static void e1(int b)
{
    garantir((void **)&cod, &capcod, ncod + 1, 1);
    cod[ncod++] = (uint8_t)b;
}
static void e2(int a, int b) { e1(a); e1(b); }
static void e3(int a, int b, int c) { e1(a); e1(b); e1(c); }
static void e4(uint32_t v) { e1(v & 0xFF); e1((v >> 8) & 0xFF); e1((v >> 16) & 0xFF); e1(v >> 24); }
static uint32_t ler4(int p) { return cod[p] | cod[p + 1] << 8 | cod[p + 2] << 16 | (uint32_t)cod[p + 3] << 24; }
static void por4(int p, uint32_t v) { cod[p] = v & 0xFF; cod[p + 1] = (v >> 8) & 0xFF; cod[p + 2] = (v >> 16) & 0xFF; cod[p + 3] = v >> 24; }

static int aloca_dados(int tam, int alin)
{
    ndados = (ndados + alin - 1) & ~(alin - 1);
    int p = ndados;
    garantir((void **)&dados, &capdados, ndados + tam + 1, 1);
    memset(dados + ndados, 0, tam);
    ndados += tam;
    return p;
}

static int aloca_bss(int tam, int alin)
{
    nbss = (nbss + alin - 1) & ~(alin - 1);
    int p = nbss;
    nbss += tam;
    return p;
}

/* ------------------------------------------------------------ saltos */

static void invalidar_ultimas(void);   /* de quem inclui */

/* ------------------------------------------------------------ optimizaciones (D108)
 *
 * Cada grupo se desactiva solo (cc -fno-mirilla etc.; -O0 desactiva todos),
 * para comparar el tamaño y el tiempo del programa generado. Las optimizaciones del
 * cc y del Pascal usan estas mismas claves.
 *   OTIM_MIRILLA     push/pop vecinos, salto a la instrucción siguiente
 *   OTIM_IMEDIATOS   operandos inmediatos y desplazamientos de 8 bits, operando
 *                    derecho en la memoria, constantes sin pasar por registro
 *   OTIM_SALTOS      comparar y saltar directo en las condiciones (&&, ||)
 *   OTIM_REDUCAO     desplazamientos en lugar de * y / por potencias de 2, x*0, x*1, x+0 */
#define OTIM_MIRILLA   1
#define OTIM_IMEDIATOS 2
#define OTIM_SALTOS    4
#define OTIM_REDUCAO   8
static unsigned otim_ativas = OTIM_MIRILLA | OTIM_IMEDIATOS | OTIM_SALTOS | OTIM_REDUCAO;
static bool otim(unsigned b) { return (otim_ativas & b) != 0; }

/* Trata una opción de la línea de comandos (-O0, -O1, -fno-mirilla...); false si no lo es. */
static bool opcao_otimizacao(const char *a)
{
    static const struct { const char *nome; unsigned bit; } t[] = {
        {"mirilla", OTIM_MIRILLA}, {"inmediatos", OTIM_IMEDIATOS},
        {"saltos", OTIM_SALTOS}, {"reduccion", OTIM_REDUCAO},
    };
    if (!compara_texto(a, "-O0")) { otim_ativas = 0; return true; }
    if (!compara_texto(a, "-O1") || !compara_texto(a, "-O")) { otim_ativas = 15; return true; }
    if (a[0] == '-' && a[1] == 'f' && a[2] == 'n' && a[3] == 'o' && a[4] == '-')
        for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
            if (!compara_texto(a + 5, t[i].nome)) { otim_ativas &= ~t[i].bit; return true; }
    return false;
}

/* Primera posición en la que la mirilla puede tocar: un salto (o un bucle que
 * vuelve) llega o puede llegar ahí, así que el código anterior no se une al
 * posterior. */
static int barreira;
static void marcar_rotulo(void) { barreira = ncod; }

/* Desplazamiento de una instrucción [ebp+d]: el modrm acaba de emitirse con
 * el modo de 32 bits (mod=10, rm=101); con un d pequeño pasa al de 8 bits. */
static void ed(uint32_t d)
{
    if (otim(OTIM_IMEDIATOS) && (cod[ncod - 1] & 0xC7) == 0x85 && (int32_t)d >= -128 && (int32_t)d <= 127) {
        cod[ncod - 1] ^= 0xC0;           /* 10 -> 01 */
        e1((int)(d & 0xFF));
        return;
    }
    e4(d);
}

/* Un inmediato de 8 bits con extensión de signo ("83 /n ib") en lugar de
 * "op eax, imm32" cuando cabe: op = /n (0 add, 1 or, 4 and, 5 sub, 6 xor,
 * 7 cmp), reg = registro (0 = eax). */
static void aplicar_imediato(int op, int reg, int32_t v, int opcode_eax)
{
    if (otim(OTIM_IMEDIATOS) && v >= -128 && v <= 127) { e3(0x83, 0xC0 | op << 3 | reg, v & 0xFF); return; }
    if (reg == 0) { e1(opcode_eax); e4((uint32_t)v); return; }
    e2(0x81, 0xC0 | op << 3 | reg);
    e4((uint32_t)v);
}

/* Última comparación: "setcc al; movzx eax, al" en [comparacao_ini,
 * comparacao_fim). Si justo después viene "test eax, eax; jz/jnz", el salto usa
 * directo las flags de la comparación (jcc). */
static int comparacao_ini = -1, comparacao_fim = -1, comparacao_cc;

static void setcc(int cc)
{
    comparacao_ini = ncod;
    e3(0x0F, cc, 0xC0);                 /* setcc al */
    e3(0x0F, 0xB6, 0xC0);               /* movzx eax, al */
    comparacao_fim = ncod;
    comparacao_cc = cc;
}

static int salto_condicional(int op)
{
    if ((op == 0x84 || op == 0x85) && ncod == comparacao_fim + 2 && cod[ncod - 2] == 0x85 && cod[ncod - 1] == 0xC0) {
        ncod = comparacao_ini;
        invalidar_ultimas();
        return 0x80 | ((comparacao_cc & 0x0F) ^ (op == 0x84));   /* jz: salta si la comparación dio falso */
    }
    return op;
}

/* Saltos hacia delante: lista enlazada en los propios huecos. */
static int saltar(int op, int lista)   /* op: 0xE9 (jmp), 0x84 (jz), 0x85 (jnz) */
{
    op = salto_condicional(op);
    if (op == 0xE9) e1(0xE9);
    else e2(0x0F, op);
    int p = ncod;
    e4((uint32_t)lista);
    return p;
}

static void remendar(int lista, int alvo)
{
    if (lista == -1) return;
    while (lista != -1) {
        int prox = (int)ler4(lista);
        /* Salto (jmp o jcc) a la instrucción siguiente: desaparece. Sólo si nada
         * más llega en medio o justo después de él (barrera). */
        if (alvo == ncod && lista + 4 == ncod && otim(OTIM_MIRILLA)) {
            int ini = cod[lista - 1] == 0xE9 ? lista - 1
                    : (lista >= 2 && cod[lista - 2] == 0x0F && (cod[lista - 1] & 0xF0) == 0x80) ? lista - 2 : -1;
            if (ini >= 0 && barreira <= ini) {
                ncod = alvo = ini;
                invalidar_ultimas();
                lista = prox;
                continue;
            }
        }
        por4(lista, (uint32_t)(alvo - (lista + 4)));
        lista = prox;
    }
    if (alvo == ncod) barreira = ncod;
}

/* "a && b" / "a || b" recién montado (0/1 en EAX): los huecos de los saltos
 * (el primero es el último salto emitido) y donde empieza el "mov eax, 0/1", para
 * que una condición (if, while, otro &&) salte directo sin montar el 0/1 (D108).
 * Quien incluye pone a cero logico_fim en invalidar_ultimas(). */
static int logico_fim = -1, logico_ini, logico_n, logico_barr;
static bool logico_e;
static int *logico_buracos;

/* Llamar antes de emitir el "mov eax, 0/1": l = lista de saltos del operador. */
static void logico_gravar(int l, bool e)
{
    int n = 0;
    for (int h = l; h != -1; h = (int)ler4(h)) n++;
    if (logico_buracos) libera(logico_buracos);
    logico_buracos = memoria((size_t)(n ? n : 1) * sizeof(int));
    n = 0;
    for (int h = l; h != -1; h = (int)ler4(h)) logico_buracos[n++] = h;
    logico_n = n;
    logico_e = e;
    logico_ini = ncod;
    logico_barr = barreira;
}

/* Llamar después de montar el 0/1 (y de cualquier parche): marca el fin. */
static void logico_fechar(void) { logico_fim = ncod; }

/* Si el último valor es el 0/1 de un &&/|| recién montado, quita el 0/1 y deja que los
 * saltos vayan directo: "salta si falso" a la lista (true), el cuerpo sigue
 * justo después. *lista es la lista de saltos a la que sumar. */
static bool logico_saltar_se_falso(int *lista)
{
    if (!otim(OTIM_SALTOS) || logico_fim != ncod || logico_n < 2) return false;
    int *h = logico_buracos, n = logico_n;
    ncod = logico_ini;
    barreira = logico_barr;
    invalidar_ultimas();
    if (logico_e) {                         /* todos saltan "si falso": basta con juntar las listas */
        int prox = *lista;
        for (int i = n - 1; i >= 0; i--) { por4(h[i], (uint32_t)prox); prox = h[i]; }
        *lista = prox;
        return true;
    }
    /* "||": los saltos son "si verdadero". El último pasa a "si falso" y los
     * otros caen justo después de él, donde sigue el cuerpo. */
    cod[h[0] - 1] ^= 1;
    for (int i = n - 1; i >= 1; i--) por4(h[i], i == n - 1 ? (uint32_t)-1 : (uint32_t)h[i + 1]);
    remendar(h[1], ncod);
    por4(h[0], (uint32_t)*lista);
    *lista = h[0];
    return true;
}

static void saltar_para(int op, int alvo)
{
    op = salto_condicional(op);
    if (op == 0xE9) e1(0xE9);
    else e2(0x0F, op);
    e4((uint32_t)(alvo - (ncod + 4)));
}

static void mov_eax(uint32_t v) { e1(0xB8); e4(v); }
static int ultimo_push = -1;   /* donde termina el último "push eax" emitido por push_eax() */
static void push_eax(void) { e1(0x50); ultimo_push = ncod; }
static void pop_ecx(void)
{
    /* "push eax; pop ecx" se convierte en "mov ecx, eax" (el push puede estar en la barrera, pero no después de ella) */
    if (otim(OTIM_MIRILLA) && ultimo_push == ncod && ncod >= 1 && cod[ncod - 1] == 0x50 && barreira < ncod) {
        ncod--;
        invalidar_ultimas();
        e2(0x89, 0xC1);
        return;
    }
    e1(0x59);
}
static void test_eax(void) { e2(0x85, 0xC0); }

/* k tal que v == 1 << k (v > 0), o -1. */
static int pot2(uint32_t v)
{
    for (int k = 0; k < 32; k++) if (v == 1u << k) return k;
    return -1;
}

/* EAX / constante (resto = false) o EAX % constante (v != 0). Potencia de 2: desplazamientos,
 * con el ajuste que hace que la división con signo redondee hacia cero (D108);
 * si no, idiv/div con la constante en ECX (sin la pila). */
static void dividir_constante(bool resto, bool sem, int v)
{
    int k = pot2((uint32_t)v);
    if (otim(OTIM_REDUCAO)) {
        if (v == 1) { if (resto) e2(0x31, 0xC0); return; }       /* x/1 = x, x%1 = 0 */
        if (k > 0 && sem) {
            if (!resto) e3(0xC1, 0xE8, k);                         /* shr eax, k */
            else aplicar_imediato(4, 0, (int32_t)(uint32_t)(v - 1), 0x25); /* and eax, v-1 */
            return;
        }
        if (k > 0 && k < 31) {
            int m = v - 1;
            if (!resto) {
                e1(0x99);                                             /* cdq */
                aplicar_imediato(4, 2, m, 0);                              /* and edx, 2^k-1 (sólo negativos) */
                e2(0x01, 0xD0);                                       /* add eax, edx */
                e3(0xC1, 0xF8, k);                                    /* sar eax, k */
            } else {
                e2(0x89, 0xC1);                                       /* mov ecx, eax */
                e1(0x99);                                             /* cdq */
                aplicar_imediato(4, 2, m, 0);                              /* and edx, 2^k-1 */
                e2(0x01, 0xD0);                                       /* add eax, edx */
                aplicar_imediato(4, 0, ~m, 0x25);                          /* and eax, ~(2^k-1) */
                e2(0x29, 0xC1);                                       /* sub ecx, eax */
                e2(0x89, 0xC8);                                       /* mov eax, ecx */
            }
            return;
        }
    }
    e1(0xB9); e4((uint32_t)v);                                        /* mov ecx, v */
    if (sem) { e2(0x31, 0xD2); e2(0xF7, 0xF1); }                      /* xor edx, edx; div ecx */
    else { e1(0x99); e2(0xF7, 0xF9); }                                /* cdq; idiv ecx */
    if (resto) e2(0x89, 0xD0);                                    /* mov eax, edx */
}


/* ------------------------------------------------------------ punto flotante (D039)
 *
 * SSE2 (predeterminado con la CPU que lo tiene): el valor real queda en XMM0; XMM7 es
 * borrador. x87 (sin SSE2, o -x87): el valor queda en ST(0). */

static bool usar_sse = true;
static bool usou_real;               /* generó alguna instrucción de la x87/SSE2 (2ª pasada: ajusta la FPU al inicio) */
static bool usou_real_programa;      /* ... fuera de la biblioteca (1ª pasada) */
static int lista_para_inteiro = -1;  /* "call" esperando la rutina ST(0) -> EAX */

static void x87(void)
{
    if (descartando) return;
    usou_real = true;
    if (!compilando_biblioteca) usou_real_programa = true;
}

/* Instrucción SSE: [prefijo] 0F op modrm. Prefijo F2 = double (sd), F3 =
 * float (ss), 66 = ucomisd, 0 = movaps/xorps/ucomiss. */
static void sse(int pre, int op, int modrm) { x87(); if (pre) e1(pre); e3(0x0F, op, modrm); }
#define XMM(a, b) (0xC0 | (a) << 3 | (b))   /* modrm registro-registro */
#define MEM_XMM7(modrm) (((modrm) & 0xC7) | 0x38)   /* la misma dirección, con XMM7 */

static void f2(int a, int b) { x87(); e2(a, b); }
static void f3(int a, int b, int c) { x87(); e3(a, b, c); }
static void esp_menos(int n) { e3(0x83, 0xEC, n); }   /* sub esp, n */
static void esp_mais(int n) { e3(0x83, 0xC4, n); }    /* add esp, n */
static void fstp_st0(void) { f2(0xDD, 0xD8); }

/* Constantes reales quedan en los datos (las iguales se reaprovechan). */
typedef struct { uint32_t lo, hi; bool f; int off; } constante_t;
static constante_t *constantes;
static int nconstantes, capconstantes;

static uint32_t endereco_bits(uint32_t lo, uint32_t hi, bool eh_float)
{
    for (int i = 0; i < nconstantes; i++)
        if (constantes[i].lo == lo && constantes[i].hi == hi && constantes[i].f == eh_float)
            return DADOS_BASE + (uint32_t)constantes[i].off;
    int off = aloca_dados(eh_float ? 4 : 8, eh_float ? 4 : 8);
    memcpy(dados + off, &lo, 4);
    if (!eh_float) memcpy(dados + off + 4, &hi, 4);
    garantir((void **)&constantes, &capconstantes, nconstantes + 1, sizeof(constante_t));
    constante_t *c = &constantes[nconstantes++];
    c->lo = lo; c->hi = hi; c->f = eh_float; c->off = off;
    return DADOS_BASE + (uint32_t)off;
}

static uint32_t endereco_constante(double v, bool eh_float)
{
    uint32_t lo, hi = 0;
    if (eh_float) { float x = (float)v; memcpy(&lo, &x, 4); }
    else { memcpy(&lo, &v, 4); memcpy(&hi, (char *)&v + 4, 4); }
    return endereco_bits(lo, hi, eh_float);
}

/* x87: ST(0) -> EAX cortando la parte fraccionaria, con un "call" a la
 * rutina del fin del código (emitir_rotinas_finais). */
static void chamar_para_inteiro(void)
{
    x87();
    e1(0xE8);
    int p = ncod;
    e4(descartando ? 0 : (uint32_t)lista_para_inteiro);
    if (!descartando) lista_para_inteiro = p;
}

/* Funciones integradas que se convierten en instrucciones de la x87 (D039). Los argumentos son
 * double; con dos, ST(0) = primero y ST(1) = segundo. "comum" es el nombre
 * de la función de la biblioteca de C que se convierte en la misma instrucción. */
static const struct { const char *nome, *comum; int nargs; uint8_t n; uint8_t cod[18]; } embutidas[] = {
    {"__builtin_sqrt", "sqrt", 1, 2, {0xD9, 0xFA}},                          /* fsqrt */
    {"__builtin_fabs", "fabs", 1, 2, {0xD9, 0xE1}},                          /* fabs */
    {"__builtin_sin", "sin", 1, 2, {0xD9, 0xFE}},                            /* fsin */
    {"__builtin_cos", "cos", 1, 2, {0xD9, 0xFF}},                            /* fcos */
    {"__builtin_tan", "tan", 1, 4, {0xD9, 0xF2, 0xDD, 0xD8}},                /* fptan; fstp st0 (el 1.0) */
    {"__builtin_atan", "atan", 1, 4, {0xD9, 0xE8, 0xD9, 0xF3}},              /* fld1; fpatan */
    {"__builtin_atan2", "atan2", 2, 4, {0xD9, 0xC9, 0xD9, 0xF3}},            /* fxch; fpatan: atan(y/x) */
    {"__builtin_rint", NULL, 1, 2, {0xD9, 0xFC}},                            /* frndint (al par más próximo) */
    {"__builtin_log2", NULL, 1, 6, {0xD9, 0xE8, 0xD9, 0xC9, 0xD9, 0xF1}},    /* fld1; fxch; fyl2x */
    {"__builtin_log", NULL, 1, 6, {0xD9, 0xED, 0xD9, 0xC9, 0xD9, 0xF1}},     /* fldln2; fxch; fyl2x */
    {"__builtin_log10", NULL, 1, 6, {0xD9, 0xEC, 0xD9, 0xC9, 0xD9, 0xF1}},   /* fldlg2; fxch; fyl2x */
    /* 2^x: n = rint(x), f = x - n; (2^f - 1) + 1, escalado por 2^n. */
    {"__builtin_exp2", NULL, 1, 16, {0xD9, 0xC0, 0xD9, 0xFC, 0xDC, 0xE9, 0xD9, 0xC9, 0xD9, 0xF0,
                                     0xD9, 0xE8, 0xDE, 0xC1, 0xD9, 0xFD}},  /* (+ fstp st1 abajo) */
    /* fprem hasta C2 = 0 (reducción completa); queda el resto en ST(0). */
    {"__builtin_fmod", NULL, 2, 11, {0xD9, 0xF8, 0xDF, 0xE0, 0xF6, 0xC4, 0x04, 0x75, 0xF7, 0xDD, 0xD9}},
    {"__builtin_ldexp", NULL, 2, 4, {0xD9, 0xFD, 0xDD, 0xD9}},               /* fscale; fstp st1 */
};
#define N_EMBUTIDAS ((int)(sizeof embutidas / sizeof embutidas[0]))

/* ------------------------------------------------------------ comienzo y fin del programa */

static int pos_iniciar_fpu;   /* 5 bytes al comienzo de _inicio: se convierte en "call" si el programa usa la x87 */

static void reservar_iniciar_fpu(void)
{
    pos_iniciar_fpu = ncod;
    e1(0x0F); e2(0x1F, 0x44); e2(0x00, 0x00);   /* nop de 5 bytes */
}

/* Cuerpo de __syscall(n, a, b, c): la llamada al sistema (int 0x80), con los
 * argumentos en la pila como en C (el primero en [esp+4]). */
static void __attribute__((unused)) emitir_rotina_syscall(void)
{
    e1(0x53);                      /* push ebx */
    e1(0x8B); e3(0x44, 0x24, 0x08);/* mov eax, [esp+8] */
    e1(0x8B); e3(0x5C, 0x24, 0x0C);/* mov ebx, [esp+12] */
    e1(0x8B); e3(0x4C, 0x24, 0x10);/* mov ecx, [esp+16] */
    e1(0x8B); e3(0x54, 0x24, 0x14);/* mov edx, [esp+20] */
    e2(0xCD, 0x80);                /* int 0x80 */
    e1(0x5B);                      /* pop ebx */
    e1(0xC3);
}

/* Rutinas que sólo entran en el programa si se usan, al final del código. */
static void emitir_rotinas_finais(void)
{
    if (lista_para_inteiro != -1) {
        /* ST(0) -> EAX cortando hacia cero: cambia el redondeo de la x87 sólo
         * durante el fistp (sin SSE3 no hay fisttp). */
        remendar(lista_para_inteiro, ncod);
        lista_para_inteiro = -1;
        static const uint8_t r[] = {
            0x83, 0xEC, 0x0C,                   /* sub esp, 12 */
            0xD9, 0x3C, 0x24,                   /* fnstcw [esp] */
            0x66, 0x8B, 0x04, 0x24,             /* mov ax, [esp] */
            0x80, 0xCC, 0x0C,                   /* or ah, 0x0C (cortar) */
            0x66, 0x89, 0x44, 0x24, 0x02,       /* mov [esp+2], ax */
            0xD9, 0x6C, 0x24, 0x02,             /* fldcw [esp+2] */
            0xDF, 0x7C, 0x24, 0x04,             /* fistp qword [esp+4] */
            0xD9, 0x2C, 0x24,                   /* fldcw [esp] */
            0x8B, 0x44, 0x24, 0x04,             /* mov eax, [esp+4] */
            0x83, 0xC4, 0x0C,                   /* add esp, 12 */
            0xC3,                               /* ret */
        };
        for (unsigned i = 0; i < sizeof r; i++) e1(r[i]);
    }
    if (usou_real && usar_sse) {
        /* El programa usa SSE2: comprueba en CPUID (bit 26 de EDX) y, sin él,
         * termina con un mensaje en vez de caer en una instrucción inválida. */
        static const char msg[] = GX_MSG_SSE2;
        int off = aloca_dados((int)sizeof msg - 1, 1);
        memcpy(dados + off, msg, sizeof msg - 1);
        cod[pos_iniciar_fpu] = 0xE8;
        por4(pos_iniciar_fpu + 1, (uint32_t)(ncod - (pos_iniciar_fpu + 5)));
        e1(0x53);                                   /* push ebx */
        mov_eax(1);
        e2(0x0F, 0xA2);                             /* cpuid */
        e1(0x5B);                                   /* pop ebx */
        e2(0xF7, 0xC2); e4(1u << 26);               /* test edx, SSE2 */
        e2(0x75, 34);                               /* jnz fin */
        mov_eax(modo_linux ? 4 : SIS_ESCREVER);
        e1(0xBB); e4(2);                            /* mov ebx, 2 (errores) */
        e1(0xB9); e4(DADOS_BASE + (uint32_t)off);   /* mov ecx, msg */
        e1(0xBA); e4((uint32_t)sizeof msg - 1);     /* mov edx, tamaño */
        e2(0xCD, 0x80);
        mov_eax(modo_linux ? 1 : SIS_SAIR);
        e1(0xBB); e4(1);
        e2(0xCD, 0x80);
        e1(0xC3);                                   /* fin: ret */
    } else if (usou_real) {
        /* La x87 empieza con precisión de 64 bits; C quiere double (53 bits),
         * para que los resultados sean los mismos que los de SSE2 y GCC (D039).
         * Palabra de control 0x027F: excepciones enmascaradas, redondear al
         * más próximo, precisión doble. */
        cod[pos_iniciar_fpu] = 0xE8;
        por4(pos_iniciar_fpu + 1, (uint32_t)(ncod - (pos_iniciar_fpu + 5)));
        static const uint8_t r[] = {
            0x68, 0x7F, 0x02, 0x00, 0x00,       /* push 0x027F */
            0xD9, 0x2C, 0x24,                   /* fldcw [esp] */
            0x58,                               /* pop eax */
            0xC3,
        };
        for (unsigned i = 0; i < sizeof r; i++) e1(r[i]);
    }
}
