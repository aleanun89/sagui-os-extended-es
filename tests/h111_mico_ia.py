#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H111 - Asistente IA dentro del Mico (mejora 043).

Inicia un servidor compatible con OpenAI en el anfitrión, abre el Mico,
selecciona el panel Asistente y comprueba la respuesta en streaming, en la
pantalla y en el registro. No requiere Internet ni una clave real.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA  # noqa: E402
from servidor_falso_ia import servidor_falso_ia  # noqa: E402

RESPOSTA = "Hola, isto e' uma resposta de exemplo do servidor falso."


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por (\d+): "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[2]), a=int(g[3]), x=int(g[4]) + 2, y=int(g[5]) + 24)


def layout(serial, desde):
    g = re.findall(r"mico: layout (\d+)x(\d+), lateral (\d+), editor (\d+),(\d+) (\d+)x(\d+), panel (\d+),(\d+) (\d+)x(\d+)", serial[desde:])[-1]
    return tuple(int(v) for v in g)


def esperar(serial_fn, texto, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        s = serial_fn()
        if texto in s:
            return s
        time.sleep(0.1)
    raise AssertionError(f"tiempo agotado esperando {texto!r}")


def contar_color(m, x0, y0, x1, y1, color):
    ppm = os.path.join(SAIDA, "h111-ide-tela.ppm")
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    with open(ppm, "rb") as f:
        datos = f.read()
    _, ancho, alto, _, _ = datos.split(None, 4)
    ancho, alto = int(ancho), int(alto)
    pixeles = datos[-ancho * alto * 3:]
    n = 0
    for y in range(y0, min(y1, alto)):
        for x in range(x0, min(x1, ancho)):
            i = (y * ancho + x) * 3
            if tuple(pixeles[i:i + 3]) == color:
                n += 1
    return n


def main():
    srv = servidor_falso_ia().arrancar()
    url = f"http://10.0.2.2:{srv.porta}/v1"
    try:
        with Maquina("h111", "ide", extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(1)
            m.comando(f"export IA_URL={url}", tempo=30)
            m.comando("export IA_MODELO=modelo-de-prova", tempo=30)
            marca = len(m.serial())
            m.digitar("mico /ejemplos/mico\n", 0.04)
            esperar(lambda: m.serial()[marca:], "mico: pronto", 30)
            w = janela(m.serial(), "Mico")
            L, A, lat, ex, ey, el, ea, px, py, pl, pa = layout(m.serial(), marca)
            verificar("\"Asistente\"" not in m.serial()[marca:], "la ventana Mico queda abierta para usar el panel integrado")

            # La cuarta pestaña empieza tras Terminal, Problemas (0) y Búsqueda (0).
            x_asistente = w["x"] + px + 8 + 80 + 4 + 120 + 4 + 112 + 4 + 30
            y_pestanas = w["y"] + py + 10
            m.clique(x_asistente, y_pestanas)
            time.sleep(0.4)
            subrayado = contar_color(m, w["x"] + px + 328, w["y"] + py + 19,
                                    w["x"] + px + 420, w["y"] + py + 22, (11, 122, 62))
            verificar(subrayado >= 100, f"la pestaña Asistente está activa en el panel inferior ({subrayado} px verdes)")

            # El campo del panel está a 58 píxeles del borde inferior de su área.
            x_campo = w["x"] + px + 100
            y_campo = w["y"] + py + pa - 58 + 10
            m.clique(x_campo, y_campo)
            m.digitar("Explica esta funcion LENTO", 0.03)
            desde = len(m.serial())
            m.tecla("ret", 0.08)

            # El servidor envía un fragmento primero y los siguientes cada 0,4 s.
            time.sleep(0.25)
            parcial = contar_color(m, w["x"] + px + 8, w["y"] + py + 22,
                                   w["x"] + px + pl - 8, w["y"] + py + pa - 60, (30, 35, 40))
            verificar(parcial > 10, f"la primera parte de la respuesta ya aparece durante el streaming ({parcial} px)")
            esperar(lambda: m.serial()[desde:], "mico: asistente respuesta", 20)
            time.sleep(0.3)
            tinta = contar_color(m, w["x"] + px + 8, w["y"] + py + 22,
                                  w["x"] + px + pl - 8, w["y"] + py + pa - 60, (30, 35, 40))
            verificar(tinta > 100, f"la respuesta completa aparece en pantalla ({tinta} px de texto)")
            verificar(RESPOSTA in m.serial()[desde:], "el registro confirma que el Mico recibió la respuesta")
            verificar("[SAGUI] EXCEPCION" not in m.serial()[desde:], "el kernel no registró excepciones")
            m.captura("H111-mico-asistente-ia", guardar=False)
    finally:
        srv.parar()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as e:
        print("FALLO:", e)
        sys.exit(1)
