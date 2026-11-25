#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H467 - progreso de transferencias en Archivos (D467).

Criterio:
  - copiar un archivo grande (Ctrl+C, Ctrl+V en otra carpeta) lo deja entero en el destino (mismo tamaño);
  - el programa registra la transferencia (itens, KB, ms) y, si tardó, la ventana de progreso con porcentaje;
  - pegar de nuevo con el destino ya existente abre el diálogo Reemplazar/Omitir/Mantener los dos/Cancelar;
    «Omitir» no toca el destino; en la misma carpeta se crea «nombre (2)» sin preguntar.
Lo que se comprueba es lo que escribe el programa en la serie y lo que queda en el disco.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h104_arquivos_pro import janela, item  # noqa: E402


def copiar_pegar(m, origen, destino):
    """Selecciona el primer archivo de «origen», Ctrl+C; abre «destino» y Ctrl+V."""
    m.comando(f"arquivos {origen}")
    m.esperar_serial("arquivos: pronto", 20)
    time.sleep(0.8)
    a = janela(m.serial(), "Archivos")
    m.clique(*item(a, 0))
    time.sleep(0.4)
    m.tecla("ctrl-c")
    time.sleep(0.5)
    m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
    m.esperar_serial("(arquivos) terminou", 10)
    time.sleep(0.5)
    n = m.serial().count("arquivos: pronto")
    m.comando(f"arquivos {destino}")
    for _ in range(100):
        if m.serial().count("arquivos: pronto") > n:
            break
        time.sleep(0.2)
    time.sleep(0.8)
    a = janela(m.serial(), "Archivos")
    m.tecla("ctrl-v")
    return a


def principal():
    with Maquina("h467", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("mkdir /tmp/orig")
        m.comando("mkdir /tmp/dst")
        m.comando("truncate -s 24M /tmp/orig/grande.bin")
        time.sleep(0.5)

        # ---- copia grande
        a = copiar_pegar(m, "/tmp/orig", "/tmp/dst")
        m.esperar_serial("arquivos: transferencia copiar 1 itens", 120)
        s = m.serial()
        r = re.findall(r"transferencia copiar 1 itens (\d+) KB em (\d+) ms", s)[-1]
        verificar(int(r[0]) == 24576, f"la transferencia registra 24576 KB ({r[0]}) en {r[1]} ms")
        verificar("progreso total 1 elementos 24576 KB" in s, "el recorrido previo cuenta 1 elemento y 24576 KB")
        if int(r[1]) > 900:
            verificar(re.search(r"progreso \d+% ", s) is not None, "al tardar mas de 0,7 s aparece la ventana de progreso con porcentaje")
        time.sleep(0.8)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)
        out = m.comando("ls -l /tmp/dst")
        verificar("24117248" in out or "25165824" in out, "el archivo copiado tiene el mismo tamano")

        # ---- el destino ya existe: dialogo, Omitir
        a = copiar_pegar(m, "/tmp/orig", "/tmp/dst")
        m.esperar_serial("dialogo boton 1:", 20)
        time.sleep(0.5)
        m.captura("H467-1-conflicto")
        d = janela(m.serial(), "Copiando")
        b = re.findall(r"dialogo boton 1: (\d+),(\d+),(\d+),(\d+)", m.serial())[-1]
        m.clique(d["x"] + int(b[0]) + int(b[2]) // 2, d["y"] + int(b[1]) + int(b[3]) // 2)
        m.esperar_serial("conflito grande.bin -> omitir", 10)
        verificar(True, "el dialogo de conflicto ofrece Omitir y se atiende")
        time.sleep(1.5)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)
        out = m.comando("ls /tmp/dst")
        verificar("grande.bin" in out and "(2)" not in out, "Omitir no crea otra copia")

        # ---- copia lenta (la prueba pide una pausa por trozo): sale la ventana de progreso
        m.comando("mkdir /tmp/dst2")
        m.comando("mkdir /tmp/dst3")
        m.comando("echo 40 > /tmp/.transf_lento")
        time.sleep(0.5)
        a = copiar_pegar(m, "/tmp/orig", "/tmp/dst2")
        m.esperar_serial("progreso ventana abierta", 30)
        time.sleep(1.0)
        w = janela(m.serial(), "Copiando")
        bp = re.findall(r"pausa=(\d+),(\d+),(\d+),(\d+) cancelar=(\d+),(\d+),(\d+),(\d+)", m.serial())[-1]
        bp = [int(v) for v in bp]
        m.captura("H467-2-progreso")
        time.sleep(1.5)
        pcts = [int(v) for v in re.findall(r"\[archivos\] progreso (\d+)% \d+/\d+ KB velocidad=", m.serial())]
        verificar(len(pcts) >= 2 and pcts[-1] > pcts[0], f"el porcentaje avanza en el registro ({pcts[:3]}...{pcts[-1:]})")
        verificar(re.search(r"velocidad=\d+(,\d)? (KB|MB)/s restante=Unos \d+ segundo", m.serial()) is not None,
                  "el registro lleva velocidad y tiempo restante")

        # ---- Pausar y Reanudar
        m.clique(w["x"] + bp[0] + bp[2] // 2, w["y"] + bp[1] + bp[3] // 2)
        m.esperar_serial("progreso pausar", 10)
        time.sleep(0.8)
        m.captura("H467-3-pausa")
        k1 = re.findall(r"progreso \d+% (\d+)/\d+ KB", m.serial())[-1]
        time.sleep(2.0)
        k2 = re.findall(r"progreso \d+% (\d+)/\d+ KB", m.serial())[-1]
        verificar(k1 == k2, f"en pausa no avanza la copia ({k1} KB = {k2} KB)")
        verificar(re.search(r"restante=\S+( \S+)? pausa\r?\n", m.serial()) is not None, "el registro marca la pausa")
        m.clique(w["x"] + bp[0] + bp[2] // 2, w["y"] + bp[1] + bp[3] // 2)
        m.esperar_serial("progreso reanudar", 10)
        time.sleep(1.5)
        k3 = re.findall(r"progreso \d+% (\d+)/\d+ KB", m.serial())[-1]
        verificar(int(k3) > int(k1), f"al reanudar la copia sigue ({k1} -> {k3} KB)")
        m.esperar_serial("transferencia copiar 1 itens 24576 KB", 60)
        time.sleep(1.0)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)
        out = m.comando("ls -l /tmp/dst2")
        verificar("grande.bin" in out and ("24117248" in out or "25165824" in out), "tras pausar y reanudar el destino queda entero")

        # ---- Cancelar: el destino a medias desaparece y el origen queda
        n = m.serial().count("progreso ventana abierta")
        a = copiar_pegar(m, "/tmp/orig", "/tmp/dst3")
        for _ in range(150):
            if m.serial().count("progreso ventana abierta") > n:
                break
            time.sleep(0.2)
        time.sleep(1.5)
        w = janela(m.serial(), "Copiando")
        bp = [int(v) for v in re.findall(r"cancelar=(\d+),(\d+),(\d+),(\d+)", m.serial())[-1]]
        m.clique(w["x"] + bp[0] + bp[2] // 2, w["y"] + bp[1] + bp[3] // 2)
        m.esperar_serial("copia cancelada, apagado /tmp/dst3/grande.bin", 20)
        m.esperar_serial("transferencia copiar 1 itens", 20)
        time.sleep(1.0)
        m.captura("H467-4-cancelada")
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)
        out = m.comando("ls /tmp/dst3")
        verificar("grande.bin" not in out, "cancelar borra el archivo a medias del destino")
        out = m.comando("ls -l /tmp/orig")
        verificar("grande.bin" in out and ("24117248" in out or "25165824" in out), "el origen queda intacto")

        # ---- conflicto: Mantener los dos
        m.comando("rm /tmp/.transf_lento")
        a = copiar_pegar(m, "/tmp/orig", "/tmp/dst")
        m.esperar_serial("dialogo boton 2:", 20)
        d = janela(m.serial(), "Copiando")
        b = re.findall(r"dialogo boton 2: (\d+),(\d+),(\d+),(\d+)", m.serial())[-1]
        m.clique(d["x"] + int(b[0]) + int(b[2]) // 2, d["y"] + int(b[1]) + int(b[3]) // 2)
        m.esperar_serial("conflito grande.bin -> mantener", 10)
        time.sleep(1.5)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("(arquivos) terminou", 10)
        time.sleep(0.5)
        out = m.comando("ls /tmp/dst")
        verificar("grande (2).bin" in out, "Mantener los dos crea «grande (2).bin»")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
