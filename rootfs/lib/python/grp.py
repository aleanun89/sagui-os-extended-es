# SPDX-License-Identifier: GPL-3.0-or-later
# grp.py - la base de datos de grupos de SaguiOS con la interfaz del grp de CPython (D419).
#
# Dos fuentes, como en Linux con los grupos privados de usuario:
#   - cada cuenta de /etc/usuarios tiene un grupo con su nombre y su gid (sin miembros
#     listados: es su grupo principal);
#   - /etc/grupos (nombre:gid:miembro,miembro, D340), p. ej. «admin:10:ana,sagui».
# Si dos entradas comparten gid, manda la del archivo de grupos.
from collections import namedtuple
import pwd

struct_group = namedtuple("struct_group", ["gr_name", "gr_passwd", "gr_gid", "gr_mem"])


def _leer():
    grupos = []
    gids = set()
    try:
        with open("/etc/grupos", "r") as f:
            texto = f.read()
    except OSError:
        texto = ""
    for linea in texto.split("\n"):
        linea = linea.strip()
        if not linea or linea[0] == "#":
            continue
        c = linea.split(":")
        if len(c) < 2:
            continue
        try:
            gid = int(c[1])
        except ValueError:
            continue
        miembros = [m for m in (c[2].split(",") if len(c) > 2 else []) if m]
        grupos.append(struct_group(c[0], "x", gid, miembros))
        gids.add(gid)
    nombres = set(g.gr_name for g in grupos)
    for u in pwd.getpwall():
        if u.pw_gid not in gids and u.pw_name not in nombres:
            gids.add(u.pw_gid)
            nombres.add(u.pw_name)
            grupos.append(struct_group(u.pw_name, "x", u.pw_gid, []))
    return grupos


def getgrall():
    """Todos los grupos."""
    return _leer()


def getgrnam(nombre):
    if not isinstance(nombre, str):
        raise TypeError("getgrnam() argument must be str, not " + type(nombre).__name__)
    for g in _leer():
        if g.gr_name == nombre:
            return g
    raise KeyError("getgrnam(): name not found: " + repr(nombre))


def getgrgid(gid):
    if not isinstance(gid, int):
        raise TypeError("an integer is required")
    for g in _leer():
        if g.gr_gid == gid:
            return g
    raise KeyError("getgrgid(): gid not found: " + str(gid))
