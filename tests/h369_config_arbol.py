#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H369 (D367) - Configuración con árbol de secciones.

Parte estática (sin QEMU):
  - la tabla seccoes[] de configuracion.c y conf_arbol.SECCIONES (el orden del árbol) coinciden;
  - los 16 colores de acento dan al menos 3:1 de contraste con el texto blanco de las barras.
Parte QEMU a 800x600 (con una tarjeta de sonido HDA):
  1. La ventana cabe en la pantalla (788 de ancho) y arranca en la primera sección; el árbol recorre las 15 secciones
     (cada clic deja «configuracion: pagina X» en el registro) y se hacen capturas H369-*.png.
  2. Cada sección enseña Aceptar / Cancelar / Aplicar abajo a la derecha (píxeles del botón) y nada se sale por la derecha.
  3. Las 16 muestras de acento caben en el grupo (sus colores se leen por píxel, también el borde derecho de la última).
  4. Cambios en varias secciones: teclado (ABNT2), acento, «Sonido al capturar», silenciar; «Predeterminado» devuelve el teclado.
  5. El árbol se pliega y se despliega; Kernel se desplaza con la barra y enseña el pie.
  6. «Cancelar» deshace lo de esta apertura (el acento vuelve al de antes).
  7. «configuracion --ventana kernel» y «--seccion fuentes» abren la sección pedida; sin argumento, la última de la vez anterior.
  8. Dependencias entre opciones (conf_reglas.c): prueba de unidad en el PC que recorre todas las combinaciones; «kconfig» rechaza
     las combinaciones sin sentido sin guardar nada; en la ventana, con la imagen sin PAE, NX sale deshabilitada y un clic no hace nada.
Capturas docs/capturas/H369-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, cor_do_pixel, RAIZ  # noqa: E402
import conf_arbol as ca  # noqa: E402

FUENTE = os.path.join(RAIZ, "userland", "programas", "configuracion.c")
BOTON = (0xE2, 0xE6, 0xEA)   # COR_BOTAO


def luminancia(c):
    t = []
    for sh in (16, 8, 0):
        v = ((c >> sh) & 255) / 255
        t.append(v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4)
    return 0.2126 * t[0] + 0.7152 * t[1] + 0.0722 * t[2]


def parte_estatica():
    src = open(FUENTE, encoding="utf-8").read()
    chaves = re.findall(r'\[S_\w+\]\s*=\s*\{\s*"([^"]+)"', src)
    verificar(tuple(chaves) == ca.SECCIONES, f"el orden de seccoes[] es el de conf_arbol.SECCIONES ({chaves})")
    bloque = src[src.index("#define CORES_QTD"):src.index("/* ---", src.index("#define CORES_QTD"))]
    cols = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{6})", bloque)]
    verificar(len(cols) == 16, f"hay 16 colores de acento ({len(cols)})")
    for c in cols:
        ratio = 1.05 / (luminancia(c) + 0.05)
        verificar(ratio >= 3.0, f"el acento {c:06x} da {ratio:.2f}:1 con el texto blanco")
    return cols


def parte_unidad():
    """La prueba de unidad de las reglas de dependencia, compilada y ejecutada en el PC."""
    import subprocess
    exe = os.path.join(RAIZ, "build", "testes", "conf_reglas_unidad")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-I", os.path.join(RAIZ, "userland", "inc"), "-o", exe,
                    os.path.join(RAIZ, "tests", "conf_reglas_unidad.c"), os.path.join(RAIZ, "userland", "libsagui", "conf_reglas.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and " 0 fallos" in r.stdout, f"unidad de conf_reglas: {r.stdout.strip()[-120:]}")


def esperar(m, patron, tempo, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1500:])


def abrir(m, orden="configuracion --ventana &"):
    n = len(m.serial())
    m.comando(orden, 20)
    esperar(m, r'"Configuraci.n"', 30, n)
    time.sleep(1.5)
    return ca.ventana(m, n), n


def recorte(png, x, y, l, a):
    from h162_fuentes import region
    return region(png, x, y, l, a)


def a_config(m):
    m.clique(300, 12)       # el botón de Configuración en la barra de tareas
    time.sleep(0.6)


def a_terminal(m):
    m.clique(150, 12)       # el botón del Terminal
    time.sleep(0.6)


def pagina(m, v, chave, desde):
    ca.ir(m, v, chave)
    esperar(m, r"configuracion: pagina " + re.escape(ca.LOG[chave]), 10, desde)
    return len(m.serial())


def botones_pie(png, v):
    """True si Aceptar, Cancelar y Aplicar están abajo a la derecha (píxeles de fondo de botón sobre el texto)."""
    ok = True
    for n in ("aceptar", "cancelar", "aplicar"):
        x, y = ca.boton_pie(v, n)
        ok &= cor_do_pixel(png, x - 30, y - 10) == BOTON
    return ok


def parte_qemu(cols):
    img = imagem_de_teste("", ["--vesa", "--resolucao", "800x600", "--sesion-sagui"])
    audio = ["-audiodev", "none,id=aud0", "-device", "intel-hda", "-device", "hda-duplex,audiodev=aud0"]
    with Maquina("h369", "ide", imagem=img, snapshot=False, extras=audio) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        out = m.comando("configuracion mostrar", 20)
        acento0 = re.search(r"Acento:\s+([0-9a-f]{6})", out).group(1)
        v, n0 = abrir(m)
        verificar(v["l"] == 788, f"la ventana mide 788 de ancho a 800x600 ({v['l']})")
        verificar(v["x"] >= 0 and v["x"] + v["l"] + 2 <= 800 and v["y"] >= 0 and v["y"] + v["a"] + 2 <= 600,
                  f"cabe en la pantalla ({v})")
        esperar(m, r"configuracion: pagina Pantalla", 10, n0)
        m.captura("H369-1-pantalla")
        # --- 1 y 2: el árbol recorre todas las secciones y cada una deja los tres botones del pie a la vista
        cursor = len(m.serial())
        for ch in ca.HOJAS:
            if ch != "tela":    # Pantalla ya es la sección abierta: un clic sobre ella no cambia nada
                cursor = pagina(m, v, ch, cursor)
            time.sleep(0.5)
            png = m.captura("H369-rec-" + ch, guardar=False)
            verificar(botones_pie(png, v), f"[{ch}] Aceptar, Cancelar y Aplicar abajo a la derecha")
            # el borde derecho de la ventana (8 px de margen) está limpio: nada se sale del panel
            verificar(cor_do_pixel(png, v["x"] + v["l"] - 3, v["y"] + 150) == (0xF4, 0xF4, 0xF0), f"[{ch}] el margen derecho está limpio")
        for ch, nombre in (("aceleracion", "2-aceleracion"), ("kernel", "3-kernel"), ("escritorio", "4-colores-y-fondo"),
                           ("sonido", "5-sonido"), ("capturas", "6-capturas"), ("red", "7-publicidad"), ("ia", "8-ia"),
                           ("mandos", "9-mandos"), ("usuarios", "10-usuarios")):
            cursor = pagina(m, v, ch, cursor)
            time.sleep(0.6)
            m.captura("H369-" + nombre)
        # --- 3: las 16 muestras de acento caben y se leen por píxel
        cursor = pagina(m, v, "escritorio", cursor)
        time.sleep(0.6)
        png = m.captura("H369-acentos", guardar=False)
        px, py, pw, ph = ca.geometria(v["l"], v["a"])
        for i, c in enumerate(cols):
            x, y = ca.panel(v, *ca.muestra_acento(i))
            leido = cor_do_pixel(png, x, y)
            verificar(leido == (c >> 16, (c >> 8) & 255, c & 255) or i == 0,
                      f"la muestra {i} tiene su color ({leido} / {c:06x})")
            derecha = cor_do_pixel(png, x + 12, y)
            verificar(derecha != (0xF4, 0xF4, 0xF0), f"la muestra {i} no está cortada por la derecha")
        gx = ca.panel(v, 18 + pw + 22 - 42, 0)[0]   # borde derecho del grupo (x local 18 + ancho LARG - 42)
        xult = ca.panel(v, ca.muestra_acento(15)[0] + 13, 0)[0]
        verificar(xult < gx, f"la última muestra queda dentro del grupo ({xult} < {gx})")
        # --- 4: cambios en varias secciones
        m.clique(*ca.panel(v, *ca.muestra_acento(6)))          # Rojo
        time.sleep(0.6)
        cursor = pagina(m, v, "teclado", cursor)
        ca.clic_panel(m, v["x"], v["y"], 30 + 20, 26 + 26 + 6, v["l"], v["a"])   # ABNT2
        time.sleep(0.6)
        cursor = pagina(m, v, "capturas", cursor)
        ca.clic_panel(m, v["x"], v["y"], 30 + 7, 149, v["l"], v["a"])   # «Sonido al capturar» (segundo grupo)
        time.sleep(0.6)
        m.captura("H369-11-cambios")
        a_terminal(m)
        out = m.comando("configuracion mostrar", 20)   # (con ABNT2 no se usan ni «;» ni «/»)
        verificar("Teclado:   abnt2" in out, f"el teclado ABNT2 se aplicó ({out.strip()[:80]})")
        verificar("Acento:    c0392b" in out, "el acento Rojo se aplicó")
        a_config(m)
        # «Predeterminado» devuelve el teclado a ES (las teclas de la prueba son las de un teclado ES)
        cursor = pagina(m, v, "teclado", cursor)
        m.clique(*ca.boton_pie(v, "predeterminado"))
        time.sleep(0.8)
        a_terminal(m)
        out = m.comando("configuracion mostrar", 20)
        verificar("Teclado:   es" in out, "«Predeterminado» vuelve al teclado español")
        out = m.comando("cat /tmp/capturas.conf", 20)
        verificar("capturas_sonido=0" in out, f"la casilla «Sonido al capturar» se desmarcó ({out.strip()})")
        a_config(m)
        # sonido: silenciar
        cursor = pagina(m, v, "sonido", cursor)
        m.clique(*ca.panel(v, ca.xr_silenciar(v), ca.SND["vol_y"] - 22 + 7))
        time.sleep(0.8)
        a_terminal(m)
        out = m.comando("configuracion mostrar", 20)
        verificar("(silencio)" in out, f"la casilla Silenciar deja el sonido en silencio ({out.strip()[:120]})")
        a_config(m)
        m.clique(*ca.panel(v, ca.xr_silenciar(v), ca.SND["vol_y"] - 22 + 7))
        time.sleep(0.6)
        # --- 5: plegar y desplegar el árbol; desplazar Kernel
        png_a = m.captura("H369-arbol-a", guardar=False)
        x, y = ca.arbol(v, "sistema")
        m.clique(v["x"] + 8 + 6 + 4, y)                         # la caja «-» de Sistema
        time.sleep(0.6)
        png_b = m.captura("H369-12-arbol-plegado")
        zona = (v["x"] + 10, v["y"] + 10, 170, 200)
        verificar(recorte(png_a, *zona) != recorte(png_b, *zona), "plegar «Sistema» cambia el árbol")
        m.clique(v["x"] + 8 + 6 + 4, y)                         # y se vuelve a abrir
        time.sleep(0.6)
        cursor = pagina(m, v, "kernel", cursor)
        time.sleep(0.5)
        ca.al_final(m, v)
        time.sleep(0.6)
        m.captura("H369-13-kernel-final")
        # --- 6: Cancelar deshace el acento
        m.clique(*ca.boton_pie(v, "cancelar"))
        time.sleep(1.2)                  # la ventana se cierra y el foco vuelve al Terminal
        out = m.comando("configuracion mostrar", 20)
        verificar(f"Acento:    {acento0}" in out, f"«Cancelar» devuelve el acento ({acento0})")
        # --- 7: secciones por la línea de órdenes y la última de la vez anterior
        v2, n1 = abrir(m, "configuracion --ventana kernel &")
        esperar(m, r"configuracion: pagina Kernel", 10, n1)
        m.clique(*ca.boton_pie(v2, "aceptar"))
        time.sleep(1)
        v3, n2 = abrir(m, "configuracion --seccion fuentes &")
        esperar(m, r"configuracion: pagina Fuentes", 10, n2)
        m.clique(*ca.boton_pie(v3, "aceptar"))
        time.sleep(1)
        v4, n3 = abrir(m)
        esperar(m, r"configuracion: pagina Fuentes", 10, n3)
        verificar(True, "sin argumento se abre la última sección")
        m.clique(*ca.boton_pie(v4, "aceptar"))
        time.sleep(1)
        # --- 8: «kconfig» rechaza lo que no tiene sentido y no guarda nada (sin hacer falta ser administrador)
        out = m.comando("kconfig pae=no nx=si", 20)
        verificar("combinación no válida" in out and "Requiere PAE" in out, f"nx=si con pae=no se rechaza ({out.strip()[-120:]})")
        out = m.comando("kconfig cache_disco=no vaciado_s=5", 20)
        verificar("combinación no válida" in out and "Sin efecto" in out, f"vaciado_s con la caché apagada se rechaza ({out.strip()[-140:]})")
        out = m.comando("kconfig nx=si", 20)
        verificar("combinación no válida" not in out, "nx=si con pae en la imagen no es una combinación inválida")


def parte_sin_pae():
    """Imagen sin PAE: en Sistema > Kernel NX está deshabilitada (con su motivo) y pulsarla no pide permisos ni cambia nada."""
    img = imagem_de_teste("", ["--vesa", "--resolucao", "800x600", "--sesion-sagui", "--sem-pae"])
    with Maquina("h369b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        v, n0 = abrir(m, "configuracion --ventana kernel &")
        esperar(m, r"configuracion: pagina Kernel", 10, n0)
        time.sleep(0.8)
        m.captura("H369-14-kernel-sin-pae")
        n = len(m.serial())
        # la fila de NX es la segunda de Memoria: y local 118 + 30 (la primera casilla está en 118)
        ca.clic_panel(m, v["x"], v["y"], 30 + 7, 118 + 30 + 7, v["l"], v["a"])
        time.sleep(1.5)
        s = m.serial()[n:]
        verificar("pedido" not in s and "ELEVAR" not in s and "dialogo de permisos" not in s, "pulsar NX deshabilitada no pide permisos")
        m.captura("H369-15-nx-deshabilitada")
        m.clique(*ca.boton_pie(v, "aceptar"))
        time.sleep(1)


if __name__ == "__main__":
    cols = parte_estatica()
    parte_unidad()
    parte_qemu(cols)
    parte_sin_pae()
    print("H369 OK")
