#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H41 - Administrador de tareas (D043).

Critério: `tareas` abre a janela e lista os processos (incluindo sagsh e o
próprio tareas); o uso de CPU sobe com um programa que gira sem dormir
(/testes/girar); ordenar pela coluna; não deixa finalizar o processo 0 nem
uma tarefa do kernel; "Finalizar proceso" + "Sí" mata o processo (ele some da
lista e do `ps`); a aba Rendimiento desenha os gráficos; a aba Ventanas fecha
a janela de outro programa (SIS_PEDIR_FECHAR); `tareas -t`; Ctrl+Shift+Esc.
Capturas em docs/capturas/H41-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar  # noqa: E402

# Geometria da janela (userland/programas/tareas.c).
ABAS = {"procesos": 10 + 50, "rendimiento": 112 + 58, "ventanas": 230 + 48, "detalles": 488 + 42}
ABA_Y = 20
LINHA0_Y = 65 + 9           # centro da primeira linha da tabela
LIN_A = 18
COL_X = {"PID": 13 + 24, "Nombre": 61 + 64, "CPU %": 313 + 32, "Núcleo": 585 + 28}
PAUSA = (12 + 55, 430 + 14)
ACAO = (482 + 85, 430 + 14)
LISTA_JAN_Y = 66 + 2 + 10   # centro do primeiro item da lista de janelas


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24, fx=int(g[2]), fy=int(g[3]))


def esperar_novo(m, padrao, n_antes, tempo=20):
    """Espera aparecer na serial mais uma linha que case com o padrão."""
    fim = time.time() + tempo
    while time.time() < fim:
        achados = re.findall(padrao, m.serial())
        if len(achados) > n_antes:
            return achados[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tempo esgotado esperando {padrao!r}")


def contar(m, padrao):
    return len(re.findall(padrao, m.serial()))


def ultima_lista(m):
    lista = re.findall(r"tareas: lista \d+ procesos: ([^\r\n]*)", m.serial())[-1]
    return [(int(p.split(" ", 1)[0]), p.split(" ", 1)[1]) for p in lista.split(", ")]


def main():
    with Maquina("h41", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("calculadora")
        m.esperar_serial("calculadora: pronta", 20)
        time.sleep(0.5)
        # A Calculadora tem o foco; volta ao Terminal clicando na barra de título dele.
        term = janela(m.serial(), "Terminal")
        m.clique(term["fx"] + 60, term["fy"] + 10)
        m.comando("/testes/girar &")
        m.esperar_serial("girar: comecando", 20)
        pid_g = int(re.findall(r"girar: comecando \(pid (\d+)\)", m.serial())[-1])
        m.comando("tareas")
        m.esperar_serial("tareas: pronto", 20)
        pid_t = int(re.findall(r"tareas: pronto \(pid (\d+)\)", m.serial())[-1])
        j = janela(m.serial(), "Administrador de tareas")
        ox, oy = j["x"], j["y"]

        # --- Procesos
        nomes = [n for _, n in ultima_lista(m)]
        pids = [p for p, _ in ultima_lista(m)]
        verificar("sagsh" in nomes and "tareas" in nomes and "ocioso" in nomes and "janelas" in nomes,
                  f"lista os processos, incluindo sagsh e o próprio tareas: {nomes}")
        verificar(pid_g in pids and pid_t in pids, f"girar (pid {pid_g}) e tareas (pid {pid_t}) na lista")
        n = contar(m, r"tareas: cpu \d+% mayor")
        alto = None
        for _ in range(6):
            g = esperar_novo(m, r"tareas: cpu (\d+)% mayor pid (\d+) (\S+) (\d+)%", n, 10)
            n = contar(m, r"tareas: cpu \d+% mayor")
            if int(g[1]) == pid_g and int(g[3]) >= 50 and int(g[0]) >= 50:
                alto = g
                break
        verificar(alto is not None, f"o uso de CPU sobe com o girar: total {alto and alto[0]}%, girar {alto and alto[3]}%")

        n = contar(m, r"tareas: ordenar por")
        m.clique(ox + COL_X["CPU %"], oy + 42 + 10)
        o = esperar_novo(m, r"tareas: ordenar por (.*)", n)
        verificar(o.strip() == "CPU % desc", f"clique no cabeçalho ordena: {o.strip()}")
        time.sleep(1.3)
        m.captura("H41-tareas-procesos")
        n = contar(m, r"tareas: ordenar por")
        m.clique(ox + COL_X["PID"], oy + 42 + 10)
        o = esperar_novo(m, r"tareas: ordenar por (.*)", n)
        verificar(o.strip() == "PID asc", f"volta a ordenar por PID: {o.strip()}")

        # Columna "Núcleo" (D157): existe en la cabecera y se puede ordenar por ella.
        n = contar(m, r"tareas: ordenar por")
        m.clique(ox + COL_X["Núcleo"], oy + 42 + 10)
        o = esperar_novo(m, r"tareas: ordenar por (.*)", n)
        verificar(o.strip().startswith("N") and o.strip().endswith("desc"), f"columna Núcleo ordenable: {o.strip()!r}")
        time.sleep(1.3)
        m.captura("H41-tareas-nucleo")
        n = contar(m, r"tareas: ordenar por")
        m.clique(ox + COL_X["PID"], oy + 42 + 10)
        esperar_novo(m, r"tareas: ordenar por (.*)", n)

        # Pausa: a tabela para de mudar e as posições ficam previsíveis.
        m.clique(ox + PAUSA[0], oy + PAUSA[1])
        m.esperar_serial("tareas: pausado", 10)
        time.sleep(0.5)
        lista = ultima_lista(m)

        def selecionar(pid):
            i = [p for p, _ in lista].index(pid)
            k = contar(m, r"tareas: seleccionado pid")
            m.clique(ox + COL_X["Nombre"], oy + LINHA0_Y + i * LIN_A)
            s = esperar_novo(m, r"tareas: seleccionado pid (\d+)", k)
            verificar(int(s) == pid, f"seleciona a linha do pid {pid}")

        # O processo 0 (ocioso) e a tarefa "janelas" do kernel não podem ser finalizados.
        for pid in (0, [p for p, nm in lista if nm == "janelas"][0]):
            selecionar(pid)
            k = contar(m, r"tareas: no se puede finalizar pid")
            m.clique(ox + ACAO[0], oy + ACAO[1])
            s = esperar_novo(m, r"tareas: no se puede finalizar pid (\d+)", k)
            verificar(int(s) == pid, f"recusa finalizar o pid {pid} (kernel)")
            time.sleep(0.8)
            if pid == 0:
                m.captura("H41-tareas-no-kernel")
            m.tecla("ret")   # OK na mensagem
            time.sleep(0.6)
        verificar("processo 0 (" not in m.serial(), "o ocioso continua vivo")

        # Finalizar o girar, com confirmação.
        selecionar(pid_g)
        k = contar(m, r"tareas: confirmar finalizar")
        m.clique(ox + ACAO[0], oy + ACAO[1])
        esperar_novo(m, r"tareas: confirmar finalizar pid (\d+)", k)
        time.sleep(0.8)
        d = janela(m.serial(), "Finalizar proceso")
        m.captura("H41-tareas-confirmar")
        m.clique(d["x"] + d["l"] - 16 - 45, d["y"] + d["a"] - 40 + 13)   # "Sí"
        m.esperar_serial(f"tareas: finalizar pid {pid_g} (girar) -> 0", 10)
        m.esperar_serial(f"processo {pid_g} (girar) terminou", 10)
        verificar(True, "Finalizar proceso + Sí mata o girar")
        m.clique(ox + PAUSA[0], oy + PAUSA[1])
        m.esperar_serial("tareas: reanudado", 10)
        # Fica como zumbi até o pai (o sagsh) recolher o código de saída.
        fim = time.time() + 10
        while (pid_g, "girar (zombi)") not in ultima_lista(m) and time.time() < fim:
            time.sleep(0.3)
        verificar((pid_g, "girar (zombi)") in ultima_lista(m), "o girar aparece como zombi até o shell recolhê-lo")

        # --- Rendimiento
        m.clique(ox + ABAS["rendimiento"], oy + ABA_Y)
        m.esperar_serial("tareas: pestana rendimiento", 10)
        n = contar(m, r"tareas: rendimiento cpu")
        for _ in range(3):
            esperar_novo(m, r"tareas: rendimiento cpu", n, 10)
            n = contar(m, r"tareas: rendimiento cpu")
        r = re.findall(r"tareas: rendimiento cpu (\d+)% mem (\d+)/(\d+) KB \((\d+)%\) procesos (\d+) muestras (\d+)", m.serial())[-1]
        cpu, usada, total, pct, procs, amostras = map(int, r)
        verificar(0 <= cpu <= 100 and 0 < usada < total and procs >= 5 and amostras >= 5,
                  f"Rendimiento: CPU {cpu}%, memoria {usada}/{total} KB ({pct}%), {procs} procesos, {amostras} muestras")
        nl = re.findall(r"tareas: nucleos (\d+): ((?:\d+=\d+%\s?)+)", m.serial())[-1]
        usos = [int(x) for x in re.findall(r"\d+=(\d+)%", nl[1])]
        verificar(int(nl[0]) >= 1 and len(usos) == int(nl[0]) and all(0 <= u <= 100 for u in usos) and "0=" in nl[1],
                  f"Rendimiento: sección Núcleos con {nl[0]} CPU, usos {usos}")
        m.captura("H41-tareas-rendimiento")
        # D368: barra lateral de recursos; las líneas de swap, disco y GPU salen en cada muestra.
        verificar(re.search(r"tareas: swap tope \d+ KB almacen \d+ KB datos \d+ KB comprimida \d+ KB salidas \d+ entradas \d+", m.serial()),
                  "Rendimiento: el log incluye la swap")
        verificar(re.search(r"tareas: disco ativa [01] bloques \d+/\d+", m.serial()), "Rendimiento: el log incluye el disco")
        verificar(re.search(r"tareas: gpu 2d \S+ \d+ ops/s", m.serial()), "Rendimiento: el log incluye la GPU")

        # --- Detalles (D368): el proceso elegido, con sus hilos
        m.clique(ox + ABAS["detalles"], oy + ABA_Y)
        m.esperar_serial("tareas: pestana detalles", 10)
        m.esperar_serial("tareas: detalles pid", 10)
        m.captura("H41-tareas-detalles")

        # --- Ventanas: fecha a Calculadora pela lista
        m.clique(ox + ABAS["ventanas"], oy + ABA_Y)
        m.esperar_serial("tareas: pestana ventanas", 10)
        time.sleep(0.5)
        jl = re.findall(r"tareas: ventanas \d+: ([^\r\n]*)", m.serial())[-1].split("; ")
        ids = [int(x.split(" ", 1)[0]) for x in jl]
        titulos = [x.split(" ", 1)[1] for x in jl]
        verificar("Calculadora" in titulos and "Terminal" in titulos and "Administrador de tareas" in titulos,
                  f"lista as janelas: {titulos}")
        i = titulos.index("Calculadora")
        k = contar(m, r"tareas: seleccionada ventana")
        m.clique(ox + 200, oy + LISTA_JAN_Y + i * 20)
        s = esperar_novo(m, r"tareas: seleccionada ventana (\d+)", k)
        verificar(int(s) == ids[i], "seleciona a Calculadora na lista")
        time.sleep(0.3)
        m.captura("H41-tareas-ventanas")
        m.clique(ox + ACAO[0], oy + ACAO[1])
        m.esperar_serial(f"fechar janela {ids[i]} (Calculadora) a pedido do processo {pid_t}", 10)
        m.esperar_serial("(calculadora) terminou", 10)
        verificar(True, "\"Cerrar ventana\" fecha a Calculadora (EV_FECHAR, como o X)")

        # Fecha o tareas pelo X; o Terminal volta a ter o foco.
        m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)
        m.esperar_serial(f"processo {pid_t} (tareas) terminou", 10)
        time.sleep(0.5)
        out = m.comando("")   # o sagsh recolhe os processos de fundo antes do prompt
        # sagsh avisa como o bash (D037): "[n]+ Terminado   /testes/girar".
        verificar(re.search(r"\[\d+\][+-]? +Terminado +\S*girar", out) is not None, "o sagsh recolhe o girar")
        out = m.comando("ps")
        verificar("girar" not in out and "sagsh" in out, "o ps já não mostra o girar")
        n = len(re.findall(r"PID PADRE ESTADO", m.serial()))
        m.comando("tareas -t")
        fim = time.time() + 15
        while len(re.findall(r"PID PADRE ESTADO", m.serial())) <= n and time.time() < fim:
            time.sleep(0.3)
        time.sleep(1)
        saida = m.serial().rsplit("tareas -t", 1)[-1]
        verificar(re.search(r"CPU 0: \d+,\d %", saida) and "NUCL." in saida, "tareas -t añade una línea por núcleo y la columna NUCL.")
        verificar("PID PADRE ESTADO" in saida and "sagsh" in saida and re.search(r"CPU: \d+,\d %", saida),
                  "tareas -t mostra uma instantânea no terminal")
        # Um programa em segundo plano que escreve no Terminal não pode fechar o shell (D043).
        out = m.comando("echo sigo vivo")
        verificar("sigo vivo" in out and "hasta luego" not in m.serial(), "o sagsh continua vivo depois do tareas -t")

        # --- Ctrl+Shift+Esc
        n = m.serial().count("tareas: pronto")
        m.tecla("ctrl-shift-esc")
        m.esperar_serial("Ctrl+Shift+Esc -> /bin/tareas", 10)
        fim = time.time() + 15
        while m.serial().count("tareas: pronto") <= n and time.time() < fim:
            time.sleep(0.3)
        verificar(m.serial().count("tareas: pronto") > n, "Ctrl+Shift+Esc abre o Administrador de tareas")
        time.sleep(0.5)
        verificar(pid_g not in [p for p, _ in ultima_lista(m)], "depois de recolhido, o girar sumiu da lista")
        time.sleep(0.5)
        m.tecla("ctrl-shift-esc")
        m.esperar_serial("Administrador de tareas ja aberto", 10)
        verificar(True, "Ctrl+Shift+Esc de novo só traz a janela para cima")

        # --- Ícone na área de trabalho (segunda coluna, D043)
        j2 = janela(m.serial(), "Administrador de tareas")
        pid2 = int(re.findall(r"tareas: pronto \(pid (\d+)\)", m.serial())[-1])
        m.clique(j2["fx"] + j2["l"] + 4 - 2 - 20 + 8, j2["fy"] + 12)
        m.esperar_serial(f"processo {pid2} (tareas) terminou", 10)
        n = m.serial().count("tareas: pronto")
        m.clique(20 + 96 + 8 + 48, 24 + 16 + 32, duplo=True)
        m.esperar_serial("icone Tareas -> /bin/tareas", 10)
        fim = time.time() + 15
        while m.serial().count("tareas: pronto") <= n and time.time() < fim:
            time.sleep(0.3)
        verificar(m.serial().count("tareas: pronto") > n, "duplo clique no ícone Tareas abre o programa")
    return 0


if __name__ == "__main__":
    sys.exit(main())
