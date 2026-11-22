#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H433 (D433) - Ethernet USB Realtek RTL8152, RTL8153, RTL8153B, RTL8156 y RTL8156B
(kernel/rede/ure.c, experimental, sin probar en hardware).

  1. anfitrion: kernel/rede/ure_logica.h con ASan y UBSan (tests/recursos/ure_anfitriao.c):
     identificación del chip, tabla de VID:PID, secuencias de registros de cada familia
     contra un chip simulado, arranque sin respuesta (plazos acotados y paso dicho),
     recepción agregada (tramas, alineación, errores, ruido al azar) y trama de envío;
  2. fuentes: enganches (usb.c, usb.h, rede.c, rede.h), cabeceras de terceros con su
     aviso de copyright, tablas generadas, que no entra código GPL-2 ni firmware, y
     la documentación (decisión D433, terceros.md, docs/ure.md);
  3. QEMU sin adaptador: arranca igual y no aparece ningún mensaje [URE].
     QEMU no emula este chip: la prueba real es la guía de docs/ure.md (Amilo).

Uso: h433_ure.py [anfitrion] [fuentes] [sin]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar  # noqa: E402


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "ure_anfitriao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"),
                    "-o", exe, os.path.join(RAIZ, "tests", "recursos", "ure_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("  " + l)
    verificar(r.returncode == 0 and "ure_anfitriao ok" in r.stdout,
              "lógica del Ethernet USB Realtek (C, ASan/UBSan): " + (r.stdout + r.stderr)[-300:])
    n = sum(1 for l in r.stdout.splitlines() if l.startswith("ok "))
    print("  %d comprobaciones en C" % n)


def parte_fuentes():
    usb_c, usb_h = ler("kernel/usb/usb.c"), ler("kernel/inc/usb.h")
    verificar("ure_usb_reconhecer(d)" in usb_c and "ure_usb_retirado(d)" in usb_c,
              "usb.c llama a ure_usb_reconhecer() al enumerar y a ure_usb_retirado() al retirar")
    verificar("bool      ure;" in usb_h and "ure_usb_reconhecer(usb_disp_t *d);" in usb_h,
              "usb.h: campo ure en usb_disp_t y prototipos")
    rede_c, rede_h = ler("kernel/rede/rede.c"), ler("kernel/inc/rede.h")
    verificar("rede_usb_eth_registrar(placa_t *p)" in rede_c and "rede_arrancou = true;" in rede_c and "ativar_usb();" in rede_c,
              "rede.c: el USB se activa al llegar a rede_iniciar() y en caliente")
    verificar("rede_usb_eth_registrar(placa_t *p);" in rede_h, "rede.h declara rede_usb_eth_registrar()")
    # D445: el USB es una interfaz más (eth0 sin PCI, eth1 detrás de ella), a la vez que la PCI y la Wi-Fi
    verificar("if_nova_eth(IF_USB)" in rede_c and "usb_if = f;" in rede_c and "iface_remover(usb_if);" in rede_c,
              "rede.c: el USB tiene su propia interfaz y al retirarlo se quita (la ruta pasa a otra)")
    ure_c = ler("kernel/rede/ure.c")
    verificar("GPL-3.0-only" in ure_c and "EXPERIMENTAL" in ure_c and "[URE]" in ure_c,
              "ure.c: cabecera SPDX, aviso experimental y prefijo [URE] en el registro")
    for pieza in ("ure_nome_passo(", "rede_usb_eth_registrar(&U.placa)", "usb_rx_abrir(d, U.rx_fis, URE_BUF_RX)"):
        verificar(pieza in ure_c, "ure.c usa " + pieza)
    logica = ler("kernel/rede/ure_logica.h")
    verificar("ure_rx_siguiente(" in logica and "ure_tx_montar(" in logica and "ure_iniciar_chip(" in logica,
              "ure_logica.h tiene la recepción, el envío y el inicio del chip")
    for f in ("kernel/rede/ure_logica.h", "kernel/rede/ure_ids.h", "kernel/rede/ure.c", "kernel/terceros/bsd/ure/ureg.h",
              "tests/recursos/ure_anfitriao.c", "tools/gerar_ure_ids.py", "tools/gerar_ureg.py", "docs/ure.md"):
        verificar(os.path.exists(os.path.join(RAIZ, f)), "existe " + f)
    cab = ler("kernel/terceros/bsd/ure/ureg.h")
    verificar("Kevin Lo" in cab and "Redistribution and use in source and binary forms" in cab,
              "ureg.h: aviso de copyright de OpenBSD (Kevin Lo) y su licencia conservados")
    verificar("URE_RXPKT_LEN_MASK" in cab and "GENERADO por" in cab, "ureg.h: registros extraídos y marcados como generados")
    ids = ler("kernel/rede/ure_ids.h")
    pares = set((int(a, 16), int(b, 16)) for a, b in re.findall(r"\{ 0x([0-9A-F]{4}), 0x([0-9A-F]{4}) \}", ids))
    verificar(len(pares) >= 50 and (0x0BDA, 0x8152) in pares and (0x0BDA, 0x8153) in pares and (0x0BDA, 0x8156) in pares,
              "ure_ids.h: %d VID:PID, con 0bda:8152, 8153 y 8156" % len(pares))
    wifi = ler("kernel/rede/wifi_usb_ids.h")
    wifi_pares = set((int(a, 16), int(b, 16)) for a, b in re.findall(r"\{ 0x([0-9A-Fa-f]{4}), 0x([0-9A-Fa-f]{4}) \}", wifi))
    verificar(not (pares & wifi_pares), "ningún VID:PID de Ethernet USB está en las tablas del Wi-Fi USB")
    for malo in ("r8152", "rge-rtl", "rtl8153a_", "GPL-2.0"):
        verificar(malo not in ure_c.lower() and malo not in logica.lower(),
                  "sin código ni firmware de Linux/Realtek GPL: no aparece «%s»" % malo)
    verificar("ure_loadfirmware" not in logica and "firmware" not in ure_c.lower(),
              "no necesita firmware (solo el 8157/8159, fuera de alcance)")
    ter = ler("docs/terceros.md")
    for k in ("bsd/ure/ureg.h", "ure_ids.h", "if_urereg.h", "usbdevs.h", "D433"):
        verificar(k in ter, "docs/terceros.md registra " + k)
    dec = ler("docs/decisoes.md")
    verificar("## D433." in dec and "ure.md" in dec, "D433 en docs/decisoes.md")
    guia = ler("docs/ure.md")
    verificar("Cómo probarlo" in guia and "0bda:8156" in guia and "[URE]" in guia and "experimental" in guia.lower(),
              "docs/ure.md: guía para el Amilo y qué buscar en el registro")
    comp = ler("docs/compatibilidad.md")
    verificar("RTL8156" in comp or "RTL8153" in comp, "docs/compatibilidad.md menciona los Ethernet USB Realtek")


def parte_sin(imagem):
    with Maquina("h433-sin", "ide", imagem=imagem) as m:
        m.esperar_serial("sagui:/$", 150)
        out = m.comando("ifconfig")
        verificar("eth0" in out or "lo" in out, "sin adaptador USB: ifconfig responde")
        verificar("[URE]" not in m.serial(), "sin adaptador: ningún mensaje [URE] en el registro")


def main():
    partes = sys.argv[1:] or ["anfitrion", "fuentes", "sin"]
    if "anfitrion" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    if "sin" in partes:
        parte_sin(imagem_console())
    print("H433 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
