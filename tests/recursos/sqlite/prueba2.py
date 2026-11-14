# SPDX-License-Identifier: GPL-2.0-or-later
# row_factory, liberación automática y cerrojo entre procesos del módulo sqlite3 (D118).
# Uso: python prueba2.py CARPETA            (todo lo que no necesita dos procesos)
#      python prueba2.py CARPETA escritor   (abre una escritura, la mantiene unos segundos)
#      python prueba2.py CARPETA intento    (intenta escribir con timeout corto y largo)
import sys
import sqlite3
import time
import gc

d = sys.argv[1]
modo = sys.argv[2] if len(sys.argv) > 2 else ""

if modo == "escritor":
    c = sqlite3.connect(d + "/l.db")
    c.execute("create table if not exists t(x)")
    c.commit()
    c.execute("insert into t values (1)")     # abre la transacción: toma el cerrojo
    print("escritor: con el cerrojo")
    time.sleep(6)
    c.commit()
    c.close()
    print("escritor: libre")
    sys.exit()

if modo == "intento":
    import os
    for i in range(300):                      # esperar a que el escritor tenga el cerrojo
        try:
            os.stat(d + "/l.db-lock")
            break
        except OSError:
            time.sleep(0.1)
    c = sqlite3.connect(d + "/l.db", timeout=0.2)
    try:
        c.execute("insert into t values (2)")
        print("intento: INSERT SIN ERROR (mal)")
    except sqlite3.OperationalError as e:
        print("intento corto:", e)
    c.close()
    c = sqlite3.connect(d + "/l.db", timeout=30)
    c.execute("insert into t values (3)")       # espera a que el escritor termine
    c.commit()
    print("intento largo: ok", c.execute("select count(*) from t").fetchone()[0])
    c.close()
    sys.exit()

# ---- row_factory
c = sqlite3.connect(d + "/r.db")
c.execute("create table if not exists p(id integer primary key, nombre text, edad integer)")
c.execute("delete from p")
c.executemany("insert into p(nombre, edad) values (?, ?)", [("Ana", 30), ("Luis", 25)])
c.commit()
print(c.row_factory)
c.row_factory = sqlite3.Row
f = c.execute("select id, nombre, edad from p order by id").fetchone()
print(f[0], f["nombre"], f["NOMBRE"], f[-1], f.keys(), len(f), tuple(f), list(f))
print(f[1:], f == f, f != f)
try:
    f["nada"]
except IndexError as e:
    print("IndexError", e)
rows = c.execute("select * from p order by id").fetchall()
print([r["nombre"] for r in rows], rows[0] == rows[1], rows[0] == c.execute("select * from p order by id").fetchone())
print(rows[0] == (1, "Ana", 30))
n, e = rows[1]["nombre"], rows[1]["edad"]
print(n, e)
cur = c.cursor()
cur.row_factory = lambda cursor, fila: {d[0]: v for d, v in zip(cursor.description, fila)}
print(cur.execute("select nombre, edad from p order by id").fetchall())
cur2 = c.cursor()
cur2.row_factory = None
print(cur2.execute("select nombre from p order by id").fetchone())
c.row_factory = lambda cursor, fila: fila[0]
print(c.execute("select edad from p order by id").fetchall())
c.row_factory = None
print(c.execute("select nombre from p order by id").fetchone())
c.close()

# ---- liberación automática: se sueltan 200 conexiones y cursores sin close()
base = d + "/g.db"
c = sqlite3.connect(base)
c.execute("create table if not exists g(x integer)")
c.execute("delete from g")
c.executemany("insert into g values (?)", [(i,) for i in range(200)])
c.commit()
c.close()
def ciclo(veces):
    for i in range(veces):
        k = sqlite3.connect(base)
        cu = k.execute("select * from g")
        cu.fetchone()          # sentencia a medias, sin cerrar nada
        k.execute("create table if not exists h(x)")
        k = None
        cu = None


ciclo(30)                      # calentar: el área del malloc llega a su tamaño de régimen
gc.collect()
memoria = getattr(sqlite3, "_memoria_total", lambda: 0)   # solo existe en SaguiOS
antes = memoria()
ciclo(200)
gc.collect()
despues = memoria()
print("200 conexiones soltadas sin close(): el area del malloc no crece:", despues - antes < 262144)
# cerrar a mano y luego dejar que el finalizador pase: sin doble liberación
for i in range(50):
    k = sqlite3.connect(base)
    cu = k.cursor()
    cu.execute("select * from g")
    cu.close()
    k.close()
    k.close()
    k = None
    cu = None
gc.collect()
k = sqlite3.connect(base)
print(k.execute("select count(*) from g").fetchone()[0], k.execute("pragma integrity_check").fetchone()[0])
k.close()
print("fin")
