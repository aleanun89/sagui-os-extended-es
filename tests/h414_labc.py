#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H414 - Laboratorio de C: editor, compilar con libtcc, errores con su línea, ejecutar, -b, entrada (D414).

Partes (por defecto todas; o las que se pidan: h414_labc.py pc saguios):
  pc       /apps/bin/labc es un ELF32 estático; /apps/labc/ejemplos lleva los diez ejemplos y todos compilan
           con el TCC cruzado del PC; el icono está en Inicio > Educación (barra.c) y la ayuda existe.
  saguios  en SaguiOS: se abre el Laboratorio con un ejemplo; F5 compila y ejecuta (salida en el panel y en el
           log); un ejemplo con un error de compilación muestra el mensaje con su línea y el clic en él lleva el
           editor a esa línea; el ejemplo de límites pasa en silencio sin -b y con -b se para en su línea; la
           entrada de teclado llega al programa; una ventana de la libsagui se abre desde el Laboratorio y
           Detener mata un programa que no termina; guardar con Ctrl+S cambia el archivo.
Capturas: docs/capturas/H414-*.png.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
FUENTES_TCC = os.path.join(RAIZ, "userland", "terceros", "tcc", "fuentes", ".preparado")
CRUZADO = os.path.join(BUILD, "tcc", "anfitrion", "i386-tcc")
TCC_RAIZ = os.path.join(BUILD, "tcc", "raiz")
EJ = os.path.join(RAIZ, "userland", "programas", "labc", "ejemplos")
BORDA, TITULO = 2, 24


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-800:]))


def pc():
    verificar(os.path.isfile(os.path.join(APPS_RAIZ, "bin", "labc")), "la imagen lleva /apps/bin/labc")
    h = subprocess.run(["readelf", "-hl", "-W", os.path.join(BUILD, "userland", "bin", "labc.elf")],
                       capture_output=True, text=True).stdout
    verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "/apps/bin/labc: ELF32 estático de i386")
    ej = sorted(f for f in os.listdir(EJ) if f.endswith(".c"))
    verificar(len(ej) == 10, "hay diez ejemplos (%d)" % len(ej))
    for f in ej:
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "labc", "ejemplos", f)), "/apps/labc/ejemplos/%s" % f)
    d = tempfile.mkdtemp(prefix="h414-")
    for f in ej:
        r = subprocess.run([CRUZADO, "-B" + TCC_RAIZ, "-o", os.path.join(d, f[:-2]), os.path.join(EJ, f)],
                           capture_output=True, text=True)
        esperado = f.startswith("10-")
        verificar((r.returncode != 0) == esperado, "el ejemplo %s %s (%s)" % (f, "falla a propósito" if esperado else "compila",
                                                                        r.stderr.strip()[-120:]))
    b = open(os.path.join(RAIZ, "kernel", "gui", "barra.c"), encoding="utf-8").read()
    verificar('"/apps/bin/labc"' in b , "Laboratorio de C en el menú Inicio (barra.c)")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "laboratorio-c.md")), "ayuda /ayuda/laboratorio-c.md")
    dd = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D414." in dd, "D414 está en docs/decisoes.md")


def ventana(m, titulo, desde=0):
    g = re.findall(r'janela (\d+) criada por (\d+): "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo),
                   m.serial()[desde:])[-1]
    return dict(id=int(g[0]), l=int(g[2]), a=int(g[3]), x=int(g[4]) + BORDA, y=int(g[5]) + TITULO)


def lanzar(m, args, lento):
    marca = len(m.serial())
    m.digitar("labc %s" % args, 0.03)
    m.tecla("ret")
    esperar(m, r"labc: listo", 60 * lento, marca)
    time.sleep(1.5)
    return marca, ventana(m, "Laboratorio de C", marca)


def cerrar(m, w, marca):
    m.mouse_para(w["x"] + 300, w["y"] + 100)
    m.tecla("ctrl-q")
    esperar(m, r"labc: cerrado", 30, marca)
    time.sleep(1)


def saguios():
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "labc")):
        print("AVISO: la imagen no lleva labc (tools/tcc_fuentes.sh y make): se salta la parte «saguios»")
        return
    import h136_opentyrian as h136
    lento = 1 if KVM else 4
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos-mb", "64"])
    with Maquina("h414", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        EJD = "/apps/labc/ejemplos"
        # 1. hola: se abre con el ejemplo, F5 compila y ejecuta
        marca, w = lanzar(m, "%s/01-hola.c" % EJD, lento)
        m.clique(w["x"] + 400, w["y"] + 120)   # el foco al editor
        m.captura("H414-1-editor-resaltado")
        m.tecla("f5")
        esperar(m, r"labc: compilar bien: 0 mensajes, (\d+) ms", 60 * lento, marca)
        esperar(m, r"labc: salida: 2 \+ 3 = 5", 60 * lento, marca)
        s = m.serial()[marca:]
        verificar("labc: salida: Hola, mundo desde el Laboratorio de C!" in s, "F5 compila y ejecuta: sale el saludo")
        esperar(m, r"labc: programa terminado st=1 cod=0", 30 * lento, marca)
        ms = int(re.findall(r"labc: compilar bien: 0 mensajes, (\d+) ms", s)[-1])
        print("    compilar hola.c con libtcc: %d ms (%s)" % (ms, "KVM" if KVM else "TCG"))
        time.sleep(1)
        m.captura("H414-2-hola-salida")
        cerrar(m, w, marca)
        # 2. errores con su línea y clic que va a la línea
        marca, w = lanzar(m, "%s/10-errores.c" % EJD, lento)
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("f5")
        esperar(m, r"labc: compilar con errores: 1 mensajes", 60 * lento, marca)
        e = esperar(m, r"labc: error linea (\d+): ", 10, marca)
        verificar(int(e) > 0, "el error de compilación trae su línea (%s)" % e)
        s = m.serial()[marca:]
        marca2 = len(m.serial())
        # al fallar, el cursor ya salta a la línea del error; un clic en el mensaje lo repite desde otra
        verificar("labc: ir a la linea %s" % e in s, "al compilar con error el cursor va a la línea %s" % e)
        time.sleep(1)
        m.captura("H414-3-errores")
        # la fila del primer error: panel de abajo (alto 180, pestañas 20): el panel empieza en h - 20 - 180 + 20
        m.clique(w["x"] + 200, w["y"] + w["a"] - 20 - 180 + 20 + 8)
        esperar(m, r"labc: ir a la linea %s" % e, 20, marca2)
        verificar(True, "un clic en el mensaje lleva el editor a la línea del error")
        cerrar(m, w, marca)
        # 3. límites: sin -b pasa en silencio, con -b se para en su línea
        marca, w = lanzar(m, "%s/09-limites.c" % EJD, lento)
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("f5")
        esperar(m, r"labc: programa terminado st=1 cod=0", 60 * lento, marca)
        s = m.serial()[marca:]
        verificar("labc: salida: puntos[5] = 25" in s and "labc: salida: fin" in s, "sin -b el fallo pasa en silencio")
        marca2 = len(m.serial())
        m.clique(w["x"] + 438 + 6, w["y"] + 8 + 6)           # casilla -b
        time.sleep(0.5)
        m.captura("H414-4-limites-casilla")
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("f5")
        esperar(m, r"labc: programa terminado", 90 * lento, marca2)
        s = m.serial()[marca2:]
        verificar("BCHECK" in s or "RUNTIME ERROR" in s, "con -b se detecta el acceso fuera del array (%r)" % s[-300:])
        verificar(re.search(r"09-limites\.c:1[0-3]", s) is not None or re.search(r"labc: salida:.*:1[0-3]", s) is not None,
                  "con -b el aviso dice la línea del fallo (%r)" % s[-400:])
        verificar("labc: salida: puntos[5] = 25" not in s, "con -b el programa no sigue tras el error")
        time.sleep(1)
        m.captura("H414-5-limites-bcheck")
        cerrar(m, w, marca)
        # 4. entrada de teclado
        marca, w = lanzar(m, "%s/07-entrada.c" % EJD, lento)
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("f5")
        esperar(m, r"labc: salida: Como te llamas\?", 60 * lento, marca) if False else time.sleep(3 * lento)
        m.clique(w["x"] + 300, w["y"] + w["a"] - 20 - 12)   # el cuadro Entrada
        m.digitar("Ana", 0.05)
        m.tecla("ret")
        time.sleep(1)
        m.digitar("11", 0.05)
        m.tecla("ret")
        esperar(m, r"labc: salida: .*Hola Ana, el ano que viene tendras 12", 60 * lento, marca)
        verificar(True, "lo escrito en Entrada llega al programa (scanf)")
        esperar(m, r"labc: programa terminado st=1 cod=0", 30 * lento, marca)
        time.sleep(1)
        m.captura("H414-6-entrada")
        cerrar(m, w, marca)
        # 5. una ventana de la libsagui desde el Laboratorio; Detener mata un programa que no termina
        marca, w = lanzar(m, "%s/08-ventana.c" % EJD, lento)
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("f5")
        esperar(m, r'janela \d+ criada por \d+: "Ventana desde el Laboratorio" 320x200', 60 * lento, marca)
        time.sleep(1.5)
        m.captura("H414-7-ventana-desde-el-laboratorio")
        verificar(True, "el programa del Laboratorio abre una ventana con la libsagui")
        # el botón Detener; el primer clic solo da el foco a la ventana del Laboratorio
        m.clique(w["x"] + 344 + 40, w["y"] + 15)
        time.sleep(1)
        m.clique(w["x"] + 344 + 40, w["y"] + 15)
        esperar(m, r"labc: programa terminado st=2 cod=9", 30 * lento, marca)
        verificar(True, "Detener mata el programa (señal 9)")
        cerrar(m, w, marca)
        # 6. guardar con Ctrl+S: abre un archivo con ruta, escribe y guarda
        m.comando("cp %s/01-hola.c /tmp/guardar.c" % EJD, 30)
        marca, w = lanzar(m, "/tmp/guardar.c", lento)
        m.clique(w["x"] + 400, w["y"] + 200)
        m.tecla("ctrl-end")
        m.digitar("//MARCA_H414", 0.05)
        m.tecla("ctrl-s")
        time.sleep(2 * lento)
        cerrar(m, w, marca)
        marca = len(m.serial())
        m.comando("tail -n 2 /tmp/guardar.c", 30)
        time.sleep(2)
        verificar("MARCA_H414" in m.serial()[marca:], "Ctrl+S guarda el archivo (%r)" % m.serial()[marca:][-200:])
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def main():
    if not os.path.exists(FUENTES_TCC):
        print("AVISO: faltan las fuentes de TCC (tools/tcc_fuentes.sh): h414 se salta")
        return 0
    partes = sys.argv[1:] or ["pc", "saguios"]
    for p in partes:
        {"pc": pc, "saguios": saguios}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
