# SPDX-License-Identifier: GPL-2.0-or-later
# bson.py - codificación BSON (bsonspec.org) en Python puro.
#
# El mismo código vale para CPython y para el MicroPython de SaguiOS: solo
# usa struct, binascii, os.urandom y time. Tipos:
#   None, bool, int (int32 o int64), float, str, bytes (binario), dict y SON
#   (documentos), list/tuple (arrays), ObjectId, fechas (datetime en
#   CPython; Fecha donde no hay datetime) y Regex.
import struct
import binascii
import time

from .errors import InvalidId, InvalidDocument

try:
    import os
    _azar = os.urandom
except (ImportError, AttributeError):  # pragma: no cover
    import random

    def _azar(n):
        return bytes(random.getrandbits(8) for _ in range(n))

try:
    import datetime as _dt
    _EPOCA = _dt.datetime(1970, 1, 1)
except ImportError:
    _dt = None

_MAX32 = 2147483647
_MAX64 = 9223372036854775807


class ObjectId:
    """Identificador de 12 bytes: 4 de tiempo (s), 5 al azar y 3 de contador."""
    _azar_proceso = _azar(5)
    _contador = int.from_bytes(_azar(3), "big")

    def __init__(self, oid=None):
        if oid is None:
            ObjectId._contador = (ObjectId._contador + 1) & 0xFFFFFF
            t = int(time.time())
            self._b = struct.pack(">I", t & 0xFFFFFFFF) + ObjectId._azar_proceso + \
                struct.pack(">I", ObjectId._contador)[1:]
        elif isinstance(oid, ObjectId):
            self._b = oid._b
        elif isinstance(oid, (bytes, bytearray)) and len(oid) == 12:
            self._b = bytes(oid)
        elif isinstance(oid, str) and len(oid) == 24:
            try:
                self._b = binascii.unhexlify(oid)
            except (ValueError, TypeError):
                raise InvalidId("%r no es un ObjectId válido (24 cifras hexadecimales)" % (oid,))
        else:
            raise InvalidId("%r no es un ObjectId válido (24 cifras hexadecimales)" % (oid,))

    @staticmethod
    def is_valid(oid):
        try:
            ObjectId(oid)
            return True
        except (InvalidId, TypeError):
            return False

    @property
    def binary(self):
        return self._b

    @property
    def generation_time(self):
        s = struct.unpack(">I", self._b[:4])[0]
        if _dt:
            return _EPOCA + _dt.timedelta(seconds=s)
        return Fecha(s * 1000)

    def __str__(self):
        return binascii.hexlify(self._b).decode()

    def __repr__(self):
        return "ObjectId('%s')" % self

    def __eq__(self, o):
        return isinstance(o, ObjectId) and o._b == self._b

    def __ne__(self, o):
        return not self.__eq__(o)

    def __lt__(self, o):
        return self._b < o._b

    def __le__(self, o):
        return self._b <= o._b

    def __gt__(self, o):
        return self._b > o._b

    def __ge__(self, o):
        return self._b >= o._b

    def __hash__(self):
        return hash(self._b)


class Fecha:
    """Fecha BSON (milisegundos desde 1970, UTC) donde no hay datetime."""

    def __init__(self, ms=None):
        self.ms = int(time.time() * 1000) if ms is None else int(ms)

    def __repr__(self):
        t = time.gmtime(self.ms // 1000)
        return "Fecha('%04d-%02d-%02dT%02d:%02d:%02d.%03dZ')" % (t[0], t[1], t[2], t[3], t[4], t[5], self.ms % 1000)

    def __eq__(self, o):
        return isinstance(o, Fecha) and o.ms == self.ms

    def __lt__(self, o):
        return self.ms < o.ms

    def __hash__(self):
        return hash(self.ms)


class Regex:
    """Expresión regular BSON (tipo 0x0B): patrón y opciones ("i", "m", "s")."""

    def __init__(self, pattern, flags=""):
        self.pattern = pattern
        self.flags = flags

    def __repr__(self):
        return "Regex(%r, %r)" % (self.pattern, self.flags)

    def __eq__(self, o):
        return isinstance(o, Regex) and o.pattern == self.pattern and o.flags == self.flags


class Timestamp:
    def __init__(self, time, inc):
        self.time = time
        self.inc = inc

    def __repr__(self):
        return "Timestamp(%d, %d)" % (self.time, self.inc)

    def __eq__(self, o):
        return isinstance(o, Timestamp) and (o.time, o.inc) == (self.time, self.inc)


class Binary:
    """Binario con subtipo distinto de 0 (los bytes normales son el subtipo 0).
    No hereda de bytes: MicroPython no deja heredar de los tipos nativos."""

    def __init__(self, datos, subtype=0):
        self.datos = bytes(datos)
        self.subtype = subtype

    def __eq__(self, o):
        return isinstance(o, Binary) and o.datos == self.datos and o.subtype == self.subtype

    def __len__(self):
        return len(self.datos)

    def __repr__(self):
        return "Binary(%r, %d)" % (self.datos, self.subtype)


class SON:
    """Documento con el orden de las claves garantizado (en MicroPython los
    dict no lo garantizan). SON([("a", 1), ("b", -1)])."""

    def __init__(self, pares=None, **kw):
        self.pares = []
        if pares:
            for k, v in (pares.items() if hasattr(pares, "items") else pares):
                self[k] = v
        for k in kw:
            self[k] = kw[k]

    def __setitem__(self, k, v):
        for i in range(len(self.pares)):
            if self.pares[i][0] == k:
                self.pares[i] = (k, v)
                return
        self.pares.append((k, v))

    def __getitem__(self, k):
        for kk, v in self.pares:
            if kk == k:
                return v
        raise KeyError(k)

    def __contains__(self, k):
        for kk, v in self.pares:
            if kk == k:
                return True
        return False

    def get(self, k, defecto=None):
        return self[k] if k in self else defecto

    def items(self):
        return list(self.pares)

    def keys(self):
        return [k for k, v in self.pares]

    def __len__(self):
        return len(self.pares)

    def __iter__(self):
        return iter(self.keys())

    def to_dict(self):
        return dict(self.pares)

    def __repr__(self):
        return "SON(%r)" % (self.pares,)


# ------------------------------------------------------------ codificación

def _cadena(s):
    b = s.encode("utf-8")
    return struct.pack("<i", len(b) + 1) + b + b"\x00"


def _clave(k):
    if not isinstance(k, str):
        raise InvalidDocument("las claves de un documento son cadenas, no %s" % type(k).__name__)
    b = k.encode("utf-8")
    if b"\x00" in b:
        raise InvalidDocument("una clave no puede tener el carácter NUL")
    return b + b"\x00"


def _ms_de_fecha(v):
    if v.tzinfo is not None:
        v = v.replace(tzinfo=None) - v.utcoffset()
    d = v - _EPOCA
    return (d.days * 86400 + d.seconds) * 1000 + d.microseconds // 1000


def _elemento(k, v, prof):
    c = _clave(k)
    if v is None:
        return b"\x0a" + c
    if v is True or v is False:
        return b"\x08" + c + (b"\x01" if v else b"\x00")
    if isinstance(v, int):
        if -_MAX32 - 1 <= v <= _MAX32:
            return b"\x10" + c + struct.pack("<i", v)
        if -_MAX64 - 1 <= v <= _MAX64:
            return b"\x12" + c + struct.pack("<q", v)
        raise OverflowError("entero demasiado grande para BSON (64 bits)")
    if isinstance(v, float):
        return b"\x01" + c + struct.pack("<d", v)
    if isinstance(v, str):
        return b"\x02" + c + _cadena(v)
    if isinstance(v, (dict, SON)):
        return b"\x03" + c + _documento(v, prof + 1)
    if isinstance(v, (list, tuple)):
        return b"\x04" + c + _documento([(str(i), v[i]) for i in range(len(v))], prof + 1, True)
    if isinstance(v, ObjectId):
        return b"\x07" + c + v._b
    if isinstance(v, Binary):
        return b"\x05" + c + struct.pack("<iB", len(v.datos), v.subtype) + v.datos
    if isinstance(v, (bytes, bytearray, memoryview)):
        v = bytes(v)
        return b"\x05" + c + struct.pack("<iB", len(v), 0) + v
    if isinstance(v, Fecha):
        return b"\x09" + c + struct.pack("<q", v.ms)
    if _dt and isinstance(v, _dt.datetime):
        return b"\x09" + c + struct.pack("<q", _ms_de_fecha(v))
    if isinstance(v, Regex):
        return b"\x0b" + c + v.pattern.encode() + b"\x00" + "".join(sorted(v.flags)).encode() + b"\x00"
    if isinstance(v, Timestamp):
        return b"\x11" + c + struct.pack("<II", v.inc, v.time)
    if hasattr(v, "pattern") and hasattr(v, "flags") and isinstance(getattr(v, "pattern"), str):  # re.compile de CPython
        f = ""
        for letra, bit in (("i", 2), ("m", 8), ("s", 16)):
            if v.flags & bit:
                f += letra
        return b"\x0b" + c + v.pattern.encode() + b"\x00" + f.encode() + b"\x00"
    raise InvalidDocument("no se puede codificar en BSON un %s" % type(v).__name__)


def _documento(d, prof=0, es_lista=False):
    if prof > 32:
        raise InvalidDocument("documento anidado más de 32 niveles")
    partes = []
    pares = d if es_lista else (d.items() if hasattr(d, "items") else d)
    for k, v in pares:
        partes.append(_elemento(k, v, prof))
    cuerpo = b"".join(partes)
    return struct.pack("<i", len(cuerpo) + 5) + cuerpo + b"\x00"


def encode(doc):
    """dict, SON o lista de pares (clave, valor) -> bytes BSON."""
    if isinstance(doc, (list, tuple)):
        return _documento(doc, 0, True)
    return _documento(doc)


codificar = encode


# ------------------------------------------------------------ decodificación

def _fecha(ms):
    if _dt:
        try:
            return _EPOCA + _dt.timedelta(milliseconds=ms)
        except OverflowError:
            return Fecha(ms)
    return Fecha(ms)


def _leer_doc(b, i, es_array, fin_max):
    n = struct.unpack_from("<i", b, i)[0]
    if n < 5 or i + n > fin_max or b[i + n - 1] != 0:
        raise InvalidDocument("BSON mal formado")
    fin = i + n - 1
    i += 4
    res = [] if es_array else {}
    while i < fin:
        t = b[i]
        j = b.find(b"\x00", i + 1)
        if j < 0 or j > fin:
            raise InvalidDocument("BSON mal formado (clave)")
        k = None if es_array else bytes(b[i + 1:j]).decode("utf-8")
        i = j + 1
        if t == 0x01:
            v = struct.unpack_from("<d", b, i)[0]
            i += 8
        elif t == 0x02:
            ln = struct.unpack_from("<i", b, i)[0]
            v = bytes(b[i + 4:i + 3 + ln]).decode("utf-8")
            i += 4 + ln
        elif t == 0x03 or t == 0x04:
            ln = struct.unpack_from("<i", b, i)[0]
            v = _leer_doc(b, i, t == 0x04, fin)
            i += ln
        elif t == 0x05:
            ln = struct.unpack_from("<i", b, i)[0]
            sub = b[i + 4]
            v = bytes(b[i + 5:i + 5 + ln])
            if sub != 0:
                v = Binary(v, sub)
            i += 5 + ln
        elif t == 0x07:
            v = ObjectId(bytes(b[i:i + 12]))
            i += 12
        elif t == 0x08:
            v = b[i] != 0
            i += 1
        elif t == 0x09:
            v = _fecha(struct.unpack_from("<q", b, i)[0])
            i += 8
        elif t == 0x0A or t == 0xFF or t == 0x7F:
            v = None
        elif t == 0x0B:
            j = b.find(b"\x00", i)
            j2 = b.find(b"\x00", j + 1)
            v = Regex(bytes(b[i:j]).decode(), bytes(b[j + 1:j2]).decode())
            i = j2 + 1
        elif t == 0x10:
            v = struct.unpack_from("<i", b, i)[0]
            i += 4
        elif t == 0x11:
            inc, tt = struct.unpack_from("<II", b, i)
            v = Timestamp(tt, inc)
            i += 8
        elif t == 0x12:
            v = struct.unpack_from("<q", b, i)[0]
            i += 8
        else:
            raise InvalidDocument("tipo BSON no soportado: 0x%02x" % t)
        if es_array:
            res.append(v)
        else:
            res[k] = v
    return res


def decode(b, i=0):
    """bytes BSON -> dict."""
    return _leer_doc(b, i, False, len(b))


decodificar = decode


def decode_all(b):
    """Varios documentos seguidos -> lista de dict."""
    res = []
    i = 0
    while i < len(b):
        n = struct.unpack_from("<i", b, i)[0]
        res.append(_leer_doc(b, i, False, len(b)))
        i += n
    return res
