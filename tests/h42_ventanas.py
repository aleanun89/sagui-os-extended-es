#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H42 - ventanas redimensionables, maximizar, minimizar y pantalla completa (D044).

Critério:
  - arrastar o canto do Terminal aumenta a janela; o kernel e o Terminal
    registram o tamanho novo e as colunas x linhas novas; o texto continua lá;
  - maximizar e restaurar pelo botão e pelo duplo clique no título;
  - minimizar e restaurar pelo botão da janela na barra de tarefas (D065);
  - o Editor também redimensiona;
  - um programa de tamanho fixo (Calculadora) não muda ao arrastar o canto;
  - F11 põe o Sagui 3D em tela cheia (1024x768, ampliado com faixas pretas)
    e Esc / F11 voltam;
  - um programa pede a tela cheia pela chamada de sistema e recebe 1024x768.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qemu  # noqa: E402
from qemu import Maquina, verificar, cor_do_pixel, tamanho_png, ler_ppm, SAIDA, TERM_MX, TERM_MY, TERM_FILA  # noqa: E402

BORDA, TITULO = 2, 24


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def canto(j):
    """Um ponto dentro da alça do canto de baixo à direita."""
    return j["x"] + j["l"] + 2 * BORDA - 6, j["y"] + j["a"] + TITULO + BORDA - 6


def botao(j, n):
    """Centro do n-ésimo botão da barra de título contando da direita (0 = X)."""
    return j["x"] + j["l"] + 2 * BORDA - BORDA - 20 * (n + 1) + 8, j["y"] + 12


def texto_terminal(m, j, cols, lins):
    """Lê o texto da grade do Terminal na tela (célula de 8x16 a partir da margem)."""
    ppm = os.path.join(SAIDA, "h42-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    larg, _, px = ler_ppm(ppm)
    x, y = j["x"] + BORDA + TERM_MX, j["y"] + TITULO + TERM_MY
    return qemu.texto_terminal(px, larg, x, y, cols, lins)


def botao_na_barra(m, ident):
    """O botão da janela na barra de tarefas (D065), pela última disposição no
    log: devolve (x, largura, estado), estado "f" = foco, "m" = minimizada."""
    linha = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    x, l, e = re.findall(rf" {ident}@(\d+)\+(\d+)([fm]?)", linha)[0]
    return int(x), int(l), e


def esperar(m, padrao, tempo=15):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial())
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def main():
    with Maquina("h42", "ide", extras=["-audiodev", "none,id=som", "-machine", "pcspk-audiodev=som"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        t = janela(m.serial(), "Terminal")
        m.comando("echo hola ventanas")

        # ---- Terminal: arrastar o canto
        x0, y0 = canto(t)
        m.arrastar(x0, y0, x0 + 64, y0 + 112)
        nl, na = t["l"] + 64, t["a"] + 112
        g = esperar(m, rf"janela {t['id']} redimensionada para (\d+)x(\d+)")
        verificar((int(g[0]), int(g[1])) == (nl, na), f"arrastar o canto aumenta o Terminal para {g[0]}x{g[1]}")
        g = esperar(m, r"terminal: janela (\d+)x(\d+), (\d+) colunas x (\d+) linhas")
        cols, lins = (nl - TERM_MX - 12) // 8, (na - 2 * TERM_MY) // TERM_FILA   # D385: márgenes y barra de desplazamiento
        verificar((int(g[0]), int(g[1]), int(g[2]), int(g[3])) == (nl, na, cols, lins),
                  f"o Terminal recalcula a grade: {g[2]} colunas x {g[3]} linhas (antes 94 x 28)")
        time.sleep(0.5)
        t.update(l=nl, a=na)
        m.captura("H42-1-terminal-redimensionado")
        tela = texto_terminal(m, t, cols, lins)
        verificar(any("hola ventanas" in l for l in tela), "o texto anterior continua visível depois de redimensionar")
        out = m.comando("echo " + "x" * 98 + "FIM")
        time.sleep(0.5)
        tela = texto_terminal(m, t, cols, lins)
        verificar(any(l.endswith("xFIM") and len(l) >= 100 for l in tela),
                  "uma linha de 101 letras cabe numa linha só (as colunas novas são usadas)")

        # ---- maximizar e restaurar pelo botão
        m.clique(*botao(t, 1))
        esperar(m, rf"janela {t['id']} maximizada")
        g = esperar(m, r"terminal: janela (1020)x(718), (\d+) colunas x (\d+) linhas")
        verificar(True, f"botão maximizar: o Terminal ocupa a tela ({g[0]}x{g[1]}, {g[2]}x{g[3]} letras)")
        time.sleep(0.5)
        png = m.captura("H42-2-terminal-maximizado")
        verificar(cor_do_pixel(png, 1015, 700) == (0x12, 0x16, 0x1E), "o fundo do Terminal chega ao canto da tela")
        tm = dict(t, x=0, y=24, l=1020, a=718)
        m.clique(*botao(tm, 1))
        esperar(m, rf"janela {t['id']} restaurada")
        esperar(m, rf"terminal: janela {nl}x{na},")
        verificar(True, f"botão restaurar: volta a {nl}x{na}")

        # ---- duplo clique no título: maximiza e restaura
        antes = m.serial().count(f"janela {t['id']} maximizada")
        m.clique(t["x"] + 200, t["y"] + 12, duplo=True)
        time.sleep(0.5)
        verificar(m.serial().count(f"janela {t['id']} maximizada") == antes + 1, "duplo clique no título maximiza")
        esperar(m, r"terminal: janela 1020x718")
        antes = m.serial().count(f"janela {t['id']} restaurada")
        m.clique(300, 24 + 12, duplo=True)
        time.sleep(0.5)
        verificar(m.serial().count(f"janela {t['id']} restaurada") == antes + 1, "outro duplo clique restaura")
        time.sleep(0.5)
        verificar(re.findall(r"terminal: janela (\d+)x(\d+),", m.serial())[-1] == (str(nl), str(na)),
                  "e o Terminal volta ao tamanho de antes")

        # ---- minimizar e restaurar pela barra superior
        m.clique(*botao(t, 2))
        esperar(m, rf"janela {t['id']} minimizada")
        time.sleep(0.3)
        png = m.captura("H42-3-terminal-minimizado")
        verificar(cor_do_pixel(png, t["x"] + 20, t["y"] + 100) != (0x12, 0x16, 0x1E), "minimizado, o Terminal some da tela")
        bx, bl, estado = botao_na_barra(m, t["id"])
        verificar(estado == "m" and cor_do_pixel(png, bx + bl - 3, 10) == (0x1E, 0x27, 0x30),
                  "e o botão dele na barra de tarefas fica com o estilo de minimizada")
        m.clique(bx + bl // 2, 12)
        esperar(m, rf"janela {t['id']} restaurada da barra")
        time.sleep(0.3)
        png = m.captura("h42-restaurado", guardar=False)
        verificar(cor_do_pixel(png, t["x"] + 20, t["y"] + 100) == (0x12, 0x16, 0x1E), "clicar no botão da barra traz o Terminal de volta")

        # ---- Editor
        m.comando("editor /sobre.txt")
        m.esperar_serial("editor: pronto", 20)
        time.sleep(0.5)
        e = janela(m.serial(), "Editor - sin nombre")
        x0, y0 = canto(e)
        m.arrastar(x0, y0, x0 - 120, y0 + 80)
        g = esperar(m, r"editor: janela (\d+)x(\d+), (\d+) linhas visiveis")
        verificar((int(g[0]), int(g[1])) == (e["l"] - 120, e["a"] + 80),
                  f"o Editor também redimensiona ({g[0]}x{g[1]}, {g[2]} linhas visíveis)")
        time.sleep(0.5)
        m.captura("H42-4-editor-redimensionado")
        e.update(l=e["l"] - 120, a=e["a"] + 80)
        m.clique(*botao(e, 1))
        g = esperar(m, r"editor: janela (1020)x(718), (\d+) linhas visiveis")
        verificar(True, f"e maximiza ({g[2]} linhas visíveis)")
        m.clique(*botao(dict(e, x=0, y=24, l=1020, a=718), 0))   # fechar (sem alterações: fecha direto)
        m.esperar_serial("editor: fechado", 10)

        # ---- programa de tamanho fixo: o canto não redimensiona
        m.comando("calculadora")
        m.esperar_serial('"Calculadora"', 15)
        time.sleep(0.8)
        c = janela(m.serial(), "Calculadora")
        x0, y0 = canto(c)
        m.arrastar(x0, y0, x0 + 100, y0 + 100)
        m.clique(*botao(c, 1))      # sem maximizar, este é o botão de minimizar
        time.sleep(0.5)
        s = m.serial()
        verificar(f"janela {c['id']}: pedido de tamanho" not in s and f"janela {c['id']} redimensionada" not in s
                  and f"janela {c['id']} maximizada" not in s,
                  "a Calculadora (tamanho fixo) não muda de tamanho nem maximiza")
        # Sem botão de maximizar, o primeiro botão à esquerda do X é o de minimizar.
        verificar(f"janela {c['id']} minimizada" in s, "ela tem só minimizar e fechar")
        time.sleep(0.3)
        bx, bl, estado = botao_na_barra(m, c["id"])
        verificar(estado == "m", "o botão da Calculadora na barra fica como minimizada")
        m.clique(bx + bl // 2, 12)
        esperar(m, rf"janela {c['id']} restaurada da barra")
        m.clique(*botao(c, 0))
        m.esperar_serial("(calculadora) terminou", 10)

        # ---- Sagui 3D em tela cheia com F11
        m.comando("sagui3d")
        m.esperar_serial("sagui3d: tela de inicio", 20)
        time.sleep(1)
        s3 = janela(m.serial(), "Sagui 3D")
        m.tecla("f11")
        esperar(m, rf"janela {s3['id']} em tela cheia")
        time.sleep(1)
        png = m.captura("H42-5-sagui3d-tela-cheia")
        verificar(tamanho_png(png) == (1024, 768), "captura em 1024x768")
        verificar(cor_do_pixel(png, 500, 5) == (0, 0, 0), "a barra superior sumiu (faixa preta em cima)")
        verificar(cor_do_pixel(png, 3, 300) != (0, 0, 0) and cor_do_pixel(png, 1020, 300) != (0, 0, 0),
                  "o jogo (640x448) foi ampliado até as bordas da tela (1024 de largura)")
        m.tecla("ret")
        m.esperar_serial("sagui3d: inicio do jogo", 10)
        time.sleep(2)
        m.captura("H42-6-sagui3d-jogando-tela-cheia")
        marca = len(m.serial())
        m.tecla("esc")
        esperar(m, rf"janela {s3['id']} saiu da tela cheia")
        time.sleep(0.5)
        verificar("sagui3d: pausa" not in m.serial()[marca:], "Esc sai da tela cheia (e não chega ao jogo)")
        png = m.captura("h42-sagui3d-janela", guardar=False)
        verificar(cor_do_pixel(png, 500, 5) == (0x18, 0x20, 0x2A), "a barra superior voltou")
        m.tecla("f11")
        time.sleep(0.5)
        m.tecla("f11")
        time.sleep(0.5)
        s = m.serial()[marca:]
        verificar(s.count(f"janela {s3['id']} em tela cheia") == 1 and s.count(f"janela {s3['id']} saiu da tela cheia") == 2,
                  "F11 entra e F11 sai")
        m.tecla("esc")
        m.esperar_serial("sagui3d: pausa", 5)
        m.tecla("q")
        time.sleep(1)

        # ---- tela cheia pedida pelo próprio programa
        m.digitar("/testes/tela-cheia\n", 0.04)
        g = esperar(m, r"tela-cheia: tamanho (\d+)x(\d+) estado (\d)")
        verificar(g == ("1024", "768", "2"), "o programa pede a tela cheia e passa a desenhar em 1024x768")
        time.sleep(0.5)
        png = m.captura("H42-7-programa-tela-cheia")
        verificar(cor_do_pixel(png, 512, 400) == (0x2A, 0x6F, 0xD8) and cor_do_pixel(png, 0, 0) == (0xFF, 0xFF, 0xFF),
                  "desenhado 1:1, sem moldura, do canto (0,0) ao (1023,767)")
        m.tecla("t")
        g = esperar(m, r"tela-cheia: tamanho (320)x(200) estado (0)")
        verificar(True, "ao sair da tela cheia volta a 320x200")
        m.tecla("q")
        time.sleep(1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
