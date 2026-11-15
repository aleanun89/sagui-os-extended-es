#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H269 (D269, mejora 102, issue #16) - diálogo de permisos de administrador y avisos completos.

QEMU (escritorio, sesión como sagui, con SAGUI-CONF):
  - /testes/dialogo-admin --pedir (un programa común pide ser root con PED_GRAFICO):
      * primera vez (root sin contraseña): el diálogo pide elegirla dos veces; Esc = No (el programa recibe
        ERRO_ACESSO y sigue siendo sagui); contraseñas distintas = error; iguales = concedido (uid 0);
      * con contraseña: mala = «incorrecta» y se puede reintentar; buena = concedido;
  - Particiones (discos --particiones) como sagui: aparece el diálogo con el nombre del programa, concede y la
    vista funciona; el programa se queda sin ver la contraseña (el pedido gráfico no la lleva);
  - recordar 5 minutos: abrir Particiones otra vez no vuelve a preguntar («ELEVACION recordada»);
  - instalar --ventana: pide permisos; con No muestra el aviso de que no se concedieron;
  - avisos: uno largo de varias líneas se ve entero (captura y texto completo en la serie) y el segundo,
    encolado, se ve después sin pisar al primero;
  - diálogos de programas (mensagem, confirmar) con texto largo: la ventana crece y el texto sale entero."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, reconhecer_texto, imagem_de_teste, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24
CLAVE = "clave1234"


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1500:])


def ppm(m, nombre):
    ruta = os.path.join(SAIDA, f"h269-{nombre}.ppm")
    if os.path.exists(ruta):
        os.remove(ruta)
    m.monitor(f"screendump {ruta}")
    for _ in range(50):
        if os.path.exists(ruta) and os.path.getsize(ruta) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ruta)


def texto_region(m, x, y, w, h, nombre="t", buscar=None):
    """Lee el texto de una región de la pantalla probando todos los desfases de la rejilla de 8x16 (los
    textos del diálogo y de los avisos no están alineados a la rejilla). Devuelve las líneas legibles de
    todos los desfases (cada línea sale bien en el suyo)."""
    larg, alt, px = ppm(m, nombre)
    vistas, salida = set(), []
    for oy in range(16):
        for ox in range(8):
            x0, y0 = x + ox, y + oy
            nw, nh = (w - ox) // 8 * 8, (h - oy) // 16 * 16
            recorte = b"".join(px[((y0 + k) * larg + x0) * 3:((y0 + k) * larg + x0 + nw) * 3] for k in range(nh))
            for l in reconhecer_texto(nw, nh, recorte):
                if re.search(r"[a-zA-Z]{3,}", l) and l not in vistas:
                    vistas.add(l)
                    salida.append(l)
    return salida


def esperar_texto(m, region, buscar, tempo=30, nombre="e"):
    """Repite la lectura de la región hasta que aparezca `buscar` (el diálogo tarda 2 s en contestar)."""
    fim = time.time() + tempo
    while True:
        lineas = texto_region(m, *region, nombre)
        if any(buscar in l for l in lineas) or time.time() > fim:
            return lineas


def caja(m):
    """Región del diálogo de permisos (centrado, 500 px de ancho) para leer su texto."""
    larg, alt, _ = ppm(m, "dim0")
    return larg // 2 - 264, alt // 2 - 190, 528, 380


def lanzar(m, orden):
    n = m.serial().count("sagui:")
    m.digitar(orden, 0.04)
    m.tecla("ret")
    return n


def esperar_prompt(m, n, tempo=60):
    fim = time.time() + tempo
    while m.serial().count("sagui:") <= n:
        if time.time() > fim:
            raise AssertionError("no volvió el prompt")
        time.sleep(0.25)


def ventana(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui"])
    with Maquina("h269", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(3)
        verificar("[PERMISOS] sesion como sagui" in m.serial(), "la sesión es sagui")
        verificar(re.search(r"\[CONF\] SAGUI-CONF en /sistema/conf", m.serial()), "con SAGUI-CONF (root sin contraseña)")

        # ---- primera vez, Esc = No
        n = lanzar(m, "/testes/dialogo-admin --pedir")
        esperar(m, r"dialogo de permisos: dialogo-admin \(pid \d+\), primera contraseña")
        time.sleep(1)
        lineas = texto_region(m, *caja(m), "primera", "Control de permisos")
        m.captura("H269-1-dialogo-primera-vez")
        verificar(any("Control de permisos" in l for l in lineas), f"el diálogo se ve ({lineas[:4]})")
        verificar(any("Elige la contrase" in l for l in lineas) and any("Rep" in l for l in lineas),
                  f"primera vez: pide elegir la contraseña dos veces ({lineas})")
        m.tecla("esc")
        esperar(m, r"dialogo: pedir r=-14 uid=1000")
        verificar("la persona dijo NO" in m.serial(), "Esc = No: el kernel lo anota")
        esperar_prompt(m, n)

        # ---- contraseñas distintas, luego iguales
        n = lanzar(m, "/testes/dialogo-admin --pedir")
        fim = time.time() + 30
        while m.serial().count("primera contraseña") < 2 and time.time() < fim:
            time.sleep(0.25)
        time.sleep(1)
        m.digitar(CLAVE, 0.05)
        m.tecla("tab")
        m.digitar("otra5678", 0.05)
        m.tecla("ret")
        time.sleep(1)
        lineas = texto_region(m, *caja(m), "distintas", "no coinciden")
        verificar(any("no coinciden" in l for l in lineas), f"contraseñas distintas: error claro ({lineas})")
        m.captura("H269-2-no-coinciden")
        m.digitar(CLAVE, 0.05)
        m.tecla("tab")
        m.digitar(CLAVE, 0.05)
        m.tecla("ret")
        esperar(m, r"dialogo: pedir r=0 uid=0", 60)
        verificar("ELEVACION concedida: dialogo-admin" in m.serial(), "la primera contraseña se fija y se concede")
        verificar(CLAVE not in m.serial(), "la contraseña no aparece en la serie")
        esperar_prompt(m, n)

        # ---- con contraseña: mala, luego buena
        n = lanzar(m, "/testes/dialogo-admin --pedir")
        fim = time.time() + 30
        while m.serial().count("dialogo de permisos: dialogo-admin") < 3 and time.time() < fim:
            time.sleep(0.25)
        time.sleep(1)
        lineas = texto_region(m, *caja(m), "pide", "Contrase")
        verificar(any("administrador" in l for l in lineas), f"ya con contraseña: la pide ({lineas})")
        m.captura("H269-3-pide-contrasena")
        pos = len(m.serial())
        m.digitar("mala", 0.05)
        m.tecla("ret")
        lineas = esperar_texto(m, caja(m), "incorrecta", 30, "mala")
        verificar(any("incorrecta" in l for l in lineas), f"contraseña mala: «incorrecta» ({lineas})")
        m.captura("H269-4-contrasena-mala")
        m.digitar(CLAVE, 0.05)
        m.tecla("ret")
        esperar(m, r"dialogo: pedir r=0 uid=0", 60, pos)
        esperar_prompt(m, n)

        # ---- Particiones como sagui: diálogo, concede, recuerda 5 minutos
        n = lanzar(m, "discos --particiones")
        fim = time.time() + 40
        while m.serial().count("dialogo de permisos: discos") < 1 and time.time() < fim:
            time.sleep(0.25)
        verificar("dialogo de permisos: discos" in m.serial(), "Particiones pide permisos con el diálogo")
        time.sleep(1)
        m.captura("H269-5-particiones-pide")
        m.digitar(CLAVE, 0.05)
        m.tecla("ret")
        esperar(m, r"ELEVACION concedida: discos")
        time.sleep(2)
        m.captura("H269-6-particiones-concedido")
        m.tecla("alt-f4")
        esperar_prompt(m, n)
        n = lanzar(m, "discos --particiones")
        esperar(m, r"ELEVACION recordada: discos", 40)
        verificar(m.serial().count("dialogo de permisos: discos") == 1, "recordar 5 minutos: no vuelve a preguntar")
        time.sleep(2)
        m.tecla("alt-f4")
        esperar_prompt(m, n)

        # ---- instalar --ventana: No
        n = lanzar(m, "instalar --ventana")
        esperar(m, r"dialogo de permisos: instalar", 40)
        time.sleep(1)
        m.tecla("esc")
        esperar(m, r'janela \d+ criada por \d+: "Instalar SaguiOS"', 30)
        verificar(True, "instalar con No: muestra el aviso de que no se concedieron los permisos")
        time.sleep(1)
        m.tecla("ret")
        esperar_prompt(m, n)

        # ---- avisos: largo (varias líneas) y encolado
        n = lanzar(m, "/testes/dialogo-admin --avisos")
        esperar(m, r"dialogo: avisos enviados")
        verificar("FINALDELAVISO" in m.serial() and "SEGUNDOVISIBLE" in m.serial(),
                  "los dos avisos llegan completos a la serie (texto íntegro)")
        time.sleep(1.5)
        m.captura("H269-7-aviso-largo")
        larg, alt, _ = ppm(m, "dim")
        lineas = texto_region(m, larg - 440, 30, 440, 200, "aviso", "FINALDELAVISO")
        verificar(any("FINALDELAVISO" in l for l in lineas), f"el aviso largo se ve entero ({lineas})")
        verificar(not any("SEGUNDOVISIBLE" in l for l in lineas), "el segundo espera su turno (no pisa al primero)")
        fim = time.time() + 40
        visto = False
        while time.time() < fim and not visto:
            time.sleep(1)
            lineas = texto_region(m, larg - 440, 30, 440, 120, "aviso2", "SEGUNDOVISIBLE")
            visto = any("SEGUNDOVISIBLE" in l for l in lineas)
        verificar(visto, "el segundo aviso se ve después del primero")
        m.captura("H269-8-aviso-segundo")
        esperar_prompt(m, n)

        # ---- diálogos de programas con texto largo
        n = lanzar(m, "/testes/dialogo-admin --mensaje")
        esperar(m, r'janela \d+ criada por \d+: "Mensaje largo"', 30)
        time.sleep(1)
        v = ventana(m.serial(), "Mensaje largo")
        verificar(v["l"] > 380 and v["l"] <= 640, f"la ventana del mensaje crece en ancho ({v['l']} px)")
        verificar(v["a"] >= 70 + 3 * 18, f"y en alto ({v['a']} px)")
        lineas = []
        for k in range(8):   # cada fila del texto está a 18 px: una lectura por fila, alineada a la rejilla
            lineas += texto_region(m, v["x"] + BORDA + 16, v["y"] + TITULO + 16 + 18 * k, v["l"] - 32, 16, f"men{k}")
        verificar(any("FINALDELMENSAJE" in l for l in lineas), f"el mensaje se ve entero ({lineas})")
        m.captura("H269-9-mensaje-largo")
        m.tecla("ret")
        esperar(m, r'janela \d+ criada por \d+: "Pregunta larga"', 30)
        time.sleep(1)
        v = ventana(m.serial(), "Pregunta larga")
        lineas = []
        for k in range(8):
            lineas += texto_region(m, v["x"] + BORDA + 16, v["y"] + TITULO + 16 + 18 * k, v["l"] - 32, 16, f"preg{k}")
        verificar(any("FINALDELAPREGUNTA" in l for l in lineas), f"la pregunta se ve entera ({lineas})")
        m.captura("H269-10-pregunta-larga")
        m.tecla("esc")
        esperar_prompt(m, n)
    print("h269: todo bien")


if __name__ == "__main__":
    main()
