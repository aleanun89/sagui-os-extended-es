#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H209 - el _PRT con uACPI y la autoprueba de las IRQ por el IOAPIC (D211).

Criterio (todas con --smp --ioapic, cuatro CPU, e1000, EHCI con pendrive y
sonido HDA):
  q35   (--uacpi, -machine q35): en el arranque la autoprueba del EHCI, la
        e1000 y la HDA pasa por la entrada ISA; después la tarea acpi-aml
        llama a \\_PIC(1), el _PRT da a cada función la GSI 16-23 que el
        QEMU le asigna (el ICH9 de QEMU: ranura s, pin p -> PIRQ E-H,
        16 + 4 + (s + p - 1) % 4; y, sin KVM, el contador de interrupciones
        del IOAPIC del propio QEMU tiene esas GSI), la autoprueba vuelve a
        pasar por esas GSI y los dispositivos funcionan (ping, pendrive,
        infopc con la vía de cada IRQ);
  pc    (--uacpi, i440fx): el _PRT va por los enlaces LNKx y da la misma IRQ
        que dejó la BIOS (la GSI 5, 9, 10 u 11); la autoprueba pasa;
  falla (--uacpi --irq-prueba prt-errada, q35): el _PRT da a los dispositivos
        con sonda una GSI sin nada conectado; la autoprueba lo ve y esa línea
        pasa al 8259 (modo mixto) sin perder el dispositivo: ping, pendrive e
        infopc dicen "8259";
  amilo (--uacpi --irq-prueba isa-errada, q35): como en el Amilo Pi 1536, la
        entrada ISA no entrega; la autoprueba del arranque cae al 8259 y,
        con el _PRT, vuelve al IOAPIC por las GSI de verdad.

  volcados (en el PC, sin QEMU; solo con SAGUI_ACPIDUMPS=<carpeta> con los
        volcados de uACPI/ACPIDumps): tools/uacpi_volcado.c --prt con el mismo
        prt.c del kernel sobre el Dell Inspiron 6400 (945/ICH7-M, como el
        Amilo) y el Toshiba Satellite L655 (HM55): el EHCI y los UHCI tienen
        GSI y el enlace del modo PIC (PIRQ) coincide con la GSI del modo APIC.

Uso: h209_ioapic_prt.py [q35] [pc] [falla] [amilo] [volcados]   (sin argumentos, todo)
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SONDAS = ("e1000", "HDA", "EHCI")


def imagem(*opcoes):
    return imagem_de_teste("", ["--vesa", "--console", "--smp", "--ioapic", *opcoes])


def maquina(nome, img, q35=True):
    pen = criar_pendrive(f"h209-{nome}-pen", arquivos={"hola.txt": b"hola desde el pendrive\n"})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    if q35:
        extras = ["-machine", "q35", "-nic", "user,model=e1000"] + extras
    return Maquina(f"h209-{nome}", "ide", imagem=img, extras=extras, som="hda", nucleos=4)


def esperar_prt(m, rotulo):
    m.esperar_serial("sagui:/$", 120)
    m.esperar_serial("[IOAPIC] tabla (_PRT)", 60)
    time.sleep(0.5)
    s = m.serial().replace("\r", "")
    verificar("EXCECAO" not in s and "EXCEPCION" not in s, f"[{rotulo}] sin excepciones")
    verificar("[PRT] \\_PIC(1) (modo APIC): no error" in s, f"[{rotulo}] se llama a \\_PIC(1)")
    return s


def tabla_prt(s):
    """{(bus, disp, fun): (pin, gsi, linea_bios)} de las lineas [PRT] del log."""
    t = {}
    for b, d, f, p, g, l in re.findall(r"\[PRT\] (\w\w):(\w\w)\.(\w) INT([A-D]) -> .*?-> GSI (\d+) .*?"
                                       r"\(la BIOS dejo la IRQ (\d+)\)", s):
        t[(int(b, 16), int(d, 16), int(f, 16))] = ("ABCD".index(p) + 1, int(g), int(l))
    return t


def sondas_bdf(s):
    """{nombre: (bus, disp, fun)} de las sondas que pasaron por la autoprueba."""
    r = {}
    for nome, b, d, f in re.findall(r"\[IOAPIC\] autoprueba \(arranque\): (\S+) (\w\w):(\w\w)\.(\w)", s):
        r[nome] = (int(b, 16), int(d, 16), int(f, 16))
    return r


def info_irq_ioapic(m):
    """Contadores por pin del IOAPIC del QEMU (solo sin KVM); {} si no los da."""
    m.mon.sendall(b"info irq\n")
    resp = m._ler_monitor()
    resp = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", resp).replace("\r", "")
    i = resp.find("IRQ statistics for ioapic")
    if i < 0:
        return {}
    cont = {}
    for linha in resp[i:].split("\n")[1:]:
        mm = re.match(r"\s*(\d+):\s*(\d+)", linha)
        if not mm:
            break
        cont[int(mm.group(1))] = int(mm.group(2))
    return cont


def funcionan(m, rotulo):
    out = m.comando("ping -c 3 10.0.2.2", tempo=30)
    verificar("3 paquetes enviados, 3 recibidos, 0% perdidos" in out, f"[{rotulo}] ping -c 3 10.0.2.2: 3 respuestas")
    m.comando("montar usb0p1", 30)
    out = m.comando("ls /usb", 30)
    verificar("hola.txt" in out.lower(), f"[{rotulo}] el pendrive USB (EHCI) se lee")
    return m.comando("infopc -t pci", 60)


def q35():
    with maquina("q35", imagem("--uacpi")) as m:
        s = esperar_prt(m, "q35")
        for n in SONDAS:
            verificar(re.search(rf"autoprueba \(arranque\): {n} \S+ GSI \d+ -> IRQ \d+: llega", s),
                      f"[q35] en el arranque la IRQ de {n} llega por la entrada ISA")
        t, sb = tabla_prt(s), sondas_bdf(s)
        verificar(len(sb) == 3, f"[q35] las tres sondas (e1000, HDA, EHCI) estan: {sb}")
        for n, (b, d, f) in sb.items():
            pino, gsi, linha = t.get((b, d, f), (0, -1, 0))
            esperado = 16 + 4 + (d + pino - 1) % 4
            verificar(gsi == esperado, f"[q35] {n} {b:02x}:{d:02x}.{f}: el _PRT da la GSI {gsi}; el ICH9 del QEMU, "
                                       f"{esperado} (ranura {d}, INT{'ABCD'[pino - 1] if pino else '?'})")
            verificar(re.search(rf"autoprueba \(_PRT\): {n} \S+ GSI {gsi} -> IRQ {linha}: llega", s),
                      f"[q35] con el _PRT la IRQ de {n} llega por la GSI {gsi} (vector de la IRQ {linha})")
        infopc = funcionan(m, "q35")
        verificar(re.search(r"IRQ \d+\s+00:\w\w\.\d INT\w\s+EHCI\s+IOAPIC GSI 2\d \(_PRT\)", infopc),
                  "[q35] infopc pci: el EHCI va por el IOAPIC con la GSI del _PRT")
        cont = info_irq_ioapic(m)
        if cont:
            for n, (b, d, f) in sb.items():
                gsi = t.get((b, d, f), (0, -1, 0))[1]
                verificar(cont.get(gsi, 0) > 0, f"[q35] el IOAPIC del QEMU conto interrupciones en la GSI {gsi} ({n}): "
                                                f"{cont.get(gsi, 0)}")
        else:
            print("  (sin contadores del IOAPIC del QEMU: con KVM no los da)")


def pc():
    with maquina("pc", imagem("--uacpi"), q35=False) as m:
        s = esperar_prt(m, "pc")
        t, sb = tabla_prt(s), sondas_bdf(s)
        verificar(len(sb) == 3, f"[pc] las tres sondas estan: {sb}")
        for n, (b, d, f) in sb.items():
            pino, gsi, linha = t.get((b, d, f), (0, -1, 0))
            verificar(gsi == linha and gsi in (5, 9, 10, 11),
                      f"[pc] {n}: el _PRT (enlace LNKx) da la GSI {gsi}, la IRQ {linha} que dejo la BIOS")
            verificar(re.search(rf"autoprueba \(_PRT\): {n} \S+ GSI {gsi} -> IRQ {linha}: llega", s),
                      f"[pc] la IRQ de {n} llega por la GSI {gsi}")
        verificar(re.search(r"\[PRT\] .* -> \\_SB_\.PCI0 \\_SB_\.LNK[A-D] -> GSI", s), "[pc] el _PRT usa los enlaces LNKx")
        funcionan(m, "pc")


def falla():
    with maquina("falla", imagem("--uacpi", "--irq-prueba", "prt-errada")) as m:
        s = esperar_prt(m, "falla")
        erradas = re.findall(r"\[PRT\] prueba: (\S+) \S+ recibe la GSI equivocada (\d+)", s)
        verificar(len(erradas) == 3, f"[falla] los tres dispositivos con sonda reciben una GSI equivocada: {erradas}")
        for n, g in erradas:
            verificar(re.search(rf"autoprueba \(_PRT\): {n} \S+ GSI {g} no llega -> PIC \(IRQ \d+\)", s),
                      f"[falla] {n}: la GSI {g} no llega -> PIC")
        infopc = funcionan(m, "falla")
        verificar(re.search(r"EHCI\s+8259 \(dentro del IOAPIC\) -> CPU 0", infopc),
                  "[falla] infopc pci: el EHCI va por el 8259 dentro del IOAPIC")
        verificar("autoprueba: no llegaba por el IOAPIC, va por el 8259" in infopc,
                  "[falla] infopc dice por que")


def amilo():
    with maquina("amilo", imagem("--uacpi", "--irq-prueba", "isa-errada")) as m:
        s = esperar_prt(m, "amilo")
        verificar("la entrada ISA de la IRQ" in s and "(simulado, --irq-prueba isa-errada)" in s,
                  "[amilo] la entrada ISA no entrega (simulado)")
        for n in SONDAS:
            verificar(re.search(rf"autoprueba \(arranque\): {n} \S+ GSI \d+ no llega -> PIC \(IRQ \d+\)", s),
                      f"[amilo] en el arranque {n} cae al 8259")
            verificar(re.search(rf"autoprueba \(_PRT\): {n} \S+ GSI 2\d -> IRQ \d+: llega", s),
                      f"[amilo] con el _PRT {n} vuelve al IOAPIC por su GSI 20-23")
        infopc = funcionan(m, "amilo")
        verificar(re.search(r"EHCI\s+IOAPIC GSI 2\d \(_PRT\)", infopc), "[amilo] infopc: el EHCI por el _PRT")


def volcados():
    pasta = os.environ.get("SAGUI_ACPIDUMPS")
    if not pasta:
        print("  volcados: sin SAGUI_ACPIDUMPS, no se prueban volcados de máquinas reales")
        return
    import h158_bateria
    exe = h158_bateria.compilar_volcado()
    casos = (("Inspiron_6400", "945", ["00:1d.7 INTA EHCI", "00:1d.0 INTA UHCI 1", "00:1d.3 INTD UHCI 4"]),
             ("Satellite_L655", "hm55", ["00:1d.0 INTA EHCI 1", "00:1a.0 INTA EHCI 2"]))
    for nome, chip, ehci in casos:
        arqs = [os.path.join(pasta, f) for f in sorted(os.listdir(pasta)) if nome in f and f.endswith(".bin")]
        if not arqs:
            print(f"  volcados: no está el de {nome} en {pasta}")
            continue
        r = subprocess.run([exe, "--prt", chip, arqs[0]], capture_output=True, text=True, timeout=600)
        ok = r.returncode == 0 and "SIN COLGARSE" in r.stdout
        verificar(ok, f"[volcados] {nome}: uacpi_volcado --prt {chip} termina" + ("" if ok else "\n" + r.stdout[-2000:]))
        apic = r.stdout[r.stdout.find("modo APIC"):]
        for e in ehci:
            verificar(re.search(re.escape(e) + r"\s+-> \S+ -> GSI (1[6-9]|2[0-3]) nivel activa baja", apic),
                      f"[volcados] {nome}: {e} tiene GSI 16-23 en el modo APIC")
        c = re.search(r"coherencia PIRQ -> GSI 16-23: (\d+) de (\d+)", r.stdout)
        verificar(c and int(c.group(1)) == int(c.group(2)) > 10,
                  f"[volcados] {nome}: el PIRQ del modo PIC y la GSI del APIC coinciden ({c.group(0) if c else '?'})")


def main():
    partes = sys.argv[1:] or ["q35", "pc", "falla", "amilo", "volcados"]
    for p in partes:
        {"q35": q35, "pc": pc, "falla": falla, "amilo": amilo, "volcados": volcados}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
