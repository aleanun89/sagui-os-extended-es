#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H371 (D371, auditoría de usuarios de la v8.5.3) - tres fallos de seguridad entre cuentas, corregidos:

A. Consola, sesión root, /home en ext2 (una cuenta «ana» con /home/ana 0700 creada por root):
   1. sagui no puede renombrar ni borrar /home/ana (/home es de sagui y antes bastaba escribir en /home
      para quitarla y dejar en su sitio una carpeta suya); sus propios archivos de /home, sí.
   2. ana no puede ponerle la primera contraseña a sagui (que de serie no tiene) con «su sagui» ni con
      «passwd sagui» (antes pasaba a ser sagui y le quitaba la cuenta); su propia primera contraseña, sí.
   3. con SAGUI-CONF, el vncd (root) ignora de /home/.config/vncd.conf todo menos fps_max (antes sagui
      podía poner «seguridad=ninguna» y «escuchar=red-local»).
B. Escritorio, /home en la RAM, sin SAGUI-CONF, ana como única cuenta (sagui con «!» en la sombra):
   1. con una /home/ana que no es de ana (la dejó sagui: en el ramdisk es de sagui) no se entra:
      aparece la pantalla de inicio de sesión;
   2. sin ella, el kernel la crea de ana (0700) y se entra directo como ana."""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

TRAB = os.path.join(SAIDA, "h371")
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
    """Una orden en el shell (root o no) con una marca propia al final."""
    orden.n += 1
    marca = f"FIN-{orden.n:03d}"
    antes = len(m.serial())
    # la marca escrita va entre comillas: el eco de lo tecleado no la contiene tal cual
    m.digitar(f"{texto}; echo FIN-'{orden.n:03d}'", 0.03)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}\b", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def parte_a():
    img = imagem_de_teste("", ["--console", "--sesion-root", "--home-mb", "32", "--home-fs", "ext2"])
    with Maquina("h371a", "ide", imagem=img) as m:
        esperar(m, r"supervisor en marcha", 120)
        esperar(m, r"(root|sagui):\S*\$ ", 60)
        verificar("uid=0" in orden(m, "id"), "la sesión de la prueba es root")
        out = orden(m, "echo ana:1001:1001:/home/ana:/bin/sagsh:Ana >> /etc/usuarios; mkdir /home/ana; "
                       "chown ana:ana /home/ana; chmod 700 /home/ana; ls -l --color=never /home")
        verificar(re.search(r"^drwx------ ana +ana .* ana/?$", out, re.M), f"/home/ana de ana, 0700 ({out[-400:]!r})")
        # A1: sagui no quita la carpeta de otra cuenta
        orden(m, "su sagui -c 'mv /home/ana /home/robada'")
        orden(m, "su sagui -c 'rmdir /home/ana'")
        out = orden(m, "ls -l --color=never /home")
        verificar(re.search(r"^drwx------ ana +ana .* ana/?$", out, re.M), "sagui no renombra ni borra /home/ana")
        # (mv, al negársele el renombrado, prueba a copiar y deja una /home/robada vacía y de sagui: no es la de ana)
        verificar(not re.search(r" ana +ana .* robada/?$", out, re.M), "/home/robada no es la carpeta de ana")
        verificar("borrar o renombrar (carpeta de otra cuenta)" in m.serial(), "el kernel anota la negativa")
        out = orden(m, "su sagui -c 'echo x > /home/propio.txt; mv /home/propio.txt /home/propio2.txt; "
                       "rm /home/propio2.txt && echo PROPIO-OK'")
        verificar("PROPIO-OK" in out, "sagui renombra y borra lo suyo en /home")
        # A2: ana no le pone la primera contraseña a sagui
        orden(m, "printf 'robo1234\\nrobo1234\\nrobo1234\\nrobo1234\\nrobo1234\\nrobo1234\\n' > /tmp/p; chmod 644 /tmp/p")
        out = orden(m, "su ana -c 'su sagui -c id < /tmp/p'")
        verificar("uid=1000" not in out.split("su ana -c")[-1], f"ana no pasa a ser sagui con su ({out[-400:]!r})")
        orden(m, "su ana -c 'passwd sagui < /tmp/p'")
        out = orden(m, "cat /etc/sombra")
        verificar(not re.search(r"^sagui:pbkdf2", out, re.M), "ana no le pone contraseña a sagui (passwd)")
        verificar("solo ella misma o root se la ponen" in m.serial(), "el gestor anota por qué")
        out = orden(m, "su ana -c 'passwd < /tmp/p'")
        out = orden(m, "cat /etc/sombra")
        verificar(re.search(r"^ana:pbkdf2", out, re.M), "ana sí elige su propia primera contraseña")
        verificar("robo1234" not in m.serial().replace("robo1234\\n", ""), "ninguna contraseña en la serie")
        # A3: el vncd (root) no acepta de la carpeta de sagui que le quiten la contraseña ni que abra la red
        orden(m, "su sagui -c 'mkdir -p /home/.config; echo seguridad=ninguna > /home/.config/vncd.conf; "
                 "echo escuchar=red-local >> /home/.config/vncd.conf; echo fps_max=5 >> /home/.config/vncd.conf'")
        pos = len(m.serial())
        orden(m, "servicio iniciar vncd")
        esperar(m, r"vncd: escuchando en el puerto 5900", 40, pos)
        s = m.serial()[pos:]
        verificar("se ignora seguridad" in s and "se ignora escuchar" in s, "el vncd ignora seguridad y escuchar de /home")
        verificar("solo desde esta máquina" in s, "sigue escuchando solo desde esta máquina")
        verificar("se ignora fps_max" not in s, "fps_max de /home sí vale")
        orden(m, "servicio parar vncd")


def imagen_b(nome, carpeta_plantada):
    raiz = os.path.join(TRAB, "rootfs-" + nome)
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(B, "rootfs"), raiz)
    with open(os.path.join(raiz, "etc", "usuarios"), "a") as f:
        f.write("ana:1001:1001:/home/ana:/bin/sagsh:Ana\n")
    with open(os.path.join(raiz, "etc", "sombra"), "w") as f:
        f.write("sagui:!\n")
    if carpeta_plantada:
        os.makedirs(os.path.join(raiz, "home", "ana", ".config"), exist_ok=True)
        with open(os.path.join(raiz, "home", "ana", ".config", "plantado.txt"), "w") as f:
            f.write("lo dejo sagui\n")
    sgfs = os.path.join(TRAB, nome + ".sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, nome + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(B, "boot", "mbr.bin"), "--estagio2", os.path.join(B, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--sesion-sagui", "--sin-conf"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def parte_b():
    os.makedirs(TRAB, exist_ok=True)
    img = imagen_b("plantada", True)
    with Maquina("h371b", "ide", imagem=img) as m:
        esperar(m, r"\[SESION\] /home/ana no es una carpeta de ana", 120)
        esperar(m, r"\[SESION\] pantalla de inicio de sesion: 1 cuenta\(s\)", 30)
        time.sleep(3)
        s = m.serial()
        verificar("entrada directa" not in s, "con una carpeta ajena no se entra directo")
        verificar("sesion local: uid 1001" not in s, "la sesión no pasa a ana")
        verificar(not re.search(r"\[SAGUI\] terminal: \d+", s), "no se abre el Terminal")
        m.captura("H371-carpeta-ajena")
    img = imagen_b("limpia", False)
    with Maquina("h371c", "ide", imagem=img) as m:
        esperar(m, r"\[SESION\] carpeta /home/ana creada para ana: 0", 120)
        esperar(m, r"\[SESION\] una sola cuenta sin contrasena \(ana\): entrada directa", 30)
        verificar("sesion local: uid 1001 gid 1001, carpeta /home/ana" in m.serial(), "entra ana con /home/ana")


def main():
    parte_a()
    parte_b()
    print("H371 OK")


if __name__ == "__main__":
    main()
