#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H158 - batería del portátil por ACPI con uACPI (D192).

QEMU no emula batería: la prueba le añade una SSDT propia (-acpitable) con
una batería falsa \\_SB.BAT0 (PNP0C0A, _STA/_BIX o _BIF/_BST) y un cargador
\\_SB.ADP0 (ACPI0003, _PSR). El AML lo genera esta misma prueba (no hace
falta iasl). El _BST cuenta sus lecturas y a la segunda hace
Notify(BAT0, 0x80): así se prueba también el camino de los avisos.

Criterio:
  (0) anfitrión: kernel/lib/bateria_conv.c (porcentaje y tiempo) con casos
      raros de BIOS reales, también con -fsanitize=address,undefined;
  (a) descargando al 73 % (_BIX, mWh, sin cargador): el kernel arranca
      uACPI, encuentra BAT0 y ADP0; el icono aparece en la bandeja con el
      relleno verde y sin rayo; la ayuda dice «Batería: 73 % (quedan 2 h 00
      min)»; el panel se abre; `bateria` lo cuenta (y -v el aviso Notify);
      `acpi` sigue igual (S5 y botón) y `apagar-pc` apaga QEMU;
  (b) cargando al 40 % (_BIF, mAh, enchufado): rayo amarillo, «cargando,
      1 h 12 min para llenarse», `bateria` dice «cargando»;
  (c) descargando al 8 %: relleno rojo y la notificación de batería baja,
      una sola vez;
  (d) sin batería (QEMU tal cual): uACPI arranca, no hay icono de batería,
      `bateria` dice que no hay y el botón de encendido (system_powerdown)
      sigue apagando;
  (e) imagen sin --uacpi (lo de por defecto) y la batería falsa: uACPI no
      arranca, no hay icono, `bateria` dice «imagen sin --uacpi» y
      `apagar-pc` apaga.

(a)-(d) usan una imagen con --uacpi: el intérprete está desactivado por defecto.

  (0b) anfitrión: uACPI tal cual con tools/uacpi_volcado.c (la misma
      secuencia que la tarea acpi-aml) sobre una máquina mínima con la
      batería de (a): _BIX y _BST dan 73 % y 2 h 00 min; con
      SAGUI_ACPIDUMPS=<carpeta>, también cada volcado de acpidump de esa
      carpeta (uACPI/ACPIDumps): ninguno se cuelga ni se cae.

SAGUI_SO_ANFITRIAO=1: solo las partes (0) y (0b), sin QEMU.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, imagem_de_teste, cor_do_pixel,  # noqa: E402
                  FalhaTeste, BUILD)

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(BUILD, "testes", "h158")

IC_Y = 3
CLARO = (0xD8, 0xDE, 0xE4)
VERDE = (0x6F, 0xD9, 0x8F)
ROJO = (0xFF, 0x4A, 0x3A)
AMARILLO = (0xFF, 0xD2, 0x3F)
FONDO_BARRA = (0x18, 0x20, 0x2A)


# ------------------------------------------------------------ AML (el mismo esquema que h147)

def pkglen(n):
    """Codifica la longitud de un paquete cuyo contenido tiene n bytes."""
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
    return b"\x75" + nome(n)


def igual(a, b):          # LEqual(a, b)
    return b"\x93" + a + b


def notify(n, v):
    return b"\x86" + nome(n) + inteiro(v)


def eisaid(s):
    """EisaId("PNP0C0A") -> el entero de 32 bits que guarda el AML (0x0A0CD041)."""
    v = ((ord(s[0]) - 0x40) << 26) | ((ord(s[1]) - 0x40) << 21) | ((ord(s[2]) - 0x40) << 16) | int(s[3:], 16)
    return struct.unpack("<I", struct.pack(">I", v))[0]


def tabela(assin, oem_id, corpo):
    tam = 36 + len(corpo)
    cab = bytearray(assin.encode() + struct.pack("<I", tam) + bytes([2, 0]) + b"SAGUI " +
                    oem_id.encode().ljust(8, b" ") + struct.pack("<I", 1) + b"SAGI" + struct.pack("<I", 1))
    t = cab + corpo
    t[9] = (-sum(t)) & 0xFF
    return bytes(t)


def ssdt_bateria(bix=None, bif=None, bst=(0, 0, 0, 0), psr=0):
    """Una SSDT con \\_SB.BAT0 y \\_SB.ADP0. bix: los 20 valores de _BIX;
    bif: los 13 de _BIF (los textos como str). bst: estado, consumo,
    restante, tensión."""
    def valor(v):
        return cadeia(v) if isinstance(v, str) else inteiro(v)
    bat = [name("_HID", inteiro(eisaid("PNP0C0A"))),
           name("_UID", inteiro(1)),
           name("CNT", inteiro(0)),
           method("_STA", 0, ret(inteiro(0x1F)))]
    if bix:
        bat.append(method("_BIX", 0, ret(pacote(*[valor(v) for v in bix]))))
    if bif:
        bat.append(method("_BIF", 0, ret(pacote(*[valor(v) for v in bif]))))
    bat.append(method("_BST", 0,
                      incrementar("CNT"),
                      se(igual(nome("CNT"), inteiro(2)), notify("\\_SB.BAT0", 0x80)),
                      ret(pacote(*[inteiro(v) for v in bst]))))
    adp = [name("_HID", cadeia("ACPI0003")),
           method("_PSR", 0, ret(inteiro(psr)))]
    corpo = scope("\\_SB", device("BAT0", *bat), device("ADP0", *adp))
    return tabela("SSDT", "BATERIA", corpo)


def gravar_ssdt(nome_caso, dados):
    os.makedirs(DIR, exist_ok=True)
    arq = os.path.join(DIR, f"ssdt-{nome_caso}.aml")
    with open(arq, "wb") as f:
        f.write(dados)
    return arq


# _BIX (ACPI 4.0, revisión 0): revisión, unidad, diseño, completa, tecnología,
# tensión, aviso, bajo, ciclos, precisión, muestreo máx/mín, promedio máx/mín,
# granularidad 1 y 2, modelo, serie, tipo, fabricante.
BIX_73 = [0, 0, 5200, 5000, 1, 11100, 500, 200, 42, 95000, 0, 0, 0, 0, 10, 25, "SAGUI-BAT", "0001", "LION", "SaguiOS"]
BST_73 = (1, 1825, 3650, 11400)            # descargando: 3650/5000 = 73 %, 3650/1825 h = 2 h 00 min
BIF_40 = [1, 4400, 4000, 1, 10800, 400, 150, 10, 10, "SAGUI-BAT2", "0002", "LION", "SaguiOS"]
BST_40 = (2, 2000, 1600, 12000)            # cargando: 1600/4000 = 40 %, (4000-1600)/2000 h = 1 h 12 min
BIF_8 = [0, 5000, 5000, 1, 11100, 500, 200, 10, 10, "SAGUI-BAT3", "0003", "LION", "SaguiOS"]
BST_8 = (1, 2000, 400, 10900)              # descargando: 8 %, 12 min


# ------------------------------------------------------------ (0) anfitrión

PROGRAMA_ANFITRIAO = r'''
#include <stdio.h>
#include <string.h>
#include "bateria_conv.h"

#define D BATERIA_DESCONHECIDO
static int falhas;

static bateria_bruta_t bat(uint32_t unid, uint32_t proj, uint32_t total, uint32_t est, uint32_t taxa, uint32_t rest)
{
    bateria_bruta_t b;
    memset(&b, 0, sizeof b);
    b.existe = b.presente = b.info_ok = b.estado_ok = true;
    b.unidade = unid; b.projeto = proj; b.total = total;
    b.projeto_mv = 11100; b.aviso = 500; b.baixo = 200; b.ciclos = D;
    b.estado = est; b.taxa = taxa; b.restante = rest; b.tensao_mv = 11400;
    return b;
}

static void caso(const char *nome, const bateria_bruta_t *b, int n, uint32_t adapt, int pct, int min, uint32_t estado)
{
    info_bateria_t s;
    memset(&s, 0x55, sizeof s);
    bateria_calcular(b, n, adapt, &s);
    char t[24];
    bateria_texto_tempo(s.minutos, t, sizeof t);
    int ok = s.porcentagem == pct && s.minutos == min && s.estado == estado;
    printf("%s %s: pct %d min %d (%s) estado %u presente %u\n", ok ? "ok" : "FALLO", nome, s.porcentagem, s.minutos, t,
           s.estado, s.presente);
    if (!ok) falhas++;
}

int main(void)
{
    bateria_bruta_t b[2];
    b[0] = bat(0, 5200, 5000, 1, 1825, 3650);
    caso("descarga 73", b, 1, 1, 73, 120, 1);
    b[0] = bat(1, 4400, 4000, 2, 2000, 1600);
    caso("carga 40", b, 1, 2, 40, 72, 2);
    b[0] = bat(0, 5000, 5000, 1, 0, 2500);
    caso("consumo 0", b, 1, 1, 50, -1, 1);
    b[0] = bat(0, 5000, 5000, 1, D, 2500);
    caso("consumo desconocido", b, 1, 1, 50, -1, 1);
    b[0] = bat(0, 5000, D, 1, 1000, 2500);
    caso("sin carga completa: la de diseno", b, 1, 1, 50, 150, 1);
    b[0] = bat(0, D, D, 1, 1000, 2500);
    caso("sin capacidades", b, 1, 1, -1, 150, 1);
    b[0] = bat(0, 5000, 4800, 0, 0, 4900);
    caso("restante mayor que la completa", b, 1, 2, 100, -1, 0);
    b[0] = bat(0, 5000, 5000, 3, 1000, 2500);
    caso("carga y descarga a la vez, enchufado", b, 1, 2, 50, 150, 2);
    b[0] = bat(0, 5000, 5000, 3, 1000, 2500);
    caso("carga y descarga a la vez, sin cargador", b, 1, 1, 50, 150, 1);
    b[0] = bat(0, 5000, 5000, 1, 1, 4000);
    caso("consumo absurdo: mas de un dia", b, 1, 1, 80, -1, 1);
    b[0] = bat(0, 5000, 5000, 1, 1000, 2500);
    caso("descargando con el cargador puesto: sin tiempo", b, 1, 2, 50, -1, 1);
    b[0] = bat(0, 5000, 5000, 1, 1000, 2500);
    b[1] = bat(0, 3000, 3000, 1, 500, 500);
    caso("dos baterias", b, 2, 1, 38, 120, 1);
    b[1].presente = 0;
    caso("dos, la segunda quitada", b, 2, 1, 50, 150, 1);
    b[1] = bat(1, 3000, 3000, 1, 500, 500);
    caso("dos de unidades distintas: solo la primera", b, 2, 1, 50, 150, 1);
    b[0].presente = 0;
    b[1].presente = 0;
    {
        info_bateria_t s;
        bateria_calcular(b, 2, 2, &s);
        int ok = !s.presente && s.porcentagem == -1 && s.minutos == -1;
        printf("%s sin bateria puesta\n", ok ? "ok" : "FALLO");
        if (!ok) falhas++;
    }
    b[0] = bat(0, 0xFFFFFFF0u, 0x7FFFFFFFu, 1, 0x7FFFFFFFu, 0x7FFFFFFFu);
    caso("valores absurdos", b, 1, 1, -1, -1, 1);
    char t[24];
    bateria_texto_tempo(59, t, sizeof t); if (strcmp(t, "59 min")) { printf("FALLO 59 min: %s\n", t); falhas++; }
    bateria_texto_tempo(605, t, sizeof t); if (strcmp(t, "10 h 05 min")) { printf("FALLO 10 h 05: %s\n", t); falhas++; }
    bateria_texto_tempo(-1, t, sizeof t); if (t[0]) { printf("FALLO -1: %s\n", t); falhas++; }
    bateria_texto_tempo(120, t, 4); if (strcmp(t, "2 h")) { printf("FALLO recorte: %s\n", t); falhas++; }
    printf("falhas %d\n", falhas);
    return falhas != 0;
}
'''


def anfitriao():
    os.makedirs(DIR, exist_ok=True)
    fonte = os.path.join(DIR, "h158_anfitriao.c")
    with open(fonte, "w") as f:
        f.write(PROGRAMA_ANFITRIAO)
    inc = os.path.join(RAIZ, "kernel", "inc")
    conv = os.path.join(RAIZ, "kernel", "lib", "bateria_conv.c")
    for nome_exe, extra in (("h158_anfitriao", ["-O2"]),
                            ("h158_anfitriao_asan", ["-O1", "-g", "-fsanitize=address,undefined",
                                                     "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome_exe)
        r = subprocess.run(["gcc", *extra, "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", inc, "-o", exe, fonte, conv],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, f"anfitrión: compila bateria_conv.c ({nome_exe})\n{r.stderr}")
        r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
        verificar(r.returncode == 0 and "falhas 0" in r.stdout, f"anfitrión: los casos de bateria_conv ({nome_exe})\n{r.stdout}")
    print("  anfitrión: porcentaje y tiempo correctos en todos los casos")


# ------------------------------------------------------------ uACPI en el anfitrión (0b)

def volcado_texto(tabelas):
    """Las tablas en el formato de texto de acpidump ("XXXX @ 0x..." y
    líneas "    0000: 44 53 ...")."""
    linhas = []
    for t in tabelas:
        linhas.append(f"{t[:4].decode('latin1')} @ 0x0000000000000000")
        for i in range(0, len(t), 16):
            linhas.append(f"    {i:04X}: " + " ".join(f"{b:02X}" for b in t[i:i + 16]))
        linhas.append("")
    return "\n".join(linhas) + "\n"


def compilar_volcado():
    """tools/uacpi_volcado.c con uACPI tal cual, para el PC."""
    exe = os.path.join(DIR, "uacpi_volcado")
    fontes = [os.path.join(RAIZ, "tools", "uacpi_volcado.c"), os.path.join(RAIZ, "kernel", "lib", "bateria_conv.c"),
              os.path.join(RAIZ, "kernel", "lib", "brilho_conv.c"),   # D403: --portatil
              os.path.join(RAIZ, "kernel", "dispositivos", "prt.c")]   # D211: el _PRT con --prt
    srcdir = os.path.join(RAIZ, "kernel", "terceros", "uacpi", "source")
    fontes += sorted(os.path.join(srcdir, f) for f in os.listdir(srcdir) if f.endswith(".c"))
    r = subprocess.run(["gcc", "-O1", "-g", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "terceros", "uacpi", "include"),
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe, *fontes], capture_output=True, text=True)
    verificar(r.returncode == 0, f"anfitrión: compila tools/uacpi_volcado.c con uACPI\n{r.stderr[-2000:]}")
    return exe


def anfitriao_volcados():
    """(0b) El intérprete en el PC: con una máquina mínima y la batería de la
    parte (a), y, con SAGUI_ACPIDUMPS=<carpeta>, con volcados de acpidump de
    portátiles reales (uACPI/ACPIDumps): ninguno puede colgarse ni caerse."""
    exe = compilar_volcado()
    dsdt = tabela("DSDT", "MINIMA", scope("\\_SB", device("PCI0", name("_HID", inteiro(eisaid("PNP0A03"))))))
    facp = bytearray(tabela("FACP", "MINIMA", bytes(244 - 36)))
    facp[8] = 4
    facp[9] = 0
    facp[9] = (-sum(facp)) & 0xFF
    arq = os.path.join(DIR, "volcado-minimo.txt")
    with open(arq, "w") as f:
        f.write(volcado_texto([dsdt, bytes(facp), ssdt_bateria(bix=BIX_73, bst=BST_73, psr=0)]))
    r = subprocess.run([exe, arq], capture_output=True, text=True, timeout=120)
    verificar(r.returncode == 0 and "SIN COLGARSE" in r.stdout, f"anfitrión: uACPI con la máquina mínima\n{r.stdout}{r.stderr}")
    verificar("BAT0 _BIX: ok (20 elementos)" in r.stdout and "porcentaje 73 tiempo '2 h 00 min'" in r.stdout,
              f"anfitrión: _BIX y _BST de la batería falsa dan 73 % y 2 h 00 min\n{r.stdout}")
    pasta = os.environ.get("SAGUI_ACPIDUMPS")
    if not pasta:
        print("  anfitrión: sin SAGUI_ACPIDUMPS, no se prueban volcados de máquinas reales")
        return
    arqs = sorted(os.path.join(pasta, f) for f in os.listdir(pasta) if f.endswith((".bin", ".txt", ".dat")))
    for a in arqs:
        r = subprocess.run([exe, a], capture_output=True, text=True, timeout=300)
        if r.returncode == 2 and "sin tablas" in r.stderr:
            print(f"  volcado {os.path.basename(a)}: no es un volcado de acpidump (se salta)")
            continue
        res = re.search(r"^resultado: .*$|^no hay bater.a.*$", r.stdout, re.M)
        fim = re.search(r"^fin: .*$", r.stdout, re.M)
        print(f"  volcado {os.path.basename(a)}: {res.group(0) if res else '-'}; {fim.group(0) if fim else '-'}")
        verificar(r.returncode == 0 and "SIN COLGARSE" in r.stdout,
                  f"anfitrión: {os.path.basename(a)} se interpreta sin colgarse ni caerse (código {r.returncode})\n"
                  f"{r.stdout[-3000:]}{r.stderr[-2000:]}")


# ------------------------------------------------------------ QEMU

def esperar(m, padrao, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def icones(m):
    g = re.findall(r"\[GUI\] bandeja (\d+)\+(\d+):([^\r\n]*)", m.serial())[-1]
    return {n: int(x) for n, x in re.findall(r"(\w+)@(\d+)", g[2])}


def terminou(m, tempo):
    try:
        m.proc.wait(timeout=tempo)
        return True
    except Exception:
        return False


def comum_uacpi(m, nome_caso):
    """uACPI arrancó, cargó las tablas (con la SSDT de la prueba) y encontró la batería."""
    s = esperar(m, r"\[UACPI\] listo en \d+ ms[^\r\n]*", 90)
    s = m.serial()
    verificar("EXCECAO" not in s and "PANICO" not in s.upper(), f"{nome_caso}: arranca sin excepciones")
    verificar('[UACPI] tarea "acpi-aml" creada' in s, f"{nome_caso}: la tarea acpi-aml existe")
    verificar("[UACPI] no se pudo cargar" not in s, f"{nome_caso}: uACPI carga las tablas")
    verificar(re.search(r"\[BATERIA\] bateria \(PNP0C0A\): \\_SB_?\.BAT0", s) is not None,
              f"{nome_caso}: encuentra \\_SB.BAT0")
    verificar(re.search(r"\[BATERIA\] cargador \(ACPI0003\): \\_SB_?\.ADP0", s) is not None,
              f"{nome_caso}: encuentra \\_SB.ADP0")


def bandeja_bat(m, nome_caso, pct, estado):
    g = esperar(m, rf"\[GUI\] bandeja: som \d+ volume \d+[^\r\n]*, bat {pct} e{estado} a\d", 90)
    verificar(True, f"{nome_caso}: la bandeja lee la batería ({g})")
    time.sleep(1.5)
    ic = icones(m)
    verificar("bat" in ic, f"{nome_caso}: el icono de la batería está en la bandeja ({ic})")
    verificar(ic["bat"] < ic["red"] < ic["vol"], f"{nome_caso}: batería, red y volumen, en ese orden ({ic})")
    return ic


def dica(m, ic):
    marca = len(m.serial())
    m.mouse_para(ic["bat"] + 8, 12)
    return esperar(m, r'bandeja: dica "(Bater[^"]*)"', 20, marca)


def imagem_uacpi():
    """La imagen de escritorio con el intérprete de AML (desactivado por defecto, D192)."""
    return imagem_de_teste("", ["--vesa", "--uacpi"])


def parte_a():
    ssdt = gravar_ssdt("a", ssdt_bateria(bix=BIX_73, bst=BST_73, psr=0))
    with Maquina("h158a", "ide", imagem=imagem_uacpi(), extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        comum_uacpi(m, "a")
        ic = bandeja_bat(m, "a", 73, 1)
        png = m.captura("H158-1-descargando")
        x = ic["bat"]
        verificar(cor_do_pixel(png, x + 1, IC_Y + 7) == CLARO, "a: el contorno de la batería está dibujado")
        verificar(cor_do_pixel(png, x + 3, IC_Y + 7) == VERDE, "a: con el 73 % el relleno es verde")
        verificar(cor_do_pixel(png, x + 11, IC_Y + 7) == FONDO_BARRA, "a: el relleno no llega al final (73 %)")
        verificar(cor_do_pixel(png, x + 8, IC_Y + 2) != AMARILLO, "a: descargando no hay rayo")
        t = dica(m, ic)
        verificar(t == "Batería: 73 % (quedan 2 h 00 min)", f"a: la ayuda emergente ({t!r})")
        marca = len(m.serial())
        m.clique(ic["bat"] + 8, 12)
        esperar(m, r"bandeja: painel bateria aberto em \(\d+,\d+\) \d+x\d+", 15, marca)
        time.sleep(0.8)
        m.captura("H158-2-panel")
        m.tecla("esc")
        time.sleep(0.5)
        out = m.comando("bateria -v")
        verificar("Batería: 73 % (descargando)" in out, f"a: `bateria` dice 73 % descargando\n{out}")
        verificar("Tiempo restante: 2 h 00 min" in out, "a: `bateria` da el tiempo restante")
        verificar("Capacidad: 3650 de 5000 mWh (diseño 5200 mWh)" in out, "a: `bateria` da la capacidad (_BIX)")
        verificar("Cargador: desenchufado" in out, "a: `bateria` dice que el cargador está desenchufado")
        verificar("Modelo: SAGUI-BAT (SaguiOS, LION, serie 0001)" in out, "a: el modelo, el fabricante y la serie de _BIX")
        verificar("Ciclos de carga: 42" in out, "a: los ciclos de _BIX")
        esperar(m, r"\[BATERIA\] aviso del firmware 80", 30)
        out = m.comando("bateria -v")
        g = re.search(r"avisos (\d+)", out)
        verificar(g and int(g.group(1)) >= 1, f"a: el Notify(BAT0, 0x80) del _BST llega al driver ({out.strip()})")
        out = m.comando("acpi")
        verificar(re.search(r"Apagado ACPI: disponible \(S5 = \d/\d, DSDT\)", out) is not None
                  and "Botón de encendido: atendido (SCI en la IRQ 9" in out, f"a: `acpi` sigue igual con uACPI\n{out}")
        marca = len(m.serial())
        m.digitar("apagar-pc\n", 0.05)
        esperar(m, r"\[ACPI\] S5: SLP_TYPa=\d SLP_TYPb=\d", 30, marca)
        verificar(terminou(m, 20), "a: con uACPI, apagar-pc sigue apagando QEMU (S5)")


def parte_b():
    ssdt = gravar_ssdt("b", ssdt_bateria(bif=BIF_40, bst=BST_40, psr=1))
    with Maquina("h158b", "ide", imagem=imagem_uacpi(), extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        comum_uacpi(m, "b")
        ic = bandeja_bat(m, "b", 40, 2)
        png = m.captura("H158-3-cargando")
        x = ic["bat"]
        verificar(cor_do_pixel(png, x + 8, IC_Y + 2) == AMARILLO, "b: cargando, el rayo amarillo")
        verificar(cor_do_pixel(png, x + 3, IC_Y + 7) == VERDE, "b: el relleno es verde")
        t = dica(m, ic)
        verificar(t == "Batería: 40 % (cargando, 1 h 12 min para llenarse)", f"b: la ayuda emergente ({t!r})")
        out = m.comando("bateria")
        verificar("Batería: 40 % (cargando)" in out and "Tiempo para llenarse: 1 h 12 min" in out,
                  f"b: `bateria` dice cargando\n{out}")
        verificar("Capacidad: 1600 de 4000 mAh (diseño 4400 mAh)" in out and "Cargador: enchufado" in out,
                  "b: capacidad en mAh (_BIF) y cargador enchufado")


def parte_c():
    ssdt = gravar_ssdt("c", ssdt_bateria(bif=BIF_8, bst=BST_8, psr=0))
    with Maquina("h158c", "ide", imagem=imagem_uacpi(), extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        comum_uacpi(m, "c")
        ic = bandeja_bat(m, "c", 8, 1)
        esperar(m, r"bandeja: aviso de bateria baja \(8 %\)", 30)
        esperar(m, r"notificacao: Bater.a baja: 8 %", 15)
        verificar(True, "c: por debajo del 10 % sale la notificación de batería baja")
        time.sleep(0.5)
        png = m.captura("H158-4-baja")
        verificar(cor_do_pixel(png, ic["bat"] + 3, IC_Y + 7) == ROJO, "c: por debajo del 15 % el relleno es rojo")
        time.sleep(12)   # otras dos lecturas de 5 s: no se repite
        verificar(m.serial().count("aviso de bateria baja") == 1, "c: el aviso sale una sola vez")


def parte_d():
    with Maquina("h158d", "ide", imagem=imagem_uacpi()) as m:
        m.esperar_serial("sagui:/$", 180)
        esperar(m, r"\[UACPI\] listo en \d+ ms", 90)
        esperar(m, r"\[BATERIA\] no hay bateria ni cargador ACPI", 30)
        time.sleep(2)
        ic = icones(m)
        verificar("bat" not in ic, f"d: sin batería no hay icono ({ic})")
        out = m.comando("bateria")
        verificar("No hay batería (el ACPI de este equipo no describe ninguna)" in out, f"d: `bateria` sin batería\n{out}")
        marca = len(m.serial())
        m.monitor("system_powerdown")
        esperar(m, r"\[ACPI\] botao de energia apertado", 15, marca)
        verificar(terminou(m, 25), "d: con uACPI, el botón de encendido (system_powerdown) sigue apagando")


def parte_e():
    ssdt = gravar_ssdt("e", ssdt_bateria(bix=BIX_73, bst=BST_73, psr=0))
    img = imagem_de_teste("", ["--vesa", "--console"])   # sin --uacpi: lo de por defecto
    with Maquina("h158e", "ide", imagem=img, extras=["-acpitable", f"file={ssdt}"]) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        s = m.serial()
        verificar("[UACPI] desactivado (la imagen no lleva la opcion --uacpi)" in s, "e: sin --uacpi, uACPI no arranca")
        verificar("acpi-aml" not in s and "[BATERIA]" not in s, "e: ni tarea acpi-aml ni driver de batería")
        out = m.comando("bateria")
        verificar("Batería: no disponible (desactivado (imagen sin --uacpi))" in out, f"e: `bateria`\n{out}")
        out = m.comando("acpi")
        verificar("Apagado ACPI: disponible" in out, "e: el apagado ACPI no cambia")
        marca = len(m.serial())
        m.digitar("apagar-pc\n", 0.05)
        esperar(m, r"\[ACPI\] S5: SLP_TYPa=\d SLP_TYPb=\d", 30, marca)
        verificar(terminou(m, 20), "e: apagar-pc apaga QEMU")


def main():
    try:
        anfitriao()
        anfitriao_volcados()
        if os.environ.get("SAGUI_SO_ANFITRIAO") != "1":
            for parte in (parte_a, parte_b, parte_c, parte_d, parte_e):
                parte()
                print(f"  {parte.__name__}: ok")
    except (FalhaTeste, AssertionError) as e:
        print(f"FALHOU: {e}")
        return 1
    print("H158 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
