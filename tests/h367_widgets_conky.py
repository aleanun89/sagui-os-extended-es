#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H367 (D365, v8.5.3) - widgets de escritorio y Conky.

Criterio:
  - «widgets --arrancar» lanza los widgets activos de /home/.config/widgets.conf (reloj, calendario, notas,
    monitor del sistema y conky): cada uno abre una ventana de escritorio (SIS_CRIAR_WIDGET, registro «[GUI] widget N»);
  - no tienen botón en la barra de tareas (la barra queda igual que sin widgets);
  - quedan por debajo de una ventana normal (el Terminal) y por encima del fondo (se ven fuera de él), y sus
    esquinas redondeadas son transparentes (color clave);
  - conky muestra valores reales del sistema (el conky.conf de ejemplo, que crea la primera vez);
  - se recarga solo al cambiar conky.conf y una variable desconocida no rompe nada: se muestra tal cual y se anota;
  - menú del escritorio «Editar widgets»: un widget se arrastra con el ratón y guarda su posición en widgets.conf;
  - menú del widget «Quitar widget»: el widget termina y queda desactivado;
  - el widget de notas abre una ventana normal para editar y guarda el texto;
  - los widgets activos arrancan solos al iniciar la sesión otra vez.
Capturas: docs/capturas/H367-*.png.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, SAIDA, BUILD, opcoes_apps  # noqa: E402

DIREITO = 2
LINHA_A, SEP_A = 22, 9   # kernel/gui/menu.c
CONF = "/home/.config/widgets.conf"


def imagem_persistente(nome):
    destino = os.path.join(SAIDA, nome + ".img")
    os.makedirs(SAIDA, exist_ok=True)
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", os.path.join(BUILD, "ramdisk.sgfs"), "--saida", destino, "--vesa",
                    "--dados-mb", "32", *opcoes_apps()], check=True, stdout=subprocess.DEVNULL)
    return destino


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def widgets_criados(m, desde=0):
    """{nombre: id} de los «[GUI] widget N criado por P: "nombre"»."""
    return {n: int(i) for i, n in re.findall(r'\[GUI\] widget (\d+) criado por \d+: "([^"]+)"', m.serial()[desde:])}


def geometria(m, wid, desde=0):
    g = re.findall(rf"\[GUI\] widget {wid} colocado em \((-?\d+),(-?\d+)\) (\d+)x(\d+)", m.serial()[desde:])
    return tuple(int(v) for v in g[-1]) if g else None


def abrir_menu(m, x, y):
    marca = len(m.serial())
    m.clique(x, y, botao=DIREITO)
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=marca)
    ls = re.findall(r"\[GUI\] menu linhas: ([^\n]*)", m.serial()[marca:])[-1].split("|")
    return tuple(int(v) for v in g), ls


def clique_linha(m, menu, ls, texto):
    i = [k for k, l in enumerate(ls) if texto in l][0]
    seps = ls[:i].count("-")
    mx, my, ml = menu[0], menu[1], menu[2]
    m.clique(mx + ml // 2, my + 2 + (i - seps) * LINHA_A + seps * SEP_A + LINHA_A // 2)


def escuro(c):
    return c[0] < 0x40 and c[1] < 0x50 and c[2] < 0x48


def linhas_conky(m, desde):
    """Las líneas «[CONKY] linea N: texto» de la primera renderización posterior a «desde»."""
    s = m.serial()[desde:]
    ini = s.find("[CONKY] linea 0:")
    if ini < 0:
        return None
    ls = {}
    for n, t in re.findall(r"\[CONKY\] linea (\d+): ([^\r\n]*)", s[ini:]):
        if int(n) in ls:
            break
        ls[int(n)] = t
    return ls


def main():
    img = imagem_persistente("h367")
    # ---------------------------------------------------------------- sesión 1
    with Maquina("h367-a", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("[SAGUI] widgets:", 120)   # el arranque de la sesión llama a «widgets --arrancar» (sin widgets activos: no hace nada)
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        mt = re.findall(r'"Terminal" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
        tx, ty, tl, ta = int(mt[2]) - 2, int(mt[3]) - 24, int(mt[0]) + 4, int(mt[1]) + 28
        base = m.captura("H367-0-sin-widgets")

        # ---- activar los cinco y arrancarlos
        m.comando("mkdir /home/.config")
        m.comando(f"echo relogio=1 > {CONF}")
        for k in ("calendario", "notas", "sistema", "conky"):
            m.comando(f"echo {k}=1 >> {CONF}")
        marca = len(m.serial())
        m.comando("widgets --arrancar", 30)
        fim = time.time() + 40
        while time.time() < fim and (len(widgets_criados(m, marca)) < 5 or "[CONKY] linea 0:" not in m.serial()[marca:]):
            time.sleep(0.3)
        w = widgets_criados(m, marca)
        verificar(set(w) == {"relogio", "calendario", "notas", "sistema", "conky"},
                  f"los cinco widgets abren su ventana de escritorio: {sorted(w)}")
        time.sleep(1.5)
        png = m.captura("H367-1-widgets")
        g = {n: geometria(m, i, marca) for n, i in w.items()}
        verificar(all(g.values()), f"cada widget se coloca en su sitio: {g}")

        # ---- sin botón en la barra: la zona de las tareas (entre el botón Inicio y el reloj) no cambió
        difer = [x for x in range(100, 760, 12) if cor_do_pixel(png, x, 12) != cor_do_pixel(base, x, 12)]
        verificar(not difer, f"la barra de tareas queda igual con cinco widgets abiertos (difiere en x={difer[:5]})")

        # ---- debajo del Terminal y encima del fondo (conky está en (20,40) 300x288; el Terminal lo cubre a partir de x=184)
        cx, cy, cl, ca = g["conky"]
        verificar(cx < tx < cx + cl and cy < ty + ta, f"conky se solapa con el Terminal (conky {g['conky']}, terminal {(tx, ty, tl, ta)})")
        px, py = tx + 70, ty + 120   # dentro del Terminal y dentro de conky
        verificar(cor_do_pixel(png, px, py) == cor_do_pixel(base, px, py),
                  "dentro del Terminal, conky no se ve: la ventana normal está por encima")
        qx, qy = cx + 5, cy + 160   # el borde del panel de conky, fuera del Terminal
        verificar(escuro(cor_do_pixel(png, qx, qy)) and not escuro(cor_do_pixel(base, qx, qy)),
                  f"fuera del Terminal, el panel de conky se ve sobre el fondo {cor_do_pixel(png, qx, qy)}")
        # esquinas redondeadas: el píxel de la esquina del reloj es el del fondo (color clave)
        rx, ry, rl, ra = g["relogio"]
        verificar(cor_do_pixel(png, rx, ry) == cor_do_pixel(base, rx, ry), "la esquina del reloj deja ver el fondo (esquinas redondeadas)")
        verificar(cor_do_pixel(png, rx + 20, ry + 4) != cor_do_pixel(base, rx + 20, ry + 4), "y el panel del reloj sí cubre el fondo")

        # ---- conky: valores reales del conky.conf de ejemplo
        ls = linhas_conky(m, marca)
        verificar(ls is not None and len(ls) >= 16, f"conky escribió sus líneas en el registro: {ls and len(ls)}")
        verificar(re.match(r"\S+SaguiOS v", ls[0]) is not None, f"nombre del equipo y versión reales: {ls[0]!r}")
        verificar(re.search(r"\d+(h|m|s)", ls[2]) is not None, f"tiempo en marcha real: {ls[2]!r}")
        # el total depende de la memoria del QEMU (SAGUI_MEM: 512M en rodar_paralelo, 2G suelta)
        mm = re.search(r"([\d.]+)(KiB|MiB|GiB) / ([\d.]+)(MiB|GiB)(\d+)%", ls[4])
        total_mib = mm and float(mm.group(3)) * (1024 if mm.group(4) == "GiB" else 1)
        verificar(mm is not None and 256 <= total_mib <= 2300, f"memoria usada y total reales: {ls[4]!r}")
        verificar("n/d" not in ls[6] and re.search(r"[\d.]+\w+ / [\d.]+\w+", ls[6]) is not None, f"uso del disco real: {ls[6]!r}")
        verificar("[barra" in ls[5] and "[grafica" in ls[3], "barras y gráficas")
        verificar(re.match(r"\d\d:\d\d:\d\d", ls[15].strip()) is not None, f"la hora: {ls[15]!r}")
        verificar(not any("${" in t for t in ls.values()), "ninguna variable del ejemplo quedó sin interpretar")

        # ---- recarga con variables desconocidas: se muestran tal cual, no rompen nada
        marca = len(m.serial())
        t = "/home/.config/c.tmp"
        m.comando(f"echo 'minimum_size 330 0' > {t}")
        m.comando(f"echo TEXT >> {t}")
        m.comando(f"echo 'UP ${{uptime}} CPU ${{cpu}} MEM ${{mem}}/${{memmax}} ${{memperc}}' >> {t}")
        m.comando(f"echo 'HOST ${{nodename}} K ${{kernel}} FS ${{fs_used /}}/${{fs_size /}}' >> {t}")
        m.comando(f"echo 'BAD ${{foobar 1}} ${{nosoportada}} X$$Y' >> {t}")
        m.comando(f"echo '${{cpubar 6}} ${{cpugraph 20,100}}' >> {t}")
        m.comando(f"mv {t} /home/.config/conky.conf")
        esperar(m, r"\[CONKY\] /home/\.config/conky\.conf cambi", 20, marca)
        marca2 = marca
        fim = time.time() + 30
        while time.time() < fim and "[CONKY] linea 3:" not in m.serial()[marca2:]:
            time.sleep(0.3)
        ls = linhas_conky(m, marca2)
        verificar(ls is not None and len(ls) == 4, f"conky se recargó con el fichero nuevo: {ls}")
        verificar(re.match(r"UP (\d+[dhm] )*\d+s CPU \d+ MEM [\d.]+(KiB|MiB|GiB)/[\d.]+(MiB|GiB) \d+$", ls[0]) is not None, f"valores reales: {ls[0]!r}")
        verificar(re.match(r"HOST \S+ K \S+ FS [\d.]+\w+/[\d.]+\w+$", ls[1]) is not None and "n/d" not in ls[1], f"host, núcleo y disco: {ls[1]!r}")
        verificar(ls[2] == "BAD ${foobar 1} ${nosoportada} X$Y", f"las variables desconocidas se muestran tal cual: {ls[2]!r}")
        verificar(re.match(r"\[barra \d+\] \[grafica \d+\]$", ls[3]) is not None, f"barra y gráfica: {ls[3]!r}")
        verificar("variable no soportada: foobar" in m.serial()[marca2:], "la variable desconocida se anota en el registro")
        time.sleep(1.5)
        m.captura("H367-2-conky-propio")
        w2 = widgets_criados(m, marca2)
        verificar("conky" in w2, "el conky nuevo tiene su ventana (el anterior terminó)")

        # ---- editar widgets desde el menú del escritorio y arrastrar el reloj
        menu, ls_m = abrir_menu(m, 500, 650)
        verificar(any("Añadir widgets" in l for l in ls_m) and any("Editar widgets" in l for l in ls_m),
                  f"el menú del escritorio ofrece añadir y editar widgets: {ls_m}")
        marca = len(m.serial())
        clique_linha(m, menu, ls_m, "Editar widgets")
        esperar(m, r"\[GUI\] editar widgets: si", 10, marca)
        wid = w["relogio"]
        marca = len(m.serial())
        # la parte del reloj que no tapa el Terminal: a la derecha de su borde
        x0 = max(rx, tx + tl + 4) + 20
        # a la columna de la izquierda, bajo conky (el agarre es a x0 - rx del borde del widget)
        m.arrastar(x0, ry + 60, 30 + (x0 - rx), 400 + 60)
        nuevo = esperar(m, rf"\[GUI\] janela {wid} movida para \((\d+),(\d+)\)", 15, marca)
        nx, ny = int(nuevo[0]), int(nuevo[1])
        verificar((nx, ny) != (rx, ry) and ny > ry + 200, f"el reloj se arrastra con el ratón: {(rx, ry)} -> {(nx, ny)}")
        time.sleep(1)
        m.captura("H367-3-editar-widgets")
        r = m.comando(f"cat {CONF}")
        verificar(f"relogio.x={nx}" in r and f"relogio.y={ny}" in r, f"la posición se guarda en widgets.conf: {r.strip()[-120:]}")
        # salir del modo de edición (menú del escritorio otra vez)
        menu, ls_m = abrir_menu(m, 500, 650)
        marca = len(m.serial())
        clique_linha(m, menu, ls_m, "Editar widgets")
        esperar(m, r"\[GUI\] editar widgets: no", 10, marca)

        # ---- quitar el reloj desde su menú
        menu, ls_m = abrir_menu(m, nx + 60, ny + 120)
        verificar(any("Quitar widget" in l for l in ls_m), f"el menú del widget: {ls_m}")
        marca = len(m.serial())
        clique_linha(m, menu, ls_m, "Quitar widget")
        esperar(m, rf"\[GUI\] widget {wid} \(relogio\): quitar", 10, marca)
        time.sleep(2)
        r = m.comando(f"cat {CONF}")
        verificar("relogio=0" in r and "calendario=1" in r, f"quitar desactiva el widget y no toca los demás: {r.strip()[-140:]}")
        r = m.comando("widgets --lista")
        verificar(re.search(r"relogio\s+activo|relogio\s+inactivo", r) and "inactivo" in re.search(r"relogio[^\n]*", r).group(0),
                  f"el reloj ya no corre: {re.search(r'relogio[^\n]*', r).group(0)}")

        # ---- notas: un clic abre una ventana normal para editar; al cerrarla se guarda
        gn = geometria(m, w["notas"])
        marca = len(m.serial())
        m.clique(gn[0] + 100, gn[1] + 10)   # la parte de arriba de la nota, que el Terminal no tapa
        esperar(m, r'\[GUI\] janela \d+ criada por \d+: "Nota adhesiva"', 15, marca)
        time.sleep(1)
        m.digitar("hola widget", 0.05)
        time.sleep(0.5)
        m.captura("H367-4-notas")
        m.tecla("alt-f4")
        time.sleep(2)
        r = m.comando("cat /home/.config/widget-notas.txt")
        verificar("hola widget" in r, f"la nota se guardó en /home/.config/widget-notas.txt: {r.strip()[-80:]}")
        m.tecla("ret")
        time.sleep(0.5)
        m.comando("sync")
        verificar(True, "datos sincronizados")

    # ---------------------------------------------------------------- sesión 2: arranque automático
    with Maquina("h367-b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        fim = time.time() + 40
        while time.time() < fim and len(widgets_criados(m)) < 4:
            time.sleep(0.3)
        w = widgets_criados(m)
        verificar(set(w) == {"calendario", "notas", "sistema", "conky"},
                  f"al iniciar la sesión arrancan solos los widgets activos (y no el quitado): {sorted(w)}")
        time.sleep(1.5)
        m.captura("H367-5-arranque-automatico")
        r = m.comando(f"cat {CONF}")
        verificar(re.search(r"relogio\.x=\d+", r) is not None, "la posición del reloj sigue guardada")
    print("H367 OK")


if __name__ == "__main__":
    main()
