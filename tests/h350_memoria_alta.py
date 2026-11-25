#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H350 - memoria alta sin PAE (D348, fase 1 de la mejora 125).

Criterio:
  - el auditor (tools/auditar_memalta.py) no encuentra en el kernel ninguna
    dirección física de página de programa usada como puntero;
  - con -m 3G y -m 4G (BIOS) el kernel gestiona más de 1,875 GB: la zona alta
    aparece en el arranque, en `free` y en `mem`; cuatro procesos a la vez
    llenan 600 MB cada uno (2,4 GB en total, más de lo que cabe en la baja),
    esperan a que el sistema tenga en uso lo de todos y comprueban que el
    patrón y la suma de todo su contenido no cambiaron; después la memoria
    vuelve, y no hay excepciones del kernel ni errores de las ventanas;
  - lo mismo con --smp (4 CPU) y con UEFI (OVMF de 64 bits), si está;
  - con -m 512M y -m 1G no hay zona alta y todo sigue como antes;
  - las autopruebas del kernel (memoria, paginacao, anel3) pasan con -m 3G.
Con SAGUI_KVM=1 se prueba con KVM (la CPU es la del anfitrión).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, achar_ovmf, BUILD  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UEFI_DIR = os.path.join(BUILD, "boot", "uefi")


def free_m(m):
    """{"mem": (total, usado, libre), "baja": ..., "alta": ...} en MB, de `free -m`."""
    out = m.comando("free -m")
    r = {}
    for nome, rot in (("mem", r"Mem:"), ("baja", r"baja:"), ("alta", r"alta:")):
        g = re.search(rot + r"\s+(\d+)\s+(\d+)\s+(\d+)", out)
        r[nome] = tuple(int(x) for x in g.groups()) if g else (-1, -1, -1)
    return r, out


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("ERRO memalta" not in s, f"[{et}] sin errores de las ventanas temporales")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")


def com_alta(nome, memoria, et, imagem=None, nucleos=1, firmware=None, k=4, mb=600):
    img = imagem or imagem_console()
    with Maquina(nome, "ide", imagem=img, memoria=memoria, nucleos=nucleos, firmware=firmware) as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        g = re.search(r"memoria alta: (\d+) paginas livres \((\d+) MB\)", s)
        alta_mb = int(g.group(2)) if g else 0
        verificar(alta_mb > 800, f"[{et}] el arranque ve memoria alta ({alta_mb} MB)")
        verificar("janelas temporais" in s, f"[{et}] ventanas temporales reservadas")
        f0, out = free_m(m)
        verificar(f0["mem"][0] > 1920, f"[{et}] free: {f0['mem'][0]} MB gestionados (más de 1,875 GB)")
        verificar(f0["alta"][0] > 800 and f0["baja"][0] > 1800, f"[{et}] free: baja {f0['baja'][0]} MB, alta {f0['alta'][0]} MB")
        out = m.comando("mem")
        verificar(re.search(r"Alta \(solo programas\):\s+\d{3,} MB", out) is not None, f"[{et}] mem muestra la memoria alta")
        out = m.comando(f"/testes/memoria varios {k} {mb}", 1800)
        picos = [int(x) for x in re.findall(r"pico del sistema (\d+) MB", out)]
        altas = [int(x) for x in re.findall(r"retener \d+: pico del sistema \d+ MB, alta usada (\d+) MB", out)]
        pico = max(picos) if picos else -1
        verificar(f"varios: {k} de {k} procesos con {mb} MB cada uno: todos ok" in out,
                  f"[{et}] {k} procesos de {mb} MB: patrón y suma correctos tras llenar la memoria")
        verificar(out.count("igual, 0 errores") == k, f"[{et}] las {k} sumas de comprobación coinciden")
        verificar(pico > 1920, f"[{et}] en uso a la vez: {pico} MB (más que la memoria baja)")
        verificar(altas and max(altas) > 800, f"[{et}] páginas altas en uso: {max(altas) if altas else -1} MB")
        f1, _ = free_m(m)
        verificar(abs(f1["mem"][2] - f0["mem"][2]) <= 8, f"[{et}] al terminar vuelve la memoria ({f0['mem'][2]} -> {f1['mem'][2]} MB libres)")
        verificar(f1["alta"][2] >= f0["alta"][2] - 1, f"[{et}] vuelve toda la alta")
        # Programas normales con la alta en uso: el cc compila y el ejecutable funciona.
        out = m.comando("cc /ejemplos/hola.c -o /tmp/hola", tempo=180)
        out2 = m.comando("/tmp/hola Tito")
        verificar("(cc) terminou com codigo 0" in out and "Hola, Tito!" in out2, f"[{et}] cc y su ejecutable funcionan")
        sem_erros(m.serial(), et)
        m.captura(f"H350-{nome}")


def sem_alta(memoria):
    et = memoria
    with Maquina(f"h350-{memoria}", "ide", imagem=imagem_console(), memoria=memoria) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("memoria alta:" not in s, f"[{et}] sin zona alta")
        f0, _ = free_m(m)
        verificar(f0["alta"][0] == 0 and f0["mem"][0] == f0["baja"][0], f"[{et}] free: alta 0, todo baja ({f0['mem'][0]} MB)")
        out = m.comando("/testes/memoria 200", 300)
        verificar("patron ok en 200 MB" in out and "codigo 0" in out, f"[{et}] un programa reserva y comprueba 200 MB")
        out = m.comando("/testes/memoria varios 2 100", 300)
        verificar("varios: 2 de 2 procesos con 100 MB cada uno: todos ok" in out, f"[{et}] dos procesos a la vez")
        sem_erros(m.serial(), et)


def autotestes():
    """Las autopruebas del kernel (memoria, paginación, anillo 3) con -m 3G: cuentan baja y alta."""
    for t in ("memoria", "paginacao", "anel3"):
        with Maquina(f"h350-auto-{t}", "ide", imagem=imagem_de_teste(t), memoria="3G") as m:
            m.esperar_serial(f"[TESTE] {t} ", 180)
            s = m.serial()
            verificar(f"[TESTE] {t} ok" in s and "[TESTE] falha" not in s, f"[3G] autoprueba del kernel '{t}' pasa")
            verificar("memoria alta:" in s, f"[3G] autoprueba '{t}' con zona alta")
            sem_erros(s, f"3G {t}")


def main():
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "auditar_memalta.py")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "auditor: ninguna dirección física de programa usada como puntero" +
              ("" if r.returncode == 0 else "\n" + r.stdout))

    so = os.environ.get("H350_SO", "")   # p. ej. H350_SO=3G para una sola parte
    partes = [
        ("auto", autotestes),
        ("512M", lambda: sem_alta("512M")),
        ("1G", lambda: sem_alta("1G")),
        ("3G", lambda: com_alta("h350-3G", "3G", "3G")),
        ("4G", lambda: com_alta("h350-4G", "4G", "4G", k=4, mb=650)),
        ("smp", lambda: com_alta("h350-smp", "3G", "3G --smp", imagem=imagem_de_teste("", ["--vesa", "--console", "--smp"]),
                                 nucleos=4)),
    ]
    if achar_ovmf("x64") and os.path.isdir(UEFI_DIR):
        partes.append(("uefi", lambda: com_alta("h350-uefi", "3G", "3G UEFI", firmware="x64",
                                                 imagem=imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI_DIR]))))
    for nome, f in partes:
        if so and nome not in so.split(","):
            continue
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
