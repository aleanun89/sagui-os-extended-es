#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H478 - asyncio, Microdot y bottle.py en SaguiOS (D478, docs/decisoes.md).

Partes (por defecto todas; o las que se pidan: h478_microdot_bottle.py pc saguios):
  pc       /lib/python lleva asyncio, microdot y bottle.py; /ejemplos/web; documentación.
  saguios  en SaguiOS (red de usuario de QEMU con redirección de puertos): asyncio en marcha (sleep, gather); una aplicación de
           bottle.py servida a 0.0.0.0:8080 y consultada desde el anfitrión: página con plantilla, ruta con parámetro, JSON, formulario
           POST, POST con JSON, fichero estático, 404, abort(403), redirección; wget del propio SaguiOS; y Microdot a pelo en el 5000.
Capturas: docs/capturas/H478-*.png.
"""
import json
import os
import re
import socket
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar  # noqa: E402


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


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class SinRedirigir(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


def pedir(puerto, ruta, datos=None, cab=None, seguir=True, tempo=40):
    req = urllib.request.Request("http://127.0.0.1:%d%s" % (puerto, ruta), data=datos, headers=cab or {})
    op = urllib.request.build_opener() if seguir else urllib.request.build_opener(SinRedirigir)
    try:
        with op.open(req, timeout=tempo) as r:
            return r.status, r.read(), dict(r.headers)
    except urllib.error.HTTPError as e:
        return e.code, e.read(), dict(e.headers)


def esperar_http(puerto, ruta, tempo=90):
    fim = time.time() + tempo
    while time.time() < fim:
        try:
            return pedir(puerto, ruta, tempo=10)
        except Exception:
            time.sleep(1)
    raise AssertionError("el servidor no responde en el puerto %d" % puerto)


def pc():
    for f in ("asyncio/__init__.py", "asyncio/core.py", "microdot/microdot.py", "microdot/__init__.py", "bottle.py"):
        verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "lib", "python", f)), "rootfs/lib/python/%s" % f)
    for f in ("web/bottle_hola.py", "web/microdot_hola.py", "web/views/pagina.tpl", "web/estaticos/hola.txt"):
        verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ejemplos", f)), "rootfs/ejemplos/%s" % f)
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("**Microdot 2.8.0**" in t, "docs/terceros.md tiene la fila de Microdot")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D478." in d, "D478 está en docs/decisoes.md")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "microdot-bottle.md")), "rootfs/ayuda/microdot-bottle.md")


def saguios():
    import h136_opentyrian as h136
    p8080, p5000 = puerto_libre(), puerto_libre()
    img = imagem_de_teste("", ["--vesa", "--jogos-mb", "64"])
    extras = ["-nic", "user,model=e1000,hostfwd=tcp:127.0.0.1:%d-:8080,hostfwd=tcp:127.0.0.1:%d-:5000" % (p8080, p5000)]
    with Maquina("h478", "ide", imagem=img, memoria="512M", extras=extras) as m:
        esperar(m, r"sagui:/\$", 200, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        out = orden(m, "python -c \"import asyncio; print(asyncio.__name__)\"", 30)
        verificar("asyncio" in out, "asyncio se importa (%r)" % out[-100:])
        out = orden(m, "python /ejemplos/web/prueba_asyncio.py", 60)
        verificar("gather [2, 4, 6]" in out and "tarea 10" in out, "asyncio: gather y create_task (%r)" % out[-200:])
        orden(m, "python /ejemplos/web/bottle_hola.py > /tmp/web.log 2>&1 &", 30)
        s, cuerpo, cab = esperar_http(p8080, "/")
        t = cuerpo.decode()
        verificar(s == 200 and "<h1>Hola desde SaguiOS</h1>" in t and "<li>Microdot</li>" in t and "<li>MicroPython</li>" in t
                  and "&amp; <b>sin escapar</b>" in t and "text/html" in cab.get("Content-Type", ""),
                  "página con plantilla (for, if, {{}} y {{!}}) (%r)" % t[:300])
        s, cuerpo, _ = pedir(p8080, "/hola/mundo")
        verificar(s == 200 and cuerpo == b"Hola, mundo", "ruta con parámetro (%r)" % cuerpo)
        s, cuerpo, cab = pedir(p8080, "/api/suma?a=40&b=2")
        verificar(s == 200 and json.loads(cuerpo) == {"a": 40, "b": 2, "suma": 42} and "application/json" in cab.get("Content-Type", ""),
                  "JSON desde un dict (%r)" % cuerpo)
        s, cuerpo, _ = pedir(p8080, "/formulario")
        verificar(s == 200 and b"<form" in cuerpo, "formulario GET")
        s, cuerpo, _ = pedir(p8080, "/formulario", datos=urllib.parse.urlencode({"texto": "a&b <hola>"}).encode(),
                             cab={"Content-Type": "application/x-www-form-urlencoded"})
        verificar(s == 200 and cuerpo == b"Recibido: a&amp;b &lt;hola&gt;", "formulario POST, escapado en la plantilla (%r)" % cuerpo)
        s, cuerpo, _ = pedir(p8080, "/api/eco", datos=json.dumps({"x": [1, 2, {"y": "z"}]}).encode(), cab={"Content-Type": "application/json"})
        verificar(s == 201 and json.loads(cuerpo) == {"eco": {"x": [1, 2, {"y": "z"}]}}, "POST con JSON y response.status = 201 (%s %r)" % (s, cuerpo))
        s, cuerpo, cab = pedir(p8080, "/estaticos/hola.txt")
        verificar(s == 200 and cuerpo == b"Fichero estatico de SaguiOS\n" and "text/plain" in cab.get("Content-Type", ""), "static_file (%r)" % cuerpo)
        s, cuerpo, cab = pedir(p8080, "/estaticos/pagina.html")
        verificar(s == 200 and "text/html" in cab.get("Content-Type", ""), "static_file: tipo por la extensión")
        s, _, _ = pedir(p8080, "/estaticos/no-existe.txt")
        verificar(s == 404, "static_file de algo que no existe: 404 (%s)" % s)
        s, _, _ = pedir(p8080, "/estaticos/../bottle_hola.py")
        verificar(s in (403, 404), "static_file no sale de su carpeta (%s)" % s)
        s, cuerpo, _ = pedir(p8080, "/ruta/que/no/hay")
        verificar(s == 404 and b"404" in cuerpo, "404 de una ruta que no existe (%s %r)" % (s, cuerpo[:60]))
        s, cuerpo, _ = pedir(p8080, "/prohibido")
        verificar(s == 403 and "Aquí no".encode() in cuerpo, "abort(403) (%s %r)" % (s, cuerpo))
        s, _, cab = pedir(p8080, "/viejo", seguir=False)
        verificar(s == 302 and cab.get("Location") == "/", "redirect (%s %s)" % (s, cab.get("Location")))
        s, _, _ = pedir(p8080, "/hola/x", datos=b"a=1")
        verificar(s == 405, "POST a una ruta solo GET: 405 (%s)" % s)
        # desde el propio SaguiOS: wget
        out = orden(m, "wget -q -O /tmp/w1.txt http://127.0.0.1:8080/hola/desde-wget; cat /tmp/w1.txt", 60)
        verificar("Hola, desde-wget" in out, "wget del propio SaguiOS (%r)" % out[-120:])
        out = orden(m, "wget -q -O /tmp/w2.txt 'http://127.0.0.1:8080/api/suma?a=1&b=2'; cat /tmp/w2.txt", 60)
        verificar('"suma": 3' in out, "wget: JSON (%r)" % out[-120:])
        m.captura("H478-1-bottle")
        # Microdot a pelo
        orden(m, "python /ejemplos/web/microdot_hola.py > /tmp/md.log 2>&1 &", 30)
        s, cuerpo, _ = esperar_http(p5000, "/json")
        verificar(s == 200 and json.loads(cuerpo) == {"sistema": "SaguiOS", "lenguaje": "MicroPython"}, "Microdot: JSON (%r)" % cuerpo)
        s, cuerpo, _ = pedir(p5000, "/")
        verificar(s == 200 and b"Hola desde Microdot" in cuerpo, "Microdot: página")
        s, cuerpo, _ = pedir(p5000, "/suma", datos=b'{"a": 20, "b": 22}', cab={"Content-Type": "application/json"})
        verificar(s == 200 and json.loads(cuerpo) == {"suma": 42}, "Microdot: POST JSON (%r)" % cuerpo)
        # varias peticiones seguidas (el servidor sigue vivo)
        for i in range(10):
            s, cuerpo, _ = pedir(p8080, "/hola/n%d" % i)
            verificar(cuerpo == ("Hola, n%d" % i).encode(), "petición %d" % i)
        m.captura("H478-2-microdot")


def main():
    partes = sys.argv[1:] or ["pc", "saguios"]
    if "pc" in partes:
        pc()
    if "saguios" in partes:
        saguios()
    print("H478 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
