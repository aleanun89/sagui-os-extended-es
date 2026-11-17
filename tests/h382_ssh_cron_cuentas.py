#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H382 (D371, fase 5) - SSH y cron por cuenta, con sagui ya en /home/sagui.

Consola, sesión sagui, /home en ext2, SAGUI-CONF y la migración de sagui (--migrar-home):
- SSH: la clave de sagui (en su /home/.ssh, de verdad /home/sagui/.ssh) entra como sagui y ve su carpeta; la de
  ana (en /home/ana/.ssh) entra como ana y ve la suya; una clave en un archivo que otros pueden escribir no
  vale (StrictModes); el /home/.ssh/sshd.conf de sagui no cambia el sshd (es del sistema).
- cron: la tabla de sagui corre como sagui; la de ana no corre hasta que el administrador la pone en
  /etc/cron.permitidos, y entonces corre como ana."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import h254_usuarios as h254  # noqa: E402

PW = "raiz2468"


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


def main():
    os.makedirs(h254.TRAB, exist_ok=True)
    k_sagui, k_ana, k_mala = h254.gerar_chave("h382_sagui"), h254.gerar_chave("h382_ana"), h254.gerar_chave("h382_mala")
    pub = lambda k: open(k + ".pub").read().strip()
    img = imagem_de_teste("", ["--console", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2", "--migrar-home"])
    porta = h254.puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]
    with Maquina("h382", "ide", imagem=img, extras=extras) as m:
        esperar(m, r"supervisor en marcha", 150)
        esperar(m, r"sagui:\S*\$ ", 60)
        time.sleep(2)
        verificar("casa_migrar: hecha: sagui en /home/sagui" in m.serial(), "sagui ya está en /home/sagui")
        out = interativo(m, "admin id", [("Contraseña nueva:", PW), ("Repítela:", PW)])
        verificar("uid=0(root)" in out, "root con contraseña")
        # sagui: su clave, su crontab y un sshd.conf que no debe valer
        orden(m, f"mkdir -p /home/.ssh /home/.config; echo '{pub(k_sagui)}' > /home/.ssh/authorized_keys; "
                 "chmod 700 /home/.ssh; chmod 600 /home/.ssh/authorized_keys; echo puerto=2299 > /home/.ssh/sshd.conf; "
                 "echo DE-SAGUI > /home/nota.txt")
        orden(m, "echo '* * * * * id -u >> /tmp/cron-sagui' > /tmp/t; crontab /tmp/t")
        # ana (la crea root): su clave, su crontab; y una clave en un archivo que puede escribir cualquiera
        orden(m, f"echo {PW} | admin 'echo ana:1001:1001:/home/ana:/bin/sagsh:Ana >> /etc/usuarios; "
                 "cp /etc/usuarios /sistema/conf/etc/usuarios; mkdir -p /home/ana/.ssh /home/ana/.config; "
                 f"echo {pub(k_ana)} > /home/ana/.ssh/authorized_keys; echo DE-ANA > /home/ana/nota.txt; "
                 "echo \"* * * * * id -u >> /tmp/cron-ana\" > /home/ana/.config/crontab; "
                 "chown 1001:1001 /home/ana /home/ana/.ssh /home/ana/.ssh/authorized_keys /home/ana/.config /home/ana/.config/crontab /home/ana/nota.txt; "
                 "chmod 700 /home/ana /home/ana/.ssh; chmod 600 /home/ana/.ssh/authorized_keys'", 120)
        pos = len(m.serial())
        orden(m, f"echo {PW} | admin 'servicio reiniciar cron; servicio iniciar sshd'", 120)
        esperar(m, r"escuchando en el puerto (\d+)", 60, pos)
        verificar(re.search(r"escuchando en el puerto 22 ", m.serial()[pos:]), "el sshd.conf de la carpeta de sagui no vale")
        r = h254.ssh(porta, k_sagui, "sagui", "id; cat /home/nota.txt")
        verificar(b"uid=1000(sagui)" in r.stdout and b"DE-SAGUI" in r.stdout, f"la clave de sagui entra como sagui ({r.stdout!r} {r.stderr!r})")
        r = h254.ssh(porta, k_ana, "ana", "id; cat /home/nota.txt")
        verificar(b"uid=1001(ana)" in r.stdout and b"DE-ANA" in r.stdout, f"la clave de ana entra como ana y ve su carpeta ({r.stdout!r} {r.stderr!r})")
        r = h254.ssh(porta, k_mala, "sagui", "id")
        verificar(b"uid=" not in r.stdout, "una clave que no está en ningún archivo no entra")
        # cron: sagui sí, ana no (aún)
        time.sleep(75)
        out = orden(m, "cat /tmp/cron-sagui; cat /tmp/cron-ana")
        verificar(re.search(r"^1000$", out, re.M), f"la tabla de sagui corre como sagui ({out[-300:]!r})")
        verificar(not re.search(r"^1001$", out, re.M), "la de ana no corre sin permiso")
        orden(m, f"echo {PW} | admin 'echo ana > /etc/cron.permitidos; servicio reiniciar cron'", 120)
        time.sleep(75)
        out = orden(m, "cat /tmp/cron-ana")
        verificar(re.search(r"^1001$", out, re.M), f"con permiso, la de ana corre como ana ({out[-300:]!r})")
        # StrictModes: ana deja su authorized_keys escribible por todos
        orden(m, f"echo {PW} | admin 'chmod 666 /home/ana/.ssh/authorized_keys; servicio reiniciar sshd'", 120)
        time.sleep(3)
        r = h254.ssh(porta, k_ana, "ana", "id")
        verificar(b"uid=1001" not in r.stdout, "con el archivo escribible por otros, la clave de ana no vale")
        verificar("no se usa: la carpeta o el archivo los puede cambiar otro" in m.serial(), "el sshd lo dice")
    print("H382 OK")


if __name__ == "__main__":
    main()
