#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H413 - GNU make dentro de SaguiOS (D413, docs/make.md).

Partes (por defecto todas; o las que se pidan: h413_make.py pc saguios):
  pc       /apps/bin/make es un ELF32 estático; /apps/make lleva el ejemplo, el léeme y la licencia;
           /apps/aoen/fuentes y /apps/tcc/fuentes llevan su Makefile; docs y fuentes fuera de git.
  saguios  en SaguiOS: un proyecto de varios ficheros con dependencias, include, variables y regla
           implícita se compila con make y tcc; la segunda vez no hace nada; al tocar un fichero solo
           se recompila ese; -n, -C, variables en la línea de órdenes, -j2, wildcard y funciones;
           un error de la receta para make; y AoEN y TCC se compilan dentro con make.
Capturas: docs/capturas/H413-*.png. (AoEN compilado con make se comprueba por su existencia; que arranca lo prueba h392 con compilar.sh.)
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, BUILD  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
FUENTES = os.path.join(RAIZ, "userland", "terceros", "make", "fuentes", ".preparado")


def esperar(m, padrao, tempo, desde):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError("tiempo agotado esperando %r (%r)" % (padrao, m.serial()[desde:][-600:]))


def orden(m, texto, tempo):
    """Escribe una orden en el Terminal y espera al prompt siguiente; devuelve lo que salió."""
    marca = len(m.serial())
    m.digitar(texto, 0.03)
    m.tecla("ret")
    esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(texto))
    return m.serial()[marca:].replace("\r", "")


def pc():
    mk = os.path.join(APPS_RAIZ, "bin", "make")
    verificar(os.path.isfile(mk), "la imagen lleva /apps/bin/make")
    h = subprocess.run(["readelf", "-hl", "-W", os.path.join(BUILD, "userland", "bin", "make.elf")],
                       capture_output=True, text=True).stdout
    verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "/apps/bin/make: ELF32 estático de i386")
    for f in ("leeme.txt", "LICENCIA-MAKE.txt", "ejemplos/proyecto/Makefile", "ejemplos/proyecto/principal.c",
              "ejemplos/proyecto/opciones.mk"):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "make", f)), "/apps/make/%s" % f)
    verificar(os.path.isfile(os.path.join(APPS_RAIZ, "aoen", "fuentes", "Makefile")), "/apps/aoen/fuentes/Makefile")
    if os.path.isdir(os.path.join(APPS_RAIZ, "tcc", "fuentes")):
        verificar(os.path.isfile(os.path.join(APPS_RAIZ, "tcc", "fuentes", "Makefile")), "/apps/tcc/fuentes/Makefile")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("GNU make" in t and "GPL-3.0-or-later" in t, "docs/terceros.md tiene la fila de GNU make")
    ig = subprocess.run(["git", "ls-files", "userland/terceros/make"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar("fuentes/" not in ig, "las fuentes de make no van en git")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D413." in d, "D413 está en docs/decisoes.md")


def saguios():
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "make")):
        print("AVISO: la imagen no lleva make (tools/make_fuentes.sh y make): se salta la parte «saguios»")
        return
    import h136_opentyrian as h136
    lento = 1 if KVM else 4
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos-mb", "64"])
    with Maquina("h413", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150, 0)
        time.sleep(4)
        h136.focar_terminal(m)
        out = orden(m, "make --version", 30)
        verificar("GNU Make 4.4.1" in out, "make --version (%r)" % out[-200:])
        orden(m, "cp -r /apps/make/ejemplos/proyecto /tmp/proyecto", 60)
        orden(m, "cd /tmp/proyecto", 30)
        # 1. primera compilación: variables, include, regla implícita, dependencias de cabeceras
        out = orden(m, "make", 120 * lento)
        verificar("enlazado: calc" in out and out.count("-c -o") == 3, "make compila el proyecto con tcc (%r)" % out[-500:])
        verificar("principal.o" in out and "calculos.o" in out and "util.o" in out, "compila los tres ficheros")
        out = orden(m, "./calc 7", 30)
        verificar("factorial(7) = 5040" in out and "proyecto 1.0" in out and "7 es primo" in out,
                  "el programa funciona (include de opciones.mk, DEFINES) (%r)" % out[-200:])
        m.captura("H413-1-make-proyecto")
        # 2. recompilar solo lo cambiado
        out = orden(m, "make", 30)
        verificar("-c -o" not in out and "is up to date" in out, "la segunda vez no recompila nada (%r)" % out[-200:])
        orden(m, "sleep 2", 10)
        out = orden(m, "touch calculos.c && make", 60 * lento)
        verificar(out.count("-c -o") == 1 and "-c -o calculos.o" in out and "enlazado: calc" in out,
                  "al tocar calculos.c solo se recompila calculos.c y se enlaza (%r)" % out[-400:])
        orden(m, "sleep 2", 10)
        out = orden(m, "touch util.h && make", 90 * lento)
        verificar(out.count("-c -o") == 2 and "-c -o principal.o" in out and "-c -o util.o" in out,
                  "al tocar util.h se recompilan los dos que lo incluyen (%r)" % out[-400:])
        # 3. objetivos, -n, variables de la línea de órdenes, -C
        out = orden(m, "make -n clean", 30)
        verificar("rm -f" in out and "principal.o" in out, "make -n muestra sin ejecutar (%r)" % out[-200:])
        out = orden(m, "make probar", 60)
        verificar("factorial(6) = 720" in out, "objetivo falso probar (%r)" % out[-200:])
        out = orden(m, "make clean && ls", 30)
        verificar("principal.c" in out and "principal.o" not in out.split("rm -f")[-1].split("\n", 1)[-1], "make clean (%r)" % out[-200:])
        out = orden(m, "make CC=nocompilador", 30)
        verificar("nocompilador" in out and ("Error" in out or "error" in out), "CC en la línea de órdenes y el error de la receta para make (%r)" % out[-300:])
        orden(m, "cd /", 30)
        out = orden(m, "make -C /tmp/proyecto clean && make -C /tmp/proyecto -s 2>&1 | tail -3", 120 * lento)
        verificar("Entering directory" in out and "enlazado: calc" in out, "-C y -s (%r)" % out[-300:])
        # 4. -j2, wildcard, funciones, $(shell), cadenas de reglas
        orden(m, "cd /tmp", 30)
        orden(m, "mkdir j && cd j", 30)
        # el Makefile se escribe línea a línea con printf (las tabulaciones, con \\t)
        mk = [
            r"printf 'A = $(wildcard *.txt)\n' > Makefile",
            r"printf 'B = $(patsubst %%.txt,%%.out,$(A))\n' >> Makefile",
            r"printf 'todo: $(B)\n\t@echo hecho: $(B) $(words $(A)) $(shell echo hola)\n' >> Makefile",
            r"printf '%%.out: %%.txt\n\t@echo convierto $< en $@; sleep 1; cp $< $@\n' >> Makefile",
        ]
        for l in mk:
            orden(m, l, 20)
        orden(m, "echo uno > a.txt; echo dos > b.txt; echo tres > c.txt; echo cuatro > d.txt", 30)
        t0 = time.time()
        out = orden(m, "make -j4", 120 * lento)
        t = time.time() - t0
        verificar("hecho: a.out b.out c.out d.out 4 hola" in out and out.count("convierto") == 4,
                  "-j4, wildcard, patsubst, words y shell (%r)" % out[-400:])
        out = orden(m, "make -j4 -O", 30)
        verificar("hecho:" in out, "segunda pasada con -O sin trabajo (%r)" % out[-300:])
        # 5. AoEN y TCC con make dentro
        if os.path.isdir(os.path.join(APPS_RAIZ, "aoen", "fuentes")):
            orden(m, "cp -r /apps/aoen/fuentes /tmp/aoen", 120 * lento)
            orden(m, "cd /tmp/aoen", 30)
            t0 = time.time()
            out = orden(m, "time make -j2", 900 if not KVM else 300)
            segundos = time.time() - t0
            verificar("listo: ./aoen" in out, "make compila AoEN dentro de SaguiOS (%r)" % out[-400:])
            print("    AoEN con make -j2 en %.1f s de reloj (%s)" % (segundos, "KVM" if KVM else "TCG"))
            out = orden(m, "make", 30)
            verificar("-c -o" not in out, "AoEN: la segunda vez no recompila (%r)" % out[-200:])
            orden(m, "sleep 2; touch game.c", 20)
            out = orden(m, "make", 300 * lento)
            verificar(out.count("-c -o") == 1 and "listo: ./aoen" in out, "AoEN: al tocar game.c solo se recompila ese (%r)" % out[-300:])
            out = orden(m, "test -x aoen && echo AOEN_EXISTE; ls -l aoen", 30)
            verificar("AOEN_EXISTE" in out, "make deja el ejecutable ./aoen (%r)" % out[-200:])
            m.captura("H413-2-aoen-con-make")
            orden(m, "cd /tmp", 30)
        if os.path.isdir(os.path.join(APPS_RAIZ, "tcc", "fuentes")):
            orden(m, "cp -r /apps/tcc/fuentes /tmp/tccf", 120 * lento)
            orden(m, "cd /tmp/tccf", 30)
            out = orden(m, "time make", 900 if not KVM else 300)
            verificar("listo: ./tcc" in out, "TCC se compila a sí mismo con make (%r)" % out[-300:])
            out = orden(m, "./tcc -run /apps/tcc/ejemplos/hola.c make", 120 * lento)
            verificar("Hola desde TCC (2 argumentos)" in out, "el TCC hecho con make funciona (%r)" % out[-200:])
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def main():
    if not os.path.exists(FUENTES):
        print("AVISO: faltan las fuentes de make (tools/make_fuentes.sh): h413 se salta")
        return 0
    partes = sys.argv[1:] or ["pc", "saguios"]
    for p in partes:
        {"pc": pc, "saguios": saguios}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
