# Python en SaguiOS: red, procesos y señales al estilo CPython

El `python` del sistema es MicroPython 1.24, pero con módulos escritos sobre lo nativo de
SaguiOS que se usan como los de CPython: `socket`, `ssl`, `select`, `selectors`,
`subprocess`, `signal`, `os` (con `fork`, `execv`, `waitpid`, `pipe`...), `pwd`, `grp` y
`resource`. Los de Python viven en `/lib/python`; los de C, en el propio programa.

    python /testes/python_posix.py        # las pruebas del sistema: todas deben dar "ok"

Los textos van en UTF-8 dentro de Python y en Latin-1 hacia los programas y los archivos
(D068): lo que escribe un hijo en modo texto se lee como `str` correctamente.

## socket

Solo IPv4 (`AF_INET`), TCP (`SOCK_STREAM`) y UDP (`SOCK_DGRAM`). El anfitrión de QEMU es
`10.0.2.2`; `127.0.0.1` sirve entre procesos del mismo SaguiOS.

    import os, socket

    # servidor TCP en un proceso hijo, cliente en el padre
    srv = socket.create_server(("0.0.0.0", 9000))
    pid = os.fork()
    if pid == 0:
        c, dir_ = srv.accept()
        c.sendall(c.recv(100).upper())
        os._exit(0)
    cli = socket.create_connection(("127.0.0.1", 9000), timeout=5)
    cli.sendall(b"hola")
    print(cli.recv(100))                       # b'HOLA'
    os.waitpid(pid, 0)

    # UDP
    a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    a.bind(("0.0.0.0", 9001))
    a.settimeout(2)                            # socket.timeout (subclase de OSError) si no llega nada
    datos, quien = a.recvfrom(1500)

    # no bloqueante con selectors
    import selectors
    s = socket.socket()
    s.setblocking(False)                       # O_NONBLOCK real
    e = s.connect_ex(("10.0.2.2", 80))         # 0 o EINPROGRESS
    sel = selectors.DefaultSelector()
    sel.register(s, selectors.EVENT_WRITE)
    sel.select(5)                              # listo para escribir al conectar
    print(s.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR))   # 0 = conectado

Qué hay: `socket`, `connect`, `connect_ex`, `bind`, `listen`, `accept`, `send`, `sendall`,
`recv`, `recv_into`, `sendto`, `recvfrom`, `recvfrom_into`, `settimeout`, `gettimeout`,
`setblocking`, `shutdown`, `getsockname`, `getpeername`, `makefile`, `dup`, `detach`,
`setsockopt`/`getsockopt`, `family`/`type`; `create_connection`, `create_server`,
`getaddrinfo`, `gethostbyname(_ex)`, `getservbyname`, `getservbyport`, `getprotobyname`,
`getnameinfo`, `htons`/`htonl`/`ntohs`/`ntohl`, `inet_aton`/`ntoa`/`pton`/`ntop`,
`SOCK_NONBLOCK`, `MSG_DONTWAIT`, `MSG_WAITALL` y las constantes de siempre. Los errores
son `OSError` con `errno` (`ECONNREFUSED`, `EAGAIN`, `EINPROGRESS`, `ETIMEDOUT`...);
`socket.timeout` y `socket.gaierror` son subclases.

**Lo que no hay:** IPv6 (`AF_INET6` da `EAFNOSUPPORT`), `AF_UNIX`, `socketpair`,
`sendmsg`/`recvmsg`, `MSG_PEEK`, `gethostbyaddr` (no hay DNS inverso). `setsockopt` guarda los
valores y `getsockopt` los devuelve, pero el kernel no los usa.

`ssl.wrap_socket`/`SSLContext` (solo cliente) sobre la libtls del sistema; por defecto valida
los certificados con el almacén de `certificados`.

## subprocess

    import subprocess
    r = subprocess.run(["ls", "/bin"], capture_output=True, text=True)
    print(r.returncode, r.stdout.split()[:3])

    p = subprocess.Popen(["cat"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    salida, _ = p.communicate(b"datos\n", timeout=5)

    subprocess.check_output("echo hola | wc -c", shell=True)   # b'5\n' (shell = sagsh -c)
    subprocess.run("sleep 10", shell=True, timeout=1)          # TimeoutExpired y mata al hijo

`run`, `Popen`, `communicate`, `wait`, `poll`, `send_signal`, `terminate`, `kill`,
`returncode` (negativo = lo mató esa señal), `check_output`, `check_call`, `call`,
`getoutput`, `getstatusoutput`, `CalledProcessError`, `TimeoutExpired`, `PIPE`, `STDOUT`,
`DEVNULL`, `cwd`, `env`, `shell`. Va sobre `fork` + `execve` + `pipe` + `waitpid`.
Un programa que no existe da `OSError` con `errno.ENOENT` (MicroPython no tiene
`FileNotFoundError`). Se ignoran `start_new_session`, `pass_fds`, `preexec_fn`, `user`,
`group`. `/dev/null` no existe en el SGFS: se emula (lee vacío, escribe en un temporal borrado).

## signal

    import signal, time

    def plazo(n, frame):
        raise RuntimeError("se acabó el tiempo")

    signal.signal(signal.SIGALRM, plazo)
    signal.alarm(2)
    try:
        time.sleep(10)                 # el tratador interrumpe la espera con su excepción
    except RuntimeError as e:
        print(e)

`signal`, `getsignal`, `alarm`, `setitimer(ITIMER_REAL, s)` (un solo disparo), `getitimer`,
`pause`, `raise_signal`, `pthread_sigmask`, `sigpending`, `valid_signals`, `strsignal`,
`default_int_handler`, `SIG_DFL`, `SIG_IGN` y `SIGHUP`...`SIGSYS`.

Los tratadores de Python no corren dentro de la señal (sería inseguro): el kernel llama a un
manipulador de C mínimo que levanta una bandera, y el intérprete la mira en cada salto hacia
atrás de un bucle y al volver de `time.sleep`, `select`, `os.waitpid`, `os.read` y de los
`recv`/`accept` de `socket`. Si el tratador levanta una excepción, sale de la espera. Los
tratadores solo corren en el hilo principal. **Ctrl+C** sigue terminando el programa
mientras no instales un tratador de `SIGINT` (con `signal.default_int_handler` obtienes
`KeyboardInterrupt`). No hay `set_wakeup_fd`, `siginterrupt` ni temporizadores periódicos.

## os

`fork`, `_exit`, `execv`, `execve`, `execvp`, `execvpe`, `waitpid` (con `WNOHANG`), `wait`,
`WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`, `waitstatus_to_exitcode`, `kill`,
`killpg`, `getpid`, `getppid`, `getpgid`, `getuid`, `getgid`, `pipe`, `dup`, `dup2`, `close`,
`read`, `write`, `open` (con `O_*`), `isatty`, `set_blocking`, `set_inheritable`, `environ`,
`devnull`.

    r, w = os.pipe()
    pid = os.fork()
    if pid == 0:                       # el hijo
        os.dup2(w, 1)
        os.execv("/bin/echo", ["echo", "desde exec"])
    os.close(w)
    print(os.read(r, 100))
    print(os.waitpid(pid, 0))          # (pid, estado)

`fork` copia al escribir (barato). En el hijo solo queda el hilo que llamó. No hay
`setuid`, `chown`, `setsid` ni `os.nice`.

## pwd, grp y resource

    import pwd, grp, resource
    pwd.getpwnam("root").pw_uid        # 0
    pwd.getpwuid(1000).pw_name         # 'sagui'
    grp.getgrall()                     # los grupos de las cuentas y los de /etc/grupos
    resource.getrusage(resource.RUSAGE_SELF).ru_utime    # segundos de CPU del proceso

`pwd` lee `/etc/usuarios` y `grp`, `/etc/grupos` (más un grupo por cuenta). `getrusage` da el
tiempo de CPU y la memoria del proceso (el kernel no separa usuario y sistema ni cuenta fallos de
página: el resto es 0; `RUSAGE_CHILDREN` da ceros). `getrlimit` da lo real: `RLIMIT_NOFILE` 32,
`RLIMIT_STACK` 8 MB, `RLIMIT_CORE` 0, y «sin límite» en lo demás. `setrlimit` no puede
cambiar nada (`OSError` `EPERM`; pedir el valor actual no hace nada).

## Diferencias con CPython

Es MicroPython: sin `FileNotFoundError`, `TimeoutError` ni las demás subclases de `OSError` (usa `e.errno`),
sin `asyncio`, sin `enum` de `signal`/`socket` (las constantes son enteros), `dict` sin
orden. Los sockets y el `select` duermen en tics de 10 ms entre sondeos.
