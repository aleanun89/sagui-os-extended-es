#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H388 - IA local (D380, mejora 139): el motor propio `llm` dentro de SaguiOS.

El modelo real (LFM Open License) no va a git ni a las imágenes: la prueba usa el GGUF
sintético de tools/llm_modelo_prueba.py (arquitectura LFM3, Q4_K/Q6_K/Q8_0/Q4_0,
tokenizador BPE) en un pendrive. Comprueba:
  1. con 2 GB: --info; los tokens generados con una semilla fija son los mismos que da el
     motor compilado para el PC (make llm-pc), por la mejor ruta de la CPU, por la SSE2 y por la de C (x87);
  2. --contexto fuera de 12k-48k se rechaza; un modelo cuya caché no cabe da el error de
     memoria en español antes de cargar nada;
  3. `llm --servidor` contesta a `ia` por 127.0.0.1;
  4. `llm --anunciar` pone el logo «IA Powered» abajo a la izquierda (captura);
  5. con 1 GB: `llm` se niega («necesita más de 1,8 GB») y el logo no sale (captura)."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, SAIDA, RAIZ, BUILD, cor_do_pixel, tamanho_png  # noqa: E402

PROMPT = "hola sagui que tal"
OPC = "--contexto 512 --temp 0.8 --semilla 7 -n 40 --mostrar-ids"


def gerados(texto):
    g = re.findall(r"GERADOS:([ 0-9]*)", texto)
    return g[-1].split() if g else None


def preparar():
    subprocess.run(["make", "-s", "llm-pc"], cwd=RAIZ, check=True, stdout=subprocess.DEVNULL)
    os.makedirs(SAIDA, exist_ok=True)
    sint, grande = os.path.join(SAIDA, "h388-sint.gguf"), os.path.join(SAIDA, "h388-grande.gguf")
    gen = os.path.join(RAIZ, "tools", "llm_modelo_prueba.py")
    subprocess.run([sys.executable, gen, sint], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, gen, grande, "--grande"], check=True, stdout=subprocess.DEVNULL)
    # Lo esperado, por ruta: SSE2 y SSSE3 dan lo mismo (mismos enteros y la misma cuenta de
    # float); la de C acumula en otro orden y, en este modelo aleatorio (caótico), se separa.
    esperado = {}
    for rota in ("sse2", "c"):
        r = subprocess.run([os.path.join(BUILD, "host", "llm-pc"), "-m", sint, "-p", PROMPT, "--ruta", rota] + OPC.split(),
                           capture_output=True, text=True, errors="replace", check=True)
        esperado[rota] = gerados(r.stdout)
        verificar(esperado[rota] and len(esperado[rota]) >= 10, "el motor del PC genera tokens (ruta %s)" % rota)
    with open(sint, "rb") as f:
        ds = f.read()
    with open(grande, "rb") as f:
        dg = f.read()
    pen = criar_pendrive("h388-pen", mb=64, rotulo="LLM", arquivos={"sint.gguf": ds, "grande.gguf": dg})
    return pen, esperado


def regiao(png, passo=4):
    """El rincón de abajo a la izquierda, donde va el logo «IA Powered» (sea cual sea la resolución)."""
    _, alt = tamanho_png(png)
    return [cor_do_pixel(png, x, y) for y in range(alt - 54, alt - 8, passo) for x in range(8, 140, passo)]


def main():
    pen, esperado = preparar()
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h388", "ide", memoria="2G", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("/usb" in out, "el pendrive está montado")
        out = m.comando("llm --info -m /usb/sint.gguf", 60)
        verificar("lfm3, 8 bloques" in out and "Q4_0" in out and "Q8_0" in out, "llm --info describe el modelo")
        out = m.comando("llm -m /usb/sint.gguf -p '%s' %s" % (PROMPT, OPC), 180)
        g = gerados(out)
        print("PC:      ", " ".join(esperado["sse2"]))
        print("SaguiOS: ", " ".join(g or []))
        rota = re.findall(r"ruta (\S+)", out)
        verificar(g == esperado["sse2"], "los tokens generados en SaguiOS son los del PC (ruta %s)" % (rota[-1] if rota else "?"))
        for r in ("sse2", "c"):
            out = m.comando("llm -m /usb/sint.gguf -p '%s' %s --ruta %s" % (PROMPT, OPC, r), 180)
            verificar(gerados(out) == esperado[r], "ruta %s: los mismos tokens que en el PC" % r)
        out = m.comando("env LLM_SIN_SSSE3=1 llm --info -m /usb/grande.gguf", 60)
        verificar("de 4k a 48k" in out and "no tiene SSSE3" in out and "Contexto:       4096" in out,
                  "sin SSSE3: contexto de 4k a 48k (4k de serie) y la recomendación")
        out = m.comando("llm -m /usb/sint.gguf --contexto 64k -p hola", 60)
        verificar("48k" in out and "Cargando" not in out, "un contexto de 64k se rechaza")
        out = m.comando("llm -m /usb/grande.gguf --contexto 48k -p hola", 60)
        verificar("el proceso solo puede tener" in out and "Cargando" not in out,
                  "el modelo cuya caché no cabe da el error de memoria antes de cargar")
        out = m.comando("llm -m /usb/sint.gguf -p hola -n 4", 120)
        verificar("lectura:" in out, "sin --contexto (12k de serie, limitado al del modelo) funciona")
        # el servidor compatible con OpenAI y el cliente `ia` de siempre, por 127.0.0.1
        ini = len(m.serial())
        m.comando("llm -m /usb/sint.gguf --contexto 512 --servidor &", 30)
        fim = time.time() + 120
        while "IA local en http://127.0.0.1:8080" not in m.serial()[ini:] and time.time() < fim:
            time.sleep(0.5)
        pid = re.findall(r"executar /apps/bin/llm -> processo (\d+)", m.serial()[ini:])
        out = m.comando("ia --url http://127.0.0.1:8080/v1 hola", 120)
        verificar("[llm] petici" in m.serial()[ini:] and "rror" not in out, "ia habla con llm --servidor por 127.0.0.1")
        if pid:
            m.comando("kill %s" % pid[-1], 30)
        antes = m.captura("H388-antes", guardar=False)
        out = m.comando("llm --anunciar -m /usb/sint.gguf", 60)
        verificar("IA local activa" in out, "llm --anunciar acepta el equipo y el modelo")
        time.sleep(1)
        depois = m.captura("H388-ia-powered")
        verificar(regiao(antes) != regiao(depois),
                  "el logo «IA Powered» aparece abajo a la izquierda")
        m.comando("umount /usb")
    with Maquina("h388b", "ide", memoria="1G", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(1)
        m.comando("montar usb0")
        out = m.comando("llm -m /usb/sint.gguf -p hola", 60)
        verificar("1,8 GB" in out and "Cargando" not in out, "con 1 GB, llm se niega con un mensaje claro")
        antes = m.captura("H388-1gb-antes", guardar=False)
        out = m.comando("llm --anunciar -m /usb/sint.gguf", 60)
        verificar("1,8 GB" in out, "con 1 GB, --anunciar no activa la IA local")
        time.sleep(1)
        depois = m.captura("H388-sin-ia-1gb")
        verificar(regiao(antes) == regiao(depois), "con 1 GB no sale el logo")
        m.comando("umount /usb")
    print("H388 OK")


if __name__ == "__main__":
    main()
