# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._filtros - el filtro por columna del Explorador SQLite (D333): lo que el usuario escribe en el
# cuadro de una columna se convierte en un filtro de la biblioteca (el mismo diccionario que entiende
# `consultar`). Sin tkinter: se prueba en el anfitrión.
#
#     texto       significa
#     Ana         igual a «Ana»
#     >5  >=5     mayor, mayor o igual (también <  <=  <>  !=  =)
#     ~ana        contiene «ana» (sin distinguir mayúsculas)
#     NULL        es NULL (distinto del texto vacío)
#     !NULL       no es NULL
#     ""          el texto vacío entre comillas dobles: igual a ''
from basedatos import valor_desde_texto, ErrorConsulta

AYUDA = "x = igual, >5 <5 >=5 <=5 <>x, ~x contiene, NULL, !NULL"

_OPS = ((">=", "$gte"), ("<=", "$lte"), ("<>", "$ne"), ("!=", "$ne"), (">", "$gt"), ("<", "$lt"), ("=", "$eq"))


def condicion(col, texto, tipo=None):
    """El filtro de una columna ({col: valor} o {col: {op: valor}}) o None si el texto está vacío."""
    t = (texto or "").strip()
    if t == "":
        return None
    if t == "NULL":
        return {col: None}
    if t == "!NULL":
        return {col: {"$ne": None}}
    if t == '""':
        return {col: ""}
    if t[0] == "~":
        x = t[1:]
        if x == "":
            raise ErrorConsulta("falta el texto que buscar después de «~»")
        return {col: {"$contiene": x}}
    for pref, op in _OPS:
        if t.startswith(pref):
            x = t[len(pref):].strip()
            if x == "":
                raise ErrorConsulta("falta el valor después de «%s»" % pref)
            if x == "NULL":
                return {col: {op: None}} if op in ("$eq", "$ne") else None
            return {col: {op: valor_desde_texto(x, tipo)}}
    return {col: valor_desde_texto(t, tipo)}


def combinar(condiciones):
    """Varias condiciones de columna con AND: None, la propia condición o {"$and": [...]}."""
    c = [x for x in condiciones if x]
    if not c:
        return None
    if len(c) == 1:
        return c[0]
    return {"$and": c}


def describir(col, texto):
    return "%s %s" % (col, texto.strip())
