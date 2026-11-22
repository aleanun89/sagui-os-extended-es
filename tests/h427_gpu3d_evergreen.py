#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H427 - 3D por la GPU: driver de las Radeon Evergreen y Northern Islands sin Cayman
(HD 5000-6000, HD 7500M/7600M) sobre la capa común gpu3d (D427).

QEMU no emula ninguna Radeon, así que NADA de aquí ejecuta el driver contra una tarjeta
(eso se prueba en el Toshiba con la HD 7600M «Turks», docs/gpu3d.md). Lo que se comprueba:

  fuentes  sin QEMU: el driver es un gpu3d_driver_t de la capa común (D285), arranca
           apagado como las R6xx/R7xx, no toca pantalla, HDP (salvo su vaciado), DMA, MC
           ni SRBM_SOFT_RESET, todas sus esperas tienen plazo, la sonda es la del 2D y no
           escribe, y las direcciones del MC son de 64 bits (D408); el microcódigo está con
           su licencia y sus sumas, y la documentación (D427, terceros, gpu3d.md, gpus.md).
  unidad   en el PC: tests/evergreen_3d_unidad.c (sombreadores contra una segunda
           codificación y palabras de xf86-video-ati, recorrido de los paquetes como el CP,
           valores de inicio y un simulador del ISA; también con ASan/UBSan).
  qemu     con la tarjeta de QEMU: no hay driver 3D, «gpu3d» lo dice y no falla nada, y el
           microcódigo de Evergreen/NI está en /lib/firmware/radeon.
"""
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h427")
SHA = {
    "CEDAR_pfp.bin": "bf614bcdd72fd23a919d2bf39ec065824f0c327e50b9ba3d9d39cb675764fcf3",
    "CEDAR_me.bin": "86cec272c18673356c0e8d774d7ed48976baa631f79734225522a928e9089db4",
    "REDWOOD_pfp.bin": "bf614bcdd72fd23a919d2bf39ec065824f0c327e50b9ba3d9d39cb675764fcf3",
    "REDWOOD_me.bin": "bc98d6afb1fa179dc2113e0c16bb609f4d395020cfb7e47652304ccf5206cade",
    "JUNIPER_pfp.bin": "5f4e98e1bd41bbbba68fb4c1b8761ab0fd7c2f9f4f088c44b1464b7c2e08a9e2",
    "JUNIPER_me.bin": "3890437894d09dbb0b99900e661a8a0d3ad264a650b6ad2d84e5b8e1156cc9a0",
    "CYPRESS_pfp.bin": "5f4e98e1bd41bbbba68fb4c1b8761ab0fd7c2f9f4f088c44b1464b7c2e08a9e2",
    "CYPRESS_me.bin": "3890437894d09dbb0b99900e661a8a0d3ad264a650b6ad2d84e5b8e1156cc9a0",
    "BARTS_pfp.bin": "c9cfed97f54c24190761524e2fbbab80ff1ac07f09d84bb3c955c2337ea7e4f6",
    "BARTS_me.bin": "37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b",
    "TURKS_pfp.bin": "65d98665384252ddd627c365dbfd2f238b0538570d9e1dad6d4382bb93723089",
    "TURKS_me.bin": "37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b",
    "CAICOS_pfp.bin": "210b9c42cf7866fcacf9239016142e799f52901ef3b001f7081fc92ff2c43401",
    "CAICOS_me.bin": "37fa5fb7cdb13df94c9a64d7ba752b63119f063f64326b67ec54656cd184363b",
}


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def sin_comentarios(s):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", s, flags=re.S))


def fuentes():
    drv = sin_comentarios(ler("kernel/dispositivos/gpu3d_evergreen.c"))
    capa = sin_comentarios(ler("kernel/gui/gpu3d.c"))
    pac = sin_comentarios(ler("kernel/dispositivos/evergreen_3d_pacotes.c"))
    verificar(re.search(r"const gpu3d_driver_t gpu3d_evergreen = \{\s*\"evergreen\", eg_iniciar, eg_alvo, eg_limpar, eg_textura, "
                        r"eg_desenhar, eg_esperar, eg_ler, eg_desligar, eg_detalle, eg_memoria,\s*\"evergreen\", eg_faixas_ajustes,", drv),
              "el driver es un gpu3d_driver_t con sus diez funciones y el diagnóstico común (D390)")
    verificar("g3d_arena_iniciar" in drv and "g3d_buffer_criar" in drv and "g3d_buffer_soltar" in drv,
              "usa la memoria de GPU y los búferes comunes (g3d_*)")
    fx = re.search(r"eg_faixas_ajustes\[\] = \{(.*?)\};", drv, re.S)
    verificar(fx is not None, "evergreen: la tabla de rangos de los ajustes")
    for a, b in re.findall(r"\{ 0x([0-9A-F]+)u, 0x([0-9A-F]+)u \}", fx.group(1)):
        a, b = int(a, 16), int(b, 16)
        verificar((0x28000 <= a <= b <= 0x28FFC) or (0x8800 <= a <= b < 0xAC00),
                  f"evergreen: el rango {a:#x}-{b:#x} es del contexto o de la configuración por la cola, fuera de GRBM y CP")
    # No se enciende solo: la capa lo deja apagado salvo --gpu3d=radeon; Cayman (9) no entra.
    ini = capa[capa.index("void gpu3d_iniciar(void)"):capa.index("const char *gpu3d_resumo")]
    eg = ini[ini.index("d2 == &acel_r600 && geracao == 8"):]
    eg = eg[:eg.index("} else if (d2 == &acel_radeon")]
    verificar("drv = &gpu3d_evergreen;" in eg and "estado = GPU3D_APAGADO;" in eg and "pedido == GPU3D_CAB_RADEON" in eg,
              "las Evergreen/NI arrancan apagadas: solo «gpu3d probar/encender» o --gpu3d=radeon")
    verificar("->iniciar(" not in ini, "al arrancar la capa no inicia ningún driver")
    verificar("drv == &gpu3d_r600 || drv == &gpu3d_evergreen" in capa, "al encender, las mismas comprobaciones que las R7xx")
    verificar("extern const gpu3d_driver_t gpu3d_evergreen;" in ler("kernel/inc/gpu3d.h"), "gpu3d.h declara el driver")
    # La sonda: la del 2D, sin escribir nada.
    son = drv[drv.index("static bool eg_sondar"):drv.index("static void hdp(void)")]
    verificar("wr(" not in son and "r600_sondar(" in son and "vm0 & 1" in son and "EVG_CUR_CONTROL" in son and "r600_fim_2d" in son
              and "BAR_SEGURO" in son, "la sonda reutiliza la del 2D, no escribe y comprueba memoria virtual, cursores, 2D y BAR")
    verificar("geracao != EVG_GEN_EVERGREEN" in son, "Cayman y las demás generaciones no entran")
    # Qué registros escribe por MMIO.
    escritos = set(re.findall(r"\bwr\((\w+)", drv)) - {"uint32_t"}
    permitidos = {"R6_CP_ME_CNTL", "R6_SCRATCH_UMSK", "R6_GRBM_SOFT_RESET", "R6_CP_RB_CNTL", "R6_CP_PFP_UCODE_ADDR",
                  "R6_CP_PFP_UCODE_DATA", "R6_CP_ME_RAM_WADDR", "R6_CP_ME_RAM_DATA", "R6_CP_ME_RAM_RADDR", "R6_CP_SEM_WAIT_TIMER",
                  "EG3D_CP_SEM_INCOMPLETE_TIMER_CNTL", "R6_CP_RB_WPTR_DELAY", "R6_CP_RB_RPTR_WR", "R6_CP_RB_WPTR",
                  "R6_CP_RB_RPTR_ADDR", "R6_CP_RB_RPTR_ADDR_HI", "EG3D_SCRATCH_ADDR", "R6_CP_RB_BASE", "R6_CP_DEBUG",
                  "SCRATCH_TESTE", "SCRATCH_VALLA", "pares", "EG3D_GRBM_GFX_INDEX", "EG3D_RLC_GFX_INDEX"}
    verificar(escritos <= permitidos, f"el driver solo escribe registros del CP y la lista de inicio ({sorted(escritos - permitidos)})")
    rst = re.search(r"#define EG3D_SOFT_RESET_CP_Y_3D\s+\(([^\n]*)\)", ler("kernel/inc/evergreen_3d.h"))
    verificar(rst and "wr(R6_GRBM_SOFT_RESET, EG3D_SOFT_RESET_CP_Y_3D)" in drv and "SRBM_SOFT_RESET" not in drv,
              "solo se reinician el CP y los bloques del 3D que dependen de él (como evergreen_cp_resume); nunca SRBM")
    bits = [int(x) for x in re.findall(r"1u << (\d+)", rst.group(1))]
    verificar(sorted(bits) == [0, 5, 8, 9, 10, 14], f"GRBM_SOFT_RESET: CP, PA, SPI, SH, SX y VGT ({bits})")
    verificar(not re.search(r"\b(irq\w*|msi\w*)\b", drv, re.I), "sin interrupciones: sondeo con plazo")
    verificar(not re.search(r"while\s*\(\s*(1|true)\s*\)|for\s*\(\s*;\s*;\s*\)", drv), "sin bucles sin fin")
    esperas = re.findall(r"while \((?:rd\(|livre\(\)|!prazo)[^\n]*\n?[^\n]*", drv)
    verificar(len(esperas) >= 5 and all("prazo_vencido" in e for e in esperas), f"toda espera del driver tiene plazo ({len(esperas)})")
    verificar("SAGUI_SEM_FIRMWARE" in drv and "/lib/firmware/radeon/" in drv and "npfp != EG3D_PFP_PALAVRAS * 4" in drv
              and "nme != EG3D_ME_PALAVRAS * 4" in drv, "microcódigo del sistema de archivos, con el tamaño comprobado; nada con SEM_FIRMWARE=1")
    verificar("return tocou ? G3D_FALHA : G3D_NAO;" in drv, "lo que falla antes de tocar la tarjeta no cuenta como fallo de la GPU")
    verificar("static uint64_t mc_zona" in drv and "static uint64_t mc_tela" in drv and "mc_zona >> 8" in drv,
              "las direcciones del MC son de 40 bits (D408): CP_RB_BASE con los bits 39:8")
    # La lista de inicio no toca pantalla, HDP, DMA, MC ni reinicios (también lo comprueba la unidad).
    ini_m = pac[pac.index("int eg3d_inicio_motor"):pac.index("static const uint32_t zeros")]
    regs = [int(x, 16) for x in re.findall(r"\bW\((0x[0-9A-Fa-f]+)", ini_m)]
    verificar(len(regs) >= 25 and all(r < 0x10000 and not (0x6000 <= r < 0x8000) and not (0x2000 <= r < 0x3000)
                                      and not (0x5400 <= r < 0x5500) and not (0xD000 <= r < 0xE000) and r not in (0x0E60, 0x8020)
                                      for r in regs),
              f"los {len(regs)} registros del inicio del motor no son de pantalla, HDP, MC, DMA ni reinicios")
    # El vaciado del HDP de Evergreen es por su registro (el rodeo HDP_DEBUG1 es solo de las R7xx).
    d2 = sin_comentarios(ler("kernel/dispositivos/acel_r600.c"))
    hdpf = d2[d2.index("void r600_hdp_vaciar("):]
    verificar("b->gen == 7 && !b->agp" in hdpf[:400], "r600_hdp_vaciar: HDP_DEBUG1 solo en las R7xx")
    # Microcódigo y licencia.
    for nome, sha in SHA.items():
        with open(os.path.join(RAIZ, "firmware", "radeon", nome), "rb") as f:
            datos = f.read()
        verificar(hashlib.sha256(datos).hexdigest() == sha, f"firmware/radeon/{nome} tiene su SHA-256")
        verificar(len(datos) == (4480 if "_pfp" in nome else 5504), f"firmware/radeon/{nome} mide lo de Evergreen")
    guion = ler("tools/firmware_radeon.sh")
    verificar(all(sha in guion for sha in SHA.values()), "tools/firmware_radeon.sh lleva las mismas sumas")
    # Documentación.
    dec = ler("docs/decisoes.md")
    verificar("## D427." in dec, "la decisión D427 está en docs/decisoes.md")
    ter = ler("docs/terceros.md")
    verificar("gpu3d_evergreen.c" in ter and "TURKS_pfp.bin" in ter, "docs/terceros.md registra las referencias y el microcódigo")
    guia = ler("docs/gpu3d.md")
    verificar(re.search(r"^## Radeon Evergreen/NI", guia, re.M) and "Qué probar en el Toshiba" in guia, "docs/gpu3d.md: sección y qué probar")
    verificar("D427" in ler("docs/gpus.md"), "docs/gpus.md dice el 3D de Evergreen/NI (D427)")
    for rel in ("kernel/dispositivos/gpu3d_evergreen.c", "kernel/dispositivos/evergreen_3d_pacotes.c", "kernel/inc/evergreen_3d.h",
                "tests/evergreen_3d_unidad.c"):
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in ler(rel), f"{rel} lleva su SPDX")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    inc = ["-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc")]
    fontes = [os.path.join(RAIZ, "tests", "evergreen_3d_unidad.c"),
              os.path.join(RAIZ, "kernel", "dispositivos", "evergreen_3d_pacotes.c"),
              os.path.join(RAIZ, "kernel", "dispositivos", "r600_3d_pacotes.c")]
    for nome, flags in (("evergreen_3d_unidad", ["-O2", "-Wall", "-Wextra", "-Werror"]),
                        ("evergreen_3d_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        r = subprocess.run(["gcc"] + flags + inc + ["-o", exe] + fontes + ["-lm"], capture_output=True, text=True)
        verificar(r.returncode == 0, f"{nome} compila\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
        verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{nome}: sombreadores, paquetes, inicio y simulador")


def qemu():
    with Maquina("h427", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[GPU3D] evergreen" not in s and "[GPU3D] encender" not in s, "sin Radeon el driver no se toca al arrancar")
        out = m.comando("gpu3d", 30).replace("\r", "")
        verificar("sin GPU con driver 3D" in out and "driver software" in out, "«gpu3d»: sin driver 3D para la tarjeta de QEMU, software")
        out = m.comando("gpu3d probar; echo codigo=$?", 60).replace("\r", "")
        verificar("codigo=1" in out, "«gpu3d probar» sin driver: lo dice y devuelve 1")
        out = m.comando("ls /lib/firmware/radeon", 20)
        verificar("TURKS_pfp.bin" in out and "TURKS_me.bin" in out and "CEDAR_me.bin" in out and "LICENSE.radeon.txt" in out,
                  "el microcódigo de Evergreen/NI y su licencia están en /lib/firmware/radeon")
        out = m.comando("echo vivo427", 20)
        s = m.serial()
        verificar("vivo427" in out and "EXCECAO" not in s and "PANICO" not in s, "el sistema sigue respondiendo, sin excepciones")


def main():
    partes = sys.argv[1:] or ["fuentes", "unidad", "qemu"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "unidad": unidad, "qemu": qemu}[p]()
    print("H427 OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
