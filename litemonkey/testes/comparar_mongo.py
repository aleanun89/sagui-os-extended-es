#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""comparar_mongo.py - las mismas consultas en lite-monkey y en MongoDB.

Solo si hay pymongo y un MongoDB al que conectar (MONGO_URI, por defecto
mongodb://127.0.0.1:27017; o "mongod" en el PATH, que se arranca en una
carpeta temporal). Si no, avisa y sale con 0.
  python3 comparar_mongo.py --servidor build/litemonkey
"""
import argparse
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "python"))
import litemonkey  # noqa: E402

DATOS = [{"_id": i, "n": i, "grupo": "abc"[i % 3], "tags": ["t%d" % (i % 4), "x"] if i % 5 else [],
          "sub": {"v": (i * 37) % 100, "s": "texto %d" % i}, "opt": None if i % 7 == 0 else i % 7}
         for i in range(200)]
CONSULTAS = [
    {}, {"n": {"$gt": 150}}, {"n": {"$gte": 10, "$lt": 20}}, {"grupo": "b"}, {"grupo": {"$in": ["a", "c"]}},
    {"grupo": {"$nin": ["a"]}}, {"tags": "t1"}, {"tags": {"$size": 0}}, {"tags": {"$all": ["t2", "x"]}},
    {"sub.v": {"$lte": 10}}, {"sub.s": {"$regex": "^texto 1[0-9]$"}}, {"opt": None}, {"opt": {"$exists": False}},
    {"opt": {"$ne": None}}, {"$or": [{"n": 1}, {"sub.v": 0}]}, {"$and": [{"n": {"$gt": 5}}, {"n": {"$lt": 9}}]},
    {"n": {"$not": {"$gt": 3}}}, {"$nor": [{"grupo": "a"}, {"n": {"$gt": 100}}]},
    {"tags": {"$elemMatch": {"$regex": "^t[23]"}}},
]
ORDENES = [[("n", -1)], [("grupo", 1), ("n", -1)], [("sub.v", 1), ("_id", 1)]]
AGREGACIONES = [
    [{"$group": {"_id": "$grupo", "n": {"$sum": 1}, "s": {"$sum": "$n"}, "m": {"$max": "$sub.v"}}}, {"$sort": {"_id": 1}}],
    [{"$match": {"n": {"$lt": 20}}}, {"$unwind": "$tags"}, {"$group": {"_id": "$tags", "c": {"$sum": 1}}}, {"$sort": {"_id": 1}}],
    [{"$sort": {"n": -1}}, {"$skip": 3}, {"$limit": 4}, {"$project": {"_id": 0, "n": 1, "g": "$grupo"}}],
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--servidor", required=True)
    a = ap.parse_args()
    try:
        import pymongo
    except ImportError:
        print("comparar_mongo: no hay pymongo; se omite la comparación")
        return 0
    tmp = tempfile.mkdtemp(prefix="lmk-mongo-")
    procs = []
    try:
        uri = os.environ.get("MONGO_URI", "mongodb://127.0.0.1:27017")
        if shutil.which("mongod") and "MONGO_URI" not in os.environ:
            s = socket.socket()
            s.bind(("127.0.0.1", 0))
            p = s.getsockname()[1]
            s.close()
            os.makedirs(os.path.join(tmp, "mongo"))
            procs.append(subprocess.Popen(["mongod", "--dbpath", os.path.join(tmp, "mongo"), "--port", str(p),
                                           "--bind_ip", "127.0.0.1"], stdout=subprocess.DEVNULL))
            uri = "mongodb://127.0.0.1:%d" % p
            time.sleep(2)
        try:
            mc = pymongo.MongoClient(uri, serverSelectionTimeoutMS=3000)
            mc.admin.command("ping")
        except Exception as e:  # noqa: BLE001
            print("comparar_mongo: no hay MongoDB en %s (%s); se omite la comparación" % (uri, e))
            return 0
        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        lp = s.getsockname()[1]
        s.close()
        procs.append(subprocess.Popen([a.servidor, "--puerto", str(lp), "--datos", os.path.join(tmp, "lmk")],
                                      stderr=subprocess.DEVNULL))
        time.sleep(0.5)
        lc = litemonkey.Cliente("127.0.0.1", lp)
        m, l = mc.lmk_comparar.datos, lc.lmk_comparar.datos
        m.drop()
        l.drop()
        m.insert_many([dict(d) for d in DATOS])
        l.insert_many([dict(d) for d in DATOS])
        fallos = 0
        for q in CONSULTAS:
            a1 = sorted(d["_id"] for d in m.find(q))
            a2 = sorted(d["_id"] for d in l.find(q))
            if a1 != a2:
                fallos += 1
                print("DISTINTO find(%r): mongo %d, lmk %d" % (q, len(a1), len(a2)))
        for o in ORDENES:
            a1 = [d["_id"] for d in m.find().sort(o)]
            a2 = [d["_id"] for d in l.find().sort(o)]
            if a1 != a2:
                fallos += 1
                print("DISTINTO sort(%r)" % (o,))
        for p in AGREGACIONES:
            a1, a2 = list(m.aggregate(p)), list(l.aggregate(p))
            if a1 != a2:
                fallos += 1
                print("DISTINTO aggregate(%r):\n  mongo %r\n  lmk   %r" % (p, a1, a2))
        print("comparar_mongo: %d consultas, %d diferencias" % (len(CONSULTAS) + len(ORDENES) + len(AGREGACIONES), fallos))
        return 1 if fallos else 0
    finally:
        for p in procs:
            p.terminate()
            p.wait()
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
