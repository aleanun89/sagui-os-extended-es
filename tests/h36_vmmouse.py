#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H36 - mouse com posição absoluta numa máquina virtual (vmmouse, D035).

Critério: com a porta dos fundos da VMware ligada no QEMU (vmport=on), o
kernel detecta o vmmouse e pede posições absolutas; um clique enviado pelo
QMP numa posição absoluta da janela chega ao SaguiOS na mesma posição da
tela, sem desvio acumulado; sem vmport, o mouse continua PS/2 relativo.
"""
import json
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA  # noqa: E402

ABS_MAX = 0x7FFF   # faixa dos eixos absolutos no QMP (input-send-event)


class Qmp:
    def __init__(self, caminho):
        for _ in range(100):
            if os.path.exists(caminho):
                break
            time.sleep(0.05)
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(caminho)
        self.f = self.s.makefile("rw")
        self.f.readline()   # saudação
        self.comando("qmp_capabilities")

    def comando(self, nome, **args):
        self.f.write(json.dumps({"execute": nome, "arguments": args}) + "\n")
        self.f.flush()
        while True:
            r = json.loads(self.f.readline())
            if "return" in r or "error" in r:
                return r

    def clicar_abs(self, fx, fy):
        """Move para a fração (fx, fy) da janela e clica com o botão esquerdo."""
        pos = [{"type": "abs", "data": {"axis": "x", "value": int(fx * ABS_MAX)}},
               {"type": "abs", "data": {"axis": "y", "value": int(fy * ABS_MAX)}}]
        self.comando("input-send-event", events=pos)
        time.sleep(0.3)
        for baixo in (True, False):
            self.comando("input-send-event",
                         events=[{"type": "btn", "data": {"down": baixo, "button": "left"}}])
            time.sleep(0.3)


def ultimo_clique(serial):
    linhas = [l for l in serial.splitlines() if "[MOUSE] botao esquerdo apertado em (" in l]
    if not linhas:
        return None
    x, y = linhas[-1].split("(")[1].split(")")[0].split(",")
    return int(x), int(y)


def main():
    qmp = os.path.join(SAIDA, "h36.qmp")
    if os.path.exists(qmp):
        os.remove(qmp)
    with Maquina("h36", "ide", vmmouse=True, extras=["-qmp", f"unix:{qmp},server,nowait"]) as m:
        m.esperar_serial("[SAGUI] boot ok", 120)
        verificar("[MOUSE] vmmouse: posicao absoluta" in m.serial(), "vmmouse detectado e em modo absoluto")
        q = Qmp(qmp)
        for fx, fy in ((0.25, 0.5), (0.75, 0.25), (0.25, 0.5)):
            q.clicar_abs(fx, fy)
            pos = ultimo_clique(m.serial())
            esperado = (int(fx * 1024), int(fy * 768))
            verificar(pos is not None and abs(pos[0] - esperado[0]) <= 1 and abs(pos[1] - esperado[1]) <= 1,
                      f"clique absoluto em {esperado} chega em {pos}")

    with Maquina("h36", "ide") as m:
        m.esperar_serial("[SAGUI] boot ok", 120)
        s = m.serial()
        verificar("mouse PS/2 ligado" in s and "vmmouse" not in s, "sem vmport: mouse PS/2 relativo, como no G470")
    return 0


if __name__ == "__main__":
    sys.exit(main())
