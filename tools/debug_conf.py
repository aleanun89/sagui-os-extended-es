#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""debug_conf.py - prepara el árbol de SAGUI-CONF de las imágenes de DEPURACIÓN (D418).

Lo usa «make debug_variantes» (tools/debug_variantes.sh). Escribe en una carpeta lo que
mkimagem.py (--conf-extra) copia a SAGUI-CONF de cada imagen de disco:

  etc/ssh/authorized_keys          la clave PÚBLICA del PC (solo ssh-ed25519) para root
  etc/ssh/ssh_host_ed25519_key(.pub)   la clave FIJA del servidor (para que known_hosts no cambie)
  etc/ssh/sshd.conf                solo_red_local=si
  etc/servicios/activados          sshd=si y ntp=si (arrancan solos)
  etc/zona                         Europe/Madrid

La clave del servidor se genera UNA vez y vive FUERA de git, en la carpeta privada
(por defecto ~/SO/private/debug-ssh, permisos 600). Nunca se imprime: solo su huella.
Además deja en la carpeta privada un known_hosts listo para el cliente ssh.

Uso: debug_conf.py --salida CARPETA [--clave PUB] [--privado DIR]
"""
import argparse
import base64
import hashlib
import os
import stat
import struct
import sys

MARCA_CLAVE = "SAGUIOS-ED25519-SEMILLA"   # el formato de la clave del servidor de userland/programas/sshd.c
TZ_MADRID = "CET-1CEST,M3.5.0,M10.5.0/3"   # la misma que userland/libsagui/zonas.c


def blob_ed25519(pub):
    t = b"ssh-ed25519"
    return struct.pack(">I", len(t)) + t + struct.pack(">I", len(pub)) + pub


def publica_de_semilla(semilla):
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
    k = Ed25519PrivateKey.from_private_bytes(semilla)
    return k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def huella(pub):
    h = hashlib.sha256(blob_ed25519(pub)).digest()
    return "SHA256:" + base64.b64encode(h).decode().rstrip("=")


def escribir(ruta, texto, modo=0o644):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    fd = os.open(ruta, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, modo)
    with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
        f.write(texto)
    os.chmod(ruta, modo)


def semilla_fija(privado):
    """Lee (o crea la primera vez) la semilla del servidor en la carpeta privada."""
    os.makedirs(privado, mode=0o700, exist_ok=True)
    os.chmod(privado, 0o700)
    ruta = os.path.join(privado, "ssh_host_ed25519_key")
    if not os.path.exists(ruta):
        escribir(ruta, MARCA_CLAVE + " " + base64.b64encode(os.urandom(32)).decode() + "\n", 0o600)
        print(f"debug_conf: clave del servidor creada (una vez) en {privado}")
    with open(ruta, encoding="utf-8") as f:
        t = f.read().split()
    if len(t) < 2 or t[0] != MARCA_CLAVE:
        sys.exit(f"debug_conf: {ruta} no tiene el formato esperado")
    if stat.S_IMODE(os.stat(ruta).st_mode) & 0o077:
        os.chmod(ruta, 0o600)
    semilla = base64.b64decode(t[1])
    if len(semilla) != 32:
        sys.exit(f"debug_conf: {ruta}: la semilla no mide 32 bytes")
    return semilla


def principal():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--salida", required=True, help="carpeta donde se deja el árbol de SAGUI-CONF (se vacía antes)")
    ap.add_argument("--clave", default=os.path.expanduser("~/.ssh/saguios_amilo.pub"), help="clave pública del PC (ssh-ed25519)")
    ap.add_argument("--privado", default=os.environ.get("DEBUG_PRIVADO") or os.path.expanduser("~/SO/private/debug-ssh"),
                    help="carpeta privada (fuera de git) con la clave del servidor")
    a = ap.parse_args()
    if not os.path.isfile(a.clave):
        sys.exit(f"debug_conf: no existe la clave pública {a.clave} (DEBUG_CLAVE=...)")
    lineas = [l.strip() for l in open(a.clave, encoding="utf-8") if l.strip() and not l.startswith("#")]
    if not lineas or not all(l.startswith("ssh-ed25519 ") for l in lineas):
        sys.exit("debug_conf: sshd solo acepta claves ssh-ed25519 (una por línea) y sin opciones delante")
    if any("PRIVATE KEY" in l for l in lineas):
        sys.exit("debug_conf: eso es una clave PRIVADA: pasa la .pub")
    semilla = semilla_fija(a.privado)
    pub = publica_de_semilla(semilla)
    linea_pub = "ssh-ed25519 " + base64.b64encode(blob_ed25519(pub)).decode() + " root@saguios"
    escribir(os.path.join(a.privado, "ssh_host_ed25519_key.pub"), linea_pub + "\n", 0o644)
    escribir(os.path.join(a.privado, "known_hosts"), "# SaguiOS de depuración (D418): vale para cualquier IP\n* " + linea_pub + "\n", 0o644)
    # El árbol de SAGUI-CONF
    import shutil
    if os.path.isdir(a.salida):
        shutil.rmtree(a.salida)
    s = a.salida
    escribir(os.path.join(s, "etc/ssh/authorized_keys"), "\n".join(lineas) + "\n", 0o644)
    escribir(os.path.join(s, "etc/ssh/ssh_host_ed25519_key"), MARCA_CLAVE + " " + base64.b64encode(semilla).decode() + "\n", 0o600)
    escribir(os.path.join(s, "etc/ssh/ssh_host_ed25519_key.pub"), linea_pub + "\n", 0o644)
    escribir(os.path.join(s, "etc/ssh/sshd.conf"),
             "# Imagen de depuración (D418): solo desde la red local.\npuerto=22\nsolo_red_local=si\n", 0o644)
    escribir(os.path.join(s, "etc/servicios/activados"),
             "# Imagen de depuración (D418): sshd y ntp arrancan solos.\nsshd=si\nntp=si\n", 0o644)
    escribir(os.path.join(s, "etc/zona"),
             f"# Zona horaria (imagen de depuración, D418)\nzona=Europe/Madrid\ntz={TZ_MADRID}\nformato=24\n", 0o644)
    print(f"debug_conf: huella del servidor: {huella(pub)}")
    print(f"debug_conf: known_hosts listo en {os.path.join(a.privado, 'known_hosts')}")


if __name__ == "__main__":
    principal()
