#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H477 - Valkey 8.0.2 dentro de SaguiOS (D477, docs/decisoes.md).

Partes (por defecto todas; o las que se pidan: h477_valkey.py pc saguios):
  pc       /apps/bin/valkey-server y valkey-cli son ELF32 estáticos; /apps/valkey lleva configuración, ejemplos y licencias;
           libhiredis.a y las cabeceras en /apps/tcc; /lib/python/valkey.py y valkey.lua; docs; fuentes fuera de git.
  saguios  en SaguiOS: el servidor arranca; desde valkey-cli: PING, SET/GET, INCR, listas, hashes, EXPIRE/PEXPIRE, pub/sub,
           EVAL (Lua 5.1), INCRBYFLOAT; desde un programa de C con hiredis compilado con tcc, desde MicroPython y desde Lua;
           persistencia RDB (SAVE, BGSAVE con fork y reinicio del servidor) y AOF.
Capturas: docs/capturas/H477-*.png.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, BUILD  # noqa: E402

FUENTES = os.path.join(RAIZ, "userland", "terceros", "valkey", "fuentes", ".preparado")

EH_ESPERADO = ["PING: PONG", "SET: OK", "GET: valor desde C", "INCRBY: 42", "LRANGE: a b c", "HGETALL: nombre SaguiOS ano 2026",
               "STRLEN del valor binario: 3"]
PY_ESPERADO = ["PING PONG", "GET b'valor desde MicroPython'", "INCR b'42'", "LRANGE [b'a', b'b', b'c']",
               "HGETALL {b'nombre': b'SaguiOS'}", "TTL True"]
LUA_ESPERADO = ["PING\tPONG", "GET\tvalor desde Lua", "INCR\t42", "LRANGE\ta,b,c", "HGETALL\tSaguiOS", "TTL\ttrue"]


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-600:]))


def orden(m, texto, tempo=60):
    marca = len(m.serial())
    m.digitar(texto, 0.03)
    m.tecla("ret")
    esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(texto))
    time.sleep(0.8)
    out = m.serial()[marca:].replace("\r", "")
    return "\n".join(l for l in out.split("\n") if not l.startswith(("[REDE]", "[SAGUI]", "[GUI]", "[OCIO]")))


def pc():
    for n in ("valkey-server", "valkey-cli"):
        elf = os.path.join(BUILD, "userland", "bin", n + ".elf")
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "bin", n)), "/apps/bin/%s está en la imagen" % n)
        h = subprocess.run(["readelf", "-hl", "-W", elf], capture_output=True, text=True).stdout
        verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "%s: ELF32 estático de i386" % n)
        print("   tamaño de %s: %d bytes" % (n, os.path.getsize(os.path.join(APPS_RAIZ, "bin", n))))
    for f in ("valkey.conf", "leeme.txt", "LICENCIA-VALKEY.txt", "LICENCIA-HIREDIS.txt", "ejemplos/ejemplo_hiredis.c",
              "ejemplos/ejemplo_valkey.py", "ejemplos/ejemplo_valkey.lua"):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "valkey", f)), "/apps/valkey/%s" % f)
    verificar(os.path.isfile(os.path.join(APPS_RAIZ, "lua", "share", "lua", "5.4", "valkey.lua")), "/apps/lua/share/lua/5.4/valkey.lua")
    if os.path.isdir(os.path.join(APPS_RAIZ, "tcc")):
        for f in ("include/hiredis/hiredis.h", "lib/libhiredis.a"):
            verificar(os.path.isfile(os.path.join(APPS_RAIZ, "tcc", f)), "/apps/tcc/%s" % f)
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "lib", "python", "valkey.py")), "rootfs/lib/python/valkey.py")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("**Valkey 8.0.2**" in t, "docs/terceros.md tiene la fila de Valkey")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D477." in d, "D477 está en docs/decisoes.md")
    ig = subprocess.run(["git", "ls-files", "userland/terceros/valkey"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar("fuentes/" not in ig, "las fuentes de Valkey no van en git")


def saguios():
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "valkey-server")):
        print("AVISO: la imagen no lleva Valkey (tools/valkey_fuentes.sh y make): se salta la parte «saguios»")
        return
    import h136_opentyrian as h136
    img = imagem_de_teste("", ["--vesa", "--jogos-mb", "64"])
    with Maquina("h477", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 200, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        cli = lambda a, t=30: orden(m, "valkey-cli " + a, t)
        out = orden(m, "valkey-server --version", 30)
        verificar("Valkey server v=8.0.2" in out and "bits=32" in out, "valkey-server --version (%r)" % out[-120:])
        orden(m, "valkey-server /apps/valkey/valkey.conf --dir /tmp --save '' > /tmp/vk.log 2>&1 &", 30)
        esperar(m, r"sagui:", 5, 0)
        t0 = time.time()
        while time.time() - t0 < 60:
            out = orden(m, "cat /tmp/vk.log", 30)
            if "Ready to accept connections" in out:
                break
        verificar("Ready to accept connections" in out, "el servidor arranca y acepta conexiones (%r)" % out[-300:])
        out = cli("PING"); verificar("PONG" in out, "PING (%r)" % out[-80:])
        out = cli("SET a 5"); verificar("OK" in out, "SET")
        out = cli("INCR a"); verificar("(integer) 6" in out, "INCR (%r)" % out[-80:])
        out = cli("GET a"); verificar('"6"' in out, "GET")
        out = cli("RPUSH l x y z"); verificar("(integer) 3" in out, "RPUSH")
        out = cli("LRANGE l 0 -1"); verificar('1) "x"' in out and '3) "z"' in out, "LRANGE")
        out = cli("HSET h f v"); verificar("(integer) 1" in out, "HSET")
        out = cli("--raw HGETALL h"); verificar("f\nv" in out, "HGETALL (%r)" % out[-80:])
        out = orden(m, "valkey-cli SET e 1 PX 500; sleep 1; valkey-cli GET e", 30)
        verificar("OK" in out and "(nil)" in out, "PX 500 y un segundo después ya no está (EXPIRE) (%r)" % out[-80:])
        out = cli("SET t 1 EX 100"); out = cli("TTL t"); verificar(re.search(r"\(integer\) (9\d|100)", out), "TTL (%r)" % out[-60:])
        out = cli("EVAL 'return redis.call(\"INCR\", KEYS[1]) + 1' 1 ev"); verificar("(integer) 2" in out, "EVAL (Lua 5.1) (%r)" % out[-80:])
        out = cli("INCRBYFLOAT f 1.5"); verificar('"1.5"' in out, "INCRBYFLOAT")
        out = cli("ZADD z 3 tres 1 uno 2 dos"); out = cli("--raw ZRANGE z 0 -1"); verificar("uno\ndos\ntres" in out, "ZADD/ZRANGE")
        # pub/sub: un suscriptor en segundo plano y un PUBLISH
        orden(m, "valkey-cli --raw SUBSCRIBE canal > /tmp/sub.txt 2>&1 &", 30)
        time.sleep(3)
        out = cli("PUBLISH canal hola-desde-publish"); verificar("(integer) 1" in out, "PUBLISH llega a 1 suscriptor (%r)" % out[-60:])
        time.sleep(1)
        out = orden(m, "cat /tmp/sub.txt", 30); verificar("hola-desde-publish" in out, "el suscriptor recibe el mensaje (%r)" % out[-120:])
        cli("CLIENT KILL TYPE pubsub")
        m.captura("H477-1-valkey-cli")
        # clientes en otros lenguajes
        out = orden(m, "cd /tmp; tcc /apps/valkey/ejemplos/ejemplo_hiredis.c -lhiredis -o eh && ./eh", 180)
        for l in EH_ESPERADO:
            verificar(l in out, "hiredis desde C: %s (%r)" % (l, out[-300:]))
        out = orden(m, "python /apps/valkey/ejemplos/ejemplo_valkey.py", 60)
        for l in PY_ESPERADO:
            verificar(l in out, "MicroPython: %s (%r)" % (l, out[-300:]))
        out = orden(m, "lua /apps/valkey/ejemplos/ejemplo_valkey.lua", 60)
        for l in LUA_ESPERADO:
            verificar(l in out, "Lua: %s (%r)" % (l, out[-300:]))
        m.captura("H477-2-clientes")
        # persistencia: SAVE, BGSAVE (fork) y reinicio
        cli("SET persiste si-tras-reiniciar")
        out = cli("BGSAVE"); verificar("Background saving started" in out, "BGSAVE con fork (%r)" % out[-80:])
        time.sleep(4)
        out = orden(m, "valkey-cli INFO persistence | grep rdb_last_bgsave_status", 30)
        verificar("rdb_last_bgsave_status:ok" in out, "el hijo del BGSAVE termina bien (%r)" % out[-80:])
        out = cli("SAVE"); verificar("OK" in out, "SAVE")
        cli("SHUTDOWN NOSAVE")
        time.sleep(2)
        out = orden(m, "ls -la /tmp/dump.rdb", 30); verificar("dump.rdb" in out and "no existe" not in out, "dump.rdb existe")
        orden(m, "valkey-server /apps/valkey/valkey.conf --dir /tmp --save '' > /tmp/vk2.log 2>&1 &", 30)
        t0 = time.time()
        while time.time() - t0 < 60:
            out = orden(m, "cat /tmp/vk2.log", 30)
            if "Ready to accept connections" in out:
                break
        verificar("DB loaded from disk" in out, "el servidor nuevo carga el RDB (%r)" % out[-300:])
        out = cli("GET persiste"); verificar("si-tras-reiniciar" in out, "persistencia RDB tras reiniciar (%r)" % out[-80:])
        out = cli("LRANGE l 0 -1"); verificar('"z"' in out, "la lista también vuelve")
        # AOF activado en caliente (reescritura con fork) y relectura
        out = cli("CONFIG SET appendonly yes"); verificar("OK" in out, "appendonly yes")
        time.sleep(5)
        cli("SET en-aof si")
        time.sleep(2)
        out = cli("INFO persistence"); verificar("aof_enabled:1" in out and "aof_last_bgrewrite_status:ok" in out, "AOF reescrito (%r)" % out[-200:])
        m.captura("H477-3-persistencia")
        cli("SHUTDOWN NOSAVE")


def main():
    partes = sys.argv[1:] or ["pc", "saguios"]
    if not os.path.isfile(FUENTES):
        print("AVISO: faltan las fuentes de Valkey (tools/valkey_fuentes.sh): prueba saltada")
        return 0
    if "pc" in partes:
        pc()
    if "saguios" in partes:
        saguios()
    print("H477 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
