#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H450 - reiniciar y apagar terminan siempre, en poco tiempo y en el orden de D450.

Uso: h450_reinicio.py [a] [b] [c] [d] [e] [f] [g]   (sin argumentos, todo)

El QEMU corre con -no-reboot: un reinicio (por cualquier método) lo cierra igual que el S5. Lo que
dice qué pasó son las marcas del kernel en el serie («[ENERGIA] +N ms, paso P: ...»):
  (a) BIOS, sin SMP: «reiniciar» en un i440fx (FADT 1.0, sin RESET_REG: reinicia el 8042) y tres
      veces seguidas en un q35 (una máquina cada vez; RESET_REG en E/S 0xCF9 <- 0xF): los pasos 1-6 en
      orden y QEMU termina en < 20 s;
  (b) cada método por separado: «reiniciar teclado», «reiniciar cf9» y «reiniciar triple»
      reinician con ese método (sin pasar al siguiente);
  (c) SMP (--smp --ioapic, 4 CPU) con un NVMe y el arranque por xHCI: se paran los 3 secundarios,
      el NVMe recibe el aviso de apagado y el xHCI se para; reiniciar y apagar-pc terminan;
  (d) uACPI (--uacpi): apagar-pc ejecuta _PTS(5) y reiniciar no; los dos terminan;
  (e) UEFI (OVMF de 64 bits): reiniciar y apagar-pc terminan (sin 8042 ni BIOS en medio);
  (f) sin ACPI (--sem-acpi): reiniciar sigue reiniciando (sin RESET_REG, por el 8042);
  (g) lo más parecido al Dell: UEFI x64, q35, SMP con IOAPIC, uACPI y el escritorio: tres reinicios por el
      RESET_REG y un apagado con _PTS(5).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, imagem_console, criar_disco, BUILD  # noqa: E402

PLAZO_S = 20   # desde la orden hasta que el QEMU se cierra
UEFI = os.path.join(BUILD, "boot", "uefi")
MARCA = re.compile(r"\[ENERGIA\] \+(\d+) ms, paso (\d+): ([^\r\n]*)")


def marcas(m, desde):
    return [(int(a), int(b), c) for a, b, c in MARCA.findall(m.serial()[desde:])]


def ordenar(m, orden, rotulo):
    """Escribe la orden y espera a que el QEMU se cierre solo. Devuelve las marcas y los segundos."""
    desde = len(m.serial())
    t = time.time()
    m.digitar(orden + "\n", 0.04)
    try:
        m.proc.wait(timeout=PLAZO_S + 5)
    except Exception:
        pass
    seg = time.time() - t
    ms = marcas(m, desde)
    verificar(m.proc.poll() is not None, f"{rotulo}: «{orden}» cierra el QEMU (terminó: {seg:.1f} s)")
    verificar(seg < PLAZO_S, f"{rotulo}: en menos de {PLAZO_S} s ({seg:.1f} s)")
    pasos = [p for _, p, _ in ms]
    verificar(pasos and pasos == sorted(pasos) and pasos[0] == 1 and pasos[-1] == 6,
              f"{rotulo}: pasos 1 a 6 en orden ({pasos})")
    for ms_, p, txt in ms:
        print(f"    {rotulo}: +{ms_} ms paso {p}: {txt}")
    return ms, seg


def ultimo_metodo(ms):
    return [t for _, p, t in ms if p == 6][-1]


def parte_a():
    """i440fx (FADT 1.0, sin RESET_REG: el 8042) y q35 (FADT 3, RESET_REG en E/S 0xCF9 <- 0xF), tres veces."""
    img = imagem_console()
    for i, maq in enumerate(("pc", "q35", "q35", "q35")):
        with Maquina(f"h450-a{i}", "ide", imagem=img, extras=["-machine", maq]) as m:
            m.esperar_serial("sagui:/$", 120)
            s = m.serial()
            ms, _ = ordenar(m, "reiniciar", f"a{i}-{maq}")
            ult = ultimo_metodo(ms)
            if maq == "pc":
                verificar("[ACPI] RESET_REG:" not in s, f"a{i}: la FADT 1.0 del i440fx no tiene RESET_REG")
                verificar("ACPI RESET_REG: no hay" in " ".join(t for _, _, t in ms), f"a{i}: lo dice y sigue")
                verificar(ult.startswith("8042"), f"a{i}: reinicia el 8042 ({ult})")
            else:
                verificar(re.search(r"\[ACPI\] RESET_REG: espacio 1 direccion 0:cf9 valor [0-9a-f]+;", s) is not None,
                          f"a{i}: el kernel lee el RESET_REG del q35 (E/S 0xCF9)")
                verificar(ult.startswith("ACPI RESET_REG: E/S"), f"a{i}: reinicia el RESET_REG del ACPI ({ult})")


def parte_b():
    img = imagem_console()
    for metodo, texto in (("teclado", "8042"), ("cf9", "0xCF9"), ("triple", "triple fallo")):
        with Maquina(f"h450-b-{metodo}", "ide", imagem=img) as m:
            m.esperar_serial("sagui:/$", 120)
            ms, _ = ordenar(m, f"reiniciar {metodo}", f"b-{metodo}")
            ult = ultimo_metodo(ms)
            verificar(ult.startswith(texto), f"b-{metodo}: reinicia el primero que se pide ({ult})")
            verificar(f"metodo pedido" in ms[0][2], f"b-{metodo}: la marca 1 dice el método pedido")


def parte_c():
    img = imagem_de_teste("", ["--vesa", "--console", "--smp", "--ioapic"])
    nv, _ = criar_disco("h450-nvme", [dict(tipo="fat32", mb=40, rotulo="NVME450")])
    extras = ["-drive", f"if=none,id=nv0,format=raw,file={nv},snapshot=on",
              "-device", "nvme,id=nvme0,serial=SG450", "-device", "nvme-ns,drive=nv0,bus=nvme0,nsid=1"]
    for orden in ("reiniciar", "apagar-pc"):
        with Maquina(f"h450-c-{orden}", "xhci", imagem=img, nucleos=4, extras=extras) as m:
            m.esperar_serial("sagui:/$", 180)
            verificar("[NVME]" in m.serial(), f"c-{orden}: el NVMe está")
            ms, _ = ordenar(m, orden, f"c-{orden}")
            todo = " | ".join(t for _, _, t in ms)
            verificar("procesadores: 3 de 3 secundarios parados" in todo, f"c-{orden}: se paran los 3 secundarios")
            g = re.search(r"USB parado: xHCI (\d+)", todo)
            verificar(g and int(g.group(1)) >= 1, f"c-{orden}: el xHCI se para")
            g = re.search(r"NVMe (\d+) terminados", todo)
            verificar(g and int(g.group(1)) == 1, f"c-{orden}: el NVMe termina el aviso de apagado")
            if orden == "apagar-pc":
                verificar("S5 por ACPI" in ultimo_metodo(ms), "c: apagar-pc acaba en el S5")


def parte_d():
    img = imagem_de_teste("", ["--vesa", "--console", "--uacpi"])
    for orden in ("apagar-pc", "reiniciar"):
        with Maquina(f"h450-d-{orden}", "ide", imagem=img) as m:
            m.esperar_serial("sagui:/$", 120)
            fim = time.time() + 30
            while "[UACPI] listo" not in m.serial() and time.time() < fim:
                time.sleep(0.5)
            verificar("[UACPI] listo" in m.serial(), f"d-{orden}: uACPI cargado")
            ms, _ = ordenar(m, orden, f"d-{orden}")
            acpi = [t for _, p, t in ms if p == 4 and t.startswith("ACPI:")]
            verificar(acpi, f"d-{orden}: la marca del ACPI ({acpi})")
            if orden == "apagar-pc":
                verificar("_PTS(5): hecho" in acpi[0], f"d: apagar-pc ejecuta _PTS(5) ({acpi[0]})")
            else:
                verificar("_PTS(5): no" in acpi[0], f"d: reiniciar no ejecuta _PTS ({acpi[0]})")


def parte_e():
    img = imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI])
    for orden in ("reiniciar", "apagar-pc"):
        with Maquina(f"h450-e-{orden}", "ide", imagem=img, firmware="x64") as m:
            m.esperar_serial("sagui:/$", 180)
            verificar("firmware=uefi-x64" in m.serial().lower(), f"e-{orden}: arrancó por UEFI")
            ms, _ = ordenar(m, orden, f"e-{orden}")


def parte_f():
    img = imagem_de_teste("", ["--vesa", "--console", "--sem-acpi"])
    with Maquina("h450-f", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        ms, _ = ordenar(m, "reiniciar", "f")
        todo = " | ".join(t for _, _, t in ms)
        verificar("ACPI RESET_REG: no hay" in todo, "f: con --sem-acpi no se usa el RESET_REG")
        verificar("8042" in ultimo_metodo(ms), "f: reinicia el 8042")


def parte_g():
    """Lo más parecido al Dell: UEFI x64, q35 (RESET_REG), SMP con IOAPIC, uACPI y el escritorio."""
    img = imagem_de_teste("", ["--vesa", "--uefi", UEFI, "--smp", "--ioapic", "--uacpi"])
    for i, orden in enumerate(("reiniciar", "reiniciar", "reiniciar", "apagar-pc")):
        with Maquina(f"h450-g{i}", "ide", imagem=img, firmware="x64", nucleos=4,
                     extras=["-machine", "q35"]) as m:
            m.esperar_serial("sagui:/$", 240)
            fim = time.time() + 30
            while "[UACPI] listo" not in m.serial() and time.time() < fim:
                time.sleep(0.5)
            ms, _ = ordenar(m, orden, f"g{i}-{orden}")
            todo = " | ".join(t for _, _, t in ms)
            verificar("procesadores: 3 de 3 secundarios parados" in todo, f"g{i}: se paran los 3 secundarios")
            if orden == "reiniciar":
                verificar(ultimo_metodo(ms).startswith("ACPI RESET_REG: E/S"), f"g{i}: reinicia el RESET_REG")
            else:
                verificar("_PTS(5): hecho" in todo and "S5 por ACPI" in ultimo_metodo(ms), f"g{i}: _PTS(5) y S5")


def main():
    partes = sys.argv[1:] or ["a", "b", "c", "d", "e", "f", "g"]
    for p in partes:
        print(f"--- parte {p}")
        globals()[f"parte_{p}"]()
    print("H450 OK")


if __name__ == "__main__":
    main()
