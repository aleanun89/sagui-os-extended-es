# SPDX-License-Identifier: GPL-2.0-or-later
"""
emu86box.py - control de 86Box para pruebas a petición de SaguiOS (D301).

86Box emula PC antiguos (Pentium, Pentium II, K6; 3dfx, S3, ATI, Matrox; Sound Blaster; NE2000)
que no tenemos. Esta biblioteca tiene una interfaz parecida a la de qemu.py (Maquina):
esperar un texto en el serie, enviar teclas, capturar la pantalla y apagar.

NUNCA forma parte de la batería: el nombre del fichero y de las pruebas que lo usan no empieza
por «h» + dos dígitos (ver rodar_paralelo.py). Las ROM (BIOS con copyright) viven en
~/.local/saguitools/86box/roms, fuera del repositorio. Instalación: tools/86box_instalar.sh.

Cómo funciona:
  - la configuración sale de tools/86box/maquinas/<maquina>.cfg (plantilla sin ROM, con
    @IMAGEN@, @PISTAS@, @SERIE@ y @CAPTURAS@), que se copia a build/86box/<nombre>/;
  - la imagen se copia (dispersa) a esa carpeta: 86Box escribe en ella;
  - COM1 va a un fichero por el dispositivo «File (COM)» de 86Box (el serie del kernel);
  - con sin_pantalla=True se levanta un Xvfb propio (sin sudo, ver el envoltorio xvfb);
  - teclas con xdotool (XTEST) y capturas con xwd -> PNG;
  - todo se para por PID, nunca con pkill -f.
"""
import os
import pty
import re
import shutil
import threading
import tty
import signal
import struct
import subprocess
import time
import zlib

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_86BOX = os.path.expanduser("~/.local/saguitools/86box")
BIN = os.path.join(BASE_86BOX, "bin")
ROMS = os.path.join(BASE_86BOX, "roms")
MAQUINAS = os.path.join(RAIZ, "tools", "86box", "maquinas")
SALIDA = os.path.join(RAIZ, "build", "86box")
TEMPO_MAX = 180
# Las ROM no se pueden subir NUNCA (D301): se usan siempre desde ~/.local/saguitools, sin copiarlas.
assert not os.path.abspath(ROMS).startswith(RAIZ + os.sep), "las ROM de 86Box no pueden estar dentro del repositorio"


class FalhaTeste(Exception):
    pass


def instalado():
    """True si 86Box, las ROM y las herramientas de pantalla están donde las pone el instalador."""
    return all(os.path.exists(r) for r in (os.path.join(BIN, "86box"), os.path.join(ROMS, "machines"),
                                           os.path.join(BIN, "xvfb"), os.path.join(BIN, "xdotool"),
                                           os.path.join(BIN, "xwd")))


def maquinas_disponibles():
    return sorted(f[:-4] for f in os.listdir(MAQUINAS) if f.endswith(".cfg"))


def xwd_a_png(origen, destino):
    """Convierte una captura de xwd (32 bpp, TrueColor) a PNG sin ImageMagick."""
    with open(origen, "rb") as f:
        d = f.read()
    cab = struct.unpack(">25I", d[:100])
    tam_cab, ancho, alto = cab[0], cab[4], cab[5]
    orden, bpp, bpl, ncolores = cab[7], cab[11], cab[12], cab[19]
    mr, mg, mb = cab[14], cab[15], cab[16]
    px = d[tam_cab + ncolores * 12:]
    if bpp not in (24, 32) or (mr, mg, mb) != (0xFF0000, 0x00FF00, 0x0000FF) or orden != 0:
        raise FalhaTeste(f"xwd no soportado: {bpp} bpp, máscaras {mr:x}/{mg:x}/{mb:x}, orden {orden}")
    bx = bpp // 8
    filas = []
    for y in range(alto):
        f = px[y * bpl:y * bpl + ancho * bx]
        fila = bytearray(b"\x00" + bytes(ancho * 3))   # filtro PNG 0 + RGB
        fila[1::3] = f[2::bx]
        fila[2::3] = f[1::bx]
        fila[3::3] = f[0::bx]
        filas.append(bytes(fila))

    def trozo(tipo, datos):
        c = struct.pack(">I", len(datos)) + tipo + datos
        return c + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", ancho, alto, 8, 2, 0, 0, 0)) +
           trozo(b"IDAT", zlib.compress(b"".join(filas), 6)) + trozo(b"IEND", b""))
    with open(destino, "wb") as f:
        f.write(png)
    return ancho, alto


# xdotool manda la tecla de un teclado de EE. UU. y SaguiOS la lee con la distribución española: los signos
# van por la tecla que los da en el teclado español (un "-" a secas salía como "'").
TECLAS_SIGNOS = {" ": "space", "\n": "Return", "\t": "Tab", "-": "slash", "_": "shift+slash", "/": "shift+7",
                 ".": "period", ",": "comma", ":": "shift+period", "=": "shift+0", "&": "shift+6",
                 "<": "less", ">": "shift+less"}


class Maquina86Box:
    """Una máquina de tools/86box/maquinas arrancando una imagen de SaguiOS (disco IDE raw)."""

    def __init__(self, maquina, imagem, nome=None, sin_pantalla=True, salida=None, resolucion="1280x1024x24",
                 teclado_ritmo=0.12):
        if not instalado():
            raise FalhaTeste("86Box no instalado: bash tools/86box_instalar.sh")
        cfg = maquina if os.path.isfile(maquina) else os.path.join(MAQUINAS, maquina + ".cfg")
        if not os.path.isfile(cfg):
            raise FalhaTeste(f"no existe la máquina {maquina!r}; hay: {', '.join(maquinas_disponibles())}")
        self.cfg_plantilla = cfg
        self.maquina = os.path.basename(cfg)[:-4]
        self.nome = nome or f"{self.maquina}-{os.getpid()}"
        self.imagem_origen = os.path.abspath(imagem)
        self.sin_pantalla = sin_pantalla
        self.resolucion = resolucion
        self.ritmo = teclado_ritmo
        self.dir = salida or os.path.join(SALIDA, self.nome)
        self.dir_vm = os.path.join(self.dir, "vm")
        self.dir_capturas = os.path.join(self.dir, "capturas")
        self.arq_serial = os.path.join(self.dir, "serie.txt")
        self.imagem = os.path.join(self.dir, "disco.img")
        self.proc = None
        self.proc_x = None
        self.display = None
        self._tmp_x = None

    # ------------------------------------------------------------ ciclo de vida
    def _preparar(self):
        shutil.rmtree(self.dir, ignore_errors=True)
        for d in (self.dir_vm, self.dir_capturas):
            os.makedirs(d)
        # Copia dispersa: 86Box escribe en el disco y la original no debe cambiar.
        subprocess.run(["cp", "--sparse=always", self.imagem_origen, self.imagem], check=True)
        # COM1: 86Box (passthrough) abre este pty como puerto serie del anfitrión; un hilo copia lo
        # que sale a serie.txt al momento (el dispositivo «File (COM)» de 86Box escribe en bloques
        # de 4 KB y el serie llegaría con retraso).
        self._pty_m, self._pty_s = pty.openpty()
        tty.setraw(self._pty_s)
        self.pty = os.ttyname(self._pty_s)
        self._hilo = threading.Thread(target=self._leer_pty, daemon=True)
        self._hilo.start()
        pistas = os.path.getsize(self.imagem) // (63 * 16 * 512)
        with open(self.cfg_plantilla, encoding="utf-8") as f:
            t = f.read()
        for k, v in (("@IMAGEN@", self.imagem), ("@PISTAS@", str(pistas)), ("@SERIE@", self.pty),
                     ("@CAPTURAS@", self.dir_capturas)):
            t = t.replace(k, v)
        with open(os.path.join(self.dir_vm, "86box.cfg"), "w", encoding="utf-8") as f:
            f.write(t)
        open(self.arq_serial, "w").close()

    def _leer_pty(self):
        with open(self.arq_serial, "ab", buffering=0) as f:
            while True:
                try:
                    d = os.read(self._pty_m, 4096)
                except OSError:
                    return
                if not d:
                    return
                f.write(d)

    def enviar_serial(self, texto):
        """Escribe en COM1 de la máquina (entrada del puerto serie)."""
        os.write(self._pty_m, texto.encode("latin-1"))

    def _levantar_xvfb(self):
        fich = os.path.join(self.dir, "display")
        self.proc_x = subprocess.Popen([os.path.join(BIN, "xvfb"), fich, self.resolucion],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(fich) and open(fich).read().strip():
                break
            if self.proc_x.poll() is not None:
                raise FalhaTeste("Xvfb no arranca (ver tools/86box_instalar.sh --comprobar)")
            time.sleep(0.1)
        self.display = ":" + open(fich).read().strip()
        time.sleep(0.5)

    def ligar(self):
        # Un SIGTERM (p. ej. el timeout de la orden) debe pasar por desligar(): si no, 86Box y Xvfb quedan huérfanos.
        try:
            signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(SystemExit(143)))
        except ValueError:
            pass   # no es el hilo principal
        self._preparar()
        env = dict(os.environ)
        if self.sin_pantalla:
            self._levantar_xvfb()
            env["DISPLAY"] = self.display
        else:
            self.display = env.get("DISPLAY")
            if not self.display:
                raise FalhaTeste("sin DISPLAY: usa sin_pantalla=True")
        env["QT_QPA_PLATFORM"] = "xcb"
        env["XDG_RUNTIME_DIR"] = self.dir_vm
        self.env = env
        self.log = open(os.path.join(self.dir, "86box.log"), "wb")
        self.proc = subprocess.Popen([os.path.join(BIN, "86box"), "-P", self.dir_vm, "-R", ROMS, "-N", "-W",
                                      os.path.join(self.dir_vm, "86box.cfg")],
                                     env=env, stdout=self.log, stderr=subprocess.STDOUT)
        return self

    def desligar(self):
        for p in (self.proc, self.proc_x):
            if p is None or p.poll() is not None:
                continue
            os.kill(p.pid, signal.SIGTERM)
            try:
                p.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.kill(p.pid, signal.SIGKILL)
                p.wait(timeout=10)
        if getattr(self, "log", None):
            self.log.close()
        for fd in (getattr(self, "_pty_s", None), getattr(self, "_pty_m", None)):
            if fd is not None:
                try:
                    os.close(fd)
                except OSError:
                    pass

    def __enter__(self):
        return self.ligar()

    def __exit__(self, *a):
        self.desligar()

    # ------------------------------------------------------------ serie
    def serial_bruto(self):
        if not os.path.exists(self.arq_serial):
            return ""
        with open(self.arq_serial, "rb") as f:
            return f.read().decode("latin-1")

    def serial(self):
        # Igual que qemu.py: el prompt «sagui@saguios:» se normaliza a «sagui:».
        return re.sub(r"(?:sagui|root)@saguios:", "sagui:", self.serial_bruto())

    def esperar_serial(self, texto, tempo=TEMPO_MAX):
        fim = time.time() + tempo
        while time.time() < fim:
            if texto in self.serial():
                return True
            if self.proc.poll() is not None:
                raise FalhaTeste(f"86Box terminó antes de aparecer {texto!r} (ver 86box.log)")
            time.sleep(0.5)
        raise FalhaTeste(f"tiempo agotado esperando {texto!r} en el serie")

    # ------------------------------------------------------------ pantalla
    def _xdo(self, *args):
        return subprocess.run([os.path.join(BIN, "xdotool"), *args], env=self.env, capture_output=True, text=True)

    def ventana(self):
        """Id de la ventana principal de 86Box (la mayor de las del proceso), o None."""
        r = self._xdo("search", "--pid", str(self.proc.pid), "--onlyvisible")
        mejor, area = None, 0
        for wid in r.stdout.split():
            g = self._xdo("getwindowgeometry", "--shell", wid).stdout
            m = dict(l.split("=") for l in g.split() if "=" in l)
            a = int(m.get("WIDTH", 0)) * int(m.get("HEIGHT", 0))
            if a > area:
                mejor, area = wid, a
        return mejor

    def captura(self, nome, guardar=False, solo_vm=True):
        """Captura la ventana de 86Box (o toda la pantalla del Xvfb con solo_vm=False) a PNG.
        Se deja en <carpeta>/capturas/<nome>.png; guardar=True la copia además a docs/capturas/."""
        xwd = os.path.join(self.dir_capturas, nome + ".xwd")
        wid = self.ventana() if solo_vm else None
        cmd = [os.path.join(BIN, "xwd"), "-silent", "-out", xwd] + (["-id", wid] if wid else ["-root"])
        r = subprocess.run(cmd, env=self.env, capture_output=True)
        if r.returncode != 0 or not os.path.exists(xwd):
            raise FalhaTeste("xwd falló: " + r.stderr.decode("utf-8", "replace"))
        png = os.path.join(self.dir_capturas, nome + ".png")
        xwd_a_png(xwd, png)
        os.remove(xwd)
        if guardar:
            shutil.copy(png, os.path.join(RAIZ, "docs", "capturas", nome + ".png"))
        return png

    def pasar_bios(self, tempo=90, tecla="F1"):
        """Las BIOS de 86Box paran con «CMOS checksum error - Press F1 to continue» en el primer
        arranque (la CMOS está vacía). Pulsa la tecla cada 2 s hasta que el kernel escribe algo
        por el serie (o se agota el tiempo). Devuelve True si el serie ya tiene texto."""
        fim = time.time() + tempo
        while time.time() < fim:
            if self.serial_bruto():
                return True
            if self.proc.poll() is not None:
                raise FalhaTeste("86Box terminó durante la BIOS (ver 86box.log)")
            self.tecla(tecla, 0.1)
            time.sleep(2)
        return bool(self.serial_bruto())

    # ------------------------------------------------------------ teclado
    def tecla(self, nome, espera=None):
        """Una tecla (o combinación «ctrl+alt+Delete») por XTEST, sobre la ventana bajo el puntero."""
        wid = self.ventana()
        if wid:
            self._xdo("mousemove", "--window", wid, "20", "20")
        self._xdo("key", nome)
        time.sleep(self.ritmo if espera is None else espera)

    # D307: SaguiOS lee el teclado con la distribución española y el X de 86Box manda la posición
    # de la tecla de un teclado estadounidense: los signos se escriben por su sitio en el ES.
    TECLAS_ES = {" ": "space", "\n": "Return", "\t": "Tab", "-": "slash", "_": "shift+slash", "/": "shift+7",
                 ".": "period", ",": "comma", ":": "shift+period", ";": "shift+comma", "=": "shift+0"}

    def digitar(self, texto, espera=None):
        for c in texto:
            self.tecla(TECLAS_SIGNOS.get(c, c), espera)

    def comando(self, texto, tempo=60):
        """Escribe una orden en el terminal y espera el prompt siguiente (por el serie)."""
        antes = self.serial()
        n = antes.count("sagui:")
        self.digitar(texto)
        self.tecla("Return")
        fim = time.time() + tempo
        while self.serial().count("sagui:") <= n:
            if time.time() > fim:
                raise FalhaTeste(f"la orden {texto!r} no volvió al prompt")
            time.sleep(0.3)
        return self.serial()[len(antes):].replace("\r", "")


def verificar(condicao, mensagem):
    if not condicao:
        raise FalhaTeste(mensagem)
    print("  ok:", mensagem)


def _principal():
    """Línea de órdenes: la usa tools/86box.sh (lanzador manual)."""
    import argparse
    ap = argparse.ArgumentParser(description="Arranca una máquina de 86Box con una imagen de SaguiOS")
    ap.add_argument("maquina", help="nombre de tools/86box/maquinas/*.cfg (o ruta de un .cfg)")
    ap.add_argument("imagen", help="imagen raw .img (BIOS, no UEFI)")
    ap.add_argument("--sin-pantalla", action="store_true", help="con Xvfb propio (sin ventana)")
    ap.add_argument("--segundos", type=int, default=60, help="cuánto dejarla encendida (60)")
    ap.add_argument("--captura", default="", help="PNG de la pantalla al terminar")
    ap.add_argument("--sin-f1", action="store_true", help="no pulsar F1 en el aviso de CMOS de la BIOS")
    ap.add_argument("--esperar", default="", help="terminar antes si el serie contiene este texto")
    ap.add_argument("--salida", default="", help="carpeta de trabajo (por defecto build/86box/<máquina>-<pid>)")
    a = ap.parse_args()
    # Un SIGTERM al lanzador debe pasar por desligar(): si no quedan 86Box y Xvfb huérfanos.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(SystemExit(143)))
    m = Maquina86Box(a.maquina, a.imagen, sin_pantalla=a.sin_pantalla, salida=a.salida or None)
    with m:
        print(f"86Box: {m.maquina}, carpeta {m.dir}, pantalla {m.display}", flush=True)
        if not a.sin_f1:
            m.pasar_bios()
        fim = time.time() + a.segundos
        while time.time() < fim and m.proc.poll() is None:
            if a.esperar and a.esperar in m.serial():
                break
            time.sleep(0.5)
        if a.captura:
            png = m.captura("final")
            shutil.copy(png, a.captura)
            print("captura:", a.captura)
        print("serie:", m.arq_serial, f"({len(m.serial_bruto())} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(_principal())
