// SPDX-License-Identifier: GPL-2.0-or-later
/* cron.c - análisis de las líneas de cron y cálculo de la próxima hora
 * (D216, mejora 074). El formato está en cron.h. Lo usan el servicio cron,
 * crontab (que valida la tabla antes de instalarla) y "cron --proxima", que la
 * prueba compara con una referencia en Python. */
#include "sagui.h"
#include "cron.h"

static const char *const nomes_meses[] = { "jan", "feb", "mar", "apr", "may", "jun",
                                           "jul", "aug", "sep", "oct", "nov", "dec", NULL };
static const char *const nomes_dias[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL };

static int minuscula(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static void erro_em(char *erro, int max, const char *fmt, ...)
{
    if (!erro || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(erro, (size_t)max, fmt, ap);
    va_end(ap);
}

/* Un número o un nombre de tres letras ("mon", "jan"). */
static bool ler_valor(const char **pp, const char *const *nomes, int base, int *v)
{
    const char *p = *pp;
    if (*p >= '0' && *p <= '9') {
        int n = 0, dig = 0;
        while (*p >= '0' && *p <= '9' && dig < 4) { n = n * 10 + (*p++ - '0'); dig++; }
        if (*p >= '0' && *p <= '9') return false;
        *v = n;
        *pp = p;
        return true;
    }
    if (!nomes) return false;
    for (int i = 0; nomes[i]; i++) {
        if (minuscula(p[0]) == nomes[i][0] && minuscula(p[1]) == nomes[i][1] && minuscula(p[2]) == nomes[i][2]) {
            int c = minuscula(p[3]);
            if (c >= 'a' && c <= 'z') return false;   /* "monday": no */
            *v = i + base;
            *pp = p + 3;
            return true;
        }
    }
    return false;
}

static void por_bit(uint32_t *baixo, uint32_t *alto, int v)
{
    if (v < 32) *baixo |= 1u << v;
    else *alto |= 1u << (v - 32);
}

static bool tem_bit(uint32_t baixo, uint32_t alto, int v)
{
    return v < 32 ? (baixo >> v) & 1 : (alto >> (v - 32)) & 1;
}

/* Un campo: lista de elementos separados por comas; cada uno "*", "N", "N-M",
 * con "/paso" opcional ("5/10" = desde 5 hasta el máximo, de 10 en 10). */
static bool campo(const char *texto, const char *nome, int min, int max, const char *const *nomes, int base,
                  bool semana, uint32_t *baixo, uint32_t *alto, bool *livre, char *erro, int me)
{
    *baixo = *alto = 0;
    *livre = texto[0] == '*';
    const char *p = texto;
    for (;;) {
        int ini, fim, passo = 1;
        bool unico = false;
        if (*p == '*') { ini = min; fim = max; p++; }
        else {
            if (!ler_valor(&p, nomes, base, &ini)) { erro_em(erro, me, "%s «%s» no válido", nome, texto); return false; }
            fim = ini;
            unico = true;
            if (*p == '-') {
                p++;
                if (!ler_valor(&p, nomes, base, &fim)) { erro_em(erro, me, "%s «%s»: rango incompleto", nome, texto); return false; }
                unico = false;
            }
        }
        if (*p == '/') {
            p++;
            if (!ler_valor(&p, NULL, 0, &passo) || passo < 1) {
                erro_em(erro, me, "%s «%s»: el paso tiene que ser un número mayor que 0", nome, texto);
                return false;
            }
            if (unico) fim = max;   /* "5/10" como en Vixie cron */
        }
        if (ini < min || fim > max || ini > fim) {
            erro_em(erro, me, "%s «%s» fuera de rango (%d-%d)", nome, texto, min, max);
            return false;
        }
        for (int v = ini; v <= fim; v += passo)
            por_bit(baixo, alto, semana && v == 7 ? 0 : v);
        if (*p == ',') { p++; continue; }
        if (*p) { erro_em(erro, me, "%s «%s»: sobra «%s»", nome, texto, p); return false; }
        return true;
    }
}

static const struct { const char *nome; int especial; const char *campos; } atajos[] = {
    { "reboot",   CRON_ARRANQUE, NULL },
    { "hourly",   CRON_HORARIA,  "0 * * * *" },
    { "daily",    CRON_DIARIA,   "0 0 * * *" },
    { "midnight", CRON_DIARIA,   "0 0 * * *" },
    { "weekly",   CRON_SEMANAL,  "0 0 * * 0" },
    { "monthly",  CRON_MENSUAL,  "0 0 1 * *" },
    { "yearly",   CRON_ANUAL,    "0 0 1 1 *" },
    { "annually", CRON_ANUAL,    "0 0 1 1 *" },
};

/* Copia la palabra siguiente (hasta un espacio) y avanza. */
static const char *palavra(const char *p, char *dst, int max)
{
    int n = 0;
    while (*p && *p != ' ' && *p != '\t') {
        if (n < max - 1) dst[n++] = *p;
        p++;
    }
    dst[n] = 0;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Los cinco campos de "texto" en *q. Devuelve lo que sigue (la orden) o NULL. */
static const char *cinco_campos(const char *p, cron_quando_t *q, char *erro, int me)
{
    char c[5][48];
    for (int i = 0; i < 5; i++) {
        if (!*p) { erro_em(erro, me, "faltan campos: hacen falta 5 (minuto hora día mes día_semana) y la orden"); return NULL; }
        p = palavra(p, c[i], sizeof c[i]);
    }
    uint32_t nada;
    bool livre;
    if (!campo(c[0], "minuto", 0, 59, NULL, 0, false, &q->minutos_baixo, &q->minutos_alto, &livre, erro, me)) return NULL;
    if (!campo(c[1], "hora", 0, 23, NULL, 0, false, &q->horas, &nada, &livre, erro, me)) return NULL;
    if (!campo(c[2], "día", 1, 31, NULL, 0, false, &q->dias, &nada, &q->dia_livre, erro, me)) return NULL;
    if (!campo(c[3], "mes", 1, 12, nomes_meses, 1, false, &q->meses, &nada, &livre, erro, me)) return NULL;
    if (!campo(c[4], "día de la semana", 0, 7, nomes_dias, 0, true, &q->semana, &nada, &q->semana_livre, erro, me)) return NULL;
    return p;
}

int cron_analisar_linha(const char *linha, cron_tarefa_t *t, char *erro, int max_erro)
{
    if (erro && max_erro > 0) erro[0] = 0;
    memset(t, 0, sizeof *t);
    const char *p = linha;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#' || *p == '\n' || *p == '\r') return 0;
    /* NOMBRE=valor: una variable para las tareas de la tabla. */
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_') {
        const char *v = p;
        while ((*v >= 'A' && *v <= 'Z') || (*v >= 'a' && *v <= 'z') || (*v >= '0' && *v <= '9') || *v == '_') v++;
        if (*v == '=') return 2;
    }
    bool sem_recuperar = false;
    if (*p == '!') {
        sem_recuperar = true;
        p++;
        if (*p != '@') { erro_em(erro, max_erro, "el «!» solo va delante de un atajo (@daily...)"); return -1; }
    }
    if (*p == '@') {
        char nome[16];
        p = palavra(p + 1, nome, sizeof nome);
        int k = -1;
        for (int i = 0; i < (int)(sizeof atajos / sizeof atajos[0]); i++)
            if (compara_texto(nome, atajos[i].nome) == 0) k = i;
        if (k < 0) {
            erro_em(erro, max_erro, "atajo «@%s» desconocido (@reboot, @hourly, @daily, @weekly, @monthly, @yearly)", nome);
            return -1;
        }
        t->q.especial = atajos[k].especial;
        if (atajos[k].campos && !cinco_campos(atajos[k].campos, &t->q, erro, max_erro)) return -1;
        t->q.recuperar = atajos[k].especial >= CRON_HORARIA && !sem_recuperar;
    } else {
        p = cinco_campos(p, &t->q, erro, max_erro);
        if (!p) return -1;
        t->q.especial = CRON_NORMAL;
    }
    int n = (int)tam_texto(p);
    while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r' || p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
    if (n == 0) { erro_em(erro, max_erro, "falta la orden que hay que ejecutar"); return -1; }
    if (n >= CRON_COMANDO_MAX) { erro_em(erro, max_erro, "la orden es demasiado larga (máximo %d letras)", CRON_COMANDO_MAX - 1); return -1; }
    memcpy(t->comando, p, (size_t)n);
    t->comando[n] = 0;
    return 1;
}

static bool dia_casa(const cron_quando_t *q, const fuso_data_t *d)
{
    bool dm = (q->dias >> d->dia) & 1, ds = (q->semana >> d->dia_semana) & 1;
    /* Como Vixie cron: con los dos restringidos, basta uno; si uno es "*", los dos. */
    return (q->dia_livre || q->semana_livre) ? dm && ds : dm || ds;
}

bool cron_casa(const cron_quando_t *q, const fuso_data_t *d)
{
    if (q->especial == CRON_ARRANQUE) return false;
    return tem_bit(q->minutos_baixo, q->minutos_alto, d->minuto) && ((q->horas >> d->hora) & 1) &&
           ((q->meses >> d->mes) & 1) && dia_casa(q, d);
}

uint32_t cron_proxima(const cron_quando_t *q, uint32_t depois)
{
    if (q->especial == CRON_ARRANQUE) return 0;
    uint32_t t = (depois / 60 + 1) * 60;
    uint32_t limite = t + 5u * 366u * 86400u;
    /* Salta días y horas enteros que no casan: como mucho unas pocas vueltas
     * por día. Los saltos son en UTC y la hora local se recalcula en cada
     * vuelta, así que los cambios de horario no se saltan ningún minuto. */
    while (t < limite) {
        fuso_data_t d;
        hora_local(t, &d);
        uint32_t s = (uint32_t)d.hora * 3600u + (uint32_t)d.minuto * 60u + d.segundo;
        if (!((q->meses >> d.mes) & 1) || !dia_casa(q, &d)) {
            /* Hasta las 23:00 de ese día; desde ahí, de hora en hora (un día de 23 o 25 horas no engaña). */
            t += s < 23u * 3600u ? 23u * 3600u - s : 3600u - ((uint32_t)d.minuto * 60u + d.segundo);
            continue;
        }
        if (!((q->horas >> d.hora) & 1)) { t += 3600u - ((uint32_t)d.minuto * 60u + d.segundo); continue; }
        if (!tem_bit(q->minutos_baixo, q->minutos_alto, d.minuto)) { t += 60u - d.segundo; continue; }
        return t;
    }
    return 0;
}

uint32_t cron_periodo(int especial)
{
    switch (especial) {
    case CRON_HORARIA: return 3600u;
    case CRON_DIARIA:  return 86400u;
    case CRON_SEMANAL: return 7u * 86400u;
    case CRON_MENSUAL: return 30u * 86400u;
    case CRON_ANUAL:   return 365u * 86400u;
    }
    return 0;
}

int cron_comprobar_tabla(const char *texto, int fd_erros)
{
    int erros = 0, num = 0;
    const char *p = texto;
    while (*p) {
        const char *fim = acha_char(p, '\n');
        int n = fim ? (int)(fim - p) : (int)tam_texto(p);
        char linha[CRON_COMANDO_MAX + 128];
        if (n >= (int)sizeof linha) n = (int)sizeof linha - 1;
        memcpy(linha, p, (size_t)n);
        linha[n] = 0;
        num++;
        cron_tarefa_t t;
        char erro[160];
        if (cron_analisar_linha(linha, &t, erro, sizeof erro) < 0) {
            erros++;
            if (fd_erros >= 0) imprimir_em(fd_erros, "línea %d: %s\n", num, erro);
        }
        if (!fim) break;
        p = fim + 1;
    }
    return erros;
}
