#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H153 - el navegador NetSurf (D184): abre una página local y otra por HTTP.

Criterio:
  - /apps/bin/netsurf está en la imagen, en la partición SAGUI-APPS (D194; si no se prepararon las fuentes con
    tools/netsurf_fuentes.sh, la prueba lo dice y sale con 77: no aplica);
  - abre file:///usb/prueba.html (tests/datos/netsurf/, en un pendrive) y en
    la ventana aparecen la caja roja y la verde de estilo.css, el PNG azul y
    el JPEG naranja (decodificados por la libsagui);
  - escribiendo en la barra de direcciones la misma página por HTTP
    (http://10.0.2.2:<puerto>/prueba.html, un servidor en el anfitrión), el
    fetcher propio la baja (con su CSS y sus imágenes) y la dibuja igual;
  - un clic en el enlace lleva a la segunda página (la caja amarilla);
  - sin excepciones del kernel ni faltas de página.
Capturas en docs/capturas/H153-*.png.
"""
import functools
import http.server
import os
import re
import sys
import threading
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, criar_pendrive, ler_ppm, opcoes_apps, SAIDA, BUILD  # noqa: E402
from h60_barra_tareas import esperar, janela  # noqa: E402
import h136_opentyrian as h136  # noqa: E402

DATOS = os.path.join(RAIZ, "tests", "datos", "netsurf")

ROJO = (0xE0, 0x10, 0x10)
VERDE = (0x10, 0xC0, 0x20)
AZUL_PNG = (0x20, 0x40, 0xE0)
NARANJA_JPG = (0xE0, 0xA0, 0x00)
AMARILLO = (0xF0, 0xD0, 0x00)


class Silencioso(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def pantalla(m, nome):
    ppm = os.path.join(SAIDA, "h153-" + nome + ".ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


def contar(tela, j, cor, tol=24):
    """Píxeles de la ventana j con el color dado (con tolerancia: el JPEG no
    da el color exacto). Devuelve (cuántos, x mínimo, y mínimo)."""
    l, a, px = tela
    n, xmin, ymin = 0, 99999, 99999
    for y in range(j["y"], min(a, j["y"] + j["a"])):
        base = y * l * 3
        for x in range(j["x"], min(l, j["x"] + j["l"])):
            i = base + 3 * x
            if (abs(px[i] - cor[0]) <= tol and abs(px[i + 1] - cor[1]) <= tol
                    and abs(px[i + 2] - cor[2]) <= tol):
                n += 1
                xmin = min(xmin, x)
                ymin = min(ymin, y)
    return n, xmin, ymin


def esperar_cores(m, j, cores, nome, tempo=90):
    """Espera a que estén todos los colores (al menos minimo píxeles cada uno)."""
    fim = time.time() + tempo
    while True:
        tela = pantalla(m, nome)
        res = {c: contar(tela, j, cor) for c, (cor, _) in cores.items()}
        if all(res[c][0] >= minimo for c, (_, minimo) in cores.items()) or time.time() > fim:
            return res
        time.sleep(3)


def sem_falhas(s, quando):
    verificar("Fallo de página" not in s, f"{quando}: sin faltas de página")
    verificar("EXCECAO" not in s.upper() and "excepción" not in s.lower(), f"{quando}: sin excepciones del kernel")
    verificar("netsurf: abort" not in s and "assert falló" not in s, f"{quando}: NetSurf no aborta")


def main():
    if not os.path.exists(os.path.join(BUILD, "userland", "bin", "netsurf")):
        print("H153: /bin/netsurf no está compilado (falta tools/netsurf_fuentes.sh); no aplica")
        return 77
    arquivos = {n: open(os.path.join(DATOS, n), "rb").read() for n in sorted(os.listdir(DATOS))}
    pen = criar_pendrive("h153-netsurf", arquivos=arquivos)
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0),
                                               functools.partial(Silencioso, directory=DATOS))
    porta = servidor.server_address[1]
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    imagem = imagem_de_teste("", ["--vesa"] + opcoes_apps())   # D194: NetSurf está en /apps
    extras = ["-nic", "user,model=e1000",
              "-device", "usb-ehci,id=ehci",
              "-drive", "if=none,id=pen,format=raw,file=" + pen,
              "-device", "usb-storage,bus=ehci.0,drive=pen,removable=on,id=dev-pen"]
    try:
        with Maquina("h153", "ide", imagem=imagem, extras=extras) as m:
            esperar(m, r"\[DISCO\] usb0: ", 60)
            esperar(m, r"sagui:/\$", 120)
            time.sleep(4)
            h136.focar_terminal(m)
            desde = len(m.serial())
            m.digitar("montar usb0")
            m.tecla("ret")
            esperar(m, r"\[MONTAGEM\] usb0.*montado em /usb", 40, desde=desde)

            # ---- 1: la página local (file:)
            marca = len(m.serial())
            m.digitar("netsurf file:///usb/prueba.html")
            m.tecla("ret")
            esperar(m, r'janela \d+ criada por \d+: "NetSurf"', 60, desde=marca)
            j = janela(m.serial()[marca:], "NetSurf")
            res = esperar_cores(m, j, {"rojo": (ROJO, 200 * 80 // 2), "verde": (VERDE, 300 * 60 // 2),
                                       "png": (AZUL_PNG, 64 * 64 // 2), "jpg": (NARANJA_JPG, 64 * 64 // 2)},
                                "file")
            m.captura("H153-1-pagina-local")
            print("H153: file:", {c: v[0] for c, v in res.items()})
            verificar(res["rojo"][0] >= 200 * 80 // 2, "file: la caja roja de estilo.css se dibuja (%d px)" % res["rojo"][0])
            verificar(res["verde"][0] >= 300 * 60 // 2, "file: la caja verde se dibuja (%d px)" % res["verde"][0])
            verificar(res["verde"][1] > res["rojo"][1] + 30 and res["verde"][2] > res["rojo"][2],
                      "file: la verde va debajo de la roja y con margen a la izquierda (CSS)")
            verificar(res["png"][0] >= 64 * 64 // 2, "file: el PNG se decodifica y se dibuja (%d px)" % res["png"][0])
            verificar(res["jpg"][0] >= 64 * 64 // 2, "file: el JPEG se decodifica y se dibuja (%d px)" % res["jpg"][0])
            sem_falhas(m.serial()[marca:], "file")

            # ---- 2: la misma página por HTTP, escrita en la barra de direcciones
            marca = len(m.serial())
            # La barra de direcciones está arriba, a la derecha de los botones
            # (j es el marco: la barra de título ocupa los primeros ~25 px).
            m.clique(j["x"] + j["l"] // 2, j["y"] + 40)
            time.sleep(0.5)
            m.tecla("ctrl-u")   # en la caja de texto de fbtk, Ctrl+U la vacía
            m.digitar(f"http://10.0.2.2:{porta}/prueba.html")
            m.tecla("ret")
            esperar(m, r"\[NETSURF\] http: respuesta 200", 90, desde=marca)
            time.sleep(3)
            res = esperar_cores(m, j, {"rojo": (ROJO, 200 * 80 // 2), "verde": (VERDE, 300 * 60 // 2),
                                       "png": (AZUL_PNG, 64 * 64 // 2), "jpg": (NARANJA_JPG, 64 * 64 // 2)},
                                "http")
            m.captura("H153-2-pagina-http")
            s = m.serial()[marca:]
            print("H153: http:", {c: v[0] for c, v in res.items()})
            verificar(f"http://10.0.2.2:{porta}/estilo.css" in s, "http: baja la hoja de estilo por el fetcher propio")
            verificar(f"http://10.0.2.2:{porta}/cuadro.png" in s, "http: baja el PNG")
            verificar(res["rojo"][0] >= 200 * 80 // 2 and res["verde"][0] >= 300 * 60 // 2,
                      "http: las cajas de estilo.css se dibujan")
            verificar(res["png"][0] >= 64 * 64 // 2 and res["jpg"][0] >= 64 * 64 // 2, "http: las imágenes se dibujan")
            sem_falhas(s, "http")

            # ---- 3: un clic en el enlace lleva a la segunda página
            marca = len(m.serial())
            tela = pantalla(m, "enlace")
            # El enlace es el texto azul (#0000ee o parecido) más bajo de la ventana.
            l, a, px = tela
            alvo = None
            for y in range(j["y"] + j["a"] - 30, j["y"] + 40, -1):
                for x in range(j["x"] + 5, j["x"] + 300):
                    i = (y * l + x) * 3
                    if px[i] < 60 and px[i + 1] < 60 and px[i + 2] > 180:
                        alvo = (x + 3, y)
                        break
                if alvo:
                    break
            verificar(alvo is not None, "se encuentra el enlace en la captura")
            m.clique(*alvo)
            esperar(m, r"segunda\.html", 60, desde=marca)
            res = esperar_cores(m, j, {"amarillo": (AMARILLO, 250 * 100 // 2)}, "segunda")
            m.captura("H153-3-segunda-pagina")
            verificar(res["amarillo"][0] >= 250 * 100 // 2,
                      "el clic en el enlace abre la segunda página (%d px amarillos)" % res["amarillo"][0])
            sem_falhas(m.serial()[marca:], "enlace")
    finally:
        servidor.shutdown()
    print("H153: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
