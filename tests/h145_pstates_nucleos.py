#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H145 (D175) - P-states por CPU con SMP, con el driver de frecuencia simulado.

QEMU no tiene los MSR de P-state y el driver se desactiva en máquinas
virtuales; con mkimagem.py --frecuencia-simulada cada CPU tiene un «registro»
en memoria (P0 2100, P1 1600, P2 1050 MHz; la BIOS deja la CPU 1 en P1 y las
demás en P2) que solo escribe la propia CPU, en su tic. Criterio, con --smp y
cuatro CPU:
  - el driver ve las cuatro CPU y cada secundario lee el estado en que lo dejó
    su BIOS y sube a P0 (rendimiento) él mismo;
  - a demanda por núcleo: con cargas distintas, las ociosas bajan por
    escalones hasta P2 y las cargadas siguen en P0;
  - --paquete: todas el estado de la más cargada; --por-nucleo vuelve;
  - 80 °C: todas al más lento («limitada»); por debajo de 72 °C vuelven;
  - --desactivar: cada CPU vuelve a SU estado de la BIOS (la 1 a P1, las
    demás a P2); --activar sube otra vez;
  - «tareas -t» muestra el estado y la frecuencia de cada CPU;
  - un #GP al escribir en la CPU 2 (MSR inexistente: real con KVM, simulado
    sin KVM) desactiva el driver en todas y el sistema sigue;
  - con cuatro CPU sin --smp: una sola, como antes (sin «Por CPU»).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402

N = 4


def kvm():
    return os.environ.get("SAGUI_KVM") == "1"


def cpus(m):
    """Ejecuta «frecuencia» y devuelve {cpu: {campo: valor}} de sus líneas de registro."""
    out = m.comando("frecuencia", 30)
    r = {}
    for g in re.finditer(r"frecuencia: cpu (\d+) atual (-?\d+) pedido (-?\d+) alvo (-?\d+) bios (-?\d+) uso (\d+) "
                         r"escritas (\d+) pendente (\d+) coord (\d+)", out):
        r[int(g.group(1))] = dict(atual=int(g.group(2)), pedido=int(g.group(3)), alvo=int(g.group(4)),
                                  bios=int(g.group(5)), uso=int(g.group(6)), escritas=int(g.group(7)),
                                  pendente=int(g.group(8)), coord=int(g.group(9)))
    return r, out


def esperar_estados(m, esperado, tempo, rotulo):
    """Repite «frecuencia» hasta que el estado de cada CPU sea el esperado."""
    fim = time.time() + tempo
    ultimo = None
    while time.time() < fim:
        c, out = cpus(m)
        ultimo = {k: v["atual"] for k, v in c.items()}
        if ultimo == esperado and not any(v["pendente"] for v in c.values()):
            verificar(True, rotulo)
            return c, out
        time.sleep(0.5)
    verificar(False, f"{rotulo}: {ultimo} != {esperado}")


def com_smp():
    img = imagem_de_teste("", ["--vesa", "--console", "--frecuencia-simulada", "--smp"])
    with Maquina("h145", "ide", imagem=img, nucleos=N) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[SAGUI\] nucleos=4", s), "arrancan las cuatro CPU")
        verificar("[FREQ] 4 CPU: cada una escribe su propio registro en su tic; coordinacion por nucleo" in s,
                  "el driver ve las cuatro CPU")
        time.sleep(1)
        s = m.serial()
        verificar("[FREQ] CPU 1: la BIOS la dejo en P1" in s and "[FREQ] CPU 2: la BIOS la dejo en P2" in s and
                  "[FREQ] CPU 3: la BIOS la dejo en P2" in s, "cada secundario lee su propio estado de la BIOS")
        c, out = esperar_estados(m, {0: 0, 1: 0, 2: 0, 3: 0}, 20, "rendimiento: las cuatro CPU en P0")
        verificar(all(c[k]["escritas"] >= 1 for k in c) and c[1]["bios"] == 1 and c[2]["bios"] == 2,
                  "cada CPU escribió su registro y recuerda su estado de la BIOS")
        verificar("Por CPU:" in out and re.search(r"CPU 3 \(núcleo 3\): P0 2100 MHz", out),
                  "frecuencia muestra el estado de cada CPU")

        # A demanda, cada núcleo según su carga.
        for k, u in ((0, 90), (1, 5), (2, 5), (3, 90)):
            m.comando(f"frecuencia --simular-uso-cpu {k} {u}", 20)
        m.comando("frecuencia demanda", 20)
        esperar_estados(m, {0: 0, 1: 2, 2: 2, 3: 0}, 30, "a demanda: las CPU ociosas bajan a P2, las cargadas siguen en P0")
        s = m.serial()
        verificar("[FREQ] CPU 1: P0 -> P1 (a demanda" in s and "[FREQ] CPU 1: P1 -> P2 (a demanda" in s,
                  "a demanda: la CPU 1 baja por escalones")
        out = m.comando("frecuencia -c -n 1", 20)
        verificar(re.search(r"CPU0 P0 CPU1 P2 CPU2 P2 CPU3 P0", out), "frecuencia -c: el estado de cada CPU")

        # Paquete: todas el de la más cargada.
        out = m.comando("frecuencia --paquete", 20)
        verificar("Coordinación: por paquete" in out, "--paquete aceptado")
        c, _ = esperar_estados(m, {0: 0, 1: 0, 2: 0, 3: 0}, 20, "paquete: todas en P0 (la más cargada manda)")
        verificar(all(v["coord"] == 1 for v in c.values()), "INFO_FREQUENCIA dice «paquete»")
        m.comando("frecuencia --por-nucleo", 20)
        esperar_estados(m, {0: 0, 1: 2, 2: 2, 3: 0}, 30, "--por-nucleo: vuelven a bajar las ociosas")

        # Temperatura: manda sobre todo.
        m.comando("frecuencia --simular-temp 80", 20)
        _, out = esperar_estados(m, {0: 2, 1: 2, 2: 2, 3: 2}, 20, "80 °C: todas las CPU al más lento")
        verificar("LIMITADA POR TEMPERATURA" in out and "en todas las CPU" in m.serial(), "80 °C: limitada, lo dice el log")
        m.comando("frecuencia --simular-temp 75", 20)
        esperar_estados(m, {0: 2, 1: 2, 2: 2, 3: 2}, 10, "75 °C: sigue limitada (histéresis)")
        m.comando("frecuencia --simular-temp 50", 20)
        esperar_estados(m, {0: 0, 1: 2, 2: 2, 3: 0}, 20, "50 °C: las cargadas vuelven a P0")

        # Desactivar: cada una a SU estado de la BIOS.
        m.comando("frecuencia rendimiento", 20)
        esperar_estados(m, {0: 0, 1: 0, 2: 0, 3: 0}, 20, "rendimiento: todas en P0")
        m.comando("frecuencia --desactivar", 20)
        _, out = esperar_estados(m, {0: 2, 1: 1, 2: 2, 3: 2}, 20,
                                 "--desactivar: cada CPU vuelve a su estado de la BIOS (la 1 a P1)")
        verificar("(cada CPU en el suyo)" in m.serial() and "desactivado, política" in out, "--desactivar: lo dice el log")
        m.comando("frecuencia --simular-temp 85", 20)
        time.sleep(1)
        esperar_estados(m, {0: 2, 1: 1, 2: 2, 3: 2}, 5, "desactivado: ninguna CPU escribe nada")
        m.comando("frecuencia --simular-temp 50", 20)
        m.comando("frecuencia --activar", 20)
        esperar_estados(m, {0: 0, 1: 0, 2: 0, 3: 0}, 20, "--activar: todas en P0")

        # El Administrador de tareas en modo texto.
        antes = len(m.serial())
        m.comando("tareas -t", 30)
        fim = time.time() + 30
        while time.time() < fim and not re.search(r"\(tareas\) terminou", m.serial()[antes:]):
            time.sleep(0.3)
        out = m.serial()[antes:].replace("\r", "").replace("\x1b[K", "")
        for k in range(N):
            verificar(re.search(rf"CPU {k}: \d+,\d %  \w+  \(n\S+cleo \d+, hilo \d+\)  P0 2100 MHz", out), f"tareas -t: CPU {k} en P0 2100 MHz")

        # #GP en la CPU 2: el driver se desactiva en todas.
        m.comando("frecuencia --prueba-gp 2", 20)
        fim = time.time() + 10
        while time.time() < fim and "[FREQ] #GP al escribir" not in m.serial():
            time.sleep(0.3)
        s = m.serial()
        if kvm():
            verificar("[FREQ] prueba: escritura en 0xbadf00d -> #GP absorbido (CPU 2)" in s,
                      "KVM: el wrmsr de la CPU 2 da #GP y se absorbe")
        else:
            verificar("[FREQ] prueba: escritura en 0xbadf00d -> " in s and "(CPU 2)" in s,
                      "sin KVM: la CPU 2 hace la escritura (fallo simulado)")
        c, out = cpus(m)
        verificar("desactivado por un fallo" in out, "tras el #GP de la CPU 2 el driver queda desactivado en todas")
        escritas = {k: v["escritas"] for k, v in c.items()}
        m.comando("frecuencia ahorro", 20)
        time.sleep(1)
        c, _ = cpus(m)
        verificar({k: v["escritas"] for k, v in c.items()} == escritas, "después del #GP ninguna CPU vuelve a escribir")
        out = m.comando("echo sigo vivo", 20)
        verificar("sigo vivo" in out and "EXCECAO" not in m.serial(), "el sistema sigue funcionando")


def sem_smp():
    img = imagem_de_teste("", ["--vesa", "--console", "--frecuencia-simulada"])
    with Maquina("h145u", "ide", imagem=img, nucleos=N) as m:
        m.esperar_serial("sagui:/$", 120)
        c, out = cpus(m)
        verificar(not c and "Por CPU" not in out and "P0 2100 MHz (actual)" in out,
                  "cuatro CPU sin --smp: una sola, como antes")
        verificar("[FREQ] 4 CPU" not in m.serial() and "EXCECAO" not in m.serial(), "sin --smp: nada de varias CPU")


def main():
    com_smp()
    sem_smp()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
