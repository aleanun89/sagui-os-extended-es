#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H393 - El Terminal y sagsh con mejor formato; «ayuda» en vez de «ajuda»; ~/.sagshrc (D385, v8.5.4.10).

Criterio:
  unidad (PC): tests/terminal_unidad.c (interlineado en terminal.conf, con lo de siempre);
  QEMU (escritorio, Terminal con TERM=xterm-256color):
  - el saludo dice «ayuda»; «ayuda» lista por secciones y sin pasar del ancho; «ajuda» (el nombre de antes)
    da lo mismo; «ayuda alias» lista los nombres en español; «type ajuda» lo explica;
  - el shell sabe el tamaño de la ventana: COLUMNS=94 y LINES=28 (pregunta ESC [ 18 t en cada prompt);
  - «ls /bin» en columnas que caben en 94; «ls -1» y las tuberías, uno por línea; orden sin distinguir
    mayúsculas y con las carpetas primero en el terminal; «ls -l» alineado; «ls -s» el formato de antes;
  - el prompt sale con color y no queda ningún cursor «fantasma» en las líneas de órdenes anteriores;
  - una línea larga sigue en la fila de abajo; la barra de desplazamiento aparece con historial y el clic en
    ella sube una página; Mayús+RePág también;
  - al redimensionar la ventana, el siguiente prompt pone COLUMNS al ancho nuevo;
  - ~/.sagshrc: un alias y un PS1 definidos en él valen en un shell nuevo; un error de sintaxis avisa y el
    shell se abre igual (con lo de antes del error); «sagsh --sin-rc» no lo lee; /etc/skel/.sagshrc existe;
  - rendimiento: «seq 1 20000» (se anota el tiempo; antes de D385, 2,7 s con KVM).
Capturas H393-*.png."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, texto_terminal, RAIZ, SAIDA, TERM_MX, TERM_MY, TERM_FILA, TERM_DY  # noqa: E402

BORDA, TITULO = 2, 24
DIR = os.path.join(SAIDA, "h393")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "terminal_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DTERMINAL_ANFITRION", "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-o", exe, os.path.join(RAIZ, "tests", "terminal_unidad.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "terminal_unidad: todo bien (con el interlineado)")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]), cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def pantalla(m, j, cols=94, lins=28):
    ppm = os.path.join(SAIDA, "h393-tela.ppm")
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

    def pixel(x, y):   # relativo a la ventana (área de cliente)
        o = ((j["cy"] + y) * larg + j["cx"] + x) * 3
        return px[o], px[o + 1], px[o + 2]
    return tela, pixel


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def sin_esc(t):
    return re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", t)


def linhas(out):
    """Las líneas de la salida, sin secuencias y sin los avisos del kernel ([SAGUI] proceso terminó)."""
    return [l for l in sin_esc(out).split("\n") if not l.startswith("[SAGUI]")]


verde = lambda r, g, b: g > r + 40 and g > b + 40          # noqa: E731
FUNDO = (0x12, 0x16, 0x1E)


CURSOR = (0xD8, 0xDD, 0xE2)   # el bloque del cursor del tema oscuro


def celdas_cursor(px, filas=28, cols=94):
    """Las celdas (fila, columna) donde se ve el bloque del cursor (la esquina de arriba a la derecha es fondo o cursor)."""
    return [(f, c) for f in range(filas) for c in range(cols)
            if px(TERM_MX + c * 8 + 7, TERM_MY + f * TERM_FILA) == CURSOR]


def editar_y_ver(m, t, teclas_antes, esperado_fila, esperado_texto, rotulo):
    """Teclea, mira dónde queda el cursor y qué dice la pantalla."""
    for k in teclas_antes:
        if k.startswith("+"):
            m.digitar(k[1:], 0.05)
        else:
            m.tecla(k, 0.12)
    time.sleep(0.6)
    tela, px = pantalla(m, t)
    cur = celdas_cursor(px)
    return tela, cur


def roda(m, entalhes):
    passo = -1 if entalhes > 0 else 1
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {passo}")
        time.sleep(0.15)
    time.sleep(0.4)


def parte_qemu():
    with Maquina("h393", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1.5)
        s = m.serial()
        t = janela(s, "Terminal")
        verificar("Escribe ayuda" in s and "ajuda" not in s[s.find("executar /bin/sagsh"):], "el saludo del shell dice «ayuda»")

        # ---- tamaño del terminal
        out = m.comando("echo cols=$COLUMNS lins=$LINES")
        verificar("cols=94 lins=28" in out, f"el shell sabe el tamaño de la ventana ({out.strip()[-40:]!r})")
        verificar("\x1b[18t" not in m.serial_bruto(), "la pregunta del tamaño no sale por la serie")

        # ---- ayuda
        out = m.comando("ayuda")
        ls_ = linhas(out)
        verificar("Archivos y carpetas" in out and "Texto" in out and "Red" in out and "Juegos" in out and "El shell" in out,
                  "ayuda: por secciones")
        largas = [l for l in ls_ if len(l) > 94]
        verificar(not largas, f"ayuda: ninguna línea pasa de 94 columnas ({largas[:2]})")
        fila = [l for l in ls_ if re.match(r"^  ls\s+lista archivos", l)]
        verificar(fila and re.search(r"\S\s{3,}\S", fila[0][20:]), f"ayuda: en columnas ({fila[:1]})")
        m.captura("H393-1-ayuda")
        out2 = m.comando("ajuda")
        verificar("Archivos y carpetas" in out2 and len(linhas(out2)) == len(ls_), "«ajuda» (el nombre de antes) sigue valiendo")
        out = m.comando("type ajuda")
        verificar("otro nombre de ayuda" in out, f"type ajuda: otro nombre de ayuda ({out.strip()[-80:]!r})")
        out = m.comando("ayuda alias")
        verificar("listar = ls" in out and "buscar = grep" in out and "limpiar = clear" in out, "ayuda alias: los nombres en español")
        out = m.comando("ayuda grep")
        verificar("Uso: grep" in out and "buscar" in out and "procurar (el de antes)" in out, "ayuda grep: uso y otros nombres")
        out = m.comando("buscar nada /etc/sagshrc; echo st=$?")
        verificar("st=1" in out and "no encontrado" not in out, "el alias en español «buscar» es grep")
        m.comando("clear")

        # ---- ls
        out = m.comando("ls /bin")
        fs = [l for l in linhas(out) if l.strip() and "ls /bin" not in l and "sagui:" not in l]
        nprog = len(os.listdir(os.path.join(RAIZ, "build", "rootfs", "bin")))
        verificar(fs and all(len(l) < 94 for l in fs), f"ls /bin: ninguna fila pasa de 94 ({max((len(l) for l in fs), default=0)})")
        verificar(len(fs) * 3 < nprog and len(fs[0].split()) >= 4, f"ls /bin en columnas: {len(fs)} filas para unos {nprog} programas")
        verificar("bytes" not in out, "ls /bin: ya no el formato de antes")
        m.captura("H393-2-ls-columnas")
        out = m.comando("ls /bin | head -3")
        fs = [l for l in linhas(out) if l.strip() and "sagui:" not in l and "head" not in l]
        verificar(len(fs) == 3 and all(" " not in l for l in fs), f"ls en una tubería: uno por línea ({fs})")
        m.comando("mkdir -p /tmp/o/zeta; touch /tmp/o/b.txt /tmp/o/A.txt /tmp/o/c.txt /tmp/o/.oculto")
        out = m.comando("ls -1 /tmp/o")
        nomes = [l.strip() for l in linhas(out) if l.strip() and "sagui:" not in l and "ls -1" not in l]
        verificar(nomes == ["A.txt", "b.txt", "c.txt", "zeta"], f"ls -1: orden sin distinguir mayúsculas ({nomes})")
        out = m.comando("ls /tmp/o")
        f = [l for l in linhas(out) if "zeta/" in l]
        verificar(f and f[0].strip().startswith("zeta/") and "A.txt" in f[0], f"ls en el terminal: carpetas primero, en columnas ({f[:1]})")
        out = m.comando("ls -a /tmp/o")
        verificar(".oculto" in out, "ls -a: los ocultos")
        out = m.comando("ls -l /etc")
        fl = [l for l in linhas(out) if re.match(r"^[d-][rwx-]{9} ", l)]
        cols_data = {re.search(r"\d\d/\d\d/\d{4} \d\d:\d\d", l).start() for l in fl if re.search(r"\d\d/\d\d/\d{4}", l)}
        verificar(len(fl) >= 3 and len(cols_data) == 1, f"ls -l: las fechas alineadas en una columna ({cols_data})")
        out = m.comando("ls -lh /bin/sagsh")
        verificar(re.search(r"\d+(\.\d)?K\s+/bin/sagsh", out), f"ls -lh: tamaño legible ({out.strip()[-80:]!r})")
        out = m.comando("ls -s /tmp/o")
        verificar(re.search(r"^b\.txt\s+0 bytes", out, re.M), "ls -s: el formato de antes")
        m.comando("clear")

        # ---- editar la línea con las flechas: el cursor de la pantalla va donde el del shell
        m.comando("clear")
        P = len("root@saguios:/$ ")   # el prompt de las imágenes de prueba (sesión root, sin «#»)
        tela, cur = editar_y_ver(m, t, ["+echo hola", "left", "left", "left"], 0, None, "")
        verificar(cur == [(0, P + 6)], f"echo hola y 3 flechas a la izquierda: el cursor, en la «o» de «hola» ({cur})")
        tela, cur = editar_y_ver(m, t, ["+X"], 0, None, "")
        verificar(tela[0].endswith("$ echo hXola") and cur == [(0, P + 7)],
                  f"insertar X: «echo hXola» y el cursor tras la X ({tela[0]!r}, {cur})")
        tela, cur = editar_y_ver(m, t, ["home"], 0, None, "")
        verificar(cur == [(0, P)], f"Inicio: el cursor al principio de la orden ({cur})")
        tela, cur = editar_y_ver(m, t, ["end"], 0, None, "")
        verificar(cur == [(0, P + 10)], f"Fin: el cursor al final ({cur})")
        out = m.comando("", teclas_finais=("ret",))
        verificar(re.search(r"^hXola$", out, re.M), "la orden editada se ejecuta como se ve")
        m.captura("H393-8-cursor-flechas")
        # una línea más larga que la ventana: Inicio, flechas, insertar; Fin; cruzar la frontera de fila
        m.comando("clear")
        larga = "echo " + "a" * 100 + "FIN"            # 16 + 108 = 124 letras: dos filas de 94
        tela, cur = editar_y_ver(m, t, ["+" + larga], 0, None, "")
        verificar(cur == [(1, (P + len(larga)) % 94)], f"línea larga: el cursor al final, en la segunda fila ({cur})")
        tela, cur = editar_y_ver(m, t, ["ctrl-a"] + ["right"] * 5 + ["+Z"], 0, None, "")
        verificar(cur == [(0, P + 6)] and tela[0].startswith("root@saguios:/$ echo Zaaa"),
                  f"Ctrl+A, 5 a la derecha y Z: en la primera fila ({cur}, {tela[0][:30]!r})")
        tela, cur = editar_y_ver(m, t, ["ctrl-e"] + ["left"] * 40, 0, None, "")
        pos = P + len(larga) + 1 - 40
        verificar(cur == [(pos // 94, pos % 94)], f"Ctrl+E y 40 a la izquierda: cruza a la fila de arriba ({cur}, esperado {(pos // 94, pos % 94)})")
        tela, cur = editar_y_ver(m, t, ["+Y"], 0, None, "")
        texto = (tela[0] + tela[1]).replace("root@saguios:/$ ", "", 1)
        esperado = "echo Z" + "a" * 100 + "FIN"
        esperado = esperado[:len(esperado) - 40] + "Y" + esperado[len(esperado) - 40:]
        verificar(texto == esperado, f"insertar en medio de una línea partida: la pantalla queda bien ({texto[-50:]!r})")
        out = m.comando("", teclas_finais=("ret",))
        verificar(esperado[5:] in out.replace("\n", ""), "la orden larga editada se ejecuta como se ve")
        # con acentos (una letra Latin-1, una celda)
        m.comando("clear")
        tela, cur = editar_y_ver(m, t, ["+echo cañón", "left", "left", "+X"], 0, None, "")
        verificar(tela[0].endswith("$ echo cañXón") and cur == [(0, P + 9)], f"acentos: «cañXón» y el cursor tras la X ({tela[0]!r}, {cur})")
        out = m.comando("", teclas_finais=("ret",))
        verificar("cañXón" in out, "la orden con acentos se ejecuta como se ve")
        m.captura("H393-9-cursor-acentos")

        # ---- prompt con color, cursor sin fantasmas, línea larga
        m.comando("echo uno")
        m.comando("echo " + "palabra_larga " * 9 + "fin")
        time.sleep(0.6)
        tela, px = pantalla(m, t)
        fp = [i for i, l in enumerate(tela) if "saguios:/$ echo uno" in l]
        verificar(fp, f"se lee el prompt en la pantalla ({[l for l in tela if l][:4]})")
        f0 = fp[0]
        verde_ok = any(verde(*px(TERM_MX + x, TERM_MY + f0 * TERM_FILA + TERM_DY + y)) for x in range(0, 32) for y in range(16))
        verificar(verde_ok, "el usuario del prompt, en verde")
        # el «cursor fantasma»: tras «echo uno» no queda un bloque claro en la celda siguiente
        col = tela[f0].index("echo uno") + len("echo uno")
        celda = [px(TERM_MX + col * 8 + x, TERM_MY + f0 * TERM_FILA + TERM_DY + y) for x in range(8) for y in range(16)]
        verificar(all(c == FUNDO for c in celda), "sin cursor fantasma al final de la orden anterior")
        fl = [i for i, l in enumerate(tela) if l.startswith("palabra_larga palabra_larga") and "echo" not in l]
        verificar(fl and fl[-1] + 1 < len(tela) and tela[fl[-1] + 1].strip().endswith("fin"), "la línea larga sigue en la fila de abajo")
        m.captura("H393-3-prompt-y-linea-larga")

        # ---- historial y barra de desplazamiento
        m.comando("seq 1 300")
        time.sleep(0.5)
        tela, px = pantalla(m, t)
        xb = t["l"] - 3 - 6 + 2
        hay_barra = any(px(xb, y) != FUNDO for y in range(TERM_MY, TERM_MY + 28 * TERM_FILA))
        verificar(hay_barra, "con historial se ve la barra de desplazamiento")
        marca = len(m.serial())
        m.clique(t["cx"] + xb, t["cy"] + TERM_MY + 10)
        g = esperar(m, r"terminal: barra de desplazamiento, (\d+) lineas arriba", desde=marca)
        verificar(int(g) == 27, f"clic arriba en la barra: una página arriba ({g} líneas)")
        time.sleep(0.5)
        tela, px = pantalla(m, t)
        verificar(any("Historial: -27" in l for l in tela), "el aviso «Historial: -27» se ve en la esquina")
        m.captura("H393-4-barra-desplazamiento")
        m.tecla("shift-pgup", 0.5)
        time.sleep(0.4)
        tela, px = pantalla(m, t)
        verificar(any("Historial: -54" in l for l in tela), "Mayús+RePág sube otra página")
        m.tecla("shift-pgdn", 0.3)
        m.tecla("shift-pgdn", 0.3)
        time.sleep(0.4)
        tela, px = pantalla(m, t)
        verificar(not any("Historial:" in l for l in tela), "Mayús+AvPág vuelve abajo")
        marca = len(m.serial())
        m.mouse_para(t["cx"] + 300, t["cy"] + 200)
        roda(m, -2)
        esperar(m, r"terminal: historial (\d+) lineas arriba", desde=marca)
        verificar(True, "la rueda sube en el historial")
        m.tecla("spc", 0.3)
        m.tecla("backspace", 0.3)

        # ---- redimensionar: COLUMNS nuevo en el prompt siguiente
        x0, y0 = t["x"] + t["l"] + 2 * BORDA - 6, t["y"] + t["a"] + TITULO + BORDA - 6
        marca = len(m.serial())
        m.arrastar(x0, y0, x0 + 80, y0 + 36)
        g = esperar(m, r"terminal: janela (\d+)x(\d+), (\d+) colunas x (\d+) linhas", desde=marca)
        out = m.comando("echo cols=$COLUMNS lins=$LINES")
        verificar(f"cols={g[2]} lins={g[3]}" in out, f"tras redimensionar, COLUMNS y LINES nuevos ({g[2]}x{g[3]}: {out.strip()[-30:]!r})")
        t = janela(m.serial(), "Terminal") if "redimensionada" not in m.serial()[marca:] else dict(t, l=int(g[0]), a=int(g[1]))
        m.comando("clear")

        # ---- ~/.sagshrc
        out = m.comando("head -3 /etc/skel/.sagshrc")
        verificar("~/.sagshrc" in out, "existe el ~/.sagshrc de ejemplo en /etc/skel")
        m.comando("printf '%s\\n' \"alias hola393='echo hola-desde-rc'\" \"PS1='sagui:rc393\\$ '\" > $HOME/.sagshrc; echo rc=$?")
        out = m.comando("sagsh -i")
        out = m.comando("hola393")
        verificar("hola-desde-rc" in out, "un alias de ~/.sagshrc vale en un shell nuevo")
        verificar("sagui:rc393$" in m.serial(), "el PS1 de ~/.sagshrc vale")
        m.captura("H393-5-sagshrc")
        m.comando("exit")
        m.comando("printf '%s\\n' \"alias antes393='echo antes-del-error'\" 'if then fi fi' \"alias despues393='echo no'\" > $HOME/.sagshrc")
        marca = len(m.serial())
        m.comando("sagsh -i")
        s = m.serial()[marca:]
        verificar("error de sintaxis" in s and "aviso:" in s and ".sagshrc" in s, "un error en ~/.sagshrc avisa")
        out = m.comando("antes393; despues393; echo st=$?")
        verificar("antes-del-error" in out and "st=127" in out, "el shell se abre igual, con lo de antes del error")
        m.captura("H393-6-sagshrc-error")
        m.comando("exit")
        m.comando("printf '%s\\n' \"alias hola393='echo hola-desde-rc'\" > $HOME/.sagshrc")
        m.comando("sagsh --sin-rc -i")
        out = m.comando("hola393; echo st=$?")
        verificar("st=127" in out and "hola-desde-rc" not in out, "sagsh --sin-rc no lee ~/.sagshrc")
        m.comando("exit")
        m.comando("rm $HOME/.sagshrc")

        # ---- rendimiento
        m.comando("clear")
        t0 = time.time()
        m.comando("seq 1 20000", 300)
        dt = time.time() - t0
        print(f"H393: seq 1 20000 en el Terminal: {dt:.1f} s (antes de D385: 2,7 s con KVM)")
        verificar(dt < 60, f"seq 1 20000 termina ({dt:.1f} s)")
        m.captura("H393-7-tras-seq")
    return 0


def main():
    unidad()
    return parte_qemu()


if __name__ == "__main__":
    sys.exit(main())
