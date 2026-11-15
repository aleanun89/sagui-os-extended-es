#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H60 - barra de tareas, menú Inicio, submenús y resolución desde el escritorio (mejora 031, D064-D066).

Critério:
  - a barra superior tem o botão Inicio (verde, com o sagui), um botão por
    janela (o da janela com o foco destacado) e o relógio à direita;
  - menu Inicio pelo mouse: Inicio > Accesorios > Calculadora abre a
    Calculadora (submenu ao passar o mouse); "Apagar..." está ativo (D113);
  - pelo teclado: Ctrl+Esc abre com a primeira linha destacada, setas até
    "Gráficos", seta à direita abre o submenu, à esquerda fecha, Enter abre
    o Paint; a tecla Windows abre e fecha o menu;
  - botões das tarefas: clique traz para cima, outro clique minimiza (botão
    "apagado"), outro restaura; botão direito abre o menu da barra de título
    (Restaurar numa minimizada, Cerrar);
  - "Ejecutar..." abre o diálogo (que não ganha botão) e lança o Editor;
    "Reiniciar..." pergunta antes (e Esc não reinicia); o relógio mostra a data;
  - menu da área de trabalho: "Configuración de pantalla..." abre a janela
    Pantalla; o submenu "Resolución" marca a atual e troca para 800x600 e
    640x480 a quente, com a barra recolocada;
  - muitas janelas: em 640x480 aparece o "»" com as que não cabem (e o menu
    dele traz uma de volta); em 1280x1024 e 1024x768 os botões estreitam
    e cabem todos.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, tamanho_png  # noqa: E402

BORDA, TITULO = 2, 24
LINHA_A, SEP_A = 22, 9          # kernel/gui/menu.c
DIREITO = 2
BARRA = (0x18, 0x20, 0x2A)      # kernel/gui/barra.c
VERDE_INICIO = (0x0B, 0x7A, 0x3E)
MARCA_FOCO = (0xFF, 0xDF, 0x4A)
MINIMIZADA = (0x1E, 0x27, 0x30)
TAREFA_MIN, TAREFA_MAX = 72, 170


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


# ---------------------------------------------------------------- barra

RE_BARRA = re.compile(r"\[GUI\] barra (\d+): inicio (\d+)\+(\d+), (\d+) tarefas:([^,\n]*)"
                      r"(?:, mais (\d+) \((\d+) ocultas\))?, relogio (\d+)")


def ler_barra(linha):
    g = RE_BARRA.match(linha)
    botoes = [dict(id=int(i), x=int(x), l=int(l), estado=e) for i, x, l, e in re.findall(r"(\d+)@(\d+)\+(\d+)([fm]?)", g[5])]
    return dict(L=int(g[1]), n=int(g[4]), botoes=botoes, mais=int(g[6]) if g[6] else None,
                ocultas=int(g[7]) if g[7] else 0, relogio=int(g[8]))


def esperar_barra(m, cond, desde=0, tempo=20):
    """A última disposição da barra no log que satisfaz cond."""
    fim = time.time() + tempo
    while time.time() < fim:
        for l in reversed(re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial()[desde:])):
            b = ler_barra(l)
            if cond(b):
                return b
            break   # só vale a mais recente
        time.sleep(0.2)
    raise AssertionError("a barra não chegou ao estado esperado: " + re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1])


def botao(b, ident):
    """O botão da janela ident (estado None se ela não tem botão)."""
    return next((x for x in b["botoes"] if x["id"] == ident), dict(id=ident, x=-1, l=0, estado=None))


def focar(m, ident):
    """Clica no botão da janela se ela ainda não tem o foco (com o foco, o clique minimizaria)."""
    b = esperar_barra(m, lambda b: True)
    if botao(b, ident)["estado"] != "f":
        m.clique(*centro(botao(b, ident)))
        esperar_barra(m, lambda b: botao(b, ident)["estado"] == "f")


def centro(bt):
    return bt["x"] + bt["l"] // 2, 12


# ---------------------------------------------------------------- menus

def painel(m, desde, sub=None):
    """O último painel aberto depois de "desde": (x, y, l, a, linhas)."""
    if sub is None:
        g = esperar(m, r"menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com \d+ linhas\r?\n\[GUI\] menu linhas: ([^\r\n]*)", desde=desde)
    else:
        g = esperar(m, rf"submenu \d+ \({re.escape(sub)}\) aberto em \((\d+),(\d+)\) (\d+)x(\d+) com \d+ linhas\r?\n"
                       r"\[GUI\] menu linhas: ([^\r\n]*)", desde=desde)
    return dict(x=int(g[0]), y=int(g[1]), l=int(g[2]), a=int(g[3]), linhas=g[4].split("|"))


def y_linha(p, nome):
    """Centro (y) da linha "nome" (com os prefixos do log: > ~ *)."""
    y = p["y"] + 2
    for l in p["linhas"]:
        if l == nome:
            return y + LINHA_A // 2
        y += SEP_A if l == "-" else LINHA_A
    raise AssertionError(f"{nome!r} não está no menu: {p['linhas']}")


def clicar_aqui(m, botao_mouse=1):
    """Clica onde o cursor já está (m.clique passa pelo canto da tela e
    atravessaria o menu, abrindo e fechando submenus pelo caminho)."""
    m.monitor(f"mouse_button {botao_mouse}")
    time.sleep(0.05)
    m.monitor("mouse_button 0")
    time.sleep(0.4)


def submenu_pelo_mouse(m, p, cabecalho, item):
    """Passa o mouse no cabeçalho, entra no submenu na horizontal e desce até o item."""
    marca = len(m.serial())
    yc = y_linha(p, ">" + cabecalho)
    m.mouse_mover(p["x"] + 30, yc)
    s = painel(m, marca, sub=cabecalho)
    # Na horizontal pela linha do cabeçalho (o submenu abre ao lado, com a
    # primeira linha na altura dele) e depois na vertical, dentro do submenu.
    m.mouse_mover(s["x"] + 30, yc)
    m.mouse_mover(s["x"] + 30, y_linha(s, item))
    return s


def menu_escritorio(m, x, y):
    marca = len(m.serial())
    m.clique(x, y, botao=DIREITO)
    return painel(m, marca)


def main():
    with Maquina("h60", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        t = janela(m.serial(), "Terminal")

        # ---- a barra no boot: Inicio, o botão do Terminal (com o foco) e o relógio
        b = esperar_barra(m, lambda b: b["n"] == 1 and b["botoes"][0]["estado"] == "f")
        verificar(b["L"] == 1024 and b["relogio"] == 1024 - 168 - 10,
                  f"barra em 1024: relógio encostado à direita (x = {b['relogio']})")
        bt = botao(b, t["id"])
        verificar(bt["x"] == 92 and bt["l"] == TAREFA_MAX, f"o Terminal tem um botão na barra ({bt['x']}+{bt['l']}) e o foco")
        png = m.captura("H60-1-barra")
        verificar(cor_do_pixel(png, 84, 6) == VERDE_INICIO, "o botão Inicio é verde, à esquerda")
        verificar(cor_do_pixel(png, bt["x"] + 10, 3 + 18 - 3) == MARCA_FOCO, "o botão da janela com o foco tem a marca amarela")
        verificar(cor_do_pixel(png, 300, 2) == BARRA and cor_do_pixel(png, 1023, 12) == BARRA, "o resto da barra continua escuro")

        # ---- menu Inicio pelo mouse: Accesorios > Calculadora
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        esperar(m, r"menu Inicio aberto \(mouse\)", desde=marca)
        verificar((p["x"], p["y"]) == (2, 24), "o menu Inicio cai da barra, embaixo do botão")
        verificar(p["linhas"][:2] == ["Terminal", "Archivos"] and "-" in p["linhas"],
                  f"no alto, os programas mais usados ({', '.join(p['linhas'][:4])})")
        for g in ("Accesorios", "Gráficos", "Internet", "Juegos", "Sistema"):
            verificar(">" + g in p["linhas"], f"o grupo {g} é um submenu")
        verificar(p["linhas"][-6:] == ["Ejecutar...", "Configuración de pantalla", "Acerca de SaguiOS-Extended", "-",
                                       "Apagar...", "Reiniciar..."],
                  "embaixo: Ejecutar, Configuración de pantalla, Acerca de, Apagar... (ACPI, D113) e Reiniciar")
        s = submenu_pelo_mouse(m, p, "Accesorios", "Calculadora")
        verificar(s["x"] >= p["x"] + p["l"] - 3 and "Calculadora" in s["linhas"], "Accesorios abre ao lado ao passar o mouse")
        time.sleep(0.4)
        png = m.captura("H60-2-inicio-accesorios")
        verificar(cor_do_pixel(png, 4, 12) != BARRA and cor_do_pixel(png, s["x"] + 6, y_linha(s, "Calculadora")) == VERDE_INICIO,
                  "a linha sob o mouse no submenu fica destacada")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /bin/calculadora", desde=marca)
        m.esperar_serial('"Calculadora"', 15)
        time.sleep(0.8)
        c = janela(m.serial(), "Calculadora")
        verificar(True, "Inicio > Accesorios > Calculadora abre a Calculadora")
        b = esperar_barra(m, lambda b: b["n"] == 2 and botao(b, c["id"])["estado"] == "f")
        verificar(botao(b, t["id"])["estado"] == "", "a Calculadora ganhou um botão com o foco; o do Terminal perdeu o destaque")

        # ---- pelo teclado: Ctrl+Esc, setas, direita/esquerda, Enter
        marca = len(m.serial())
        m.tecla("ctrl-esc")
        esperar(m, r"menu Inicio aberto \(teclado\)", desde=marca)
        p = painel(m, marca)
        esperar(m, r"menu: destaque \d+ \(Terminal\)", desde=marca)
        verificar(True, "Ctrl+Esc abre o menu Inicio com a primeira linha destacada")
        escolhiveis = [l for l in p["linhas"] if l != "-" and not l.startswith("~")]
        for _ in range(escolhiveis.index(">Gráficos")):
            m.tecla("down")
        esperar(m, r"menu: destaque \d+ \(Gráficos\)", desde=marca)
        m.tecla("right")
        esperar(m, r"menu: destaque \d+ \(Paint\)", desde=marca)
        painel(m, marca, sub="Gráficos")
        verificar(True, "setas até Gráficos e a seta à direita abre o submenu com Paint destacado")
        time.sleep(0.4)
        m.captura("H60-3-inicio-teclado")
        m.tecla("left")
        esperar(m, r"submenu \d+ fechado", desde=marca)
        verificar("menu: cancelado" not in m.serial()[marca:], "a seta à esquerda fecha só o submenu")
        m.tecla("right")
        m.tecla("ret")
        esperar(m, r"inicio -> /apps/bin/paint", desde=marca)
        m.esperar_serial('"Paint - sin nombre"', 20)
        time.sleep(1)
        pt = janela(m.serial(), "Paint - sin nombre")
        verificar(True, "Enter no Paint abre o Paint")

        marca = len(m.serial())
        m.tecla("meta_l")
        esperar(m, r"menu Inicio aberto \(teclado\)", desde=marca)
        m.tecla("meta_l")
        esperar(m, r"menu: cancelado", desde=marca)
        verificar(True, "a tecla Windows abre e fecha o menu Inicio")

        # ---- botões das tarefas
        b = esperar_barra(m, lambda b: b["n"] == 3 and botao(b, pt["id"])["estado"] == "f")
        verificar([x["id"] for x in b["botoes"]] == [t["id"], c["id"], pt["id"]],
                  "três botões, na ordem em que as janelas abriram")
        marca = len(m.serial())
        m.clique(*centro(botao(b, c["id"])))
        esperar(m, rf"clique na tarefa {c['id']} \(ativar\)", desde=marca)
        esperar_barra(m, lambda b: botao(b, c["id"])["estado"] == "f", desde=marca)
        verificar(True, "clique no botão da Calculadora (sem foco) a traz para cima")
        time.sleep(0.4)
        dentro = (c["x"] + 20, c["y"] + 5)   # a barra de título da Calculadora, que agora tem o foco
        antes = cor_do_pixel(m.captura("h60-calculadora", guardar=False), *dentro)
        m.clique(*centro(botao(b, c["id"])))
        esperar(m, rf"janela {c['id']} minimizada", desde=marca)
        b = esperar_barra(m, lambda b: botao(b, c["id"])["estado"] == "m", desde=marca)
        verificar(True, "outro clique (já com o foco) minimiza")
        time.sleep(0.4)
        png = m.captura("H60-4-tarea-minimizada")
        bc = botao(b, c["id"])
        verificar(cor_do_pixel(png, bc["x"] + bc["l"] - 3, 10) == MINIMIZADA, "o botão da minimizada fica apagado")
        verificar(antes == VERDE_INICIO and cor_do_pixel(png, *dentro) != antes, "e a Calculadora some da tela")
        m.clique(*centro(bc))
        esperar(m, rf"janela {c['id']} restaurada da barra", desde=marca)
        esperar_barra(m, lambda b: botao(b, c["id"])["estado"] == "f", desde=marca)
        verificar(True, "mais um clique restaura a Calculadora, com o foco")

        # Botão direito: o menu da barra de título, pendurado na barra.
        m.clique(*botao_minimizar(pt))
        esperar(m, rf"janela {pt['id']} minimizada", desde=marca)
        b = esperar_barra(m, lambda b: botao(b, pt["id"])["estado"] == "m", desde=marca)
        marca = len(m.serial())
        m.clique(*centro(botao(b, pt["id"])), botao=DIREITO)
        p = painel(m, marca)
        verificar(p["y"] == 24 and p["linhas"][:2] == ["Restaurar", "~Minimizar"],
                  "botão direito numa tarefa minimizada: o menu da janela, com Restaurar ativo e Minimizar desativado")
        time.sleep(0.3)
        m.captura("H60-5-menu-tarea")
        m.clique(p["x"] + 40, y_linha(p, "Restaurar"))
        esperar(m, rf"janela {pt['id']} restaurada da barra", desde=marca)
        verificar(True, "Restaurar pelo botão direito traz o Paint de volta")
        marca = len(m.serial())
        b = esperar_barra(m, lambda b: botao(b, pt["id"])["estado"] == "f")
        m.clique(*centro(botao(b, pt["id"])), botao=DIREITO)
        p = painel(m, marca)
        m.clique(p["x"] + 40, y_linha(p, "Cerrar"))
        esperar(m, rf"fechar janela {pt['id']} \(Paint - sin nombre\) pelo menu", desde=marca)
        m.esperar_serial("(paint) terminou", 10)
        esperar_barra(m, lambda b: b["n"] == 2, desde=marca)
        verificar(True, "Cerrar pelo botão direito fecha o Paint, e o botão dele sai da barra")

        # ---- Ejecutar..., Reiniciar... e o relógio
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        m.clique(p["x"] + 40, y_linha(p, "Ejecutar..."))
        esperar(m, r'janela (\d+) criada por \d+: "Ejecutar"', desde=marca)
        d = janela(m.serial(), "Ejecutar")
        time.sleep(0.8)
        m.captura("H60-6-ejecutar")
        b = esperar_barra(m, lambda b: True)
        verificar(all(x["id"] != d["id"] for x in b["botoes"]), "o diálogo Ejecutar (JAN_DIALOGO) não ganha botão na barra")
        m.digitar("editor /sobre.txt\n", 0.05)
        esperar(m, r"ejecutar: /bin/editor \(pid \d+\)", desde=marca)
        m.esperar_serial("editor: pronto", 20)
        verificar(True, "Ejecutar... com \"editor /sobre.txt\" abre o Editor")
        time.sleep(0.8)
        e = janela(m.serial(), "Editor - sin nombre") if '"Editor - sin nombre"' in m.serial()[marca:] else None
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        m.clique(p["x"] + 40, y_linha(p, "Reiniciar..."))
        esperar(m, r'janela \d+ criada por \d+: "Reiniciar"', desde=marca)
        time.sleep(0.8)
        m.captura("H60-7-reiniciar-pregunta")
        m.tecla("esc")
        m.esperar_serial("(ejecutar) terminou", 10)
        time.sleep(0.5)
        verificar(m.proc.poll() is None and "reiniciando" not in m.serial()[marca:],
                  "Reiniciar... pergunta antes; Esc (No) não reinicia")
        marca = len(m.serial())
        m.clique(1000, 12)
        esperar(m, r"notificacao: \w+, \d+ de \w+ de \d{4}, \d\d:\d\d", desde=marca)
        time.sleep(0.3)
        m.captura("H60-8-fecha")
        verificar(True, "clicar no relógio mostra a data completa")
        if e:   # fecha o Editor (sem alterações) para a tela ficar mais limpa
            m.clique(e["x"] + e["l"] + 2 * BORDA - BORDA - 12, e["y"] + 12)
            m.esperar_serial("editor: fechado", 10)
            time.sleep(0.5)

        # ---- menu da área de trabalho: Configuración de pantalla y Resolución
        p = menu_escritorio(m, 1000, 700)
        verificar(p["linhas"] == ["Terminal", "Archivos", "Editor", "Calculadora", "-", "Administrador de tareas",
                                  "Configuración de pantalla...", ">Resolución",
                                  ">Ordenar iconos", "-", "Acerca de SaguiOS-Extended"],
                  f"o menu da área de trabalho tem Configuración de pantalla... e o submenu Resolución (D268: submenu «Ordenar iconos») ({p['linhas']})")
        marca = len(m.serial())
        m.mouse_mover(p["x"] + 30, y_linha(p, ">Resolución"))
        s = painel(m, marca, sub="Resolución")
        verificar("*1024x768" in s["linhas"] and "800x600" in s["linhas"] and "640x480" in s["linhas"]
                  and "1280x1024" in s["linhas"], f"Resolución lista os modos a quente, com 1024x768 marcado ({len(s['linhas'])} modos)")
        verificar(s["x"] + s["l"] <= p["x"] + 3, "sem espaço à direita, o submenu abre à esquerda")
        time.sleep(0.4)
        m.captura("H60-9-menu-escritorio-resolucion")
        m.tecla("esc")
        m.tecla("esc")
        marca = len(m.serial())
        p = menu_escritorio(m, 1000, 700)
        m.clique(p["x"] + 40, y_linha(p, "Configuración de pantalla..."))
        esperar(m, r"pantalla: janela aberta, \d+ modos", desde=marca)
        time.sleep(1)
        pa = janela(m.serial(), "Pantalla")
        m.captura("H60-10-pantalla")
        verificar(True, "Configuración de pantalla... abre a janela Pantalla (pantalla --ventana)")
        m.clique(pa["x"] + BORDA + 440 - 102 + 45, pa["y"] + TITULO + 420 - 58 + 13)   # "Cerrar"
        m.esperar_serial("(pantalla) terminou", 10)
        time.sleep(0.5)

        marca = len(m.serial())
        p = menu_escritorio(m, 1000, 700)
        submenu_pelo_mouse(m, p, "Resolución", "800x600")
        clicar_aqui(m)
        esperar(m, r"resolucao pelo menu: 800x600 -> 1", desde=marca)
        esperar(m, r"\[VIDEO\] modo trocado: 800x600", desde=marca)
        b = esperar_barra(m, lambda b: b["L"] == 800, desde=marca)
        verificar(b["relogio"] == 800 - 168 - 10 and b["n"] == 2, f"em 800x600 a barra se recoloca (relógio em {b['relogio']})")
        time.sleep(1)
        png = m.captura("H60-11-800x600")
        verificar(tamanho_png(png) == (800, 600), "captura em 800x600")
        verificar(cor_do_pixel(png, 84, 6) == VERDE_INICIO and cor_do_pixel(png, 799, 2) == BARRA,
                  "Inicio à esquerda e a barra até a borda nova")

        marca = len(m.serial())
        p = menu_escritorio(m, 400, 590)
        s = submenu_pelo_mouse(m, p, "Resolución", "640x480")
        verificar("*800x600" in s["linhas"], "agora a marca está em 800x600")
        clicar_aqui(m)
        esperar(m, r"\[VIDEO\] modo trocado: 640x480", desde=marca)
        b = esperar_barra(m, lambda b: b["L"] == 640, desde=marca)
        verificar(b["relogio"] == 640 - 72 - 10, f"em 640x480 o relógio fica curto (dia e hora, em x = {b['relogio']})")
        time.sleep(1)
        png = m.captura("H60-12-640x480")
        verificar(tamanho_png(png) == (640, 480), "captura em 640x480")

        # ---- muitas janelas em 640x480: "»"
        focar(m, t["id"])     # o Terminal recebe o teclado
        marca = len(m.serial())
        m.comando("calculadora & calculadora & calculadora & calculadora & calculadora & calculadora &", tempo=60)
        b = esperar_barra(m, lambda b: b["n"] == 8, desde=marca, tempo=40)
        time.sleep(1.5)
        b = esperar_barra(m, lambda b: b["n"] == 8)
        verificar(b["mais"] is not None and b["ocultas"] == 8 - len(b["botoes"]) and b["ocultas"] > 0,
                  f"8 janelas em 640x480: {len(b['botoes'])} botões e o \"»\" com {b['ocultas']}")
        verificar(all(x["l"] >= TAREFA_MIN for x in b["botoes"]) and b["mais"] + 24 <= b["relogio"],
                  f"os botões não ficam menores que {TAREFA_MIN} pixels e nada passa por cima do relógio")
        verificar(any(x["estado"] == "f" for x in b["botoes"]), "a janela com o foco continua com botão visível")
        png = m.captura("H60-13-muchas-640")
        marca = len(m.serial())
        m.clique(b["mais"] + 12, 12)
        p = painel(m, marca)
        verificar(len(p["linhas"]) == b["ocultas"], f"\"»\" lista as {b['ocultas']} janelas que não couberam")
        time.sleep(0.3)
        m.captura("H60-14-menu-mas")
        visiveis = {x["id"] for x in b["botoes"]}
        m.clique(p["x"] + 40, y_linha(p, p["linhas"][0]))
        g = esperar(m, r"janela (\d+) ativada pela barra", desde=marca)
        esperar_barra(m, lambda b: botao(b, int(g))["estado"] == "f", desde=marca)
        verificar(int(g) not in visiveis, f"escolher uma no menu do \"»\" traz a janela {g} para cima e ela ganha um botão")

        # ---- a mesma coisa em 1280x1024 e 1024x768: os botões estreitam e cabem
        focar(m, t["id"])
        for L, A in ((1280, 1024), (1024, 768)):
            marca = len(m.serial())
            m.comando(f"pantalla {L}x{A}", tempo=60)
            b = esperar_barra(m, lambda b: b["L"] == L and b["n"] == 8, desde=marca)
            verificar(b["mais"] is None and len(b["botoes"]) == 8 and all(TAREFA_MIN <= x["l"] < TAREFA_MAX for x in b["botoes"]),
                      f"em {L}x{A} as 8 janelas cabem, com botões mais estreitos ({b['botoes'][0]['l']} pixels)")
            time.sleep(1)
            png = m.captura(f"H60-{15 if L == 1280 else 16}-muchas-{L}")
            verificar(tamanho_png(png) == (L, A) and cor_do_pixel(png, L - 1, 2) == BARRA, f"captura em {L}x{A}")
    return 0


def botao_minimizar(j):
    """O botão de minimizar de uma janela redimensionável (o terceiro da direita)."""
    return j["x"] + j["l"] + 2 * BORDA - BORDA - 60 + 8, j["y"] + 12


if __name__ == "__main__":
    sys.exit(main())
