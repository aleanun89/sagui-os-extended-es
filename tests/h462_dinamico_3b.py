#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H462 - /bin dinámico salvo lo esencial (D462, enlace dinámico fase 3b).

Criterio:
  A. En el PC: los programas de BIN_ESTATICOS (Makefile) son estáticos en build/rootfs/bin y los del
     initramfs (tools/mkinitramfs.py) están todos en esa lista; el resto de /bin que sale de la regla
     común es dinámico (PT_INTERP = /lib/ld-sagui.so.1) y solo necesita libsagui.so.1,
     libsagui_medios.so.1 y libc.so.1, que están en /lib (build/lib) con ld-sagui.so.1.
  B. La imagen de serie (raíz SAGUI-RAIZ, live): el escritorio arranca; /testes/dinamico3b, desde el
     Terminal de la sesión, ve en /bin lo mismo que el PC (estáticos y dinámicos) y cada programa
     dinámico pasa del intérprete con «--help» (ni código 127 ni «ld-sagui:»); sin excepciones.
  C. Lo mismo con el ramdisk de siempre (RAIZ_DISCO=0, «completo»; clasica=True).
  D. Vuelta atrás: los mismos programas enlazados con DINAMICO=0 (en build/h462-estatico, sin tocar
     build/) en una copia de la raíz: todo /bin estático y el escritorio y los programas funcionan igual.
     H462_SIN_ESTATICO=1 se salta esta parte (compila unos 150 programas la primera vez).
Imprime las medidas: tamaños, memoria de la sesión, arranque y tiempo de lanzar programas.
"""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, BUILD, RAIZ, INITRAMFS, ROOTFS  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 240 if KVM else 900
ESPERA_MS = 2500 if KVM else 6000


def readelf(opc, f):
    return subprocess.run(["readelf", opc, "-W", f], capture_output=True, text=True).stdout


def es_elf(f):
    with open(f, "rb") as h:
        return h.read(4) == b"\x7fELF"


def es_dinamico(f):
    return "Requesting program interpreter: /lib/ld-sagui.so.1" in readelf("-l", f)


def estaticos_makefile():
    with open(os.path.join(RAIZ, "Makefile"), encoding="utf-8") as h:
        s = h.read()
    m = re.search(r"^BIN_ESTATICOS := ((?:.*\\\n)*.*)$", s, re.M)
    return set(m.group(1).replace("\\\n", " ").split())


def tam_pasta(p):
    return sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(p) for f in fs)


def no_pc():
    est = estaticos_makefile()
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import mkinitramfs  # noqa: E402
    falta = sorted(set(mkinitramfs.PROGRAMAS) - est)
    verificar(not falta, f"los programas del initramfs están en BIN_ESTATICOS ({falta})")
    binp = os.path.join(ROOTFS, "bin")
    clas = {}
    for n in sorted(os.listdir(binp)):
        f = os.path.join(binp, n)
        if os.path.isfile(f) and es_elf(f):
            clas[n] = es_dinamico(f)
    mal_est = sorted(n for n in est if n in clas and clas[n])
    verificar(not mal_est, f"los esenciales son estáticos ({len([n for n in est if n in clas])} en /bin; dinámicos: {mal_est})")
    faltan = sorted(n for n in est if n not in clas)
    verificar(not faltan, f"todos los esenciales están en /bin ({faltan})")
    din = sorted(n for n, d in clas.items() if d)
    verificar(len(din) >= 100, f"/bin dinámico: {len(din)} de {len(clas)} programas")
    permitidas = {"libsagui.so.1", "libsagui_medios.so.1", "libc.so.1"}
    raras = []
    for n in din:
        nec = set(re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", readelf("-d", os.path.join(binp, n))))
        if not nec <= permitidas or "libsagui.so.1" not in nec:
            raras.append((n, sorted(nec)))
    verificar(not raras, f"los dinámicos solo piden libsagui.so.1 (+ medios, libc) ({raras[:5]})")
    for so in ("ld-sagui.so.1",) + tuple(sorted(permitidas)):
        verificar(os.path.isfile(os.path.join(ROOTFS, "lib", so)), f"/lib/{so} en la raíz")
    medios = sum(1 for n in din if "libsagui_medios" in readelf("-d", os.path.join(binp, n)))
    print(f"    MEDIDA /bin: {tam_pasta(binp)} bytes ({len(din)} dinámicos, {len(clas) - len(din)} estáticos, "
          f"{medios} con libsagui_medios); /lib: {tam_pasta(os.path.join(ROOTFS, 'lib'))} bytes")
    return clas


def sesion(nombre, imagem, clas_esperada, todo_estatico=False):
    """Arranca el escritorio, lanza /testes/dinamico3b desde el Terminal y comprueba lo que ve."""
    t0 = time.time()
    with Maquina(nombre, "ide", imagem=imagem, memoria="1G") as m:
        m.esperar_serial("sagui:", TEMPO)
        t_prompt = time.time() - t0
        time.sleep(15)   # que el panel, los widgets y los servicios terminen de arrancar
        out = m.comando("/testes/dinamico3b %d" % ESPERA_MS, tempo=TEMPO * 4)
        s = m.serial().replace("\r", "")
    # Los avisos del kernel («[SAGUI] executar ...») salen directos por el serie y pueden caer en medio de una
    # línea del Terminal: se quitan para volver a juntarla.
    out = re.sub(r"\[SAGUI\] [^\n]*\n", "", out)
    arr = re.findall(r"\[ARRANQUE\] (\w+): (\d+) ms desde el encendido", s)
    print(f"    MEDIDA [{nombre}] prompt del Terminal a los {t_prompt:.1f} s; " +
          ", ".join(f"{a}={b} ms" for a, b in arr if a in ("escritorio", "configuracion")))
    for l in out.split("\n"):
        if l.startswith(("D3B MEM", "D3B TEMPO", "D3B LANZADOS", "D3B ABIERTOS")) or " MAL" in l or "[FALHA]" in l:
            print(f"    {l.strip()}")
    vista = {}
    for nome, tipo in re.findall(r"^D3B BIN (\S+) (din|est) \d+", out, re.M):
        vista[nome] = tipo == "din"
    for n in clas_esperada:   # una línea partida por un aviso del kernel en medio del serie
        if n not in vista:
            g = re.search(r"(?:^|\s)%s (din|est) \d+" % re.escape(n), out, re.M)
            if g:
                vista[n] = g.group(1) == "din"
    if todo_estatico:
        verificar(vista and not any(vista.values()), f"[{nombre}] todo /bin estático ({sum(vista.values())} dinámicos)")
    else:
        dif = sorted(n for n in clas_esperada if vista.get(n) != clas_esperada[n])
        verificar(vista and not dif, f"[{nombre}] /bin en SaguiOS igual que en el PC ({len(vista)} programas; distintos: {dif[:8]})")
    verificar("RESULTADO dinamico3b: SUCESSO" in out, f"[{nombre}] dinamico3b: SUCESSO")
    verificar("EXCECAO" not in s, f"[{nombre}] sin excepciones del kernel")
    return out


def construir_estatico(clas):
    """D: los programas dinámicos de /bin otra vez con DINAMICO=0 (en build/h462-estatico) y una raíz con ellos."""
    bdir = os.path.join(BUILD, "h462-estatico")
    din = [n for n, d in clas.items() if d]
    alvos = [os.path.join("build", "h462-estatico", "userland", "bin", n) for n in din]
    r = subprocess.run(["make", "-j2", "B=build/h462-estatico", "DINAMICO=0", *alvos], cwd=RAIZ,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    verificar(r.returncode == 0, "make DINAMICO=0 de los programas de /bin" + ("" if r.returncode == 0 else ": " + r.stderr[-800:]))
    raiz = os.path.join(bdir, "rootfs")
    if os.path.exists(raiz):
        shutil.rmtree(raiz)
    shutil.copytree(ROOTFS, raiz, symlinks=True)
    for n in din:
        shutil.copy2(os.path.join(bdir, "userland", "bin", n), os.path.join(raiz, "bin", n))
    quedan = [n for n in din if es_dinamico(os.path.join(raiz, "bin", n))]
    verificar(not quedan, f"con DINAMICO=0 no queda ningún dinámico en /bin ({quedan[:5]})")
    print(f"    MEDIDA /bin con DINAMICO=0: {tam_pasta(os.path.join(raiz, 'bin'))} bytes")
    return raiz


def main():
    clas = no_pc()
    sesion("h462", imagem_de_teste("", ["--vesa"]), clas)
    sesion("h462-clasica", imagem_de_teste("", ["--vesa"], clasica=True), clas)
    if os.environ.get("H462_SIN_ESTATICO") == "1":
        print("    (H462_SIN_ESTATICO=1: sin la parte de la vuelta atrás)")
        return 0
    raiz = construir_estatico(clas)
    sesion("h462-estatico", imagem_de_teste("", ["--vesa", "--ramdisk", INITRAMFS, "--raiz", raiz]),
           clas, todo_estatico=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
