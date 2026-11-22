#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H455 (D455) - la aplicación Ayuda (/bin/ayuda-gui).

Criterio:
 (a) se abre (ventana «Ayuda»), el índice se descubre solo y tiene las tres categorías y sus documentos
     (guías del kernel y de las aplicaciones, «Programación» con un apartado por lenguaje, juegos);
 (b) abre una guía en español, cambia al inglés (guia-kernel_EN.md) y al portugués por un enlace;
 (c) sigue un enlace entre documentos, y atrás (<) vuelve al anterior;
 (d) busca en el documento (coincidencias) y en toda la ayuda (lista de resultados; un clic abre el documento);
 (e) copia un bloque de código con su botón;
 (f) un documento nuevo con cabecera <!-- categoria: ...; orden: ... --> aparece solo en el índice
     (se prueba en la parte de consola con el índice en texto);
 (g) F1 en el escritorio abre la Ayuda; `ayuda -g TEMA` abre el tema y, en la consola de texto, imprime el documento.
Capturas sintéticas: H455-indice, H455-guia-es, H455-guia-en, H455-busqueda, H455-resultados, H455-codigo.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

BARRA = 36
FILA0 = BARRA + 4      # primera fila del índice, dentro de la ventana
FILA = 20


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24, fx=int(m[2]), fy=int(m[3]))


def esperar_n(m, texto, n, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        if m.serial().count(texto) >= n:
            return
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {n}x {texto!r}")


def filas(serial):
    """Las filas del último volcado del índice: [(tipo, etiqueta)]."""
    ult = serial.rfind("ayuda: fila 0 ")
    return [(t, e.strip()) for _, t, e in re.findall(r"ayuda: fila (\d+) (\w+) (.*)", serial[ult:])
            if True]


def fila_de(serial, etiqueta):
    f = filas(serial)
    for i, (_, e) in enumerate(f):
        if e == etiqueta:
            return i
    raise AssertionError(f"no hay fila {etiqueta!r} en {f}")


def clic_fila(m, j, i):
    m.clique(j["x"] + 60, j["y"] + FILA0 + i * FILA + 10)


def parte_consola():
    with Maquina("h455-consola", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("ayuda -g lua", tempo=60)
        time.sleep(1.5)
        out = m.serial()
        verificar("lua: el lenguaje Lua 5.4" in out or "el lenguaje Lua 5.4 dentro de SaguiOS" in out,
                  "sin escritorio, «ayuda -g lua» imprime el documento en texto")


def parte_grafica():
    with Maquina("h455", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        # un documento nuevo con cabecera aparece solo en el índice (D455)
        m.comando("mkdir -p /ayuda/lenguajes")
        m.comando("echo '<!-- categoria: Programación/Zig; orden: 5 -->' > /ayuda/lenguajes/zig.md")
        m.comando("echo '# Zig: lenguaje de prueba' >> /ayuda/lenguajes/zig.md")
        m.comando("ayuda-gui")
        m.esperar_serial("ayuda: pronto", 40)
        s = m.serial()
        n = int(re.findall(r"ayuda: indice (\d+) documentos, (\d+) categorias", s)[-1][0])
        cats = int(re.findall(r"ayuda: indice (\d+) documentos, (\d+) categorias", s)[-1][1])
        verificar(n >= 34, f"el índice descubre {n} documentos")
        verificar(cats >= 3, f"hay {cats} categorías")
        f = filas(s)
        etiquetas = [e for _, e in f]
        for c in ("Conoce SaguiOS", "Programación", "Juegos y aplicaciones"):
            verificar(c in etiquetas, f"categoría «{c}» en el índice")
        verificar(any(t == "doc" and "kernel" in e.lower() for t, e in f), "la guía del kernel está en el índice")
        verificar("Zig" in etiquetas, "un .md nuevo con cabecera aparece solo en el índice (Programación/Zig)")
        verificar("Lua" in etiquetas and "Pascal" in etiquetas and "C" in etiquetas, "un apartado por lenguaje (C, Pascal, Lua...)")
        j = janela(s, "Ayuda")
        time.sleep(1)
        m.mouse_para(j["x"] + 880, j["y"] + 400)
        m.captura("H455-indice")

        # ---- una guía en español
        i = fila_de(s, next(e for t, e in f if t == "doc" and "kernel" in e.lower()))
        clic_fila(m, j, i)
        m.esperar_serial("ayuda: abierto /ayuda/guias/guia-kernel.md", 20)
        verificar(True, "abre la guía del kernel en español")
        time.sleep(1)
        s = m.serial()
        verificar(int(re.findall(r"guia-kernel\.md \((\d+) bytes, (\d+) titulos", s)[-1][1]) > 20, "la guía tiene sus títulos")
        m.mouse_para(j["x"] + 880, j["y"] + 400)
        m.captura("H455-guia-es")

        # ---- idioma: ES -> EN con el botón
        m.clique(j["x"] + 162, j["y"] + 18)
        m.esperar_serial("ayuda: abierto /ayuda/guias/guia-kernel_EN.md", 20)
        verificar("ayuda: idioma EN" in m.serial(), "el botón de idioma cambia a inglés y abre guia-kernel_EN.md")
        time.sleep(1)
        m.mouse_para(j["x"] + 880, j["y"] + 400)
        m.captura("H455-guia-en")

        # ---- enlace entre documentos: la línea de idiomas de la guía (Português)
        s = m.serial()
        enl = re.findall(r"ayuda: enlace-en (\S+) en (\d+),(\d+)", s[:s.rfind("ayuda: abierto /ayuda/guias/guia-kernel_EN.md")])
        pt = [e for e in enl if e[0] == "guia-kernel_PT.md"][-1]
        m.clique(j["x"] + 241 + int(pt[1]) + 12, j["y"] + BARRA + int(pt[2]) + 8)
        m.esperar_serial("ayuda: abierto /ayuda/guias/guia-kernel_PT.md", 20)
        verificar(True, "un clic en un enlace abre otro documento (guia-kernel_PT.md)")
        # ---- atrás
        m.clique(j["x"] + 8 + 14, j["y"] + 18)
        m.esperar_serial("ayuda: historial atras /ayuda/guias/guia-kernel_EN.md", 20)
        verificar(True, "atrás vuelve al documento anterior")
        m.clique(j["x"] + 8 + 32 + 14, j["y"] + 18)
        m.esperar_serial("ayuda: historial adelante /ayuda/guias/guia-kernel_PT.md", 20)
        verificar(True, "adelante vuelve al siguiente")
        m.clique(j["x"] + 162, j["y"] + 18)   # PT -> ES
        m.esperar_serial("ayuda: idioma ES", 20)

        # ---- buscar en el documento
        m.clique(j["x"] + 360, j["y"] + 18)
        time.sleep(0.3)
        m.digitar("kernel", 0.08)
        esperar_n(m, "ayuda: buscar 'kernel'", 1, 20)
        c = int(re.findall(r"ayuda: buscar 'kernel' (\d+) coincidencias", m.serial())[-1])
        verificar(c >= 3, f"buscar en el documento: {c} coincidencias")
        time.sleep(0.5)
        m.mouse_para(j["x"] + 880, j["y"] + 400)
        m.captura("H455-busqueda")

        # ---- buscar en toda la ayuda (Ctrl+G) y abrir un resultado
        m.tecla("ctrl-g")
        m.esperar_serial("ayuda: busqueda global 'kernel'", 30)
        r = re.findall(r"ayuda: busqueda global 'kernel' (\d+) resultados en (\d+) documentos", m.serial())[-1]
        verificar(int(r[0]) >= 3 and int(r[1]) >= 2, f"búsqueda global: {r[0]} resultados en {r[1]} documentos")
        time.sleep(0.5)
        m.mouse_para(j["x"] + 880, j["y"] + 400)
        m.captura("H455-resultados")
        m.clique(j["x"] + 100, j["y"] + BARRA + 28 + 10)
        m.esperar_serial("ayuda: resultado 0 abre", 20)
        verificar(True, "un clic en un resultado abre el documento")

        # ---- cierra la Ayuda (Alt+F4) y F1 en el escritorio sin ventanas la abre de nuevo
        m.tecla("alt-f4")
        m.esperar_serial("ayuda: fechado", 20)
        m.tecla("meta_l-d")          # Windows+D: minimiza todo, el escritorio queda sin ventana con foco
        time.sleep(1)
        n0 = m.serial().count("ayuda: pronto")
        m.tecla("f1")
        esperar_n(m, "ayuda: pronto", n0 + 1, 30)
        verificar(True, "F1 en el escritorio abre la Ayuda")
        m.tecla("alt-f4")
        esperar_n(m, "ayuda: fechado", 2, 20)
        m.tecla("meta_l-d")          # restaura el Terminal
        time.sleep(1)

        # ---- copiar un bloque de código
        m.comando("ayuda-gui tcc")
        m.esperar_serial("ayuda: abierto /ayuda/tcc.md", 30)
        time.sleep(1)
        s = m.serial()
        j2 = janela(s, "Ayuda")
        bl = re.findall(r"ayuda: bloque (\d+) en (\d+),(\d+),(\d+)", s[:s.rfind("ayuda: abierto /ayuda/tcc.md")])
        nb = min(4, int(re.findall(r"tcc\.md \(\d+ bytes, \d+ titulos, (\d+) bloques", s)[-1]))
        bl = bl[len(bl) - nb:] if nb else []
        bl = bl[-int(re.findall(r"tcc\.md \(\d+ bytes, \d+ titulos, (\d+) bloques", s)[-1]):][:4]
        verificar(len(bl) >= 1, f"tcc.md tiene bloques de código ({len(bl)})")
        _, bx, by, bw = map(int, bl[0])
        m.clique(j2["x"] + 241 + bx + bw - 30, j2["y"] + BARRA + by + 12)
        m.esperar_serial("ayuda: copiado bloque 0", 20)
        verificar(True, "el botón Copiar copia el bloque de código")
        m.mouse_para(j2["x"] + 880, j2["y"] + 400)
        m.captura("H455-codigo")
        m.tecla("alt-f4")
        esperar_n(m, "ayuda: fechado", 3, 20)

        # ---- imágenes (D455): ruta relativa al .md, escalada a la columna, texto debajo; si falta, [imagen: ...]
        m.comando("mkdir -p /ayuda/sistemas-operativos")
        for linea in ("# Prueba de imagenes", "", "![Anillos de proteccion](img/anillos.png)", "",
                      "![Falta](img/no-existe.png)", "", "Texto despues de las imagenes."):
            m.comando(f"echo '{linea}' >> /ayuda/sistemas-operativos/prueba-imagenes.md")
        m.comando("ayuda-gui /ayuda/sistemas-operativos/prueba-imagenes.md")
        m.esperar_serial("ayuda: abierto /ayuda/sistemas-operativos/prueba-imagenes.md", 30)
        time.sleep(1)
        s = m.serial()
        pat = r"markdown: imagen /ayuda/sistemas-operativos/img/anillos\.png (\d+)x(\d+) dibujada a (\d+)x(\d+) en (\d+),(\d+)"
        r1 = re.findall(pat, s)
        verificar(len(r1) >= 1, "la imagen con ruta relativa al .md se lee y se maqueta")
        nl, na, dl, da, _, _ = map(int, r1[-1])
        verificar(dl <= nl and abs(da * nl - na * dl) <= nl, f"escalada al ancho de la columna ({nl}x{na} -> {dl}x{da}), sin agrandar")
        verificar("markdown: imagen no legible /ayuda/sistemas-operativos/img/no-existe.png" in s, "una imagen que falta queda como texto")
        j3 = janela(s, "Ayuda")
        m.mouse_para(j3["x"] + 880, j3["y"] + 400)
        m.captura("H455-imagenes")
        n_antes = len(r1)
        m.clique(j3["x"] + 123, j3["y"] + 18)   # botón A+: letra 2x: la maquetación se rehace con la mitad de ancho y la imagen sigue entera
        m.esperar_serial("ayuda: tamano de letra 2x", 10)
        time.sleep(1)
        r2 = re.findall(pat, m.serial())
        verificar(len(r2) > n_antes, "con la letra 2x la imagen se maqueta de nuevo")
        dl2 = int(r2[-1][2])
        verificar(dl2 <= dl, f"con la letra 2x la columna es la mitad: la imagen ({dl2}) no es más ancha que antes ({dl}) en unidades de maquetación")
        m.mouse_para(j3["x"] + 880, j3["y"] + 400)
        m.captura("H455-imagenes-2x")


if __name__ == "__main__":
    parte_consola()
    parte_grafica()
    print("H455 ok")
