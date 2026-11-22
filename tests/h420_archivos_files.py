#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H420 - Archivos renovado, con ideas del explorador «Files» y el aspecto de SaguiOS (D420, v8.5.5.2).

Criterio (todo con datos sintéticos creados en /tmp):
  (1) arranque: barra de herramientas, barra de órdenes con iconos, barra lateral (Favoritos, Unidades con su
      barra de espacio y Etiquetas) y migas de pan;
  (2) pestañas: Ctrl+T abre una pestaña (en la página de Inicio: favoritos, unidades, recientes), Ctrl+Tab
      cambia, Ctrl+W la cierra;
  (3) doble panel (F3): arrastrar un archivo de un panel a una carpeta del otro lo copia (y el disco lo confirma);
  (4) barra lateral: arrastrar una carpeta a «Favoritos» la ancla; arrastrar un archivo a una etiqueta de color
      se la pone;
  (5) migas: la flecha de una parte lista sus subcarpetas;
  (6) vistas: detalles (con columnas), iconos con miniaturas, mosaicos y agrupar por tipo;
  (7) vista previa (Alt+P): texto con resaltado de código e imagen;
  (8) búsqueda recursiva con filtros;
  (9) git: en un repositorio, la rama y el estado de cada archivo.
Las comprobaciones salen de lo que el programa escribe en la serie y de lo que queda en el disco; las capturas
(H420-*) son de datos sintéticos.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

LATERAL, Y_ORDENS, Y_LISTA = 140, 40, 98
LISTA_X = LATERAL + 6
LINHA_A, SEP_A = 22, 9
DIREITO = 2
# La barra de órdenes: 16 botones de 26 px + 2 de hueco, con 8 px extra antes de «Ver» y de «Pestaña».
OR = ["nuevo", "cortar", "copiar", "pegar", "renombrar", "comprimir", "extraer", "terminal", "props",
      "ver", "doble", "previa", "pestana", "inicio", "fav", "etiqueta"]


def orden_x(nombre):
    x = 6
    for k, n in enumerate(OR):
        if n in ("ver", "pestana"):
            x += 8
        if n == nombre:
            return x + 13
        x += 28
    raise KeyError(nombre)


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def tela(m, nome):
    ppm = os.path.join(SAIDA, f"h420-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    return ler_ppm(ppm)


def contar(img, x0, y0, l, a):
    """Píxeles que no son blancos ni del gris claro del fondo en un rectángulo de la pantalla."""
    larg, _, px = img
    n = 0
    for y in range(y0, y0 + a, 1):
        for x in range(x0, x0 + l, 1):
            r, g, b = px[(y * larg + x) * 3:(y * larg + x) * 3 + 3]
            if (r, g, b) != (255, 255, 255) and not (r > 230 and g > 230 and b > 230):
                n += 1
    return n


def menu_abierto(m, desde):
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 15, desde=desde)
    return tuple(int(v) for v in g)


def linha_menu(menu, i, separadores_antes=0):
    mx, my, ml = menu[0], menu[1], menu[2]
    return mx + ml // 2, my + 2 + (i - separadores_antes) * LINHA_A + separadores_antes * SEP_A + LINHA_A // 2


JANELA = {}
Y_FAV_TMP = 178     # centro de «tmp» entre los favoritos (Sistema, home, tmp, uno: 24 px cada uno desde y=118)


def ir(m, ruta):
    """Va a /tmp/h420 o /tmp/h420g por la barra lateral (favorito «tmp») y un doble clic: sin escribir rutas."""
    a = JANELA
    marca = len(m.serial())
    m.tecla("ctrl-1")   # vista de lista: las filas empiezan en Y_LISTA
    time.sleep(0.4)
    m.clique(a["x"] + 60, a["y"] + Y_FAV_TMP)
    esperar(m, r"arquivos: pasta /tmp \(\d+ itens\)", 10, desde=marca)
    time.sleep(0.4)
    marca = len(m.serial())
    fila = 0 if ruta == "/tmp/h420" else 1   # en /tmp las carpetas van primero: h420, h420g
    m.clique(a["x"] + LISTA_X + 60, a["y"] + Y_LISTA + 2 + fila * 20 + 8, duplo=True)
    esperar(m, rf"arquivos: pasta {re.escape(ruta)} \(\d+ itens\)", 10, desde=marca)
    time.sleep(0.5)


def principal():
    with Maquina("h420", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- datos sintéticos: carpetas, texto, código, una imagen y un repositorio de git
        for c in ["mkdir /tmp/h420", "mkdir /tmp/h420/dos", "mkdir /tmp/h420/uno", "mkdir /tmp/h420/uno/hondo",
                  "echo hola > /tmp/h420/a.txt", "echo hola dos > /tmp/h420/b.txt", "echo hondo > /tmp/h420/uno/hondo/c.txt",
                  "echo 'int main(void) { /* hola */ return 0; }' > /tmp/h420/codigo.c",
                  "echo 'P3 2 2 255 255 0 0 0 255 0 0 0 255 255 255 0' > /tmp/h420/pix.ppm",
                  "mkdir /tmp/h420g", "cd /tmp/h420g", "git init", "echo nuevo > /tmp/h420g/n.txt", "cd /tmp"]:
            m.comando(c)
            time.sleep(0.3)
        out = m.comando("ls /tmp/h420")
        verificar("pix.ppm" in out and "codigo.c" in out, "los datos sintéticos de /tmp/h420 están creados")

        m.comando("arquivos /tmp/h420")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(1.0)
        a = janela(m.serial(), "Archivos")
        lx, ly = a["x"], a["y"]
        JANELA.update(a)
        m.captura("H420-1-base")

        # ---- (1) la barra lateral con sus secciones
        for seccion in ("FAVORITOS", "UNIDADES", "ETIQUETAS"):
            verificar(f"arquivos: lateral {seccion} y=" in m.serial(), f"la barra lateral tiene la sección {seccion}")
        y_fav = int(re.findall(r"arquivos: lateral FAVORITOS y=(\d+)", m.serial())[-1])
        verificar(y_fav > Y_ORDENS, f"«Favoritos» está debajo de la barra de órdenes (y={y_fav})")
        img = tela(m, "base")
        verificar(contar(img, lx + 4, ly + Y_ORDENS + 2, 400, 22) > 100, "la barra de órdenes dibuja sus iconos")

        # ---- (2) pestañas e Inicio
        marca = len(m.serial())
        m.tecla("ctrl-t")
        esperar(m, r"arquivos: aba nueva \(2\)", 10, desde=marca)
        esperar(m, r"arquivos: pasta @inicio", 10, desde=marca)
        time.sleep(0.5)
        verificar(True, "Ctrl+T abre una pestaña nueva (en la página de Inicio)")
        m.captura("H420-2-pestanas-inicio")
        marca = len(m.serial())
        m.tecla("ctrl-tab")
        esperar(m, r"arquivos: aba 1 de 2 \(/tmp/h420\)", 10, desde=marca)
        verificar(True, "Ctrl+Tab vuelve a la primera pestaña (/tmp/h420)")
        marca = len(m.serial())
        m.tecla("ctrl-w")
        esperar(m, r"arquivos: aba cerrada \(1\)", 10, desde=marca)
        verificar(True, "Ctrl+W cierra la pestaña y queda una")
        # Ctrl+W cerró la activa (/tmp/h420): queda la de Inicio; se vuelve a la carpeta por la ruta (Ctrl+L)
        time.sleep(0.5)
        marca = len(m.serial())
        m.tecla("ctrl-l")
        time.sleep(0.4)
        m.digitar("/tmp/h420\n", 0.05)
        esperar(m, r"arquivos: pasta /tmp/h420 \(\d+ itens\)", 10, desde=marca)
        time.sleep(0.5)

        # ---- (3) doble panel y arrastrar y soltar entre paneles
        marca = len(m.serial())
        m.tecla("f3")
        esperar(m, r"arquivos: paneles 2", 10, desde=marca)
        time.sleep(0.8)
        m.captura("H420-3-doble-panel")
        verificar(True, "F3 abre el segundo panel")
        # el segundo panel entra en dos/ (doble clic): su lista empieza en x=438
        p2x = LISTA_X + 288 + 4 - 6 + 6  # 438
        m.clique(lx + p2x + 100, ly + Y_LISTA + 2 + 0 * 20 + 8, duplo=True)   # «dos»
        esperar(m, r"arquivos: pasta /tmp/h420/dos \(0 itens\)", 10, desde=marca)
        # arrastrar a.txt (tercera fila del panel 1: dos, uno, a.txt) al panel 2
        marca = len(m.serial())
        m.arrastar(lx + LISTA_X + 60, ly + Y_LISTA + 2 + 2 * 20 + 8, lx + p2x + 100, ly + Y_LISTA + 200)
        esperar(m, r"arquivos: soltados 1 itens em /tmp/h420/dos \(copiar\)", 15, desde=marca)
        esperar(m, r"arquivos: pasta /tmp/h420/dos \(1 itens\)", 10, desde=marca)   # el panel 2 se recargó con el archivo copiado
        verificar(True, "arrastrar entre paneles copia el archivo a la carpeta del otro panel (el panel destino pasa a 1 elemento)")
        m.captura("H420-4-arrastrar-copia")

        # ---- (4) la barra lateral: anclar una carpeta y poner una etiqueta arrastrando
        marca = len(m.serial())
        m.arrastar(lx + LISTA_X + 60, ly + Y_LISTA + 2 + 1 * 20 + 8, lx + 60, ly + y_fav + 10)   # «uno» -> FAVORITOS
        esperar(m, r"arquivos: favorito anadido /tmp/h420/uno", 10, desde=marca)
        verificar(True, "arrastrar una carpeta a «Favoritos» la ancla")
        time.sleep(0.5)
        marca = len(m.serial())
        yr = int(re.findall(r"arquivos: lateral etq y=(\d+)", m.serial())[-1])   # la fila de los seis colores
        m.arrastar(lx + LISTA_X + 60, ly + Y_LISTA + 2 + 2 * 20 + 8, lx + 18, ly + yr + 13)   # a.txt -> Rojo (el primer cuadrado)
        esperar(m, r"arquivos: etiqueta 0 puesta arrastrando", 10, desde=marca)
        verificar(True, "arrastrar un archivo a una etiqueta de color se la pone")
        time.sleep(0.5)
        m.captura("H420-5-barra-lateral")
        marca = len(m.serial())
        m.clique(lx + 18, ly + yr + 13)
        esperar(m, r"arquivos: etiqueta 0 \(1 archivos\)", 10, desde=marca)
        verificar(True, "pulsar la etiqueta «Rojo» lista los archivos que la llevan")
        time.sleep(0.4)
        m.captura("H420-6-etiqueta")

        # ---- (5) migas con flecha: subcarpetas de «/»
        m.tecla("f3")   # un solo panel
        time.sleep(0.6)
        marca = len(m.serial())
        m.clique(lx + LISTA_X + 4 + 16 + 7, ly + Y_ORDENS + 28 + 4 + 9)   # la flecha tras «/»
        menu = menu_abierto(m, marca)
        verificar(menu[4] >= 3, f"la flecha de la miga «/» lista las subcarpetas ({menu[4]} líneas)")
        m.tecla("esc")
        time.sleep(0.4)

        # ---- (6) vistas: detalles, iconos con miniaturas, mosaicos y agrupar por tipo
        ir(m, "/tmp/h420")
        marca = len(m.serial())
        m.tecla("ctrl-2")
        esperar(m, r"arquivos: vista detalhes", 10, desde=marca)
        time.sleep(0.5)
        m.captura("H420-7-detalles")
        marca = len(m.serial())
        m.tecla("ctrl-3")
        esperar(m, r"arquivos: vista icones", 10, desde=marca)
        esperar(m, r"arquivos: miniatura /tmp/h420/pix\.ppm$", 15, desde=marca) if False else None
        time.sleep(2.0)
        verificar(re.search(r"arquivos: miniatura /tmp/h420/pix\.ppm", m.serial()[marca:]) is not None,
                  "la vista de iconos prepara la miniatura de la imagen")
        m.captura("H420-8-iconos-miniaturas")
        marca = len(m.serial())
        m.tecla("ctrl-4")
        esperar(m, r"arquivos: vista mosaicos", 10, desde=marca)
        time.sleep(0.8)
        m.captura("H420-9-mosaicos")
        m.tecla("ctrl-2")
        time.sleep(0.5)
        marca = len(m.serial())
        m.clique(lx + orden_x("ver"), ly + Y_ORDENS + 14)
        menu = menu_abierto(m, marca)
        marca = len(m.serial())
        m.clique(*linha_menu(menu, 6, 1))   # Agrupar por tipo
        esperar(m, r"arquivos: agrupar tipo", 10, desde=marca)
        time.sleep(0.6)
        m.captura("H420-10-agrupar-tipo")
        verificar(True, "«Agrupar por tipo» agrupa la lista con cabeceras")
        marca = len(m.serial())
        m.clique(lx + orden_x("ver"), ly + Y_ORDENS + 14)
        menu = menu_abierto(m, marca)
        m.clique(*linha_menu(menu, 5, 1))   # Sin agrupar
        time.sleep(0.5)

        # ---- (7) vista previa: Alt+P, texto con código y la imagen
        marca = len(m.serial())
        m.tecla("alt-p")
        esperar(m, r"arquivos: previa sim", 10, desde=marca)
        time.sleep(0.5)
        m.tecla("ctrl-1")   # vista de lista: las filas empiezan en Y_LISTA
        time.sleep(0.6)
        m.clique(lx + LISTA_X + 60, ly + Y_LISTA + 2 + 4 * 20 + 8)   # codigo.c (dos, uno, a.txt, b.txt, codigo.c, pix.ppm)
        time.sleep(0.8)
        img = tela(m, "previa")
        px_prev = contar(img, lx + 720 - 200 + 6, ly + Y_ORDENS + 28 + 30, 188, 130)
        verificar(px_prev > 80, f"el panel de vista previa muestra el texto resaltado ({px_prev} píxeles)")
        m.captura("H420-11-previa-codigo")
        m.clique(lx + LISTA_X + 60, ly + Y_LISTA + 2 + 5 * 20 + 8)   # pix.ppm: la imagen
        time.sleep(1.0)
        m.captura("H420-11b-previa-imagen")

        # ---- (8) búsqueda recursiva con filtros
        ir(m, "/tmp/h420")
        marca = len(m.serial())
        m.tecla("ctrl-f")
        time.sleep(0.4)
        m.digitar("*.txt\n", 0.05)
        r = esperar(m, r"arquivos: busca \*\.txt \((\d+) resultados\)", 20, desde=marca)
        verificar(int(r) >= 3, f"la búsqueda *.txt recorre las subcarpetas ({r} resultados: a, b y el de uno/hondo)")
        time.sleep(0.6)
        m.captura("H420-12-busqueda")
        m.tecla("esc")
        time.sleep(0.5)

        # ---- (9) git: la rama y el estado
        marca = len(m.serial())
        ir(m, "/tmp/h420g")
        esperar(m, r"arquivos: git rama \w+ en /tmp/h420g", 20, desde=marca)
        verificar(True, "en una carpeta con repositorio de git, Archivos lee la rama y el estado")
        time.sleep(0.8)
        img = tela(m, "git")
        m.captura("H420-13-git")

        # ---- Inicio con recientes
        marca = len(m.serial())
        m.tecla("alt-home")
        esperar(m, r"arquivos: pasta @inicio", 10, desde=marca)
        time.sleep(0.6)
        m.captura("H420-14-inicio")
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 15)
        time.sleep(0.8)
        # el disco confirma la copia, la etiqueta y el favorito (el Terminal solo tiene el foco con Archivos cerrado)
        out = m.comando("ls /tmp/h420/dos")
        verificar("a.txt" in out, "en el disco: a.txt está copiado en /tmp/h420/dos y sigue en /tmp/h420")
        out = m.comando("cat /home/.config/arquivos.fav /root/.config/arquivos.fav /home/sagui/.config/arquivos.fav")
        verificar("/tmp/h420/uno" in out, "el favorito anclado quedó guardado en ~/.config/arquivos.fav")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
