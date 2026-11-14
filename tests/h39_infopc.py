#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H39 - infopc: información del PC al estilo CPU-Z (D041).

Criterio: `infopc -t` en QEMU muestra el fabricante y el nombre comercial de la
CPU, el hipervisor TCG de QEMU, una frecuencia medida > 0, el mapa E820 con
regiones usables, SMBIOS con "QEMU" como fabricante del sistema, el host
bridge i440FX (8086:1237) y la VGA de Bochs (1234:1111) en el PCI y el vídeo a
1024x768. Con `-cpu pentium2` desaparece SSE de la lista de instrucciones y con
`-cpu core2duo -smp 2` aparecen SSSE3 y dos hilos lógicos. La versión gráfica
abre su ventana en el escritorio y cambia de pestaña.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def instrucoes(out):
    m = re.search(r"Instrucciones:\n((?:  .*\n)+)", out)
    return set(m.group(1).split()) if m else set()


KVM = os.environ.get("SAGUI_KVM") == "1"


def main():
    console = imagem_console()
    with Maquina("h39", "ide", imagem=console, memoria="512M") as m:   # D172: la prueba espera 512 MB (SMBIOS), también suelta
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("infopc -t", 120)
        fab = re.search(r"^Fabricante:\s+(\S+)", out, re.M)
        # D172: con KVM la CPU es la del anfitrión (Intel o AMD), no la qemu32 de TCG.
        valido = ("GenuineIntel", "AuthenticAMD") if KVM else ("GenuineIntel",)
        verificar(fab and fab.group(1) in valido, f"CPU: fabricante {fab.group(1) if fab else '?'}")
        nome = re.search(r"^Nombre comercial:\s+(.+)$", out, re.M)
        verificar(nome and "QEMU" in nome.group(1), f"CPU: nombre comercial {nome.group(1) if nome else '?'}")
        verificar(re.search(r"^Familia/modelo:\s+familia 6 .*modelo 6 ", out, re.M), "CPU: familia 6, modelo 6 (qemu32)")
        if KVM:
            verificar('"KVMKVMKVM' in out and "= KVM" in out, "CPU: hipervisor KVM detectado")
        else:
            verificar('"TCGTCGTCGTCG" = QEMU' in out, "CPU: hipervisor TCG de QEMU detectado")
        mhz = re.search(r"^Frecuencia \(TSC\):\s+(\d+),(\d+) MHz", out, re.M)
        verificar(mhz and int(mhz.group(1)) > 0, f"CPU: frecuencia medida {mhz.group(1) if mhz else '?'} MHz")
        ins = instrucoes(out)
        verificar({"FPU", "MMX", "SSE", "SSE2"} <= ins, f"CPU: instrucciones {' '.join(sorted(ins))}")
        # Con KVM, el qemu32 sobre un AMD no informa de cachés (sin hoja 4 ni 0x80000005).
        verificar(re.search(r"^  L1d\s+\d+ KB", out, re.M) or (KVM and "la CPU no informa las cach" in out),
                  "CPU: caché L1d (hoja 4)")
        verificar(len(re.findall(r"^  0x[0-9a-f]{16}  0x[0-9a-f]{16} .*usable \(libre\)", out, re.M)) >= 2,
                  "Memoria: E820 con regiones usables")
        verificar(re.search(r"^Gestionada por el kernel:\s+\d+ MB", out, re.M) and "D008" in out,
                  "Memoria: gestionada por el kernel y nota sobre el límite")
        verificar(re.search(r"^SMBIOS \d+\.\d+ ", out, re.M), "SMBIOS: tabla encontrada")
        verificar(re.search(r"Sistema \(tipo 1\)\n\s+Fabricante:\s+QEMU", out), "SMBIOS: fabricante del sistema QEMU")
        verificar(re.search(r"BIOS \(tipo 0\)\n\s+Fabricante:\s+SeaBIOS", out), "SMBIOS: BIOS SeaBIOS")
        verificar(re.search(r"Módulos de memoria \(tipo 17\)\n.*512 MB", out), "SMBIOS: módulo de 512 MB")
        verificar("omitido" in out, "SMBIOS: el número de serie no se muestra")
        verificar(re.search(r"^  00:00\.0  8086:1237  Puente host .*Intel", out, re.M), "PCI: host bridge i440FX 8086:1237")
        verificar(re.search(r"^  00:02\.0  1234:1111  Vídeo VGA", out, re.M), "PCI: VGA 1234:1111")
        verificar(re.search(r"^Resolución:\s+1024x768, 32 bits", out, re.M), "Vídeo: 1024x768, 32 bits")
        verificar(re.search(r"^VBE:\s+versión [23]\.\d", out, re.M), "Vídeo: información del VBE")
        verificar(re.search(r"^Teclado:\s+ES", out, re.M) and re.search(r"^Procesos:\s+\d+", out, re.M),
                  "Sistema: teclado y procesos")
        verificar("terminou com codigo 0" in out, "infopc -t termina sin errores")
        m.captura("H39-infopc-texto")

        out = m.comando("infopc -t pci")
        verificar("== PCI ==" in out and "== CPU ==" not in out, "infopc -t pci muestra solo una sección")
        out = m.comando("infopc -t nada")
        verificar("Uso: infopc" in out, "sección desconocida muestra el uso")
        out = m.comando("infopc")
        verificar("== CPU ==" in out and "== Sistema ==" in out, "sin escritorio, infopc imprime el informe")

    with Maquina("h39p2", "ide", imagem=console, extras=["-cpu", "pentium2"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("infopc -t cpu", 120)
        ins = instrucoes(out)
        verificar("MMX" in ins and "SSE" not in ins and "SSE2" not in ins,
                  f"-cpu pentium2: sin SSE ({' '.join(sorted(ins))})")
        # Con KVM el fabricante es el del anfitrión (AMD): no hay "Pentium II" ni "Core (Merom)".
        verificar("modelo 5 " in out and (KVM or "Pentium II" in out), "-cpu pentium2: familia 6 modelo 5, Pentium II")

    with Maquina("h39c2", "ide", imagem=console, extras=["-cpu", "core2duo", "-smp", "2"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("infopc -t cpu", 120)
        ins = instrucoes(out)
        verificar({"SSE3", "SSSE3"} <= ins, f"-cpu core2duo: con SSSE3 ({' '.join(sorted(ins))})")
        verificar("Core 2 Duo" in out.replace("(TM)2", " 2") or "Core(TM)2 Duo" in out, "-cpu core2duo: nombre comercial")
        verificar(KVM or "Core (Merom)" in out, "-cpu core2duo: microarquitectura Core (Merom)")
        verificar("2 hilo(s)" in out, "-smp 2: dos hilos lógicos")

    with Maquina("h39g", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("infopc")
        m.esperar_serial("infopc: janela aberta", 30)
        s = m.serial()
        g = re.findall(r'criada por \d+: "Información del PC" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        verificar(g, "la ventana \"Información del PC\" se abre")
        verificar("infopc: secao cpu" in s, "empieza en la pestaña CPU")
        ox, oy = int(g[-1][2]) + 2, int(g[-1][3]) + 24
        time.sleep(0.5)
        m.captura("H39-infopc-cpu")
        m.clique(ox + 10 + 3 * 106 + 50, oy + 20)   # pestaña PCI (abas de 100 px, D093)
        m.esperar_serial("infopc: secao pci", 15)
        verificar(True, "clic en la pestaña PCI")
        time.sleep(0.5)
        m.captura("H39-infopc-pci")
        m.digitar("3", 0.08)                         # tecla 3: Placa/BIOS
        m.esperar_serial("infopc: secao placa", 15)
        verificar(True, "tecla 3 cambia a Placa/BIOS")
        m.tecla("esc")
        m.esperar_serial("(infopc) terminou com codigo 0", 15)
        verificar(m.serial().count("(infopc) terminou com codigo 0") >= 2, "la ventana se cierra con Esc sin errores")
    return 0


if __name__ == "__main__":
    sys.exit(main())
