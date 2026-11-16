# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._muestreo - el esquema por muestreo de una colección documental (D333): por campo, los tipos con
# su porcentaje, cuántos documentos lo tienen (y cuántos le faltan: los «huecos») y sus valores más
# frecuentes. Sin tkinter: se prueba en el anfitrión.


def _tipo(v):
    if v is None:
        return "nulo"
    if isinstance(v, bool):
        return "logico"
    if isinstance(v, int):
        return "entero"
    if isinstance(v, float):
        return "real"
    if isinstance(v, str):
        return "texto"
    if isinstance(v, (list, tuple)):
        return "lista"
    if isinstance(v, dict):
        return "documento"
    if isinstance(v, (bytes, bytearray)):
        return "binario"
    n = type(v).__name__
    if n == "ObjectId":
        return "objectid"
    if n in ("Fecha", "datetime"):
        return "fecha"
    return n.lower()


def _recorrer(doc, prefijo, salida, nivel):
    for k, v in doc.items():
        nombre = prefijo + k
        salida.append((nombre, v))
        if isinstance(v, dict) and nivel < 2:
            _recorrer(v, nombre + ".", salida, nivel + 1)


def muestrear(docs, frecuentes=3):
    """[{"campo", "tipos": [(tipo, porcentaje)], "presente": n, "ausente": n, "pct": porcentaje de documentos
    que lo tienen, "frecuentes": [(texto, veces)]}] con el _id primero y el resto en el orden en que aparecen."""
    total = len(docs)
    orden, info = [], {}
    for d in docs:
        pares = []
        _recorrer(d, "", pares, 0)
        for nombre, v in pares:
            r = info.get(nombre)
            if r is None:
                r = info[nombre] = {"tipos": {}, "n": 0, "vals": {}}
                orden.append(nombre)
            t = _tipo(v)
            r["tipos"][t] = r["tipos"].get(t, 0) + 1
            r["n"] += 1
            if t in ("texto", "entero", "real", "logico", "nulo", "objectid"):
                txt = "null" if v is None else (("true" if v else "false") if isinstance(v, bool) else str(v))
                if len(txt) > 24:
                    txt = txt[:23] + "~"
                if len(r["vals"]) < 400 or txt in r["vals"]:
                    r["vals"][txt] = r["vals"].get(txt, 0) + 1
    if "_id" in info:
        orden.remove("_id")
        orden.insert(0, "_id")
    res = []
    for nombre in orden:
        r = info[nombre]
        tipos = sorted(r["tipos"].items(), key=lambda p: (-p[1], p[0]))
        vals = sorted(r["vals"].items(), key=lambda p: (-p[1], p[0]))
        # un valor que sale una sola vez en una colección de muchos documentos no es «frecuente»
        vals = [p for p in vals if p[1] > 1 or total <= 3][:frecuentes]
        res.append({"campo": nombre, "tipos": [(t, c * 100 // max(1, r["n"])) for t, c in tipos],
                    "presente": r["n"], "ausente": total - r["n"], "pct": r["n"] * 100 // max(1, total),
                    "frecuentes": vals})
    return res
