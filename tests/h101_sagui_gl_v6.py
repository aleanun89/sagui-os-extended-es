#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H101 (D130, mejora 054) - SaguiGL v6: texturas, mezcla, niebla y luces.

Criterio:
  - el motor libsgl (userland/libsgl/sgl.c) se compila en el anfitrión con gcc
    (tests/sgl/sglv6.c, -DSGL_ANFITRIAO) y da los valores esperados de la
    escena con textura y de la escena completa;
  - dentro del SaguiOS el mismo programa (/bin/sglv6) da exactamente los mismos
    valores: el motor es determinista y la ruta SIMD (SSE2) coincide bit a bit
    con la ruta de referencia;
  - con solo la textura se ven los 64 texels de la textura de 8x8 (si no se
    viera la textura, la escena sería de un solo color);
  - la escena completa (textura + luz + niebla + mezcla) tiene muchos colores y
    la ruta SIMD no se diferencia de la de referencia en ningún píxel.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, RAIZ  # noqa: E402

# Las mismas banderas con las que se compila el motor para el SaguiOS
# (Makefile: SGL_FLAGS), para que el resultado sea el mismo.
FLAGS = ["-O2", "-msse2", "-mfpmath=sse", "-ffp-contract=off", "-fno-math-errno", "-Wall"]


def esperado():
    """Compila la prueba en el anfitrión y devuelve sus dos escenas."""
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


def main():
    esp = esperado()
    ref = {k: dict(v) for k, v in esp.items()}
    verificar("textura" in esp and "completa" in esp, "el anfitrión da las dos escenas")
    print("  (anfitrión:", "textura", esp["textura"], "| completa", esp["completa"], ")")

    # La textura tiene que verse: con solo la textura hay 64 colores (los 64
    # texels), no uno solo.
    verificar(esp["textura"].get("pintados") == "9216", f"la escena de la textura pinta 9216 píxeles ({esp['textura'].get('pintados')})")
    verificar(esp["textura"].get("cores") == "64", f"se ven los 64 texels de la textura ({esp['textura'].get('cores')})")
    # La escena completa tiene que tener muchos colores y las dos rutas, la de
    # referencia y la SIMD, tienen que coincidir píxel a píxel.
    verificar(esp["completa"].get("pintados") == "9216", f"la escena completa pinta 9216 píxeles ({esp['completa'].get('pintados')})")
    verificar(int(esp["completa"].get("cores", "0")) > 100, f"la escena completa tiene muchos colores ({esp['completa'].get('cores')})")
    verificar(esp["completa"].get("dif") == "0", f"la ruta SIMD coincide con la de referencia ({esp['completa'].get('dif')})")

    console = imagem_console()
    with Maquina("h101", "ide", imagem=console) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sglv6", 120)
        print("  (SaguiOS:", out.strip().replace("\n", " | "), ")")
        sagui = _linhas(out)
        verificar("textura" in sagui and "completa" in sagui, "sglv6 dentro del SaguiOS da las dos escenas")
        verificar(sagui.get("textura") == ref["textura"],
                  f"la escena de la textura en el SaguiOS: {sagui.get('textura')} = anfitrión {ref['textura']}")
        verificar(sagui.get("completa") == ref["completa"],
                  f"la escena completa en el SaguiOS: {sagui.get('completa')} = anfitrión {ref['completa']}")
        verificar("FALLO" not in out, "sglv6 no informa de fallos dentro del SaguiOS")
    print("H101 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
