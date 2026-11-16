#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H359 - Radeon de la v8.3 completas (D357): 3D en R6xx además de R7xx, el estado del 3D
de las R6xx/R7xx que se perdió al fusionar, y el diagnóstico por etapas del 2D y el 3D.

QEMU no emula ninguna Radeon: nada de aquí ejecuta los drivers contra una tarjeta. Partes:

  fuentes  sin QEMU: la capa elige gpu3d_r600 para R6xx y R7xx y los deja APAGADOS (salvo
           --gpu3d=radeon); el driver distingue las familias R6xx (microcódigo, orden de
           carga, inicio, ME_INITIALIZE, SURFACE_BASE_UPDATE, mezcla de la R600, caché de
           vértices); los tres drivers Radeon (r600 3D, r500 3D, r600dma 2D) dicen la etapa;
           el microcódigo de las R6xx con su SHA-256; documentación (D357, gpus.md, gpu3d.md).
  unidad   en el PC: tests/r600_3d_unidad.c (ahora también el inicio y el estado de las R6xx),
           normal y con ASan/UBSan.
  firmware el ramdisk construido lleva el microcódigo R6xx en /lib/firmware/radeon.
  qemu     sin Radeon nada cambia: «gpu3d» y «gpu3d probar» lo dicen, el microcódigo está, y
           con --gpu3d=simulado la capa común sigue pasando la autoprueba («gpu3d probar
           --detalle» BIEN) y el detalle no falla.

Uso: h359_radeon_v83.py [fuentes] [unidad] [firmware] [qemu]
"""
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h359")
# linux-firmware 20241110 (tools/firmware_radeon.sh). RV620 = RV610 y RV635 = RV630 byte a byte.
SHA_R6 = {
    "R600_pfp.bin": "87e8cf139b918d34389e425cde183e6f45d6aa5692b7f6ea56c9657539af81eb",
    "R600_me.bin": "94ee102cbffdd94d34c823f72280f0455fa814bb2251087a92de01c4a891b014",
    "RV610_pfp.bin": "c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246",
    "RV610_me.bin": "566508c94ab890868242298a338e9362e893115a349f8548d26b9280ef9f4b4f",
    "RV630_pfp.bin": "c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246",
    "RV630_me.bin": "2a7e310ab6fd2d8c466ddbb394ae0da26303b1da9f924c04d7cbc4f47836e9e0",
    "RV670_pfp.bin": "c0f58b75401acef29861a399430ce51681a39771ca30d020de24358dc1909246",
    "RV670_me.bin": "7630b5499e2f73b81098dac53b047480cb58bcba07e6604d2cee481ed1c7bc7b",
}


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def sin_comentarios(s):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", "", s, flags=re.S))


def sha(rel):
    with open(os.path.join(RAIZ, rel), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def fuentes():
    capa = sin_comentarios(ler("kernel/gui/gpu3d.c"))
    ini = capa[capa.index("void gpu3d_iniciar(void)"):capa.index("const char *gpu3d_resumo")]
    r6 = ini[ini.index("d2 == &acel_r600 && (geracao == 7 || geracao == 6)"):]
    r6 = r6[:r6.index("} else if (d2 == &acel_radeon")]
    verificar("drv = &gpu3d_r600;" in r6 and "estado = GPU3D_APAGADO;" in r6 and "estado = GPU3D_SIN_PROBAR;" in r6
              and "pedido == GPU3D_CAB_RADEON" in r6,
              "R6xx y R7xx: el driver se elige y queda APAGADO (o sin probar con --gpu3d=radeon); antes se perdía el estado")
    verificar('"R7xx" : "R6xx"' in r6, "la tarjeta se llama «Radeon R6xx» o «Radeon R7xx» (Configuración la marca experimental)")
    verificar('contem(ac3.placa, "R6xx")' in ler("userland/programas/configuracion.c"), "Configuración avisa de que el 3D R6xx es experimental")

    drv = sin_comentarios(ler("kernel/dispositivos/gpu3d_r600.c"))
    for nome, fam in (("RV620", "R63D_RV610"), ("RV630", "R63D_RV630"), ("RV635", "R63D_RV635"), ("RV670", "R63D_RV670"),
                      ("R600", "R63D_R600")):
        verificar(re.search(rf'"{nome}", \d\)\) familia = {fam};', drv), f"la familia {nome} se reconoce")
    verificar('!txt_compara_n(base.nome, "RV610", 5) || !txt_compara_n(base.nome, "RV620", 5)' in drv,
              "la RV610 y la RV620 comparten valores")
    verificar("(base.gen == 7) != r7" in drv, "la familia tiene que cuadrar con la generación de la tabla 2D")
    verificar('"R600", "RV610", "RV630", "RV670", "RV630"' in drv, "microcódigo por familia (RV620 -> RV610, RV635 -> RV630)")
    verificar("r7 ? R7_PFP_UCODE_PALAVRAS : R6_PFP_UCODE_PALAVRAS" in drv and "r7 ? R7_ME_UCODE_PALAVRAS : R6_ME_UCODE_PALAVRAS" in drv,
              "tamaños del microcódigo por generación")
    carga = drv[drv.index("if (r7) {\n        wr(R6_CP_PFP_UCODE_ADDR, 0);"):]
    carga = carga[:carga.index("wr(R6_CP_ME_RAM_RADDR, 0);")]
    i_else = carga.index("} else {")
    verificar(carga.index("R6_CP_PFP_UCODE_DATA") < carga.index("R6_CP_ME_RAM_DATA") < i_else and
              carga.index("R6_CP_ME_RAM_DATA", i_else) < carga.index("R6_CP_PFP_UCODE_DATA", i_else),
              "orden de carga: R7xx PFP y ME (rv770), R6xx ME y PFP (r600)")
    verificar("rd(r7 ? R7_MC_ARB_RAMCFG : R6_RAMCFG)" in drv and "rd(r7 ? R7_SX_DEBUG_1 : R6_SX_DEBUG_1)" in drv
              and "if (r7) { le.smx_dc_ctl0" in drv, "en R6xx se leen RAMCFG y SX_DEBUG_1 de su sitio y no los de las R7xx")
    verificar("r63d_inicio_motor(familia" in drv and "r63d_me_initialize(&l, r7," in drv and "r63d_estado_base(&l, familia, r7," in drv,
              "inicio, ME_INITIALIZE y estado base según la generación")
    verificar("R63D_PRECISA_SBU(familia)" in drv and drv.count("familia == R63D_R600") >= 2 and "R63D_SEM_CACHE_VTX(familia)" in drv,
              "SURFACE_BASE_UPDATE en las RV6xx, mezcla sin «por superficie» en la R600, caché de vértices por familia")
    verificar("r63d_vs(s, r7)" in drv and "r63d_ps_cor(s, r7)" in drv and "r63d_ps_tex(s, r7)" in drv,
              "los sombreadores con la codificación de su generación")
    for n in range(1, 7):
        verificar(f"etapa = {n};" in drv, f"gpu3d_r600: se apunta la etapa {n}")
    verificar("prefixar_etapa(m, nm)" in drv and '"etapa %d (%s): %s"' in drv and "ultima etapa del encendido" in drv,
              "gpu3d_r600: el motivo y el detalle dicen la etapa")
    pac = sin_comentarios(ler("kernel/dispositivos/r600_3d_pacotes.c"))
    r6i = pac[pac.index("int r6_inicio_motor("):pac.index("int r63d_inicio_motor(")]
    regs = [int(x, 16) for x in re.findall(r"\bW\((0x[0-9A-Fa-f]+)", r6i)]
    verificar(len(regs) >= 28 and all(r < 0x10000 and not (0x2000 <= r < 0x3000) and not (0x6000 <= r < 0x8000)
                                      and not (0xD000 <= r < 0xD100) for r in regs),
              f"r6_inicio_motor: {len(regs)} registros, ninguno de MC/HDP/ARB, pantalla ni DMA")
    verificar("0x2C14" not in r6i and "0x2418" not in r6i and "0x246C" not in r6i,
              "r6_inicio_motor no borra las superficies del HDP ni toca el árbitro de memoria")
    verificar("(1u << 14) | (1u << 4)" in pac and "(1u << 14) | (1u << 0)" not in pac,
              "PA_SC_MODE_CNTL de las R6xx: WALK_ORDER_ENABLE (bit 4), no MSAA_ENABLE")
    verificar("0x28D34" in pac and "R63D_IT_SURFACE_BASE_UPDATE" in pac, "DB_PREFETCH_LIMIT y SURFACE_BASE_UPDATE en la superficie")

    r5 = sin_comentarios(ler("kernel/dispositivos/gpu3d_r500.c"))
    verificar("r5_iniciar_etapas(" in r5 and '"etapa %d (%s): %s"' in r5 and "ultima etapa del encendido" in r5
              and all(f"etapa = {n};" in r5 for n in range(1, 7)), "gpu3d_r500: el motivo y el detalle dicen la etapa")
    d2 = sin_comentarios(ler("kernel/dispositivos/acel_r600.c"))
    verificar("r6_iniciar_etapas(" in d2 and '"etapa %d: %s"' in d2 and "ultima etapa del inicio del driver" in d2
              and all(f"etapa = {n};" in d2 for n in range(1, 7)), "acel_r600 (2D): el motivo y el detalle dicen la etapa")
    verificar(ler("kernel/gui/acel2d.c").count('char m[128] = "";') == 2, "acel2d: cabe la etapa delante del motivo")

    for nome, s in SHA_R6.items():
        verificar(sha(f"firmware/radeon/{nome}") == s, f"firmware/radeon/{nome} con su SHA-256 de linux-firmware")
        verificar(s in ler("tools/firmware_radeon.sh") and nome in ler("tools/firmware_radeon.sh"),
                  f"tools/firmware_radeon.sh baja y comprueba {nome}")

    dec = ler("docs/decisoes.md")
    verificar("## D357." in dec, "la decisión D357 está en docs/decisoes.md")
    verificar(re.search(r"Radeon R6xx/R7xx.*D357", ler("docs/gpus.md")), "docs/gpus.md: la fila de R6xx/R7xx cita la D357")
    guia = ler("docs/gpu3d.md")
    verificar("R6xx" in guia and "D357" in guia and "etapa" in guia and "L635" in guia and "Amilo" in guia and "dv7" in guia,
              "docs/gpu3d.md: R6xx, etapas y qué probar en dv7, Amilo y L635")
    ter = ler("docs/terceros.md")
    verificar("R600_me.bin" in ter or "R6xx" in ter, "docs/terceros.md registra el microcódigo de las R6xx")


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
        verificar(r.returncode == 0 and "inicio del motor (R6xx y R7xx): todo bien" in r.stdout,
                  f"{nome}: sombreadores, paquetes e inicio del motor de R6xx y R7xx")


def firmware():
    rf = os.path.join(RAIZ, "build", "rootfs", "lib", "firmware", "radeon")
    for nome, s in SHA_R6.items():
        f = os.path.join(rf, nome)
        verificar(os.path.exists(f), f"el ramdisk lleva /lib/firmware/radeon/{nome}")
        with open(f, "rb") as fh:
            verificar(hashlib.sha256(fh.read()).hexdigest() == s, f"y es el de firmware/radeon/ ({nome})")


def arrancar(m, tempo=240):
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()
        if "area de trabalho no ar" in s or "sagui:/$" in s:
            break
        if m.proc.poll() is not None:
            raise FalhaTeste("el QEMU terminó durante el arranque")
        time.sleep(0.5)
    else:
        raise FalhaTeste("el escritorio no llegó")
    time.sleep(3)


def qemu():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=radeon"])
    with Maquina("h359r", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar("[GPU3D] r600" not in s and "[GPU3D] encender" not in s and "[ACEL2D] r600" not in s,
                  "--gpu3d=radeon sin Radeon: no se toca ningún driver Radeon")
        out = m.comando("gpu3d", 30)
        verificar("3D por GPU: sin GPU con driver 3D (software)" in out, "«gpu3d»: sin driver 3D para la tarjeta de QEMU")
        out = m.comando("gpu3d probar --detalle; echo codigo=$?", 60)
        verificar("sin 3D por GPU" in out and "codigo=1" in out, "«gpu3d probar --detalle» sin driver: lo dice y devuelve 1")
        out = m.comando("acel2d probar --detalle; echo codigo=$?", 60)
        verificar("acel2d probar:" in out and "codigo=" in out, "«acel2d probar --detalle» responde")
        out = m.comando("ls /lib/firmware/radeon", 30)
        verificar(all(n in out for n in ("R600_me.bin", "RV610_pfp.bin", "RV630_me.bin", "RV670_me.bin", "RV710_me.bin", "R520_cp.bin")),
                  "el microcódigo de R5xx, R6xx y R7xx está en /lib/firmware/radeon")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado"])
    with Maquina("h359s", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d probar --detalle", 300)
        print("   ", "\n    ".join(l for l in out.splitlines() if "gpu3d probar" in l)[:1200])
        verificar("gpu3d probar: BIEN" in out, "la capa común con el driver simulado: autoprueba y medida bien")
        out = m.comando("gpu3d apagar", 30)
        verificar("apagado (software;" in out and "apagado a mano" in out, "«gpu3d apagar»: el estado y el motivo")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")


def main():
    todas = {"fuentes": fuentes, "unidad": unidad, "firmware": firmware, "qemu": qemu}
    quais = [a for a in sys.argv[1:] if a in todas] or list(todas)
    try:
        for q in quais:
            print(f"== {q} ==")
            todas[q]()
    except FalhaTeste as e:
        print("FALHOU:", e)
        return 1
    print("H359: Radeon de la v8.3 (R6xx en el 3D, estado del 3D, etapas): BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
