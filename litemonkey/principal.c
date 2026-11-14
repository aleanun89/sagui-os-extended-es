// SPDX-License-Identifier: GPL-2.0-or-later
/* principal.c - el servidor litemonkey: opciones, conexiones y tramas.
 *
 * Un solo hilo con un bucle de eventos (plat_esperar) y muchos clientes a la
 * vez. Cada trama es
 *     longitud (u32 LE, con la cabecera) | id de petición (u32) | código (u32) | documento BSON
 * Código 1 = petición; la respuesta lleva el mismo id y el código 2. */
#include "plataforma.h"
#include "nucleo/lmk.h"

#define CONEXIONES_MAX 256
#define CAB_TRAMA      12

typedef struct {
    int        fd;
    lmk_buf    ent, sal;
    uint32_t   ent_ini, sal_ini;
    lmk_sesion ses;
    char       quien[48];
} conexion_t;

static conexion_t *con[CONEXIONES_MAX];
static int ncon;
static bool hablador;

static void logf_(const char *t) { plat_escribir_log(t); }

static void cerrar_conexion(int i)
{
    conexion_t *c = con[i];
    if (hablador) {
        char m[96];
        lmk_fmt(m, sizeof m, "conexión cerrada: %s", c->quien);
        logf_(m);
    }
    plat_cerrar_socket(c->fd);
    buf_liberar(&c->ent);
    buf_liberar(&c->sal);
    lmk_libera(c);
    con[i] = con[--ncon];
}

/* Intenta mandar lo pendiente; false = la conexión se cayó. */
static bool enviar(conexion_t *c)
{
    while (c->sal_ini < c->sal.n) {
        int r = plat_enviar(c->fd, c->sal.p + c->sal_ini, c->sal.n - c->sal_ini);
        if (r < 0) return false;
        if (r == 0) break;
        c->sal_ini += (uint32_t)r;
    }
    if (c->sal_ini == c->sal.n) {
        c->sal.n = c->sal_ini = 0;
        if (c->sal.cap > 8u * 1024 * 1024) buf_liberar(&c->sal);
    }
    return true;
}

static void responder(conexion_t *c, uint32_t id, const lmk_buf *doc)
{
    buf_u32(&c->sal, CAB_TRAMA + doc->n);
    buf_u32(&c->sal, id);
    buf_u32(&c->sal, LMK_OP_RESPUESTA);
    buf_poner(&c->sal, doc->p, doc->n);
}

static void responder_error(conexion_t *c, uint32_t id, const lmk_error *e)
{
    lmk_buf d = { 0 };
    uint32_t ini = bson_inicio(&d);
    bson_poner_double(&d, "ok", 0.0);
    bson_poner_texto(&d, "errmsg", e->msg);
    bson_poner_int32(&d, "code", e->codigo);
    bson_fin(&d, ini);
    responder(c, id, &d);
    buf_liberar(&d);
}

/* Procesa las tramas completas. Devuelve false si hay que cerrar la
 * conexión (trama imposible) y pone *apagar con la orden shutdown. */
static bool procesar(lmk_servidor *s, conexion_t *c, lmk_buf *resp, bool *apagar)
{
    for (;;) {
        uint32_t hay = c->ent.n - c->ent_ini;
        if (hay < CAB_TRAMA) break;
        const uint8_t *t = c->ent.p + c->ent_ini;
        uint32_t len = lmk_le32(t), id = lmk_le32(t + 4), cod = lmk_le32(t + 8);
        if (len < CAB_TRAMA + 5 || len > LMK_TRAMA_MAX) return false;
        if (hay < len) {
            buf_reservar(&c->ent, len - hay);
            break;
        }
        lmk_error err = { 0 };
        const uint8_t *doc = t + CAB_TRAMA;
        if (cod != LMK_OP_PETICION) {
            lmk_fallo(&err, ERR_VALOR, "código de trama desconocido (1 = petición)");
            responder_error(c, id, &err);
        } else if (!bson_validar(doc, len - CAB_TRAMA, &err)) {
            responder_error(c, id, &err);
        } else {
            if (!servidor_orden(s, &c->ses, doc, resp)) *apagar = true;
            responder(c, id, resp);
            if (resp->cap > 16u * 1024 * 1024) buf_liberar(resp);
        }
        c->ent_ini += len;
    }
    if (c->ent_ini == c->ent.n) {
        c->ent.n = c->ent_ini = 0;
        if (c->ent.cap > 8u * 1024 * 1024) buf_liberar(&c->ent);
    } else if (c->ent_ini > 1024 * 1024) {
        memmove(c->ent.p, c->ent.p + c->ent_ini, c->ent.n - c->ent_ini);
        c->ent.n -= c->ent_ini;
        c->ent_ini = 0;
    }
    return true;
}

static void ayuda(void)
{
    plat_escribir_log(
        "Uso: litemonkey [opciones]\n"
        "  --puerto N        puerto TCP (7979)\n"
        "  --datos CARPETA   dónde se guardan las bases\n"
        "  --escuchar DIR    dirección local (todas; p. ej. 127.0.0.1)\n"
        "  --sync            sincroniza el disco tras cada escritura (lento y seguro)\n"
        "  --memoria MB      documentos en memoria (el resto se lee del disco)\n"
        "  --usuario U --clave C   pide autenticación (HMAC-SHA256; sin TLS: solo en la red local)\n"
        "  -v                cuenta las conexiones\n"
        "  --version");
}

static bool numero(const char *s, uint32_t *v)
{
    uint32_t x = 0;
    if (!*s) return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9' || x > 100000000) return false;
        x = x * 10 + (uint32_t)(*s - '0');
    }
    *v = x;
    return true;
}

int lmk_principal(int argc, char **argv)
{
    uint32_t puerto = LMK_PUERTO, memoria = plat_memoria_mb();
    const char *datos = plat_carpeta_datos(), *escuchar = NULL, *usuario = NULL, *clave = NULL;
    bool sync = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--puerto") && v && numero(v, &puerto) && puerto && puerto < 65536) i++;
        else if (!strcmp(a, "--datos") && v) { datos = v; i++; }
        else if (!strcmp(a, "--escuchar") && v) { escuchar = v; i++; }
        else if (!strcmp(a, "--memoria") && v && numero(v, &memoria)) i++;
        else if (!strcmp(a, "--usuario") && v) { usuario = v; i++; }
        else if (!strcmp(a, "--clave") && v) { clave = v; i++; }
        else if (!strcmp(a, "--sync")) sync = true;
        else if (!strcmp(a, "-v")) hablador = true;
        else if (!strcmp(a, "--version")) { plat_escribir_log(LMK_VERSION); return 0; }
        else if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "--ayuda")) { ayuda(); return 0; }
        else {
            char m[160];
            lmk_fmt(m, sizeof m, "litemonkey: opción no válida o sin valor: %s (--ayuda)", a);
            plat_escribir_log(m);
            return 2;
        }
    }
    if ((usuario == NULL) != (clave == NULL)) {
        plat_escribir_log("litemonkey: --usuario y --clave van juntos");
        return 2;
    }
    static lmk_motor m;
    lmk_error err = { 0 };
    uint64_t t0 = plat_ms();
    if (!motor_abrir(&m, datos, (uint64_t)memoria * 1024 * 1024, sync, &err)) {
        char msg[400];
        lmk_fmt(msg, sizeof msg, "litemonkey: %s (%s)", err.msg, datos);
        plat_escribir_log(msg);
        return 1;
    }
    /* Las colecciones se cargan al usarlas por primera vez; aquí, todas, para
     * avisar ya de un archivo cortado y para que la primera consulta no espere. */
    uint32_t ncol = 0, ndocs = 0;
    for (uint32_t i = 0; i < m.nbases; i++)
        for (uint32_t k = 0; k < m.bases[i]->ncol; k++) {
            lmk_coleccion *c = base_coleccion(m.bases[i], m.bases[i]->col[k]->nombre, false, &err);
            if (c) { ncol++; ndocs += c->vivas; }
            else {
                char msg[400];
                lmk_fmt(msg, sizeof msg, "litemonkey: %s.%s: %s", m.bases[i]->nombre, m.bases[i]->col[k]->nombre, err.msg);
                plat_escribir_log(msg);
            }
        }
    int ls = plat_escuchar(escuchar, (int)puerto);
    if (ls < 0) {
        char msg[160];
        lmk_fmt(msg, sizeof msg, "litemonkey: no se pudo escuchar en el puerto %u", puerto);
        plat_escribir_log(msg);
        motor_cerrar(&m);
        return 1;
    }
    lmk_servidor *s = servidor_nuevo(&m, usuario, clave);
    {
        char msg[400];
        lmk_fmt(msg, sizeof msg, "%s: escuchando en %s:%u; datos en %s (%u colecciones, %u documentos, %u ms)%s%s",
                LMK_VERSION, escuchar ? escuchar : "*", puerto, m.datos, ncol, ndocs, (uint32_t)(plat_ms() - t0),
                sync ? "; --sync" : "", usuario ? "; con autenticación (sin TLS: solo en la red local)" : "");
        plat_escribir_log(msg);
    }
    static int fds[CONEXIONES_MAX + 1];
    static uint8_t quiere[CONEXIONES_MAX + 1], listo[CONEXIONES_MAX + 1];
    lmk_buf resp = { 0 };
    uint64_t ultimo_tic = plat_ms();
    bool apagar = false;
    while (!apagar && !plat_parar()) {
        fds[0] = ls;
        quiere[0] = 0;
        for (int i = 0; i < ncon; i++) {
            fds[i + 1] = con[i]->fd;
            quiere[i + 1] = con[i]->sal_ini < con[i]->sal.n;
        }
        uint64_t ahora = plat_ms();
        int plazo = (int)(ultimo_tic + 1000 > ahora ? ultimo_tic + 1000 - ahora : 0);
        plat_esperar(fds, quiere, listo, ncon + 1, plazo);
        /* conexiones nuevas */
        if (listo[0] & 1) {
            for (;;) {
                char quien[48];
                int fd = plat_aceptar(ls, quien, sizeof quien);
                if (fd < 0) break;
                conexion_t *c = ncon < CONEXIONES_MAX ? lmk_aloca(sizeof *c) : NULL;
                if (!c) { plat_cerrar_socket(fd); continue; }
                memset(c, 0, sizeof *c);
                c->fd = fd;
                lmk_fmt(c->quien, sizeof c->quien, "%s", quien);
                con[ncon++] = c;
                if (hablador) {
                    char msg[96];
                    lmk_fmt(msg, sizeof msg, "conexión de %s", quien);
                    logf_(msg);
                }
            }
        }
        /* datos (de atrás adelante: cerrar mueve la última al hueco) */
        int n0 = ncon;
        for (int i = n0 - 1; i >= 0; i--) {
            conexion_t *c = con[i];
            uint8_t l = 0;
            for (int k = 1; k <= n0; k++)
                if (fds[k] == c->fd) { l = listo[k]; break; }
            bool vivo = true;
            if (l & 1) {
                for (int vueltas = 0; vueltas < 64 && vivo; vueltas++) {
                    if (!buf_reservar(&c->ent, 64 * 1024)) { vivo = false; break; }
                    int r = plat_recibir(c->fd, c->ent.p + c->ent.n, c->ent.cap - c->ent.n);
                    if (r == -1) break;
                    if (r <= 0) { vivo = false; break; }
                    c->ent.n += (uint32_t)r;
                    if (!procesar(s, c, &resp, &apagar)) vivo = false;
                    if (c->sal.n - c->sal_ini > 0 && !enviar(c)) vivo = false;
                }
            }
            if (vivo && (l & 2) && !enviar(c)) vivo = false;
            if (!vivo) cerrar_conexion(i);
        }
        ahora = plat_ms();
        if (ahora - ultimo_tic >= 1000) {
            ultimo_tic = ahora;
            servidor_tic(s);
        }
    }
    /* que las respuestas pendientes (la del shutdown) salgan */
    for (int i = 0; i < ncon; i++) enviar(con[i]);
    while (ncon) cerrar_conexion(ncon - 1);
    plat_cerrar_socket(ls);
    servidor_liberar(s);
    buf_liberar(&resp);
    motor_cerrar(&m);
    plat_escribir_log("litemonkey: cerrado (todo escrito en el disco)");
    return 0;
}
