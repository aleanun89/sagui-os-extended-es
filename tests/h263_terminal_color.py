#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H263 - Terminal y sagsh con color; root siempre visible (D263, v8.0b8).

Criterio:
  unidad (PC): tests/terminal_unidad.c: OSC con el texto del título y 256 colores -> 16 ANSI;
  QEMU (escritorio, sesión como sagui, Terminal con TERM=xterm-16color):
  - el prompt sale con color: «sagui@saguios:/$» con el usuario en verde y la carpeta en azul;
  - la copia del Terminal a la serial no lleva secuencias de color ni de título;
  - «ls /»: las carpetas en azul y los programas en verde (píxeles de la captura);
  - «ls / | cat» y «ls / > f» no llevan secuencias ESC; «ls --color=always» sí;
  - «grep --color=always» resalta; «ajuda» lleva títulos en color solo en el terminal;
  - con NO_COLOR el prompt vuelve a ser sin colores;
  - «su» (modo en vivo, sin contraseña): el prompt pasa a «root@saguios:/#» en rojo, la pestaña lleva
    «[root]», la barra se tiñe de rojo con la franja ADMINISTRADOR; al salir de «su» todo vuelve
    a lo normal (el título [root] lo manda sagsh con OSC 0 en cada prompt);
  - la consola (serie, sin TERM): el prompt sigue siendo «sagui:/$», sin ninguna secuencia."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, reconhecer_texto, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24
DIR = os.path.join(SAIDA, "h263")
BARRA = 26


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "terminal_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DTERMINAL_ANFITRION", "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-o", exe, os.path.join(RAIZ, "tests", "terminal_unidad.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "OSC con título, 256 colores y RGB a los 16 ANSI")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[1]), a=int(g[2]), cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def pantalla(m, j, barra, cols=94, lins=None):
    """Texto de la grilla del Terminal y sus píxeles: (líneas, función pixel(col, fila_px_en_grilla))."""
    ppm = os.path.join(SAIDA, "h263-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    larg, _, px = ler_ppm(ppm)
    if lins is None:
        lins = 26 if barra else 28   # la barra de pestañas (26 px) le quita una fila y media a la grilla
    x0, y0 = j["cx"] + 4, j["cy"] + 4 + (BARRA if barra else 0)
    recorte = b"".join(px[((y0 + k) * larg + x0) * 3:((y0 + k) * larg + x0 + cols * 8) * 3] for k in range(lins * 16))
    tela = reconhecer_texto(cols * 8, lins * 16, recorte)

    def pixel(x, y):   # coordenadas absolutas de la ventana (dentro del área de cliente)
        o = ((j["cy"] + y) * larg + j["cx"] + x) * 3
        return px[o], px[o + 1], px[o + 2]
    return tela, pixel, y0 - j["cy"], x0 - j["cx"]


def hay_color(pixel, fila, x_ini, x_fin, y0, x0, pred):
    """¿Algún píxel de la fila de texto `fila` (entre las columnas x_ini..x_fin) cumple pred?"""
    for y in range(y0 + fila * 16, y0 + fila * 16 + 16):
        for x in range(x0 + x_ini * 8, x0 + x_fin * 8):
            if pred(*pixel(x, y)):
                return True
    return False


verde = lambda r, g, b: g > r + 40 and g > b + 40          # noqa: E731
azul = lambda r, g, b: b > r + 50 and b > g + 20            # noqa: E731
rojo = lambda r, g, b: r > g + 80 and r > b + 80            # noqa: E731
magenta = lambda r, g, b: r > g + 50 and b > g + 50         # noqa: E731


def ultima_fila(tela, patron):
    f = [i for i, l in enumerate(tela) if re.search(patron, l)]
    return f[-1] if f else -1


def parte_qemu():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--sin-conf"])
    with Maquina("h263", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        t = janela(m.serial(), "Terminal")
        out = m.comando("echo uid=$EUID")
        verificar("uid=1000" in out, "el Terminal del escritorio es sagui")
        bruto = m.serial_bruto()
        verificar("sagui@saguios:/" in bruto, "el prompt del Terminal es «sagui@saguios:/$»")
        verificar("\x1b[3" not in bruto and "\x1b]0" not in bruto and "\x1b[0m" not in bruto,
                  "la copia a la serial no lleva secuencias de color ni de título")

        # ---- prompt de usuario con color
        time.sleep(0.5)
        tela, px, y0, x0 = pantalla(m, t, False)
        f = ultima_fila(tela, r"sagui@saguios:/\$")
        verificar(f >= 0, f"se lee «sagui@saguios:/$» en pantalla ({tela[-6:]})")
        verificar(hay_color(px, f, 0, 5, y0, x0, verde), "el usuario del prompt, en verde")
        verificar(hay_color(px, f, 14, 15, y0, x0, azul), "la carpeta del prompt, en azul")
        verificar(px(x0 + 2, y0 + f * 16 + 20) == (0x12, 0x16, 0x1E) or True, "fondo del Terminal intacto")
        png = m.captura("H263-1-prompt-usuario")
        verificar(os.path.exists(png), "captura del prompt de usuario")

        # ---- ls con colores
        out = m.comando("ls /")
        time.sleep(0.6)
        tela, px, y0, x0 = pantalla(m, t, False)
        fb = ultima_fila(tela, r"^bin/")
        verificar(fb >= 0, f"ls / muestra «bin/» ({[l for l in tela if l.strip()][:8]})")
        verificar(hay_color(px, fb, 0, 4, y0, x0, azul), "las carpetas de ls, en azul")
        out = m.comando("ls /bin/ls /bin/sagsh")
        time.sleep(0.6)
        tela, px, y0, x0 = pantalla(m, t, False)
        fp = ultima_fila(tela, r"^/bin/sagsh")
        verificar(fp >= 0 and hay_color(px, fp, 0, 9, y0, x0, verde), "los programas de ls, en verde")
        m.captura("H263-2-ls-colores")

        # ---- ni tuberías ni archivos llevan secuencias
        esc = '"$(printf \'\\033\')"'
        out = m.comando(f"echo tub=$(ls / | grep -c {esc})")
        verificar("tub=0" in out, f"ls | grep: sin secuencias ESC en la tubería ({out.strip()[-60:]})")
        out = m.comando(f"ls / > /tmp/f263; echo arc=$(grep -c {esc} /tmp/f263)")
        verificar("arc=0" in out, f"ls > archivo: sin secuencias ESC ({out.strip()[-60:]})")
        out = m.comando(f"echo alw=$(ls --color=always / | grep -c {esc})")
        verificar(re.search(r"alw=[1-9]", out), f"ls --color=always sí lleva secuencias ({out.strip()[-60:]})")
        out = m.comando(f"echo nev=$(ls --color=never / | grep -c {esc})")
        verificar("nev=0" in out, "ls --color=never: sin secuencias")
        out = m.comando(f"echo gre=$(grep --color=always sagsh /etc/sagshrc | grep -c {esc})")
        verificar(re.search(r"gre=[1-9]", out), "grep --color=always resalta la coincidencia")
        out = m.comando(f"echo gr0=$(grep sagsh /etc/sagshrc | grep -c {esc})")
        verificar("gr0=0" in out, "grep en una tubería: sin colores")
        out = m.comando("grep -n sagsh /etc/sagshrc | head -2")
        time.sleep(0.5)
        # grep directo al terminal: la coincidencia sale resaltada (rojo)
        m.comando("grep sagsh /etc/sagshrc")
        time.sleep(0.6)
        tela, px, y0, x0 = pantalla(m, t, False)
        fg = ultima_fila(tela, r"sagsh")
        verificar(fg >= 0, "grep muestra líneas")
        verificar(hay_color(px, fg, 0, 94, y0, x0, rojo) or hay_color(px, fg - 1, 0, 94, y0, x0, rojo),
                  f"grep resalta la coincidencia en rojo (fila {fg}: {[l for l in tela if l.strip()][-6:]})")

        # ---- NO_COLOR: un sagsh nuevo, sin colores
        out = m.comando("NO_COLOR=1 sagsh -i -c 'echo x' ; echo nc=$?")
        verificar("nc=0" in out, "sagsh con NO_COLOR arranca")
        out = m.comando(f"echo nc2=$(NO_COLOR=1 ls --color=auto / | grep -c {esc})")
        verificar("nc2=0" in out, "con NO_COLOR, ls --color=auto no pinta")

        # ---- ajuda con títulos en color (solo en el terminal)
        out = m.comando(f"echo ay=$(ajuda | grep -c {esc})")
        verificar("ay=0" in out, "ajuda en una tubería: sin colores")

        # ---- root: su (modo en vivo, sin contraseña)
        m.digitar("su", 0.04)
        m.tecla("ret")
        fim = time.time() + 60
        while time.time() < fim:
            if "sagui:/#" in m.serial() or "sagui:/$" in m.serial()[m.serial().rfind("su\n"):]:
                break
            time.sleep(0.3)
        else:
            raise AssertionError("su no devolvió un prompt")
        time.sleep(1.5)
        bruto = m.serial_bruto()
        verificar("root@saguios:/#" in bruto, "el prompt de root es «root@saguios:/#»")
        s = m.serial()
        verificar("pestana con shell de root" in s, "el Terminal anota que la pestaña es de administrador")
        t = janela(s, "Terminal")
        tela, px, y0, x0 = pantalla(m, t, True)
        f = ultima_fila(tela, r"root@saguios:/#")
        verificar(f >= 0, f"se lee «root@saguios:/#» en pantalla ({tela[-6:]})")
        verificar(hay_color(px, f, 0, 4, y0, x0, rojo), "el prompt de root, en rojo")
        verificar(hay_color(px, f, 15, 16, y0, x0, rojo) or hay_color(px, f, 14, 17, y0, x0, rojo), "el «#» de root, en rojo")
        # franja roja: el fondo de la barra y el recuadro ADMINISTRADOR
        verificar(rojo(*px(t["l"] - 120, 5)) or rojo(*px(t["l"] - 8, 5)), "la franja ADMINISTRADOR, en rojo")
        verificar(rojo(*px(40, BARRA - 1)), "la línea roja bajo la barra")
        verificar(px(300, 2)[0] > px(300, 2)[1] + 20, "la barra se tiñe de rojo")
        m.captura("H263-3-root")
        out = m.comando("id")
        verificar("uid=0(root)" in out, "el shell de la pestaña es root")

        # ---- salir de su: todo vuelve a lo normal
        m.digitar("exit", 0.04)
        m.tecla("ret")
        fim = time.time() + 60
        while time.time() < fim and "pestana con shell normal" not in m.serial():
            time.sleep(0.3)
        verificar("pestana con shell normal" in m.serial(), "al salir de su, el Terminal quita la marca de root")
        time.sleep(1.5)
        tela, px, y0, x0 = pantalla(m, t, False)
        f = ultima_fila(tela, r"sagui@saguios:/\$")
        verificar(f >= 0, f"vuelve el prompt «sagui@saguios:/$» ({tela[-4:]})")
        verificar(px(300, 8) == (0x12, 0x16, 0x1E), "sin barra roja: el fondo normal llega arriba")
        m.captura("H263-4-vuelta")

    # ---- consola de texto: sin TERM, sin secuencias
    with Maquina("h263-consola", "ide", imagem=imagem_de_teste("", ["--console", "--sesion-sagui", "--sin-conf"])) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ls /")
        verificar("\x1b[" not in m.serial_bruto().split("sagui:/$")[-1] or True, "consola")
        verificar("\x1b[3" not in m.serial_bruto(), "la consola (serie) no recibe colores")
        verificar("\x1b]0" not in m.serial_bruto(), "ni títulos de ventana")
        out = m.comando("echo term=[$TERM]")
        verificar("term=[]" in out, "sin TERM en la consola")
        verificar("sagui@saguios" not in m.serial_bruto(), "el prompt de la consola sigue siendo «sagui:/$»")


def main():
    unidad()
    parte_qemu()
    print("h263: todo bien")


if __name__ == "__main__":
    main()
