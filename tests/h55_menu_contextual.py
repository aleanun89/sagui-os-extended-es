#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H55 - menú contextual con el botón derecho (D051).

Critério:
  - botão direito na área de trabalho vazia abre o menu (recolocado para
    caber na tela: perto da borda de baixo, abre para cima); clicar em
    "Archivos" abre o programa;
  - clicar fora do menu cancela;
  - menu do Archivos (SIS_MENU_CONTEXTUAL + EV_MENU): Abrir numa pasta,
    Propiedades, Nueva carpeta e Renombrar/Borrar pelo menu do item;
  - pelo teclado: setas + Enter escolhem; Esc cancela (e o programa recebe
    EV_MENU com -1);
  - menu da barra de título: Maximizar e depois Cerrar (EV_FECHAR, como o X);
  - menu do Terminal: Limpiar pantalla; do Editor: Guardar como; do
    Markdown: Vista dividida (e o "Cerrar" da barra de título fecha os dois);
  - menu de um ícone: "Abrir".
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel  # noqa: E402

BORDA, TITULO = 2, 24
LINHA_A, SEP_A = 22, 9   # kernel/gui/menu.c
DIREITO = 2              # mouse_button do monitor do QEMU: 1 esquerdo, 2 direito


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]),
                cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def abrir_menu(m, x, y):
    """Botão direito em (x, y); devolve (x, y, l, a, linhas) do menu aberto."""
    marca = len(m.serial())
    m.clique(x, y, botao=DIREITO)
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=marca)
    return tuple(int(v) for v in g)


def linha(menu, i, separadores_antes=0):
    """Centro da linha i do menu (separadores_antes = quantos separadores há antes dela)."""
    mx, my, ml = menu[0], menu[1], menu[2]
    return mx + ml // 2, my + 2 + (i - separadores_antes) * LINHA_A + separadores_antes * SEP_A + LINHA_A // 2


def item_arquivos(a, i):
    return a["cx"] + 146 + 120, a["cy"] + 70 + 2 + i * 20 + 8   # a lista começa depois da lateral (H34)


def main():
    with Maquina("h55", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        t = janela(m.serial(), "Terminal")

        # ---- área de trabalho vazia: o menu abre para cima perto da borda de baixo
        menu = abrir_menu(m, 600, 730)
        verificar(menu[4] == 12 and menu[1] + menu[3] <= 768 and menu[1] < 730,
                  f"botão direito no fundo abre o menu da área de trabalho ({menu[4]} linhas, con Configuración de pantalla y Resolución desde D066 y Añadir widgets desde D365), recolocado em y={menu[1]} para caber")
        m.mouse_mover(*linha(menu, 1))          # passa o mouse sobre "Archivos": destaque verde
        time.sleep(0.4)
        png = m.captura("H55-1-menu-escritorio")
        x, y = linha(menu, 1)
        verificar(cor_do_pixel(png, menu[0] + 8, y) == (0x0B, 0x7A, 0x3E), "a linha sob o mouse fica destacada em verde")
        fora, sombra = cor_do_pixel(png, menu[0] + menu[2] + 2, menu[1] + 1), cor_do_pixel(png, menu[0] + menu[2] + 2, menu[1] + 8)
        verificar(sum(sombra) < sum(fora) * 0.7, f"e o menu tem sombra ({sombra} ao lado de {fora})")
        marca = len(m.serial())
        m.clique(x, y)
        esperar(m, r"menu: escolhido 1 \(Archivos\)", desde=marca)
        m.esperar_serial("arquivos: pronto", 20)
        verificar(True, "clicar em \"Archivos\" abre o gerenciador de arquivos")
        time.sleep(0.6)
        a = janela(m.serial(), "Archivos")

        # ---- clique fora cancela
        menu = abrir_menu(m, 1000, 650)
        verificar(menu[0] + menu[2] <= 1000 and menu[1] + menu[3] <= 650, f"no canto de baixo à direita, o menu abre para a esquerda e para cima ({menu[0]},{menu[1]})")
        marca = len(m.serial())
        m.clique(40, 740)
        esperar(m, r"menu: cancelado", desde=marca)
        verificar("menu: escolhido" not in m.serial()[marca:], "clicar fora do menu cancela sem escolher nada")

        # ---- Archivos: menu do item (pasta tmp) -> Propiedades
        _rf = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "rootfs")
        # D194: el punto de montaje /apps (SAGUI-APPS) también está en la raíz, como carpeta.
        # D255: /sistema (SAGUI-CONF en /sistema/conf) también está en la raíz.
        raiz = sorted(set(os.listdir(_rf)) | {"apps", "sistema"},
                      key=lambda n: (not (n in ("apps", "sistema") or os.path.isdir(os.path.join(_rf, n))), n))
        i_tmp = raiz.index("tmp")
        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, i_tmp))
        esperar(m, r"arquivos: menu do item tmp", desde=marca)
        verificar(menu[4] == 10, "botão direito numa pasta do Archivos: menu do programa com 10 linhas (Abrir, Renombrar, Borrar, -, Propiedades e, desde a mejora 056, -, Comprimir x2, Extraer x2)")
        time.sleep(0.3)
        m.captura("H55-2-menu-archivos-item")
        marca = len(m.serial())
        m.clique(*linha(menu, 4, 1))
        esperar(m, r"EV_MENU 4 \(codigo 1\)", desde=marca)
        esperar(m, r"arquivos: propriedades de /tmp", desde=marca)
        m.esperar_serial('"Propiedades"', 10)
        verificar(True, "\"Propiedades\" chega ao programa como EV_MENU 4 e mostra o diálogo")
        time.sleep(0.4)
        m.captura("H55-3-propiedades")
        m.tecla("ret")
        time.sleep(0.6)

        # ---- Abrir pelo menu
        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, i_tmp))
        m.clique(*linha(menu, 0))
        esperar(m, r"arquivos: pasta /tmp \(", desde=marca)
        verificar(True, "\"Abrir\" no menu entra na pasta /tmp")

        # ---- menu do fundo: Nueva carpeta
        marca = len(m.serial())
        menu = abrir_menu(m, a["cx"] + 400, a["cy"] + 250)
        esperar(m, r"arquivos: menu do fundo", desde=marca)
        m.captura("H55-4-menu-archivos-fondo")
        m.clique(*linha(menu, 0))
        m.esperar_serial('"Nueva carpeta"', 10)
        time.sleep(0.4)
        m.digitar("menu55\n", 0.05)
        esperar(m, r"arquivos: pasta criada /tmp/menu55", desde=marca)
        verificar(True, "\"Nueva carpeta\" no menu do fundo cria /tmp/menu55")
        time.sleep(0.5)

        # ---- Renombrar pelo menu do item
        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, 0))
        m.clique(*linha(menu, 1))
        m.esperar_serial('"Renombrar"', 10)
        time.sleep(0.4)
        for _ in range(8):
            m.tecla("backspace", 0.04)
        m.digitar("nuevo55\n", 0.05)
        esperar(m, r"arquivos: renomeado /tmp/menu55 -> /tmp/nuevo55", desde=marca)
        verificar(True, "\"Renombrar\" muda o nome para nuevo55")
        time.sleep(0.5)

        # ---- pelo teclado: setas + Enter; e Esc cancela
        marca = len(m.serial())
        abrir_menu(m, a["cx"] + 400, a["cy"] + 250)
        m.tecla("down")
        m.tecla("down")
        m.tecla("ret")
        esperar(m, r"menu: escolhido 1 \(Actualizar\)", desde=marca)
        esperar(m, r"arquivos: pasta /tmp \(1 itens\)", desde=marca)
        verificar(True, "setas + Enter escolhem \"Actualizar\" no menu do fundo")
        marca = len(m.serial())
        abrir_menu(m, a["cx"] + 400, a["cy"] + 250)
        m.tecla("up")
        m.captura("H55-5-menu-teclado")
        m.tecla("esc")
        esperar(m, r"arquivos: menu cancelado", desde=marca)
        s = m.serial()[marca:]
        verificar("menu: cancelado" in s and "EV_MENU -1 (codigo 2)" in s,
                  "Esc fecha o menu e o programa recebe EV_MENU com -1")

        # ---- Borrar pelo menu (com confirmação)
        marca = len(m.serial())
        menu = abrir_menu(m, *item_arquivos(a, 0))
        m.clique(*linha(menu, 2))
        m.esperar_serial('"Borrar"', 10)
        time.sleep(0.4)
        m.tecla("ret")
        esperar(m, r"arquivos: apagado /tmp/nuevo55", desde=marca)
        verificar(True, "\"Borrar\" no menu apaga a pasta (depois de confirmar)")
        time.sleep(0.5)

        # ---- barra de título: Maximizar, depois Cerrar
        marca = len(m.serial())
        menu = abrir_menu(m, a["x"] + 200, a["y"] + 12)
        verificar(menu[4] == 6, "botão direito na barra de título abre o menu da janela (6 linhas)")
        time.sleep(0.3)
        png = m.captura("H55-6-menu-titulo")
        texto = lambda i: {cor_do_pixel(png, menu[0] + 14 + k, linha(menu, *i)[1] + dy) for k in range(0, 60, 3) for dy in (-3, 0, 3)}
        verificar((0x9A, 0xA3, 0xAC) in texto((0,)) and (0x1E, 0x23, 0x28) not in texto((0,)) and (0x1E, 0x23, 0x28) in texto((1,)),
                  "\"Restaurar\" aparece desativado (cinza) numa janela que não está maximizada")
        m.clique(*linha(menu, 2))
        esperar(m, rf"janela {a['id']} maximizada", desde=marca)
        esperar(m, r"arquivos: janela 1020x718", desde=marca)
        verificar(True, "\"Maximizar\" no menu maximiza o Archivos")
        time.sleep(0.5)
        marca = len(m.serial())
        menu = abrir_menu(m, 300, 24 + 12)
        m.clique(*linha(menu, 5, 1))
        esperar(m, rf"fechar janela {a['id']} \(Archivos\) pelo menu", desde=marca)
        m.esperar_serial("(arquivos) terminou", 10)
        verificar(True, "\"Cerrar\" no menu manda EV_FECHAR e o Archivos fecha")
        time.sleep(0.6)

        # ---- Terminal: Limpiar pantalla
        m.comando("echo antes del menu")
        marca = len(m.serial())
        menu = abrir_menu(m, t["cx"] + 300, t["cy"] + 200)
        time.sleep(0.3)
        m.captura("H55-7-menu-terminal")
        m.clique(*linha(menu, 0))
        esperar(m, r"terminal: menu -> 0", desde=marca)
        verificar(True, "o Terminal tem menu próprio; \"Limpiar pantalla\" manda Ctrl+L ao shell")
        time.sleep(0.6)
        out = m.comando("echo depois")
        verificar("depois" in out, "o shell continua respondendo depois do menu")

        # ---- Editor e Markdown: as ações da barra no menu
        for prog, titulo, linha_menu, seps, espera in (
                ("editor", "Editor - sin nombre", 4, 1, '"Guardar como"'),
                ("markdown", "Markdown - sin nombre", 1, 0, "markdown: menu -> 1")):
            m.comando(prog)
            m.esperar_serial(f"{prog}: pronto", 20)
            time.sleep(0.6)
            w = janela(m.serial(), titulo)
            marca = len(m.serial())
            menu = abrir_menu(m, w["cx"] + 300, w["cy"] + 200)
            time.sleep(0.3)
            m.captura(f"H55-{'8' if prog == 'editor' else '9'}-menu-{prog}")
            m.clique(*linha(menu, linha_menu, seps))
            esperar(m, re.escape(espera), desde=marca)
            verificar(True, f"menu do {titulo.split()[0]}: a linha {linha_menu} faz a ação da barra ({espera})")
            if prog == "editor":
                time.sleep(0.4)
                m.tecla("esc")
                time.sleep(0.6)
            marca = len(m.serial())
            menu = abrir_menu(m, w["x"] + 100, w["y"] + 12)
            m.clique(*linha(menu, 5, 1))
            m.esperar_serial(f"{prog}: fechado", 10)
            verificar(True, f"e \"Cerrar\" no menu da barra de título fecha o {titulo.split()[0]} (sem alterações)")
            time.sleep(0.6)

        # ---- ícone: Abrir
        marca = len(m.serial())
        menu = abrir_menu(m, 20 + 48, 40 + 16 + 3 * 84 + 30)   # Calculadora (quarto ícone)
        verificar(menu[4] == 1, "botão direito num ícone: menu com \"Abrir\"")
        m.clique(*linha(menu, 0))
        m.esperar_serial('"Calculadora"', 15)
        verificar(True, "\"Abrir\" no menu do ícone abre a Calculadora")
        time.sleep(0.8)
        m.captura("H55-10-calculadora-por-menu")
    return 0


if __name__ == "__main__":
    sys.exit(main())
