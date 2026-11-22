# SPDX-License-Identifier: GPL-3.0-or-later
# pwd.py - la base de datos de usuarios de SaguiOS con la interfaz del pwd de CPython (D419).
#
# Lee /etc/usuarios (nombre:uid:gid:carpeta:shell:descripcion, D255/D340) y le suma las
# cuentas de serie (root, sagui, servicio) que el sistema da por buenas aunque el archivo
# falte. No hay contraseñas aquí: pw_passwd es siempre "x" (las claves están en /etc/sombra,
# que solo lee el administrador).
from collections import namedtuple

struct_passwd = namedtuple("struct_passwd", ["pw_name", "pw_passwd", "pw_uid", "pw_gid", "pw_gecos", "pw_dir", "pw_shell"])

_SERIE = (
    ("root", 0, 0, "/root", "/bin/sagsh", "Administrador"),
    ("sagui", 1000, 1000, "/home", "/bin/sagsh", "Usuario del escritorio"),
    ("servicio", 100, 100, "/", "", "Servicios sin escritorio"),
)


def _leer():
    cuentas = []
    vistos = set()
    try:
        with open("/etc/usuarios", "r") as f:
            texto = f.read()
    except OSError:
        texto = ""
    for linea in texto.split("\n"):
        linea = linea.strip()
        if not linea or linea[0] == "#":
            continue
        c = linea.split(":")
        if len(c) < 4:
            continue
        try:
            uid = int(c[1])
            gid = int(c[2])
        except ValueError:
            continue
        casa = c[3]
        shell = c[4] if len(c) > 4 else ""
        desc = c[5] if len(c) > 5 else ""
        if c[0] in vistos:
            continue
        vistos.add(c[0])
        cuentas.append(struct_passwd(c[0], "x", uid, gid, desc, casa, shell))
    for n, uid, gid, casa, shell, desc in _SERIE:
        if n not in vistos:
            vistos.add(n)
            cuentas.append(struct_passwd(n, "x", uid, gid, desc, casa, shell))
    return cuentas


def getpwall():
    """Todas las cuentas (las del archivo y las de serie)."""
    return _leer()


def getpwnam(nombre):
    if not isinstance(nombre, str):
        raise TypeError("getpwnam() argument must be str, not " + type(nombre).__name__)
    for c in _leer():
        if c.pw_name == nombre:
            return c
    raise KeyError("getpwnam(): name not found: " + repr(nombre))


def getpwuid(uid):
    if not isinstance(uid, int):
        raise TypeError("an integer is required")
    for c in _leer():
        if c.pw_uid == uid:
            return c
    raise KeyError("getpwuid(): uid not found: " + str(uid))
