# SPDX-License-Identifier: GPL-3.0-or-later
# subprocess.py - lanzar programas con la interfaz del subprocess de CPython (D419), sobre
# fork + execve + pipe + waitpid de SaguiOS (D401).
#
# Hecho: run, Popen (stdin/stdout/stderr = PIPE, DEVNULL, STDOUT, un descriptor o un archivo;
# bufsize e ignorados; cwd, env, executable, shell, text/universal_newlines/encoding/errors),
# communicate (con plazo y entrada), wait y poll (con plazo), returncode (negativo si lo mató una
# señal), send_signal/terminate/kill, check_output, check_call, call, getoutput,
# getstatusoutput, CalledProcessError, TimeoutExpired, CompletedProcess, list2cmdline.
#
# Diferencias con CPython:
#   - shell=True ejecuta «/bin/sagsh -c orden» (la shell de SaguiOS, no /bin/sh).
#   - El texto va en Latin-1 hacia y desde los programas del sistema (D068): lo que lee un
#     proceso en modo texto es Latin-1 -> str, y lo que se le escribe, str -> Latin-1. Con
#     encoding="utf-8" se usa UTF-8. Los bytes (el modo por defecto) pasan sin tocar.
#   - Un programa que no se encuentra levanta OSError con errno ENOENT (MicroPython no tiene
#     FileNotFoundError).
#   - Sin start_new_session, pass_fds, preexec_fn, user, group, umask: se aceptan y se ignoran
#     (no hay setsid ni setuid en SaguiOS). close_fds siempre es True.
#   - /dev/null no existe en el SGFS: DEVNULL lee de un pipe vacío y escribe en un archivo
#     temporal borrado (lo mismo que hace sagsh).
#   - Popen.stdin/stdout/stderr son archivos de open(); con text=True se convierten Latin-1.
import os
import select
import signal
import time

PIPE = -1
STDOUT = -2
DEVNULL = -3

_ESPERA_MS = 5


class SubprocessError(Exception):
    pass


class CalledProcessError(SubprocessError):
    def __init__(self, returncode, cmd, output=None, stderr=None):
        self.returncode = returncode
        self.cmd = cmd
        self.output = output
        self.stderr = stderr

    @property
    def stdout(self):
        return self.output

    def __str__(self):
        if self.returncode and self.returncode < 0:
            return "Command '%s' died with signal %d." % (self.cmd, -self.returncode)
        return "Command '%s' returned non-zero exit status %d." % (self.cmd, self.returncode)


class TimeoutExpired(SubprocessError):
    def __init__(self, cmd, timeout, output=None, stderr=None):
        self.cmd = cmd
        self.timeout = timeout
        self.output = output
        self.stderr = stderr

    @property
    def stdout(self):
        return self.output

    def __str__(self):
        return "Command '%s' timed out after %s seconds" % (self.cmd, self.timeout)


class CompletedProcess:
    def __init__(self, args, returncode, stdout=None, stderr=None):
        self.args = args
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr

    def __repr__(self):
        a = ["args=" + repr(self.args), "returncode=" + repr(self.returncode)]
        if self.stdout is not None:
            a.append("stdout=" + repr(self.stdout))
        if self.stderr is not None:
            a.append("stderr=" + repr(self.stderr))
        return "CompletedProcess(" + ", ".join(a) + ")"

    def check_returncode(self):
        if self.returncode:
            raise CalledProcessError(self.returncode, self.args, self.stdout, self.stderr)


def list2cmdline(seq):
    """Une los argumentos en una línea (comillas dobles si llevan espacios), como en Windows;
    sagsh las entiende igual."""
    res = []
    for a in seq:
        a = str(a)
        if not a or " " in a or "\t" in a or '"' in a:
            a = '"' + a.replace("\\", "\\\\").replace('"', '\\"') + '"'
        res.append(a)
    return " ".join(res)


# ------------------------------------------------------------ texto <-> bytes (Latin-1, D068)

def _a_bytes(s, encoding):
    if isinstance(s, (bytes, bytearray)):
        return bytes(s)
    if encoding:
        e = encoding.lower().replace("_", "-")
        if e in ("utf-8", "utf8"):
            return s.encode()
    return bytes([ord(c) if ord(c) < 256 else 63 for c in s])


def _a_texto(b, encoding):
    if b is None:
        return None
    if encoding:
        e = encoding.lower().replace("_", "-")
        if e in ("utf-8", "utf8"):
            return b.decode()
    try:
        s = b.decode()
        if len(s) == len(b):      # todo ASCII: sin multibyte la longitud no cambia
            return s
    except Exception:
        pass
    return "".join([chr(c) for c in b])


def _fd_de(x, defecto):
    if x is None:
        return defecto
    if isinstance(x, int):
        return x
    return x.fileno()


class Popen:
    def __init__(self, args, bufsize=-1, executable=None, stdin=None, stdout=None, stderr=None,
                 preexec_fn=None, close_fds=True, shell=False, cwd=None, env=None,
                 universal_newlines=None, startupinfo=None, creationflags=0, restore_signals=True,
                 start_new_session=False, pass_fds=(), encoding=None, errors=None, text=None,
                 user=None, group=None, extra_groups=None, umask=-1, pipesize=-1, process_group=None):
        self.args = args
        self.returncode = None
        self.pid = None
        self.stdin = self.stdout = self.stderr = None
        self.encoding = encoding
        self.errors = errors
        self.text_mode = bool(text or universal_newlines or encoding or errors)
        self._entrada_pendiente = None
        self._sal_acum = {"stdout": [], "stderr": []}

        if isinstance(args, (str, bytes)):
            lista = [args]
        else:
            lista = list(args)
        if shell:
            lista = ["/bin/sagsh", "-c"] + lista
            if executable is None:
                executable = "/bin/sagsh"
        if not lista:
            raise ValueError("args must not be empty")
        lista = [a.decode() if isinstance(a, bytes) else str(a) for a in lista]
        prog = executable if executable is not None else lista[0]

        # descriptores del hijo: (lado del padre, lado del hijo)
        propios = []   # los que hay que cerrar en el padre tras el fork
        h_in = h_out = h_err = None
        p_in = p_out = p_err = None
        try:
            if stdin == PIPE:
                h_in, p_in = os.pipe()
                propios.append(h_in)
            elif stdin == DEVNULL:
                h_in = os.open(os.devnull, os.O_RDONLY)
                propios.append(h_in)
            else:
                h_in = _fd_de(stdin, None)
            if stdout == PIPE:
                p_out, h_out = os.pipe()
                propios.append(h_out)
            elif stdout == DEVNULL:
                h_out = os.open(os.devnull, os.O_WRONLY)
                propios.append(h_out)
            else:
                h_out = _fd_de(stdout, None)
            if stderr == PIPE:
                p_err, h_err = os.pipe()
                propios.append(h_err)
            elif stderr == DEVNULL:
                h_err = os.open(os.devnull, os.O_WRONLY)
                propios.append(h_err)
            elif stderr == STDOUT:
                h_err = h_out if h_out is not None else 1
            else:
                h_err = _fd_de(stderr, None)

            # el pipe de errores del exec: se cierra solo si el exec sale bien (FD_CLOEXEC)
            er, ew = os.pipe()
            os.set_inheritable(ew, False)
            try:
                pid = os.fork()
            except OSError:
                os.close(er)
                os.close(ew)
                raise
            if pid == 0:
                self._hijo(prog, lista, h_in, h_out, h_err, cwd, env, ew, p_in, p_out, p_err, er)
            self.pid = pid
            os.close(ew)
        except BaseException:
            for fd in propios + [x for x in (p_in, p_out, p_err) if x is not None]:
                try:
                    os.close(fd)
                except OSError:
                    pass
            raise
        for fd in propios:
            os.close(fd)

        # ¿falló el exec en el hijo? El hijo escribe «errno texto» antes de salir.
        datos = b""
        while True:
            try:
                parte = os.read(er, 512)
            except OSError:
                break
            if not parte:
                break
            datos += parte
        os.close(er)
        if datos:
            try:
                os.waitpid(pid, 0)
            except OSError:
                pass
            self.returncode = 255
            try:
                num, _, txt = datos.decode().partition(" ")
                n = int(num)
            except Exception:
                n, txt = 5, "error al lanzar el programa"
            for fd in (p_in, p_out, p_err):
                if fd is not None:
                    os.close(fd)
            raise OSError(n, txt)

        # En modo texto, el archivo de open() ya convierte Latin-1 <-> UTF-8.
        modo = "" if self.text_mode else "b"
        if p_in is not None:
            self.stdin = open(p_in, "w" + modo)
        if p_out is not None:
            self.stdout = open(p_out, "r" + modo)
        if p_err is not None:
            self.stderr = open(p_err, "r" + modo)

    def _hijo(self, prog, lista, h_in, h_out, h_err, cwd, env, ew, p_in, p_out, p_err, er):
        # Solo corre en el proceso hijo: nunca vuelve.
        try:
            for fd in (p_in, p_out, p_err, er):
                if fd is not None:
                    os.close(fd)
            # si un descriptor del hijo ya es el 0, 1 o 2 «equivocado», se copia antes
            if h_in is not None and h_in != 0:
                os.dup2(h_in, 0)
            if h_out is not None and h_out != 1:
                os.dup2(h_out, 1)
            if h_err is not None and h_err != 2:
                os.dup2(h_err, 2)
            for fd in range(3, 32):
                if fd != ew:
                    try:
                        os.close(fd)
                    except OSError:
                        pass
            if cwd is not None:
                os.chdir(cwd)
            if env is None:
                os.execvp(prog, lista)
            else:
                os.execvpe(prog, lista, env)
        except BaseException as e:
            try:
                n = getattr(e, "errno", None) or (e.args[0] if e.args and isinstance(e.args[0], int) else 5)
                txt = e.args[1] if len(e.args) > 1 else str(e)
                os.write(ew, ("%d %s" % (n, txt)).encode())
            except BaseException:
                pass
        os._exit(255)

    # ---------------------------------------------------------------- estado

    def _recoger(self, bloquear):
        if self.returncode is not None:
            return self.returncode
        try:
            pid, estado = os.waitpid(self.pid, 0 if bloquear else os.WNOHANG)
        except OSError:
            self.returncode = 0     # ya lo recogió otro
            return 0
        if pid == self.pid:
            self.returncode = os.waitstatus_to_exitcode(estado)
        return self.returncode

    def poll(self):
        return self._recoger(False)

    def wait(self, timeout=None):
        if self.returncode is not None:
            return self.returncode
        if timeout is None:
            return self._recoger(True)
        fin = time.ticks_add(time.ticks_ms(), int(timeout * 1000))
        while True:
            if self._recoger(False) is not None:
                return self.returncode
            resto = time.ticks_diff(fin, time.ticks_ms())
            if resto <= 0:
                raise TimeoutExpired(self.args, timeout)
            time.sleep_ms(min(_ESPERA_MS, resto))

    def send_signal(self, sig):
        if self.returncode is None:
            os.kill(self.pid, sig)

    def terminate(self):
        self.send_signal(signal.SIGTERM)

    def kill(self):
        self.send_signal(signal.SIGKILL)

    # ---------------------------------------------------------------- communicate

    def communicate(self, input=None, timeout=None):
        if self._entrada_pendiente is None and input is not None and self.stdin is None:
            raise ValueError("stdin was not PIPE")
        fin = None if timeout is None else time.ticks_add(time.ticks_ms(), int(timeout * 1000))
        if self._entrada_pendiente is None and input is not None:
            self._entrada_pendiente = _a_bytes(input, self.encoding) if self.text_mode else bytes(input)
        elif self._entrada_pendiente is None:
            self._entrada_pendiente = b""
        sal = {}     # fd -> (nombre, archivo, trozos leídos)
        for nombre in ("stdout", "stderr"):
            f = getattr(self, nombre)
            if f is not None and not f.closed:
                sal[f.fileno()] = (nombre, f, self._sal_acum[nombre])
        entrada = self._entrada_pendiente
        stdin = self.stdin if (self.stdin is not None and not self.stdin.closed) else None
        if stdin is not None and not entrada:
            stdin.close()
            stdin = None
        if stdin is not None:
            os.set_blocking(stdin.fileno(), False)
        try:
            while sal or stdin is not None:
                resto = None
                if fin is not None:
                    resto = time.ticks_diff(fin, time.ticks_ms())
                    if resto <= 0:
                        raise TimeoutExpired(self.args, timeout)
                lec = [x[1] for x in sal.values()]
                esc = [stdin] if stdin is not None and entrada else []
                if esc:
                    # un pipe de escritura siempre «está listo» para select: se prueba y, si
                    # no cabe, se espera un poco a que el hijo lea o escriba
                    try:
                        n = os.write(stdin.fileno(), entrada[:32768])
                        entrada = entrada[n:]
                    except OSError as e:
                        if e.errno == 11:           # EAGAIN: el pipe está lleno
                            n = 0
                        else:                       # EPIPE: el hijo ya no lee
                            entrada = b""
                    if not entrada:
                        stdin.close()
                        stdin = None
                    espera = 0.01 if not n else 0
                else:
                    espera = None if resto is None else resto / 1000
                if lec:
                    if espera is None:
                        r, _, _ = select.select(lec, [], [])
                    else:
                        r, _, _ = select.select(lec, [], [], espera)
                    for f in r:
                        fd = f.fileno()
                        try:
                            datos = os.read(fd, 32768)
                        except OSError:
                            datos = b""
                        if datos:
                            sal[fd][2].append(datos)
                        else:
                            f.close()
                            del sal[fd]
                elif espera:
                    time.sleep(espera)
        except BaseException:
            self._entrada_pendiente = entrada
            if stdin is not None and not stdin.closed:
                os.set_blocking(stdin.fileno(), True)
            raise
        self._entrada_pendiente = None
        if self.stdin is not None and not self.stdin.closed:
            self.stdin.close()
        if fin is not None:
            resto = time.ticks_diff(fin, time.ticks_ms())
            if resto < 0:
                resto = 0
            self.wait(resto / 1000)
        else:
            self.wait()
        salida = b"".join(self._sal_acum["stdout"]) if self.stdout is not None else None
        error = b"".join(self._sal_acum["stderr"]) if self.stderr is not None else None
        self._sal_acum = {"stdout": [], "stderr": []}
        if self.text_mode:
            salida = _a_texto(salida, self.encoding)
            error = _a_texto(error, self.encoding)
        return (salida, error)

    # ---------------------------------------------------------------- cierre

    def __enter__(self):
        return self

    def __exit__(self, tipo, valor, tb):
        for f in (self.stdout, self.stderr, self.stdin):
            if f is not None:
                try:
                    f.close()
                except OSError:
                    pass
        self.wait()

    def __repr__(self):
        return "<Popen: returncode: %s args: %r>" % (self.returncode, self.args)


# ---------------------------------------------------------------- funciones de alto nivel

def run(*popenargs, input=None, capture_output=False, timeout=None, check=False, **kwargs):
    if input is not None:
        if kwargs.get("stdin") is not None:
            raise ValueError("stdin and input arguments may not both be used.")
        kwargs["stdin"] = PIPE
    if capture_output:
        if kwargs.get("stdout") is not None or kwargs.get("stderr") is not None:
            raise ValueError("stdout and stderr arguments may not be used with capture_output.")
        kwargs["stdout"] = PIPE
        kwargs["stderr"] = PIPE
    with Popen(*popenargs, **kwargs) as proceso:
        try:
            salida, error = proceso.communicate(input, timeout=timeout)
        except TimeoutExpired as e:
            proceso.kill()
            try:
                salida, error = proceso.communicate()
            except BaseException:
                salida = error = None
            e.output = salida
            e.stderr = error
            raise
        except BaseException:
            proceso.kill()
            raise
        codigo = proceso.poll()
        if check and codigo:
            raise CalledProcessError(codigo, popenargs[0] if popenargs else kwargs.get("args"), salida, error)
    return CompletedProcess(popenargs[0] if popenargs else kwargs.get("args"), codigo, salida, error)


def call(*popenargs, timeout=None, **kwargs):
    with Popen(*popenargs, **kwargs) as p:
        try:
            return p.wait(timeout=timeout)
        except BaseException:
            p.kill()
            raise


def check_call(*popenargs, **kwargs):
    codigo = call(*popenargs, **kwargs)
    if codigo:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise CalledProcessError(codigo, cmd)
    return 0


def check_output(*popenargs, timeout=None, **kwargs):
    if "stdout" in kwargs:
        raise ValueError("stdout argument not allowed, it will be overridden.")
    return run(*popenargs, stdout=PIPE, timeout=timeout, check=True, **kwargs).stdout


def getstatusoutput(cmd, encoding=None, errors=None):
    try:
        datos = check_output(cmd, shell=True, text=True, stderr=STDOUT, encoding=encoding, errors=errors)
        codigo = 0
    except CalledProcessError as e:
        datos = e.output
        codigo = e.returncode
    if datos and datos[-1:] == "\n":
        datos = datos[:-1]
    return codigo, datos


def getoutput(cmd, encoding=None, errors=None):
    return getstatusoutput(cmd, encoding=encoding, errors=errors)[1]
