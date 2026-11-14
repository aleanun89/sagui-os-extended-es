# 066 - Clientes de MySQL/MariaDB y MongoDB para Python

- **Estado:** propuesta para **mucho más adelante** (sin versión; decidido por el mantenedor el 2026-11-08)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 032 (bibliotecas de Python), 027 (libtls), D046 (sockets), 064 (SQLite), 065 (web en Python, v8)

## Motivación

Conectarse desde Python en SaguiOS a bases de datos de otro equipo de la red, o a una en la nube:
**MySQL/MariaDB** (como PyMySQL) y **MongoDB** (como pymongo). La base de datos corre fuera; en
SaguiOS solo va el cliente.

## ¿Funcionan PyMySQL y pymongo tal cual?

- **PyMySQL** (MIT) está escrito solo en Python, pero usa partes de CPython que MicroPython no
  tiene: `datetime` y `decimal` completos, `hmac`, `functools`, `warnings` y codificaciones.
  Hace falta una **versión adaptada** con la misma API.
- **pymongo** (Apache-2.0) es grande. Depende de `bson` (con partes en C), `dnspython`, hilos
  para vigilar el servidor y muchos módulos de CPython. **No funciona en MicroPython.** Hace
  falta un **cliente propio** con la API básica de pymongo.

## Lo que ya tenemos

| Pieza | Estado |
| --- | --- |
| Sockets TCP (`socket`, `select`) | sí (`rede_sagui.c`, D046) |
| `struct`, `json`, `re`, `binascii`, `random` | sí |
| `hashlib`: md5, sha1, sha256, sha384, sha512 | sí (`hashlib_sagui.c`) |
| TLS cliente (`ssl` sobre la libtls, con certificados) | sí (`ssl_sagui.c`, 027) |
| `hmac`, PBKDF2 | **no**: hay que escribirlos en Python (son cortos) |
| `datetime`/`decimal` para los tipos de las bases | **no**: versiones mínimas |
| DNS **SRV/TXT** (para `mongodb+srv://`, MongoDB Atlas) | **no**: el resolutor solo pide A |
| RSA para cifrar la contraseña sin TLS (MySQL 8) | **no**: con TLS no hace falta |

## Propuesta

**1. `pymysql` adaptado** (`/lib/python/pymysql/`), compatible con la API de PyMySQL y la DB-API:
- protocolo cliente/servidor de MySQL 4.1+:
  - saludo inicial;
  - autenticación **`mysql_native_password`** (SHA1) y **`caching_sha2_password`** (SHA256,
    el de MySQL 8). La autenticación completa de este último va por **TLS**; sin TLS, error
    claro o RSA en una segunda fase;
  - `COM_QUERY` y resultados en texto;
- `connect()`, `cursor()`, `execute()` con parámetros `%s` correctamente escapados,
  `executemany`, `fetchone`/`fetchmany`/`fetchall`, `commit`/`rollback`, `DictCursor`,
  `autocommit`, `ssl=`;
- tipos: int, float, str, bytes y None, y fechas como `datetime` mínimo.

**2. Cliente MongoDB** (`/lib/python/pymongo/`, más un `bson` propio):
- **BSON** completo para codificar y decodificar: documentos, arrays, string, int32/int64,
  double, bool, null, ObjectId, fecha y binario;
- protocolo **OP_MSG** (MongoDB 3.6+);
- autenticación **SCRAM-SHA-256** (y SHA-1), con `hmac` y PBKDF2 propios;
- TLS con `tls=True`;
- API básica de pymongo:
  - `MongoClient(uri)`, `client.db.coleccion`;
  - `insert_one`/`insert_many`, `find` (filtro, proyección, `sort`, `limit`, `skip`, cursor
    con `getMore`), `find_one`;
  - `update_one`/`update_many`, `delete_one`/`delete_many`, `count_documents`;
  - `aggregate` sencillo, `create_index` y `list_collection_names`.
- Sin hilos de vigilancia y con un solo servidor. Sin conjuntos de réplicas, salvo conectarse
  al primario indicado.
- `mongodb+srv://` (Atlas) necesita **consultas DNS SRV y TXT**, que hay que añadir al
  resolutor. Sin eso, solo direcciones `mongodb://host:puerto`.

## Pruebas

- Anfitrión: MariaDB y MongoDB en contenedores (o instalados) en el PC de desarrollo, a los que
  SaguiOS llega por `10.0.2.2`:
  - crear tablas y colecciones, insertar, consultar, actualizar y borrar;
  - parámetros con caracteres peligrosos (inyección SQL);
  - transacciones;
  - autenticación con y sin TLS;
  - comparar resultados con PyMySQL y pymongo del anfitrión.
- Sin servidores disponibles: servidores falsos en Python que responden el protocolo (como el
  falso de OpenAI de la 043).

## Criterios de aceptación

- [ ] `pymysql`: conectar a MariaDB 10+ y MySQL 8 (con TLS), consultas con parámetros, `DictCursor`
- [ ] `pymongo`: CRUD, cursores largos, SCRAM-SHA-256, TLS
- [ ] Ejemplos en `/ejemplos/bases-de-datos/`
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Modelo sugerido: **Sonnet medio**. Tamaño orientativo:
  - `pymysql`: unas 1.200 líneas de Python;
  - `bson` y `pymongo`: unas 1.800;
  - DNS SRV/TXT: unas 200 de C.
- Aplazada a mucho más adelante. Mientras tanto, la base documental propia es lite-monkey (067, v5.4).
