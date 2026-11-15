# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._seguridad - análisis sencillo de SQL para el Explorador (mejora 127, D304): separa las
# sentencias y detecta las peligrosas (DROP, DELETE o UPDATE sin WHERE) para pedir confirmación.
# Sin dependencias de tkinter: se prueba en el anfitrión.

PALABRAS = ("select", "from", "where", "insert", "into", "values", "update", "set", "delete", "create",
            "table", "drop", "alter", "index", "view", "order", "by", "group", "having", "limit", "offset",
            "join", "left", "inner", "outer", "on", "as", "and", "or", "not", "null", "is", "in", "like",
            "between", "distinct", "union", "all", "primary", "key", "default", "unique", "asc", "desc",
            "begin", "commit", "rollback", "pragma", "case", "when", "then", "else", "end", "count",
            "sum", "avg", "min", "max", "exists", "if", "integer", "text", "real", "blob")


def _sin_literales(sql):
    """El SQL sin cadenas ni comentarios (para analizarlo sin que un texto engañe)."""
    r, i, n = [], 0, len(sql)
    while i < n:
        c = sql[i]
        if c in ("'", '"'):
            j = i + 1
            while j < n and sql[j] != c:
                j += 1
            r.append(" ")
            i = j + 1
        elif c == "-" and sql[i:i + 2] == "--":
            while i < n and sql[i] != "\n":
                i += 1
        elif c == "/" and sql[i:i + 2] == "/*":
            j = sql.find("*/", i + 2)
            i = n if j < 0 else j + 2
            r.append(" ")
        else:
            r.append(c)
            i += 1
    return "".join(r)


def sentencias(sql):
    """Las sentencias del texto, separadas por «;» (fuera de cadenas y comentarios), sin vacías."""
    # se parte el texto original por las mismas posiciones de «;» del texto limpio
    r, ini, i, n = [], 0, 0, len(sql)
    while i < n:
        c = sql[i]
        if c in ("'", '"'):
            j = i + 1
            while j < n and sql[j] != c:
                j += 1
            i = j + 1
            continue
        if sql[i:i + 2] == "--":
            while i < n and sql[i] != "\n":
                i += 1
            continue
        if sql[i:i + 2] == "/*":
            j = sql.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == ";":
            r.append(sql[ini:i])
            ini = i + 1
        i += 1
    r.append(sql[ini:])
    return [s.strip() for s in r if _sin_literales(s).strip()]


def peligrosa(sentencia):
    """Un texto que explica por qué hay que confirmar la sentencia, o None si es inocua: DROP,
    y DELETE o UPDATE sin WHERE."""
    t = _sin_literales(sentencia).lower().replace("(", " ").replace(")", " ").replace(",", " ").split()
    if not t:
        return None
    if t[0] == "drop":
        return "DROP borra para siempre una tabla, un índice o una vista."
    if t[0] == "delete" and "where" not in t:
        return "DELETE sin WHERE borra TODAS las filas de la tabla."
    if t[0] == "update" and "where" not in t:
        return "UPDATE sin WHERE cambia TODAS las filas de la tabla."
    return None


