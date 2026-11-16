# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._consulta - la barra de consulta del Explorador lite-monkey (D333): lo que el usuario escribe en el
# filtro, la proyección, el orden, «saltar» y «límite» se convierte en lo que entiende la biblioteca. Sin tkinter:
# se prueba en el anfitrión.
import json
from basedatos import ErrorConsulta
from basedatos import _json


def filtro(texto):
    """El filtro como diccionario ({} si está vacío). Es JSON: {"edad": {"$gt": 30}}."""
    t = (texto or "").strip()
    if not t:
        return {}
    try:
        f = _json.cargar(t)
    except ValueError as e:
        raise ErrorConsulta("el filtro no es un JSON válido: %s" % e)
    if not isinstance(f, dict):
        raise ErrorConsulta("el filtro tiene que ser un documento JSON entre llaves")
    return f


def proyeccion(texto):
    """None, un diccionario JSON ({"a": 1}) o una lista de campos: «a, b» (solo esos) o «-a, -b» (todos menos esos)."""
    t = (texto or "").strip()
    if not t:
        return None
    if t[0] == "{":
        try:
            p = _json.cargar(t)
        except ValueError as e:
            raise ErrorConsulta("la proyección no es un JSON válido: %s" % e)
        return p
    campos = [c.strip() for c in t.split(",") if c.strip()]
    if all(c.startswith("-") for c in campos):
        return dict((c[1:], 0) for c in campos)
    if any(c.startswith("-") for c in campos):
        raise ErrorConsulta("la proyección no mezcla campos incluidos y excluidos (solo _id puede ir con -)")
    return dict((c, 1) for c in campos)


def orden(texto):
    """None o una lista de (campo, 1|-1): «precio», «-precio, nombre» o un JSON {"precio": -1}."""
    t = (texto or "").strip()
    if not t:
        return None
    if t[0] == "{":
        try:
            d = _json.cargar(t)
        except ValueError as e:
            raise ErrorConsulta("el orden no es un JSON válido: %s" % e)
        return [(k, -1 if (v == -1 or str(v).lower().startswith("d")) else 1) for k, v in d.items()]
    r = []
    for c in t.split(","):
        c = c.strip()
        if not c:
            continue
        if c.startswith("-"):
            r.append((c[1:], -1))
        else:
            r.append((c.lstrip("+"), 1))
    return r


def entero(texto, defecto, minimo=0, nombre="valor"):
    t = (texto or "").strip()
    if not t:
        return defecto
    try:
        n = int(t)
    except ValueError:
        raise ErrorConsulta("«%s» no es un número entero (%s)" % (t, nombre))
    if n < minimo:
        raise ErrorConsulta("%s no puede ser menor que %d" % (nombre, minimo))
    return n


def resumen(f, p, o, saltar, limite):
    """Una línea para el historial y las favoritas."""
    return json.dumps({"f": f, "p": p, "o": o, "s": saltar, "l": limite})
