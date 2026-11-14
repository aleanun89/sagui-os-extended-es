#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H89 (D118, mejora 064) - Rendimiento de SQLite en SaguiOS, en C (".bench" del programa sqlite3) y
en Python (tests/recursos/sqlite/bench.py): inserciones por segundo con y sin transacción, consultas
por clave primaria y por índice y un recorrido completo, en /tmp (RAM) y en /home (ext2 en un
pendrive). Imprime las cifras (no hay umbrales estrictos: solo que cada medida sea > 0 y que las
comprobaciones de filas y suma coincidan con lo esperado) y también comprueba que
repr('b\\xe9') de Python escapa o no las tildes (mira docs/sqlite.md).

Con KVM: sg kvm -c "SAGUI_KVM=1 python3 tests/h89_sqlite_rendimiento.py"; sin KVM: sin variable.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco, pendrive_qemu, RAIZ  # noqa: E402

REC = os.path.join(RAIZ, "tests", "recursos", "sqlite")
N = 200000


def cifras(salida):
    """{"insertar en una transaccion": 12345, ...} a partir de las líneas «bench: ...»."""
    r = {}
    for m in re.finditer(r"bench: ([^:\n]+): (\d+) en (\d+) ms = (\d+) por segundo", salida):
        r[m.group(1).split(" (")[0]] = int(m.group(4))
    return r


def comprobar(nombre, salida, n):
    c = cifras(salida)
    verificar(len(c) == 5 and all(v > 0 for v in c.values()), f"{nombre}: cinco medidas positivas")
    esperado = f"bench: comprobacion: {n + max(10, n // 10)} filas, {2 * n} aciertos, suma"
    if esperado not in salida:
        print(salida)
    verificar(esperado in salida, f"{nombre}: filas leídas y aciertos correctos")
    print(f"  CIFRAS {nombre}: " + ", ".join(f"{k} = {v}/s" for k, v in c.items()))
    return c


def main():
    arq = {"bench.py": open(os.path.join(REC, "bench.py"), "rb").read(), "repr.py": open(os.path.join(REC, "repr.py"), "rb").read()}
    img, _ = criar_disco("h89-pen", [dict(tipo="ext2", mb=24, rotulo="SQBEN", arquivos=arq, opcoes=["-b", "4096", "-I", "256"])])
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    kvm = os.environ.get("SAGUI_KVM") == "1"
    print(f"== SQLite: rendimiento ({'con KVM' if kvm else 'sin KVM'}), N = {N}")
    with Maquina("h89", "ide", imagem=imagem_console(apps=True), extras=extras) as m:   # D194: sqlite3 en /apps
        m.esperar_serial("sagui:/$", 120)
        verificar("montado en /home" in m.comando("mount usb0p1 /home"), "ext2 montado en /home")
        for v, nombre in (("/tmp", "RAM"), ("/home", "ext2")):
            n = N if v == "/tmp" else 2000
            out = m.comando(f"rm -f {v}/b.db; sqlite3 {v}/b.db \".bench {n}\"", tempo=900)
            comprobar(f"C (sqlite3) en {nombre}", out, n)
            out = m.comando(f"python /home/bench.py {v} {n}", tempo=900)
            comprobar(f"Python en {nombre}", out, n)
        out = m.comando("python /home/repr.py")
        print("  repr con tildes:", [l for l in out.splitlines() if "'b" in l or "b'" in l])
        verificar("[TESTE] falha" not in m.serial() and "PANICO" not in m.serial(), "nenhum erro no kernel")
        m.comando("umount /home")
    return 0


if __name__ == "__main__":
    sys.exit(main())
