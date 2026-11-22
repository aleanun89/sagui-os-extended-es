# SPDX-License-Identifier: GPL-3.0-or-later
# selectors.py - la interfaz de alto nivel de CPython (DefaultSelector, register, select...)
# sobre el select de SaguiOS (D419).
#
# Sirve cualquier objeto con fileno() y ioctl de sondeo: sockets, archivos de open() y los de
# os.fdopen/subprocess, y también números de descriptor (se envuelven en un archivo que no
# cierra el descriptor). No hay epoll ni kqueue: DefaultSelector, SelectSelector y PollSelector
# son lo mismo, basado en select.select (que duerme en tics de 10 ms entre sondeos).
import select
from collections import namedtuple

EVENT_READ = 1
EVENT_WRITE = 2

SelectorKey = namedtuple("SelectorKey", ["fileobj", "fd", "events", "data"])


def _fd_de(obj):
    if isinstance(obj, int):
        fd = obj
    else:
        try:
            fd = obj.fileno()
        except AttributeError:
            raise ValueError("Invalid file object: " + repr(obj))
    if fd < 0:
        raise ValueError("Invalid file descriptor: " + str(fd))
    return fd


class BaseSelector:
    def register(self, fileobj, events, data=None):
        raise NotImplementedError

    def unregister(self, fileobj):
        raise NotImplementedError

    def modify(self, fileobj, events, data=None):
        self.unregister(fileobj)
        return self.register(fileobj, events, data)

    def select(self, timeout=None):
        raise NotImplementedError

    def close(self):
        pass

    def get_key(self, fileobj):
        m = self.get_map()
        if m is None:
            raise RuntimeError("Selector is closed")
        fd = _fd_de(fileobj)
        if fd in m:
            return m[fd]
        raise KeyError(repr(fileobj) + " is not registered")

    def get_map(self):
        raise NotImplementedError

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


class SelectSelector(BaseSelector):
    def __init__(self):
        self._llaves = {}      # fd -> SelectorKey
        self._objetos = {}     # fd -> el objeto que select entiende

    def get_map(self):
        return self._llaves

    def register(self, fileobj, events, data=None):
        if not events or events & ~(EVENT_READ | EVENT_WRITE):
            raise ValueError("Invalid events: " + repr(events))
        fd = _fd_de(fileobj)
        if fd in self._llaves:
            raise KeyError("%r (FD %d) is already registered" % (fileobj, fd))
        llave = SelectorKey(fileobj, fd, events, data)
        self._llaves[fd] = llave
        if isinstance(fileobj, int):
            # un número de descriptor: select necesita un objeto con ioctl
            self._objetos[fd] = open(fd, "rb", closefd=False)
        else:
            self._objetos[fd] = fileobj
        return llave

    def unregister(self, fileobj):
        fd = _fd_de(fileobj)
        try:
            llave = self._llaves.pop(fd)
        except KeyError:
            raise KeyError(repr(fileobj) + " is not registered")
        del self._objetos[fd]
        return llave

    def select(self, timeout=None):
        if timeout is not None and timeout < 0:
            timeout = 0
        lec = [self._objetos[fd] for fd, k in self._llaves.items() if k.events & EVENT_READ]
        esc = [self._objetos[fd] for fd, k in self._llaves.items() if k.events & EVENT_WRITE]
        if not lec and not esc:
            if timeout:
                import time
                time.sleep(timeout)
            return []
        if timeout is None:
            r, w, _ = select.select(lec, esc, [])
        else:
            r, w, _ = select.select(lec, esc, [], timeout)
        listos = {}
        for o in r:
            listos[o.fileno()] = listos.get(o.fileno(), 0) | EVENT_READ
        for o in w:
            listos[o.fileno()] = listos.get(o.fileno(), 0) | EVENT_WRITE
        res = []
        for fd, ev in listos.items():
            llave = self._llaves.get(fd)
            if llave:
                res.append((llave, ev & llave.events))
        return res

    def close(self):
        self._llaves = {}
        self._objetos = {}


PollSelector = SelectSelector
DefaultSelector = SelectSelector
