# SPDX-License-Identifier: GPL-2.0-or-later
# valkey.py - cliente pequeño de Valkey para MicroPython de SaguiOS (D477). Habla RESP2 por socket.
#
#   import valkey
#   r = valkey.Valkey()            # 127.0.0.1:6379
#   r.set("a", 1); r.get("a")      # b'1'
#   r.incr("n"); r.rpush("l", "x", "y"); r.lrange("l", 0, -1)
#   r.hset("h", "campo", "valor"); r.hgetall("h")      # {b'campo': b'valor'}
#   r.command("ZADD", "z", 1, "uno")                    # cualquier orden
#   p = r.pubsub(); p.subscribe("canal"); p.get_message()
import socket


class ValkeyError(Exception):
    pass


class Valkey:
    def __init__(self, host="127.0.0.1", port=6379, timeout=10):
        self.s = socket.socket()
        self.s.settimeout(timeout)
        self.s.connect((host, port))
        self.buf = b""

    def close(self):
        self.s.close()

    # --- protocolo
    def _enviar(self, *args):
        partes = [b"*%d\r\n" % len(args)]
        for a in args:
            if isinstance(a, str):
                a = a.encode()
            elif isinstance(a, (bytes, bytearray)):
                pass
            else:
                a = str(a).encode()
            partes.append(b"$%d\r\n" % len(a))
            partes.append(bytes(a))
            partes.append(b"\r\n")
        self.s.sendall(b"".join(partes))

    def _leer_mas(self):
        d = self.s.recv(4096)
        if not d:
            raise ValkeyError("conexion cerrada")
        self.buf += d

    def _linea(self):
        while True:
            i = self.buf.find(b"\r\n")
            if i >= 0:
                l = self.buf[:i]
                self.buf = self.buf[i + 2:]
                return l
            self._leer_mas()

    def _n(self, n):
        while len(self.buf) < n + 2:
            self._leer_mas()
        d = self.buf[:n]
        self.buf = self.buf[n + 2:]
        return d

    def leer(self):
        l = self._linea()
        t, resto = l[:1], l[1:]
        if t == b"+":
            return resto.decode()
        if t == b"-":
            raise ValkeyError(resto.decode())
        if t == b":":
            return int(resto)
        if t == b"$":
            n = int(resto)
            return None if n < 0 else self._n(n)
        if t == b"*":
            n = int(resto)
            return None if n < 0 else [self.leer() for _ in range(n)]
        raise ValkeyError("respuesta rara: %r" % l)

    def command(self, *args):
        self._enviar(*args)
        return self.leer()

    # --- órdenes de uso común
    def ping(self): return self.command("PING")
    def set(self, k, v, *extra): return self.command("SET", k, v, *extra)
    def get(self, k): return self.command("GET", k)
    def delete(self, *k): return self.command("DEL", *k)
    def incr(self, k): return self.command("INCR", k)
    def incrby(self, k, n): return self.command("INCRBY", k, n)
    def expire(self, k, s): return self.command("EXPIRE", k, s)
    def ttl(self, k): return self.command("TTL", k)
    def exists(self, k): return self.command("EXISTS", k)
    def keys(self, patron="*"): return self.command("KEYS", patron)
    def lpush(self, k, *v): return self.command("LPUSH", k, *v)
    def rpush(self, k, *v): return self.command("RPUSH", k, *v)
    def lrange(self, k, a, b): return self.command("LRANGE", k, a, b)
    def hset(self, k, campo, valor): return self.command("HSET", k, campo, valor)
    def hget(self, k, campo): return self.command("HGET", k, campo)
    def hgetall(self, k):
        l = self.command("HGETALL", k)
        return {l[i]: l[i + 1] for i in range(0, len(l), 2)}
    def publish(self, canal, msg): return self.command("PUBLISH", canal, msg)
    def pubsub(self): return PubSub(self)


class PubSub:
    def __init__(self, v):
        self.v = v

    def subscribe(self, *canales):
        self.v._enviar("SUBSCRIBE", *canales)
        return [self.v.leer() for _ in canales]

    def get_message(self):
        # [b'message', canal, datos]
        return self.v.leer()
