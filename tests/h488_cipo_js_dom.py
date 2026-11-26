#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H488 - Cipó: JavaScript y DOM (D488).

Dos partes:

1. En el PC (rápida): el puente del QuickJS con el DOM en JavaScript (qjs_dom.js)
   y un motor de juguete (tests/web/qjs_anfitriao/prova.c). Cada caso de
   tests/web/qjs_anfitriao/casos/ dice con líneas «//= ...» lo que tiene que
   escribir, en orden: árbol mutable (appendChild, insertBefore, innerHTML,
   fragmentos, clones, outerHTML, classList, style), eventos (captura y burbuja,
   once, stopImmediatePropagation, ratón, teclado, foco, input, change, submit,
   personalizados), bucle de eventos (microtareas, temporizadores, setInterval,
   requestAnimationFrame, MutationObserver) e historial (pushState, replaceState,
   popstate, hashchange, location). Al final las dos copias del árbol (la del
   JavaScript y la del motor) tienen que ser la misma.

2. En QEMU: una página con un contador (click), una lista que crece con
   insertBefore/appendChild, medidas (getBoundingClientRect, offset*, client*,
   getComputedStyle, innerWidth, scrollHeight), una SPA con pushState y popstate
   (Retroceso vuelve sin descargar), y un juego con requestAnimationFrame y las
   flechas (preventDefault: la página no se desplaza). Se mide el remaquetado
   incremental frente al completo y se comprueba que pintan lo mismo (la firma
   de la lista de pintura).
"""
import http.server
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
from h135_qjs_dom import png_pixels, caixa_de  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANF = os.path.join(RAIZ, "tests", "web", "qjs_anfitriao")
Q = os.path.join(RAIZ, "userland", "web", "quickjs")
OBJ = os.path.join(RAIZ, "build", "anfitriao_qjs")


# ------------------------------------------------------------------ 1. en el PC

def compilar_prova():
    os.makedirs(OBJ, exist_ok=True)
    env = dict(os.environ)
    env.pop("CPATH", None)
    cc = ["gcc", "-O1", "-g", "-w", "-D_GNU_SOURCE"]
    objs = []
    for f in ["quickjs", "cutils", "libregexp", "libunicode", "dtoa"]:
        src, o = os.path.join(Q, f + ".c"), os.path.join(OBJ, f + ".o")
        if not os.path.exists(o) or os.path.getmtime(o) < os.path.getmtime(src):
            subprocess.run(cc + ['-DCONFIG_VERSION="anfitriao"', "-c", src, "-o", o], check=True, env=env)
        objs.append(o)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "js2h.py"), os.path.join(Q, "qjs_dom.js"),
                    os.path.join(OBJ, "qjs_dom.h"), "qjs_dom_js"], check=True)
    for src, o, extra in [(os.path.join(Q, "qjs_bridge.c"), "ponte.o", ["-DQJS_ANFITRIAO", "-I" + OBJ]),
                          (os.path.join(ANF, "prova.c"), "prova.o", [])]:
        subprocess.run(cc + extra + ["-I" + ANF, "-I" + Q, "-c", src, "-o", os.path.join(OBJ, o)], check=True, env=env)
        objs.append(os.path.join(OBJ, o))
    exe = os.path.join(OBJ, "prova")
    subprocess.run(["gcc", "-o", exe] + objs + ["-lm"], check=True, env=env)
    return exe


def parte_pc():
    exe = compilar_prova()
    casos = sorted(f for f in os.listdir(os.path.join(ANF, "casos")) if f.endswith(".js"))
    verificar(len(casos) >= 4, "hay casos para el puente en el PC")
    for c in casos:
        cam = os.path.join(ANF, "casos", c)
        esperado = [l[4:].rstrip("\n") for l in open(cam, encoding="utf-8") if l.startswith("//= ")]
        r = subprocess.run([exe, os.path.join(ANF, "pagina.html"), cam], capture_output=True, text=True, timeout=60)
        saida = r.stdout.splitlines()
        i = 0
        for linha in saida:
            if i < len(esperado) and linha.startswith(esperado[i]):
                i += 1
        if i < len(esperado):
            print("\n".join("      | " + l for l in saida[:80]))
        verificar(r.returncode == 0, f"{c}: el puente no se cae")
        verificar(i == len(esperado), f"{c}: escribe lo esperado ({i}/{len(esperado)}; falta «{esperado[i] if i < len(esperado) else ''}»)")
        verificar(not any(l.startswith("qjs: erro") for l in saida), f"{c}: sin errores de JavaScript")


# ------------------------------------------------------------------ 2. en QEMU

REGLAS = "\n".join(f".r{i} {{ color: #{(i * 37) % 256:02x}{(i * 11) % 256:02x}40; }} .lista .r{i} b {{ font-weight: bold; }}" for i in range(250))
ITENS = "\n".join(f'<li class="item r{i % 250}"><b>elemento</b> {i}</li>' for i in range(300))

PAGINA = """<!doctype html><html><head><title>Cipo JS DOM</title>
<style>
body { margin: 0; font-family: sans-serif; font-size: 13px; }
#barra { position: relative; height: 44px; background: #f4f4f4; }
.bt { position: absolute; top: 6px; width: 110px; height: 30px; color: #fff; }
#boton { left: 8px; background: #e00000; }
#agregar { left: 130px; background: #00a000; }
#rutaA { left: 252px; background: #0000d0; display: block; text-decoration: none; }
#cuenta { position: absolute; left: 380px; top: 12px; }
#vista { height: 30px; padding: 4px 8px; background: #fff8d0; }
#medir { width: 200px; height: 50px; padding: 10px; border: 5px solid #333; margin: 7px; }
#juego { position: relative; width: 320px; height: 110px; background: #dddddd; margin: 8px; }
#nave { position: absolute; left: 10px; top: 80px; width: 20px; height: 20px; background: #ff00ff; }
#bola { position: absolute; left: 0; top: 10px; width: 10px; height: 10px; background: #000; }
.lista li { margin: 1px 0; }
.nuevo { background: #e0ffe0; }
REGLAS
</style></head>
<body>
<div id="barra"><div id="boton" class="bt">Contador</div><div id="agregar" class="bt">Agregar</div><a id="rutaA" class="bt" href="/ruta/a">Ruta A</a><span id="cuenta">Clics: 0</span></div>
<div id="vista">Inicio</div>
<div id="juego"><div id="nave"></div><div id="bola"></div></div>
<div id="medir">medir</div>
<ul id="lista" class="lista">
ITENS
</ul>
<script>
function caja(id) { var r = document.getElementById(id).getBoundingClientRect(); return [r.left, r.top, r.width, r.height].join(' '); }
var cuenta = 0, out = document.getElementById('cuenta'), lista = document.getElementById('lista'), nuevos = 0;
document.getElementById('boton').addEventListener('click', function (e) { cuenta++; out.textContent = 'Clics: ' + cuenta; console.log('contador ' + cuenta + ' ' + e.target.id); });
document.addEventListener('click', function (e) {
  if (e.target.id !== 'agregar') return;
  for (var i = 0; i < 5; i++) { var li = document.createElement('li'); li.className = 'item nuevo'; li.textContent = 'nuevo ' + (++nuevos); lista.insertBefore(li, lista.firstElementChild); }
  console.log('lista ' + lista.children.length + ' primero ' + lista.firstElementChild.textContent);
});
var vista = document.getElementById('vista');
function render(p) { vista.innerHTML = '<b>Ruta</b> ' + p; console.log('spa ruta ' + p); }
document.getElementById('rutaA').addEventListener('click', function (e) { e.preventDefault(); history.pushState({ r: 'a' }, '', '/ruta/a'); render(location.pathname); });
window.addEventListener('popstate', function (e) { render(location.pathname); console.log('spa popstate ' + JSON.stringify(e.state)); });
var m = document.getElementById('medir'), r = m.getBoundingClientRect(), cs = getComputedStyle(m);
console.log('medidas ' + [r.width, r.height, m.offsetWidth, m.offsetHeight, m.clientWidth, m.clientHeight, cs.width, cs.paddingLeft, cs.display, m.offsetLeft].join(' '));
console.log('ventana ' + innerWidth + ' ' + innerHeight + ' ' + (document.documentElement.scrollHeight > innerHeight));
['boton', 'agregar', 'rutaA', 'nave'].forEach(function (id) { console.log('pos ' + id + ' ' + caja(id)); });
var nave = document.getElementById('nave'), bola = document.getElementById('bola'), x = 10, cuadros = 0, bx = 0, dir = 4;
document.addEventListener('keydown', function (e) {
  if (e.key === 'ArrowRight') { x += 20; e.preventDefault(); }
  else if (e.key === 'ArrowLeft') { x -= 20; e.preventDefault(); }
  else return;
  nave.style.left = x + 'px';
  console.log('nave x=' + x + ' ' + caja('nave'));
});
function paso(t) {
  cuadros++; bx += dir; if (bx > 300 || bx < 0) dir = -dir;
  bola.style.left = bx + 'px'; bola.style.top = (10 + cuadros % 50) + 'px';
  if (cuadros === 30) console.log('juego cuadros 30 ' + (typeof t));
  if (cuadros < 600) requestAnimationFrame(paso); else console.log('juego fin');
}
requestAnimationFrame(paso);
setTimeout(function () {
  function prueba(n) {
    var t = Date.now();
    for (var i = 0; i < n; i++) { var li = document.createElement('li'); li.className = 'item r' + i; li.textContent = 'medida ' + i; lista.appendChild(li); lista.lastElementChild.offsetHeight; }
    return (Date.now() - t) / n;
  }
  var inc = prueba(12), f1 = __sagui_nat.firma();
  __sagui_nat.incremental(0);
  var f2 = __sagui_nat.firma(), comp = prueba(12);
  __sagui_nat.incremental(1);
  console.log('medida incremental ' + inc.toFixed(1) + ' ms completo ' + comp.toFixed(1) + ' ms');
  console.log('firma ' + (f1 === f2 ? 'igual' : 'distinta ' + f1 + ' ' + f2));
}, 300);
console.log('script listo');
</script>
</body></html>""".replace("REGLAS", REGLAS).replace("ITENS", ITENS)


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    pedidos = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        Tratador.pedidos.append(self.path)
        corpo = PAGINA.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def vermelho(r, g, b):
    return r > 180 and g < 60 and b < 60


def magenta(r, g, b):
    return r > 200 and b > 200 and g < 60


def esperar(m, texto, desde=0, tempo=60):
    limite = time.time() + tempo
    while time.time() < limite:
        s = m.serial().replace("\r", "")
        if texto in s[desde:]:
            return s
        time.sleep(0.2)
    return m.serial().replace("\r", "")


def parte_qemu():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h488", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Cipo JS DOM"', 120)
            s = esperar(m, "firma ", tempo=90)
            s = esperar(m, "juego cuadros 30", tempo=60)
            verificar("cipo: js: script listo" in s, "el script de la página corrió entero")
            verificar("qjs: erro" not in s, "sin errores de JavaScript al cargar")
            md = re.search(r"cipo: js: medidas (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)", s)
            verificar(md is not None, "el script midió la caja")
            if md:
                print("    medidas:", md.group(0))
                verificar(md.groups()[:6] == ("230", "80", "230", "80", "220", "70"),
                          "getBoundingClientRect, offsetWidth/Height y clientWidth/Height (200x50 + 10 de relleno + 5 de borde)")
                verificar(md.group(7) == "200px" and md.group(8) == "10px" and md.group(9) == "block",
                          "getComputedStyle: width, padding-left y display")
                verificar(md.group(10) == "7", "offsetLeft con el margen")
            verificar(re.search(r"cipo: js: ventana \d+ \d+ true", s) is not None, "innerWidth/innerHeight y scrollHeight")
            verificar("cipo: js: juego cuadros 30 number" in s, "requestAnimationFrame corre cuadro a cuadro (con el tiempo)")
            mi = re.search(r"cipo: js: medida incremental ([\d.]+) ms completo ([\d.]+) ms", s)
            verificar(mi is not None, "se midió el remaquetado incremental frente al completo")
            if mi:
                inc, comp = float(mi.group(1)), float(mi.group(2))
                print(f"    remaquetado tras appendChild + offsetHeight: incremental {inc} ms, completo {comp} ms")
                verificar(inc < comp, "el remaquetado incremental es más rápido que el completo")
            verificar("cipo: js: firma igual" in s, "el incremental pinta lo mismo que el completo (misma firma)")
            verificar(re.search(r"cipo: js: layout \d+ incremental: \d+ estilos", s) is not None, "hay maquetaciones incrementales")
            verificar(re.search(r"cipo: js: pintadas \d+ de \d+ filas", s) is not None, "se repinta solo una franja")
            fr = re.findall(r"cipo: js: pintadas (\d+) de (\d+) filas", s)
            if fr:
                parciais = sum(1 for a, b in fr if int(a) < int(b))
                print(f"    repintados parciales: {parciais} de {len(fr)}")
                verificar(parciais > 0, "algún repintado es de menos filas que la página visible")

            # la posición de la página en la pantalla: el botón rojo
            pos = {k: tuple(float(v) for v in vs.split()) for k, vs in re.findall(r"cipo: js: pos (\w+) ([\d. -]+)", s)}
            verificar("boton" in pos and "nave" in pos, "el script dijo dónde están sus elementos")
            cap = m.captura("H488-cipo-js-dom")
            larg, alt, canais, linhas = png_pixels(cap)
            jan = re.search(r'janela \d+ criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
            verificar(jan is not None, "la ventana del Cipó tiene origen conocido")
            jl, ja, jx, jy = map(int, jan.groups())
            regiao = (jx, jy + 70, jl, ja - 70)     # debajo de la barra del Cipó: sus iconos rojos no cuentan
            bt = caixa_de(linhas, canais, vermelho, regiao)
            verificar(bt is not None, "el botón rojo se ve")
            if bt is None or "boton" not in pos:
                return 1
            ox, oy = bt[0] - pos["boton"][0], bt[1] - pos["boton"][1]

            def centro(k):
                x, y, w, h = pos[k]
                return int(ox + x + w / 2), int(oy + y + h / 2)

            # contador
            desde = len(m.serial().replace("\r", ""))
            m.clique(*centro("boton"))
            time.sleep(0.8)
            m.clique(*centro("boton"))
            s = esperar(m, "contador 2", desde, 20)
            verificar("cipo: js: contador 1 boton" in s[desde:] and "cipo: js: contador 2 boton" in s[desde:], "el contador cuenta los clics")
            # la lista crece (delegación en el document)
            desde = len(m.serial().replace("\r", ""))
            m.clique(*centro("agregar"))
            s = esperar(m, "cipo: js: lista ", desde, 20)
            verificar(re.search(r"cipo: js: lista 3\d\d primero nuevo 5", s[desde:]) is not None, "la lista crece con insertBefore")
            # el juego: las flechas mueven la nave y no desplazan la página
            desde = len(m.serial().replace("\r", ""))
            for _ in range(3):
                m.tecla("right", 0.5)
            s = esperar(m, "nave x=70", desde, 20)
            verificar("cipo: js: nave x=70" in s[desde:], "las flechas llegan al keydown de la página")
            verificar("cipo: rolagem para" not in s[desde:], "el preventDefault de las flechas evita el desplazamiento")
            time.sleep(1.0)
            cap = m.captura("H488-cipo-js-juego")
            larg, alt, canais, linhas = png_pixels(cap)
            nv = caixa_de(linhas, canais, magenta, regiao)
            nx = ox + pos["nave"][0] + 60
            verificar(nv is not None and abs(nv[0] - nx) <= 2, f"la nave se ve 60 px a la derecha ({nv[0] if nv else None} ~ {nx:.0f})")
            # la SPA: pushState sin navegar y Retroceso con popstate
            desde = len(m.serial().replace("\r", ""))
            pedidos = len(Tratador.pedidos)
            m.clique(*centro("rutaA"))
            s = esperar(m, "spa ruta /ruta/a", desde, 20)
            verificar("cipo: js: spa ruta /ruta/a" in s[desde:], "el clic en la ruta la pinta (pushState)")
            verificar(re.search(r"cipo: js: historia push \S+/ruta/a", s[desde:]) is not None, "la dirección cambia sin navegar")
            verificar("cipo: abrindo" not in s[desde:], "no se descarga otra página")
            desde = len(m.serial().replace("\r", ""))
            m.tecla("backspace", 0.5)
            s = esperar(m, "spa popstate", desde, 20)
            verificar("cipo: js: popstate -1" in s[desde:] and "cipo: js: spa popstate null" in s[desde:], "Retroceso vuelve a la entrada de antes con popstate")
            verificar("cipo: js: spa ruta /" in s[desde:], "la SPA pinta la ruta de antes")
            verificar("cipo: abrindo" not in s[desde:] and len(Tratador.pedidos) == pedidos, "volver no descarga la página")
            m.captura("H488-cipo-js-spa")
            s = m.serial().replace("\r", "")
            verificar("Fallo de página" not in s, "el navegador no se cayó")
            verificar("qjs: erro" not in s, "ningún error de JavaScript")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


def main():
    parte_pc()
    if os.environ.get("H488_SO_PC"):
        return 0
    return parte_qemu()


if __name__ == "__main__":
    sys.exit(main())
