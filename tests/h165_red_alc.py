#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H165 (D200) - driver alc de las Ethernet Atheros AR813x/AR815x
(AR8152 v1.1, PCI 1969:2060, del Toshiba Satellite L635).

El QEMU no emula ninguna Atheros (ni la RTL8168 del r8169), así que la
tarjeta solo se prueba en hardware real. Aquí:
  1. (anfitrión) compila tests/recursos/alc_anfitriao.c con el MISMO
     kernel/rede/alc_logica.h: modelos, MAC de los PAR, formato del TPD y
     del RRD, resolución del enlace del PHY, MAC_CFG/TXQ/RXQ/DMA_CFG y el
     PM_CFG sin ASPM.
  2. (fuentes) el driver está enganchado: rede.c lo detecta como
     experimental, infopc conoce los seis modelos con el driver «alc».
  3. (QEMU, e1000 y RTL8139) la red sigue igual: la misma línea de
     arranque, DHCP, ping, ningún mensaje [ALC] y «ifconfig eth0
     experimental» no toca una tarjeta que ya funciona.
  4. (QEMU sin tarjeta) arranca igual, sin [ALC], y «ifconfig eth0
     experimental» dice que no hay tarjeta (no hay driver experimental que
     encender).

Uso: h165_red_alc.py [anfitriao] [fuentes] [e1000] [rtl8139] [sin]
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar  # noqa: E402


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "alc_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "alc_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and linhas and linhas[-1] == "TUDO OK",
              f"lógica del alc en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones)")


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def parte_fuentes():
    rede = ler("kernel/rede/rede.c")
    verificar("alc_procurar(&placa, false)" in rede and "procurar_experimental = alc_procurar" in rede,
              "rede.c detecta la Atheros como experimental")
    verificar("procurar_experimental(&placa, true)" in rede, "REDE_EXPERIMENTAL enciende el driver detectado")
    infopc = ler("userland/programas/infopc.c")
    for m in ("2060", "2062", "1073", "1083", "1063", "1062"):
        verificar(f'{{ 0x1969, 0x{m}, 0, 0, 0, EXPERIMENTAL, "alc" }}' in infopc, f"infopc: 1969:{m} experimental, driver alc")
    nomes = infopc + ler("userland/inc/pcinomes.h")   # los nombres PCI se movieron a pcinomes.h (76bf3cee)
    verificar('0x19692060' in nomes and 'AR8152 v1.1' in nomes, "infopc: nombre de la AR8152 v1.1")
    alc = ler("kernel/rede/alc.c")
    verificar("p->experimental = true;" in alc, "el alc siempre es experimental (sin probar en hardware)")
    verificar('p->nome = "alc";' in alc, "el driver se llama alc")
    regs = ler("kernel/terceros/bsd/alc/alc_regs.h")
    verificar(regs.startswith("// SPDX-License-Identifier: BSD-2-Clause") and "Pyun YongHyeon" in regs,
              "los registros de FreeBSD con su licencia BSD y su copyright")
    verificar("kernel/terceros/bsd/alc/" in ler("docs/terceros.md"), "docs/terceros.md registra el alc")


def sin_alc(m):
    s = m.serial()
    verificar("[ALC]" not in s, "ningún mensaje [ALC] en el arranque")
    verificar("sin activar (ifconfig eth0 experimental)" not in s, "no hay tarjeta experimental detectada")
    return s


def parte_tarjeta(modelo, imagem):
    with Maquina(f"h165-{modelo}", "ide", imagem=imagem, extras=["-nic", f"user,model={modelo}"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = sin_alc(m)
        nome = "e1000" if modelo == "e1000" else "rtl8139"
        verificar(f"[ ok ] Red: {nome} 52:54:00:12:34:56, 10.0.2.15 (DHCP)" in s,
                  f"{nome}: la misma línea de arranque, 10.0.2.15 por DHCP")
        verificar(f"[REDE] placa {nome}" in s, f"{nome}: la pila la enciende como siempre")
        out = m.comando("ifconfig")
        verificar(f"driver {nome}" in out and "(EXPERIMENTAL)" not in out, f"ifconfig: driver {nome}, no experimental")
        verificar("Descartes en la pila" not in out, "ifconfig sin el diagnóstico de los experimentales")
        out = m.comando("ifconfig eth0 experimental")
        verificar("Driver activado" not in out, "ifconfig eth0 experimental no toca una tarjeta que funciona")
        out = m.comando("ping -c 3 10.0.2.2", tempo=30)
        verificar("3 paquetes enviados, 3 recibidos, 0% perdidos" in out, f"{nome}: ping -c 3 10.0.2.2")
        out = m.comando("ifconfig")
        verificar(f"driver {nome}" in out and "IPv4 10.0.2.15/24" in out, f"{nome}: sigue igual después")
        sin_alc(m)


def parte_sin_tarjeta(imagem):
    with Maquina("h165-sin", "ide", imagem=imagem, extras=["-nic", "none"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = sin_alc(m)
        verificar("[ ok ] Red:" not in s, "sin tarjeta: sin línea de red en el arranque")
        out = m.comando("ifconfig eth0 experimental")
        verificar("Driver activado" not in out and "ifconfig:" in out, "sin tarjeta: ifconfig eth0 experimental da error")
        sin_alc(m)


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "e1000", "rtl8139", "sin"]
    if "anfitriao" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    imagem = None
    for p in ("e1000", "rtl8139", "sin"):
        if p in partes and imagem is None:
            imagem = imagem_console()
    if "e1000" in partes:
        parte_tarjeta("e1000", imagem)
    if "rtl8139" in partes:
        parte_tarjeta("rtl8139", imagem)
    if "sin" in partes:
        parte_sin_tarjeta(imagem)
    print("H165 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
