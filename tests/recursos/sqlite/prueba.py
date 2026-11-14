# SPDX-License-Identifier: GPL-2.0-or-later
import sys
import sqlite3
d = sys.argv[1]
c = sqlite3.connect(d + "/p.db")
c.execute("create table t(id integer primary key, n text, v real, b blob)")
c.executemany("insert into t(n,v,b) values(?,?,?)", [("a", 1.5, b"xy"), ("bé", 2, None), ("c", None, b"")])
c.commit()
cur = c.execute("select id, length(n), v, b from t where id>?", (1,))
print(cur.description[0][0], cur.description[1][0], len(cur.description))
print(cur.fetchall())
cur = c.cursor()
cur.execute("insert into t(n) values(:x)", {"x": "zz"})
print(cur.lastrowid, cur.rowcount)
c.commit()
try:
    c.execute("insert into t(id) values(1)")
except sqlite3.IntegrityError:
    print("IntegrityError")
try:
    c.execute("selec 1")
except sqlite3.OperationalError:
    print("OperationalError")
c.rollback()
c.execute("insert into t(n) values('tmp')")
c.rollback()
print(c.execute("select count(*) from t").fetchone())
with c:
    c.execute("update t set v = v * 2 where v is not null")
for r in c.execute("select id, v from t order by id"):
    print(r)
print(c.execute("select length(n), hex(n) from t where id=2").fetchone())
print(c.execute("select 9007199254740993, -5, 'x', x'00ff', null").fetchone())
cur = c.execute("select id from t order by id")
print(cur.fetchone(), cur.fetchmany(2), cur.fetchone(), cur.fetchone())
print(c.execute("select ? + ?", (2, 3.5)).fetchall())
c.close()
print("fin")
