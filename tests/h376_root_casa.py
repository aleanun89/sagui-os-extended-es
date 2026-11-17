#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H376 (D371, fase 2) - /root, HOME y lo que ven los programas elevados.

A. Consola, sesión sagui, /home en ext2 y SAGUI-CONF, sin instantánea:
   - /root existe (0700, de root) y se guarda en SAGUI-CONF (/sistema/conf/raiz): lo que root escribe en
     /root sigue tras reiniciar; sagui no lo ve.
   - «su» da HOME=/root; /root no puede llenar SAGUI-CONF (reserva de 4 MB).
B. Escritorio en vivo (sin SAGUI-CONF), ana como única cuenta (sagui con «!»):
   - /root está en la RAM;
   - lo elevado con «admin» ve el árbol real (/home/ana) pero escribe su configuración (/home/.config) en la
     de ana (P2 b); «su» suelta la carpeta: /home/.config es el de verdad y HOME=/root."""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

PW = "raiz2468"
TRAB = os.path.join(SAIDA, "h376")
B = os.path.join(RAIZ, "build")


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def orden(m, texto, tempo=90):
    orden.n += 1
    marca = f"FIN-{orden.n:03d}"
    antes = len(m.serial())
    m.digitar(f"{texto}; echo FIN-'{orden.n:03d}'", 0.03)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}\b", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def interativo(m, texto, pasos, tempo=90):
    desde = len(m.serial())
    m.digitar(texto, 0.04)
    m.tecla("ret")
    for pregunta, resposta in pasos:
        esperar(m, re.escape(pregunta), tempo, desde)
        desde = len(m.serial())
        m.digitar(resposta, 0.04)
        m.tecla("ret")
    time.sleep(3)
    return m.serial()[desde:]


def reiniciar(m):
    m.digitar("reiniciar", 0.04)
    m.tecla("ret")
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, "reinicia")


def parte_a():
    img = imagem_de_teste("", ["--console", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2"])
    with Maquina("h376a", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"supervisor en marcha", 120)
        esperar(m, r"sagui:\S*\$ ", 60)
        time.sleep(2)
        verificar("[CONF] /root en SAGUI-CONF (/sistema/conf/raiz): si" in m.serial(), "/root en SAGUI-CONF")
        out = interativo(m, "admin id", [("Contraseña nueva:", PW), ("Repítela:", PW)])
        verificar("uid=0(root)" in out, "root con contraseña")
        out = orden(m, f"echo {PW} | admin 'echo DATO-ROOT > /root/nota; ls -la --color=never /root; ls -l --color=never /'")
        verificar(re.search(r"^drwx------ root +root .* root/?$", out, re.M), f"/root 0700 de root ({out[-600:]!r})")
        verificar(re.search(r"nota", out), "root escribe en /root")
        out = orden(m, "cat /root/nota; ls /root")
        verificar("DATO-ROOT" not in out.split("cat /root/nota")[-1], "sagui no lee /root")
        out = orden(m, f"echo {PW} | su -c 'echo CASA=$HOME'")
        verificar("CASA=/root" in out, f"su: HOME=/root ({out[-300:]!r})")
        # /root no puede llenar SAGUI-CONF: siempre quedan 4 MB para las contraseñas y las claves
        pos = len(m.serial())
        orden(m, f"echo {PW} | admin 'yes | head -c 14000000 > /root/grande; rm /root/grande'", 300)
        verificar("[CONF] /root: sin sitio" in m.serial()[pos:], "/root se para antes de llenar SAGUI-CONF")
        reiniciar(m)
    with Maquina("h376b", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"supervisor en marcha", 120)
        esperar(m, r"sagui:\S*\$ ", 60)
        time.sleep(2)
        out = orden(m, f"echo {PW} | admin cat /root/nota")
        verificar("DATO-ROOT" in out.split("admin cat")[-1], "lo de /root sigue tras reiniciar")


def imagen_vivo():
    raiz = os.path.join(TRAB, "rootfs-ana")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(B, "rootfs"), raiz)
    with open(os.path.join(raiz, "etc", "usuarios"), "a") as f:
        f.write("ana:1001:1001:/home/ana:/bin/sagsh:Ana\n")
    with open(os.path.join(raiz, "etc", "sombra"), "w") as f:
        f.write("sagui:!\n")
    sgfs = os.path.join(TRAB, "ana.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "ana.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(B, "boot", "mbr.bin"), "--estagio2", os.path.join(B, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--sesion-sagui", "--sin-conf"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def parte_b():
    os.makedirs(TRAB, exist_ok=True)
    img = imagen_vivo()
    with Maquina("h376c", "ide", imagem=img) as m:
        esperar(m, r"\[SESION\] una sola cuenta sin contrasena \(ana\): entrada directa", 120)
        verificar("[CONF] /root en la RAM (sin SAGUI-CONF)" in m.serial(), "en vivo, /root en la RAM")
        esperar(m, r"\[SAGUI\] terminal: \d+", 60)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(4)
        out = orden(m, "id; mkdir -p /home/.config")
        verificar("uid=1001(ana)" in out, "el Terminal es ana")
        out = orden(m, "admin 'echo DE-ADMIN > /home/.config/prueba-admin; ls /home'")
        verificar(re.search(r"^ana/?$", out, re.M), f"admin ve el árbol real ({out[-300:]!r})")
        out = orden(m, "cat /home/.config/prueba-admin")
        verificar("DE-ADMIN" in out.split("cat /home")[-1], "admin guarda su configuración en la de ana (P2 b)")
        out = orden(m, "su -c 'echo DE-SU > /home/.config/prueba-su; echo CASA=$HOME'")
        verificar("CASA=/root" in out, f"su: HOME=/root ({out[-300:]!r})")
        out = orden(m, "cat /home/.config/prueba-su")
        verificar("DE-SU" not in out.split("cat /home")[-1], "su suelta la carpeta de ana: no escribe en la suya")
    print("H376 OK")


def main():
    parte_a()
    parte_b()


if __name__ == "__main__":
    main()
