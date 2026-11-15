#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Pruebas de unidad de la biblioteca `basedatos` (mejora 127, D304) en el anfitrión, con
CPython, `sqlite3` y el servidor `litemonkey` del PC (make -C litemonkey).

    python3 tests/basedatos_unidad.py
"""
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True      # no dejar __pycache__ dentro de rootfs/ (rompe el ramdisk)
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "rootfs", "lib", "python"))
sys.path.insert(0, os.path.join(RAIZ, "litemonkey", "python"))
import basedatos as B  # noqa: E402

total = [0, 0]


def comprobar(cond, msg):
    total[0] += 1
    if cond:
        print("ok   " + msg)
    else:
        total[1] += 1
        print("FALLO " + msg)


def lanza(exc, f, *a, **kw):
    try:
        f(*a, **kw)
    except exc:
        return True
    except Exception as e:
        print("   (otra excepción: %r)" % (e,))
        return False
    return False


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ---------------------------------------------------------------- analizar
comprobar(B.analizar("sqlite:/home/datos.db") == ("sqlite", "/home/datos.db"), "analizar sqlite:/ruta")
comprobar(B.analizar("sqlite:///tmp/x.db") == ("sqlite", "/tmp/x.db"), "analizar sqlite:///ruta")
comprobar(B.analizar("/tmp/y.sqlite") == ("sqlite", "/tmp/y.sqlite"), "una ruta .sqlite suelta")
t, p = B.analizar("litemonkey://u:c@10.0.2.2:7000/mibase")
comprobar(t == "litemonkey" and p == ("10.0.2.2", 7000, "mibase", "u", "c"), "analizar litemonkey con usuario")
comprobar(B.analizar("litemonkey://127.0.0.1")[1][:3] == ("127.0.0.1", 7979, "test"), "litemonkey: puerto y base por defecto")
comprobar(lanza(B.ErrorConexion, B.analizar, "mysql://x"), "origen desconocido: ErrorConexion")

# ---------------------------------------------------------------- SQLite
tmp = tempfile.mkdtemp()
ruta = os.path.join(tmp, "datos.db")
bd = B.conectar("sqlite:" + ruta)
comprobar(bd.tipo == "sqlite" and bd.tablas() == [], "sqlite: base nueva sin tablas")
bd.ejecutar("CREATE TABLE contactos (id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, edad INTEGER, saldo REAL)")
bd.ejecutar("CREATE TABLE sin_clave (a TEXT, b INTEGER)")
bd.ejecutar("CREATE INDEX i_nombre ON contactos(nombre)")
comprobar(bd.tablas() == ["contactos", "sin_clave"], "sqlite: tablas()")
es = bd.esquema("contactos")
comprobar([c["nombre"] for c in es] == ["id", "nombre", "edad", "saldo"], "sqlite: esquema, nombres")
comprobar(es[0]["clave"] and es[1]["obligatorio"] and es[2]["tipo"] == "INTEGER", "sqlite: esquema, clave/obligatorio/tipo")
comprobar(lanza(B.ErrorConsulta, bd.esquema, "nope"), "sqlite: esquema de una tabla inexistente da ErrorConsulta")
comprobar(bd.indices("contactos") == [{"nombre": "i_nombre", "unico": False, "columnas": ["nombre"]}], "sqlite: indices()")
comprobar(bd.clave("contactos") == "id" and bd.clave("sin_clave") == "rowid", "sqlite: clave primaria o rowid")

nombres = ["Ana", "Beto", "Carla", "Dani", "Eva", "O'Brien", 'Zoe "Z"']
for i, n in enumerate(nombres):
    bd.insertar("contactos", {"nombre": n, "edad": 20 + i * 5, "saldo": i * 1.5})
comprobar(bd.contar("contactos") == 7, "sqlite: insertar y contar")
f = bd.consultar("contactos", {"nombre": "O'Brien"})
comprobar(len(f) == 1 and f[0]["edad"] == 45, "sqlite: una comilla simple en el valor no rompe nada")
f = bd.consultar("contactos", {"nombre": 'Zoe "Z"'})
comprobar(len(f) == 1, "sqlite: comillas dobles en el valor")
comprobar(bd.contar("contactos", {"edad": {"$gte": 30, "$lt": 45}}) == 3, "sqlite: filtro con operadores")
comprobar(bd.contar("contactos", {"nombre": {"$in": ["Ana", "Eva", "Nadie"]}}) == 2, "sqlite: $in")
comprobar(bd.contar("contactos", {"$or": [{"nombre": "Ana"}, {"edad": {"$gt": 45}}]}) == 2, "sqlite: $or")
comprobar(bd.contar("contactos", {"nombre": {"$contiene": "AR"}}) == 1, "sqlite: $contiene (sin distinguir mayúsculas)")
comprobar(bd.contar("contactos", ("edad > ? AND nombre LIKE ?", [20, "%a"])) == 2, "sqlite: filtro como (SQL, parámetros)")
comprobar(bd.contar("contactos", "edad >= 40") == 3, "sqlite: filtro como texto SQL")
comprobar([x["nombre"] for x in bd.consultar("contactos", orden="-edad", limite=2)] == ['Zoe "Z"', "O'Brien"], "sqlite: orden descendente y límite")
comprobar([x["nombre"] for x in bd.consultar("contactos", orden=["nombre"], limite=2, desplazamiento=2)] == ["Carla", "Dani"], "sqlite: desplazamiento (paginación)")
comprobar([x["nombre"] for x in bd.consultar("contactos", orden=[("edad", -1)], limite=1)] == ['Zoe "Z"'], "sqlite: orden como pares")
comprobar(bd.consultar("contactos", {"nombre": "Ana"})[0]["id"] == 1, "sqlite: la fila lleva su clave")

# nombre de campo hostil: se entrecomilla, no ejecuta nada
hostil = 'nombre"; DROP TABLE contactos; --'
comprobar(bd.consultar("contactos", {hostil: 1}) == [], "sqlite: un nombre de campo hostil no encuentra nada ni ejecuta nada")
comprobar("contactos" in bd.tablas(), "sqlite: la tabla sigue ahí tras el nombre hostil")
# valor hostil: un parámetro, nunca SQL
bd.insertar("contactos", {"nombre": "x'); DROP TABLE contactos; --", "edad": 1})
comprobar("contactos" in bd.tablas() and bd.contar("contactos") == 8, "sqlite: un valor hostil es solo un dato")
bd.borrar("contactos", {"edad": 1})

n = bd.actualizar("contactos", {"edad": 99, "saldo": None}, {"nombre": "Ana"})
comprobar(n == 1 and bd.consultar("contactos", {"id": 1})[0]["edad"] == 99, "sqlite: actualizar con filtro")
comprobar(bd.consultar("contactos", {"id": 1})[0]["saldo"] is None, "sqlite: None se guarda como NULL")
comprobar(bd.consultar("contactos", {"saldo": None})[0]["id"] == 1, "sqlite: filtro con None es IS NULL")
n = bd.actualizar("contactos", {"edad": 50}, clave=2)
comprobar(n == 1 and bd.consultar("contactos", {"id": 2})[0]["edad"] == 50, "sqlite: actualizar por clave")
comprobar(lanza(B.ErrorConsulta, bd.actualizar, "contactos", {"edad": 1}), "sqlite: actualizar sin filtro ni clave se niega")
comprobar(lanza(B.ErrorConsulta, bd.borrar, "contactos"), "sqlite: borrar sin filtro ni clave se niega")
comprobar(bd.borrar("contactos", clave=3) == 1 and bd.contar("contactos") == 6, "sqlite: borrar por clave")
comprobar(bd.borrar("contactos", {"edad": {"$gt": 60}}) == 1, "sqlite: borrar con filtro")
comprobar(lanza(B.ErrorIntegridad, bd.insertar, "contactos", {"edad": 5}), "sqlite: NOT NULL incumplido da ErrorIntegridad")
comprobar(not bd._con.in_transaction, "sqlite: un error no deja abierta una transacción implícita")
comprobar(lanza(B.ErrorIntegridad, bd.insertar, "contactos", {"id": 2, "nombre": "otra"}), "sqlite: clave repetida da ErrorIntegridad")

bd.insertar("sin_clave", {"a": "uno", "b": 1})
bd.insertar("sin_clave", {"a": "dos", "b": 2})
f = bd.consultar("sin_clave", orden="b")
comprobar([x["rowid"] for x in f] == [1, 2] and bd.columnas("sin_clave") == ["rowid", "a", "b"], "sqlite: sin clave primaria usa rowid")
bd.actualizar("sin_clave", {"a": "DOS"}, clave=2)
comprobar(bd.consultar("sin_clave", {"b": 2})[0]["a"] == "DOS", "sqlite: actualizar por rowid")

r = bd.ejecutar("SELECT nombre FROM contactos WHERE edad > ? ORDER BY edad", [30])
comprobar(isinstance(r, list) and "nombre" in r[0], "sqlite: ejecutar un SELECT con parámetros")
r = bd.ejecutar("SELECT nombre FROM contactos WHERE edad > :e AND nombre <> :n", {"e": 0, "n": "Ana"})
comprobar(len(r) >= 1, "sqlite: parámetros con nombre")
comprobar(bd.ejecutar("UPDATE sin_clave SET b = b + ?", [10]) == 2, "sqlite: ejecutar un UPDATE devuelve las filas afectadas")
comprobar(lanza(B.ErrorConsulta, bd.ejecutar, "SELEC 1"), "sqlite: SQL mal escrito da ErrorConsulta")
cols, filas = bd.consulta_sql("SELECT nombre, edad FROM contactos ORDER BY id")
comprobar(cols == ["nombre", "edad"] and isinstance(filas[0], list), "sqlite: consulta_sql da columnas en orden y filas como listas")

# transacciones
antes = bd.contar("contactos")
try:
    with bd.transaccion():
        bd.insertar("contactos", {"nombre": "T1"})
        bd.insertar("contactos", {"nombre": "T2"})
        comprobar(bd.en_transaccion, "sqlite: dentro de la transacción")
        raise RuntimeError("falla")
except RuntimeError:
    pass
comprobar(bd.contar("contactos") == antes and not bd.en_transaccion, "sqlite: la transacción con error se deshace")
with bd.transaccion():
    bd.insertar("contactos", {"nombre": "T3"})
comprobar(bd.contar("contactos") == antes + 1, "sqlite: la transacción buena se confirma")
bd.cerrar()
bd2 = B.conectar(ruta)
comprobar(bd2.contar("contactos") == antes + 1, "sqlite: los cambios están en el fichero (se reabre)")
bd2.empezar()
bd2.insertar("contactos", {"nombre": "T4"})
bd2.revertir()
comprobar(bd2.contar("contactos") == antes + 1, "sqlite: empezar/revertir")
bd2.crear_tabla("nueva", [("id", "INTEGER", True), ("texto", "TEXT")])
comprobar("nueva" in bd2.tablas() and bd2.clave("nueva") == "id", "sqlite: crear_tabla")
comprobar(lanza(B.ErrorConsulta, bd2.crear_tabla, "mala", [("a", "NOSE")]), "sqlite: tipo de columna no válido")
bd2.borrar_tabla("nueva")
comprobar("nueva" not in bd2.tablas(), "sqlite: borrar_tabla")
bd2.cerrar()

# un fichero que no es una base
malo = os.path.join(tmp, "malo.db")
with open(malo, "w") as fh:
    fh.write("esto no es una base de datos de SQLite, ni de lejos" * 5)
comprobar(lanza(B.ErrorConexion, B.conectar, "sqlite:" + malo), "sqlite: un fichero que no es una base da ErrorConexion")

bd3 = B.conectar(ruta)
cols, filas, n = bd3.sql("SELECT nombre, edad FROM contactos ORDER BY id LIMIT 2")
comprobar(cols == ["nombre", "edad"] and len(filas) == 2 and n == 2, "sqlite: sql() de un SELECT")
cols, filas, n = bd3.sql("UPDATE contactos SET edad = edad + 1 WHERE nombre = ?", ["Beto"])
comprobar(cols == [] and n == 1, "sqlite: sql() de un UPDATE da las filas afectadas")
bd3.cerrar()
comprobar(B.conectar(ruta).consultar("contactos", {"nombre": "Beto"})[0]["edad"] == 51, "sqlite: sql() confirma lo que escribe")

# análisis de SQL del Explorador
from basedatos import _seguridad as SG  # noqa: E402
comprobar(SG.sentencias("select 1; delete from t; ") == ["select 1", "delete from t"], "seguridad: separa sentencias")
comprobar(SG.sentencias("select 'a;b'; -- c ; d\nselect 2") == ["select 'a;b'", "-- c ; d\nselect 2"], "seguridad: ; en cadenas y comentarios")
comprobar(SG.peligrosa("DROP TABLE x") and SG.peligrosa("delete from t") and SG.peligrosa("UPDATE t SET a = 1"), "seguridad: DROP, DELETE y UPDATE sin WHERE")
comprobar(SG.peligrosa("delete from t where a = 1") is None and SG.peligrosa("select 'drop' from t") is None
          and SG.peligrosa("update t set a = 'where'") is not None, "seguridad: con WHERE y textos que engañan")

# conversiones de celdas
V = B.valor_desde_texto
comprobar(V("12", "INTEGER") == 12 and V("1.5", "REAL") == 1.5 and V("", "INTEGER") is None, "valor_desde_texto: tipos SQLite")
comprobar(V("hola", "TEXT") == "hola" and V("", "TEXT") == "", "valor_desde_texto: texto")
comprobar(V("7") == 7 and V("2.5") == 2.5 and V("true") is True and V("null") is None and V("ab") == "ab", "valor_desde_texto: adivinar")
comprobar(lanza(B.ErrorConsulta, V, "abc", "INTEGER"), "valor_desde_texto: un número mal escrito da error")
comprobar(B.a_csv(["a", "b"], [{"a": 1, "b": 'x,"y"'}, [2, None]]) == 'a,b\n1,"x,""y"""\n2,\n', "a_csv: comillas y vacíos")

# ---------------------------------------------------------------- lite-monkey
servidor = os.path.join(RAIZ, "litemonkey", "build", "litemonkey")
if not os.path.exists(servidor):
    print("(falta %s: make -C litemonkey; se omite lite-monkey)" % servidor)
else:
    datos = os.path.join(tmp, "lmk")
    os.mkdir(datos)
    puerto = puerto_libre()
    srv = subprocess.Popen([servidor, "--puerto", str(puerto), "--datos", datos, "--escuchar", "127.0.0.1"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", puerto), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)
        lm = B.conectar("litemonkey://127.0.0.1:%d/prueba" % puerto)
        comprobar(lm.tipo == "litemonkey" and lm.tablas() == [], "lmk: base vacía")
        for i, n in enumerate(nombres):
            lm.insertar("contactos", {"nombre": n, "edad": 20 + i * 5, "activo": i % 2 == 0})
        lm.insertar("contactos", {"nombre": "Sin edad", "notas": ["a", "b"]})
        comprobar(lm.tablas() == ["contactos"] and lm.colecciones() == ["contactos"], "lmk: tablas() y colecciones()")
        comprobar(lm.contar("contactos") == 8, "lmk: insertar y contar")
        es = lm.esquema("contactos")
        nom = [c["nombre"] for c in es]
        comprobar(nom[0] == "_id" and set(nom) == {"_id", "nombre", "edad", "activo", "notas"}, "lmk: esquema deducido de una muestra")
        d = dict((c["nombre"], c) for c in es)
        comprobar(d["edad"]["tipo"] == "entero" and d["activo"]["tipo"] == "logico" and d["notas"]["tipo"] == "lista", "lmk: tipos deducidos")
        comprobar(d["edad"]["frecuencia"] == 87 and d["_id"]["clave"], "lmk: frecuencia del campo y clave")
        comprobar(lm.clave("contactos") == "_id", "lmk: la clave es _id")
        comprobar(lm.contar("contactos", {"edad": {"$gte": 30, "$lt": 45}}) == 3, "lmk: filtro con operadores")
        comprobar(lm.contar("contactos", '{"edad": {"$gt": 40}}') == 2, "lmk: filtro como JSON")
        comprobar(lanza(B.ErrorConsulta, lm.contar, "contactos", "{mal"), "lmk: JSON mal escrito da ErrorConsulta")
        comprobar(lm.contar("contactos", {"nombre": {"$contiene": "AR"}}) == 1, "lmk: $contiene")
        f = lm.consultar("contactos", {"edad": {"$exists": True}}, orden="-edad", limite=2)
        comprobar([x["nombre"] for x in f] == ['Zoe "Z"', "O'Brien"], "lmk: orden descendente y límite")
        f = lm.consultar("contactos", {"edad": {"$exists": True}}, orden="nombre", limite=2, desplazamiento=2)
        comprobar([x["nombre"] for x in f] == ["Carla", "Dani"], "lmk: desplazamiento")
        oid = lm.consultar("contactos", {"nombre": "Ana"})[0]["_id"]
        comprobar(lm.consultar("contactos", {"_id": str(oid)})[0]["nombre"] == "Ana", "lmk: _id como 24 cifras hexadecimales")
        n = lm.actualizar("contactos", {"edad": 99}, clave=oid)
        comprobar(n == 1 and lm.consultar("contactos", {"nombre": "Ana"})[0]["edad"] == 99, "lmk: actualizar por clave")
        n = lm.actualizar("contactos", {"edad": 1}, {"nombre": "Eva"})
        comprobar(n == 1, "lmk: actualizar con filtro")
        comprobar(lanza(B.ErrorConsulta, lm.actualizar, "contactos", {"edad": 1}), "lmk: actualizar sin filtro se niega")
        comprobar(lanza(B.ErrorConsulta, lm.borrar, "contactos"), "lmk: borrar sin filtro se niega")
        comprobar(lm.borrar("contactos", clave=str(oid)) == 1 and lm.contar("contactos") == 7, "lmk: borrar por clave (texto)")
        comprobar(lm.borrar("contactos", {"edad": 1}) == 1, "lmk: borrar con filtro")
        lm.insertar("contactos", {"_id": "fijo", "nombre": "A"})
        comprobar(lanza(B.ErrorIntegridad, lm.insertar, "contactos", {"_id": "fijo"}), "lmk: _id repetido da ErrorIntegridad")
        comprobar(lanza(B.ErrorBD, lm.ejecutar, "SELECT 1"), "lmk: ejecutar() no existe")
        comprobar(lanza(B.ErrorBD, lm.empezar), "lmk: sin transacciones")
        lm._col("contactos").create_index("edad")
        comprobar(any(i["columnas"] == ["edad"] for i in lm.indices("contactos")), "lmk: indices()")
        lm.crear_tabla("otra")
        comprobar("otra" in lm.tablas(), "lmk: crear_tabla")
        lm.borrar_tabla("otra")
        comprobar("otra" not in lm.tablas(), "lmk: borrar_tabla")
        comprobar("prueba" in lm.bases(), "lmk: bases()")
        lm.cerrar()
        comprobar(lanza(B.ErrorConexion, B.conectar, "litemonkey://127.0.0.1:%d/x" % puerto_libre()), "lmk: sin servidor da ErrorConexion")
    finally:
        srv.terminate()
        srv.wait()

shutil.rmtree(tmp, ignore_errors=True)
print("basedatos_unidad: %d comprobaciones, %d fallos" % (total[0], total[1]))
sys.exit(1 if total[1] else 0)
