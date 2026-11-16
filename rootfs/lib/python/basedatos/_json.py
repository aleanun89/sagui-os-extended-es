# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._json - documentos de lite-monkey como JSON editable (D333). Los tipos que JSON no tiene van con
# la notación extendida {"$oid": "..."} y {"$fecha": milisegundos}; un binario no se puede escribir en JSON y
# el editor se niega a guardar un documento que lo tenga. Sin tkinter: se prueba en el anfitrión.
import json


def a_json(v):
    """Un valor de documento con solo tipos de JSON."""
    if isinstance(v, dict):
        return dict((k, a_json(x)) for k, x in v.items())
    if isinstance(v, (list, tuple)):
        return [a_json(x) for x in v]
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    n = type(v).__name__
    if n == "ObjectId":
        return {"$oid": str(v)}
    if n == "Fecha":
        return {"$fecha": v.ms}
    if n == "datetime":
        import calendar
        return {"$fecha": calendar.timegm(v.timetuple()) * 1000 + v.microsecond // 1000}
    if isinstance(v, (bytes, bytearray)) or n == "Binary":
        return {"$binario": "no editable"}
    return str(v)


def tiene_binarios(v):
    if isinstance(v, dict):
        return any(tiene_binarios(x) for x in v.values())
    if isinstance(v, (list, tuple)):
        return any(tiene_binarios(x) for x in v)
    return isinstance(v, (bytes, bytearray)) or type(v).__name__ in ("Binary", "Regex")


def de_json(v, m):
    """Lo contrario de a_json; `m` es el módulo litemonkey (para ObjectId y Fecha)."""
    if isinstance(v, dict):
        if len(v) == 1 and "$oid" in v:
            return m.ObjectId(v["$oid"])
        if len(v) == 1 and "$fecha" in v:
            return m.Fecha(int(v["$fecha"]))
        return dict((k, de_json(x, m)) for k, x in v.items())
    if isinstance(v, list):
        return [de_json(x, m) for x in v]
    return v


def texto_json(v, sangria=2):
    """El valor como texto JSON con sangría (MicroPython no admite `indent`: se hace a mano)."""
    return _bonito(a_json(v), 0, sangria)


def _bonito(v, nivel, paso):
    pad = " " * ((nivel + 1) * paso)
    cierre = " " * (nivel * paso)
    if isinstance(v, dict):
        if not v:
            return "{}"
        partes = ["%s%s: %s" % (pad, json.dumps(k), _bonito(x, nivel + 1, paso)) for k, x in v.items()]
        return "{\n" + ",\n".join(partes) + "\n" + cierre + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        if all(not isinstance(x, (dict, list)) for x in v) and len(json.dumps(v)) < 70:
            return json.dumps(v)
        partes = [pad + _bonito(x, nivel + 1, paso) for x in v]
        return "[\n" + ",\n".join(partes) + "\n" + cierre + "]"
    return json.dumps(v)


# ------------------------------------------------------------ lector estricto
# El json.loads de MicroPython perdona un texto cortado ({"a": 1, "b": ) y devuelve lo que alcanzó a leer; el editor
# tiene que negarse a guardar eso, así que el texto se valida con un analizador propio antes de usar json.loads.

def _ws(t, i):
    while i < len(t) and t[i] in " \t\r\n":
        i += 1
    return i


def _valor(t, i, prof):
    if prof > 60:
        raise ValueError("demasiado anidado")
    i = _ws(t, i)
    if i >= len(t):
        raise ValueError("el texto termina antes de acabar el valor (posición %d)" % i)
    c = t[i]
    if c == "{":
        i = _ws(t, i + 1)
        if i < len(t) and t[i] == "}":
            return i + 1
        while True:
            i = _ws(t, i)
            if i >= len(t) or t[i] != '"':
                raise ValueError("se esperaba el nombre de un campo entre comillas (posición %d)" % i)
            i = _cadena(t, i)
            i = _ws(t, i)
            if i >= len(t) or t[i] != ":":
                raise ValueError("falta «:» (posición %d)" % i)
            i = _valor(t, i + 1, prof + 1)
            i = _ws(t, i)
            if i < len(t) and t[i] == ",":
                i += 1
                continue
            if i < len(t) and t[i] == "}":
                return i + 1
            raise ValueError("se esperaba «,» o «}» (posición %d)" % i)
    if c == "[":
        i = _ws(t, i + 1)
        if i < len(t) and t[i] == "]":
            return i + 1
        while True:
            i = _valor(t, i, prof + 1)
            i = _ws(t, i)
            if i < len(t) and t[i] == ",":
                i += 1
                continue
            if i < len(t) and t[i] == "]":
                return i + 1
            raise ValueError("se esperaba «,» o «]» (posición %d)" % i)
    if c == '"':
        return _cadena(t, i)
    for lit in ("true", "false", "null"):
        if t.startswith(lit, i):
            return i + len(lit)
    j = i
    while j < len(t) and t[j] in "+-0123456789.eE":
        j += 1
    if j == i:
        raise ValueError("valor no válido (posición %d)" % i)
    try:
        float(t[i:j])
    except ValueError:
        raise ValueError("número no válido (posición %d)" % i)
    return j


def _cadena(t, i):
    j = i + 1
    while j < len(t):
        if t[j] == "\\":
            j += 2
            continue
        if t[j] == '"':
            return j + 1
        j += 1
    raise ValueError("cadena sin cerrar (posición %d)" % i)


def cargar(texto):
    """json.loads, pero estricto: un texto cortado, con basura al final o mal formado da ValueError."""
    t = texto
    fin = _valor(t, 0, 0)
    if _ws(t, fin) != len(t):
        raise ValueError("hay texto de más después del valor (posición %d)" % _ws(t, fin))
    return json.loads(t)
