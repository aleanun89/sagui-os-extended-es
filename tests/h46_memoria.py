#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H46 - frontera kernel/usuario en 2 GB (D049).

Criterio: con 256 MB el kernel gestiona ~250 MB, los programas funcionan y la
autoprueba de memoria pasa; con 2 GB gestiona ~1,9 GB y un programa reserva
más de 1 GB (lo que obliga a usar páginas físicas por encima de 1 GB) y
comprueba un patrón en todas ellas; con 3 GB el mapa 1:1 se queda en 1920 MB
(las ventanas empiezan en 0x78000000) y el sistema arranca y ejecuta
programas. Desde la D348 lo que pasa de 1920 MB es memoria alta para los
programas: con 2 GB se gestionan ~2 GB y con 3 GB, ~3 GB (h350 lo prueba a fondo). El cc genera ejecutables en 0x80000000 que funcionan, y un programa
que lee la antigua zona de usuario (0x40000000) termina con "Fallo de página"
sin afectar al resto.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste  # noqa: E402


def gestionada(out):
    g = re.search(r"Gestionada por el kernel:\s+(\d+) MB", out)
    return int(g.group(1)) if g else -1


def libre(out):
    g = re.search(r"Libre:\s+(\d+) MB", out)
    return int(g.group(1)) if g else -1


def main():
    console = imagem_console()

    # 256 MB: autoprueba de memoria y programas.
    with Maquina("h46-256M", "ide", imagem=imagem_de_teste("memoria"), memoria="256M") as m:
        m.esperar_serial("[TESTE] memoria ", 120)
        s = m.serial()
        verificar("[TESTE] memoria ok" in s, "[256M] autoprueba de memoria pasa")
        verificar("1:1 ate 10000000 (256 MB)" in s, "[256M] mapa 1:1 solo hasta el final de la RAM (256 MB)")
    with Maquina("h46-256M", "ide", imagem=console, memoria="256M") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("mem")
        g = gestionada(out)
        # Lo que no gestiona el kernel es él mismo y el disco en RAM, que crece con el
        # sistema (con el sshd, v8.0b5, quedan 239 MB): margen hasta 232.
        verificar(232 <= g <= 256, f"[256M] mem: {g} MB gestionados (~240)")
        out = m.comando("infopc -t", 120)
        verificar(re.search(r"Gestionada por el kernel:\s+2(?:3[2-9]|[45]\d) MB", out) and "D049" in out,
                  "[256M] infopc: memoria gestionada y nota D049")
        out = m.comando("/testes/memoria 64", 120)
        verificar("patron ok en 64 MB" in out and "codigo 0" in out, "[256M] un programa reserva y comprueba 64 MB")

    # Imagen por defecto (512 MB): cc en 0x80000000 y fallo de página en 0x40000000.
    with Maquina("h46-512M", "ide", imagem=console, memoria="512M") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        # D349: páginas de 4 MB sin PAE, de 2 MB con PAE (la CPU del QEMU lo tiene).
        verificar(re.search(r"1:1 ate 20000000 \(512 MB\), paginas de [24] MB", s) is not None,
                  "[512M] mapa 1:1 en páginas grandes hasta 512 MB")
        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=120)
        verificar("(cc) terminou com codigo 0" in out, "cc compila /ejemplos/hola.c")
        out = m.comando("/tmp/hola Tito")
        verificar("Hola, Tito!" in out and "codigo 0" in out, "el ejecutable del cc (en 0x80000000) funciona")
        out = m.comando("/testes/memoria ler 40000000", 60)
        verificar("Fallo de p" in out and "0x40000000" in out and "lectura" in out,
                  "leer 0x40000000 (antigua zona de usuario): fallo de página")
        verificar("no debería" not in out, "la lectura no devolvió nada")
        out = m.comando("echo sigo vivo")
        verificar("sigo vivo" in out, "solo termina ese programa: el shell sigue")
        m.captura("H46-fallo-0x40000000")

    # 2 GB: ~1,9 GB gestionados y más de 1 GB en un solo programa.
    with Maquina("h46-2G", "ide", imagem=console, memoria="2G") as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("1:1 ate 78000000 (1920 MB)" in s, "[2G] mapa 1:1 hasta las ventanas (0x78000000)")
        out = m.comando("mem")
        g, l0 = gestionada(out), libre(out)
        verificar(1850 <= g <= 2048, f"[2G] mem: {g} MB gestionados (~1,9 GB bajos + la alta, D348)")
        out = m.comando("/testes/memoria 1300", 600)
        r = re.search(r"usada (\d+) MB", out)
        usada = int(r.group(1)) if r else -1
        verificar("1300 MB reservados en 0x8" in out, "[2G] un programa reserva 1300 MB a partir de 0x80000000")
        verificar(usada > 1300, f"[2G] el kernel tiene {usada} MB en uso: hay páginas por encima de 1 GB físico")
        verificar("patron ok en 1300 MB" in out and "codigo 0" in out, "[2G] patrón correcto en los 1300 MB")
        out = m.comando("mem")
        verificar(abs(libre(out) - l0) <= 4, "[2G] al terminar, el programa devuelve la memoria")
        m.captura("H46-2G-mem")

    # 3 GB: el kernel se queda en 2 GB y no tropieza con el agujero de MMIO.
    with Maquina("h46-3G", "ide", imagem=console, memoria="3G") as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("1:1 ate 78000000 (1920 MB)" in s, "[3G] mapa 1:1 limitado a 0x78000000")
        out = m.comando("mem")
        g = gestionada(out)
        verificar(2900 <= g <= 3072, f"[3G] mem: {g} MB gestionados (con la memoria alta, D348)")
        r = re.search(r"Memoria RAM total \(E820\):\s+(\d+) MB", out)
        verificar(r and int(r.group(1)) > 3000, "[3G] el E820 ve los 3 GB")
        out = m.comando("/testes/memoria 256", 300)
        verificar("patron ok en 256 MB" in out and "codigo 0" in out, "[3G] los programas funcionan")
    return 0


if __name__ == "__main__":
    sys.exit(main())
