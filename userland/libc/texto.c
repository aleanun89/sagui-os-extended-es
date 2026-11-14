/* texto.c - cadenas, caracteres, números y ordenación para NetSurf (D184).
 *
 * La libsagui tiene sus propias funciones de texto con nombres en portugués
 * (tam_texto, compara_texto...) y solo memcpy/memset/memmove/memcmp con los
 * nombres de la C. Aquí están las de string.h, strings.h, ctype.h y la parte
 * numérica de stdlib.h. Partes de esto salen de la libc del OpenTyrian
 * (userland/terceros/opentyrian/libc/libc_ot.c, D156).
 */
#include "sagui.h"

#include "ctype.h"
#include "errno.h"
#include "inttypes.h"
#include "limits.h"
#include "stdlib.h"
#include "string.h"

/* ------------------------------------------------------------- ctype */

int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
int isgraph(int c) { return c > 32 && c < 127; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isprint(int c) { return c >= 32 && c < 127; }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isascii(int c) { return c >= 0 && c < 128; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

/* ------------------------------------------------------------- string.h */

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

size_t strnlen(const char *s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

int strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || !ca) return ca - cb;
    }
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || !ca) return ca - cb;
    }
    return 0;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++)) ;
    return r;
}

char *stpcpy(char *d, const char *s)
{
    while ((*d = *s++)) d++;
    return d;
}

char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

size_t strlcpy(char *d, const char *s, size_t n)
{
    size_t l = strlen(s);
    if (n) {
        size_t c = l < n - 1 ? l : n - 1;
        memcpy(d, s, c);
        d[c] = 0;
    }
    return l;
}

size_t strlcat(char *d, const char *s, size_t n)
{
    size_t ld = strnlen(d, n);
    if (ld == n) return n + strlen(s);
    return ld + strlcpy(d + ld, s, n - ld);
}

char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

char *strncat(char *d, const char *s, size_t n)
{
    char *p = d + strlen(d);
    while (n-- && *s) *p++ = *s++;
    *p = 0;
    return d;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) {
        if (*s == (char)c) r = s;
        if (!*s) return (char *)r;
    }
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c) return (void *)p;
    return NULL;
}

void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--)
        if (*--p == (unsigned char)c) return (void *)p;
    return NULL;
}

void *memmem(const void *h, size_t hn, const void *n, size_t nn)
{
    if (nn == 0) return (void *)h;
    const unsigned char *a = h;
    for (size_t i = 0; i + nn <= hn; i++)
        if (a[i] == *(const unsigned char *)n && memcmp(a + i, n, nn) == 0)
            return (void *)(a + i);
    return NULL;
}

char *strstr(const char *h, const char *n)
{
    size_t ln = strlen(n);
    if (!ln) return (char *)h;
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, ln) == 0) return (char *)h;
    return NULL;
}

/* strchrnul y strcasestr: las da NetSurf (utils/utils.c) cuando no hay
 * _GNU_SOURCE, que es lo que ve su utils/config.h. */

size_t strspn(const char *s, const char *aceitos)
{
    size_t n = 0;
    while (s[n] && strchr(aceitos, s[n])) n++;
    return n;
}

size_t strcspn(const char *s, const char *recusados)
{
    size_t n = 0;
    while (s[n] && !strchr(recusados, s[n])) n++;
    return n;
}

char *strpbrk(const char *s, const char *aceitos)
{
    s += strcspn(s, aceitos);
    return *s ? (char *)s : NULL;
}

char *strtok_r(char *s, const char *sep, char **resto)
{
    if (!s) s = *resto;
    if (!s) return NULL;
    s += strspn(s, sep);
    if (!*s) { *resto = NULL; return NULL; }
    char *fim = s + strcspn(s, sep);
    if (*fim) { *fim = 0; *resto = fim + 1; }
    else *resto = NULL;
    return s;
}

char *strtok(char *s, const char *sep)
{
    static char *resto;
    return strtok_r(s, sep, &resto);
}

char *strsep(char **s, const char *sep)
{
    char *ini = *s;
    if (!ini) return NULL;
    char *fim = ini + strcspn(ini, sep);
    if (*fim) { *fim = 0; *s = fim + 1; }
    else *s = NULL;
    return ini;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

char *strndup(const char *s, size_t n)
{
    size_t l = strnlen(s, n);
    char *d = malloc(l + 1);
    if (!d) return NULL;
    memcpy(d, s, l);
    d[l] = 0;
    return d;
}

char *strerror(int erro)
{
    switch (erro) {
    case 0: return "Sin error";
    case EPERM: return "Operación no permitida";
    case ENOENT: return "No existe el fichero o la carpeta";
    case EIO: return "Error de entrada/salida";
    case EBADF: return "Descriptor no válido";
    case ENOMEM: return "No hay memoria";
    case EACCES: return "Permiso denegado";
    case EEXIST: return "Ya existe";
    case ENOTDIR: return "No es una carpeta";
    case EISDIR: return "Es una carpeta";
    case EINVAL: return "Argumento no válido";
    case EMFILE: return "Demasiados ficheros abiertos";
    case ENOSPC: return "No queda espacio";
    case EROFS: return "Sistema de ficheros de solo lectura";
    case EPIPE: return "Tubería rota";
    case ERANGE: return "Fuera de rango";
    case ENOSYS: return "No existe en el SaguiOS";
    case ENOTEMPTY: return "La carpeta no está vacía";
    case ETIMEDOUT: return "Tiempo agotado";
    case ECONNREFUSED: return "Conexión rechazada";
    default: return "Error desconocido";
    }
}

/* ------------------------------------------------------------- números */

int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }
long long llabs(long long v) { return v < 0 ? -v : v; }
div_t div(int a, int b) { div_t r = { a / b, a % b }; return r; }
ldiv_t ldiv(long a, long b) { ldiv_t r = { a / b, a % b }; return r; }

/* Lee el número sin signo en la base pedida; *estouro si no cabe en 64 bits. */
static unsigned long long ler_sem_sinal(const char *s, char **fim, int base, bool *neg, bool *estouro)
{
    const char *p = s;
    while (isspace((unsigned char)*p)) p++;
    *neg = false;
    *estouro = false;
    if (*p == '-') { *neg = true; p++; }
    else if (*p == '+') p++;
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2])) {
        base = 16;
        p += 2;
    } else if (base == 0) {
        base = (p[0] == '0') ? 8 : 10;
    }
    unsigned long long v = 0;
    bool algum = false;
    for (;; p++) {
        int c = (unsigned char)*p, d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        unsigned long long novo = v * (unsigned)base + (unsigned)d;
        if (novo / (unsigned)base != v) *estouro = true;
        v = novo;
        algum = true;
    }
    if (fim) *fim = (char *)(algum ? p : s);
    return v;
}

long long strtoll(const char *s, char **fim, int base)
{
    bool neg, est;
    unsigned long long v = ler_sem_sinal(s, fim, base, &neg, &est);
    if (est || v > (unsigned long long)LLONG_MAX + neg) {
        errno = ERANGE;
        return neg ? LLONG_MIN : LLONG_MAX;
    }
    return neg ? -(long long)v : (long long)v;
}

unsigned long long strtoull(const char *s, char **fim, int base)
{
    bool neg, est;
    unsigned long long v = ler_sem_sinal(s, fim, base, &neg, &est);
    if (est) { errno = ERANGE; return ULLONG_MAX; }
    return neg ? -v : v;
}

long strtol(const char *s, char **fim, int base)
{
    long long v = strtoll(s, fim, base);
    if (v > LONG_MAX) { errno = ERANGE; return LONG_MAX; }
    if (v < LONG_MIN) { errno = ERANGE; return LONG_MIN; }
    return (long)v;
}

unsigned long strtoul(const char *s, char **fim, int base)
{
    bool neg, est;
    unsigned long long v = ler_sem_sinal(s, fim, base, &neg, &est);
    if (est || v > ULONG_MAX) { errno = ERANGE; return ULONG_MAX; }
    return neg ? -(unsigned long)v : (unsigned long)v;
}

intmax_t strtoimax(const char *s, char **fim, int base) { return strtoll(s, fim, base); }
uintmax_t strtoumax(const char *s, char **fim, int base) { return strtoull(s, fim, base); }
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }
long long atoll(const char *s) { return strtoll(s, NULL, 10); }

/* strtod, strtof y atof (con redondeo correcto) están en numeros.c (D210). */

/* ------------------------------------------------------------- aleatorio */

static unsigned long long semente = 1;

int rand(void)
{
    semente = semente * 6364136223846793005ull + 1442695040888963407ull;
    return (int)((semente >> 33) & 0x7FFFFFFF);
}

void srand(unsigned s) { semente = s; }
long random(void) { return rand(); }
void srandom(unsigned s) { srand(s); }

/* ------------------------------------------------------------- ordenación */

static void trocar(unsigned char *a, unsigned char *b, size_t tam)
{
    while (tam--) { unsigned char t = *a; *a++ = *b; *b++ = t; }
}

static void afundar(unsigned char *b, size_t i, size_t n, size_t tam,
                    int (*cmp)(const void *, const void *))
{
    for (;;) {
        size_t maior = i, e = 2 * i + 1, d = e + 1;
        if (e < n && cmp(b + e * tam, b + maior * tam) > 0) maior = e;
        if (d < n && cmp(b + d * tam, b + maior * tam) > 0) maior = d;
        if (maior == i) return;
        trocar(b + i * tam, b + maior * tam, tam);
        i = maior;
    }
}

/* Heapsort: O(n log n) sin memoria extra (la de OpenTyrian era por
 * inserción, cuadrática; NetSurf ordena listas largas). */
void qsort(void *base, size_t n, size_t tam, int (*cmp)(const void *, const void *))
{
    unsigned char *b = base;
    if (n < 2 || tam == 0) return;
    for (size_t i = n / 2; i-- > 0; ) afundar(b, i, n, tam, cmp);
    for (size_t fim = n - 1; fim > 0; fim--) {
        trocar(b, b + fim * tam, tam);
        afundar(b, 0, fim, tam, cmp);
    }
}

void *bsearch(const void *chave, const void *base, size_t n, size_t tam,
              int (*cmp)(const void *, const void *))
{
    const unsigned char *b = base;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t meio = (lo + hi) / 2;
        int c = cmp(chave, b + meio * tam);
        if (c == 0) return (void *)(b + meio * tam);
        if (c < 0) hi = meio; else lo = meio + 1;
    }
    return NULL;
}
