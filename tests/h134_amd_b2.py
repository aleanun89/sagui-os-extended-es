#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H134 (D158, mejora 054 parte B2) - el armazón del driver 3D de las GPU de AMD.

Lo que se comprueba aquí (el QEMU no emula ninguna GPU de AMD):

  - el programa `amd` arranca y funciona dentro del SaguiOS;
  - la tabla de familias del driver está: Evergreen, R5xx y R300/R420 (las de
    B2) y la de R6xx/R7xx (B1, la primera);
  - sin GPU de AMD, la herramienta lo dice claro y el motor que está en uso es
    el de software (SaguiGL), que es el camino por defecto;
  - el vigilante de la GPU tiene un plazo máximo y, cuando la GPU no responde,
    el motor vuelve al software (aquí con una GPU simulada: `amd --simular`);
  - el motor por software sigue igual: el programa sglv6 da en el SaguiOS los
    mismos valores que en el anfitrión (como en la h101);
  - la opción de arranque SGL_MOTOR=software desactiva la aceleración;
  - el kernel mapea de verdad un BAR: con un dispositivo PCI de mentira en el
    QEMU (una e1000), `amd --bar` lee sus registros a través del mapa.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, imagem_de_teste, RAIZ  # noqa: E402

# Las mismas banderas con las que se compila el motor para el SaguiOS
# (Makefile: SGL_FLAGS), para que el resultado sea el mismo.
FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-Wall"]


def sgl_esperado():
    """Compila la prueba del motor en el anfitrión y devuelve sus dos escenas."""
    exe = os.path.join(RAIZ, "build", "sglv6_anfitriao")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    fontes = [os.path.join(RAIZ, "tests/sgl/sglv6.c"), os.path.join(RAIZ, "userland/libsgl/sgl.c")]
    subprocess.run(["gcc"] + FLAGS + ["-w", "-DSGL_ANFITRIAO", "-I" + os.path.join(RAIZ, "userland/libsgl"),
                                      "-o", exe] + fontes, check=True)
    out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    return _linhas(out)


def _linhas(out):
    """Convierte la salida del programa en {escena: {clave: valor}}."""
    cenas = {}
    for linha in out.splitlines():
        partes = linha.split()
        if partes and partes[0] in ("textura", "completa"):
            pares = partes[1:]
            cenas[partes[0]] = {pares[i]: pares[i + 1] for i in range(0, len(pares) - 1, 2)}
    return cenas


def fam_qual_pci(saida, dispositivo):
    """Saca las coordenadas (bus, dispositivo, función) de una línea del
    informe del PCI («00:03.0  8086:100e ...»)."""
    for linha in saida.splitlines():
        m = re.match(r"\s*([0-9a-f]{2}):([0-9a-f]{2})\.(\d+)\s+([0-9a-f]{4}):([0-9a-f]{4})", linha)
        if m and m.group(4) + ":" + m.group(5) == dispositivo:
            return int(m.group(1), 16), int(m.group(2), 16), int(m.group(3))
    return None


def main():
    ref = sgl_esperado()
    verificar("textura" in ref and "completa" in ref, "el anfitrión da las dos escenas del motor")

    # ------------------------------------------------- 1: el motor y la tabla
    console = imagem_console()
    with Maquina("h134", "ide", imagem=console) as m:
        m.esperar_serial("sagui:/$", 120)

        out = m.comando("amd", 60)
        print("  (amd:", out.strip().replace("\n", " | "), ")")
        verificar("Motor en uso: software" in out, "el motor en uso es el de software")
        verificar("no hay ninguna GPU AMD" in out, "sin GPU de AMD, la herramienta lo dice")
        verificar("Vigilante de la GPU:" in out, "la herramienta dice el plazo del vigilante")

        out = m.comando("amd --tabla", 60)
        print("  (tabla:", out.strip().replace("\n", " | ")[:400], ")")
        for fam in ("AMD Evergreen", "AMD R5xx", "AMD R300/R420", "AMD R6xx/R7xx"):
            verificar(fam in out, f"la tabla tiene la familia {fam}")
        verificar("MM_INDEX 0xe000" in out, "Evergreen: el MM_INDEX en 0xe000")
        verificar("MM_INDEX 0xd00" in out, "R6xx: el MM_INDEX en 0xd00")
        verificar("MM_INDEX 0x4000" in out, "R5xx y R300: el MM_INDEX en 0x4000")
        verificar("microcodo si" in out and "microcodo no" in out,
                  "la tabla dice qué familias necesitan microcodo")

        # ------------------------------------- 2: el vigilante y la caida
        out = m.comando("amd --simular", 60)
        print("  (simular:", out.strip().replace("\n", " | "), ")")
        verificar("el vigilante: se paso de plazo" in out, "el vigilante vence el plazo")
        verificar("caida al motor por software" in out, "la GPU que falla hace caer al motor por software")
        verificar("motor tras el fallo: software" in out, "despues del fallo el motor es el de software")

        # ------------------------------- 3: el motor por software sigue igual
        out = m.comando("sglv6", 120)
        sagui = _linhas(out)
        verificar("textura" in sagui and "completa" in sagui, "sglv6 dentro del SaguiOS da las dos escenas")
        verificar(sagui.get("textura") == ref["textura"],
                  f"la escena de la textura: {sagui.get('textura')} = anfitrión {ref['textura']}")
        verificar(sagui.get("completa") == ref["completa"],
                  f"la escena completa: {sagui.get('completa')} = anfitrión {ref['completa']}")

    # ------------------------------------- 4: la opcion de arranque (SGL_MOTOR)
    software = imagem_de_teste("", ["--vesa", "--console", "--sgl-motor", "software"])
    with Maquina("h134sw", "ide", imagem=software) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("amd", 60)
        print("  (amd con SGL_MOTOR=software:", out.strip().replace("\n", " | "), ")")
        verificar("desactivada por la opcion de arranque" in out,
                  "con la opcion de arranque la aceleracion esta desactivada")
        verificar("Motor en uso: software" in out, "y el motor sigue siendo el de software")

    # ------------------------------- 5: el kernel mapea un BAR de verdad
    extras = ["-netdev", "user,id=n0", "-device", "e1000,netdev=n0"]
    console = imagem_console()
    with Maquina("h134bar", "ide", imagem=console, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        pci = m.comando("infopc -t pci", 60)
        print("  (pci:", pci.strip().replace("\n", " | "), ")")
        onde = fam_qual_pci(pci, "8086:100e")
        verificar(onde is not None, "el QEMU tiene la e1000 de mentira")
        bus, disp, func = onde
        out = m.comando(f"amd --bar {bus} {disp} {func} 0 0 64", 60)
        print("  (bar:", out.strip().replace("\n", " | "), ")")
        verificar("mapeado en" in out, "el kernel mapea el BAR del dispositivo")
        verificar("mapa deshecho" in out, "y el mapa se puede deshacer")
        # La e1000 tiene su registro de estado en el primer dword del BAR 0.
        achado = re.search(r"\+0000: ([0-9a-f]{8})", out)
        verificar(achado is not None and achado.group(1) != "00000000",
                  f"los registros del dispositivo se leen a través del mapa ({achado.group(1) if achado else '?'})")

        # La maquinaria de lectura de los registros de una familia (los directos
        # y el par MM_INDEX/MM_DATA) con un dispositivo de verdad: aquí los
        # valores no son de una Radeon, pero el camino sí.
        out = m.comando('amd --leer %d %d %d "AMD R6xx/R7xx"' % (bus, disp, func), 60)
        print("  (ler:", out.strip().replace("\n", " | "), ")")
        verificar("DEVICE_ID" in out and "REV_ID" in out, "se leen los registros directos de la familia")
        verificar("GRBM_STATUS" in out, "se lee el registro indirecto (por MM_INDEX)")
        verificar("Fallo de p" not in out, "la lectura no causa ninguna falta de página")

    print("H134 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
