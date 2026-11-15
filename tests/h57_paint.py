#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H57 - Paint (/apps/bin/paint, D056).

Critério:
 (a) `paint /tmp/prueba.bmp` (arquivo novo) abre um lienzo branco de 640x480;
 (b) com o mouse: escolher cor, ferramenta e grossura; um retângulo vermelho
     de grossura 3 arrastando; o balde enche o interior de azul e pinta
     exatamente os pixels de dentro; um traço de lápiz; uma elipse cheia
     arrastando com o botão direito (cor secundária, escolhida com o botão
     direito na paleta);
 (c) Ctrl+Z duas vezes desfaz a elipse e o traço (vários níveis);
 (d) Ctrl+S grava o BMP; `visor -i` confere o arquivo: BMP 24 bits 640x480,
     cores de pixels e a soma de todos os pixels igual à do desenho
     simulado no host;
 (e) Ctrl+O abre o BMP gravado; o balde com o botão direito (secundária)
     pinta o interior de verde; a ferramenta de texto escreve "Hola"; Ctrl+S
     grava de novo e a soma confere (o texto com a fonte do sistema);
 (f) o BMP aberto no visor a 1:1 mostra na tela os mesmos pixels;
 (g) `paint /imagenes/sagui.bmp` abre o arquivo; Ctrl+N cria um lienzo novo;
     o ícone da área de trabalho abre o Paint.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA, RAIZ  # noqa: E402

BORDA, TITULO = 2, 24
BARRA, LATERAL, PALETA, ESTADO, MARGEM = 34, 76, 46, 20, 8
VERMELHO, AZUL, VERDE, AMARELO = 3, 8, 6, 5   # índices na paleta do Paint
CORES = {VERMELHO: 0xED1C24, AZUL: 0x3F48CC, VERDE: 0x22B14C, AMARELO: 0xFFF200}
F_LAPIS, F_RET, F_ELIPSE_CHEIA, F_BALDE, F_TEXTO = 0, 5, 8, 9, 11


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO,
                fx=int(g[3]), fy=int(g[4]))


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


class Paint:
    """Coordenadas de tela das partes do Paint."""
    def __init__(self, m, j):
        self.m, self.j = m, j

    def tela(self, x, y):
        return self.j["x"] + LATERAL + MARGEM + x, self.j["y"] + BARRA + MARGEM + y

    def cor(self, i, botao=1):
        py = self.j["y"] + self.j["a"] - ESTADO - PALETA
        self.m.clique(self.j["x"] + 60 + (i % 14) * 20 + 9, py + 4 + (i // 14) * 20 + 9, botao=botao)

    def ferramenta(self, i):
        self.m.clique(self.j["x"] + 6 + (i % 2) * 34 + 16, self.j["y"] + BARRA + 6 + (i // 2) * 34 + 16)

    def grosor(self, i):
        self.m.clique(self.j["x"] + 38, self.j["y"] + BARRA + 6 + 6 * 34 + 22 + i * 20 + 9)

    def arrastar(self, x0, y0, x1, y1, botao=1):
        m = self.m
        m.mouse_para(*self.tela(x0, y0))
        m.monitor(f"mouse_button {botao}")
        time.sleep(0.2)
        m.mouse_mover(*self.tela(x1, y1))
        m.monitor("mouse_button 0")
        time.sleep(0.6)

    def fechar(self):
        self.m.clique(self.j["fx"] + self.j["l"] + 4 - 2 - 20 + 8, self.j["fy"] + 12)


# ------------------------------------------------------------ o desenho simulado no host

def fonte():
    src = open(os.path.join(RAIZ, "build", "gerado", "fonte8x16.c")).read()
    linhas = [l for l in src.splitlines() if l.strip().startswith("{")]
    return [bytes(int(v, 16) for v in re.findall(r"0x([0-9A-F]{2})", l.split("/*")[0])) for l in linhas]


def desenho(interior, texto=None):
    px = [[0xFFFFFF] * 640 for _ in range(480)]
    for y in range(50, 201):
        for x in range(50, 251):
            borda = x < 53 or x > 247 or y < 53 or y > 197
            px[y][x] = CORES[VERMELHO] if borda else interior
    if texto:
        gl = fonte()
        x0, y0, esc = 300, 400, 2
        for k, ch in enumerate(texto):
            g = gl[ord(ch)]
            for lin in range(16):
                for col in range(8):
                    if g[lin] & (0x80 >> col):
                        for dy in range(esc):
                            for dx in range(esc):
                                px[y0 + lin * esc + dy][x0 + k * 8 * esc + col * esc + dx] = 0
    return px


def fnv(px):
    h = 2166136261
    for lin in px:
        for c in lin:
            for v in ((c >> 16) & 255, (c >> 8) & 255, c & 255):
                h ^= v
                h = (h * 16777619) & 0xFFFFFFFF
    return h


def conferir(m, caminho, px, rotulo):
    pontos = ((50, 50), (52, 52), (53, 53), (100, 100), (247, 197), (250, 200), (350, 325), (500, 120), (10, 10), (639, 479))
    args = " ".join(f"{x} {y}" for x, y in pontos)
    out = m.comando(f"visor -i {caminho} {args} | cat", 60)
    g = re.search(rf"{re.escape(caminho)}: BMP 24 bits, 640 x 480, suma ([0-9a-f]{{8}})", out)
    verificar(g is not None, f"{rotulo}: o arquivo é um BMP de 24 bits, 640x480")
    lidos = dict(((int(a), int(b)), int(c, 16)) for a, b, c in re.findall(r"pixel (\d+),(\d+): ([0-9a-f]{6})", out))
    errados = [(p, f"{lidos.get(p, -1):06x}", f"{px[p[1]][p[0]]:06x}") for p in pontos if lidos.get(p) != px[p[1]][p[0]]]
    verificar(not errados, f"{rotulo}: cores dos pixels conferem {errados}")
    verificar(int(g.group(1), 16) == fnv(px), f"{rotulo}: soma de todos os pixels {g.group(1)} = {fnv(px):08x}")


def main():
    with Maquina("h57", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("ajuda paint")
        verificar("paint [archivo.bmp]" in out, "ajuda paint explica o uso")

        # ---- (a)
        ini = len(m.serial())
        m.comando("paint /tmp/prueba.bmp")
        esperar(m, r"paint: pronto, lienzo 640x480 en \(84,42\)", 30, ini)
        time.sleep(1)
        p = Paint(m, janela(m.serial(), "Paint - sin nombre"))
        verificar(True, "paint abre com um lienzo de 640x480")

        # ---- (b) retângulo, balde, lápiz e elipse com o botão direito
        ini = len(m.serial())
        p.cor(VERMELHO)
        p.ferramenta(F_RET)
        p.grosor(1)
        esperar(m, r"paint: grosor 3", 10, ini)
        p.arrastar(50, 50, 250, 200)
        esperar(m, r"paint: trazo rectangulo de \(50,50\) a \(250,200\) color ed1c24", 10, ini)
        verificar(True, "retângulo vermelho de (50,50) a (250,200) arrastando o mouse")
        p.cor(AZUL)
        p.ferramenta(F_BALDE)
        m.clique(*p.tela(100, 100))
        n = int(esperar(m, r"paint: relleno en \(100,100\) con 3f48cc: (\d+) pixeles", 10, ini))
        verificar(n == 195 * 145, f"balde: pintou exatamente o interior ({n} pixels = 195 x 145)")
        p.cor(VERDE)
        p.ferramenta(F_LAPIS)
        p.arrastar(300, 300, 400, 350)
        esperar(m, r"paint: trazo lapiz de \(300,300\) a \(400,350\) color 22b14c", 10, ini)
        verificar(True, "traço de lápiz verde")
        p.cor(AMARELO, botao=2)
        esperar(m, r"paint: color secundario fff200", 10, ini)
        p.ferramenta(F_ELIPSE_CHEIA)
        p.arrastar(420, 60, 600, 180, botao=2)
        esperar(m, r"paint: trazo elipse_rellena de \(420,60\) a \(600,180\) color fff200", 10, ini)
        verificar(True, "elipse cheia amarela com o botão direito (cor secundária)")
        time.sleep(0.5)
        m.captura("H57-paint-dibujo")

        # ---- (c) desfazer dois níveis
        ini = len(m.serial())
        m.tecla("ctrl-z")
        esperar(m, r"paint: deshacer \(quedan 3 niveles\)", 10, ini)
        m.tecla("ctrl-z")
        esperar(m, r"paint: deshacer \(quedan 2 niveles\)", 10, ini)
        verificar(True, "Ctrl+Z duas vezes: sobram o retângulo e o relleno")
        time.sleep(0.5)
        m.captura("H57-paint-deshacer")

        # ---- (d) gravar e conferir
        m.tecla("ctrl-s")
        esperar(m, r"paint: guardado /tmp/prueba.bmp 640x480 BMP 24 bits", 10, ini)
        verificar(True, "Ctrl+S grava /tmp/prueba.bmp")
        p.fechar()
        m.esperar_serial("paint: fechado", 10)
        time.sleep(0.5)
        conferir(m, "/tmp/prueba.bmp", desenho(CORES[AZUL]), "BMP gravado")

        # ---- (e) Ctrl+O, balde com a secundária, texto
        ini = len(m.serial())
        m.comando("paint")
        esperar(m, r"paint: pronto", 30, ini)
        time.sleep(1)
        p = Paint(m, janela(m.serial(), "Paint - sin nombre"))
        m.tecla("ctrl-o")
        esperar(m, r'"Abrir" \d+x\d+', 10, ini)
        time.sleep(0.5)
        for _ in range(12):
            m.tecla("backspace", 0.05)
        m.digitar("/tmp/prueba.bmp", 0.05)
        m.tecla("ret")
        esperar(m, r"paint: abierto /tmp/prueba.bmp BMP 24 bits 640x480", 10, ini)
        verificar(True, "Ctrl+O abre o BMP gravado")
        time.sleep(0.5)
        p.cor(VERDE, botao=2)
        p.ferramenta(F_BALDE)
        m.clique(*p.tela(100, 100), botao=2)
        n = int(esperar(m, r"paint: relleno en \(100,100\) con 22b14c: (\d+) pixeles", 10, ini))
        verificar(n == 195 * 145, "balde com o botão direito: interior verde (secundária)")
        p.ferramenta(F_TEXTO)
        m.clique(*p.tela(300, 400))
        esperar(m, r'"Texto" \d+x\d+', 10, ini)
        time.sleep(0.5)
        m.digitar("Hola", 0.05)
        m.tecla("ret")
        esperar(m, r'paint: texto "Hola" en \(300,400\)', 10, ini)
        verificar(True, "ferramenta de texto escreve Hola")
        time.sleep(0.5)
        m.captura("H57-paint-texto")
        m.tecla("ctrl-s")
        esperar(m, r"paint: guardado /tmp/prueba.bmp", 10, ini)
        p.fechar()
        m.esperar_serial("paint: fechado", 10)
        time.sleep(0.5)
        final = desenho(CORES[VERDE], "Hola")
        conferir(m, "/tmp/prueba.bmp", final, "BMP com texto")

        # ---- (f) no visor, a 1:1, a tela mostra os mesmos pixels
        ini = len(m.serial())
        m.comando("visor /tmp/prueba.bmp")
        esperar(m, r"visor: pronto", 30, ini)
        time.sleep(1)
        v = janela(m.serial(), "Visor de imágenes")
        m.mouse_para(v["x"] + v["l"] - 30, v["y"] + 15)
        m.tecla("1")
        ox, oy = map(int, esperar(m, r"visor: zoom 100% en \(-?\d+,-?\d+\), imagen en \((-?\d+),(-?\d+)\)", 10, ini))
        time.sleep(1)
        ppm = os.path.join(SAIDA, "h57-visor.ppm")
        if os.path.exists(ppm):
            os.remove(ppm)
        m.monitor(f"screendump {ppm}")
        time.sleep(1)
        larg, alt, px = ler_ppm(ppm)
        area_a = v["a"] - 34 - 22
        certos = total = 0
        for y in range(0, 480, 7):
            for x in range(0, 640, 7):
                sx, sy = v["x"] + ox + x, v["y"] + 34 + oy + y
                if 0 <= ox + x < v["l"] and 0 <= oy + y < area_a:
                    total += 1
                    i = (sy * larg + sx) * 3
                    c = final[y][x]
                    certos += tuple(px[i:i + 3]) == ((c >> 16) & 255, (c >> 8) & 255, c & 255)
        verificar(total > 3000 and certos == total, f"visor a 1:1: {certos}/{total} pixels da tela iguais ao desenho")
        m.captura("H57-visor-dibujo")
        m.clique(v["fx"] + v["l"] + 4 - 2 - 20 + 8, v["fy"] + 12)
        m.esperar_serial("visor: fechado", 10)
        time.sleep(0.5)

        # ---- (g) paint arquivo.bmp, Ctrl+N e o ícone
        ini = len(m.serial())
        m.comando("paint /imagenes/sagui.bmp")
        esperar(m, r"paint: abierto /imagenes/sagui.bmp BMP 8 bits 128x128", 30, ini)
        esperar(m, r"paint: pronto, lienzo 128x128", 10, ini)
        verificar(True, "paint /imagenes/sagui.bmp abre o arquivo")
        time.sleep(1)
        m.captura("H57-paint-sagui")
        p = Paint(m, janela(m.serial(), "Paint - sin nombre"))
        m.tecla("ctrl-n")
        esperar(m, r'"Nuevo dibujo" \d+x\d+', 10, ini)
        time.sleep(0.5)
        for _ in range(10):
            m.tecla("backspace", 0.05)
        m.digitar("320x200", 0.05)
        m.tecla("ret")
        esperar(m, r"paint: lienzo nuevo 320x200", 10, ini)
        verificar(True, "Ctrl+N pede o tamanho e cria um lienzo de 320x200")
        p.fechar()
        m.esperar_serial("paint: fechado", 10)
        time.sleep(0.5)
        ini = len(m.serial())
        m.clique(150, 40 + 2 * 84 + 30, duplo=True)
        esperar(m, r"\[GUI\] icone Paint -> /apps/bin/paint", 15, ini)
        esperar(m, r"paint: pronto, lienzo 640x480", 20, ini)
        verificar(True, "o ícone da área de trabalho abre o Paint")
    return 0


if __name__ == "__main__":
    sys.exit(main())
