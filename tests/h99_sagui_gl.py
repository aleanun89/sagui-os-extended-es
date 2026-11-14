#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H99 (D127) - SaguiGL: la galería 3D se compila dentro del SaguiOS con el cc.

Criterio:
  - el cc compila /ejemplos/sagui_gl/sagui_gl.c sin errores: el motor entero
    (userland/libsgl/sgl.c) va dentro del ejemplo, con la aritmética entera del
    rasterizador en coma flotante, porque el cc no tiene enteros de 64 bits;
  - el programa compilado con el cc dibuja las cuatro escenas y da exactamente
    las mismas sumas FNV, los mismos píxeles dibujados y los mismos colores
    que el mismo ejemplo compilado con el GCC en el anfitrión (el motor es
    determinista);
  - dos ejecuciones seguidas dan la misma suma (el motor no cambia con el
    tiempo);
  - en el escritorio el ejemplo abre su ventana y la escena 3D se ve en la
    pantalla (captura en docs/capturas/).
"""
import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, SAIDA, RAIZ  # noqa: E402

EXEMPLO = os.path.join(RAIZ, "rootfs", "ejemplos", "sagui_gl", "sagui_gl.c")
LIBSGL = os.path.join(RAIZ, "userland", "libsgl")
# Las mismas banderas con las que se compila el motor para el SaguiOS
# (Makefile: SGL_FLAGS), para que el resultado sea el mismo.
FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno"]
CENAS = (1, 2, 3, 4)
# El fondo de la escena (glClearColor(0.07, 0.09, 0.14)).
FUNDO = (18, 23, 36)


def referencia_anfitriao():
    """Compila el ejemplo con el GCC y devuelve {cena: (soma, pixels, cores)}."""
    exe = os.path.join(tempfile.gettempdir(), "sagui_gl_anfitriao")
    subprocess.run(["gcc"] + FLAGS + ["-w", "-DSAGUI_ANFITRIAO", "-I" + LIBSGL, "-o", exe, EXEMPLO],
                   check=True)
    r = {}
    for cena in CENAS:
        out = subprocess.run([exe, "--soma", "3", str(cena)], check=True, capture_output=True, text=True).stdout
        m = re.search(r"soma ([0-9a-f]{8}) pixels (\d+) cores (\d+)", out)
        verificar(m, f"el anfitrión: salida de la escena {cena}")
        r[cena] = (m.group(1), int(m.group(2)), int(m.group(3)))
    return r


def leitura(dump):
    """Devuelve (anchura, altura, píxeles) de un PPM del screendump."""
    with open(dump, "rb") as f:
        datos = f.read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", datos)
    w, h = int(m.group(1)), int(m.group(2))
    return w, h, datos[m.end():]


def captura_bruta(m, nome):
    """Screendump a un PPM (sin convertir) y devuelve los píxeles."""
    ppm = os.path.join(SAIDA, f"h99-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    w, h, px = leitura(ppm)
    os.remove(ppm)
    return w, h, px


def main():
    ref = referencia_anfitriao()
    print("  (anfitrión, GCC:", ", ".join(f"{c}:{ref[c][0]}" for c in CENAS), ")")

    # ---- el cc compila el ejemplo y da las mismas sumas
    with Maquina("h99", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("cc /ejemplos/sagui_gl/sagui_gl.c -o /tmp/sagui_gl", 240)
        verificar("erro" not in out.lower(), f"el cc compila el ejemplo sin errores ({out.strip()[-120:]})")
        verificar("codigo 0" in out, "el cc termina con código 0")
        out = m.comando("ls -l /tmp/sagui_gl", 30)
        verificar(re.search(r"\d+\s+/tmp/sagui_gl", out), "el cc deja el ejecutable en /tmp/sagui_gl")
        for cena in CENAS:
            out = m.comando(f"/tmp/sagui_gl --soma 3 {cena}", 120)
            g = re.search(r"soma ([0-9a-f]{8}) pixels (\d+) cores (\d+)", out)
            verificar(g, f"la escena {cena} se dibuja y escribe su suma")
            nosso = (g.group(1), int(g.group(2)), int(g.group(3)))
            verificar(nosso == ref[cena], f"escena {cena}: el cc da {nosso} y el GCC {ref[cena]}")
        # El motor es determinista: dos ejecuciones, la misma suma.
        out1 = m.comando("/tmp/sagui_gl --soma 5 4", 120)
        out2 = m.comando("/tmp/sagui_gl --soma 5 4", 120)
        s1 = re.search(r"soma ([0-9a-f]{8})", out1)
        s2 = re.search(r"soma ([0-9a-f]{8})", out2)
        verificar(s1 and s2 and s1.group(1) == s2.group(1),
                  "dos ejecuciones del motor dan la misma suma")

    # ---- en el escritorio el ejemplo abre su ventana y se ve la escena
    with Maquina("h99b", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        w, h, antes = captura_bruta(m, "antes")
        fundo_antes = sum(1 for i in range(0, len(antes), 3)
                           if (antes[i], antes[i + 1], antes[i + 2]) == FUNDO)
        out = m.comando("cc /ejemplos/sagui_gl/sagui_gl.c -o /tmp/sagui_gl", 240)
        verificar("erro" not in out.lower(), "el cc compila el ejemplo en el escritorio")
        out = m.comando("/tmp/sagui_gl &", 60)
        time.sleep(4)
        w2, h2, depois = captura_bruta(m, "depois")
        mudou = sum(1 for i in range(0, min(len(antes), len(depois)), 3)
                     if antes[i:i + 3] != depois[i:i + 3])
        fundo_depois = sum(1 for i in range(0, len(depois), 3)
                            if (depois[i], depois[i + 1], depois[i + 2]) == FUNDO)
        verificar(mudou > 50000, f"la ventana aparece en la pantalla ({mudou} píxeles cambiados)")
        verificar(fundo_depois - fundo_antes > 50000,
                  f"se ve el fondo de la escena 3D ({fundo_depois - fundo_antes} píxeles)")
        m.captura("H99-sagui-gl")
        # El ejemplo sigue corriendo: la máquina se descarta al terminar la prueba.
    print("H99 OK")
    return 0


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # noqa: BLE001
        print("FALLO:", e)
        sys.exit(1)
