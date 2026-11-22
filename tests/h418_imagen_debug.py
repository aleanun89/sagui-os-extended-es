#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H418 (D418) - imágenes de depuración para probar por SSH sin códigos.

Anfitrión: tools/debug_conf.py genera el árbol de SAGUI-CONF con la clave pública, sshd y ntp activados,
zona Europe/Madrid y una clave de servidor FIJA (la huella no cambia entre dos ejecuciones); `make variantes`
rechaza DEBUG_*; publicar.sh rechaza un árbol con authorized_keys y una carpeta de imágenes con la marca.

QEMU (reenvío slirp al puerto 22): imagen con --conf-extra, arrancada sin tocar nada: entra
`ssh -i clave root@127.0.0.1 id` con la huella fija (StrictHostKeyChecking=yes con el known_hosts generado),
y se captura el escritorio con el aviso rojo de imagen de prueba.
"""
import os
import shutil
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, BUILD, opcoes_ramdisk  # noqa: E402

TRAB = os.path.join(SAIDA, "h418")


def run(*a, **k):
    return subprocess.run(list(a), capture_output=True, text=True, errors="replace", **k)


def huella(pub):
    return run("ssh-keygen", "-lf", pub).stdout.split()[1]


def anfitrion():
    shutil.rmtree(TRAB, ignore_errors=True)
    os.makedirs(TRAB)
    clave = os.path.join(TRAB, "clave")
    run("ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", "h418", "-f", clave)
    priv = os.path.join(TRAB, "privado")
    conf = os.path.join(TRAB, "conf")
    script = os.path.join(RAIZ, "tools", "debug_conf.py")
    r = run("python3", script, "--salida", conf, "--clave", clave + ".pub", "--privado", priv)
    verificar(r.returncode == 0, "debug_conf.py genera el árbol\n" + r.stdout + r.stderr)
    h1 = huella(os.path.join(priv, "ssh_host_ed25519_key.pub"))
    verificar(h1 in r.stdout, f"imprime la huella ({h1})")
    verificar(oct(os.stat(os.path.join(priv, "ssh_host_ed25519_key")).st_mode & 0o777) == "0o600", "la clave del servidor es 0600")
    shutil.rmtree(conf)
    run("python3", script, "--salida", conf, "--clave", clave + ".pub", "--privado", priv)
    verificar(huella(os.path.join(conf, "etc/ssh/ssh_host_ed25519_key.pub")) == h1, "la huella es FIJA entre dos ejecuciones")
    t = lambda p: open(os.path.join(conf, p)).read()
    verificar(open(clave + ".pub").read().split()[1] in t("etc/ssh/authorized_keys"), "authorized_keys lleva la clave pública")
    verificar("sshd=si" in t("etc/servicios/activados") and "ntp=si" in t("etc/servicios/activados"), "sshd y ntp activados")
    verificar("solo_red_local=si" in t("etc/ssh/sshd.conf"), "solo_red_local=si")
    verificar("Europe/Madrid" in t("etc/zona"), "zona Europe/Madrid")
    # una clave privada se rechaza
    r = run("python3", script, "--salida", conf + "2", "--clave", clave, "--privado", priv)
    verificar(r.returncode != 0, "una clave privada en DEBUG_CLAVE se rechaza")
    # make variantes rechaza DEBUG_*
    r = run("make", "-C", RAIZ, "variantes", "DEBUG_DESTINO=/tmp/x")
    verificar(r.returncode != 0 and "DEBUG_" in r.stdout + r.stderr, "make variantes falla si se le pasa DEBUG_*")
    # publicar.sh
    tmp = TRAB
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import h260_publicar as h
    repo = h.repo_limpio(tmp, "repo_k")
    h.escribir(repo, "etc/ssh/authorized_keys", open(clave + ".pub").read())
    h.git(repo, "add", "-A")
    h.git(repo, "commit", "-q", "-m", "clave")
    rc, out = h.preparar(repo, tmp, os.path.join(tmp, "dest_k"))
    verificar(rc != 0 and "depuración" in out, "publicar.sh preparar rechaza un árbol con authorized_keys")
    im = os.path.join(TRAB, "imgs")
    os.makedirs(im)
    with open(os.path.join(im, "saguios.img"), "wb") as f:
        f.write(b"\0" * 4096 + b"IMAGEN-DE-DEPURACION-D418-NO-DISTRIBUIR" + b"\0" * 4096)
    r = run("bash", os.path.join(RAIZ, "tools", "publicar.sh"), "comprobar-imagenes", im)
    verificar(r.returncode != 0 and "depuración" in r.stderr, "publicar.sh (release) rechaza una imagen con la marca")
    os.makedirs(im + "2")
    open(os.path.join(im + "2", "saguios.img"), "wb").write(b"\0" * 4096)
    r = run("bash", os.path.join(RAIZ, "tools", "publicar.sh"), "comprobar-imagenes", im + "2")
    verificar(r.returncode == 0, "una imagen normal pasa la comprobación")
    return clave, priv, conf


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def qemu(clave, priv, conf):
    # Como una imagen de serie (sin --teste: los servicios sí arrancan) con SAGUI-CONF de depuración
    img = os.path.join(TRAB, "h418.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    *opcoes_ramdisk(), "--saida", img, "--vesa", "--conf-extra", conf], check=True, stdout=subprocess.DEVNULL)
    with open(img, "rb") as f:
        verificar(b"IMAGEN-DE-DEPURACION-D418-NO-DISTRIBUIR" in f.read(), "la imagen lleva la marca de depuración")
    porta = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]
    with Maquina("h418", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sshd: escuchando", 150)
        time.sleep(3)
        kh = os.path.join(priv, "known_hosts")
        base = ["ssh", "-p", str(porta), "-F", "/dev/null", "-i", clave, "-o", "IdentitiesOnly=yes", "-o", "IdentityAgent=none",
                "-o", "BatchMode=yes", "-o", "LogLevel=ERROR", "-o", f"UserKnownHostsFile={kh}", "-o", "StrictHostKeyChecking=yes",
                "-o", "ConnectTimeout=20"]
        r = run(*base, "root@127.0.0.1", "id", timeout=90)
        verificar(r.returncode == 0 and "uid=0" in r.stdout, f"ssh -i clave root@127.0.0.1 id entra sin códigos: {r.stdout!r} {r.stderr!r}")
        r = run("ssh-keyscan", "-T", "20", "-p", str(porta), "-t", "ed25519", "127.0.0.1")
        kf = os.path.join(TRAB, "scan")
        open(kf, "w").write(r.stdout)
        verificar(huella(kf) == huella(os.path.join(priv, "ssh_host_ed25519_key.pub")), "la huella que ve el cliente es la fija")
        r = run(*base, "root@127.0.0.1", "zona", timeout=60)
        verificar("Madrid" in r.stdout, f"zona Europe/Madrid: {r.stdout[:80]!r}")
        m.captura("H418-debug-escritorio")
        print("captura: docs/capturas/H418-debug-escritorio.png")


def main():
    clave, priv, conf = anfitrion()
    if "--sin-qemu" not in sys.argv:
        qemu(clave, priv, conf)
    print("H418 OK")


if __name__ == "__main__":
    main()
