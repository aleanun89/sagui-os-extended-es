#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H251 (D253, mejora 097) - sshd: redirección local de puertos (ssh -L).

Canales direct-tcpip (RFC 4254, 7.2) en el servidor SSH de SaguiOS, para entrar
cifrado a un servicio que solo escucha en 127.0.0.1 (el VNC). La lógica de a
qué destinos se deja abrir un túnel (sshd_logica.h) se prueba en el anfitrión
con tests/ssh_unidad.c, que lanza h248.

En QEMU (red de usuario con hostfwd al puerto 22), con el ssh de OpenSSH del
anfitrión y /testes/ecotcp (un servidor TCP de eco) dentro del SaguiOS:
  - `ssh -L puerto:localhost:7777 ... 'tee'`: lo que se manda por el túnel
    vuelve igual; al cerrar el envío llega el «FIN n» del servidor y después el
    cierre (el fin de envío pasa sin cortar la vuelta);
  - 1 MB del servidor al cliente (más que cualquier ventana) llega íntegro y
    cierra el servidor; 300 KB de ida y vuelta, también;
  - seis conexiones a la vez por el mismo túnel, cada una con sus datos;
  - mientras pasa 1 MB por el túnel, la orden de la misma conexión (tee)
    sigue respondiendo;
  - un cliente que corta a medias no estropea nada; si muere el servidor de
    destino, el cliente ve el cierre;
  - `ssh -N -L` (sin canal de sesión) también vale; al noveno túnel a la vez
    se rechaza (límite por conexión) y, al cerrar los demás, vuelve a caber;
  - de serie (redirigir=local) se rechazan los destinos que no son esta
    máquina (10.0.2.2, la propia dirección de la tarjeta, un nombre), y el
    anfitrión no recibe ninguna conexión; un puerto sin nadie da «no se pudo
    conectar»; `ssh -R` se rechaza;
  - redirigir=no: nada; redirigir=red-local: 10.0.2.2 sí, 8.8.8.8 no;
  - todo queda en el registro (abierto con su destino, rechazado, cerrado).
"""
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA, FalhaTeste  # noqa: E402
import h248_ssh as base  # noqa: E402

TRAB = os.path.join(SAIDA, "h251")
ECO = 7777      # /testes/ecotcp dentro del SaguiOS
ECO2 = 7778     # otro, al que se mata con un túnel abierto
NADIE = 7999    # nadie escucha


def patron(n):
    return bytes(i % 251 for i in range(251)) * (n // 251) + bytes(i % 251 for i in range(n % 251))


def conectar(porta, tempo=30):
    s = socket.create_connection(("127.0.0.1", porta), timeout=tempo)
    s.settimeout(tempo)
    return s


def ler_tudo(s, limite=None):
    """Hasta que el otro lado cierre (o hasta «limite» bytes)."""
    d = bytearray()
    while limite is None or len(d) < limite:
        try:
            b = s.recv(65536)
        except ConnectionResetError:
            break
        if not b:
            break
        d += b
    return bytes(d)


def eco(porta, datos, tempo=120):
    """Manda «datos», cierra el envío y devuelve todo lo que llegue hasta el cierre."""
    s = conectar(porta, tempo)
    saida = []
    t = threading.Thread(target=lambda: saida.append(ler_tudo(s)))
    t.start()
    s.sendall(datos)
    s.shutdown(socket.SHUT_WR)
    t.join(tempo)
    s.close()
    if t.is_alive() or not saida:
        raise FalhaTeste("el túnel no cerró la conexión de eco")
    return saida[0]


def dame(porta, n, tempo=180):
    s = conectar(porta, tempo)
    s.sendall(b"DAME %d\n" % n)
    d = ler_tudo(s)
    s.close()
    return d


def cerrada(porta, tempo=20):
    """True si el ssh local acepta y cierra sin dar nada (el canal no se abrió)."""
    try:
        s = conectar(porta, tempo)
        s.sendall(b"hay alguien\n")
        d = s.recv(100)
        s.close()
        return d == b""
    except (ConnectionResetError, BrokenPipeError):
        return True
    except socket.timeout:
        return False


def esperar_puerto(porta, proc, tempo=40):
    fim = time.time() + tempo
    while time.time() < fim:
        if proc.poll() is not None:
            raise FalhaTeste(f"ssh terminó antes de abrir el túnel (código {proc.returncode}): {proc.stderr.read()[-300:]!r}")
        try:
            socket.create_connection(("127.0.0.1", porta), timeout=2).close()
            return
        except OSError:
            time.sleep(0.3)
    raise FalhaTeste("el ssh del anfitrión no abrió el puerto local del túnel")


class Tunel:
    """Un ssh del anfitrión con varios -L (y, si se da, una orden en la misma conexión)."""

    def __init__(self, cl, destinos, orden=None, extra=()):
        self.portas = [base.puerto_libre() for _ in destinos]
        cmd = ["ssh", "-p", str(cl.porta)] + cl.opcoes(extra=["ExitOnForwardFailure=yes"] + list(extra))
        for p, d in zip(self.portas, destinos):
            cmd += ["-L", f"127.0.0.1:{p}:{d}"]
        cmd += ([] if orden else ["-N"]) + ["root@127.0.0.1"] + ([orden] if orden else [])
        self.p = subprocess.Popen(cmd, stdin=subprocess.PIPE if orden else subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        esperar_puerto(self.portas[0], self.p)

    def fechar(self):
        if self.p.poll() is None:
            self.p.terminate()
            try:
                self.p.wait(10)
            except subprocess.TimeoutExpired:
                self.p.kill()
                self.p.wait()
        for f in (self.p.stdin, self.p.stdout, self.p.stderr):
            if f:
                f.close()

    def linea(self, texto, tempo=15):
        """Por la orden de la misma conexión (tee): manda una línea y espera a que vuelva. -> segundos."""
        t0 = time.time()
        self.p.stdin.write(texto)
        self.p.stdin.flush()
        saida = []
        t = threading.Thread(target=lambda: saida.append(self.p.stdout.readline()), daemon=True)
        t.start()
        t.join(tempo)
        if not saida or saida[0] != texto:
            raise FalhaTeste(f"la orden de la misma conexión no respondió ({saida!r})")
        return time.time() - t0


class Anfitrion:
    """Un servidor en el anfitrión (10.0.2.2 visto desde el SaguiOS): contesta «anfitrion:» + lo recibido."""

    def __init__(self):
        self.s = socket.socket()
        self.s.bind(("127.0.0.1", 0))
        self.s.listen(8)
        self.porta = self.s.getsockname()[1]
        self.conexoes = 0
        threading.Thread(target=self._servir, daemon=True).start()

    def _servir(self):
        while True:
            try:
                c, _ = self.s.accept()
            except OSError:
                return
            self.conexoes += 1
            try:
                c.settimeout(10)
                d = c.recv(1000)
                c.sendall(b"anfitrion:" + d)
            except OSError:
                pass
            c.close()

    def fechar(self):
        self.s.close()


def maquina():
    base.TRAB = TRAB
    chave = base.gerar_chave("clave_h251")
    img = base.construir_imagen(chave + ".pub")
    porta = base.puerto_libre()
    cl = base.Cliente(porta, chave)
    anf = Anfitrion()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]
    tuneis = []
    with Maquina("h251", "ide", imagem=img, snapshot=False, extras=extras) as m:
        try:
            m.esperar_serial("sagui:/$", 120)
            base.esperar_log(m, "10.0.2.15 (DHCP)", 30)
            m.comando("servicio iniciar sshd", 30)
            base.esperar_log(m, "sshd: escuchando en el puerto 22", 40)
            m.comando(f"/testes/ecotcp {ECO} &", 30)
            base.esperar_log(m, f"ecotcp: escuchando en el puerto {ECO}", 20)
            m.comando(f"/testes/ecotcp {ECO2} &", 30)
            base.esperar_log(m, f"ecotcp: escuchando en el puerto {ECO2}", 20)
            r = cl.ssh("sshd --ayuda")
            verificar(b"ssh -L 5900:localhost:5900" in r.stdout and b"redirigir" in r.stdout,
                      "sshd --ayuda explica el túnel y la opción redirigir")

            # --- un ssh con una orden (tee) y cinco redirecciones en la misma conexión
            marca = len(m.serial())
            destinos = [f"localhost:{ECO}", f"127.0.0.1:{ECO2}", f"10.0.2.2:{anf.porta}", f"localhost:{NADIE}",
                        "ejemplo.com:80", f"10.0.2.15:{ECO}"]
            t = Tunel(cl, destinos, orden="tee /home/tunel-tee.txt")
            tuneis.append(t)
            p_eco, p_eco2, p_anf, p_nadie, p_nome, p_propia = t.portas
            verificar(t.linea(b"antes\n") < 10, "ssh -L ... 'tee': la orden de la conexión responde")

            d = eco(p_eco, b"hola por el tunel\n")
            verificar(d == b"hola por el tunel\n\nFIN 18\n",
                      f"ssh -L puerto:localhost:{ECO}: ida, vuelta, fin de envío y cierre ({d!r})")
            base.esperar_log(m, f"túnel a 127.0.0.1:{ECO} abierto (pedido desde 127.0.0.1:", 10, marca)
            base.esperar_log(m, f"túnel a 127.0.0.1:{ECO} cerrado (18 bytes de ida, 26 de vuelta)", 10, marca)
            verificar(True, "el registro anota el túnel abierto (destino y origen) y cerrado (bytes en cada sentido)")

            t0 = time.time()
            d = dame(p_eco, 1000000)
            verificar(d == patron(1000000), f"1 MB del servidor al cliente por el túnel, íntegro, y cierra el servidor "
                      f"({len(d)} bytes, {time.time() - t0:.1f} s)")
            datos = os.urandom(300 * 1024)
            t0 = time.time()
            d = eco(p_eco, datos, 240)
            verificar(d == datos + b"\nFIN %d\n" % len(datos),
                      f"300 KB de ida y vuelta por el túnel salen iguales ({time.time() - t0:.1f} s)")

            # --- varios a la vez, y la orden de la misma conexión mientras tanto
            res = {}

            def uno(i):
                try:
                    if i % 3 == 2:
                        res[i] = dame(p_eco, 200000 + i) == patron(200000 + i)
                    else:
                        dd = os.urandom(40000 + i * 1111)
                        res[i] = eco(p_eco if i % 2 else p_eco2, dd) == dd + b"\nFIN %d\n" % len(dd)
                except (OSError, FalhaTeste) as e:
                    res[i] = repr(e)
            hilos = [threading.Thread(target=uno, args=(i,)) for i in range(6)]
            t0 = time.time()
            for h in hilos:
                h.start()
            for h in hilos:
                h.join(240)
            verificar(all(res.get(i) is True for i in range(6)),
                      f"seis conexiones a la vez por la misma sesión, cada una con sus datos ({time.time() - t0:.1f} s; {res})")

            grande = {}
            h = threading.Thread(target=lambda: grande.update(d=dame(p_eco, 1000000)))
            h.start()
            time.sleep(0.3)
            tempos = [t.linea(b"durante %d\n" % i) for i in range(5)]
            h.join(240)
            verificar(grande.get("d") == patron(1000000) and max(tempos) < 5,
                      f"con 1 MB pasando por el túnel, la orden de la misma conexión responde (máx. {max(tempos):.2f} s)")

            # --- el cliente corta a medias: no pasa nada
            s = conectar(p_eco)
            s.sendall(b"DAME 3000000\n")
            ler_tudo(s, 20000)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b"\1\0\0\0\0\0\0\0")   # cierre con RST
            s.close()
            time.sleep(1)
            d = eco(p_eco, b"sigue vivo\n")
            verificar(d == b"sigue vivo\n\nFIN 11\n" and t.linea(b"despues\n") < 10,
                      "un cliente que corta a media transferencia no estropea el túnel ni la orden")

            # --- destinos que no se permiten (de serie: solo esta máquina)
            marca = len(m.serial())
            verificar(cerrada(p_anf), "túnel a 10.0.2.2 (otra máquina): la conexión se cierra sin datos")
            base.esperar_log(m, f"túnel a 10.0.2.2:{anf.porta} (pedido desde 127.0.0.1:", 10, marca)
            base.esperar_log(m, "RECHAZADO: solo se redirige a esta máquina: localhost (redirigir=local)", 10, marca)
            verificar(anf.conexoes == 0, "y el anfitrión no recibió ninguna conexión")
            verificar(cerrada(p_propia), "túnel a la dirección de la tarjeta (10.0.2.15): rechazado también (solo 127.x)")
            base.esperar_log(m, f"túnel a 10.0.2.15:{ECO} (pedido", 10, marca)
            verificar(cerrada(p_nome), "túnel a un nombre (ejemplo.com): rechazado")
            base.esperar_log(m, "túnel a ejemplo.com:80 (pedido", 10, marca)
            base.esperar_log(m, "RECHAZADO: el destino tiene que ser localhost o una dirección IPv4", 10, marca)
            verificar(cerrada(p_nadie), f"túnel a localhost:{NADIE}, donde no escucha nadie: se cierra")
            base.esperar_log(m, f"túnel a localhost:{NADIE} (pedido", 10, marca)
            verificar(re.search(rf"túnel a localhost:{NADIE} \(pedido desde [\d.:]+\) rechazado: \S", m.serial()[marca:]),
                      "y el registro dice que no se pudo conectar")
            verificar(t.linea(b"tras los rechazos\n") < 10 and eco(p_eco, b"x") == b"x\nFIN 1\n",
                      "tras los rechazos, el túnel bueno y la orden siguen funcionando")

            # --- el servidor de destino muere con un túnel abierto: el cliente ve el cierre
            s = conectar(p_eco2)
            s.sendall(b"antes de morir\n")
            verificar(s.recv(100) == b"antes de morir\n", f"otro túnel (127.0.0.1:{ECO2}) abierto y con eco")
            r = cl.ssh("ps")
            # los dos ecotcp: el segundo que se lanzó (el de ECO2) tiene el pid más alto
            pid = sorted(int(ln.split()[0]) for ln in r.stdout.decode("latin-1").splitlines()
                         if "ecotcp" in ln and ln.split()[0].isdigit())
            if len(pid) == 2:
                cl.ssh(f"kill {pid[1]}")
                s.settimeout(20)
                try:
                    fim = s.recv(100)
                except ConnectionResetError:
                    fim = b""
                except socket.timeout:
                    fim = None
                verificar(fim == b"", "al morir el servidor de destino, el cliente del túnel ve el cierre")
                verificar(cerrada(p_eco2), "y un túnel nuevo a ese puerto ya no se abre")
            else:
                raise FalhaTeste(f"no se encontraron los dos ecotcp en ps: {r.stdout[-400:]!r}")
            s.close()

            # la orden termina y con ella el ssh: código 0, y tee guardó todo
            t.p.stdin.close()
            t.p.stdin = None
            verificar(t.p.wait(30) == 0, "al acabar la orden, el ssh con túneles termina con código 0")
            t.fechar()
            r = cl.ssh("wc -l /home/tunel-tee.txt")
            verificar(r.stdout.split()[:1] == [b"8"], f"la orden recibió sus 8 líneas mientras tanto ({r.stdout!r})")

            # --- ssh -N (sin canal de sesión) y el límite de túneles por conexión
            marca = len(m.serial())
            t = Tunel(cl, [f"localhost:{ECO}"])
            tuneis.append(t)
            time.sleep(1.5)   # la conexión de tanteo de esperar_puerto ya se cerró
            abiertos = []
            for i in range(8):
                s = conectar(t.portas[0])
                s.sendall(b"t%d\n" % i)
                verificar(s.recv(100) == b"t%d\n" % i, f"ssh -N -L: túnel {i + 1} de 8 a la vez")
                abiertos.append(s)
            verificar(cerrada(t.portas[0]), "el noveno túnel a la vez se rechaza (límite por conexión)")
            base.esperar_log(m, "túnel RECHAZADO: ya hay 8 en esta conexión", 10, marca)
            for i, s in enumerate(abiertos):
                s.sendall(b"otra\n")
                if s.recv(100) != b"otra\n":
                    raise FalhaTeste(f"el túnel {i + 1} dejó de funcionar tras el rechazo")
                s.shutdown(socket.SHUT_WR)
                fim = ler_tudo(s)
                s.close()
                if fim != b"\nFIN 8\n":
                    raise FalhaTeste(f"el túnel {i + 1} no cerró bien: {fim!r}")
            verificar(True, "los ocho siguen funcionando y cierran limpios")
            time.sleep(1)
            verificar(eco(t.portas[0], b"cabe otra vez\n") == b"cabe otra vez\n\nFIN 14\n",
                      "al cerrarse, vuelve a haber sitio para otro túnel")
            verificar(t.p.poll() is None, "el ssh -N sigue conectado sin ningún canal abierto")
            t.fechar()

            # --- ssh -R no se admite
            marca = len(m.serial())
            cmd = ["ssh", "-p", str(porta)] + cl.opcoes(extra=["ExitOnForwardFailure=yes"]) + \
                  ["-N", "-R", f"{NADIE}:127.0.0.1:{anf.porta}", "root@127.0.0.1"]
            r = subprocess.run(cmd, capture_output=True, timeout=60, stdin=subprocess.DEVNULL)
            verificar(r.returncode != 0, f"ssh -R (redirección remota): rechazada (código {r.returncode})")
            base.esperar_log(m, "redirección remota (ssh -R) RECHAZADA", 10, marca)

            # --- redirigir=no y redirigir=red-local (la configuración se lee en cada conexión)
            marca = len(m.serial())
            cl.ssh("mkdir /home/.ssh; echo redirigir=no > /home/.ssh/sshd.conf")
            t = Tunel(cl, [f"localhost:{ECO}"])
            tuneis.append(t)
            verificar(cerrada(t.portas[0]), "con redirigir=no, ni a localhost")
            base.esperar_log(m, "RECHAZADO: redirección de puertos desactivada (redirigir=no)", 10, marca)
            t.fechar()
            verificar(cl.ssh("echo sigo").stdout == b"sigo\n", "y las órdenes siguen funcionando")

            cl.ssh("echo redirigir=red-local > /home/.ssh/sshd.conf")
            # (localhost el primero: es el puerto con el que tantea esperar_puerto)
            t = Tunel(cl, [f"localhost:{ECO}", f"10.0.2.2:{anf.porta}", "8.8.8.8:53"])
            tuneis.append(t)
            s = conectar(t.portas[1])
            s.sendall(b"hola\n")
            d = ler_tudo(s)
            s.close()
            verificar(d == b"anfitrion:hola\n" and anf.conexoes == 1,
                      f"con redirigir=red-local, el túnel a 10.0.2.2 llega al anfitrión ({d!r})")
            verificar(eco(t.portas[0], b"y local\n") == b"y local\n\nFIN 8\n", "y a localhost también")
            verificar(cerrada(t.portas[2]), "pero no a Internet (8.8.8.8)")
            base.esperar_log(m, "RECHAZADO: destino fuera de la red local (redirigir=red-local)", 10, marca)
            t.fechar()

            cl.ssh("echo redirigir=quizas > /home/.ssh/sshd.conf")
            t = Tunel(cl, [f"localhost:{ECO}"])
            tuneis.append(t)
            verificar(cerrada(t.portas[0]), "un valor de redirigir que no se entiende cierra la puerta (como no)")
            t.fechar()
            cl.ssh("rm /home/.ssh/sshd.conf")
            verificar(eco(Tunel_tmp(cl, tuneis).portas[0], b"de serie\n") == b"de serie\n\nFIN 9\n",
                      "sin la opción (de serie, redirigir=local), a localhost sí")

            r = cl.ssh("cat /tmp/.servicios/sshd.log /logs/sshd.log 2>/dev/null")
            n = r.stdout.decode("latin-1").count("túnel")
            verificar(n >= 30, f"el registro del sshd tiene las líneas de los túneles ({n})")
            verificar("sshd: sesión sin memoria" not in m.serial() and "PANICO" not in m.serial().upper(),
                      "sin errores de memoria ni pánicos")
        finally:
            for t in tuneis:
                t.fechar()
            anf.fechar()


def Tunel_tmp(cl, tuneis):
    t = Tunel(cl, [f"localhost:{ECO}"])
    tuneis.append(t)
    return t


def main():
    os.makedirs(TRAB, exist_ok=True)
    for prog in ("ssh", "ssh-keygen"):
        if not shutil.which(prog):
            print(f"FALHOU: falta {prog} (OpenSSH) en el anfitrión")
            return 1
    maquina()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
