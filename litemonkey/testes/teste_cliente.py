#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""teste_cliente.py - pruebas del servidor litemonkey y del cliente Python
con CPython, en el PC (make -C litemonkey test).

Arranca el servidor en un puerto libre con una carpeta temporal y comprueba:
  1. los casos de testes/casos_cliente.py (los mismos que corre SaguiOS);
  2. varios clientes a la vez (hilos);
  3. persistencia: shutdown y arranque con los mismos datos;
  4. corte: kill -9 sin avisar y un registro a medias al final del archivo;
  5. autenticación HMAC-SHA256;
  6. un cliente que manda basura no tumba el servidor.
"""
import argparse
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "python"))
import litemonkey  # noqa: E402
from litemonkey import errors  # noqa: E402

fallos = 0


def ver(cond, msg):
    global fallos
    print(("  ok: " if cond else "  FALLO: ") + msg)
    if not cond:
        fallos += 1


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Servidor:
    def __init__(self, binario, datos, extra=()):
        self.binario, self.datos, self.extra = binario, datos, list(extra)
        self.puerto = puerto_libre()
        self.p = None

    def arrancar(self):
        self.log = open(os.path.join(self.datos + ".log"), "a+")
        self.p = subprocess.Popen([self.binario, "--puerto", str(self.puerto), "--datos", self.datos,
                                   "--escuchar", "127.0.0.1"] + self.extra, stdout=self.log, stderr=self.log)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", self.puerto), 0.2).close()
                return self
            except OSError:
                time.sleep(0.05)
        raise RuntimeError("el servidor no arrancó")

    def texto_log(self):
        if not self.log.closed:
            self.log.flush()
        with open(self.datos + ".log") as f:
            return f.read()

    def parar(self, senal=None):
        if senal:
            self.p.send_signal(senal)
        self.p.wait(10)
        self.log.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--servidor", required=True)
    a = ap.parse_args()
    binario = os.path.abspath(a.servidor)
    tmp = tempfile.mkdtemp(prefix="lmk-")
    datos = os.path.join(tmp, "datos")
    try:
        print("1. casos del cliente")
        s = Servidor(binario, datos).arrancar()
        env = dict(os.environ, PYTHONPATH=os.path.join(AQUI, "..", "python"))
        r = subprocess.run([sys.executable, os.path.join(AQUI, "casos_cliente.py"), "127.0.0.1", str(s.puerto)],
                           capture_output=True, text=True, env=env, timeout=120)
        for l in r.stdout.splitlines():
            if l.startswith("FALLO") or l.startswith("casos:"):
                print("   ", l)
        ver(r.returncode == 0 and "FALLO" not in r.stdout and ", 0 fallos" in r.stdout,
            "casos_cliente.py sin fallos" + ("" if r.returncode == 0 else "\n" + r.stderr[-2000:]))

        print("2. varios clientes a la vez")
        def trabajador(k, res):
            try:
                cl = litemonkey.Cliente("127.0.0.1", s.puerto)
                col = cl.hilos.datos
                for i in range(200):
                    col.insert_one({"hilo": k, "i": i})
                    if i % 20 == 0:
                        assert col.count_documents({"hilo": k}) == i + 1
                res[k] = col.count_documents({"hilo": k})
                cl.close()
            except Exception as e:  # noqa: BLE001
                res[k] = repr(e)
        res = {}
        hilos = [threading.Thread(target=trabajador, args=(k, res)) for k in range(8)]
        for h in hilos:
            h.start()
        for h in hilos:
            h.join(60)
        cl = litemonkey.Cliente("127.0.0.1", s.puerto)
        ver(all(res.get(k) == 200 for k in range(8)) and cl.hilos.datos.count_documents({}) == 1600,
            "8 clientes x 200 inserciones intercaladas: %r" % res)

        print("2b. cursor con borrados y ranuras reutilizadas entre lotes")
        col = cl.cursores.datos
        col.insert_many([{"_id": i, "x": i} for i in range(20)])
        cur = col.find({"x": {"$lt": 100}}).sort("x").batch_size(5)
        primeros = [next(cur)["_id"] for _ in range(5)]
        col.delete_many({"_id": {"$gte": 10}})
        col.insert_many([{"_id": 1000 + i, "x": 1000 + i} for i in range(10)])   # ocupan las ranuras libres
        resto = [d["_id"] for d in cur]
        ver(primeros == list(range(5)) and resto == list(range(5, 10)),
            "getMore no devuelve borrados ni otros documentos en su ranura: %r" % resto)

        print("3. persistencia tras shutdown")
        cl.hilos.datos.create_index("i")
        cl.hilos.datos.update_many({"i": {"$lt": 10}}, {"$set": {"marca": True}})
        cl.shutdown()
        s.parar()
        ver("cerrado" in s.texto_log(), "shutdown: el servidor escribe todo y termina")
        s = Servidor(binario, datos).arrancar()
        cl = litemonkey.Cliente("127.0.0.1", s.puerto)
        col = cl.hilos.datos
        ver(col.count_documents({}) == 1600 and col.count_documents({"marca": True}) == 80, "datos tras reiniciar")
        ver(col.find({"i": 5}).explain()["queryPlanner"]["winningPlan"]["stage"] == "IXSCAN", "el índice sigue tras reiniciar")

        print("4. corte sin avisar")
        col.insert_many([{"corte": i} for i in range(500)])
        col.delete_many({"corte": {"$lt": 100}})
        cl.close()
        s.parar(signal.SIGKILL)
        archivo = os.path.join(datos, "hilos", "datos.lmk")
        with open(archivo, "ab") as f:   # registro a medias, como si se cortara la luz al escribir
            f.write(b"\x01\x40\x00\x00\x00\x40\x00\x00\x00\x02a\x00")
        s = Servidor(binario, datos).arrancar()
        cl = litemonkey.Cliente("127.0.0.1", s.puerto)
        col = cl.hilos.datos
        ver(col.count_documents({"corte": {"$exists": True}}) == 400, "kill -9: lo confirmado sigue ahí (400)")
        ver("se descartan 12 bytes" in s.texto_log(), "aviso del registro cortado en el registro del servidor")
        col.insert_one({"despues": 1})
        ver(col.count_documents({"despues": 1}) == 1, "se puede seguir escribiendo después")
        antes = col.stats()["storageSize"]
        col.delete_many({"corte": {"$exists": True}})
        r = col.compact()
        ver(r["storageSize"] < antes, "compact: %d -> %d bytes" % (antes, r["storageSize"]))
        ver(not os.path.exists(archivo + ".tmp"), "sin archivo temporal tras compactar")
        cl.shutdown()
        s.parar()

        print("5. autenticación")
        datos2 = os.path.join(tmp, "auth")
        s = Servidor(binario, datos2, ["--usuario", "tito", "--clave", "s3cr3to"]).arrancar()
        cl = litemonkey.Cliente("127.0.0.1", s.puerto)
        try:
            cl.a.b.find_one()
            ver(False, "sin autenticar debería fallar")
        except errors.OperationFailure as e:
            ver(e.code == 13, "sin autenticar: error 13")
        try:
            litemonkey.Cliente("127.0.0.1", s.puerto, usuario="tito", clave="mala")
            ver(False, "clave mala debería fallar")
        except errors.AuthenticationFailed:
            ver(True, "clave mala: AuthenticationFailed")
        cl = litemonkey.Cliente("litemonkey://tito:s3cr3to@127.0.0.1:%d" % s.puerto)
        cl.a.b.insert_one({"x": 1})
        ver(cl.a.b.count_documents({}) == 1, "con usuario y clave (URI) funciona")
        cl.shutdown()
        s.parar()

        print("6. basura en la red")
        s = Servidor(binario, datos).arrancar()
        for basura in (b"hola mundo\r\n\r\n", struct.pack("<III", 100, 1, 1) + b"\x05\x00", struct.pack("<III", 0xFFFFFFF0, 1, 1),
                       struct.pack("<III", 22, 1, 1) + struct.pack("<i", 10) + b"\x02a\x00\x10"):
            k = socket.create_connection(("127.0.0.1", s.puerto))
            k.sendall(basura)
            k.settimeout(0.5)
            try:
                k.recv(100)
            except OSError:
                pass
            k.close()
        cl = litemonkey.Cliente("127.0.0.1", s.puerto)
        ver(cl.ping()["ok"] == 1, "el servidor sigue vivo tras tramas rotas")
        cl.shutdown()
        s.parar()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("teste_cliente: %s" % ("todo bien" if not fallos else "%d fallos" % fallos))
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
