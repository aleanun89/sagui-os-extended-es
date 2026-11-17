#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H384 (D371, fase 4) - sagui pasa de /home entero a /home/sagui, una vez, reanudable y sin borrar nada.

Consola, sesión sagui, /home en ext2 y SAGUI-CONF, sin instantánea:
1. Arranque 1 sin migrar (--sin-migrar-home, como un sistema de antes): sagui deja archivos en /home, root un
   archivo suyo (como lo de antes de D282) y la carpeta de otra cuenta (ana, 0700); se simula una migración
   cortada a medias (/home/sagui.nuevo con una carpeta ya movida y la marca «empezada» en SAGUI-CONF).
2. Arranque 2 (se quita el bit de la cabecera, como una imagen normal): la migración sigue donde iba y acaba;
   sagui ve sus archivos en /home (su carpeta), puede cambiar el de root, no ve a ana; /home es de root (0755),
   /home/sagui de sagui 0700, /home/compartido 1777 (la misma para todas, pegajosa), /etc/usuarios dice /home/sagui;
   las claves de la máquina que había en /home (SSH, VNC) pasan a /etc y no a la carpeta de sagui.
3. Arranque 3: no se repite; todo sigue. En el anfitrión: /sagui/doc.txt en la partición y nada suelto."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, extrair_particao, debugfs, particao_dados_img  # noqa: E402

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


def reiniciar(m):
    m.digitar("reiniciar", 0.04)
    m.tecla("ret")
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, "reinicia")


def arrancar(m):
    esperar(m, r"supervisor en marcha", 150)
    esperar(m, r"sagui:\S*\$ ", 60)
    time.sleep(2)


def main():
    img = imagem_de_teste("", ["--console", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2", "--sin-migrar-home"])
    with Maquina("h384a", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        verificar("casa_migrar" not in m.serial(), "con --sin-migrar-home no se migra")
        out = interativo(m, "admin id", [("Contraseña nueva:", PW), ("Repítela:", PW)])
        verificar("uid=0(root)" in out, "root con contraseña")
        orden(m, "echo DOC-SAGUI > /home/doc.txt; mkdir -p /home/.config /home/fotos /home/musica; "
                 "echo F1 > /home/fotos/f1.txt; echo M1 > /home/musica/m1.txt; echo CONF > /home/.config/prueba.conf")
        orden(m, f"echo {PW} | admin 'echo VIEJO > /home/viejo-root.txt; "
                 "echo ana:1001:1001:/home/ana:/bin/sagsh:Ana >> /etc/usuarios; cp /etc/usuarios /sistema/conf/etc/usuarios; "
                 "mkdir /home/ana; echo DE-ANA > /home/ana/a.txt; chown 1001:1001 /home/ana /home/ana/a.txt; chmod 700 /home/ana; "
                 "mkdir -p /home/.ssh; echo SAGUIOS-ED25519-SEMILLA CLAVEVIEJA > /home/.ssh/ssh_host_ed25519_key; "
                 "echo vncvieja > /home/.config/vncd.passwd; "
                 "mkdir /home/sagui.nuevo; mv /home/fotos /home/sagui.nuevo/; echo empezada > /sistema/conf/etc/migracion-home'")
        out = orden(m, "ls -a /home")
        verificar("sagui.nuevo" in out and "doc.txt" in out, "una migración cortada a medias, simulada")
        reiniciar(m)
    with open(img, "r+b") as f:   # como una imagen normal: el bit de la cabecera fuera
        f.seek(512 + 162)
        b = f.read(1)[0]
        f.seek(512 + 162)
        f.write(bytes([b & ~0x02]))
    with Maquina("h384b", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        s = m.serial()
        verificar("(se sigue lo empezado)" in s, "la migración sigue la empezada")
        verificar(re.search(r"casa_migrar: hecha: sagui en /home/sagui \(0\)", s), "la migración termina")
        verificar("[CONF] /home: de root" in s, "el kernel pasa /home a root")
        verificar("sesion local: uid 1000 gid 1000, carpeta /home/sagui" in s, "la sesión de la consola, con /home/sagui")
        out = orden(m, "id; pwd; ls -a /home; cat /home/doc.txt /home/fotos/f1.txt /home/musica/m1.txt /home/.config/prueba.conf")
        verificar("DOC-SAGUI" in out and "F1" in out and "M1" in out and "CONF" in out, f"sagui ve todo lo suyo en /home ({out[-500:]!r})")
        verificar("sagui.nuevo" not in out and "lost+found" not in out, "sin restos de la migración")
        out = orden(m, "echo NUEVO >> /home/viejo-root.txt; cat /home/viejo-root.txt; cat /home/ana/a.txt")
        verificar("NUEVO" in out.split("cat /home/viejo")[-1], "sagui cambia lo de root que estaba en su /home (D282.7 en su carpeta)")
        verificar("DE-ANA" not in out.split("cat /home/ana")[-1], "sagui no ve lo de ana")
        out = orden(m, f"echo {PW} | admin 'ls -la --color=never /home; cat /etc/usuarios'")
        verificar(re.search(r"^drwx------ sagui +sagui .* sagui/?$", out, re.M), f"/home/sagui de sagui y 0700 ({out[-900:]!r})")
        verificar(re.search(r"^drwxrwxrwt root +root .* compartido/?$", out, re.M), "/home/compartido 1777 de root")
        verificar(re.search(r"^drwx------ ana +ana .* ana/?$", out, re.M), "la carpeta de ana no se toca")
        verificar(re.search(r"^sagui:1000:1000:/home/sagui:", out, re.M), "/etc/usuarios: sagui en /home/sagui")
        verificar(not re.search(r"doc\.txt", out.split("ls -la")[-1].split("cat /etc")[0]), "nada suelto en /home")
        out = orden(m, "echo X > /home/compartido/de-sagui; ls -l --color=never /home/compartido")
        verificar("de-sagui" in out.split("ls -l")[-1], "sagui escribe en /home/compartido")
        # D371 (revisión): la carpeta común es la de verdad (no /home/sagui/compartido) y con el bit pegajoso;
        # las claves de la máquina que había en /home no pasan a la carpeta de sagui.
        out = orden(m, f"echo {PW} | admin 'echo A-\"1\"; ls --color=never /home/compartido; echo B-\"1\"; "
                       "echo R > /home/compartido/de-root; ls -a --color=never /home/sagui/.ssh /home/sagui/.config; echo C-\"1\"; "
                       "cat /etc/ssh/ssh_host_ed25519_key /etc/vnc/vncd.passwd; echo D-\"1\"'")
        def tramo(a, b):
            return out.split(f"\n{a}-1")[-1].split(f"\n{b}-1")[0]
        verificar("de-sagui" in tramo("A", "B"), f"la común es /home/compartido ({out[-700:]!r})")
        verificar("ssh_host_ed25519_key" not in tramo("B", "C") and "vncd.passwd" not in tramo("B", "C"),
                  f"las claves de la máquina no pasan a la carpeta de sagui ({out[-700:]!r})")
        verificar("SAGUIOS-ED25519-SEMILLA CLAVEVIEJA" in tramo("C", "D") and "vncvieja" in tramo("C", "D"),
                  "y siguen en /etc (SAGUI-CONF)")
        out = orden(m, "rm /home/compartido/de-root; echo E-'1'; ls --color=never /home/compartido")
        verificar("de-root" in out.split("\nE-1")[-1], "sagui no borra lo de otro en /home/compartido (pegajoso)")
        reiniciar(m)
    with Maquina("h384c", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        verificar("casa_migrar" not in m.serial(), "la migración no se repite")
        out = orden(m, "cat /home/doc.txt")
        verificar("DOC-SAGUI" in out.split("cat /home")[-1], "todo sigue tras otro arranque")
        orden(m, "sync")
    ini = particao_dados_img(img)[0]
    part = extrair_particao(img, ini, 32)
    verificar(b"DOC-SAGUI" in debugfs(part, "cat /sagui/doc.txt"), "en el anfitrión: /sagui/doc.txt en la partición")
    verificar(b"DOC-SAGUI" not in debugfs(part, "cat /doc.txt"), "y no suelto en la raíz")
    print("H384 OK")


if __name__ == "__main__":
    main()
