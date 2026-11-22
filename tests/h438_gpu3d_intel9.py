#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H438 (D438) - 3D por la GPU en Intel Gen9 (Skylake ... Comet Lake; la UHD 620 8086:3ea0) por el
motor de render (RCS) con execlists: driver gpu3d_intel9 sobre la capa común gpu3d (D285).

QEMU no emula ninguna GPU de Intel, así que NADA de aquí ejecuta el driver contra una tarjeta (eso
se prueba en el portátil con la UHD 620, docs/gpu3d.md «Intel Gen9-12 (3D)»). Lo que se comprueba:

  unidad   en el PC: tests/intel9_3d_unidad.c (ensamblador contra los vectores del ensamblador de
           Mesa, un decodificador y simulador propios que ejecutan los dos sombreadores, recorrido
           de la tanda con las longitudes de genxml, imagen del contexto del RCS de i915 y tabla de
           workarounds), también con ASan/UBSan;
  fuentes  sin QEMU: el driver es un gpu3d_driver_t, arranca apagado, mira el modelo exacto antes
           de leer la tarjeta y solo arranca en Gen9, la sonda solo lee, todas sus esperas tienen
           plazo, con el plazo agotado para el motor (STOP_RING) y no vuelve a probar hasta
           reiniciar, no toca la pantalla, no reinicia la GPU ni carga firmware; el 2D (D434) deja
           el bit 1 del forcewake del GT para el 3D;
  docs     D438, docs/gpu3d.md, docs/gpus.md y docs/terceros.md;
  qemu     con la tarjeta de QEMU: no hay driver 3D de Intel, «gpu3d» lo dice y nada falla.

Uso: h438_gpu3d_intel9.py [unidad] [fuentes] [docs] [qemu]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h438")
DRIVER = "kernel/dispositivos/gpu3d_intel9.c"
PACOTES = "kernel/dispositivos/intel9_3d_pacotes.c"


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def sin_comentarios(s):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", s, flags=re.S))


def funcion(codigo, nome):
    i = codigo.index(nome)
    j = codigo.index("\n}\n", i)
    return codigo[i:j]


def unidad():
    os.makedirs(DIR, exist_ok=True)
    inc = ["-DINTEL9_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc")]
    fontes = [os.path.join(RAIZ, "tests", "intel9_3d_unidad.c"), os.path.join(RAIZ, PACOTES)]
    for nome, flags in (("intel9_3d_unidad", ["-O2", "-Wall", "-Wextra", "-Werror"]),
                        ("intel9_3d_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        r = subprocess.run(["gcc"] + flags + inc + ["-o", exe] + fontes + ["-lm"], capture_output=True, text=True)
        verificar(r.returncode == 0, f"{nome} compila\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
        verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{nome}: ensamblador, sombreadores, tanda, contexto y workarounds")


def fuentes():
    d = ler(DRIVER)
    drv = sin_comentarios(d)
    capa = sin_comentarios(ler("kernel/gui/gpu3d.c"))
    verificar(re.search(r"const gpu3d_driver_t gpu3d_intel9 = \{\s*\"intel9\", i9_iniciar, i9_alvo, i9_limpar, i9_textura, "
                        r"i9_desenhar, i9_esperar, i9_ler, i9_desligar, i9_detalle, i9_memoria,\s*\"intel9\", i9_faixas,", drv),
              "el driver es un gpu3d_driver_t con sus diez funciones y el diagnóstico común (D390)")
    verificar("extern const gpu3d_driver_t gpu3d_intel9;" in ler("kernel/inc/gpu3d.h"), "gpu3d.h declara el driver")
    # Arranca apagado: solo «gpu3d probar/encender» o --gpu3d=intel.
    ini = capa[capa.index("void gpu3d_iniciar(void)"):capa.index("const char *gpu3d_resumo")]
    g9 = ini[ini.index("d2 == &acel_intel_exec && geracao == 9"):]
    g9 = g9[:g9.index("} else if (d2 == &acel_r600")]
    verificar("drv = &gpu3d_intel9;" in g9 and "estado = GPU3D_APAGADO;" in g9 and "pedido == GPU3D_CAB_INTEL" in g9,
              "la Gen9 arranca apagada: solo «gpu3d probar/encender» o --gpu3d=intel")
    verificar("->iniciar(" not in ini, "al arrancar la capa no inicia ningún driver")
    # Orden del encendido: modelo exacto y solo Gen9 antes de leer la tarjeta.
    ini9 = funcion(drv, "static int i9_iniciar(")
    verificar(ini9.index("intel_variante_exec(d->modelo") < ini9.index("pci_le32(d, 0x10)") and
              ini9.index("geracao != 9") < ini9.index("pci_le32(d, 0x10)"),
              "mira la lista exacta de IDs y que sea Gen9 antes de leer los BAR")
    for f in ("static bool superficies_livres(", "static bool sondar_gtt("):
        cuerpo = funcion(drv, f)
        verificar(" wr(" not in cuerpo and "e[2 * k] =" not in cuerpo and "e[2 * k + 1] =" not in cuerpo,
                  f"{f.split('(')[0].split()[-1]}: solo lee")
    verificar(ini9.index("sondar_gtt(") < ini9.index("etapa = 1;") < ini9.index("intel_pte_gen9(") < ini9.index("etapa = 2;") <
              ini9.index("fw_pegar(") < ini9.index("etapa = 3;") < ini9.index("intel_exec_ocupado(stat)") <
              ini9.index("etapa = 4;") < ini9.index("wa_mmio()") < ini9.index("EX_GFX_MODE_LISTA"),
              "etapas en orden: sonda, GTT de la zona, forcewake, RCS sin uso, workarounds y execlists")
    verificar(ini9.index("intel_exec_ocupado(stat)") < ini9.index("wr(RCS + EX_RING_MODE"), "si el RCS ya está en uso, no se toca")
    verificar("if (travado)" in ini9 and ini9.index("if (travado)") < ini9.index("pci_le32(d, 0x10)"),
              "si una tanda no terminó antes, no se vuelve a encender hasta reiniciar")
    # Plazos.
    # D448: la tarea de la frecuencia es un bucle que duerme 200 ms en cada vuelta (no espera a la GPU). Desde D457/D434
    # es común con el 2D (kernel/dispositivos/intel_rps.c) y el 3D solo la llama.
    rps = ler("kernel/dispositivos/intel_rps.c")
    tf = funcion(rps, "static void tarefa_rps(void *arg)\n{")
    verificar("bloquear(&canal)" in tf and "acordar_em" in tf and "RPNSWREQ" in tf and "antes" in tf,
              "la tarea de la frecuencia (intel_rps.c) duerme en cada vuelta y solo vuelve a la frecuencia del firmware")
    verificar("intel_rps_ligar(mmio, INTEL_RPS_3D)" in drv and "intel_rps_trabalho()" in drv and "intel_rps_soltar(INTEL_RPS_3D)" in drv,
              "el 3D pide la frecuencia al gobernador común (ligar, trabajo al mandar, soltar al apagar)")
    verificar("intel_rps_ligar(mmio, INTEL_RPS_2D)" in ler("kernel/dispositivos/acel_intel_exec.c"),
              "el 2D también (D434: antes iba a 100 MHz)")
    resto = drv
    whiles = len(re.findall(r"\bwhile\s*\(", resto)) + len(re.findall(r"for\s*\(\s*;\s*;\s*\)", resto))
    plazos = len(re.findall(r"prazo_vencido\(", resto))
    verificar(whiles > 0 and plazos >= whiles, f"cada bucle de espera tiene su plazo ({whiles} bucles, {plazos} comprobaciones)")
    esp = funcion(drv, "static bool esperar_tanda(")
    verificar("MASC_ON(EX_STOP_RING)" in esp and "travado = true" in esp and "log_motor(" in esp,
              "con el plazo agotado: STOP_RING, los registros del motor al log y no se repite")
    verificar("eir_antes" in esp and "falta_antes" in esp, "un error nuevo del motor (EIR) o un fallo de página también para")
    # Lo que no se hace.
    verificar(not re.search(r"/lib/firmware|vfs_|\b\w*(guc|huc)\w*\b", drv, re.I), "sin firmware, GuC ni HuC")
    verificar("0x941" not in drv.lower() and "gdrst" not in drv.lower(), "nunca reinicia la GPU (GDRST)")
    escritas = re.findall(r"\bwr\(([^,]+),", drv)
    malas = [e for e in escritas if re.match(r"\s*(0x[67][0-9A-Fa-f]{4}\b|INTEL_DSP|INTEL_STOLEN)", e)]
    verificar(not malas, f"no escribe registros de la pantalla ni la zona reservada ({len(escritas)} escrituras)" +
              (f": {malas}" if malas else ""))
    pac = sin_comentarios(ler(PACOTES))
    regs = [int(x, 16) for x in re.findall(r"\{ 0x([0-9A-F]+)u, ", pac)]
    verificar(regs and all(r < 0x10000 for r in regs), "los workarounds son registros del GT y del RCS (ninguno de la pantalla)")
    verificar("if (faltan)" in ini9 and ini9.index("sondar_gtt(") < ini9.index("if (faltan)"),
              "el GTT solo se escribe si faltan entradas de la zona, ya comprobadas dentro de la robada")
    verificar("FW_BIT_GT" in drv and "EX_MASCARA_OFF(0xFFFDu)" in ler("kernel/dispositivos/acel_intel_exec.c"),
              "forcewake del GT: el 2D usa el bit 0 y ya no borra el 1, que es del 3D")
    verificar("_Static_assert(EX_AREA_MAX <= INTEL_EXEC_ZONA2D_MAX" in ler("kernel/dispositivos/acel_intel_exec.c") and
              "INTEL_EXEC_ZONA2D_MAX" in ini9, "la zona del 3D va detrás de la del 2D (D434)")
    verificar("SPDX-License-Identifier: GPL-2.0-or-later" in d.splitlines()[0], "el driver tiene su SPDX")
    for rel in (PACOTES, "kernel/inc/intel9_3d.h", "tests/intel9_3d_unidad.c"):
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in ler(rel), f"{rel} lleva su SPDX")


def docs():
    dec = ler("docs/decisoes.md")
    verificar("## D438." in dec and "## D448." in dec and "## D453." in dec, "las decisiones D438, D448 (velocidad) y D453 (v2) están en docs/decisoes.md")
    guia = ler("docs/gpu3d.md")
    verificar(re.search(r"^## Intel Gen9-12 \(3D\)", guia, re.M) and "Qué probar en el portátil" in guia,
              "docs/gpu3d.md: sección «Intel Gen9-12 (3D)» y qué probar")
    verificar("D438" in ler("docs/gpus.md"), "docs/gpus.md dice el 3D de Gen9 (D438)")
    ter = ler("docs/terceros.md")
    verificar("intel9_3d" in ter and "genxml" in ter, "docs/terceros.md registra las referencias (Mesa genxml, i915)")


def qemu():
    with Maquina("h438", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[GPU3D] intel9" not in s and "[GPU3D] encender" not in s, "sin Intel el driver no se toca al arrancar")
        out = m.comando("gpu3d", 30).replace("\r", "")
        verificar("intel9" not in out and "Intel Gen9" not in out, "«gpu3d»: la tarjeta de QEMU no tiene el driver de Gen9")
        out = m.comando("echo vivo438", 20)
        s = m.serial()
        verificar("vivo438" in out and "EXCECAO" not in s and "PANICO" not in s, "el sistema sigue respondiendo, sin excepciones")


def main():
    partes = sys.argv[1:] or ["unidad", "fuentes", "docs", "qemu"]
    for p in partes:
        print(f"== {p}")
        {"unidad": unidad, "fuentes": fuentes, "docs": docs, "qemu": qemu}[p]()
    print("H438 OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
