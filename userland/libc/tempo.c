/* tempo.c - hora y fecha para NetSurf (D184), sobre el reloj del SaguiOS
 * (hora_utc, tempo_ms) y su huso horario (D080, fuso.h).
 *
 * Carencias: time_t es de 32 bits (el kernel da los segundos UTC en un
 * unsigned de 32 bits: vale hasta 2106 sin signo, 2038 con signo) y el
 * reloj monótono son milisegundos de 32 bits (se extienden a 64 aquí
 * mirando cuándo dan la vuelta, cada 49,7 días).
 */
#include "sagui.h"

#include "ctype.h"
#include "errno.h"
#include "stdio.h"
#include "string.h"
#include "sys/resource.h"
#include "sys/time.h"
#include "sys/times.h"
#include "time.h"

static const char *dias_curtos[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *dias_longos[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *meses_curtos[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char *meses_longos[] = { "January", "February", "March", "April", "May", "June", "July",
                                      "August", "September", "October", "November", "December" };

/* El reloj monótono en 64 bits: tempo_ms() da la vuelta cada 2^32 ms. */
static unsigned long long monotono_ms(void)
{
    static unsigned ultimo;
    static unsigned long long voltas;
    unsigned agora = tempo_ms();
    if (agora < ultimo) voltas += 1ull << 32;
    ultimo = agora;
    return voltas + agora;
}

time_t time(time_t *t)
{
    time_t v = (time_t)hora_utc(NULL);
    if (t) *t = v;
    return v;
}

/* El instante en que empezó el programa: lo toma un constructor (inicio.asm
 * llama a los de .init_array antes de principal) o, si no lo hubo, la primera
 * consulta. El SaguiOS no cuenta el tiempo de CPU de cada proceso: clock(),
 * times() y getrusage() dan el tiempo real desde ese instante (carencia). */
static unsigned long long ms_inicio;
static int com_inicio;

__attribute__((constructor)) static void tempo_inicio(void)
{
    ms_inicio = monotono_ms();
    com_inicio = 1;
}

static unsigned long long ms_programa(void)
{
    if (!com_inicio) tempo_inicio();
    return monotono_ms() - ms_inicio;
}

clock_t clock(void) { return (clock_t)(ms_programa() & 0x7FFFFFFF); }

clock_t times(struct tms *t)
{
    unsigned long long cs = ms_programa() / 10;
    if (t) { t->tms_utime = (clock_t)cs; t->tms_stime = t->tms_cutime = t->tms_cstime = 0; }
    return (clock_t)((monotono_ms() / 10) & 0x7FFFFFFF);
}

int getrusage(int quem, struct rusage *r)
{
    if (!r) { errno = EFAULT; return -1; }
    memset(r, 0, sizeof *r);
    if (quem == RUSAGE_SELF) {
        unsigned long long ms = ms_programa();
        r->ru_utime.tv_sec = (time_t)(ms / 1000);
        r->ru_utime.tv_usec = (suseconds_t)(ms % 1000) * 1000;
    }
    return 0;
}
double difftime(time_t a, time_t b) { return (double)a - (double)b; }
void tzset(void) { }

int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    hora_t h;
    hora_utc(&h);
    if (tv) {
        tv->tv_sec = (time_t)h.segundos;
        tv->tv_usec = (suseconds_t)h.ms * 1000;
    }
    return 0;
}

int clock_gettime(clockid_t relogio, struct timespec *ts)
{
    if (!ts) { errno = EFAULT; return -1; }
    if (relogio == CLOCK_MONOTONIC) {
        unsigned long long ms = monotono_ms();
        ts->tv_sec = (time_t)(ms / 1000);
        ts->tv_nsec = (long)(ms % 1000) * 1000000L;
        return 0;
    }
    if (relogio == CLOCK_REALTIME) {
        hora_t h;
        hora_utc(&h);
        ts->tv_sec = (time_t)h.segundos;
        ts->tv_nsec = (long)h.ms * 1000000L;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int nanosleep(const struct timespec *pedido, struct timespec *resto)
{
    if (resto) { resto->tv_sec = 0; resto->tv_nsec = 0; }
    dormir((unsigned)(pedido->tv_sec * 1000 + pedido->tv_nsec / 1000000));
    return 0;
}

static void de_fuso(const fuso_data_t *d, struct tm *r)
{
    r->tm_year = (int)d->ano - 1900;
    r->tm_mon = (int)d->mes - 1;
    r->tm_mday = d->dia;
    r->tm_hour = d->hora;
    r->tm_min = d->minuto;
    r->tm_sec = d->segundo;
    r->tm_wday = d->dia_semana;
    r->tm_yday = d->dia_ano;
    r->tm_isdst = d->verao ? 1 : 0;
    r->tm_gmtoff = d->desvio;
    r->tm_zone = NULL;
}

struct tm *gmtime_r(const time_t *t, struct tm *r)
{
    fuso_data_t d;
    fuso_decompor((uint32_t)*t, &d);
    de_fuso(&d, r);
    r->tm_zone = "GMT";
    return r;
}

struct tm *gmtime(const time_t *t)
{
    static struct tm r;
    return gmtime_r(t, &r);
}

struct tm *localtime_r(const time_t *t, struct tm *r)
{
    static char abrev[16];
    fuso_data_t d;
    hora_local((uint32_t)*t, &d);
    de_fuso(&d, r);
    strlcpy(abrev, d.abrev, sizeof abrev);
    r->tm_zone = abrev;
    return r;
}

struct tm *localtime(const time_t *t)
{
    static struct tm r;
    return localtime_r(t, &r);
}

/* Normaliza los campos (mes 13 -> enero del año siguiente, etc.) y
 * devuelve los segundos desde 1970 como si la hora fuera UTC. */
static long long segundos_de(struct tm *t)
{
    long long ano = t->tm_year + 1900LL;
    long long mes = t->tm_mon;
    ano += mes / 12;
    mes %= 12;
    if (mes < 0) { mes += 12; ano--; }
    long long m = mes + 1;
    long long a = ano - (m <= 2);
    long long era = (a >= 0 ? a : a - 399) / 400;
    long long yoe = a - era * 400;
    long long mp = (m + 9) % 12;
    long long doy = (153 * mp + 2) / 5 + t->tm_mday - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long dias = era * 146097 + doe - 719468;
    return dias * 86400LL + t->tm_hour * 3600LL + t->tm_min * 60LL + t->tm_sec;
}

time_t timegm(struct tm *t)
{
    long long s = segundos_de(t);
    time_t r = (time_t)s;
    gmtime_r(&r, t);
    return r;
}

time_t mktime(struct tm *t)
{
    long long local = segundos_de(t);
    if (local < 0 || local > 0xFFFFFFFFLL) { errno = ERANGE; return (time_t)-1; }
    int situacao;
    time_t r = (time_t)local_para_utc((uint32_t)local, &situacao);
    localtime_r(&r, t);
    return r;
}

static size_t por(char *s, size_t max, size_t n, const char *t)
{
    while (*t && n + 1 < max) s[n++] = *t++;
    return n;
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *t)
{
    size_t n = 0;
    if (!max) return 0;
    for (const char *p = fmt; *p; p++) {
        if (n + 1 >= max) { s[max - 1] = 0; return 0; }
        if (*p != '%') { s[n++] = *p; continue; }
        char b[64];
        b[0] = 0;
        p++;
        switch (*p) {
        case 'a': snprintf(b, sizeof b, "%s", dias_curtos[t->tm_wday % 7]); break;
        case 'A': snprintf(b, sizeof b, "%s", dias_longos[t->tm_wday % 7]); break;
        case 'b': case 'h': snprintf(b, sizeof b, "%s", meses_curtos[t->tm_mon % 12]); break;
        case 'B': snprintf(b, sizeof b, "%s", meses_longos[t->tm_mon % 12]); break;
        case 'c': snprintf(b, sizeof b, "%s %s %2d %02d:%02d:%02d %d", dias_curtos[t->tm_wday % 7],
                           meses_curtos[t->tm_mon % 12], t->tm_mday, t->tm_hour, t->tm_min,
                           t->tm_sec, t->tm_year + 1900); break;
        case 'C': snprintf(b, sizeof b, "%02d", (t->tm_year + 1900) / 100); break;
        case 'd': snprintf(b, sizeof b, "%02d", t->tm_mday); break;
        case 'e': snprintf(b, sizeof b, "%2d", t->tm_mday); break;
        case 'D': snprintf(b, sizeof b, "%02d/%02d/%02d", t->tm_mon + 1, t->tm_mday, t->tm_year % 100); break;
        case 'F': snprintf(b, sizeof b, "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday); break;
        case 'H': snprintf(b, sizeof b, "%02d", t->tm_hour); break;
        case 'I': snprintf(b, sizeof b, "%02d", t->tm_hour % 12 ? t->tm_hour % 12 : 12); break;
        case 'j': snprintf(b, sizeof b, "%03d", t->tm_yday + 1); break;
        case 'm': snprintf(b, sizeof b, "%02d", t->tm_mon + 1); break;
        case 'M': snprintf(b, sizeof b, "%02d", t->tm_min); break;
        case 'n': strcpy(b, "\n"); break;
        case 'p': strcpy(b, t->tm_hour < 12 ? "AM" : "PM"); break;
        case 'R': snprintf(b, sizeof b, "%02d:%02d", t->tm_hour, t->tm_min); break;
        case 's': {
            struct tm c = *t;
            snprintf(b, sizeof b, "%ld", (long)mktime(&c));
            break;
        }
        case 'S': snprintf(b, sizeof b, "%02d", t->tm_sec); break;
        case 't': strcpy(b, "\t"); break;
        case 'T': snprintf(b, sizeof b, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec); break;
        case 'u': snprintf(b, sizeof b, "%d", t->tm_wday ? t->tm_wday : 7); break;
        case 'w': snprintf(b, sizeof b, "%d", t->tm_wday); break;
        case 'x': snprintf(b, sizeof b, "%02d/%02d/%02d", t->tm_mon + 1, t->tm_mday, t->tm_year % 100); break;
        case 'X': snprintf(b, sizeof b, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec); break;
        case 'y': snprintf(b, sizeof b, "%02d", t->tm_year % 100); break;
        case 'Y': snprintf(b, sizeof b, "%d", t->tm_year + 1900); break;
        case 'z': {
            long g = t->tm_gmtoff;
            snprintf(b, sizeof b, "%c%02ld%02ld", g < 0 ? '-' : '+', (g < 0 ? -g : g) / 3600, ((g < 0 ? -g : g) / 60) % 60);
            break;
        }
        case 'Z': snprintf(b, sizeof b, "%s", t->tm_zone ? t->tm_zone : "UTC"); break;
        case '%': strcpy(b, "%"); break;
        case 0: p--; break;
        default: b[0] = '%'; b[1] = *p; b[2] = 0; break;
        }
        size_t antes = n;
        n = por(s, max, n, b);
        if (n - antes < strlen(b)) { s[max - 1] = 0; return 0; }
    }
    s[n] = 0;
    return n;
}

static const char *ler_num(const char *s, int min, int max, int ncifras, int *v)
{
    int r = 0, c = 0;
    while (c < ncifras && isdigit((unsigned char)*s)) { r = r * 10 + (*s++ - '0'); c++; }
    if (!c || r < min || r > max) return NULL;
    *v = r;
    return s;
}

static const char *ler_nome(const char *s, const char **curtos, const char **longos, int n, int *v)
{
    for (int i = 0; i < n; i++) {
        size_t l = strlen(longos[i]);
        if (strncasecmp(s, longos[i], l) == 0) { *v = i; return s + l; }
        if (strncasecmp(s, curtos[i], 3) == 0) { *v = i; return s + 3; }
    }
    return NULL;
}

/* strptime: las conversiones que piden NetSurf (%s) y las fechas HTTP. */
char *strptime(const char *s, const char *fmt, struct tm *t)
{
    for (const char *f = fmt; *f && s; f++) {
        if (isspace((unsigned char)*f)) { while (isspace((unsigned char)*s)) s++; continue; }
        if (*f != '%') { if (*s != *f) return NULL; s++; continue; }
        f++;
        int v;
        switch (*f) {
        case 'a': case 'A': s = ler_nome(s, dias_curtos, dias_longos, 7, &t->tm_wday); break;
        case 'b': case 'B': case 'h': s = ler_nome(s, meses_curtos, meses_longos, 12, &t->tm_mon); break;
        case 'd': case 'e': while (*s == ' ') s++; s = ler_num(s, 1, 31, 2, &t->tm_mday); break;
        case 'H': s = ler_num(s, 0, 23, 2, &t->tm_hour); break;
        case 'M': s = ler_num(s, 0, 59, 2, &t->tm_min); break;
        case 'S': s = ler_num(s, 0, 61, 2, &t->tm_sec); break;
        case 'm': s = ler_num(s, 1, 12, 2, &v); if (s) t->tm_mon = v - 1; break;
        case 'Y': s = ler_num(s, 0, 9999, 4, &v); if (s) t->tm_year = v - 1900; break;
        case 'y': s = ler_num(s, 0, 99, 2, &v); if (s) t->tm_year = v < 69 ? v + 100 : v; break;
        case 'j': s = ler_num(s, 1, 366, 3, &v); if (s) t->tm_yday = v - 1; break;
        case 'T': s = strptime(s, "%H:%M:%S", t); break;
        case 'D': s = strptime(s, "%m/%d/%y", t); break;
        case 'n': case 't': while (isspace((unsigned char)*s)) s++; break;
        case 'Z': while (isalpha((unsigned char)*s)) s++; break;
        case 's': {
            if (!isdigit((unsigned char)*s)) return NULL;
            long long seg = 0;
            while (isdigit((unsigned char)*s)) seg = seg * 10 + (*s++ - '0');
            time_t tt = (time_t)seg;
            localtime_r(&tt, t);
            break;
        }
        case '%': if (*s != '%') return NULL; s++; break;
        default: return NULL;
        }
    }
    return (char *)s;
}

char *asctime(const struct tm *t)
{
    static char b[32];
    snprintf(b, sizeof b, "%s %s %2d %02d:%02d:%02d %d\n", dias_curtos[t->tm_wday % 7],
             meses_curtos[t->tm_mon % 12], t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
             t->tm_year + 1900);
    return b;
}

char *ctime(const time_t *t) { return asctime(localtime(t)); }
