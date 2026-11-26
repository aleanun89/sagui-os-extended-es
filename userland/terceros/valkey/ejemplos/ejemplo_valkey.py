# ejemplo_valkey.py - cliente de Valkey en MicroPython (D477): python ejemplo_valkey.py [host [puerto]]
import sys
import valkey

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
puerto = int(sys.argv[2]) if len(sys.argv) > 2 else 6379
r = valkey.Valkey(host=host, port=puerto)
print("PING", r.ping())
r.delete("py:n", "py:l", "py:h")
r.set("py:clave", "valor desde MicroPython")
print("GET", r.get("py:clave"))
r.incr("py:n"); r.incrby("py:n", 41)
print("INCR", r.get("py:n"))
r.rpush("py:l", "a", "b", "c")
print("LRANGE", r.lrange("py:l", 0, -1))
r.hset("py:h", "nombre", "SaguiOS")
print("HGETALL", r.hgetall("py:h"))
r.set("py:t", "x"); r.expire("py:t", 100)
print("TTL", 0 < r.ttl("py:t") <= 100)
r.close()
