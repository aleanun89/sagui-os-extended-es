# SPDX-License-Identifier: GPL-3.0-or-later
# python_posix.py - pruebas de socket, subprocess, signal, os (fork/exec/pipe), pwd, grp y
# resource del Python del sistema (D419, tests/h419_python_posix.py).
#
#     python /testes/python_posix.py
#
# Cada prueba escribe "POSIXPY ok <nombre>" o "POSIXPY FALLO <nombre>: ...";
# al final, "RESULTADO python_posix: <n> fallos".
import os
import sys
import time
import errno
import socket
import select
import selectors
import signal
import subprocess
import threading
import pwd
import grp
import resource

fallos = 0
PRUEBAS = []


def prueba(f):
    PRUEBAS.append(f)
    return f


def comprobar(cond, nombre, detalle=""):
    global fallos
    if cond:
        print("POSIXPY ok", nombre)
    else:
        fallos += 1
        print("POSIXPY FALLO %s: %s" % (nombre, detalle))


def esperar(cond, ms=10000):
    fin = time.ticks_add(time.ticks_ms(), ms)
    while not cond():
        if time.ticks_diff(fin, time.ticks_ms()) <= 0:
            return False
        time.sleep_ms(5)
    return True


# ------------------------------------------------------------ socket

@prueba
def socket_tcp_hilo():
    srv = socket.create_server(("127.0.0.1", 9101))
    visto = []

    def servir():
        c, dir_ = srv.accept()
        visto.append(dir_[0])
        d = c.recv(100)
        c.sendall(d.upper())
        c.close()

    t = threading.Thread(target=servir)
    t.start()
    cli = socket.create_connection(("127.0.0.1", 9101), timeout=5)
    cli.sendall(b"hola socket")
    r = cli.recv(100)
    cli.close()
    t.join()
    srv.close()
    comprobar(r == b"HOLA SOCKET" and visto == ["127.0.0.1"], "TCP servidor y cliente (hilo)", repr((r, visto)))


@prueba
def socket_tcp_fork():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", 9102))
    srv.listen(2)
    pid = os.fork()
    if pid == 0:
        c, _ = srv.accept()
        f = c.makefile("rb")
        linea = f.readline()
        c.sendall(b"eco:" + linea)
        c.close()
        os._exit(0)
    srv.close()
    time.sleep(0.2)
    cli = socket.socket()
    cli.settimeout(5)
    cli.connect(("127.0.0.1", 9102))
    cli.sendall(b"una linea\n")
    r = cli.recv(100)
    cli.close()
    p, st = os.waitpid(pid, 0)
    comprobar(r == b"eco:una linea\n" and p == pid and os.WIFEXITED(st) and os.WEXITSTATUS(st) == 0,
              "TCP servidor en un proceso hijo (fork) + makefile", repr((r, st)))


@prueba
def socket_no_bloqueante():
    srv = socket.create_server(("127.0.0.1", 9103))
    srv.setblocking(False)
    try:
        srv.accept()
        ok_eagain = False
    except OSError as e:
        ok_eagain = e.errno == errno.EAGAIN
    cli = socket.socket(socket.AF_INET, socket.SOCK_STREAM | socket.SOCK_NONBLOCK)
    e = cli.connect_ex(("127.0.0.1", 9103))
    ok_conn = e in (0, errno.EINPROGRESS)
    _, w, _ = select.select([], [cli], [], 3)
    ok_w = cli in w and cli.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR) == 0
    r, _, _ = select.select([srv], [], [], 3)
    ok_r = srv in r
    c, _ = srv.accept()
    cli.send(b"ping")
    r, _, _ = select.select([c], [], [], 3)
    d = c.recv(10) if r else b""
    try:
        cli.recv(10)
        ok_eagain2 = False
    except OSError as e2:
        ok_eagain2 = e2.errno == errno.EAGAIN
    for s in (c, cli, srv):
        s.close()
    comprobar(ok_eagain and ok_conn and ok_w and ok_r and d == b"ping" and ok_eagain2,
              "no bloqueante: accept/recv EAGAIN, connect_ex, select, SO_ERROR",
              repr((ok_eagain, e, ok_w, ok_r, d, ok_eagain2)))


@prueba
def socket_rechazado_y_timeout():
    s = socket.socket()
    s.settimeout(3)
    e = s.connect_ex(("127.0.0.1", 9199))
    s.close()
    comprobar(e == errno.ECONNREFUSED, "connect_ex a un puerto cerrado da ECONNREFUSED", str(e))
    srv = socket.create_server(("127.0.0.1", 9104))
    c = socket.create_connection(("127.0.0.1", 9104), timeout=5)
    a, _ = srv.accept()
    c.settimeout(0.3)
    t0 = time.ticks_ms()
    try:
        c.recv(10)
        ok = False
    except socket.timeout as e:
        ok = isinstance(e, OSError) and e.errno == errno.ETIMEDOUT
    dt = time.ticks_diff(time.ticks_ms(), t0)
    for x in (a, c, srv):
        x.close()
    comprobar(ok and 200 <= dt < 1500, "recv con settimeout levanta socket.timeout", "%s %d ms" % (ok, dt))


@prueba
def socket_udp():
    a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    a.bind(("0.0.0.0", 9105))
    b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    b.sendto(b"datagrama", ("127.0.0.1", 9105))
    r, _, _ = select.select([a], [], [], 3)
    d, de = a.recvfrom(100) if r else (b"", None)
    a.sendto(bytes(reversed(d)), de)
    b.settimeout(3)
    d2, de2 = b.recvfrom(100)
    # timeout y EAGAIN
    a.settimeout(0.2)
    try:
        a.recvfrom(10)
        ok_t = False
    except socket.timeout:
        ok_t = True
    a.setblocking(False)
    try:
        a.recvfrom(10)
        ok_e = False
    except OSError as e:
        ok_e = e.errno == errno.EAGAIN
    buf = bytearray(20)
    b.sendto(b"into", ("127.0.0.1", 9105))
    time.sleep(0.2)
    a.setblocking(True)
    a.settimeout(2)
    n, de3 = a.recvfrom_into(buf)
    a.close()
    b.close()
    comprobar(d == b"datagrama" and d2 == b"amargatad" and de2[1] == 9105 and ok_t and ok_e and bytes(buf[:n]) == b"into",
              "UDP sendto/recvfrom/recvfrom_into, timeout y EAGAIN", repr((d, d2, de2, ok_t, ok_e, n)))


@prueba
def socket_funciones():
    ai = socket.getaddrinfo("127.0.0.1", 80)
    comprobar(ai[0][0] == socket.AF_INET and ai[0][4] == ("127.0.0.1", 80), "getaddrinfo con IP", repr(ai))
    ai = socket.getaddrinfo("127.0.0.1", "http", type=socket.SOCK_STREAM)
    comprobar(len(ai) == 1 and ai[0][4][1] == 80 and ai[0][1] == socket.SOCK_STREAM, "getaddrinfo con servicio y tipo", repr(ai))
    comprobar(socket.gethostbyname("127.0.0.1") == "127.0.0.1" and socket.getservbyname("https") == 443,
              "gethostbyname y getservbyname")
    comprobar(socket.htons(0x1234) == 0x3412 and socket.ntohl(socket.htonl(0x01020304)) == 0x01020304
              and socket.inet_pton(socket.AF_INET, "10.0.2.15") == b"\x0a\x00\x02\x0f"
              and socket.inet_ntop(socket.AF_INET, b"\x0a\x00\x02\x0f") == "10.0.2.15", "htons, htonl, inet_pton, inet_ntop")
    s = socket.socket()
    comprobar(s.family == socket.AF_INET and s.type == socket.SOCK_STREAM and
              s.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE) == socket.SOCK_STREAM, "atributos family/type y SO_TYPE")
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    comprobar(s.getsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR) == 1 and
              s.getsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY) == 1, "setsockopt/getsockopt guardan el valor")
    s.settimeout(2.5)
    comprobar(s.gettimeout() == 2.5, "gettimeout")
    s.setblocking(False)
    comprobar(s.gettimeout() == 0.0, "setblocking(False) = timeout 0")
    d = s.dup()
    s.close()
    d.close()
    try:
        socket.gethostbyname("no-existe.invalido")
        ok = False
    except socket.gaierror as e:
        ok = isinstance(e, OSError)
    except OSError:
        ok = True
    comprobar(ok, "un nombre que no resuelve levanta gaierror")


@prueba
def socket_selectors():
    a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    a.bind(("0.0.0.0", 9106))
    b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sel = selectors.DefaultSelector()
    sel.register(a, selectors.EVENT_READ, "dato-a")
    antes = sel.select(0.1)
    b.sendto(b"x", ("127.0.0.1", 9106))
    despues = sel.select(3)
    ok = antes == [] and len(despues) == 1 and despues[0][0].data == "dato-a" and despues[0][1] == selectors.EVENT_READ
    sel.unregister(a)
    sel.close()
    a.close()
    b.close()
    comprobar(ok, "selectors.DefaultSelector con sockets", repr((antes, despues)))


# ------------------------------------------------------------ subprocess

@prueba
def sub_run():
    r = subprocess.run(["echo", "hola mundo"], capture_output=True)
    comprobar(r.returncode == 0 and r.stdout == b"hola mundo\n" and r.stderr == b"", "run echo con capture_output", repr(r))
    r = subprocess.run(["echo", "texto"], stdout=subprocess.PIPE, text=True)
    comprobar(r.stdout == "texto\n", "run con text=True", repr(r.stdout))
    r = subprocess.run(["ls", "/bin"], stdout=subprocess.PIPE)
    comprobar(r.returncode == 0 and b"sagsh" in r.stdout.split(b"\n"), "run ls /bin", repr(r.returncode))
    r = subprocess.run(["ls", "/no/existe"], capture_output=True)
    comprobar(r.returncode != 0, "run con código de retorno distinto de cero", repr(r.returncode))
    r = subprocess.run("exit 3", shell=True)
    comprobar(r.returncode == 3, "run shell=True con exit 3", repr(r.returncode))
    r = subprocess.run("echo hola | wc -c", shell=True, capture_output=True, text=True)
    comprobar(r.stdout.strip() == "5", "shell=True con una tubería", repr(r.stdout))
    try:
        subprocess.run(["ls", "/no/existe"], check=True, capture_output=True)
        ok = False
    except subprocess.CalledProcessError as e:
        ok = e.returncode != 0 and isinstance(e, subprocess.SubprocessError)
    comprobar(ok, "check=True levanta CalledProcessError")
    comprobar(subprocess.check_output(["echo", "x"]) == b"x\n", "check_output")
    comprobar(subprocess.call(["echo", "mudo"], stdout=subprocess.DEVNULL) == 0, "call con stdout=DEVNULL")
    comprobar(subprocess.getoutput("echo hola") == "hola", "getoutput")
    st, out = subprocess.getstatusoutput("echo a; exit 2")
    comprobar(st == 2 and out == "a", "getstatusoutput", repr((st, out)))
    try:
        subprocess.check_call(["ls", "/no/existe"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ok = False
    except subprocess.CalledProcessError:
        ok = True
    comprobar(ok, "check_call levanta CalledProcessError")


@prueba
def sub_popen():
    p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    out, err = p.communicate(b"datos\n")
    comprobar(out == b"datos\n" and err is None and p.returncode == 0 and p.pid > 0, "Popen cat + communicate", repr((out, err, p.returncode)))
    # una entrada grande: cat devuelve lo mismo (sin interbloqueo de tuberías)
    grande = bytes(range(256)) * 800     # 204800 bytes
    p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    out, _ = p.communicate(grande)
    comprobar(out == grande, "communicate con 200 KB de ida y vuelta", "%d bytes" % len(out))
    p = subprocess.Popen("echo bien; ls /no/existe", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out, _ = p.communicate()
    comprobar(b"bien" in out and len(out.split(b"\n")) > 2, "stderr=STDOUT junta las dos salidas", repr(out))
    p = subprocess.Popen(["env"], env={"FOO": "bar"}, stdout=subprocess.PIPE)
    out, _ = p.communicate()
    comprobar(b"FOO=bar" in out.split(b"\n"), "env= se pasa al hijo", repr(out))
    r = subprocess.run(["ls"], cwd="/bin", stdout=subprocess.PIPE)
    comprobar(b"sagsh" in r.stdout.split(b"\n"), "cwd= cambia la carpeta del hijo")
    p = subprocess.Popen(["cat"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)
    out, _ = p.communicate()
    comprobar(out == b"" and p.returncode == 0, "stdin=DEVNULL: cat termina enseguida", repr(out))
    try:
        subprocess.Popen(["/no/existe/programa"])
        ok = False
    except OSError as e:
        ok = e.errno == errno.ENOENT
    comprobar(ok, "un programa que no existe levanta OSError(ENOENT)")
    p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    comprobar(p.poll() is None, "poll() = None mientras corre")
    p.stdin.write(b"abc\n")
    p.stdin.close()
    comprobar(p.stdout.readline() == b"abc\n", "Popen.stdin.write y stdout.readline")
    comprobar(p.wait(5) == 0 and p.returncode == 0, "wait con plazo")
    p.stdout.close()
    p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    p.terminate()
    rc = p.wait(5)
    comprobar(rc == -signal.SIGTERM, "terminate(): returncode = -SIGTERM", str(rc))
    p.stdin.close()
    p.stdout.close()
    with subprocess.Popen(["echo", "ctx"], stdout=subprocess.PIPE) as q:
        d = q.stdout.read()
    comprobar(d == b"ctx\n" and q.returncode == 0, "Popen como gestor de contexto")


@prueba
def sub_timeout():
    t0 = time.ticks_ms()
    try:
        subprocess.run("sleep 5", shell=True, timeout=0.5)
        ok = False
    except subprocess.TimeoutExpired as e:
        ok = e.timeout == 0.5
    dt = time.ticks_diff(time.ticks_ms(), t0)
    comprobar(ok and dt < 3000, "run(timeout=) levanta TimeoutExpired y mata al hijo", "%s %d ms" % (ok, dt))
    p = subprocess.Popen("sleep 5", shell=True)
    try:
        p.wait(timeout=0.3)
        ok = False
    except subprocess.TimeoutExpired:
        ok = True
    p.kill()
    rc = p.wait()
    comprobar(ok and rc == -signal.SIGKILL, "wait(timeout) y kill(): returncode = -SIGKILL", str(rc))


# ------------------------------------------------------------ os: procesos

@prueba
def os_fork_pipe():
    r, w = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.close(r)
        os.write(w, b"desde el hijo %d" % os.getpid())
        os._exit(7)
    os.close(w)
    dato = b""
    while True:
        x = os.read(r, 100)
        if not x:
            break
        dato += x
    os.close(r)
    p, st = os.waitpid(pid, 0)
    comprobar(dato == b"desde el hijo %d" % pid and p == pid and os.WIFEXITED(st) and os.WEXITSTATUS(st) == 7
              and os.waitstatus_to_exitcode(st) == 7, "fork + pipe + waitpid y código de salida", repr((dato, st)))
    pid = os.fork()
    if pid == 0:
        os._exit(0 if os.getppid() > 0 else 1)
    p, st = os.waitpid(pid, 0)
    comprobar(os.WEXITSTATUS(st) == 0 and os.getppid() > 0, "getppid en el hijo")
    try:
        os.waitpid(-1, os.WNOHANG)
        ok = False
    except OSError as e:
        ok = e.errno == errno.ECHILD
    comprobar(ok, "waitpid sin hijos da ECHILD")


@prueba
def os_exec_dup2():
    r, w = os.pipe()
    pid = os.fork()
    if pid == 0:
        os.dup2(w, 1)
        os.close(r)
        os.close(w)
        os.execv("/bin/echo", ["echo", "exec", "bien"])
        os._exit(99)
    os.close(w)
    d = b""
    while True:   # echo puede escribir en varios trozos: leer hasta el final
        t = os.read(r, 100)
        if not t:
            break
        d += t
    os.close(r)
    p, st = os.waitpid(pid, 0)
    comprobar(d == b"exec bien\n" and st == 0, "fork + dup2 + execv", repr((d, st)))
    pid = os.fork()
    if pid == 0:
        try:
            os.execvp("no-existe-este-programa", ["x"])
        except OSError as e:
            os._exit(42 if e.errno == errno.ENOENT else 43)
        os._exit(44)
    p, st = os.waitpid(pid, 0)
    comprobar(os.WEXITSTATUS(st) == 42, "execvp de algo que no existe: ENOENT", repr(st))


@prueba
def os_senales_hijo():
    pid = os.fork()
    if pid == 0:
        time.sleep(30)
        os._exit(0)
    time.sleep(0.2)
    os.kill(pid, signal.SIGTERM)
    p, st = os.waitpid(pid, 0)
    comprobar(os.WIFSIGNALED(st) and os.WTERMSIG(st) == signal.SIGTERM and os.waitstatus_to_exitcode(st) == -15,
              "kill(SIGTERM) a un hijo: WIFSIGNALED y WTERMSIG", repr(st))
    pid = os.fork()
    if pid == 0:
        time.sleep(30)
        os._exit(0)
    p, st = os.waitpid(pid, os.WNOHANG)
    comprobar(p == 0, "waitpid con WNOHANG mientras el hijo corre")
    os.kill(pid, signal.SIGKILL)
    p, st = os.waitpid(pid, 0)
    comprobar(os.WTERMSIG(st) == signal.SIGKILL, "SIGKILL")


@prueba
def os_varios():
    u = os.getuid()
    g = os.getgid()
    comprobar(isinstance(u, int) and isinstance(g, int) and os.geteuid() == u, "getuid/getgid", repr((u, g)))
    fd = os.open("/tmp/posix_prueba.txt", os.O_WRONLY | os.O_CREAT | os.O_TRUNC)
    os.write(fd, b"abc")
    os.close(fd)
    fd = os.open("/tmp/posix_prueba.txt", os.O_RDONLY)
    d = os.read(fd, 10)
    os.close(fd)
    try:
        os.open("/tmp/posix_prueba.txt", os.O_WRONLY | os.O_CREAT | os.O_EXCL)
        ok = False
    except OSError as e:
        ok = e.errno == errno.EEXIST
    os.remove("/tmp/posix_prueba.txt")
    comprobar(d == b"abc" and ok, "os.open/read/write/close y O_EXCL")
    n = os.open(os.devnull, os.O_RDONLY)
    comprobar(os.read(n, 10) == b"", "os.devnull se lee como vacío")
    os.close(n)
    with open(os.devnull, "w") as f:
        f.write("a nadie")
    comprobar(True, "open(os.devnull, 'w')")
    r, w = os.pipe()
    comprobar(os.get_blocking(r) is True, "get_blocking")
    os.set_blocking(r, False)
    try:
        os.read(r, 1)
        ok = False
    except OSError as e:
        ok = e.errno == errno.EAGAIN
    comprobar(ok and os.get_blocking(r) is False, "set_blocking(False): read da EAGAIN")
    d = os.dup(w)
    os.write(d, b"z")
    comprobar(os.read(r, 5) == b"z", "dup")
    for x in (r, w, d):
        os.close(x)
    os.environ["MI_VAR_PRUEBA"] = "42"
    r = subprocess.run(["env"], stdout=subprocess.PIPE)
    comprobar(b"MI_VAR_PRUEBA=42" in r.stdout.split(b"\n"), "os.environ llega a los hijos")


# ------------------------------------------------------------ signal

@prueba
def senal_alarma():
    llamado = []

    def trat(n, frame):
        llamado.append(n)

    antes = signal.signal(signal.SIGALRM, trat)
    comprobar(antes == signal.SIG_DFL and signal.getsignal(signal.SIGALRM) is trat, "signal() devuelve el tratador anterior")
    signal.alarm(1)
    ok = esperar(lambda: llamado, 4000)
    comprobar(ok and llamado == [signal.SIGALRM], "alarm(1) llama al tratador", repr(llamado))
    llamado.clear()
    signal.alarm(1)
    t0 = time.ticks_ms()
    while not llamado and time.ticks_diff(time.ticks_ms(), t0) < 4000:
        pass     # un bucle sin llamadas: el tratador entra por el gancho del intérprete
    comprobar(llamado == [signal.SIGALRM], "el tratador corre dentro de un bucle de Python que no duerme", repr(llamado))
    resto = signal.alarm(10)
    resto2 = signal.alarm(0)
    comprobar(resto == 0 and resto2 in (9, 10), "alarm devuelve los segundos que quedaban", repr((resto, resto2)))
    signal.signal(signal.SIGALRM, signal.SIG_DFL)


class _Plazo(Exception):
    pass


@prueba
def senal_excepcion():
    def trat(n, frame):
        raise _Plazo("fin del plazo")

    signal.signal(signal.SIGALRM, trat)
    signal.alarm(1)
    t0 = time.ticks_ms()
    try:
        time.sleep(5)
        ok = False
    except _Plazo:
        ok = True
    dt = time.ticks_diff(time.ticks_ms(), t0)
    comprobar(ok and 800 <= dt < 3000, "el tratador interrumpe time.sleep con una excepción", "%s %d ms" % (ok, dt))
    signal.alarm(1)
    try:
        while True:
            pass
    except _Plazo:
        ok = True
    comprobar(ok, "el tratador interrumpe un bucle con una excepción")
    signal.signal(signal.SIGALRM, signal.SIG_DFL)


@prueba
def senal_envio_propio():
    vistos = []
    signal.signal(signal.SIGUSR1, lambda n, f: vistos.append(n))
    signal.signal(signal.SIGUSR2, lambda n, f: vistos.append(n))
    signal.raise_signal(signal.SIGUSR1)
    os.kill(os.getpid(), signal.SIGUSR2)
    comprobar(vistos == [signal.SIGUSR1, signal.SIGUSR2], "raise_signal y os.kill a sí mismo", repr(vistos))
    signal.signal(signal.SIGUSR1, signal.SIG_IGN)
    signal.raise_signal(signal.SIGUSR1)
    comprobar(signal.getsignal(signal.SIGUSR1) == signal.SIG_IGN and vistos == [signal.SIGUSR1, signal.SIGUSR2],
              "SIG_IGN ignora la señal")
    # un tratador cuya señal llega mientras un hijo hace waitpid: SIGUSR2 al padre desde el hijo
    pid = os.fork()
    if pid == 0:
        time.sleep(0.3)
        os.kill(os.getppid(), signal.SIGUSR2)
        os._exit(0)
    vistos.clear()
    p, st = os.waitpid(pid, 0)     # la señal interrumpe waitpid; el tratador corre y la espera sigue
    comprobar(p == pid and vistos == [signal.SIGUSR2], "una señal durante waitpid: corre el tratador y sigue esperando", repr(vistos))
    for n in (signal.SIGUSR1, signal.SIGUSR2):
        signal.signal(n, signal.SIG_DFL)
    try:
        signal.signal(signal.SIGKILL, lambda *a: None)
        ok = False
    except OSError:
        ok = True
    try:
        signal.signal(99, signal.SIG_IGN)
        ok2 = False
    except ValueError:
        ok2 = True
    comprobar(ok and ok2 and signal.strsignal(signal.SIGINT) == "Interrupt", "errores de signal() y strsignal")
    old = signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGUSR1})
    ahora = signal.pthread_sigmask(signal.SIG_SETMASK, old)
    comprobar(isinstance(old, set) and signal.SIGUSR1 in ahora and signal.SIGUSR1 not in signal.pthread_sigmask(signal.SIG_BLOCK, set()),
              "pthread_sigmask devuelve el conjunto anterior")
    try:
        signal.default_int_handler(2, None)
        ok = False
    except KeyboardInterrupt:
        ok = True
    comprobar(ok, "default_int_handler levanta KeyboardInterrupt")


@prueba
def senal_en_hilo():
    vistos = []
    signal.signal(signal.SIGUSR1, lambda n, f: vistos.append(("tratador", threading.current_thread() is not None)))
    t = threading.Thread(target=lambda: time.sleep(0.5))
    t.start()
    os.kill(os.getpid(), signal.SIGUSR1)
    time.sleep(0.2)
    t.join()
    signal.signal(signal.SIGUSR1, signal.SIG_DFL)
    comprobar(len(vistos) == 1, "la señal llega al hilo principal aunque haya otro hilo", repr(vistos))
    try:
        r = []
        th = threading.Thread(target=lambda: r.append(_prueba_signal_en_hilo()))
        th.start()
        th.join()
        ok = r == [True]
    except Exception as e:
        ok = False
    comprobar(ok, "signal.signal desde un hilo que no es el principal: ValueError")


def _prueba_signal_en_hilo():
    try:
        signal.signal(signal.SIGUSR1, signal.SIG_IGN)
        return False
    except ValueError:
        return True


# ------------------------------------------------------------ pwd, grp, resource

@prueba
def cuentas():
    r = pwd.getpwnam("root")
    comprobar(r.pw_uid == 0 and r.pw_gid == 0 and r[0] == "root" and r.pw_dir == "/root", "pwd.getpwnam('root')", repr(r))
    s = pwd.getpwnam("sagui")
    comprobar(s.pw_uid == 1000 and pwd.getpwuid(1000).pw_name == "sagui", "pwd.getpwuid(1000) = sagui", repr(s))
    comprobar(pwd.getpwuid(os.getuid()).pw_uid == os.getuid(), "pwd.getpwuid(os.getuid())")
    nombres = [c.pw_name for c in pwd.getpwall()]
    comprobar("root" in nombres and "sagui" in nombres, "pwd.getpwall", repr(nombres))
    try:
        pwd.getpwnam("nadie-aqui")
        ok = False
    except KeyError:
        ok = True
    try:
        pwd.getpwuid(54321)
        ok2 = False
    except KeyError:
        ok2 = True
    comprobar(ok and ok2, "pwd: KeyError con cuentas que no existen")
    g = grp.getgrgid(0)
    comprobar(g.gr_name == "root" and g.gr_gid == 0 and isinstance(g.gr_mem, list), "grp.getgrgid(0)", repr(g))
    comprobar(grp.getgrnam("sagui").gr_gid == 1000, "grp.getgrnam('sagui')")
    comprobar(grp.getgrgid(os.getgid()).gr_gid == os.getgid(), "grp.getgrgid(os.getgid())")
    comprobar(len(grp.getgrall()) >= 2, "grp.getgrall", repr(grp.getgrall()))
    try:
        grp.getgrnam("sin-grupo")
        ok = False
    except KeyError:
        ok = True
    comprobar(ok, "grp: KeyError con un grupo que no existe")


@prueba
def recursos():
    a = resource.getrusage(resource.RUSAGE_SELF)
    t0 = time.ticks_ms()
    x = 0
    while time.ticks_diff(time.ticks_ms(), t0) < 300:
        x += 1
    b = resource.getrusage(resource.RUSAGE_SELF)
    comprobar(isinstance(a.ru_utime, float) and a.ru_maxrss > 0 and b.ru_utime > a.ru_utime and len(a) == 16,
              "getrusage(RUSAGE_SELF): CPU y memoria", repr((a.ru_utime, b.ru_utime, a.ru_maxrss)))
    c = resource.getrusage(resource.RUSAGE_CHILDREN)
    comprobar(c.ru_utime == 0.0, "getrusage(RUSAGE_CHILDREN) da ceros")
    comprobar(resource.getrlimit(resource.RLIMIT_NOFILE) == (32, 32) and resource.getrlimit(resource.RLIMIT_CORE) == (0, 0)
              and resource.getrlimit(resource.RLIMIT_NPROC) == (resource.RLIM_INFINITY, resource.RLIM_INFINITY),
              "getrlimit: NOFILE, CORE e infinito")
    resource.setrlimit(resource.RLIMIT_NOFILE, (32, 32))
    try:
        resource.setrlimit(resource.RLIMIT_NOFILE, (10, 10))
        ok = False
    except OSError as e:
        ok = e.errno == errno.EPERM
    comprobar(ok and resource.getpagesize() == 4096, "setrlimit con otro valor da EPERM")


# ------------------------------------------------------------ principal

def principal():
    for f in PRUEBAS:
        try:
            f()
        except BaseException as e:
            global fallos
            fallos += 1
            print("POSIXPY FALLO %s: excepcion %s %r" % (f.__name__, type(e).__name__, e))
            sys.print_exception(e)
    print("RESULTADO python_posix: %d fallos" % fallos)


if len(sys.argv) > 1:
    PRUEBAS = [f for f in PRUEBAS if f.__name__ in sys.argv[1:]]
principal()
