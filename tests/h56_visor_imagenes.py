#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H56 - visor de imágenes (/bin/visor, D054 e D055).

Critério:
 (a) `visor -i` lê todas as imagens de /imagenes (BMP 8, 24 e 32 bits; PNG
     com paleta, RGB entrelaçado e cinza+alfa; PPM P6 e P3; PGM P5) e a soma
     FNV-1a dos pixels é a mesma calculada no host a partir do gerador
     (tools/mkimagens.py): os leitores e o inflate decodificam certo;
 (b) `visor /imagenes/degradado.bmp` abre ajustada à janela; a barra de
     estado mostra nome, tamanho e zoom; a cor no centro confere;
 (c) "1" põe 1:1 (pixels exatos na tela), a roda e +/- mudam o zoom em
     torno do cursor, arrastar e as setas deslocam a imagem ampliada;
 (d) AvPág/RePág e as setas (com a imagem ajustada) passam pelas imagens da
     pasta em ordem alfabética;
 (e) F11 põe em tela cheia (1024x768, fundo preto) e volta;
 (f) o ícone da área de trabalho abre o visor sem arquivo, com o seletor
     das imagens de /imagenes; duplo clique abre uma;
 (g) duplo clique numa .png no Archivos abre o visor.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, reconhecer_texto, SAIDA, RAIZ  # noqa: E402

sys.path.insert(0, os.path.join(RAIZ, "tools"))
import mkimagens  # noqa: E402

BORDA, TITULO = 2, 24
BARRA, RODAPE = 34, 22
NOMES = sorted(mkimagens.arquivos())


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO,
                fx=int(g[3]), fy=int(g[4]))


def fnv(pixels):
    h = 2166136261
    for r, g, b in pixels:
        for v in (r, g, b):
            h ^= v
            h = (h * 16777619) & 0xFFFFFFFF
    return h


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def tela(m):
    ppm = os.path.join(SAIDA, "h56-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


def pixel(t, x, y):
    larg, _, px = t
    i = (y * larg + x) * 3
    return tuple(px[i:i + 3])


def texto_em(t, x0, y0, cols, lins=1):
    larg, _, px = t
    recorte = b"".join(px[((y0 + k) * larg + x0) * 3:((y0 + k) * larg + x0 + cols * 8) * 3] for k in range(lins * 16))
    return reconhecer_texto(cols * 8, lins * 16, recorte)


def perto(a, b, tol):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def roda(m, entalhes):
    """+ para baixo (no monitor do QEMU dz > 0 é para cima)."""
    passo = -1 if entalhes > 0 else 1
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {passo}")
        time.sleep(0.15)
    time.sleep(0.5)


def main():
    with Maquina("h56", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("ayuda visor")
        verificar("visor [-i] [archivo | carpeta]" in out, "ajuda visor explica o uso")

        # ---- (a) todos os formatos, conferidos pixel a pixel pela soma
        for nome in NOMES:
            l, a, px = mkimagens.esperado(nome)
            out = m.comando(f"visor -i /imagenes/{nome} | cat", 60)
            g = re.search(rf"/imagenes/{re.escape(nome)}: (.*), (\d+) x (\d+), suma ([0-9a-f]{{8}})", out)
            verificar(g is not None, f"{nome}: visor -i leu ({g.group(1) if g else out[-200:]!r})")
            verificar((int(g.group(2)), int(g.group(3))) == (l, a) and int(g.group(4), 16) == fnv(px),
                      f"{nome}: {l}x{a}, soma {g.group(4)} = {fnv(px):08x} (todos os pixels certos)")
        out = m.comando("visor -i /imagenes/mandelbrot.png 0 0 160 120 | cat")
        _, _, pxm = mkimagens.esperado("mandelbrot.png")
        esp = "%02x%02x%02x" % pxm[120 * 320 + 160]
        verificar(f"pixel 160,120: {esp}" in out, f"cor de um pixel pedido ({esp})")
        out = m.comando("visor -i /leia-me.txt | cat")
        verificar("formato desconocido" in out, "arquivo que não é imagem: erro claro")

        # ---- (b) abre ajustada
        ini = len(m.serial())
        m.comando("visor /imagenes/degradado.bmp")
        esperar(m, r"visor: aberto /imagenes/degradado.bmp BMP 24 bits 160x120 \(2 de 9\)", 30, ini)
        m.esperar_serial("visor: pronto (/imagenes/degradado.bmp)", 20)
        time.sleep(1.5)
        j = janela(m.serial(), "Visor de imágenes")
        al, aa = j["l"], j["a"] - BARRA - RODAPE
        m.mouse_para(j["x"] + j["l"] - 30, j["y"] + 15)   # o cursor fora da imagem
        time.sleep(0.5)
        t = tela(m)
        rodape = texto_em(t, j["x"] + 8, j["y"] + j["a"] - RODAPE + 3, 80)[0]
        zoom_aj = min(al * 1000 // 160, aa * 1000 // 120) // 10
        verificar("degradado.bmp" in rodape and "160 x 120" in rodape and "BMP 24 bits" in rodape
                  and f"{zoom_aj}% (ajustada)" in rodape and "2 de 9" in rodape,
                  f"barra de estado: {rodape.strip()!r}")
        _, _, pxd = mkimagens.esperado("degradado.bmp")
        c = pixel(t, j["x"] + al // 2, j["y"] + BARRA + aa // 2)
        verificar(perto(c, pxd[60 * 160 + 80], 10), f"centro da janela = centro da imagem ampliada ({c})")
        m.captura("H56-visor-ajustada")

        # ---- (c) 1:1, zoom e deslocamento
        ini = len(m.serial())
        m.tecla("1")
        zx, zy, ox, oy = map(int, esperar(m, r"visor: zoom 100% en \((-?\d+),(-?\d+)\), imagen en \((-?\d+),(-?\d+)\)", 10, ini))
        time.sleep(0.8)
        t = tela(m)
        certos = sum(pixel(t, j["x"] + ox + x, j["y"] + BARRA + oy + y) == pxd[y * 160 + x]
                     for x, y in ((0, 0), (10, 20), (159, 0), (80, 60), (159, 119), (37, 101)))
        verificar(certos == 6, f"1:1: os pixels na tela são os da imagem ({certos}/6, imagem em {ox},{oy})")
        m.mouse_para(j["x"] + al // 2, j["y"] + BARRA + aa // 2)
        ini = len(m.serial())
        roda(m, -2)   # para cima: aproxima
        esperar(m, r"visor: zoom 200% en", 10, ini)
        verificar(True, "roda para cima: 100% -> 150% -> 200%")
        roda(m, 1)
        esperar(m, r"visor: zoom 150% en", 10, ini)
        verificar(True, "roda para baixo: 150%")
        ini = len(m.serial())
        for _ in range(4):
            m.tecla("bracket_right", 0.3)   # "+" no teclado ES
        g = esperar(m, r"visor: zoom 600% en \((-?\d+),(-?\d+)\), imagen en \((-?\d+),(-?\d+)\)", 10, ini)
        ox, oy = int(g[2]), int(g[3])
        verificar(True, f"+ quatro vezes: 600% (imagem 960x720 em {ox},{oy})")
        ini = len(m.serial())
        m.arrastar(j["x"] + 400, j["y"] + 250, j["x"] + 300, j["y"] + 200)
        nx, ny = map(int, esperar(m, r"visor: arrastada a \((-?\d+),(-?\d+)\)", 10, ini))
        verificar((nx, ny) == (max(ox - 100, al - 960), max(oy - 50, aa - 720)),
                  f"arrastar 100,50 desloca a imagem ({ox},{oy} -> {nx},{ny})")
        ini = len(m.serial())
        m.tecla("left")
        g = esperar(m, r"visor: desplazada a \((-?\d+),(-?\d+)\)", 10, ini)
        verificar(int(g[0]) == min(nx + 48, 0), f"seta para a esquerda desloca 48 px ({g[0]})")
        m.captura("H56-visor-zoom")
        m.tecla("slash")   # "-" no teclado ES
        esperar(m, r"visor: zoom 400% en", 10, ini)
        verificar(True, "- diminui o zoom (400%)")

        # ---- (d) outras imagens da pasta
        ini = len(m.serial())
        m.tecla("a")
        esperar(m, r"visor: ajustar", 10, ini)
        m.tecla("pgdn")
        esperar(m, r"visor: aberto /imagenes/hojas.png PNG gris\+alfa 64x64 \(3 de 9\)", 10, ini)
        m.tecla("pgdn")
        esperar(m, r"visor: aberto /imagenes/mandelbrot.png PNG paleta 320x240 \(4 de 9\)", 10, ini)
        verificar(True, "AvPág: hojas.png e mandelbrot.png (ordem alfabética)")
        time.sleep(1)
        m.captura("H56-visor-mandelbrot")
        ini = len(m.serial())
        m.tecla("pgup")
        esperar(m, r"visor: aberto /imagenes/hojas.png", 10, ini)
        m.tecla("left")
        esperar(m, r"visor: aberto /imagenes/degradado.bmp", 10, ini)
        m.tecla("left")
        esperar(m, r"visor: aberto /imagenes/atardecer.png PNG RGB entr. 160x120 \(1 de 9\)", 10, ini)
        m.tecla("left")
        esperar(m, r"visor: aberto /imagenes/vidrio.bmp BMP 32 bits 64x64 \(9 de 9\)", 10, ini)
        verificar(True, "RePág e seta esquerda voltam; da primeira vai para a última")
        m.tecla("end")
        m.tecla("pgup")
        esperar(m, r"visor: aberto /imagenes/selva.ppm PPM 160x120 \(8 de 9\)", 10, ini)
        m.tecla("pgup")
        esperar(m, r"visor: aberto /imagenes/sagui.bmp BMP 8 bits 128x128 \(7 de 9\)", 10, ini)
        time.sleep(1)

        # ---- (e) tela cheia
        ini = len(m.serial())
        m.tecla("f11")
        esperar(m, r"visor: ventana 1024x768 \(pantalla completa\)", 15, ini)
        time.sleep(1.5)
        t = tela(m)
        _, _, pxs = mkimagens.esperado("sagui.bmp")
        verificar(pixel(t, 5, 384) == (0, 0, 0) and pixel(t, 1018, 5) == (0, 0, 0),
                  "tela cheia: faixas pretas dos lados (imagem quadrada)")
        verificar(perto(pixel(t, 512, 3), pxs[0 * 128 + 64], 0) and perto(pixel(t, 512, 766), pxs[127 * 128 + 64], 0),
                  "tela cheia: a imagem ocupa a altura toda (768 px)")
        m.captura("H56-visor-pantalla-completa")
        ini = len(m.serial())
        m.tecla("f11")
        esperar(m, r"visor: ventana (\d+)x(\d+)\r?\n", 15, ini)
        verificar(True, "F11 de novo volta à janela")
        time.sleep(0.5)
        m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)   # X da janela
        m.esperar_serial("visor: fechado", 10)
        time.sleep(0.5)

        # ---- (f) ícone da área de trabalho: seletor
        ini = len(m.serial())
        m.clique(150, 40 + 84 + 30, duplo=True)
        esperar(m, r"\[GUI\] icone Visor -> /bin/visor", 15, ini)
        m.esperar_serial("visor: pronto (sin imagen)", 20)
        time.sleep(1)
        j = janela(m.serial(), "Visor de imágenes")
        m.captura("H56-visor-selector")
        ini = len(m.serial())
        m.clique(j["x"] + 16 + 60, j["y"] + BARRA + 52 + 2 + 2 * 20 + 8, duplo=True)   # 3º item
        esperar(m, r"visor: aberto /imagenes/hojas.png", 10, ini)
        verificar(True, "ícone abre o visor com o seletor; duplo clique abre hojas.png")
        m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)
        time.sleep(1)

        # ---- (g) Archivos
        ini = len(m.serial())
        m.comando("arquivos /imagenes")
        esperar(m, r"arquivos: pronto", 20, ini)
        time.sleep(0.5)
        a = janela(m.serial(), "Archivos")
        i = NOMES.index("mandelbrot.png")
        m.clique(a["x"] + 146 + 120, a["y"] + 70 + 2 + i * 20 + 8, duplo=True)
        esperar(m, r"visor: aberto /imagenes/mandelbrot.png", 20, ini)
        verificar(True, "duplo clique numa .png no Archivos abre o visor")
    return 0


if __name__ == "__main__":
    sys.exit(main())
