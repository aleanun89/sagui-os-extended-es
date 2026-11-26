// SPDX-License-Identifier: GPL-2.0-or-later
/* qjs_rede.c - la red del JavaScript del Cipó: fetch, XMLHttpRequest, cookies, almacenamiento
 * y WebSocket (D487).
 *
 * Aquí solo están las funciones nativas (__sagui_*); la API del navegador que las usa está en
 * qjs_rede.js (embebido por tools/js2h.py). El puente (qjs_bridge.c) llama a qjs_rede_preparar()
 * al crear el motor de cada página y a qjs_rede_url() cuando cambia la dirección. */
#include "sagui.h"
#include "quickjs.h"
#include "rede.h"
#include "qjs_rede_js.h"   /* generado: const char qjs_rede_js[] */

static char pagina_url[REDE_URL_MAX];
static int pedidos_pagina;
#define PEDIDOS_MAX 400

void qjs_rede_url(const char *url) { copia_texto(pagina_url, url ? url : "", sizeof pagina_url); }

/* ---- utilidades ---- */

static char *arg_texto(JSContext *ctx, JSValueConst v)
{
    size_t n = 0;
    const char *s = JS_ToCStringLen(ctx, &n, v);
    if (!s) return NULL;
    char *o = aloca(n + 1);
    if (o) { memcpy(o, s, n); o[n] = 0; }
    JS_FreeCString(ctx, s);
    return o;
}

/* El cuerpo como bytes: ArrayBuffer, o texto UTF-8. */
static uint8_t *arg_bytes(JSContext *ctx, JSValueConst v, int *n, bool *propio)
{
    *n = 0; *propio = false;
    size_t t = 0;
    uint8_t *p = JS_GetArrayBuffer(ctx, &t, v);
    if (p) { *n = (int)t; return p; }
    JS_FreeValue(ctx, JS_GetException(ctx));   /* no era un ArrayBuffer: se limpia el error */
    if (JS_IsUndefined(v) || JS_IsNull(v)) return NULL;
    size_t tl = 0;
    const char *s = JS_ToCStringLen(ctx, &tl, v);
    if (!s) return NULL;
    uint8_t *o = aloca(tl + 1);
    if (o) { memcpy(o, s, tl); *n = (int)tl; *propio = true; }
    JS_FreeCString(ctx, s);
    return o;
}

/* __sagui_ua(): el User-Agent. */
static JSValue nat_ua(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    return JS_NewString(ctx, REDE_UA);
}

/* __sagui_utf8_enc(texto) -> ArrayBuffer */
static JSValue nat_utf8_enc(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    size_t n = 0;
    const char *s = argc > 0 ? JS_ToCStringLen(ctx, &n, argv[0]) : NULL;
    if (!s) return JS_NewArrayBufferCopy(ctx, (const uint8_t *)"", 0);
    JSValue r = JS_NewArrayBufferCopy(ctx, (const uint8_t *)s, n);
    JS_FreeCString(ctx, s);
    return r;
}

/* __sagui_utf8_dec(ArrayBuffer) -> texto */
static JSValue nat_utf8_dec(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    size_t n = 0;
    uint8_t *p = argc > 0 ? JS_GetArrayBuffer(ctx, &n, argv[0]) : NULL;
    if (!p) { JS_FreeValue(ctx, JS_GetException(ctx)); return JS_NewString(ctx, ""); }
    return JS_NewStringLen(ctx, (const char *)p, n);
}

/* __sagui_http(metodo, url, cabs, cuerpo, flags) -> [codigo, urlFinal, cabeceras, ArrayBuffer, extra] | "error" */
static JSValue nat_http(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    if (argc < 5) return JS_NewString(ctx, "argumentos");
    if (pedidos_pagina >= PEDIDOS_MAX) return JS_NewString(ctx, "demasiados pedidos");
    pedidos_pagina++;
    char *metodo = arg_texto(ctx, argv[0]), *url = arg_texto(ctx, argv[1]), *cabs = arg_texto(ctx, argv[2]);
    int flags = 0, n = 0;
    JS_ToInt32(ctx, &flags, argv[4]);
    bool propio = false;
    uint8_t *corpo = arg_bytes(ctx, argv[3], &n, &propio);
    JSValue res;
    if (!metodo || !url || !cabs) { res = JS_NewString(ctx, "sin memoria"); goto fim; }
    {
        rede_pedido_t q;
        memset(&q, 0, sizeof q);
        q.metodo = metodo; q.url = url; q.cabs = cabs;
        q.corpo = corpo; q.ncorpo = n;
        q.iniciador = pagina_url[0] ? pagina_url : NULL;
        q.maximo = 4 * 1024 * 1024;
        if (flags & 1) q.flags |= RP_CREDENCIAIS;
        if (flags & 2) q.flags |= RP_MANUAL;
        if (flags & 4) q.flags |= RP_SEM_CACHE;
        if (flags & 8) q.flags |= RP_SEM_COOKIES;
        if (flags & 16) q.flags |= RP_NO_CORS;
        rede_resposta_t r;
        memset(&r, 0, sizeof r);
        int rc = rede_executar(&q, &r);
        if (rc < 0) {
            res = JS_NewString(ctx, r.cors_bloqueado ? "cors" : r.erro ? r.erro : "red");
        } else {
            char *cj = aloca(8192);
            if (cj) rede_cab_para_js(r.cab ? r.cab : "", q.iniciador && !rede_mesma_origem(r.url, q.iniciador), cj, 8192);
            res = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, res, 0, JS_NewInt32(ctx, r.codigo));
            JS_SetPropertyUint32(ctx, res, 1, JS_NewString(ctx, r.url));
            JS_SetPropertyUint32(ctx, res, 2, JS_NewString(ctx, cj ? cj : ""));
            JS_SetPropertyUint32(ctx, res, 3, JS_NewArrayBufferCopy(ctx, r.corpo ? r.corpo : (const uint8_t *)"", r.corpo ? (size_t)r.tam : 0));
            JS_SetPropertyUint32(ctx, res, 4, JS_NewInt32(ctx, (r.redirigido ? 1 : 0) | (r.de_cache ? 2 : 0) | (r.truncado ? 4 : 0)));
            libera(cj);
        }
        rede_resposta_liberar(&r);
    }
fim:
    libera(metodo); libera(url); libera(cabs);
    if (propio) libera(corpo);
    return res;
}

/* __sagui_cookie(url, [set]) */
static JSValue nat_cookie(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_NewString(ctx, "");
    char *url = arg_texto(ctx, argv[0]);
    if (!url) return JS_NewString(ctx, "");
    if (argc >= 2) {
        char *sc = arg_texto(ctx, argv[1]);
        if (sc) { cookie_guardar(pagina_url[0] ? pagina_url : url, sc, true); libera(sc); }
        libera(url);
        return JS_UNDEFINED;
    }
    char *o = aloca(8192);
    if (o) cookie_texto(pagina_url[0] ? pagina_url : url, o, 8192, true);
    JSValue r = JS_NewString(ctx, o ? o : "");
    libera(o);
    libera(url);
    return r;
}

/* __sagui_alm(area, op, k, v): op 0 getItem, 1 setItem, 2 removeItem, 3 clear, 4 key(i), 5 length */
static JSValue nat_alm(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int area = 0, op = 0;
    if (argc < 2) return JS_NULL;
    JS_ToInt32(ctx, &area, argv[0]);
    JS_ToInt32(ctx, &op, argv[1]);
    const char *url = pagina_url;
    if (!url[0]) return JS_NULL;
    char *k = argc > 2 ? arg_texto(ctx, argv[2]) : NULL;
    JSValue r = JS_NULL;
    switch (op) {
    case 0: { char *v = k ? rede_alm_obter(area, url, k) : NULL; if (v) { r = JS_NewString(ctx, v); libera(v); } break; }
    case 1: {
        char *v = argc > 3 ? arg_texto(ctx, argv[3]) : NULL;
        int rc = (k && v) ? rede_alm_definir(area, url, k, v) : -1;
        r = JS_NewInt32(ctx, rc);
        libera(v);
        break;
    }
    case 2: if (k) rede_alm_remover(area, url, k); break;
    case 3: rede_alm_limpar(area, url); break;
    case 4: { int i = 0; if (argc > 2) JS_ToInt32(ctx, &i, argv[2]); char *v = rede_alm_chave(area, url, i); if (v) { r = JS_NewString(ctx, v); libera(v); } break; }
    case 5: r = JS_NewInt32(ctx, rede_alm_tamanho(area, url)); break;
    }
    libera(k);
    return r;
}

/* ---- WebSocket ---- */

/* __sagui_ws_abrir(url, protocolos) -> id | "error" */
static JSValue nat_ws_abrir(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    char *url = argc > 0 ? arg_texto(ctx, argv[0]) : NULL, *pr = argc > 1 ? arg_texto(ctx, argv[1]) : NULL;
    char erro[160] = "";
    int id = url ? ws_abrir(url, pagina_url[0] ? pagina_url : NULL, pr, erro, sizeof erro) : -1;
    libera(url); libera(pr);
    if (id < 0) return JS_NewString(ctx, erro[0] ? erro : "error");
    return JS_NewInt32(ctx, id);
}

static JSValue nat_ws_estado(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1;
    if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    return JS_NewInt32(ctx, ws_estado(id));
}

/* __sagui_ws_enviar(id, datos, binario) */
static JSValue nat_ws_enviar(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1, bin = 0, n = 0;
    if (argc < 3) return JS_NewInt32(ctx, -1);
    JS_ToInt32(ctx, &id, argv[0]);
    JS_ToInt32(ctx, &bin, argv[2]);
    bool propio = false;
    uint8_t *d = arg_bytes(ctx, argv[1], &n, &propio);
    int rc = ws_enviar(id, d ? d : (const uint8_t *)"", n, bin != 0);
    if (propio) libera(d);
    return JS_NewInt32(ctx, rc);
}

static JSValue nat_ws_fechar(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1, cod = 1000;
    if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &cod, argv[1]);
    char *raz = argc > 2 ? arg_texto(ctx, argv[2]) : NULL;
    int rc = ws_fechar(id, cod, raz ? raz : "");
    libera(raz);
    return JS_NewInt32(ctx, rc);
}

/* __sagui_ws_receber(id) -> [tipo, datos, codigo] | null */
static JSValue nat_ws_receber(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1, tipo = 0, cod = 0;
    if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    uint8_t *d = NULL;
    int n = ws_receber(id, &d, &tipo, &cod);
    if (n < 0) return JS_NULL;
    JSValue r = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, r, 0, JS_NewInt32(ctx, tipo));
    JS_SetPropertyUint32(ctx, r, 1, tipo == 2 ? JS_NewArrayBufferCopy(ctx, d ? d : (const uint8_t *)"", (size_t)n)
                                              : JS_NewStringLen(ctx, d ? (const char *)d : "", (size_t)n));
    JS_SetPropertyUint32(ctx, r, 2, JS_NewInt32(ctx, cod));
    libera(d);
    return r;
}

static JSValue nat_ws_liberar(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1;
    if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    ws_liberar(id);
    return JS_UNDEFINED;
}

static JSValue nat_ws_protocolo(JSContext *ctx, JSValueConst este, int argc, JSValueConst *argv)
{
    int id = -1;
    if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    return JS_NewString(ctx, ws_protocolo(id));
}

#define REG(n, f, a) JS_SetPropertyStr(ctx, global, n, JS_NewCFunction(ctx, f, n, a))

/* Registra los nativos y corre la API de red en JavaScript. Después del prelude del puente. */
void qjs_rede_preparar(JSContext *ctx)
{
    pedidos_pagina = 0;
    ws_liberar_todos();   /* los WebSocket de la página anterior */
    JSValue global = JS_GetGlobalObject(ctx);
    REG("__sagui_ua", nat_ua, 0);
    REG("__sagui_utf8_enc", nat_utf8_enc, 1);
    REG("__sagui_utf8_dec", nat_utf8_dec, 1);
    REG("__sagui_http", nat_http, 5);
    REG("__sagui_cookie", nat_cookie, 2);
    REG("__sagui_alm", nat_alm, 4);
    REG("__sagui_ws_abrir", nat_ws_abrir, 2);
    REG("__sagui_ws_estado", nat_ws_estado, 1);
    REG("__sagui_ws_enviar", nat_ws_enviar, 3);
    REG("__sagui_ws_fechar", nat_ws_fechar, 3);
    REG("__sagui_ws_receber", nat_ws_receber, 1);
    REG("__sagui_ws_liberar", nat_ws_liberar, 1);
    REG("__sagui_ws_protocolo", nat_ws_protocolo, 1);
    JSValue pv = JS_Eval(ctx, qjs_rede_js, sizeof qjs_rede_js - 1, "<rede>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(pv)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        registrar("qjs: erro na rede: %s\n", msg ? msg : "?");
        if (msg) JS_FreeCString(ctx, msg);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, pv);
    JS_FreeValue(ctx, global);
}
