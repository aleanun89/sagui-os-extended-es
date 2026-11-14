# SPDX-License-Identifier: GPL-2.0-or-later
# casos_cliente.py - casos del cliente litemonkey contra un servidor vivo.
#
# El mismo archivo se ejecuta con CPython en el PC (testes/teste_cliente.py)
# y con el MicroPython de SaguiOS (tests/h88_litemonkey.py): así se
# comprueba que el cliente se comporta igual en los dos.
#   python casos_cliente.py [host] [puerto] [base]
# Cada comprobación escribe "ok: ..." o "FALLO: ..."; al final, el resumen.
import sys
import litemonkey
from litemonkey import ObjectId, SON, errors

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
puerto = int(sys.argv[2]) if len(sys.argv) > 2 else 7979
nombre_base = sys.argv[3] if len(sys.argv) > 3 else "casos"
fallos = 0
bien = 0


def ver(cond, msg):
    global fallos, bien
    if cond:
        bien += 1
        print("ok:", msg)
    else:
        fallos += 1
        print("FALLO:", msg)


def lanza(f, clase):
    try:
        f()
    except clase as e:
        return e
    return None


c = litemonkey.Cliente(host, puerto)
ver(c.server_info()["version"] == "litemonkey 1", "hello: versión litemonkey 1")
c.drop_database(nombre_base)
db = c[nombre_base]
gente = db.gente

# ---- inserción y búsqueda por _id
r = gente.insert_one({"nombre": "Ana", "edad": 30, "ciudad": "Lima", "tags": ["a", "b"]})
ver(isinstance(r.inserted_id, ObjectId), "insert_one devuelve un ObjectId")
ver(gente.find_one({"_id": r.inserted_id})["nombre"] == "Ana", "find_one por _id")
ver(gente.find_one(r.inserted_id)["edad"] == 30, "find_one(ObjectId)")
docs = []
for i in range(300):
    docs.append({"_id": i, "n": i, "par": i % 2 == 0, "nombre": "persona%03d" % i,
                 "dir": {"ciudad": ["Madrid", "Lima", "Quito"][i % 3], "cp": 28000 + i},
                 "notas": [i % 10, (i * 7) % 10]})
r = gente.insert_many(docs)
ver(len(r.inserted_ids) == 300 and r.inserted_ids[5] == 5, "insert_many: 300 _id")
ver(gente.count_documents({}) == 301, "count_documents total")
ver(gente.estimated_document_count() == 301, "estimated_document_count")

# ---- consultas
ver(gente.count_documents({"n": {"$gte": 100, "$lt": 150}}) == 50, "$gte y $lt")
ver(gente.count_documents({"dir.ciudad": "Quito"}) == 100, "campo anidado con punto")
ver(gente.count_documents({"notas": 3}) == 60, "comparación con arrays (algún elemento)")
ver(gente.count_documents({"n": {"$in": [1, 2, 3, 999]}}) == 3, "$in")
ver(gente.count_documents({"n": {"$nin": [1, 2, 3]}, "par": True}) == 149, "$nin")
ver(gente.count_documents({"edad": {"$exists": True}}) == 1, "$exists")
ver(gente.count_documents({"$or": [{"n": 5}, {"nombre": "Ana"}]}) == 2, "$or")
ver(gente.count_documents({"nombre": {"$regex": "^persona0[0-4]"}}) == 50, "$regex con clase y ^")
ver(gente.count_documents({"nombre": {"$regex": "PERSONA29.", "$options": "i"}}) == 10, "$regex con opción i")
ver(gente.count_documents({"n": {"$not": {"$lt": 290}}}) == 11, "$not (incluye los que no tienen n)")
ver(gente.count_documents({"n": {"$ne": 7}, "par": False}) == 149, "$ne")
ver([d["n"] for d in gente.find({"par": True}, {"n": 1, "_id": 0}).sort("n", -1).skip(2).limit(3)] == [294, 292, 290],
    "find con sort descendente, skip y limit")
d = gente.find_one({"n": 10}, {"dir.ciudad": 1, "_id": 0})
ver(d == {"dir": {"ciudad": "Lima"}}, "proyección de un campo anidado: %r" % (d,))
d = gente.find_one({"n": 10}, {"notas": 0, "dir": 0, "nombre": 0})
ver(sorted(d.keys()) == ["_id", "n", "par"], "proyección de exclusión")
orden = [(d["dir"]["ciudad"], d["n"]) for d in gente.find({"n": {"$lt": 6}}).sort([("dir.ciudad", 1), ("n", -1)])]
ver(orden == [("Lima", 4), ("Lima", 1), ("Madrid", 3), ("Madrid", 0), ("Quito", 5), ("Quito", 2)], "sort por dos campos")

# ---- cursores por lotes
cur = gente.find({"n": {"$exists": True}}).batch_size(7)
vistos = [d["n"] for d in cur]
ver(len(vistos) == 300 and len(set(vistos)) == 300, "cursor con batch_size 7: getMore hasta el final")
cur = gente.find().batch_size(5)
primero = next(cur)
cur.close()
ver(primero is not None and not cur.alive, "cerrar un cursor a medias (killCursors)")

# ---- actualizaciones
r = gente.update_one({"n": 1}, {"$set": {"nombre": "uno", "extra.nivel": 2}, "$inc": {"n": 1000}})
ver(r.matched_count == 1 and r.modified_count == 1, "update_one: $set y $inc")
ver(gente.find_one({"n": 1001})["extra"] == {"nivel": 2}, "$set crea el subdocumento")
r = gente.update_many({"par": True}, {"$push": {"notas": 99}})
ver(r.matched_count == 150 and r.modified_count == 150, "update_many con $push (150)")
ver(gente.count_documents({"notas": 99}) == 150, "$push visible")
r = gente.update_many({"par": True}, {"$pull": {"notas": 99}, "$unset": {"dir.cp": ""}})
ver(r.modified_count == 150 and gente.count_documents({"notas": 99}) == 0, "$pull y $unset")
ver(gente.count_documents({"dir.cp": {"$exists": False}}) == 151, "$unset de un campo anidado")
r = gente.update_one({"n": 2}, {"$rename": {"nombre": "nom"}})
ver(gente.find_one({"n": 2}).get("nom") == "persona002", "$rename")
r = gente.update_one({"n": 2}, {"$set": {"nom": "persona002"}})
ver(r.matched_count == 1 and r.modified_count == 0, "$set al mismo valor: modified_count 0")
r = gente.update_one({"nombre": "nuevo"}, {"$set": {"edad": 5}}, upsert=True)
ver(isinstance(r.upserted_id, ObjectId) and gente.find_one({"nombre": "nuevo"})["edad"] == 5, "upsert crea el documento")
r = gente.replace_one({"n": 3}, {"n": 3, "reemplazado": True})
d = gente.find_one({"n": 3})
ver(r.modified_count == 1 and d["_id"] == 3 and d.get("reemplazado") and "nombre" not in d, "replace_one conserva el _id")
ver(lanza(lambda: gente.update_one({"n": 3}, {"n": 4}), ValueError) is not None, "update_one sin operadores: ValueError")
e = lanza(lambda: gente.update_one({"n": 3}, {"$set": {"_id": 7}}), errors.OperationFailure)
ver(e is not None and e.code == 66, "cambiar el _id: error 66")
e = lanza(lambda: gente.update_one({"n": 3}, {"$foo": {"a": 1}}), errors.OperationFailure)
ver(e is not None and e.code == 2, "operador desconocido: error 2")

# ---- borrado
r = gente.delete_one({"par": False})
ver(r.deleted_count == 1, "delete_one")
r = gente.delete_many({"n": {"$gte": 200, "$lt": 1000}})
ver(r.deleted_count == 100, "delete_many (100)")
ver(gente.find_one({"n": 250}) is None, "find_one de un borrado: None")

# ---- errores de clave
e = lanza(lambda: gente.insert_one({"_id": 5, "x": 1}), errors.DuplicateKeyError)
ver(e is not None and e.code == 11000, "DuplicateKeyError en _id")
e = lanza(lambda: gente.insert_many([{"_id": 9001}, {"_id": 5}, {"_id": 9002}]), errors.BulkWriteError)
ver(e is not None and e.details["nInserted"] == 1, "insert_many ordenado se para en el duplicado")
ver(gente.count_documents({"_id": {"$in": [9001, 9002]}}) == 1, "solo entró el primero")

# ---- índices
ix = gente.create_index("dir.ciudad")
ver(ix == "dir.ciudad_1", "create_index devuelve el nombre")
plan = gente.find({"dir.ciudad": "Lima"}).explain()
ver(plan["queryPlanner"]["winningPlan"]["stage"] == "IXSCAN", "explain: IXSCAN con el índice")
ver(gente.find({"edad": 5}).explain()["queryPlanner"]["winningPlan"]["stage"] == "COLLSCAN", "explain: COLLSCAN sin índice")
ver(gente.find({"_id": 7}).explain()["queryPlanner"]["winningPlan"]["stage"] == "IDHACK", "explain: _id por la tabla hash")
gente.create_index([("n", litemonkey.DESCENDING)])
nombres = sorted(i["name"] for i in gente.list_indexes())
ver(nombres == ["_id_", "dir.ciudad_1", "n_-1"], "list_indexes: %r" % (nombres,))
ver(gente.count_documents({"n": {"$gte": 190, "$lt": 195}}) == 5, "índice descendente con rango")
gente.drop_index("n_-1")
ver("n_-1" not in gente.index_information(), "drop_index")
unicos = db.unicos
unicos.insert_many([{"email": "a@x"}, {"email": "b@x"}])
unicos.create_index("email", unique=True)
e = lanza(lambda: unicos.insert_one({"email": "a@x"}), errors.DuplicateKeyError)
ver(e is not None, "índice único: DuplicateKeyError")
e = lanza(lambda: unicos.update_one({"email": "b@x"}, {"$set": {"email": "a@x"}}), errors.DuplicateKeyError)
ver(e is not None and unicos.count_documents({"email": "b@x"}) == 1, "índice único también al actualizar")
unicos.insert_one({"otro": 1})
e = lanza(lambda: unicos.insert_one({"otro": 2}), errors.DuplicateKeyError)
ver(e is not None, "índice único: dos documentos sin el campo chocan (null), como en MongoDB")
unicos.drop()

# ---- distinct y agregación
ver(gente.distinct("dir.ciudad") == ["Lima", "Madrid", "Quito"], "distinct")
res = list(gente.aggregate([
    {"$match": {"n": {"$lt": 30}}},
    {"$group": {"_id": "$dir.ciudad", "cuantos": {"$sum": 1}, "media": {"$avg": "$n"}, "max": {"$max": "$n"}}},
    {"$sort": {"_id": 1}},
]))
ver([(g["_id"], g["cuantos"], g["max"]) for g in res] == [(None, 1, 3), ("Lima", 9, 28), ("Madrid", 9, 27), ("Quito", 10, 29)],
    "aggregate: $match, $group y $sort: %r" % (res,))
res = list(gente.aggregate([{"$match": {"n": {"$lt": 3}}}, {"$unwind": "$notas"}, {"$count": "total"}]))
ver(res == [{"total": 4}], "aggregate: $unwind y $count: %r" % (res,))
res = list(gente.aggregate([{"$match": {"n": 10}}, {"$project": {"_id": 0, "ciudad": "$dir.ciudad", "doble": {"$multiply": ["$n", 2]}}}]))
ver(res == [{"ciudad": "Lima", "doble": 20}], "aggregate: $project con expresiones: %r" % (res,))
res = list(gente.aggregate([{"$sort": {"n": -1}}, {"$skip": 1}, {"$limit": 2}, {"$project": {"n": 1, "_id": 0}}]))
ver(res == [{"n": 198}, {"n": 197}], "aggregate: $sort, $skip y $limit: %r" % (res,))

# ---- tipos
tipos = db.tipos
tipos.drop()
valor = {"_id": "t", "nulo": None, "si": True, "no": False, "i32": -5, "i64": 2 ** 40, "real": 2.5,
         "texto": "ñandú €", "bin": b"\x00\x01\xff", "lista": [1, [2, 3], {"a": None}], "oid": ObjectId("0123456789abcdef01234567"),
         "sub": {"a": {"b": {"c": 1}}}}
tipos.insert_one(valor)
leido = tipos.find_one("t")
ver(leido == valor, "ida y vuelta de todos los tipos")
try:
    import datetime
    ahora = datetime.datetime(2026, 11, 8, 12, 30, 15, 123000)
except ImportError:
    ahora = litemonkey.Fecha(1793968215123)
tipos.insert_one({"_id": "f", "cuando": ahora})
ver(tipos.find_one("f")["cuando"] == ahora, "fecha (datetime o Fecha)")
e = lanza(lambda: tipos.insert_one({"_id": "grande", "x": "a" * (1024 * 1024)}), errors.OperationFailure)
ver(e is not None, "documento de más de 1 MB rechazado")

# ---- administración
ver(nombre_base in c.list_database_names(), "list_database_names")
ver(sorted(db.list_collection_names()) == ["gente", "tipos"], "list_collection_names")
st = gente.stats()
ver(st["count"] == gente.count_documents({}) and st["nindexes"] == 2, "stats")
ver(db.command("ping")["ok"] == 1, "command ping")
tipos.drop()
ver(db.list_collection_names() == ["gente"], "drop de una colección")
e = lanza(lambda: c.nada.x.find_one({"a": {"$bad": 1}}), errors.OperationFailure)
ver(e is not None and e.code == 2, "filtro no válido: OperationFailure")
ver(c.nada.x.find_one() is None and c.nada.x.count_documents({}) == 0, "colección que no existe: vacía")
c.close()
print("casos: %d ok, %d fallos" % (bien, fallos))
