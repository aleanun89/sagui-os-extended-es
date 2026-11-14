# 064 - SQLite 3 para C y Python

- **Estado:** en curso, **v5.3** (empezada para la v5.2, aplazada y puesta en la v5.3 por el mantenedor el 2026-11-08; rama `sqlite`)
- **Prioridad:** media
- **Alta:** 2026-11-08
- **Relacionadas:** 032 (bibliotecas de Python), 040 (`cc`), 035 (bibliotecas compartidas), D034 (MicroPython), D073 (terceros)

## Motivación

Una base de datos de verdad dentro de SaguiOS: guardar datos de programas (agenda, notas,
puntuaciones de SaguiMark, historial del navegador) y aprender SQL en un PC antiguo.
**SQLite es de dominio público**: sin problemas de licencia, ni ahora ni al distribuir.

## Propuesta

1. **Biblioteca `libsqlite3`** a partir de la *amalgamación* oficial (`sqlite3.c`, un solo archivo),
   en `userland/terceros/sqlite/`, registrada en `docs/terceros.md`. Se compila en el
   anfitrión (gcc -m32, freestanding, `-Os`) con:
   - `SQLITE_OS_OTHER=1` y un **VFS propio** (`sqlite_vfs_sagui.c`) sobre las llamadas de
     SaguiOS: abrir, leer y escribir en una posición, tamaño, borrar, existe, y además
     truncar y sincronizar;
   - `SQLITE_THREADSAFE=0` (no hay hilos), sin extensiones cargables y sin `mmap`;
   - memoria con el `malloc` de la libsagui, aleatoriedad con `aleatorio` y hora con el RTC;
   - cerrojos de archivo sencillos. Al principio, un solo proceso escribe a la vez, y está
     documentado.
2. **Kernel:** faltan dos llamadas al sistema, **truncar** un archivo y **sincronizar**
   (vaciar a disco), en FAT32, ext2 y SGFS. Sin truncar no funcionan `VACUUM` ni el diario
   (journal) normal.
3. **C:**
   - `sqlite3.h` y la biblioteca para los programas del sistema, compilados en el anfitrión;
   - un programa **`sqlite3`**, el intérprete de órdenes SQL clásico (`.tables`, `.schema`,
     `.mode`), o uno propio más pequeño;
   - para el `cc` de dentro de SaguiOS hace falta enlazar con bibliotecas (varios `.c` y `.a`),
     previsto en la v6 (040). Hasta entonces, solo los programas compilados en el anfitrión
     pueden usar SQLite.
4. **Python:** módulo **`sqlite3`** en MicroPython, un subconjunto de la DB-API 2.0:
   `connect`, `cursor`, `execute` con parámetros `?`, `executemany`, `fetchone`, `fetchall`,
   `commit`, `rollback`, `close`, `row_factory` sencillo y `with conn:`.
5. **Tamaño:** SQLite ocupa unos 500-700 KB con `-Os` y las opciones de omitir. Hoy la imagen
   está al 71 % de la meta de 6 MB. Si se enlaza estáticamente en `sqlite3` y en `python` se
   duplica, así que conviene una de dos:
   - meterlo solo en Python más un `sqlite3` mínimo;
   - esperar a las bibliotecas compartidas (035).
   O revisar la meta de tamaño.

## Pruebas

- Anfitrión: el mismo SQL en SQLite del anfitrión y en SaguiOS (consultas, `JOIN`, índices,
  transacciones, `ROLLBACK`); las salidas deben coincidir.
- QEMU:
  - crear una base en `/home` (FAT32 y ext2), reiniciar y volver a leerla;
  - `PRAGMA integrity_check`;
  - `VACUUM`;
  - una base creada en el anfitrión se lee en SaguiOS y al revés;
  - Python: `import sqlite3` con parámetros, `fetchall` y `commit`.
- Rendimiento: inserciones por segundo dentro de una transacción (podría entrar en SaguiMark).

## Criterios de aceptación

- [x] Llamadas para truncar y sincronizar en FAT32, ext2 y SGFS
- [x] `libsqlite3` con VFS propio, y programa `sqlite3`
- [x] Módulo `sqlite3` en Python (subconjunto DB-API)
- [x] Bases intercambiables con el anfitrión; `integrity_check` correcto tras reiniciar
- [x] Test automático en `tests/`
- [x] Decisión registrada en `docs/decisoes.md`

## Notas

- Modelo sugerido: **Sonnet medio** para el VFS, el módulo de Python y el programa; el
  truncar y sincronizar del kernel, con cuidado (afecta a FAT32 y ext2).
- Tamaño orientativo: unas 1.500 líneas propias (VFS, módulo, pruebas); SQLite se usa tal cual.

## Estado (hecha en la v5.3, 2026-11-08)

Implementada y probada. Decisión **D116**; documentación en `docs/sqlite.md`; fila en
`docs/terceros.md` (dominio público; la auditoría de licencias dice «todo cuadra»).

- **Kernel:** `SIS_TRUNCAR` 111 y `SIS_SINCRONIZAR` 112 en RAM, FAT32 y ext2.
- **Biblioteca:** SQLite 3.54.1 con VFS propio (`sqlite_vfs_sagui.c`) y libc mínima (`sqlite_libc.c`).
- **Programa** `/bin/sqlite3` y **módulo Python** `sqlite3` (en `python` y `python-x87`).
- **Prueba:** `tests/h87_sqlite.py` (ext2 y FAT32, reinicio, `integrity_check`, `VACUUM`, salida idéntica
  al sqlite3 del anfitrión, bases intercambiadas en los dos sentidos, Python contra CPython, `e2fsck`,
  `fsck.vfat`) y `userland/testes/truncar.c` (truncar/sincronizar en RAM, ext2 y FAT32).
- **Tamaño:** ~530 KB por ejecutable (`sqlite3`, `python`, `python-x87`): la imagen pasa del 57 % al
  76,3 % de la meta de 8 MB. Quedan ~1,6 MB. Si hiciera falta recuperar espacio: quitar el `sqlite3`
  (el módulo de Python ya da SQL) o esperar a las bibliotecas compartidas (035).
- **Criterios de aceptación:** todos cumplidos. Rendimiento (inserciones por segundo) sin medir.
- **Límites:** un solo escritor por base (sin cerrojos de archivo), sin WAL, bases < 2 GB; el asignador
  de la libsagui es de primer ajuste lineal; el módulo de Python no trae `row_factory`.

## Pulido (v5.3.1, 2026-11-08, D118)

Hecho: `row_factory` y `sqlite3.Row` (`con.` y `cursor.`), liberación automática (finalizadores;
200 conexiones soltadas sin fuga), cerrojo entre procesos con la carpeta `base.db-lock` (atómica con
`criar_pasta`, sin tocar el kernel) y `connect(timeout=)` con `database is locked`, medición de
rendimiento (`.bench` en C, `bench.py` en Python, `tests/h89_sqlite_rendimiento.py`; cifras en
`docs/sqlite.md`: p. ej. con KVM 1,5 M inserciones/s en una transacción en C y 588 000 en Python, y 3
confirmaciones/s por fila en ext2). `repr()` de `str` con tildes sale escapado (`'b\xe9'`): está fijo
en `objstrunicode.c` del MicroPython vendorizado, sin opción; documentado. Pendiente: cerrojo de
lectura (compartido) y recuperación de cerrojos huérfanos.
