#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mide «Ampliar /home» del primer arranque (D338). No es una prueba de la batería
(no empieza por hNN): sirve para ver dónde se va el tiempo antes y después de un cambio.

  medir_ampliar_home.py [ide|usb|xhci|uhci] [GB] [--sin-datos] [--ioapic] [--sucio]

Hace la imagen de la parte 4 de h318 (/home de 64 MB, SAGUI-APPS, juegos y SAGUI-CONF detrás),
la graba en un disco de GB gigas, arranca, pulsa «Ampliar» y anota:
  - el tiempo total del anfitrión (del clic al resultado);
  - las líneas de tiempos de la libsagui (comprobación, copia, ext2) y la de la reserva del
    kernel ([BLOCOS] ...: lo leído y grabado, ms en el driver, IRQ por línea y CPU 0 ociosa);
  - e2fsck -fn de cada lógica al acabar.
Con --sin-datos la imagen no lleva partición de /home (la básica): se crea SAGUI-DATOS.
Resultados en build/testes/medir-ampliar-<modo>-<GB>.txt."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, extrair_particao, opcoes_apps, RAIZ, SAIDA, BUILD  # noqa: E402

MB = 1024 * 1024


def sfdisk(img):
    r = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True).stdout
    t = {}
    for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)", r):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), m.group(4))
    return t


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    modo = args[0] if args else "ide"
    gb = int(args[1]) if len(args) > 1 else 2
    sem_dados = "--sin-datos" in sys.argv
    ioapic = ["--ioapic"] if "--ioapic" in sys.argv else []
    img = os.path.join(SAIDA, f"medir-ampliar-{modo}.img")
    os.makedirs(SAIDA, exist_ok=True)
    b = BUILD
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", f"{b}/boot/mbr.bin", "--estagio2", f"{b}/boot/estagio2.bin", "--kernel", f"{b}/kernel.bin",
                    "--kernel-elf", f"{b}/kernel.elf", "--ramdisk", f"{b}/ramdisk.sgfs", "--saida", img,
                    "--vesa", *([] if sem_dados else ["--dados-mb", "64"]), *ioapic, *opcoes_apps()], check=True, stdout=subprocess.DEVNULL)
    with open(img, "r+b") as f:
        f.seek(0, 2)
        fim = f.tell()
        if "--sucio" in sys.argv:   # el espacio libre lleno de datos viejos (lo peor para leer antes de poner a cero)
            trozo = bytes([0x5A]) * MB
            while fim < gb * 1024 * MB:
                f.write(trozo[:min(MB, gb * 1024 * MB - fim)])
                fim = f.tell()
        f.truncate(gb * 1024 * MB)
    t0 = sfdisk(img)
    linhas = []
    with Maquina(f"medir-ampliar-{gb}", modo, imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: pregunta", 300)
        time.sleep(2)
        s = m.serial()
        mm = re.search(r"ampliar-home: pregunta: ganar (\d+) MB \((\w+)\)[^\n]*botones y=(\d+) ampliar=(\d+)", s)
        v = re.findall(r'criada por \d+: "Primer arranque: espacio libre" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        jx, jy = int(v[-1][2]) + 2, int(v[-1][3]) + 24
        m.clique(jx + int(mm.group(4)), jy + int(mm.group(3)) + 13)
        ini = time.time()
        m.esperar_serial("ampliar-home: empieza", 60)
        m.esperar_serial("ampliar-home: resultado", 3600)
        total = time.time() - ini
        time.sleep(1)
        s = m.serial()
        trecho = s[s.rfind("ampliar-home: empieza"):]
        for ln in trecho.splitlines():
            if re.search(r"particiones: tamaño|particiones: /home|redimensao:|nao chega|IRQ no llega|\[BLOCOS\] .*reserva de|ampliar-home: resultado|formatar:|\[CACHE\]|\[ATA\]|\[EHCI\]|\[XHCI\]|\[UHCI\]", ln):
                linhas.append(ln.strip())
        m.digitar("apagar-pc\n", 0.05)
        fim = time.time() + 60
        while time.time() < fim and m.proc.poll() is None:
            time.sleep(0.3)
    t = sfdisk(img)
    linhas.insert(0, f"modo {modo}, disco de {gb} GB{' sin /home' if sem_dados else ''}{' con IOAPIC' if ioapic else ''}{' con el espacio libre sucio' if '--sucio' in sys.argv else ''}, KVM={os.environ.get('SAGUI_KVM', '0')}: "
                     f"{mm.group(0)}; tiempo total {total:.1f} s")
    for n, p in sorted(t.items()):
        if n < 5:
            continue
        part = extrair_particao(img, p[0], p[1] // 2048)
        r = subprocess.run(["/usr/sbin/e2fsck", "-fn", part], capture_output=True, text=True)
        linhas.append(f"lógica {n} ({p[1] // 2048} MB): e2fsck -fn {'limpio' if r.returncode == 0 else 'FALLA: ' + r.stdout[-300:]}")
        os.remove(part)
    linhas.append(f"tabla antes {t0}")
    linhas.append(f"tabla después {t}")
    salida = os.path.join(SAIDA, f"medir-ampliar-{modo}-{gb}{'-sd' if sem_dados else ''}{'-ioapic' if ioapic else ''}{'-sucio' if '--sucio' in sys.argv else ''}.txt")
    with open(salida, "w") as f:
        f.write("\n".join(linhas) + "\n")
    print("\n".join(linhas))
    os.remove(img)


if __name__ == "__main__":
    main()
