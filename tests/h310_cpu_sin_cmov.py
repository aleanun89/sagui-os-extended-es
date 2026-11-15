#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H310 - CPU sin CMOV: aviso claro y máquina parada, sin reinicios (D310, issue #35).

El kernel y los programas se compilan para i686 y llevan CMOV/FCMOV. En una i586 (AMD K6,
Pentium, Pentium MMX) la primera CMOV daba #UD, triple fallo y un reinicio en bucle justo
después de «[SAGUI] compilacion». Ahora la etapa 2 lo comprueba antes de cargar nada.

Criterio:
  - con -cpu pentium (sin CMOV) y -cpu 486 (sin CMOV; según la versión del QEMU, con o sin
    CPUID), la etapa 2 escribe en pantalla «Esta CPU no es i686» y lo que hace falta; el
    kernel no llega a arrancar (nada en el serie) y, 30 s después, la máquina sigue encendida
    (el QEMU se lanza con -no-reboot: un reinicio lo cerraría) con el mismo aviso;
  - con -cpu pentium2 (la i686 más antigua del QEMU) arranca como siempre, sin el aviso;
  - por UEFI de 32 bits (OVMF IA32) con -cpu pentium, si el OVMF llega a arrancar el
    cargador, el cargador escribe el mismo aviso (sale por el serie, que el OVMF replica)
    y tampoco reinicia. Si el propio OVMF no funciona en esa CPU, se dice y no cuenta.
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, achar_ovmf, FalhaTeste  # noqa: E402

AVISO = "Esta CPU no es i686"
REQUISITO = "SaguiOS necesita un Pentium Pro / Pentium II / AMD Athlon o posterior"
NO_COMPAT = "K6-III y los Pentium/Pentium MMX no son compatibles"
ESPERA = 30


def junto(texto):
    """Sin espacios: el aviso ocupa varias líneas de 80 columnas y se corta en cualquier sitio."""
    return "".join(texto.split())


def sin_cmov(cpu, rot, captura=None):
    with Maquina(f"h310{rot}", "ide", extras=["-cpu", cpu]) as m:
        m.esperar_tela(AVISO, 60)
        tela = m.tela_texto()
        print("\n".join("    | " + l for l in tela if l.strip()))
        texto = junto("".join(l.ljust(80) for l in tela))
        verificar(junto("SaguiOS - etapa 2") in texto, f"[{cpu}] la etapa 2 arrancó")
        verificar(junto(REQUISITO) in texto and junto(NO_COMPAT) in texto, f"[{cpu}] el aviso dice qué CPU hace falta")
        verificar("[1/6]" not in texto, f"[{cpu}] se para antes de leer el kernel ([1/6] no aparece)")
        if junto("no tiene CPUID") in texto:
            print(f"    [{cpu}] causa: sin CPUID")
        else:
            verificar(junto("falta la instrucci") in texto, f"[{cpu}] la causa es la falta de CMOV")
        if captura:
            m.captura(captura)
        time.sleep(ESPERA)
        verificar(m.proc.poll() is None, f"[{cpu}] {ESPERA} s después la máquina sigue encendida (sin reinicios)")
        verificar(m.serial_bruto().count("[SAGUI]") == 0, f"[{cpu}] el kernel no llegó a arrancar (0 arranques en el serie)")
        verificar(junto(AVISO) in junto("".join(l.ljust(80) for l in m.tela_texto())), f"[{cpu}] el aviso sigue en pantalla")


def con_cmov(cpu):
    with Maquina("h310p2", "ide", extras=["-cpu", cpu]) as m:
        m.esperar_serial("[SAGUI] boot ok", 300)
        verificar(m.serial_bruto().count("[SAGUI] boot ok") == 1, f"[{cpu}] arranca una vez, como siempre")
        verificar(AVISO not in m.serial_bruto(), f"[{cpu}] sin aviso de CPU")


def uefi_ia32_sin_cmov():
    if not achar_ovmf("ia32"):
        print("    UEFI IA32: no hay OVMF de 32 bits; se omite")
        return
    with Maquina("h310ue", "ide", firmware="ia32", extras=["-cpu", "pentium"]) as m:
        fim = time.time() + 120
        while time.time() < fim and AVISO not in m.serial_bruto() and m.proc.poll() is None:
            time.sleep(0.5)
        s = m.serial_bruto()
        if AVISO not in s:
            if "SaguiOS - cargador UEFI" in s:
                raise FalhaTeste("UEFI IA32: el cargador arrancó pero no avisó de la CPU")
            print("    UEFI IA32: el OVMF no llega al cargador con -cpu pentium; no cuenta")
            return
        verificar(REQUISITO in s.replace("\r\n", " ").replace("\n", " "), "[UEFI IA32 pentium] el cargador avisa")
        time.sleep(ESPERA)
        verificar(m.proc.poll() is None, f"[UEFI IA32 pentium] {ESPERA} s después sigue encendida")
        verificar("[SAGUI]" not in m.serial_bruto(), "[UEFI IA32 pentium] el kernel no llegó a arrancar")


def main():
    sin_cmov("pentium", "p5", captura="H310-cpu-sin-cmov")
    sin_cmov("486", "486")
    con_cmov("pentium2")
    uefi_ia32_sin_cmov()
    return 0


if __name__ == "__main__":
    sys.exit(main())
