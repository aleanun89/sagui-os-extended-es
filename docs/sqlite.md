# SQLite en SaguiOS (mejora 064, D116)

SQLite 3.54.1 (dominio público) en tres sitios: el programa `/bin/sqlite3`, el módulo `sqlite3` de
`python` / `python-x87` y la biblioteca (`userland/terceros/sqlite/`, para programas compilados en el
anfitrión; el `cc` de dentro todavía no enlaza bibliotecas).

## Programa `sqlite3`

```
sqlite3 /home/datos.db "create table t(x, y)" "insert into t values (1, 'a')" "select * from t"
sqlite3 /home/datos.db < guion.sql
sqlite3 -csv -header /home/datos.db "select * from t"
sqlite3                       # interactivo, base temporal en memoria; ".open archivo" para una real
```

Órdenes: `.tables [patrón]`, `.schema [tabla]`, `.mode list|column|csv|line|tabs`, `.headers on|off`,
`.separator SEP`, `.nullvalue TEXTO`, `.read archivo`, `.open archivo`, `.help`, `.quit`.

## Python

```python
import sqlite3
con = sqlite3.connect("/home/datos.db")
con.execute("create table if not exists t(id integer primary key, n text)")
con.executemany("insert into t(n) values (?)", [("a",), ("b",)])
con.commit()
for fila in con.execute("select id, n from t where id > ?", (0,)):
    print(fila)
con.close()
```

Subconjunto de la DB-API 2.0: ver D116 y D118. `with con:` confirma (o deshace si hubo excepción) pero
no cierra la conexión.

- **`row_factory`:** `con.row_factory = sqlite3.Row` (o en un cursor) da filas con `fila[0]`,
  `fila["nombre"]` (sin distinguir mayúsculas ASCII), `fila.keys()`, `len`, iteración e igualdad; también
  vale cualquier función `(cursor, fila)`; `None` devuelve tuplas. El cursor hereda el de la conexión.
- **Liberación automática (D118):** una conexión o un cursor sin `close()` se cierran solos cuando el
  recolector los recoge (finalizador); cerrar a mano y luego soltar no libera dos veces. Como SQLite usa
  `malloc` y no el montón de Python, `connect()` recoge basura cuando hay 8 conexiones abiertas y los
  cursores con filas pendientes (un `fetchone()` deja la sentencia viva) provocan una recolección al
  pasar de 32; así, abrir y soltar 200 conexiones en un bucle no hace crecer la memoria (h87). Aun así,
  lo correcto es `close()` (o `contextlib.closing`).
- **`timeout`:** `connect(ruta, timeout=5.0)` reintenta durante ese tiempo si otro proceso tiene la base
  bloqueada, y después lanza `OperationalError: database is locked`.
- **`repr()` con tildes:** en este MicroPython `repr('bé')` da `'b\xe9'` (CPython: `'bé'`); `print` y
  `str` salen bien. Está fijo en `py/objstrunicode.c` (no hay opción de configuración) y esa carpeta
  se regenera desde el MicroPython oficial, así que no se toca; es solo cosmético.

## Cerrojo entre procesos (D118)

El kernel no tiene creación exclusiva de archivos ni `flock`, pero `criar_pasta` es atómico. El VFS usa
la **carpeta** `base.db-lock` como cerrojo del escritor: se crea al pasar a RESERVED y se borra al
volver a SHARED/NONE o al cerrar. Un segundo proceso que intenta escribir recibe `database is locked`
(`SQLITE_BUSY`), o espera si se pidió `timeout`. Probado con dos procesos en ext2 y FAT32.

- Los **lectores no toman cerrojo**: no se bloquean entre sí, pero uno que coincide con un commit puede
  ver páginas a medias (error de lectura o datos del instante anterior).
- **Cerrojos huérfanos:** si un proceso muere con una escritura abierta queda la carpeta
  `base.db-lock` (y un `base.db-journal`): hay que borrarla a mano (`rmdir base.db-lock`); la siguiente
  apertura deshará el diario. Todos los accesos a la base deben pasar por este VFS (una base abierta
  desde otro sistema operativo no respeta el cerrojo).

## Rendimiento (h89, `.bench` del programa `sqlite3` y `tests/recursos/sqlite/bench.py`)

Filas por segundo (el reloj del sistema tiene resolución de 10 ms: con N grande es fiable al ~5 %).
N = 200 000 en RAM (`/tmp`), 2 000 en ext2 sobre un pendrive USB (QEMU, disco virtual). «Sin
transacción» = una confirmación (con escritura del diario y sincronización) por fila. Medido el
2026-11-08, QEMU en un PC de desarrollo (WSL2).

| medida | C, KVM | Python, KVM | C, sin KVM | Python, sin KVM |
|---|---|---|---|---|
| insertar sin transacción, RAM | 133 000 | 100 000 | 13 900 | 9 800 |
| insertar en una transacción, RAM | 1 540 000 | 588 000 | 150 000 | 52 800 |
| consulta por clave primaria, RAM | 1 250 000 | 435 000 | 120 000 | 32 100 |
| consulta por índice, RAM | 1 110 000 | 144 000 | 109 000 | 15 200 |
| recorrido completo, RAM | 22 000 000 | 2 440 000 | 1 375 000 | 206 000 |
| insertar sin transacción, ext2 | 3 | 3 | 3 | 3 |
| insertar en una transacción, ext2 | 4 760 | 4 650 | 4 550 | 4 260 |
| consultas y recorrido, ext2 (caché) | > 100 000 | > 25 000 | > 100 000 | > 22 000 |

Lo que más cuesta es la confirmación en el disco: unos 250-330 ms por commit en ext2 (diario +
sincronización con el retraso de 1 s de la escritura de datos y la espera del dispositivo), así que
conviene agrupar las escrituras en transacciones (de 3 a 4 500 filas por segundo). Las cifras de
consultas en ext2 son un límite inferior (la medida cabe en 1-2 ticks de 10 ms).

## Límites

- **Un solo escritor por base**, ahora con el cerrojo de arriba (sin cerrojo de lectura).
- Sin WAL (diario normal, `journal_mode=DELETE`), sin `mmap`, sin extensiones cargables, sin UTF-16.
- Bases de menos de 2 GB. Las bases son intercambiables con el anfitrión (mismo formato).
- Cada ejecutable que lo incluye pesa unos 530 KB más.
