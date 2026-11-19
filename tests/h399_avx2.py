#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H399 - AVX opcional (D381): estado YMM guardado con XSAVE y la ruta AVX2 del motor `llm`.

  A. CPU con AVX2 (-cpu max: en TCG la emula QEMU; con SAGUI_KVM=1 es la del anfitrión):
     el kernel registra «[CPU] AVX: encendido» y AVX2 en su línea de recursos; /testes/ymm
     (tres procesos y dos hilos con ymm0-7 cargados a la vez) no ve mitades altas de otros ni
     registros pisados; lscpu enseña avx2 e infopc «AVX activo»; `llm` elige la ruta AVX2 y
     sus tokens son los del motor del PC; con --ruta avx2/ssse3/sse2 salen los mismos.
  B. La misma CPU con la imagen --sem-avx (lo que pone «kconfig avx=no»): «AVX: apagado»,
     /testes/ymm dice «sin AVX» sin ejecutar nada VEX y no hay excepciones.
  C. CPU sin AVX (qemu32, la de serie): «la CPU no tiene AVX» y /testes/ymm «sin AVX».
  D. SMP (--smp, 2 CPU, -cpu max): los secundarios encienden AVX y /testes/ymm sigue bien
     con los procesos pasando de una CPU a otra.
  E. Solo con SAGUI_KVM=1: tok/s de las rutas SSSE3 y AVX2 (y SSE2; la mejor de tres) con el modelo real si está
     (SAGUI_LLM_MODELO o input_share/modelos/LFM3-1.7B-Q4_K_M.gguf del árbol); si no, con el
     sintético (la cifra entonces no dice mucho). Solo informa: la medida no falla la prueba.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu, mtools,  # noqa: E402
                  RAIZ, BUILD, SAIDA)

KVM = os.environ.get("SAGUI_KVM") == "1"
CPU_AVX2 = ["-cpu", "max"]
PROMPT = "hola sagui que tal"
OPC = "--contexto 512 --temp 0.8 --semilla 7 -n 40 --mostrar-ids"


def gerados(texto):
    g = re.findall(r"GERADOS:([ 0-9]*)", texto)
    return g[-1].split() if g else None


def modelo_sintetico():
    subprocess.run(["make", "-s", "llm-pc"], cwd=RAIZ, check=True, stdout=subprocess.DEVNULL)
    os.makedirs(SAIDA, exist_ok=True)
    sint = os.path.join(SAIDA, "h399-sint.gguf")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "llm_modelo_prueba.py"), sint],
                   check=True, stdout=subprocess.DEVNULL)
    r = subprocess.run([os.path.join(BUILD, "host", "llm-pc"), "-m", sint, "-p", PROMPT, "--ruta", "sse2"] + OPC.split(),
                       capture_output=True, text=True, errors="replace", check=True)
    esperado = gerados(r.stdout)
    verificar(esperado and len(esperado) >= 10, "el motor del PC genera tokens")
    return sint, esperado


def modelo_real():
    for c in (os.environ.get("SAGUI_LLM_MODELO"),
              os.path.join(RAIZ, "input_share", "modelos", "LFM3-1.7B-Q4_K_M.gguf"),
              os.path.join(os.path.dirname(RAIZ), "sagui-os-main", "input_share", "modelos", "LFM3-1.7B-Q4_K_M.gguf")):
        if c and os.path.isfile(c):
            return c
    return None


def sem_excecao(s, rot):
    verificar("EXCECAO" not in s and "#UD" not in s and "#GP" not in s, f"[{rot}] ninguna excepción")


def parte_a(sint, esperado):
    with open(sint, "rb") as f:
        pen = criar_pendrive("h399-pen", mb=64, rotulo="LLM", arquivos={"sint.gguf": f.read()})
    extras = CPU_AVX2 + ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h399a", "ide", memoria="2G", extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(1)
        s = m.serial()
        verificar("[CPU] AVX: encendido" in s, "[A] el kernel enciende AVX (XSAVE, x87|SSE|AVX)")
        verificar(re.search(r"\[CPU\] .*: .*\bAVX AVX2\b", s), "[A] el registro del kernel nombra AVX y AVX2")
        out = m.comando("/testes/ymm 3 3", 240)
        print("    " + "\n    ".join(l for l in out.splitlines() if l.startswith("ymm:")))
        verificar("ymm: estado limpio bien" in out, "[A] un proceso nuevo empieza con las mitades altas a cero")
        verificar("ymm: ok" in out and "FALLO" not in out, "[A] tres procesos y dos hilos con YMM no se pisan")
        cortes = sum(int(c) for c in re.findall(r"vueltas, (\d+) con otro por medio", out))
        verificar(cortes > 0, f"[A] y de verdad se cruzaron ({cortes} vueltas con otro por medio)")
        out = m.comando("lscpu", 60)
        verificar(re.search(r"Flags:.*\bavx2\b", out), "[A] lscpu enseña avx2")
        out = m.comando("infopc -t cpu", 120)
        verificar("AVX activo" in out, "[A] infopc dice que AVX está activo")
        out = m.comando("montar usb0")
        verificar("/usb" in out, "[A] el pendrive está montado")
        out = m.comando("llm -m /usb/sint.gguf -p '%s' %s" % (PROMPT, OPC), 240)
        rota = re.findall(r"ruta (\S+)", out)
        verificar(rota and rota[-1] == "AVX2", "[A] llm elige la ruta AVX2 (%s)" % (rota[-1] if rota else "?"))
        verificar(gerados(out) == esperado, "[A] ruta AVX2: los tokens son los del PC")
        for r in ("ssse3", "sse2"):
            out = m.comando("llm -m /usb/sint.gguf -p '%s' %s --ruta %s" % (PROMPT, OPC, r), 240)
            verificar(gerados(out) == esperado, f"[A] ruta {r}: los mismos tokens")
        out = m.comando("echo fin")
        verificar("fin" in out, "[A] el sistema sigue respondiendo")
        sem_excecao(m.serial(), "A")
        m.comando("umount /usb")


def parte_b():
    with Maquina("h399b", "ide", imagem=imagem_de_teste("/testes/ymm 2 2", ["--sem-avx"]), extras=CPU_AVX2) as m:
        m.esperar_serial("ymm: sin AVX", 240)
        s = m.serial()
        verificar("[CPU] AVX: apagado" in s, "[B] con --sem-avx el kernel no enciende AVX aunque la CPU lo tenga")
        verificar("ymm: sin AVX" in s, "[B] /testes/ymm ve que no hay AVX utilizable")
        sem_excecao(s, "B")


def parte_c():
    with Maquina("h399c", "ide", imagem=imagem_de_teste("/testes/ymm 2 2"), extras=["-cpu", "qemu32"]) as m:
        m.esperar_serial("ymm: sin AVX", 240)
        s = m.serial()
        verificar("[CPU] AVX: la CPU no tiene AVX" in s, "[C] qemu32: «la CPU no tiene AVX»")
        sem_excecao(s, "C")


def parte_d():
    with Maquina("h399d", "ide", imagem=imagem_de_teste("/testes/ymm 3 4", ["--smp"]), extras=CPU_AVX2, nucleos=2) as m:
        m.esperar_serial("ymm: ok", 300)
        time.sleep(1)
        s = m.serial()
        print("    " + "\n    ".join(l.strip() for l in s.splitlines() if l.startswith("ymm:")))
        verificar("CPU 1 arrancou" in s, "[D] con --smp arranca el secundario (AVX encendido también en él)")
        verificar("sin AVX/XSAVE en esta CPU" not in s, "[D] ningún secundario se quedó parado por AVX")
        verificar("ymm: ok" in s and "FALLO" not in s, "[D] con dos CPU los YMM tampoco se pisan")
        sem_excecao(s, "D")


def tok_s(out):
    l = re.findall(r"lectura: \d+ tokens en \d+ ms \((\d+),(\d+) tok/s\); generación: \d+ tokens en \d+ ms \((\d+),(\d+) tok/s\)", out)
    if not l:
        return None
    a, b, c, d = l[-1]
    return float(f"{a}.{b}"), float(f"{c}.{d}")


def parte_e(sint):
    real = modelo_real()
    nome = "real.gguf" if real else "sint.gguf"
    pen = criar_pendrive("h399-pen-e", mb=(1400 if real else 64), rotulo="LLM", arquivos={} if real else None)
    if real:
        _, rc = mtools(pen, "mcopy", "-o", real, "p:/real.gguf")
        verificar(rc == 0, "[E] el modelo real va al pendrive")
    else:
        mtools(pen, "mcopy", "-o", sint, "p:/sint.gguf")
    extras = CPU_AVX2 + ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    ids = ",".join(str(i) for i in range(100, 141))
    with Maquina("h399e", "ide", memoria="3G", extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(1)
        m.comando("montar usb0")
        # Con KVM dentro de WSL2 (virtualización anidada) la primera vez que el invitado toca su RAM
        # cuesta mucho, y cada `llm` carga el modelo en páginas que pueden ser nuevas: una vuelta de
        # calentamiento, luego las rutas alternadas tres veces y la mejor de cada una.
        cmd = f"llm -m /usb/{nome} --contexto 512 --hilos 1 --tokens {ids} -n 8 --ruta %s"
        m.comando(cmd % "ssse3", 900)
        medidas = {"sse2": None, "ssse3": None, "avx2": None}
        for _ in range(3):
            for r in medidas:
                t = tok_s(m.comando(cmd % r, 900))
                print(f"    [E] {nome} ruta {r}: lectura/generación (tok/s) = {t}")
                if t and (not medidas[r] or t[0] > medidas[r][0]):
                    medidas[r] = t
        for r in medidas:
            print(f"    [E] la mejor de {r}: {medidas[r]}")
        if medidas.get("avx2") and medidas.get("ssse3"):
            la, ga = medidas["avx2"]
            ls, gs = medidas["ssse3"]
            print(f"    [E] AVX2 frente a SSSE3: lectura x{la / ls:.2f}, generación x{ga / gs:.2f}")
        verificar(all(medidas.values()), "[E] las tres rutas dan su medida")
        m.comando("umount /usb")


def main():
    sint, esperado = modelo_sintetico()
    parte_a(sint, esperado)
    parte_b()
    parte_c()
    parte_d()
    if KVM:
        parte_e(sint)
    print("H399: todo correcto")


if __name__ == "__main__":
    main()
