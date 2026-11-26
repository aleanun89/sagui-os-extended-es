#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H483 (D483, v8.5.6.5) - fondo rotativo y acento automático.

Parte estática (sin QEMU): las fuentes tienen el servicio (--rotar), las órdenes (fondo-rotar, fondo-siguiente,
fondo-acento), la página de Colores y fondo, la opción «Siguiente fondo» del menú del escritorio y D483.
Parte QEMU (1024x768, /home persistente):
  (1) la prueba escribe tres imágenes PPM sintéticas de colores claros distintos y una cuarta con grises, blanco
      y negro (que no deben contar);
  (2) «configuracion fondo-acento si» y «fondo-rotar CARPETA 5s orden»: el servicio cambia de fondo cada 5 s, en orden,
      y tras el primero con un fundido; el escritorio toma el color de cada imagen (píxel) y el título activo el acento
      calculado como lo calcula el programa (color dominante, oscurecido hasta 4,5:1 con el texto blanco);
  (3) «fondo-siguiente» (lo mismo que la opción del menú del escritorio) avanza uno;
  (4) «azar»: nunca repite el fondo de antes; «corte»: sin fundido;
  (5) la imagen con grises y blanco da el color vivo que tiene (no el gris) y una foto gris deja el acento como está;
  (6) la ventana de Configuración > Colores y fondo enseña las opciones;
  (7) se guarda en sagui.conf y, tras reiniciar (misma imagen), el servicio vuelve a arrancar y el acento sigue.
Capturas docs/capturas/H483-*.png."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import estilos_png as png  # noqa: E402
from qemu import Maquina, verificar, imagem_de_teste, RAIZ  # noqa: E402

ANCHO, ALTO = 1024, 768
IMAGENES = (("1-rojo", (240, 120, 120)), ("2-verde", (120, 220, 130)), ("3-azul", (120, 160, 240)))


def leer(ruta):
    return open(os.path.join(RAIZ, ruta), encoding="utf-8").read()


def acento_esperado(c):
    """El mismo cálculo que acento_do_fundo para una imagen de un solo color (sin grises): mezcla con negro de 5 en 5 %
    hasta que 1050 / (lum + 50) >= 4,5 con la luminancia por 1000 de gamma 2."""
    r, g, b = c
    pct = 0
    while pct < 100:
        rr, gg, bb = r * (100 - pct) // 100, g * (100 - pct) // 100, b * (100 - pct) // 100
        lum = (2126 * rr * rr + 7152 * gg * gg + 722 * bb * bb) // 650250
        if 2100 >= 9 * (lum + 50):
            return rr << 16 | gg << 8 | bb
        pct += 5
    return 0


def hex3(c):
    return (c >> 16, (c >> 8) & 255, c & 255)


def esperar(m, patron, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(patron, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1500:])


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def ppm(color):
    return "P3 2 2 255 " + " ".join("%d %d %d" % color for _ in range(4))


def estatica():
    c = leer("userland/programas/configuracion.c")
    for t in ("--rotar", "fondo-rotar", "fondo-siguiente", "fondo-acento", "acento_do_fundo", "fondo_rotar_seg", "acento_auto",
              "fundo_aplicar_fundido", "Tomar el color de acento del fondo", "fondo_tipo", "fondo-color"):
        verificar(t in c, f"configuracion.c tiene «{t}»")
    k = leer("kernel/gui/area_trabalho.c")
    verificar("AF_FONDO_SIG" in k and "Siguiente fondo" in k, "el menú del escritorio tiene «Siguiente fondo»")
    d = leer("docs/decisoes.md")
    verificar("## D483." in d, "docs/decisoes.md tiene D483")
    verificar(acento_esperado((240, 120, 120)) != 0, "el cálculo esperado da un acento")
    for _n, col in IMAGENES:
        a = acento_esperado(col)
        verificar(a != 0 and a != (col[0] << 16 | col[1] << 8 | col[2]), f"el color {col} se oscurece a {a:06X} para dar 4,5:1")


def sacar(m, nombre, desde):
    """Espera al siguiente «fondo siguiente» tras `desde` y devuelve (ruta, modo, acento, n)."""
    patron = r"configuracion: fondo siguiente (\S+) \((orden|azar)\) -> (-?\d+)"
    fim = time.time() + 40
    while time.time() < fim:
        g = re.findall(patron, m.serial()[desde:])
        if g:
            return g[0], len(m.serial())
        time.sleep(0.3)
    raise AssertionError(f"no hubo cambio de fondo ({nombre})\n" + m.serial()[-1500:])


def comprobar(m, ruta, color, titulo_ventana, nombre, serie_desde):
    """El fondo y el título activo tienen los colores esperados; devuelve la captura."""
    a = acento_esperado(color)
    time.sleep(2.2)   # el fundido y el repintado
    s = m.serial()
    g = re.findall(r"configuracion: acento automatico ([0-9a-f]{6}) \(fondo (\S+)\)", s[serie_desde:])
    g = [x for x in g if x[1] == ruta]
    verificar(g and int(g[-1][0], 16) == a, f"[{nombre}] acento automático en la serie {g[-1] if g else None} = {a:06X}")
    img = png.leer(m.captura("H483-" + nombre))
    verificar(png.pixel(img, 900, 700) == hex3(color[0] << 16 | color[1] << 8 | color[2]),
              f"[{nombre}] el escritorio tiene el color de la imagen {png.pixel(img, 900, 700)} = {color}")
    tx, ty = titulo_ventana
    verificar(png.pixel(img, tx, ty) == hex3(a), f"[{nombre}] el título activo tiene el acento {png.pixel(img, tx, ty)} = {a:06X}")
    return img


def qemu():
    img_path = imagem_de_teste("h483", ["--vesa", "--resolucao", f"{ANCHO}x{ALTO}", "--dados-mb", "16"])
    with Maquina("h483", "ide", imagem=img_path, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(3)
        n0 = len(m.serial())
        m.comando("mkdir /home/fondosprueba", 20)
        for nombre, col in IMAGENES:
            m.comando(f'echo "{ppm(col)}" > /home/fondosprueba/{nombre}.ppm', 20)
        # cuarta imagen: dos puntos de un azul vivo, uno gris, uno blanco, uno casi negro: solo cuenta el azul
        m.comando('echo "P3 3 2 255 40 90 220 40 90 220 128 128 128 255 255 255 10 10 10 40 90 220" > /home/fondosprueba/9-mixta.ppm', 20)
        out = m.comando("ls /home/fondosprueba", 20)
        verificar("1-rojo.ppm" in out and "9-mixta.ppm" in out, "las imágenes de prueba están en /home/fondosprueba")
        # el Terminal activo: su título lleva el acento
        sr = m.serial()
        t = janela(sr, "Terminal")
        tit = (t["x"] - 2 + 3, t["y"] - 24 + 3)
        # (2) acento automático + rotación cada 5 s, en orden, con fundido
        n = len(m.serial())
        m.comando("configuracion fondo /home/fondosprueba/9-mixta.ppm estirado", 30)   # el modo lo conserva la rotación
        m.comando("configuracion fondo-acento si", 20)
        m.comando("configuracion fondo-rotar /home/fondosprueba 5s orden", 30)
        esperar(m, r"configuracion: fondo rotativo en marcha", 20, n)
        vistos = []
        (r1, _o, _c), pos = sacar(m, "1", n)
        vistos.append(r1)
        verificar(r1.endswith("/1-rojo.ppm"), f"el primero es el de nombre menor: {r1}")
        comprobar(m, r1, IMAGENES[0][1], tit, "1-rojo", n)
        pos = len(m.serial())
        (r2, _o, _c), pos2 = sacar(m, "2", pos)
        verificar(r2.endswith("/2-verde.ppm"), f"en orden: sigue el verde: {r2}")
        verificar("fundido" in m.serial()[pos:], "el segundo cambio se hace con fundido (el primero no tiene fondo anterior en el servicio)")
        comprobar(m, r2, IMAGENES[1][1], tit, "2-verde", pos)
        pos = len(m.serial())
        (r3, _o, _c), _p = sacar(m, "3", pos)
        verificar(r3.endswith("/3-azul.ppm"), f"y luego el azul: {r3}")
        comprobar(m, r3, IMAGENES[2][1], tit, "3-azul", pos)
        # (5) la mixta: el azul vivo (sin gris, blanco ni negro)
        pos = len(m.serial())
        (r4, _o, _c), _p = sacar(m, "4", pos)
        verificar(r4.endswith("/9-mixta.ppm"), f"la cuarta es la mixta: {r4}")
        time.sleep(2.2)
        g = re.findall(r"acento automatico ([0-9a-f]{6}) \(fondo \S+9-mixta", m.serial()[pos:])
        verificar(g and int(g[0], 16) == acento_esperado((40, 90, 220)), f"la mixta da el azul vivo {g} (sin gris, blanco ni negro)")
        pos = len(m.serial())
        (r5, _o, _c), _p = sacar(m, "5", pos)
        verificar(r5.endswith("/1-rojo.ppm"), f"después de la última vuelve a la primera: {r5}")
        # (3) fondo-siguiente a mano (lo que hace «Siguiente fondo» del menú del escritorio)
        n = len(m.serial())
        out = m.comando("configuracion fondo-siguiente", 40)
        verificar("2-verde" in out, f"fondo-siguiente a mano: {out.strip()[-80:]}")
        # (4) al azar: nunca el mismo dos veces seguidas
        m.comando("configuracion fondo-rotar /home/fondosprueba 5s azar corte", 30)
        seq = []
        pos = len(m.serial())
        for _ in range(5):
            (r, o, _c), pos = sacar(m, "azar", pos)
            seq.append((r, o))
        verificar(all(o == "azar" for _r, o in seq), "el orden es «azar»")
        verificar(all(seq[i][0] != seq[i + 1][0] for i in range(len(seq) - 1)), f"al azar no repite el de antes: {[s[0][-12:] for s in seq]}")
        verificar("fundido" not in m.serial()[len(m.serial()) - 200:], "con «corte» no hay fundido")
        # (7) lo guardado
        out = m.comando("cat /home/.config/sagui.conf", 20)
        verificar("fondo_rotar_carpeta=/home/fondosprueba" in out and "fondo_rotar_seg=5" in out and "acento_auto=1" in out and "fondo_rotar_azar=1" in out,
                  "sagui.conf guarda la carpeta, el intervalo, el orden y el acento automático")
        m.comando("configuracion fondo-rotar /home/fondosprueba 5s orden", 30)
        m.comando("sync", 30)
        # (6) la ventana: un selector excluyente (Color / Imagen / Presentación) y solo las opciones del tipo elegido
        import conf_arbol as ca
        n = len(m.serial())
        m.comando("configuracion --ventana escritorio &", 20)
        esperar(m, r'criada por \d+: "Configuraci.n"', 30, n)
        time.sleep(2.5)
        v = janela(m.serial(), "Configuración")
        XL = 30
        def radio(x, y):   # el círculo de una opción en coordenadas locales del panel
            return ca.panel(v, XL + x + 8, y + 7)
        def archivo():
            m.clique(150, 12)   # el Terminal al frente
            time.sleep(0.8)
            o = m.comando("cat /home/.config/sagui.conf", 20)
            m.clique(350, 12)   # y Configuración otra vez
            time.sleep(0.8)
            return o
        m.captura("H483-5-presentacion")
        m.clique(*radio(110, 32))            # Imagen
        time.sleep(2.5)
        m.captura("H483-6-tipo-imagen")
        verificar("fondo_tipo=imagen" in archivo(), "clic en «Imagen»: el tipo guardado es imagen")
        esperar(m, r"fondo rotativo desactivado: termina", 10)
        m.clique(*radio(0, 32))              # Color
        time.sleep(2)
        m.clique(*radio(200, 86))            # Color sólido
        time.sleep(1.5)
        m.captura("H483-7-color-solido")
        pos = len(m.serial())
        m.clique(*ca.panel(v, XL + 2 * 32 + 13, 114 + 13))   # la muestra 3, azul 2A6FD8
        time.sleep(2.5)
        azul = (0x2A, 0x6F, 0xD8)
        a = acento_esperado(azul)
        g = re.findall(r"acento automatico ([0-9a-f]{6}) \(fondo color\)", m.serial()[pos:])
        verificar(g and int(g[-1], 16) == a, f"color sólido: el acento sale de ese color {g[-1] if g else None} = {a:06X}")
        img = png.leer(m.captura("H483-7-tipo-color"))
        verificar(png.pixel(img, 60, 700) == hex3(0x2A6FD8), f"color sólido: el escritorio es 2A6FD8 ({png.pixel(img, 60, 700)})")
        verificar("fondo_tipo=color" in archivo() and "fondo_color_solido=1" in archivo() and "fondo_color=2a6fd8" in archivo(),
                  "el color sólido se guarda (fondo_tipo=color, fondo_color_solido=1, fondo_color=2a6fd8)")
        n = len(m.serial())
        m.clique(*radio(230, 32))            # Presentación (rotativo)
        esperar(m, r"fondo rotativo en marcha", 15, n)
        time.sleep(2)
        m.captura("H483-8-tipo-presentacion")
        verificar("fondo_tipo=rotativo" in archivo(), "clic en «Presentación»: el tipo es rotativo y el servicio arranca")
        m.clique(150, 12)
        time.sleep(0.8)
        m.comando("sync", 30)
    with Maquina("h483", "ide", imagem=img_path, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(4)
        s = m.serial()
        verificar("configuracion: lanzo el fondo rotativo" in s and "fondo rotativo en marcha" in s, "tras reiniciar, el servicio vuelve a arrancar")
        (r, _o, _c), _p = sacar(m, "reinicio", 0)
        verificar("/home/fondosprueba/" in r, f"y cambia de fondo en la carpeta guardada: {r}")
        g = re.findall(r"acento automatico ([0-9a-f]{6})", m.serial())
        verificar(g, "tras reiniciar, el fondo vuelve a dar acento automático")
        verificar("fondo_tipo=rotativo" in m.comando("cat /home/.config/sagui.conf", 20), "el tipo «rotativo» se guardó y vuelve tras reiniciar")
        m.comando("configuracion fondo-rotar no", 30)
        esperar(m, r"fondo rotativo desactivado: termina", 10)
        # migración: un sagui.conf de antes (sin fondo_tipo) se interpreta por lo que tenía
        m.comando("echo \"fondo=/home/fondosprueba/1-rojo.ppm\" > /home/.config/sagui.conf", 20)
        m.comando("echo \"fondo_modo=estirado\" >> /home/.config/sagui.conf", 20)
        o = m.comando("configuracion fondo-tipo", 20)
        verificar("Tipo de fondo: imagen" in o, f"migración: fondo con imagen y sin tipo = imagen ({o.strip()[-120:]!r}) {m.comando("cat /home/.config/sagui.conf", 20)!r}")
        m.comando("echo \"fondo_rotar_seg=60\" >> /home/.config/sagui.conf", 20)
        verificar("Tipo de fondo: rotativo" in m.comando("configuracion fondo-tipo", 20), "migración: con intervalo de rotación y sin tipo = rotativo")
        m.comando("echo \"fondo_tipo=color\" >> /home/.config/sagui.conf", 20)
        verificar("Tipo de fondo: color" in m.comando("configuracion fondo-tipo", 20), "con fondo_tipo en el archivo manda él")
        m.comando("configuracion fondo-acento no", 30)
        m.comando("configuracion fondo ninguno", 30)
        m.comando("sync", 30)
    print("H483 ok")


if __name__ == "__main__":
    estatica()
    if not os.environ.get("SAGUI_SIN_QEMU"):
        qemu()
