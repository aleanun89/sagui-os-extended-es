#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H440 - awk (The One True AWK) y Lua 5.4 dentro de SaguiOS (D440, docs/awk-lua.md).

Partes (por defecto todas; o las que se pidan: h440_awk_lua.py pc saguios):
  pc       /bin/awk y /apps/bin/lua, /apps/bin/luac son ELF32 estáticos; /apps/lua lleva ejemplos, léeme y licencia;
           /apps/tcc/include tiene lua.h y /apps/tcc/lib/liblua.a; docs, ayuda y fuentes fuera de git.
  saguios  en SaguiOS: awk con un guion (sumas por columnas, gsub, sub, split, printf %.2f, getline desde un pipe,
           print a un pipe, ENVIRON, system, BEGIN/END) con la salida que da el awk del PC; Lua con pruebas.lua
           (cadenas, tablas, metatablas, corrutinas, string.format, math, io, os, pcall, require de un módulo
           propio, io.popen, os.execute) comparado con el Lua 5.4.9 del PC, luac, el modo interactivo, y un
           programa de C con Lua dentro compilado con tcc.
Capturas: docs/capturas/H440-*.png.
"""
import hashlib
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
FUENTES_AWK = os.path.join(RAIZ, "userland", "terceros", "awk", "fuentes", ".preparado")
FUENTES_LUA = os.path.join(RAIZ, "userland", "terceros", "lua", "fuentes", ".preparado")

AWK_ESPERADO = """== informe ==
manzana    8    10.25
pera       3     6.40
uva       10     7.50
total 24.15 en 5 filas, media 4.830
2 adios mundo, adios awk
adios [mundo], adios awk
4 ad
desde-un-pipe 13 DESDE 7
suma del pipe 6
4 7 1 0 0 1024 1 0.25
valor
regex ok
system devolvio 3
003.1|ff|A|fin
"""


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-600:]))


def orden(m, texto, tempo):
    """Escribe una orden en el Terminal y espera al prompt siguiente; devuelve lo que salió."""
    marca = len(m.serial())
    m.digitar(texto, 0.03)
    m.tecla("ret")
    esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(texto))
    time.sleep(0.8)   # la salida de la orden y el prompt llegan por caminos distintos a la serie
    return m.serial()[marca:].replace("\r", "")


def pc():
    awk = os.path.join(BUILD, "userland", "bin", "awk.elf")
    lua = os.path.join(BUILD, "userland", "bin", "lua.elf")
    luac = os.path.join(BUILD, "userland", "bin", "luac.elf")
    for f, imagen in ((awk, os.path.join(BUILD, "rootfs", "bin", "awk")),
                      (lua, os.path.join(APPS_RAIZ, "bin", "lua")), (luac, os.path.join(APPS_RAIZ, "bin", "luac"))):
        verificar(os.path.isfile(imagen), "la imagen lleva %s" % os.path.relpath(imagen, BUILD))
        h = subprocess.run(["readelf", "-hl", "-W", f], capture_output=True, text=True).stdout
        verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "%s: ELF32 estático de i386" % os.path.basename(f))
    for f in ("lib/liblua.so.5.4",):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, f)), "/apps/%s" % f)
    verificar(os.path.isfile(os.path.join(APPS_RAIZ, "lua", "lua-din")), "/apps/lua/lua-din")
    for f in ("leeme.txt", "LICENCIA-LUA.txt", "ejemplos/pruebas.lua", "ejemplos/miutil.lua", "ejemplos/incrustar.c"):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "lua", f)), "/apps/lua/%s" % f)
    verificar(os.path.isfile(os.path.join(BUILD, "rootfs", "ejemplos", "awk", "informe.awk")), "/ejemplos/awk/informe.awk")
    if os.path.isdir(os.path.join(APPS_RAIZ, "tcc")):
        for f in ("include/lua.h", "include/lauxlib.h", "include/lualib.h", "include/luaconf.h", "lib/liblua.a"):
            verificar(os.path.isfile(os.path.join(APPS_RAIZ, "tcc", f)), "/apps/tcc/%s" % f)
    for f in ("awk.md", "lua.md"):
        verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", f)), "rootfs/ayuda/%s" % f)
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("One True AWK" in t and "**Lua 5.4.9**" in t, "docs/terceros.md tiene las filas de awk y Lua")
    ig = subprocess.run(["git", "ls-files", "userland/terceros/awk", "userland/terceros/lua"], cwd=RAIZ,
                        capture_output=True, text=True).stdout
    verificar("fuentes/" not in ig, "las fuentes de awk y Lua no van en git")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D440." in d, "D440 está en docs/decisoes.md")


def saguios():
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "lua")) or not os.path.isfile(os.path.join(BUILD, "rootfs", "bin", "awk")):
        print("AVISO: la imagen no lleva awk y lua (tools/awk_fuentes.sh, tools/lua_fuentes.sh y make): se salta la parte «saguios»")
        return
    import h136_opentyrian as h136
    lento = 1 if KVM else 4
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos-mb", "64"])
    with Maquina("h440", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        # ---- awk ----
        out = orden(m, "awk --version", 30)
        verificar("awk version 20261106" in out, "awk --version (%r)" % out[-200:])
        out = orden(m, "which awk", 30)
        verificar("/bin/awk" in out, "awk está en el PATH, en /bin (%r)" % out[-100:])
        orden(m, "cd /ejemplos/awk", 30)
        orden(m, "export AWKPRUEBA=valor", 30)
        # la salida va a un fichero: los avisos del kernel se mezclan con la del terminal (se compara su SHA-256)
        out = orden(m, "awk -f informe.awk ventas.txt > /tmp/awk_salida.txt; echo FIN_AWK $?", 60 * lento)
        verificar("FIN_AWK 0" in out, "informe.awk termina sin error (%r)" % out[-200:])
        out = orden(m, "sha256sum /tmp/awk_salida.txt", 30)
        sha_awk = hashlib.sha256(AWK_ESPERADO.encode("utf-8")).hexdigest()
        verificar(sha_awk in out, "informe.awk da lo mismo que el awk del PC (%r)" % out[-200:])
        orden(m, "cat /tmp/awk_salida.txt", 30)
        m.captura("H440-1-awk-informe")
        out = orden(m, "echo 10 20 30 | awk '{ print $1 + $2 + $3, NF }'", 30)
        verificar("60 3" in out, "awk lee la entrada estándar (%r)" % out[-100:])
        out = orden(m, "awk 'BEGIN { printf \"%.2f %5.1f %d\\n\", 2/3, 3.14159, 7.9; x = 0.1 + 0.2; print x, 1e6, 100000 * 100000 }'", 30)
        verificar("0.67   3.1 7" in out and "0.3 1000000 10000000000" in out, "punto flotante y printf (%r)" % out[-200:])
        out = orden(m, "awk 'END { print NR, s } { s += $2 }' ventas.txt", 30)
        verificar("5 21" in out, "END y suma por columna (%r)" % out[-100:])
        out = orden(m, "echo a:b:c | awk -F: '{ print $2 NF }'", 30)
        verificar("\nb3\n" in out, "-F (%r)" % out[-100:])
        out = orden(m, "echo 'uno dos' | awk '{ gsub(/o/, \"0\"); print }'", 30)
        verificar("un0 d0s" in out, "gsub con expresión regular (%r)" % out[-100:])
        out = orden(m, "awk 'BEGIN { while ((\"echo a b c\" | getline l) > 0) n = split(l, p); print n, p[3] }'", 30)
        verificar("3 c" in out, "getline desde un pipe y split (%r)" % out[-100:])
        # ---- lua ----
        out = orden(m, "lua -v", 30)
        verificar("Lua 5.4.9  Copyright (C) 1994-2026 Lua.org, PUC-Rio" in out, "lua -v (%r)" % out[-200:])
        out = orden(m, "lua -e 'print(1 + 1, 10 // 3, 2^0.5)'", 30)
        verificar("2\t3\t1.4142135623731" in out, "lua -e (%r)" % out[-100:])
        orden(m, "cp /apps/lua/ejemplos/* /tmp/", 30)
        orden(m, "cd /tmp", 30)
        orden(m, "export LUAPRUEBA=valor", 30)
        esperado = open(os.path.join(RAIZ, "tests", "h440_lua_esperado.txt"), encoding="utf-8").read()
        out = orden(m, "lua pruebas.lua > lua_salida.txt 2>&1; echo FIN_LUA $?", 180 * lento)
        verificar("FIN_LUA 0" in out, "lua pruebas.lua termina sin error (%r)" % out[-300:])
        # la salida entera de pruebas.lua es idéntica (sha256) a la del Lua 5.4.9 del PC
        out = orden(m, "sha256sum lua_salida.txt", 60)
        sha = hashlib.sha256(open(os.path.join(RAIZ, "tests", "h440_lua_esperado.txt"), "rb").read()).hexdigest()
        if sha not in out:
            obtenido = orden(m, "cat lua_salida.txt > /dev/null; cat lua_salida.txt", 60).split("\n", 1)[1]
            lineas = [l for l in obtenido.split("\n") if not re.match(r"(sagui:|\[SAGUI\]|\x1b)", l)]
            for i, (x, y) in enumerate(zip([l for l in lineas if l != ""], [l for l in esperado.split("\n") if l != ""])):
                if x != y:
                    print("    diferencia en la línea %d: %r != %r" % (i + 1, x, y))
        verificar(sha in out, "la salida de pruebas.lua es idéntica a la del Lua 5.4.9 del PC (%r)" % out[-200:])
        out = orden(m, "tail -1 lua_salida.txt", 30)
        verificar("PRUEBAS_LUA_OK" in out, "pruebas.lua acaba con PRUEBAS_LUA_OK")
        m.captura("H440-2-lua-pruebas")
        # luac y ejecutar el código de bytes
        out = orden(m, "luac -o p.luac pruebas.lua && lua p.luac | tail -1", 120 * lento)
        verificar("PRUEBAS_LUA_OK" in out, "luac compila y lua ejecuta el código de bytes (%r)" % out[-200:])
        out = orden(m, "luac -l -p miutil.lua | head -3", 30)
        verificar("main <miutil.lua" in out, "luac -l (%r)" % out[-200:])
        # el mismo intérprete enlazado dinámicamente con liblua.so.5.4 (D378): /apps/lua/lua-din
        if os.path.isfile(os.path.join(APPS_RAIZ, "lua", "lua-din")):
            out = orden(m, "/apps/lua/lua-din -e 'print(_VERSION, 6 * 7, string.format(\"%.3f\", math.pi))'", 60)
            verificar("Lua 5.4\t42\t3.142" in out, "lua-din (dinámico, con liblua.so.5.4) (%r)" % out[-200:])
            orden(m, "/apps/lua/lua-din pruebas.lua > din_salida.txt 2>&1", 180 * lento)
            out = orden(m, "sha256sum din_salida.txt", 60)
            verificar(sha in out, "pruebas.lua con liblua.so da la misma salida (%r)" % out[-200:])
        # modo interactivo por la entrada estándar y con el teclado
        out = orden(m, "printf 'x = 5\\nprint(x * 8)\\n' | lua -i", 60)
        verificar("40" in out, "lua -i lee de la entrada estándar (%r)" % out[-200:])
        marca = len(m.serial())
        m.digitar("lua", 0.03)
        m.tecla("ret")
        esperar(m, r"> $", 60, marca)
        m.digitar("print(40 + 2)", 0.03)
        m.tecla("ret")
        esperar(m, r"42", 30, marca)
        m.digitar("os.exit()", 0.03)
        m.tecla("ret")
        esperar(m, r"sagui:[^\n]*\$", 30, marca + 20)
        m.captura("H440-3-lua-interactivo")
        # ---- un programa en C con Lua dentro, con tcc ----
        if os.path.isfile(os.path.join(APPS_RAIZ, "bin", "tcc")):
            out = orden(m, "tcc incrustar.c -llua -o incrustar && ./incrustar", 240 * lento)
            verificar("resultado: 6.5 Lua 5.4" in out, "tcc compila un programa con Lua dentro (%r)" % out[-900:])
            out = orden(m, "./incrustar 'return #string.rep(\"ab\", 21) * 2'", 60)
            verificar("resultado: 84" in out, "el programa ejecuta código de Lua (%r)" % out[-200:])
            out = orden(m, "./incrustar 'error(\"fallo\")'", 60)
            verificar("error de Lua:" in out and "fallo" in out, "los errores de Lua llegan a C (%r)" % out[-200:])
            m.captura("H440-4-lua-con-tcc")
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def main():
    partes = sys.argv[1:] or ["pc", "saguios"]
    if not (os.path.exists(FUENTES_AWK) and os.path.exists(FUENTES_LUA)):
        print("AVISO: faltan las fuentes de awk o Lua (tools/awk_fuentes.sh, tools/lua_fuentes.sh): h440 se salta")
        return 0
    for p in partes:
        {"pc": pc, "saguios": saguios}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
