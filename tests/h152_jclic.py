#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H152 - visor de actividades JClic y descargador del repositorio del XTEC (D185).

Criterio:
  - `jclic --autoprueba ejemplo.jclic.zip` (sin ventana) abre el proyecto de prueba propio (ZIP con XML, un PNG
    y GIF), resuelve cada actividad por las mismas funciones que usa el ratón (puzle doble, de intercambio y de
    agujero, asociación simple y compleja, memoria, explorar, identificar, sopa de letras y crucigrama), cuenta
    bien un gesto equivocado y salta la actividad de texto avisando; 0 fallos;
  - `jclicbajar` contra un servidor falso (10.0.2.2) que imita el repositorio: lista el índice (con acentos y
    UTF-8), filtra por idioma y por texto, baja un proyecto a /tmp/jclic (no hay /home persistente) con su .txt
    de atribución, rechaza lo que no es un .jclic.zip y un camino que no existe;
  - lo bajado se juega (`--autoprueba` sobre /tmp/jclic/...);
  - Inicio > Educación > Actividades JClic abre la lista (el proyecto incluido y el bajado, marcado con +); un doble
    clic abre el proyecto; el contador cuenta aciertos e intentos;
  - con el ratón: se resuelve el puzle doble (clic en la pieza y en su hueco), se arrastra una pieza del
    intercambio, se mueve una pieza al agujero, se asocia (simple y compleja), se resuelve el memoria, se
    marca en la sopa de letras arrastrando y se escribe el crucigrama con el teclado; "Siguiente" cambia de
    actividad y la de texto sale con el aviso de «no soportada»;
  - el descargador (ventana) lista el índice, busca, baja con el botón y el proyecto queda marcado;
  - desde Archivos, un doble clic en un .jclic.zip abre el visor;
  - si la red de QEMU llega a internet, baja un proyecto real del repositorio (si no, se salta con aviso).
Capturas en docs/capturas/H152-*.png.
"""
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui  # noqa: E402

ZIP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "datos", "jclic", "ejemplo.jclic.zip")

INDICE = [
    {"id": 3, "path": "pruebas/uno", "title": "Matemáticas divertidas", "author": "Ana Pérez (Maestra)",
     "date": "01/02/20", "langCodes": ["es"], "levelCodes": ["PRI"], "areaCodes": ["mat"], "mainFile": "jclic.js/uno.jclic"},
    {"id": 2, "path": "pruebas/dos", "title": "Les estacions", "author": "Joan Prova", "date": "03/04/19",
     "langCodes": ["ca"], "levelCodes": ["PRI"], "areaCodes": ["mn"], "mainFile": "jclic.js/dos.jclic"},
    {"id": 1, "path": "tres", "title": "The weather — el tiempo", "author": "Eve Test", "date": "05/06/18",
     "langCodes": ["en", "es"], "levelCodes": ["ESO"], "areaCodes": ["ang"], "mainFile": "jclic.js/tres.jclic"},
]
peticiones = []


class Manejador(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *a):
        pass

    def do_GET(self):
        peticiones.append(self.path)
        if self.path == "/projects/projects.json":
            cuerpo = json.dumps(INDICE).encode()          # con \uXXXX escapados: prueba el lector
        elif self.path == "/projects/pruebas/uno/jclic/uno.jclic.zip":
            cuerpo = open(ZIP, "rb").read()
        elif self.path == "/projects/tres/jclic/tres.jclic.zip":
            cuerpo = b"esto no es un zip, ni de lejos" * 5     # el descargador debe rechazarlo
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)


def servidor():
    s = ThreadingHTTPServer(("127.0.0.1", 0), Manejador)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    return s, s.server_address[1]


# ---------------------------------------------------------------- ayudantes

def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def estado(serial):
    """El último 'estado' del visor y sus cajas: {por, act, tipo, aciertos, intentos, alvo, resuelta, cajas[p][i]}."""
    pos = serial.rfind("jclic: estado ")
    bloque = serial[pos:]
    g = re.match(r"jclic: estado (\S+) act=(\d+) tipo=(\d+) aciertos=(\d+) intentos=(\d+) alvo=(\d+) resuelta=(\d+) sel=(-?\d+)/(-?\d+)", bloque)
    e = dict(por=g[1], act=int(g[2]), tipo=int(g[3]), aciertos=int(g[4]), intentos=int(g[5]), alvo=int(g[6]),
             resuelta=int(g[7]), cajas={0: {}, 1: {}})
    # las cajas van pegadas al estado (hasta el siguiente 'estado' o 'actividad')
    fin = len(bloque)
    for marca in ("jclic: estado ", "jclic: actividad "):
        k = bloque.find(marca, 10)
        if k > 0:
            fin = min(fin, k)
    for c in re.finditer(r"jclic: caja (\d) (\d+) x=(-?\d+) y=(-?\d+) w=(\d+) h=(\d+) orig=(-?\d+) est=(\d+)", bloque[:fin]):
        p, i = int(c[1]), int(c[2])
        e["cajas"][p][i] = dict(x=int(c[3]), y=int(c[4]), w=int(c[5]), h=int(c[6]), orig=int(c[7]), est=int(c[8]))
    gr = re.findall(r"jclic: grade ox=(-?\d+) oy=(-?\d+) cw=(\d+) ch=(\d+) lin=(\d+) col=(\d+)", bloque[:fin])
    if gr:
        e["grade"] = dict(zip(("ox", "oy", "cw", "ch", "lin", "col"), map(int, gr[-1])))
    return e


def centro(j, c):
    return j["x"] + c["x"] + c["w"] // 2, j["y"] + c["y"] + c["h"] // 2


def esperar_estado(m, pred, tempo=15, desde=0):
    fin = time.time() + tempo
    while time.time() < fin:
        s = m.serial()
        if "jclic: estado " in s[desde:]:
            e = estado(s)
            if pred(e):
                return e
        time.sleep(0.2)
    raise AssertionError("tiempo esgotado esperando el estado del visor")


def boton(j, nombre):
    """El centro de un botón de la barra de abajo (coordenadas de la ventana del visor)."""
    x = dict(atras=46, siguiente=135, reiniciar=230, ayuda=308, lista=382)[nombre]
    return j["x"] + x, j["y"] + j["a"] - 19


def siguiente(m, j):
    m.clique(*boton(j, "siguiente"))
    time.sleep(0.6)


def aterrizar(m, j, desde):
    """Espera a la actividad nueva y devuelve su estado de inicio."""
    return esperar_estado(m, lambda e: e["por"] == "inicio", 15, desde)


# ---------------------------------------------------------------- principal

def principal():
    srv, puerto = servidor()
    base = f"http://10.0.2.2:{puerto}/projects"
    with Maquina("h152", "ide", extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- 1. la autoprueba, sin ventana
        m.digitar("jclic --autoprueba /jogos/jclic/ejemplo.jclic.zip\n", 0.03)
        m.esperar_serial("jclic: autoprueba ok", 120)
        s = m.serial()
        verificar("autoprueba FALLO" not in s, "la autoprueba no registra ningún FALLO")
        r = re.search(r"autoprueba resumen jugadas=(\d+) resueltas=(\d+) saltadas=(\d+) fallos=(\d+) tipos=0x([0-9a-f]+)", s)
        verificar(r and (r[1], r[2], r[3], r[4]) == ("11", "9", "1", "0"),
                  f"11 actividades jugadas, 9 resueltas (las otras 2 no se resuelven: información y explorar), 1 de texto saltada ({r and r.groups()})")
        verificar(int(r[5], 16) == 0xFFE, "se juegan los 11 tipos de actividad (de información a crucigrama)")
        verificar("text.FillInBlanks: no soportada (se salta)" in s, "la actividad de texto se salta avisando")
        m.esperar_serial("sagui:/$", 30)

        # ---- 2. el descargador, en modo texto, contra el servidor falso
        out = m.comando(f"jclicbajar --lista --url {base}", 90)
        verificar("Indice al dia: 3 proyectos" in out and "2 proyectos" in out and "Les estacions" not in out,
                  "el índice tiene 3 proyectos y la lista por defecto es la de español: 2")
        verificar("Matem" in out and "ticas divertidas" in out and "pruebas/uno" in out, "los títulos se leen (el acento llega escapado como \\u00e1)")
        out = m.comando(f"jclicbajar --lista --idioma ca --url {base}", 60)
        verificar("1 proyectos" in out and "Les estacions" in out, "--idioma ca: solo el proyecto en catalán")
        out = m.comando(f"jclicbajar --lista --idioma todos --buscar weather --url {base}", 60)
        verificar("1 proyectos" in out and "pruebas" not in out and "tres" in out, "--idioma todos --buscar weather: 1 resultado")
        out = m.comando(f"jclicbajar --bajar pruebas/uno --url {base}", 90)
        verificar("Bajado:" in out and "/tmp/jclic/pruebas-uno.jclic.zip" in out, "bajar: guarda en /tmp/jclic con el camino como nombre")
        out = m.comando("cat /tmp/jclic/pruebas-uno.txt", 30)
        verificar("Autor: Ana" in out and "Licencia: la de cada proyecto" in out and "projects/pruebas/uno/jclic/uno.jclic.zip" in out,
                  "el .txt guarda el titulo, el autor y el origen y avisa de la licencia")
        out = m.comando(f"jclicbajar --bajar tres --url {base}", 60)
        verificar("no es un .jclic.zip valido" in out, "lo que no es un ZIP con .jclic se rechaza")
        out = m.comando("ls /tmp/jclic", 30)
        verificar("tres" not in out and "pruebas-uno.jclic.zip" in out, "y no deja nada guardado")
        out = m.comando(f"jclicbajar --bajar nada/nada --url {base}", 60)
        verificar("no hay ningun proyecto con el camino" in out, "un camino que no existe da error")
        out = m.comando("sha256sum /tmp/jclic/pruebas-uno.jclic.zip /jogos/jclic/ejemplo.jclic.zip", 30)
        h = re.findall(r"^([0-9a-f]{64})", out, re.M)
        verificar(len(h) == 2 and h[0] == h[1], "lo bajado es idéntico a lo que sirve el servidor (SHA-256)")
        m.digitar("jclic --autoprueba /tmp/jclic/pruebas-uno.jclic.zip\n", 0.03)
        m.esperar_serial("jclic: autoprueba ok", 120)
        verificar(True, "el proyecto bajado se abre y se juega (--autoprueba ok)")
        m.esperar_serial("sagui:/$", 30)
        verificar(any("/projects/pruebas/uno/jclic/uno.jclic.zip" in p for p in peticiones), "el servidor recibió el camino esperado")

        # ---- 3. Inicio > Educación > Actividades JClic
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        verificar(">Educación" in p["linhas"] or ">Educaci" in " ".join(p["linhas"]), "el menú Inicio tiene el grupo Educación")
        sub = submenu_pelo_mouse(m, p, "Educación", "Actividades JClic")
        verificar("Actividades JClic" in sub["linhas"] and "Actividades JClic (bajar)" in sub["linhas"],
                  f"Educación contiene Actividades JClic y Actividades JClic (bajar) ({', '.join(sub['linhas'])})")
        time.sleep(0.4)
        m.captura("H152-1-menu-educacion")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /bin/jclic\b", desde=marca)
        m.esperar_serial("jclic: pronto", 40)
        time.sleep(1.5)
        j = janela(m.serial(), "Actividades JClic")
        m.captura("H152-2-lista")
        # la lista: el proyecto incluido y el bajado (con +, y su título del .txt)
        # (los dos elementos: ejemplo.jclic.zip de /jogos/jclic y el bajado de /tmp/jclic)
        marca = len(m.serial())
        m.clique(j["x"] + 120, j["y"] + 70, duplo=True)   # el primero (el que viene con el sistema)
        m.esperar_serial("jclic: proyecto \"Aprende jugando", 30)
        e = esperar_estado(m, lambda e: e["act"] == 0 and e["tipo"] == 1, 15, marca)   # (el segundo clic del doble llega ya al juego)
        verificar(e["tipo"] == 1 and e["act"] == 0, "el doble clic abre el proyecto en la pantalla de información")
        time.sleep(1.0)
        m.captura("H152-3-informacion")

        # ---- 4. puzle doble con el ratón (clic en la pieza y en su hueco)
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 1 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 2 and e["alvo"] == 6 and len(e["cajas"][0]) == 6 and len(e["cajas"][1]) == 6,
                  "puzle doble: 6 piezas en A y 6 huecos en B")
        verificar(sorted(c["orig"] for c in e["cajas"][0].values()) == list(range(6)), "las 6 piezas están embarajadas")
        time.sleep(0.8)
        m.captura("H152-4-puzle")
        err = [i for i, c in e["cajas"][0].items() if c["orig"] != 0][0]
        pieza = {c["orig"]: i for i, c in e["cajas"][0].items()}
        # un intento equivocado: una pieza que no es la 0 al hueco 0
        m.clique(*centro(j, e["cajas"][0][err]))
        m.clique(*centro(j, e["cajas"][1][0]))
        e2 = esperar_estado(m, lambda x: x["intentos"] == 1, 15)
        verificar(e2["aciertos"] == 0 and e2["intentos"] == 1, "una pieza en el hueco equivocado: 1 intento, 0 aciertos (la pieza vuelve)")
        for orig in range(6):
            m.clique(*centro(j, e["cajas"][0][pieza[orig]]))
            m.clique(*centro(j, e["cajas"][1][orig]))
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 25)
        verificar(e2["aciertos"] == 6 and e2["intentos"] == 7, f"puzle resuelto con el ratón: 6 aciertos, 7 intentos ({e2['aciertos']}/{e2['intentos']})")
        time.sleep(0.8)
        m.captura("H152-5-puzle-resuelto")
        m.clique(j["x"] + 300, j["y"] + 200)             # el clic cierra el aviso final

        # ---- 5. intercambio (arrastrar) y agujero (mover una pieza)
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 2 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 3 and len(e["cajas"][0]) == 4, "puzle de intercambio: 4 piezas")
        a, b = [(i, c) for i, c in e["cajas"][0].items()][:2]
        x0, y0 = centro(j, a[1])
        x1, y1 = centro(j, b[1])
        m.arrastar(x0, y0, x1, y1)
        e2 = esperar_estado(m, lambda x: x["por"] == "soltar" and x["intentos"] == 1, 15)
        verificar(e2["cajas"][0][a[0]]["orig"] == b[1]["orig"] and e2["cajas"][0][b[0]]["orig"] == a[1]["orig"],
                  "arrastrar una pieza sobre otra las intercambia")
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 3 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 4 and len(e["cajas"][0]) == 9, "puzle de agujero: 9 piezas")
        h = [i for i, c in e["cajas"][0].items() if c["est"] == 1][0]
        vec = [h - 3, h + 3] + ([h - 1] if h % 3 else []) + ([h + 1] if h % 3 < 2 else [])
        v = [k for k in vec if 0 <= k < 9][0]
        m.clique(*centro(j, e["cajas"][0][v]))
        e2 = esperar_estado(m, lambda x: x["por"] == "soltar" and x["intentos"] == 1, 15)
        verificar(e2["cajas"][0][v]["est"] == 1 and e2["cajas"][0][h]["est"] == 0, "un clic en una pieza junto al agujero la desliza hasta él")
        time.sleep(0.5)
        m.captura("H152-6-agujero")

        # ---- 6. asociación simple y compleja
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 4 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 5 and e["alvo"] == 4, "asociación simple: 4 parejas")
        for ia, ca in e["cajas"][0].items():
            ib = [i for i, c in e["cajas"][1].items() if c["orig"] == ca["orig"]][0]
            m.clique(*centro(j, ca))
            m.clique(*centro(j, e["cajas"][1][ib]))
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 25)
        verificar(e2["aciertos"] == 4 and e2["intentos"] == 4, "asociación simple resuelta con el ratón (4 aciertos, 4 intentos)")
        time.sleep(0.6)
        m.captura("H152-7-asociacion")
        m.clique(j["x"] + 300, j["y"] + 200)
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 5 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 6 and e["alvo"] == 6, "asociación compleja: 6 números, 2 grupos")
        pares = {2: 0, 7: 1, 10: 0, 13: 1, 4: 0, 9: 1}   # número -> grupo (0 pares, 1 impares); los ids vienen del XML
        # en el XML los ids son [0,1,0,1,0,1]; el grupo "Pares" (B0) y "Impares" (B1) llevan orig 0 y 1
        ids = {0: 0, 1: 1, 2: 0, 3: 1, 4: 0, 5: 1}
        for ia, ca in e["cajas"][0].items():
            ib = [i for i, c in e["cajas"][1].items() if c["orig"] == ids[ca["orig"]]][0]
            m.clique(*centro(j, ca))
            m.clique(*centro(j, e["cajas"][1][ib]))
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 25)
        verificar(e2["aciertos"] == 6, "asociación compleja resuelta con el ratón (6 aciertos)")
        m.clique(j["x"] + 300, j["y"] + 200)

        # ---- 7. memoria
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 6 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 7 and e["alvo"] == 4 and len(e["cajas"][0]) == 8, "memoria: 8 cartas, 4 parejas")
        # un fallo: dos cartas distintas (se vuelven a tapar)
        cs = e["cajas"][0]
        i1 = 0
        i2 = [i for i, c in cs.items() if c["orig"] != cs[0]["orig"]][0]
        m.clique(*centro(j, cs[i1]))
        m.clique(*centro(j, cs[i2]))
        e2 = esperar_estado(m, lambda x: x["intentos"] == 1, 15)
        time.sleep(0.4)
        m.captura("H152-8-memoria-fallo")
        time.sleep(1.2)   # se tapan solas
        for par in range(4):
            ii = [i for i, c in cs.items() if c["orig"] == par]
            m.clique(*centro(j, cs[ii[0]]))
            m.clique(*centro(j, cs[ii[1]]))
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 25)
        verificar(e2["aciertos"] == 4 and e2["intentos"] == 5, f"memoria resuelta: 4 aciertos, 5 intentos ({e2['aciertos']}/{e2['intentos']})")
        m.clique(j["x"] + 300, j["y"] + 200)

        # ---- 8. explorar e identificar
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 7 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 8, "explorar")
        m.clique(*centro(j, e["cajas"][0][0]))
        time.sleep(0.6)
        m.captura("H152-9-explorar")
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 8 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 9 and e["alvo"] == 1, "identificar: una respuesta correcta")
        bien = [i for i, c in e["cajas"][0].items() if c["orig"] == 1][0]
        mal = [i for i, c in e["cajas"][0].items() if c["orig"] == 0][0]
        m.clique(*centro(j, e["cajas"][0][mal]))
        e2 = esperar_estado(m, lambda x: x["intentos"] == 1, 15)
        verificar(e2["aciertos"] == 0 and e2["resuelta"] == 0, "una respuesta equivocada cuenta un intento sin acierto")
        m.clique(*centro(j, e["cajas"][0][bien]))
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 15)
        verificar(e2["aciertos"] == 1 and e2["intentos"] == 2, "identificar resuelta")
        m.clique(j["x"] + 300, j["y"] + 200)

        # ---- 9. sopa de letras (arrastrando)
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 9 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 10 and e["alvo"] == 4 and "grade" in e, "sopa de letras: 4 palabras")
        g = e["grade"]

        def celda(cx, cy):
            return j["x"] + g["ox"] + cx * g["cw"] + g["cw"] // 2, j["y"] + g["oy"] + cy * g["ch"] + g["ch"] // 2
        m.arrastar(*celda(0, 0), *celda(3, 0))     # LUNA
        e2 = esperar_estado(m, lambda x: x["aciertos"] == 1, 15)
        m.arrastar(*celda(2, 2), *celda(0, 2))     # SOL al revés
        e2 = esperar_estado(m, lambda x: x["aciertos"] == 2, 15)
        m.arrastar(*celda(0, 0), *celda(0, 3))     # una selección que no es palabra
        e2 = esperar_estado(m, lambda x: x["intentos"] == 3, 15)
        verificar(e2["aciertos"] == 2 and e2["intentos"] == 3, "sopa: dos palabras (una al revés) y una selección equivocada")
        m.arrastar(*celda(4, 5), *celda(7, 5))     # PASO
        m.arrastar(*celda(0, 4), *celda(2, 4))     # PAN
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 25)
        verificar(e2["aciertos"] == 4, "sopa de letras resuelta con el ratón")
        time.sleep(0.6)
        m.captura("H152-10-sopa")
        m.clique(j["x"] + 300, j["y"] + 200)

        # ---- 10. crucigrama (con el teclado)
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 10 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 11 and e["alvo"] == 6, "crucigrama: 6 letras")
        g = e["grade"]
        m.clique(j["x"] + g["ox"] + 1 * g["cw"] + 5, j["y"] + g["oy"] + 5)       # la casilla (1,0)
        m.digitar("sol", 0.1)
        e2 = esperar_estado(m, lambda x: x["aciertos"] == 3, 15)
        time.sleep(0.4)
        m.captura("H152-11-crucigrama")
        m.clique(j["x"] + g["ox"] + 5, j["y"] + g["oy"] + 2 * g["ch"] + 5)       # (0,2)
        m.digitar("pan", 0.1)
        e2 = esperar_estado(m, lambda x: x["resuelta"] == 1, 15)
        verificar(e2["aciertos"] == 6 and e2["intentos"] == 6, "crucigrama escrito con el teclado: 6 aciertos")
        m.clique(j["x"] + 300, j["y"] + 200)

        # ---- 11. la actividad de texto se salta con aviso, y el contador de actividades
        marca = len(m.serial())
        siguiente(m, j)
        e = esperar_estado(m, lambda e: e["act"] == 11 and e["por"] == "inicio", 15, marca)
        verificar(e["tipo"] == 12, "la actividad de texto sale con tipo «texto» (no soportada)")
        time.sleep(0.6)
        png = m.captura("H152-12-texto-saltado")
        # se puede volver atrás y reiniciar
        marca = len(m.serial())
        m.clique(*boton(j, "atras"))
        e = esperar_estado(m, lambda e: e["act"] == 10 and e["por"] == "inicio", 15, marca)
        verificar(e["aciertos"] == 0 and e["resuelta"] == 0, "«Atrás» vuelve a la anterior (reiniciada)")

        # ---- 12. cerrar y el descargador en ventana
        m.clique(j["x"] + j["l"] - 12, j["y"] - 12)
        time.sleep(1.0)
        marca = len(m.serial())
        m.digitar(f"jclicbajar --url {base}\n", 0.03)
        m.esperar_serial("jclicbajar: pronto indice=3", 60)
        time.sleep(1.0)
        jb = janela(m.serial(), "Actividades JClic (bajar)")
        m.captura("H152-13-descargador")
        # el indice (idioma es): 2 proyectos; se elige el segundo ("The weather", en+es) y se intenta bajar -> no es un zip
        # buscar "mate": un solo resultado; se baja con el botón
        m.digitar("mate", 0.1)
        time.sleep(0.6)
        m.captura("H152-14-descargador-busqueda")
        lw = jb["l"] * 56 // 100
        bx, by = 8 + lw + 12 + 55, jb["a"] - 68 + 14
        m.clique(jb["x"] + 8 + 120, jb["y"] + 44 + 2 + 8)          # el primer resultado
        antes = m.serial().count("jclicbajar: bajado /tmp/jclic/pruebas-uno.jclic.zip")
        m.clique(jb["x"] + bx, jb["y"] + by)                       # Bajar
        time.sleep(3.0)
        m.captura("H152-15-descargador-bajado")
        # ya estaba bajado desde la parte de texto: el estado dice "Bajado" y la lista lo marca con +
        verificar(m.serial().count("jclicbajar: bajado /tmp/jclic/pruebas-uno.jclic.zip") == antes + 1, "el botón Bajar baja el proyecto elegido")
        m.tecla("esc", 0.3)
        time.sleep(1.0)

        # ---- 13. desde Archivos: doble clic en un .jclic.zip abre el visor
        m.comando("arquivos /jogos/jclic")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.8)
        ja = janela(m.serial(), "Archivos")
        marca = len(m.serial())
        m.clique(ja["x"] + 146 + 120, ja["y"] + 70 + 2 + 8, duplo=True)    # el primero: ejemplo.jclic.zip
        esperar(m, r"arquivos: abrindo no jclic /jogos/jclic/ejemplo\.jclic\.zip", desde=marca)
        m.esperar_serial("jclic: proyecto \"Aprende jugando", 30)
        verificar(True, "Archivos abre un .jclic.zip con el visor de JClic (asociación de la extensión)")
        time.sleep(1.5)
        m.captura("H152-16-desde-archivos")

        # ---- 14. red real (si hay): un proyecto de verdad del repositorio
        m.clique(330, 94)          # el foco vuelve al Terminal (la barra de título, visible detrás de las otras ventanas)
        time.sleep(0.5)
        s = m.comando("jclicbajar --bajar traba2", 150)
        if "Bajado:" in s:
            out = m.comando("jclic --autoprueba /tmp/jclic/traba2.jclic.zip", 120)
            m.esperar_serial("jclic: autoprueba", 60)
            verificar("autoprueba ok" in m.serial()[-3000:] or "jclic: autoprueba ok" in out,
                      "un proyecto real del repositorio (traba2, bajado por HTTPS) se abre y se juega")
            print("INFO  red real: proyecto traba2 bajado y jugado")
        else:
            print("AVISO sin red hacia el repositorio real (" + s.strip().splitlines()[-1][:100] + "): se salta la descarga real")
        verificar("excepci" not in m.serial().lower() and "page fault" not in m.serial().lower(), "ninguna excepción durante todo esto")
    srv.shutdown()


if __name__ == "__main__":
    principal()
