#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H284 - 3D por la GPU: driver de las Radeon R7xx sobre la capa común gpu3d (D284).

QEMU no emula ninguna Radeon, así que NADA de aquí ejecuta el driver contra una
tarjeta. Lo que se comprueba:

  fuentes  sin QEMU: el driver es un gpu3d_driver_t de la capa común (D285; no queda
           otra capa), arranca apagado, no toca pantalla, HDP, DMA ni SRBM_SOFT_RESET,
           todas sus esperas tienen plazo; el microcódigo está con su licencia y sus
           sumas, y la documentación (D284, terceros, gpu3d.md).
  unidad   en el PC: tests/r600_3d_unidad.c (paquetes PM4, sombreadores contra una
           segunda codificación, valores de inicio; también con ASan/UBSan).
  qemu     con la tarjeta de QEMU: no hay driver 3D, «gpu3d» lo dice y no falla nada,
           y el microcódigo está en /lib/firmware/radeon.
La capa común (autoprueba, vigilante, SaguiGL por la GPU) la prueba h285 con su
driver simulado.
"""
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h284")
SHA = {
    "RV710_pfp.bin": "7fd6822683b36a31173b42faa12a1cf2ab99b90db6bbca5fa8ca72ecedc833d5",
    "RV710_me.bin": "91f43071fd815b2d89722338923b6dc7e201752a2cab28d33b3b5152d251aa13",
    "RV730_pfp.bin": "7fd6822683b36a31173b42faa12a1cf2ab99b90db6bbca5fa8ca72ecedc833d5",
    "RV730_me.bin": "8e8434837a357e183bb7d6ec6b055bf3b99d83ce3bffb076acf575f49d6ef15c",
    "RV770_pfp.bin": "bdaefcfb54b5fec41aa8b3de8fe98ecfab57616db40cf6107d0609ebe15dc966",
    "RV770_me.bin": "99c54f6a5a8e30cf57135cdd44e11d7252f5274ac1f5145e5f8c3b2995f629d0",
    "LICENSE.radeon.txt": "fc6223d4bfe9f2f9e2eddc44b9fe5721d0caf49f01cb08d602906add686d8c6f",
}


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def sin_comentarios(s):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", s, flags=re.S))


def fuentes():
    drv = sin_comentarios(ler("kernel/dispositivos/gpu3d_r600.c"))
    capa = sin_comentarios(ler("kernel/gui/gpu3d.c"))
    pac = sin_comentarios(ler("kernel/dispositivos/r600_3d_pacotes.c"))
    # Una sola capa común: gpu3d (D285). La capa propia «acel3d» ya no existe.
    for rel in ("kernel/gui/acel3d.c", "kernel/inc/acel3d.h", "userland/programas/acel3d.c"):
        verificar(not os.path.exists(os.path.join(RAIZ, rel)), f"{rel} ya no existe (la capa común es gpu3d)")
    verificar("acel3d" not in ler("kernel/inc/abi.h") and "acel3d" not in ler("userland/libsgl/sgl.c"),
              "ni la ABI ni SaguiGL conocen otra capa que gpu3d")
    verificar(re.search(r"const gpu3d_driver_t gpu3d_r600 = \{\s*\"r600\", r3_iniciar, r3_alvo, r3_limpar, r3_textura, r3_desenhar, "
                        r"r3_esperar, r3_ler, r3_desligar, r3_detalle, r3_memoria,", drv),
              "el driver es un gpu3d_driver_t con sus diez funciones")
    verificar("g3d_arena_iniciar" in drv and "g3d_buffer_criar" in drv and "g3d_buffer_soltar" in drv,
              "usa la memoria de GPU y los búferes comunes (g3d_*)")
    # D390: diagnóstico común. Los ajustes de /sistema/conf/gpu3d-r600.conf, solo registros que van por la cola
    # (contexto y configuración del 3D), nunca GRBM ni CP (0x8000-0x87FF), el MC ni la pantalla.
    verificar('"r600", r3_faixas_ajustes,' in drv and "r3_vistas" in drv and "ajustes_por(&l, G3D_AJ_INICIO);" in drv
              and "ajustes_por(&l, G3D_AJ_DIBUJO);" in drv, "r600: fichero, rangos, vistas y ajustes en la cola (D390)")
    fx = re.search(r"r3_faixas_ajustes\[\] = \{(.*?)\};", drv, re.S)
    verificar(fx is not None, "r600: la tabla de rangos de los ajustes")
    for a, b in re.findall(r"\{ 0x([0-9A-F]+)u, 0x([0-9A-F]+)u \}", fx.group(1)):
        a, b = int(a, 16), int(b, 16)
        verificar((0x28000 <= a <= b <= 0x28FFC) or (0x8800 <= a <= b < 0xAC00),
                  f"r600: el rango {a:#x}-{b:#x} es del contexto o de la configuración por la cola, fuera de GRBM y CP")
    # No se enciende solo: la capa lo deja apagado salvo --gpu3d=radeon.
    ini = capa[capa.index("void gpu3d_iniciar(void)"):capa.index("const char *gpu3d_resumo")]
    r7 = ini[ini.index("d2 == &acel_r600 && (geracao == 7 || geracao == 6)"):]   # D357: también R6xx
    r7 = r7[:r7.index("} else if (d2)")]
    verificar("drv = &gpu3d_r600;" in r7 and "estado = GPU3D_APAGADO;" in r7 and "pedido == GPU3D_CAB_RADEON" in r7,
              "las R7xx arrancan apagadas: solo «gpu3d probar/encender» o --gpu3d=radeon")
    verificar("->iniciar(" not in ini, "al arrancar la capa no inicia ningún driver")
    abrir = capa[capa.index("if (p.op == GPU3D_OP_ABRIR)"):]
    verificar("if (estado != GPU3D_ATIVO && estado != GPU3D_SIN_PROBAR) return GPU3D_SEM_SUPORTE;" in abrir,
              "un programa no enciende un 3D apagado: sigue por software")
    verificar('"radeon": 4' in ler("tools/mkimagem.py") and re.search(r"#define GPU3D_CAB_RADEON\s+4\b", ler("kernel/inc/gpu3d.h")),
              "--gpu3d=radeon es el modo 4 de CAB_GPU3D (reservado por la D285)")
    # El driver: qué registros escribe por MMIO.
    son = drv[drv.index("static bool r3_sondar"):drv.index("static void hdp(void)")]
    verificar("wr(" not in son, "la sonda no escribe ningún registro")
    escritos = set(re.findall(r"\bwr\((\w+)", drv)) - {"uint32_t"}   # la definición de wr()
    permitidos = {"R6_CP_ME_CNTL", "R6_SCRATCH_UMSK", "R6_GRBM_SOFT_RESET", "R6_CP_RB_CNTL", "R6_CP_PFP_UCODE_ADDR",
                  "R6_CP_PFP_UCODE_DATA", "R6_CP_ME_RAM_WADDR", "R6_CP_ME_RAM_DATA", "R6_CP_ME_RAM_RADDR", "R6_CP_SEM_WAIT_TIMER",
                  "R6_CP_RB_WPTR_DELAY", "R6_CP_RB_RPTR_WR", "R6_CP_RB_WPTR", "R6_CP_RB_RPTR_ADDR", "R6_CP_RB_RPTR_ADDR_HI",
                  "R6_CP_RB_BASE", "R6_CP_DEBUG", "SCRATCH_TESTE", "SCRATCH_VALLA", "pares",
                  # D406: la GART (solo con «gpu3d encender gtt»): sus registros de gart_r700_logica.h, la lista de
                  # encender/apagar (p) y los valores guardados antes (gart.regs); nada del MC fuera de la apertura.
                  "R7G_MC_VM_SYS_APER_HIGH", "R7G_MC_VM_SYS_APER_LOW", "R7G_VM_CONTEXT0_INVAL_HI", "R7G_VM_CONTEXT0_INVAL_LO",
                  "R7G_VM_CONTEXT0_REQUEST_RESPONSE", "gart", "p"}
    verificar(escritos <= permitidos, f"el driver solo escribe registros del CP y la lista de inicio ({sorted(escritos - permitidos)})")
    verificar("wr(R6_GRBM_SOFT_RESET, R6_SOFT_RESET_CP)" in drv and "0x7fff" not in drv.lower() and "SRBM_SOFT_RESET" not in drv,
              "solo se reinicia el CP: nunca el motor entero ni SRBM")
    verificar(not re.search(r"\b(irq\w*|msi\w*)\b", drv, re.I), "sin interrupciones: sondeo con plazo")
    verificar(not re.search(r"while\s*\(\s*(1|true)\s*\)|for\s*\(\s*;\s*;\s*\)", drv), "sin bucles sin fin")
    esperas = re.findall(r"while \((?:rd\(|livre\(\)|!prazo)[^\n]*\n?[^\n]*", drv)
    verificar(len(esperas) >= 5 and all("prazo_vencido" in e for e in esperas), f"toda espera del driver tiene plazo ({len(esperas)})")
    verificar("r600_sondar(" in drv and "vm0 & 1" in drv and "D1CUR_CONTROL" in drv and "r600_fim_2d" in drv,
              "la sonda reutiliza la del 2D y comprueba la memoria virtual, el cursor y que va detrás del 2D")
    verificar("SAGUI_SEM_FIRMWARE" in drv and "/lib/firmware/radeon/" in drv and "npfp != pal_pfp * 4" in drv
              and "R7_PFP_UCODE_PALAVRAS : R6_PFP_UCODE_PALAVRAS" in drv,
              "microcódigo del sistema de archivos, con el tamaño comprobado; nada con SEM_FIRMWARE=1")
    verificar("return tocou ? G3D_FALHA : G3D_NAO;" in drv, "lo que falla antes de tocar la tarjeta no cuenta como fallo de la GPU")
    # La lista de inicio no toca pantalla, HDP ni DMA (también lo comprueba la unidad).
    regs = [int(x, 16) for x in re.findall(r"\bW\((0x[0-9A-Fa-f]+)", pac)]
    verificar(len(regs) >= 35 and all(r < 0x10000 and not (0x6000 <= r < 0x8000) and not (0x2C00 <= r < 0x3000)
                                      and not (0xD000 <= r < 0xD100) and r != 0x0E60 for r in regs),
              f"los {len(regs)} registros del inicio del motor no son de pantalla, HDP, DMA ni reinicios")
    perm = ler("kernel/nucleo/permisos.c")
    verificar("INFO_GPU3D_CTL" in perm and "INFO_GPU3D_PROVA" in perm, "encender y probar el 3D son del administrador")
    # El 2D sigue con sus reglas (la sonda compartida no cambió qué comprueba).
    d2 = sin_comentarios(ler("kernel/dispositivos/acel_r600.c"))
    verificar("bool r600_sondar(" in d2 and "if (!r600_sondar(d, geracao, t, &base, m, nm)) return false;" in d2,
              "el 2D por DMA usa la misma sonda de solo lectura")
    # Microcódigo y licencia.
    for nome, sha in SHA.items():
        with open(os.path.join(RAIZ, "firmware", "radeon", nome), "rb") as f:
            verificar(hashlib.sha256(f.read()).hexdigest() == sha, f"firmware/radeon/{nome} tiene su SHA-256")
    verificar("in binary form only" in ler("firmware/radeon/LICENSE.radeon.txt"), "la licencia de redistribución va al lado")
    guion = ler("tools/firmware_radeon.sh")
    verificar(all(sha in guion for sha in SHA.values()), "tools/firmware_radeon.sh lleva las mismas sumas")
    mk = ler("Makefile")
    verificar(re.search(r"ifeq \(\$\(SEM_FIRMWARE\),1\)\nFIRMWARE_RAMDISK :=\nFIRMWARE_RADEON :=\n", mk) and "lib/firmware/radeon" in mk,
              "SEM_FIRMWARE=1 deja fuera el microcódigo; si no, va en /lib/firmware/radeon")
    # Documentación.
    dec = ler("docs/decisoes.md")
    verificar("## D284." in dec and "## D285." in dec, "las decisiones D284 y D285 están en docs/decisoes.md")
    ter = ler("docs/terceros.md")
    verificar("gpu3d_r600.c" in ter and "firmware/radeon/" in ter, "docs/terceros.md registra las referencias y el microcódigo")
    guia = ler("docs/gpu3d.md")
    verificar(re.search(r"^## Radeon (R6xx y )?R7xx", guia, re.M) and "gpu3d probar --detalle" in guia and "dmesg > /home/gpu3d.txt" in guia
              and "Probado en el HP dv7" in guia, "docs/gpu3d.md dice qué ejecutar en el dv7 y que ya está probado allí (D402)")
    for rel in ("kernel/dispositivos/gpu3d_r600.c", "kernel/dispositivos/r600_3d_pacotes.c", "kernel/inc/r600_3d.h",
                "kernel/inc/r600_base.h"):
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in ler(rel), f"{rel} lleva su SPDX")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    inc = ["-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc")]
    fontes = [os.path.join(RAIZ, "tests", "r600_3d_unidad.c"), os.path.join(RAIZ, "kernel", "dispositivos", "r600_3d_pacotes.c")]
    for nome, flags in (("r600_3d_unidad", ["-O2", "-Wall", "-Wextra", "-Werror"]),
                        ("r600_3d_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        r = subprocess.run(["gcc"] + flags + inc + ["-o", exe] + fontes, capture_output=True, text=True)
        verificar(r.returncode == 0, f"{nome} compila\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
        verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{nome}: sombreadores, paquetes e inicio del motor")


def qemu():
    with Maquina("h284", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[GPU3D] r600" not in s and "[GPU3D] encender" not in s, "sin Radeon el driver no se toca al arrancar")
        out = m.comando("gpu3d", 30).replace("\r", "")
        verificar("sin GPU con driver 3D" in out and "driver software" in out, "«gpu3d»: sin driver 3D para la tarjeta de QEMU, software")
        out = m.comando("gpu3d probar; echo codigo=$?", 60).replace("\r", "")
        verificar("sin 3D por GPU" in out and "codigo=1" in out, "«gpu3d probar» sin driver: lo dice y devuelve 1")
        out = m.comando("gpu3d encender; echo codigo=$?", 60).replace("\r", "")
        verificar("codigo=1" in out, "«gpu3d encender» sin driver: no enciende nada")
        out = m.comando("ls /lib/firmware/radeon", 20)
        verificar("RV710_pfp.bin" in out and "RV710_me.bin" in out and "LICENSE.radeon.txt" in out,
                  "el microcódigo y su licencia están en /lib/firmware/radeon")
        out = m.comando("echo vivo284", 20)
        s = m.serial()
        verificar("vivo284" in out and "EXCECAO" not in s and "PANICO" not in s, "el sistema sigue respondiendo, sin excepciones")
        verificar("[GPU3D] apagado" not in s, "nada apagó un 3D que nunca se encendió")


def main():
    partes = sys.argv[1:] or ["fuentes", "unidad", "qemu"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "unidad": unidad, "qemu": qemu}[p]()
    print("H284 OK")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
