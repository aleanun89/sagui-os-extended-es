# Lua 5.4 en SaguiOS (D440)

- **Origen:** https://www.lua.org/, Lua 5.4.9 (PUC-Rio, publicada el 2026-08-10). Tarball
  `https://www.lua.org/ftp/lua-5.4.9.tar.gz`, SHA-256 `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`.
- **Licencia:** MIT (aviso en `lua.h`, que `tools/lua_fuentes.sh` guarda como `fuentes/LICENCIA-LUA.txt` y se copia a
  `/apps/lua/LICENCIA-LUA.txt`). Compatible con la GPL v3.
- **Qué no va a git:** `fuentes/` (lo baja `tools/lua_fuentes.sh`: los `.c` y `.h` de `src/`, sin Makefile). **Un solo cambio:**
  `LUA_ROOT` pasa de `/usr/local/` a `/apps/lua/` en `luaconf.h` (lo hace un `sed` del propio guion): así `require` busca en
  `/apps/lua/share/lua/5.4/`.
- **Qué va a git (nuestro):** `lua.mk` (cómo se compila: `-DLUA_USE_POSIX`, la x87 y la libc común), `plataforma/lua_sagui.h`
  (`getc_unlocked`, `flockfile` y `funlockfile` que faltan en la libc), `plataforma/principal_main.c`, `ejemplos/` y `leeme.txt`.
- **Instalado en:** `/apps/bin/lua` y `/apps/bin/luac`; `/apps/lua` (ejemplos, léeme, licencia, `lua-din` y la raíz de los
  módulos); `/apps/tcc/lib/liblua.a` y `/apps/tcc/include/{lua.h,lauxlib.h,lualib.h,luaconf.h,lua.hpp}` (para `tcc ... -llua`);
  `/apps/lib/liblua.so.5.4` (enlace dinámico D378).
- **Qué no hay:** carga dinámica de módulos de C (`package.loadlib`; sin `LUA_USE_DLOPEN`) ni `readline` (el modo interactivo
  lee con `fgets` y el terminal edita la línea).
- **Cómo se comprueba:** `tests/h440_awk_lua.py` (su salida con `pruebas.lua` es idéntica, byte a byte, a la del Lua 5.4.9 del PC).
