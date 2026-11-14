// SPDX-License-Identifier: GPL-2.0-or-later
/* cc.c - el compilador de C de SaguiOS.
 *
 *   cc programa.c              -> genera el ejecutable "programa"
 *   cc programa.c -o nombre    -> genera "nombre"
 *   cc -x87 programa.c         -> float/double con la x87 (el predeterminado sin SSE2)
 *   cc -sse2 programa.c        -> float/double con SSE2 (el predeterminado con SSE2)
 *
 * Compila un subconjunto amplio de C directamente a código de máquina i386 y
 * escribe un ejecutable ELF que el cargador del kernel (carregador.c) ejecuta
 * como cualquier programa de /bin. No hay ensamblador ni enlazador: es una
 * pasada única, del texto a los bytes.
 *
 * Cómo funciona, en orden:
 *   1. Preprocesador y analizador léxico juntos: el texto se convierte en una lista de
 *      "fichas" (tokens). #define (con y sin parámetros), #include "archivo",
 *      #ifdef/#ifndef/#if/#else/#endif se tratan aquí. #include <...> se
 *      ignora: la biblioteca estándar (abajo, BIBLIOTECA) ya viene incorporada.
 *   2. Análisis sintáctico por descenso recursivo, generando código en el mismo
 *      momento, como una máquina de pila: el resultado de toda expresión queda en
 *      EAX; el operando izquierdo de una operación binaria va a la pila.
 *   3. Los lvalues ("a" en "a = 1") usan un truco clásico de los compiladores de
 *      una pasada: una variable siempre genera "dirección en EAX" seguido de
 *      "carga [EAX]". Si después viene un "=", el compilador borra la última
 *      instrucción (la carga) y se queda con la dirección.
 *   4. Los saltos hacia delante quedan en una lista enlazada dentro del propio
 *      código (cada hueco de 4 bytes guarda la posición del hueco anterior)
 *      y se parchean cuando se conoce el destino.
 *   5. Al final, el código, los datos y la BSS se convierten en tres segmentos PT_LOAD
 *      en un archivo ELF32 (direcciones fijas, así que no necesita reubicación).
 *
 * Tipos: char, short, int, long (= int), unsigned, float, double (long
 * double = double), punteros, vectores (incluso de varias dimensiones),
 * struct, union, enum, typedef y punteros a función. No hay long long,
 * ni struct pasada por valor a funciones, ni goto, ni campos de bits.
 *
 * Punto flotante (D039): SSE2 escalar (un valor float/double queda en XMM0
 * en vez de EAX; retorno en XMM0) o la FPU x87 (valor y retorno en ST(0),
 * como en el ABI i386). El predeterminado sigue a la CPU donde corre cc (D076): con SSE2,
 * SSE2; sin él (Pentium III, Athlon XP), x87, para que el programa funcione
 * allí mismo. "-sse2" y "-x87" fuerzan una u otra (con aviso si es SSE2
 * en una CPU sin él). double ocupa 8 bytes en la pila de los argumentos; float, 4
 * (con prototipo).
 *
 * El programa se compila dos veces: la primera sólo descubre qué funciones de
 * la biblioteca son alcanzables desde main; la segunda genera el código sin
 * las otras.
 *
 * La opción "-linux" (sólo para desarrollo, fuera de SaguiOS) cambia los
 * números de las llamadas al sistema por los del Linux i386, para probar el
 * compilador en la computadora donde se construye SaguiOS. */
#include "sagui.h"

/* Lo que es común con el compilador de Pascal (D085): emisión de código,
 * saltos, punto flotante, principio/fin del programa y el ELF. */
#define GX_NOME "cc"
#define GX_MSG_SSE2 "este programa usa SSE2 y la CPU no lo tiene (compilalo con cc -x87)\n"
#include "gerador_x86.h"
#include "elf_saida.h"
/* ------------------------------------------------------------ utilidades */

static const char *arq_erro = "?";
static int linha_erro;

static void falhar(const char *fmt, ...) __attribute__((noreturn));
static void falhar(const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(msg, sizeof msg, fmt, ap);
    va_end(ap);
    imprimir_em(ERROS, "%s:%d: error: %s\n", arq_erro, linha_erro, msg);
    sair(1);
}

/* ------------------------------------------------------------ nombres */

/* Fichas: los caracteres simples usan su propio código; el resto viene de aquí. */
enum {
    F_FIM = 0, F_NUM = 256, F_TEXTO, F_NOME, F_FIM_LINHA,
    F_IGUAL, F_DIF, F_MENOR_IG, F_MAIOR_IG, F_E, F_OU, F_MAISMAIS, F_MENOSMENOS,
    F_SETA, F_SHL, F_SHR, F_RETICENCIAS, F_REAL,
    F_MAIS_AT, F_MENOS_AT, F_VEZES_AT, F_DIV_AT, F_MOD_AT, F_E_AT, F_OU_AT, F_XOU_AT, F_SHL_AT, F_SHR_AT,
    /* palavras reservadas */
    P_INT, P_CHAR, P_VOID, P_SHORT, P_LONG, P_UNSIGNED, P_SIGNED, P_STRUCT, P_UNION, P_ENUM,
    P_TYPEDEF, P_STATIC, P_EXTERN, P_CONST, P_VOLATILE, P_REGISTER, P_AUTO, P_INLINE, P_BOOL,
    P_IF, P_ELSE, P_WHILE, P_DO, P_FOR, P_RETURN, P_BREAK, P_CONTINUE, P_SWITCH, P_CASE,
    P_DEFAULT, P_SIZEOF, P_GOTO, P_FLOAT, P_DOUBLE,
};

static const char *const reservadas[] = {
    "int", "char", "void", "short", "long", "unsigned", "signed", "struct", "union", "enum",
    "typedef", "static", "extern", "const", "volatile", "register", "auto", "inline", "_Bool",
    "if", "else", "while", "do", "for", "return", "break", "continue", "switch", "case",
    "default", "sizeof", "goto", "float", "double",
};

struct macro;
struct simbolo;
struct tipo;

typedef struct nome {
    char           *s;
    int             reservada;      /* P_* o 0 */
    struct macro   *macro;
    struct simbolo *global;         /* ambito de enlace: vale em todas as unidades */
    struct simbolo *unit;           /* so na sua unidade de traducao (D133) */
    struct tipo    *etiqueta;       /* struct/union/enum com este nome (ambito da unidade) */
    struct tipo    *etiqueta_global; /* os tipos da biblioteca, para todas as unidades */
    struct nome    *prox;
    /* Funciones que ésta llama o cuya dirección usa (1ª pasada). */
    struct nome   **refs;
    int             nrefs, caprefs;
    bool            alcancavel;     /* la función se usa desde main */
    bool            trocada;        /* el programa definió su versión de esta función de la biblioteca */
} nome_t;

#define HASH_NOMES 1024
static nome_t *nomes[HASH_NOMES];

static nome_t *nome_de(const char *s, int n)
{
    uint32_t h = 5381;
    for (int i = 0; i < n; i++) h = h * 33 + (uint8_t)s[i];
    h %= HASH_NOMES;
    for (nome_t *x = nomes[h]; x; x = x->prox)
        if ((int)tam_texto(x->s) == n && memcmp(x->s, s, n) == 0) return x;
    nome_t *x = memoria(sizeof *x);
    x->s = duplicar_texto(s, n);
    x->prox = nomes[h];
    nomes[h] = x;
    return x;
}

/* ------------------------------------------------------------ fichas */

typedef struct {
    int         tipo;
    int         val;          /* número; o índice del texto literal */
    bool        sem_sinal;    /* número con sufijo u o demasiado grande para int */
    bool        real_float;   /* F_REAL con sufijo f */
    bool        longo;        /* número con sufijo ll: ocupa 64 bits (D128) */
    uint32_t    val_hi;       /* la parte alta del número de 64 bits */
    double      real;         /* F_REAL: el valor */
    nome_t     *nome;
    nome_t     *nao_expandir; /* viene de la expansión de esta macro */
    const char *arq;
    int         linha;
} ficha_t;

typedef struct { char *bytes; int n; } literal_t;
static literal_t *literais;
static int nliterais, capliterais;

static int novo_literal(char *bytes, int n)
{
    garantir((void **)&literais, &capliterais, nliterais + 1, sizeof(literal_t));
    literais[nliterais].bytes = bytes;
    literais[nliterais].n = n;
    return nliterais++;
}

/* ---- fuentes abiertas (el archivo principal, los #include y la biblioteca) ---- */

typedef struct {
    const char *texto;
    int         pos, n, linha;
    const char *nome;
    int         nivel_cond;   /* profundidad de #if cuando empezó el archivo */
} fonte_t;

#define MAX_FONTES 16
static fonte_t fontes[MAX_FONTES];
static int nfontes;
static bool inicio_de_linha = true;
static bool em_diretiva;      /* leyendo una línea de #define etc.: el fin de línea es una ficha */

/* #if anidados: sólo guardamos si cada nivel ya tuvo una rama verdadera. */
#define MAX_COND 32
static bool cond_ja_valeu[MAX_COND];
static int ncond;

static void abrir_fonte(const char *nome, const char *texto, int n)
{
    if (nfontes == MAX_FONTES) falhar("#include anidados demasiado");
    fonte_t *f = &fontes[nfontes++];
    f->texto = texto;
    f->pos = 0;
    f->n = n;
    f->linha = 1;
    f->nome = nome;
    f->nivel_cond = ncond;
    inicio_de_linha = true;
}

static char *ler_arquivo(const char *caminho, int *tam)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return NULL;
    int cap = 4096, n = 0, r;
    char *buf = memoria(cap + 1);
    while ((r = ler(fd, buf + n, (unsigned)(cap - n))) > 0) {
        n += r;
        if (n == cap) {
            char *novo = memoria(cap * 2 + 1);
            memcpy(novo, buf, n);
            libera(buf);
            buf = novo;
            cap *= 2;
        }
    }
    fechar(fd);
    buf[n] = 0;
    *tam = n;
    return buf;
}

static fonte_t *F(void) { return &fontes[nfontes - 1]; }
static int car(int k) { fonte_t *f = F(); return f->pos + k < f->n ? (uint8_t)f->texto[f->pos + k] : 0; }

static bool eh_letra(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xC0; }
static bool eh_digito(int c) { return c >= '0' && c <= '9'; }

/* Salta espacios y comentarios. Devuelve true si pasó por un fin de línea
 * durante una directiva (que entonces termina). */
static bool pular_espacos(void)
{
    for (;;) {
        int c = car(0);
        if (c == '\n') {
            if (em_diretiva) return true;
            F()->pos++;
            F()->linha++;
            inicio_de_linha = true;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            F()->pos++;
        } else if (c == '\\' && car(1) == '\n') {          /* línea continuada */
            F()->pos += 2;
            F()->linha++;
        } else if (c == '/' && car(1) == '/') {
            while (car(0) && car(0) != '\n') F()->pos++;
        } else if (c == '/' && car(1) == '*') {
            F()->pos += 2;
            while (car(0) && !(car(0) == '*' && car(1) == '/')) {
                if (car(0) == '\n') F()->linha++;
                F()->pos++;
            }
            if (!car(0)) falhar("comentario sin cerrar");
            F()->pos += 2;
        } else {
            return false;
        }
    }
}

static int ler_escape(void)
{
    int c = car(0);
    F()->pos++;
    if (c != '\\') return c;
    c = car(0);
    F()->pos++;
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
        int v = c - '0';
        for (int i = 0; i < 2 && car(0) >= '0' && car(0) <= '7'; i++) v = v * 8 + car(0) - '0', F()->pos++;
        return v & 0xFF;
    }
    case 'x': {
        int v = 0;
        for (;;) {
            int d = car(0);
            if (d >= '0' && d <= '9') v = v * 16 + d - '0';
            else if (d >= 'a' && d <= 'f') v = v * 16 + d - 'a' + 10;
            else if (d >= 'A' && d <= 'F') v = v * 16 + d - 'A' + 10;
            else break;
            F()->pos++;
        }
        return v & 0xFF;
    }
    case 'a': return 7;
    case 'b': return 8;
    case 'f': return 12;
    case 'v': return 11;
    case 'e': return 27;
    default:  return c;   /* \\ \' \" \? */
    }
}

static void diretiva(void);

/* ¿El número que empieza aquí es de punto flotante? ("1.5", ".5", "2.", "1e-3";
 * los hexadecimales no.) */
static bool eh_numero_real(void)
{
    if (car(0) == '0' && (car(1) == 'x' || car(1) == 'X')) return false;
    int k = 0;
    while (eh_digito(car(k))) k++;
    if (car(k) == '.') return true;
    if (car(k) == 'e' || car(k) == 'E') {
        k++;
        if (car(k) == '+' || car(k) == '-') k++;
        return eh_digito(car(k));
    }
    return false;
}

/* Lee "123.456e-7" en long double (64 bits de mantisa, el cc se compila
 * para la x87): hasta 19 cifras significativas exactas, después la escala
 * por 10^e. El redondeo final a double queda correcto en casi
 * todos los casos. Sufijo f: float; l: long double (= double). */
static double ler_real(bool *eh_float)
{
    fonte_t *f = F();
    unsigned long long m = 0;
    int nd = 0, e = 0;
    bool ponto = false;
    for (;;) {
        int c = car(0);
        if (eh_digito(c)) {
            if (nd < 19) {
                m = m * 10 + (unsigned)(c - '0');
                if (m) nd++;
                if (ponto) e--;
            } else if (!ponto) {
                e++;
            }
        } else if (c == '.' && !ponto) {
            ponto = true;
        } else {
            break;
        }
        f->pos++;
    }
    if (car(0) == 'e' || car(0) == 'E') {
        f->pos++;
        bool neg = false;
        if (car(0) == '+' || car(0) == '-') neg = car(0) == '-', f->pos++;
        int x = 0;
        while (eh_digito(car(0))) { if (x < 100000) x = x * 10 + car(0) - '0'; f->pos++; }
        e += neg ? -x : x;
    }
    *eh_float = false;
    if (car(0) == 'f' || car(0) == 'F') { *eh_float = true; f->pos++; }
    else if (car(0) == 'l' || car(0) == 'L') f->pos++;
    long double v = (long double)m, p = 1, b = 10;
    int k = e < 0 ? -e : e;
    if (k > 5000) k = 5000;
    while (k) { if (k & 1) p *= b; b *= b; k >>= 1; }
    v = e < 0 ? v / p : v * p;
    return (double)v;
}

/* Multiplica un número de 64 bits (vhi:v) por la base y le suma d (D128).
 * El acarreo a la palabra alta puede ser de varios bits (hasta base-1), así que
 * se calcula por mitades de 16 bits: caben en 32 bits sin desbordarse. */
static void multiplicar64(uint32_t *vhi, uint32_t *v, uint32_t base, uint32_t d)
{
    uint32_t vh = *vhi, vl = *v;
    uint32_t baixo = vl & 0xFFFFu, alto = vl >> 16;
    uint32_t t = baixo * base + d;          /* base <= 16: cabe en 32 bits */
    uint32_t r0 = t & 0xFFFFu, c1 = t >> 16;
    uint32_t t2 = alto * base + c1;
    uint32_t r1 = t2 & 0xFFFFu, c2 = t2 >> 16;
    if (vh > (0xFFFFFFFFu - c2) / base) falhar("numero grande demais para 64 bits");
    *vhi = vh * base + c2;
    *v = (r1 << 16) | r0;
}

/* Lee la siguiente ficha directamente del texto, ya tratando las directivas. */
static void lex_bruto(ficha_t *t)
{
    for (;;) {
        if (nfontes == 0) { t->tipo = F_FIM; return; }
        if (pular_espacos()) {             /* fin de la línea de la directiva */
            t->tipo = F_FIM_LINHA;
            t->arq = F()->nome;
            t->linha = F()->linha;
            return;
        }
        if (!car(0)) {
            if (em_diretiva) { t->tipo = F_FIM_LINHA; return; }
            if (ncond > F()->nivel_cond) { arq_erro = F()->nome; linha_erro = F()->linha; falhar("falta #endif"); }
            nfontes--;
            inicio_de_linha = true;
            continue;
        }
        if (car(0) == '#' && inicio_de_linha && !em_diretiva) {
            F()->pos++;
            diretiva();
            continue;
        }
        break;
    }
    fonte_t *f = F();
    inicio_de_linha = false;
    t->arq = f->nome;
    t->linha = f->linha;
    t->nao_expandir = NULL;
    t->sem_sinal = false;
    arq_erro = f->nome;
    linha_erro = f->linha;
    int c = car(0);
    if (eh_letra(c)) {
        int ini = f->pos;
        while (eh_letra(car(0)) || eh_digito(car(0))) f->pos++;
        t->tipo = F_NOME;
        t->nome = nome_de(f->texto + ini, f->pos - ini);
        if (t->nome->reservada && !em_diretiva) t->tipo = t->nome->reservada;
        return;
    }
    if (eh_digito(c) || (c == '.' && eh_digito(car(1)))) {
        if (eh_numero_real()) {
            t->tipo = F_REAL;
            t->real = ler_real(&t->real_float);
            if (eh_letra(car(0)) || car(0) == '.') falhar("numero invalido");
            return;
        }
        t->longo = false;      /* el número es de 32 bits, salvo que diga "ll" (D128) */
        t->val_hi = 0;
        uint32_t v = 0, vhi = 0;
        bool grande = false;
        /* El número se acumula en dos palabras de 32 bits (D128). */
        #define ACUMULA(base)                                                          \
            do {                                                                       \
                multiplicar64(&vhi, &v, (uint32_t)(base), (uint32_t)digito);            \
                f->pos++;                                                             \
            } while (0)
        if (c == '0' && (car(1) == 'x' || car(1) == 'X')) {
            f->pos += 2;
            for (;;) {
                int digito = car(0);
                if (digito >= '0' && digito <= '9') digito -= '0';
                else if (digito >= 'a' && digito <= 'f') digito -= 'a' - 10;
                else if (digito >= 'A' && digito <= 'F') digito -= 'A' - 10;
                else break;
                ACUMULA(16);
            }
            grande = v > 0x7FFFFFFFu || vhi != 0;
        } else if (c == '0') {
            while (car(0) >= '0' && car(0) <= '7') {
                int digito = car(0) - '0';
                ACUMULA(8);
            }
            grande = v > 0x7FFFFFFFu || vhi != 0;
        } else {
            while (eh_digito(car(0))) {
                int digito = car(0) - '0';
                ACUMULA(10);
            }
            grande = v > 0x7FFFFFFFu || vhi != 0;
        }
        #undef ACUMULA
        t->sem_sinal = grande;
        int ls = 0;
        bool sem_sinal_u = false;
        while (car(0) == 'u' || car(0) == 'U' || car(0) == 'l' || car(0) == 'L') {
            if (car(0) == 'u' || car(0) == 'U') sem_sinal_u = true;
            else ls++;
            f->pos++;
        }
        if (ls > 1) {          /* sufijo ll: el número es de 64 bits (D128) */
            t->longo = true;
            t->val_hi = vhi;
            t->sem_sinal = sem_sinal_u;   /* con signo, salvo que diga "u" */
        } else if (sem_sinal_u) {
            t->sem_sinal = true;
        }
        if (eh_letra(car(0)) || car(0) == '.') falhar("numero invalido");
        t->tipo = F_NUM;
        t->val = (int)v;
        return;
    }
    if (c == '\'') {
        f->pos++;
        t->tipo = F_NUM;
        t->val = (signed char)ler_escape();
        if (car(0) != '\'') falhar("caracter sin cerrar");
        f->pos++;
        return;
    }
    if (c == '"') {
        f->pos++;
        int cap = 32, n = 0;
        char *b = memoria(cap);
        while (car(0) != '"') {
            if (!car(0) || car(0) == '\n') falhar("texto sin cerrar");
            if (n + 2 >= cap) {
                char *novo = memoria(cap * 2);
                memcpy(novo, b, n);
                libera(b);
                b = novo;
                cap *= 2;
            }
            b[n++] = (char)ler_escape();
        }
        f->pos++;
        b[n] = 0;
        t->tipo = F_TEXTO;
        t->val = novo_literal(b, n);
        return;
    }
    /* Puntuación: las más largas primero. */
    static const struct { const char *s; int f; } ops[] = {
        {"<<=", F_SHL_AT}, {">>=", F_SHR_AT}, {"...", F_RETICENCIAS},
        {"==", F_IGUAL}, {"!=", F_DIF}, {"<=", F_MENOR_IG}, {">=", F_MAIOR_IG}, {"&&", F_E}, {"||", F_OU},
        {"++", F_MAISMAIS}, {"--", F_MENOSMENOS}, {"->", F_SETA}, {"<<", F_SHL}, {">>", F_SHR},
        {"+=", F_MAIS_AT}, {"-=", F_MENOS_AT}, {"*=", F_VEZES_AT}, {"/=", F_DIV_AT}, {"%=", F_MOD_AT},
        {"&=", F_E_AT}, {"|=", F_OU_AT}, {"^=", F_XOU_AT},
    };
    for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        int n = (int)tam_texto(ops[i].s);
        if (f->pos + n <= f->n && memcmp(f->texto + f->pos, ops[i].s, n) == 0) {
            f->pos += n;
            t->tipo = ops[i].f;
            return;
        }
    }
    if (acha_char("+-*/%&|^~!<>=?:;,.()[]{}#", c)) {
        f->pos++;
        t->tipo = c;
        return;
    }
    falhar("caracter inesperado '%c'", c);
}

/* ---- macros ---- */

typedef struct macro {
    nome_t   *nome;
    bool      funcional;
    int       nparams;
    nome_t  **params;
    ficha_t  *corpo;
    int       ncorpo;
} macro_t;

/* Fichas devueltas para leerse de nuevo (expansiones de macro). */
static ficha_t *pendentes;
static int npendentes, cappendentes;

static void devolver(const ficha_t *t)
{
    garantir((void **)&pendentes, &cappendentes, npendentes + 1, sizeof(ficha_t));
    pendentes[npendentes++] = *t;
}

static void lex_proxima(ficha_t *t)
{
    if (npendentes) *t = pendentes[--npendentes];
    else lex_bruto(t);
}

static void definir_macro_texto(const char *nome, const char *corpo);

static int valor_cond(void);

static void pular_linha(void)
{
    while (car(0) && car(0) != '\n') {
        if (car(0) == '\\' && car(1) == '\n') { F()->pos++; F()->linha++; }
        F()->pos++;
    }
}

/* Salta una rama falsa de #if hasta el #else/#endif del mismo nivel. Devuelve
 * true si se detuvo en un #else (o #elif verdadero) que debe compilarse. */
static bool pular_ramo(void)
{
    int nivel = 0;
    for (;;) {
        /* comienzo de una línea */
        while (car(0) == ' ' || car(0) == '\t') F()->pos++;
        if (!car(0)) falhar("falta #endif");
        if (car(0) == '#') {
            F()->pos++;
            while (car(0) == ' ' || car(0) == '\t') F()->pos++;
            int ini = F()->pos;
            while (eh_letra(car(0))) F()->pos++;
            const char *p = F()->texto + ini;
            int n = F()->pos - ini;
            if ((n == 2 && !memcmp(p, "if", 2)) || (n == 5 && !memcmp(p, "ifdef", 5)) || (n == 6 && !memcmp(p, "ifndef", 6))) {
                nivel++;
            } else if (n == 5 && !memcmp(p, "endif", 5)) {
                if (nivel-- == 0) { pular_linha(); return false; }
            } else if (nivel == 0 && n == 4 && !memcmp(p, "else", 4)) {
                pular_linha();
                return true;
            } else if (nivel == 0 && n == 4 && !memcmp(p, "elif", 4)) {
                if (!cond_ja_valeu[ncond - 1]) {
                    if (valor_cond()) return true;
                    continue;   /* valor_cond ya consumió la línea */
                }
            }
        }
        pular_linha();
        if (car(0) == '\n') { F()->pos++; F()->linha++; }
    }
}

/* Lee las fichas hasta el fin de la línea de la directiva (sin expandir macros). */
static ficha_t *ler_linha_diretiva(int *n)
{
    ficha_t *v = NULL;
    int cap = 0;
    *n = 0;
    em_diretiva = true;
    for (;;) {
        ficha_t t;
        lex_bruto(&t);
        if (t.tipo == F_FIM_LINHA || t.tipo == F_FIM) break;
        garantir((void **)&v, &cap, *n + 1, sizeof(ficha_t));
        v[(*n)++] = t;
    }
    em_diretiva = false;
    return v;
}

static int avaliar_fichas(ficha_t *v, int n);

/* Condición de #if/#elif: expresión constante, con defined(NOMBRE). */
static int valor_cond(void)
{
    int n;
    ficha_t *v = ler_linha_diretiva(&n);
    /* Cambia defined X / defined(X) por 0 o 1 y las macros simples por su valor. */
    ficha_t *w = memoria(sizeof(ficha_t) * (n + 1) * 4 + sizeof(ficha_t));
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (v[i].tipo == F_NOME && !compara_texto(v[i].nome->s, "defined")) {
            int j = i + 1;
            bool par = j < n && v[j].tipo == '(';
            if (par) j++;
            if (j >= n || v[j].tipo != F_NOME) falhar("defined sin nombre");
            w[m] = v[j];
            w[m].tipo = F_NUM;
            w[m++].val = v[j].nome->macro != NULL;
            i = j + (par ? 1 : 0);
        } else if (v[i].tipo == F_NOME && v[i].nome->macro && !v[i].nome->macro->funcional &&
                   v[i].nome->macro->ncorpo == 1 && v[i].nome->macro->corpo[0].tipo == F_NUM) {
            w[m] = v[i].nome->macro->corpo[0];
            m++;
        } else if (v[i].tipo == F_NOME) {
            w[m] = v[i];
            w[m].tipo = F_NUM;
            w[m++].val = 0;   /* un nombre desconocido vale 0, como en C */
        } else {
            w[m++] = v[i];
        }
    }
    w[m].tipo = F_FIM;
    int r = avaliar_fichas(w, m);
    libera(w);
    if (v) libera(v);
    if (car(0) == '\n') { F()->pos++; F()->linha++; }
    inicio_de_linha = true;
    return r;
}

static void diretiva(void)
{
    while (car(0) == ' ' || car(0) == '\t') F()->pos++;
    int ini = F()->pos;
    while (eh_letra(car(0))) F()->pos++;
    const char *p = F()->texto + ini;
    int n = F()->pos - ini;
    arq_erro = F()->nome;
    linha_erro = F()->linha;
#define EH(s) (n == (int)sizeof(s) - 1 && !memcmp(p, s, n))
    if (n == 0) {                                  /* "#" solo */
        pular_linha();
    } else if (EH("define")) {
        while (car(0) == ' ' || car(0) == '\t') F()->pos++;
        int i = F()->pos;
        while (eh_letra(car(0)) || eh_digito(car(0))) F()->pos++;
        if (i == F()->pos) falhar("#define sin nombre");
        macro_t *m = memoria(sizeof *m);
        m->nome = nome_de(F()->texto + i, F()->pos - i);
        if (car(0) == '(') {                       /* con parámetros: sin espacio antes del ( */
            F()->pos++;
            m->funcional = true;
            m->params = memoria(sizeof(nome_t *) * 16);
            em_diretiva = true;
            for (;;) {
                ficha_t t;
                lex_bruto(&t);
                if (t.tipo == ')' && m->nparams == 0) break;
                if (t.tipo != F_NOME) falhar("parametro de macro invalido");
                if (m->nparams == 16) falhar("macro con demasiados parametros");
                m->params[m->nparams++] = t.nome;
                lex_bruto(&t);
                if (t.tipo == ')') break;
                if (t.tipo != ',') falhar("se esperaba , o ) en la macro");
            }
            em_diretiva = false;
        }
        m->corpo = ler_linha_diretiva(&m->ncorpo);
        m->nome->macro = m;
    } else if (EH("undef")) {
        int k;
        ficha_t *v = ler_linha_diretiva(&k);
        if (k >= 1 && v[0].tipo == F_NOME) v[0].nome->macro = NULL;
    } else if (EH("include")) {
        while (car(0) == ' ' || car(0) == '\t') F()->pos++;
        if (car(0) == '"') {
            F()->pos++;
            int i = F()->pos;
            while (car(0) && car(0) != '"' && car(0) != '\n') F()->pos++;
            char *nome = duplicar_texto(F()->texto + i, F()->pos - i);
            pular_linha();
            /* Relativo a la carpeta del archivo que incluye. */
            char caminho[256];
            const char *barra = acha_ultimo_char(F()->nome, '/');
            if (nome[0] != '/' && barra) {
                int k = (int)(barra - F()->nome) + 1;
                if (k > 200) k = 200;
                memcpy(caminho, F()->nome, k);
                copia_texto(caminho + k, nome, sizeof caminho - k);
            } else {
                copia_texto(caminho, nome, sizeof caminho);
            }
            int tam;
            char *texto = ler_arquivo(caminho, &tam);
            if (!texto) falhar("no se puede abrir \"%s\"", caminho);
            abrir_fonte(duplicar_texto(caminho, (int)tam_texto(caminho)), texto, tam);
            return;
        }
        pular_linha();   /* <stdio.h> etc.: la biblioteca ya está incorporada */
    } else if (EH("ifdef") || EH("ifndef")) {
        int k;
        ficha_t *v = ler_linha_diretiva(&k);
        if (k < 1 || v[0].tipo != F_NOME) falhar("#ifdef sin nombre");
        bool def = v[0].nome->macro != NULL;
        bool vale = EH("ifdef") ? def : !def;
        if (ncond == MAX_COND) falhar("#if anidados demasiado");
        cond_ja_valeu[ncond++] = vale;
        if (!vale) {
            if (pular_ramo()) cond_ja_valeu[ncond - 1] = true;
            else ncond--;   /* saltó hasta el #endif */
        }
    } else if (EH("if")) {
        if (ncond == MAX_COND) falhar("#if anidados demasiado");
        bool vale = valor_cond() != 0;
        cond_ja_valeu[ncond++] = vale;
        if (!vale) {
            if (pular_ramo()) cond_ja_valeu[ncond - 1] = true;
            else ncond--;   /* saltó hasta el #endif */
        }
        return;
    } else if (EH("else") || EH("elif")) {
        if (ncond == 0) falhar("#else sin #if");
        pular_linha();
        /* Estábamos en la rama verdadera: salta el resto hasta el #endif. */
        while (pular_ramo()) ;
        ncond--;
    } else if (EH("endif")) {
        if (ncond == 0) falhar("#endif sin #if");
        ncond--;
        pular_linha();
    } else if (EH("pragma") || EH("line")) {
        pular_linha();
    } else if (EH("error")) {
        int i = F()->pos;
        pular_linha();
        falhar("#error%s", duplicar_texto(F()->texto + i, F()->pos - i));
    } else {
        falhar("directiva desconocida #%s", duplicar_texto(p, n));
    }
#undef EH
    inicio_de_linha = true;
}

static void definir_macro_texto(const char *nome, const char *corpo)
{
    char *t = memoria(tam_texto(nome) + tam_texto(corpo) + 16);
    formatar_texto(t, tam_texto(nome) + tam_texto(corpo) + 16, "#define %s %s\n", nome, corpo);
    abrir_fonte("<cc>", t, (int)tam_texto(t));
    ficha_t f;
    lex_bruto(&f);   /* procesa la directiva; la fuente termina y se cierra */
    if (f.tipo != F_FIM) devolver(&f);
}

/* Siguiente ficha con las macros expandidas. */
static void lex_expandida(ficha_t *t)
{
    for (;;) {
        lex_proxima(t);
        if (t->tipo == F_NOME && !compara_texto(t->nome->s, "__LINE__")) {
            t->tipo = F_NUM;
            t->val = t->linha;
            return;
        }
        if (t->tipo != F_NOME || !t->nome->macro || t->nao_expandir == t->nome) return;
        macro_t *m = t->nome->macro;
        ficha_t *args[16];
        int nargs[16], cargs[16];
        if (m->funcional) {
            ficha_t u;
            lex_proxima(&u);
            if (u.tipo != '(') { devolver(&u); return; }
            int k = 0, nivel = 0;
            for (int i = 0; i < 16; i++) args[i] = NULL, nargs[i] = cargs[i] = 0;
            for (;;) {
                lex_proxima(&u);
                if (u.tipo == F_FIM) falhar("fin del archivo dentro de la macro %s", m->nome->s);
                if (nivel == 0 && u.tipo == ')') break;
                if (nivel == 0 && u.tipo == ',') {
                    if (++k >= 16) falhar("macro con demasiados argumentos");
                    continue;
                }
                if (u.tipo == '(') nivel++;
                if (u.tipo == ')') nivel--;
                garantir((void **)&args[k], &cargs[k], nargs[k] + 1, sizeof(ficha_t));
                args[k][nargs[k]++] = u;
            }
            if (m->nparams != k + 1 && !(m->nparams == 0 && k == 0 && nargs[0] == 0))
                falhar("la macro %s espera %d argumentos", m->nome->s, m->nparams);
        }
        /* Arma la expansión y devuelve de atrás para adelante. */
        for (int i = m->ncorpo - 1; i >= 0; i--) {
            ficha_t c = m->corpo[i];
            int p = -1;
            if (c.tipo == F_NOME)
                for (int j = 0; j < m->nparams; j++)
                    if (m->params[j] == c.nome) p = j;
            if (p >= 0) {
                for (int j = nargs[p] - 1; j >= 0; j--) devolver(&args[p][j]);
            } else {
                c.linha = t->linha;
                c.arq = t->arq;
                if (c.tipo == F_NOME && c.nome == m->nome) c.nao_expandir = m->nome;
                devolver(&c);
            }
        }
        if (m->funcional)
            for (int i = 0; i < 16; i++) if (args[i]) libera(args[i]);
    }
}

/* Todas las fichas del programa, ya preprocesadas. */
static ficha_t *fichas;
static int nfichas, capfichas;
static int pf;                       /* ficha actual */

/* Unidades de traducción (D133): cada fichero de origen se lee por
 * separado y guarda sus fichas seguidas; al final compila cada unidad con
 * su propio ámbito de nombres "static". */
#define MAX_UNIDADES 32
typedef struct { int inicio, fim; } unidade_t;
static unidade_t unidades[MAX_UNIDADES];
static int nunidades;

static void ler_unidade(void)
{
    if (nunidades == MAX_UNIDADES) falhar("demasiados ficheros de origem");
    int inicio = nfichas;
    for (;;) {
        ficha_t t = {0};
        lex_expandida(&t);
        if (t.tipo == F_NOME && t.nome->reservada) t.tipo = t.nome->reservada;
        garantir((void **)&fichas, &capfichas, nfichas + 1, sizeof(ficha_t));
        fichas[nfichas++] = t;
        if (t.tipo == F_FIM) break;
    }
    unidades[nunidades].inicio = inicio;
    unidades[nunidades].fim = nfichas - 1;   /* la ficha del fin del fichero */
    nunidades++;
}

/* El ámbito de la unidad (los "static" y las etiquetas de struct) vale
 * sólo hasta el final del fichero: se limpia al empezar cada unidad. */
static void limpar_unidade(void)
{
    for (int h = 0; h < HASH_NOMES; h++)
        for (nome_t *x = nomes[h]; x; x = x->prox) { x->unit = NULL; x->etiqueta = NULL; }
}

/* ------------------------------------------------------------ tipos */

enum { T_VOID, T_CHAR, T_UCHAR, T_SHORT, T_USHORT, T_INT, T_UINT, T_LONGLONG, T_ULONGLONG, T_FLOAT, T_DOUBLE, T_PTR, T_VETOR, T_STRUCT, T_FUNC };

typedef struct membro {
    nome_t        *nome;
    struct tipo   *t;
    int            desloc;
    struct membro *prox;
} membro_t;

typedef struct tipo {
    int           k;
    int           tam, alin;
    struct tipo  *base;      /* T_PTR, T_VETOR: el elemento; T_FUNC: el retorno */
    int           n;         /* T_VETOR: elementos (-1 = aún no se sabe); T_FUNC: parámetros (-1 = sin prototipo) */
    bool          variadica;
    bool          completo;  /* struct ya definida */
    bool          uniao;
    membro_t     *membros;
    struct tipo  *ptr;       /* caché de "puntero a este tipo" */
    struct tipo **params;
} tipo_t;

static tipo_t t_void = {T_VOID, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_char = {T_CHAR, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_uchar = {T_UCHAR, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_short = {T_SHORT, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_ushort = {T_USHORT, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_int = {T_INT, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_uint = {T_UINT, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};
/* long long (D128): 8 bytes, alineado a 4 como en el ABI i386. El valor va en
 * EDX:EAX (alto: bajo), como en el ABI de los enteros de 64 bits. */
static tipo_t t_longlong = {T_LONGLONG, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_ulonglong = {T_ULONGLONG, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0};
/* double alineado a 4, como en el ABI i386 (struct {char c; double d;} ocupa 12 bytes). */
static tipo_t t_float = {T_FLOAT, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0};
static tipo_t t_double = {T_DOUBLE, 8, 4, 0, 0, 0, 0, 0, 0, 0, 0};

static tipo_t *ponteiro(tipo_t *base)
{
    if (!base->ptr) {
        tipo_t *t = memoria(sizeof *t);
        t->k = T_PTR;
        t->tam = t->alin = 4;
        t->base = base;
        base->ptr = t;
    }
    return base->ptr;
}

static tipo_t *vetor(tipo_t *base, int n)
{
    tipo_t *t = memoria(sizeof *t);
    t->k = T_VETOR;
    t->base = base;
    t->n = n;
    t->tam = n < 0 ? 0 : base->tam * n;
    t->alin = base->alin;
    return t;
}

static bool eh_inteiro(tipo_t *t) { return t->k >= T_CHAR && t->k <= T_ULONGLONG; }
static bool eh_real(tipo_t *t) { return t->k == T_FLOAT || t->k == T_DOUBLE; }
static bool eh_escalar(tipo_t *t) { return eh_inteiro(t) || eh_real(t) || t->k == T_PTR; }
/* Enteros de 64 bits (D128). */
static bool eh_longlong(tipo_t *t) { return t->k == T_LONGLONG || t->k == T_ULONGLONG; }
static bool eh_ll_sem_sinal(tipo_t *t) { return t->k == T_ULONGLONG; }

/* Vector y función, usados como valor, se convierten en puntero. */
static tipo_t *decair(tipo_t *t)
{
    if (t->k == T_VETOR) return ponteiro(t->base);
    if (t->k == T_FUNC) return ponteiro(t);
    return t;
}

/* Tamaño de lo que apunta un puntero (para la aritmética). */
static int passo(tipo_t *t)
{
    tipo_t *b = decair(t)->base;
    if (b->k == T_VOID || b->k == T_FUNC) return 1;
    if (b->k == T_STRUCT && !b->completo) falhar("struct incompleta");
    return b->tam;
}

/* ------------------------------------------------------------ símbolos */

enum { S_GLOBAL, S_LOCAL, S_FUNC, S_CONST, S_TIPO };

typedef struct simbolo {
    nome_t *nome;
    int     classe;
    tipo_t *t;
    int     valor;          /* dirección (global), desplazamiento en EBP (local), posición en el código (función), valor (constante) */
    bool    definida;
    bool    da_biblioteca;
    int     chamadas;       /* lista de "call" esperando a la función */
    int     absolutos;      /* lista de direcciones absolutas esperando a la función */
    bool    usada;
} simbolo_t;

#define MAX_LOCAIS 1024
static simbolo_t *locais[MAX_LOCAIS];
static int nlocais;

/* Ambitos dos simbolos (D133): E_LOCAL dentro de uma funcao, E_UNIDADE so na
 * sua unidade de traducao (o "static" do C) e E_GLOBAL em todas (o que se
 * liga entre ficheiros). */
enum { E_LOCAL, E_UNIDADE, E_GLOBAL };

static simbolo_t *procurar(nome_t *n)
{
    for (int i = nlocais - 1; i >= 0; i--)
        if (locais[i]->nome == n) return locais[i];
    if (n->unit) return n->unit;
    return n->global;
}

static simbolo_t *novo_simbolo(nome_t *n, int classe, tipo_t *t, int ambito)
{
    simbolo_t *s = memoria(sizeof *s);
    s->nome = n;
    s->classe = classe;
    s->t = t;
    s->chamadas = s->absolutos = -1;
    s->da_biblioteca = compilando_biblioteca;
    if (ambito == E_LOCAL) {
        if (nlocais == MAX_LOCAIS) falhar("demasiadas variables locales");
        locais[nlocais++] = s;
    } else if (ambito == E_UNIDADE) {
        n->unit = s;
    } else {
        n->global = s;
    }
    return s;
}

/* ------------------------------------------------------------ código */

/* La carga del último lvalue (ver el comentario del principio del archivo). */
static int carga_ini = -1, carga_fim = -1;
/* Punto flotante (ver más abajo): el fin del "fld [ecx]" después de grabar
 * un real, y el último operando real cargado directo de la memoria. */
static int recarga_ini = -1, recarga_fim = -1;
static int simples_ini = -1, simples_fim = -1;
/* Variable local ([ebp+d], modrm 0x85) o global ([d], modrm 0x05) leída con
 * una sola instrucción ("mov eax, [ebp+d]"). exigir_endereco() cambia la carga
 * por la dirección ("lea eax, [ebp+d]") y anota dónde está, para que "x = ...",
 * "x += ..." y "x++" graben directo en la memoria. */
static bool carga_direta;
static int direta_modrm;
static uint32_t direta_disp;
static int endereco_ini = -1, endereco_fim = -1;
/* Última constante entera: "mov eax, valor" en [const_ini, const_fim). */
static int const_ini = -1, const_fim = -1, const_val;

/* Optimizaciones (D108). "x++" como instrucción: la carga del valor antiguo/nuevo
 * (inc_ld_ini, inc_ld_len) sobra y sale en descartar(). "&&"/"||" que acaba
 * de montarse (0/1 en EAX): los huecos de los saltos y donde empieza el
 * "mov eax, 0/1", para que una condición salte directo (saltar_se_falso).
 * Dirección de vector/struct de variable (mov eax, C / lea eax, [ebp+d]) para
 * que "v[i]" y "s.m" usen la dirección en el propio acceso. */
static int inc_fim = -1, inc_ld_ini, inc_ld_len;
static int base_ini = -1, base_fim = -1, base_modrm;
static uint32_t base_disp;

/* El código cambió de forma que las anotaciones de arriba no acompañan (se borró
 * un trozo, o un salto llega aquí): olvida todas. */
static void invalidar_ultimas(void)
{
    simples_fim = recarga_fim = const_fim = comparacao_fim = endereco_fim = -1;
    inc_fim = logico_fim = base_fim = -1;
}

static void remendar_aqui(int lista)
{
    remendar(lista, ncod);
    carga_fim = -1;   /* alguien salta aquí: la última instrucción ya no es "sólo" una carga */
    invalidar_ultimas();
}

static int op_carga(tipo_t *t);

static int pre_t(tipo_t *t) { return t->k == T_FLOAT ? 0xF3 : 0xF2; }

/* EAX = valor en [EAX] (modrm 0x00) o [ECX] (modrm 0x01). */
static void carregar_de(tipo_t *t, int modrm)
{
    switch (t->k) {
    case T_CHAR:   e3(0x0F, 0xBE, modrm); break;
    case T_UCHAR:  e3(0x0F, 0xB6, modrm); break;
    case T_SHORT:  e3(0x0F, 0xBF, modrm); break;
    case T_USHORT: e3(0x0F, 0xB7, modrm); break;
    case T_LONGLONG: case T_ULONGLONG:   /* EDX:EAX = [mem] (D128) */
        /* Sólo con la dirección en un registro (EAX: 0x00, ECX: 0x01): las dos
         * mitades se leen de la misma dirección. */
        if (modrm != 0x00 && modrm != 0x01) falhar("carga de 64 bits con direccióndirecta no soportada");
        e3(0x8B, 0x50 | (modrm & 1), 0x04);   /* mov edx, [eax+4] / [ecx+4] */
        e2(0x8B, modrm);                      /* mov eax, [eax] / [ecx] */
        break;
    case T_FLOAT: case T_DOUBLE:
        if (usar_sse) sse(pre_t(t), 0x10, modrm);   /* movss/movsd xmm0, [eax] o [ecx] */
        else f2(op_carga(t), modrm);                /* fld [eax] o [ecx] */
        break;
    default:       e2(0x8B, modrm); break;
    }
}

/* Dirección en EAX -> valor en EAX, si el tipo es escalar. */
static void carregar(tipo_t *t)
{
    if (!eh_escalar(t)) return;
    carga_ini = ncod;
    carregar_de(t, 0x00);
    carga_fim = ncod;
    carga_direta = false;
}

static void emitir_endereco(int modrm, uint32_t disp)
{
    if (modrm == 0x85) { e2(0x8D, 0x85); ed(disp); }   /* lea eax, [ebp+d] */
    else mov_eax(disp);
}

/* [mem] = valor actual (EAX o ST(0)); un real sigue en ST(0). */
static void guardar_mem(tipo_t *t, int modrm, uint32_t disp)
{
    switch (t->k) {
    case T_CHAR: case T_UCHAR:   e2(0x88, modrm); break;
    case T_SHORT: case T_USHORT: e3(0x66, 0x89, modrm); break;
    case T_LONGLONG: case T_ULONGLONG:   /* [mem] = EDX:EAX (D128) */
        e2(0x89, modrm); ed(disp);            /* mov [mem], eax */
        e2(0x89, modrm | 0x10); ed(disp + 4); /* mov [mem+4], edx */
        return;
    case T_FLOAT: case T_DOUBLE:
        if (usar_sse) { sse(pre_t(t), 0x11, modrm); ed(disp); return; }   /* movsd [mem], xmm0 */
        f2(op_carga(t), modrm | 0x18); ed(disp);           /* fstp [mem] */
        recarga_ini = ncod;
        f2(op_carga(t), modrm); ed(disp);                  /* fld [mem] */
        recarga_fim = ncod;
        return;
    default:                     e2(0x89, modrm); break;
    }
    ed(disp);
}

/* [ECX] = EAX (reg 0) o EDX (reg 2). */
static void guardar(tipo_t *t, int reg)
{
    int modrm = 0x01 | reg << 3;
    switch (t->k) {
    case T_CHAR: case T_UCHAR:   e2(0x88, modrm); break;
    case T_SHORT: case T_USHORT: e3(0x66, 0x89, modrm); break;
    case T_LONGLONG: case T_ULONGLONG:   /* [ECX] = EDX:EAX (D128) */
        e2(0x89, 0x01);            /* mov [ecx], eax */
        e3(0x89, 0x51, 0x04);      /* mov [ecx+4], edx */
        break;
    case T_STRUCT: case T_VETOR: falhar("asignacion de este tipo no soportada");
    case T_FLOAT: case T_DOUBLE:   /* fstp [ecx] y fld [ecx]: el valor grabado (ya redondeado) sigue en ST(0) */
        if (usar_sse) { sse(pre_t(t), 0x11, 0x01); break; }   /* movsd [ecx], xmm0 */
        f2(op_carga(t), 0x19);
        recarga_ini = ncod;
        f2(op_carga(t), 0x01);
        recarga_fim = ncod;
        break;
    default:                     e2(0x89, modrm); break;
    }
}

/* Ajusta EAX al tipo (corta y extiende el signo de los tipos pequeños). */
static void converter(tipo_t *para)
{
    switch (para->k) {
    case T_CHAR:   e3(0x0F, 0xBE, 0xC0); break;
    case T_UCHAR:  e3(0x0F, 0xB6, 0xC0); break;
    case T_SHORT:  e3(0x0F, 0xBF, 0xC0); break;
    case T_USHORT: e3(0x0F, 0xB7, 0xC0); break;
    case T_LONGLONG:   /* el valor de 32 bits en EAX -> EDX:EAX con signo (D128) */
        e2(0x89, 0xC2);           /* mov edx, eax */
        e3(0xC1, 0xFA, 0x1F);     /* sar edx, 31 */
        break;
    case T_ULONGLONG:
        e2(0x31, 0xD2);           /* xor edx, edx */
        break;
    default: break;
    }
}

/* Multiplica EAX (reg 0) o ECX (reg 1) por n. */
static void escalar(int reg, int n)
{
    if (n == 1) return;
    for (int k = 1; k < 31; k++)
        if (n == 1 << k) { e3(0xC1, 0xE0 | reg, k); return; }
    e2(0x69, 0xC0 | reg << 3 | reg);
    e4((uint32_t)n);
}

/* ---- punto flotante (D039) ----
 *
 * SSE2 (predeterminado): un valor float/double queda en XMM0 (float como float, ss;
 * double como sd). El operando izquierdo real de una operación va a XMM1,
 * XMM2... (uno por nivel, hasta XMM6); el derecho se calcula en XMM0. XMM7 es
 * borrador. Retorno de función en XMM0.
 *
 * x87 (cc -x87): el valor queda en ST(0); la pila de la x87 queda vacía entre
 * instrucciones y en cada llamada de función. El operando izquierdo real queda en
 * la propia pila de la x87 (el derecho se calcula encima), hasta 4 niveles.
 * Retorno en ST(0), como en el ABI i386.
 *
 * En ambos, más allá del límite el izquierdo va a la pila del
 * procesador (8 bytes), y una llamada de función en medio de una expresión
 * guarda en memoria los operandos en espera y los devuelve después. */

static int passada;                  /* 1: sólo descubre las funciones usadas; 2: genera el programa */
static bool cabecalho_biblioteca;     /* a unidade atual e' o cabecalho da biblioteca (D133) */
static simbolo_t *funcao_atual;
static int x87_guardados;            /* operandos izquierdos en espera en la x87 o en XMM1.. */
#define X87_MAX_GUARDADOS 4
#define SSE_MAX_GUARDADOS 6
enum { ESQ_INTEIRO, ESQ_MEMORIA, ESQ_REG, ESQ_LONGLONG };   /* dónde salvar_esquerdo() puso el operando */

/* Último operando "simple" (variable o constante real cargada directo
 * de la memoria): una operación con él se convierte en "fadd qword [ebp+d]" etc. */
static int simples_modrm;
static uint32_t simples_disp;
static tipo_t *simples_t;


/* Opcode de la x87 para un operando de memoria: D9/DD (fld, fst, fstp) o
 * D8/DC (fadd, fmul, fsub, fdiv). */
static int op_carga(tipo_t *t) { return t->k == T_FLOAT ? 0xD9 : 0xDD; }
static int op_conta(tipo_t *t) { return t->k == T_FLOAT ? 0xD8 : 0xDC; }

/* EAX (entero del tipo t) -> ST(0). */
static void marcar_simples(int ini, int modrm, uint32_t disp);
static tipo_t *ty;

static void ll_para_real(void);
static void real_para_ll(void);

static void inteiro_para_real(tipo_t *t)
{
    t = decair(t);
    if (!eh_inteiro(t)) falhar("se esperaba un numero");
    if (eh_longlong(t)) {          /* D128: lo hace la biblioteca */
        ll_para_real();
        ty = &t_double;
        carga_fim = -1;
        invalidar_ultimas();
        return;
    }
    if (ncod == const_fim) {            /* constante: ya se convierte a double aquí */
        double v = t->k == T_UINT ? (double)(uint32_t)const_val : (double)const_val;
        ncod = const_ini;
        invalidar_ultimas();
        uint32_t a = endereco_constante(v, false);
        int ini = ncod;
        if (usar_sse) sse(0xF2, 0x10, 0x05);   /* movsd xmm0, [constante] */
        else f2(0xDD, 0x05);                   /* fld qword [constante] */
        e4(a);
        ty = &t_double;
        marcar_simples(ini, 0x05, a);
        return;
    }
    if (t->k == T_UINT) {
        e2(0x6A, 0x00);                 /* push 0 */
        push_eax();
        f3(0xDF, 0x2C, 0x24);           /* fild qword [esp] */
        if (usar_sse) {
            f3(0xDD, 0x1C, 0x24);       /* fstp qword [esp] */
            sse(0xF2, 0x10, 0x04); e1(0x24);   /* movsd xmm0, [esp] */
        }
        esp_mais(8);
    } else if (usar_sse) {
        sse(0xF2, 0x2A, 0xC0);          /* cvtsi2sd xmm0, eax */
    } else {
        push_eax();
        f3(0xDB, 0x04, 0x24);           /* fild dword [esp] */
        esp_mais(4);
    }
}

/* Real -> EAX, cortando la parte fraccionaria (como manda C). SSE2:
 * cvttsd2si; para unsigned, valores de 2^31 a 2^32 pasan por x - 2^32.
 * x87: una rutina al final del código, la misma para int y unsigned (fistp de
 * 64 bits con el redondeo cambiado a "cortar"). */
static void real_para_inteiro(tipo_t *de, tipo_t *para)
{
    if (eh_longlong(para)) {       /* D128: lo hace la biblioteca */
        real_para_ll();
        ty = para;
        return;
    }
    if (usar_sse) {
        if (de->k == T_FLOAT && para->k != T_UINT) {
            sse(0xF3, 0x2C, 0xC0);      /* cvttss2si eax, xmm0 */
            converter(para);
            return;
        }
        if (de->k == T_FLOAT) sse(0xF3, 0x5A, XMM(0, 0));   /* cvtss2sd xmm0, xmm0 */
        sse(0xF2, 0x2C, 0xC0);          /* cvttsd2si eax, xmm0 */
        if (para->k == T_UINT) {
            sse(0, 0x28, XMM(7, 0));    /* movaps xmm7, xmm0 */
            sse(0xF2, 0x5C, 0x3D); e4(endereco_constante(4294967296.0, false));   /* subsd xmm7, [2^32] */
            sse(0xF2, 0x2C, XMM(1, 7)); /* cvttsd2si ecx, xmm7 */
            e1(0x3D); e4(0x80000000u);  /* cmp eax, 0x80000000 (fuera del int) */
            e3(0x0F, 0x44, 0xC1);       /* cmove eax, ecx */
        }
        converter(para);
        return;
    }
    chamar_para_inteiro();
    converter(para);
}

/* Redondea ST(0) a float (pasa por la memoria). En SSE2 el valor ya es
 * float (converter_valor hizo cvtsd2ss). */
static void arredondar_float(void)
{
    if (usar_sse) return;
    esp_menos(4);
    f3(0xD9, 0x1C, 0x24);               /* fstp dword [esp] */
    f3(0xD9, 0x04, 0x24);               /* fld dword [esp] */
    esp_mais(4);
}

/* Convierte el valor actual (tipo "de", en EAX o ST(0)) al tipo "para". */
static void converter_valor(tipo_t *de, tipo_t *para)
{
    de = decair(de);
    if (eh_real(para)) {
        if (!eh_real(de)) { inteiro_para_real(de); de = &t_double; }
        if (usar_sse && de->k != para->k)
            sse(para->k == T_FLOAT ? 0xF2 : 0xF3, 0x5A, XMM(0, 0));   /* cvtsd2ss / cvtss2sd xmm0, xmm0 */
    } else if (eh_real(de)) {
        if (!eh_inteiro(para)) falhar("no se puede convertir float/double a este tipo");
        real_para_inteiro(de, para);
    } else {
        if (eh_longlong(de) && eh_longlong(para)) return;   /* ya está en 64 bits (D128) */
        converter(para);
    }
}

/* Descarta un valor real que nadie va a usar (instrucción "x = 1.5;",
 * llamada de función double cuyo resultado se ignora...). */
static void descartar_inteiro(void);

static void descartar(tipo_t *t)
{
    if (!eh_real(t)) { descartar_inteiro(); return; }
    if (usar_sse) return;   /* en SSE2 no hay nada que quitar */
    if (ncod == recarga_fim) { ncod = recarga_ini; invalidar_ultimas(); return; }   /* quita el "fld" de guardar() */
    fstp_st0();
}

/* El valor entero de la expresión no se va a usar (instrucción "x = 5;", "i++;"):
 * "mov eax, 5; mov [x], eax" se convierte en "mov dword [x], 5", e "i++" no carga
 * el valor (mirilla, D108). */
static void descartar_inteiro(void)
{
    if (!otim(OTIM_MIRILLA)) return;
    if (inc_fim == ncod && inc_ld_ini >= barreira) {
        int ini = inc_ld_ini, len = inc_ld_len;
        if (ini + len == ncod) {                      /* ++i: la carga es la última */
            ncod = ini;
        } else {                                      /* i++: la carga es la primera */
            memmove(cod + ini, cod + ini + len, (size_t)(ncod - ini - len));
            ncod -= len;
        }
        invalidar_ultimas();
        return;
    }
    if (const_fim >= 0 && const_fim + 1 < ncod) {
        int m = cod[const_fim + 1];
        int len = m == 0x45 ? 3 : (m == 0x85 || m == 0x05) ? 6 : 0;
        if (len && const_ini >= 0 && const_fim == const_ini + 5 && cod[const_ini] == 0xB8 &&
            cod[const_fim] == 0x89 && ncod == const_fim + len && const_ini >= barreira) {
            uint8_t d[4];
            int nd = len - 2;
            memcpy(d, cod + const_fim + 2, (size_t)nd);
            uint32_t v = ler4(const_ini + 1);
            ncod = const_ini;
            e2(0xC7, m);
            for (int i = 0; i < nd; i++) e1(d[i]);
            e4(v);
            invalidar_ultimas();
        }
    }
}

/* Valor actual -> flags (como "test eax, eax"): para un real, EAX = (x != 0),
 * con NaN contando como verdadero, como en C. */
static void testar(tipo_t *t)
{
    if (eh_longlong(t)) {
        /* El valor de 64 bits no es cero si alguna de sus mitades no lo es. La
         * mitad baja se comprueba antes (si no, al moverla a AL se pierde). */
        e2(0x85, 0xC0);           /* test eax, eax */
        e3(0x0F, 0x95, 0xC0);     /* setne al */
        e2(0x85, 0xD2);           /* test edx, edx */
        e3(0x0F, 0x95, 0xD1);     /* setne cl */
        e2(0x08, 0xC8);           /* or al, cl */
        e3(0x0F, 0xB6, 0xC0);     /* movzx eax, al */
        return;
    }
    if (eh_real(t)) {
        if (usar_sse) {
            sse(0, 0x57, XMM(7, 7));    /* xorps xmm7, xmm7 */
            sse(t->k == T_FLOAT ? 0 : 0x66, 0x2E, XMM(0, 7));   /* ucomiss/ucomisd xmm0, xmm7 */
        } else {
            f2(0xD9, 0xEE);             /* fldz */
            f2(0xDF, 0xE9);             /* fucomip st0, st1 */
            fstp_st0();
        }
        e3(0x0F, 0x95, 0xC0);           /* setne al */
        e3(0x0F, 0x9A, 0xC1);           /* setp cl */
        e2(0x08, 0xC8);                 /* or al, cl */
        e3(0x0F, 0xB6, 0xC0);           /* movzx eax, al */
    }
    test_eax();
}

/* Salta si el valor actual es falso, sumando el salto a la lista. Si el valor es
 * el 0/1 de un "&&" o "||" recién montado, no monta el 0/1: los saltos del
 * propio "&&"/"||" van directo al destino (comparar y saltar, D108). */
static int saltar_se_falso(int lista)
{
    if (!eh_real(ty) && logico_saltar_se_falso(&lista)) return lista;
    testar(ty);
    return saltar(0x84, lista);
}

/* Operando izquierdo de una operación binaria: entero -> pila del
 * procesador; real -> queda en la x87 (ningún código) o, demasiado hondo, va a
 * la pila del procesador. Quien lo usa sabe cuál fue por el tamaño del
 * código generado (cero = quedó en la x87). */
static int salvar_esquerdo(tipo_t *t)
{
    if (eh_longlong(t)) {
        /* El valor de 64 bits (EDX:EAX) va a la pila: 8 bytes, el bajo abajo. */
        e1(0x52);        /* push edx (alto) */
        e1(0x50);        /* push eax (bajo) */
        return ESQ_LONGLONG;
    }
    if (!eh_real(t)) { push_eax(); return ESQ_INTEIRO; }
    if (x87_guardados < (usar_sse ? SSE_MAX_GUARDADOS : X87_MAX_GUARDADOS)) {
        x87_guardados++;
        if (usar_sse) sse(0, 0x28, XMM(x87_guardados, 0));   /* movaps xmmN, xmm0 */
        return ESQ_REG;
    }
    esp_menos(8);
    if (usar_sse) { sse(0xF2, 0x11, 0x04); e1(0x24); }   /* movsd [esp], xmm0 (el float va junto, en los 4 de abajo) */
    else f3(0xDD, 0x1C, 0x24);          /* fstp qword [esp] */
    return ESQ_MEMORIA;
}

/* Antes de una llamada: los operandos guardados en la x87 -> memoria. */
static int guardar_x87(void)
{
    int k = x87_guardados;
    if (k) {
        esp_menos(8 * k);
        for (int i = 0; i < k; i++) {
            if (usar_sse) { sse(0xF2, 0x11, 0x44 | (i + 1) << 3); e2(0x24, 8 * i); }   /* movsd [esp+8i], xmm(i+1) */
            else { f3(0xDD, 0x5C, 0x24); e1(8 * i); }   /* fstp qword [esp+8i] */
        }
    }
    x87_guardados = 0;
    return k;
}

/* Después de la llamada: de vuelta a la x87, debajo del resultado (si es real). */
static void restaurar_x87(int k, bool resultado_real)
{
    x87_guardados = k;
    if (!k) return;
    if (usar_sse) {   /* el resultado está en XMM0: no cambia */
        for (int i = 0; i < k; i++) { sse(0xF2, 0x10, 0x44 | (i + 1) << 3); e2(0x24, 8 * i); }
        esp_mais(8 * k);
        return;
    }
    if (resultado_real && k == 1) {
        f3(0xDD, 0x04, 0x24);           /* fld qword [esp] */
        f2(0xD9, 0xC9);                 /* fxch */
        esp_mais(8);
        return;
    }
    int r = 0;
    if (resultado_real) { esp_menos(8); f3(0xDD, 0x1C, 0x24); r = 8; }   /* el resultado también */
    for (int i = k - 1; i >= 0; i--) { f3(0xDD, 0x44, 0x24); e1(r + 8 * i); }   /* fld qword [esp+r+8i] */
    if (r) f3(0xDD, 0x04, 0x24);
    esp_mais(8 * k + r);
}

/* Dirección de la función (para "call" o para guardar en un puntero). */
static nome_t raizes;   /* referencias hechas fuera de cualquier función (_inicio, inicializadores) */

static void registrar_ref(nome_t *n)
{
    if (passada != 1 || descartando) return;
    nome_t *de = funcao_atual ? funcao_atual->nome : &raizes;
    for (int i = 0; i < de->nrefs; i++) if (de->refs[i] == n) return;
    garantir((void **)&de->refs, &de->caprefs, de->nrefs + 1, sizeof(nome_t *));
    de->refs[de->nrefs++] = n;
}

static void chamar_funcao(simbolo_t *f)
{
    registrar_ref(f->nome);
    f->usada = true;
    e1(0xE8);
    if (f->definida) { e4((uint32_t)(f->valor - (ncod + 4))); return; }
    int p = ncod;
    e4(descartando ? 0 : (uint32_t)f->chamadas);
    if (!descartando) f->chamadas = p;
}

static void endereco_funcao(simbolo_t *f)
{
    registrar_ref(f->nome);
    f->usada = true;
    e1(0xB8);
    if (f->definida) { e4(COD_BASE + f->valor); return; }
    int p = ncod;
    e4(descartando ? 0 : (uint32_t)f->absolutos);
    if (!descartando) f->absolutos = p;
}

static void definir_funcao_aqui(simbolo_t *f)
{
    f->definida = true;
    f->valor = ncod;
    marcar_rotulo();
    remendar(f->chamadas, ncod);
    f->chamadas = -1;
    for (int l = f->absolutos; l != -1;) {
        int prox = (int)ler4(l);
        por4(l, COD_BASE + ncod);
        l = prox;
    }
    f->absolutos = -1;
}

/* ------------------------------------------------------------ análisis */

/* ty: tipo del valor que está en EAX (o ST(0)), declarado más arriba. */

static ficha_t *tk(void) { return &fichas[pf]; }
static int tt(void) { return fichas[pf].tipo; }
static int tt_a(int k) { return pf + k < nfichas ? fichas[pf + k].tipo : F_FIM; }

static void avancar(void)
{
    if (fichas[pf].tipo != F_FIM) pf++;
    arq_erro = fichas[pf].arq ? fichas[pf].arq : arq_erro;
    linha_erro = fichas[pf].linha;
}

static bool aceitar(int t)
{
    if (tt() != t) return false;
    avancar();
    return true;
}

static const char *descrever(int t)
{
    static char b[8];
    switch (t) {
    case F_FIM: return "el fin del archivo";
    case F_NUM: return "un numero";
    case F_TEXTO: return "un texto";
    case F_NOME: return "un nombre";
    }
    if (t < 256) { b[0] = '\''; b[1] = (char)t; b[2] = '\''; b[3] = 0; return b; }
    if (t >= P_INT) return reservadas[t - P_INT];
    return "un operador";
}

static void esperar_ficha(int t)
{
    if (tt() == t) { avancar(); return; }
    char quero[16];
    copia_texto(quero, descrever(t), sizeof quero);
    falhar("se esperaba %s y vino %s", quero, tt() == F_NOME ? tk()->nome->s : descrever(tt()));
}

static nome_t *esperar_nome(void)
{
    if (tt() != F_NOME) falhar("se esperaba un nombre y vino %s", descrever(tt()));
    nome_t *n = tk()->nome;
    avancar();
    return n;
}

static bool eh_nome_de_tipo(int k)
{
    if (pf + k >= nfichas) return false;
    ficha_t *t = &fichas[pf + k];
    if (t->tipo == F_NOME) {
        simbolo_t *s = procurar(t->nome);
        return s && s->classe == S_TIPO;
    }
    switch (t->tipo) {
    case P_INT: case P_CHAR: case P_VOID: case P_SHORT: case P_LONG: case P_UNSIGNED: case P_SIGNED:
    case P_STRUCT: case P_UNION: case P_ENUM: case P_TYPEDEF: case P_STATIC: case P_EXTERN:
    case P_CONST: case P_VOLATILE: case P_REGISTER: case P_AUTO: case P_INLINE: case P_BOOL:
    case P_FLOAT: case P_DOUBLE:
        return true;
    }
    return false;
}

/* ---- expresiones constantes (tamaño de vector, case, enum, #if) ---- */

static int const_cond(void);
static tipo_t *nome_de_tipo(void);

static int const_unario(void)
{
    if (tt() == F_NUM) { int v = tk()->val; avancar(); return v; }
    if (tt() == F_REAL) falhar("se esperaba una constante entera");
    if (aceitar('-')) return -const_unario();
    if (aceitar('+')) return const_unario();
    if (aceitar('~')) return ~const_unario();
    if (aceitar('!')) return !const_unario();
    if (aceitar(P_SIZEOF)) {
        bool par = tt() == '(';
        if (par && eh_nome_de_tipo(1)) {
            avancar();
            tipo_t *t = nome_de_tipo();
            esperar_ficha(')');
            return t->tam;
        }
        if (par) avancar();
        nome_t *n = esperar_nome();
        simbolo_t *s = procurar(n);
        if (!s) falhar("'%s' no declarado", n->s);
        if (par) esperar_ficha(')');
        return s->t->tam;
    }
    if (tt() == '(') {
        if (eh_nome_de_tipo(1)) {      /* conversión: (int) x */
            avancar();
            tipo_t *t = nome_de_tipo();
            esperar_ficha(')');
            int v = const_unario();
            if (t->k == T_CHAR) v = (signed char)v;
            else if (t->k == T_UCHAR) v = (uint8_t)v;
            else if (t->k == T_SHORT) v = (short)v;
            else if (t->k == T_USHORT) v = (uint16_t)v;
            return v;
        }
        avancar();
        int v = const_cond();
        esperar_ficha(')');
        return v;
    }
    if (tt() == F_NOME) {
        simbolo_t *s = procurar(tk()->nome);
        if (s && s->classe == S_CONST) { avancar(); return s->valor; }
        falhar("'%s' no es una constante", tk()->nome->s);
    }
    falhar("se esperaba una constante");
}

static int precedencia(int t)
{
    switch (t) {
    case F_OU: return 4;
    case F_E: return 5;
    case '|': return 6;
    case '^': return 7;
    case '&': return 8;
    case F_IGUAL: case F_DIF: return 9;
    case '<': case '>': case F_MENOR_IG: case F_MAIOR_IG: return 10;
    case F_SHL: case F_SHR: return 11;
    case '+': case '-': return 12;
    case '*': case '/': case '%': return 13;
    }
    return 0;
}

static int const_binario(int minimo)
{
    int a = const_unario();
    for (;;) {
        int op = tt(), p = precedencia(op);
        if (!p || p < minimo) return a;
        avancar();
        int b = const_binario(p + 1);
        switch (op) {
        case F_OU: a = a || b; break;
        case F_E: a = a && b; break;
        case '|': a |= b; break;
        case '^': a ^= b; break;
        case '&': a &= b; break;
        case F_IGUAL: a = a == b; break;
        case F_DIF: a = a != b; break;
        case '<': a = a < b; break;
        case '>': a = a > b; break;
        case F_MENOR_IG: a = a <= b; break;
        case F_MAIOR_IG: a = a >= b; break;
        case F_SHL: a <<= b; break;
        case F_SHR: a >>= b; break;
        case '+': a += b; break;
        case '-': a -= b; break;
        case '*': a *= b; break;
        case '/': if (!b) falhar("division por cero"); a /= b; break;
        case '%': if (!b) falhar("division por cero"); a %= b; break;
        }
    }
}

static int const_cond(void)
{
    int c = const_binario(4);
    if (!aceitar('?')) return c;
    int a = const_cond();
    esperar_ficha(':');
    int b = const_cond();
    return c ? a : b;
}

/* Constante real (inicialización de globales float/double): + - * / sobre
 * números, constantes de enum, sizeof y conversiones, calculada en double. */
static double real_binario(int minimo);

static double real_unario(void)
{
    if (tt() == F_REAL) { double v = tk()->real; bool f = tk()->real_float; avancar(); return f ? (double)(float)v : v; }
    if (tt() == F_NUM) { double v = tk()->sem_sinal ? (double)(uint32_t)tk()->val : (double)tk()->val; avancar(); return v; }
    if (aceitar('-')) return -real_unario();
    if (aceitar('+')) return real_unario();
    if (tt() == P_SIZEOF) return const_unario();
    if (tt() == '(' && eh_nome_de_tipo(1)) {
        avancar();
        tipo_t *t = nome_de_tipo();
        esperar_ficha(')');
        double v = real_unario();
        if (t->k == T_FLOAT) return (double)(float)v;
        if (t->k == T_UINT) return (double)(uint32_t)v;
        if (eh_inteiro(t)) return (double)(int)v;
        return v;
    }
    if (aceitar('(')) {
        double v = real_binario(12);
        esperar_ficha(')');
        return v;
    }
    if (tt() == F_NOME) {
        simbolo_t *s = procurar(tk()->nome);
        if (s && s->classe == S_CONST) { avancar(); return s->valor; }
    }
    falhar("se esperaba una constante");
}

static double real_binario(int minimo)
{
    double a = real_unario();
    for (;;) {
        int op = tt(), p = precedencia(op);
        if (p < 12 || p < minimo) return a;
        avancar();
        double b = real_binario(p + 1);
        switch (op) {
        case '+': a = a + b; break;
        case '-': a = a - b; break;
        case '*': a = a * b; break;
        case '/': a = a / b; break;
        default: falhar("operador invalido en una constante real");
        }
    }
}

/* #if: evalúa una lista de fichas suelta, cambiando la lista actual por ella. */
static int avaliar_fichas(ficha_t *v, int n)
{
    if (n == 0) falhar("#if sin condicion");
    ficha_t *salvo = fichas;
    int salvo_n = nfichas, salvo_p = pf;
    fichas = v;
    nfichas = n + 1;
    pf = 0;
    int r = const_cond();
    if (tt() != F_FIM) falhar("condicion de #if invalida");
    fichas = salvo;
    nfichas = salvo_n;
    pf = salvo_p;
    return r;
}

/* ---- declaraciones ---- */

enum { A_NADA = 0, A_STATIC = 1, A_EXTERN = 2, A_TYPEDEF = 4 };

static tipo_t *especificadores(int *armazenamento);
static tipo_t *declarador(tipo_t *base, nome_t **nome);

static nome_t *params_nomes[64];   /* nombres de los parámetros de la última lista leída */
static int params_n;

static tipo_t *struct_ou_union(bool uniao)
{
    nome_t *etq = tt() == F_NOME ? esperar_nome() : NULL;
    tipo_t *t = NULL;
    if (etq) {
        /* O rotulo da struct: o da unidade, ou o da biblioteca (D133). */
        tipo_t *e = etq->etiqueta ? etq->etiqueta : etq->etiqueta_global;
        if (e && e->k == T_STRUCT) {
            t = e;
            if (tt() == '{' && t->completo) falhar("struct %s redefinida", etq->s);
        }
    }
    if (!t) {
        t = memoria(sizeof *t);
        t->k = T_STRUCT;
        t->uniao = uniao;
        t->alin = 1;
        if (etq) {
            if (cabecalho_biblioteca) etq->etiqueta_global = t;
            else etq->etiqueta = t;
        }
    }
    if (!aceitar('{')) return t;
    membro_t **ultimo = &t->membros;
    int desloc = 0, tam = 0, alin = 1;
    while (!aceitar('}')) {
        tipo_t *base = especificadores(NULL);
        do {
            nome_t *n = NULL;
            tipo_t *mt = declarador(base, &n);
            if (!n) falhar("miembro sin nombre");
            if (mt->k == T_STRUCT && !mt->completo) falhar("miembro de tipo incompleto");
            if (mt->k == T_VETOR && mt->n < 0) falhar("vector de tamano desconocido en struct");
            membro_t *m = memoria(sizeof *m);
            m->nome = n;
            m->t = mt;
            if (uniao) {
                m->desloc = 0;
                if (mt->tam > tam) tam = mt->tam;
            } else {
                desloc = (desloc + mt->alin - 1) & ~(mt->alin - 1);
                m->desloc = desloc;
                desloc += mt->tam;
                tam = desloc;
            }
            if (mt->alin > alin) alin = mt->alin;
            *ultimo = m;
            ultimo = &m->prox;
        } while (aceitar(','));
        esperar_ficha(';');
    }
    t->alin = alin;
    t->tam = (tam + alin - 1) & ~(alin - 1);
    t->completo = true;
    return t;
}

static int profundidade_funcao;   /* > 0 dentro de una función */

static tipo_t *enumeracao(void)
{
    if (tt() == F_NOME) avancar();   /* la etiqueta del enum no importa: es int */
    if (!aceitar('{')) return &t_int;
    int v = 0;
    while (!aceitar('}')) {
        nome_t *n = esperar_nome();
        if (aceitar('=')) v = const_cond();
        simbolo_t *s = novo_simbolo(n, S_CONST, &t_int, profundidade_funcao > 0 ? E_LOCAL : E_UNIDADE);
        s->valor = v++;
        if (!aceitar(',')) { esperar_ficha('}'); break; }
    }
    return &t_int;
}

static bool eh_real_base(tipo_t *t) { return t == &t_float || t == &t_double; }

static tipo_t *especificadores(int *armazenamento)
{
    int arm = 0, sem_sinal = 0, com_sinal = 0, curto = 0, longo = 0;
    tipo_t *base = NULL;
    bool viu_int = false;
    for (;;) {
        int t = tt();
        if (t == P_STATIC) arm |= A_STATIC;
        else if (t == P_EXTERN) arm |= A_EXTERN;
        else if (t == P_TYPEDEF) arm |= A_TYPEDEF;
        else if (t == P_CONST || t == P_VOLATILE || t == P_REGISTER || t == P_AUTO || t == P_INLINE) ;
        else if (t == P_UNSIGNED) sem_sinal = 1;
        else if (t == P_SIGNED) com_sinal = 1;
        else if (t == P_SHORT) curto = 1;
        else if (t == P_LONG) longo++;
        else if (t == P_INT) viu_int = true;
        else if (t == P_CHAR && !base) base = &t_char;
        else if (t == P_VOID && !base) base = &t_void;
        else if (t == P_BOOL && !base) base = &t_uchar;
        else if (t == P_FLOAT && !base) base = &t_float;
        else if (t == P_DOUBLE && !base) base = &t_double;
        else if ((t == P_STRUCT || t == P_UNION) && !base) {
            avancar();
            base = struct_ou_union(t == P_UNION);
            continue;
        } else if (t == P_ENUM && !base) {
            avancar();
            base = enumeracao();
            continue;
        } else if (t == F_NOME && !base && !viu_int && !sem_sinal && !com_sinal && !curto && !longo) {
            simbolo_t *s = procurar(tk()->nome);
            if (!s || s->classe != S_TIPO) break;
            base = s->t;
        } else {
            break;
        }
        avancar();
    }
    (void)com_sinal;
    if (eh_real_base(base)) {
        if (sem_sinal || curto || viu_int || (base == &t_float && longo) || longo > 1) falhar("tipo invalido");
        if (armazenamento) *armazenamento = arm;
        else if (arm) falhar("static/extern/typedef no se permiten aqui");
        return base;   /* long double = double */
    }
    if (longo > 1) {
        /* long long (D128): 8 bytes, con o sin signo. */
        if (base || curto || viu_int) falhar("tipo invalido");
        base = sem_sinal ? &t_ulonglong : &t_longlong;
        if (armazenamento) *armazenamento = arm;
        else if (arm) falhar("static/extern/typedef no se permiten aqui");
        return base;
    }
    if (base == &t_char) base = sem_sinal ? &t_uchar : &t_char;
    else if (!base) base = curto ? (sem_sinal ? &t_ushort : &t_short) : (sem_sinal ? &t_uint : &t_int);
    if (armazenamento) *armazenamento = arm;
    else if (arm) falhar("static/extern/typedef no se permiten aqui");
    return base;
}

/* Parámetros: "(void)", "()", "(int a, char *b, ...)". */
static tipo_t *lista_de_parametros(tipo_t *retorno)
{
    tipo_t *f = memoria(sizeof *f);
    f->k = T_FUNC;
    f->tam = f->alin = 1;
    f->base = retorno;
    f->params = memoria(sizeof(tipo_t *) * 64);
    nome_t *nomes_aqui[64];
    int n = 0;
    if (tt() == P_VOID && tt_a(1) == ')') {
        avancar();
        avancar();
        f->n = 0;
    } else if (aceitar(')')) {
        f->n = -1;   /* sin prototipo: cualquier número de argumentos */
    } else {
        for (;;) {
            if (aceitar(F_RETICENCIAS)) { f->variadica = true; esperar_ficha(')'); break; }
            tipo_t *b = especificadores(NULL);
            nome_t *pn = NULL;
            tipo_t *pt = declarador(b, &pn);
            if (pt->k == T_VETOR) pt = ponteiro(pt->base);
            if (pt->k == T_FUNC) pt = ponteiro(pt);
            if (pt->k == T_STRUCT) falhar("struct por valor como parametro no esta soportado (usa un puntero)");
            if (n == 64) falhar("demasiados parametros");
            f->params[n] = pt;
            nomes_aqui[n++] = pn;
            if (aceitar(')')) break;
            esperar_ficha(',');
        }
        f->n = n;
    }
    for (int i = 0; i < n; i++) params_nomes[i] = nomes_aqui[i];
    params_n = n;
    return f;
}

static int pular_parenteses(int p)   /* p apunta a '(': devuelve la posición después del ')' */
{
    int nivel = 0;
    for (;; p++) {
        if (fichas[p].tipo == F_FIM) falhar("parentesis sin cerrar");
        if (fichas[p].tipo == '(') nivel++;
        if (fichas[p].tipo == ')' && --nivel == 0) return p + 1;
    }
}

static tipo_t *sufixos(tipo_t *base)
{
    if (aceitar('[')) {
        int n = -1;
        if (tt() != ']') {
            n = const_cond();
            if (n < 0) falhar("tamano de vector negativo");
        }
        esperar_ficha(']');
        tipo_t *dentro = sufixos(base);
        if (dentro->k == T_VETOR && dentro->n < 0) falhar("solo la primera dimension puede quedar vacia");
        return vetor(dentro, n);
    }
    if (aceitar('(')) {
        if (base->k == T_VETOR || base->k == T_FUNC) falhar("funcion no puede devolver vector o funcion");
        return lista_de_parametros(base);
    }
    return base;
}

/* Declarador: punteros, nombre (opcional en los nombres de tipo) y sufijos. En el
 * caso "(*nombre)(params)", lo que viene después de los paréntesis se lee primero
 * y se convierte en la base del declarador de dentro. */
static tipo_t *declarador(tipo_t *base, nome_t **nome)
{
    while (aceitar('*')) {
        base = ponteiro(base);
        while (tt() == P_CONST || tt() == P_VOLATILE) avancar();
    }
    if (tt() == '(' && (tt_a(1) == '*' || tt_a(1) == '(')) {
        int dentro = pf + 1;
        pf = pular_parenteses(pf);
        tipo_t *t = sufixos(base);
        int fim = pf;
        pf = dentro;
        t = declarador(t, nome);
        if (tt() != ')') falhar("declarador invalido");
        pf = fim;
        return t;
    }
    if (tt() == F_NOME) *nome = esperar_nome();
    return sufixos(base);
}

static tipo_t *nome_de_tipo(void)
{
    tipo_t *b = especificadores(NULL);
    nome_t *n = NULL;
    tipo_t *t = declarador(b, &n);
    if (n) falhar("nombre inesperado dentro del tipo");
    return t;
}

/* ------------------------------------------------------------ expresiones */

static void expr(void);
static void expr_atrib(void);
static void unario(void);

static int local_desloc, local_max;
static int ultimo_texto_n;

static void exigir_endereco(void)
{
    if (carga_fim < 0 || ncod != carga_fim) falhar("se esperaba una variable (lvalue)");
    ncod = carga_ini;
    carga_fim = -1;
    invalidar_ultimas();
    endereco_fim = -1;
    if (carga_direta) {
        carga_direta = false;
        endereco_ini = ncod;
        emitir_endereco(direta_modrm, direta_disp);
        endereco_fim = ncod;
    }
}

/* Texto literal (con los vecinos concatenados: "ab" "cd") en los datos. */
static int texto_literal(void)
{
    int total = 0;
    for (int p = pf; fichas[p].tipo == F_TEXTO; p++) total += literais[fichas[p].val].n;
    int off = aloca_dados(total + 1, 1);
    int k = 0;
    while (tt() == F_TEXTO) {
        literal_t *l = &literais[tk()->val];
        memcpy(dados + off + k, l->bytes, l->n);
        k += l->n;
        avancar();
    }
    ultimo_texto_n = total + 1;
    return off;
}

static tipo_t *promover(tipo_t *t)
{
    t = decair(t);
    if (t->k == T_UINT || t->k == T_PTR) return t;
    if (eh_longlong(t)) return t;      /* los de 64 bits no se degradan (D128) */
    if (!eh_inteiro(t)) falhar("se esperaba un numero");
    return &t_int;
}

static tipo_t *aritmetico(tipo_t *a, tipo_t *b)
{
    if (eh_longlong(a) || eh_longlong(b)) {       /* D128 */
        if (eh_ll_sem_sinal(a) || eh_ll_sem_sinal(b)) return &t_ulonglong;
        return &t_longlong;
    }
    return (a->k == T_UINT || b->k == T_UINT) ? &t_uint : &t_int;
}

/* Llamada: los argumentos se calculan de izquierda a derecha y
 * se apilan; después el orden se invierte en la propia pila, para quedar como
 * la convención de C (primer argumento arriba). */
/* Un argumento ya calculado -> pila. Con prototipo, toma el tipo del
 * parámetro (float: 4 bytes, double: 8); sin prototipo o después del "...",
 * float se convierte en double (promoción de C). Devuelve cuántos bytes apiló. */
static int empilhar_argumento(tipo_t *p)
{
    tipo_t *t = decair(ty);
    if (p) {
        converter_valor(t, p);
        t = p;
    } else if (t->k == T_FLOAT) {
        converter_valor(t, &t_double);
        t = &t_double;
    }
    if (!eh_real(t)) {
        if (eh_longlong(t)) {
            /* El valor de 64 bits (EDX:EAX) ocupa 8 bytes en la pila. */
            e1(0x52);        /* push edx (alto) */
            e1(0x50);        /* push eax (bajo) */
            return 8;
        }
        push_eax();
        return 4;
    }
    esp_menos(t->tam);
    if (usar_sse) { sse(pre_t(t), 0x11, 0x04); e1(0x24); }   /* movss/movsd [esp], xmm0 */
    else f3(op_carga(t), 0x1C, 0x24);   /* fstp dword/qword [esp] */
    return t->tam;
}

static void mov_esp(int op, int reg, int d)   /* op 0x8B: reg = [esp+d]; 0x89: [esp+d] = reg */
{
    if (otim(OTIM_IMEDIATOS) && d == 0) { e3(op, 0x04 | reg << 3, 0x24); return; }
    if (otim(OTIM_IMEDIATOS) && d >= -128 && d <= 127) { e3(op, 0x44 | reg << 3, 0x24); e1(d & 0xFF); return; }
    e3(op, 0x84 | reg << 3, 0x24);
    e4((uint32_t)d);
}

/* Llamada: los argumentos se calculan de izquierda a derecha y
 * se apilan; después el orden se invierte en la propia pila, para quedar como
 * la convención de C (primer argumento arriba). Con algún double, los
 * tamaños varían: los argumentos se copian a un espacio nuevo abajo. */
static void chamada(simbolo_t *f, tipo_t *ft, bool indireta)
{
    int guardados = guardar_x87();
    if (indireta) push_eax();
    int n = 0, total = 0, tams[64];
    if (!aceitar(')')) {
        for (;;) {
            expr_atrib();
            if (ty->k == T_STRUCT) falhar("struct por valor como argumento no esta soportado (usa un puntero)");
            if (n == 64) falhar("demasiados argumentos");
            tams[n] = empilhar_argumento(ft->n >= 0 && n < ft->n ? ft->params[n] : NULL);
            total += tams[n++];
            if (aceitar(')')) break;
            esperar_ficha(',');
        }
    }
    if (ft->n >= 0 && (n < ft->n || (n > ft->n && !ft->variadica)))
        falhar("'%s' espera %d argumentos y recibio %d", f ? f->nome->s : "la funcion", ft->n, n);
    int liberar = total;
    if (total == 4 * n) {
        for (int i = 0; i < n / 2; i++) {
            int a = 4 * i, b = 4 * (n - 1 - i);
            mov_esp(0x8B, 0, a);       /* mov eax, [esp+a] */
            mov_esp(0x8B, 1, b);       /* mov ecx, [esp+b] */
            mov_esp(0x89, 1, a);       /* mov [esp+a], ecx */
            mov_esp(0x89, 0, b);       /* mov [esp+b], eax */
        }
    } else {
         /* El argumento i se apiló en [esp + (suma de los siguientes)]; va
          * a [esp + (suma de los anteriores)], después de "sub esp, total". */
        e2(0x81, 0xEC); e4((uint32_t)total);
        int antes = 0, depois = total;
        for (int i = 0; i < n; i++) {
            depois -= tams[i];
            for (int k = 0; k < tams[i]; k += 4) {
                mov_esp(0x8B, 0, total + depois + k);
                mov_esp(0x89, 0, antes + k);
            }
            antes += tams[i];
        }
        liberar = 2 * total;
    }
    if (indireta) {
        mov_esp(0x8B, 0, liberar);         /* la dirección de la función, apilada antes */
        e2(0xFF, 0xD0);                    /* call eax */
        liberar += 4;
    } else {
        chamar_funcao(f);
    }
    if (liberar) aplicar_imediato(0, 4, liberar, 0);          /* add esp, n */
    ty = ft->base;
    restaurar_x87(guardados, eh_real(ty));
    carga_fim = -1;
    invalidar_ultimas();
}

/* Funciones integradas que se convierten en instrucciones de la x87 (D039): la tabla está en
 * gerador_x86.h. La biblioteca usa éstas para sqrt, sin, fmod, exp, log...;
 * una llamada del programa a sqrt/fabs/sin/cos/tan/atan/atan2 de la biblioteca
 * también se convierte en la instrucción. */

/* Un argumento de función integrada: expresión -> ST(0) como real. */
static void argumento_real(void)
{
    expr_atrib();
    converter_valor(ty, &t_double);
}

static void embutida(int k)
{
    esperar_ficha('(');
    argumento_real();
    bool sse_direto = usar_sse && embutidas[k].comum &&
                      (!compara_texto(embutidas[k].comum, "sqrt") || !compara_texto(embutidas[k].comum, "fabs"));
    if (sse_direto) {
        esperar_ficha(')');
        if (embutidas[k].comum[0] == 's') sse(0xF2, 0x51, XMM(0, 0));   /* sqrtsd xmm0, xmm0 */
        else {
            sse(0xF2, 0x10, 0x3D); e4(endereco_bits(0xFFFFFFFFu, 0x7FFFFFFFu, false));   /* movsd xmm7, [máscara] */
            sse(0, 0x54, XMM(0, 7));    /* andps xmm0, xmm7: zera o sinal */
        }
    } else if (usar_sse) {
        /* Las otras son de la x87: los argumentos pasan por la memoria. */
        esp_menos(8);
        sse(0xF2, 0x11, 0x04); e1(0x24);   /* movsd [esp], xmm0 */
        int fundo = 0;
        if (embutidas[k].nargs == 2) {
            esperar_ficha(',');
            argumento_real();
            esp_menos(8);
            sse(0xF2, 0x11, 0x04); e1(0x24);
            f3(0xDD, 0x04, 0x24);       /* fld qword [esp]: o 2º */
            fundo = 8;
        }
        f3(0xDD, 0x44, 0x24); e1(fundo);   /* fld qword [esp+fundo]: ST0 = 1º, ST1 = 2º */
        esperar_ficha(')');
        for (int i = 0; i < embutidas[k].n; i++) e1(embutidas[k].cod[i]);
        if (!compara_texto(embutidas[k].nome, "__builtin_exp2")) e2(0xDD, 0xD9);   /* fstp st1 */
        f3(0xDD, 0x1C, 0x24);           /* fstp qword [esp] */
        sse(0xF2, 0x10, 0x04); e1(0x24);   /* movsd xmm0, [esp] */
        esp_mais(8 + fundo);
    } else {
        if (embutidas[k].nargs == 2) {
            esperar_ficha(',');
            int forma = salvar_esquerdo(&t_double);
            argumento_real();
            if (forma == ESQ_REG) {
                x87_guardados--;
                f2(0xD9, 0xC9);         /* fxch: ST0 = 1º, ST1 = 2º */
            } else {
                f3(0xDD, 0x04, 0x24);   /* fld qword [esp]: ST0 = 1º, ST1 = 2º */
                esp_mais(8);
            }
        }
        esperar_ficha(')');
        for (int i = 0; i < embutidas[k].n; i++) e1(embutidas[k].cod[i]);
        if (!compara_texto(embutidas[k].nome, "__builtin_exp2")) e2(0xDD, 0xD9);   /* fstp st1 */
    }
    ty = &t_double;
    carga_fim = -1;
    invalidar_ultimas();
}

static int procurar_embutida(nome_t *n, bool comum)
{
    for (int i = 0; i < N_EMBUTIDAS; i++) {
        const char *x = comum ? embutidas[i].comum : embutidas[i].nome;
        if (x && !compara_texto(n->s, x)) return i;
    }
    return -1;
}

/* Variable o constante real cargada directo de la memoria: se guarda como
 * operando "simple" (ver operar_real). */
static void marcar_simples(int ini, int modrm, uint32_t disp)
{
    if (!eh_real(ty)) return;
    simples_ini = ini;
    simples_fim = ncod;
    simples_modrm = modrm;
    simples_disp = disp;
    simples_t = ty;
}

/* Variable local (modrm 0x85) o global (0x05): el valor con una instrucción
 * sola, o la dirección, si es vector o struct. */
static void variavel(tipo_t *t, int modrm, uint32_t disp)
{
    ty = t;
    if (!eh_escalar(t)) {
        base_ini = ncod;
        emitir_endereco(modrm, disp);
        base_fim = ncod; base_modrm = modrm; base_disp = disp;
        return;
    }
    carga_ini = ncod;
    if (eh_longlong(t)) {
        /* El valor de 64 bits: la dirección va a EAX y de ahí se leen las dos
         * mitades (EDX:EAX = [dirección], D128). La dirección se puede volver a
         * emitir (exigir_endereco), así que sigue siendo un lvalue normal. */
        emitir_endereco(modrm, disp);
        carregar_de(t, 0x00);
    } else {
        carregar_de(t, modrm);
        ed(disp);
        marcar_simples(carga_ini, modrm, disp);
    }
    carga_fim = ncod;
    carga_direta = true;
    direta_modrm = modrm;
    direta_disp = disp;
}

static void primario(void)
{
    ficha_t *t = tk();
    if (t->tipo == F_NUM && t->longo) {
        /* Constante de 64 bits (D128): el valor va en EDX:EAX. No se marca como
         * constante del compilador (susurneos de 32 bits), para que el
         * optimizador no la mezcle. */
        mov_eax((uint32_t)t->val);
        e1(0xBA); e4(t->val_hi);      /* mov edx, la parte alta */
        ty = t->sem_sinal ? &t_ulonglong : &t_longlong;
        avancar();
        return;
    }
    if (t->tipo == F_NUM) {
        const_ini = ncod;
        mov_eax((uint32_t)t->val);
        const_fim = ncod;
        const_val = t->val;
        ty = t->sem_sinal ? &t_uint : &t_int;
        avancar();
        return;
    }
    if (t->tipo == F_REAL) {
        ty = t->real_float ? &t_float : &t_double;
        uint32_t a = endereco_constante(t->real, t->real_float);
        int ini = ncod;
        if (usar_sse) sse(pre_t(ty), 0x10, 0x05);   /* movss/movsd xmm0, [constante] */
        else f2(op_carga(ty), 0x05);                /* fld [constante] */
        e4(a);
        marcar_simples(ini, 0x05, a);
        avancar();
        return;
    }
    if (t->tipo == F_TEXTO) {
        int off = texto_literal();
        mov_eax(DADOS_BASE + off);
        ty = vetor(&t_char, ultimo_texto_n);
        return;
    }
    if (aceitar('(')) {
        expr();
        esperar_ficha(')');
        return;
    }
    if (t->tipo == F_NOME) {
        nome_t *n = t->nome;
        avancar();
        simbolo_t *s = procurar(n);
        if (!s && tt() == '(' && n->s[0] == '_') {
            int k = procurar_embutida(n, false);
            if (k >= 0) { embutida(k); return; }
            if (!compara_texto(n->s, "__tam_argumento")) {   /* espacio de un tipo en la pila (va_arg) */
                avancar();
                tipo_t *a = nome_de_tipo();
                esperar_ficha(')');
                mov_eax((uint32_t)((a->tam + 3) & ~3));
                ty = &t_int;
                return;
            }
        }
        if (!s) {
            if (tt() != '(') falhar("'%s' no declarado", n->s);
            /* Función sin prototipo (como en el C antiguo): devuelve int. */
            tipo_t *f = memoria(sizeof *f);
            f->k = T_FUNC;
            f->tam = f->alin = 1;
            f->base = &t_int;
            f->n = -1;
            s = novo_simbolo(n, S_FUNC, f, E_GLOBAL);
        }
        switch (s->classe) {
        case S_CONST:
            const_ini = ncod;
            mov_eax((uint32_t)s->valor);
            const_fim = ncod;
            const_val = s->valor;
            ty = &t_int;
            return;
        case S_TIPO:
            falhar("uso inesperado del tipo '%s'", n->s);
        case S_FUNC:
            if (tt() == '(' && s->da_biblioteca && !compilando_biblioteca) {
                int k = procurar_embutida(n, true);
                if (k >= 0) { embutida(k); return; }
            }
            if (aceitar('(')) { chamada(s, s->t, false); return; }
            endereco_funcao(s);
            ty = s->t;
            return;
        case S_LOCAL:
            variavel(s->t, 0x85, (uint32_t)s->valor);
            return;
        case S_GLOBAL:
            variavel(s->t, 0x05, (uint32_t)s->valor);
            return;
        }
    }
    falhar("expresion invalida (vino %s)", descrever(t->tipo));
}

static void incremento(bool mais, bool depois)
{
    tipo_t *t = ty;
    if (!eh_escalar(t)) falhar("++ o -- en algo que no es numero ni puntero");
    exigir_endereco();
    if (eh_real(t) && usar_sse) {
        e2(0x89, 0xC1);              /* mov ecx, eax */
        carregar_de(t, 0x01);        /* movsd xmm0, [ecx] */
        uint32_t um = endereco_constante(1.0, t->k == T_FLOAT);
        int op = mais ? 0x58 : 0x5C; /* addsd / subsd */
        if (depois) {
            sse(0, 0x28, XMM(7, 0)); /* movaps xmm7, xmm0: queda el antiguo en XMM0 */
            sse(pre_t(t), op, 0x3D); e4(um);
            sse(pre_t(t), 0x11, 0x39);   /* movsd [ecx], xmm7 */
        } else {
            sse(pre_t(t), op, 0x05); e4(um);
            sse(pre_t(t), 0x11, 0x01);   /* movsd [ecx], xmm0 */
        }
        ty = t;
        carga_fim = -1;
        return;
    }
    if (eh_real(t)) {
        e2(0x89, 0xC1);              /* mov ecx, eax */
        carregar_de(t, 0x01);        /* fld [ecx] */
        f2(0xD9, 0xE8);              /* fld1 */
        if (depois) {
            f2(0xD8, mais ? 0xC1 : 0xE9);   /* fadd st0, st1 / fsubr st0, st1: ST0 = nuevo, ST1 = antiguo */
            f2(op_carga(t), 0x19);          /* fstp [ecx]: queda el antiguo */
        } else {
            f2(0xDE, mais ? 0xC1 : 0xE9);   /* faddp / fsubp st1: ST0 = nuevo */
            guardar(t, 0);
        }
        ty = t;
        carga_fim = -1;
        return;
    }
    int d = t->k == T_PTR ? passo(t) : 1;
    if (eh_longlong(t)) {
        /* El valor de 64 bits (EDX:EAX): se suma 1 con el acarreo. */
        e2(0x89, 0xC1);                  /* mov ecx, eax */
        carregar_de(t, 0x01);            /* EDX:EAX = [ecx] */
        if (depois) { e1(0x52); e1(0x50); }   /* el valor viejo, en la pila */
        /* Con "add eax, -1" el CF indica acarreo (no préstamo), así que el
         * decremento usa la pareja "sub / sbb": CF = préstamo hacia la palabra
         * de arriba. */
        if (mais) { e3(0x83, 0xC0, 0x01); e3(0x83, 0xD2, 0x00); }  /* add eax,1; adc edx,0 */
        else { e3(0x83, 0xE8, 0x01); e3(0x83, 0xD2, 0x00); }     /* sub eax,1; sbb edx,0 */
        guardar(t, 0);                        /* [ecx] = EDX:EAX */
        if (depois) { e1(0x58); e1(0x5A); }   /* pop eax; pop edx: el valor viejo */
        ty = t;
        carga_fim = -1;
        return;
    }
    if (endereco_fim == ncod && t->tam == 4) {   /* variable simple de 32 bits */
        int modrm = direta_modrm;
        uint32_t disp = direta_disp;
        ncod = endereco_ini;
        int ld = ncod;
        if (depois) { e2(0x8B, modrm); ed(disp); }                     /* mov eax, [mem] */
        int len = ncod - ld;
        if (otim(OTIM_IMEDIATOS) && d >= -128 && d <= 127) {
            e2(0x83, (mais ? 0x00 : 0x28) | modrm); ed(disp); e1(d & 0xFF);   /* add/sub dword [mem], d8 */
        } else {
            e2(0x81, (mais ? 0x00 : 0x28) | modrm); ed(disp); e4((uint32_t)d);   /* add/sub dword [mem], d */
        }
        if (!depois) { ld = ncod; e2(0x8B, modrm); ed(disp); len = ncod - ld; }
        ty = t;
        carga_fim = -1;
        inc_fim = ncod; inc_ld_ini = ld; inc_ld_len = len;
        return;
    }
    e2(0x89, 0xC1);                  /* mov ecx, eax */
    carregar_de(t, 0x01);            /* eax = [ecx] */
    if (depois) {
        e2(0x89, 0xC2);              /* mov edx, eax */
        e2(0x81, mais ? 0xC2 : 0xEA); e4((uint32_t)d);
        guardar(t, 2);
    } else {
        e1(mais ? 0x05 : 0x2D); e4((uint32_t)d);
        guardar(t, 0);
        converter(t);
    }
    ty = t;
    carga_fim = -1;
}

static void posfixo(void)
{
    primario();
    for (;;) {
        if (aceitar('[')) {
            tipo_t *b = decair(ty);
            if (b->k != T_PTR) falhar("indice en algo que no es vector ni puntero");
            int sc = passo(b);
            bool pot = otim(OTIM_IMEDIATOS) && (sc == 1 || sc == 2 || sc == 4 || sc == 8);
            int sib = (sc == 1 ? 0 : sc == 2 ? 1 : sc == 4 ? 2 : 3) << 6;
            if (otim(OTIM_IMEDIATOS) && base_fim == ncod) {
                /* vector de variable: la dirección entra en el propio lea (o en el acceso, si el índice es constante) */
                int bm = base_modrm;
                uint32_t bd = base_disp;
                ncod = base_ini;
                invalidar_ultimas();
                int ini = ncod;
                expr();
                if (!eh_inteiro(ty)) falhar("el indice debe ser un entero");
                esperar_ficha(']');
                if (const_fim == ncod && const_ini == ini) {
                    ncod = ini;
                    invalidar_ultimas();
                    variavel(b->base, bm, bd + (uint32_t)(const_val * sc));
                    continue;
                }
                if (!pot) escalar(0, sc);
                int s2 = pot ? sib : 0;
                if (bm == 0x85 && (int32_t)bd >= -128 && (int32_t)bd <= 127) {
                    e3(0x8D, 0x44, s2 | 0x05); e1((int)(bd & 0xFF));      /* lea eax, [ebp+eax*sc+d8] */
                } else if (bm == 0x85) { e3(0x8D, 0x84, s2 | 0x05); e4(bd); }   /* lea eax, [ebp+eax*sc+d] */
                else if (pot) { e3(0x8D, 0x04, s2 | 0x05); e4(bd); }            /* lea eax, [eax*sc+C] */
                else { e1(0x05); e4(bd); }                                     /* add eax, C */
                ty = b->base;
                carregar(ty);
                continue;
            }
            int p0 = ncod;
            push_eax();
            expr();
            if (!eh_inteiro(ty)) falhar("el indice debe ser un entero");
            esperar_ficha(']');
            if (otim(OTIM_IMEDIATOS) && const_fim == ncod && const_ini == p0 + 1 && cod[p0] == 0x50) {
                /* índice constante: sólo suma el desplazamiento (sin pila) */
                ncod = p0;
                invalidar_ultimas();
                if (const_val * sc) aplicar_imediato(0, 0, const_val * sc, 0x05);
            } else {
                if (otim(OTIM_IMEDIATOS) && carga_direta && ncod == carga_fim && carga_ini == p0 + 1 &&
                    cod[carga_ini] == 0x8B && cod[p0] == 0x50) {
                    /* el índice es una variable: "push eax" se convierte en "mov ecx, eax" antes de la carga */
                    int len = ncod - carga_ini;
                    e1(0);                            /* garantiza la holgura de un byte */
                    memmove(cod + p0 + 2, cod + p0 + 1, (size_t)len);
                    cod[p0] = 0x89; cod[p0 + 1] = 0xC1;
                    ncod = p0 + 2 + len;
                    invalidar_ultimas();
                } else if (!pot) {
                    escalar(0, sc);
                    pop_ecx();
                    e2(0x01, 0xC8);              /* add eax, ecx */
                    goto indexado;
                } else {
                    pop_ecx();
                }
                if (pot) e3(0x8D, 0x04, sib | 0x01);   /* lea eax, [ecx+eax*sc] */
                else { escalar(0, sc); e2(0x01, 0xC8); }
            }
          indexado:
            ty = b->base;
            carregar(ty);
        } else if (aceitar('(')) {
            tipo_t *f = ty;
            if (f->k == T_PTR && f->base->k == T_FUNC) f = f->base;
            if (f->k != T_FUNC) falhar("llamada a algo que no es una funcion");
            chamada(NULL, f, true);
        } else if (tt() == '.' || tt() == F_SETA) {
            bool seta = tt() == F_SETA;
            avancar();
            tipo_t *st = ty;
            if (seta) {
                st = decair(ty);
                if (st->k != T_PTR) falhar("-> en algo que no es puntero");
                st = st->base;
            }
            if (st->k != T_STRUCT) falhar("miembro de algo que no es struct");
            if (!st->completo) falhar("struct incompleta");
            nome_t *n = esperar_nome();
            membro_t *m = st->membros;
            while (m && m->nome != n) m = m->prox;
            if (!m) falhar("la struct no tiene el miembro '%s'", n->s);
            if (!seta && otim(OTIM_IMEDIATOS) && base_fim == ncod) {
                /* s.m con s variable: el desplazamiento entra en la dirección de la variable */
                int bm = base_modrm;
                uint32_t bd = base_disp;
                ncod = base_ini;
                invalidar_ultimas();
                variavel(m->t, bm, bd + (uint32_t)m->desloc);
                continue;
            }
            if (m->desloc) aplicar_imediato(0, 0, m->desloc, 0x05);   /* add eax, desloc */
            ty = m->t;
            carregar(ty);
        } else if (tt() == F_MAISMAIS || tt() == F_MENOSMENOS) {
            bool mais = tt() == F_MAISMAIS;
            avancar();
            incremento(mais, true);
        } else {
            return;
        }
    }
}

static void unario(void)
{
    int t = tt();
    if (t == '-' || t == '~') {
        avancar();
        unario();
        if (eh_real(ty) && t == '-' && usar_sse) {
            sse(pre_t(ty), 0x10, 0x3D); e4(endereco_constante(-0.0, ty->k == T_FLOAT));   /* movsd xmm7, [-0.0] */
            sse(0, 0x57, XMM(0, 7));        /* xorps xmm0, xmm7: cambia el signo */
            carga_fim = -1;
            invalidar_ultimas();
            return;
        }
        if (eh_real(ty) && t == '-') {
            f2(0xD9, 0xE0);                 /* fchs */
            carga_fim = -1;
            invalidar_ultimas();
            return;
        }
        ty = promover(ty);
        if (eh_longlong(ty)) {
            /* Negar de 64 bits: not + 1 (el acarreo lo lleva la suma). */
            if (t == '-') {
                e2(0xF7, 0xD0); e2(0xF7, 0xD2);            /* not eax; not edx */
                e3(0x83, 0xC0, 0x01); e3(0x83, 0xD2, 0x00); /* add eax, 1; adc edx, 0 */
            } else { e2(0xF7, 0xD0); e2(0xF7, 0xD2); }   /* not eax; not edx */
            carga_fim = -1;
            return;
        }
        e2(0xF7, t == '-' ? 0xD8 : 0xD0);   /* neg / not eax */
        carga_fim = -1;
        return;
    }
    if (t == '+') {
        avancar();
        unario();
        if (!eh_real(ty)) ty = promover(ty);
        carga_fim = -1;
        invalidar_ultimas();
        return;
    }
    if (t == '!') {
        avancar();
        unario();
        testar(ty);
        e3(0x0F, 0x94, 0xC0);   /* sete al */
        e3(0x0F, 0xB6, 0xC0);   /* movzx eax, al */
        ty = &t_int;
        return;
    }
    if (t == '*') {
        avancar();
        unario();
        tipo_t *p = decair(ty);
        if (p->k != T_PTR) falhar("* en algo que no es puntero");
        ty = p->base;
        if (ty->k == T_VOID) falhar("* en un puntero a void");
        carga_fim = -1;
        carregar(ty);
        return;
    }
    if (t == '&') {
        avancar();
        unario();
        if (ty->k == T_VETOR || ty->k == T_STRUCT || ty->k == T_FUNC) {
            ty = ponteiro(ty);
            carga_fim = -1;
            return;
        }
        exigir_endereco();
        ty = ponteiro(ty);
        return;
    }
    if (t == F_MAISMAIS || t == F_MENOSMENOS) {
        avancar();
        unario();
        incremento(t == F_MAISMAIS, false);
        return;
    }
    if (t == P_SIZEOF) {
        avancar();
        if (tt() == '(' && eh_nome_de_tipo(1)) {
            avancar();
            tipo_t *s = nome_de_tipo();
            esperar_ficha(')');
            if (s->k == T_STRUCT && !s->completo) falhar("sizeof de struct incompleta");
            mov_eax((uint32_t)s->tam);
            ty = &t_uint;
            return;
        }
        int salvo = ncod;
        bool d = descartando;
        descartando = true;
        unario();
        descartando = d;
        ncod = salvo;
        carga_fim = -1;
        invalidar_ultimas();
        mov_eax((uint32_t)ty->tam);
        ty = &t_uint;
        return;
    }
    if (t == '(' && eh_nome_de_tipo(1)) {   /* conversión: (tipo) valor */
        avancar();
        tipo_t *c = nome_de_tipo();
        esperar_ficha(')');
        unario();
        if (c->k == T_STRUCT || c->k == T_VETOR) falhar("conversion a struct o vector");
        if (c->k == T_VOID) descartar(ty);
        else if (eh_real(c) || eh_real(ty)) {
            if (c->k == T_PTR) falhar("conversion de float/double a puntero");
            converter_valor(ty, c);
            if (c->k == T_FLOAT) arredondar_float();
        } else converter_valor(ty, c);   /* de 64 bits a 64 bits no hace nada (D128) */
        ty = c;
        carga_fim = -1;
        invalidar_ultimas();
        return;
    }
    posfixo();
}

/* ------------------------------------------------------------ long long (D128) ----
 *
 * El valor de 64 bits va en EDX:EAX (alto: bajo). Las cuentas que no se pueden
 * hacer con instrucciones de 32 bits (multiplicar, dividir, desplazar,
 * comparar) las hace la biblioteca: el compilador llama a una función auxiliar
 * que recibe punteros a los dos operandos y a un hueco donde deja el
 * resultado (8 bytes, la palabra baja primero).
 *
 * Los tres huecos (24 bytes) se reservan en la pila de la función que llama
 * (EBP), como variables locales más:
 *    0: el operando izquierdo   1: el operando derecho   2: el resultado */

static int ll_hueco_base;

static int ll_hueco(int n)
{
    if (!ll_hueco_base) {
        local_desloc -= 24;
        local_desloc &= ~3;
        if (-local_desloc > local_max) local_max = -local_desloc;
        ll_hueco_base = local_desloc;
    }
    return ll_hueco_base + n * 8;
}

/* La función auxiliar de la biblioteca. Todavía no está compilada (la
 * biblioteca va después del programa): se crea el símbolo y la definición de
 * la biblioteca lo reutiliza. */
static simbolo_t *ll_auxiliar(const char *nome)
{
    nome_t *n = nome_de(nome, (int)tam_texto(nome));
    simbolo_t *s = procurar(n);
    if (!s) {
        tipo_t *f = memoria(sizeof *f);
        f->k = T_FUNC;
        f->tam = f->alin = 1;
        f->base = &t_int;
        f->n = -1;
        s = novo_simbolo(n, S_FUNC, f, E_GLOBAL);
    }
    return s;
}

/* Llama a la auxiliar: el operando izquierdo está en la pila (8 bytes) y el
 * derecho en EDX:EAX. Los dos van a sus huecos y el resultado (8 bytes) queda
 * en EDX:EAX. */
static void chamar_ll(const char *nome)
{
    simbolo_t *s = ll_auxiliar(nome);
    int da = ll_hueco(0), db = ll_hueco(1), dr = ll_hueco(2);
    e2(0x89, 0x85); e4((uint32_t)db);         /* mov [ebp+db], eax */
    e2(0x89, 0x95); e4((uint32_t)(db + 4));   /* mov [ebp+db+4], edx */
    e1(0x58);                                 /* pop eax (bajo del izquierdo) */
    e1(0x5A);                                 /* pop edx (alto del izquierdo) */
    e2(0x89, 0x85); e4((uint32_t)da);         /* mov [ebp+da], eax */
    e2(0x89, 0x95); e4((uint32_t)(da + 4));   /* mov [ebp+da+4], edx */
    /* Los argumentos se apilan al revés (como en C): el primero queda arriba. */
    e2(0x8D, 0x8D); e4((uint32_t)dr);   /* lea ecx, [ebp+dr] */
    e1(0x51);                                 /* push ecx (el resultado) */
    e2(0x8D, 0x8D); e4((uint32_t)db);   /* lea ecx, [ebp+db] */
    e1(0x51);                                 /* push ecx (el derecho) */
    e2(0x8D, 0x8D); e4((uint32_t)da);   /* lea ecx, [ebp+da] */
    e1(0x51);                                 /* push ecx (el izquierdo) */
    chamar_funcao(s);
    e2(0x81, 0xC4); e4(12);                   /* add esp, 12 */
    e2(0x8B, 0x95); e4((uint32_t)(dr + 4));   /* mov edx, [ebp+dr+4] */
    e2(0x8B, 0x85); e4((uint32_t)dr);         /* mov eax, [ebp+dr] */
}

/* De 64 bits a double: lo hace la biblioteca. El valor está en EDX:EAX. */
static void ll_para_real(void)
{
    int da = ll_hueco(0), dr = ll_hueco(1);
    e2(0x89, 0x85); e4((uint32_t)da);         /* mov [ebp+da], eax */
    e2(0x89, 0x95); e4((uint32_t)(da + 4));   /* mov [ebp+da+4], edx */
    e2(0x8D, 0x8D); e4((uint32_t)dr);   /* lea ecx, [ebp+dr] */
    e1(0x51);                                 /* push ecx (el resultado) */
    e2(0x8D, 0x8D); e4((uint32_t)da);   /* lea ecx, [ebp+da] */
    e1(0x51);                                 /* push ecx (el valor) */
    chamar_funcao(ll_auxiliar("__ll_to_double"));
    e2(0x81, 0xC4); e4(8);                    /* add esp, 8 */
    if (usar_sse) { sse(0xF2, 0x10, 0x85); e4((uint32_t)dr); }   /* movsd xmm0, [ebp+dr] */
    else { f2(0xDD, 0x85); e4((uint32_t)dr); }                    /* fld qword [ebp+dr] */
    carga_fim = -1;
    invalidar_ultimas();
}

/* De double a 64 bits: lo hace la biblioteca. El real está en XMM0 (SSE2) o
 * en ST(0) (x87). */
static void real_para_ll(void)
{
    int da = ll_hueco(0), dr = ll_hueco(1);
    if (usar_sse) { sse(0xF2, 0x11, 0x85); e4((uint32_t)da); }    /* movsd [ebp+da], xmm0 */
    else { f2(0xDD, 0x9D); e4((uint32_t)da); }                    /* fstp qword [ebp+da] */
    e2(0x8D, 0x8D); e4((uint32_t)dr);   /* lea ecx, [ebp+dr] */
    e1(0x51);                                 /* push ecx (el resultado) */
    e2(0x8D, 0x8D); e4((uint32_t)da);   /* lea ecx, [ebp+da] */
    e1(0x51);                                 /* push ecx (el valor) */
    chamar_funcao(ll_auxiliar("__ll_from_double"));
    e2(0x81, 0xC4); e4(8);                    /* add esp, 8 */
    e2(0x8B, 0x95); e4((uint32_t)(dr + 4));   /* mov edx, [ebp+dr+4] */
    e2(0x8B, 0x85); e4((uint32_t)dr);         /* mov eax, [ebp+dr] */
    carga_fim = -1;
    invalidar_ultimas();
}

/* Una cuenta entre dos valores de 64 bits. El izquierdo (tipo a) está en la
 * pila (8 bytes) y el derecho (tipo b) en EDX:EAX. */
static void operar_ll(int op, tipo_t *a, tipo_t *b, int forma)
{
    a = decair(a);
    b = decair(b);
    bool sem = eh_ll_sem_sinal(a) || eh_ll_sem_sinal(b);
    if (forma == ESQ_INTEIRO) {
        /* El izquierdo era de 32 bits: se extiende a 64 (queda en 8 bytes). */
        e1(0x58);                    /* pop eax */
        e2(0x89, 0xC2);              /* mov edx, eax */
        if (eh_ll_sem_sinal(a)) e2(0x31, 0xD2);
        else e3(0xC1, 0xFA, 0x1F);
        e1(0x52); e1(0x50);          /* push edx; push eax */
    }
    if (!eh_longlong(b)) {
        /* El derecho era de 32 bits: se extiende. */
        e2(0x89, 0xC2);              /* mov edx, eax */
        if (eh_ll_sem_sinal(b)) e2(0x31, 0xD2);
        else e3(0xC1, 0xFA, 0x1F);
    }
    switch (op) {
    case '+': e3(0x03, 0x04, 0x24); e3(0x13, 0x54, 0x24); e1(0x04); e3(0x83, 0xC4, 0x08); break;
    case '-':
        /* El derecho está en EDX:EAX y el izquierdo en la pila: se resta al revés
         * y se invierte el resultado (not + 1), para que quede izquierda - derecha. */
        e3(0x2B, 0x04, 0x24); e3(0x1B, 0x54, 0x24); e1(0x04);   /* sub eax, [esp]; sbb edx, [esp+4] */
        e2(0xF7, 0xD0); e2(0xF7, 0xD2);                 /* not eax; not edx */
        e3(0x83, 0xC0, 0x01); e3(0x83, 0xD2, 0x00);     /* add eax, 1; adc edx, 0 */
        e3(0x83, 0xC4, 0x08);                           /* add esp, 8 */
        break;
    case '&': e3(0x23, 0x04, 0x24); e3(0x23, 0x54, 0x24); e1(0x04); e3(0x83, 0xC4, 0x08); break;
    case '|': e3(0x0B, 0x04, 0x24); e3(0x0B, 0x54, 0x24); e1(0x04); e3(0x83, 0xC4, 0x08); break;
    case '^': e3(0x33, 0x04, 0x24); e3(0x33, 0x54, 0x24); e1(0x04); e3(0x83, 0xC4, 0x08); break;
    case '*': chamar_ll("__ll_mul"); break;
    case '/': chamar_ll(sem ? "__ll_udiv" : "__ll_div"); break;
    case '%': chamar_ll(sem ? "__ll_umod" : "__ll_mod"); break;
    case F_SHL: chamar_ll("__ll_shl"); break;
    case F_SHR: chamar_ll(sem ? "__ll_shr" : "__ll_sar"); break;
    case F_IGUAL: case F_DIF: case '<': case '>': case F_MENOR_IG: case F_MAIOR_IG:
        chamar_ll(sem ? "__ll_ucmp" : "__ll_cmp");
        e3(0x83, 0xF8, 0x00);      /* cmp eax, 0 */
        switch (op) {
        case F_IGUAL: setcc(0x94); break;      /* sete */
        case F_DIF: setcc(0x95); break;        /* setne */
        case '<': setcc(0x9C); break;          /* setl */
        case '>': setcc(0x9F); break;          /* setg */
        case F_MENOR_IG: setcc(0x9E); break;   /* setle */
        default: setcc(0x9D); break;          /* setge */
        }
        ty = &t_int;
        return;
    default:
        falhar("operador invalido con long long");
    }
    ty = aritmetico(a, b);
}

/* ------------------------------------------------------------ operaciones */

static void operar(int op, tipo_t *a, tipo_t *b)
{
    a = decair(a);
    b = decair(b);
    if (!eh_escalar(a) || !eh_escalar(b)) falhar("operandos invalidos");
    bool sem = a->k == T_UINT || b->k == T_UINT || a->k == T_PTR || b->k == T_PTR;
    switch (op) {
    case '+':
        if (a->k == T_PTR && b->k == T_PTR) falhar("suma de dos punteros");
        if (a->k == T_PTR) { escalar(0, passo(a)); e2(0x01, 0xC8); ty = a; return; }
        if (b->k == T_PTR) { escalar(1, passo(b)); e2(0x01, 0xC8); ty = b; return; }
        e2(0x01, 0xC8);
        ty = aritmetico(a, b);
        return;
    case '-':
        if (a->k == T_PTR && b->k == T_PTR) {
            e2(0x29, 0xC1); e2(0x89, 0xC8);          /* eax = ecx - eax */
            int s = passo(a);
            if (s > 1) { e1(0xB9); e4((uint32_t)s); e1(0x99); e2(0xF7, 0xF9); }   /* idiv */
            ty = &t_int;
            return;
        }
        if (b->k == T_PTR) falhar("entero menos puntero");
        if (a->k == T_PTR) escalar(0, passo(a));
        e2(0x29, 0xC1); e2(0x89, 0xC8);
        ty = a->k == T_PTR ? a : aritmetico(a, b);
        return;
    case '*':
        e3(0x0F, 0xAF, 0xC1);   /* imul eax, ecx */
        ty = aritmetico(a, b);
        return;
    case '/': case '%':
        e1(0x91);               /* xchg eax, ecx */
        ty = aritmetico(a, b);
        if (ty == &t_uint) { e2(0x31, 0xD2); e2(0xF7, 0xF1); }   /* xor edx,edx; div ecx */
        else { e1(0x99); e2(0xF7, 0xF9); }                        /* cdq; idiv ecx */
        if (op == '%') e2(0x89, 0xD0);
        return;
    case F_SHL: case F_SHR:
        e1(0x91);
        ty = promover(a);
        e2(0xD3, op == F_SHL ? 0xE0 : (ty == &t_uint ? 0xE8 : 0xF8));
        return;
    case '&': e2(0x21, 0xC8); ty = aritmetico(a, b); return;
    case '|': e2(0x09, 0xC8); ty = aritmetico(a, b); return;
    case '^': e2(0x31, 0xC8); ty = aritmetico(a, b); return;
    }
    int cc;
    switch (op) {
    case F_IGUAL: cc = 0x94; break;
    case F_DIF: cc = 0x95; break;
    case '<': cc = sem ? 0x92 : 0x9C; break;
    case F_MENOR_IG: cc = sem ? 0x96 : 0x9E; break;
    case '>': cc = sem ? 0x97 : 0x9F; break;
    default: cc = sem ? 0x93 : 0x9D; break;   /* >= */
    }
    e2(0x39, 0xC1);             /* cmp ecx, eax */
    setcc(cc);
    ty = &t_int;
}

/* Comparación ya hecha (flags como "ucomisd izquierdo, derecho"): CF = 1 si
 * menor, ZF = 1 si igual; con NaN, ZF = PF = CF = 1. Por eso "<" y "<="
 * comparan con los lados cambiados y usan seta/setae: NaN da falso. */
static void resultado_comparacao(int op)
{
    ty = &t_int;
    switch (op) {
    case '<': case '>': setcc(0x97); return;                  /* seta */
    case F_MENOR_IG: case F_MAIOR_IG: setcc(0x93); return;    /* setae */
    case F_IGUAL: e3(0x0F, 0x94, 0xC0); e3(0x0F, 0x9B, 0xC1); e2(0x20, 0xC8); break;   /* sete; setnp; and */
    default: e3(0x0F, 0x95, 0xC0); e3(0x0F, 0x9A, 0xC1); e2(0x08, 0xC8); break;        /* setne; setp; or */
    }
    e3(0x0F, 0xB6, 0xC0);               /* movzx eax, al */
}

/* SSE2: izquierdo en XMM l (tipo ta), derecho en XMM0 (tipo tb) o, si
 * mem_modrm >= 0, en memoria (tipo tb). Resultado en XMM0 (o EAX). */
static void conta_sse(int op, tipo_t *r, int l, tipo_t *ta, tipo_t *tb, int mem_modrm, uint32_t disp)
{
    int pre = r->k == T_FLOAT ? 0xF3 : 0xF2;
    int x = op == '+' ? 0x58 : op == '*' ? 0x59 : op == '-' ? 0x5C : op == '/' ? 0x5E : 0x2E;
    /* Los dos operandos en la precisión del resultado: cvtss2sd o cvtsd2ss. */
    if (ta->k != r->k) sse(ta->k == T_FLOAT ? 0xF3 : 0xF2, 0x5A, XMM(l, l));
    if (mem_modrm >= 0) {
        /* l = 0: "addsd xmm0, [mem]" etc. */
        if (tb->k != r->k) {
            sse(tb->k == T_FLOAT ? 0xF3 : 0xF2, 0x5A, MEM_XMM7(mem_modrm)); ed(disp);   /* cvtss2sd/cvtsd2ss xmm7, [mem] */
        } else if (x == 0x2E) {
            sse(pre, 0x10, MEM_XMM7(mem_modrm)); ed(disp);    /* comparación: movsd xmm7, [mem] */
        } else {
            sse(pre, x, mem_modrm); ed(disp);
            ty = r;
            return;
        }
        if (x != 0x2E) { sse(pre, x, XMM(0, 7)); ty = r; return; }
        /* izquierdo en XMM0, derecho en XMM7 */
        if (op == '<' || op == F_MENOR_IG) sse(pre == 0xF3 ? 0 : 0x66, 0x2E, XMM(7, 0));
        else sse(pre == 0xF3 ? 0 : 0x66, 0x2E, XMM(0, 7));
        resultado_comparacao(op);
        return;
    }
    if (tb->k != r->k) sse(tb->k == T_FLOAT ? 0xF3 : 0xF2, 0x5A, XMM(0, 0));
    switch (op) {
    case '+': case '*': sse(pre, x, XMM(0, l)); ty = r; return;   /* xmm0 = xmm0 op xl */
    case '-': case '/':
        sse(pre, x, XMM(l, 0));         /* xl = xl op xmm0 */
        sse(0, 0x28, XMM(0, l));        /* movaps xmm0, xl */
        ty = r;
        return;
    }
    if (op == '<' || op == F_MENOR_IG) sse(pre == 0xF3 ? 0 : 0x66, 0x2E, XMM(0, l));   /* derecho con izquierdo */
    else sse(pre == 0xF3 ? 0 : 0x66, 0x2E, XMM(l, 0));                              /* izquierdo con derecho */
    resultado_comparacao(op);
}

/* Operación con algún operando real. El izquierdo (tipo a) se guardó por
 * salvar_esquerdo() entre esq_ini y esq_fim en el código; el
 * derecho (tipo b) acaba de calcularse. lit_ini >= 0: el izquierdo es la
 * constante entera lit_val ("mov eax, n; push eax" en lit_ini). Resultado
 * en XMM0/ST(0) (cuentas) o EAX (comparaciones). */
static void operar_real(int op, tipo_t *a, tipo_t *b, int forma, int esq_ini, int esq_fim, int lit_ini, int lit_val)
{
    a = decair(a);
    b = decair(b);
    if (a->k == T_PTR || b->k == T_PTR) falhar("operandos invalidos (puntero y float/double)");
    if (op != '+' && op != '-' && op != '*' && op != '/' && op != F_IGUAL && op != F_DIF &&
        op != '<' && op != '>' && op != F_MENOR_IG && op != F_MAIOR_IG)
        falhar("operador invalido con float/double");
    /* float con float o con entero da float (conversiones usuales de C). */
    tipo_t *r = (a->k == T_DOUBLE || b->k == T_DOUBLE) ? &t_double : &t_float;
    int reg = 0;                                      /* SSE2: XMM del izquierdo guardado */
    if (forma == ESQ_REG) reg = x87_guardados--;
    if (!eh_real(b) && ncod == const_fim && const_ini == esq_fim) {
        inteiro_para_real(b);           /* "x * 2": el 2 se convierte en constante double (y operando simple) */
        b = ty;
    }
    bool simples = eh_real(a) && eh_real(b) && esq_fim == simples_ini && ncod == simples_fim;
    if (!eh_real(a) && lit_ini >= 0) {
        /* "2 * x": el izquierdo era "mov eax, 2; push eax"; se convierte en un nop de
         * 6 bytes y la cuenta usa la constante double en memoria. */
        static const uint8_t nop6[] = {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00};
        memcpy(cod + lit_ini, nop6, 6);
        if (!eh_real(b)) { inteiro_para_real(b); b = &t_double; }
        double v = a->k == T_UINT ? (double)(uint32_t)lit_val : (double)lit_val;
        uint32_t c = endereco_constante(v, false);
        if (usar_sse) {
            sse(0xF2, 0x10, 0x3D); e4(c);   /* movsd xmm7, [constante] */
            conta_sse(op, r, 7, &t_double, b, -1, 0);
            return;
        }
        int x = op == '+' ? 0 : op == '*' ? 1 : op == '-' ? 5 : op == '/' ? 7 : -1;   /* fsubr/fdivr: c - ST0, c / ST0 */
        if (x >= 0) {
            f2(0xDC, 0x05 | x << 3); e4(c);
            ty = r;
            return;
        }
        f2(0xDD, 0x05); e4(c);          /* fld: ST0 = izquierdo, ST1 = derecho */
        if (op == '<' || op == F_MENOR_IG) f2(0xD9, 0xC9);   /* fxch */
        f2(0xDF, 0xE9);                 /* fucomip st0, st1 */
        fstp_st0();
        resultado_comparacao(op);
        return;
    }
    if (simples) {
        /* El derecho es una variable o constante: se suprime su código y el
         * del izquierdo va a la pila/registro. "x * y" se convierte en: carga x;
         * mulsd/fmul [y]. */
        tipo_t *st = simples_t;
        int modrm = simples_modrm;
        uint32_t disp = simples_disp;
        ncod = esq_ini;
        invalidar_ultimas();
        if (usar_sse) { conta_sse(op, r, 0, a, st, modrm, disp); return; }
        int x = op == '+' ? 0 : op == '*' ? 1 : op == '-' ? 4 : op == '/' ? 6 : -1;
        if (x >= 0) {
            f2(op_conta(st), modrm | x << 3); ed(disp);   /* fadd/fmul/fsub/fdiv [mem] */
            ty = r;
            return;
        }
        f2(op_carga(st), modrm); ed(disp);                 /* fld [mem]: ST0 = derecho, ST1 = izquierdo */
    } else {
        if (!eh_real(b)) { inteiro_para_real(b); b = &t_double; }
        if (usar_sse) {
            tipo_t *ta = a;
            if (forma == ESQ_MEMORIA) {
                sse(0xF2, 0x10, 0x3C); e1(0x24);   /* movsd xmm7, [esp] */
                esp_mais(8);
                reg = 7;
            } else if (forma == ESQ_INTEIRO) {
                if (a->k == T_UINT) {
                    e1(0x58);           /* pop eax */
                    e2(0x6A, 0x00);     /* push 0 */
                    push_eax();
                    f3(0xDF, 0x2C, 0x24);   /* fild qword [esp] */
                    f3(0xDD, 0x1C, 0x24);   /* fstp qword [esp] */
                    sse(0xF2, 0x10, 0x3C); e1(0x24);   /* movsd xmm7, [esp] */
                    esp_mais(8);
                } else {
                    sse(0xF2, 0x2A, 0x3C); e1(0x24);   /* cvtsi2sd xmm7, [esp] */
                    esp_mais(4);
                }
                reg = 7;
                ta = &t_double;
            }
            conta_sse(op, r, reg, ta, b, -1, 0);
            return;
        }
        if (forma != ESQ_REG) {
            /* x87: el izquierdo vuelve de la pila del procesador por encima. */
            if (forma == ESQ_MEMORIA) {
                f3(0xDD, 0x04, 0x24);   /* fld qword [esp] */
                esp_mais(8);
            } else if (a->k == T_UINT) {
                e1(0x58);               /* pop eax */
                e2(0x6A, 0x00);         /* push 0 */
                push_eax();
                f3(0xDF, 0x2C, 0x24);   /* fild qword [esp] */
                esp_mais(8);
            } else {
                f3(0xDB, 0x04, 0x24);   /* fild dword [esp] */
                esp_mais(4);
            }
            /* ST0 = izquierdo, ST1 = derecho */
            switch (op) {
            case '+': f2(0xDE, 0xC1); ty = r; return;   /* faddp st1 */
            case '*': f2(0xDE, 0xC9); ty = r; return;   /* fmulp st1 */
            case '-': f2(0xDE, 0xE1); ty = r; return;   /* fsubrp st1: ST1 = ST0 - ST1 */
            case '/': f2(0xDE, 0xF1); ty = r; return;   /* fdivrp st1: ST1 = ST0 / ST1 */
            }
            if (op == '<' || op == F_MENOR_IG) f2(0xD9, 0xC9);   /* fxch */
            f2(0xDF, 0xE9);             /* fucomip st0, st1 */
            fstp_st0();
            resultado_comparacao(op);
            return;
        }
    }
    /* x87, ST0 = derecho, ST1 = izquierdo */
    switch (op) {
    case '+': f2(0xDE, 0xC1); ty = r; return;   /* faddp st1 */
    case '*': f2(0xDE, 0xC9); ty = r; return;   /* fmulp st1 */
    case '-': f2(0xDE, 0xE9); ty = r; return;   /* fsubp st1: ST1 = ST1 - ST0 */
    case '/': f2(0xDE, 0xF9); ty = r; return;   /* fdivp st1: ST1 = ST1 / ST0 */
    }
    if (op == '>' || op == F_MAIOR_IG) f2(0xD9, 0xC9);   /* fxch: ST0 = izquierdo */
    f2(0xDF, 0xE9);                     /* fucomip st0, st1 */
    fstp_st0();
    resultado_comparacao(op);
}

/* Enteros con el derecho constante: "add eax, 5", "cmp eax, 10"... sin la
 * pila. EAX = izquierdo. */
static bool operar_imediato(int op, tipo_t *a, tipo_t *b, int v)
{
    a = decair(a);
    if (!eh_escalar(a) || eh_real(a)) return false;
    bool sem = a->k == T_UINT || b->k == T_UINT || a->k == T_PTR;
    bool red = otim(OTIM_REDUCAO);
    int cc = -1;
    switch (op) {
    case '+': case '-':
        if (a->k == T_PTR) { v *= passo(a); ty = a; } else ty = aritmetico(a, b);
        if (v == 0 && red) return true;                 /* x + 0, x - 0 */
        aplicar_imediato(op == '+' ? 0 : 5, 0, v, op == '+' ? 0x05 : 0x2D);   /* add/sub eax, v */
        return true;
    case '*':
        ty = aritmetico(a, b);
        if (red) {
            int k = v > 0 ? pot2((uint32_t)v) : -1;
            if (v == 1) return true;                    /* x * 1 */
            if (v == 0) { e2(0x31, 0xC0); return true; }   /* x * 0 */
            if (k > 0) { e3(0xC1, 0xE0, k); return true; }  /* shl eax, k */
            if (v == 3 || v == 5 || v == 9) {            /* lea eax, [eax + eax*(v-1)] */
                e3(0x8D, 0x04, (v == 3 ? 0x40 : v == 5 ? 0x80 : 0xC0));
                return true;
            }
        }
        if (otim(OTIM_IMEDIATOS) && v >= -128 && v <= 127) { e3(0x6B, 0xC0, v & 0xFF); return true; }
        e2(0x69, 0xC0); e4((uint32_t)v);                /* imul eax, eax, v */
        return true;
    case '&':
        ty = aritmetico(a, b);
        if (red && v == -1) return true;
        if (red && v == 0) { e2(0x31, 0xC0); return true; }
        aplicar_imediato(4, 0, v, 0x25);
        return true;
    case '|':
        ty = aritmetico(a, b);
        if (red && v == 0) return true;
        aplicar_imediato(1, 0, v, 0x0D);
        return true;
    case '^':
        ty = aritmetico(a, b);
        if (red && v == 0) return true;
        aplicar_imediato(6, 0, v, 0x35);
        return true;
    case '/': case '%':
        if (a->k == T_PTR || v == 0 || !otim(OTIM_REDUCAO)) return false;   /* v == 0: el error ocurre en tiempo de ejecución */
        ty = aritmetico(a, b);
        dividir_constante(op == '%', ty == &t_uint, v);
        return true;
    case F_SHL: case F_SHR:
        ty = promover(a);
        if (red && (v & 31) == 0) return true;
        e3(0xC1, op == F_SHL ? 0xE0 : (ty == &t_uint ? 0xE8 : 0xF8), v & 31);
        return true;
    case F_IGUAL: cc = 0x94; break;
    case F_DIF: cc = 0x95; break;
    case '<': cc = sem ? 0x92 : 0x9C; break;
    case F_MENOR_IG: cc = sem ? 0x96 : 0x9E; break;
    case '>': cc = sem ? 0x97 : 0x9F; break;
    case F_MAIOR_IG: cc = sem ? 0x93 : 0x9D; break;
    default: return false;
    }
    aplicar_imediato(7, 0, v, 0x3D);         /* cmp eax, v */
    setcc(cc);
    ty = &t_int;
    return true;
}

/* El derecho es una variable de 32 bits leída con una instrucción sola
 * ("mov eax, [mem]", justo después del "push eax" del izquierdo): la cuenta usa la
 * memoria directo ("add eax, [mem]", "cmp eax, [mem]"), sin pila. */
static bool operar_memoria(int op, tipo_t *a, int esq_ini, int esq_fim)
{
    if (!otim(OTIM_IMEDIATOS) || !carga_direta || ncod != carga_fim || carga_ini != esq_fim ||
        esq_fim - esq_ini != 1 || cod[carga_ini] != 0x8B || cod[esq_ini] != 0x50)
        return false;
    a = decair(a);
    tipo_t *b = decair(ty);
    if (!eh_escalar(a) || !eh_escalar(b) || eh_real(a) || eh_real(b)) return false;
    bool ponteiro = a->k == T_PTR || b->k == T_PTR;
    int cc = -1, opc = 0;
    bool sem = a->k == T_UINT || b->k == T_UINT || ponteiro;
    switch (op) {
    case '+': opc = 0x03; break;
    case '-': opc = 0x2B; break;
    case '&': opc = 0x23; break;
    case '|': opc = 0x0B; break;
    case '^': opc = 0x33; break;
    case '*': case '/': case '%': case F_SHL: case F_SHR: break;
    case F_IGUAL: cc = 0x94; break;
    case F_DIF: cc = 0x95; break;
    case '<': cc = sem ? 0x92 : 0x9C; break;
    case F_MENOR_IG: cc = sem ? 0x96 : 0x9E; break;
    case '>': cc = sem ? 0x97 : 0x9F; break;
    case F_MAIOR_IG: cc = sem ? 0x93 : 0x9D; break;
    default: return false;
    }
    if (ponteiro && cc < 0) return false;
    int modrm = direta_modrm;
    uint32_t disp = direta_disp;
    ncod = esq_ini;                     /* quita el "push eax" y la carga */
    invalidar_ultimas();
    if (cc >= 0) {
        e2(0x3B, modrm); ed(disp);      /* cmp eax, [mem] */
        setcc(cc);
        ty = &t_int;
        return true;
    }
    if (opc) {
        e2(opc, modrm); ed(disp);       /* add/sub/and/or/xor eax, [mem] */
        ty = aritmetico(a, b);
        return true;
    }
    if (op == '*') {
        e3(0x0F, 0xAF, modrm); ed(disp);   /* imul eax, [mem] */
        ty = aritmetico(a, b);
        return true;
    }
    e2(0x8B, modrm | 0x08); ed(disp);   /* mov ecx, [mem] */
    if (op == F_SHL || op == F_SHR) {
        ty = promover(a);
        e2(0xD3, op == F_SHL ? 0xE0 : (ty == &t_uint ? 0xE8 : 0xF8));   /* shl/shr/sar eax, cl */
        return true;
    }
    ty = aritmetico(a, b);
    if (ty == &t_uint) { e2(0x31, 0xD2); e2(0xF7, 0xF1); }   /* xor edx, edx; div ecx */
    else { e1(0x99); e2(0xF7, 0xF9); }                        /* cdq; idiv ecx */
    if (op == '%') e2(0x89, 0xD0);
    return true;
}

/* Operación binaria con el izquierdo (tipo a) ya en la pila. lit_ini: si el
 * izquierdo es una constante entera, dónde está su "mov eax" (si no, -1). */
static void operar_binario(int op, tipo_t *a, int forma, int esq_ini, int esq_fim, int lit_ini, int lit_val)
{
    if (eh_longlong(a) || eh_longlong(ty)) {
        /* Cuentas de 64 bits (D128): el izquierdo y el derecho se ponen en
         * EDX:EAX y las cuenta la biblioteca cuando no hay instrucción. */
        operar_ll(op, a, ty, forma);
        carga_fim = -1;
        return;
    }
    if (eh_real(a) || eh_real(ty)) {
        operar_real(op, a, ty, forma, esq_ini, esq_fim, lit_ini, lit_val);
    } else if (ncod == const_fim && const_ini == esq_fim && eh_inteiro(ty) && esq_fim - esq_ini == 1) {
        int v = const_val;
        tipo_t *b = ty;
        ncod = esq_ini;                 /* quita el "push eax" y el "mov eax, v" */
        invalidar_ultimas();
        if (lit_ini >= 0 && otim(OTIM_REDUCAO) && eh_inteiro(decair(a)) &&
            (op == '+' || op == '-' || op == '*' || op == '&' || op == '|' || op == '^')) {
            /* constante op constante: la cuenta se hace aquí */
            uint32_t x = (uint32_t)lit_val, y = (uint32_t)v;
            uint32_t r = op == '+' ? x + y : op == '-' ? x - y : op == '*' ? x * y :
                         op == '&' ? x & y : op == '|' ? x | y : x ^ y;
            ncod = lit_ini;
            tipo_t *rt = aritmetico(decair(a), b);
            const_ini = ncod;
            mov_eax(r);
            const_fim = ncod;
            const_val = (int)r;
            ty = rt;
        } else if (!operar_imediato(op, a, b, v)) {
            e2(0x89, 0xC1);             /* mov ecx, eax */
            mov_eax((uint32_t)v);
            operar(op, a, b);
        }
    } else if (lit_ini >= 0 && lit_ini + 5 == esq_ini && otim(OTIM_IMEDIATOS) && carga_direta && ncod == carga_fim &&
               carga_ini == esq_fim && esq_fim - esq_ini == 1 && cod[carga_ini] == 0x8B &&
               eh_inteiro(decair(a)) && eh_inteiro(decair(ty)) &&
               (op == '+' || op == '*' || op == '&' || op == '|' || op == '^' || op == F_IGUAL || op == F_DIF ||
                op == '<' || op == '>' || op == F_MENOR_IG || op == F_MAIOR_IG)) {
        /* constante op variable: "2 * x" es "x * 2" (con el sentido cambiado en las comparaciones) */
        tipo_t *bt = ty;
        int modrm = direta_modrm;
        uint32_t disp = direta_disp;
        ncod = lit_ini;
        invalidar_ultimas();
        e2(0x8B, modrm); ed(disp);
        int op2 = op == '<' ? '>' : op == '>' ? '<' : op == F_MENOR_IG ? F_MAIOR_IG : op == F_MAIOR_IG ? F_MENOR_IG : op;
        operar_imediato(op2, bt, decair(a), lit_val);
    } else if (operar_memoria(op, a, esq_ini, esq_fim)) {
        /* hecho */
    } else {
        pop_ecx();
        operar(op, a, ty);
    }
    carga_fim = -1;
}

static void binario(int minimo)
{
    unario();
    for (;;) {
        int op = tt(), p = precedencia(op);
        if (!p || p < minimo) return;
        avancar();
        if (op == F_E || op == F_OU) {
            /* "a || b || c": todos los operandos saltan al mismo lugar
             * (una lista sola); el 0/1 se monta una vez, al final. En un "&&" un
             * operando que es él mismo "&&"/"||" salta directo (D108). */
            int cc = op == F_E ? 0x84 : 0x85;
            int l;
            if (op == F_E) l = saltar_se_falso(-1);
            else { testar(ty); l = saltar(cc, -1); }
            for (;;) {
                binario(p + 1);
                if (op == F_E) l = saltar_se_falso(l);
                else { testar(ty); l = saltar(cc, l); }
                if (!aceitar(op)) break;
            }
            logico_gravar(l, op == F_E);      /* si una condición viene justo después, el 0/1 ni se monta */
            mov_eax(op == F_E ? 1 : 0);
            int fim = saltar(0xE9, -1);
            remendar_aqui(l);
            mov_eax(op == F_E ? 0 : 1);
            remendar_aqui(fim);
            ty = &t_int;
            logico_fechar();
            continue;
        }
        tipo_t *a = ty;
        int lit_ini = !eh_real(a) && ncod == const_fim ? const_ini : -1, lit_val = const_val;
        int esq_ini = ncod;
        int forma = salvar_esquerdo(a);
        int esq_fim = ncod;
        binario(p + 1);
        operar_binario(op, a, forma, esq_ini, esq_fim, lit_ini, lit_val);
    }
}

static void condicional(void)
{
    binario(4);
    if (!aceitar('?')) return;
    int senao = saltar_se_falso(-1);
    expr();
    tipo_t *a = decair(ty);
    esperar_ficha(':');
    int fim = saltar(0xE9, -1);
    remendar_aqui(senao);
    condicional();
    tipo_t *b = decair(ty);
    if (eh_real(a) || eh_real(b)) {
        /* Las dos ramas tienen que dejar el valor en el mismo lugar y del mismo
         * manera (XMM0/ST0, float o double). */
        tipo_t *r = (a->k == T_DOUBLE || b->k == T_DOUBLE) ? &t_double : &t_float;
        converter_valor(b, r);
        if (!eh_real(a) || (usar_sse && a->k != r->k)) {
            int depois = saltar(0xE9, -1);
            remendar_aqui(fim);         /* el ramo "a" salta aquí y convierte */
            converter_valor(a, r);
            remendar_aqui(depois);
        } else {
            remendar_aqui(fim);
        }
        ty = r;
        return;
    }
    remendar_aqui(fim);
    if (a->k == T_PTR) ty = a;
    else if (b->k == T_PTR) ty = b;
    else if (a->k == T_VOID || b->k == T_VOID) ty = &t_void;
    else if (a->k == T_STRUCT) ty = a;
    else ty = aritmetico(a, b);
}

static int op_de_atribuicao(int t)
{
    switch (t) {
    case F_MAIS_AT: return '+';
    case F_MENOS_AT: return '-';
    case F_VEZES_AT: return '*';
    case F_DIV_AT: return '/';
    case F_MOD_AT: return '%';
    case F_E_AT: return '&';
    case F_OU_AT: return '|';
    case F_XOU_AT: return '^';
    case F_SHL_AT: return F_SHL;
    case F_SHR_AT: return F_SHR;
    }
    return 0;
}

static void copiar_struct(int tam)   /* ESI = origen, EDI = destino */
{
    e1(0xB9); e4((uint32_t)tam);   /* mov ecx, tam */
    e2(0xF3, 0xA4);                /* rep movsb */
}

static void expr_atrib(void)
{
    condicional();
    int t = tt();
    if (t == '=') {
        avancar();
        tipo_t *alvo = ty;
        if (alvo->k == T_STRUCT) {
            push_eax();
            expr_atrib();
            if (ty->k != T_STRUCT || ty->tam != alvo->tam) falhar("asignacion entre tipos distintos");
            e2(0x89, 0xC6);      /* mov esi, eax */
            e1(0x5F);            /* pop edi */
            e1(0x57);            /* push edi */
            copiar_struct(alvo->tam);
            e1(0x58);            /* pop eax */
            ty = alvo;
            carga_fim = -1;
            return;
        }
        if (alvo->k == T_VETOR) falhar("no se puede asignar a un vector entero");
        if (alvo->k == T_FUNC) falhar("no se puede asignar a una funcion");
        exigir_endereco();
        if (endereco_fim == ncod) {     /* variable simple: graba directo, sin pila */
            int modrm = direta_modrm;
            uint32_t disp = direta_disp;
            ncod = endereco_ini;
            expr_atrib();
            if (ty->k == T_STRUCT || ty->k == T_VOID) falhar("tipos incompatibles en la asignacion");
            converter_valor(ty, alvo);
            guardar_mem(alvo, modrm, disp);
            ty = alvo;
            carga_fim = -1;
            return;
        }
        push_eax();
        expr_atrib();
        if (ty->k == T_STRUCT || ty->k == T_VOID) falhar("tipos incompatibles en la asignacion");
        if (eh_real(alvo) || eh_real(ty)) {
            converter_valor(ty, alvo);
            pop_ecx();
            guardar(alvo, 0);
        } else {
            pop_ecx();
            guardar(alvo, 0);
            if (!eh_longlong(alvo)) converter(alvo);   /* el de 64 bits ya está (D128) */
        }
        ty = alvo;
        carga_fim = -1;
        return;
    }
    int op = op_de_atribuicao(t);
    if (!op) return;
    avancar();
    tipo_t *alvo = ty;
    if (!eh_escalar(alvo)) falhar("operador de asignacion en algo que no es numero ni puntero");
    exigir_endereco();
    if (endereco_fim == ncod) {         /* variable simple: "x += y" sin la pila */
        int modrm = direta_modrm;
        uint32_t disp = direta_disp;
        ncod = endereco_ini;
        if (eh_longlong(alvo)) {          /* D128: la dirección va a EAX */
            emitir_endereco(modrm, disp);
            carregar_de(alvo, 0x00);
        } else {
            carregar_de(alvo, modrm);
            ed(disp);
        }
        int ini = ncod;
        int forma = salvar_esquerdo(alvo);
        int fim = ncod;
        expr_atrib();
        operar_binario(op, alvo, forma, ini, fim, -1, 0);
        converter_valor(ty, alvo);
        guardar_mem(alvo, modrm, disp);
        ty = alvo;
        carga_fim = -1;
        return;
    }
    push_eax();
    carregar_de(alvo, 0x00);
    int esq_ini = ncod;
    int forma = salvar_esquerdo(alvo);
    int esq_fim = ncod;
    expr_atrib();
    if (eh_real(alvo) || eh_real(ty)) {
        operar_binario(op, alvo, forma, esq_ini, esq_fim, -1, 0);
        converter_valor(ty, alvo);
        pop_ecx();
        guardar(alvo, 0);
    } else {
        operar_binario(op, alvo, forma, esq_ini, esq_fim, -1, 0);
        pop_ecx();
        guardar(alvo, 0);
        converter(alvo);
    }
    ty = alvo;
    carga_fim = -1;
}

static void expr(void)
{
    expr_atrib();
    while (aceitar(',')) { descartar(ty); expr_atrib(); }
}

/* ------------------------------------------------------------ inicializadores */

static bool eh_char(tipo_t *t) { return t->k == T_CHAR || t->k == T_UCHAR; }

/* "int v[] = {1, 2, 3}" y "char s[] = "abc"": descubre el tamaño mirando
 * las fichas adelante, antes de reservar el espacio. */
static tipo_t *completar_vetor(tipo_t *t)
{
    int p = pf + 1;   /* después del '=' */
    if (fichas[p].tipo == F_TEXTO && eh_char(t->base)) {
        int n = 1;
        for (; fichas[p].tipo == F_TEXTO; p++) n += literais[fichas[p].val].n;
        return vetor(t->base, n);
    }
    if (fichas[p].tipo != '{') falhar("vector sin tamano");
    int nivel = 0, n = 0;
    bool vazio = true;
    for (;; p++) {
        int k = fichas[p].tipo;
        if (k == F_FIM) falhar("inicializacion sin cerrar");
        if (k == '{' || k == '(' || k == '[') {
            if (nivel == 1) vazio = false;
            nivel++;
            continue;
        }
        if (k == '}' || k == ')' || k == ']') {
            if (--nivel == 0) break;
            continue;
        }
        if (nivel == 1 && k == ',') {
            if (fichas[p + 1].tipo != '}') n++;
            continue;
        }
        if (nivel == 1) vazio = false;
    }
    return vetor(t->base, vazio ? 0 : n + 1);
}

static void escrever_dado(int off, int tam, uint32_t v)
{
    for (int i = 0; i < tam; i++) dados[off + i] = (uint8_t)(v >> (8 * i));
}

static void init_global(tipo_t *t, int off)
{
    if (t->k == T_VETOR && eh_char(t->base) && tt() == F_TEXTO) {
        int s = texto_literal();
        int n = ultimo_texto_n < t->n ? ultimo_texto_n : t->n;
        memcpy(dados + off, dados + s, n);
        return;
    }
    if (t->k == T_VETOR || t->k == T_STRUCT) {
        esperar_ficha('{');
        if (t->k == T_VETOR) {
            for (int i = 0; tt() != '}'; i++) {
                if (i >= t->n) falhar("demasiados elementos en la inicializacion");
                init_global(t->base, off + i * t->base->tam);
                if (!aceitar(',')) break;
            }
        } else {
            for (membro_t *m = t->membros; tt() != '}'; m = t->uniao ? NULL : m->prox) {
                if (!m) falhar("demasiados elementos en la inicializacion");
                init_global(m->t, off + m->desloc);
                if (!aceitar(',')) break;
            }
        }
        esperar_ficha('}');
        return;
    }
    uint32_t v;
    simbolo_t *s;
    if (eh_real(t)) {
        double r = real_binario(12);
        if (t->k == T_FLOAT) { float f = (float)r; memcpy(dados + off, &f, 4); }
        else memcpy(dados + off, &r, 8);
        return;
    }
    if (tt() == F_TEXTO) {
        if (t->k != T_PTR) falhar("texto para algo que no es puntero");
        v = DADOS_BASE + texto_literal();
    } else if ((tt() == '&' && tt_a(1) == F_NOME) ||
               (tt() == F_NOME && (s = procurar(tk()->nome)) &&
                ((s->classe == S_GLOBAL && s->t->k == T_VETOR) || s->classe == S_FUNC))) {
        aceitar('&');
        s = procurar(esperar_nome());
        if (!s || (s->classe != S_GLOBAL && s->classe != S_FUNC)) falhar("se esperaba una variable global o una funcion");
        if (s->classe == S_FUNC) {
            registrar_ref(s->nome);
            if (!s->definida) falhar("la funcion '%s' debe estar definida antes", s->nome->s);
            v = COD_BASE + s->valor;
        } else {
            v = (uint32_t)s->valor;
        }
    } else {
        v = (uint32_t)const_cond();
    }
    escrever_dado(off, t->tam, v);
}

static void lea_edi(int d) { e2(0x8D, 0xBD); ed((uint32_t)d); }   /* lea edi, [ebp+d] */

static void init_local_elemento(tipo_t *t, int d);

static void init_local_agregado(tipo_t *t, int d)
{
    if (t->k == T_VETOR && eh_char(t->base) && tt() == F_TEXTO) {
        int s = texto_literal();
        int n = ultimo_texto_n < t->n ? ultimo_texto_n : t->n;
        e1(0xBE); e4(DADOS_BASE + s);   /* mov esi, texto */
        lea_edi(d);
        copiar_struct(n);
        return;
    }
    if (t->k == T_STRUCT && tt() != '{') {   /* struct a = b; */
        expr_atrib();
        if (ty->k != T_STRUCT || ty->tam != t->tam) falhar("inicializacion con tipo distinto");
        e2(0x89, 0xC6);
        lea_edi(d);
        copiar_struct(t->tam);
        return;
    }
    esperar_ficha('{');
    if (t->k == T_VETOR) {
        for (int i = 0; tt() != '}'; i++) {
            if (i >= t->n) falhar("demasiados elementos en la inicializacion");
            init_local_elemento(t->base, d + i * t->base->tam);
            if (!aceitar(',')) break;
        }
    } else {
        for (membro_t *m = t->membros; tt() != '}'; m = t->uniao ? NULL : m->prox) {
            if (!m) falhar("demasiados elementos en la inicializacion");
            init_local_elemento(m->t, d + m->desloc);
            if (!aceitar(',')) break;
        }
    }
    esperar_ficha('}');
}

static void init_local_elemento(tipo_t *t, int d)
{
    if (t->k == T_VETOR || t->k == T_STRUCT) { init_local_agregado(t, d); return; }
    expr_atrib();
    if (ty->k == T_STRUCT) falhar("tipos incompatibles en la inicializacion");
    converter_valor(ty, t);
    guardar_mem(t, 0x85, (uint32_t)d);   /* [ebp+d] = valor */
    ty = t;
    descartar(t);
}

static void init_local(tipo_t *t, int d)
{
    if (t->k == T_VETOR || (t->k == T_STRUCT && tt() == '{')) {
        lea_edi(d);                    /* pone todo a cero antes: lo que falte queda en 0 */
        e1(0xB9); e4((uint32_t)t->tam);
        e2(0x31, 0xC0);
        e2(0xF3, 0xAA);                /* rep stosb */
    }
    init_local_elemento(t, d);
}

/* ------------------------------------------------------------ instrucciones */

typedef struct { int quebra, continua, alvo_continua; bool eh_switch; } laco_t;
static laco_t lacos[64];
static int nlacos;

typedef struct { int *valores, *posicoes; int n, cap, padrao; } escolha_t;
static escolha_t *escolha_atual;

static laco_t *abrir_laco(bool sw, int alvo)
{
    if (nlacos == 64) falhar("bucles anidados demasiado");
    laco_t *l = &lacos[nlacos++];
    l->quebra = l->continua = -1;
    l->alvo_continua = alvo;
    l->eh_switch = sw;
    return l;
}

static int novo_local(tipo_t *t)
{
    int alin = t->alin < 4 ? 4 : t->alin;
    local_desloc -= t->tam;
    local_desloc &= ~(alin - 1);
    if (-local_desloc > local_max) local_max = -local_desloc;
    return local_desloc;
}

/* Declarar una funcion. "arquivo" = true si es "static": el simbolo queda
 * solo en su unidad de traduccion (D133). */
static simbolo_t *declarar_funcao(nome_t *n, tipo_t *t, bool arquivo)
{
    simbolo_t *s = arquivo ? n->unit : n->global;
    if (s && s->classe == S_FUNC) {
        if (s->t->n < 0 && t->n >= 0) s->t = t;
        return s;
    }
    if (s) falhar("'%s' ya fue declarado como otra cosa", n->s);
    if (arquivo && n->global && n->global->classe == S_FUNC)
        falhar("'%s' ya fue declarado sin \"static\"", n->s);
    return novo_simbolo(n, S_FUNC, t, arquivo ? E_UNIDADE : E_GLOBAL);
}

static void variavel_estatica(simbolo_t *s, tipo_t *t)
{
    if (aceitar('=')) {
        int off = aloca_dados(t->tam, t->alin);
        s->valor = (int)(DADOS_BASE + off);
        init_global(t, off);
    } else {
        s->valor = (int)(BSS_BASE + aloca_bss(t->tam, t->alin));
    }
}

static void declaracao_local(void)
{
    int arm;
    tipo_t *base = especificadores(&arm);
    if (aceitar(';')) return;
    do {
        nome_t *n = NULL;
        tipo_t *t = declarador(base, &n);
        if (!n) falhar("declaracion sin nombre");
        if (arm & A_TYPEDEF) { novo_simbolo(n, S_TIPO, t, E_LOCAL); continue; }
        if (t->k == T_FUNC) { declarar_funcao(n, t, (arm & A_STATIC) != 0); continue; }
        if (t->k == T_VOID) falhar("variable de tipo void");
        if (t->k == T_VETOR && t->n < 0) {
            if (tt() != '=') falhar("vector '%s' sin tamano", n->s);
            t = completar_vetor(t);
        }
        if (t->k == T_STRUCT && !t->completo) falhar("struct incompleta");
        if (arm & (A_STATIC | A_EXTERN)) {
            simbolo_t *s = novo_simbolo(n, S_GLOBAL, t, E_LOCAL);
            variavel_estatica(s, t);
            continue;
        }
        simbolo_t *s = novo_simbolo(n, S_LOCAL, t, E_LOCAL);
        s->valor = novo_local(t);
        if (aceitar('=')) init_local(t, s->valor);
    } while (aceitar(','));
    esperar_ficha(';');
}

static void instrucao(void);

static void bloco(void)   /* después del '{' */
{
    int salvo_n = nlocais, salvo_d = local_desloc;
    while (!aceitar('}')) {
        if (tt() == F_FIM) falhar("falta '}'");
        if (eh_nome_de_tipo(0)) declaracao_local();
        else instrucao();
    }
    nlocais = salvo_n;
    local_desloc = salvo_d;
}

static void condicao(void)
{
    esperar_ficha('(');
    expr();
    if (ty->k == T_STRUCT || ty->k == T_VOID) falhar("condicion invalida");
    esperar_ficha(')');
}

static void instrucao(void)
{
    switch (tt()) {
    case '{':
        avancar();
        bloco();
        return;
    case ';':
        avancar();
        return;
    case P_IF: {
        avancar();
        condicao();
        int senao = saltar_se_falso(-1);
        instrucao();
        if (aceitar(P_ELSE)) {
            int fim = saltar(0xE9, -1);
            remendar_aqui(senao);
            instrucao();
            remendar_aqui(fim);
        } else {
            remendar_aqui(senao);
        }
        return;
    }
    case P_WHILE: {
        avancar();
        int topo = ncod;
        marcar_rotulo();
        carga_fim = -1;
        condicao();
        int fim = saltar_se_falso(-1);
        laco_t *l = abrir_laco(false, topo);
        instrucao();
        saltar_para(0xE9, topo);
        remendar_aqui(fim);
        remendar_aqui(l->quebra);
        nlacos--;
        return;
    }
    case P_DO: {
        avancar();
        int topo = ncod;
        marcar_rotulo();
        carga_fim = -1;
        laco_t *l = abrir_laco(false, -1);
        instrucao();
        esperar_ficha(P_WHILE);
        remendar_aqui(l->continua);
        condicao();
        testar(ty);
        esperar_ficha(';');
        saltar_para(0x85, topo);
        remendar_aqui(l->quebra);
        nlacos--;
        return;
    }
    case P_FOR: {
        avancar();
        esperar_ficha('(');
        int salvo_n = nlocais, salvo_d = local_desloc;
        if (eh_nome_de_tipo(0)) declaracao_local();
        else { if (tt() != ';') { expr(); descartar(ty); } esperar_ficha(';'); }
        int cond = ncod;
        marcar_rotulo();
        carga_fim = -1;
        invalidar_ultimas();
        int fim = -1;
        if (tt() != ';') { expr(); fim = saltar_se_falso(-1); }
        esperar_ficha(';');
        int corpo = saltar(0xE9, -1);
        int inc = ncod;
        marcar_rotulo();
        if (tt() != ')') { expr(); descartar(ty); }
        esperar_ficha(')');
        saltar_para(0xE9, cond);
        remendar_aqui(corpo);
        laco_t *l = abrir_laco(false, inc);
        instrucao();
        saltar_para(0xE9, inc);
        remendar_aqui(fim);
        remendar_aqui(l->quebra);
        nlacos--;
        nlocais = salvo_n;
        local_desloc = salvo_d;
        return;
    }
    case P_RETURN: {
        avancar();
        tipo_t *r = funcao_atual->t->base;
        if (tt() != ';') {
            expr();
            if (ty->k == T_STRUCT || r->k == T_STRUCT) falhar("devolver struct por valor no esta soportado");
            if (r->k == T_VOID) descartar(ty);
            else if (eh_real(r) || eh_real(ty)) {
                if (r->k == T_PTR) falhar("devolver float/double como puntero");
                converter_valor(ty, r);
                if (r->k == T_FLOAT) arredondar_float();
            } else if (eh_longlong(r) && eh_longlong(ty)) {
                /* el valor ya está en EDX:EAX (D128) */
            } else converter(r);
        }
        esperar_ficha(';');
        e1(0xC9);   /* leave */
        e1(0xC3);   /* ret */
        return;
    }
    case P_BREAK:
        avancar();
        esperar_ficha(';');
        if (!nlacos) falhar("break fuera de un bucle o switch");
        lacos[nlacos - 1].quebra = saltar(0xE9, lacos[nlacos - 1].quebra);
        return;
    case P_CONTINUE: {
        avancar();
        esperar_ficha(';');
        int i = nlacos - 1;
        while (i >= 0 && lacos[i].eh_switch) i--;
        if (i < 0) falhar("continue fuera de un bucle");
        if (lacos[i].alvo_continua >= 0) saltar_para(0xE9, lacos[i].alvo_continua);
        else lacos[i].continua = saltar(0xE9, lacos[i].continua);
        return;
    }
    case P_SWITCH: {
        avancar();
        esperar_ficha('(');
        expr();
        if (!eh_inteiro(decair(ty)) && decair(ty)->k != T_PTR) falhar("switch necesita un entero");
        esperar_ficha(')');
        int temp = novo_local(&t_int);
        e2(0x89, 0x85); ed((uint32_t)temp);   /* mov [ebp+temp], eax */
        int despacho = saltar(0xE9, -1);
        escolha_t e;
        memset(&e, 0, sizeof e);
        e.padrao = -1;
        escolha_t *anterior = escolha_atual;
        escolha_atual = &e;
        laco_t *l = abrir_laco(true, -1);
        instrucao();
        l->quebra = saltar(0xE9, l->quebra);
        remendar_aqui(despacho);
        for (int i = 0; i < e.n; i++) {
            e2(0x8B, 0x85); ed((uint32_t)temp);   /* mov eax, [ebp+temp] */
            e1(0x3D); e4((uint32_t)e.valores[i]); /* cmp eax, valor */
            saltar_para(0x84, e.posicoes[i]);     /* je */
        }
        if (e.padrao >= 0) saltar_para(0xE9, e.padrao);
        remendar_aqui(l->quebra);
        nlacos--;
        escolha_atual = anterior;
        if (e.valores) { libera(e.valores); libera(e.posicoes); }
        return;
    }
    case P_CASE: {
        avancar();
        if (!escolha_atual) falhar("case fuera de un switch");
        int v = const_cond();
        esperar_ficha(':');
        escolha_t *e = escolha_atual;
        int cap = e->cap;
        garantir((void **)&e->valores, &cap, e->n + 1, sizeof(int));
        garantir((void **)&e->posicoes, &e->cap, e->n + 1, sizeof(int));
        e->valores[e->n] = v;
        e->posicoes[e->n++] = ncod;
        marcar_rotulo();
        carga_fim = -1;
        return;
    }
    case P_DEFAULT:
        avancar();
        esperar_ficha(':');
        if (!escolha_atual) falhar("default fuera de un switch");
        escolha_atual->padrao = ncod;
        marcar_rotulo();
        carga_fim = -1;
        return;
    case P_GOTO:
        falhar("goto no esta soportado");
    }
    expr();
    descartar(ty);
    esperar_ficha(';');
}

/* ------------------------------------------------------------ nivel de archivo */

static void definicao_de_funcao(simbolo_t *f, tipo_t *t)
{
    nome_t *pn[64];
    int np = params_n;
    for (int i = 0; i < np; i++) pn[i] = params_nomes[i];
    if (f->definida) {
        /* Un programa puede cambiar una función de la biblioteca por la suya. */
        if (!f->da_biblioteca || compilando_biblioteca) falhar("funcion '%s' redefinida", f->nome->s);
        f->definida = false;
        f->nome->trocada = true;
    }
    /* 2ª pasada: una función de la biblioteca que main no usa (o que el
     * programa cambió por la suya) se analiza, pero el código se tira. */
    bool pular = passada == 2 && compilando_biblioteca && (!f->nome->alcancavel || f->nome->trocada);
    int salvo_cod = ncod, salvo_dados = ndados, salvo_bss = nbss, salvo_const = nconstantes;
    bool salvo_desc = descartando;
    if (pular) descartando = true;
    f->da_biblioteca = compilando_biblioteca;
    f->t = t;
    if (!pular) definir_funcao_aqui(f);
    funcao_atual = f;
    profundidade_funcao++;
    nlocais = 0;
    local_desloc = local_max = 0;
    ll_hueco_base = 0;      /* los huecos de 64 bits son de esta función (D128) */
    int desloc = 8;
    for (int i = 0; i < np; i++) {
        if (!pn[i]) falhar("parametro sin nombre");
        simbolo_t *s = novo_simbolo(pn[i], S_LOCAL, t->params[i], E_LOCAL);
        s->valor = desloc;
        desloc += (t->params[i]->tam + 3) & ~3;   /* double: 8 bytes */
    }
    e1(0x55);              /* push ebp */
    e2(0x89, 0xE5);        /* mov ebp, esp */
    e2(0x81, 0xEC);        /* sub esp, tamaño de los locales (parcheado al final) */
    int quadro = ncod;
    e4(0);
    esperar_ficha('{');
    bloco();
    if (eh_real(t->base) && usar_sse) sse(0, 0x57, XMM(0, 0));   /* sin return: devuelve 0.0 (xorps xmm0, xmm0) */
    else if (eh_real(t->base)) f2(0xD9, 0xEE);                   /* (fldz) */
    else e2(0x31, 0xC0);                    /* sin return: devuelve 0 */
    e1(0xC9);
    e1(0xC3);
    por4(quadro, (uint32_t)((local_max + 3) & ~3));
    nlocais = 0;
    profundidade_funcao--;
    funcao_atual = NULL;
    if (pular) {
        ncod = salvo_cod;
        barreira = ncod;
        ndados = salvo_dados;
        nbss = salvo_bss;
        nconstantes = salvo_const;
        descartando = salvo_desc;
        carga_fim = -1;
        invalidar_ultimas();
    }
}

static void declaracao_global(void)
{
    int arm;
    tipo_t *base = especificadores(&arm);
    if (aceitar(';')) return;
    bool primeiro = true;
    do {
        nome_t *n = NULL;
        tipo_t *t = declarador(base, &n);
        if (!n) falhar("declaracion sin nombre");
        if (arm & A_TYPEDEF) {
            /* Um "typedef" pertence a sua unidade de tradução (D133); os da
             * biblioteca são de todas (para que o programa os use). */
            int ambito = cabecalho_biblioteca ? E_GLOBAL : E_UNIDADE;
            if (ambito == E_UNIDADE && n->unit) falhar("o tipo '%s' ja foi definido", n->s);
            if (ambito == E_GLOBAL && n->global) falhar("o tipo '%s' ja foi definido", n->s);
            novo_simbolo(n, S_TIPO, t, ambito);
        } else if (t->k == T_FUNC) {
            simbolo_t *f = declarar_funcao(n, t, (arm & A_STATIC) != 0);
            if (primeiro && tt() == '{') { definicao_de_funcao(f, t); return; }
        } else {
            if (t->k == T_VOID) falhar("variable de tipo void");
            if (t->k == T_VETOR && t->n < 0) {
                if (tt() != '=') falhar("vector '%s' sin tamano", n->s);
                t = completar_vetor(t);
            }
            if (t->k == T_STRUCT && !t->completo) falhar("struct incompleta");
            bool arquivo = (arm & A_STATIC) != 0;
            simbolo_t *s = arquivo ? n->unit : n->global;
            if (s) {
                if (s->classe != S_GLOBAL || s->t->tam != t->tam) falhar("'%s' redeclarado", n->s);
                if (tt() == '=') falhar("variable '%s' redefinida", n->s);
            } else {
                s = novo_simbolo(n, S_GLOBAL, t, arquivo ? E_UNIDADE : E_GLOBAL);
                variavel_estatica(s, t);
            }
        }
        primeiro = false;
    } while (aceitar(','));
    esperar_ficha(';');
}

/* Comienzo del programa generado:
 *   _inicio: llama a main(argc, argv) y termina con el valor devuelto;
 *   __syscall(n, a, b, c): la llamada al sistema (int 0x80). */
static void emitir_inicio(void)
{
    reservar_iniciar_fpu();
    simbolo_t *m = novo_simbolo(nome_de("main", 4), S_FUNC, NULL, E_GLOBAL);
    tipo_t *f = memoria(sizeof *f);
    f->k = T_FUNC;
    f->tam = f->alin = 1;
    f->base = &t_int;
    f->n = -1;
    m->t = f;
    e3(0x8B, 0x04, 0x24);          /* mov eax, [esp]     argc */
    /* argv: en SaguiOS, [esp+4] apunta al vector; en Linux, el vector
     * empieza ahí mismo. */
    e1(modo_linux ? 0x8D : 0x8B); e3(0x4C, 0x24, 0x04);   /* mov (ou lea) ecx, [esp+4] */
    e3(0x83, 0xE4, 0xF0);          /* and esp, -16 */
    e1(0x51);                      /* push ecx */
    e1(0x50);                      /* push eax */
    chamar_funcao(m);
    e2(0x89, 0xC3);                /* mov ebx, eax */
    mov_eax(modo_linux ? 1 : SIS_SAIR);
    e2(0xCD, 0x80);
    e2(0xEB, 0xFE);                /* nunca llega aquí */
    m->usada = false;

    tipo_t *sc = memoria(sizeof *sc);
    sc->k = T_FUNC;
    sc->tam = sc->alin = 1;
    sc->base = &t_int;
    sc->n = 4;
    sc->params = memoria(sizeof(tipo_t *) * 4);
    for (int i = 0; i < 4; i++) sc->params[i] = &t_int;
    simbolo_t *s = novo_simbolo(nome_de("__syscall", 9), S_FUNC, sc, E_GLOBAL);
    definir_funcao_aqui(s);
    emitir_rotina_syscall();

    /* D167: las dos atómicas que necesitan los hilos de la biblioteca
     * (mutex, semáforo, malloc). Son instrucciones con "lock", que el C de
     * este compilador no puede escribir:
     *   __troca(p, v): *p = v y devuelve el valor anterior (xchg);
     *   __cas(p, esperado, novo): si *p == esperado, *p = novo; devuelve el
     *   valor anterior de *p (lock cmpxchg). */
    tipo_t *tt = memoria(sizeof *tt);
    tt->k = T_FUNC;
    tt->tam = tt->alin = 1;
    tt->base = &t_int;
    tt->n = 2;
    tt->params = memoria(sizeof(tipo_t *) * 2);
    tt->params[0] = tt->params[1] = &t_int;
    definir_funcao_aqui(novo_simbolo(nome_de("__troca", 7), S_FUNC, tt, E_GLOBAL));
    e1(0x8B); e3(0x4C, 0x24, 0x04);   /* mov ecx, [esp+4] */
    e1(0x8B); e3(0x44, 0x24, 0x08);   /* mov eax, [esp+8] */
    e2(0x87, 0x01);                   /* xchg [ecx], eax (lleva "lock" implícito) */
    e1(0xC3);                         /* ret */
    tipo_t *tc = memoria(sizeof *tc);
    tc->k = T_FUNC;
    tc->tam = tc->alin = 1;
    tc->base = &t_int;
    tc->n = 3;
    tc->params = memoria(sizeof(tipo_t *) * 3);
    tc->params[0] = tc->params[1] = tc->params[2] = &t_int;
    definir_funcao_aqui(novo_simbolo(nome_de("__cas", 5), S_FUNC, tc, E_GLOBAL));
    e1(0x8B); e3(0x4C, 0x24, 0x04);   /* mov ecx, [esp+4] */
    e1(0x8B); e3(0x44, 0x24, 0x08);   /* mov eax, [esp+8] */
    e1(0x8B); e3(0x54, 0x24, 0x0C);   /* mov edx, [esp+12] */
    e2(0xF0, 0x0F); e2(0xB1, 0x11);   /* lock cmpxchg [ecx], edx */
    e1(0xC3);                         /* ret */
}

/* ------------------------------------------------------------ biblioteca */

#include "cc_biblioteca.h"

/* ------------------------------------------------------------ principal */

static const char NOME_BIBLIOTECA[] = "<biblioteca>";
static const char NOME_CABECALHO[] = "<cabecalho>";

/* Funciones "débiles" (__real_*): sólo entran si el programa usa punto
 * flotante, aunque otra función de la biblioteca las llame. */
static bool eh_fraca(nome_t *n) { return !memcmp(n->s, "__real_", 7); }

static void alcancar(nome_t *n)
{
    if (n->alcancavel) return;
    if (n != &raizes) n->alcancavel = true;
    for (int i = 0; i < n->nrefs; i++)
        if (!eh_fraca(n->refs[i]) || usou_real_programa) alcancar(n->refs[i]);
}

static void compilar(void)
{
    emitir_inicio();
    for (int u = 0; u < nunidades; u++) {
        limpar_unidade();
        pf = unidades[u].inicio;
        linha_erro = fichas[pf].linha;
        arq_erro = fichas[pf].arq;
        while (pf < unidades[u].fim) {
            compilando_biblioteca = tk()->arq == NOME_BIBLIOTECA || tk()->arq == NOME_CABECALHO;
            cabecalho_biblioteca = tk()->arq == NOME_CABECALHO;
            declaracao_global();
        }
    }
    compilando_biblioteca = false;
}

int principal(int argc, char **argv)
{
    /* D236: el kernel da la x87 a 53 bits; el cc lee las constantes reales en
     * long double (ler_real) y las quiere con los 64 bits de mantisa, como antes. */
    x87_precisao(X87_64_BITS);
    const char *saida = NULL;
    /* Os ficheiros de origem: mais de um e' ligado num so programa (D133);
     * cada um e' uma unidade de traducao aparte. */
    const char *entradas[MAX_UNIDADES];
    int nentradas = 0;
    int fpu = 0;   /* 0 = segundo a CPU, 1 = -sse2, 2 = -x87 */
    for (int i = 1; i < argc; i++) {
        if (!compara_texto(argv[i], "-o") && i + 1 < argc) saida = argv[++i];
        else if (!compara_texto(argv[i], "-linux")) modo_linux = true;
        else if (!compara_texto(argv[i], "-x87")) fpu = 2;
        else if (!compara_texto(argv[i], "-sse2")) fpu = 1;
        else if (opcao_otimizacao(argv[i])) continue;
        else if (argv[i][0] == '-') { nentradas = 0; i = argc; }
        else {
            if (nentradas == MAX_UNIDADES) {
                imprimir_em(ERROS, "cc: demasiados ficheros de origen\n");
                return 2;
            }
            entradas[nentradas++] = argv[i];
        }
    }
    if (!nentradas) {
        imprimir_em(ERROS, "uso: cc programa.c [mais.c ...] [-o nome] [-x87 | -sse2] [-O0 | -O1] "
                           "[-fno-mirilla|inmediatos|saltos|reducao]\n");
        return 2;
    }
    /* D076: sin opcion, el programa sale para la CPU donde se compila. El propio
    * programa todavia verifica CPUID al empezar, si usa SSE2. */
    bool tem_sse2 = cpu_tem_sse2();
    usar_sse = fpu == 1 || (fpu == 0 && tem_sse2);
    if (fpu == 1 && !tem_sse2)
        imprimir_em(ERROS, "cc: aviso: esta CPU no tiene SSE2; el programa compilado con -sse2 no funcionara en ella "
                           "(sin -sse2, cc usa la FPU x87)\n");
    registrar("cc: coma flotante con %s (%s)", usar_sse ? "SSE2" : "x87",
              fpu == 1 ? "-sse2" : fpu == 2 ? "-x87" : tem_sse2 ? "la CPU tiene SSE2" : "la CPU no tiene SSE2");
    char nome_saida[128];
    if (!saida) {   /* "carpeta/prog.c" -> "prog", en la carpeta actual */
        const char *base = acha_ultimo_char(entradas[0], '/');
        base = base ? base + 1 : entradas[0];
        size_t n = tam_texto(base);
        copia_texto(nome_saida, base, sizeof nome_saida);
        if (n > 2 && n < sizeof nome_saida && base[n - 2] == '.' && base[n - 1] == 'c') nome_saida[n - 2] = 0;
        else copia_texto(nome_saida, "a.out", sizeof nome_saida);
        saida = nome_saida;
    }
    for (unsigned i = 0; i < sizeof reservadas / sizeof reservadas[0]; i++)
        nome_de(reservadas[i], (int)tam_texto(reservadas[i]))->reservada = P_INT + (int)i;

    definir_macro_texto("__SAGUIOS__", "1");
    if (modo_linux) definir_macro_texto("__LINUX__", "1");
    /* O cabecalho da biblioteca (os tipos, as macros e os prototipo) vem
     * primeiro: assim o programa ja os tem, como com os <...> do C. */
    abrir_fonte(NOME_CABECALHO, BIBLIOTECA_CABECALHO, (int)tam_texto(BIBLIOTECA_CABECALHO));
    ler_unidade();
    for (int i = 0; i < nentradas; i++) {
        int tam;
        char *texto = ler_arquivo(entradas[i], &tam);
        if (!texto) {
            imprimir_em(ERROS, "cc: no se puede abrir %s\n", entradas[i]);
            return 1;
        }
        abrir_fonte(entradas[i], texto, tam);
        ler_unidade();
    }
    /* As funcoes da biblioteca vem no fim: as que o programa nao usa sao
     * apagadas na segunda passada. */
    abrir_fonte(NOME_BIBLIOTECA, BIBLIOTECA, (int)tam_texto(BIBLIOTECA));
    ler_unidade();

    /* 1a pasada: descubre que funciones usa cada una. */
    passada = 1;
    compilar();
    alcancar(&raizes);
    alcancar(nome_de("main", 4));
    /* 2a pasada: todo de nuevo, sin las funciones de la biblioteca no usadas. */
    passada = 2;
    for (int h = 0; h < HASH_NOMES; h++)
        for (nome_t *x = nomes[h]; x; x = x->prox) {
            x->global = NULL; x->etiqueta = NULL; x->etiqueta_global = NULL; x->unit = NULL;
        }
    ncod = ndados = nbss = nconstantes = 0;
    barreira = 0; ultimo_push = -1;
    usou_real = false;
    lista_para_inteiro = -1;
    compilar();
    emitir_rotinas_finais();
    simbolo_t *m = nome_de("main", 4)->global;
    if (!m || m->classe != S_FUNC || !m->definida) {
        imprimir_em(ERROS, "%s: error: falta la funcion main\n", entradas[0]);
        return 1;
    }
    for (int h = 0; h < HASH_NOMES; h++)
        for (nome_t *x = nomes[h]; x; x = x->prox) {
            simbolo_t *s = x->global;
            if (s && s->classe == S_FUNC && !s->definida && s->da_biblioteca && eh_fraca(x)) {
                /* Funcao "debil" da biblioteca deixada fora: quem a chama
                 * recebe 0 (ex.: printf com %f num programa sem float). */
                definir_funcao_aqui(s);
                e2(0x31, 0xC0);
                e1(0xC3);
            }
            if (s && s->classe == S_FUNC && !s->definida && (s->chamadas != -1 || s->absolutos != -1)) {
                imprimir_em(ERROS, "%s: error: la funcion '%s' se usa pero no esta definida\n", entradas[0], x->s);
                return 1;
            }
        }
    int r = escrever_elf(saida);
    if (r < 0) {
        imprimir_em(ERROS, "cc: no se puede escribir %s: %s\n", saida, nome_erro(r));
        return 1;
    }
    return 0;
}
