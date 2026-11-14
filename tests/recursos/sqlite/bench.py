# SPDX-License-Identifier: GPL-2.0-or-later
# Rendimiento del módulo sqlite3 de Python (mejora 064, D118): las mismas pruebas que ".bench"
# del programa sqlite3 (C). Uso: python bench.py [carpeta] [N]
import sys
import time
import sqlite3
d = sys.argv[1] if len(sys.argv) > 1 else "/tmp"
n = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
n_sin = max(10, n // 10)


def cifra(que, ops, t0):
    ms = max(1, time.ticks_diff(time.ticks_ms(), t0))
    print("bench: %s: %d en %d ms = %d por segundo" % (que, ops, ms, ops * 1000 // ms))


c = sqlite3.connect(d + "/bench_py.db")
c.execute("drop table if exists bench_t")
c.execute("create table bench_t(id integer primary key, k integer, v text)")
c.execute("create index bench_k on bench_t(k)")
c.commit()
t0 = time.ticks_ms()
for i in range(n_sin):
    c.execute("insert into bench_t(k, v) values (?, 'valor de prueba')", (i * 7,))
    c.commit()
cifra("insertar sin transaccion (una confirmacion por fila)", n_sin, t0)
t0 = time.ticks_ms()
for i in range(n):
    c.execute("insert into bench_t(k, v) values (?, 'valor de prueba')", (i * 7,))
c.commit()
cifra("insertar en una transaccion", n, t0)
t0 = time.ticks_ms()
hallados = 0
for i in range(n):
    if c.execute("select v from bench_t where id = ?", (i % n_sin + 1,)).fetchone():
        hallados += 1
cifra("consulta por clave primaria", n, t0)
t0 = time.ticks_ms()
for i in range(n):
    if c.execute("select id from bench_t where k = ?", ((i % n_sin) * 7,)).fetchone():
        hallados += 1
cifra("consulta por indice", n, t0)
t0 = time.ticks_ms()
filas = 0
suma = 0
for fila in c.execute("select id, k, v from bench_t"):
    suma += fila[1]
    filas += 1
cifra("recorrido completo (filas leidas)", filas, t0)
c.execute("drop table bench_t")
c.commit()
c.close()
print("bench: comprobacion: %d filas, %d aciertos, suma %d" % (filas, hallados, suma))
