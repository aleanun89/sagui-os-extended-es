#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H119 (D147) - o ramdisk da imagem tem de estar inteiro.

Foi assim que o SaguiOS ficou com o kernel a arrancar sem um único programa:
o build/ramdisk.sgfs ficou com 32 bytes (a cabeçalho e nada mais) porque a
pasta de origem da cópia se perdeu a meio, e o mksgfs.py escrevia esse
ficheiro vazio sem reclamar. O kernel arrancava, o menu Inicio abria, pero
nenhuma aplicação abria (o carregador devolvia -2: ficheiro inexistente).

Critério:
  - o mksgfs.py falha (código de saída diferente de zero) quando a pasta de
    origem não existe ou quando está vazia: antes escrevia um SGFS sem
    entradas, sem dar erro;
  - o mkimagem.py recusa montar uma imagem com um ramdisk vazio ou corrompido;
  - o ramdisk que existe é um SGFS válido (entradas e soma de verificação) e
    traz os programas dentro (bin/terminal);
  - a imagem de disco lleva dentro, byte a byte, ese mismo ramdisk;
  - con la imagen así, el kernel arranca con el ramdisk lleno y el Terminal
    se abre de verdad (el pid es mayor que cero).
D379 (M1): la imagen de serie lleva el initramfs (build/initramfs.sgfs, entero y con /etc/raiz.conf y el
shell) y la raíz en SAGUI-RAIZ; el ramdisk completo (build/ramdisk.sgfs) se sigue haciendo y comprobando
(sale de él build/rootfs y lo usan saguios-completo.img y RAIZ_DISCO=0).
"""
import os
import re
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, BUILD, IMAGEM, SAIDA  # noqa: E402

MKSGFS = os.path.join(RAIZ, "tools", "mksgfs.py")
MKIMAGEM = os.path.join(RAIZ, "tools", "mkimagem.py")
RAMDISK = os.path.join(BUILD, "ramdisk.sgfs")
TAM_CAB, TAM_ENTRADA = 32, 72


def roda_mksgfs(origem, saida):
    return subprocess.run([sys.executable, MKSGFS, origem, saida], capture_output=True, text=True)


def le_cabecera(dados):
    """(entradas, tamanho total) de un SGFS, sin comprobar nada más."""
    t = struct.unpack("<4sHHIIIIII", dados[:TAM_CAB])
    return t[3], t[6]


def main():
    with tempfile.TemporaryDirectory() as tmp:
        # ---- el mksgfs no puede montar un ramdisk vacío en silencio ----
        vazia = os.path.join(tmp, "vazia")
        os.makedirs(vazia)
        r = roda_mksgfs(vazia, os.path.join(tmp, "vazio.sgfs"))
        verificar(r.returncode != 0, "mksgfs falla con una carpeta vacía")
        verificar(not os.path.exists(os.path.join(tmp, "vazio.sgfs")),
                  "mksgfs no deja un SGFS sin entradas cuando la carpeta está vacía")

        r = roda_mksgfs(os.path.join(tmp, "no-existe"), os.path.join(tmp, "nada.sgfs"))
        verificar(r.returncode != 0, "mksgfs falla con una carpeta que no existe")

        # una carpeta de verdad sí se monta, y el resultado valida
        boa = os.path.join(tmp, "boa")
        os.makedirs(os.path.join(boa, "bin"))
        with open(os.path.join(boa, "bin", "programa"), "wb") as f:
            f.write(b"\x7fELF\x01\x01\x01" + b"\0" * 56)
        saida = os.path.join(tmp, "boa.sgfs")
        r = roda_mksgfs(boa, saida)
        verificar(r.returncode == 0, "mksgfs monta una carpeta con ficheros")
        with open(saida, "rb") as f:
            dados = f.read()
        entradas, total = le_cabecera(dados)
        verificar(entradas == 2, f"el SGFS tiene las dos entradas (bin y bin/programa): {entradas}")
        verificar(total == len(dados), "el tamaño del SGFS cuadra con el fichero")

    # ---- el mkimagem no monta una imagen con el ramdisk roto ----
    if os.path.exists(RAMDISK) and os.path.exists(IMAGEM):
        with tempfile.TemporaryDirectory() as tmp:
            vazio = os.path.join(tmp, "vazio.sgfs")
            with open(vazio, "wb") as f:
                f.write(b"SGFS" + struct.pack("<HHIIIIII", 1, TAM_ENTRADA, 0, 32, 32, 32, 0, 0))
            r = subprocess.run([sys.executable, MKIMAGEM,
                                "--mbr", os.path.join(BUILD, "boot", "mbr.bin"),
                                "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                                "--kernel", os.path.join(BUILD, "kernel.bin"),
                                "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                                "--ramdisk", vazio,
                                "--saida", os.path.join(tmp, "mala.img")],
                               capture_output=True, text=True)
            verificar(r.returncode != 0, "mkimagem recusa un ramdisk sin entradas")
            verificar("entradas" in (r.stdout + r.stderr), "mkimagem dice por qué lo rechaza")

    # ---- el ramdisk de verdad está entero ----
    if not os.path.exists(RAMDISK):
        verificar(False, "no hay build/ramdisk.sgfs: hay que compilar antes (make)")
    with open(RAMDISK, "rb") as f:
        rd = f.read()
    magic, _v, _te, entradas, _to, _do, total, _s, _r = struct.unpack("<4sHHIIIIII", rd[:TAM_CAB])
    verificar(magic == b"SGFS", "el ramdisk es un SGFS")
    verificar(entradas > 50, f"el ramdisk tiene entradas ({entradas})")
    verificar(total == len(rd), "el tamaño del ramdisk cuadra")
    verificar(sum(rd[TAM_CAB:]) & 0xFFFFFFFF == _s, "la suma de verificación del ramdisk cuadra")
    caminhos = []
    for i in range(entradas):
        off = TAM_CAB + i * TAM_ENTRADA
        caminhos.append(rd[off:off + 56].split(b"\0")[0].decode("latin-1"))
    for esperado in ("bin/terminal", "bin/arquivos", "bin/sagsh", "lib/python"):   # D324: correo vive en /apps
        verificar(esperado in caminhos, f"el ramdisk trae {esperado}")

    # ---- D379 (M1): el initramfs de la imagen, también entero ----
    ini = os.path.join(BUILD, "initramfs.sgfs")
    if os.path.exists(ini):
        with open(ini, "rb") as f:
            rd = f.read()
        magic, _v, _te, entradas, _to, _do, total, _s, _r = struct.unpack("<4sHHIIIIII", rd[:TAM_CAB])
        verificar(magic == b"SGFS" and total == len(rd) and sum(rd[TAM_CAB:]) & 0xFFFFFFFF == _s,
                  "el initramfs es un SGFS entero (tamaño y suma)")
        caminhos = [rd[TAM_CAB + i * TAM_ENTRADA:TAM_CAB + i * TAM_ENTRADA + 56].split(b"\0")[0].decode("latin-1")
                    for i in range(entradas)]
        for esperado in ("bin/sagsh", "etc/raiz.conf", "etc/rescate.txt"):
            verificar(esperado in caminhos, f"el initramfs trae {esperado}")

    # ---- y la imagen lleva ese ramdisk dentro ----
    with open(IMAGEM, "rb") as f:
        img = f.read()
    pos = img.find(rd[:64])
    verificar(pos >= 0, "la imagen lleva el ramdisk dentro")
    if pos >= 0:
        verificar(img[pos:pos + len(rd)] == rd, "el ramdisk de la imagen es byte a byte el que se montó")

    # ---- y con esa imagen las aplicaciones abren ----
    with Maquina("h119", "ide", imagem=IMAGEM) as m:
        m.esperar_serial("area de trabalho no ar", 120)
        g = re.findall(r"Ramdisk SGFS: (\d+) bytes em 0x[0-9a-f]+, (\d+) entradas", m.serial())
        verificar(bool(g), "el kernel anuncia el ramdisk")
        if g:
            verificar(int(g[-1][1]) == entradas, f"el kernel ve las mismas entradas ({g[-1][1]})")
        # el terminal que abre el kernel al arrancar
        m.esperar_serial("[SAGUI] terminal:", 30)   # sale poco después de «area de trabalho no ar»: con KVM se leía antes
        t = re.findall(r"\[SAGUI\] terminal: (-?\d+)", m.serial())
        verificar(bool(t) and int(t[-1]) > 0, f"el kernel abre el terminal al arrancar (pid {t[-1] if t else '?'})")

        # y el que abre el usuario desde el menú Inicio
        m.clique(40, 12)
        m.esperar_serial("[GUI] menu aberto em ", 15)
        p = re.findall(r"\[GUI\] menu aberto em \((\d+),(\d+)\)", m.serial())[-1]
        m.clique(66, int(p[1]) + 20)
        m.esperar_serial("[GUI] inicio -> /bin/terminal (pid ", 20)
        pid = re.findall(r"\[GUI\] inicio -> /bin/terminal \(pid (-?\d+)\)", m.serial())[-1]
        verificar(int(pid) > 0, f"el Terminal se abre desde el menú Inicio (pid {pid})")
        verificar("No se pudo abrir el programa" not in m.serial(), "no sale el aviso de programa no abierto")
        m.captura("H119-terminal-abierto")

    return 0


if __name__ == "__main__":
    sys.exit(main())
