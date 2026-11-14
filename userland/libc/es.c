/* es.c - stdio para NetSurf (D184): FILE con búfer sobre los descriptores
 * del SaguiOS, la familia de printf (con %f/%g, %lld y %zu) y sscanf.
 *
 * El formateador y el FILE salen de la libc del OpenTyrian (D156,
 * userland/terceros/opentyrian/libc/libc_ot.c), con lo que NetSurf pide de
 * más: stdin, fdopen, fileno, setbuf/setvbuf, getline, búfer de escritura,
 * vasprintf, y un sscanf que distingue %f (float) de %lf (double).
 */
#include "sagui.h"

#include "ctype.h"
#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "numeros_libc.h"

#define TAM_BUF 4096

struct ns_ficheiro {
    int  fd;
    int  eof, erro;
    int  pode_ler, pode_escrever, acrescentar;
    long pos;                 /* posición lógica (la que ve el programa) */
    unsigned char *buf;
    int  buf_n, buf_pos;      /* lectura: bytes válidos y ya consumidos */
    int  escritos;            /* escritura: bytes en el búfer sin enviar */
    int  empurra;             /* carácter de ungetc (-1 = ninguno) */
    int  sem_buffer;          /* setbuf(f, NULL): escribe al momento */
    int  padrao;              /* stdin/stdout/stderr: no se liberan */
    unsigned char buf_fixo[TAM_BUF];
    struct ns_ficheiro *prox; /* los abiertos con fopen/fdopen, para fflush(NULL) */
};

static FILE f_entrada = { 0, 0, 0, 1, 0, 0, 0, NULL, 0, 0, 0, -1, 1, 1, { 0 } };
static FILE f_saida   = { 1, 0, 0, 0, 1, 0, 0, NULL, 0, 0, 0, -1, 1, 1, { 0 } };
static FILE f_erro    = { 2, 0, 0, 0, 1, 0, 0, NULL, 0, 0, 0, -1, 1, 1, { 0 } };

FILE *stdin = &f_entrada;
FILE *stdout = &f_saida;
FILE *stderr = &f_erro;

/* Los FILE abiertos (sin los tres estándar): exit() y fflush(NULL) los vacían
 * todos, como pide la C. */
static FILE *abertos;

static FILE *novo_ficheiro(int fd, int ler_, int escr, int acresc)
{
    FILE *f = malloc(sizeof *f);
    if (!f) { errno = ENOMEM; return NULL; }
    memset(f, 0, sizeof *f);
    f->fd = fd;
    f->pode_ler = ler_;
    f->pode_escrever = escr;
    f->acrescentar = acresc;
    f->buf = f->buf_fixo;
    f->empurra = -1;
    f->prox = abertos;
    abertos = f;
    return f;
}

static int modo_de(const char *modo, int *ler_, int *escr, int *acresc)
{
    int criar = 0, trunc = 0;
    *ler_ = *escr = *acresc = 0;
    switch (modo[0]) {
    case 'r': *ler_ = 1; break;
    case 'w': *escr = 1; criar = trunc = 1; break;
    case 'a': *escr = 1; criar = 1; *acresc = 1; break;
    default: return -1;
    }
    if (strchr(modo, '+')) { *ler_ = 1; *escr = 1; }
    return (*ler_ ? ABRIR_LER : 0) | (*escr ? ABRIR_ESCREVER : 0) |
           (criar ? ABRIR_CRIAR : 0) | (trunc ? ABRIR_TRUNCAR : 0) |
           (*acresc ? ABRIR_ACRESCENTAR : 0);
}

FILE *fopen(const char *caminho, const char *modo)
{
    int l, e, a;
    if (!caminho || !modo) { errno = EINVAL; return NULL; }
    int m = modo_de(modo, &l, &e, &a);
    if (m < 0) { errno = EINVAL; return NULL; }
    int fd = abrir(caminho, m);
    if (fd < 0) { errno = __ns_errno_de(fd); return NULL; }
    FILE *f = novo_ficheiro(fd, l, e, a);
    if (!f) fechar(fd);
    return f;
}

FILE *fdopen(int fd, const char *modo)
{
    int l, e, a;
    if (modo_de(modo, &l, &e, &a) < 0) { errno = EINVAL; return NULL; }
    return novo_ficheiro(fd, l, e, a);
}

/* freopen (D230: Vanilla Conquer manda stderr a su registro). Abre el fichero
 * nuevo y lo pone en el mismo FILE: en los tres estándar, con duplicar() sobre
 * su descriptor (0, 1 o 2), para que lo que se escriba directamente en el
 * descriptor también vaya al fichero; en los demás, el descriptor viejo se
 * cierra. Sin camino (cambiar solo el modo) no se admite. */
FILE *freopen(const char *caminho, const char *modo, FILE *f)
{
    int l, e, a;
    if (!f || !modo) { errno = EINVAL; return NULL; }
    if (!caminho) { errno = ENOSYS; return NULL; }
    int m = modo_de(modo, &l, &e, &a);
    if (m < 0) { errno = EINVAL; return NULL; }
    fflush(f);
    int fd = abrir(caminho, m);
    if (fd < 0) { errno = __ns_errno_de(fd); return NULL; }
    if (f->padrao) {
        int r = duplicar(fd, f->fd);
        fechar(fd);
        if (r < 0) { errno = __ns_errno_de(r); return NULL; }
    } else {
        fechar(f->fd);
        f->fd = fd;
    }
    f->eof = f->erro = 0;
    f->pode_ler = l;
    f->pode_escrever = e;
    f->acrescentar = a;
    f->pos = 0;
    f->buf_n = f->buf_pos = f->escritos = 0;
    f->empurra = -1;
    return f;
}

int fileno(FILE *f) { return f ? f->fd : -1; }

/* Envía lo que haya en el búfer de escritura. */
static int despejar(FILE *f)
{
    int feito = 0;
    while (feito < f->escritos) {
        int r = escrever(f->fd, f->buf + feito, (unsigned)(f->escritos - feito));
        if (r <= 0) { f->erro = 1; f->escritos = 0; return EOF; }
        feito += r;
    }
    f->escritos = 0;
    return 0;
}

/* Antes de leer: el búfer de escritura sale. Antes de escribir: lo leído de
 * más se descarta y el descriptor vuelve a la posición lógica. */
static void para_ler(FILE *f)
{
    if (f->escritos) despejar(f);
}

static void para_escrever(FILE *f)
{
    if (f->buf_n || f->empurra >= 0) {
        f->buf_n = f->buf_pos = 0;
        f->empurra = -1;
        if (!f->padrao) posicionar(f->fd, (int)f->pos, POS_INICIO);
    }
}

int fflush(FILE *f)
{
    if (!f) {
        int r = 0;
        for (FILE *g = abertos; g; g = g->prox)
            if (g->escritos && despejar(g) == EOF) r = EOF;
        if (fflush(stdout) == EOF) r = EOF;
        if (fflush(stderr) == EOF) r = EOF;
        return r;
    }
    return f->escritos ? despejar(f) : 0;
}

int fclose(FILE *f)
{
    if (!f) return EOF;
    int r = fflush(f);
    if (f->padrao) return r;
    for (FILE **p = &abertos; *p; p = &(*p)->prox)
        if (*p == f) { *p = f->prox; break; }
    if (fechar(f->fd) < 0) r = EOF;
    free(f);
    return r;
}

static size_t ler_bytes(FILE *f, unsigned char *p, size_t n)
{
    size_t feito = 0;
    if (!f->buf) f->buf = f->buf_fixo;   /* stdin/stdout/stderr */
    para_ler(f);
    if (n && f->empurra >= 0) {
        p[feito++] = (unsigned char)f->empurra;
        f->empurra = -1;
        f->pos++;
    }
    while (feito < n) {
        if (f->buf_pos >= f->buf_n) {
            if (f->eof) break;
            /* Lecturas grandes van directas, sin pasar por el búfer. */
            if (n - feito >= TAM_BUF) {
                int r = ler(f->fd, p + feito, (unsigned)(n - feito));
                if (r <= 0) { f->eof = 1; if (r < 0 && r != ERRO_PIPE_QUEBRADO) f->erro = 1; break; }
                feito += (size_t)r;
                f->pos += r;
                continue;
            }
            /* Una tubería sin quien escriba (ERRO_PIPE_QUEBRADO) es el fin, no un error. */
            int r = ler(f->fd, f->buf, TAM_BUF);
            if (r <= 0) { f->eof = 1; if (r < 0 && r != ERRO_PIPE_QUEBRADO) f->erro = 1; break; }
            f->buf_n = r;
            f->buf_pos = 0;
        }
        size_t disp = (size_t)(f->buf_n - f->buf_pos);
        if (disp > n - feito) disp = n - feito;
        memcpy(p + feito, f->buf + f->buf_pos, disp);
        f->buf_pos += (int)disp;
        feito += disp;
        f->pos += (long)disp;
    }
    return feito;
}

size_t fread(void *p, size_t tam, size_t n, FILE *f)
{
    if (!f || !p || tam == 0 || n == 0) return 0;
    return ler_bytes(f, p, tam * n) / tam;
}

size_t fwrite(const void *p, size_t tam, size_t n, FILE *f)
{
    if (!f || !p || tam == 0 || n == 0) return 0;
    size_t total = tam * n;
    if (!f->buf) f->buf = f->buf_fixo;
    para_escrever(f);
    const unsigned char *b = p;
    size_t feito = 0;
    while (feito < total) {
        if (f->sem_buffer || (f->escritos == 0 && total - feito >= TAM_BUF)) {
            if (f->escritos && despejar(f) == EOF) break;
            int r = escrever(f->fd, b + feito, (unsigned)(total - feito));
            if (r <= 0) { f->erro = 1; break; }
            feito += (size_t)r;
            f->pos += r;
            continue;
        }
        size_t cabe = (size_t)(TAM_BUF - f->escritos);
        if (cabe > total - feito) cabe = total - feito;
        memcpy(f->buf + f->escritos, b + feito, cabe);
        f->escritos += (int)cabe;
        feito += cabe;
        f->pos += (long)cabe;
        if (f->escritos == TAM_BUF && despejar(f) == EOF) break;
    }
    /* stdout y stderr van a la consola sin esperar (como con un terminal). */
    if (f->padrao && f->escritos) despejar(f);
    return feito / tam;
}

int fseek(FILE *f, long desloc, int origem)
{
    if (!f || f->padrao) { errno = ESPIPE; return -1; }
    fflush(f);
    long nova;
    if (origem == SEEK_SET) nova = desloc;
    else if (origem == SEEK_CUR) nova = f->pos + desloc;
    else if (origem == SEEK_END) {
        int r = posicionar(f->fd, 0, POS_FIM);
        if (r < 0) { errno = __ns_errno_de(r); return -1; }
        nova = r + desloc;
    } else { errno = EINVAL; return -1; }
    if (nova < 0) { errno = EINVAL; return -1; }
    int r = posicionar(f->fd, (int)nova, POS_INICIO);
    if (r < 0) { errno = __ns_errno_de(r); return -1; }
    f->pos = nova;
    f->buf_n = f->buf_pos = 0;
    f->eof = 0;
    f->empurra = -1;
    return 0;
}

int fseeko(FILE *f, off_t desloc, int origem) { return fseek(f, desloc, origem); }
long ftell(FILE *f) { return f ? f->pos : -1; }
off_t ftello(FILE *f) { return ftell(f); }
void rewind(FILE *f) { fseek(f, 0, SEEK_SET); clearerr(f); }

int fgetc(FILE *f)
{
    unsigned char c;
    if (!f || ler_bytes(f, &c, 1) != 1) return EOF;
    return c;
}

int getc(FILE *f) { return fgetc(f); }
int getchar(void) { return fgetc(stdin); }

int ungetc(int c, FILE *f)
{
    if (!f || c == EOF || f->empurra >= 0) return EOF;
    f->empurra = (unsigned char)c;
    f->eof = 0;
    if (f->pos > 0) f->pos--;
    return (unsigned char)c;
}

int fputc(int c, FILE *f)
{
    unsigned char b = (unsigned char)c;
    return fwrite(&b, 1, 1, f) == 1 ? b : EOF;
}

int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c) { return fputc(c, stdout); }

char *fgets(char *s, int n, FILE *f)
{
    if (!s || n <= 0 || !f) return NULL;
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) { if (i == 0) return NULL; break; }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = 0;
    return s;
}

ssize_t getdelim(char **linha, size_t *n, int delim, FILE *f)
{
    if (!linha || !n || !f) { errno = EINVAL; return -1; }
    size_t usados = 0;
    for (;;) {
        int c = fgetc(f);
        if (c == EOF) break;
        if (usados + 2 > *n || !*linha) {
            size_t novo = *n ? *n * 2 : 128;
            char *p = realloc(*linha, novo);
            if (!p) { errno = ENOMEM; return -1; }
            *linha = p;
            *n = novo;
        }
        (*linha)[usados++] = (char)c;
        if (c == delim) break;
    }
    if (usados == 0) return -1;
    (*linha)[usados] = 0;
    return (ssize_t)usados;
}

ssize_t getline(char **linha, size_t *n, FILE *f) { return getdelim(linha, n, '\n', f); }

int fputs(const char *s, FILE *f)
{
    if (!s || !f) return EOF;
    size_t n = strlen(s);
    if (n == 0) return 0;
    return fwrite(s, 1, n, f) == n ? (int)n : EOF;
}

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF) return EOF;
    return fputc('\n', stdout) == EOF ? EOF : 0;
}

int feof(FILE *f) { return f && f->eof && f->buf_pos >= f->buf_n && f->empurra < 0; }
int ferror(FILE *f) { return f && f->erro; }
void clearerr(FILE *f) { if (f) { f->eof = 0; f->erro = 0; } }

void setbuf(FILE *f, char *buf)
{
    if (f) f->sem_buffer = (buf == NULL);
}

int setvbuf(FILE *f, char *buf, int modo, size_t tam)
{
    (void)buf; (void)tam;
    if (!f) return -1;
    f->sem_buffer = (modo == _IONBF);
    return 0;
}

void setlinebuf(FILE *f) { setvbuf(f, NULL, _IOLBF, 0); }

/* Sin fork ni sh -c con tuberías a la vista del FILE (carencia): popen no
 * lanza nada. */
FILE *popen(const char *cmd, const char *modo) { (void)cmd; (void)modo; errno = ENOSYS; return NULL; }
int pclose(FILE *f) { (void)f; errno = ECHILD; return -1; }

int remove(const char *caminho)
{
    int r = apagar(caminho);
    if (r == ERRO_E_PASTA) r = remover_pasta(caminho);
    if (r < 0) { errno = __ns_errno_de(r); return -1; }
    return 0;
}

int rename(const char *de, const char *para)
{
    int r = renomear(de, para);
    if (r == ERRO_JA_EXISTE) {
        /* POSIX: el destino se sustituye. */
        apagar(para);
        r = renomear(de, para);
    }
    if (r < 0) { errno = __ns_errno_de(r); return -1; }
    return 0;
}

FILE *tmpfile(void)
{
    char nome[64];
    for (int i = 0; i < 100; i++) {
        formatar_texto(nome, sizeof nome, "/tmp/ns%u-%d", tempo_ms(), i);
        info_arquivo_t inf;
        if (info_arquivo(nome, &inf) < 0) return fopen(nome, "w+");
    }
    errno = EEXIST;
    return NULL;
}

char *tmpnam(char *s)
{
    static char nome[L_tmpnam];
    char *d = s ? s : nome;
    static int cont;
    formatar_texto(d, L_tmpnam, "/tmp/ns%u-%d", tempo_ms(), cont++);
    return d;
}

void perror(const char *s)
{
    if (s && *s) { fputs(s, stderr); fputs(": ", stderr); }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

/* ------------------------------------------------------------- formato */

typedef struct {
    char *buf;
    size_t max;
    size_t pos;
} saida_t;

static void poe(saida_t *s, char c)
{
    if (s->buf && s->max > 0 && s->pos + 1 < s->max) s->buf[s->pos] = c;
    s->pos++;
}

static void poe_rep(saida_t *s, char c, int n) { for (int i = 0; i < n; i++) poe(s, c); }

static void poe_cadeia(saida_t *s, const char *t, int n)
{
    for (int i = 0; i < n; i++) poe(s, t[i]);
}

/* El número ya en cifras, con signo/prefijo, ancho y relleno. */
static void poe_numero(saida_t *s, const char *pref, const char *dig, int ndig,
                       int larg, bool esq, bool zero)
{
    int np = (int)strlen(pref);
    int total = ndig + np;
    int extra = larg > total ? larg - total : 0;
    if (!esq && !zero) poe_rep(s, ' ', extra);
    poe_cadeia(s, pref, np);
    if (!esq && zero) poe_rep(s, '0', extra);
    poe_cadeia(s, dig, ndig);
    if (esq) poe_rep(s, ' ', extra);
}

/* Cifras de u en la base dada, con al menos prec cifras. */
static int cifras(char *dig, unsigned long long u, unsigned base, bool maiusc, int prec)
{
    const char *d = maiusc ? "0123456789ABCDEF" : "0123456789abcdef";
    char t[24];
    int m = 0, n = 0;
    while (u) {
        t[m++] = d[u % base];
        u /= base;
    }
    if (prec < 0) prec = 1;
    for (int i = m; i < prec && n < 64; i++) dig[n++] = '0';
    while (m) dig[n++] = t[--m];
    return n;
}

/* Un real (%e/%f/%g): el cuerpo exacto sale de numeros.c (D210). Aparte y
 * sin «inline» para que el búfer grande no engorde la pila de cada printf. */
static __attribute__((noinline)) void poe_real(saida_t *s, double v, char conv, int prec,
                                               int larg, bool esq, bool zero, bool mais,
                                               bool espaco, bool alt)
{
    char buf[LIBC_REAL_TAM];
    int n = __libc_real_texto(buf, v, conv, prec, alt);
    const char *pref = "";
    if (__builtin_signbit(v)) pref = "-";
    else if (mais) pref = "+";
    else if (espaco) pref = " ";
    /* inf y nan no se rellenan con ceros */
    poe_numero(s, pref, buf, n, larg, esq, zero && !__builtin_isinf(v) && !__builtin_isnan(v));
}

static int formatar(saida_t *s, const char *fmt, va_list ap)
{
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { poe(s, *p); continue; }
        p++;
        if (*p == '%') { poe(s, '%'); continue; }
        bool esq = false, zero = false, mais = false, espaco = false, alt = false;
        for (;; p++) {
            if (*p == '-') esq = true;
            else if (*p == '0') zero = true;
            else if (*p == '+') mais = true;
            else if (*p == ' ') espaco = true;
            else if (*p == '#') alt = true;
            else break;
        }
        int larg = 0;
        if (*p == '*') { larg = va_arg(ap, int); if (larg < 0) { esq = true; larg = -larg; } p++; }
        else while (*p >= '0' && *p <= '9') larg = larg * 10 + (*p++ - '0');
        int prec = -1;
        if (*p == '.') {
            p++;
            prec = 0;
            if (*p == '*') { prec = va_arg(ap, int); p++; }
            else while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
        }
        /* Tamaño: hh/h/l/ll/z/j/t/L. En i386, long, size_t y ptrdiff_t son
         * de 32 bits; long long e intmax_t de 64. */
        int longs = 0;
        bool longo_duplo = false;   /* %Lf: long double (12 bytes en la pila) */
        for (;; p++) {
            if (*p == 'l') longs++;
            else if (*p == 'j' || *p == 'q') longs = 2;
            else if (*p == 'L') longo_duplo = true;
            else if (*p == 'h' || *p == 'z' || *p == 't') ;
            else break;
        }
        char conv = *p;
        if (conv == 0) break;
        char dig[80];
        switch (conv) {
        case 'd': case 'i': {
            long long v = longs >= 2 ? va_arg(ap, long long) : (long long)va_arg(ap, long);
            bool neg = v < 0;
            unsigned long long u = neg ? (unsigned long long)(-(v + 1)) + 1ull : (unsigned long long)v;
            int n = (prec == 0 && u == 0) ? 0 : cifras(dig, u, 10, false, prec);
            const char *pref = neg ? "-" : (mais ? "+" : (espaco ? " " : ""));
            poe_numero(s, pref, dig, n, larg, esq, zero && prec < 0);
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            unsigned long long u = longs >= 2 ? va_arg(ap, unsigned long long)
                                              : (unsigned long long)va_arg(ap, unsigned long);
            unsigned base = conv == 'o' ? 8 : (conv == 'u' ? 10 : 16);
            int n = (prec == 0 && u == 0) ? 0 : cifras(dig, u, base, conv == 'X', prec);
            const char *pref = "";
            if (alt && u && conv == 'x') pref = "0x";
            else if (alt && u && conv == 'X') pref = "0X";
            else if (alt && conv == 'o' && (n == 0 || dig[0] != '0')) pref = "0";
            poe_numero(s, pref, dig, n, larg, esq, zero && prec < 0);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            poe_numero(s, "", &c, 1, larg, esq, false);
            break;
        }
        case 's': {
            const char *t = va_arg(ap, const char *);
            if (!t) t = "(null)";
            int n = prec >= 0 ? (int)strnlen(t, (size_t)prec) : (int)strlen(t);
            poe_numero(s, "", t, n, larg, esq, false);
            break;
        }
        case 'p': {
            void *v = va_arg(ap, void *);
            int n = cifras(dig, (unsigned long)v, 16, false, 1);
            poe_numero(s, "0x", dig, n, larg, esq, false);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            double v = longo_duplo ? (double)va_arg(ap, long double) : va_arg(ap, double);
            poe_real(s, v, conv, prec, larg, esq, zero, mais, espaco, alt);
            break;
        }
        case 'n': {
            int *q = va_arg(ap, int *);
            if (q) *q = (int)s->pos;
            break;
        }
        default:
            poe(s, '%');
            poe(s, conv);
            break;
        }
    }
    if (s->buf && s->max > 0) s->buf[s->pos < s->max ? s->pos : s->max - 1] = 0;
    return (int)s->pos;
}

int vsnprintf(char *s, size_t n, const char *fmt, va_list ap)
{
    saida_t saida = { s, n, 0 };
    return formatar(&saida, fmt, ap);
}

int snprintf(char *s, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(s, n, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *s, const char *fmt, va_list ap) { return vsnprintf(s, 0x7FFFFFFF, fmt, ap); }

int sprintf(char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

int vasprintf(char **s, const char *fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    *s = malloc((size_t)n + 1);
    if (!*s) return -1;
    return vsnprintf(*s, (size_t)n + 1, fmt, ap);
}

int asprintf(char **s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(s, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    if (!f) return -1;
    char buf[1024];
    va_list ap2;
    va_copy(ap2, ap);
    saida_t saida = { buf, sizeof buf, 0 };
    int n = formatar(&saida, fmt, ap2);
    va_end(ap2);
    if (n < (int)sizeof buf) {
        fwrite(buf, 1, (size_t)n, f);
        return n;
    }
    char *grande = malloc((size_t)n + 1);
    if (!grande) { fwrite(buf, 1, sizeof buf - 1, f); return n; }
    saida_t s2 = { grande, (size_t)n + 1, 0 };
    formatar(&s2, fmt, ap);
    fwrite(grande, 1, (size_t)n, f);
    free(grande);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------------- sscanf */

int vsscanf(const char *s, const char *fmt, va_list ap)
{
    int convertidos = 0;
    const char *p = s;
    for (const char *f = fmt; *f; f++) {
        if (isspace((unsigned char)*f)) {
            while (isspace((unsigned char)*p)) p++;
            continue;
        }
        if (*f != '%' || f[1] == '%') {
            if (*f == '%') f++;
            if (*p != *f) return convertidos ? convertidos : (*p ? 0 : EOF);
            p++;
            continue;
        }
        f++;
        bool guardar = true;
        if (*f == '*') { guardar = false; f++; }
        int larg = 0;
        while (*f >= '0' && *f <= '9') larg = larg * 10 + (*f++ - '0');
        int longs = 0, curto = 0;
        bool longo_duplo = false;   /* %Lf: long double * */
        for (;; f++) {
            if (*f == 'l') longs++;
            else if (*f == 'h') curto++;
            else if (*f == 'L') longo_duplo = true;
            else if (*f == 'z' || *f == 'j' || *f == 't') { if (*f == 'j') longs = 2; }
            else break;
        }
        char conv = *f;
        if (conv == 0) break;
        if (conv != 'c' && conv != '[' && conv != 'n')
            while (isspace((unsigned char)*p)) p++;
        if (!*p && conv != 'n') return convertidos ? convertidos : EOF;
        switch (conv) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': {
            /* El ancho limita las cifras: se copian a un búfer. */
            char tmp[40];
            int n = 0, max = larg ? larg : 39;
            if (max > 39) max = 39;
            while (p[n] && n < max && !isspace((unsigned char)p[n])) { tmp[n] = p[n]; n++; }
            tmp[n] = 0;
            int base = conv == 'x' || conv == 'X' ? 16 : (conv == 'o' ? 8 : (conv == 'i' ? 0 : 10));
            char *fim;
            unsigned long long v;
            if (conv == 'd' || conv == 'i') v = (unsigned long long)strtoll(tmp, &fim, base);
            else v = strtoull(tmp, &fim, base);
            if (fim == tmp) return convertidos;
            p += fim - tmp;
            if (guardar) {
                void *dest = va_arg(ap, void *);
                if (longs >= 2) *(unsigned long long *)dest = v;
                else if (curto == 1) *(unsigned short *)dest = (unsigned short)v;
                else if (curto >= 2) *(unsigned char *)dest = (unsigned char)v;
                else *(unsigned int *)dest = (unsigned int)v;
                convertidos++;
            }
            break;
        }
        case 'c': {
            int n = larg ? larg : 1;
            char *dest = guardar ? va_arg(ap, char *) : NULL;
            for (int i = 0; i < n; i++) {
                if (!*p) return convertidos;
                if (dest) dest[i] = *p;
                p++;
            }
            if (dest) convertidos++;
            break;
        }
        case 's': {
            const char *ini = p;
            while (*p && !isspace((unsigned char)*p) && (!larg || p - ini < larg)) p++;
            if (guardar) {
                char *dest = va_arg(ap, char *);
                memcpy(dest, ini, (size_t)(p - ini));
                dest[p - ini] = 0;
                convertidos++;
            }
            break;
        }
        case '[': {
            /* Conjunto: [abc], [^abc], [a-z]. */
            f++;
            bool nega = false;
            if (*f == '^') { nega = true; f++; }
            unsigned char conj[32];
            memset(conj, 0, sizeof conj);
            const char *ini_conj = f;
            for (; *f && (*f != ']' || f == ini_conj); f++) {
                unsigned char a = (unsigned char)*f;
                if (f[1] == '-' && f[2] && f[2] != ']') {
                    for (unsigned c = a; c <= (unsigned char)f[2]; c++) conj[c >> 3] |= (unsigned char)(1u << (c & 7));
                    f += 2;
                } else conj[a >> 3] |= (unsigned char)(1u << (a & 7));
            }
            if (!*f) return convertidos;
            const char *ini = p;
            while (*p && (!larg || p - ini < larg)) {
                unsigned char c = (unsigned char)*p;
                bool esta = (conj[c >> 3] >> (c & 7)) & 1;
                if (esta == nega) break;
                p++;
            }
            if (p == ini) return convertidos;
            if (guardar) {
                char *dest = va_arg(ap, char *);
                memcpy(dest, ini, (size_t)(p - ini));
                dest[p - ini] = 0;
                convertidos++;
            }
            break;
        }
        case 'f': case 'e': case 'g': case 'E': case 'G': case 'a': {
            /* %f a un float con strtof: pasar por el doble redondearía dos veces */
            char *fim = NULL;
            bool simples = !longs && !longo_duplo;
            double v = 0;
            float vf = 0;
            if (simples) vf = strtof(p, &fim);
            else v = strtod(p, &fim);
            if (fim == p) return convertidos;
            p = fim;
            if (guardar) {
                if (longo_duplo) *va_arg(ap, long double *) = v;
                else if (longs) *va_arg(ap, double *) = v;
                else *va_arg(ap, float *) = vf;
                convertidos++;
            }
            break;
        }
        case 'n': {
            int *dest = va_arg(ap, int *);
            if (dest) *dest = (int)(p - s);
            break;
        }
        default:
            return convertidos;
        }
    }
    return convertidos;
}

int sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE *f, const char *fmt, ...)
{
    /* Lee una línea y la analiza (basta para los ficheros de opciones). */
    char linha[1024];
    if (!fgets(linha, sizeof linha, f)) return EOF;
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(linha, fmt, ap);
    va_end(ap);
    return r;
}
