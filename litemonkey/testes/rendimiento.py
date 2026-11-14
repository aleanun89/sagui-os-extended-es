# SPDX-License-Identifier: GPL-2.0-or-later
# rendimiento.py - cifras de rendimiento de lite-monkey.
#
#   python rendimiento.py [host] [puerto] [N]          (CPython o MicroPython)
#   python3 rendimiento.py --servidor build/litemonkey  (arranca uno en el PC)
# Mide inserciones por segundo (por lotes y de una en una), búsquedas por _id
# y por índice, un recorrido completo y una actualización masiva.
import sys
import time

try:
    _ns = time.time_ns

    def ahora():
        return _ns() / 1e9
except AttributeError:
    ahora = time.time


def medir(nombre, n, f, unidad="op/s", repetir=False):
    """repetir: f no cambia nada y se repite hasta pasar 0,3 s (el reloj de
    SaguiOS va de 10 en 10 ms)."""
    t0 = ahora()
    f()
    dt = ahora() - t0
    total = n
    while repetir and dt < 0.3:
        f()
        total += n
        dt = ahora() - t0
    n = total
    v = n / dt if dt > 0 else 0
    print("rendimiento: %-34s %10.0f %s  (%d en %.2f s)" % (nombre, v, unidad, n, dt))
    return v


def principal(host, puerto, N):
    import litemonkey
    c = litemonkey.Cliente(host, puerto)
    col = c.rendimiento.docs
    col.drop()
    POCOS = N // 10 if N >= 1000 else N
    lote = []
    for i in range(N):
        lote.append({"_id": i, "k": i % 1000, "nombre": "documento %d" % i, "valor": i * 1.5,
                     "tags": ["a", "b"], "sub": {"x": i, "y": "texto"}})

    def insertar_lotes():
        for i in range(0, N, 1000):
            col.insert_many(lote[i:i + 1000])
    medir("insert_many (lotes de 1000)", N, insertar_lotes, "doc/s")
    otra = c.rendimiento.uno_a_uno
    otra.drop()

    def insertar_uno():
        for i in range(POCOS):
            otra.insert_one({"_id": i, "v": i})
    medir("insert_one (una petición cada uno)", POCOS, insertar_uno)

    def por_id():
        for i in range(POCOS):
            col.find_one({"_id": (i * 7919) % N})
    medir("find_one por _id", POCOS, por_id, repetir=True)
    t0 = ahora()
    col.create_index("k")
    print("rendimiento: %-34s %10.2f s" % ("create_index sobre %d docs" % N, ahora() - t0))

    def por_indice():
        for i in range(POCOS):
            col.find_one({"k": i % 1000})
    medir("find_one por índice", POCOS, por_indice, repetir=True)

    def rango():
        n = 0
        for i in range(10):
            n += len(list(col.find({"k": {"$gte": i * 10, "$lt": i * 10 + 10}}, {"_id": 1})))
        assert n == N // 10, n
    medir("find por rango de índice (1 %)", N // 10, rango, "doc/s", True)

    def recorrido():
        assert col.count_documents({"sub.y": "texto", "valor": {"$gte": 0}}) == N
    medir("recorrido completo (count con filtro)", N, recorrido, "doc/s", True)

    def leer_todo():
        assert len(list(col.find())) == N
    medir("find() de todos (cursor)", N, leer_todo, "doc/s", True)

    def actualizar():
        assert col.update_many({}, {"$inc": {"valor": 1}}).modified_count == N
    medir("update_many $inc de todos", N, actualizar, "doc/s")
    c.rendimiento.docs.drop()
    c.rendimiento.uno_a_uno.drop()
    c.close()


def con_servidor(binario, N):
    import os
    import socket
    import subprocess
    import tempfile
    import shutil
    aqui = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(aqui, "..", "python"))
    tmp = tempfile.mkdtemp(prefix="lmk-rend-")
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    puerto = s.getsockname()[1]
    s.close()
    p = subprocess.Popen([binario, "--puerto", str(puerto), "--datos", tmp, "--escuchar", "127.0.0.1"],
                         stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", puerto), 0.2).close()
                break
            except OSError:
                time.sleep(0.05)
        principal("127.0.0.1", puerto, N)
    finally:
        p.terminate()
        p.wait()
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "--servidor":
        con_servidor(a[1], int(a[2]) if len(a) > 2 else 100000)
    else:
        principal(a[0] if a else "127.0.0.1", int(a[1]) if len(a) > 1 else 7979, int(a[2]) if len(a) > 2 else 10000)
