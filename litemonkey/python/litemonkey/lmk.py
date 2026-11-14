# SPDX-License-Identifier: GPL-2.0-or-later
# lmk.py - consola interactiva de lite-monkey (como la de MongoDB, pero en
# Python). Funciona en CPython (orden lmk tras pip install -e) y en SaguiOS
# (/bin/lmk).
#
#   lmk [host[:puerto]] [--usuario U --clave C] [--base NOMBRE] [--eval EXPRESION]...
#
# Dentro: "use tienda", "show dbs", "show collections", "it" (más
# resultados), "help" y expresiones de Python con la API del cliente:
#   db.productos.insert_one({"nombre": "té", "precio": 3.5})
#   db.productos.find({"precio": {"$lt": 10}}).sort("precio")
# true, false y null valen como en JSON; también insertOne, findOne...
import sys

from .cliente import Cliente, Cursor, CommandCursor, InsertOneResult, InsertManyResult, UpdateResult, DeleteResult
from .bson import ObjectId, SON, Regex, Fecha, Binary
from . import errors

try:
    import datetime as _dt
except ImportError:
    _dt = None

POR_PAGINA = 20

AYUDA = """Órdenes de lmk:
  show dbs | show collections     bases y colecciones
  use BASE                        cambia de base (db)
  it                              más resultados del último find
  exit | quit                     salir (o Ctrl+D)
Expresiones (Python, con true/false/null):
  db.col.insert_one({...})          db.col.insert_many([{...}, ...])
  db.col.find({filtro}, {proyeccion}).sort("campo", -1).limit(5)
  db.col.find_one({...})            db.col.count_documents({...})
  db.col.update_one({filtro}, {"$set": {...}})   update_many, replace_one
  db.col.delete_one({...})          delete_many
  db.col.aggregate([{"$group": {"_id": "$campo", "n": {"$sum": 1}}}])
  db.col.create_index("campo")      db.col.distinct("campo")   db.col.drop()
  db.col.stats()   db.stats()       cliente.server_status()"""


def _cadena(s):
    r = '"'
    for ch in s:
        if ch == '"' or ch == "\\":
            r += "\\" + ch
        elif ch == "\n":
            r += "\\n"
        elif ord(ch) < 32:
            r += "\\u%04x" % ord(ch)
        else:
            r += ch
    return r + '"'


def texto(v, sangria=0):
    """Un valor como texto al estilo de la consola de MongoDB."""
    if v is None:
        return "null"
    if v is True:
        return "true"
    if v is False:
        return "false"
    if isinstance(v, str):
        return _cadena(v)
    if isinstance(v, ObjectId):
        return 'ObjectId("%s")' % v
    if _dt and isinstance(v, _dt.datetime):
        return 'ISODate("%s")' % v.isoformat()
    if isinstance(v, Fecha):
        return repr(v)
    if isinstance(v, (bytes, bytearray)):
        return "BinData(%d bytes)" % len(v)
    if isinstance(v, (dict, SON)):
        if not v:
            return "{}"
        partes = ["%s: %s" % (_cadena(k) if not _simple(k) else k, texto(x, sangria + 2)) for k, x in v.items()]
        linea = "{ " + ", ".join(partes) + " }"
        if len(linea) + sangria <= 100:
            return linea
        esp = " " * (sangria + 2)
        return "{\n" + ",\n".join(esp + p for p in partes) + "\n" + " " * sangria + "}"
    if isinstance(v, (list, tuple)):
        partes = [texto(x, sangria + 2) for x in v]
        linea = "[ " + ", ".join(partes) + " ]" if partes else "[]"
        if len(linea) + sangria <= 100:
            return linea
        esp = " " * (sangria + 2)
        return "[\n" + ",\n".join(esp + p for p in partes) + "\n" + " " * sangria + "]"
    return repr(v)


def _simple(k):
    if not k or not (k[0].isalpha() or k[0] == "_"):
        return False
    for ch in k:
        if not (ch.isalpha() or ch.isdigit() or ch == "_"):
            return False
    return True


class Consola:
    def __init__(self, cliente, base):
        self.cliente = cliente
        self.cursor = None
        self.por_pagina = POR_PAGINA
        self.ns = {"cliente": cliente, "client": cliente, "ObjectId": ObjectId, "SON": SON, "Regex": Regex,
                   "Fecha": Fecha, "Binary": Binary, "true": True, "false": False, "null": None,
                   "ASCENDING": 1, "DESCENDING": -1}
        self.usar(base)

    def usar(self, base):
        self.base = base
        self.ns["db"] = self.cliente[base]
        return "base actual: %s" % base

    def mostrar_cursor(self):
        n = 0
        while n < self.por_pagina:
            try:
                d = next(self.cursor)
            except StopIteration:
                self.cursor = None
                break
            print(texto(d))
            n += 1
        if self.cursor is not None:
            print('(escribe "it" para ver más)')
        elif n == 0:
            print("(sin resultados)")

    def ejecutar(self, linea):
        linea = linea.strip()
        if not linea:
            return
        p = linea.split()
        if p[0] in ("exit", "quit", "salir"):
            raise EOFError
        if p[0] in ("help", "ayuda", "?"):
            print(AYUDA)
            return
        if p[0] == "use" and len(p) == 2:
            print(self.usar(p[1]))
            return
        if p[0] == "show" and len(p) == 2:
            if p[1] in ("dbs", "databases"):
                for d in self.cliente.list_databases():
                    print("%-20s %10d bytes" % (d["name"], d["sizeOnDisk"]))
            elif p[1] in ("collections", "tables"):
                for c in sorted(self.ns["db"].list_collection_names()):
                    print(c)
            else:
                print("show dbs | show collections")
            return
        if linea == "it":
            if self.cursor is None:
                print("no hay más resultados")
            else:
                self.mostrar_cursor()
            return
        try:
            r = eval(linea, self.ns)
        except SyntaxError:
            exec(linea, self.ns)
            return
        self.ns["_"] = r
        if isinstance(r, (Cursor, CommandCursor)):
            self.cursor = r
            self.mostrar_cursor()
        elif isinstance(r, (InsertOneResult, InsertManyResult, UpdateResult, DeleteResult)):
            print(repr(r))
        elif r is not None:
            print(texto(r))

    def bucle(self):
        print("lmk: conectado a %s:%d (%s). help = ayuda; exit = salir" %
              (self.cliente.host, self.cliente.port, self.cliente.server_info_cache.get("version", "?")))
        while True:
            try:
                linea = input("lmk:%s> " % self.base)
            except (EOFError, KeyboardInterrupt):
                print()
                return
            try:
                self.ejecutar(linea)
            except EOFError:
                return
            except errors.OperationFailure as e:
                print("error del servidor:", e)
            except Exception as e:  # noqa: BLE001
                print("error: %s: %s" % (type(e).__name__, e))


def principal(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    host, usuario, clave, base, evals = "127.0.0.1", None, None, "test", []
    i = 0
    while i < len(argv):
        a = argv[i]
        v = argv[i + 1] if i + 1 < len(argv) else None
        if a in ("-h", "--help", "--ayuda"):
            print("uso: lmk [host[:puerto]] [--usuario U --clave C] [--base NOMBRE] [--eval EXPRESION]...")
            return 0
        if a in ("-u", "--usuario") and v:
            usuario = v
            i += 1
        elif a in ("-p", "--clave") and v:
            clave = v
            i += 1
        elif a in ("-b", "--base") and v:
            base = v
            i += 1
        elif a in ("-e", "--eval") and v:
            evals.append(v)
            i += 1
        elif not a.startswith("-"):
            host = a
        else:
            print("lmk: opción no válida: %s" % a)
            return 2
        i += 1
    try:
        cl = Cliente(host, usuario=usuario, clave=clave)
    except errors.LiteMonkeyError as e:
        print("lmk: %s" % e)
        return 1
    c = Consola(cl, base)
    if evals:
        codigo = 0
        for e in evals:
            try:
                c.por_pagina = 1 << 30
                c.ejecutar(e)
            except errors.OperationFailure as ex:
                print("error del servidor:", ex)
                codigo = 1
            except Exception as ex:  # noqa: BLE001
                print("error: %s: %s" % (type(ex).__name__, ex))
                codigo = 1
        cl.close()
        return codigo
    c.bucle()
    cl.close()
    return 0


if __name__ == "__main__":
    sys.exit(principal())
