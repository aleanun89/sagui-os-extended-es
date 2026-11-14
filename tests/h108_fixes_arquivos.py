#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H108 - a grelha de icones e as accoes de grupo do Archivos (D139).

Critério:
  - na vista de icones a roda rola a grelha por LINHAS INTEIRAS: os items
    continuam nas mesmas celulas (a grelha nao se desloca de lado) e o log
    diz em que item a rolagem comecou;
  - com varios elementos marcados, o botao direito da um menu de grupo
    ("Abrir (N)", "Copiar", "Cortar", "Borrar (N)", "Propriedades (N)",
    "Comprimir em .zip" e "Comprimir em .tar.gz");
  - "Comprimir en .zip" junta todos os elementos marcados num unico arquivo
    que se pode listar depois.

A grelha e lida da propria tela: o nome de cada item esta desenhado na
fonte 8x16 do sistema, por isso o teste le os caracteres pixel a pixel.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, _fonte, SAIDA  # noqa: E402

# A janela do Archivos: a lista comeca depois do painel lateral (LATERAL=140).
LISTA_X, LISTA_Y = 146, 70
# As celulas da grelha de icones (largura e altura, com o passo do codigo).
CEL_L, CEL_A = 92, 74
# A cor do rectangulo de selecao (COR_SELECAO da libsagui).
COR_SELECAO = (0x2A, 0x6F, 0xD8)
# O menu: altura da linha e do separador (kernel/gui/menu.c).
LINHA_A, SEP_A = 22, 9
DIREITO = 2              # mouse_button do monitor do QEMU: 1 esquerdo, 2 direito
N_ITENS = 60             # ficheiros de texto na pasta de teste
TOTAL = N_ITENS + 1     # mais o PDF, para a associacao por extensao
# Os nomes da pasta, como os ordena o Archivos (o PDF vai el primero).
NOMES = sorted(["doc.pdf"] + [f"f{i:02d}.txt" for i in range(1, N_ITENS + 1)])


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
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def item_lista(a, i):
    """O centro do item i da lista (vista de lista)."""
    return a["x"] + LISTA_X + 120, a["y"] + LISTA_Y + 2 + i * 20 + 8


def botao(a, nome):
    """O centro de um botao da barra de ferramentas."""
    bt = {"voltar": 8, "nova": 106, "borrar": 224, "adelante": 304, "arriba": 390,
          "ocultos": 476, "vista": 588, "buscar": 636}
    return a["x"] + bt[nome] + 35, a["y"] + 7 + 13


def caractere(px, larg, x, y):
    """Le um caracter da fonte 8x16 da tela (como reconhece_texto, mas numa posicao)."""
    fonte = _fonte()
    cores = []
    for j in range(16):
        ini = ((y + j) * larg + x) * 3
        cores.append([px[ini + 3 * i: ini + 3 * i + 3] for i in range(8)])
    fundo = cores[0][7]
    frente = next((c for fila in cores for c in fila if c != fundo), None)
    if frente is None:
        return " "
    achou = " "
    for f, b in ((frente, fundo), (fundo, frente)):
        bits = bytes(sum(0x80 >> i for i in range(8) if fila[i] == f) for fila in cores)
        if bits in fonte:
            achou = fonte[bits]
            break
    return achou


def nomes_da_linha(px, larg, a, linha, colunas):
    """Os nomes dos itens de uma linha da grelha (lidos da tela)."""
    nomes = []
    for c in range(colunas):
        x = a["x"] + LISTA_X + 6 + c * CEL_L + 2
        y = a["y"] + LISTA_Y + 4 + linha * CEL_A + 46
        nome = ""
        for k in range(10):
            nome += caractere(px, larg, x + 8 * k, y)
        nomes.append(nome.rstrip())
    return nomes


def celulas_marcadas(px, larg, a):
    """O canto esquerdo de cada celula marcada (o rectangulo de selecao).

    A celula marcada e um rectangulo azul de 88 px de largura, com 4 px de
    branco entre uma e outra: por isso os comecos das celulas sao os comecos
    das sequencias de azul. A linha de pixels usada e a que fica entre o
    icone e o nome, onde so ha o azul do rectangulo."""
    x0 = a["x"] + LISTA_X
    larg_lista = a["l"] - 140 - 14
    y = a["y"] + LISTA_Y + 4 + 38
    celulas = []
    dentro = False
    for x in range(x0, min(x0 + larg_lista, larg)):
        i = (y * larg + x) * 3
        tem = (px[i], px[i + 1], px[i + 2]) == COR_SELECAO
        if tem and not dentro:
            celulas.append(x)
            dentro = True
        elif not tem:
            dentro = False
    return celulas


def linha_do_menu(menu, i, separadores_antes=0):
    """O centro da linha i do menu (com os separadores ja contados)."""
    mx, my, ml = menu[0], menu[1], menu[2]
    return mx + ml // 2, my + 2 + (i - separadores_antes) * LINHA_A + separadores_antes * SEP_A + LINHA_A // 2


def principal():
    with Maquina("h108", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- preparacao: uma pasta com muitos ficheiros
        m.comando("mkdir /tmp/grupo")
        time.sleep(0.3)
        m.comando("for i in $(seq -w 1 %d); do touch /tmp/grupo/f$i.txt; done" % N_ITENS)
        time.sleep(1.0)
        m.comando("touch /tmp/grupo/doc.pdf")     # para a associacao por extensao
        time.sleep(0.5)
        out = m.comando("ls /tmp/grupo | wc -l")
        achado = re.search(rf"\b{N_ITENS + 1}\b", out)
        verificar(achado is not None, f"a pasta de teste tem {N_ITENS} ficheiros de texto e o PDF")

        m.comando("arquivos /tmp/grupo")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.8)
        a = janela(m.serial(), "Archivos")
        colunas = (a["l"] - 140 - 14 - 12) // CEL_L
        verificar(colunas == 6, f"a grelha tem {colunas} colunas")

        # ---- a associacao por extensao: um .pdf abre no Visor de PDF
        m.clique(*item_lista(a, 0))       # "doc.pdf" e o primeiro item da lista
        time.sleep(0.4)
        marca_ab = len(m.serial())
        m.clique(a["x"] + 28, a["y"] + 190)     # o botao "Abrir con..." do lateral
        g = esperar(m, r"\[GUI\] menu linhas: (.*)", 15, desde=marca_ab)
        verificar("*visorpdf" in g, f"o .pdf abre por omissao no Visor de PDF ({g[:70]})")
        m.tecla("esc")
        time.sleep(0.4)

        # ---- a vista de icones
        m.clique(*botao(a, "vista"))
        m.esperar_serial("arquivos: vista detalhes", 10)
        m.clique(*botao(a, "vista"))
        m.esperar_serial("arquivos: vista icones", 10)
        time.sleep(0.5)

        # ---- marcar tudo (Ctrl+A) para ver as celulas seleccionadas
        m.tecla("ctrl-a")
        m.esperar_serial(f"arquivos: selecionados {TOTAL} itens", 10)
        time.sleep(0.5)
        m.captura("H108-1-grelha")

        ppm = os.path.join(SAIDA, "h108-grelha.ppm")
        m.monitor(f"screendump {ppm}")
        for _ in range(50):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.1)
        larg, _, px = ler_ppm(ppm)

        nomes = nomes_da_linha(px, larg, a, 0, colunas)
        esperado = NOMES[:colunas]
        verificar(nomes == esperado, f"a primeira linha da grelha tem os primeiros items ({nomes[0]}...{nomes[-1]})")

        cel = celulas_marcadas(px, larg, a)
        verificar(len(cel) == colunas and all((x - (a["x"] + LISTA_X + 6)) % CEL_L == 0 for x in cel),
                  f"as {colunas} celulas marcadas caem nas colunas da grelha")

        # ---- a roda: tem de rolar por linhas inteiras (D139)
        marca = len(m.serial())
        m.mouse_para(a["x"] + LISTA_X + 120, a["y"] + LISTA_Y + 100)
        time.sleep(0.3)
        m.monitor("mouse_move 0 0 -1")      # uma entalhe para baixo
        time.sleep(0.8)
        g = esperar(m, r"arquivos: rolou a grelha para o item (\d+) \(linha (\d+) de (\d+)\)", 15, desde=marca)
        item_rodado, linha = int(g[0]), int(g[1])
        verificar(item_rodado % colunas == 0,
                  f"a rolagem comecou no item {item_rodado}, que e o inicio de uma linha (multiplo de {colunas})")

        m.monitor(f"screendump {ppm}")
        time.sleep(0.4)
        larg, _, px = ler_ppm(ppm)
        nomes = nomes_da_linha(px, larg, a, 0, colunas)
        esperado = NOMES[item_rodado:item_rodado + colunas]
        verificar(nomes == esperado,
                  f"depois da roda a grelha mostra a linha seguinte, sem se deslocar de lado ({nomes[0]}...{nomes[-1]})")
        cel = celulas_marcadas(px, larg, a)
        verificar(len(cel) == colunas and all((x - (a["x"] + LISTA_X + 6)) % CEL_L == 0 for x in cel),
                  "as celulas continuam nas mesmas colunas depois da roda")
        m.captura("H108-2-grelha-rolada")

        # ---- o menu de grupo (botao direito num item marcado)
        marca = len(m.serial())
        m.clique(a["x"] + LISTA_X + 6 + 92 + 20, a["y"] + LISTA_Y + 4 + 20, botao=DIREITO)
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 15, desde=marca)
        menu = tuple(int(v) for v in g)
        texto = esperar(m, r"\[GUI\] menu linhas: (.*)", 10, desde=marca)
        wait = esperar(m, r"arquivos: menu do grupo \((\d+) itens\)", 10, desde=marca)
        verificar(int(wait) == TOTAL, f"com {TOTAL} itens marcados o botao direito da o menu do grupo (o log diz {wait})")
        linhas_menu = [l.strip() for l in texto.replace("\r", "").split("|") if l.strip()]
        esperado = ["Abrir (%d)" % TOTAL, "Copiar", "Cortar", "-", "Borrar (%d)" % TOTAL,
                    "Propiedades (%d)" % TOTAL, "-", "Comprimir en .zip", "Comprimir en .tar.gz"]
        verificar(linhas_menu == esperado, f"o menu de grupo tem as accoes de sempre ({linhas_menu})")
        time.sleep(0.4)
        m.captura("H108-3-menu-grupo")

        # ---- "Comprimir en .zip" junta todos os itens marcados
        marca = len(m.serial())
        m.clique(*linha_do_menu(menu, 7, separadores_antes=2))
        esperar(m, r"arquivos: comprimir (\d+) itens -> (\S+) \(codigo (\d+)\)", 60, desde=marca)
        g = esperar(m, r"arquivos: comprimir (\d+) itens -> (\S+) \(codigo (\d+)\)", 60, desde=marca)
        verificar(int(g[0]) == TOTAL and int(g[2]) == 0,
                  f"a compressao do grupo juntou {g[0]} itens em {g[1]} (codigo {g[2]})")
        time.sleep(1.0)

        # ---- o Terminal confere o arquivo (a janela do Archivos tem de fechar
        # para que a serie va para o Terminal e nao para o explorador)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 15)
        time.sleep(0.8)
        out = m.comando("unzip -l /tmp/grupo/grupo.zip")
        linhas = [l for l in out.splitlines() if ".txt" in l or ".pdf" in l]
        verificar(len(linhas) == TOTAL,
                  f"o .zip do grupo tem os {TOTAL} ficheiros (a listagem mostra {len(linhas)})")
        m.captura("H108-4-arquivo-criado")

    return 0


if __name__ == "__main__":
    sys.exit(principal())
