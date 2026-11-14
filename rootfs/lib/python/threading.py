# SPDX-License-Identifier: GPL-2.0-or-later
# threading.py - hilos de alto nivel sobre el modulo _thread (D170).
#
# Lo minimo de la biblioteca de CPython: Thread (start, join, is_alive,
# name, ident), Lock, RLock, Event, current_thread y active_count.
#
# Como en el MicroPython, hay un GIL: un solo hilo ejecuta Python a la vez.
# Los hilos sirven para esperar (time.sleep, la red, la entrada) mientras
# otro trabaja; el calculo puro no va mas rapido con varias CPU.
#
# Al acabar el programa principal acaba el proceso con todos sus hilos (a
# diferencia de CPython, que espera a los que no son "daemon"): use join().
import _thread
import time

Lock = _thread.allocate_lock
get_ident = _thread.get_ident

_vivos = {}          # ident -> Thread, los que estan corriendo
_vivos_lock = _thread.allocate_lock()
_contador = 0


def _novo_nome():
    global _contador
    _contador += 1
    return "Thread-%d" % _contador


class RLock:
    """Un cerrojo que el mismo hilo puede tomar varias veces."""

    def __init__(self):
        self._lock = _thread.allocate_lock()
        self._dono = None
        self._contagem = 0

    def acquire(self, blocking=True, timeout=-1):
        yo = _thread.get_ident()
        if self._dono == yo:
            self._contagem += 1
            return True
        if not _tomar(self._lock, blocking, timeout):
            return False
        self._dono = yo
        self._contagem = 1
        return True

    def release(self):
        if self._dono != _thread.get_ident():
            raise RuntimeError("no se puede soltar un RLock que no es tuyo")
        self._contagem -= 1
        if self._contagem == 0:
            self._dono = None
            self._lock.release()

    __enter__ = acquire

    def __exit__(self, *a):
        self.release()


def _tomar(lock, blocking=True, timeout=-1):
    """lock.acquire con tiempo maximo (el _thread no lo trae)."""
    if not blocking:
        return lock.acquire(False)
    if timeout is None or timeout < 0:
        return lock.acquire()
    fin = time.ticks_add(time.ticks_ms(), int(timeout * 1000))
    while not lock.acquire(False):
        if time.ticks_diff(fin, time.ticks_ms()) <= 0:
            return False
        time.sleep_ms(10)
    return True


class Event:
    """Una senal: wait() espera hasta que alguien llame a set()."""

    def __init__(self):
        self._flag = False
        self._lock = _thread.allocate_lock()
        self._lock.acquire()     # tomado mientras la senal no este puesta

    def is_set(self):
        return self._flag

    def set(self):
        if not self._flag:
            self._flag = True
            self._lock.release()

    def clear(self):
        if self._flag:
            self._flag = False
            self._lock.acquire()

    def wait(self, timeout=None):
        if self._flag:
            return True
        if _tomar(self._lock, True, -1 if timeout is None else timeout):
            self._lock.release()   # deja pasar a los demas que esperan
            return True
        return self._flag


class Thread:
    """Thread(target=f, args=(...)).start() ejecuta f(*args) en otro hilo."""

    def __init__(self, group=None, target=None, name=None, args=(), kwargs=None, daemon=None):
        self._target = target
        self._args = args
        self._kwargs = kwargs or {}
        self.name = name or _novo_nome()
        self.daemon = bool(daemon)
        self.ident = None
        self._iniciado = False
        self._fim = _thread.allocate_lock()

    def run(self):
        if self._target is not None:
            self._target(*self._args, **self._kwargs)

    def _inicio(self):
        self.ident = _thread.get_ident()
        with _vivos_lock:
            _vivos[self.ident] = self
        try:
            self.run()
        except SystemExit:
            pass
        except BaseException as e:
            print("Exception in thread %s:" % self.name)
            import sys
            sys.print_exception(e)
        finally:
            with _vivos_lock:
                _vivos.pop(self.ident, None)
            self._fim.release()

    def start(self):
        if self._iniciado:
            raise RuntimeError("threads can only be started once")
        self._iniciado = True
        self._fim.acquire()          # lo suelta el hilo al acabar
        _thread.start_new_thread(self._inicio, ())

    def is_alive(self):
        return self._iniciado and self._fim.locked()

    def join(self, timeout=None):
        if not self._iniciado:
            raise RuntimeError("cannot join thread before it is started")
        if _tomar(self._fim, True, -1 if timeout is None else timeout):
            self._fim.release()

    def __repr__(self):
        return "<Thread(%s, %s)>" % (self.name, "started" if self.is_alive() else "stopped")


class _Principal(Thread):
    def __init__(self):
        super().__init__(name="MainThread")
        self.ident = _thread.get_ident()
        self._iniciado = True
        self._fim.acquire()


_principal = _Principal()


def main_thread():
    return _principal


def current_thread():
    t = _vivos.get(_thread.get_ident())
    return t if t is not None else _principal


def active_count():
    return len(_vivos) + 1


def enumerate():
    return [_principal] + list(_vivos.values())
