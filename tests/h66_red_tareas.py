#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H66 (D079) - pestaña «Red» del Administrador de tareas.

Critério:
  - o loopback (127.0.0.1) funciona: nc -l na VM recebe de nc 127.0.0.1 e a
    interface "lo" conta o tráfego;
  - com um curl lento (servidor HTTP do anfitrião que manda 1,2 MB em ~30 s)
    a aba Red mostra eth0 conectada, por DHCP, com os contadores subindo, a
    velocidade de recepção > 0 e o gráfico do último minuto com tráfego;
  - a lista de conexões mostra a do curl (TCP, 10.0.2.2:porta, ESTABLECIDA)
    com o PID e o nome do processo;
  - botão direito na conexão abre o menu de contexto; "Cerrar conexión..."
    pede confirmação e corta a conexão (RST): o curl termina com erro;
  - "Renovar DHCP" renova a concessão sem travar a janela;
  - "Desactivar interfaz" (com confirmação) e "Activar interfaz";
  - fora da aba Red o programa não consulta a rede;
  - a janela é redimensionável: maximizada, a aba usa o espaço todo.
Capturas em docs/capturas/H66-*.png.
"""
import http.server
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar  # noqa: E402

# Geometria de userland/programas/tareas.c no tamanho inicial (664x470).
ABA_RED = (328 + 32, 20)
TAB_X = 12
LINHA0_Y = 242 + 1 + 20 + 2 + 9          # centro da 1ª linha da tabela de conexões
LIN_A = 18
B_RENOVAR = (356 + 60, 42 + 11)
B_ATIVAR = (484 + 84, 42 + 11)
LINHA_MENU, SEP_MENU = 22, 9             # kernel/gui/menu.c
DIREITO = 2

LENTO_BLOCOS, LENTO_PAUSA, LENTO_BLOCO = 120, 0.25, 10 * 1024


class Lento(http.server.BaseHTTPRequestHandler):
    """Manda 1,2 MB em ~30 s: a conexão fica aberta enquanto o teste olha."""
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        n = LENTO_BLOCOS * LENTO_BLOCO
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(n))
        self.send_header("Connection", "close")
        self.end_headers()
        for _ in range(LENTO_BLOCOS):
            try:
                self.wfile.write(b"r" * LENTO_BLOCO)
                self.wfile.flush()
            except OSError:
                return   # cortaram a conexão (é o que o teste quer)
            time.sleep(LENTO_PAUSA)


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24, fx=int(g[2]), fy=int(g[3]))


def esperar(m, padrao, desde=0, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tempo esgotado esperando {padrao!r}")


def linhas_red(m, nome, desde=0):
    """(rx_bytes, tx_bytes, vel_rx, vel_tx, estado) de cada amostra da interface."""
    return [(int(g[1]), int(g[2]), int(g[3]), int(g[4]), g[0]) for g in re.findall(
        rf"tareas: red {nome} (\S+) rx (\d+) tx (\d+) bytes rx \d+ tx \d+ paquetes vel rx (\d+) tx (\d+) B/s",
        m.serial()[desde:])]


def sim(m, titulo):
    """Clica em "Sí" no diálogo de confirmação (userland/libsagui/gui.c)."""
    time.sleep(0.8)
    d = janela(m.serial(), titulo)
    m.clique(d["x"] + d["l"] - 16 - 45, d["y"] + d["a"] - 40 + 13)


def main():
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Lento)
    porta = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        with Maquina("h66", "ide") as m:
            m.esperar_serial("sagui:/$", 120)
            esperar(m, r"\[REDE\] DHCP: ip 10\.0\.2\.15", tempo=30)
            time.sleep(1)

            # --- loopback: nc -l e nc 127.0.0.1 na própria VM
            m.comando("echo -n > /tmp/vazio")   # entrada vazia: o nc -l em segundo plano não lê o terminal
            m.comando("nc -l 9000 < /tmp/vazio > /tmp/lo.txt &")
            time.sleep(1)
            m.comando("echo hola por lo | nc 127.0.0.1 9000", tempo=30)
            time.sleep(1)
            out = m.comando("cat /tmp/lo.txt")
            verificar("hola por lo" in out, "loopback: nc 127.0.0.1 chega ao nc -l da própria VM")

            # --- tráfego: um curl lento em segundo plano
            m.comando(f"curl -o /tmp/lento.bin http://10.0.2.2:{porta}/lento &")
            pid_curl = int(esperar(m, r"executar /bin/curl -> processo (\d+)"))
            m.comando("tareas &")
            m.esperar_serial("tareas: pronto", 20)
            j = janela(m.serial(), "Administrador de tareas")
            ox, oy = j["x"], j["y"]
            marca = len(m.serial())
            m.clique(ox + ABA_RED[0], oy + ABA_RED[1])
            esperar(m, r"tareas: pestana red", marca, 10)

            con = esperar(m, rf"tareas: conexiones \d+: ([^\r\n]*TCP 10\.0\.2\.15:\d+ 10\.0\.2\.2:{porta} ESTABLECIDA pid {pid_curl} curl[^\r\n]*)",
                          marca, 20)
            verificar(True, f"a lista de conexões mostra a do curl com PID e nome: {con[:90]}")
            time.sleep(4)
            eth = linhas_red(m, "eth0", marca)
            verificar(len(eth) >= 3, f"a aba Red amostra a eth0 a cada segundo ({len(eth)} amostras)")
            verificar(eth[-1][4] == "conectada" and " dhcp" in m.serial()[marca:], "eth0 conectada, por DHCP")
            verificar(eth[-1][0] > eth[0][0], f"os bytes recebidos sobem ({eth[0][0]} -> {eth[-1][0]})")
            verificar(max(e[2] for e in eth) > 1000, f"velocidade de recepção > 1 KB/s (máx. {max(e[2] for e in eth)} B/s)")
            graf = re.findall(r"tareas: grafica red eth0 escala (\d+) B/s muestras (\d+) suma rx (\d+) tx (\d+)", m.serial()[marca:])
            verificar(graf and int(graf[-1][2]) > 0 and int(graf[-1][1]) > 0,
                      f"o gráfico do último minuto tem tráfego (escala {graf and graf[-1][0]} B/s, {graf and graf[-1][1]} amostras)")
            lo = linhas_red(m, "lo", marca)
            verificar(lo and lo[-1][0] > 0, f"a interface lo contou o tráfego do nc ({lo and lo[-1][0]} bytes)")
            m.captura("H66-tareas-red")

            # --- seleciona a conexão do curl (a tabela ordena por protocolo: TCP primeiro)
            achou = False
            for i in range(4):
                k = len(m.serial())
                m.clique(ox + TAB_X + 30, oy + LINHA0_Y + i * LIN_A)
                g = esperar(m, r"tareas: seleccionada conexion (\S+) puerto (\d+) pid (-?\d+)", k, 10)
                if int(g[2]) == pid_curl:
                    achou = True
                    linha = i
                    break
            verificar(achou, "clique seleciona a conexão do curl")

            # --- botão direito: menu de contexto; "Cerrar conexión..." com confirmação
            k = len(m.serial())
            m.clique(ox + TAB_X + 30, oy + LINHA0_Y + linha * LIN_A, botao=DIREITO)
            menu = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", k, 10)
            mx, my, ml = int(menu[0]), int(menu[1]), int(menu[2])
            verificar(int(menu[4]) == 4, f"botão direito na conexão abre o menu ({menu[4]} linhas)")
            time.sleep(0.5)
            m.captura("H66-tareas-menu")
            m.clique(mx + ml // 2, my + 2 + LINHA_MENU // 2)   # linha 0: "Cerrar conexión..."
            esperar(m, r"tareas: confirmar cerrar conexion id \d+", k, 10)
            time.sleep(0.8)
            m.captura("H66-tareas-confirmar")
            sim(m, "Cerrar conexión")
            r = esperar(m, r"tareas: cerrar conexion id \d+ -> (-?\d+)", k, 10)
            verificar(r == "0", "Sí corta a conexão (REDE_FECHAR -> 0)")
            verificar("cortada a pedido (RST)" in m.serial()[k:], "o kernel manda o RST")
            cod = esperar(m, rf"processo {pid_curl} \(curl\) terminou com codigo (-?\d+)", k, 20)
            verificar(cod != "0", f"o curl termina com erro (código {cod})")
            time.sleep(1.5)
            verificar(not re.search(rf"TCP [^;]*:{porta} ESTABLECIDA pid {pid_curl}", re.findall(r"tareas: conexiones [^\r\n]*", m.serial())[-1]),
                      "a conexão sai da lista")

            # --- Renovar DHCP (sem travar: a janela continua amostrando)
            k = len(m.serial())
            m.clique(ox + B_RENOVAR[0], oy + B_RENOVAR[1])
            r = esperar(m, r"tareas: renovar dhcp -> (-?\d+)", k, 10)
            verificar(r == "0", "Renovar DHCP -> 0")
            esperar(m, r"\[REDE\] DHCP: concessao renovada", k, 15)
            verificar(True, "o servidor DHCP confirma a renovação")
            n0 = len(linhas_red(m, "eth0", k))
            time.sleep(2.5)
            verificar(len(linhas_red(m, "eth0", k)) > n0, "a janela segue atualizando durante a renovação")

            # --- Desactivar / Activar
            k = len(m.serial())
            m.clique(ox + B_ATIVAR[0], oy + B_ATIVAR[1])
            sim(m, "Desactivar interfaz")
            r = esperar(m, r"tareas: eth0 desactivar -> (-?\d+)", k, 10)
            verificar(r == "0" and "[REDE] eth0 desativada" in m.serial()[k:], "Desactivar interfaz (com confirmação)")
            esperar(m, r"tareas: red eth0 desactivada", k, 10)
            time.sleep(1.2)
            m.captura("H66-tareas-desactivada")
            k = len(m.serial())
            m.clique(ox + B_ATIVAR[0], oy + B_ATIVAR[1])
            r = esperar(m, r"tareas: eth0 activar -> (-?\d+)", k, 10)
            verificar(r == "0" and "[REDE] eth0 ativada" in m.serial()[k:], "Activar interfaz")
            esperar(m, r"tareas: red eth0 conectada", k, 10)

            # --- fora da aba Red, nada de consultas à rede
            k = len(m.serial())
            m.clique(ox + 10 + 50, oy + 20)   # Procesos
            esperar(m, r"tareas: pestana procesos", k, 10)
            k = len(m.serial())
            time.sleep(3)
            verificar("tareas: red " not in m.serial()[k:], "na aba Procesos a rede não é consultada")
            m.clique(ox + ABA_RED[0], oy + ABA_RED[1])
            esperar(m, r"tareas: pestana red", k, 10)

            # --- maximizar (duplo clique no título): a aba usa a janela toda
            k = len(m.serial())
            m.clique(j["fx"] + 120, j["fy"] + 10, duplo=True)
            t = esperar(m, r"tareas: tamano (\d+)x(\d+)", k, 10)
            verificar(int(t[0]) > 664 and int(t[1]) > 470, f"maximizada: {t[0]}x{t[1]}")
            esperar(m, r"tareas: red eth0", k, 10)
            time.sleep(1.2)
            m.captura("H66-tareas-red-maximizada")
    finally:
        srv.shutdown()
    print("H66 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
