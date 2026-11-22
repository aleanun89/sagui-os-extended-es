# awk y Lua 5.4 dentro de SaguiOS (D440)

Dos herramientas pequeñas y muy usadas en guiones, portadas sin cambiar su código (v8.5.5.2).

| | awk | Lua |
|---|---|---|
| Qué es | *The One True AWK* (Kernighan), etiqueta `20261106` | Lua 5.4.9 (PUC-Rio) |
| Licencia | permisiva de Lucent (tipo HPND) | MIT |
| Fuentes | `tools/awk_fuentes.sh` (SHA-256 fijo; no van a git) | `tools/lua_fuentes.sh` (SHA-256 fijo; no van a git) |
| Instalado en | `/bin/awk` (140 KB, en el ramdisk), `/apps/awk`, `/ejemplos/awk` | `/apps/bin/lua` (210 KB) y `luac` (135 KB), `/apps/lua`, `/apps/lib/liblua.so.5.4`, `/apps/tcc/lib/liblua.a` y `/apps/tcc/include/lua*.h` |
| Se compila con | `userland/terceros/awk/awk.mk` | `userland/terceros/lua/lua.mk` |
| Opción | `make AWK_DENTRO=0` lo quita | `make LUA_DENTRO=0` lo quita |
| Prueba | `tests/h440_awk_lua.py` | la misma |

Sin las fuentes (no se han bajado) el `make` solo avisa y las salta, como con GNU make y TCC. Las dos entran también en
`make SEM_GPL=1`. Ayuda: `ayuda awk`, `ayuda lua` y `/ayuda/awk.md`, `/ayuda/lua.md`.

## awk en `/bin`

Es la herramienta que usan los guiones (`awk '{ print $1 }'`), por eso va en el ramdisk con `sed` y `grep`, y no en la
partición de las aplicaciones: 140 KB. Necesita yacc para `awkgram.y`; el anfitrión no trae bison, así que
`userland/terceros/awk/gerado/` guarda `awkgram.tab.c` y `.h` ya generados (bison 3.8.2) y `tools/awk_fuentes.sh` vigila
el SHA-256 de `awkgram.y`: si cambia en una versión nueva hay que regenerarlos con
`bison -d -b awkgram awkgram.y`. `maketab` (del upstream) saca `proctab.c` en el PC al compilar.

La libc de SaguiOS no tiene caracteres anchos y su `MB_CUR_MAX` es 1: awk trabaja por bytes (Latin-1), como el terminal.
`length("año")` da 3. Los pipes (`"orden" | getline`, `print | "orden"`) y `system()` usan `popen` y `system` (D401) con `/bin/sh`.

## Lua

`lua.mk` compila los 33 ficheros de la biblioteca con la x87 (`-mfpmath=387`: todo son dobles) y la libc común, con
`-DLUA_USE_POSIX` (`io.popen`, `os.execute`, `os.tmpname` con `mkstemp`, `isatty`). `plataforma/lua_sagui.h` aporta
`getc_unlocked`, `flockfile` y `funlockfile`. El `sed` de `lua_fuentes.sh` cambia `LUA_ROOT` a `/apps/lua/`: `require`
busca `./?.lua`, `/apps/lua/share/lua/5.4/?.lua` y `/apps/lua/lib/lua/5.4/?.lua`.

- **`liblua.a`** va a `/apps/tcc/lib` con las cabeceras en `/apps/tcc/include`: `tcc incrustar.c -llua -o incrustar`
  (`/apps/lua/ejemplos/incrustar.c` registra una función de C y ejecuta código de Lua).
- **`liblua.so.5.4`** (D378): la misma biblioteca como PIC, enlazada con `libc.so.1` y `libsagui.so.1`, en `/apps/lib`.
  `/apps/lua/lua-din` es el intérprete enlazado dinámicamente con ella (lo ejecuta h440 y da la misma salida).
- **No hay:** `package.loadlib` (sin `dlopen` en un programa estático) ni `readline`: el modo interactivo lee con `fgets` y
  el terminal edita la línea.

## Pruebas

`python3 tests/h440_awk_lua.py` (partes `pc` y `saguios`): ELF32 estáticos, ficheros instalados y documentación; y en
QEMU, `informe.awk` (sumas por columnas, `gsub`, `sub`, `split`, `printf %.2f`, `getline` desde un pipe, `print` a un
pipe, `ENVIRON`, `system`) con la salida del awk del PC, `pruebas.lua` (cadenas, tablas, metatablas, corrutinas,
`string.format`, `math`, `io`, `os`, `pcall`, `require` de un módulo propio, `io.popen`, `os.execute`) con una salida
idéntica (SHA-256) a la del Lua 5.4.9 del PC, `luac`, el modo interactivo (por la entrada estándar y por el teclado),
`lua-din` y un programa de C con Lua dentro compilado con `tcc`.
