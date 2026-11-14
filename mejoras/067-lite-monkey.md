# 067 - lite-monkey: motor NoSQL propio en C, inspirado en MongoDB

- **Estado:** hecha en la rama `lite-monkey` (**v5.4**, D117; manual en `docs/litemonkey.md`)
- **Prioridad:** media-alta
- **Alta:** 2026-11-08
- **Relacionadas:** 064 (SQLite, v5.3: truncar y sincronizar), 066 (clientes MySQL/MongoDB), 065 (web en Python y *loopback*), D046 (sockets)

## Idea

Un motor de base de datos **documental** propio, escrito en **C optimizado**, que funciona igual
en el **PC (Linux/WSL)** y en **SaguiOS**. Escucha en el **puerto 7979**. Viene con un **cliente
Python** que funciona tanto en CPython (PC) como en el MicroPython de SaguiOS. La API se
parece a MongoDB/pymongo, pero es más básica. El nombre: el sagüí es un mono (*monkey*).

## Arquitectura

- **Código en C99 portátil**, en `litemonkey/`:
  - `nucleo/`: documentos, codificación, consultas, actualizaciones, índices, almacenamiento
    y protocolo. No incluye nada del sistema operativo.
  - una capa fina de plataforma: `plataforma_posix.c` (Linux/WSL: sockets BSD, archivos,
    `fsync`) y `plataforma_sagui.c` (llamadas de la libsagui).
- **Servidor `litemonkey`:**
  - de un solo hilo, con bucle de eventos sobre `select` y muchos clientes a la vez;
  - opciones `--puerto 7979` (por defecto), `--datos CARPETA`, `--escuchar DIRECCION`,
    `--sync` y `--usuario/--clave`.
  - En el PC se compila con `make -C litemonkey` (gcc). En SaguiOS es `/bin/litemonkey`,
    compilado por el Makefile principal, con la carpeta de datos por defecto
    `/home/litemonkey` (o `/tmp` si no hay `/home` persistente).

## Modelo de datos

- **Jerarquía:** bases → colecciones → documentos.
- **Tipos:** null, bool, int32/int64, double, string UTF-8, binario, array, documento,
  **ObjectId** (12 bytes, como Mongo) y fecha (ms desde 1970).
- **`_id`** obligatorio y único; si falta, se genera un ObjectId.
- **Codificación:** la de **BSON** (especificación abierta), así el formato en disco y en la red
  es el mismo y es fácil de depurar con herramientas BSON.
- **Límites:** documento de hasta 1 MB y anidamiento de hasta 32 niveles.

## Protocolo (TCP 7979)

- Tramas: `longitud (u32 LE) | id de petición (u32) | código (u32) | documento BSON`.
- Las órdenes son documentos, al estilo de Mongo: `{"insert": "col", "$db": "db",
  "documents": [...]}`. Las respuestas, `{"ok": 1, ...}` o `{"ok": 0, "errmsg": "...", "code": N}`.
- Cursores: `find` devuelve el primer lote y un `cursor_id`; `getMore` y `killCursors`
  piden el resto o lo cierran. Un cursor caduca a los 10 minutos.
- **Versión** en el saludo (`hello`): `litemonkey 1`, con la lista de órdenes disponibles.

## Funcionalidad (más básica que Mongo)

- **CRUD:** `insert` (uno o varios), `find` (filtro, proyección, `sort`, `skip`, `limit`),
  `update` (uno o varios, `upsert`), `replace`, `delete` (uno o varios), `count`, `distinct`.
- **Operadores de consulta:**
  - `$eq $ne $gt $gte $lt $lte $in $nin $exists $and $or $not`;
  - `$regex` sencillo (`^`, `$`, `.`, `*`, `+`, `?`, clases), con la regex de la libsagui o
    una propia;
  - campos anidados con punto (`"direccion.ciudad"`) y comparación con arrays (cualquier
    elemento).
- **Actualizaciones:** `$set $unset $inc $push $pull $rename`.
- **Agregación básica:** `$match $project $sort $skip $limit $group` (con `$sum $avg $min
  $max $count`).
- **Índices:**
  - `_id` siempre;
  - índices de un campo, ascendentes o descendentes, y `unique`;
  - el planificador usa un índice si el filtro tiene igualdad o rango sobre ese campo;
  - `explain` dice cuál.
- **Administración:** `listDatabases`, `listCollections`, `drop`, `dropDatabase`,
  `createIndex`, `dropIndex`, `listIndexes`, `stats`, `compact`, `ping`.
- **Autenticación opcional:** usuario y clave con desafío y respuesta **HMAC-SHA256**; la clave
  nunca viaja en claro. Sin TLS en la primera versión, así que hay que avisar de usarlo solo en
  la red local.

## Almacenamiento (rápido y seguro ante cortes)

- Un archivo por colección (`base/coleccion.lmk`), **solo de añadir**: cada registro es
  `[tipo inserción/actualización/borrado | longitud | documento | CRC32]`.
- **Al abrir** se recorre el archivo y se reconstruyen en memoria el índice de `_id`
  (tabla hash) y los demás índices (árbol B o vector ordenado). Un registro final cortado o
  con CRC malo, típico de un corte de luz, se descarta y se avisa.
- **Compactación:** cuando más de la mitad del archivo es basura, y con la orden `compact`, se
  escribe un archivo nuevo y se **renombra** encima. Así no hace falta truncar.
- **Durabilidad:**
  - `--sync`: sincroniza tras cada escritura (lento y seguro);
  - por defecto, cada 1 s o cada N escrituras;
  - en SaguiOS, `sincronizar` llega con la 064 (v5.3). Mientras tanto, el cierre de archivo.
- **Caché:** documentos en memoria hasta un límite configurable (`--memoria MB`, por defecto 16
  en SaguiOS y 256 en el PC). El resto se lee del disco por desplazamiento.

## Cliente Python `litemonkey`

- Paquete puro en Python (`litemonkey/python/litemonkey/`), **el mismo código para CPython y
  MicroPython**: `/lib/python/litemonkey/` en SaguiOS e instalable con `pip install -e` en el PC.
- **API parecida a pymongo:**
  - `cliente = litemonkey.Cliente("10.0.2.2", 7979)` (alias `MongoClient`);
  - `cliente["tienda"]["productos"]` o `cliente.tienda.productos`;
  - `insert_one`/`insert_many` (con `inserted_id(s)`), `find` (cursor iterable con `.sort()`,
    `.skip()`, `.limit()`), `find_one`, `update_one`/`update_many`, `replace_one`,
    `delete_one`/`delete_many`, `count_documents`, `distinct`, `aggregate`, `create_index`,
    `drop`, `list_collection_names`;
  - clases `ObjectId` y `errors.DuplicateKeyError`.
- **BSON** propio en Python, para codificar y decodificar.
- **Herramienta `lmk`:** una consola interactiva sencilla (en Python) para hacer consultas a mano.

## Red en SaguiOS

- **Del PC a SaguiOS y al revés:** el servidor en SaguiOS se alcanza desde el PC con la
  redirección de puertos de QEMU (`hostfwd=tcp::7979-:7979`). Un cliente en SaguiOS llega
  al servidor del PC por `10.0.2.2:7979`.
- **Cliente y servidor en el mismo SaguiOS:** por **loopback (127.0.0.1)**, que **ya existía**
  desde D079 (la cola de `rede.c` que entrega a la propia pila los paquetes para 127.x): no hubo
  que tocar la pila, solo probarlo (h88). Para que un servidor espere en muchos sockets sin
  sondear de 10 en 10 ms se añadió `SIS_ESPERAR_REDE` (113, D117).

## Pruebas

- **PC (sin QEMU):** pruebas unitarias del motor en C (codificación, consultas, índices,
  recuperación tras un archivo cortado y compactación), y del cliente Python con CPython contra
  el servidor del PC. **Comparación con MongoDB**, si está instalado, para un conjunto de
  consultas.
- **QEMU (`h88_litemonkey`):**
  - servidor en SaguiOS y cliente desde el PC;
  - servidor en el PC y cliente Python en SaguiOS;
  - cliente y servidor dentro de SaguiOS (*loopback*);
  - persistencia tras reiniciar con `/home` persistente;
  - varios clientes a la vez.
- **Rendimiento:** inserciones por segundo, búsquedas por `_id` y por índice, y un recorrido
  completo; en el PC y en SaguiOS.

## Criterios de aceptación

- [x] Servidor en Linux/WSL y en SaguiOS, puerto 7979, varios clientes a la vez
- [x] CRUD, operadores, índices, agregación básica y cursores
- [x] Recuperación tras corte (CRC) y compactación
- [x] Cliente Python idéntico en CPython y MicroPython, y consola `lmk`
- [x] *Loopback* mínimo en SaguiOS
- [x] Tests en el PC y `tests/h88_litemonkey.py`
- [x] Decisión registrada en `docs/decisoes.md` (D117) y documentación en `docs/litemonkey.md`

## Cómo quedó (D117)

- **Hecho:** todo lo de arriba. Además de lo pedido: `$size`, `$all`, `$elemMatch`, `$mul`,
  `$min`, `$max`, `$addToSet`, `$pop`, `$setOnInsert`, `$addFields`, `$unwind` y `$count`,
  índices multiclave, `explain` con claves y documentos examinados, `serverStatus` y
  `shutdown`.
- **Loopback:** ya existía desde D079 (la mejora lo daba por pendiente); h88 lo prueba con
  cliente y servidor en el mismo SaguiOS. Lo que sí hizo falta en el kernel fue
  `SIS_ESPERAR_REDE` (113), para que un servidor con muchos sockets duerma hasta que llegue
  algo sin hacer sondeo de 10 en 10 ms.
- **Sincronizar en SaguiOS:** usa `SIS_SINCRONIZAR` en cuanto exista (064); mientras tanto,
  cierra y reabre el archivo.
- **Comparación con MongoDB:** `make -C litemonkey mongo` (`testes/comparar_mongo.py`) existe
  pero en el PC de desarrollo no había `mongod` ni Docker integrado en el WSL: no se ha podido
  ejecutar. Queda pendiente pasarla una vez donde haya MongoDB.
- **Pendiente / fuera de alcance:** índices compuestos, TLS, varios usuarios, `$lookup` y
  grupos/alternativas en las regex (ver «Límites» en el manual).

## Estado de la rama (parada el 2026-11-08; resuelto el 2026-11-08)

La rama se paró a medias y se retomó después de publicar la v5.3. Lo que quedaba pendiente
está hecho:

- fusión de `main` (v5.3, SQLite): `SIS_TRUNCAR` 111 y `SIS_SINCRONIZAR` 112 conviven con
  `SIS_ESPERAR_REDE` 113; `plataforma_sagui.c` sincroniza con `sincronizar()` (ya no cierra y
  reabre el archivo);
- h16 y h24 cuentan los programas de `/bin` con la lista real de la imagen
  (`build/rootfs/bin`), que incluye `/bin/litemonkey` y el script `/bin/lmk`;
- cursores: un documento borrado entre lotes, o el que reutiliza su ranura, ya no sale en el
  `getMore` (generación por ranura); pruebas en `teste_nucleo.c` y `teste_cliente.py`;
- limpiezas en `bson_camino_simple` y `$push`.

Sigue sin poder ejecutarse `make -C litemonkey mongo` (no hay MongoDB en el PC de desarrollo).
