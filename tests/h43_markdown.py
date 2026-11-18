#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H43 - visor e editor de Markdown (/bin/markdown, D045).

Critério:
 (a) no console, `markdown -t /ejemplos/ejemplo.md` imprime o documento
     maquetado: títulos sublinhados com === e ---, marcadores de lista
     aninhados, tarefas, tabela alinhada, código recuado, citação, enlaces,
     imagem como "[imagen: ...]" e escapes;
 (b) na área de trabalho, `markdown /ejemplos/ejemplo.md` abre no visor
     (títulos e maquetação registrados na serial, captura H43-visor);
     Ctrl+E vai para o editor, uma linha "## Prueba" é escrita e salva com
     Ctrl+S (em UTF-8), Ctrl+E volta ao visor e o novo título aparece;
     fechar com alterações mostra o aviso; um clique num enlace a outro .md
     abre esse arquivo; a vista dividida funciona; o Archivos abre .md com o
     markdown.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EJEMPLO = open(os.path.join(RAIZ, "rootfs", "ejemplos", "ejemplo.md"), encoding="utf-8").read()
BARRA = 36          # barra de botões do markdown
ALT_PAINEL = 600 - BARRA - 20


def janela(serial, titulo):
    m = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24, fx=int(m[2]), fy=int(m[3]))


def fechar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def esperar_n(m, texto, n, tempo=20):
    fim = time.time() + tempo
    while time.time() < fim:
        if m.serial().count(texto) >= n:
            return True
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {n}x {texto!r}")


def parte_console():
    with Maquina("h43-console", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        antes = len(m.serial())
        m.comando("markdown -t /ejemplos/ejemplo.md")
        m.esperar_serial("(markdown) terminou com codigo 0", 30)
        time.sleep(0.5)
        # sem o log do kernel e o prompt que o sagsh redesenha (o markdown roda em segundo plano)
        linhas = [re.sub(r"\[SAGUI\].*", "", l) for l in m.serial()[antes:].replace("\r", "").split("\n")
                  if not l.startswith("sagui:")]
        texto = "\n".join(linhas)

        def tem(*seq):
            """As linhas aparecem nessa ordem, uma logo depois da outra."""
            for i in range(len(linhas) - len(seq) + 1):
                if all(linhas[i + k].rstrip() == seq[k] for k in range(len(seq))):
                    return True
            return False

        verificar(tem("Ejemplo de Markdown", "==================="), "H1 sublinhado com ===")
        verificar(tem("Listas", "------"), "H2 sublinhado com ---")
        verificar("con cursiva, negrita, las dos a la vez, tachado y" in texto.replace("\n", " "),
                  "ênfase, tachado e código sem os sinais de Markdown")
        verificar(tem("* Primer elemento", "* Segundo elemento, con una lista dentro:", "  - anidada con sangría",
                      "  - otra más", "    + y un tercer nivel", "* Tercer elemento"), "listas aninhadas com marcadores")
        verificar(tem("1. Uno", "2. Dos", "3. Tres"), "lista numerada")
        verificar(tem("[x] Escribir el visor", "[x] Escribir el editor", "[ ] Ventanas redimensionables"), "lista de tarefas")
        verificar(tem("    int principal(int argc, char **argv)", "    {"), "bloco de código entre cercas, recuado")
        verificar(tem("    $ markdown -t /ejemplos/ejemplo.md"), "bloco de código com recuo")
        verificar(tem("> Una cita puede tener énfasis y ocupar varias líneas.", ">", "> > Y también puede haber citas dentro de citas."),
                  "citações (também aninhadas)")
        verificar(tem("Elemento  |  Sintaxis  | Nivel", "----------+------------+------",
                      "Título    |  # Título  |     1", "Subtítulo |   ## Sub   |     2", "Lista     | - elemento |     -"),
                  "tabela com colunas alinhadas (esquerda, centro, direita)")
        verificar("ayuda del programa <../ayuda/markdown.md>" in texto and "https://commonmark.org" in texto, "enlaces")
        verificar(tem("[imagen: El sagui en la rama]"), "imagem como [imagen: alt]")
        verificar(tem("Escapes: *esto no es cursiva* y # esto no es un encabezado."), "escapes com \\")
        verificar(tem("-" * 78), "regra horizontal")
        out = m.comando("markdown -t /naoexiste.md")
        m.esperar_serial("no se ha podido abrir /naoexiste.md", 20)
        verificar(True, "arquivo inexistente: mensagem de erro")


def parte_grafica():
    with Maquina("h43", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("ayuda markdown")
        verificar("markdown [-t] [archivo.md]" in out, "ajuda markdown explica o uso")

        # ---- visor
        m.comando("markdown /ejemplos/ejemplo.md")
        m.esperar_serial("markdown: pronto (modo visor)", 30)
        s = m.serial()
        tit = re.findall(r"markdown: h(\d) (.*)", s)
        verificar([t for t, _ in tit] == ["1", "2", "3", "2", "2", "2", "2", "4", "5", "6"],
                  f"títulos H1 a H6 reconhecidos ({len(tit)})")
        mq = re.findall(r"markdown: maquetado (\d+) bloques, (\d+) lineas, (\d+) elementos, alto (\d+) px", s)[-1]
        blocos, linhas, alto = int(mq[0]), int(mq[1]), int(mq[3])
        verificar(blocos >= 30 and linhas >= 50 and alto > ALT_PAINEL, f"maquetado: {blocos} blocos, {linhas} linhas, {alto} px")
        time.sleep(1)
        j = janela(m.serial(), "Markdown - sin nombre")
        m.mouse_para(j["x"] + 790, j["y"] + 300)   # tira o cursor de cima do texto
        m.captura("H43-visor")

        # ---- editor: acrescenta "## Prueba" e salva
        m.tecla("ctrl-e")
        m.esperar_serial("markdown: modo editor", 10)
        time.sleep(0.5)
        m.captura("H43-editor")
        m.tecla("ctrl-end")
        m.digitar("\n## Prueba", 0.05)
        m.esperar_serial("markdown: titulo Markdown - ejemplo.md *", 10)
        verificar(True, "editar põe * no título")
        m.tecla("ctrl-s")
        m.esperar_serial("markdown: guardado /ejemplos/ejemplo.md", 10)
        n = int(re.findall(r"markdown: guardado /ejemplos/ejemplo.md \((\d+) bytes\)", m.serial())[-1])
        esperado = len((EJEMPLO + "\n## Prueba").encode("utf-8"))
        verificar(n == esperado, f"Ctrl+S grava em UTF-8 ({n} bytes, esperado {esperado})")
        fim = time.time() + 10
        while time.time() < fim and sum(l.strip().endswith("markdown: titulo Markdown - ejemplo.md")
                                        for l in m.serial().split("\n")) < 2:
            time.sleep(0.2)
        titulos = re.findall(r"markdown: titulo (.*)", m.serial())
        verificar(titulos[-1].strip() == "Markdown - ejemplo.md", "depois de salvar o título perde o *")
        ini = len(m.serial())
        m.tecla("ctrl-e")
        m.esperar_serial("markdown: modo visor", 10)
        time.sleep(0.5)
        depois = m.serial()[ini:]
        verificar("markdown: h2 Prueba" in depois, "de volta ao visor, o novo título aparece")
        m.tecla("end")
        time.sleep(0.5)
        m.captura("H43-visor-prueba")

        # ---- fechar com alterações: aviso
        m.tecla("f5")
        esperar_n(m, "markdown: modo editor", 2)
        m.digitar("x", 0.05)
        esperar_n(m, "markdown: titulo Markdown - ejemplo.md *", 2)
        fechar(m, janela(m.serial(), "Markdown - sin nombre"))
        m.esperar_serial('"Markdown" 380x', 10)
        time.sleep(0.5)
        m.captura("H43-aviso")
        verificar(True, "fechar com alterações pergunta se quer guardar")
        m.tecla("esc")                                     # "No"
        m.esperar_serial("markdown: fechado", 10)
        verificar(True, "'No' fecha sem guardar")
        time.sleep(0.5)

        out = m.comando("markdown -t /ejemplos/ejemplo.md | cat")
        verificar("Prueba\n------" in out and "Título    |  # Título  |     1" in out,
                  "o arquivo salvo (UTF-8) reabre com o título novo e os acentos")

        # ---- enlace a outro .md e vista dividida
        m.comando("markdown /ejemplos/ejemplo.md")
        esperar_n(m, "markdown: pronto (modo visor)", 2, 30)
        s = m.serial()
        alto = int(re.findall(r"markdown: maquetado \d+ bloques, \d+ lineas, \d+ elementos, alto (\d+) px", s)[-1])
        ex, ey = map(int, re.findall(r"markdown: enlace \d+ \.\./ayuda/markdown\.md en (\d+),(\d+)", s)[-1])
        j = janela(s, "Markdown - sin nombre")
        m.tecla("end")
        time.sleep(0.5)
        desl = max(0, alto - ALT_PAINEL)
        m.clique(j["x"] + ex + 12, j["y"] + BARRA + ey - desl + 8)
        m.esperar_serial("markdown: aberto /ayuda/markdown.md", 10)
        verificar(True, "clique no enlace ../ayuda/markdown.md abre a ajuda")
        time.sleep(0.5)
        m.clique(j["x"] + 434 + 36, j["y"] + 6 + 12)        # Dividir
        m.esperar_serial("markdown: modo dividido", 10)
        m.tecla("ctrl-end")
        m.digitar("\n- Nuevo elemento", 0.05)
        time.sleep(1)
        m.mouse_para(j["x"] + 790, j["y"] + 300)
        m.captura("H43-dividido")
        verificar(True, "vista dividida: editor à esquerda, vista prévia à direita")
        fechar(m, j)
        m.esperar_serial('"Markdown" 380x', 10)
        time.sleep(0.4)
        m.tecla("esc")
        esperar_n(m, "markdown: fechado", 2)

        # ---- Archivos abre .md com o markdown
        m.comando("arquivos /ejemplos")
        m.esperar_serial("arquivos: pronto", 20)
        time.sleep(0.5)
        a = janela(m.serial(), "Archivos")
        base = os.path.join(RAIZ, "rootfs", "ejemplos")   # o Archivos põe as pastas primeiro (/ejemplos/mico, D075)
        arqs = sorted(os.listdir(base), key=lambda n: (not os.path.isdir(os.path.join(base, n)), n))
        i = arqs.index("ejemplo.md")
        m.clique(a["x"] + 146 + 120, a["y"] + 70 + 2 + i * 20 + 8, duplo=True)
        esperar_n(m, "markdown: aberto /ejemplos/ejemplo.md", 3)
        verificar(True, "duplo clique num .md no Archivos abre o markdown")


def main():
    parte_console()
    parte_grafica()
    return 0


if __name__ == "__main__":
    sys.exit(main())
