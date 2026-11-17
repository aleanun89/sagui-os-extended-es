// SPDX-License-Identifier: GPL-2.0-or-later
/* conf_reglas.c - la tabla de dependencias entre opciones de configuración (D367). Ver conf_reglas.h. */
#include "conf_reglas.h"

static int igual(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int es_tri(const char *n) { return igual(n, "a_2d") || igual(n, "a_3d") || igual(n, "a_sdl") || igual(n, "a_disco"); }

int conf_activo(const char *nome, int valor) { return es_tri(nome) ? valor != 2 : valor != 0; }
int conf_valor_inactivo(const char *nome) { return es_tri(nome) ? 2 : 0; }

/* Las opciones del kernel (kconfig, D353) y las de la aceleración (D323). Los nombres de los dos grupos no se pisan. */
const conf_regla_t conf_reglas[] = {
    /* kernel */
    { "nx",                 "pae",         CR_REQUIERE,   0, -1, N_("Requiere PAE") },
    { "cache_alta",         "pae",         CR_REQUIERE,   0, -1, N_("Requiere PAE") },
    { "cache_alta",         "cache_disco", CR_REQUIERE,   0, -1, N_("Requiere la caché de disco") },
    { "escritura_diferida", "cache_disco", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con la caché de disco apagada") },
    { "cache_max_mb",       "cache_disco", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con la caché de disco apagada") },
    { "sucios_max_mb",      "cache_disco", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con la caché de disco apagada") },
    { "vaciado_s",          "cache_disco", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con la caché de disco apagada") },
    { "swap_mb",            "swap",        CR_SIN_EFECTO, 0, -1, N_("Sin efecto con la swap apagada") },
    { "gobernador",         "frecuencia",  CR_SIN_EFECTO, 0, -1, N_("Sin efecto con el control de frecuencia apagado") },
    /* aceleración: SDL por la GPU usa el 3D; el 3D comparte el anillo con el 2D en las Intel */
    { "a_sdl",              "a_3d",        CR_REQUIERE,   0, -1, N_("Requiere el 3D por la GPU") },
    { "a_3d",               "a_2d",        CR_REQUIERE,   1, -1, N_("Requiere el 2D por la GPU (comparten el anillo)") },
    /* barra y menú Inicio (D361, claves de sagui.conf; el estilo Moderno es BC_MENU_MODERNO = 3) */
    { "inicio_buscador",    "inicio_menu", CR_SIN_EFECTO, 0, 3, N_("Solo con el estilo de menú Moderno") },
    { "inicio_translucido", "inicio_menu", CR_SIN_EFECTO, 0, 3, N_("Solo con el estilo de menú Moderno") },
    { "barra_fecha",        "barra_reloj", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con el reloj oculto") },
    { "barra_segundos",     "barra_reloj", CR_SIN_EFECTO, 0, -1, N_("Sin efecto con el reloj oculto") },
};
const int conf_reglas_n = (int)(sizeof conf_reglas / sizeof conf_reglas[0]);

static int padre_activo(const conf_regla_t *r, int v) { return r->padre_igual >= 0 ? v == r->padre_igual : conf_activo(r->padre, v); }

static int vale(conf_leer_fn leer, void *ctx, const char *n, int *v)
{
    int x = leer(n, ctx);
    if (x < 0) return 0;
    *v = x;
    return 1;
}

const char *conf_bloqueo(const char *opcion, conf_leer_fn leer, void *ctx, int comparte)
{
    for (int i = 0; i < conf_reglas_n; i++) {
        const conf_regla_t *r = &conf_reglas[i];
        int v;
        if (!igual(r->opcion, opcion) || (r->solo_anillo && !comparte) || !vale(leer, ctx, r->padre, &v)) continue;
        if (!padre_activo(r, v)) return r->motivo;
    }
    return NULL;
}

/* Estado con correcciones encima del real (la cascada es recursiva: 2D -> 3D -> SDL). */
typedef struct { conf_leer_fn leer; void *ctx; conf_ajuste_t *aj; int n, max; } sup_t;

static int leer_sup(const char *nome, void *c)
{
    sup_t *s = c;
    for (int i = s->n - 1; i >= 0; i--)
        if (igual(s->aj[i].nome, nome)) return s->aj[i].valor;
    return s->leer(nome, s->ctx);
}

static void cascada(sup_t *s, const char *padre, int comparte, int prof)
{
    if (prof > 4) return;
    for (int i = 0; i < conf_reglas_n; i++) {
        const conf_regla_t *r = &conf_reglas[i];
        if (r->clase != CR_REQUIERE || !igual(r->padre, padre) || (r->solo_anillo && !comparte)) continue;
        int pv = leer_sup(padre, s), hv = leer_sup(r->opcion, s);
        if (pv < 0 || hv < 0 || padre_activo(r, pv) || !conf_activo(r->opcion, hv)) continue;
        if (s->n >= s->max) return;
        s->aj[s->n].nome = r->opcion;
        s->aj[s->n].valor = conf_valor_inactivo(r->opcion);
        s->aj[s->n].motivo = r->motivo;
        s->n++;
        cascada(s, r->opcion, comparte, prof + 1);
    }
}

int conf_ajustes(const char *opcion, int valor, conf_leer_fn leer, void *ctx, int comparte, conf_ajuste_t *out, int max)
{
    sup_t s = { leer, ctx, out, 0, max };
    if (max < 1) return 0;
    out[0].nome = opcion; out[0].valor = valor; out[0].motivo = NULL;   /* el cambio pedido va debajo, y se quita al final */
    s.n = 1;
    cascada(&s, opcion, comparte, 0);
    for (int i = 1; i < s.n; i++) out[i - 1] = out[i];
    return s.n - 1;
}

const char *conf_invalido(conf_leer_fn leer, void *ctx, int comparte, const char **opcion)
{
    for (int i = 0; i < conf_reglas_n; i++) {
        const conf_regla_t *r = &conf_reglas[i];
        int pv, hv;
        if (r->clase != CR_REQUIERE || (r->solo_anillo && !comparte) || !vale(leer, ctx, r->padre, &pv) || !vale(leer, ctx, r->opcion, &hv)) continue;
        if (!padre_activo(r, pv) && conf_activo(r->opcion, hv)) { if (opcion) *opcion = r->opcion; return r->motivo; }
    }
    return NULL;
}
