# SPDX-License-Identifier: GPL-3.0-or-later
# resource.py - uso de recursos y límites, con la interfaz del resource de CPython (D419).
#
# Lo que SaguiOS sabe de verdad:
#   - getrusage(RUSAGE_SELF): el tiempo de CPU del proceso (ru_utime, en segundos; el kernel no
#     separa usuario y sistema: ru_stime es 0) y su memoria (ru_maxrss, en KB: la que usa ahora,
#     no el máximo histórico). Los demás contadores (fallos de página, cambios de contexto...) no
#     los lleva el kernel y valen 0. RUSAGE_CHILDREN no se acumula: da todo ceros.
#   - getrlimit: RLIMIT_NOFILE = 32 (la tabla de descriptores), RLIMIT_STACK = 8 MB (la pila del
#     programa) y RLIMIT_CORE = 0 (no hay volcados de memoria). El resto da RLIM_INFINITY: el
#     sistema no impone ahí un límite por proceso.
#   - setrlimit: SaguiOS no tiene límites que se puedan cambiar. Pedir el valor que ya hay no hace
#     nada; cualquier otro da OSError(EPERM).
import os
from collections import namedtuple

error = OSError

RUSAGE_SELF = 0
RUSAGE_CHILDREN = -1
RUSAGE_THREAD = 1

RLIM_INFINITY = -1
RLIMIT_CPU = 0
RLIMIT_FSIZE = 1
RLIMIT_DATA = 2
RLIMIT_STACK = 3
RLIMIT_CORE = 4
RLIMIT_RSS = 5
RLIMIT_NPROC = 6
RLIMIT_NOFILE = 7
RLIMIT_MEMLOCK = 8
RLIMIT_AS = 9

struct_rusage = namedtuple("struct_rusage", [
    "ru_utime", "ru_stime", "ru_maxrss", "ru_ixrss", "ru_idrss", "ru_isrss", "ru_minflt", "ru_majflt",
    "ru_nswap", "ru_inblock", "ru_oublock", "ru_msgsnd", "ru_msgrcv", "ru_nsignals", "ru_nvcsw", "ru_nivcsw"])

_LIMITES = {
    RLIMIT_NOFILE: (32, 32),
    RLIMIT_STACK: (8 * 1024 * 1024, 8 * 1024 * 1024),
    RLIMIT_CORE: (0, 0),
}


def getrusage(quien):
    if quien == RUSAGE_SELF or quien == RUSAGE_THREAD:
        cpu_ms, memoria_kb, _ = os._infoproc()
        return struct_rusage(cpu_ms / 1000, 0.0, memoria_kb, *([0] * 13))
    if quien == RUSAGE_CHILDREN:
        return struct_rusage(0.0, 0.0, *([0] * 14))
    raise ValueError("invalid who parameter")


def getrlimit(recurso):
    if not isinstance(recurso, int) or recurso < 0 or recurso > RLIMIT_AS:
        raise ValueError("invalid resource specified")
    return _LIMITES.get(recurso, (RLIM_INFINITY, RLIM_INFINITY))


def setrlimit(recurso, limites):
    actual = getrlimit(recurso)
    nuevo = tuple(limites)
    if len(nuevo) != 2:
        raise ValueError("expected a tuple of 2 integers")
    if nuevo == actual:
        return
    raise OSError(1, "SaguiOS no permite cambiar los límites de recursos")


def getpagesize():
    return 4096
