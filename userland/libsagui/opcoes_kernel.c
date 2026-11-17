// SPDX-License-Identifier: GPL-2.0-or-later
/* opcoes_kernel.c - textos y lectura de las opciones del kernel que se guardan desde el sistema (D353).
 * Ver userland/inc/opcoes_kernel.h. Una opción que el kernel enseña y aquí no se conoce sale con su
 * clave como nombre (así un kernel más nuevo no deja la lista a medias). */
#include "opcoes_kernel.h"
#include "conf_reglas.h"

typedef struct { const char *nome, *rotulo, *ayuda; } texto_op_t;

static const texto_op_t textos[] = {
    { "pae", N_("Paginación PAE (más de 4 GB)"), N_("Con «no», la paginación de siempre: la memoria por encima de 4 GB no se usa.") },
    { "nx", N_("Protección NX"), N_("Las páginas de datos no se pueden ejecutar (solo con PAE y si la CPU lo tiene).") },
    { "cache_disco", N_("Caché de disco"), N_("Guarda en la RAM lo leído y escrito en los discos.") },
    { "cache_alta", N_("Caché en la memoria alta"), N_("Con «no», la caché se queda en la memoria baja (como mucho 64 MB).") },
    { "escritura_diferida", N_("Escritura diferida"), N_("Lo escrito se graba unos segundos después; con «no», al momento.") },
    { "dma", N_("DMA de los discos IDE"), N_("Con «no», todo por PIO: más lento, pero sirve si el DMA da problemas.") },
    { "cache_max_mb", N_("Tamaño máximo de la caché"), N_("Solo puede bajar el tope automático.") },
    { "sucios_max_mb", N_("Máximo sin grabar en la caché"), N_("Cuánto puede esperar a grabarse; menos = menos riesgo si se va la luz.") },
    { "vaciado_s", N_("Grabar lo pendiente cada"), N_("Segundos que espera un bloque escrito antes de grabarse (3 de serie).") },
    { "frecuencia", N_("Control de frecuencia de la CPU"), N_("Con «no», la CPU queda como la dejó la BIOS.") },
    { "gobernador", N_("Gobernador de la frecuencia"), N_("Rendimiento, ahorro o a demanda; se aplica también al momento.") },
    { "swap", N_("Swap comprimida en la RAM"), N_("Cuando falta memoria, comprime en la RAM las páginas menos usadas; con «no», no hay swap.") },
    { "swap_mb", N_("Tamaño máximo de la swap"), N_("Tope del almacén comprimido; automático = la mitad de la RAM (un cuarto sin memoria alta).") },
};

static const texto_op_t *texto_de(const char *nome)
{
    for (unsigned i = 0; i < sizeof textos / sizeof textos[0]; i++)
        if (compara_texto(textos[i].nome, nome) == 0) return &textos[i];
    return NULL;
}

int kconf_ler(info_kconf_t *v, int max, unsigned *estado)
{
    int r = info_sistema(INFO_KCONF, v, (unsigned)max);
    if (r < 0) return r;
    if (estado) *estado = (unsigned)r >> 16;
    return r & 0xFFFF;
}

int kconf_fijar(int hueco, unsigned valor)
{
    return info_sistema(INFO_KCONF_GRAVAR, (void *)(long)KCONF_PEDIDO(KCONF_OP_FIJAR, hueco, valor), 0);
}

int kconf_quitar(int hueco)
{
    return info_sistema(INFO_KCONF_GRAVAR, (void *)(long)KCONF_PEDIDO(KCONF_OP_QUITAR, hueco, 0), 0);
}

int kconf_quitar_todo(void)
{
    return info_sistema(INFO_KCONF_GRAVAR, (void *)(long)KCONF_PEDIDO(KCONF_OP_QUITAR_TODO, 0, 0), 0);
}

const char *kconf_rotulo(const char *nome)
{
    const texto_op_t *t = texto_de(nome);
    return t ? _(t->rotulo) : nome;
}

const char *kconf_ayuda(const char *nome)
{
    const texto_op_t *t = texto_de(nome);
    return t ? _(t->ayuda) : "";
}

static bool termina_em(const char *s, const char *fim)
{
    size_t a = tam_texto(s), b = tam_texto(fim);
    return a >= b && compara_texto(s + a - b, fim) == 0;
}

static const char *const gobernadores[] = { "rendimiento", "ahorro", "demanda" };

void kconf_texto_valor(const info_kconf_t *o, unsigned v, char *buf, size_t n)
{
    if (o->tipo == KCONF_SINO) copia_texto(buf, v ? _("sí") : _("no"), n);
    else if (v == 0) copia_texto(buf, _("automático"), n);
    else if (o->tipo == KCONF_LISTA && compara_texto(o->nome, "gobernador") == 0 && v <= 3)
        copia_texto(buf, v == 1 ? _("rendimiento") : v == 2 ? _("ahorro") : _("a demanda"), n);
    else if (termina_em(o->nome, "_mb")) formatar_texto(buf, n, "%u MB", v);
    else if (termina_em(o->nome, "_s")) formatar_texto(buf, n, "%u s", v);
    else formatar_texto(buf, n, "%u", v);
}

int kconf_interpretar(const info_kconf_t *o, const char *t, unsigned *v)
{
    if (compara_texto(t, "defecto") == 0 || compara_texto(t, "imagen") == 0) return 1;
    if (o->tipo == KCONF_SINO) {
        if (compara_texto(t, "si") == 0 || compara_texto(t, "sí") == 0 || compara_texto(t, "1") == 0) { *v = 1; return 0; }
        if (compara_texto(t, "no") == 0 || compara_texto(t, "0") == 0) { *v = 0; return 0; }
        return -1;
    }
    if (compara_texto(t, "auto") == 0 || compara_texto(t, "automatico") == 0 || compara_texto(t, "automático") == 0) { *v = 0; return 0; }
    if (o->tipo == KCONF_LISTA && compara_texto(o->nome, "gobernador") == 0) {
        for (unsigned i = 0; i < 3; i++)
            if (compara_texto(t, gobernadores[i]) == 0) { *v = i + 1; return 0; }
        if (compara_texto(t, "a_demanda") == 0) { *v = 3; return 0; }
    }
    if (*t < '0' || *t > '9') return -1;
    for (const char *p = t; *p; p++)
        if (*p < '0' || *p > '9') return -1;
    int x = texto_para_int(t);
    if (x < 0 || x > 65535 || (x && (x < o->min || x > o->max))) return -1;
    *v = (unsigned)x;
    return 0;
}

unsigned kconf_siguiente(const info_kconf_t *o, unsigned v)
{
    static const unsigned esc_cache[] = { 0, 16, 32, 64, 128, 256, 512, 1024 };
    static const unsigned esc_sucios[] = { 0, 1, 2, 4, 8, 16 };
    static const unsigned esc_vaciado[] = { 0, 1, 3, 5, 10, 30 };
    static const unsigned esc_swap[] = { 0, 16, 64, 256, 512, 1024 };
    if (o->tipo == KCONF_SINO) return !v;
    if (o->tipo == KCONF_LISTA) return v >= o->max ? 0 : v + 1;
    const unsigned *e = NULL;
    unsigned n = 0;
    if (compara_texto(o->nome, "cache_max_mb") == 0) { e = esc_cache; n = 8; }
    else if (compara_texto(o->nome, "sucios_max_mb") == 0) { e = esc_sucios; n = 6; }
    else if (compara_texto(o->nome, "vaciado_s") == 0) { e = esc_vaciado; n = 6; }
    else if (compara_texto(o->nome, "swap_mb") == 0) { e = esc_swap; n = 6; }
    if (!e) return v >= o->max ? 0 : v < o->min ? o->min : v + 1;
    for (unsigned i = 0; i < n; i++)
        if (e[i] > v && (e[i] >= o->min || e[i] == 0) && e[i] <= o->max) return e[i];
    return 0;
}

bool kconf_pide_reinicio(const info_kconf_t *o)
{
    if (o->tipo == KCONF_LISTA && compara_texto(o->nome, "gobernador") == 0 && o->proximo == 0) return false;
    return o->proximo != o->efetivo;
}

/* ---- D367: dependencias ---- */

typedef struct { const info_kconf_t *ops; int n; int val[KCONF_MAX]; } estado_k_t;

static int leer_k(const char *nome, void *c)
{
    estado_k_t *e = c;
    for (int i = 0; i < e->n; i++)
        if (compara_texto(e->ops[i].nome, nome) == 0) return e->val[i];
    return -1;
}

static void estado_de(estado_k_t *e, const info_kconf_t *ops, int n)
{
    e->ops = ops;
    e->n = n > KCONF_MAX ? KCONF_MAX : n;
    for (int i = 0; i < e->n; i++) e->val[i] = ops[i].proximo;
}

const char *kconf_bloqueo(const info_kconf_t *ops, int n, const char *nome)
{
    estado_k_t e;
    estado_de(&e, ops, n);
    const char *m = conf_bloqueo(nome, leer_k, &e, 0);
    return m ? _(m) : NULL;
}

int kconf_resolver(const info_kconf_t *ops, int n_ops, const kconf_cambio_t *pedidos, int n, kconf_cambio_t *salida, int max, char *error, size_t ne)
{
    estado_k_t e;
    estado_de(&e, ops, n_ops);
    /* lo que la imagen ya traía mal (p. ej. NX con --sem-pae) no impide cambiar otras cosas: solo cuenta lo nuevo */
    const char *op0 = NULL;
    const char *mal0 = conf_invalido(leer_k, &e, 0, &op0);
    int ns = 0;
    int pidio[KCONF_MAX] = { 0 };
    for (int i = 0; i < n; i++) {
        int k = -1;
        for (int j = 0; j < e.n; j++) if (ops[j].hueco == pedidos[i].hueco) k = j;
        if (k < 0 || ns >= max) continue;
        e.val[k] = pedidos[i].quitar ? ops[k].imagem : (int)pedidos[i].valor;
        pidio[k] = 1;
        salida[ns++] = pedidos[i];
    }
    /* en cascada: lo que depende de algo que quedó apagado, y no se pidió, se apaga solo con su aviso */
    for (int i = 0; i < n; i++) {
        int k = -1;
        for (int j = 0; j < e.n; j++) if (ops[j].hueco == pedidos[i].hueco) k = j;
        if (k < 0) continue;
        conf_ajuste_t aj[8];
        int na = conf_ajustes(ops[k].nome, e.val[k], leer_k, &e, 0, aj, 8);
        for (int a = 0; a < na; a++) {
            int d = -1;
            for (int j = 0; j < e.n; j++) if (compara_texto(ops[j].nome, aj[a].nome) == 0) d = j;
            if (d < 0 || pidio[d] || ns >= max) continue;
            e.val[d] = aj[a].valor;
            salida[ns].hueco = ops[d].hueco; salida[ns].valor = (unsigned)aj[a].valor; salida[ns].quitar = 0;
            salida[ns].aviso = _(aj[a].motivo);
            ns++;
        }
    }
    /* el resultado: nada inválido, y lo pedido no puede quedar deshabilitado */
    const char *op = NULL;
    const char *mal = conf_invalido(leer_k, &e, 0, &op);
    if (mal && !(mal0 && op0 && op && compara_texto(op0, op) == 0)) { formatar_texto(error, ne, "%s: %s", op, _(mal)); return -1; }
    for (int i = 0; i < n; i++) {
        int k = -1;
        for (int j = 0; j < e.n; j++) if (ops[j].hueco == pedidos[i].hueco) k = j;
        if (k < 0 || pedidos[i].quitar || pedidos[i].valor == 0) continue;
        const char *m = conf_bloqueo(ops[k].nome, leer_k, &e, 0);
        if (m) { formatar_texto(error, ne, "%s: %s", ops[k].nome, _(m)); return -1; }
    }
    return ns;
}
