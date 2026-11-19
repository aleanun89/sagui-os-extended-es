#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H404 - funciones de los portátiles por ACPI (D403): ACPI Video, Fujitsu, Toshiba y teclas.

QEMU no emula nada de esto: la prueba añade SSDT propias (-acpitable) que imitan
lo que se ve en el AML de los portátiles reales. El AML lo genera esta misma
prueba (no hace falta iasl; el esquema es el de h158).

Criterio:
  (0) anfitrión: kernel/lib/brilho_conv.c (niveles del _BCL, _BQC por valor o por
      índice, porcentajes y pasos) con casos raros de BIOS, también con
      -fsanitize=address,undefined;
  (0b) anfitrión: tools/uacpi_volcado.c --portatil con las SSDT de (a) y (b) y, con
      SAGUI_ACPIDUMPS=<carpeta>, con volcados de portátiles reales;
  (a) ACPI Video + Fujitsu: \\_SB.GFX0 (_DOS/_DOD) con la salida LCD0
      (_BCL de 10 niveles, _BCM, _BQC), FUJ02B1 (RBLL/GBLL/SBLL) y FUJ02E3 (FUNC).
      El _DOS de la SSDT hace Notify(LCD0, 0x86) al llamarlo: el aviso de verdad
      (Notify desde el AML) sube el brillo del 50 al 60 % con el aviso en
      pantalla; `brillo 80`, `brillo +`, `brillo -` y los avisos 0x86/0x87
      (simulados con `portatil notificar`) lo cambian; el FUJ02B1 avisa de una
      tecla y su SBLL cambia también el brillo de la pantalla (lo hizo el
      firmware): no se sube dos veces; el FUJ02E3 da el botón de la radio
      (0x420): modo avión y la luz de la radio; la tecla de pantalla (0x80 del
      dispositivo de vídeo) da su aviso; las teclas de volumen del teclado
      (sendkey volumedown/volumeup/audiomute) cambian el volumen;
  (b) Toshiba: \\_SB.VALZ (TOS1900) con INFO, ENAB y SPFC (la HCI): ENAB avisa
      con 0x80 y la primera tecla (0x141) sube el brillo HCI; después silencio
      (0x101), radio (0x142) y pantalla (0x13F); `brillo 100` y `brillo 0` por la
      HCI; `portatil` da el ventilador y el interruptor de la radio;
  (c) uACPI sin esos dispositivos (QEMU tal cual): nada se rompe, `brillo` dice
      que no hay control, `portatil` no encuentra nada y las teclas de volumen
      siguen funcionando;
  (d) imagen sin --uacpi (lo de por defecto): `brillo` dice por qué no, las
      teclas de volumen funcionan y `portatil tablas` escribe las tablas ACPI
      en el formato de acpidump (DSDT y FACP, sumas de comprobación correctas).

SAGUI_SO_ANFITRIAO=1: solo las partes (0) y (0b), sin QEMU.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, BUILD  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(BUILD, "testes", "h404")


# ------------------------------------------------------------ AML (el esquema de h147/h158, ampliado)

def pkglen(n):
    for nb in range(1, 5):
        tot = n + nb
        if nb == 1 and tot < 0x40:
            return bytes([tot])
        if nb > 1 and tot < (1 << (4 + 8 * (nb - 1))):
            b = [((nb - 1) << 6) | (tot & 0xF)]
            for i in range(1, nb):
                b.append((tot >> (4 + 8 * (i - 1))) & 0xFF)
            return bytes(b)
    raise ValueError(n)


def nome(s):
    raiz = s.startswith("\\")
    s = s.lstrip("\\")
    segs = [x.ljust(4, "_").encode() for x in s.split(".")] if s else []
    pre = b"\\" if raiz else b""
    if not segs:
        return pre + b"\x00"
    if len(segs) == 1:
        return pre + segs[0]
    if len(segs) == 2:
        return pre + b"\x2e" + b"".join(segs)
    return pre + b"\x2f" + bytes([len(segs)]) + b"".join(segs)


def inteiro(v):
    if v == 0:
        return b"\x00"
    if v == 1:
        return b"\x01"
    if v < 0x100:
        return b"\x0a" + bytes([v])
    if v < 0x10000:
        return b"\x0b" + struct.pack("<H", v)
    return b"\x0c" + struct.pack("<I", v)


def cadeia(s):
    return b"\x0d" + s.encode() + b"\x00"


def pacote(*elems):
    corpo = bytes([len(elems)]) + b"".join(elems)
    return b"\x12" + pkglen(len(corpo)) + corpo


def name(n, dado):
    return b"\x08" + nome(n) + dado


def scope(n, *termos):
    corpo = nome(n) + b"".join(termos)
    return b"\x10" + pkglen(len(corpo)) + corpo


def device(n, *termos):
    corpo = nome(n) + b"".join(termos)
    return b"\x5b\x82" + pkglen(len(corpo)) + corpo


def method(n, nargs, *termos):
    corpo = nome(n) + bytes([nargs]) + b"".join(termos)
    return b"\x14" + pkglen(len(corpo)) + corpo


def se(pred, *termos):
    corpo = pred + b"".join(termos)
    return b"\xa0" + pkglen(len(corpo)) + corpo


def ret(x):
    return b"\xa4" + x


def incrementar(n):
    return b"\x75" + (nome(n) if isinstance(n, str) else n)


def igual(a, b):
    return b"\x93" + a + b


def maior(a, b):
    return b"\x94" + a + b


def menor(a, b):
    return b"\x95" + a + b


def guardar(src, dst):    # Store(src, dst)
    return b"\x70" + src + dst


def indice(pkg, i):       # Index(pkg, i) sin destino
    return b"\x88" + pkg + i + b"\x00"


def deref(x):
    return b"\x83" + x


def notify(n, v):
    return b"\x86" + nome(n) + inteiro(v)


ARG = [bytes([0x68 + i]) for i in range(7)]
LOCAL = [bytes([0x60 + i]) for i in range(8)]


def eisaid(s):
    v = ((ord(s[0]) - 0x40) << 26) | ((ord(s[1]) - 0x40) << 21) | ((ord(s[2]) - 0x40) << 16) | int(s[3:], 16)
    return struct.unpack("<I", struct.pack(">I", v))[0]


def tabela(assin, oem_id, corpo):
    tam = 36 + len(corpo)
    cab = bytearray(assin.encode() + struct.pack("<I", tam) + bytes([2, 0]) + b"SAGUI " +
                    oem_id.encode().ljust(8, b" ") + struct.pack("<I", 1) + b"SAGI" + struct.pack("<I", 1))
    t = cab + corpo
    t[9] = (-sum(t)) & 0xFF
    return bytes(t)


def ssdt_video_fujitsu():
    """\\_SB.GFX0 con LCD0 (brillo 10-100 %, ahora 50), FUJ02B1 y FUJ02E3."""
    bcl = [100, 40] + list(range(10, 101, 10))
    lcd = device("LCD0",
                 name("_ADR", inteiro(0x400)),
                 name("BRIL", inteiro(50)),
                 method("_BCL", 0, ret(pacote(*[inteiro(v) for v in bcl]))),
                 method("_BCM", 1, guardar(ARG[0], nome("BRIL"))),
                 method("_BQC", 0, ret(nome("BRIL"))))
    gfx = device("GFX0",
                 name("_ADR", inteiro(0x00020000)),
                 name("DOSV", inteiro(0xFF)),
                 name("DOSN", inteiro(0)),
                 # el primer _DOS avisa de una tecla de brillo: el camino de verdad (Notify desde el AML)
                 method("_DOS", 1, guardar(ARG[0], nome("DOSV")), incrementar("DOSN"),
                        se(igual(nome("DOSN"), inteiro(1)), notify("\\_SB.GFX0.LCD0", 0x86))),
                 method("_DOD", 0, ret(pacote(inteiro(0x80010400)))),
                 lcd)
    fjbl = device("FJBL",
                  name("_HID", cadeia("FUJ02B1")),
                  name("CG", inteiro(0)),
                  name("SETL", inteiro(0xFF)),
                  method("RBLL", 0, ret(inteiro(8))),
                  # la primera lectura (al arrancar) da 3; las siguientes, 5 (la tecla subió dos)
                  method("GBLL", 0, incrementar("CG"), se(maior(nome("CG"), inteiro(1)), ret(inteiro(5))), ret(inteiro(3))),
                  # este firmware, al confirmar el nivel, también pone el brillo de la pantalla al 90 %
                  method("SBLL", 1, guardar(ARG[0], nome("SETL")), guardar(inteiro(90), nome("\\_SB.GFX0.LCD0.BRIL"))))
    func = [
        se(igual(ARG[0], inteiro(0x1000)),
           se(igual(ARG[1], inteiro(0)), ret(inteiro(0x00020020))),      # bit 5 radio, bit 17 luz de la radio
           se(igual(ARG[1], inteiro(4)), incrementar("C4"),
              se(maior(nome("C4"), inteiro(1)), guardar(inteiro(1), nome("ARMD"))), ret(inteiro(0))),
           se(igual(ARG[1], inteiro(5)), guardar(ARG[3], nome("LEDR")), ret(inteiro(0))),
           ret(inteiro(0))),
        se(igual(ARG[0], inteiro(0x1002)),
           se(igual(ARG[1], inteiro(1)),
              se(nome("ARMD"), guardar(inteiro(0), nome("ARMD")), ret(inteiro(0x420))),
              ret(inteiro(0))),
           ret(inteiro(0))),
        ret(inteiro(0x80000000)),
    ]
    fext = device("FEXT",
                  name("_HID", cadeia("FUJ02E3")),
                  name("C4", inteiro(0)),
                  name("ARMD", inteiro(0)),
                  name("LEDR", inteiro(0xFF)),
                  method("FUNC", 4, *func))
    return tabela("SSDT", "PORTATIL", scope("\\_SB", gfx, fjbl, fext))


def ssdt_toshiba():
    """\\_SB.VALZ (TOS1900): INFO con una cola de teclas, ENAB que avisa y SPFC (HCI)."""
    teclas = [0x141, 0x1C1, 0x101, 0x142, 0x13F]
    spfc = [
        guardar(pacote(*[inteiro(0)] * 6), LOCAL[0]),
        se(igual(ARG[1], inteiro(0x1E)), guardar(ARG[2], nome("HKEV")), ret(LOCAL[0])),
        se(igual(ARG[1], inteiro(0x2A)),
           se(igual(ARG[0], inteiro(0xFF00)), guardar(ARG[2], nome("HBRI"))),
           guardar(nome("HBRI"), indice(LOCAL[0], inteiro(2))), ret(LOCAL[0])),
        se(igual(ARG[1], inteiro(0x04)), guardar(inteiro(1), indice(LOCAL[0], inteiro(2))), ret(LOCAL[0])),
        se(igual(ARG[1], inteiro(0x56)), guardar(inteiro(1), indice(LOCAL[0], inteiro(2))), ret(LOCAL[0])),
        guardar(inteiro(0x8000), indice(LOCAL[0], inteiro(0))),
        ret(LOCAL[0]),
    ]
    valz = device("VALZ",
                  name("_HID", inteiro(eisaid("TOS1900"))),
                  name("ENFG", inteiro(0)),
                  name("HKEV", inteiro(0)),
                  name("HBRI", inteiro(3 << 13)),
                  name("QI", inteiro(0)),
                  name("Q", pacote(*[inteiro(v) for v in teclas])),
                  method("ENAB", 0, guardar(inteiro(1), nome("ENFG")), notify("\\_SB.VALZ", 0x80)),
                  method("INFO", 0,
                         se(menor(nome("QI"), inteiro(len(teclas))),
                            guardar(deref(indice(nome("Q"), nome("QI"))), LOCAL[0]),
                            incrementar("QI"),
                            ret(LOCAL[0])),
                         ret(inteiro(0))),
                  method("SPFC", 6, *spfc))
    return tabela("SSDT", "TOSHIBA", scope("\\_SB", valz))


def gravar_ssdt(nome_caso, dados):
    os.makedirs(DIR, exist_ok=True)
    arq = os.path.join(DIR, f"ssdt-{nome_caso}.aml")
    with open(arq, "wb") as f:
        f.write(dados)
    return arq


# ------------------------------------------------------------ (0) anfitrión

PROGRAMA_ANFITRIAO = r'''
#include <stdio.h>
#include <string.h>
#include "brilho_conv.h"

static int falhas;
#define OK(c, ...) do { if (!(c)) { printf("FALLO "); printf(__VA_ARGS__); printf("\n"); falhas++; } } while (0)

int main(void)
{
    uint32_t n[BRILHO_MAX_NIVEIS];
    /* lo normal: cargador, batería y diez niveles */
    uint64_t a[] = { 100, 40, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
    int k = brilho_niveis(a, 12, n, BRILHO_MAX_NIVEIS);
    OK(k == 10 && n[0] == 10 && n[9] == 100, "normal: %d niveles %u-%u", k, n[0], n[k - 1]);
    OK(brilho_indice(n, k, 50) == 4, "indice de 50");
    OK(brilho_indice(n, k, 54) == 4 && brilho_indice(n, k, 56) == 5, "el mas cercano");
    OK(brilho_porcentagem(n, k, 4) == 50, "50 %%");
    OK(brilho_indice_de_pct(n, k, 80) == 7 && brilho_indice_de_pct(n, k, 0) == 0 && brilho_indice_de_pct(n, k, 100) == 9, "de pct");
    OK(brilho_passo(k, 4, 1) == 5 && brilho_passo(k, 9, 1) == 9 && brilho_passo(k, 0, -1) == 0 && brilho_passo(k, 3, -5) == 0, "pasos");
    OK(brilho_passo(k, -1, 1) == 0 && brilho_passo(k, -1, -1) == 9, "paso sin nivel conocido");
    /* desordenado y con repetidos */
    uint64_t b[] = { 70, 30, 100, 30, 0, 70, 50, 100 };
    k = brilho_niveis(b, 8, n, BRILHO_MAX_NIVEIS);
    OK(k == 5 && n[0] == 0 && n[1] == 30 && n[2] == 50 && n[3] == 70 && n[4] == 100, "desordenado: %d [%u %u %u %u]", k, n[0], n[1], n[2], n[3]);
    /* 0-255 (no es porcentaje) */
    uint64_t c[] = { 255, 128, 32, 64, 128, 255 };
    k = brilho_niveis(c, 6, n, BRILHO_MAX_NIVEIS);
    OK(k == 4 && brilho_porcentagem(n, k, 3) == 100 && brilho_porcentagem(n, k, 0) == 13, "0-255: %d %d", k, brilho_porcentagem(n, k, 0));
    /* _BCL roto de dos elementos: se usan esos */
    uint64_t d[] = { 100, 50 };
    k = brilho_niveis(d, 2, n, BRILHO_MAX_NIVEIS);
    OK(k == 2 && n[0] == 50 && n[1] == 100, "dos elementos");
    /* uno solo o valores absurdos: no sirve */
    uint64_t e[] = { 100, 50, 0x123456789ull, 0x10000 };
    k = brilho_niveis(e, 4, n, BRILHO_MAX_NIVEIS);
    OK(k == 0, "valores absurdos: %d", k);
    OK(brilho_niveis(NULL, 3, n, 10) == 0, "nulo");
    /* el máximo de la lista se respeta (no se sale del arreglo) */
    uint64_t f[40];
    for (int i = 0; i < 40; i++) f[i] = (uint64_t)(39 - i);
    k = brilho_niveis(f, 40, n, 8);
    OK(k == 8 && n[0] == 0 && n[7] == 7, "lleno: %d %u..%u", k, n[0], n[7]);
    /* _BQC */
    k = brilho_niveis(a, 12, n, BRILHO_MAX_NIVEIS);
    bool ei;
    OK(brilho_bqc(n, k, 60, &ei) == 5 && !ei, "bqc valor");
    OK(brilho_bqc(n, k, 3, &ei) == 3 && ei, "bqc indice");
    OK(brilho_bqc(n, k, 55, &ei) == -1, "bqc que no encaja");
    OK(brilho_bqc(n, k, 0xFFFFFFFFull, &ei) == -1, "bqc absurdo");
    uint64_t g[] = { 7, 3, 0, 1, 2, 3, 4, 5, 6, 7 };
    int kg = brilho_niveis(g, 10, n, BRILHO_MAX_NIVEIS);
    OK(kg == 8 && brilho_bqc(n, kg, 3, &ei) == 3 && !ei, "niveles 0-7: valor = indice");
    /* lineales (Fujitsu 0..7, Toshiba 0..7) */
    OK(brilho_pct_linear(0, 8) == 0 && brilho_pct_linear(7, 8) == 100 && brilho_pct_linear(4, 8) == 57, "lineal %d", brilho_pct_linear(4, 8));
    OK(brilho_nivel_linear(100, 8) == 7 && brilho_nivel_linear(0, 8) == 0 && brilho_nivel_linear(50, 8) == 4, "nivel lineal");
    OK(brilho_pct_linear(3, 1) == -1 && brilho_nivel_linear(50, 1) == 0, "lineal con un nivel");
    printf("falhas %d\n", falhas);
    return falhas != 0;
}
'''


def anfitriao():
    os.makedirs(DIR, exist_ok=True)
    fonte = os.path.join(DIR, "h404_anfitriao.c")
    with open(fonte, "w") as f:
        f.write(PROGRAMA_ANFITRIAO)
    inc = os.path.join(RAIZ, "kernel", "inc")
    conv = os.path.join(RAIZ, "kernel", "lib", "brilho_conv.c")
    for nome_exe, extra in (("h404_anfitriao", ["-O2"]),
                            ("h404_anfitriao_asan", ["-O1", "-g", "-fsanitize=address,undefined",
                                                     "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome_exe)
        r = subprocess.run(["gcc", *extra, "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", inc, "-o", exe, fonte, conv],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, f"anfitrión: compila brilho_conv.c ({nome_exe})\n{r.stderr}")
        r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
        verificar(r.returncode == 0 and "falhas 0" in r.stdout, f"anfitrión: los casos de brilho_conv ({nome_exe})\n{r.stdout}")
    print("  anfitrión: niveles, _BQC, porcentajes y pasos correctos")


# ------------------------------------------------------------ (0b) uACPI en el anfitrión

def volcado_texto(tabelas):
    linhas = []
    for t in tabelas:
        linhas.append(f"{t[:4].decode('latin1')} @ 0x0000000000000000")
        for i in range(0, len(t), 16):
            linhas.append(f"    {i:04X}: " + " ".join(f"{b:02X}" for b in t[i:i + 16]))
        linhas.append("")
    return "\n".join(linhas) + "\n"


def anfitriao_volcado():
    """tools/uacpi_volcado.c --portatil (lo que buscan video_acpi.c, fujitsu.c y toshiba.c) sobre una máquina
    mínima con las SSDT de (a) y (b); con SAGUI_ACPIDUMPS=<carpeta>, también sobre volcados de portátiles
    reales (uACPI/ACPIDumps o `portatil tablas`): ninguno se cuelga."""
    exe = os.path.join(DIR, "uacpi_volcado")
    srcdir = os.path.join(RAIZ, "kernel", "terceros", "uacpi", "source")
    fontes = [os.path.join(RAIZ, "tools", "uacpi_volcado.c"), os.path.join(RAIZ, "kernel", "lib", "bateria_conv.c"),
              os.path.join(RAIZ, "kernel", "lib", "brilho_conv.c"), os.path.join(RAIZ, "kernel", "dispositivos", "prt.c")]
    fontes += sorted(os.path.join(srcdir, f) for f in os.listdir(srcdir) if f.endswith(".c"))
    r = subprocess.run(["gcc", "-O1", "-g", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "terceros", "uacpi", "include"),
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe, *fontes], capture_output=True, text=True)
    verificar(r.returncode == 0, f"anfitrión: compila tools/uacpi_volcado.c\n{r.stderr[-2000:]}")
    dsdt = tabela("DSDT", "MINIMA", scope("\\_SB", device("PCI0", name("_HID", inteiro(eisaid("PNP0A03"))))))
    facp = bytearray(tabela("FACP", "MINIMA", bytes(244 - 36)))
    facp[8] = 4
    facp[9] = 0
    facp[9] = (-sum(facp)) & 0xFF
    arq = os.path.join(DIR, "volcado-portatil.txt")
    with open(arq, "w") as f:
        f.write(volcado_texto([dsdt, bytes(facp), ssdt_video_fujitsu(), ssdt_toshiba()]))
    r = subprocess.run([exe, "--portatil", arq], capture_output=True, text=True, timeout=120)
    out = r.stdout
    verificar(r.returncode == 0 and "SIN COLGARSE" in out, f"anfitrión: --portatil con las SSDT de la prueba\n{out}{r.stderr}")
    verificar(re.search(r"salida \\_SB_?\.GFX0\.LCD0: _BCL no error, 12 valores, 10 niveles \(10-100\), _BQC no error 50 -> nivel 4", out)
              is not None, f"anfitrión: la salida LCD0 con 10 niveles y _BQC\n{out}")
    verificar("FUNC(0x1000,0) no error 20020" in out, "anfitrión: FUJ02E3 FUNC(0x1000, 0)")
    verificar(re.search(r"toshiba \\_SB_?\.VALZ: _STA no error \w+, HCI SPFC, INFO 1, ENAB 1", out) is not None and
              "HCI brillo (002a): no error, resultado 0 valor 6000" in out, f"anfitrión: la HCI de Toshiba\n{out}")
    verificar("portatil: 1 dispositivos de video, 1 salidas con brillo, 2 de Fujitsu, 1 de Toshiba" in out,
              f"anfitrión: el resumen\n{out}")
    print("  anfitrión: uacpi_volcado --portatil encuentra lo mismo que el kernel")
    pasta = os.environ.get("SAGUI_ACPIDUMPS")
    if not pasta:
        return
    for a in sorted(os.path.join(pasta, f) for f in os.listdir(pasta) if f.endswith((".bin", ".txt", ".dat"))):
        r = subprocess.run([exe, "--portatil", a], capture_output=True, text=True, timeout=300)
        if r.returncode == 2 and "sin tablas" in r.stderr:
            continue
        res = re.search(r"^portatil: .*$", r.stdout, re.M)
        print(f"  volcado {os.path.basename(a)}: {res.group(0) if res else '-'}")
        verificar(r.returncode == 0 and "SIN COLGARSE" in r.stdout,
                  f"anfitrión: {os.path.basename(a)} sin colgarse\n{r.stdout[-3000:]}{r.stderr[-1000:]}")


# ------------------------------------------------------------ QEMU

def esperar(m, padrao, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def sem_erros(m, caso):
    s = m.serial()
    verificar("EXCECAO" not in s and "PANICO" not in s.upper(), f"{caso}: sin excepciones")


def uacpi_listo(m, caso):
    esperar(m, r"\[UACPI\] listo en \d+ ms", 120)
    verificar("[UACPI] no se pudo cargar" not in m.serial(), f"{caso}: uACPI carga las tablas")


def brillo(m, args=""):
    return m.comando(("brillo " + args).strip())


def pct_de(out):
    g = re.search(r"Brillo: (\d+) %", out)
    return int(g.group(1)) if g else None


def osd(m, marca, padrao, tempo=20):
    return esperar(m, r"\[GUI\] osd \d+ -?\d+: " + padrao, tempo, marca)


def volumen(m):
    g = re.search(r"Volumen: (\d+) %", m.comando("volumen"))
    return int(g.group(1)) if g else None


def teclas_volumen(m, caso):
    """volumedown, volumeup y audiomute por el teclado (E0 2E, E0 30, E0 20)."""
    v0 = volumen(m)
    verificar(v0 is not None, f"{caso}: hay tarjeta de sonido (volumen {v0})")
    marca = len(m.serial())
    m.tecla("volumedown", 0.3)
    esperar(m, r"\[ACCIONES\] volumen- \(teclado\)", 15, marca)
    osd(m, marca, r"Volumen: \d+ %")
    v1 = volumen(m)
    verificar(v1 == max(0, v0 - 5), f"{caso}: la tecla de bajar el volumen ({v0} -> {v1})")
    marca = len(m.serial())
    m.tecla("volumeup", 0.3)
    esperar(m, r"\[ACCIONES\] volumen\+ \(teclado\)", 15, marca)
    v2 = volumen(m)
    verificar(v2 == min(100, v1 + 5), f"{caso}: la tecla de subir el volumen ({v1} -> {v2})")
    marca = len(m.serial())
    m.tecla("audiomute", 0.3)
    osd(m, marca, r"Volumen: silencio")
    out = m.comando("volumen")
    verificar("(silencio)" in out, f"{caso}: la tecla de silencio calla\n{out}")
    marca = len(m.serial())
    m.tecla("volumeup", 0.3)
    osd(m, marca, r"Volumen: \d+ %")
    out = m.comando("volumen")
    verificar("(silencio)" not in out, f"{caso}: subir el volumen quita el silencio\n{out}")


def imagem_uacpi():
    return imagem_de_teste("", ["--vesa", "--uacpi"])


def parte_a():
    ssdt = gravar_ssdt("a", ssdt_video_fujitsu())
    with Maquina("h404a", "ide", imagem=imagem_uacpi(), som="ac97", extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        uacpi_listo(m, "a")
        s = m.serial()
        verificar(re.search(r"\[VIDEO\] dispositivo de video \\_SB_?\.GFX0 \(PCI sin comprobar\): 1 salida", s) is not None,
                  "a: encuentra el dispositivo de vídeo \\_SB.GFX0 con una salida con brillo")
        verificar(re.search(r"\[VIDEO\] brillo por \\_SB_?\.GFX0\.LCD0: 10 niveles \(10-100\)", s) is not None,
                  "a: la salida LCD0 con los 10 niveles del _BCL")
        verificar(re.search(r"\[FUJITSU\] FUJ02B1 \\_SB_?\.FJBL: 8 niveles, nivel 3", s) is not None, "a: FUJ02B1 con 8 niveles")
        verificar(re.search(r"\[FUJITSU\] FUJ02E3 \\_SB_?\.FEXT: funciones 20020", s) is not None, "a: FUJ02E3 y sus funciones")
        verificar("[TOSHIBA] no hay interfaz HCI" in s, "a: no hay Toshiba")
        # El Notify(LCD0, 0x86) del _DOS: el camino de verdad
        esperar(m, r"\[VIDEO\] aviso 86 de la salida", 30)
        osd(m, 0, r"Brillo: 60 %", 30)
        out = brillo(m)
        verificar(re.search(r"Brillo: 60 % \(nivel 6 de 10, ACPI Video \\_SB_?\.GFX0\.LCD0\)", out) is not None,
                  f"a: el aviso 0x86 del firmware subió el brillo del 50 al 60 %\n{out}")
        out = m.comando("portatil")
        verificar("ACPI Video: 1 dispositivo(s), 1 salida(s) con brillo" in out and "_DOS(0)" in out,
                  f"a: `portatil` da el ACPI Video y el _DOS(0)\n{out}")
        verificar("Fujitsu:" in out and "brillo FUJ02B1" in out and "teclas FUJ02E3" in out, f"a: `portatil` da Fujitsu\n{out}")
        # la orden
        verificar(pct_de(brillo(m, "80")) == 80, "a: brillo 80")
        verificar(pct_de(brillo(m, "+")) == 90, "a: brillo + sube un nivel")
        verificar(pct_de(brillo(m, "-2")) == 70, "a: brillo -2 baja dos niveles")
        verificar(pct_de(brillo(m, "47")) == 50, "a: brillo 47 elige el nivel más cercano (50)")
        out = brillo(m, "101")
        verificar("uso: brillo" in out, "a: brillo 101 no vale")
        # avisos simulados del ACPI Video
        marca = len(m.serial())
        m.comando("portatil notificar video 0x86")
        osd(m, marca, r"Brillo: 60 %")
        marca = len(m.serial())
        m.comando("portatil notificar video 0x87")
        osd(m, marca, r"Brillo: 50 %")
        verificar(pct_de(brillo(m)) == 50, "a: 0x86 y 0x87 suben y bajan un nivel")
        # Fujitsu: la tecla de brillo (FUJ02B1), con el firmware que ya puso el 90 %
        marca = len(m.serial())
        m.comando("portatil notificar fujitsu-brillo 0x80")
        esperar(m, r"\[FUJITSU\] tecla de brillo: nivel 3 -> 5 de 8", 20, marca)
        esperar(m, r"\[PORTATIL\] el firmware ya cambio el brillo", 20, marca)
        osd(m, marca, r"Brillo: 90 %")
        verificar(pct_de(brillo(m)) == 90, "a: lo que puso el firmware (90 %) no se sube otra vez")
        # Fujitsu: el botón de la radio (FUJ02E3, 0x420) y su luz
        marca = len(m.serial())
        m.comando("portatil notificar fujitsu 0x80")
        esperar(m, r"\[ACCIONES\] radio \(fujitsu\)", 20, marca)
        osd(m, marca, r"Radio inal.mbrica: apagada")
        esperar(m, r"\[FUJITSU\] luz de la radio: apagada", 20, marca)
        out = m.comando("portatil")
        verificar("Radio: apagada (modo avión)" in out and "última: radio, por Fujitsu" in out,
                  f"a: el botón de la radio de Fujitsu pone el modo avión\n{out}")
        marca = len(m.serial())
        m.comando("portatil radio")
        esperar(m, r"\[FUJITSU\] luz de la radio: encendida", 20, marca)
        verificar("Radio: encendida" in m.comando("portatil"), "a: `portatil radio` la vuelve a encender")
        # la tecla de pantalla
        marca = len(m.serial())
        m.comando("portatil notificar pantalla 0x80")
        osd(m, marca, r"Cambio de pantalla")
        time.sleep(0.6)
        m.captura("H404-1-aviso")
        teclas_volumen(m, "a")
        sem_erros(m, "a")


def parte_b():
    ssdt = gravar_ssdt("b", ssdt_toshiba())
    with Maquina("h404b", "ide", imagem=imagem_uacpi(), som="ac97", extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        uacpi_listo(m, "b")
        s = m.serial()
        verificar(re.search(r"\[TOSHIBA\] \\_SB_?\.VALZ por SPFC: teclas por INFO, brillo HCI si, ventilador 1, radio 1", s)
                  is not None, "b: encuentra la HCI de Toshiba (SPFC), las teclas, el brillo, el ventilador y la radio")
        verificar("[VIDEO] no hay dispositivos de video ACPI" in s, "b: sin ACPI Video")
        # ENAB avisa: la primera tecla (0x141) sube el brillo HCI del nivel 3 al 4
        esperar(m, r"\[TOSHIBA\] tecla 141", 30)
        osd(m, 0, r"Brillo: 57 %", 30)
        out = brillo(m)
        verificar(re.search(r"Brillo: 57 % \(nivel 5 de 8, Toshiba \\_SB_?\.VALZ\)", out) is not None,
                  f"b: la tecla 0x141 subió el brillo por la HCI\n{out}")
        verificar(pct_de(brillo(m, "100")) == 100 and pct_de(brillo(m, "0")) == 0, "b: brillo 100 y 0 por la HCI")
        out = m.comando("portatil")
        verificar("teclas por INFO" in out and "Ventilador: en marcha (1)" in out and "Interruptor de la radio: encendido" in out,
                  f"b: `portatil` da las teclas, el ventilador y el interruptor\n{out}")
        # la cola: 0x1C1 (soltar: nada), 0x101 silencio, 0x142 radio, 0x13F pantalla
        marca = len(m.serial())
        m.comando("portatil notificar toshiba 0x80")
        esperar(m, r"\[TOSHIBA\] tecla 1c1", 20, marca)
        m.comando("portatil notificar toshiba 0x80")
        esperar(m, r"\[ACCIONES\] silencio \(toshiba\)", 20, marca)
        osd(m, marca, r"Volumen: silencio")
        m.comando("portatil notificar toshiba 0x80")
        esperar(m, r"\[ACCIONES\] radio \(toshiba\)", 20, marca)
        m.comando("portatil notificar toshiba 0x80")
        esperar(m, r"\[ACCIONES\] pantalla \(toshiba\)", 20, marca)
        tramo = m.serial()[marca:]
        tramo = tramo[:tramo.index("[ACCIONES] silencio (toshiba)")]
        verificar("[ACCIONES] brillo" not in tramo, "b: soltar la tecla (0x1C1) no hace nada")
        out = m.comando("portatil")
        verificar("Radio: apagada (modo avión)" in out, f"b: la tecla 0x142 pone el modo avión\n{out}")
        sem_erros(m, "b")


def parte_c():
    with Maquina("h404c", "ide", imagem=imagem_uacpi(), som="ac97") as m:
        m.esperar_serial("sagui:/$", 180)
        uacpi_listo(m, "c")
        esperar(m, r"\[PORTATIL\] no hay control del brillo por ACPI", 30)
        s = m.serial()
        verificar("[FUJITSU] no hay dispositivos de Fujitsu" in s and "[TOSHIBA] no hay interfaz HCI" in s,
                  "c: ni Fujitsu ni Toshiba")
        out = brillo(m)
        verificar("brillo: este equipo no deja cambiar el brillo" in out, f"c: `brillo` sin control\n{out}")
        out = brillo(m, "50")
        verificar("brillo: este equipo no deja cambiar el brillo" in out, f"c: `brillo 50` sin control\n{out}")
        out = m.comando("portatil")
        verificar("Brillo: sin control" in out and "ACPI Video: 0 dispositivo(s)" in out and "Fujitsu: no" in out
                  and "Toshiba: no" in out, f"c: `portatil` no encuentra nada\n{out}")
        marca = len(m.serial())
        m.comando("portatil notificar video 0x86")   # un aviso sin el dispositivo: no pasa nada
        time.sleep(1)
        verificar("[ACCIONES]" not in m.serial()[marca:], "c: un aviso para un dispositivo que no hay no hace nada")
        marca = len(m.serial())
        m.comando("portatil accion brillo+")
        osd(m, marca, r"Este equipo no deja cambiar el brillo")
        teclas_volumen(m, "c")
        sem_erros(m, "c")


def suma_ok(t):
    return sum(t) & 0xFF == 0


def ler_volcado(texto):
    """Las tablas del formato de acpidump: {firma: [bytes, ...]}."""
    tabs, atual = {}, None
    for l in texto.splitlines():
        g = re.match(r"^(\S{4}) @ 0x[0-9A-Fa-f]+$", l)
        if g:
            atual = bytearray()
            tabs.setdefault(g.group(1), []).append(atual)
            continue
        g = re.match(r"^\s+[0-9A-F]{4,8}:((?: [0-9A-F]{2}){1,16})", l)
        if g and atual is not None:
            atual += bytes(int(x, 16) for x in g.group(1).split())
    return tabs


def parte_d():
    img = imagem_de_teste("", ["--vesa"])   # sin --uacpi: lo de por defecto
    with Maquina("h404d", "ide", imagem=img, som="ac97") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        s = m.serial()
        verificar("acpi-aml" not in s and "[PORTATIL]" not in s and "[FUJITSU]" not in s and "[TOSHIBA]" not in s
                  and "dispositivos de video ACPI" not in s, "d: sin --uacpi no se ejecuta nada")
        out = brillo(m)
        verificar("brillo: no disponible (desactivado (imagen sin --uacpi))" in out, f"d: `brillo` dice por qué no\n{out}")
        teclas_volumen(m, "d")
        out = m.comando("portatil tablas /tmp/acpi.txt")
        g = re.search(r"(\d+) tablas \((\d+) bytes\) en /tmp/acpi.txt", out)
        verificar(g and int(g.group(1)) >= 4, f"d: `portatil tablas` escribe las tablas\n{out}")
        # el volcado vuelve por el serie con cat (no es grande en QEMU: unos 10 KB)
        out = m.comando("cat /tmp/acpi.txt", tempo=120)
        tabs = ler_volcado(out)
        verificar("DSDT" in tabs and "FACP" in tabs, f"d: el volcado tiene la DSDT y la FACP ({sorted(tabs)})")
        for ass in ("DSDT", "FACP"):
            t = bytes(tabs[ass][0])
            verificar(t[:4] == ass.encode() and len(t) == struct.unpack("<I", t[4:8])[0] and suma_ok(t),
                      f"d: {ass} entera y con la suma bien ({len(t)} bytes)")
        sem_erros(m, "d")


def main():
    try:
        anfitriao()
        anfitriao_volcado()
        if os.environ.get("SAGUI_SO_ANFITRIAO") != "1":
            for parte in (parte_a, parte_b, parte_c, parte_d):
                parte()
                print(f"  {parte.__name__}: ok")
    except (FalhaTeste, AssertionError) as e:
        print(f"FALHOU: {e}")
        return 1
    print("H404 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
