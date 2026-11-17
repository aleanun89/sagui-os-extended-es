#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H90 (D119, mejora 058) - atajos de teclado del escritorio (v6.0 beta).

Criterio:
  - AltGr sigue escribiendo (AltGr+2 = "@"), también con Ctrl a la vez (Ctrl+AltGr+2, como
    mandan algunos anfitriones), sin abrir el Administrador de tareas;
  - Alt+Tab rápido alterna entre las dos últimas ventanas (y vuelve);
  - con Alt apretada: panel centrado con icono y título; cada Tab avanza, Mayús+Tab retrocede,
    Esc cancela; al soltar Alt la elegida gana el foco;
  - Alt+Tab trae (restaura) una ventana minimizada;
  - la tecla Windows sola abre y cierra el menú Inicio;
  - Windows+D minimiza todo (sin abrir el menú) y repetirlo lo restaura;
  - Ctrl+Alt+Supr abre el Administrador de tareas y, otra vez, sólo lo trae arriba;
  - Alt+F4 cierra la ventana activa con el mismo aviso que la X: el Editor con cambios pregunta
    si guarda; sin ventana activa no hace nada.

Las teclas mantenidas (Alt durante varios Tab) van por QMP (input-send-event), que separa
pulsar y soltar; lo demás por sendkey.
"""
import json
import os
import re
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA, cor_do_pixel  # noqa: E402


class Qmp:
    def __init__(self, caminho):
        for _ in range(100):
            if os.path.exists(caminho):
                break
            time.sleep(0.05)
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(caminho)
        self.s.settimeout(10)
        self.buf = b""
        self._ler()   # saludo
        self.cmd("qmp_capabilities")

    def _ler(self):
        while b"\n" not in self.buf:
            self.buf += self.s.recv(65536)
        linha, self.buf = self.buf.split(b"\n", 1)
        return json.loads(linha)

    def cmd(self, nome, **args):
        self.s.sendall(json.dumps({"execute": nome, "arguments": args}).encode() + b"\n")
        while True:
            r = self._ler()
            if "return" in r or "error" in r:
                return r

    def tecla(self, nome, apertar):
        self.cmd("input-send-event", events=[{"type": "key", "data": {"down": apertar,
                                                                       "key": {"type": "qcode", "data": nome}}}])
        time.sleep(0.12)

    def apertar(self, *nomes):
        for n in nomes:
            self.tecla(n, True)

    def soltar(self, *nomes):
        for n in nomes:
            self.tecla(n, False)

    def toque(self, nome):
        self.apertar(nome)
        self.soltar(nome)


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
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def foco(m):
    """El id de la ventana con el foco, por la última línea de la barra en el log."""
    l = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    f = re.findall(r"(\d+)@\d+\+\d+f", l)
    return int(f[0]) if f else None


def esperar_foco(m, ident, tempo=10):
    fim = time.time() + tempo
    while time.time() < fim:
        if foco(m) == ident:
            return True
        time.sleep(0.2)
    raise AssertionError(f"la ventana {ident} no ganó el foco (foco: {foco(m)})")


def main():
    sock = os.path.join(SAIDA, "h90-qmp.sock")
    if os.path.exists(sock):
        os.remove(sock)
    with Maquina("h90", "ide", extras=["-qmp", f"unix:{sock},server,nowait"]) as m:
        q = Qmp(sock)
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        t = janela(m.serial(), "Terminal")

        # ---- AltGr sigue escribiendo
        out = m.comando("echo x@y")
        verificar("x@y" in out, "AltGr+2 escribe @ en el terminal")
        marca = len(m.serial())
        m.digitar("echo a", 0.05)
        q.apertar("ctrl", "alt_r", "2")
        q.soltar("2", "alt_r", "ctrl")
        m.digitar("b", 0.05)
        m.tecla("ret")
        esperar(m, r"\na@b", desde=marca)
        verificar("Ctrl+Alt+Supr" not in m.serial()[marca:], "Ctrl+AltGr+2 también escribe @ (y no es Ctrl+Alt+Supr)")

        out = m.comando("ajuda atajos")
        verificar("Windows+D" in out and "Alt+F4" in out and "Ctrl+Alt+Supr" in out, "ajuda atajos explica los atajos")

        # ---- tres ventanas: Terminal, Calculadora y Editor
        m.comando("calculadora; sleep 1; editor")
        m.esperar_serial('"Calculadora"', 20)
        c = janela(m.serial(), "Calculadora")
        m.esperar_serial("editor: pronto", 20)
        e = janela(m.serial(), "Editor - sin nombre")
        esperar_foco(m, e["id"])
        time.sleep(0.5)

        # ---- Alt+Tab rápido: las dos últimas
        marca = len(m.serial())
        m.tecla("alt-tab", 0.3)
        esperar(m, rf"alt\+tab: ativada janela {c['id']}", desde=marca)
        esperar_foco(m, c["id"])
        verificar(True, "Alt+Tab rápido pasa del Editor a la Calculadora (la anterior)")
        marca = len(m.serial())
        m.tecla("alt-tab", 0.3)
        esperar(m, rf"alt\+tab: ativada janela {e['id']}", desde=marca)
        esperar_foco(m, e["id"])
        verificar(True, "otro Alt+Tab rápido vuelve al Editor")

        # ---- Alt apretada: panel, Tab avanza, Mayús+Tab retrocede
        marca = len(m.serial())
        q.apertar("alt")
        q.toque("tab")
        esperar(m, rf"alt\+tab: 3 janelas, escolhida 1 \(id {c['id']}", desde=marca)
        g = esperar(m, r"alt\+tab: painel (\d+)x(\d+) em \((\d+),(\d+)\)", desde=marca)
        pl, pa, px, py = map(int, g)
        verificar(abs((px + pl // 2) - 512) <= 1 and abs((py + pa // 2) - 384) <= 1, f"panel centrado ({pl}x{pa} en {px},{py})")
        q.toque("tab")
        esperar(m, rf"alt\+tab: 3 janelas, escolhida 2 \(id {t['id']}", desde=marca)
        time.sleep(0.4)
        png = m.captura("H90-alt-tab")
        verificar(cor_do_pixel(png, px + 1, py + pa // 2) != cor_do_pixel(png, px - 4, py + pa // 2),
                  "el panel se ve en pantalla mientras Alt está apretada")
        verificar("ativada" not in m.serial()[marca:], "con Alt apretada, el foco no cambia")
        marca2 = len(m.serial())
        q.apertar("shift")
        q.toque("tab")
        q.soltar("shift")
        esperar(m, rf"alt\+tab: 3 janelas, escolhida 1 \(id {c['id']}", desde=marca2)
        q.toque("tab")
        q.soltar("alt")
        esperar(m, rf"alt\+tab: ativada janela {t['id']}", desde=marca)
        esperar_foco(m, t["id"])
        verificar(True, "Tab, Tab, Mayús+Tab, Tab y soltar Alt: el Terminal gana el foco")

        # ---- Esc cancela
        marca = len(m.serial())
        q.apertar("alt")
        q.toque("tab")
        q.toque("esc")
        q.soltar("alt")
        esperar(m, r"alt\+tab: cancelado", desde=marca)
        time.sleep(0.5)
        verificar("ativada" not in m.serial()[marca:] and foco(m) == t["id"], "Esc con Alt apretada cancela el Alt+Tab")

        # ---- tecla Windows sola
        marca = len(m.serial())
        m.tecla("meta_l")
        esperar(m, r"menu Inicio aberto \(teclado(, estilo \d+)?\)", desde=marca)
        time.sleep(0.4)
        m.captura("H90-tecla-windows")
        m.tecla("meta_l")
        esperar(m, r"menu: cancelado", desde=marca)
        verificar(True, "la tecla Windows sola abre y cierra el menú Inicio")

        # ---- Windows+D
        marca = len(m.serial())
        m.tecla("meta_l-d", 0.5)
        esperar(m, r"Windows\+D: 3 janelas minimizadas", desde=marca)
        time.sleep(0.6)
        verificar("menu Inicio aberto" not in m.serial()[marca:], "Windows+D no abre el menú")
        verificar(foco(m) is None, "Windows+D minimiza las tres ventanas")
        m.captura("H90-windows-d")

        # Alt+F4 sin ventana activa: nada
        marca = len(m.serial())
        m.tecla("alt-f4", 0.5)
        esperar(m, r"Alt\+F4 sem janela ativa", desde=marca)
        verificar("fechar janela" not in m.serial()[marca:], "Alt+F4 sin ventana activa no cierra nada")

        marca = len(m.serial())
        m.tecla("meta_l-d", 0.5)
        esperar(m, r"Windows\+D: 3 janelas restauradas", desde=marca)
        esperar_foco(m, t["id"])
        verificar(True, "Windows+D otra vez restaura las tres (el Terminal sigue arriba)")

        # ---- Alt+Tab restaura una minimizada
        m.tecla("meta_l-d", 0.5)
        esperar(m, r"Windows\+D: 3 janelas minimizadas", desde=marca)
        marca = len(m.serial())
        m.tecla("alt-tab", 0.4)
        esperar(m, rf"alt\+tab: ativada janela {t['id']}", desde=marca)
        esperar_foco(m, t["id"])
        verificar(True, "sin foco, Alt+Tab restaura la primera de la lista (el Terminal)")
        marca = len(m.serial())
        m.tecla("meta_l-d", 0.5)   # la Calculadora y el Editor siguen minimizados: esconde el Terminal
        esperar(m, r"Windows\+D: 1 janelas minimizadas", desde=marca)
        m.tecla("meta_l-d", 0.5)
        esperar(m, r"Windows\+D: 1 janelas restauradas", desde=marca)
        for ident in (e["id"], c["id"]):
            marca = len(m.serial())
            q.apertar("alt")
            q.toque("tab")
            while not re.search(rf"escolhida \d+ \(id {ident},", m.serial()[marca:]):
                q.toque("tab")
            q.soltar("alt")
            esperar(m, rf"alt\+tab: ativada janela {ident}", desde=marca)
            esperar_foco(m, ident)
        verificar(True, "Alt+Tab elige y restaura el Editor y la Calculadora minimizados")

        # ---- Ctrl+Alt+Supr
        marca = len(m.serial())
        m.tecla("ctrl-alt-delete", 0.5)
        esperar(m, r"Ctrl\+Alt\+Supr", desde=marca)
        m.esperar_serial('"Administrador de tareas"', 20)
        time.sleep(1)
        verificar(True, "Ctrl+Alt+Supr abre el Administrador de tareas")
        m.captura("H90-ctrl-alt-supr")
        tar = janela(m.serial(), "Administrador de tareas")
        esperar_foco(m, tar["id"])
        marca = len(m.serial())
        m.tecla("alt-tab", 0.4)
        esperar(m, r"alt\+tab: ativada", desde=marca)
        m.tecla("ctrl-alt-delete", 0.5)
        esperar(m, r"tareas ja aberto", desde=marca)
        esperar_foco(m, tar["id"])
        verificar(len(re.findall(r'criada por \d+: "Administrador de tareas"', m.serial())) == 1,
                  "otra vez, Ctrl+Alt+Supr sólo trae el que ya estaba abierto")

        # ---- Alt+F4 cierra la activa (el Administrador de tareas)
        marca = len(m.serial())
        m.tecla("alt-f4", 0.5)
        esperar(m, rf"Alt\+F4: fechar janela {tar['id']}", desde=marca)
        fim = time.time() + 10
        while time.time() < fim and foco(m) == tar["id"]:
            time.sleep(0.2)
        verificar(foco(m) != tar["id"], "Alt+F4 cierra el Administrador de tareas")

        # ---- Alt+F4 en el Editor con cambios: pregunta
        marca = len(m.serial())
        q.apertar("alt")
        q.toque("tab")
        while not re.search(rf"escolhida \d+ \(id {e['id']},", m.serial()[marca:]):
            q.toque("tab")
        q.soltar("alt")
        esperar_foco(m, e["id"])
        time.sleep(0.4)
        m.digitar("Hola", 0.05)
        m.esperar_serial("editor: titulo Editor - sin nombre *", 10)
        marca = len(m.serial())
        m.tecla("alt-f4", 0.5)
        esperar(m, rf"Alt\+F4: fechar janela {e['id']}", desde=marca)
        esperar(m, r'"Editor" 380x', desde=marca, tempo=10)
        time.sleep(0.5)
        m.captura("H90-alt-f4-editor")
        verificar("editor: fechado" not in m.serial()[marca:], "Alt+F4 en el Editor con cambios pregunta si guarda")
        d = janela(m.serial(), "Editor")
        m.clique(d["x"] + 2 + 380 - 16 - 190 + 45, d["y"] + 24 + d["a"] - 40 + 13)   # "No"
        m.esperar_serial("editor: fechado", 10)
        verificar(True, "'No' cierra el Editor sin guardar")

        # ---- Alt y AltGr solas no hacen nada
        marca = len(m.serial())
        m.tecla("alt", 0.3)
        m.tecla("alt_r", 0.3)
        verificar("alt+tab" not in m.serial()[marca:] and "menu Inicio" not in m.serial()[marca:],
                  "Alt y AltGr solas no disparan ningún atajo")
    return 0


if __name__ == "__main__":
    sys.exit(main())
