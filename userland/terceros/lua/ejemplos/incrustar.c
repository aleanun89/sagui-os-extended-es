/* incrustar.c - un programa en C con Lua dentro (D440).
 * Compilar en SaguiOS:  tcc incrustar.c -llua -o incrustar   y   ./incrustar 'return 6 * 7'
 * (las cabeceras <lua.h>, <lauxlib.h> y <lualib.h> y liblua.a están en /apps/tcc). */
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* una función de C visible desde Lua: suma(a, b, ...) devuelve la suma de sus argumentos */
static int suma(lua_State *L)
{
    int n = lua_gettop(L);
    lua_Number total = 0;
    for (int i = 1; i <= n; i++) total += luaL_checknumber(L, i);
    lua_pushnumber(L, total);
    return 1;
}

int main(int argc, char **argv)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "suma", suma);
    const char *codigo = argc > 1 ? argv[1] : "return suma(1, 2, 3.5) .. ' ' .. _VERSION";
    if (luaL_dostring(L, codigo) != LUA_OK) {
        printf("error de Lua: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    printf("resultado: %s\n", lua_tostring(L, -1));
    lua_close(L);
    return 0;
}
