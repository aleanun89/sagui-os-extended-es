#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H324 (D324) - las aplicaciones no fundamentales viven en /apps, no en el ramdisk.

El ramdisk se carga entero en la RAM al arrancar: con 32 MB y escritorio no quedaba memoria.
Criterios:
  A. (sin QEMU) el ramdisk pesa menos de MAX_RAMDISK; las aplicaciones movidas no están en su /bin y sí
     en build/apps/raiz/bin; ninguna fuente del kernel ni del userland llama a /bin/<app movida>
     (rutas rotas), y el perfil RAMDISK=completo existe en el Makefile.
  B. Con 32 MB y escritorio: arranca, quedan más de MIN_LIBRE_KB libres, y Pluma (Inicio > Accesorios),
     Cipó (Inicio > Internet) y el icono de Paint se abren desde /apps/bin. Se anota el tiempo hasta
     el escritorio.
  C. Sin la partición SAGUI-APPS: el sistema arranca igual, el menú Inicio y el escritorio ocultan lo
     que vive en /apps (Pluma, Paint, Banana...) y «which pluma» no lo encuentra.
  D. La ISO (el kernel no lee el CD, D077) lleva /apps en su ramdisk (perfil completo): llega al
     escritorio y «which pluma» da /apps/bin/pluma.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, BUILD  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui, y_linha  # noqa: E402
import h268_iconos_escritorio as h268  # noqa: E402

MAX_RAMDISK = 12 * 1024 * 1024    # el de la 8.4.2 era 19,3 MB
MIN_LIBRE_KB = 4 * 1024           # con 32 MB y escritorio
MOVIDAS = ("cipo", "pluma", "macaco", "mico", "correo", "planomini", "gebramini", "visorpdf", "chatia", "ia",
           "reproductor", "musica", "camara", "grabadora", "paint", "pascal", "cc", "spider", "sagui3d",
           "buscaminas", "banana", "chip8", "jclic", "python")
APPS_RAIZ = os.path.join(BUILD, "apps", "raiz")


def libre_kb(out):
    m = re.search(r"Libre:\s+.*\((\d+) KB\)", out)
    return int(m.group(1)) if m else -1


def parte_a():
    rd = os.path.getsize(os.path.join(BUILD, "ramdisk.sgfs"))
    verificar(rd < MAX_RAMDISK, f"A: el ramdisk pesa {rd / 1048576:.1f} MB (límite {MAX_RAMDISK // 1048576} MB)")
    bin_rd = set(os.listdir(os.path.join(BUILD, "rootfs", "bin")))
    bin_ap = set(os.listdir(os.path.join(APPS_RAIZ, "bin")))
    for n in MOVIDAS:
        if n == "python" and "python" not in bin_ap:
            continue   # MICROPYTHON=0
        verificar(n not in bin_rd and n in bin_ap, f"A: {n} está en /apps/bin y no en el /bin del ramdisk")
    verificar(os.path.isdir(os.path.join(APPS_RAIZ, "videos")) and not os.path.exists(os.path.join(BUILD, "rootfs", "videos")),
              "A: el vídeo de demostración está en /apps/videos")
    # rutas rotas: «/bin/<app movida>» sin «/apps» delante, en las fuentes que lanzan programas
    rotas = []
    patron = re.compile(r"(?<!apps)/bin/(" + "|".join(re.escape(n) for n in MOVIDAS) + r")(?![\w-])")
    for pasta in ("kernel", os.path.join("userland", "programas"), os.path.join("userland", "libsagui"),
                  os.path.join("userland", "micropython"), "rootfs", os.path.join("userland", "apps")):
        for d, _, fs in os.walk(os.path.join(RAIZ, pasta)):
            for f in fs:
                if not f.endswith((".c", ".h", ".py", ".conf")) and f not in ("lmk", "chip8-roms", "chip8-roms-gui"):
                    continue
                if "terceros" in d or "micropython_embed" in d:
                    continue
                txt = open(os.path.join(d, f), encoding="latin-1").read()
                for ln, linea in enumerate(txt.splitlines(), 1):
                    if patron.search(linea) and "#!" not in linea[:2] + "":
                        # los comentarios que solo nombran el programa no lo lanzan: se exige una cadena
                        if re.search(r'"[^"\n]*(?<!apps)/bin/(' + "|".join(MOVIDAS) + r')(?![\w-])', linea):
                            rotas.append(f"{os.path.relpath(os.path.join(d, f), RAIZ)}:{ln}: {linea.strip()[:90]}")
    verificar(not rotas, "A: ninguna ruta «/bin/<app movida>» entre comillas queda en las fuentes" + ("" if not rotas else ": " + "; ".join(rotas[:5])))
    mk = open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read()
    verificar("RAMDISK ?= base" in mk and "RAMDISK),completo" in mk, "A: el Makefile tiene RAMDISK=base|completo")


def abrir_terminal_listo(m):
    esperar(m, r"sagui:/\$", 150)
    time.sleep(2)


def parte_b():
    imagem = os.path.join(BUILD, "saguios.img")
    t0 = time.time()
    with Maquina("h324-b", "ide", imagem=imagem, memoria="32M") as m:
        m.esperar_serial('"Terminal"', 180)
        t_esc = time.time() - t0
        m.esperar_serial("sagui:/$", 60)
        print(f"H324: 32 MB, con /apps: escritorio en {t_esc:.1f} s (QEMU, {'KVM' if os.environ.get('SAGUI_KVM') == '1' else 'TCG'})")
        time.sleep(2)
        out = m.comando("mem", 30)
        kb = libre_kb(out)
        print(f"H324: 32 MB y escritorio: libres {kb // 1024} MB ({kb} KB)")
        verificar(kb >= MIN_LIBRE_KB, f"B: con 32 MB y escritorio quedan {kb // 1024} MB libres (mínimo {MIN_LIBRE_KB // 1024} MB)")
        out = m.comando("which pluma cipo python", 30)
        verificar("/apps/bin/pluma" in out and "/apps/bin/cipo" in out and "/apps/bin/python" in out,
                  "B: which encuentra las aplicaciones en /apps/bin (PATH)")
        # Inicio > Accesorios > Pluma
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Accesorios", "Pluma (procesador de textos)")
        m.mouse_mover(sub["x"] + 30, y_linha(sub, "Pluma (procesador de textos)"))
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/pluma \(pid \d+\)", 20, desde=marca)
        verificar(not re.search(r"inicio -> /apps/bin/pluma \(pid -", m.serial()[marca:]), "B: Inicio > Accesorios abre Pluma desde /apps/bin")
        esperar(m, r'janela \d+ criada por \d+: "[^"]*Pluma', 60, desde=marca)
        # el icono de Paint del escritorio
        cs = h268.casillas(m)
        verificar(cs and "paint" in cs and "banana" in cs, f"B: los iconos de /apps salen en el escritorio ({sorted(cs or {})})")
        if cs and "paint" in cs:
            marca = len(m.serial())
            m.clique(*h268.centro(*cs["paint"]), duplo=True)
            esperar(m, r"\[GUI\] icone Paint -> /apps/bin/paint \(pid \d+\)", 20, desde=marca)
            verificar(not re.search(r"icone Paint -> /apps/bin/paint \(pid -", m.serial()[marca:]), "B: el icono de Paint abre /apps/bin/paint")
        m.captura("H324-1-escritorio-32M")


def parte_c():
    imagem = imagem_de_teste("", ["--vesa"], apps=False)
    t0 = time.time()
    with Maquina("h324-c", "ide", imagem=imagem, memoria="32M") as m:
        m.esperar_serial('"Terminal"', 180)
        t_esc = time.time() - t0
        m.esperar_serial("sagui:/$", 60)
        print(f"H324: 32 MB, sin /apps: escritorio en {t_esc:.1f} s")
        time.sleep(2)
        out = m.comando("mem", 30)
        kb = libre_kb(out)
        print(f"H324: 32 MB, sin /apps: libres {kb // 1024} MB ({kb} KB)")
        verificar(kb >= MIN_LIBRE_KB, f"C: arranca igual sin /apps ({kb // 1024} MB libres)")
        out = m.comando("which pluma paint", 30)
        verificar("/apps/bin" not in out, "C: which no encuentra las aplicaciones de /apps")
        cs = h268.casillas(m)
        verificar(cs and "terminal" in cs and "paint" not in cs and "banana" not in cs and "sagui3d" not in cs and "chip8" not in cs,
                  f"C: el escritorio oculta los iconos de /apps ({sorted(cs or {})})")
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Accesorios", "Archivos")
        verificar("Pluma (procesador de textos)" not in sub["linhas"] and "Archivos" in sub["linhas"],
                  "C: Inicio > Accesorios oculta Pluma sin /apps (%s)" % ", ".join(sub["linhas"]))
        m.captura("H324-2-sin-apps")


def parte_d():
    subprocess.run(["make", "-s", "-C", RAIZ, os.path.relpath(os.path.join(BUILD, "ramdisk-cd.sgfs"), RAIZ)],
                   check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h324-cd.img")
    iso = os.path.join(SAIDA, "h324.iso")
    b = BUILD
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk-cd.sgfs"), "--saida", img, "--vesa", "--cd"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img, "--saida", iso],
                   check=True, stdout=subprocess.DEVNULL)
    os.remove(img)
    print("H324: ramdisk de la ISO: %.1f MB" % (os.path.getsize(os.path.join(b, "ramdisk-cd.sgfs")) / 1048576))
    try:
        with Maquina("h324-d", "cd", imagem=iso, memoria="256M") as m:
            m.esperar_serial('"Terminal"', 240)
            m.esperar_serial("sagui:/$", 60)
            verificar(True, "D: la ISO llega al escritorio")
            time.sleep(2)
            out = m.comando("which pluma paint", 30)
            verificar("/apps/bin/pluma" in out and "/apps/bin/paint" in out, "D: la ISO lleva las aplicaciones en /apps (perfil completo)")
    finally:
        if os.path.exists(iso):
            os.remove(iso)


def main():
    if not os.path.isdir(os.path.join(APPS_RAIZ, "bin")):
        raise AssertionError("falta %s (lo hace el make con APPS=1)" % APPS_RAIZ)
    partes = sys.argv[1:] or ["a", "b", "c", "d"]
    if "a" in partes:
        parte_a()
    if "b" in partes:
        parte_b()
    if "c" in partes:
        parte_c()
    if "d" in partes:
        parte_d()
    return 0


if __name__ == "__main__":
    sys.exit(main())
