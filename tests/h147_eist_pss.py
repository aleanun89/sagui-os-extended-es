#!/usr/bin/env python3
"""H147 (D177) - EIST de los Intel antiguos, fases A y B (solo lectura).

Criterio:
  - en el anfitrión, el lector kernel/lib/acpi_pss.c (compilado con
    -DSAGUI_HOST, también con -fsanitize=address,undefined):
      * tablas AML construidas aquí (sin iasl): _PSS/_PSD/_PCT estáticos en
        Processor(), Device ACPI0007 con Method(_PSS) que elige NPSS/SPSS,
        el esquema de SSDT dinámicas de Intel (paquete \\SSDT, _PDC con
        OperationRegion(DerefOf(Index(SSDT, n))) y Load()), SSDT dinámica en
        memoria que no es del firmware o con suma errónea (rechazada),
        Return(Local0) (ilegible), _PCT por SystemIO, VID que no cuadra,
        dominios con tablas distintas, procesador ausente de la MADT, y un
        Core 2 con IDA y SuperLFM (valores del ThinkPad T61);
      * las SSDT de CPU reales de un Dell Inspiron 6400 (Core Duo T2x00,
        chipset 945, la misma época que el Amilo; tests/datos/acpi/,
        CC-BY-4.0, uACPI/ACPIDumps): CpuPm en la RSDT, Cpu0Ist y Cpu1Ist
        cargadas por dirección; 3 estados 1833/1333/1000 MHz;
      * mil quinientas tablas estropeadas al azar: ninguna lectura fuera de sitio;
      * con SAGUI_ACPIDUMPS=<carpeta> (volcados completos de acpidump), también
        la DSDT real entera (SAGUI_PSS_CPU="modelo:0x0E status:0x... tsc:kHz"
        para validar con los MSR de esa máquina; ver la D177);
  - en QEMU (sin y con --smp y -smp 2): arranca hasta la shell, el log dice
    «[PSS] ... sin _PSS en ningun procesador» y «[EIST] sin EIST antiguo»,
    sin excepciones; las tablas de QEMU, volcadas con pmemsave y leídas en
    el anfitrión, dan los mismos procesadores sin _PSS.
"""
import os
import random
import re
import struct
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

DADOS = os.path.join(RAIZ, "tests", "datos", "acpi")

HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "acpi_pss.h"

typedef struct { uint32_t fis, tam; int reservado; uint8_t *orig, *mem; } seg_t;
static seg_t seg[64];
static int nseg;
static uint32_t raizes[32];
static int nraiz;

static const uint8_t *mapear(uint32_t fis, uint32_t tam, bool raiz)
{
    for (int i = 0; i < nseg; i++)
        if (fis >= seg[i].fis && (uint64_t)fis + tam <= (uint64_t)seg[i].fis + seg[i].tam) {
            if (!raiz && !seg[i].reservado) return NULL;
            return seg[i].mem + (fis - seg[i].fis);
        }
    return NULL;
}

static void imprimir(acpi_pss_t *r, uint32_t fiaveis)
{
    printf("geral tabelas=%u;dinamicas=%u;rejeitadas=%u;incompleto=%u;perdidos=%u;pss_bytes=%u;pss_lidos=%u;cpus=%u;total=%u;fiaveis=%u\n",
           r->tabelas, r->dinamicas, r->rejeitadas, r->aml_incompleto, r->objetos_perdidos, r->pss_bytes, r->pss_lidos,
           r->qtd_cpus, r->cpus_total, fiaveis);
    for (uint32_t i = 0; i < r->tabelas; i++)
        printf("tab%u %s;%s;%u;%u\n", i, r->tabela[i].assinatura, r->tabela[i].oem, r->tabela[i].tam, r->tabela[i].dinamica);
    for (uint32_t i = 0; i < r->qtd_cpus; i++) {
        pss_cpu_t *c = &r->cpu[i];
        char mhz[200] = "", ctl[200] = "";
        int a = 0, b = 0;
        for (uint32_t k = 0; k < c->qtd; k++) {
            a += snprintf(mhz + a, sizeof mhz - a, "%s%u", k ? "/" : "", c->est[k].mhz);
            b += snprintf(ctl + b, sizeof ctl - b, "%s%x", k ? "/" : "", c->est[k].controle);
        }
        printf("cpu%u caminho=%s;id=%u;tipo=%u;pss=%u;var=%u;din=%u;incerto=%u;de=%s;tab=%s;qtd=%u;mhz=%s;ctl=%s;"
               "psd=%u;psdtipos=%u;dom=%u;num=%u;pct=%u;esp=%u;fiavel=%u;turbo=%u;slfm=%u;ausente=%u;motivo=%s\n",
               i, c->caminho, c->id, c->tipo, c->pss_origem, c->pss_variantes, c->pss_dinamica, c->pss_incerto, c->pss_de,
               c->pss_tabela, c->qtd, mhz, ctl, c->psd_variantes, c->psd_tipos, c->psd_dominio, c->psd_num,
               c->pct_variantes, c->pct_espacos, c->fiavel, c->turbo, c->slfm, c->ausente, c->motivo);
    }
}

static uint8_t *ler_arquivo(const char *n, uint32_t *tam)
{
    FILE *f = fopen(n, "rb");
    if (!f) { perror(n); exit(2); }
    fseek(f, 0, SEEK_END);
    long t = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(t ? t : 1);
    if (fread(b, 1, t, f) != (size_t)t) exit(2);
    fclose(f);
    *tam = (uint32_t)t;
    return b;
}

static void corrigir_soma(uint8_t *t, uint32_t tam)
{
    if (tam < 36) return;
    uint32_t n = t[4] | t[5] << 8 | t[6] << 16 | (uint32_t)t[7] << 24;
    if (n > tam) n = tam;
    t[9] = 0;
    uint8_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += t[i];
    t[9] = (uint8_t)-s;
}

int main(int argc, char **argv)
{
    pss_cpu_info_t in;
    memset(&in, 0, sizeof in);
    long fuzz = 0;
    unsigned semente = 1;
    for (int i = 1; i < argc; i++) {
        char *a = argv[i];
        if (!strncmp(a, "mem:", 4)) {
            seg_t *s = &seg[nseg++];
            s->reservado = a[4] == 'D';
            char *p = a + 6;
            s->fis = (uint32_t)strtoul(p, &p, 0);
            s->orig = ler_arquivo(p + 1, &s->tam);
            s->mem = malloc(s->tam);
            memcpy(s->mem, s->orig, s->tam);
        } else if (!strncmp(a, "raiz:", 5)) raizes[nraiz++] = (uint32_t)strtoul(a + 5, NULL, 0);
        else if (!strncmp(a, "modelo:", 7)) { in.modelo = (uint32_t)strtoul(a + 7, NULL, 0); in.familia = 6;
            in.intel_fidvid = in.modelo == 0x09 || in.modelo == 0x0D || in.modelo == 0x0E || pss_core2(in.modelo); }
        else if (!strncmp(a, "status:", 7)) { in.perf_status = strtoull(a + 7, NULL, 0); in.tem_status = true; }
        else if (!strncmp(a, "tsc:", 4)) in.tsc_khz = (uint32_t)strtoul(a + 4, NULL, 0);
        else if (!strncmp(a, "presentes:", 10)) {
            char *p = a + 10;
            while (*p) { in.presentes[in.qtd_presentes++] = (uint8_t)strtoul(p, &p, 0); if (*p == ',') p++; }
        } else if (!strncmp(a, "fuzz:", 5)) { char *p; semente = (unsigned)strtoul(a + 5, &p, 0); fuzz = strtol(p + 1, NULL, 0); }
    }
    static acpi_pss_t r;
    if (!fuzz) {
        acpi_pss_ler(&r, raizes, (uint32_t)nraiz, mapear);
        imprimir(&r, acpi_pss_validar(&r, &in));
        return 0;
    }
    /* Tablas estropeadas: cada segmento en memoria del tamaño justo (ASan ve
     * cualquier lectura fuera), bytes cambiados después de la cabecera (y la
     * suma corregida, para que el lector entre), longitudes recortadas. */
    srand(semente);
    unsigned long total = 0;
    for (long it = 0; it < fuzz; it++) {
        for (int i = 0; i < nseg; i++) {
            seg_t *s = &seg[i];
            uint32_t tam = s->tam;
            int modo = rand() % 4;
            if (modo == 3 && tam > 40) tam = 36 + (uint32_t)rand() % (tam - 36);
            free(s->mem);
            s->mem = malloc(tam);
            memcpy(s->mem, s->orig, tam);
            if (tam > 36) {
                int n = 1 + rand() % 12;
                for (int k = 0; k < n; k++) {
                    uint32_t o = 36 + (uint32_t)rand() % (tam - 36);
                    s->mem[o] = (modo == 1) ? (uint8_t)(s->mem[o] ^ (1u << (rand() % 8))) : (uint8_t)rand();
                }
                if (modo == 3) {   /* la cabecera dice la longitud recortada */
                    s->mem[4] = (uint8_t)tam; s->mem[5] = (uint8_t)(tam >> 8); s->mem[6] = (uint8_t)(tam >> 16); s->mem[7] = 0;
                }
                if (!memcmp(s->mem, "SSDT", 4) || !memcmp(s->mem, "DSDT", 4)) corrigir_soma(s->mem, tam);
            }
            s->tam = tam;
        }
        acpi_pss_ler(&r, raizes, (uint32_t)nraiz, mapear);
        total += acpi_pss_validar(&r, &in) + r.tabelas;
        for (int i = 0; i < nseg; i++) {   /* vuelve el tamaño original para la próxima vuelta */
            seg_t *s = &seg[i];
            uint32_t n = s->orig[4] | s->orig[5] << 8 | s->orig[6] << 16 | (uint32_t)s->orig[7] << 24;
            s->tam = n >= 36 && n <= 0x100000 ? n : s->tam;
        }
    }
    printf("fuzz=ok vueltas=%ld suma=%lu\n", fuzz, total);
    return 0;
}
"""

# ------------------------------------------------------------ constructor de AML


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


def dword(v):
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


def processor(n, pid, *termos):
    corpo = nome(n) + bytes([pid]) + struct.pack("<I", 0x1010) + bytes([6]) + b"".join(termos)
    return b"\x5b\x83" + pkglen(len(corpo)) + corpo


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


def e_bits(a, b):   # And(a, b) sin destino
    return b"\x7b" + a + b + b"\x00"


def regiao(n, espaco, off, tam):
    return b"\x5b\x80" + nome(n) + bytes([espaco]) + off + tam


def load(n, alvo):
    return b"\x5b\x20" + nome(n) + nome(alvo)


def deref_index(n, k):
    return b"\x83\x88" + nome(n) + inteiro(k) + b"\x00"


def registro(espaco, largura=0, endereco=0):
    gr = bytes([0x82, 0x0C, 0x00, espaco, largura, 0, 0]) + struct.pack("<Q", endereco) + b"\x79\x00"
    corpo = inteiro(len(gr)) + gr
    return b"\x11" + pkglen(len(corpo)) + corpo


def pct(espaco):
    if espaco == "ffh":
        return pacote(registro(0x7F), registro(0x7F))
    return pacote(registro(0x01, 16, 0xB2), registro(0x01, 8, 0xB3))


def pss(estados):
    return pacote(*[pacote(*[dword(v) for v in e]) for e in estados])


def psd(dom, tipo, num):
    return pacote(pacote(inteiro(5), inteiro(0), inteiro(dom), inteiro(tipo), inteiro(num)))


def tabela(assin, oem_id, corpo):
    tam = 36 + len(corpo)
    cab = bytearray(assin.encode() + struct.pack("<I", tam) + bytes([1, 0]) + b"SAGUI " +
                    oem_id.encode().ljust(8, b" ") + struct.pack("<I", 1) + b"SAGI" + struct.pack("<I", 1))
    t = cab + corpo
    t[9] = (-sum(t)) & 0xFF
    return bytes(t)


# Core Duo T2400 como el del Amilo: 1833/1333/1000 MHz (bus de 166 MHz).
YONAH = [(1833, 31000, 10, 10, 0x0B2C, 0x0B2C), (1333, 20000, 10, 10, 0x081D, 0x081D), (1000, 13000, 10, 10, 0x0613, 0x0613)]
# El SPSS de Intel: Control = índice para el SMI (no se puede escribir en el MSR).
YONAH_SMM = [(1833, 31000, 110, 10, 0x83, 0), (1333, 20000, 110, 10, 0x183, 1), (1000, 13000, 110, 10, 0x283, 2)]
# ThinkPad T61 (Core 2 T7300, Merom): IDA en P0 (2201 MHz) y SuperLFM en el último (bit 15).
T61 = [(2201, 35000, 10, 10, 0x0C2F, 0x0C2F), (2200, 35000, 10, 10, 0x0B2B, 0x0B2B), (1600, 23000, 10, 10, 0x081C, 0x081C),
       (1200, 16000, 10, 10, 0x0612, 0x0612), (800, 14000, 10, 10, 0x880B, 0x880B)]

PSD_SW_ALL, PSD_HW_ALL = 0xFC, 0xFE


class Caso:
    """Tablas en memoria: (dirección, bytes, reservada) y cuáles son raíz."""

    def __init__(self, nome_caso):
        self.nome = nome_caso
        self.segs = []
        self.raizes = []

    def raiz(self, fis, dados):
        self.segs.append((fis, dados, False))
        self.raizes.append(fis)
        return self

    def memoria(self, fis, dados, reservada=True):
        self.segs.append((fis, dados, reservada))
        return self


DIR = None
EXE = None
EXE_ASAN = None


def compilar():
    global DIR, EXE, EXE_ASAN
    os.makedirs(SAIDA, exist_ok=True)
    DIR = tempfile.mkdtemp(dir=SAIDA, prefix="h147-")
    c = os.path.join(DIR, "harness.c")
    with open(c, "w") as f:
        f.write(HARNESS)
    fonte = os.path.join(RAIZ, "kernel", "lib", "acpi_pss.c")
    inc = os.path.join(RAIZ, "kernel", "inc")
    EXE = os.path.join(DIR, "harness")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", inc, "-o", EXE, c, fonte],
                       capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, f"acpi_pss.c compila en el anfitrión {r.stderr[:400]}")
    EXE_ASAN = os.path.join(DIR, "harness_asan")
    r = subprocess.run(["gcc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-DSAGUI_HOST",
                        "-I", inc, "-o", EXE_ASAN, c, fonte], capture_output=True, text=True)
    verificar(r.returncode == 0, f"acpi_pss.c compila con ASan/UBSan {r.stderr[:400]}")


def rodar(caso, *cpu, exe=None):
    args = [exe or EXE]
    for i, (fis, dados, res) in enumerate(caso.segs):
        arq = os.path.join(DIR, f"{re.sub(r'[^A-Za-z0-9]', '_', caso.nome)}-{i}.bin")
        with open(arq, "wb") as f:
            f.write(dados)
        args.append(f"mem:{'D' if res else 'R'}:{fis:#x}:{arq}")
    args += [f"raiz:{f:#x}" for f in caso.raizes] + list(cpu)
    r = subprocess.run(args, capture_output=True, text=True, timeout=600)
    verificar(r.returncode == 0, f"{caso.nome}: el lector termina bien (rc {r.returncode}) {r.stderr[-1500:]}")
    return r.stdout


def analisar(saida):
    geral, cpus, tabs = {}, [], []
    for linha in saida.splitlines():
        if linha.startswith("geral "):
            geral = dict(x.split("=", 1) for x in linha[6:].split(";"))
        elif linha.startswith("cpu"):
            cpus.append(dict(x.split("=", 1) for x in linha.split(" ", 1)[1].split(";")))
        elif linha.startswith("tab"):
            tabs.append(linha.split(" ", 1)[1].split(";"))
    return geral, cpus, tabs


def esperar(caso, cpu_args, geral_esp, cpus_esp):
    g, cpus, _ = analisar(rodar(caso, *cpu_args))
    erros = [f"geral.{k}: {g.get(k)} != {v}" for k, v in geral_esp.items() if g.get(k) != str(v)]
    for i, esp in enumerate(cpus_esp):
        if i >= len(cpus):
            erros.append(f"falta cpu{i}")
            continue
        for k, v in esp.items():
            obtido = cpus[i].get(k)
            if k == "motivo" and isinstance(v, str):
                if v not in (obtido or ""):
                    erros.append(f"cpu{i}.motivo: {obtido!r} sin {v!r}")
            elif obtido != str(v):
                erros.append(f"cpu{i}.{k}: {obtido} != {v}")
    verificar(not erros, f"{caso.nome} {erros}")
    return g, cpus


YONAH_CPU = ["modelo:0x0E", "status:0x0B2C", "tsc:1833000"]


def dsdt_pr(*procs):
    return tabela("DSDT", "SAGUIPR", scope("\\_PR", *procs))


def casos_sinteticos():
    # 1. Como QEMU: un Device ACPI0007 sin _PSS.
    d = tabela("DSDT", "QEMU", scope("\\_SB", device("C000", name("_HID", cadeia("ACPI0007")), name("_UID", inteiro(0)))))
    esperar(Caso("qemu_sin_pss").raiz(0x100000, d), YONAH_CPU, {"cpus": 1, "fiaveis": 0, "tabelas": 1},
            [{"pss": 0, "tipo": 2, "caminho": "\\_SB_.C000", "fiavel": 0, "motivo": "sin _PSS"}])

    # 2. Estáticos en Processor(): CPU0 con Name(_PSS), CPU1 con Return(\_PR.CPU0._PSS).
    d = dsdt_pr(processor("CPU0", 0, name("_PSS", pss(YONAH)), name("_PSD", psd(0, PSD_SW_ALL, 2)), name("_PCT", pct("ffh"))),
                processor("CPU1", 1, method("_PSS", 0, ret(nome("\\_PR.CPU0._PSS"))),
                          method("_PSD", 0, ret(psd(0, PSD_SW_ALL, 2))), method("_PCT", 0, ret(pct("ffh")))))
    estatico = Caso("estatico").raiz(0x100000, d)
    esperar(estatico, YONAH_CPU, {"cpus": 2, "fiaveis": 2, "incompleto": 0, "pss_bytes": 2, "pss_lidos": 2},
            [{"pss": 1, "qtd": 3, "mhz": "1833/1333/1000", "ctl": "b2c/81d/613", "esp": 1, "psdtipos": 1, "num": 2,
              "fiavel": 1, "de": "\\_PR_.CPU0._PSS", "tab": "SAGUIPR"},
             {"pss": 2, "qtd": 3, "fiavel": 1, "de": "\\_PR_.CPU0._PSS"}])
    # La misma tabla, otra CPU actual (estado más lento): sigue fiable.
    esperar(estatico, ["modelo:0x0E", "status:0x0613", "tsc:1833000"], {"fiaveis": 2}, [])
    # El VID actual no es el de la tabla: no fiable.
    esperar(estatico, ["modelo:0x0E", "status:0x0B2D", "tsc:1833000"], {"fiaveis": 0},
            [{"fiavel": 0, "motivo": "VID actual"}])
    # El FID actual no está en la tabla.
    esperar(estatico, ["modelo:0x0E", "status:0x0A28", "tsc:1833000"], {"fiaveis": 0}, [{"motivo": "FID actual"}])
    # TSC de otra CPU (2,0 GHz): el estado 0 no es el máximo.
    esperar(estatico, ["modelo:0x0E", "status:0x0B2C", "tsc:2000000"], {"fiaveis": 0}, [{"motivo": "TSC"}])
    # Una CPU que no es de estas (Nehalem): las tablas se leen pero no valen.
    esperar(estatico, ["modelo:0x1A", "status:0x0B2C"], {"fiaveis": 0}, [{"motivo": "no es un Intel con FID/VID"}])
    # Solo el procesador 0 en la MADT: el 1 queda ausente y no cuenta.
    esperar(estatico, YONAH_CPU + ["presentes:0"], {"fiaveis": 1}, [{"fiavel": 1}, {"ausente": 1, "fiavel": 0}])

    # 3. Device ACPI0007 y Method(_PSS) que elige NPSS (nativo) o SPSS (SMI): se toma el nativo.
    met = method("_PSS", 0, se(e_bits(nome("PDC0"), inteiro(1)), ret(nome("NPSS"))), ret(nome("SPSS")))
    metpct = method("_PCT", 0, se(e_bits(nome("PDC0"), inteiro(1)), ret(pct("ffh"))), ret(pct("io")))
    metpsd = method("_PSD", 0, se(e_bits(nome("PDC0"), inteiro(0x800)), ret(psd(0, PSD_HW_ALL, 2))), ret(psd(0, PSD_SW_ALL, 2)))
    cpu = lambda n, u: device(n, name("_HID", cadeia("ACPI0007")), name("_UID", inteiro(u)), name("SPSS", pss(YONAH_SMM)),
                              name("NPSS", pss(YONAH)), met, metpct, metpsd)
    d = tabela("DSDT", "SAGUIDEV", name("PDC0", dword(0x80000000)) + scope("\\_SB", cpu("CP00", 0), cpu("CP01", 1)))
    esperar(Caso("device_npss").raiz(0x100000, d), YONAH_CPU, {"cpus": 2, "fiaveis": 2},
            [{"tipo": 2, "pss": 2, "var": 2, "de": "\\_SB_.CP00.NPSS", "esp": 3, "psdtipos": 5, "fiavel": 1, "id": 0},
             {"tipo": 2, "id": 1, "fiavel": 1, "de": "\\_SB_.CP01.NPSS"}])

    # 4. SSDT dinámicas (esquema de Intel): la DSDT declara los Processor; una
    # SSDT de la RSDT tiene el paquete \SSDT y el _PDC que hace Load().
    ist0 = tabela("SSDT", "Cpu0Ist", scope("\\_PR.CPU0", name("_PSS", pss(YONAH)), name("_PSD", psd(0, PSD_SW_ALL, 2)),
                                             name("_PCT", pct("ffh"))))
    ist1 = tabela("SSDT", "Cpu1Ist", scope("\\_PR.CPU1", method("_PSS", 0, ret(nome("\\_PR.CPU0._PSS"))),
                                             method("_PSD", 0, ret(psd(0, PSD_SW_ALL, 2))), method("_PCT", 0, ret(pct("ffh")))))
    A0, A1 = 0x3FED4176, 0x3FED4400

    def pdc(cpu, k, hi):
        return scope(f"\\_PR.{cpu}", name(hi, inteiro(0)),
                     method("_PDC", 1, se(e_bits(nome("CFGD"), inteiro(1)),
                                          regiao(f"IST{k}", 0, deref_index("SSDT", 3 * k + 1), deref_index("SSDT", 3 * k + 2)),
                                          load(f"IST{k}", hi))))
    cpupm = tabela("SSDT", "CpuPm", scope("\\", name("SSDT", pacote(cadeia("CPU0IST "), dword(A0), dword(len(ist0)),
                                                                     cadeia("CPU1IST "), dword(A1), dword(len(ist1)))),
                                              name("CFGD", dword(0x113F69F1))) + pdc("CPU0", 0, "HI0") + pdc("CPU1", 1, "HI1"))
    dsdt = dsdt_pr(processor("CPU0", 0), processor("CPU1", 1))
    dinamico = lambda nome_caso: Caso(nome_caso).raiz(0x100000, dsdt).raiz(0x110000, cpupm)
    esperar(dinamico("dinamico").memoria(A0, ist0).memoria(A1, ist1), YONAH_CPU,
            {"tabelas": 4, "dinamicas": 2, "rejeitadas": 0, "fiaveis": 2, "incompleto": 0},
            [{"pss": 1, "din": 1, "tab": "Cpu0Ist", "fiavel": 1}, {"pss": 2, "din": 1, "tab": "Cpu0Ist", "fiavel": 1}])
    # La misma SSDT en memoria que el firmware no reservó (E820 tipo 1): no se lee.
    esperar(dinamico("dinamico_ram").memoria(A0, ist0, reservada=False).memoria(A1, ist1, reservada=False), YONAH_CPU,
            {"dinamicas": 0, "rejeitadas": 2, "fiaveis": 0}, [{"pss": 0}, {"pss": 0}])
    # Con la suma de verificación mal: rechazada.
    mala = bytearray(ist0)
    mala[9] ^= 0x55
    esperar(dinamico("dinamico_suma").memoria(A0, bytes(mala)).memoria(A1, ist1), YONAH_CPU,
            {"dinamicas": 1, "rejeitadas": 1, "fiaveis": 0},
            [{"pss": 0}, {"pss": 3, "motivo": "ilegible"}])
    # Sin nada en esas direcciones: rechazadas, nada se inventa.
    esperar(dinamico("dinamico_vacio"), YONAH_CPU, {"dinamicas": 0, "rejeitadas": 2, "fiaveis": 0}, [{"pss": 0}, {"pss": 0}])

    # 5. _PSS calculado: Return(Local0) -> ilegible.
    d = dsdt_pr(processor("CPU0", 0, method("_PSS", 0, b"\x70" + pss(YONAH) + b"\x60", ret(b"\x60")), name("_PCT", pct("ffh"))))
    esperar(Caso("calculado").raiz(0x100000, d), YONAH_CPU, {"fiaveis": 0}, [{"pss": 3, "motivo": "ilegible"}])

    # 6. _PCT solo por SystemIO (SMI de la BIOS): no se escribe en el MSR.
    d = dsdt_pr(processor("CPU0", 0, name("_PSS", pss(YONAH)), name("_PCT", pct("io"))))
    esperar(Caso("pct_io").raiz(0x100000, d), YONAH_CPU, {"fiaveis": 0}, [{"esp": 2, "motivo": "FFixedHW"}])

    # 7. Dos procesadores del mismo dominio con tablas distintas: ninguno fiable.
    otro = [YONAH[0], YONAH[2]]
    d = dsdt_pr(processor("CPU0", 0, name("_PSS", pss(YONAH)), name("_PSD", psd(0, PSD_SW_ALL, 2)), name("_PCT", pct("ffh"))),
                processor("CPU1", 1, name("_PSS", pss(otro)), name("_PSD", psd(0, PSD_SW_ALL, 2)), name("_PCT", pct("ffh"))))
    esperar(Caso("dominio").raiz(0x100000, d), YONAH_CPU, {"fiaveis": 0},
            [{"fiavel": 0, "motivo": "mismo dominio"}, {"fiavel": 0, "motivo": "mismo dominio"}])

    # 8. Frecuencias que suben: no fiable.
    d = dsdt_pr(processor("CPU0", 0, name("_PSS", pss(list(reversed(YONAH)))), name("_PCT", pct("ffh"))))
    esperar(Caso("crecientes").raiz(0x100000, d), YONAH_CPU, {"fiaveis": 0}, [{"motivo": "no decrecientes"}])

    # 9. Core 2 (T61): IDA en P0 y SuperLFM en el último; PERF_STATUS en P1 con
    # el máximo (bits 44:40) = 11.
    d = dsdt_pr(processor("CPU0", 0, name("_PSS", pss(T61)), name("_PSD", psd(0, PSD_SW_ALL, 2)), name("_PCT", pct("ffh"))))
    t61 = Caso("t61").raiz(0x100000, d)
    st = (0x0B << 40) | 0x0B2B
    esperar(t61, ["modelo:0x0F", f"status:{st:#x}", "tsc:2200000"], {"fiaveis": 1},
            [{"qtd": 5, "turbo": 1, "slfm": 1, "fiavel": 1}])
    esperar(t61, ["modelo:0x0F", "status:0x880B", "tsc:2200000"], {"fiaveis": 1}, [{"fiavel": 1}])
    # En un Core Duo no hay IDA ni SuperLFM: la misma tabla no es fiable.
    esperar(t61, ["modelo:0x0E", "status:0x0B2B"], {"fiaveis": 0}, [{"fiavel": 0, "turbo": 0}])
    return estatico, dinamico("fuzz").memoria(A0, ist0).memoria(A1, ist1)


def caso_inspiron():
    """Las SSDT de CPU reales del Dell Inspiron 6400 (uACPI/ACPIDumps, CC-BY-4.0)."""
    d = os.path.join(DADOS, "inspiron6400")
    ler = lambda n: open(os.path.join(d, n), "rb").read()
    cpupm, ist0, ist1 = ler("ssdt-cpupm.aml"), ler("ssdt-cpu0ist.aml"), ler("ssdt-cpu1ist.aml")
    # Las direcciones a las que apunta el paquete \SSDT de la propia CpuPm.
    trios = {m.group(1).decode().strip(): (struct.unpack("<I", m.group(2))[0], struct.unpack("<I", m.group(3))[0])
             for m in re.finditer(rb"\x0d([A-Z0-9]{4}[A-Z]{3} )\x00\x0c(.{4})\x0c(.{4})", cpupm, re.S)}
    verificar(trios.get("CPU0IST", (0, 0))[1] == len(ist0) and trios.get("CPU1IST", (0, 0))[1] == len(ist1),
              f"Inspiron 6400: el paquete SSDT de CpuPm apunta a Cpu0Ist y Cpu1Ist ({trios})")
    # La DSDT real no se guarda en el repositorio: una mínima con los mismos Processor (CPU0 id 0, CPU1 id 1).
    caso = (Caso("inspiron6400").raiz(0x100000, dsdt_pr(processor("CPU0", 0), processor("CPU1", 1))).raiz(0x110000, cpupm)
            .memoria(trios["CPU0IST"][0], ist0).memoria(trios["CPU1IST"][0], ist1))
    esp_cpu0 = {"pss": 1, "din": 1, "tab": "Cpu0Ist", "qtd": 3, "mhz": "1833/1333/1000", "ctl": "b2c/81d/613", "esp": 3,
                "psd": 3, "psdtipos": 5, "dom": 0, "fiavel": 1, "de": "\\_PR_.CPU0._PSS"}
    esp_cpu1 = {"pss": 2, "din": 1, "qtd": 3, "esp": 1, "psd": 2, "fiavel": 1, "de": "\\_PR_.CPU0._PSS"}
    g, cpus = esperar(caso, YONAH_CPU + ["presentes:0,1"],
                      {"tabelas": 4, "dinamicas": 2, "rejeitadas": 2, "fiaveis": 2, "incompleto": 0}, [esp_cpu0, esp_cpu1])
    print(f"  Inspiron 6400: {cpus[0]['mhz']} MHz, Control {cpus[0]['ctl']}, {g['dinamicas']} SSDT cargadas, "
          f"{g['rejeitadas']} rechazadas (las CST, que no están en los datos)")
    esperar(caso, ["modelo:0x0E", "status:0x081D", "tsc:1833000"], {"fiaveis": 2}, [])
    esperar(caso, ["modelo:0x0E", "status:0x0B30", "tsc:1833000"], {"fiaveis": 0}, [{"motivo": "VID actual"}])
    return caso


def caso_volcados():
    """Opcional: volcados completos (texto de acpidump) en SAGUI_ACPIDUMPS."""
    pasta = os.environ.get("SAGUI_ACPIDUMPS")
    if not pasta or not os.path.isdir(pasta):
        print("  (sin SAGUI_ACPIDUMPS: no se leen DSDT reales completas)")
        return
    for arq in sorted(os.listdir(pasta)):
        tabs, atual = [], None
        for linha in open(os.path.join(pasta, arq), errors="replace"):
            m = re.match(r"^(\w{4}) @ 0x[0-9A-Fa-f]+", linha)
            if m:
                atual = bytearray()
                tabs.append(atual)
                continue
            m = re.match(r"^\s+[0-9A-Fa-f]{4,8}: ((?:[0-9A-Fa-f]{2} ){1,16})", linha)
            if m and atual is not None:
                atual += bytes.fromhex(m.group(1).replace(" ", ""))
        aml = [bytes(t) for t in tabs if t[:4] in (b"DSDT", b"SSDT")]
        if not aml:
            continue
        trios = {}
        for t in aml:
            for m in re.finditer(rb"\x0d([A-Za-z0-9]{3,7}[A-Za-z ]?)\x00\x0c(.{4})\x0c(.{4})", t, re.S):
                trios[m.group(1).decode().strip().upper()] = (struct.unpack("<I", m.group(2))[0], struct.unpack("<I", m.group(3))[0])
        caso = Caso(f"volcado_{arq}")
        fis = 0x100000
        for t in sorted(aml, key=lambda t: t[:4] != b"DSDT"):
            oem = t[16:24].rstrip(b"\0 ").decode(errors="replace").upper()
            if oem in trios and trios[oem][1] == len(t):
                caso.memoria(trios[oem][0], t)
            else:
                caso.raiz(fis, t)
                fis += 0x20000
        # SAGUI_PSS_CPU="modelo:0x0E status:0x... tsc:1833000": los valores de la máquina real (rdmsr 0x198).
        cpu = os.environ.get("SAGUI_PSS_CPU", " ".join(YONAH_CPU)).split()
        g, cpus, tabs_lidas = analisar(rodar(caso, *cpu))
        print(f"  volcado {arq}: {g} ")
        for c in cpus:
            print(f"    {c['caminho']}: pss={c['pss']} de={c['de']} mhz={c['mhz']} ctl={c['ctl']} psd={c['psdtipos']} "
                  f"pct={c['esp']} fiavel={c['fiavel']} {c['motivo']}")
        verificar(int(g["fiaveis"]) >= 1, f"volcado {arq}: al menos un procesador con _PSS fiable")
        saida = rodar(caso, *cpu, "fuzz:7:300", exe=EXE_ASAN)
        verificar("fuzz=ok" in saida, f"volcado {arq}: 300 vueltas con la DSDT real estropeada (ASan/UBSan)")


def fuzz(*casos):
    for i, caso in enumerate(casos):
        saida = rodar(caso, *YONAH_CPU, f"fuzz:{1000 + i}:750", exe=EXE_ASAN)
        verificar("fuzz=ok" in saida, f"{caso.nome}: 750 tablas estropeadas sin lecturas fuera de sitio (ASan/UBSan)")


def anfitriao():
    compilar()
    estatico, dinamico = casos_sinteticos()
    inspiron = caso_inspiron()
    caso_volcados()
    fuzz(inspiron, dinamico)
    return estatico


# ------------------------------------------------------------ QEMU


def esperar_qemu_livre():
    # Solo al desarrollar (SAGUI_ESPERAR_QEMU=1): en la batería paralela
    # siempre hay otros QEMU y esperar aquí la colgaba hasta el tiempo máximo.
    if os.environ.get("SAGUI_ESPERAR_QEMU") != "1":
        return
    while True:
        n = subprocess.run(["pgrep", "-c", "qemu-system"], capture_output=True, text=True).stdout.strip() or "0"
        if int(n) < 3:
            return
        print(f"  ({n} QEMU en marcha: espero)")
        time.sleep(30)


def em_qemu(nome_caso, imagem, nucleos):
    esperar_qemu_livre()
    with Maquina(nome_caso, "ide", imagem=imagem, nucleos=nucleos) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s.upper(), f"{nome_caso}: arranca sin excepciones")
        verificar(re.search(r"\[PSS\] \d+ tablas \(0 cargadas con Load, 0 rechazadas\), \d+ procesadores, \d+ en la MADT; "
                            r"sin _PSS en ningun procesador", s),
                  f"{nome_caso}: el log dice «sin _PSS en ningun procesador»")
        verificar("[EIST] sin EIST antiguo: " in s, f"{nome_caso}: el log dice «sin EIST antiguo»")
        verificar("[EIST] EIST antiguo (" not in s and "-> no se cambia" not in s, f"{nome_caso}: nada que cambiar")
        verificar("[PSS] AML leido a medias" not in s, f"{nome_caso}: el AML de QEMU se entiende entero")
        procs = re.findall(r"\[PSS\] (\\\S+) \(id (\d+)\): sin _PSS", s)
        verificar(len(procs) >= nucleos, f"{nome_caso}: {len(procs)} procesadores ACPI (>= {nucleos})")
        # Las mismas tablas, volcadas y leídas en el anfitrión.
        caso = Caso(f"qemu_{nome_caso}")
        for assin, fis, tam in re.findall(r"\[PSS\] tabla (\w{4}) \"[^\"]*\" en 0x([0-9a-f]+) \((\d+) bytes\)", s):
            arq = os.path.join(DIR, f"{nome_caso}-{fis}.bin")
            resp = m.monitor(f'pmemsave 0x{fis} {tam} "{arq}"')   # entre comillas: «/» sería una división
            for _ in range(50):
                if os.path.exists(arq) and os.path.getsize(arq) == int(tam):
                    break
                time.sleep(0.1)
            verificar(os.path.exists(arq), f"{nome_caso}: pmemsave de la tabla en 0x{fis} ({resp.strip()!r})")
            caso.raiz(int(fis, 16), open(arq, "rb").read())
        verificar(caso.raizes, f"{nome_caso}: el log dice dónde están las tablas")
        g, cpus, _ = analisar(rodar(caso, "modelo:0x0E"))
        verificar(int(g["cpus"]) == len(procs) and all(c["pss"] == "0" for c in cpus) and g["incompleto"] == "0",
                  f"{nome_caso}: en el anfitrión, las tablas de QEMU dan {g['cpus']} procesadores sin _PSS ({g})")
        print(f"  {nome_caso}: {len(procs)} procesadores ({', '.join(p for p, _ in procs)}), tablas {g['tabelas']}")


def main():
    try:
        anfitriao()
        print("  anfitrión: lector y validación de _PSS correctos")
        if os.environ.get("SAGUI_SO_ANFITRIAO") != "1":
            em_qemu("h147", imagem_console(), 1)
            em_qemu("h147smp", imagem_de_teste("", ["--vesa", "--console", "--smp"]), 2)
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        return 1
    print("H147 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
