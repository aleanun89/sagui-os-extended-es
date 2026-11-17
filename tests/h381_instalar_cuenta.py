#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H381 (D371, fase 3) - el instalador con la cuenta elegida y la contraseña de root, y la primera
contraseña de root en la pantalla del sistema.

A. «instalar hdb -s --cuenta ana --clave-usuario X --clave-root X --cuenta-admin» (la opción «usar la misma
   para root» del asistente): en SAGUI-CONF (D379: o en la SAGUI-RAIZ instalada), /etc/usuarios con ana:1000:1000:/home/ana (sin sagui), la sombra
   con root y ana (PBKDF2), /etc/grupos con admin:10:ana y /etc/sistema.conf; en /home, /ana de uid 1000 y
   0700. El disco instalado arranca en la pantalla de inicio de sesión con ana; con su contraseña entra con
   /home/ana, y «admin» acepta su propia contraseña (administradora).
B. Una imagen con SAGUI-CONF y root sin contraseña (--pedir-root): el primer arranque pide elegirla en la
   pantalla del sistema; después se entra como siempre y admin la pide; el segundo arranque ya no la pide.
C. Lo mismo, pero con Esc («más tarde»): passwd root y admin, sin el diálogo, no la ponen desde la sesión."""
import os
import re
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, extrair_particao, debugfs, SAIDA  # noqa: E402
import h318_instalador as h318  # noqa: E402

CLAVE = "ana24680"
PW = "raiz1357"
MB = 1024 * 1024


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


def quitar_sesion_root(img):
    """El disco instalado copia la cabecera de la imagen de prueba (sesión root): se quita, como en una
    imagen publicada, para ver la pantalla de inicio de sesión."""
    with open(img, "r+b") as f:
        f.seek(512 + 6)
        (op,) = struct.unpack("<H", f.read(2))
        f.seek(512 + 6)
        f.write(struct.pack("<H", op & ~0x8000))


def parte_a():
    dest = h318.disco_vazio("h381-dest", 1024)
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h381-inst", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        out = m.comando(f"instalar hdb -s --cuenta ana --clave-usuario {CLAVE} --clave-root {CLAVE} --cuenta-admin", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"instalar con la cuenta ana ({out.strip()[-300:]})")
    t = h318.sfdisk(dest)
    rot = {}
    for n, v in t.items():
        if n >= 5 and v[2] != "c":
            p = extrair_particao(dest, v[0], v[1] // 2048)
            r = __import__("subprocess").run([os.path.join(h318.E2, "e2label"), p], capture_output=True, text=True).stdout.strip()
            rot[r] = p
    home, conf = rot["SAGUI-DATOS"], rot.get("SAGUI-CONF") or rot["SAGUI-RAIZ"]   # D379 (M1): /etc en la raíz instalada
    usu = debugfs(conf, "cat /etc/usuarios")
    verificar(re.search(rb"^ana:1000:1000:/home/ana:/bin/sagsh:", usu, re.M) and not re.search(rb"^sagui:", usu, re.M),
              f"/etc/usuarios: ana es la cuenta principal, sin sagui ({usu!r})")
    sombra = debugfs(conf, "cat /etc/sombra")
    verificar(re.search(rb"^root:pbkdf2-sha256\$", sombra, re.M) and re.search(rb"^ana:pbkdf2-sha256\$", sombra, re.M)
              and CLAVE.encode() not in sombra, "la sombra: root y ana con PBKDF2")
    verificar(re.search(rb"^admin:10:ana$", debugfs(conf, "cat /etc/grupos"), re.M), "ana es administradora")
    verificar(b"Configuraci" in debugfs(conf, "cat /etc/sistema.conf"), "/etc/sistema.conf creado")
    st = debugfs(home, "stat /ana")
    verificar(re.search(rb"User:\s+1000\b", st) and re.search(rb"Mode:\s+0700", st), f"/home/ana de uid 1000 y 0700 ({st[:300]!r})")
    quitar_sesion_root(dest)
    with Maquina("h381-arranca", "ide", imagem=dest, snapshot=True) as m:
        esperar(m, r"\[SESION\] pantalla de inicio de sesion: 1 cuenta\(s\)", 240)
        verificar("root sin contrasena" not in m.serial(), "root ya tiene contraseña: no se pide")
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(2)
        m.captura("H381-1-inicio-ana")
        pos = len(m.serial())
        m.digitar(CLAVE, 0.05)
        m.tecla("ret")
        esperar(m, r"\[SESION\] entra ana \(uid 1000\)", 60, pos)
        verificar("sesion local: uid 1000 gid 1000, carpeta /home/ana" in m.serial()[pos:], "la sesión de ana con /home/ana")
        esperar(m, r"\[SAGUI\] terminal: \d+", 60, pos)
        time.sleep(4)
        out = orden(m, "id; echo dato > /home/nota; ls /home")
        verificar("uid=1000(ana)" in out, "el Terminal es ana")
        out = orden(m, f"echo {CLAVE} | admin 'id; ls -l --color=never /home'")
        verificar("uid=0(root)" in out, "admin acepta la contraseña de ana (administradora)")
        verificar(re.search(r"^drwx------ ana .* ana/?$", out, re.M), "/home/ana real, 0700")


def parte_b():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--pedir-root", "--home-mb", "32", "--home-fs", "ext2"])
    with Maquina("h381-raiz", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"\[SESION\] root sin contrasena: la pantalla pide elegirla", 180)
        verificar(not re.search(r"\[SAGUI\] terminal: \d+", m.serial()), "antes de elegirla no se abre nada")
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(2)
        m.captura("H381-2-contrasena-root")
        m.digitar("ab", 0.05)
        m.tecla("ret")
        m.digitar("ab", 0.05)
        m.tecla("ret")
        time.sleep(1)
        verificar("primera contrasena de root: guardada" not in m.serial(), "una contraseña corta no vale")
        pos = len(m.serial())
        m.digitar(PW, 0.05)
        m.tecla("ret")
        m.digitar(PW, 0.05)
        m.tecla("ret")
        esperar(m, r"\[SESION\] primera contrasena de root: guardada", 60, pos)
        esperar(m, r"\[SAGUI\] terminal: \d+", 60, pos)
        time.sleep(4)
        out = orden(m, f"echo {PW} | admin id")
        verificar("uid=0(root)" in out, "admin con la contraseña elegida en la pantalla")
        verificar(PW not in m.serial().replace(f"echo {PW}", ""), "la contraseña no sale en la serie")
        m.digitar("reiniciar", 0.04)
        m.tecla("ret")
        fim = time.time() + 90
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
    with Maquina("h381-raiz2", "ide", imagem=img, snapshot=True) as m:
        esperar(m, r"\[SAGUI\] terminal: \d+", 180)
        verificar("root sin contrasena" not in m.serial(), "el segundo arranque ya no la pide")


def parte_c():
    """D371 (revisión): «más tarde» (Esc) en la pantalla de root no deja que un programa de la sesión la ponga
    él (passwd root, admin sin diálogo): solo la pantalla o el diálogo de permisos."""
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--pedir-root", "--home-mb", "32", "--home-fs", "ext2"])
    with Maquina("h381-tarde", "ide", imagem=img, snapshot=True) as m:
        esperar(m, r"\[SESION\] root sin contrasena: la pantalla pide elegirla", 180)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(2)
        pos = len(m.serial())
        m.tecla("esc")
        esperar(m, r"primera contrasena de root: mas tarde", 30, pos)
        esperar(m, r"\[SAGUI\] terminal: \d+", 60, pos)
        time.sleep(4)
        out = orden(m, "printf 'tarde135\\ntarde135\\n' > /tmp/p; passwd root < /tmp/p")
        verificar("se elige en la pantalla del sistema" in out and "cambiada" not in out,
                  f"passwd root desde la sesión no pone la primera ({out[-300:]!r})")
        out = orden(m, "admin id < /tmp/p; rm /tmp/p")
        verificar("uid=0(root)" not in out, f"admin sin diálogo tampoco ({out[-300:]!r})")
        verificar("no la pone un programa" in m.serial()[pos:], "lo niega el kernel")
        verificar("CONCEDIDO" not in m.serial()[pos:] and "ELEVACION concedida" not in m.serial()[pos:],
                  "root sigue sin contraseña: nada concedido")


def main():
    parte_c()
    parte_b()
    parte_a()
    print("H381 OK")


if __name__ == "__main__":
    main()
