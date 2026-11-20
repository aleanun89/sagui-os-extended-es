#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H71 (D092, mejora 050) - monitor de hardware: temperatura, frecuencias, sensores.

Critério:
  - no anfitrião: as fórmulas de conversão (kernel/lib/sensores_conv.c,
    compilado com -DSAGUI_HOST) dão os valores certos para cada família com
    registros simulados (Intel Core 2 e Sandy Bridge, AMD K8, 10h-16h, Zen),
    incluindo o TjMax por modelo e o desvio Tctl/Tdie dos Ryzen;
  - no QEMU, que não emula sensores: `sensores`, `temp` e `infopc -t cpu`
    mostram «no disponible» (máquina virtual) sem erros e terminam com 0;
  - a leitura protegida de MSR: `sensores --prova-msr` lê um MSR que não
    existe e o x2APIC ID com o APIC em modo xAPIC; o sistema continua de pé
    (o shell responde depois). Com KVM (SAGUI_KVM=1 ou /dev/kvm acessível)
    as duas leituras geram #GP, absorvido pelo tratador; sem KVM (TCG) o
    QEMU devolve 0 sem exceção, e o teste só confere que nada caiu;
  - `sensores -c -n 2` e `temp -c -n 2` (contínuo);
  - a pestaña Rendimiento do Administrador de tareas mostra os quatro
    gráficos e consulta os sensores só com a aba visível;
  - o informe de compatibilidade inclui as linhas dos sensores.
Capturas em docs/capturas/H71-*.png.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, RAIZ, SAIDA  # noqa: E402

HARNESS = r"""
#include <stdio.h>
#include <string.h>
#include "sensores_conv.h"
#define V(x) printf(#x "=%ld\n", (long)(x))
int main(void) {
    int mc = 0;
    /* Intel: IA32_THERM_STATUS com leitura válida (bit 31), 35 graus abaixo do TjMax. */
    V(sconv_intel_temp(0x80000000ull | (35ull << 16), 100000, &mc)); V(mc);
    V(sconv_intel_temp(35ull << 16, 100000, &mc));                       /* bit 31 apagado: inválida */
    V(sconv_intel_throttling(0x80000000ull | 0x1 | 0x2 | (1u << 10), 1));
    V(sconv_intel_tjmax_msr(0x00641400ull));                               /* 0x64 = 100 */
    V(sconv_intel_tjmax_msr(0x00000000ull));
    V(sconv_intel_tjmax_tabela(0x0E, 8, 0, 0));
    V(sconv_intel_tjmax_tabela(0x0F, 6, 0, 1));
    V(sconv_intel_tjmax_tabela(0x17, 6, 1, 1));
    V(sconv_intel_tjmax_tabela(0x1C, 10, 0, 0));
    V(sconv_intel_tjmax_tabela(0x1C, 2, 0, 0));
    V(sconv_intel_tjmax_tabela(0x0F, 1, 0, 1));                            /* paso < 2: sin el bit 30 */
    V(sconv_intel_tjmax_tabela(0x17, 7, 0, 1));                            /* D259: FreeBSD solo conoce el paso 6 */
    V(sconv_intel_tjmax_tabela(0x26, 2, 0, 0));                            /* Atom sin fila en FreeBSD: 100 */
    V(sconv_intel_mult_x10(0x0F, (9ull << 8) | (1ull << 14)));              /* Core 2: x9,5 */
    V(sconv_intel_mult_x10(0x2A, 22ull << 8));                              /* Sandy Bridge: x22 */
    V(sconv_intel_mult_max_core2_x10((11ull << 40)));
    V(sconv_intel_vid_mv(0x0F, 0x20));                                      /* 1,5 V - 32 x 12,5 mV */
    V(sconv_intel_vid_mv(0x2A, (8192ull) << 32));                           /* 1,000 V */
    /* AMD */
    V(sconv_amd_k8((49u + 40u) << 16));                                     /* 40 graus */
    V(sconv_amd_k10(400u << 21));                                           /* 400 / 8 = 50 graus */
    V(sconv_amd_zen(560u << 21));                                           /* 70 graus */
    V(sconv_amd_zen((560u << 21) | (1u << 19)));                            /* -49: 21 graus */
    V(sconv_amd_zen_desvio("AMD Ryzen 7 1700X Eight-Core Processor"));
    V(sconv_amd_zen_desvio("AMD Ryzen 7 2700X Eight-Core Processor"));
    V(sconv_amd_zen_desvio("AMD Ryzen 5 2600 Six-Core Processor"));
    V(sconv_amd_htc_limite((36u << 16) | 1));                               /* 52 + 18 = 70 graus */
    V(sconv_amd_htc_limite(36u << 16));
    V(sconv_amd_pstate_khz(0x10, (1ull << 63) | 0x0E));                     /* 100 * (14+16) = 3000 MHz */
    V(sconv_amd_pstate_khz(0x11, (1ull << 63) | 0x0D));                     /* 100 * (13+8) = 2100 MHz */
    V(sconv_amd_pstate_khz(0x11, (1ull << 63) | 0x0D | (1u << 6)));         /* /2 = 1050 MHz */
    V(sconv_amd_pstate_khz(0x17, (1ull << 63) | 0x88 | (8u << 8)));         /* 136 * 200 / 8 = 3400 MHz */
    V(sconv_amd_pstate_khz(0x17, 0x88 | (8u << 8)));                        /* PstateEn apagado */
    V(sconv_amd_pstate_khz(0x14, (1ull << 63) | 0x10));                     /* sem fórmula */
    V(sconv_amd_vid_mv(0x11, (1ull << 63) | (20u << 9)));                   /* 1550 - 250 = 1300 */
    V(sconv_amd_vid_mv(0x17, (1ull << 63) | (64u << 14)));                  /* 1550 - 400 = 1150 */
    V(sconv_k8_khz(0x0A));                                                  /* 800 + 1000 = 1800 MHz */
    V(sconv_k8_mult_max_x10(0x0Cull << 16));
    V(sconv_k8_vid_mv(0x06ull << 32));                                      /* 1550 - 150 = 1400 */
    V(sconv_freq_real_khz(2000000, 150, 100));                              /* turbo: 3000 MHz */
    V(sconv_freq_real_khz(2000000, 5000000000ull, 10000000000ull));        /* metade, com números grandes */
    V(sconv_freq_real_khz(2000000, 1, 0));
    return 0;
}
"""

ESPERADO = {
    "sconv_intel_temp(0x80000000ull | (35ull << 16), 100000, &mc)": 1, "mc": 65000,
    "sconv_intel_temp(35ull << 16, 100000, &mc)": 0,
    "sconv_intel_throttling(0x80000000ull | 0x1 | 0x2 | (1u << 10), 1)": 0x80 | 1 | 2 | 8,
    "sconv_intel_tjmax_msr(0x00641400ull)": 100000, "sconv_intel_tjmax_msr(0x00000000ull)": 0,
    "sconv_intel_tjmax_tabela(0x0E, 8, 0, 0)": 100000, "sconv_intel_tjmax_tabela(0x0F, 6, 0, 1)": 85000,
    "sconv_intel_tjmax_tabela(0x17, 6, 1, 1)": 105000, "sconv_intel_tjmax_tabela(0x1C, 10, 0, 0)": 100000,
    "sconv_intel_tjmax_tabela(0x1C, 2, 0, 0)": 90000,
    "sconv_intel_tjmax_tabela(0x0F, 1, 0, 1)": 100000, "sconv_intel_tjmax_tabela(0x17, 7, 0, 1)": 100000,
    "sconv_intel_tjmax_tabela(0x26, 2, 0, 0)": 100000,
    "sconv_intel_mult_x10(0x0F, (9ull << 8) | (1ull << 14))": 95, "sconv_intel_mult_x10(0x2A, 22ull << 8)": 220,
    "sconv_intel_mult_max_core2_x10((11ull << 40))": 110, "sconv_intel_vid_mv(0x0F, 0x20)": 1100,
    "sconv_intel_vid_mv(0x2A, (8192ull) << 32)": 1000,
    "sconv_amd_k8((49u + 40u) << 16)": 40000, "sconv_amd_k10(400u << 21)": 50000,
    "sconv_amd_zen(560u << 21)": 70000, "sconv_amd_zen((560u << 21) | (1u << 19))": 21000,
    'sconv_amd_zen_desvio("AMD Ryzen 7 1700X Eight-Core Processor")': 20000,
    'sconv_amd_zen_desvio("AMD Ryzen 7 2700X Eight-Core Processor")': 10000,
    'sconv_amd_zen_desvio("AMD Ryzen 5 2600 Six-Core Processor")': 0,
    "sconv_amd_htc_limite((36u << 16) | 1)": 70000, "sconv_amd_htc_limite(36u << 16)": 0,
    "sconv_amd_pstate_khz(0x10, (1ull << 63) | 0x0E)": 3000000,
    "sconv_amd_pstate_khz(0x11, (1ull << 63) | 0x0D)": 2100000,
    "sconv_amd_pstate_khz(0x11, (1ull << 63) | 0x0D | (1u << 6))": 1050000,
    "sconv_amd_pstate_khz(0x17, (1ull << 63) | 0x88 | (8u << 8))": 3400000,
    "sconv_amd_pstate_khz(0x17, 0x88 | (8u << 8))": 0, "sconv_amd_pstate_khz(0x14, (1ull << 63) | 0x10)": 0,
    "sconv_amd_vid_mv(0x11, (1ull << 63) | (20u << 9))": 1300,
    "sconv_amd_vid_mv(0x17, (1ull << 63) | (64u << 14))": 1150,
    "sconv_k8_khz(0x0A)": 1800000, "sconv_k8_mult_max_x10(0x0Cull << 16)": 100, "sconv_k8_vid_mv(0x06ull << 32)": 1400,
    "sconv_freq_real_khz(2000000, 150, 100)": 3000000,
    "sconv_freq_real_khz(2000000, 5000000000ull, 10000000000ull)": 1000000,
    "sconv_freq_real_khz(2000000, 1, 0)": 0,
}


def formulas_no_anfitriao():
    d = tempfile.mkdtemp(dir=SAIDA, prefix="h71-")
    c = os.path.join(d, "harness.c")
    exe = os.path.join(d, "harness")
    with open(c, "w") as f:
        f.write(HARNESS)
    r = subprocess.run(["gcc", "-O2", "-Wall", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe, c,
                        os.path.join(RAIZ, "kernel", "lib", "sensores_conv.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, f"sensores_conv.c compila no anfitrião {r.stderr.strip()[:300]}")
    saida = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    obtido = dict(l.rsplit("=", 1) for l in saida.strip().splitlines())
    erros = [f"{k}: {obtido.get(k)} != {v}" for k, v in ESPERADO.items() if int(obtido.get(k, "-999999")) != v]
    verificar(not erros, f"fórmulas por família com valores simulados ({len(ESPERADO)} casos) {erros}")


def kvm_acessivel():
    return os.environ.get("SAGUI_KVM") == "1"


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def main():
    formulas_no_anfitriao()

    with Maquina("h71", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sensores", 60)
        verificar("terminou com codigo 0" in out, "sensores termina com código 0")
        verificar(re.search(r"^Temperatura:\s+no disponible \(máquina virtual", out, re.M),
                  "sensores: temperatura «no disponible» (máquina virtual)")
        verificar(re.search(r"^Frecuencia real:\s+no disponible", out, re.M), "sensores: frecuencia real no disponible")
        verificar(re.search(r"^Frec\. nominal:\s+\d+,\d+ MHz \(TSC\)", out, re.M), "sensores: frecuencia nominal (TSC)")
        for rot in ("Multiplicador", "Tensión \\(VID\\)", "Microcódigo", "Limitación"):
            verificar(re.search(rf"^{rot}:\s+no disponible", out, re.M), f"sensores: {rot.replace(chr(92), '')} no disponible")
        verificar("Super I/O y SMBus: pendiente, desactivado" in out, "sensores: la placa no se lee (desactivado)")
        verificar("[SENSORES] familia 6 modelo 6 (hipervisor)" in out, "kernel registra la detección en el log")
        verificar("EXCECAO" not in m.serial(), "ningún error del kernel")
        m.captura("H71-sensores-texto")

        out = m.comando("temp", 30)
        verificar("CPU: temperatura no disponible" in out and "terminou com codigo 0" in out, "temp: no disponible, código 0")
        out = m.comando("sensores -c -n 2", 30)
        verificar(len(re.findall(r"^\d\d:\d\d:\d\d  temp\. no disponible", out, re.M)) == 2, "sensores -c -n 2: dos líneas")
        out = m.comando("temp -c -n 2", 30)
        verificar(out.count("CPU: temperatura no disponible") == 2, "temp -c -n 2: dos líneas")

        out = m.comando("sensores --prova-msr", 30)
        verificar("MSR 0x0BADF00D (no existe):" in out and "terminou com codigo 0" in out,
                  "lectura protegida de un MSR inexistente: el programa termina bien")
        out = m.comando("echo sigo vivo", 20)
        verificar("sigo vivo" in out and "EXCECAO" not in m.serial(), "el sistema sigue funcionando después de la prueba")
        if kvm_acessivel():
            verificar("MSR 0x0BADF00D (no existe):     no disponible" in out or
                      "0xbadf00d -> nao disponivel" in m.serial(), "KVM: MSR inexistente -> no disponible (#GP absorbido)")
            verificar("[MSR] 0xbadf00d: #GP na leitura; marcado como nao suportado" in m.serial(),
                      "KVM: el #GP se absorbe y el MSR queda marcado")
            out = m.comando("sensores --prova-msr", 30)
            verificar("nesta leitura: 0" in out, "KVM: la segunda vez no se reintenta (MSR marcado)")
        else:
            print("  (sin KVM: el QEMU TCG devuelve 0 en un MSR inexistente sin #GP; la prueba del #GP va con SAGUI_KVM=1)")

        out = m.comando("infopc -t cpu", 120)
        verificar("Sensores (monitor de hardware, solo lectura):" in out and
                  re.search(r"^  Temperatura:\s+no disponible", out, re.M) and "terminou com codigo 0" in out,
                  "infopc -t cpu: sección de sensores con «no disponible»")
        out = m.comando("infopc --informe /tmp/inf.txt", 120)
        out = m.comando("cat /tmp/inf.txt", 60)
        verificar("Sensores (monitor de hardware, solo lectura):" in out and "== SaguiMark (último resultado) ==" in out,
                  "el informe incluye los sensores y la sección de SaguiMark")

    # Pestaña Rendimiento: cuatro gráficas; los sensores solo con la pestaña visible.
    with Maquina("h71g", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("tareas &")
        m.esperar_serial("tareas: pronto", 30)
        j = janela(m.serial(), "Administrador de tareas")
        time.sleep(2.5)
        verificar("tareas: sensores" not in m.serial(), "fuera de Rendimiento no se consultan los sensores")
        m.clique(j["x"] + 112 + 58, j["y"] + 20)
        m.esperar_serial("tareas: pestana rendimiento", 15)
        # D408: la línea de sensores sale al abrir la pestaña y luego solo si cambia algo (en QEMU no cambia
        # nada); el muestreo sigue cada segundo (lo dicen las líneas de rendimiento).
        fim = time.time() + 20
        while time.time() < fim and len(re.findall(r"tareas: rendimiento cpu \d+% mem \d+/\d+ KB \(\d+%\) procesos \d+ muestras (\d+)",
                                                     m.serial())) < 4:
            time.sleep(0.3)
        r = re.findall(r"tareas: rendimiento cpu \d+% mem \d+/\d+ KB \(\d+%\) procesos \d+ muestras (\d+)", m.serial())
        verificar(len(r) >= 4, f"Rendimiento: muestras cada segundo ({len(r)})")
        g = re.findall(r"tareas: sensores temp (-?\d+) mC freq (\d+) kHz fonte (\d+) muestras (\d+)", m.serial())
        verificar(len(g) == 1, f"Rendimiento: una línea de sensores al abrir y ninguna más si no cambia nada ({len(g)})")
        verificar(g and g[-1][0] == "-1" and g[-1][2] == "0", "Rendimiento: temperatura no disponible en QEMU")
        time.sleep(0.5)
        m.captura("H71-tareas-rendimiento")
        verificar("EXCECAO" not in m.serial(), "ningún error del kernel")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
