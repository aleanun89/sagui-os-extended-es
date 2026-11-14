# lite-monkey: la base de datos documental de SaguiOS

lite-monkey es un motor de base de datos **de documentos**, escrito desde cero en C99,
inspirado en MongoDB pero más básico. El nombre viene del sagüí, que es un mono
(*monkey*). Lo forman tres piezas:

- el servidor **`litemonkey`**, que escucha en el **puerto TCP 7979**;
- el cliente **Python `litemonkey`**, con una API parecida a la de pymongo;
- la consola **`lmk`**, para hacer consultas a mano.

El mismo código C funciona en el PC (Linux/WSL) y en SaguiOS. El mismo cliente Python
funciona en CPython y en el MicroPython de SaguiOS. Decisión: D117. Mejora: 067.

## 1. Arrancar

### En el PC (Linux/WSL)

```bash
make -C litemonkey                 # litemonkey/build/litemonkey (gcc normal)
litemonkey/build/litemonkey        # datos en ~/.litemonkey, puerto 7979
make -C litemonkey test            # pruebas del núcleo (C, con ASan) y del cliente (CPython)
make -C litemonkey bench           # cifras de rendimiento
pip install -e litemonkey/python   # el paquete litemonkey y la orden lmk (mejor en un venv)
```

### En SaguiOS

```
litemonkey &                       # en segundo plano; /bin/litemonkey
lmk                                # consola, conecta con 127.0.0.1:7979
python mi_programa.py              # import litemonkey (está en /lib/python)
```

Si `/home` es un disco persistente con escritura, los datos van a `/home/litemonkey`;
si no, a `/tmp/litemonkey`, que está en la RAM y se pierde al apagar.

### Opciones del servidor

| Opción | Qué hace |
|---|---|
| `--puerto N` | puerto TCP (por defecto, 7979) |
| `--datos CARPETA` | dónde se guardan las bases (una carpeta por base) |
| `--escuchar DIRECCIÓN` | escucha solo en esa dirección, p. ej. `127.0.0.1` (solo en el PC; SaguiOS escucha en todas) |
| `--sync` | sincroniza el disco tras cada escritura (lento y seguro) |
| `--memoria MB` | límite de documentos en memoria (16 en SaguiOS, 256 en el PC); el resto se lee del disco |
| `--usuario U --clave C` | exige autenticación (ver §7) |
| `-v` | escribe cada conexión que llega y que se cierra |

Al arrancar escribe una línea como
`litemonkey 1: escuchando en *:7979; datos en /home/litemonkey (2 colecciones, 500 documentos, 30 ms)`.
Para pararlo: la orden `shutdown` (`cliente.shutdown()` en Python, o
`lmk --eval "cliente.shutdown()"`), que escribe todo en el disco antes de salir; en el PC
también `Ctrl+C`. En SaguiOS, `kill` o `Ctrl+C` lo terminan en seco: lo que ya respondió está
escrito, porque cada orden termina con sus registros en el archivo.

## 2. El cliente Python

```python
import litemonkey

cliente = litemonkey.Cliente("127.0.0.1", 7979)     # también MongoClient, o "host:puerto"
productos = cliente.tienda.productos                 # o cliente["tienda"]["productos"]

r = productos.insert_one({"nombre": "té", "precio": 3.5, "tags": ["bebida"]})
print(r.inserted_id)                                 # ObjectId('...')
productos.insert_many([{"nombre": "café", "precio": 4}, {"nombre": "pan", "precio": 1.2}])

for p in productos.find({"precio": {"$lt": 4}}, {"_id": 0}).sort("precio", -1).limit(10):
    print(p)
print(productos.find_one({"nombre": "té"}))
print(productos.count_documents({"tags": "bebida"}))

productos.update_one({"nombre": "té"}, {"$inc": {"precio": 0.5}, "$push": {"tags": "caliente"}})
productos.update_many({}, {"$set": {"tienda": "centro"}})
productos.replace_one({"nombre": "pan"}, {"nombre": "pan", "precio": 1.5}, upsert=True)
productos.delete_many({"precio": {"$gt": 100}})

productos.create_index("precio")                     # o [("precio", litemonkey.DESCENDING)], unique=True
print(productos.find({"precio": 3}).explain()["queryPlanner"]["winningPlan"])
print(productos.distinct("tags"))
print(list(productos.aggregate([
    {"$match": {"precio": {"$gt": 1}}},
    {"$group": {"_id": "$tienda", "total": {"$sum": "$precio"}, "n": {"$sum": 1}}},
])))
```

**Lo que hay:**

- `Cliente(host, port, usuario=, clave=, timeout=)` (alias `MongoClient`; también acepta
  `"litemonkey://usuario:clave@host:7979"`), `list_database_names()`, `drop_database()`,
  `server_info()`, `server_status()`, `ping()`, `shutdown()`, `close()` y `with`.
- Base: `list_collection_names()`, `create_collection()`, `drop_collection()`, `command()`,
  `stats()`.
- Colección: `insert_one`, `insert_many` (devuelven `inserted_id` e `inserted_ids`), `find`,
  `find_one` (también con un `_id` suelto), `update_one`, `update_many`, `replace_one`
  (`upsert=True`; devuelven `matched_count`, `modified_count`, `upserted_id`), `delete_one`,
  `delete_many` (`deleted_count`), `count_documents`, `estimated_document_count`, `distinct`,
  `aggregate`, `create_index`, `drop_index`, `drop_indexes`, `list_indexes`,
  `index_information`, `drop`, `stats`, `compact`, y `find_one_and_update` /
  `find_one_and_delete` (no atómicos entre clientes).
- Cursor: iterable, con `.sort()`, `.skip()`, `.limit()`, `.batch_size()`, `.explain()`,
  `.close()` y `.to_list()`. Pide los lotes siguientes (`getMore`) solo.
- Tipos: `ObjectId`, `SON` (documento con orden garantizado), `Regex`, `Binary`, `Fecha`.
- Excepciones (`litemonkey.errors`): `OperationFailure` (con `code`), `DuplicateKeyError`,
  `WriteError`, `BulkWriteError`, `CursorNotFound`, `AuthenticationFailed`,
  `ConnectionFailure`, `InvalidId`, `InvalidDocument`.

**CPython y MicroPython.** El paquete es Python puro (struct, socket, hashlib, binascii) y es
el mismo archivo en los dos. Diferencias que se notan:

- las fechas: en CPython son `datetime.datetime` (UTC, sin zona); en MicroPython, que no
  tiene `datetime`, son `litemonkey.Fecha` (milisegundos desde 1970, `.ms`);
- en MicroPython los `dict` no garantizan el orden de las claves: para el orden de un `sort`
  con varios campos usa la lista de pares (`.sort([("a", 1), ("b", -1)])`) o `SON`.

## 3. La consola `lmk`

```
lmk [host[:puerto]] [--base NOMBRE] [--usuario U --clave C] [--eval EXPRESIÓN]...
```

```
lmk:test> use tienda
lmk:tienda> show collections
lmk:tienda> db.productos.find({"precio": {"$gt": 2}}).sort("precio")
lmk:tienda> it                      (los 20 siguientes)
lmk:tienda> db.productos.insertOne({"nombre": "agua", "fria": true, "marca": null})
lmk:tienda> exit
```

Las líneas son expresiones de Python con `db`, `cliente`, `ObjectId`, `true`, `false` y `null`;
también valen los nombres de la consola de MongoDB (`insertOne`, `findOne`, `updateMany`...).
`--eval` ejecuta y sale (útil en scripts).

## 4. Modelo de datos

- **Bases → colecciones → documentos.** Una base es una carpeta; una colección, un archivo
  `base/coleccion.lmk`. Se crean solas al insertar.
- **Nombres:** letras, números, `_` y `-` (y `.` en las colecciones), hasta 64 caracteres.
- **Tipos** (BSON): null, bool, int32, int64, double, cadena UTF-8, binario, array, documento,
  ObjectId (12 bytes), fecha (ms desde 1970) y regex. Decimal128 no.
- **`_id`** obligatorio y único; si falta, el servidor (o el cliente Python) pone un ObjectId.
  No puede ser un array ni una regex, ni cambiarse al actualizar.
- **Límites:** documento de 1 MB, 32 niveles de anidamiento, 16 índices por colección.
- **Orden y comparación** como en MongoDB: MinKey < null < números < cadenas < documentos <
  arrays < binario < ObjectId < bool < fecha < timestamp < regex < MaxKey; 1, 1L y 1.0 son
  iguales.

## 5. Consultas, actualizaciones, agregación e índices

**Filtros:** igualdad (`{"a": 1}`), `$eq $ne $gt $gte $lt $lte $in $nin $exists $not $and
$or $nor $regex` (+ `$options`), y también `$size`, `$all` y `$elemMatch`. Los caminos con
punto (`"direccion.ciudad"`, `"notas.1"`) entran en los documentos y en los arrays; una
condición sobre un array se cumple si la cumple **algún** elemento (`$ne` y `$nin`: ninguno).
`{"a": null}` casa también con los documentos sin `a`.

**Regex** (propias): `^ $ . * + ?`, clases `[a-z] [^0-9]`, `\d \w \s` (y negadas), escapes y
las opciones `i`, `m` y `s`. Sin grupos, `|` ni `{n,m}` (dan error). El punto consume un
carácter UTF-8 entero.

**Proyección:** inclusión (`{"a": 1, "b.c": 1}`) o exclusión (`{"a": 0}`), con `_id` incluido
salvo `{"_id": 0}`. **Orden:** `sort` por uno o varios campos (1 o -1).

**Actualizaciones:** `$set $unset $inc $push $pull $rename` y además `$mul $min $max
$addToSet $pop $setOnInsert`; `$push` y `$addToSet` admiten `$each`; `$pull` admite un valor,
una condición (`{"$gte": 5}`) o un filtro sobre subdocumentos. Sin operadores, el documento es
un reemplazo (conserva el `_id`). `upsert` construye el documento con las igualdades del
filtro.

**Agregación:** `$match` (si va primera usa los índices), `$project` (0/1 y campos
calculados), `$addFields`/`$set`, `$sort`, `$skip`, `$limit`, `$unwind` (campo de primer
nivel), `$count` y `$group` con `$sum $avg $min $max $count $first $last $push`. Expresiones:
`"$campo"`, literales, `{"$literal": v}`, `$add $subtract $multiply $divide $concat`.

**Índices:** el de `_id` siempre (tabla hash); los demás, de **un campo**, ascendentes o
descendentes, normales o `unique` (dos documentos sin el campo chocan, como en MongoDB), y
**multiclave** (un array indexa cada elemento). Son árboles (treap) con una copia de la clave,
así no hace falta que el documento esté en memoria. El **planificador** usa, por este orden: la
tabla hash si el filtro tiene `_id` igual (o `$in`) — `IDHACK` —; un índice con igualdad o
`$in` sobre su campo; un índice con rango (`$gt`, `$lt`...); y si no, recorre todo
(`COLLSCAN`). `explain` dice el plan, el índice, cuántas claves y documentos miró.

## 6. Almacenamiento

Cada colección es un archivo de **solo añadir**:

```
cabecera "LMK1" (8 bytes), y registros:
tipo (1 byte) | longitud (u32 LE) | documento BSON | CRC32 (u32 LE, de tipo+longitud+documento)
tipo: 1 insertar, 2 actualizar (el documento entero nuevo), 3 borrar ({_id}), 4 índice, 5 borrar índice
```

- **Al abrir** se lee el archivo entero (por bloques de 1 MB) y se reconstruyen la tabla hash
  del `_id` y los índices. El primer registro cortado, con CRC malo o con un BSON imposible
  (un corte de luz a media escritura) termina la lectura: se avisa en el registro del servidor
  (`aviso: base.col: se descartan N bytes del final...`) y la colección se reescribe limpia.
- **Compactación:** cuando el archivo pasa de 256 KB y más de la mitad es basura (documentos
  viejos y borrados), y con la orden `compact`, se escribe `coleccion.lmk.tmp`, se sincroniza y
  se **renombra** encima. No hace falta truncar (SaguiOS no sabía hasta la 064). Un `.tmp` que
  quede de un corte se borra al arrancar.
- **Durabilidad:** cada orden termina con sus registros escritos (`write`); con `--sync`, además
  sincronizados. Si no, se sincroniza cada segundo y cada 1000 escrituras. En SaguiOS,
  «sincronizar» es `SIS_SINCRONIZAR` (mejora 064, D116), que vacía la caché del volumen.
- **Caché:** los documentos se guardan en memoria hasta `--memoria`; los demás se leen del
  disco por su desplazamiento cuando hacen falta. Los índices y la tabla hash siempre están en
  memoria.

## 7. Autenticación y seguridad

Con `--usuario U --clave C` el servidor solo acepta `hello`, `ping` y la autenticación hasta
que el cliente se identifica con un desafío y respuesta **HMAC-SHA256**: la clave no viaja
nunca. El servidor manda 24 bytes al azar (`nonce`); el cliente responde
`HMAC-SHA256(clave, nonce + usuario)`; y el servidor demuestra a su vez que conoce la clave con
`HMAC-SHA256(clave, "servidor" + nonce)`. **No hay TLS**: los datos viajan en claro, así que
úsalo solo en la red local (o dentro de SaguiOS, por 127.0.0.1).

## 8. Protocolo (para escribir otros clientes)

TCP 7979. Cada mensaje es una **trama**:

```
longitud (u32 LE, incluye esta cabecera) | id de petición (u32 LE) | código (u32 LE) | documento BSON
código 1 = petición; la respuesta lleva el mismo id y el código 2
```

La petición es un documento cuya **primera clave es la orden** y lleva la base en `$db`. La
respuesta es `{"ok": 1.0, ...}` o `{"ok": 0.0, "errmsg": "...", "code": N, "codeName": "..."}`.
Las órdenes, sus campos y las respuestas siguen a MongoDB:

| Orden | Campos | Respuesta |
|---|---|---|
| `hello` (`isMaster`) | | `version: "litemonkey 1"`, `commands`, `maxBsonObjectSize`, `auth` |
| `ping` | | |
| `insert` | `documents`, `ordered` | `n`, `writeErrors` |
| `find` | `filter`, `projection`, `sort`, `skip`, `limit` (negativo = un lote), `batchSize`, `singleBatch` | `cursor: {firstBatch, id, ns}` |
| `getMore` | valor = id del cursor; `collection`, `batchSize` | `cursor: {nextBatch, id, ns}` |
| `killCursors` | `cursors` | `cursorsKilled` |
| `update` | `updates: [{q, u, multi, upsert}]`, `ordered` | `n`, `nModified`, `upserted`, `writeErrors` |
| `delete` | `deletes: [{q, limit (0 o 1)}]` | `n`, `writeErrors` |
| `count` | `query`, `skip`, `limit` | `n` |
| `distinct` | `key`, `query` | `values` |
| `aggregate` | `pipeline`, `cursor: {batchSize}` | cursor |
| `explain` | valor = la orden `find` o `count` | `queryPlanner`, `executionStats` |
| `createIndexes` | `indexes: [{key, name, unique}]` | `numIndexesBefore/After` |
| `dropIndexes` | `index` (nombre, `"*"` o clave) | |
| `listIndexes`, `listCollections` | | cursor |
| `listDatabases` | | `databases: [{name, sizeOnDisk, empty}]` |
| `create`, `drop`, `dropDatabase`, `compact` | | |
| `collStats` (`stats`), `dbStats`, `serverStatus` | | estadísticas |
| `authStart` / `authFinish` | usuario; `proof` (binario de 32 bytes) | `nonce`; `serverProof` |
| `shutdown` | | el servidor escribe todo y termina |

**Cursores:** el primer lote son 101 documentos (o `batchSize`) y como mucho 4 MB; el `id` 0
significa que no hay más. Un cursor sin usar 10 minutos caduca (error 43). El cursor guarda las
ranuras de los resultados y la «generación» de cada una: un documento borrado entre dos lotes no
sale, y tampoco el que haya ocupado después su ranura. **Códigos de error**
(los de MongoDB): 2 valor no válido, 13 sin autorización, 18 autenticación fallida, 26 no
existe, 43 cursor, 59 orden desconocida, 66 `_id` inmutable, 85 índice repetido, 10334
documento grande, 11000 clave duplicada; propios: 30001 disco, 30002 BSON mal formado.

## 9. Red en SaguiOS

- **Del PC a SaguiOS:** con la redirección de QEMU (`-nic user,hostfwd=tcp::7979-:7979`) el
  PC llega al servidor de SaguiOS por `127.0.0.1:7979`.
- **De SaguiOS al PC:** el cliente de SaguiOS llega al servidor del PC por `10.0.2.2:7979`;
  basta con que el servidor del PC escuche en `127.0.0.1` (QEMU entrega ahí lo que va a 10.0.2.2).
- **Dentro de SaguiOS:** por `127.0.0.1`, el *loopback* que ya existía (D079): los paquetes a
  127.x no salen a la tarjeta y la tarea de la red los entrega como si hubieran llegado.
- **Espera sin `select`:** el servidor mira cada socket con `disponivel()` y, si ninguno tiene
  nada, duerme en la llamada nueva **`SIS_ESPERAR_REDE` (113)** hasta que la red entregue algo
  a algún socket. Antes solo se podía dormir de 10 en 10 ms y eso limitaba las peticiones
  cortas a ~100 por segundo.
- El servidor es de un solo hilo: un cliente que no lee sus respuestas bloquea el envío (en
  SaguiOS `escrever` espera a que quepa); en el PC, no (sockets no bloqueantes).

## 10. Rendimiento

`make -C litemonkey bench` (PC, 100 000 documentos de ~150 bytes, cliente CPython en la misma
máquina; el cliente Python es el cuello de botella en casi todo):

| Medida | PC |
|---|---|
| `insert_many` por lotes de 1000 | ~97 000 doc/s |
| `insert_one` (una petición cada uno) | ~9 000 op/s |
| `find_one` por `_id` / por índice | ~8 500 op/s |
| `find` por rango de índice | ~220 000 doc/s |
| recorrido completo (`count` con filtro, en el servidor) | ~10 000 000 doc/s |
| `find()` de todos | ~160 000 doc/s |
| `update_many` `$inc` de todos | ~1 000 000 doc/s |
| `create_index` sobre 100 000 documentos | 0,02 s |

En SaguiOS (QEMU con KVM; `tests/h88_litemonkey.py` imprime las cifras) el cliente MicroPython
por *loopback* hace del orden de 3 000 inserciones y 5 000-10 000 lecturas por `_id` por
segundo; desde el PC, por la red de QEMU, unas 2 000-2 500 peticiones cortas por segundo. Las
escrituras grandes están limitadas por el disco (ext2 sobre IDE emulado).

## 11. Pruebas

- `make -C litemonkey test`: `testes/teste_nucleo.c` (138 comprobaciones del núcleo con
  ASan/UBSan: BSON, comparación, regex, filtros, proyección, actualizaciones, upsert, índices,
  planificador, reapertura, compactación, registro cortado y CRC malo, caché limitada,
  agregación) y `testes/teste_cliente.py` (casos del cliente, 8 clientes a la vez, persistencia,
  `kill -9` + registro cortado, compactación, autenticación, tramas basura, cursor con
  borrados y ranuras reutilizadas entre lotes).
- `testes/casos_cliente.py`: 68 casos del cliente; **el mismo archivo** se ejecuta con CPython
  en el PC y con MicroPython en SaguiOS.
- `testes/comparar_mongo.py` (`make -C litemonkey mongo`): las mismas consultas, órdenes y
  agregaciones en lite-monkey y en un MongoDB real, si hay `pymongo` y `mongod`; si no, se omite.
- `tests/h88_litemonkey.py` (QEMU): servidor en SaguiOS y cliente en el PC; cliente y servidor
  en SaguiOS (*loopback*); 5 clientes a la vez; servidor en el PC y cliente en SaguiOS;
  persistencia tras reiniciar la VM con `/home` en disco; rendimiento.

## 12. Límites y lo que falta

- Índices de un solo campo (sin compuestos, ni de texto, ni geográficos, ni TTL); el `sort` se
  hace en memoria aunque haya índice.
- Sin transacciones ni replicación; un solo hilo; las escrituras de una orden son atómicas
  respecto a las demás órdenes (no hay concurrencia dentro del servidor), pero un corte a mitad
  de un `update_many` deja hecho lo que llegó al disco.
- Sin TLS. Un solo usuario.
- Regex sin grupos ni alternativas; `$unwind` solo de campos de primer nivel; las expresiones
  de agregación son pocas.
- Archivos de hasta 4 GB por colección (2 GB en SaguiOS).
- El protocolo no es el de MongoDB (`OP_MSG`): pymongo no puede hablar con lite-monkey, pero sí
  el cliente propio, que imita su API.
