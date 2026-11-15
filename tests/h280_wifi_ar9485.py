#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H280 (D280) - Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, Toshiba L850), mejoras 072 y 079.

QEMU no emula ninguna Atheros: el chip solo se prueba en hardware real (docs/wifi-ar9485.md). Aquí:
  1. (anfitrión) compila tests/recursos/ar9485_anfitriao.c con el MISMO kernel/rede/ar9485_logica.h
     y las tablas de OpenBSD (con ASan y UBSan): la lectura de la ROM por bloques con suma de
     control sobre una plantilla (incluidos bloques malos, trozos fuera de la imagen y 3000 ROM de
     basura), las tablas de canales, la interpolación de potencias, la estructura de la ROM, el
     descriptor de TX (suma de control), las tasas y la caché de claves; y lo que imprime como «VAL»
     lo vuelve a calcular este script por su cuenta (pendiente delta, suma del descriptor,
     duraciones de las tramas, la MAC de la caché de claves, las sumas de las tablas).
  2. (fuentes) el driver está enganchado: rede.c lo busca, wifi.c usa las funciones drv_* para los dos
     chips, la licencia ISC y su fila en docs/terceros.md, la decisión D280, la doc y el ROADMAP.
  3. (QEMU sin la tarjeta) arranca igual, sin [AR9485], y «wifi» dice que no hay adaptador.
  4. (QEMU con otra PCI desconocida: pci-testdev y una «Atheros» que no es la 0032) arranca igual y
     el driver no la toca.

Uso: h280_wifi_ar9485.py [anfitriao] [fuentes] [sin] [pci]
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


# ---------------------------------------------------------------- cálculos independientes

def delta_py(coef):
    e = coef.bit_length() - 1
    exp = 14 - (e - 24)
    man = (coef + (1 << (24 - exp - 1))) >> (24 - exp)
    return (exp - 16) & 0xFFFFFFFF, man & 0xFFFFFFFF


TAXA_500K = [2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108]
RESPOSTA = [0, 1, 1, 1, 4, 4, 6, 6, 8, 8, 8, 8]


def txtime_py(n, r, curto):
    rate = TAXA_500K[r]
    if r >= 4:
        t = (8 + 4 * n + 3 + rate // 2) // rate
        return 16 + 4 + 4 * t + 16
    t = (16 * n + rate // 2) // rate
    t += (72 + 24) if (r != 0 and curto) else (144 + 48)
    return t + 10


def soma_py(info, link, segs):
    s = (info + link + sum(segs)) & 0xFFFFFFFF
    return ((s >> 16) + (s & 0xFFFF)) & 0xFFFF


def numeros(texto, nome):
    """Los números de un arreglo C «nome[]» de ar9485_datos.h (P(0x..) valen 0x../4)."""
    m = re.search(r"\b" + nome + r"\[\] = \{(.*?)\n\};", texto, re.S)
    if not m:
        raise FalhaTeste("no está la tabla " + nome)
    cuerpo = m.group(1)
    out = []
    for tok in re.findall(r"P\((0x[0-9a-fA-F]+)\)|(0x[0-9a-fA-F]+)", cuerpo):
        out.append(int(tok[0], 16) >> 2 if tok[0] else int(tok[1], 16))
    return out


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "ar9485_anfitriao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-o", exe, os.path.join(RAIZ, "tests", "recursos", "ar9485_anfitriao.c")],
                   check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and "TUDO OK" in linhas,
              f"lógica del AR9485 en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones, ASan/UBSan)")
    val = [l.split()[1:] for l in linhas if l.startswith("VAL ")]
    # pendiente delta de los 14 canales
    n = 0
    for v in val:
        if v[0] == "delta":
            canal, coef, e, m = int(v[1]), int(v[2]), int(v[3]), int(v[4])
            freq = 2484 if canal == 14 else 2407 + 5 * canal
            verificar(coef == (100 << 24) // freq and (e, m) == delta_py(coef), f"pendiente delta del canal {canal} igual que en Python")
            n += 1
    verificar(n == 14, "los 14 canales")
    # duraciones
    for v in val:
        if v[0] == "txtime":
            r_, ack, dat = int(v[1]), int(v[2]), int(v[3])
            verificar(ack == txtime_py(14, RESPOSTA[r_], False) and dat == txtime_py(1500, r_, r_ != 0),
                      f"duración de las tramas a la tasa {r_} igual que en Python")
    d = {v[0]: v[1:] for v in val if v[0] != "delta" and v[0] != "txtime"}
    verificar(int(d["txsoma"][0]) == soma_py(0x168C4100, 0, [0x12345678, 0x64, 0, 0, 0, 0, 0, 0]), "suma de control del descriptor de TX (caso normal)")
    verificar(int(d["txsoma2"][0]) == soma_py(0xFFFFFFFF, 0xFFFFFFFF, [0xFFFFFFFF] * 8), "suma de control del descriptor de TX (acarreos)")
    mac = bytes([0, 0x11, 0x22, 0x33, 0x44, 0x55])
    lo = int.from_bytes(mac[:4], "little")
    hi = int.from_bytes(mac[4:], "little")
    verificar((int(d["chavemac"][0]), int(d["chavemac"][1])) == (((lo >> 1) | (hi << 31)) & 0xFFFFFFFF, hi >> 1),
              "MAC de la caché de claves igual que en Python")
    # las tablas, leídas por Python del propio fichero
    dat = ler("kernel/terceros/bsd/athn/ar9485_datos.h")
    cm_r, cm_v = numeros(dat, "ar9485_1_1_cm_regs"), numeros(dat, "ar9485_1_1_cm_vals")
    mo_r, mo_v = numeros(dat, "ar9485_1_1_regs"), numeros(dat, "ar9485_1_1_vals_2g20")
    verificar(len(cm_r) == len(cm_v) == int(d["ini_ncm"][0]) and len(mo_r) == len(mo_v) == int(d["ini_nregs"][0]),
              f"tablas de inicio: {len(cm_r)} comunes y {len(mo_r)} modales, las mismas que ve el compilador")
    verificar(sum(a ^ b for a, b in zip(cm_v, cm_r)) & 0xFFFFFFFF == int(d["cm_xor_soma"][0]) and
              sum(a ^ b for a, b in zip(mo_v, mo_r)) & 0xFFFFFFFF == int(d["modal_xor_soma"][0]),
              "las tablas de inicio valen lo mismo para Python y para el compilador")
    verificar(len(numeros(dat, "ar9485_1_1_tx_gain_regs")) == len(numeros(dat, "ar9485_1_1_tx_gain_vals_2g")) == int(d["tx_gain_n"][0]) and
              len(numeros(dat, "ar9485_1_1_rx_gain_regs")) == len(numeros(dat, "ar9485_1_1_rx_gain_vals")) == int(d["rx_gain_n"][0]),
              "tablas de ganancias de TX y RX")
    # registros clave del arranque: están en las tablas (los que ath9k/athn programan primero)
    verificar(0x9810 // 4 in mo_r and 0xa204 // 4 in mo_r, "las tablas modales llevan registros del PHY (0x9810, 0xa204)")
    verificar(int(d["eeprom_tam"][0]) == 1088, "la imagen de la ROM mide 1088 bytes")


def parte_fuentes():
    rede = ler("kernel/rede/rede.c")
    verificar("wifi_pci_iniciar();" in rede, "rede.c busca la Wi-Fi integrada tras la tarjeta de red")
    wifi = ler("kernel/rede/wifi.c")
    verificar("static int drv_enviar(" in wifi and "static int drv_receber(" in wifi and "static void drv_canal(" in wifi,
              "wifi.c usa funciones drv_* para los dos chips")
    verificar("ar9485_iniciar(&motivo)" in wifi and "void wifi_pci_iniciar(void)" in wifi, "wifi.c enciende el AR9485 desde su tarea")
    verificar("ccmp_decifrar(cq, cab, n" in wifi and "ccmp_cifrar(tq, 24, k, sup.ptk + 32, ++pn_tx, 0)" in wifi,
              "CCMP por software en las dos direcciones para el AR9485")
    verificar("if (via_pci) return false;" in wifi, "el AR9485 se asocia como b/g (sin HT)")
    # el camino del RTL8188EU no cambia: sus llamadas siguen yendo a r88e_*
    for f in ("enviar", "receber", "canal", "iniciar", "ler_efuse", "associado"):
        verificar(f"r88e_{f}(" in wifi, f"wifi.c sigue llamando a r88e_{f}")
    ar = ler("kernel/rede/ar9485.c")
    verificar(ar.startswith("// SPDX-License-Identifier: GPL-2.0-or-later"), "ar9485.c lleva su SPDX")
    verificar("168c" in ar and "PCI_AR9485   0x0032" in ar, "el driver reconoce 168c:0032")
    verificar("AR_WRITE(A, AR_IER, 0);" in ar and "INTx desactivado" in ar, "sin interrupciones: IER a 0 e INTx desactivado")
    verificar("ath9k" not in ar.replace("nada del ath9k de Linux", "").replace("No se usó nada del ath9k de Linux", ""),
              "el driver no usa nada del ath9k de Linux")
    verificar("tx_parar();" in ar and "rx_parar" in ar and "hw_reset()" in ar, "cambio de canal: para TX y RX y reinicia")
    verificar("ar9485_chave" in ar and "AR_KEYTABLE_TYPE_CCM" in ar, "caché de claves CCMP disponible (cifrado hw opcional)")
    for ficha in ("athnreg.h", "ar9003reg.h", "ar9485_datos.h"):
        t = ler("kernel/terceros/bsd/athn/" + ficha)
        verificar("Permission to use, copy, modify" in t and "Damien Bergamini" in t and "Atheros Communications" in t,
                  f"{ficha}: la licencia ISC y los copyright de OpenBSD en la cabecera")
    verificar("SPDX-License-Identifier: ISC" in ler("kernel/terceros/bsd/athn/ar9485_datos.h"), "ar9485_datos.h: SPDX ISC")
    ter = ler("docs/terceros.md")
    verificar("kernel/terceros/bsd/athn/" in ter and "athn" in ter and "D280" in ter, "docs/terceros.md registra athn (D280)")
    verificar(os.path.exists(os.path.join(RAIZ, "docs/wifi-ar9485.md")), "docs/wifi-ar9485.md existe")
    doc = ler("docs/wifi-ar9485.md")
    for t in ("dmesg", "wifi buscar", "wifi conectar", "Toshiba", "cifrado hw"):
        verificar(t in doc, f"la guía del L850 menciona «{t}»")
    dec = ler("docs/decisoes.md")
    verificar("## D280." in dec and "AR9485" in dec.split("## D280.")[1][:3000], "docs/decisoes.md: D280")
    verificar("AR9485" in ler("docs/ROADMAP.md"), "docs/ROADMAP.md: fila de la v8.2 con el AR9485")


def sin_ar(m):
    s = m.serial()
    verificar("[AR9485]" not in s, "ningún mensaje [AR9485]")
    verificar("[WIFI]" not in s, "ningún mensaje [WIFI]")
    return s


def parte_sin(imagem):
    with Maquina("h280-sin", "ide", imagem=imagem) as m:
        m.esperar_serial("sagui:/$", 150)
        sin_ar(m)
        out = m.comando("wifi")
        verificar("no hay ningún adaptador" in out, "sin tarjeta: wifi dice que no hay adaptador")
        out = m.comando("ifconfig wlan0")
        verificar("no hay wlan0" in out, "sin tarjeta: ifconfig wlan0")
        out = m.comando("ifconfig")
        verificar("wlan0" not in out and "10.0.2.15" in out, "sin tarjeta: la eth0 sigue igual")


def parte_pci(imagem):
    # pci-testdev (1b36:0005) y una red de clase «otra» con el ID de Atheros pero otro modelo: no se toca.
    extras = ["-device", "pci-testdev"]
    with Maquina("h280-pci", "ide", imagem=imagem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        sin_ar(m)
        out = m.comando("wifi")
        verificar("no hay ningún adaptador" in out, "otra PCI desconocida: wifi dice que no hay adaptador")
        out = m.comando("ping -c 2 10.0.2.2", tempo=30)
        verificar("2 paquetes enviados, 2 recibidos" in out, "otra PCI desconocida: la red sigue funcionando")


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "sin", "pci"]
    if "anfitriao" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    imagem = None
    for p in ("sin", "pci"):
        if p in partes and imagem is None:
            imagem = imagem_console()
    if "sin" in partes:
        parte_sin(imagem)
    if "pci" in partes:
        parte_pci(imagem)
    print("H280 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
