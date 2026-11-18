#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H45 - rueda del ratón y Markdown redimensionable (D048).

Critério:
  (a) PS/2 (vmport=off, como no G470): o kernel ativa o modo IntelliMouse
      (ID 3, pacotes de 4 bytes); `mouse_move 0 0 dz` do monitor gira a roda
      e o EV_RODA vai para a janela debaixo do cursor;
  (b) o Terminal guarda as linhas que sobem (historial) e a roda volta
      nelas; uma tecla volta para baixo;
  (c) a roda rola o visor do Markdown (3 linhas de 16 px por entalhe), o
      área de texto do Editor e a lista do Archivos;
  (d) o Markdown se redimensiona arrastando o canto e maximiza pelo botão:
      a maquetação é refeita com a largura nova (menos largura = mais linhas);
  (e) com o vmmouse (vmport=on), a roda chega pela quarta palavra da fila.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, ler_ppm, texto_terminal, SAIDA, TERM_MX, TERM_MY  # noqa: E402
from h36_vmmouse import Qmp, ABS_MAX  # noqa: E402

BORDA, TITULO = 2, 24


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def canto(j):
    return j["x"] + j["l"] + 2 * BORDA - 6, j["y"] + j["a"] + TITULO + BORDA - 6


def botao(j, n):
    return j["x"] + j["l"] + 2 * BORDA - BORDA - 20 * (n + 1) + 8, j["y"] + 12


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def roda(m, entalhes):
    """Gira a roda: + para baixo. (No monitor do QEMU, dz > 0 é "para cima".)"""
    passo = -1 if entalhes > 0 else 1
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {passo}")
        time.sleep(0.15)
    time.sleep(0.4)


def texto_janela(m, j, x, y, cols, lins):
    ppm = os.path.join(SAIDA, "h45-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    larg, _, px = ler_ppm(ppm)
    x0, y0 = j["x"] + BORDA + x, j["y"] + TITULO + y
    return texto_terminal(px, larg, x0, y0, cols, lins)


def parte_ps2():
    with Maquina("h45", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        verificar("[MOUSE] IntelliMouse (id 3): roda ligada, pacotes de 4 bytes" in s and "id=3" in s,
                  "o kernel detecta o IntelliMouse (ID 3) no PS/2 do QEMU")

        # ---- (a) EV_RODA para a janela debaixo do cursor
        t = janela(s, "Terminal")
        m.mouse_para(t["x"] + 300, t["y"] + 200)
        marca = len(m.serial())
        roda(m, 1)
        g = esperar(m, r"\[GUI\] roda (-?\d+) para janela (\d+) \((.*?)\) em", desde=marca)
        verificar("[MOUSE] roda 1 em" in m.serial()[marca:] and g[0] == "1" and int(g[1]) == t["id"],
                  "um entalhe para baixo chega como EV_RODA +1 ao Terminal (debaixo do cursor)")

        # ---- (b) historial do Terminal
        m.comando("cat /ejemplos/ejemplo.md")
        m.comando("echo fin del ejemplo")
        time.sleep(0.5)
        m.mouse_para(t["x"] + 300, t["y"] + 200)
        roda(m, -2)
        g = esperar(m, r"terminal: historial (\d+) lineas arriba \(de (\d+)\)")
        verificar(g[0] == "6" and int(g[1]) >= 50, f"dos entalhes hacia arriba: historial {g[0]} líneas arriba (de {g[1]})")
        roda(m, -10)
        g = esperar(m, r"terminal: historial (\d+) lineas arriba \(de (\d+)\)")
        verificar(g[0] == "36", f"más arriba en el historial: {g[0]} líneas")
        time.sleep(0.5)
        m.captura("H45-1-terminal-historial")
        tela = texto_janela(m, t, TERM_MX, TERM_MY, 94, 28)
        verificar(any("Historial: -36" in l for l in tela), "aviso «Historial: -36» en la esquina")
        verificar(not any("fin del ejemplo" in l and "echo" not in l for l in tela),
                  "volviendo atrás, la salida reciente ya no se ve")
        m.tecla("spc")
        time.sleep(0.6)
        tela = texto_janela(m, t, TERM_MX, TERM_MY, 94, 28)
        verificar(not any("Historial" in l for l in tela) and any("fin del ejemplo" in l for l in tela),
                  "una tecla vuelve abajo (la salida reciente otra vez a la vista)")
        m.tecla("backspace")

        # ---- (c) Markdown: a roda rola o visor
        m.comando("markdown /ejemplos/ejemplo.md")
        m.esperar_serial("markdown: pronto (modo visor)", 30)
        time.sleep(0.8)
        md = janela(m.serial(), "Markdown - sin nombre")
        m.mouse_para(md["x"] + 400, md["y"] + 300)
        roda(m, 3)
        g = esperar(m, r"markdown: rueda 1, visor en (\d+) px de (\d+)")
        verificar(g[0] == "144", f"tres entalhes: el visor baja 3 x 3 líneas de 16 px ({g[0]} px)")
        roda(m, -1)
        g = esperar(m, r"markdown: rueda -1, visor en (\d+) px")
        verificar(g == "96", "un entalhe hacia arriba sube 48 px")
        m.mouse_para(md["x"] + 790, md["y"] + 300)
        m.captura("H45-2-markdown-rueda")

        # ---- (d) redimensionar arrastando o canto
        x0, y0 = canto(md)
        m.arrastar(x0, y0, x0 - 220, y0 - 120)
        g = esperar(m, rf"janela {md['id']} redimensionada para (\d+)x(\d+)")
        nl, na = md["l"] - 220, md["a"] - 120
        verificar((int(g[0]), int(g[1])) == (nl, na), f"arrastrar la esquina: Markdown pasa a {g[0]}x{g[1]}")
        g = esperar(m, r"markdown: ventana (\d+)x(\d+), visor de (\d+) px de ancho")
        verificar(g == (str(nl), str(na), str(nl - 12)), f"Markdown rehace los paneles (visor de {g[2]} px)")
        mq = re.findall(r"markdown: maquetado \d+ bloques, (\d+) lineas, \d+ elementos, alto (\d+) px, ancho (\d+)", m.serial())
        verificar(mq[-1][2] == str(nl - 12) and int(mq[-1][0]) > int(mq[0][0]),
                  f"maquetado de nuevo a {mq[-1][2]} px: {mq[0][0]} -> {mq[-1][0]} líneas (texto reajustado)")
        time.sleep(0.5)
        md.update(l=nl, a=na)
        m.mouse_para(md["x"] + nl - 20, md["y"] + 200)
        png = m.captura("H45-3-markdown-estrecho")
        verificar(cor_do_pixel(png, md["x"] + BORDA + nl - 6, md["y"] + TITULO + na - 30) != (0xFF, 0xFF, 0xFF),
                  "la barra de desplazamiento sigue en el borde derecho")

        # ---- maximizar pelo botão
        m.clique(*botao(md, 1))
        esperar(m, rf"janela {md['id']} maximizada")
        g = esperar(m, r"markdown: ventana (1020)x(718), visor de (\d+) px de ancho")
        verificar(g[2] == "1008", "botón maximizar: Markdown ocupa la pantalla (visor de 1008 px)")
        mq2 = re.findall(r"markdown: maquetado \d+ bloques, (\d+) lineas, \d+ elementos, alto (\d+) px, ancho (\d+)", m.serial())
        verificar(mq2[-1][2] == "1008" and int(mq2[-1][0]) < int(mq[0][0]),
                  f"maquetado a 1008 px: {mq2[-1][0]} líneas (menos que a 788 px: {mq[0][0]})")
        time.sleep(0.5)
        m.mouse_para(1000, 400)
        png = m.captura("H45-4-markdown-maximizado")
        verificar(cor_do_pixel(png, 1015, 700) not in ((0x12, 0x16, 0x1E),), "la ventana llega a la esquina de la pantalla")
        marca = len(m.serial())
        roda(m, 2)
        esperar(m, r"markdown: rueda 1, visor en (\d+) px", desde=marca)
        verificar(True, "la rueda sigue funcionando maximizado")
        # vista dividida maximizada: a roda no editor (metade esquerda)
        m.tecla("ctrl-e")
        m.esperar_serial("markdown: modo editor", 10)
        mx = dict(md, x=0, y=24, l=1020, a=718)
        m.clique(mx["x"] + BORDA + 434 + 36, mx["y"] + TITULO + 6 + 12)     # Dividir
        m.esperar_serial("markdown: modo dividido", 10)
        m.mouse_para(200, 400)
        roda(m, 2)
        g = esperar(m, r"markdown: rueda 1, editor en la linea (\d+) de (\d+)")
        verificar(g[0] in ("3", "6"), f"en la vista dividida, la rueda sobre el editor lo desplaza (línea {g[0]})")
        m.mouse_para(800, 400)
        roda(m, 1)
        esperar(m, r"markdown: rueda 1, visor en (\d+) px")
        verificar(True, "y sobre la vista previa desplaza la vista previa")
        time.sleep(0.4)
        m.captura("H45-5-markdown-dividido-maximizado")
        m.clique(*botao(mx, 1))                                              # restaurar
        esperar(m, rf"janela {md['id']} restaurada")
        g = esperar(m, rf"markdown: ventana ({nl})x({na}),")
        verificar(True, f"restaurar vuelve a {nl}x{na}")
        m.clique(*botao(md, 0))
        m.esperar_serial("markdown: fechado", 10)

        # ---- Editor: área de texto
        m.comando("editor /ejemplos/rendimiento.c")
        m.esperar_serial("editor: pronto", 20)
        time.sleep(0.6)
        e = janela(m.serial(), "Editor - sin nombre")
        m.mouse_para(e["x"] + 200, e["y"] + 200)
        roda(m, 2)
        g = esperar(m, r"gui: area rolada pela roda \(1\) para a linha (\d+) de (\d+)")
        verificar(g[0] == "6", f"Editor: dos entalhes bajan el área de texto a la línea {g[0]} (de {g[1]})")
        time.sleep(0.3)
        m.captura("H45-6-editor-rueda")
        m.clique(*botao(e, 0))
        m.esperar_serial("editor: fechado", 10)

        # ---- Archivos: lista
        m.comando("arquivos /bin")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.6)
        a = janela(m.serial(), "Archivos")
        m.mouse_para(a["x"] + 300, a["y"] + 200)
        roda(m, 1)
        g = esperar(m, r"gui: lista rolada pela roda \(1\) para o item (\d+) de (\d+)")
        verificar(g[0] == "3", f"Archivos: un entalhe baja la lista 3 elementos (de {g[1]})")
        m.clique(*botao(a, 0))
        time.sleep(0.5)


def parte_vmmouse():
    qmp = os.path.join(SAIDA, "h45.qmp")
    if os.path.exists(qmp):
        os.remove(qmp)
    with Maquina("h45-vmmouse", "ide", vmmouse=True, extras=["-qmp", f"unix:{qmp},server,nowait"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("vmmouse: posicao absoluta" in m.serial(), "vmmouse activo")
        t = janela(m.serial(), "Terminal")
        q = Qmp(qmp)
        fx, fy = (t["x"] + 300) / 1024, (t["y"] + 200) / 768
        q.comando("input-send-event", events=[{"type": "abs", "data": {"axis": "x", "value": int(fx * ABS_MAX)}},
                                              {"type": "abs", "data": {"axis": "y", "value": int(fy * ABS_MAX)}}])
        time.sleep(0.4)
        marca = len(m.serial())
        for b in ("wheel-down", "wheel-up"):
            for baixo in (True, False):
                q.comando("input-send-event", events=[{"type": "btn", "data": {"down": baixo, "button": b}}])
                time.sleep(0.2)
        time.sleep(0.5)
        s = m.serial()[marca:]
        verificar("[MOUSE] roda 1 em" in s and "[MOUSE] roda -1 em" in s,
                  "con vmmouse la rueda llega en la cuarta palabra (+1 abajo, -1 arriba)")
        verificar(f"para janela {t['id']} (Terminal)" in s, "y va a la ventana bajo el cursor")


def main():
    parte_ps2()
    parte_vmmouse()
    return 0


if __name__ == "__main__":
    sys.exit(main())
