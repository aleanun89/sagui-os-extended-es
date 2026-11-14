# SPDX-License-Identifier: GPL-2.0-or-later
# python_hilos.py - pruebas de los hilos de Python (D170, tests/h141).
#
#     python /testes/python_hilos.py            todas las pruebas
#     python /testes/python_hilos.py salir-sleep    sale con un hilo dormido
#     python /testes/python_hilos.py salir-calculo  sale con un hilo calculando
#     python /testes/python_hilos.py entrada        un hilo lee una linea, el principal cuenta
#
# Cada prueba escribe "HILOSPY ok <nombre>" o "HILOSPY FALLO <nombre>: ...";
# al final, "RESULTADO python_hilos: <n> fallos".
import sys
import time
import gc
import _thread
import threading

fallos = 0


def comprobar(cond, nombre, detalle=""):
    global fallos
    if cond:
        print("HILOSPY ok", nombre)
    else:
        fallos += 1
        print("HILOSPY FALLO %s: %s" % (nombre, detalle))


def esperar(cond, ms=10000):
    fin = time.ticks_add(time.ticks_ms(), ms)
    while not cond():
        if time.ticks_diff(fin, time.ticks_ms()) <= 0:
            return False
        time.sleep_ms(5)
    return True


def p_start_new_thread():
    vistos = []
    lock = _thread.allocate_lock()

    def f(n, extra=0):
        with lock:
            vistos.append((n, extra, _thread.get_ident()))

    for i in range(3):
        _thread.start_new_thread(f, (i,), {"extra": i * 10})
    ok = esperar(lambda: len(vistos) == 3)
    ids = set(v[2] for v in vistos)
    comprobar(ok and sorted((a, b) for a, b, _ in vistos) == [(0, 0), (1, 10), (2, 20)]
              and len(ids) == 3 and _thread.get_ident() not in ids,
              "start_new_thread", repr(vistos))


def p_thread_join():
    res = [None] * 4

    def trabajo(i):
        s = 0
        for k in range(20000):
            s += k * i
        res[i] = s

    hs = [threading.Thread(target=trabajo, args=(i,)) for i in range(4)]
    for h in hs:
        h.start()
    for h in hs:
        h.join()
    esperado = [sum(k * i for k in range(20000)) for i in range(4)]
    comprobar(res == esperado and not any(h.is_alive() for h in hs), "Thread+join", repr(res))


def p_contador_lock():
    n = [0]
    lock = threading.Lock()

    def sumar():
        for _ in range(3000):
            with lock:
                v = n[0]
                v += 1
                n[0] = v

    hs = [threading.Thread(target=sumar) for _ in range(4)]
    for h in hs:
        h.start()
    for h in hs:
        h.join()
    comprobar(n[0] == 12000, "contador con Lock", "n=%d" % n[0])


def p_sleep_y_calculo():
    # Un hilo duerme (suelta el GIL) diez veces 30 ms mientras el principal
    # calcula: los dos avanzan y la espera no se alarga.
    marcas = []

    def dormilon():
        for i in range(10):
            time.sleep_ms(30)
            marcas.append(time.ticks_ms())

    t0 = time.ticks_ms()
    h = threading.Thread(target=dormilon)
    h.start()
    vueltas = 0
    durante = 0
    while h.is_alive():
        s = 0
        for k in range(500):
            s += k
        vueltas += 1
        if len(marcas) > 0 and len(marcas) < 10:
            durante += 1
    h.join()
    dt = time.ticks_diff(time.ticks_ms(), t0)
    print("    sleep+calculo: %d vueltas (%d entre marcas), %d ms" % (vueltas, durante, dt))
    comprobar(len(marcas) == 10 and durante > 0 and dt < 3000, "sleep mientras otro calcula",
              "marcas=%d durante=%d dt=%d" % (len(marcas), durante, dt))


def p_dos_calculan():
    # Dos hilos que solo calculan: el GIL va y viene, los dos avanzan.
    cuentas = [0, 0]
    parar = [False]

    def calc(i):
        while not parar[0]:
            s = 0
            for k in range(100):
                s += k
            cuentas[i] += 1

    hs = [threading.Thread(target=calc, args=(i,)) for i in range(2)]
    for h in hs:
        h.start()
    time.sleep_ms(300)
    a = list(cuentas)
    parar[0] = True
    for h in hs:
        h.join()
    print("    dos que calculan: %r" % a)
    comprobar(a[0] > 0 and a[1] > 0 and min(a) * 20 > max(a), "dos hilos que calculan avanzan los dos", repr(a))


def p_gc_carga():
    # Varios hilos crean objetos sin parar (y recolectan) mientras el
    # recolector corre en cualquiera de ellos: nada se corrompe.
    errores = []
    hechos = [0]
    lock = threading.Lock()

    def crear(semilla):
        try:
            guardados = []
            for vuelta in range(60):
                lista = []
                for k in range(80):
                    v = semilla * 100000 + vuelta * 100 + k
                    lista.append({"v": v, "s": str(v), "t": (v, [v] * 3)})
                guardados.append(lista)
                if len(guardados) > 4:
                    guardados.pop(0)
                if vuelta % 15 == 0:
                    gc.collect()
                for g in guardados:
                    for d in g:
                        v = d["v"]
                        if d["s"] != str(v) or d["t"][0] != v or d["t"][1] != [v, v, v]:
                            raise ValueError("objeto roto: %r" % (d,))
        except Exception as e:
            with lock:
                errores.append(repr(e))
        with lock:
            hechos[0] += 1

    hs = [threading.Thread(target=crear, args=(i,)) for i in range(4)]
    for h in hs:
        h.start()
    basura = 0
    while hechos[0] < 4:
        basura += len([str(i) for i in range(50)])
    for h in hs:
        h.join()
    comprobar(not errores and hechos[0] == 4, "recolector con varios hilos creando objetos", repr(errores[:2]))


def p_excepcion_en_hilo():
    def falla():
        raise ValueError("a proposito")

    h = threading.Thread(target=falla)
    h.start()
    h.join()
    comprobar(not h.is_alive(), "una excepcion en un hilo no tumba el programa")


def p_muchos():
    # Mas hilos que entradas de una tirada: los que acaban se recogen.
    hechos = [0]
    lock = threading.Lock()

    def f():
        with lock:
            hechos[0] += 1

    for tanda in range(6):
        hs = [threading.Thread(target=f) for _ in range(8)]
        for h in hs:
            h.start()
        for h in hs:
            h.join()
    comprobar(hechos[0] == 48, "48 hilos en tandas de 8", "hechos=%d" % hechos[0])


def p_rlock_event():
    # RLock: el mismo hilo lo toma dos veces; Event: un hilo espera la senal.
    r = threading.RLock()
    ev = threading.Event()
    visto = []

    def f():
        with r:
            with r:
                visto.append(threading.current_thread().name)
        ok = ev.wait(5)
        visto.append(ok)

    h = threading.Thread(target=f, name="hilo-rlock")
    h.start()
    time.sleep_ms(50)
    ev.set()
    h.join()
    sin = threading.Event().wait(0.05)
    comprobar(visto == ["hilo-rlock", True] and sin is False and threading.current_thread().name == "MainThread",
              "RLock, Event y current_thread", repr(visto))


def p_recursion():
    # Una recursion sin fin en un hilo da RuntimeError (la pila del hilo
    # tiene su limite) y el hilo y el programa siguen.
    res = []

    def hondo(n):
        return hondo(n + 1) + 1

    def f():
        try:
            hondo(0)
        except RuntimeError:
            res.append("RuntimeError")
        res.append(sum(range(100)))

    h = threading.Thread(target=f)
    h.start()
    h.join()
    comprobar(res == ["RuntimeError", 4950], "recursion sin fin en un hilo", repr(res))


def salir_con_hilos(modo):
    def dormir():
        while True:
            time.sleep_ms(5)

    def calcular():
        s = 0
        while True:
            s += 1

    f = dormir if modo == "salir-sleep" else calcular
    for _ in range(2):
        _thread.start_new_thread(f, ())
    time.sleep_ms(50)
    print("HILOSPY saliendo con hilos vivos")


def entrada_en_hilo():
    # Un hilo espera una linea del terminal (sin el GIL) mientras el
    # principal sigue contando.
    linea = []

    def leer():
        linea.append(input())

    h = threading.Thread(target=leer)
    h.start()
    tics = 0
    while h.is_alive():
        s = 0
        for k in range(2000):
            s += k
        time.sleep_ms(100)
        tics += 1
        print("HILOSPY tic", tics)
    h.join()
    print("HILOSPY leido: %s tras %d tics" % (linea[0] if linea else None, tics))


if len(sys.argv) > 1 and sys.argv[1].startswith("salir"):
    salir_con_hilos(sys.argv[1])
elif len(sys.argv) > 1 and sys.argv[1] == "entrada":
    entrada_en_hilo()
else:
    for p in (p_start_new_thread, p_thread_join, p_contador_lock, p_sleep_y_calculo,
              p_dos_calculan, p_gc_carga, p_excepcion_en_hilo, p_muchos, p_rlock_event, p_recursion):
        try:
            p()
        except Exception as e:
            comprobar(False, p.__name__, "excepcion %r" % e)
    print("RESULTADO python_hilos: %d fallos" % fallos)
