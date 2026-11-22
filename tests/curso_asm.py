#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Anexo «Ensamblador x86 en SaguiOS» (v8.5.6, D456): prueba en el PC, sin QEMU.

Comprueba rootfs/ayuda/sistemas-operativos/anexo-ensamblador{,_EN,_PT}.md:
  - existen las tres versiones (español, inglés y portugués de Brasil);
  - cada una es UTF-8, con la cabecera de la Ayuda (categoría «Conoce SaguiOS/Sistemas operativos», orden 92)
    y un primer título «# ...»; las tres comparten categoría y orden;
  - no hay caracteres de otros alfabetos (griego, cirílico, CJK, árabe, hebreo, coreano, devanagari);
  - los enlaces relativos apuntan a ficheros que existen;
  - los bloques de código de las tres versiones son idénticos (las órdenes y el código no se traducen);
  - cada cita «ruta:línea» (o «ruta:línea-línea») apunta a un fichero existente y a una línea que existe;
  - las citas clave dicen lo que el anexo afirma (ancla: fichero, línea y texto que debe contener);
  - todo el texto cabe en Latin-1 (la Ayuda dibuja con esa fuente: lo demás sale como basura);
  - el ejemplo de CPUID con TCC (el bloque marcado con «<!-- tcc: cpuid.c -->») es idéntico a rootfs/ejemplos/asm/cpuid.c,
    que prueba tests/curso_ejemplos.py dentro de SaguiOS en QEMU;
  - el ejemplo NASM del anexo (el bloque marcado con «<!-- nasm: sector.asm -->») se ensambla con `nasm -f bin`
    y sale un sector de 512 bytes que acaba en 55 AA. Si no hay `nasm` en el PC, la comprobación falla con el aviso.
Uso: python3 tests/curso_asm.py   (sale con 0 si todo bien, 1 si algo falla)
"""
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "rootfs", "ayuda", "sistemas-operativos")
BASE = "anexo-ensamblador"
SUFIJOS = ["", "_EN", "_PT"]
CABECERA_ES = "<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 92; titulo: Anexo: ensamblador x86 -->"
CABECERA = re.compile(r"^<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 92; titulo: .+ -->$")
OTROS_ALFABETOS = re.compile(
    "[Ͱ-ϿЀ-ӿ֐-׿؀-ۿऀ-ॿ"
    "぀-ヿ㐀-鿿가-힯]")
ENLACE = re.compile(r"\]\(([^)\s]+)\)")
BLOQUE = re.compile(r"```[a-z]*\n(.*?)```", re.S)
CITA = re.compile(r"(?<![\w/.])((?:boot|kernel|userland|rootfs|tests|docs|mejoras|tools)/[\w./-]+\.(?:asm|c|h|inc|py|md|sh|txt))(?::(\d+)(?:-(\d+))?)?")
MARCA_NASM = "<!-- nasm: sector.asm -->"

# Anclas: una línea concreta de una cita clave del anexo y un texto que debe aparecer en ella.
# Si el código cambia de sitio, esta prueba avisa y el anexo se corrige a la vez.
ANCLAS = [
    ("boot/mbr.asm", 31, "cli"),
    ("boot/mbr.asm", 39, "jmp 0x0000:.cs_zerado"),
    ("boot/mbr.asm", 55, "int 0x13"),
    ("boot/mbr.asm", 112, "int 0x10"),
    ("boot/mbr.asm", 150, "mov ah, 0x42"),
    ("boot/mbr.asm", 165, "parar:"),
    ("boot/mbr.asm", 191, "dw 0xAA55"),
    ("boot/estagio2.asm", 132, "[1/6] A20"),
    ("boot/estagio2.asm", 296, "lgdt [gdt_descritor]"),
    ("boot/estagio2.asm", 300, "jmp SEL_CODIGO:modo_protegido"),
    ("boot/estagio2.asm", 508, "dword [di + 20]"),
    ("boot/estagio2.asm", 512, "int 0x15"),
    ("boot/estagio2.asm", 1735, "modo_protegido:"),
    ("boot/estagio2.asm", 1742, "mov esp, 0x7C00"),
    ("boot/estagio2.asm", 1790, "0x9A"),
    ("kernel/arch/entrada.asm", 24, "rep stosb"),
    ("kernel/arch/entrada.asm", 29, "call inicio_kernel"),
    ("kernel/arch/vetores.asm", 36, "%endrep"),
    ("kernel/arch/vetores.asm", 53, "mov esp, eax"),
    ("kernel/arch/vetores.asm", 60, "add esp, 4"),
    ("kernel/arch/vetores.asm", 70, "iretd"),
    ("kernel/arch/anel3_testes.asm", 26, "int 0x80"),
    ("kernel/arch/anel3_testes.asm", 46, "cli"),
    ("kernel/arch/ap_trampolim.asm", 80, "protegido:"),
    ("kernel/arch/ap_trampolim.asm", 97, "mov cr4, eax"),
    ("kernel/arch/ap_trampolim.asm", 136, "mov cr3, eax"),
    ("kernel/arch/ap_trampolim.asm", 145, "mov cr0, eax"),
    ("boot/uefi/trampolim.asm", 48, "entrada64:"),
    ("boot/uefi/trampolim.asm", 57, "o64 retf"),
    ("boot/uefi/trampolim.asm", 68, "LME"),
    ("boot/uefi/trampolim.asm", 73, "entrada32:"),
    ("kernel/arch/idt.c", 70, "0x8E"),
    ("kernel/arch/idt.c", 72, "0xEE"),
    ("kernel/arch/idt.c", 297, "tratar_interrupcao"),
    ("kernel/inc/cpu.h", 11, "SEL_UCODIGO"),
    ("kernel/inc/cpu.h", 27, "VETOR_SISTEMA 0x80"),
    ("kernel/inc/cpu.h", 81, "sti"),
    ("kernel/inc/cpu.h", 82, "cli"),
    ("kernel/inc/abi.h", 5, "int 0x80"),
    ("kernel/inc/portas.h", 15, "inb %1, %0"),
    ("kernel/nucleo/processos.c", 603, "espaco_ativar(prox->espaco)"),
    ("kernel/nucleo/processos.c", 714, "processo_preparar_usuario"),
    ("kernel/nucleo/processos.c", 723, "0x202"),
    ("kernel/mem/paginacao.c", 492, "mov %0, %%cr3"),
    ("kernel/arch/fpu.c", 107, "fxsave"),
    ("kernel/arch/fpu.c", 295, "CR4_OSFXSR"),
    ("kernel/arch/fpu.c", 13, "512 bytes"),
    ("kernel/arch/fpu.c", 19, "-mgeneral-regs-only"),
    ("kernel/inc/sse2.h", 13, "sse2_copiar"),
    ("kernel/dispositivos/ata.c", 594, "sti; nop; nop; cli"),
    ("kernel/dispositivos/ahci.c", 167, "rdtsc"),
    ("kernel/dispositivos/sensores.c", 74, "cpuid"),
    ("kernel/dispositivos/gpu3d_r600.c", 1193, "fpu_kernel_inicio"),
    ("kernel/arch/msr.c", 105, "cpuid"),
    ("kernel/arch/excecoes.c", 34, "cli; hlt"),
    ("userland/programas/simd.c", 19, "R_MMX"),
    ("userland/programas/simd.c", 42, "fsqrt"),
    ("userland/programas/simd.c", 52, "paddw"),
    ("userland/programas/simd.c", 64, "mulps"),
    ("userland/programas/simd.c", 138, "ldmxcsr"),
    ("userland/terceros/tcc/fuentes/libtcc.c", 1206, "\"s\""),
    ("rootfs/ejemplos/mico/parpadeo.asm", 2, "chip8asm"),
    ("docs/arquitecturas.md", 9, "x86-64"),
]

fallos = []


def verificar(cond, texto):
    if cond:
        print("ok    " + texto)
    else:
        print("FALLO " + texto)
        fallos.append(texto)


def leer(ruta):
    with io.open(ruta, encoding="utf-8") as f:
        return f.read()


def lineas_de(ruta_rel):
    return leer(os.path.join(RAIZ, ruta_rel)).split("\n")


def main():
    rutas = {s: os.path.join(DIR, BASE + s + ".md") for s in SUFIJOS}
    for s, ruta in rutas.items():
        verificar(os.path.isfile(ruta), "existe rootfs/ayuda/sistemas-operativos/" + os.path.basename(ruta))
    if fallos:
        return 1

    textos = {}
    for s, ruta in rutas.items():
        nombre = os.path.basename(ruta)
        try:
            textos[s] = leer(ruta)
        except UnicodeDecodeError:
            verificar(False, nombre + " es UTF-8")
            continue
        verificar(True, nombre + " es UTF-8")
        lineas = textos[s].split("\n")
        cab = lineas[0] if lineas else ""
        if s == "":
            verificar(cab == CABECERA_ES, nombre + " tiene la cabecera de la Ayuda (categoría, orden 92)")
        else:
            verificar(bool(CABECERA.match(cab)), nombre + " tiene la cabecera de la Ayuda (categoría, orden 92)")
        primera = next((l for l in lineas[1:] if l.strip()), "")
        verificar(primera.startswith("# ") and len(primera) > 3, nombre + " tiene un primer título «# ...»")
        otros = OTROS_ALFABETOS.findall(textos[s])
        verificar(not otros, nombre + " no tiene caracteres de otros alfabetos")
        for destino in ENLACE.findall(textos[s]):
            if "://" in destino or destino.startswith(("#", "mailto:")):
                continue
            ruta_dest = os.path.normpath(os.path.join(DIR, destino.split("#")[0]))
            verificar(os.path.isfile(ruta_dest), nombre + " enlaza a " + destino)
        for m in CITA.finditer(textos[s]):
            fichero, ini, fin = m.group(1), m.group(2), m.group(3)
            ruta_cita = os.path.join(RAIZ, fichero)
            if not os.path.isfile(ruta_cita):
                if "/fuentes/" in fichero:   # fuentes de terceros que se descargan (tools/*_fuentes.sh): no están en git
                    print("aviso", nombre, "cita", fichero, "(fuentes descargadas que aquí no están)")
                    continue
                verificar(False, nombre + " cita " + fichero + " (no existe)")
                continue
            if ini:
                n = len(lineas_de(fichero))
                hasta = int(fin) if fin else int(ini)
                verificar(int(ini) <= n and hasta <= n,
                          "%s cita %s:%s (el fichero tiene %d líneas)" % (nombre, fichero, ini, n))

    for s in SUFIJOS[1:]:
        if "" in textos and s in textos:
            verificar(cabecera_sin_titulo(textos[""]) != "" and
                      cabecera_sin_titulo(textos[s]) == cabecera_sin_titulo(textos[""]),
                      BASE + s + ".md tiene la misma categoría y orden que el original")

    bloques = {s: bloques_de(textos[s]) for s in SUFIJOS if s in textos}
    if "" in bloques:
        for s in SUFIJOS[1:]:
            if s in bloques:
                verificar(bloques[""] == bloques[s],
                          "%s%s.md tiene los mismos bloques de código que el original (%d bloques)"
                          % (BASE, s, len(bloques[""])))

    for ancla in ANCLAS:
        fichero, linea, texto = ancla
        ruta = os.path.join(RAIZ, fichero)
        if not os.path.isfile(ruta):
            if "/fuentes/" in fichero:
                print("aviso: ancla en fuentes descargadas que aquí no están:", fichero)
                continue
            verificar(False, "ancla: falta " + fichero)
            continue
        l = lineas_de(fichero)
        ok = len(l) >= linea and texto in l[linea - 1]
        verificar(ok, "ancla %s:%d contiene «%s»" % (fichero, linea, texto))

    for suf, texto in sorted(textos.items()):
        fuera = sorted({c for c in texto if ord(c) > 255})
        verificar(not fuera, "%s%s.md: todo el texto cabe en Latin-1 (%s)" % (BASE, suf, " ".join("U+%04X" % ord(c) for c in fuera[:6])))
    comprobar_cpuid(textos.get(""))
    comprobar_nasm(textos.get(""))

    if fallos:
        print("\n%d comprobación(es) fallida(s)" % len(fallos))
        return 1
    print("\nanexo de ensamblador: todo bien")
    return 0


def bloques_de(texto):
    """Bloques de código: los vallados (```) y los sangrados (4 espacios o más, en grupos contiguos)."""
    vallados = BLOQUE.findall(texto)
    sin_vallas = BLOQUE.sub("", texto)
    grupos, actual = [], []
    lineas = sin_vallas.split("\n")
    for i, linea in enumerate(lineas):
        if linea.startswith("    ") and linea.strip():
            actual.append(linea)
        elif not linea.strip() and actual:
            # una línea en blanco sigue al grupo solo si la siguiente no vacía también está sangrada
            siguiente = next((l for l in lineas[i + 1:] if l.strip()), "")
            if siguiente.startswith("    "):
                actual.append("")
            else:
                grupos.append("\n".join(actual).rstrip())
                actual = []
        else:
            if actual:
                grupos.append("\n".join(actual).rstrip())
                actual = []
    if actual:
        grupos.append("\n".join(actual).rstrip())
    return vallados + grupos


def cabecera_sin_titulo(texto):
    """Categoría y orden de la cabecera (sin el título, que se traduce)."""
    cab = texto.split("\n", 1)[0]
    m = re.match(r"<!-- categoria: (.+?); orden: (\d+); titulo: .+ -->$", cab)
    return (m.group(1), m.group(2)) if m else ""


def comprobar_cpuid(texto):
    if texto is None:
        verificar(False, "ejemplo CPUID: no hay texto del anexo")
        return
    m = re.search(r"<!-- tcc: cpuid\.c -->\n```[a-z]*\n(.*?)```", texto, re.S)
    if not m:
        verificar(False, "ejemplo CPUID: falta el bloque marcado «<!-- tcc: cpuid.c -->»")
        return
    ruta = os.path.join(RAIZ, "rootfs", "ejemplos", "asm", "cpuid.c")
    verificar(os.path.isfile(ruta), "ejemplo CPUID: existe rootfs/ejemplos/asm/cpuid.c")
    if os.path.isfile(ruta):
        with io.open(ruta, encoding="utf-8") as f:
            verificar(f.read() == m.group(1), "ejemplo CPUID: el bloque del anexo es idéntico a rootfs/ejemplos/asm/cpuid.c")


def comprobar_nasm(texto):
    if texto is None:
        verificar(False, "ejemplo NASM: no hay texto del anexo")
        return
    m = re.search(re.escape(MARCA_NASM) + r"\n```[a-z]*\n(.*?)```", texto, re.S)
    if not m:
        verificar(False, "ejemplo NASM: falta el bloque marcado «" + MARCA_NASM + "»")
        return
    nasm = shutil.which("nasm")
    if not nasm:
        verificar(False, "ejemplo NASM: no hay «nasm» en el PC (sudo apt install nasm, según docs/CONTINUAR.md)")
        return
    directorio = tempfile.mkdtemp(prefix="curso_asm_")
    fuente = os.path.join(directorio, "sector.asm")
    salida = os.path.join(directorio, "sector.bin")
    with io.open(fuente, "w", encoding="utf-8", newline="\n") as f:
        f.write(m.group(1))
    r = subprocess.run([nasm, "-f", "bin", fuente, "-o", salida], capture_output=True, text=True)
    verificar(r.returncode == 0, "ejemplo NASM: nasm -f bin ensambla el bloque" + ("" if r.returncode == 0 else " (" + r.stderr.strip() + ")"))
    if r.returncode == 0:
        with open(salida, "rb") as f:
            datos = f.read()
        verificar(len(datos) == 512, "ejemplo NASM: el sector mide 512 bytes (mide %d)" % len(datos))
        verificar(datos[-2:] == b"\x55\xaa", "ejemplo NASM: el sector acaba en 55 AA")
    shutil.rmtree(directorio, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
