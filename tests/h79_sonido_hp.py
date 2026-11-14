#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H79 (D107) - ajustes de códecs HDA por códec y subsistema, y el programa «sonido».

anfitrion  kernel/dispositivos/hda_ajustes.c compilado con gcc y probado con
           identificadores simulados: el IDT 92HD75B del HP dv7 (111d:7603,
           subsistema 103c:3063) recibe el ajuste «idt71-hp» (GPIO0 encendido,
           puertos A y F como auriculares); las filas dv4/dv5/dv6/HDX/mini y la
           placa de referencia de la tabla de Linux; el subsistema del códec
           cuando el PCI no coincide; el 92HD75B2X5 (7608) con sus pines; y
           ningún ajuste para otros códecs (el de QEMU, Realtek).
hda        QEMU intel-hda + hda-duplex: sin ajuste (el códec de QEMU no está
           en la tabla) y el sonido sigue igual; «sonido» muestra el resumen,
           «--codec» vuelca los nodos, «--verbo» lee el identificador, los
           cambios a mano (--gpio, --pin, --eapd, --auto) responden y «--tono»
           se graba en el .wav: 100 Hz y 1000 Hz en los dos canales, después
           1000 Hz solo a la izquierda y solo a la derecha.
ac97       con AC'97, «sonido» dice que no hay HDA (y --tono sigue sonando).

Uso: h79_sonido_hp.py [anfitrion] [hda] [ac97]
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA  # noqa: E402
import h70_sonido as h70  # noqa: E402

HARNESS = r"""
#include "hda_ajustes.h"
int printf(const char *, ...);
static void caso(const char *nome, unsigned codec, unsigned pci, unsigned cod, unsigned gpio)
{
    hda_ajuste_t a;
    bool r = hda_ajuste_escolher(codec, pci, cod, gpio, &a);
    printf("%s=%d %s %04x gpio %02x/%02x/%02x hp %d", nome, r, a.nome ? a.nome : "-", a.ssid & 0xffff,
           a.gpio_mascara, a.gpio_direcao, a.gpio_dados, a.hp_auriculares);
    for (int i = 0; i < a.n_pinos; i++) printf(" pino %02x:%08x", a.pino_no[i], a.pino_config[i]);
    printf(" desmutar");
    for (int i = 0; i < a.n_desmutar; i++) printf(" %02x", a.nos_desmutar[i]);
    printf(" vol %02x cmos %x mapa %x\n", a.no_volume, a.verbo_gpio_cmos, a.verbo_mapa_energia);
}
int main(void)
{
    caso("dv7", 0x111d7603, 0x103c3063, 0x103c3063, 8);
    caso("dv6", 0x111d7603, 0x103c3061, 0, 8);
    caso("dv6_4gpio", 0x111d7603, 0x103c3061, 0, 4);
    caso("dv4", 0x111d76b2, 0x103c30f5, 0, 8);
    caso("dv5", 0x111d76b2, 0x103c3603, 0, 8);
    caso("hdx", 0x111d7608, 0x103c3610, 0, 8);
    caso("mini", 0x111d76b2, 0x103c361a, 0, 8);
    caso("ref", 0x111d76b0, 0x80862668, 0, 8);
    caso("so_codec", 0x111d7603, 0x17aa1234, 0x103c7012, 8);
    caso("dell", 0x111d76b2, 0x10280233, 0x10280233, 8);
    caso("b5x", 0x111d76b6, 0, 0, 8);
    caso("qemu", 0x1af40022, 0x1af41100, 0x1af41100, 0);
    caso("realtek", 0x10ec0662, 0x103c3063, 0, 2);
    printf("fones_lin=%d fones_alt=%d fones_nada=%d fones_hp=%d conv=%08x\n", hda_config_saida_fones(0x02014040),
           hda_config_saida_fones(0x90170110), hda_config_saida_fones(0x40f000f0), hda_config_saida_fones(0x0421101f),
           hda_config_para_fones(0x02014040));
    return 0;
}
"""

COMUM = "vol 28 cmos 7e7 mapa 7ec"
ESPERADO = {
    # o HP dv7 do informe: só a linha genérica de HP do Linux (GPIO0 = EAPD)
    "dv7": "1 idt71-hp 3063 gpio 01/01/01 hp 1 desmutar 0f 0a 0d " + COMUM,
    "dv6": "1 idt71-hp-dv5 3061 gpio 21/21/21 hp 1 pino 0d:90170010 desmutar 0f 0a 0d " + COMUM,
    "dv6_4gpio": "1 idt71-hp-dv5 3061 gpio 01/01/01 hp 1 pino 0d:90170010 desmutar 0f 0a 0d " + COMUM,
    "dv4": "1 idt71-hp-dv4 30f5 gpio 21/21/21 hp 1 pino 0d:90170010 desmutar 0f 0a 0d " + COMUM,
    "dv5": "1 idt71-hp-dv5 3603 gpio 21/21/21 hp 1 pino 0d:90170010 desmutar 0f 0a 0d " + COMUM,
    "hdx": "1 idt71-hp-hdx 3610 gpio 09/09/09 hp 1 pino 0f:40f000f0 pino 19:40f000f3 desmutar 0a 0d " + COMUM,
    "mini": "1 idt71-hp-m4 361a gpio 03/01/01 hp 1 desmutar 0f 0a 0d " + COMUM,
    "ref": "1 idt71-ref 2668 gpio 00/00/00 hp 0 desmutar 0f 0a 0d " + COMUM,
    "so_codec": "1 idt71-hp-dv5 7012 gpio 21/21/21 hp 1 pino 0d:90170010 desmutar 0f 0a 0d " + COMUM,
    "dell": "1 idt71 0000 gpio 01/01/01 hp 0 desmutar 0f 0a 0d " + COMUM,
    "b5x": "1 idt71 0000 gpio 01/01/01 hp 0 desmutar 0a 0d " + COMUM,
    "qemu": "0 - 0000 gpio 00/00/00 hp 0 desmutar vol 00 cmos 0 mapa 0",
    "realtek": "0 - 0000 gpio 00/00/00 hp 0 desmutar vol 00 cmos 0 mapa 0",
}


def anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    d = tempfile.mkdtemp(dir=SAIDA, prefix="h79-")
    c, exe = os.path.join(d, "harness.c"), os.path.join(d, "harness")
    with open(c, "w") as f:
        f.write(HARNESS)
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe, c,
                        os.path.join(RAIZ, "kernel", "dispositivos", "hda_ajustes.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, f"hda_ajustes.c compila no anfitrião {r.stderr.strip()[:400]}")
    saida = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    obtido = {}
    for linha in saida.strip().splitlines():
        if linha.startswith("fones_"):
            continue
        k, v = linha.split("=", 1)
        obtido[k] = v.strip()
    erros = [f"{k}: «{obtido.get(k)}» != «{v}»" for k, v in ESPERADO.items() if obtido.get(k) != v]
    verificar(not erros, f"ajuste escolhido para {len(ESPERADO)} códecs/subsistemas simulados {erros}")
    verificar("fones_lin=1 fones_alt=0 fones_nada=0 fones_hp=1 conv=0221401f" in saida,
              "saída de linha/fones reconhecida e convertida em fones como no Linux")


def hda():
    with Maquina("h79-hda", "ide", imagem=imagem_console(), som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar(re.search(r"\[HDA\] codec \d+: [0-9a-f]{8} rev .*ajuste nenhum", s) is not None,
                  "o códec do QEMU fica sem ajuste (log do arranque)")
        out = m.comando("sonido")
        verificar("Codec HDA" in out and "Ajuste: ninguno" in out and re.search(r"Pines encendidos: [0-9a-f]{2} ", out),
                  "sonido mostra o códec, o ajuste e os pinos ligados")
        verificar("Subsistema: PCI 1af4:1100" in out, "subsistema PCI do controlador na ordem fabricante:modelo")
        cid = re.search(r"Codec HDA ([0-9a-f]{4}):([0-9a-f]{4})", out)
        out = m.comando("sonido --verbo 0 f00 0")
        verificar(cid and f"-> {cid.group(1)}{cid.group(2)}" in out, "sonido --verbo lê o identificador do códec")
        out = m.comando("sonido --codec", 60)
        pinos = re.findall(r"^([0-9a-f]{2})   pin ", out, re.M)
        verificar(len(pinos) >= 1 and "config " in out and "pincap" in out and "DAC" in out,
                  f"sonido --codec vuelca os nós (pinos {pinos})")
        out = m.comando("sonido -n --gpio 1")
        verificar("GPIO: mascara" in out, "sonido --gpio responde")
        pino_saida = re.search(r"Pines encendidos: ([0-9a-f]{2})", m.comando("sonido")).group(1)
        out = m.comando(f"sonido -n --eapd {pino_saida} on")
        verificar(f"Pin {pino_saida}: EAPD" in out, "sonido --eapd responde")
        out = m.comando(f"sonido -n --pin {pino_saida} on")
        verificar(f"Pin {pino_saida} encendido" in out, "sonido --pin liga de novo o pino de saída")
        out = m.comando("sonido --auto off")
        verificar("Autosilencio: no" in out, "sonido --auto off")
        m.comando("sonido --auto on")
        out = m.comando("sonido --tono", 60)
        verificar("graves" in out and "derecha" in out, "sonido --tono anuncia os tons")
        out = m.comando("sonido --pin zz on")
        verificar("uso: sonido" in out, "argumento inválido mostra o uso")
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    print("  trechos:", tr)
    graves = [t for t in tr if h70.perto(t.f_esq, 100, 8) and h70.perto(t.f_dir, 100, 8)]
    verificar(len(graves) == 1 and h70.perto(graves[0].segundos, 1.5, 0.1), "tom de 100 Hz (graves) nos dois canais, 1,5 s")
    ambos = [t for t in tr if t.rms_esq > 3000 and t.rms_dir > 3000 and h70.perto(t.f_esq, 1000, 8)]
    so_esq = [t for t in tr if t.rms_esq > 3000 and t.rms_dir < 200 and h70.perto(t.f_esq, 1000, 8)]
    so_dir = [t for t in tr if t.rms_dir > 3000 and t.rms_esq < 200 and h70.perto(t.f_dir, 1000, 8)]
    # 1000 Hz nos dois canais seguido do só esquerdo pode sair como um trecho só (sem silêncio no meio)
    verificar(len(ambos) >= 1, "tom de 1000 Hz nos dois canais")
    verificar(len(so_dir) == 1, "tom só no canal direito")
    verificar(len(so_esq) == 1 or any(t.segundos > 2.2 for t in ambos), "tom só no canal esquerdo")


def ac97():
    with Maquina("h79-ac97", "ide", imagem=imagem_console(), som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sonido")
        verificar("no hay tarjeta HDA" in out, "com AC'97, sonido diz que não há HDA")
        out = m.comando("sonido --tono", 60)
        verificar("derecha" in out, "com AC'97, sonido --tono toca")


def main():
    quais = sys.argv[1:] or ["anfitrion", "hda", "ac97"]
    if "anfitrion" in quais:
        anfitriao()
    if "hda" in quais:
        hda()
    if "ac97" in quais:
        ac97()
    return 0


if __name__ == "__main__":
    sys.exit(main())
