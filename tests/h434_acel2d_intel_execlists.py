#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H434 (D434) - 2D de Intel Gen9, Gen11 y Gen12 por execlists (acel_intel_exec.c).

QEMU no emula ninguna GPU de Intel: el driver solo se prueba en hardware real (el mini PC
i3-1220P, 0x46A6/0x46A8). Aquí:
  anfitriao  compila tests/recursos/acel_intel_exec_anfitriao.c con el MISMO
             kernel/dispositivos/acel_intel_logica.h: los 147 identificadores exactos,
             la imagen de registros del contexto (la de i915: dirección completa y valor
             de cada registro), el descriptor, la PPGTT y el PAT, la secuencia de ELSP/ELSQ,
             el CSB (formatos de Gen9-11 y de Gen12), la marca de fin, la memoria robada,
             la PTE de la GGTT, el plano de Gen9+ y las órdenes del blitter de Gen9-12,
             con ASan y UBSan;
  fuentes    cada identificador de execlists tiene una fila de acel2d.c con su generación
             (y solo esa); ninguna fila de execlists cae en una familia sin lista; el driver
             acota todas sus esperas (cada «while» tiene su plazo), no carga firmware ni
             GuC/HuC, no escribe en la pantalla ni en el GTT salvo las entradas de la zona
             (solo si faltan y dentro de la memoria robada), comprueba el motor antes
             de tocarlo y se para si la tanda de prueba no termina;
  docs       docs/decisoes.md tiene la D434 y docs/gpus.md la fila Gen9-Gen12 en amarillo
             («sin probar en hardware»).

Uso: h434_acel2d_intel_execlists.py [anfitriao] [fuentes] [docs]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, FalhaTeste, verificar  # noqa: E402

DRIVER = "kernel/dispositivos/acel_intel_exec.c"
LOGICA = "kernel/dispositivos/acel_intel_logica.h"
CAPA = "kernel/gui/acel2d.c"


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def anfitriao():
    exe = os.path.join(SAIDA, "acel_intel_exec_anfitriao")
    os.makedirs(SAIDA, exist_ok=True)
    subprocess.run(["gcc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover", "-Wall", "-Wextra",
                    "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "acel_intel_exec_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    return r.returncode, r.stdout


def parte_anfitriao():
    rc, saida = anfitriao()
    for linha in saida.splitlines():
        if linha.startswith("FALHA"):
            raise FalhaTeste(linha)
    verificar(rc == 0 and "TODO OK" in saida, "la lógica pura de Gen9-12 cuadra en el anfitrión (imagen, descriptor, "
              "ELSP/ELSQ, CSB, PTE, plano, órdenes)")


def ids_logica():
    s = ler(LOGICA)
    out = {}
    for nome, g in (("intel_ids_gen9", 9), ("intel_ids_gen11", 11), ("intel_ids_gen12", 12)):
        m = re.search(r"static const uint16_t " + nome + r"\[\] = \{(.*?)\};", s, re.S)
        verificar(m is not None, f"{LOGICA} tiene la lista {nome}")
        for h in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1)):
            out[int(h, 16)] = g
    return out


def parte_fuentes():
    ids = ids_logica()
    verificar(len(ids) == 147, f"147 identificadores exactos de execlists ({len(ids)})")
    capa = ler(CAPA)
    filas = re.findall(r"\{\s*0x8086,\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*&acel_intel_exec,\s*(\d+),", capa)
    verificar(len(filas) == 147, f"acel2d.c tiene 147 filas de acel_intel_exec ({len(filas)})")
    mal = [i for i in ids if sum(1 for a, b, g in filas if int(a, 16) <= i <= int(b, 16)) != 1]
    verificar(not mal, "cada identificador de execlists cae en una sola fila de acel_intel_exec" + (f": {mal}" if mal else ""))
    mal = [(int(a, 16), g) for a, b, g in filas if ids.get(int(a, 16)) != int(g)]
    verificar(not mal, "cada fila tiene la generación de su lista (9, 11 o 12)" + (f": {mal}" if mal else ""))
    verificar(any(int(a, 16) == 0x46A8 and int(g) == 12 for a, b, g in filas), "el i3-1220P (0x46A8) va a Gen12")
    verificar(any(int(a, 16) == 0x46A6 and int(g) == 12 for a, b, g in filas), "el i3-1220P (0x46A6) va a Gen12")
    # ninguna fila de execlists cae en las familias sin lista (intervalos de acel2d/logica)
    sem = [(int(a, 16), int(b, 16)) for a, b in re.findall(r"\{ 0x([0-9A-F]{4}), 0x([0-9A-F]{4}), \d+, \"", ler(LOGICA))]
    cruce = [(a, b) for a, b in sem for x in ids if a <= x <= b]
    verificar(not cruce, "ningún identificador de execlists está en un intervalo de familias sin lista" +
              (f": {cruce}" if cruce else ""))

    d = ler(DRIVER)
    codigo = re.sub(r"/\*.*?\*/", "", d, flags=re.S)
    whiles = len(re.findall(r"\bwhile\s*\(", codigo)) + len(re.findall(r"for\s*\(\s*;\s*;\s*\)", codigo))
    esgot = len(re.findall(r"acel_prazo_esgotado\(", codigo))
    verificar(esgot >= whiles and whiles > 0, f"cada bucle «while» del driver tiene su plazo ({whiles} bucles, {esgot} comprobaciones)")
    verificar(not re.search(r"/lib/firmware|vfs_|\b\w*(guc|huc)\w*\b", codigo, re.I),
              "sin firmware (ni GuC ni HuC, ni el sistema de ficheros)")
    escritas = re.findall(r"\bwr\(([^,]+),", codigo)
    malas = [e for e in escritas if re.match(r"\s*(0x7|INTEL_DSP|INTEL_STOLEN|0x6)", e)]
    verificar(not malas, f"no escribe en la pantalla ni en la zona reservada ({len(escritas)} escrituras)" +
              (f": {malas}" if malas else ""))
    verificar("if (faltan)" in codigo and "intel_pte_gen9(" in codigo and "wr(EX_GFX_FLSH_CNTL" in codigo,
              "el GTT solo se escribe si faltan entradas de la zona, con el vaciado detrás")
    verificar("intel_dentro_robada(f0, n, sb, st)" in codigo and codigo.index("intel_dentro_robada(f0, n, sb, st)") <
              codigo.index("if (faltan)"), "las entradas del GTT solo se escriben si toda la zona está en la memoria robada")
    verificar("intel_robada9(gen" in codigo and "0xC0" in codigo and "intel_reservado9(gen" in codigo,
              "memoria robada y reservada con las fórmulas de Gen9+ (BSM de 64 bits en 0xC0 desde Gen11)")
    verificar("intel_ppgtt_monta(" in codigo and "intel_lrc_monta(" in codigo and "EX_BCS_BASE, 0, 0" in codigo,
              "el contexto lleva la PPGTT (PML4) y la imagen con las direcciones completas del BCS")
    verificar("intel_cmd_marca(" in codigo and "if (k & 1u)" in codigo,
              "cada tanda lleva la marca de fin y la cola alineada a 8 bytes")
    verificar("EX_MASCARA_ON(EX_STOP_RING)" in codigo, "con el tiempo agotado se para el motor (STOP_RING)")
    verificar("motor_nosso" in codigo, "«acel2d probar» puede volver a iniciar el motor que dejamos nosotros")
    verificar("intel_variante_exec(d->modelo" in codigo and codigo.index("intel_variante_exec(d->modelo") <
              codigo.index("pci_le32(d, 0x10)"), "el driver mira la lista exacta antes de leer la tarjeta")
    verificar("intel_exec_ocupado(stat)" in codigo and codigo.index("intel_exec_ocupado(stat)") <
              codigo.index("wr(EX_BCS_BASE + EX_RING_MODE"), "si el motor ya está en uso, no se toca")
    verificar("if (!ligado" in codigo and codigo.index("if (!ligado") < codigo.index("intel_envio_seq("),
              "no se envía nada sin el motor arrancado")
    verificar("in_detalle," in d and "acel_intel_exec = {" in d, "«acel2d probar --detalle» vuelca el estado (in_detalle)")
    verificar(re.search(r"^acel_intel_exec\b", ler("kernel/inc/acel2d.h"), re.M) is None and
              "extern const acel_driver_t acel_intel_exec;" in ler("kernel/inc/acel2d.h"),
              "acel2d.h declara el driver de execlists")
    verificar("SPDX-License-Identifier: GPL-2.0-or-later" in d.splitlines()[0], "el driver tiene su SPDX")


def parte_docs():
    dec = ler("docs/decisoes.md")
    verificar("## D434." in dec, "docs/decisoes.md tiene la D434")
    gpus = ler("docs/gpus.md")
    verificar(re.search(r"Gen9-Gen12.*v8\.5\.5\.3.*execlists.*sin probar", gpus, re.S) is not None,
              "docs/gpus.md: fila Gen9-Gen12 en amarillo (v8.5.5.3, execlists, sin probar en hardware)")
    terc = ler("docs/terceros.md")
    verificar("i915" in terc and "MIT" in terc, "docs/terceros.md registra los valores tomados de i915 (MIT)")


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "docs"]
    for p in partes:
        print(f"== {p}")
        {"anfitriao": parte_anfitriao, "fuentes": parte_fuentes, "docs": parte_docs}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
