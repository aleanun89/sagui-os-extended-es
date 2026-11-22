# lua: el lenguaje Lua 5.4 dentro de SaguiOS

**lua** (`/apps/bin/lua`) es Lua 5.4.9 de PUC-Rio. **luac** compila a código de bytes. Todo en C estándar, con
enteros de 64 bits y números en punto flotante de doble precisión.

## Usarlo

    lua                         modo interactivo (os.exit() sale)
    lua guion.lua uno dos       ejecuta un guion; los argumentos están en arg y en ...
    lua -e "print(2^10)"        una línea
    lua -i guion.lua            ejecuta el guion y deja el modo interactivo
    luac -o guion.luac guion.lua   compila;   lua guion.luac   lo ejecuta
    luac -l guion.lua           lista el código de bytes

## Lo básico del lenguaje

Lua es sencillo: pocas palabras reservadas y una sola estructura para todo, la **tabla**. Un primer guion,
`hola.lua`, escrito con `mico hola.lua` y ejecutado con `lua hola.lua`:

    -- hola.lua: el primer guion en Lua
    local nombre = "mundo"
    print("Hola, " .. nombre)           -- .. une textos
    for i = 1, 3 do print(i, i * i) end  -- 1 1 / 2 4 / 3 9
    local function doble(x) return 2 * x end
    print(doble(21))                     -- 42

    local frutas = {"pera", "uva"}       -- una lista es una tabla con índices 1, 2...
    frutas[#frutas + 1] = "piña"         -- # cuenta los elementos
    for i, f in ipairs(frutas) do print(i, f) end

    local sagui = {nombre = "Tito", bananas = 3}   -- una tabla con nombres
    print(sagui.nombre, sagui.bananas)

En Lua los índices de las listas empiezan en **1**, no en 0. Las variables son globales salvo que escribas
`local` delante. `if`, `while` y `repeat` terminan con `end` (`until` en el `repeat`). Los comentarios empiezan
por `--`. Las bibliotecas (`string`, `table`, `math`, `io`, `os`...) están más abajo, en «Bibliotecas».

Para ventanas o gráficos no hay biblioteca de Lua en SaguiOS: en ese caso, C o Python (ver [Programar en SaguiOS](lenguajes/indice.md)).

## Bibliotecas

`string` (con patrones y `string.format`), `table`, `math`, `utf8`, `coroutine`, `io` (`io.open`, `io.lines`,
`io.popen`), `os` (`time`, `date`, `clock`, `getenv`, `tmpname`, `remove`, `rename`, `execute`) y `debug`.
`io.popen` y `os.execute` usan `popen` y `system` del sistema (`/bin/sh`). `require` busca `./?.lua`,
`/apps/lua/share/lua/5.4/?.lua` y `/apps/lua/share/lua/5.4/?/init.lua`; los módulos de C (`.so`) no se cargan:
`package.loadlib` no existe en SaguiOS, un programa los enlaza con `luaL_requiref`.

## Ejemplos

En `/apps/lua/ejemplos`: `pruebas.lua` (recorre las bibliotecas), `miutil.lua` (un módulo para `require`) e
`incrustar.c`.

## Lua dentro de un programa en C

Las cabeceras (`lua.h`, `lauxlib.h`, `lualib.h`, `luaconf.h`, `lua.hpp`) están en `/apps/tcc/include` y la biblioteca en
`/apps/tcc/lib/liblua.a`:

    cp /apps/lua/ejemplos/incrustar.c /tmp; cd /tmp
    tcc incrustar.c -llua -o incrustar
    ./incrustar 'return 6 * 7'

Para programas enlazados dinámicamente (D378) hay también `/apps/lib/liblua.so.5.4`; `/apps/lua/lua-din` es el
intérprete enlazado así.

Licencia MIT (`/apps/lua/LICENCIA-LUA.txt`), D440.
