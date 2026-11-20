#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H410 (D410) - más Wi-Fi integradas PCIe: familia Atheros ath9k, Broadcom BCM4313 y Ralink RT3090 (experimental).

QEMU no emula ninguna de estas tarjetas. Aquí:
  1. (anfitrión) compila tests/recursos/wifi_pcie_anfitriao.c con el MISMO kernel/rede/wifi_pcie_logica.h
     (ASan y UBSan): catálogo por identificador PCI, SREV, ROM «4K» del AR9285 y de la AR5416/AR9287
     construidas a mano, canales y palabra del sintetizador; el script recalcula por su cuenta el
     sintetizador de los 14 canales.
  2. (fuentes) el hook en wifi.c, el AR9485 intacto (ar9485.c y ar9485_logica.h sin tocar respecto a la base),
     la doc, D410 y el ROADMAP.
  3. (QEMU sin la tarjeta) arranca igual: ningún [WPCI], ningún [AR9485], «wifi» dice que no hay adaptador, la red sigue.
  4. (QEMU con una PCI ajena: pci-testdev) igual.
Uso: h410_wifi_pcie.py [anfitriao] [fuentes] [sin] [pci]
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar  # noqa: E402


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "wifi_pcie_anfitriao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-o", exe, os.path.join(RAIZ, "tests", "recursos", "wifi_pcie_anfitriao.c")],
                   check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and "TUDO OK" in linhas,
              f"lógica de la Wi-Fi PCIe en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones, ASan/UBSan)")
    n = 0
    for l in linhas:
        if l.startswith("VAL synth"):
            _, _, ch, freq, palavra = l.split()
            ch, freq, palavra = int(ch), int(freq), int(palavra, 16)
            esperada = 2484 if ch == 14 else 2407 + 5 * ch
            verificar(freq == esperada, f"canal {ch}: {freq} MHz")
            verificar(palavra == (((freq << 16) // 15) | 0x20000000 | 0x10000000) & 0xFFFFFFFF, f"sintetizador del canal {ch} igual que en Python")
            n += 1
    verificar(n == 14, "los 14 canales del sintetizador")
    verificar(any(l.startswith("VAL rom4k_mac 002268aabbcc") for l in linhas), "MAC de la ROM 4K leída bien")


def parte_fuentes():
    wifi = ler("kernel/rede/wifi.c")
    verificar("wifi_pcie_identificar();" in wifi and "ar9485_procurar()" in wifi.split("wifi_pcie_identificar();")[0][-200:],
              "wifi.c llama a la identificación solo cuando no hay AR9485")
    c = ler("kernel/rede/wifi_pcie.c")
    verificar("pci_escreve32" not in c and "mmio" not in c.lower(), "wifi_pcie.c no escribe nada en la tarjeta (solo lee la configuración PCI)")
    verificar("experimental" in c, "wifi_pcie.c marca las tarjetas como experimentales")
    # el AR9485 sigue exactamente igual
    base = subprocess.run(["git", "-C", RAIZ, "merge-base", "HEAD", "v8.5.4.10r2"], capture_output=True, text=True).stdout.strip()
    if base:
        for f in ("kernel/rede/ar9485.c", "kernel/rede/ar9485_logica.h", "kernel/inc/ar9485.h"):
            d = subprocess.run(["git", "-C", RAIZ, "diff", "--quiet", base, "--", f])
            verificar(d.returncode == 0, f"{f} sin cambios respecto a la base (el AR9485 del L850 no se toca)")
    doc = ler("docs/wifi-pcie.md")
    for t in ("AR9285", "BCM4313", "RT3090", "experimental", "LICENCE.broadcom_bcm43xx", "rt2800", "L635", "dv7"):
        verificar(t in doc, f"docs/wifi-pcie.md menciona «{t}»")
    dec = ler("docs/decisoes.md")
    verificar("## D410." in dec and "ath9k" in dec.split("## D410.")[1][:6000], "docs/decisoes.md: D410")
    verificar("wifi-pcie" in ler("docs/ROADMAP.md") or "D410" in ler("docs/ROADMAP.md"), "docs/ROADMAP.md: fila de la 8.5.5.1 con D410")
    ter = ler("docs/terceros.md")
    verificar("wifi_pcie_logica.h" in ter and "D410" in ter, "docs/terceros.md: fila de D410")


def sin_wpci(m):
    s = m.serial()
    verificar("[WPCI]" not in s, "ningún mensaje [WPCI]")
    verificar("[AR9485]" not in s and "[WIFI]" not in s, "ningún mensaje [AR9485] ni [WIFI]")


def parte_maquina(nombre, extras, imagem):
    with Maquina(nombre, "ide", imagem=imagem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        sin_wpci(m)
        out = m.comando("wifi")
        verificar("no hay ningún adaptador" in out, f"{nombre}: wifi dice que no hay adaptador")
        out = m.comando("ping -c 2 10.0.2.2", tempo=30)
        verificar("2 paquetes enviados, 2 recibidos" in out, f"{nombre}: la red sigue funcionando")


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "sin", "pci"]
    if "anfitriao" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    imagem = imagem_console() if ("sin" in partes or "pci" in partes) else None
    if "sin" in partes:
        parte_maquina("h410-sin", [], imagem)
    if "pci" in partes:
        parte_maquina("h410-pci", ["-device", "pci-testdev"], imagem)
    print("H410 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
