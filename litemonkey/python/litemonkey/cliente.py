# SPDX-License-Identifier: GPL-2.0-or-later
# cliente.py - cliente de lite-monkey con una API parecida a la de pymongo.
#
#   import litemonkey
#   cliente = litemonkey.Cliente("10.0.2.2", 7979)      # o MongoClient
#   productos = cliente.tienda.productos                 # o cliente["tienda"]["productos"]
#   productos.insert_one({"nombre": "té", "precio": 3.5})
#   for p in productos.find({"precio": {"$lt": 10}}).sort("precio"):
#       print(p["nombre"])
#
# El mismo archivo funciona en CPython y en el MicroPython de SaguiOS.
import socket
import struct
import hashlib

from .bson import encode, decode, ObjectId, SON
from . import errors

PUERTO = 7979
OP_PETICION = 1
OP_RESPUESTA = 2
LOTE_INSERCION = 4 * 1024 * 1024


def _hmac_sha256(clave, msg):
    if len(clave) > 64:
        clave = hashlib.sha256(clave).digest()
    clave = clave + b"\x00" * (64 - len(clave))
    ipad = bytes([x ^ 0x36 for x in clave])
    opad = bytes([x ^ 0x5C for x in clave])
    return hashlib.sha256(opad + hashlib.sha256(ipad + msg).digest()).digest()


def _orden_sort(clave, direccion=None):
    """"campo", ("campo", -1), [("a", 1), ("b", -1)] o dict -> SON."""
    if isinstance(clave, str):
        return SON([(clave, 1 if direccion is None else direccion)])
    if isinstance(clave, (dict, SON)):
        return SON(clave)
    if isinstance(clave, tuple) and len(clave) == 2 and isinstance(clave[0], str) and isinstance(clave[1], int):
        return SON([clave])
    return SON([(k, d) for k, d in clave])


# ------------------------------------------------------------ resultados

class InsertOneResult:
    def __init__(self, inserted_id):
        self.inserted_id = inserted_id
        self.acknowledged = True

    def __repr__(self):
        return "InsertOneResult(%r)" % (self.inserted_id,)


class InsertManyResult:
    def __init__(self, inserted_ids):
        self.inserted_ids = inserted_ids
        self.acknowledged = True

    def __repr__(self):
        return "InsertManyResult(%d documentos)" % len(self.inserted_ids)


class UpdateResult:
    def __init__(self, r):
        self.raw_result = r
        ups = r.get("upserted")
        self.upserted_id = ups[0]["_id"] if ups else None
        self.matched_count = r.get("n", 0) - (len(ups) if ups else 0)
        self.modified_count = r.get("nModified", 0)
        self.acknowledged = True

    def __repr__(self):
        return "UpdateResult(coincidencias=%d, modificados=%d, upserted_id=%r)" % (
            self.matched_count, self.modified_count, self.upserted_id)


class DeleteResult:
    def __init__(self, r):
        self.raw_result = r
        self.deleted_count = r.get("n", 0)
        self.acknowledged = True

    def __repr__(self):
        return "DeleteResult(borrados=%d)" % self.deleted_count


# ------------------------------------------------------------ cursores

class CommandCursor:
    """Cursor de una respuesta {"cursor": {"firstBatch", "id", "ns"}}."""

    def __init__(self, coleccion, respuesta, batch_size=0):
        c = respuesta["cursor"]
        self._col = coleccion
        self._lote = c.get("firstBatch", [])
        self._i = 0
        self._id = c.get("id", 0)
        self._ns = c.get("ns", "")
        self._batch = batch_size
        self.alive = True

    def _pedir_mas(self):
        if not self._id:
            return False
        orden = [("getMore", self._id), ("collection", self._col.name)]
        if self._batch:
            orden.append(("batchSize", self._batch))
        r = self._col.database._orden(orden)
        c = r["cursor"]
        self._lote = c.get("nextBatch", [])
        self._i = 0
        self._id = c.get("id", 0)
        return True

    def __iter__(self):
        return self

    def __next__(self):
        while self._i >= len(self._lote):
            if not self._pedir_mas():
                self.alive = False
                raise StopIteration
        d = self._lote[self._i]
        self._i += 1
        return d

    def next(self):
        return self.__next__()

    def close(self):
        if self._id:
            try:
                self._col.database._orden([("killCursors", self._col.name), ("cursors", [self._id])])
            except errors.LiteMonkeyError:
                pass
            self._id = 0
        self.alive = False

    def to_list(self, length=None):
        r = []
        for d in self:
            r.append(d)
            if length and len(r) >= length:
                break
        return r


class Cursor(CommandCursor):
    """Cursor de find: perezoso; sort/skip/limit/batch_size antes de iterar."""

    def __init__(self, coleccion, filtro=None, projection=None, skip=0, limit=0, sort=None, batch_size=0):
        self._col = coleccion
        self._filtro = filtro or {}
        self._proy = projection
        self._skip = skip
        self._limit = limit
        self._sort = _orden_sort(sort) if sort else None
        self._batch = batch_size
        self._empezado = False
        self._lote = []
        self._i = 0
        self._id = 0
        self.alive = True

    def _comprobar(self):
        if self._empezado:
            raise errors.LiteMonkeyError("no se puede cambiar un cursor que ya empezó a leerse")

    def sort(self, clave, direccion=None):
        self._comprobar()
        self._sort = _orden_sort(clave, direccion)
        return self

    def skip(self, n):
        self._comprobar()
        self._skip = n
        return self

    def limit(self, n):
        self._comprobar()
        self._limit = n
        return self

    def batch_size(self, n):
        self._comprobar()
        self._batch = n
        return self

    def _orden_find(self):
        o = [("find", self._col.name), ("filter", self._filtro)]
        if self._proy is not None:
            p = self._proy
            if isinstance(p, (list, tuple)):
                p = SON([(k, 1) for k in p])
            o.append(("projection", p))
        if self._sort:
            o.append(("sort", self._sort))
        if self._skip:
            o.append(("skip", self._skip))
        if self._limit:
            o.append(("limit", self._limit))
        if self._batch:
            o.append(("batchSize", self._batch))
        return o

    def _empezar(self):
        self._empezado = True
        r = self._col.database._orden(self._orden_find())
        CommandCursor.__init__(self, self._col, r, self._batch)

    def __next__(self):
        if not self._empezado:
            self._empezar()
        return CommandCursor.__next__(self)

    def explain(self):
        return self._col.database._orden([("explain", SON(self._orden_find()))])

    def clone(self):
        return Cursor(self._col, self._filtro, self._proy, self._skip, self._limit, self._sort, self._batch)

    def distinct(self, clave):
        return self._col.distinct(clave, self._filtro)


# ------------------------------------------------------------ colección

class Coleccion:
    # nombres al estilo de la consola de MongoDB (db.col.insertOne) para lmk
    _ALIAS = {"insertOne": "insert_one", "insertMany": "insert_many", "findOne": "find_one",
              "updateOne": "update_one", "updateMany": "update_many", "replaceOne": "replace_one",
              "deleteOne": "delete_one", "deleteMany": "delete_many", "countDocuments": "count_documents",
              "createIndex": "create_index", "dropIndex": "drop_index", "getIndexes": "list_indexes"}

    def __init__(self, base, nombre):
        self.database = base
        self.name = nombre
        self.full_name = base.name + "." + nombre

    def __getattr__(self, n):
        if n.startswith("_"):
            raise AttributeError(n)
        if n in Coleccion._ALIAS:
            return getattr(self, Coleccion._ALIAS[n])
        return Coleccion(self.database, self.name + "." + n)

    def __getitem__(self, n):
        return Coleccion(self.database, self.name + "." + n)

    def __repr__(self):
        return "Coleccion(%r)" % self.full_name

    def __eq__(self, o):
        return isinstance(o, Coleccion) and o.full_name == self.full_name

    def _orden(self, pares):
        return self.database._orden(pares)

    def _escritura(self, pares, unico=False):
        r = self._orden(pares)
        we = r.get("writeErrors")
        if we:
            e = we[0]
            if unico or len(we) == 1 and pares[0][0] != "insert":
                cls = errors.DuplicateKeyError if e.get("code") == 11000 else errors.WriteError
                raise cls(e.get("errmsg"), e.get("code"), r)
            raise errors.BulkWriteError("errores de escritura: %s" % e.get("errmsg"), 65,
                                        {"writeErrors": we, "nInserted": r.get("n", 0)})
        return r

    # ---- inserción
    def insert_one(self, doc):
        if "_id" not in doc:
            doc["_id"] = ObjectId()
        self._escritura([("insert", self.name), ("documents", [doc])], True)
        return InsertOneResult(doc["_id"])

    def insert_many(self, docs, ordered=True):
        ids = []
        lote = []
        tam = 0
        for d in docs:
            if "_id" not in d:
                d["_id"] = ObjectId()
            ids.append(d["_id"])
            b = encode(d)
            lote.append(b)
            tam += len(b)
            if tam > LOTE_INSERCION or len(lote) >= 10000:
                self._insertar_crudo(lote, ordered)
                lote = []
                tam = 0
        if lote:
            self._insertar_crudo(lote, ordered)
        return InsertManyResult(ids)

    def _insertar_crudo(self, lote, ordered):
        # Los documentos ya codificados se juntan a mano en el array "documents".
        partes = []
        for i in range(len(lote)):
            partes.append(b"\x03" + str(i).encode() + b"\x00" + lote[i])
        cuerpo = b"".join(partes)
        arr = struct.pack("<i", len(cuerpo) + 5) + cuerpo + b"\x00"
        r = self.database._orden_crudo([("insert", self.name), ("ordered", ordered)], [(b"\x04", "documents", arr)])
        we = r.get("writeErrors")
        if we:
            raise errors.BulkWriteError("errores de escritura: %s" % we[0].get("errmsg"), 65,
                                        {"writeErrors": we, "nInserted": r.get("n", 0)})
        return r

    # ---- búsqueda
    def find(self, filter=None, projection=None, skip=0, limit=0, sort=None, batch_size=0, **kw):
        return Cursor(self, filter, projection, skip, limit, sort, batch_size)

    def find_one(self, filter=None, *args, **kw):
        if filter is not None and not isinstance(filter, (dict, SON)):
            filter = {"_id": filter}
        c = self.find(filter, *args, **kw).limit(-1)
        for d in c:
            return d
        return None

    def count_documents(self, filter=None, skip=0, limit=0):
        o = [("count", self.name), ("query", filter or {})]
        if skip:
            o.append(("skip", skip))
        if limit:
            o.append(("limit", limit))
        return self._orden(o)["n"]

    def estimated_document_count(self):
        return self._orden([("count", self.name)])["n"]

    def distinct(self, key, filter=None):
        o = [("distinct", self.name), ("key", key)]
        if filter:
            o.append(("query", filter))
        return self._orden(o)["values"]

    def aggregate(self, pipeline, batchSize=0, **kw):
        cur = SON([("batchSize", batchSize)]) if batchSize else {}
        r = self._orden([("aggregate", self.name), ("pipeline", list(pipeline)), ("cursor", cur)])
        return CommandCursor(self, r, batchSize)

    # ---- actualización
    def _update(self, filtro, u, multi, upsert, operadores):
        if not isinstance(u, (dict, SON)) or not u:
            raise ValueError("la actualización tiene que ser un documento no vacío")
        claves = list(u.keys())
        con_ops = claves[0].startswith("$")
        if operadores and not con_ops:
            raise ValueError("update_one/update_many necesitan operadores ($set, $inc...)")
        if not operadores and any(k.startswith("$") for k in claves):
            raise ValueError("replace_one no admite operadores ($...)")
        st = SON([("q", filtro or {}), ("u", u), ("multi", multi), ("upsert", upsert)])
        return UpdateResult(self._escritura([("update", self.name), ("updates", [st])], True))

    def update_one(self, filter, update, upsert=False):
        return self._update(filter, update, False, upsert, True)

    def update_many(self, filter, update, upsert=False):
        return self._update(filter, update, True, upsert, True)

    def replace_one(self, filter, replacement, upsert=False):
        return self._update(filter, replacement, False, upsert, False)

    def find_one_and_update(self, filter, update, upsert=False, return_document=False, **kw):
        """No es atómico entre varios clientes (busca y luego actualiza por _id)."""
        antes = self.find_one(filter, **kw)
        if antes is None:
            if upsert:
                r = self.update_one(filter, update, upsert=True)
                return self.find_one({"_id": r.upserted_id}) if return_document else None
            return None
        self.update_one({"_id": antes["_id"]}, update)
        return self.find_one({"_id": antes["_id"]}) if return_document else antes

    def find_one_and_delete(self, filter, **kw):
        d = self.find_one(filter, **kw)
        if d is not None:
            self.delete_one({"_id": d["_id"]})
        return d

    # ---- borrado
    def _delete(self, filtro, limite):
        st = SON([("q", filtro or {}), ("limit", limite)])
        return DeleteResult(self._escritura([("delete", self.name), ("deletes", [st])], True))

    def delete_one(self, filter):
        return self._delete(filter, 1)

    def delete_many(self, filter):
        return self._delete(filter, 0)

    # ---- índices y administración
    def create_index(self, keys, unique=False, name=None, **kw):
        k = _orden_sort(keys)
        if len(k) != 1:
            raise ValueError("lite-monkey solo tiene índices de un campo")
        campo, d = k.pares[0]
        nombre = name or "%s_%d" % (campo, d)
        spec = SON([("key", k), ("name", nombre)])
        if unique:
            spec["unique"] = True
        self._orden([("createIndexes", self.name), ("indexes", [spec])])
        return nombre

    def drop_index(self, index):
        if not isinstance(index, str):
            k = _orden_sort(index)
            index = "%s_%d" % k.pares[0]
        self._orden([("dropIndexes", self.name), ("index", index)])

    def drop_indexes(self):
        self._orden([("dropIndexes", self.name), ("index", "*")])

    def list_indexes(self):
        return CommandCursor(self, self._orden([("listIndexes", self.name)]))

    def index_information(self):
        r = {}
        for ix in self.list_indexes():
            info = {"key": list(ix["key"].items())}
            if ix.get("unique"):
                info["unique"] = True
            r[ix["name"]] = info
        return r

    def drop(self):
        try:
            self._orden([("drop", self.name)])
        except errors.OperationFailure as e:
            if e.code != 26:
                raise

    def stats(self):
        return self._orden([("collStats", self.name)])

    def compact(self):
        return self._orden([("compact", self.name)])


# ------------------------------------------------------------ base

class Base:
    def __init__(self, cliente, nombre):
        self.client = cliente
        self.name = nombre

    def __getattr__(self, n):
        if n.startswith("_"):
            raise AttributeError(n)
        return Coleccion(self, n)

    def __getitem__(self, n):
        return Coleccion(self, n)

    def __repr__(self):
        return "Base(%r)" % self.name

    def get_collection(self, n):
        return Coleccion(self, n)

    def _orden(self, pares):
        return self.client._orden(pares, self.name)

    def _orden_crudo(self, pares, crudos):
        return self.client._orden(pares, self.name, crudos)

    def command(self, orden, valor=1, **kw):
        if isinstance(orden, str):
            pares = [(orden, valor)] + list(kw.items())
        else:
            pares = list(orden.items()) + list(kw.items())
        return self._orden(pares)

    def list_collection_names(self, filter=None):
        o = [("listCollections", 1), ("nameOnly", True)]
        if filter:
            o.append(("filter", filter))
        r = self._orden(o)
        return [c["name"] for c in CommandCursor(Coleccion(self, "$cmd"), r)]

    def list_collections(self):
        return CommandCursor(Coleccion(self, "$cmd"), self._orden([("listCollections", 1)]))

    def create_collection(self, nombre):
        self._orden([("create", nombre)])
        return Coleccion(self, nombre)

    def drop_collection(self, nombre):
        if isinstance(nombre, Coleccion):
            nombre = nombre.name
        Coleccion(self, nombre).drop()

    def stats(self):
        return self._orden([("dbStats", 1)])


# ------------------------------------------------------------ cliente

class Cliente:
    """Conexión con un servidor litemonkey (puerto 7979).

    Cliente("10.0.2.2"), Cliente("10.0.2.2", 7979), Cliente("host:7979") o
    Cliente("litemonkey://usuario:clave@host:7979"). Con usuario y clave hace
    el desafío HMAC-SHA256 (la clave no viaja); sin TLS: solo en la red local."""

    def __init__(self, host="127.0.0.1", port=None, usuario=None, clave=None, timeout=30,
                 username=None, password=None, connect=True, **kw):
        usuario = usuario or username
        clave = clave or password
        if "://" in host:
            host = host.split("://", 1)[1].split("/")[0]
            if "@" in host:
                cred, host = host.rsplit("@", 1)
                if ":" in cred:
                    usuario, clave = cred.split(":", 1)
        if ":" in host:
            host, p = host.rsplit(":", 1)
            port = port or int(p)
        self.host = host
        self.port = port or PUERTO
        self._usuario = usuario
        self._clave = clave
        self._timeout = timeout
        self._s = None
        self._rid = 0
        self.server_info_cache = None
        if connect:
            self._conectar()

    def _conectar(self):
        self.close()
        try:
            dir = socket.getaddrinfo(self.host, self.port)[0][-1]
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(self._timeout)
            s.connect(dir)
        except OSError as e:
            raise errors.ConnectionFailure("no se pudo conectar con %s:%d (%s)" % (self.host, self.port, e))
        try:
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        except (OSError, AttributeError):
            pass
        self._s = s
        self.server_info_cache = self._orden([("hello", 1)], "admin")
        if self._usuario:
            self._autenticar()

    def _autenticar(self):
        r = self._orden([("authStart", self._usuario)], "admin")
        nonce = r["nonce"]
        clave = self._clave.encode("utf-8")
        prueba = _hmac_sha256(clave, nonce + self._usuario.encode("utf-8"))
        r = self._orden([("authFinish", self._usuario), ("proof", prueba)], "admin")
        if r.get("serverProof") != _hmac_sha256(clave, b"servidor" + nonce):
            self.close()
            raise errors.AuthenticationFailed("el servidor no demostró conocer la clave", 18)

    def _recibir(self, n):
        partes = []
        falta = n
        while falta > 0:
            d = self._s.recv(falta if falta < 1048576 else 1048576)
            if not d:
                raise OSError("el servidor cerró la conexión")
            partes.append(d)
            falta -= len(d)
        return b"".join(partes)

    def _orden(self, pares, base, crudos=None):
        """Manda una orden (lista de pares; la primera clave es la orden) y
        devuelve la respuesta (dict). crudos: elementos ya codificados."""
        if self._s is None:
            self._conectar()
        pares = list(pares) + [("$db", base)]
        doc = encode(pares)
        if crudos:
            extra = b"".join(t + k.encode() + b"\x00" + v for t, k, v in crudos)
            doc = struct.pack("<i", len(doc) + len(extra)) + doc[4:-1] + extra + b"\x00"
        self._rid = (self._rid + 1) & 0x7FFFFFFF
        try:
            self._s.sendall(struct.pack("<III", 12 + len(doc), self._rid, OP_PETICION) + doc)
            cab = self._recibir(12)
            n, rid, cod = struct.unpack("<III", cab)
            cuerpo = self._recibir(n - 12)
        except OSError as e:
            self.close()
            raise errors.ConnectionFailure("conexión perdida con %s:%d (%s)" % (self.host, self.port, e))
        r = decode(cuerpo)
        if not r.get("ok"):
            raise errors.desde_respuesta(r)
        return r

    def __getattr__(self, n):
        if n.startswith("_"):
            raise AttributeError(n)
        return Base(self, n)

    def __getitem__(self, n):
        return Base(self, n)

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def __repr__(self):
        return "Cliente(%r, %d)" % (self.host, self.port)

    def get_database(self, n):
        return Base(self, n)

    def close(self):
        if self._s is not None:
            try:
                self._s.close()
            except OSError:
                pass
            self._s = None

    def server_info(self):
        return self._orden([("hello", 1)], "admin")

    def ping(self):
        return self._orden([("ping", 1)], "admin")

    def server_status(self):
        return self._orden([("serverStatus", 1)], "admin")

    def list_database_names(self):
        return [d["name"] for d in self._orden([("listDatabases", 1)], "admin")["databases"]]

    def list_databases(self):
        return self._orden([("listDatabases", 1)], "admin")["databases"]

    def drop_database(self, nombre):
        if isinstance(nombre, Base):
            nombre = nombre.name
        self._orden([("dropDatabase", 1)], nombre)

    def shutdown(self):
        """Para el servidor (escribe todo en el disco antes)."""
        try:
            self._orden([("shutdown", 1)], "admin")
        except errors.ConnectionFailure:
            pass
        self.close()


MongoClient = Cliente
