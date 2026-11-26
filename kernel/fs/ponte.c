// SPDX-License-Identifier: GPL-3.0-only
/* ponte.c - el puente del VFS a un servidor de usuario: las unidades de red de /red (D485).
 *
 * Un proceso de usuario (el servicio «dav») monta una unidad con SIS_PONTE y atiende las operaciones de
 * archivos que los demás programas hacen en ella. Cada operación del sistema de archivos se convierte en un
 * pedido que va a la fila de la unidad; el servidor lo recoge (PONTE_RECEBER), hace lo suyo (HTTP, WebDAV) y
 * contesta (PONTE_RESPONDER). Quien pidió duerme en su pedido, siempre con plazo.
 *
 * Reglas para que nada se cuelgue ni se pierda:
 *   - el montaje va sin trava_fs (montagem_montar_virtual): aquí se espera a un proceso de usuario, que a su
 *     vez usa los discos;
 *   - toda espera tiene plazo; vencido, ERRO_TEMPO y el pedido queda abandonado (la respuesta tardía se tira);
 *   - sin conexión (lo dice el servidor con PONTE_ESTADO), ERRO_SEM_REDE al momento, sin preguntarle;
 *   - si el servidor muere, la unidad se desmonta sola y lo pendiente vuelve con error;
 *   - el servidor que pide algo a su propia unidad recibe ERRO_OCUPADO (no se espera a sí mismo);
 *   - cerrar no espera (puede venir de la IRQ del teclado, Ctrl+C): va a una tabla fija de la unidad;
 *   - cada pedido lo comparten la fila y quien espera (refs): lo suelta el último. Si quien esperaba murió
 *     esperando, lo suelta la fila por él (dono_vivo).
 *
 * El kernel corre con el cerrojo general (D165): una CPU a la vez aquí dentro; solo las IRQ interrumpen, y
 * desde una IRQ solo llega el cierre (fechar), que no toca la fila. */
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "montagem.h"
#include "processo.h"
#include "relogio.h"
#include "sistema.h"
#include "texto.h"

#define PLAZO_CORTO_MS  8000    /* info y listar: lo que pide Archivos al pintar */
#define PLAZO_MS        20000   /* leer, escribir, carpetas, renombrar, borrar */
#define PLAZO_LARGO_MS  60000   /* abrir (el servidor puede descargarlo entero) y sincronizar (subirlo) */
#define LOTE_LISTA      128     /* entradas por viaje al listar */
#define CACHE_LISTA_TQ  (3 * TIQUES_POR_SEGUNDO)
#define MAX_FECHOS      64

enum { PE_FILA = 1, PE_ENTREGUE, PE_RESPONDIDO };

typedef struct pedido {
    struct pedido   *prox;
    uint32_t         id;
    uint8_t          estado;
    uint8_t          refs;          /* la fila y quien espera */
    bool             abandonado;    /* quien esperaba se fue (plazo vencido) */
    bool             dono_soltou;
    processo_t      *dono;
    int              dono_pid;
    ponte_pedido_t   cab;
    uint8_t         *env;           /* datos a enviar (PO_ESCREVER) */
    uint32_t         n_env;
    ponte_resposta_t resp;
    uint8_t         *recv;          /* datos recibidos */
    uint32_t         n_recv;
} pedido_t;

typedef struct {
    disco_t     d;                  /* disco de mentira: la tabla de montajes trabaja con discos */
    bool        usado, viva;
    uint32_t    geracao;
    char        nome[PONTE_NOME_MAX], rotulo[PONTE_ROTULO_MAX];
    uint32_t    uid, gid;
    processo_t *servidor;
    int         servidor_pid;
    uint8_t     estado;             /* PONTE_EST_* */
    montagem_t *m;
    pedido_t   *fila;
    uint32_t    prox_id;
    uint32_t    fechos[MAX_FECHOS];
    volatile uint32_t n_fechos;
    uint32_t    pedidos, erros, esgotados;
    /* el último lote del listado */
    char           cache_rel[CAMINHO_MAX];
    uint32_t       cache_ini, cache_n, cache_tique;
    bool           cache_fim;       /* el lote llegó al final de la carpeta */
    entrada_pasta_t *cache;
} unidade_t;

typedef struct { uint32_t arq, geracao; } arq_ponte_t;

static unidade_t unidades[PONTE_MAX];
static uint32_t geracao_global = 1;
static disco_t disco_red;
static montagem_t *montagem_red;

/* ------------------------------------------------------------ utilidades */

static bool dono_vivo(const pedido_t *p)
{
    const processo_t *d = p->dono;
    return d && d->estado != P_LIVRE && d->estado != P_ZUMBI && d->pid == p->dono_pid;
}

static void soltar(pedido_t *p)
{
    if (--p->refs) return;
    if (p->env) kfree(p->env);
    if (p->recv) { memset(p->recv, 0, p->n_recv); kfree(p->recv); }
    memset(p, 0, sizeof *p);
    kfree(p);
}

/* La fila suelta su parte; si quien esperaba murió esperando, también la suya. */
static void fila_soltar(pedido_t *p)
{
    if (!p->dono_soltou && !dono_vivo(p)) { p->dono_soltou = true; p->refs--; }
    soltar(p);
}

static void tirar_da_fila(unidade_t *u, pedido_t *p)
{
    for (pedido_t **q = &u->fila; *q; q = &(*q)->prox)
        if (*q == p) { *q = p->prox; p->prox = NULL; return; }
}

static bool servidor_vivo(const unidade_t *u)
{
    const processo_t *s = u->servidor;
    return s && s->estado != P_LIVRE && s->estado != P_ZUMBI && s->pid == u->servidor_pid;
}

/* ¿Quien llama puede ver y usar la unidad? Su cuenta o root. */
static bool visivel(const unidade_t *u)
{
    uint32_t uid = processo_recursos()->uid;
    return uid == UID_ROOT || uid == u->uid;
}

static bool e_o_servidor(const unidade_t *u)
{
    return processo_recursos() == processo_lider(u->servidor) && processo_recursos()->pid == u->servidor_pid;
}

static bool pode_dormir(void)
{
    return processo_atual() && processo_atual()->pid != 0 && !contexto_irq();
}

static void desmontar_red_se_vazio(void);

/* La unidad deja de existir: lo pendiente vuelve con error y el montaje se quita. */
static void morrer(unidade_t *u, const char *porque)
{
    if (!u->usado || !u->viva) return;
    u->viva = false;
    klog("[PONTE] /red/%s: fuera (%s)\n", u->nome, porque);
    pedido_t *p = u->fila;
    u->fila = NULL;
    while (p) {
        pedido_t *prox = p->prox;
        p->prox = NULL;
        p->estado = PE_RESPONDIDO;
        p->resp.resultado = ERRO_NAO_EXISTE;
        p->resp.n = 0;
        acordar(p);
        fila_soltar(p);
        p = prox;
    }
    u->n_fechos = 0;
    acordar(u);
    montagem_t *m = u->m;
    u->m = NULL;
    if (m && m->usado) montagem_desmontar(m, false);
    if (u->cache) { kfree(u->cache); u->cache = NULL; }
    u->usado = false;
    desmontar_red_se_vazio();
}

/* Las unidades cuyo servidor ya no existe. */
static void revisar(void)
{
    for (int i = 0; i < PONTE_MAX; i++)
        if (unidades[i].usado && unidades[i].viva && !servidor_vivo(&unidades[i])) morrer(&unidades[i], "el servicio termino");
}

/* ------------------------------------------------------------ pedir y esperar */

/* Manda un pedido y espera la respuesta (o el plazo). En *saida queda el pedido respondido: quien llama
 * lee resp y recv y lo suelta. Devuelve 0 o un error (sin pedido que soltar). */
static int pedir(unidade_t *u, const ponte_pedido_t *cab, const void *dados, uint32_t n, uint32_t ms, pedido_t **saida)
{
    *saida = NULL;
    if (!u->usado || !u->viva) return ERRO_NAO_EXISTE;
    if (!visivel(u)) return ERRO_ACESSO;
    if (!servidor_vivo(u)) { morrer(u, "el servicio termino"); return ERRO_NAO_EXISTE; }
    if (u->estado == PONTE_EST_DESCONECTADA) { u->erros++; return ERRO_SEM_REDE; }
    if (e_o_servidor(u) || !pode_dormir()) return ERRO_OCUPADO;
    pedido_t *p = kcalloc(sizeof *p);
    if (!p) return ERRO_SEM_MEMORIA;
    if (n) {
        p->env = kmalloc(n);
        if (!p->env) { kfree(p); return ERRO_SEM_MEMORIA; }
        memcpy(p->env, dados, n);
        p->n_env = n;
    }
    p->cab = *cab;
    p->cab.n = cab->op == PO_ESCREVER ? n : cab->n;
    p->cab.uid = processo_recursos()->uid;
    if (!++u->prox_id) u->prox_id = 1;
    p->id = p->cab.id = u->prox_id;
    p->estado = PE_FILA;
    p->refs = 2;
    p->dono = processo_atual();
    p->dono_pid = processo_atual()->pid;
    pedido_t **q = &u->fila;
    while (*q) q = &(*q)->prox;
    *q = p;
    u->pedidos++;
    uint32_t geracao = u->geracao;
    acordar(u);
    uint32_t fim = relogio_tiques() + (ms + 9) / 10;
    processo_t *eu = processo_atual();
    while (p->estado != PE_RESPONDIDO) {
        if (u->geracao != geracao || !u->viva) break;
        if (!servidor_vivo(u)) { morrer(u, "el servicio termino"); break; }
        uint32_t agora = relogio_tiques();
        if ((int32_t)(agora - fim) >= 0) break;
        bloquear(p);
        eu->acordar_em = (int32_t)(fim - agora) > 50 ? agora + 50 : fim;   /* cada medio segundo mira al servidor */
        ceder();
        eu->acordar_em = 0;
    }
    if (p->estado != PE_RESPONDIDO) {   /* plazo vencido: la fila se lo queda hasta que el servidor conteste */
        p->abandonado = true;
        p->dono_soltou = true;
        if (u->geracao == geracao) u->esgotados++;
        klog("[PONTE] /red/%s: sin respuesta a la operacion %u en %u ms\n", u->nome, cab->op, ms);
        soltar(p);
        return ERRO_TEMPO;
    }
    p->dono_soltou = true;   /* a partir de aquí lo suelta quien llama */
    if (p->resp.resultado < 0 && u->geracao == geracao) u->erros++;
    *saida = p;
    return 0;
}

static void soltar_resposta(pedido_t *p) { soltar(p); }

/* Pedido sin datos de respuesta: solo el resultado (y la respuesta en *r, si no es NULL). */
static int pedir_simples(unidade_t *u, uint32_t op, const char *rel, const char *destino, uint32_t arq, uint32_t pos,
                         uint32_t modo, uint32_t ms, ponte_resposta_t *r)
{
    ponte_pedido_t cab;
    memset(&cab, 0, sizeof cab);
    cab.op = op;
    cab.arq = arq;
    cab.pos = pos;
    cab.modo = modo;
    if (rel) txt_copia_n(cab.caminho, rel, sizeof cab.caminho);
    if (destino) txt_copia_n(cab.destino, destino, sizeof cab.destino);
    pedido_t *p;
    int e = pedir(u, &cab, NULL, 0, ms, &p);
    if (e < 0) return e;
    int res = p->resp.resultado;
    if (r) *r = p->resp;
    soltar_resposta(p);
    return res;
}

static void cache_invalidar(unidade_t *u) { u->cache_tique = 0; u->cache_n = 0; }

/* ------------------------------------------------------------ el sistema de archivos de una unidad */

#define U ((unidade_t *)priv)

static int p_montar(disco_t *d, uint32_t *opcoes, void **priv) { (void)d; (void)opcoes; (void)priv; return ERRO_NAO_SUPORTADO; }

static int p_desmontar(void *priv, bool gravar)
{
    (void)gravar;
    unidade_t *u = U;
    u->m = NULL;   /* ya se está quitando: morrer no lo vuelve a quitar */
    if (u->usado && u->viva) morrer(u, "desmontada");
    return 0;
}

static int p_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    ponte_resposta_t r;
    int e = pedir_simples(U, PO_INFO, rel, NULL, 0, 0, 0, PLAZO_CORTO_MS, &r);
    if (e < 0) return e;
    inf->tipo = r.tipo == TIPO_PASTA ? TIPO_PASTA : TIPO_ARQUIVO;
    inf->tamanho = r.tamanho;
    return 0;
}

static int p_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    ponte_resposta_t r;
    int e = pedir_simples(U, PO_INFO, rel, NULL, 0, 0, 0, PLAZO_CORTO_MS, &r);
    if (e < 0) return e;
    d->modificado = d->criado = d->acessado = r.modificado;
    return 0;
}

static int p_listar(void *priv, const char *rel, int i, entrada_pasta_t *e)
{
    unidade_t *u = U;
    if (i < 0) return ERRO_INVALIDO;
    uint32_t idx = (uint32_t)i, agora = relogio_tiques();
    bool fresco = u->cache && u->cache_tique && agora - u->cache_tique < CACHE_LISTA_TQ &&
                  txt_compara(u->cache_rel, rel) == 0;
    /* Lo pide de nuevo el índice 0 (una lista nueva) y lo que se sale del lote guardado. */
    if (!fresco || idx == 0 || idx < u->cache_ini || (idx >= u->cache_ini + u->cache_n && !u->cache_fim)) {
        ponte_pedido_t cab;
        memset(&cab, 0, sizeof cab);
        cab.op = PO_LISTAR;
        cab.pos = idx;
        cab.n = LOTE_LISTA;
        txt_copia_n(cab.caminho, rel, sizeof cab.caminho);
        pedido_t *p;
        int r = pedir(u, &cab, NULL, 0, PLAZO_CORTO_MS, &p);
        if (r < 0) return r;
        if (p->resp.resultado < 0) { r = p->resp.resultado; soltar_resposta(p); return r; }
        uint32_t n = p->resp.n;
        if (n > LOTE_LISTA || n * sizeof(entrada_pasta_t) > p->n_recv) n = p->n_recv / sizeof(entrada_pasta_t);
        if (!u->cache) u->cache = kmalloc(LOTE_LISTA * sizeof(entrada_pasta_t));
        if (!u->cache) { soltar_resposta(p); return ERRO_SEM_MEMORIA; }
        memcpy(u->cache, p->recv, n * sizeof(entrada_pasta_t));
        soltar_resposta(p);
        u->cache_ini = idx;
        u->cache_n = n;
        u->cache_fim = n < LOTE_LISTA;
        u->cache_tique = relogio_tiques() ? relogio_tiques() : 1;
        txt_copia_n(u->cache_rel, rel, sizeof u->cache_rel);
    }
    if (idx >= u->cache_ini + u->cache_n) return 0;   /* fin de la carpeta */
    *e = u->cache[idx - u->cache_ini];
    e->nome[NOME_MAX - 1] = 0;
    if (e->tipo != TIPO_PASTA) e->tipo = TIPO_ARQUIVO;
    return 1;
}

static int p_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tam)
{
    unidade_t *u = U;
    ponte_resposta_t r;
    int e = pedir_simples(u, PO_ABRIR, rel, NULL, 0, 0, (uint32_t)modo, PLAZO_LARGO_MS, &r);
    if (e < 0) return e;
    arq_ponte_t *a = arq;
    a->arq = r.arq;
    a->geracao = u->geracao;
    *tam = r.tamanho;
    if (modo & (ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR)) cache_invalidar(u);
    return 0;
}

static int p_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    unidade_t *u = U;
    arq_ponte_t *a = arq;
    if (a->geracao != u->geracao) return ERRO_NAO_EXISTE;
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t k = n - feito > PONTE_DADOS ? PONTE_DADOS : n - feito;
        ponte_pedido_t cab;
        memset(&cab, 0, sizeof cab);
        cab.op = PO_LER;
        cab.arq = a->arq;
        cab.pos = pos + feito;
        cab.n = k;
        pedido_t *p;
        int r = pedir(u, &cab, NULL, 0, PLAZO_MS, &p);
        if (r < 0) return feito ? (int)feito : r;
        if (p->resp.resultado < 0) { r = p->resp.resultado; soltar_resposta(p); return feito ? (int)feito : r; }
        uint32_t got = p->resp.n;
        if (got > k) got = k;
        if (got > p->n_recv) got = p->n_recv;
        memcpy((uint8_t *)buf + feito, p->recv, got);
        soltar_resposta(p);
        feito += got;
        if (got < k) break;   /* fin del fichero */
    }
    return (int)feito;
}

static int p_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{
    unidade_t *u = U;
    arq_ponte_t *a = arq;
    if (a->geracao != u->geracao) return ERRO_NAO_EXISTE;
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t k = n - feito > PONTE_DADOS ? PONTE_DADOS : n - feito;
        ponte_pedido_t cab;
        memset(&cab, 0, sizeof cab);
        cab.op = PO_ESCREVER;
        cab.arq = a->arq;
        cab.pos = pos + feito;
        pedido_t *p;
        int r = pedir(u, &cab, (const uint8_t *)buf + feito, k, PLAZO_MS, &p);
        if (r < 0) return feito ? (int)feito : r;
        r = p->resp.resultado;
        uint32_t t = p->resp.tamanho;
        soltar_resposta(p);
        if (r < 0) return feito ? (int)feito : r;
        *tam = t;
        feito += k;
    }
    cache_invalidar(u);
    return (int)feito;
}

static int p_truncar(void *priv, void *arq, uint32_t tamanho)
{
    unidade_t *u = U;
    arq_ponte_t *a = arq;
    if (a->geracao != u->geracao) return ERRO_NAO_EXISTE;
    cache_invalidar(u);
    return pedir_simples(u, PO_TRUNCAR, NULL, NULL, a->arq, tamanho, 0, PLAZO_MS, NULL);
}

/* Sin esperar ni pedir memoria (puede venir de una IRQ): a la tabla de cierres. */
static void p_fechar(void *priv, void *arq)
{
    unidade_t *u = U;
    arq_ponte_t *a = arq;
    if (!u->usado || !u->viva || a->geracao != u->geracao) return;
    if (u->n_fechos >= MAX_FECHOS) { klog("[PONTE] /red/%s: demasiados cierres pendientes\n", u->nome); return; }
    u->fechos[u->n_fechos++] = a->arq;
    cache_invalidar(u);
    acordar(u);
}

static int p_sincronizar(void *priv, void *arq)
{
    unidade_t *u = U;
    arq_ponte_t *a = arq;
    if (a->geracao != u->geracao) return ERRO_NAO_EXISTE;
    return pedir_simples(u, PO_SINCRONIZAR, NULL, NULL, a->arq, 0, 0, PLAZO_LARGO_MS, NULL);
}

static int p_criar_pasta(void *priv, const char *rel)
{ cache_invalidar(U); return pedir_simples(U, PO_CRIAR_PASTA, rel, NULL, 0, 0, 0, PLAZO_MS, NULL); }
static int p_remover_pasta(void *priv, const char *rel)
{ cache_invalidar(U); return pedir_simples(U, PO_REMOVER_PASTA, rel, NULL, 0, 0, 0, PLAZO_MS, NULL); }
static int p_apagar(void *priv, const char *rel)
{ cache_invalidar(U); return pedir_simples(U, PO_APAGAR, rel, NULL, 0, 0, 0, PLAZO_MS, NULL); }
static int p_renomear(void *priv, const char *de, const char *para)
{
    if (!*de || !*para) return ERRO_OCUPADO;
    cache_invalidar(U);
    return pedir_simples(U, PO_RENOMEAR, de, para, 0, 0, 0, PLAZO_MS, NULL);
}
#undef U

const sistema_arquivos_t fs_ponte = {
    .nome = "webdav", .tam_arq = sizeof(arq_ponte_t),
    .montar = p_montar, .desmontar = p_desmontar, .info = p_info, .listar = p_listar,
    .abrir = p_abrir, .ler = p_ler, .escrever = p_escrever, .truncar = p_truncar, .fechar = p_fechar,
    .sincronizar = p_sincronizar, .criar_pasta = p_criar_pasta, .remover_pasta = p_remover_pasta,
    .apagar = p_apagar, .renomear = p_renomear, .datas = p_datas,
};

/* ------------------------------------------------------------ /red: la lista de las unidades de quien pregunta */

static int r_desmontar(void *priv, bool gravar) { (void)priv; (void)gravar; return 0; }
static int r_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    (void)priv;
    if (*rel) return ERRO_NAO_EXISTE;   /* una unidad viva es su propio montaje (prefijo más largo) */
    inf->tipo = TIPO_PASTA;
    inf->tamanho = 0;
    return 0;
}
static int r_listar(void *priv, const char *rel, int i, entrada_pasta_t *e)
{
    (void)priv;
    if (*rel) return ERRO_NAO_EXISTE;
    revisar();
    for (int k = 0; k < PONTE_MAX; k++) {
        unidade_t *u = &unidades[k];
        if (!u->usado || !u->viva || !visivel(u)) continue;
        if (i-- > 0) continue;
        memset(e, 0, sizeof *e);
        txt_copia_n(e->nome, u->nome, NOME_MAX);
        e->tipo = TIPO_PASTA;
        return 1;
    }
    return 0;
}
static int r_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tam)
{ (void)priv; (void)rel; (void)modo; (void)arq; (void)tam; return ERRO_ACESSO; }
static int r_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; return ERRO_ACESSO; }
static int r_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; (void)tam; return ERRO_ACESSO; }
static int r_truncar(void *priv, void *arq, uint32_t t) { (void)priv; (void)arq; (void)t; return ERRO_ACESSO; }
static void r_fechar(void *priv, void *arq) { (void)priv; (void)arq; }
static int r_nao(void *priv, const char *rel) { (void)priv; (void)rel; return ERRO_ACESSO; }
static int r_renomear(void *priv, const char *de, const char *para) { (void)priv; (void)de; (void)para; return ERRO_ACESSO; }

const sistema_arquivos_t fs_red = {
    .nome = "red", .tam_arq = 4,
    .montar = p_montar, .desmontar = r_desmontar, .info = r_info, .listar = r_listar,
    .abrir = r_abrir, .ler = r_ler, .escrever = r_escrever, .truncar = r_truncar, .fechar = r_fechar,
    .criar_pasta = r_nao, .remover_pasta = r_nao, .apagar = r_nao, .renomear = r_renomear,
};

static int montar_red(void)
{
    if (montagem_red && montagem_red->usado && montagem_red->disco == &disco_red) return 0;
    memset(&disco_red, 0, sizeof disco_red);
    txt_copia_n(disco_red.nome, "red", sizeof disco_red.nome);
    txt_copia_n(disco_red.formato, "unidades de red", sizeof disco_red.formato);
    txt_copia_n(disco_red.tipo_fs, "red", sizeof disco_red.tipo_fs);
    disco_red.presente = true;
    int erro;
    montagem_red = montagem_montar_virtual(&disco_red, "/red", &fs_red, NULL, MONTAR_SO_LEITURA | MONTAR_SEM_EXEC,
                                           UID_ROOT, GID_ROOT, 022, &erro);
    if (!montagem_red) return erro;
    montagem_red->oculto = true;
    return 0;
}

static void desmontar_red_se_vazio(void)
{
    for (int i = 0; i < PONTE_MAX; i++) if (unidades[i].usado) return;
    if (montagem_red && montagem_red->usado && montagem_red->disco == &disco_red) montagem_desmontar(montagem_red, false);
    montagem_red = NULL;
}

/* ------------------------------------------------------------ SIS_PONTE */

static bool nome_valido(const char *n)
{
    if (!n[0]) return false;
    for (int i = 0; n[i]; i++) {
        char c = n[i];
        if (i >= PONTE_NOME_MAX - 1) return false;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

static unidade_t *do_canal(uint32_t canal)
{
    if (canal >= PONTE_MAX) return NULL;
    unidade_t *u = &unidades[canal];
    return u->usado && u->viva ? u : NULL;
}

static int32_t op_montar(uint32_t ptr)
{
    ponte_montar_t pm;
    if (!usuario_ok((void *)ptr, sizeof pm, false)) return ERRO_ENDERECO;
    memcpy(&pm, (void *)ptr, sizeof pm);
    pm.nome[PONTE_NOME_MAX - 1] = 0;
    pm.rotulo[PONTE_ROTULO_MAX - 1] = 0;
    if (!nome_valido(pm.nome)) return ERRO_INVALIDO;
    processo_t *eu = processo_recursos();
    if (!eu->usuario) return ERRO_ACESSO;
    revisar();
    int livre = -1;
    for (int i = 0; i < PONTE_MAX; i++) {
        if (unidades[i].usado && txt_compara(unidades[i].nome, pm.nome) == 0) return ERRO_JA_EXISTE;
        if (!unidades[i].usado && livre < 0) livre = i;
    }
    if (livre < 0) return ERRO_LIMITE;
    int r = montar_red();
    if (r < 0) return r;
    unidade_t *u = &unidades[livre];
    memset(u, 0, sizeof *u);
    txt_copia_n(u->nome, pm.nome, sizeof u->nome);
    for (int i = 0; pm.rotulo[i]; i++) if ((uint8_t)pm.rotulo[i] < ' ') pm.rotulo[i] = ' ';
    txt_copia_n(u->rotulo, pm.rotulo, sizeof u->rotulo);
    snformatar(u->d.nome, sizeof u->d.nome, "red%d", livre);
    txt_copia_n(u->d.formato, "WebDAV", sizeof u->d.formato);
    txt_copia_n(u->d.tipo_fs, "webdav", sizeof u->d.tipo_fs);
    u->d.presente = true;
    u->uid = eu->uid;
    u->gid = eu->gid;
    u->servidor = eu;
    u->servidor_pid = eu->pid;
    u->estado = PONTE_EST_CONECTANDO;
    u->geracao = ++geracao_global;
    u->usado = u->viva = true;
    char ponto[CAMINHO_MAX];
    snformatar(ponto, sizeof ponto, "/red/%s", u->nome);
    uint32_t opcoes = MONTAR_SEM_EXEC | (pm.opcoes & MONTAR_SO_LEITURA);
    int erro;
    u->m = montagem_montar_virtual(&u->d, ponto, &fs_ponte, u, opcoes, u->uid, u->gid, 077, &erro);
    if (!u->m) {
        u->usado = u->viva = false;
        desmontar_red_se_vazio();
        return erro;
    }
    u->m->privado = true;
    klog("[PONTE] /red/%s montada por pid %d (uid %u)\n", u->nome, u->servidor_pid, u->uid);
    return livre;
}

static int32_t op_receber(uint32_t canal, uint32_t buf, uint32_t max, uint32_t ms)
{
    unidade_t *u = do_canal(canal);
    if (!u || !e_o_servidor(u)) return ERRO_ACESSO;
    if (max < sizeof(ponte_pedido_t)) return ERRO_LIMITE;
    if (!usuario_ok((void *)buf, max, true)) return ERRO_ENDERECO;
    if (ms > 5000) ms = 5000;
    uint32_t geracao = u->geracao;
    uint32_t fim = relogio_tiques() + (ms + 9) / 10;
    processo_t *eu = processo_atual();
    for (;;) {
        if (!u->usado || !u->viva || u->geracao != geracao) return ERRO_NAO_EXISTE;
        if (u->n_fechos) {   /* los cierres primero: sus escrituras ya terminaron todas */
            ponte_pedido_t cab;
            memset(&cab, 0, sizeof cab);
            cab.op = PO_FECHAR;
            cab.uid = u->uid;
            /* p_fechar puede llegar desde la IRQ del teclado (Ctrl+C): la tabla se toca sin interrupciones. */
            uint32_t fl;
            __asm__ volatile("pushfl; popl %0; cli" : "=r"(fl) :: "memory");
            cab.arq = u->fechos[0];
            for (uint32_t i = 1; i < u->n_fechos; i++) u->fechos[i - 1] = u->fechos[i];
            u->n_fechos--;
            if (fl & 0x200) __asm__ volatile("sti" ::: "memory");
            memcpy((void *)buf, &cab, sizeof cab);
            return (int32_t)sizeof cab;
        }
        for (pedido_t *p = u->fila, *prox; p; p = prox) {
            prox = p->prox;
            if (p->estado != PE_FILA) continue;
            if (p->abandonado) { tirar_da_fila(u, p); fila_soltar(p); continue; }
            if (sizeof p->cab + p->n_env > max) return ERRO_LIMITE;
            memcpy((void *)buf, &p->cab, sizeof p->cab);
            if (p->n_env) memcpy((uint8_t *)buf + sizeof p->cab, p->env, p->n_env);
            int32_t n = (int32_t)(sizeof p->cab + p->n_env);
            if (p->env) { kfree(p->env); p->env = NULL; }
            p->n_env = 0;
            p->estado = PE_ENTREGUE;
            return n;
        }
        uint32_t agora = relogio_tiques();
        if (!ms || (int32_t)(agora - fim) >= 0 || !pode_dormir()) return 0;
        bloquear(u);
        eu->acordar_em = fim;
        ceder();
        eu->acordar_em = 0;
    }
}

static int32_t op_responder(uint32_t canal, uint32_t buf, uint32_t n)
{
    unidade_t *u = do_canal(canal);
    if (!u || !e_o_servidor(u)) return ERRO_ACESSO;
    if (n < sizeof(ponte_resposta_t) || n > sizeof(ponte_resposta_t) + PONTE_DADOS) return ERRO_INVALIDO;
    if (!usuario_ok((void *)buf, n, false)) return ERRO_ENDERECO;
    ponte_resposta_t r;
    memcpy(&r, (void *)buf, sizeof r);
    pedido_t *p = u->fila;
    while (p && !(p->id == r.id && p->estado == PE_ENTREGUE)) p = p->prox;
    if (!p) return 0;   /* tardía (el pedido se abandonó y ya no está) o inventada */
    tirar_da_fila(u, p);
    if (p->abandonado) { fila_soltar(p); return 0; }
    uint32_t datos = n - (uint32_t)sizeof r;
    if (p->cab.op == PO_LER && datos > p->cab.n) datos = p->cab.n;
    if (p->cab.op != PO_LER && p->cab.op != PO_LISTAR) datos = 0;
    if (datos) {
        p->recv = kmalloc(datos);
        if (!p->recv) { r.resultado = ERRO_SEM_MEMORIA; datos = 0; }
        else memcpy(p->recv, (uint8_t *)buf + sizeof r, datos);
    }
    p->n_recv = datos;
    if (p->cab.op == PO_LER && r.n > datos) r.n = datos;
    if (p->cab.op == PO_LISTAR && r.n * sizeof(entrada_pasta_t) > datos) r.n = datos / (uint32_t)sizeof(entrada_pasta_t);
    if (r.resultado > 0) r.resultado = 0;
    p->resp = r;
    p->estado = PE_RESPONDIDO;
    acordar(p);
    fila_soltar(p);
    return 0;
}

static int32_t op_estado(uint32_t canal, uint32_t est)
{
    unidade_t *u = do_canal(canal);
    if (!u || !e_o_servidor(u)) return ERRO_ACESSO;
    if (est > PONTE_EST_CONECTANDO) return ERRO_INVALIDO;
    if (u->estado != est)
        klog("[PONTE] /red/%s: %s\n", u->nome, est == PONTE_EST_CONECTADA ? "conectada" : est == PONTE_EST_DESCONECTADA ? "desconectada" : "conectando");
    u->estado = (uint8_t)est;
    if (est == PONTE_EST_DESCONECTADA) {   /* lo que aún no recogió: error ya, sin esperar */
        cache_invalidar(u);
        for (pedido_t *p = u->fila, *prox; p; p = prox) {
            prox = p->prox;
            if (p->estado != PE_FILA) continue;
            tirar_da_fila(u, p);
            p->resp.resultado = ERRO_SEM_REDE;
            p->estado = PE_RESPONDIDO;
            acordar(p);
            fila_soltar(p);
        }
    }
    return 0;
}

static int32_t op_listar(uint32_t ptr, uint32_t n)
{
    if (n > PONTE_MAX) n = PONTE_MAX;
    if (n && !usuario_ok((void *)ptr, n * sizeof(ponte_info_t), true)) return ERRO_ENDERECO;
    revisar();
    ponte_info_t *v = (ponte_info_t *)ptr;
    uint32_t k = 0;
    for (int i = 0; i < PONTE_MAX && k < n; i++) {
        unidade_t *u = &unidades[i];
        if (!u->usado || !u->viva || !visivel(u)) continue;
        memset(&v[k], 0, sizeof v[k]);
        txt_copia_n(v[k].nome, u->nome, sizeof v[k].nome);
        txt_copia_n(v[k].rotulo, u->rotulo, sizeof v[k].rotulo);
        v[k].estado = u->estado;
        v[k].uid = u->uid;
        v[k].pid = u->servidor_pid;
        v[k].pedidos = u->pedidos;
        v[k].erros = u->erros;
        v[k].tempo_esgotado = u->esgotados;
        v[k].canal = (uint32_t)i;
        k++;
    }
    return (int32_t)k;
}

int32_t sis_ponte(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    switch (op) {
    case PONTE_MONTAR:    return op_montar(a);
    case PONTE_RECEBER:   return op_receber(a, b, c, d);
    case PONTE_RESPONDER: return op_responder(a, b, c);
    case PONTE_ESTADO:    return op_estado(a, b);
    case PONTE_LISTAR:    return op_listar(a, b);
    case PONTE_DESMONTAR: {
        if (a >= PONTE_MAX) return ERRO_INVALIDO;
        unidade_t *u = &unidades[a];
        if (!u->usado || !u->viva) return ERRO_NAO_EXISTE;
        if (!e_o_servidor(u) && processo_recursos()->uid != UID_ROOT && processo_recursos()->uid != u->uid) return ERRO_ACESSO;
        morrer(u, "desmontada");
        return 0;
    }
    default:              return ERRO_INVALIDO;
    }
}

/* D485: al cerrar la sesión, las unidades de las cuentas que ya no están (sus servidores murieron con la sesión;
 * esto solo adelanta lo que haría revisar()). */
void ponte_revisar(void) { revisar(); }

/* D485: para la bandeja: cuántas unidades tiene la cuenta "uid" y cuántas sin conexión (y el nombre de la primera). */
int ponte_bandeja(uint32_t uid, int *sem_rede, char *nome, uint32_t max)
{
    int n = 0;
    *sem_rede = 0;
    if (nome && max) nome[0] = 0;
    for (int i = 0; i < PONTE_MAX; i++) {
        unidade_t *u = &unidades[i];
        if (!u->usado || !u->viva || u->uid != uid) continue;
        if (!servidor_vivo(u)) continue;
        if (!n && nome) txt_copia_n(nome, u->nome, max);
        if (u->estado == PONTE_EST_DESCONECTADA) (*sem_rede)++;
        n++;
    }
    return n;
}
