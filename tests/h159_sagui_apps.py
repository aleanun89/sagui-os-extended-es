#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H159 (D194) - la partición SAGUI-APPS: las aplicaciones fuera del ramdisk, en /apps.

Parte A (imagen con la partición, como build/saguios.img, sobre una copia y snapshot=False):
  - el /etc/fstab monta SAGUI-APPS en /apps por su etiqueta, en lectura y escritura (ext2),
    y no la toma por el /home;
  - /apps/bin tiene NetSurf y sqlite3 (y /bin ya no), con /apps/bin en el PATH del shell:
    "which netsurf" da /apps/bin/netsurf y "sqlite3" funciona por su nombre;
  - "netsurf file:///usb/prueba.html" desde el shell dibuja la página de prueba de h153
    (las cajas del CSS, el PNG y el JPEG), con sus recursos leídos de /apps/netsurf;
  - Inicio > Internet tiene «NetSurf (navegador)» y abre /apps/bin/netsurf;
  - un fichero escrito en /apps sigue ahí después de reiniciar.
Parte B (imagen sin la partición): el sistema arranca igual, Inicio > Internet no tiene NetSurf
  (sí Cipó) y "netsurf" no se encuentra.
Parte C (CD, que no lee particiones): la ISO lleva /apps dentro de su ramdisk (el mismo árbol
  que con APPS=0) y "which netsurf"/"sqlite3" funcionan.
Capturas en docs/capturas/H159-*.png.
"""
import os
import re
import shutil
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, criar_pendrive, SAIDA, BUILD  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui, janela, y_linha, focar  # noqa: E402
import h136_opentyrian as h136  # noqa: E402
import h153_netsurf as h153  # noqa: E402

APPS = os.path.join(BUILD, "apps", "raiz")   # lo que el make pone en la partición (D194)
JOGOS = os.path.join(BUILD, "jogos", "raiz")   # la raíz de SAGUI-JOGOS (D228)


def terminal(m):
    esperar(m, r"sagui:/\$", 150)
    time.sleep(4)
    h136.focar_terminal(m)


def menu_internet(m, capturar=None):
    """Abre Inicio > Internet y devuelve sus líneas (el menú queda abierto)."""
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Internet", "Cipó (navegador)")
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    return p, sub


def extras_pen(pen):
    return ["-device", "usb-ehci,id=ehci",
            "-drive", "if=none,id=pen,format=raw,file=" + pen,
            "-device", "usb-storage,bus=ehci.0,drive=pen,removable=on,id=dev-pen"]


def parte_a():
    imagem = imagem_de_teste("", ["--vesa", "--jogos", JOGOS, "--apps", APPS])
    copia = os.path.join(SAIDA, "h159-apps.img")
    shutil.copyfile(imagem, copia)
    datos = h153.DATOS
    pen = criar_pendrive("h159-netsurf", arquivos={n: open(os.path.join(datos, n), "rb").read()
                                                  for n in sorted(os.listdir(datos))})
    with Maquina("h159-a", "ide", imagem=copia, snapshot=False, extras=extras_pen(pen)) as m:
        terminal(m)
        s = m.serial().replace("\r", "")
        verificar(re.search(r"\[FSTAB\] LABEL=SAGUI-APPS -> /apps \(auto, rw\): 0", s),
                  "A: el /etc/fstab monta SAGUI-APPS en /apps (lectura y escritura) por su etiqueta")
        verificar(not re.search(r"\[FSTAB\] arranque -> /home", s), "A: la partición de apps no se toma por el /home")
        out = m.comando("mount")
        verificar(re.search(r"/apps\s+tipo ext2\s+\(rw", out), f"A: /apps es ext2 de lectura y escritura ({out.strip()[-200:]})")
        out = m.comando("ls /apps /apps/bin /apps/netsurf")
        verificar("netsurf" in out and "sqlite3" in out and "leeme.txt" in out and "default.css" in out,
                  "A: /apps tiene bin/netsurf, bin/sqlite3, netsurf/ (recursos) y leeme.txt")
        verificar(not os.path.exists(os.path.join(BUILD, "rootfs", "bin", "netsurf"))
                  and not os.path.exists(os.path.join(BUILD, "rootfs", "bin", "sqlite3")),
                  "A: NetSurf y sqlite3 ya no van en el /bin del ramdisk")
        out = m.comando("which -a netsurf sqlite3")
        verificar("/apps/bin/netsurf" in out and "/apps/bin/sqlite3" in out, "A: /apps/bin está en el PATH (which)")
        verificar(not re.search(r"(?<!/apps)/bin/(netsurf|sqlite3)", out), f"A: y solo ahí ({out.strip()[-120:]})")
        out = m.comando("sqlite3 :memory: \"select 40+2, 'apps'\"")
        verificar("42|apps" in out, "A: sqlite3 se ejecuta desde /apps/bin por su nombre")
        out = m.comando("echo persiste-h159 > /apps/h159.txt")
        out = m.comando("cat /apps/h159.txt")
        verificar("persiste-h159" in out, "A: se escribe en /apps")

        # ---- NetSurf desde el shell, con la página de prueba de h153
        desde = len(m.serial())
        m.digitar("montar usb0")
        m.tecla("ret")
        esperar(m, r"\[MONTAGEM\] usb0.*montado em /usb", 40, desde=desde)
        marca = len(m.serial())
        m.digitar("netsurf file:///usb/prueba.html &")   # en segundo plano: el shell sigue
        m.tecla("ret")
        esperar(m, r'janela \d+ criada por \d+: "NetSurf"', 90, desde=marca)
        j = janela(m.serial()[marca:], "NetSurf")
        res = h153.esperar_cores(m, j, {"rojo": (h153.ROJO, 200 * 80 // 2), "verde": (h153.VERDE, 300 * 60 // 2),
                                        "png": (h153.AZUL_PNG, 64 * 64 // 2), "jpg": (h153.NARANJA_JPG, 64 * 64 // 2)},
                                 "h159")
        m.captura("H159-1-netsurf-shell")
        print("H159: shell:", {c: v[0] for c, v in res.items()})
        verificar(res["rojo"][0] >= 200 * 80 // 2 and res["verde"][0] >= 300 * 60 // 2,
                  "A: NetSurf desde /apps/bin dibuja las cajas de estilo.css")
        verificar(res["png"][0] >= 64 * 64 // 2 and res["jpg"][0] >= 64 * 64 // 2, "A: y las imágenes")
        h153.sem_falhas(m.serial()[marca:], "A shell")
        verificar("[SAGUI] executar /apps/bin/netsurf" in m.serial()[marca:], "A: el shell lo lanza desde /apps/bin")

        # ---- NetSurf desde Inicio > Internet
        p, sub = menu_internet(m, "H159-2-menu-internet")
        verificar("NetSurf (navegador)" in sub["linhas"], "A: Inicio > Internet tiene NetSurf (%s)" % ", ".join(sub["linhas"]))
        m.mouse_mover(sub["x"] + 30, y_linha(sub, "NetSurf (navegador)"))
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/netsurf \(pid \d+\)", 20, desde=marca)
        esperar(m, r'janela \d+ criada por \d+: "NetSurf"', 90, desde=marca)
        time.sleep(8)
        m.captura("H159-3-netsurf-menu")
        h153.sem_falhas(m.serial()[marca:], "A menú")
        verificar(not re.search(r"inicio -> /apps/bin/netsurf \(pid -", m.serial()[marca:]), "A: el menú abre NetSurf")

        # ---- reiniciar: lo escrito en /apps sigue ahí
        focar(m, janela(m.serial(), "Terminal")["id"])   # por la barra: NetSurf tapa el Terminal
        time.sleep(1)
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 90
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
        verificar(m.proc.poll() is not None, "A: reiniciar (el QEMU sale, -no-reboot)")
    with Maquina("h159-a2", "ide", imagem=copia, snapshot=False) as m:
        terminal(m)
        out = m.comando("cat /apps/h159.txt")
        verificar("persiste-h159" in out, "A: lo escrito en /apps persiste tras reiniciar")
    os.remove(copia)


def parte_b():
    imagem = imagem_de_teste("", ["--vesa"])
    with Maquina("h159-b", "ide", imagem=imagem) as m:
        terminal(m)
        verificar("SAGUI-APPS -> " not in m.serial(), "B: sin la partición de apps el sistema arranca igual")
        p, sub = menu_internet(m, "H159-4-sin-particion")
        verificar("NetSurf (navegador)" not in sub["linhas"] and "Cipó (navegador)" in sub["linhas"],
                  "B: sin la partición, Inicio > Internet no muestra NetSurf (%s)" % ", ".join(sub["linhas"]))
        m.tecla("esc")
        time.sleep(0.5)
        m.tecla("esc")
        time.sleep(0.5)
        h136.focar_terminal(m)
        out = m.comando("which netsurf")
        verificar("/apps/bin/netsurf" not in out, "B: netsurf no se encuentra")


def parte_c():
    """El CD: /apps va en su ramdisk (el mismo árbol que con APPS=0)."""
    subprocess.run(["make", "-s", "-C", RAIZ, os.path.relpath(os.path.join(BUILD, "ramdisk-cd.sgfs"), RAIZ)],
                   check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h159-cd.img")
    iso = os.path.join(SAIDA, "h159.iso")
    b = BUILD
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk-cd.sgfs"), "--saida", img, "--vesa", "--console", "--cd"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img, "--saida", iso],
                   check=True, stdout=subprocess.DEVNULL)
    os.remove(img)
    try:
        with Maquina("h159-c", "cd", imagem=iso) as m:
            m.esperar_serial("sagui:/$", 180)
            out = m.comando("which netsurf sqlite3")
            verificar("/apps/bin/netsurf" in out and "/apps/bin/sqlite3" in out,
                      "C: el CD lleva /apps en el ramdisk (which netsurf, sqlite3)")
            out = m.comando("ls /apps/netsurf")
            verificar("default.css" in out and "Messages" in out, "C: y los recursos de NetSurf")
            out = m.comando("sqlite3 :memory: \"select 6*7\"")
            verificar("42" in out, "C: sqlite3 funciona desde el CD")
    finally:
        if os.path.exists(iso):
            os.remove(iso)


def main():
    if not os.path.isdir(os.path.join(APPS, "bin")):
        raise AssertionError("falta %s (lo hace el make con APPS=1)" % APPS)
    if not os.path.exists(os.path.join(APPS, "bin", "netsurf")):
        print("H159: NetSurf no está compilado (falta tools/netsurf_fuentes.sh); no aplica")
        return 77
    partes = sys.argv[1:] or ["a", "b", "c"]
    if "a" in partes:
        parte_a()
    if "b" in partes:
        parte_b()
    if "c" in partes:
        parte_c()
    print("H159: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
