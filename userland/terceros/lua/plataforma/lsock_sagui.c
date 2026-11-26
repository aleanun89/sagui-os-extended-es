/* SPDX-License-Identifier: GPL-3.0-only */
/* lsock_sagui.c - módulo «socket» mínimo para el Lua de SaguiOS (D477): un cliente TCP, lo justo para hablar con un
 * servidor (Valkey, HTTP, ...). La interfaz copia lo básico de LuaSocket:
 *
 *   local socket = require("socket")
 *   local c, err = socket.connect("127.0.0.1", 6379)
 *   c:settimeout(5)               -- segundos (nil = sin plazo)
 *   c:send("PING\r\n")            -- devuelve los bytes enviados, o nil, error
 *   c:receive("l")                -- una línea sin el \r\n;   c:receive(n)  n bytes exactos;   c:receive("a")  hasta el cierre
 *   c:close()
 *
 * Se registra en package.preload al abrir las bibliotecas (ver luaopen_utf8_sagui más abajo y lua.mk), así que require("socket") funciona. */
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define TIPO "sagui.socket"

typedef struct {
    int fd;
    int plazo_ms;          /* -1: sin plazo */
    char *buf;             /* lo recibido y aún no entregado */
    size_t n, cap;
} sock_t;

static sock_t *aSock(lua_State *L, int i)
{
    sock_t *s = (sock_t *)luaL_checkudata(L, i, TIPO);
    return s;
}

static int err(lua_State *L, const char *m)
{
    lua_pushnil(L);
    lua_pushstring(L, m);
    return 2;
}

static int l_connect(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    int puerto = (int)luaL_checkinteger(L, 2);
    struct addrinfo pistas, *res = NULL;
    char sp[16];
    memset(&pistas, 0, sizeof pistas);
    pistas.ai_family = AF_INET;
    pistas.ai_socktype = SOCK_STREAM;
    snprintf(sp, sizeof sp, "%d", puerto);
    if (getaddrinfo(host, sp, &pistas, &res) != 0 || !res) return err(L, "no se resuelve el nombre");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { freeaddrinfo(res); return err(L, strerror(errno)); }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        int e = errno;
        close(fd);
        freeaddrinfo(res);
        return err(L, e == ECONNREFUSED ? "connection refused" : strerror(e));
    }
    freeaddrinfo(res);
    sock_t *s = (sock_t *)lua_newuserdata(L, sizeof(sock_t));
    memset(s, 0, sizeof *s);
    s->fd = fd;
    s->plazo_ms = -1;
    luaL_setmetatable(L, TIPO);
    return 1;
}

static int l_settimeout(lua_State *L)
{
    sock_t *s = aSock(L, 1);
    if (lua_isnoneornil(L, 2)) s->plazo_ms = -1;
    else s->plazo_ms = (int)(luaL_checknumber(L, 2) * 1000);
    lua_pushinteger(L, 1);
    return 1;
}

static int l_send(lua_State *L)
{
    sock_t *s = aSock(L, 1);
    size_t n;
    const char *d = luaL_checklstring(L, 2, &n);
    size_t hecho = 0;
    if (s->fd < 0) return err(L, "closed");
    while (hecho < n) {
        ssize_t r = send(s->fd, d + hecho, n - hecho, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return err(L, strerror(errno));
        }
        hecho += (size_t)r;
    }
    lua_pushinteger(L, (lua_Integer)hecho);
    return 1;
}

/* Lee más a s->buf; 1 = bien, 0 = fin de la conexión, -1 = error, -2 = plazo agotado */
static int mas(sock_t *s)
{
    if (s->plazo_ms >= 0) {
        struct pollfd p = { s->fd, POLLIN, 0 };
        int r = poll(&p, 1, s->plazo_ms);
        if (r == 0) return -2;
        if (r < 0) return -1;
    }
    if (s->n + 4096 > s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 8192;
        char *nb = (char *)realloc(s->buf, nc);
        if (!nb) return -1;
        s->buf = nb;
        s->cap = nc;
    }
    ssize_t r = recv(s->fd, s->buf + s->n, s->cap - s->n, 0);
    if (r < 0) return -1;
    if (r == 0) return 0;
    s->n += (size_t)r;
    return 1;
}

static void quitar(sock_t *s, size_t k)
{
    memmove(s->buf, s->buf + k, s->n - k);
    s->n -= k;
}

static int l_receive(lua_State *L)
{
    sock_t *s = aSock(L, 1);
    if (s->fd < 0) return err(L, "closed");
    if (lua_type(L, 2) == LUA_TNUMBER) {
        size_t k = (size_t)lua_tointeger(L, 2);
        while (s->n < k) {
            int r = mas(s);
            if (r == -2) return err(L, "timeout");
            if (r <= 0) return err(L, "closed");
        }
        lua_pushlstring(L, s->buf, k);
        quitar(s, k);
        return 1;
    }
    const char *f = luaL_optstring(L, 2, "l");
    if (*f == '*') f++;
    if (*f == 'a') {
        int r;
        while ((r = mas(s)) > 0) ;
        if (r == -2) return err(L, "timeout");
        lua_pushlstring(L, s->buf, s->n);
        s->n = 0;
        return 1;
    }
    /* línea */
    size_t desde = 0;
    for (;;) {
        for (size_t i = desde; i < s->n; i++) {
            if (s->buf[i] == '\n') {
                size_t fin = i;
                if (fin > 0 && s->buf[fin - 1] == '\r') fin--;
                lua_pushlstring(L, s->buf, fin);
                quitar(s, i + 1);
                return 1;
            }
        }
        desde = s->n;
        int r = mas(s);
        if (r == -2) return err(L, "timeout");
        if (r <= 0) return err(L, "closed");
    }
}

static int l_close(lua_State *L)
{
    sock_t *s = aSock(L, 1);
    if (s->fd >= 0) { close(s->fd); s->fd = -1; }
    free(s->buf);
    s->buf = NULL;
    s->n = s->cap = 0;
    return 0;
}

static const luaL_Reg metodos[] = {
    { "send", l_send }, { "receive", l_receive }, { "settimeout", l_settimeout }, { "close", l_close },
    { "__gc", l_close }, { NULL, NULL }
};
static const luaL_Reg funcs[] = { { "connect", l_connect }, { NULL, NULL } };

static int abrir(lua_State *L)
{
    luaL_newmetatable(L, TIPO);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, metodos, 0);
    lua_pop(L, 1);
    luaL_newlib(L, funcs);
    return 1;
}

/* Lo llama linit.c (con -Dluaopen_utf8=luaopen_utf8_sagui): abre utf8 como siempre y deja «socket» en package.preload. */
int luaopen_utf8(lua_State *L);
int luaopen_utf8_sagui(lua_State *L)
{
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, abrir);
    lua_setfield(L, -2, "socket");
    lua_pop(L, 1);
    return luaopen_utf8(L);
}
