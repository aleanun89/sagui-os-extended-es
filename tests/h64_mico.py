#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H64 - Mico, o editor de código (mejora 037, D074-D075).

Critério:
 (a) `mico -t` imprime a classe de cada letra: C (palavras-chave, tipos,
     funções, cadeias, números, pré-processador e um comentário de bloco que
     atravessa linhas), Python (docstring, def, números), sh (variáveis,
     palavras-chave), Markdown (título, ênfase, código) e CHIP-8 (rótulos,
     mnemônicos, registros, comentários);
 (b) `mico /ejemplos/mico` abre a pasta no explorador; três arquivos abrem em
     pestañas pelo explorador; o realce aparece na tela com as cores do tema
     (pixels das letras de "int", "cuadrado", "#include", do comentário e de
     uma cadeia);
 (c) buscar (Ctrl+F, F3, contagem "k de N"), reemplazar todos (Ctrl+H) e ir
     a la línea (Ctrl+G); Ctrl+S guarda de forma atômica (x.nuevo e renomear)
     e o arquivo no disco muda;
 (d) F5 num .c correto compila e executa no terminal do painel; com um erro,
     a lista de Problemas mostra arquivo:linha e o clique leva o cursor à
     linha; F5 num .py roda o MicroPython, e uma exceção vira um problema;
 (e) portapapeles do sistema (SIS_COPIAR_TEXTO/SIS_COLAR_TEXTO): Mico ->
     Editor, Editor -> Terminal (o texto colado roda no shell), Editor ->
     Mico e Pegar no terminal do Mico;
 (f) recuperação: texto não guardado é copiado a cada 5 s; depois de matar o
     Mico com kill, abri-lo de novo oferece recuperar e o texto volta;
 (g) arquivo de 20.000 linhas: abrir, Ctrl+End, RePág, rueda e Ctrl+G, com os
     tempos medidos no log;
 (h) menú Inicio > Accesorios > Mico.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402
from h60_barra_tareas import esperar_barra, botao, painel, submenu_pelo_mouse, clicar_aqui, y_linha  # noqa: E402

BORDA, TITULO = 2, 24
EXPL_CAB, EXPL_LINHA = 24, 18
# Tema claro (userland/programas/mico/desenho.c)
COR = {"tipo": 0x1F5E8C, "funcao": 0x245A7A, "preproc": 0x7A3E9D, "comentario": 0x7E8A80, "cadeia": 0x9A5B13,
       "chave": 0x0B7A3E}


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por (\d+): "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), pid=int(g[1]), l=int(g[2]), a=int(g[3]), x=int(g[4]) + BORDA, y=int(g[5]) + TITULO,
                fx=int(g[4]), fy=int(g[5]))


def layout(m, desde=0):
    g = esperar(m, r"mico: layout (\d+)x(\d+), lateral (\d+), editor (\d+),(\d+) (\d+)x(\d+), panel (\d+),(\d+) (\d+)x(\d+)", desde=desde)
    v = [int(x) for x in g]
    return dict(L=v[0], A=v[1], lat=v[2], ex=v[3], ey=v[4], el=v[5], ea=v[6], px=v[7], py=v[8], pl=v[9], pa=v[10])


def focar(m, ident):
    """Traz a janela para a frente pelo botão da barra de tarefas."""
    b = esperar_barra(m, lambda b: botao(b, ident)["estado"] is not None)
    if botao(b, ident)["estado"] != "f":
        bt = botao(b, ident)
        m.clique(bt["x"] + bt["l"] // 2, 12)
        esperar_barra(m, lambda b: botao(b, ident)["estado"] == "f")
    time.sleep(0.4)


def tem_cor(pixels, larg, x0, y0, l, a, cor):
    alvo = bytes(((cor >> 16) & 255, (cor >> 8) & 255, cor & 255))
    for y in range(y0, y0 + a):
        for x in range(x0, x0 + l):
            i = (y * larg + x) * 3
            if pixels[i:i + 3] == alvo:
                return True
    return False


def tela(m, nome):
    ppm = os.path.join(SAIDA, f"h64-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    return ler_ppm(ppm)


def classes(saida, arquivo):
    """As linhas "   N|letras" que `mico -t arquivo` imprimiu (índice = número da linha)."""
    bloco = saida.split(f"mico -t: {arquivo}", 1)[1]
    r = {}
    for l in bloco.replace("\r", "").split("\n"):
        l = re.sub(r"\[(SAGUI|GUI|PORTAPAPELES)\].*", "", l)   # o log do kernel pode cair no meio da linha
        g = re.match(r"\s*(\d+)\|(.*)$", l)
        if g:
            n = int(g[1])
            if n in r:
                break
            r[n] = g[2]
        elif l.startswith("mico -t:"):
            break
    return r


def parte_realce(m):
    m.comando("ayuda mico")
    verificar("mico [carpeta | archivo...] | mico -t archivo" in m.serial(), "ajuda mico explica o uso")
    saida = {}
    for a in ("hola.c", "saludo.py", "contar.sh", "notas.md", "parpadeo.asm"):
        antes = len(m.serial())
        m.comando(f"mico -t /ejemplos/mico/{a}")
        esperar(m, r"\(mico\) terminou com codigo 0", 20, antes)
        time.sleep(0.3)
        saida[a] = classes(m.serial()[antes:], f"/ejemplos/mico/{a}")
    c = saida["hola.c"]
    verificar(set(c[1].replace(" ", "")) == {"c"} and set(c[2].replace(" ", "")) == {"c"},
              "C: comentário de bloco que continua na linha seguinte")
    verificar(c[3] == "pppppppp sssssssss", f"C: #include <stdio.h> = pré-processador + cadeia ({c[3]!r})")
    verificar(c[5].startswith("ppppppp") and c[5].endswith("n"), f"C: #define com número ({c[5]!r})")
    verificar(c[7] == "ttt ffffffff.ttt ..", f"C: tipo, função e parâmetro ({c[7]!r})")
    verificar(c[9] == "    kkkkkk . . ..", f"C: return é palavra-chave ({c[9]!r})")
    verificar(c[14][19:25] == "ssssss" and c[14][4:8] == "tttt", f"C: cadeia \"Mico\" ({c[14]!r})")
    p = saida["saludo.py"]
    verificar(set(p[1].replace(" ", "")) == {"c"}, "Python: comentário com #")
    verificar(p[2].startswith("kkk fffffff") and "n" in p[2], f"Python: def + nome da função + número ({p[2]!r})")
    verificar(set(p[3].replace(" ", "")) == {"s"}, f"Python: docstring de três aspas ({p[3]!r})")
    verificar(p[4].strip().startswith("kkk") and "kk ttttt" in p[4], f"Python: for ... in range ({p[4]!r})")
    s = saida["contar.sh"]
    verificar(s[3].startswith("vvv.vvvvvvv"), f"sh: atribuição e ${{1:-3}} ({s[3]!r})")
    verificar(s[5].startswith("kkkkk") and "vv" in s[5], f"sh: while e $i ({s[5]!r})")
    verificar(s[6].strip().startswith("tttt ssss"), f"sh: echo e cadeia ({s[6]!r})")
    d = saida["notas.md"]
    verificar(set(d[1].replace(" ", "")) == {"h"}, "Markdown: título")
    verificar("eeeeeeeeeee" in d[3], f"Markdown: **ênfase** ({d[3]!r})")
    verificar(set(d[9].replace(" ", "")) == {"s"} and set(d[10].replace(" ", "")) == {"s"}, "Markdown: bloco ``` de código")
    a = saida["parpadeo.asm"]
    verificar(set(a[1].replace(" ", "")) == {"c"}, "CHIP-8: comentário com ;")
    verificar(a[5] == "lllllll", f"CHIP-8: rótulo ({a[5]!r})")
    verificar(a[6].strip() == "kkk", "CHIP-8: mnemônico")
    verificar(a[7].strip().startswith("kk rr. nn") and a[7].rstrip().endswith("c"), f"CHIP-8: ld v0, 10 ; x ({a[7]!r})")


def main():
    with Maquina("h64", "ide", memoria="512M") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        terminal = janela(m.serial(), "Terminal")

        # ---------------------------------------------------------------- (a)
        parte_realce(m)

        # ---------------------------------------------------------------- (b)
        marca = len(m.serial())
        m.comando("mico /ejemplos/mico")
        esperar(m, r"mico: pronto \(1 pestanas, carpeta /ejemplos/mico\)", 30, marca)
        w = janela(m.serial(), "Mico")
        lay = layout(m, marca)
        X, Y = w["x"], w["y"]
        verificar(re.search(r"mico: carpeta /ejemplos/mico \(7 entradas\)", m.serial()[marca:]) is not None,
                  "abre a pasta: 7 entradas no explorador (a pasta datos, .mico e 5 arquivos)")
        verificar("mico: .mico leido (3 opciones, 1 ordenes de ejecutar)" in m.serial()[marca:], "lê a configuração .mico da pasta")
        time.sleep(1)

        def linha_expl(i):
            return X + 100, Y + 24 + EXPL_CAB + i * EXPL_LINHA + 9

        for i, nome in ((6, "saludo.py"), (4, "notas.md"), (3, "hola.c")):
            k = len(m.serial())
            m.clique(*linha_expl(i))
            esperar(m, rf"mico: abierto /ejemplos/mico/{re.escape(nome)} \(", 15, k)
        esperar(m, r"mico: titulo Mico - hola.c - mico", 10, marca)
        verificar(True, "três arquivos abertos em pestañas pelo explorador (saludo.py, notas.md, hola.c)")
        time.sleep(1)
        m.mouse_para(X + 700, Y + lay["py"] + 100)   # o cursor do mouse longe do texto
        m.captura("H64-1-pestanas")

        # realce na tela: hola.c, do começo
        larg, alt, px = tela(m, "realce")
        tx = X + lay["ex"] + 40 + 4          # margem de 3 dígitos (40) + 4

        def celula(lin, col, n):
            return tx + col * 8, Y + lay["ey"] + (lin - 1) * 16, n * 8, 16

        verificar(tem_cor(px, larg, *celula(7, 0, 3), COR["tipo"]), "na tela: \"int\" com a cor dos tipos")
        verificar(tem_cor(px, larg, *celula(7, 4, 8), COR["funcao"]), "na tela: \"cuadrado\" com a cor das funções")
        verificar(tem_cor(px, larg, *celula(3, 0, 8), COR["preproc"]), "na tela: \"#include\" com a cor do pré-processador")
        verificar(tem_cor(px, larg, *celula(2, 3, 10), COR["comentario"]), "na tela: a 2ª linha do comentário de bloco")
        verificar(tem_cor(px, larg, *celula(14, 19, 6), COR["cadeia"]), "na tela: a cadeia \"Mico\"")
        verificar(tem_cor(px, larg, *celula(9, 4, 6), COR["chave"]), "na tela: \"return\" com a cor das palavras-chave")
        verificar(not tem_cor(px, larg, *celula(7, 13, 3), COR["funcao"]), "na tela: o segundo \"int\" não tem a cor de função")

        # ---------------------------------------------------------------- (c)
        k = len(m.serial())
        m.tecla("ctrl-f")
        m.digitar("cuadrado", 0.05)
        g = esperar(m, r"mico: buscar 'cuadrado' -> linea (\d+) \((\d+) de (\d+)\)", 10, k)
        verificar(g == ("7", "1", "3"), f"Ctrl+F busca incremental: linha 7, 1 de 3 ({g})")
        k = len(m.serial())
        m.tecla("f3")
        g = esperar(m, r"mico: buscar 'cuadrado' -> linea (\d+) \((\d+) de (\d+)\)", 10, k)
        verificar(g == ("16", "2", "3"), f"F3: a próxima ocorrência ({g})")
        time.sleep(0.5)
        m.captura("H64-2-buscar")
        m.tecla("esc")
        k = len(m.serial())
        m.tecla("ctrl-h")
        esperar(m, r"barra de busqueda \(modo 2\)", 10, k)
        for _ in range(10):
            m.tecla("backspace", 0.03)
        m.digitar("VECES", 0.05)
        m.tecla("tab")
        m.digitar("REPETICIONES", 0.05)
        m.tecla("shift-ret")
        g = esperar(m, r"mico: reemplazadas (\d+) apariciones de 'VECES'", 10, k)
        verificar(g == "2", "Ctrl+H, Reemplazar todos: 2 trocas (#define e o for)")
        m.tecla("esc")
        k = len(m.serial())
        m.tecla("ctrl-g")
        m.digitar("12", 0.05)
        m.tecla("ret")
        esperar(m, r"mico: ir a la linea 12", 10, k)
        verificar(True, "Ctrl+G 12: ir a la línea")
        k = len(m.serial())
        m.tecla("ctrl-s")
        esperar(m, r"mico: guardado /ejemplos/mico/hola.c \((\d+) bytes", 10, k)
        verificar("mico: escrito /ejemplos/mico/hola.c.nuevo y renombrado a /ejemplos/mico/hola.c" in m.serial()[k:],
                  "Ctrl+S: guardado atômico (hola.c.nuevo renomeado por cima de hola.c)")

        # ---------------------------------------------------------------- (d)
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, 0 problemas", 60, k)
        s = m.serial()[k:]
        verificar("mico: F5 cc /ejemplos/mico/hola.c -o /tmp/hola && /tmp/hola" in s, "F5 num .c: cc e executar")
        verificar("mico: tv| Hola desde Mico" in s and "mico: tv| 3 al cuadrado = 9" in s,
                  "o programa roda no terminal do painel (com REPETICIONES = 3)")
        time.sleep(0.5)
        m.captura("H64-3-f5-c")
        # um erro na linha 17
        k = len(m.serial())
        m.tecla("ctrl-g")
        m.digitar("17", 0.05)
        m.tecla("ret")
        m.tecla("end")
        m.digitar(" x = ;", 0.05)
        m.tecla("ctrl-home")
        m.tecla("f5")
        g = esperar(m, r"mico: F5 terminado en \d+ ms, (\d+) problemas", 60, k)
        verificar(int(g) >= 1, f"F5 com erro: {g} problema(s)")
        prob = re.findall(r"mico: problema (\S+):(\d+): (.*)", m.serial()[k:])
        verificar(prob and prob[0][0] == "/ejemplos/mico/hola.c" and prob[0][1] == "17",
                  f"o problema aponta hola.c:17 ({prob[0] if prob else None})")
        time.sleep(0.6)
        m.captura("H64-4-problemas")
        k = len(m.serial())
        m.clique(X + lay["px"] + 200, Y + lay["py"] + 22 + 2 + 9)
        esperar(m, r"mico: salto a /ejemplos/mico/hola.c:17", 10, k)
        esperar(m, r"mico: cursor en hola.c:17", 10, k)
        verificar(True, "clique no problema: o cursor vai para a linha 17")
        time.sleep(0.4)
        m.captura("H64-5-salto")
        m.tecla("end")              # tira o " x = ;" que causou o erro
        for _ in range(6):
            m.tecla("shift-left", 0.04)
        m.tecla("delete")
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, 0 problemas", 60, k)
        verificar(True, "sem o erro, o F5 volta a compilar")

        # Python: saludo.py (primeira pestaña)
        k = len(m.serial())
        m.tecla("ctrl-tab")
        esperar(m, r"mico: titulo Mico - saludo.py - mico", 10, k)
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, 0 problemas", 60, k)
        s = m.serial()[k:]
        verificar("mico: F5 python /ejemplos/mico/saludo.py" in s and "mico: tv| Hola, sagui 1" in s and "mico: tv| 40" in s,
                  "F5 num .py: o MicroPython roda o arquivo")
        k = len(m.serial())
        m.tecla("ctrl-end")
        m.digitar("print(nada)", 0.05)
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, 1 problemas", 60, k)
        verificar(re.search(r"mico: problema /ejemplos/mico/saludo.py:9: NameError", m.serial()[k:]) is not None,
                  "uma exceção do Python vira problema (saludo.py:9, NameError)")
        m.tecla("ctrl-end")
        m.tecla("shift-home")
        m.tecla("delete")
        m.tecla("ctrl-s")
        time.sleep(0.5)

        # ---------------------------------------------------------------- (e)
        # Mico -> Editor: a primeira linha de hola.c
        k = len(m.serial())
        m.tecla("ctrl-tab")
        m.tecla("ctrl-tab")
        esperar(m, r"mico: titulo Mico - hola.c - mico", 10, k)
        m.tecla("ctrl-home")
        m.tecla("shift-end")
        m.tecla("ctrl-c")
        n = int(esperar(m, r"mico: copiado (\d+) bytes", 10, k))
        verificar(n == len("/* hola.c - un programa pequeño para probar Mico."), f"Ctrl+C no Mico copia a seleção ({n} bytes)")
        verificar(re.search(rf"\[PORTAPAPELES\] copia \d+: {n} bytes", m.serial()[k:]) is not None, "o kernel guardou o texto")
        # o Editor, aberto pelo terminal do painel (F6 mostra o Terminal e dá o foco a ele)
        m.tecla("f6")
        time.sleep(0.4)
        k = len(m.serial())
        m.digitar("editor\n", 0.05)
        esperar(m, r"editor: pronto", 20, k)
        ed = janela(m.serial(), "Editor - sin nombre")
        time.sleep(0.8)
        m.tecla("ctrl-v")
        g = esperar(m, r"gui: area colou (\d+) bytes", 10, k)
        verificar(int(g) == n, f"Ctrl+V no Editor cola o que o Mico copiou ({g} bytes)")
        m.tecla("ctrl-a")
        m.digitar("echo portapapeles-ok", 0.05)
        m.tecla("ctrl-a")
        k = len(m.serial())
        m.tecla("ctrl-c")
        verificar(esperar(m, r"gui: area copiou (\d+) bytes", 10, k) == "20", "Editor: seleção e Ctrl+C (20 bytes)")
        time.sleep(0.4)
        m.captura("H64-6-editor-pegar")
        # Editor -> Terminal (Ctrl+Mayús+V) e o texto roda no shell
        focar(m, terminal["id"])
        k = len(m.serial())
        m.tecla("ctrl-shift-v")
        esperar(m, r"terminal: pegado 20 bytes", 10, k)
        m.tecla("ret")
        esperar(m, r"\nportapapeles-ok", 15, k)
        verificar(True, "Ctrl+Mayús+V no Terminal cola e o shell roda \"echo portapapeles-ok\"")
        # Editor -> Mico (pestaña nova) e Pegar no terminal do Mico
        focar(m, w["id"])
        k = len(m.serial())
        m.tecla("ctrl-n")
        m.tecla("ctrl-v")
        verificar(esperar(m, r"mico: pegado (\d+) bytes", 10, k) == "20", "Ctrl+V no Mico cola o texto do Editor")
        m.clique(X + lay["px"] + 300, Y + lay["py"] + 22 + 60)
        k = len(m.serial())
        m.tecla("ctrl-shift-v")
        verificar(esperar(m, r"mico: terminal pegado (\d+) bytes", 10, k) == "20", "Ctrl+Mayús+V no terminal do Mico")
        m.tecla("ret")
        time.sleep(0.5)
        m.captura("H64-7-portapapeles")
        # fecha a pestaña nova sem guardar (Esc = No)
        m.clique(X + lay["ex"] + 100, Y + lay["ey"] + 100)
        m.tecla("ctrl-w")
        esperar(m, r'criada por \d+: "Mico"', 10, k)
        time.sleep(0.6)
        m.tecla("esc")
        esperar(m, r"mico: pestana cerrada sin nombre", 10, k)

        # ---------------------------------------------------------------- (f)
        k = len(m.serial())
        m.tecla("ctrl-shift-tab")
        esperar(m, r"mico: titulo Mico - notas.md - mico", 10, k)
        m.tecla("ctrl-end")
        m.digitar("\nlinea que no se guardo", 0.05)
        esperar(m, r"mico: titulo Mico - notas.md \* - mico", 10, k)
        g = esperar(m, r"mico: recuperacion /ejemplos/mico/notas.md \((\d+) bytes, ok\)", 15, k)
        tam_recup = int(g)
        verificar(True, f"a cada 5 s, cópia de recuperação do que não foi guardado ({tam_recup} bytes)")
        focar(m, terminal["id"])
        k = len(m.serial())
        m.comando(f"kill {w['pid']}")
        esperar(m, rf"processo {w['pid']} \(mico\) terminou", 15, k)
        verificar(True, f"kill {w['pid']}: o Mico morre sem guardar")
        time.sleep(1)
        k = len(m.serial())
        m.comando("mico /ejemplos/mico")
        esperar(m, r'criada por \d+: "Mico - recuperaci', 30, k)
        verificar("mico: 1 copias de recuperacion" in m.serial()[k:], "ao abrir de novo, o Mico encontra a cópia de recuperação")
        time.sleep(0.8)
        m.captura("H64-8-recuperar-pregunta")
        m.tecla("ret")
        g = esperar(m, r"mico: recuperado /ejemplos/mico/notas.md \((\d+) bytes\)", 15, k)
        verificar(int(g) == tam_recup, f"Sí: o texto volta ({g} bytes)")
        esperar(m, r"mico: titulo Mico - notas.md \* - mico", 10, k)
        verificar(True, "a pestaña recuperada fica com cambios sin guardar (*)")
        esperar(m, r"mico: pronto", 20, k)
        w = janela(m.serial(), "Mico")
        X, Y = w["x"], w["y"]
        lay = layout(m, k)
        time.sleep(0.8)
        m.mouse_para(X + 700, Y + lay["py"] + 100)
        m.captura("H64-9-recuperado")
        m.tecla("ctrl-z")   # volta ao texto do disco: a cópia some no próximo ciclo
        time.sleep(0.5)

        # ---------------------------------------------------------------- (g)
        focar(m, terminal["id"])
        k = len(m.serial())
        m.comando("python -c \"for i in range(20000): print('int v%d = %d; /* linea %d */' % (i, i, i))\" > /tmp/grande.c",
                  tempo=180)
        out = m.comando("wc /tmp/grande.c")
        verificar("20000" in out, "o Python gerou /tmp/grande.c com 20.000 linhas")
        focar(m, w["id"])
        k = len(m.serial())
        m.tecla("ctrl-o")
        esperar(m, r'criada por \d+: "Abrir archivo"', 10, k)
        time.sleep(0.5)
        for _ in range(20):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/grande.c\n", 0.05)
        g = esperar(m, r"mico: abierto /tmp/grande.c \((\d+) lineas, (\d+) bytes, C, (\d+) ms\)", 30, k)
        tempos = {"abrir": int(g[2])}
        verificar(int(g[0]) == 20001, f"abre o arquivo grande ({g[0]} linhas, {g[1]} bytes) em {g[2]} ms")
        esperar(m, r"mico: dibujo completo, linea 1 de 20001", 10, k)
        t0 = time.time()
        k = len(m.serial())
        m.tecla("ctrl-end")
        g = esperar(m, r"mico: dibujo completo, linea (\d+) de 20001, (\d+) ms", 30, k)
        tempos["ctrl_end"] = int(g[1])
        verificar(int(g[0]) > 19900, f"Ctrl+End: o fim do arquivo (linha {g[0]}) em {g[1]} ms "
                                     f"(o realce das 20.000 linhas); {time.time() - t0:.1f} s com o QEMU")
        k = len(m.serial())
        for _ in range(15):
            m.tecla("pgup", 0.05)
        time.sleep(1.5)
        ms = [int(x) for x in re.findall(r"mico: dibujo completo, linea \d+ de 20001, (\d+) ms", m.serial()[k:])]
        tempos["repag"] = max(ms) if ms else -1
        verificar(ms and max(ms) < 500, f"15 x RePág: {len(ms)} desenhos, o mais lento {max(ms) if ms else '-'} ms")
        k = len(m.serial())
        m.mouse_para(X + lay["ex"] + 300, Y + lay["ey"] + 150)
        for _ in range(10):
            m.monitor("mouse_move 0 0 -1")
            time.sleep(0.08)
        time.sleep(1)
        ms = [int(x) for x in re.findall(r"mico: dibujo completo, linea \d+ de 20001, (\d+) ms", m.serial()[k:])]
        tempos["rueda"] = max(ms) if ms else -1
        verificar(ms and max(ms) < 500, f"rueda: {len(ms)} desenhos, o mais lento {max(ms) if ms else '-'} ms")
        k = len(m.serial())
        m.tecla("ctrl-g")
        m.digitar("10000\n", 0.05)
        g = esperar(m, r"mico: dibujo completo, linea (\d+) de 20001, (\d+) ms", 15, k)
        tempos["ir_10000"] = int(g[1])
        verificar(abs(int(g[0]) - 10000) < 40, f"Ctrl+G 10000 em {g[1]} ms")
        k = len(m.serial())
        m.digitar("x", 0.05)
        esperar(m, r"mico: titulo Mico - grande.c \* - mico", 10, k)
        verificar(True, "digitar no meio de um arquivo grande")
        print("  tiempos (ms, medidos por el propio Mico):", tempos)
        time.sleep(0.5)
        m.captura("H64-10-grande")

        m.tecla("ctrl-z")

        # ---------------------------------------------------------------- (i) mais edição
        # explorador: botão direito no vazio > Nuevo archivo...
        k = len(m.serial())
        m.clique(X + 100, Y + 24 + EXPL_CAB + 12 * EXPL_LINHA, botao=2)
        p = painel(m, k)
        m.clique(p["x"] + 40, y_linha(p, "Nuevo archivo..."))
        esperar(m, r'criada por \d+: "Nuevo archivo"', 10, k)
        time.sleep(0.5)
        m.digitar("prueba.py\n", 0.05)
        esperar(m, r"mico: abierto /ejemplos/mico/prueba.py", 10, k)
        verificar("mico: creado /ejemplos/mico/prueba.py" in m.serial()[k:], "explorador, menú: Nuevo archivo crea e abre prueba.py")
        # recuo automático, pares de parênteses, Ctrl+D, Alt+Arriba e Ctrl+/
        m.digitar("def f(x):\nreturn x", 0.05)
        m.tecla("ctrl-d")
        m.tecla("end")
        m.digitar(" + 1", 0.05)
        m.tecla("alt-up")
        k = len(m.serial())
        m.tecla("ctrl-shift-7")
        esperar(m, r"mico: comentadas lineas 2-2", 10, k)
        m.tecla("ctrl-s")
        esperar(m, r"mico: guardado /ejemplos/mico/prueba.py", 10, k)
        # duplo e triplo clique na linha 3 ("    return x")
        tx = X + lay["ex"] + 40 + 4
        m.clique(tx + 6 * 8, Y + lay["ey"] + 2 * 16 + 8, duplo=True)
        g = esperar(m, r"mico: doble clic, palabra de (\d+) letras", 10, k)
        verificar(g == "6", "duplo clique seleciona a palavra (return)")
        k = len(m.serial())
        for _ in range(3):   # três cliques seguidos, no mesmo lugar
            m.monitor("mouse_button 1")
            time.sleep(0.04)
            m.monitor("mouse_button 0")
            time.sleep(0.08)
        esperar(m, r"mico: clic triple, linea 3", 10, k)
        verificar(True, "clique triplo seleciona a linha")
        focar(m, terminal["id"])
        out = m.comando("cat /ejemplos/mico/prueba.py")
        verificar("def f(x):\n    # return x + 1\n    return x" in out.replace("\r", ""),
                  "recuo automático depois de \":\", parênteses emparelhados, Ctrl+D, Alt+Arriba e Ctrl+/ (cat do arquivo)")
        # aviso de arquivo mudado no disco (sem mudanças no Mico: recarrega sozinho)
        k = len(m.serial())
        m.comando("echo '# otra linea' >> /ejemplos/mico/prueba.py")
        focar(m, w["id"])
        esperar(m, r"mico: recargado /ejemplos/mico/prueba.py", 15, k)
        verificar("mico: /ejemplos/mico/prueba.py cambio en disco" in m.serial()[k:],
                  "o Mico nota que outro programa mudou o arquivo e o recarrega")
        # Ctrl+P (abrir por nome) e buscar na pasta
        k = len(m.serial())
        m.tecla("ctrl-p")
        m.digitar("lista", 0.05)
        m.tecla("ret")
        esperar(m, r"mico: abierto /ejemplos/mico/datos/lista.txt", 10, k)
        verificar(True, "Ctrl+P: abrir por nome (datos/lista.txt)")
        k = len(m.serial())
        m.tecla("ctrl-shift-f")
        m.digitar("Mico\n", 0.05)
        g = esperar(m, r"mico: busqueda en carpeta 'Mico': (\d+) resultados en (\d+) archivos", 15, k)
        verificar(int(g[0]) >= 5 and int(g[1]) >= 4, f"Buscar en la carpeta: {g[0]} resultados em {g[1]} arquivos")
        time.sleep(0.5)
        m.mouse_para(X + 700, Y + lay["py"] + 150)
        m.captura("H64-13-buscar-carpeta")
        k = len(m.serial())
        m.clique(X + lay["px"] + 200, Y + lay["py"] + 22 + 2 + 9)
        esperar(m, r"mico: salto a /ejemplos/mico/\S+:\d+", 10, k)
        verificar(True, "clique num resultado da busca abre o arquivo na linha")
        m.tecla("esc")

        # tema escuro pela paleta, para a captura
        k = len(m.serial())
        m.tecla("ctrl-shift-p")
        m.digitar("oscuro", 0.05)
        m.tecla("ret")
        esperar(m, r"mico: paleta -> Tema oscuro", 10, k)
        m.tecla("ctrl-tab")
        time.sleep(0.8)
        m.captura("H64-11-oscuro")
        # menu da barra
        k = len(m.serial())
        m.clique(X + 6 + 8 * 7 + 16 + 20, Y + 12)   # "Editar"
        p = painel(m, k)
        verificar("Cortar\tCtrl+X" in p["linhas"] or any(l.startswith("Cortar") for l in p["linhas"]),
                  f"menu Editar ({len(p['linhas'])} linhas, com o submenu Sangría)")
        verificar(">Sangría" in p["linhas"], "o menu Editar tem o submenu Sangría")
        time.sleep(0.4)
        m.captura("H64-12-menu-editar")
        m.tecla("esc")

        # ---------------------------------------------------------------- (h)
        k = len(m.serial())
        m.clique(40, 12)
        p = painel(m, k)
        s = submenu_pelo_mouse(m, p, "Accesorios", "Mico (editor de código)")
        verificar("Mico (editor de código)" in s["linhas"], "Inicio > Accesorios tem o Mico")
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/mico", 10, k)
        esperar(m, r"mico: pronto \(1 pestanas, carpeta -\)", 20, k)
        verificar(True, "e abre o Mico")
    return 0


if __name__ == "__main__":
    sys.exit(main())
