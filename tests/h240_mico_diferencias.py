#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H240 - Vista de diferencias y botón «Aplicar» en el Asistente del Mico (D240, mejora 043).

 (a) Unidad en el PC de la diferencia por líneas (libsagui/difer.c, Myers):
     tests/difer_unidad.c compara con la secuencia común más larga hallada
     aparte, en casos fijos y 3000 al azar;
 (b) en QEMU, con el servidor falso de la IA devolviendo un bloque de código:
     «Corregir Problemas» -> la propuesta aparece como diferencias (dos
     bloques; rojo con «-», verde con «+»); navegar entre bloques;
     «Aplicar este bloque» cambia solo ese; «Aplicar» cambia el resto; Ctrl+Z
     deshace el «Aplicar» entero de una vez y otro Ctrl+Z el bloque;
 (c) «Generar función» inserta una función; si el archivo cambia entre la
     petición y «Aplicar», avisa y recalcula en vez de aplicar; «Descartar»
     no toca el documento.
El documento se comprueba por el hash FNV-1a que el Mico escribe en el registro.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import servidor_falso_ia as sfi  # noqa: E402
from qemu import Maquina, verificar, SAIDA  # noqa: E402
from servidor_falso_ia import servidor_falso_ia  # noqa: E402
from h111_mico_ia import janela, layout, esperar  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ORIGINAL = """// PROPUESTA-ALFA
int suma(int a, int b)
{
    return a - b;
}

int doble(int x)
{
    return x * 3;
}

int main(void)
{
    return suma(1, 2);
}
"""
FINAL = ORIGINAL.replace("a - b", "a + b").replace("x * 3", "x * 2")
BLOQUE1 = ORIGINAL.replace("a - b", "a + b")
FUNCION = "int triple(int x)\n{\n    return x * 3;\n}\n\n"


ACENTOS = """// PROPUESTA-BETA: contraseña y canción
const char *msg = "Año nuevo, ¿qué tal?";
int cuenta(int n)
{
    return n + 1;   // más uno
}
"""
ACENTOS_FINAL = ACENTOS.replace("n + 1", "n + 2")


def echo_latin1(linea):
    """Una orden echo -e del sagsh que escribe la línea en Latin-1 (con \\xHH)."""
    r = ""
    for c in linea.encode("latin-1"):
        r += chr(c) if 32 <= c < 127 and chr(c) not in "'\\" else "\\x%02x" % c
    return "echo -e '" + r + "' >> /tmp/acc.c"


def fnv(texto):
    h = 2166136261
    for c in texto.encode("latin-1"):
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return "%08x" % h


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "difer_unidad")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DDIFER_PC", "-I", os.path.join(RAIZ, "userland/inc"),
                    "-o", exe, os.path.join(RAIZ, "tests/difer_unidad.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "unidad de la diferencia en el PC: " + r.stdout.strip()[-200:])


def contar(m, caja, colores, nombre):
    ppm = os.path.join(SAIDA, "h240-" + nombre + ".ppm")
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    datos = open(ppm, "rb").read()
    _, ancho, alto, _, _ = datos.split(None, 4)
    ancho, alto = int(ancho), int(alto)
    pix = datos[-ancho * alto * 3:]
    x0, y0, x1, y1 = caja
    n = 0
    for y in range(y0, min(y1, alto)):
        for x in range(x0, min(x1, ancho)):
            i = (y * ancho + x) * 3
            if tuple(pix[i:i + 3]) in colores:
                n += 1
    return n


def parte_acentos(url):
    """El código tiene tildes y eñes en un comentario y en una cadena: la diferencia que vuelve
    (UTF-8 -> Latin-1) solo debe marcar la línea que cambió de verdad."""
    sfi.RESPOSTAS_MARCA[:] = [
        ("PROPUESTA-BETA", "Sumo dos.\n```c\n" + ACENTOS_FINAL + "```\n"),
    ]
    with Maquina("h240b", "ide", extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando(f"export IA_URL={url}", tempo=30)
        m.comando("export IA_MODELO=modelo-de-prova", tempo=30)
        m.comando("rm -f /tmp/acc.c", tempo=30)
        for linea in ACENTOS.rstrip("\n").split("\n"):
            m.comando(echo_latin1(linea), tempo=30)
        marca = len(m.serial())
        m.digitar("mico /tmp/acc.c\n", 0.04)
        esperar(lambda: m.serial()[marca:], "mico: pronto", 30)
        w = janela(m.serial(), "Mico")
        L, A, lat, ex, ey, el, ea, px, py, pl, pa = layout(m.serial(), marca)
        X, Y = w["x"] + px, w["y"] + py
        m.clique(X + 8 + 80 + 4 + 120 + 4 + 112 + 4 + 30, Y + 10)
        time.sleep(0.4)
        d = len(m.serial())
        m.clique(X + 150, Y + pa - 20)
        esperar(lambda: m.serial()[d:], "mico: propuesta 1 bloques (+1 -1)", 40)
        time.sleep(0.4)
        m.captura("H240-mico-diferencias-acentos")
        d = len(m.serial())
        m.clique(X + 8 + 30, Y + pa - 28 + 10)   # «Aplicar»
        esperar(lambda: m.serial()[d:], f"propuesta aplicada entera, documento {len(ACENTOS_FINAL)} bytes hash {fnv(ACENTOS_FINAL)}", 30)
        verificar(True, "con tildes y eñes solo cambió la línea modificada (1 bloque, hash exacto en Latin-1)")


def main():
    unidad()
    sfi.RESPOSTAS_MARCA[:] = [
        ("PROPUESTA-ALFA", "Cambio la resta por una suma y el triple por el doble.\n```c\n" + FINAL + "```\nListo.\n"),
        ("Genera una", "Aquí tienes la función.\n```c\n" + FUNCION.rstrip("\n") + "\n```\n"),
    ]
    srv = servidor_falso_ia().arrancar()
    url = f"http://10.0.2.2:{srv.porta}/v1"
    try:
        with Maquina("h240", "ide", extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(1)
            m.comando(f"export IA_URL={url}", tempo=30)
            m.comando("export IA_MODELO=modelo-de-prova", tempo=30)
            m.comando("rm -f /tmp/dif.c", tempo=30)
            for linea in ORIGINAL.rstrip("\n").split("\n"):
                m.comando("echo '" + linea.replace("'", "") + "' >> /tmp/dif.c", tempo=30)
            marca = len(m.serial())
            m.digitar("mico /tmp/dif.c\n", 0.04)
            esperar(lambda: m.serial()[marca:], "mico: pronto", 30)
            w = janela(m.serial(), "Mico")
            L, A, lat, ex, ey, el, ea, px, py, pl, pa = layout(m.serial(), marca)
            X, Y = w["x"] + px, w["y"] + py
            m.clique(X + 8 + 80 + 4 + 120 + 4 + 112 + 4 + 30, Y + 10)   # pestaña «Asistente»
            time.sleep(0.4)

            def serie():
                return m.serial()

            def esperar_nuevo(texto, desde, t=30):
                return esperar(lambda: serie()[desde:], texto, t)

            # --- Corregir Problemas -> propuesta con dos bloques
            d = len(serie())
            m.clique(X + 150, Y + pa - 20)
            esperar_nuevo("mico: propuesta 2 bloques (+2 -2)", d, 40)
            time.sleep(0.5)
            area = (X + 8, Y + 22, X + pl - 8, Y + pa - 60)
            rojo = contar(m, area, {(0xFF, 0xDD, 0xDD), (0x5A, 0x26, 0x26)}, "rojo")
            verde = contar(m, area, {(0xDD, 0xF5, 0xDD), (0x1F, 0x4A, 0x2A)}, "verde")
            verificar(rojo > 500 and verde > 500, f"la vista muestra líneas quitadas en rojo ({rojo} px) y añadidas en verde ({verde} px)")
            m.captura("H240-mico-diferencias")

            # Botones: «Aplicar» «Aplicar este bloque» «Descartar» «< Anterior» «Siguiente >»
            by = Y + pa - 28 + 10
            bx_todos = X + 8 + 30
            bx_este = X + 8 + 72 + 6 + 60
            bx_desc = X + 8 + 72 + 6 + 168 + 6 + 40
            bx_ant = bx_desc + 40 + 6 + 40
            bx_sig = bx_ant + 40 + 6 + 50
            d = len(serie())
            m.clique(bx_sig, by)
            esperar_nuevo("propuesta bloque 2 de 2", d)
            d = len(serie())
            m.clique(bx_ant, by)
            esperar_nuevo("propuesta bloque 1 de 2", d)

            # --- un bloque
            d = len(serie())
            m.clique(bx_este, by)
            s = esperar_nuevo(f"propuesta bloque aplicado, documento {len(BLOQUE1)} bytes hash {fnv(BLOQUE1)}", d)
            verificar("propuesta quedan 1 bloques" in serie()[d:], "tras aplicar un bloque queda otro")
            time.sleep(0.4)
            m.captura("H240-mico-un-bloque-aplicado")

            # --- todo lo que queda
            d = len(serie())
            m.clique(bx_todos, by)
            esperar_nuevo(f"propuesta aplicada entera, documento {len(FINAL)} bytes hash {fnv(FINAL)}", d)
            time.sleep(0.4)

            # --- deshacer: el «Aplicar» entero es un paso; el bloque, otro
            m.clique(X - 20 + 40, w["y"] + ey + 30)   # el editor (da foco)
            time.sleep(0.2)
            d = len(serie())
            m.tecla("ctrl-z", 0.2)
            esperar_nuevo(f"deshacer, documento {len(BLOQUE1)} bytes hash {fnv(BLOQUE1)}", d)
            d = len(serie())
            m.tecla("ctrl-z", 0.2)
            esperar_nuevo(f"deshacer, documento {len(ORIGINAL)} bytes hash {fnv(ORIGINAL)}", d)

            # --- Generar función; el archivo cambia antes de Aplicar
            m.tecla("ctrl-home", 0.2)
            m.clique(X + 8 + 80 + 4 + 120 + 4 + 112 + 4 + 30, Y + 10)
            time.sleep(0.3)
            d = len(serie())
            m.clique(X + 350, Y + pa - 20)
            esperar_nuevo("mico: propuesta 1 bloques (+4 -0)", d, 40)
            time.sleep(0.4)
            m.captura("H240-mico-generar-funcion")
            m.clique(X - 20 + 40, w["y"] + ey + 30)
            m.tecla("ctrl-end", 0.2)
            m.digitar("z", 0.1)
            d = len(serie())
            m.clique(bx_todos, by)
            esperar_nuevo("propuesta recalculada, el archivo cambió", d)
            verificar("propuesta aplicada" not in serie()[d:], "el archivo cambió: no se aplicó, se recalculó")
            d = len(serie())
            m.clique(bx_todos, by)
            esperar_nuevo("propuesta aplicada entera", d)
            esperar_nuevo(f"hash {fnv(FUNCION[:-1] + ORIGINAL + 'z')}", d)

            # --- Descartar no toca el documento
            m.tecla("ctrl-home", 0.2)
            m.clique(X + 8 + 80 + 4 + 120 + 4 + 112 + 4 + 30, Y + 10)
            time.sleep(0.3)
            d = len(serie())
            m.clique(X + 150, Y + pa - 20)
            esperar_nuevo("mico: propuesta ", d, 40)
            time.sleep(0.4)
            d = len(serie())
            m.clique(bx_desc, by)
            esperar_nuevo("propuesta descartada", d)
            verificar("propuesta aplicada" not in serie()[d:] and "propuesta bloque aplicado" not in serie()[d:],
                      "Descartar no aplica nada")
            verificar("[SAGUI] EXCEPCION" not in serie()[marca:], "el kernel no registró excepciones")
        parte_acentos(url)
    finally:
        srv.parar()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as e:
        print("FALLO:", e)
        sys.exit(1)
