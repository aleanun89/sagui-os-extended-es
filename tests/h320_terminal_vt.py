#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H320 (D320) - el Terminal entiende lo bastante de xterm/VT220 para vim, less, htop y nano por ssh.

En el anfitrión:
  1. tests/terminal_vt_unidad.c: expectativas escritas a mano del emulador (cursor, borrado, regiones de
     desplazamiento, IL/DL, salto de línea pendiente, pantalla alternativa, modos, SGR y 256 colores, cajas DEC,
     DSR y DA, redimensionar) y las traducciones nuevas de ssh_cliente_logica.h;
  2. flujos de bytes grabados de vim, less, htop y nano reales (en un pty de 80x24 con TERM=xterm-256color)
     pasados por el emulador y comparados, fila a fila y con el cursor, con tmux como emulador de referencia.

En QEMU, en el Terminal de ventana del escritorio:
  - /testes/vtprova regiao: región de desplazamiento (LF en el fondo), IL y DL;
  - alt: pantalla alternativa y vuelta a la de antes;
  - cores: 256 colores, truecolor, negrita, tenue, subrayado, inverso, oculto y cajas DEC (píxeles);
  - dsr: respuestas a CSI 6n, 5n, c, > c y 18 t;
  - teclas: F1-F12, cursor, Inicio/Fin/RePág/Supr/Insert, Ctrl+flecha, Alt+tecla, Esc y el modo aplicación;
  - por ssh contra un servidor de prueba (el de h317): una sesión grabada de vim (pantalla alternativa,
    región, colores), la tecla pulsada llega como xterm, y al salir de vim vuelve la pantalla de antes.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, texto_terminal, imagem_de_teste, RAIZ, SAIDA, TERM_MX, TERM_MY, TERM_DY  # noqa: E402
from qemu import TERM_FILA as F  # noqa: E402

BORDA, TITULO = 2, 24
DIR = os.path.join(SAIDA, "h320")
PRG = os.path.join(RAIZ, "userland", "programas")


# ------------------------------------------------------------ anfitrión

def compilar(fuente, exe, extra=()):
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DTERMINAL_ANFITRION", "-I", PRG, *extra, "-o", exe,
                        os.path.join(RAIZ, "tests", fuente)], capture_output=True, text=True)
    verificar(r.returncode == 0, f"{fuente} compila\n{r.stderr}")


def grabar(cmd, teclas, cols=80, lins=24, espera=0.6):
    """Ejecuta cmd en un pty cols x lins con TERM=xterm-256color y devuelve (todo lo que salió, [(posición, tecla)])."""
    import fcntl
    import pty
    import select
    import signal
    import struct
    import termios
    pid, fd = pty.fork()
    if pid == 0:
        env = dict(os.environ, TERM="xterm-256color", LANG="C", LC_ALL="C", COLUMNS=str(cols), LINES=str(lins))
        os.execvpe(cmd[0], cmd, env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", lins, cols, 0, 0))
    salida = bytearray()
    pos = []

    def leer(t):
        fin = time.time() + t
        while time.time() < fin:
            r, _, _ = select.select([fd], [], [], 0.05)
            if r:
                try:
                    d = os.read(fd, 65536)
                except OSError:
                    return False
                if not d:
                    return False
                salida.extend(d)
        return True
    leer(espera)
    for ret, k in teclas:
        pos.append((len(salida), k))
        os.write(fd, k)
        if not leer(ret):
            break
    leer(0.3)
    try:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    except OSError:
        pass
    return bytes(salida), pos


def rejilla_mia(exe, datos, c=80, l=24):
    r = subprocess.run([exe, "--volcar", str(c), str(l)], input=datos, capture_output=True).stdout.decode("latin-1").split("\n")
    cur = [x for x in r if x.startswith("cursor")][0].split()
    return r[:l], (int(cur[1]), int(cur[2]))


def rejilla_tmux(datos, c=80, l=24):
    arq = os.path.join(DIR, "flujo.bin")
    open(arq, "wb").write(datos)
    sock = f"-Lh320{os.getpid()}"
    subprocess.run(["tmux", sock, "-f", "/dev/null", "kill-server"], capture_output=True)
    subprocess.run(["tmux", sock, "-f", "/dev/null", "new-session", "-d", "-x", str(c), "-y", str(l),
                    f"sh -c 'stty raw -echo; cat {arq}; sleep 20'"], check=True)
    time.sleep(0.8)
    out = subprocess.run(["tmux", sock, "capture-pane", "-p", "-t", "0"], capture_output=True, text=True).stdout.split("\n")
    cur = subprocess.run(["tmux", sock, "display", "-p", "-t", "0", "cursor #{cursor_y} #{cursor_x}"],
                         capture_output=True, text=True).stdout.split()
    subprocess.run(["tmux", sock, "kill-server"], capture_output=True)
    tr = str.maketrans("─│┌┐└┘├┤┬┴┼", "-|+++++++++")
    return [x.translate(tr).replace("▒", "#").replace("◆", "#").rstrip() for x in out[:l]], (int(cur[1]) + 1, int(cur[2]) + 1)


def aplicaciones(exe):
    """Graba vim, less, htop y nano reales y compara con tmux. Devuelve el flujo de vim para el ssh."""
    faltan = [p for p in ("vim", "less", "htop", "nano", "tmux") if not subprocess.run(["which", p], capture_output=True).stdout]
    if faltan:
        print(f"  AVISO: faltan en el anfitrión {faltan}: se omite la comparación con aplicaciones reales")
        return None
    f = os.path.join(DIR, "f.txt")
    open(f, "w").write("".join(f"linea {i} del fichero de prueba con foo y bar\n" for i in range(1, 121)))
    casos = {
        "vim": (["vim", "-u", "NONE", "-N", "-n", f], [(0.4, b":set nu\r"), (0.4, b"\x06"), (0.4, b"/foo\r"), (0.4, b"10Gdd"),
                                                  (0.4, b"ihola\x1b"), (0.4, b"\x04")]),
        "vim_sale": (["sh", "-c", f"echo PROMPT-ANTES; vim -u NONE -N -n {f}; echo DESPUES"], [(0.5, b"\x06"), (0.4, b":q\r")]),
        "less": (["less", f], [(0.4, b" "), (0.4, b" "), (0.4, b"b"), (0.4, b"/bar\r")]),
        "htop": (["htop"], [(1.2, b"\x1b[B"), (0.8, b"\x1b[B"), (0.5, b"F")]),
        "nano": (["nano", "-I", f], [(0.4, b"\x16"), (0.4, b"hola"), (0.4, b"\x07")]),
    }
    flujo_vim = None
    for n, (cmd, teclas) in casos.items():
        d, pos = grabar(cmd, teclas)
        if n == "vim_sale":
            flujo_vim = (b"\x1b[H\x1b[2J" + d, pos[-1][0] + 7)   # se parte donde se mandó «:q»
        a, ca = rejilla_mia(exe, d)
        b, cb = rejilla_tmux(d)
        malas = [i for i in range(24) if a[i] != b[i]]
        verificar(not malas and ca == cb, f"{n}: la rejilla coincide con tmux (filas distintas {malas[:4]}; cursor {ca} / {cb}; "
                                          f"{a[malas[0]]!r} / {b[malas[0]]!r})" if malas else f"{n}: la rejilla coincide con tmux (cursor {ca} / {cb})")
    return flujo_vim


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "terminal_vt_unidad")
    compilar("terminal_vt_unidad.c", exe)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "emulador: expectativas escritas a mano\n" + r.stdout)
    exe2 = os.path.join(DIR, "ssh_cliente_unidad")
    fontes = [os.path.join(RAIZ, "tests", "ssh_cliente_unidad.c"), os.path.join(RAIZ, "kernel", "lib", "sha256.c")]
    fontes += [os.path.join(RAIZ, "userland", "libtls", f + ".c") for f in ("ed25519", "hash", "chacha", "sshcifra", "x25519")]
    r = subprocess.run(["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", PRG, "-O2", "-o", exe2] + fontes, capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "ssh_cliente_unidad.c compila sin avisos" + r.stderr[:1500])
    r = subprocess.run([exe2], capture_output=True, text=True, timeout=600)
    n = re.search(r"ssh_cliente_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
    verificar(r.returncode == 0 and n and n.group(1) == n.group(2), "ssh: cajas DEC, teclas xterm y lo de siempre: " + r.stdout.strip()[-300:])
    return aplicaciones(exe)


# ------------------------------------------------------------ QEMU

def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[1]), a=int(g[2]), cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def pantalla(m, j, cols=94, lins=28):
    ppm = os.path.join(SAIDA, "h320-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    larg, _, px = ler_ppm(ppm)
    x0, y0 = j["cx"] + TERM_MX, j["cy"] + TERM_MY
    tela = texto_terminal(px, larg, x0, y0, cols, lins)

    def pixel(x, y):   # relativos a la rejilla
        o = ((y0 + y) * larg + x0 + x) * 3
        return px[o], px[o + 1], px[o + 2]
    return tela, pixel


def esperar(m, texto, desde, tempo=40):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return m.serial()[desde:]
        time.sleep(0.2)
    raise AssertionError(f"no apareció {texto!r} en la serie: {m.serial()[desde:][-500:]!r}")


def region(m, t):
    marca = len(m.serial())
    m.digitar("/testes/vtprova regiao\n", 0.04)
    esperar(m, "vtprova: paso 1", marca)
    time.sleep(0.8)
    tela, _ = pantalla(m, t)
    f = lambda i: tela[i - 1]   # noqa: E731
    verificar(f(4).startswith("L04") and f(5).startswith("L08") and f(17).startswith("L20"),
              f"región 5..20: tras tres LF las líneas suben dentro de ella ({[f(i)[:3] for i in (4, 5, 6, 17)]})")
    verificar(f(18).startswith("X1") and f(19).startswith("X2") and f(20).startswith("X3") and f(21).startswith("L21"),
              f"... el texto nuevo entra por abajo y lo de fuera no se mueve ({[f(i)[:3] for i in (18, 19, 20, 21)]})")
    verificar(f(1).startswith("L01") and f(28).startswith("L28"), "las líneas fuera de la región (1 y 28) se quedan")
    m.captura("H320-1-region")
    esperar(m, "vtprova: paso 2", marca)
    time.sleep(0.8)
    tela, _ = pantalla(m, t)
    f = lambda i: tela[i - 1]   # noqa: E731
    verificar(f(6) == "" and f(7) == "" and f(8).startswith("L09") and f(10).startswith("L12") and f(19).startswith("X1") and f(20) == "",
              f"IL 2 en la fila 6 y DL 1 en la 10 ({[f(i)[:3] for i in (6, 7, 8, 10, 19, 20)]})")
    verificar(f(21).startswith("L21"), "IL y DL no tocan lo que queda bajo la región")
    m.captura("H320-2-il-dl")
    time.sleep(3.5)


def alt(m, t):
    marca = len(m.serial())
    m.digitar("/testes/vtprova alt\n", 0.04)
    esperar(m, "vtprova: paso 1", marca)
    time.sleep(0.8)
    tela, _ = pantalla(m, t)
    verificar(tela[0].startswith("ALTERNA") and not any("ANTES" in l for l in tela),
              f"pantalla alternativa: solo se ve lo de la alternativa ({tela[:4]})")
    m.captura("H320-3-pantalla-alternativa")
    esperar(m, "vtprova: paso 2", marca)
    time.sleep(0.8)
    tela, _ = pantalla(m, t)
    verificar(tela[2].startswith("ANTES VOLVI") and not any("ALTERNA" in l for l in tela),
              f"al salir vuelve la pantalla de antes y el cursor donde estaba ({tela[2]!r})")
    m.captura("H320-4-vuelta")
    time.sleep(3.5)


def cores(m, t):
    marca = len(m.serial())
    m.digitar("/testes/vtprova cores\n", 0.04)
    esperar(m, "vtprova: colores listos", marca)
    time.sleep(0.8)
    tela, px = pantalla(m, t)
    verificar(tela[0].startswith("ROJO256 R196 V46") and "TRUE" in tela[0], f"textos de la fila de colores ({tela[0]!r})")
    verificar(tela[1].startswith("NEGRITA TENUE") and "INVERSO" in tela[1] and tela[1].rstrip().endswith("|fin"),   # el subrayado estropea el OCR
              f"negrita, tenue, subrayado, inverso y oculto ({tela[1]!r})")

    def colores_fila(fila, c0, c1):
        s = set()
        for y in range(fila * F, fila * F + F):
            for x in range(c0 * 8, c1 * 8):
                s.add(px(x, y))
        return s
    ROJO_VIVO, VERDE_VIVO, AZUL_VIVO, AZUL_FONDO = (0xFF, 0x8E, 0x8E), (0xA0, 0xE8, 0xA0), (0x8D, 0xBB, 0xFF), (0x8D, 0xBB, 0xFF)
    verificar(ROJO_VIVO in colores_fila(0, 0, 7), "91 -> rojo vivo")
    verificar(ROJO_VIVO in colores_fila(0, 8, 12), "38;5;196 -> rojo vivo de la paleta de 16")
    verificar(VERDE_VIVO in colores_fila(0, 13, 16), "38;5;46 -> verde vivo")
    verificar(AZUL_FONDO in colores_fila(0, 17, 20), "48;5;21 -> fondo azul vivo")
    verificar(VERDE_VIVO in colores_fila(0, 21, 25), "38;2;0;255;0 (truecolor) -> verde vivo")
    # subrayado: la última fila de píxeles de la celda lleva la línea
    letra = (0xD8, 0xDD, 0xE2)
    verificar(all(px(c * 8 + 4, F + TERM_DY + 15) == letra for c in range(14, 23)), "subrayado: línea en la última fila de la letra")
    # oculto: tras «INVERSO » el oculto no se ve y «|fin» sigue (cols 38..45)
    verificar(tela[1][38:45].strip() == "" or "OCULTO" not in tela[1], "el texto oculto no se ve")
    # cajas DEC: en la fila 3..7, la esquina y las líneas son trazos del color de la letra
    fondo = (0x12, 0x16, 0x1E)
    cx, cy = 4, F // 2   # D385: la celda mide F (letra + interlineado); las cajas ocupan la celda entera
    verificar(px(cx, 2 * F + cy) == letra and px(1, 2 * F + cy) == fondo and px(7, 2 * F + cy) == letra
              and px(cx, 2 * F + F - 1) == letra and px(cx, 2 * F + 1) == fondo,
              "caja DEC: la esquina ┌ (línea hacia la derecha y hacia abajo)")
    verificar(px(2 * 8 + 1, 2 * F + cy) == letra and px(2 * 8 + 6, 2 * F + cy) == letra, "caja DEC: la línea horizontal ─")
    verificar(px(0 * 8 + cx, 3 * F + 5) == letra and px(0 * 8 + 1, 3 * F + 5) == fondo, "caja DEC: la línea vertical │")
    verificar(px(cx, 4 * F + 4) == letra and px(7, 4 * F + cy) == letra and px(1, 4 * F + cy) == fondo,
              "caja DEC: el cruce ├ (arriba, abajo y derecha, sin izquierda)")
    verificar(px(5 * 8 + cx, 4 * F + cy) == letra and px(5 * 8 + 1, 4 * F + cy) == letra and px(5 * 8 + 7, 4 * F + cy) == fondo,
              "caja DEC: el cruce ┤ (arriba, abajo y izquierda)")
    verificar(tela[2].strip() == "" and "q" not in tela[2], "las cajas no escriben letras")
    # bce: «ESC [ 44 m ESC [ K» pinta el resto de la fila con el fondo
    verificar(px(90 * 8 + 3, 7 * F + 8) == (0x6C, 0x9E, 0xE0), "borrar con el fondo actual (bce): resto de la fila en azul")
    m.captura("H320-5-colores-cajas")
    time.sleep(5)


def dsr(m, t):
    marca = len(m.serial())
    m.digitar("/testes/vtprova dsr\n", 0.04)
    s = esperar(m, "vtprova: dsr listo", marca, 40)
    verificar("CSI 6n en 5;9 -> ^[[5;9R" in s, "CSI 6n devuelve la posición del cursor (5;9)")
    verificar("CSI 5n -> ^[[0n" in s, "CSI 5n: terminal en orden")
    verificar("CSI c -> ^[[?62;22c" in s, "DA primario: VT220 con color")
    verificar(re.search(r"CSI > c -> \^\[\[>0;10;0c", s), "DA secundario")
    verificar(re.search(r"CSI 18 t -> \^\[\[8;28;94t", s), "CSI 18 t: tamaño de la ventana (28x94)")


def teclas(m, t):
    marca = len(m.serial())
    m.digitar("/testes/vtprova teclas\n", 0.04)
    esperar(m, "vtprova: modo xterm listo", marca)
    time.sleep(0.5)
    casos = [("up", "^[[A"), ("f1", "^[OP"), ("f2", "^[OQ"), ("f4", "^[OS"), ("f5", "^[[15~"), ("f6", "^[[17~"), ("f10", "^[[21~"),
             ("pgup", "^[[5~"), ("pgdn", "^[[6~"), ("delete", "^[[3~"), ("insert", "^[[2~"), ("home", "^[[H"), ("end", "^[[F"),
             ("ctrl-up", "^[[1;5A"), ("ctrl-right", "^[[1;5C"), ("shift-up", "^[[1;2A"), ("alt-x", "^[x"), ("esc", "^["),
             ("a", "tecla -> a")]
    for k, _ in casos:
        n = len(m.serial())
        m.tecla(k, 0.5)
        esperar(m, "vtprova: tecla", n, 10)
    s = m.serial()[marca:]
    obtenidos = re.findall(r"vtprova: tecla -> (.*)", s)
    verificar(len(obtenidos) == len(casos), f"llegaron {len(obtenidos)} teclas de {len(casos)}: {obtenidos}")
    for (k, esp), got in zip(casos, obtenidos):
        verificar(got.strip() == esp.replace("tecla -> ", ""), f"tecla {k} -> {got.strip()} (esperada {esp.replace('tecla -> ', '')})")
    m.tecla("q", 0.3)
    time.sleep(1.5)


def ssh_vim(m, t, flujo):
    from h317_ssh_cliente import Servidor, Conexao, SENHA
    d, ini = flujo
    srv = Servidor()

    def shell(self):
        self.canal_dados(d[:ini])
        self.log("parte1 enviada")
        while True:
            r = self.receber()
            if r[0] in (96, 97):
                self.log("shell: eof")
                return self.fim(0)
            if r[0] != 94:
                continue
            from h317_ssh_cliente import Leitor
            l = Leitor(r, 1)
            l.u32()
            tec = l.str()
            self.log("tec " + tec.hex())
            if tec == b"n":
                self.canal_dados(d[ini:])
                self.log("parte2 enviada")
            elif tec == b"\x04":
                self.canal_dados(b"\r\n")
                return self.fim(0)
    Conexao.shell = shell
    srv.senha = SENHA
    srv.autorizada = None
    marca = len(m.serial())
    m.digitar(f"ssh -o StrictHostKeyChecking=accept-new -p {srv.porta} h320@10.0.2.2\n", 0.04)
    esperar(m, "Contrase", marca, 60)
    m.digitar(SENHA + "\n", 0.05)
    srv.esperar("parte1 enviada", 40)
    time.sleep(2.0)
    tela, _ = pantalla(m, t)
    return srv, tela


# ------------------------------------------------------------ principal

def parte_qemu(flujo_vim):
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--sin-conf"])
    with Maquina("h320", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        t = janela(m.serial(), "Terminal")
        region(m, t)
        alt(m, t)
        cores(m, t)
        dsr(m, t)
        teclas(m, t)
        out = m.comando("echo sigue-bien")
        verificar("sigue-bien" in out, "el Terminal sigue sirviendo al sagsh tras los modos xterm")
        if flujo_vim is not None:
            fim = time.time() + 40
            while time.time() < fim and "(DHCP)" not in m.serial():
                time.sleep(0.3)
            esperado, cur = rejilla_tmux(flujo_vim[0][:flujo_vim[1]])
            srv, tela = ssh_vim(m, t, flujo_vim)
            ok = sum(1 for i in range(23) if tela[i].rstrip() == esperado[i].rstrip())
            verificar(ok >= 21, f"vim por ssh: {ok} de 23 filas de la pantalla iguales a la de tmux "
                                f"(distintas: {[(i, tela[i], esperado[i]) for i in range(23) if tela[i].rstrip() != esperado[i].rstrip()][:3]})")
            m.captura("H320-6-vim-ssh")
            m.digitar("n", 0.1)
            srv.esperar("parte2 enviada", 20)
            time.sleep(2.0)
            tela, _ = pantalla(m, t)
            esperado, cur = rejilla_tmux(flujo_vim[0])
            ok = sum(1 for i in range(24) if tela[i].rstrip() == esperado[i].rstrip())
            verificar(ok >= 23 and tela[0].startswith("PROMPT-ANTES"),
                      f"al salir de vim vuelve la pantalla de antes ({ok} de 24 filas iguales a tmux; {tela[:3]})")
            m.captura("H320-7-vim-sale")
            n0 = len(m.serial())
            m.tecla("ctrl-d", 0.3)
            esperar(m, "sagui:/", n0, 40)
            time.sleep(1.5)
            out = m.comando("echo vuelta-ok")
            verificar("vuelta-ok" in out, "tras cerrar el ssh, el sagsh vuelve a su modo de siempre")


def main():
    flujo = unidad()
    parte_qemu(flujo)


if __name__ == "__main__":
    main()
