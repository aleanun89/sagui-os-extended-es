#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H459 - Mico, Pluma y Macaco portables: la capa «libsagui sobre SDL 2» (D459, docs/apps-portables.md).

Sin QEMU, en el anfitrión: compila las tres aplicaciones para Linux con la SDL 2 del PC
(`make -f userland/portable/Makefile`) a partir de las MISMAS fuentes que SaguiOS, las arranca con
SDL_VIDEODRIVER=dummy y un guion de eventos (SAGUI_ROTEIRO), abre un documento de ejemplo, edita, guarda y
compara lo guardado:

  - Mico: abre un .txt, Ctrl+Fin, teclea, Ctrl+S; copiar y pegar por el portapapeles de SDL (Latin-1 <-> UTF-8);
    el menú contextual (clic derecho) se dibuja y responde; el modo -t de texto.
  - Pluma: .txt -> .rtf -> .pdf por línea de órdenes (con pdf_validar); abre el .rtf, teclea, Ctrl+S y el texto
    nuevo está en el archivo; el diálogo de abrir de la libsagui se abre y se cierra con Esc.
  - Macaco: .csv -> .xlsx -> .csv de ida y vuelta; abre un .mxs, escribe una celda con una fórmula, Ctrl+S y
    comprueba el archivo; capturas.

Las capturas salen del búfer de píxeles de la ventana (sintéticas: datos del propio guion), en
SAIDA/h459/; con --docs se copian a docs/capturas/H459-*.png.

Uso: h459_apps_portables.py [--docs]
"""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import verificar, RAIZ, SAIDA, CAPTURAS  # noqa: E402

DIR = os.path.join(SAIDA, "h459")
B = os.path.join(DIR, "build")
TRAB = "/tmp/h459-ejemplo"   # ruta neutra: las capturas muestran la ruta del archivo abierto
DATOS = os.path.join(DIR, "datos")


def compilar():
    jobs = os.environ.get("SAGUI_JOBS", "4")
    env = dict(os.environ)
    env.pop("CPATH", None)   # D191: la glibc del anfitrión de env.sh no es para esto (aquí sí es el anfitrión, pero limpio)
    r = subprocess.run(["make", "-f", os.path.join(RAIZ, "userland", "portable", "Makefile"), "-j" + jobs, "B=" + B],
                       cwd=RAIZ, capture_output=True, text=True, env=env)
    verificar(r.returncode == 0, "las tres aplicaciones compilan para Linux con SDL 2: " + r.stderr[-600:])
    for a in ("mico", "pluma", "macaco"):
        verificar(os.path.exists(os.path.join(B, a)), "binario " + a)


def correr(app, args, guion, extra_env=None, timeout=60):
    """Arranca la aplicación con el guion; devuelve (código, salida)."""
    g = os.path.join(TRAB, "guion-%s.txt" % app)
    with open(g, "w", encoding="utf-8") as f:
        f.write("\n".join(guion) + "\n")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", SAGUI_ROTEIRO=g, SAGUI_DATOS=DATOS,
               HOME=os.path.join(TRAB, "casa"), LANG="es_ES.UTF-8")
    env.update(extra_env or {})
    r = subprocess.run([os.path.join(B, app)] + args, capture_output=True, text=True, env=env, timeout=timeout, cwd=TRAB)
    return r.returncode, r.stdout + r.stderr


def png_valido(ruta, ancho=None):
    d = open(ruta, "rb").read()
    verificar(d[:8] == b"\x89PNG\r\n\x1a\n", "captura PNG válida " + os.path.basename(ruta))
    if ancho:
        verificar(int.from_bytes(d[16:20], "big") == ancho, "ancho de %s = %d" % (os.path.basename(ruta), ancho))
    # no es una imagen en blanco: el PNG sin comprimir de la libsagui tiene más de 1 color
    verificar(len(set(d[100:4000])) > 4, "captura con contenido " + os.path.basename(ruta))


def leer(p):
    return open(p, "rb").read().decode("latin-1")


def prueba_mico():
    f = os.path.join(TRAB, "mico.txt")
    open(f, "w", encoding="latin-1").write("linea uno\n")
    rc, _ = correr("mico", [f], [
        "esperar 300", "tecla 0x105 2", "texto línea dos con ñ\\n", "ctrl s", "esperar 200",
        "captura " + os.path.join(DIR, "H459-mico.png"), "sair"])
    verificar(rc == 0, "Mico termina bien (rc=%d)" % rc)
    verificar(leer(f) == "linea uno\nlínea dos con ñ\n", "Mico guarda lo tecleado (con acentos en Latin-1): %r" % leer(f))
    png_valido(os.path.join(DIR, "H459-mico.png"), 960)

    # portapapeles: copiar todo y pegarlo al final
    f2 = os.path.join(TRAB, "mico-clip.txt")
    open(f2, "w", encoding="latin-1").write("abc ñ\n")
    rc, _ = correr("mico", [f2], ["esperar 300", "ctrl a", "ctrl c", "tecla 0x105 2", "ctrl v", "ctrl s", "esperar 200", "sair"])
    verificar(rc == 0 and leer(f2) == "abc ñ\nabc ñ\n", "Mico copia y pega por el portapapeles de SDL: %r" % leer(f2))

    # menú contextual: se dibuja, se elige «Seleccionar todo» y lo siguiente que se teclea lo reemplaza
    f3 = os.path.join(TRAB, "mico-menu.txt")
    open(f3, "w", encoding="latin-1").write("uno\n")
    rc, _ = correr("mico", [f3], [
        "esperar 300", "clique 500 200 2", "esperar 200", "mover 560 279",
        "captura " + os.path.join(DIR, "H459-mico-menu.png"), "clique 560 279", "texto X", "ctrl s", "esperar 200", "sair"])
    verificar(rc == 0 and leer(f3) == "X", "el menú contextual de la capa responde (Seleccionar todo + teclear): %r" % leer(f3))
    png_valido(os.path.join(DIR, "H459-mico-menu.png"), 960)

    r = subprocess.run([os.path.join(B, "mico"), "-t", f], capture_output=True, text=True,
                       env=dict(os.environ, SDL_VIDEODRIVER="dummy", SAGUI_DATOS=DATOS, HOME=os.path.join(TRAB, "casa")))
    verificar(r.returncode == 0, "mico -t (modo de texto sin ventana) rc=%d" % r.returncode)


def prueba_pluma():
    txt = os.path.join(TRAB, "pluma.txt")
    open(txt, "w", encoding="latin-1").write("Primera línea del documento.\n")
    rtf = os.path.join(TRAB, "pluma.rtf")
    pdf = os.path.join(TRAB, "pluma.pdf")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SAGUI_DATOS=DATOS, HOME=os.path.join(TRAB, "casa"))
    r = subprocess.run([os.path.join(B, "pluma"), "--convertir", txt, rtf], capture_output=True, text=True, env=env)
    verificar(r.returncode == 0 and "rc=0" in r.stdout, "pluma --convertir txt -> rtf: " + r.stdout + r.stderr)
    verificar("Primera l" in leer(rtf), "el .rtf lleva el texto")
    r = subprocess.run([os.path.join(B, "pluma"), "--convertir", rtf, pdf], capture_output=True, text=True, env=env)
    verificar(r.returncode == 0, "pluma --convertir rtf -> pdf: " + r.stdout + r.stderr)
    verificar(open(pdf, "rb").read(5) == b"%PDF-", "el PDF empieza por %PDF-")
    import pdf_validar  # noqa: E402
    info = pdf_validar.analizar(open(pdf, "rb").read())   # lanza AssertionError si el PDF esta mal formado
    verificar(info["paginas"] >= 1, "pdf_validar acepta el PDF de Pluma (%s paginas)" % info["paginas"])
    rc, _ = correr("pluma", [rtf], [
        "esperar 300", "texto Hola portable. ", "ctrl s", "esperar 300",
        "captura " + os.path.join(DIR, "H459-pluma.png"),
        "ctrl o", "esperar 400", "captura " + os.path.join(DIR, "H459-pluma-abrir.png"), "tecla 0x116", "esperar 200", "sair"])
    verificar(rc == 0, "Pluma termina bien (rc=%d)" % rc)
    verificar("Hola portable. Primera" in leer(rtf), "Pluma guarda en el .rtf lo que se tecleó")
    png_valido(os.path.join(DIR, "H459-pluma.png"), 820)
    png_valido(os.path.join(DIR, "H459-pluma-abrir.png"))


def prueba_macaco():
    csv = os.path.join(TRAB, "macaco.csv")
    open(csv, "w", encoding="latin-1").write("a;b;c\n1;2;3\n")
    xlsx = os.path.join(TRAB, "macaco.xlsx")
    csv2 = os.path.join(TRAB, "macaco-ida-vuelta.csv")
    mxs = os.path.join(TRAB, "macaco.mxs")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SAGUI_DATOS=DATOS, HOME=os.path.join(TRAB, "casa"))
    for a, b in ((csv, xlsx), (xlsx, csv2), (csv, mxs)):
        r = subprocess.run([os.path.join(B, "macaco"), "--convertir", a, b], capture_output=True, text=True, env=env)
        verificar(r.returncode == 0 and os.path.exists(b), "macaco --convertir %s -> %s: %s" % (os.path.basename(a), os.path.basename(b), r.stderr))
    verificar(leer(csv2).replace("\r", "") == "a,b,c\n1,2,3\n", "csv -> xlsx -> csv conserva las celdas: %r" % leer(csv2))
    rc, _ = correr("macaco", [mxs], [
        "esperar 300", "texto =B2+C2*10\\n", "ctrl s", "esperar 300",
        "captura " + os.path.join(DIR, "H459-macaco.png"), "sair"])
    verificar(rc == 0, "Macaco termina bien (rc=%d)" % rc)
    m = leer(mxs)
    verificar("celda A1 =B2+C2*10" in m, "Macaco guarda la fórmula escrita en A1: %r" % m[:200])
    r = subprocess.run([os.path.join(B, "macaco"), "--convertir", mxs, csv2], capture_output=True, text=True, env=env)
    verificar("32" in leer(csv2), "la fórmula se calcula al exportar (2 + 3*10 = 32): %r" % leer(csv2))
    png_valido(os.path.join(DIR, "H459-macaco.png"), 900)


def main():
    shutil.rmtree(TRAB, ignore_errors=True)
    os.makedirs(os.path.join(TRAB, "casa"), exist_ok=True)
    os.makedirs(DIR, exist_ok=True)
    # datos: las fuentes del sistema (DejaVu) que Pluma y Macaco leen de /fontes
    shutil.rmtree(DATOS, ignore_errors=True)
    os.makedirs(DATOS)
    os.symlink(os.path.join(RAIZ, "rootfs", "fontes"), os.path.join(DATOS, "fontes"))
    verificar(shutil.which("sdl2-config") is not None, "SDL 2 de desarrollo instalada (sdl2-config)")
    compilar()
    prueba_mico()
    prueba_pluma()
    prueba_macaco()
    if "--docs" in sys.argv:
        for n in ("H459-mico", "H459-mico-menu", "H459-pluma", "H459-pluma-abrir", "H459-macaco"):
            shutil.copy(os.path.join(DIR, n + ".png"), os.path.join(CAPTURAS, n + ".png"))
    print("H459 OK")


if __name__ == "__main__":
    main()
