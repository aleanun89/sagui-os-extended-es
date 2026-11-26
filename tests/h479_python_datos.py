#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H479 - Python para datos: ulab, el módulo «tablas» y @micropython.native/@viper en x86 (D479).

Criterio:
  1. /testes/datos_ulab.py (dentro de SaguiOS): estadística (media, mediana, desviación), álgebra lineal (det,
     inversa, sistema, autovalores), FFT de una señal conocida (pico en 5, amplitud N/2, ida y vuelta),
     polinomios e interpolación; todos con el resultado analítico. @micropython.native y @micropython.viper
     dan el mismo resultado que el Python puro y son más rápidos (medida de un bucle de 30 millones de pasos);
  2. /testes/datos_tablas.py: el módulo tablas con el CSV sintético /ejemplos/datos/ventas.csv: lectura con
     tipos y valores que faltan, filtrar, ordenar, groupby, merge (inner/left/right/outer), pivot, rellenar,
     CSV/JSON/.xlsx/.mxs (con Macaco)/SQLite de ida y vuelta, Valkey opcional (el aviso si no está) y gráficos
     exportados a PNG y BMP. Las cifras que imprime se contrastan aquí con el cálculo del anfitrión sobre el mismo CSV;
  3. los ejemplos de /ejemplos/datos/ (estadística, ventas, rendimiento) producen la salida esperada;
  4. el gráfico en ventana (grafico.py): se dibuja con los colores de la paleta; captura docs/capturas/H479-grafico.png.
SAGUI_KVM=1 usa KVM. Las medidas salen en el informe: «MEDIDA».
"""
import csv
import math
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, RAIZ, SAIDA, CAPTURAS  # noqa: E402
from h303_vps_manual import janela, cerrar, _escribir_png  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
TEMPO = 300 if KVM else 1200
AZUL = (0x2A, 0x6F, 0xD8)


def cifras(texto):
    return {m.group(1): float(m.group(2)) for m in re.finditer(r"^CIFRA (\S+) (\S+)$", texto, re.M)}


def calculo_anfitrion():
    """Lo mismo que imprime el script, calculado con Python del PC sobre el mismo CSV."""
    filas = list(csv.DictReader(open(os.path.join(RAIZ, "rootfs", "ejemplos", "datos", "ventas.csv"))))
    un = [int(f["unidades"]) if f["unidades"] else None for f in filas]
    pr = [float(f["precio"]) if f["precio"] else None for f in filas]
    u = [x for x in un if x is not None]
    media = sum(u) / len(u)
    std = math.sqrt(sum((x - media) ** 2 for x in u) / (len(u) - 1))
    par = [(a, b) for a, b in zip(un, pr) if a is not None and b is not None]
    mx = sum(a for a, _ in par) / len(par)
    my = sum(b for _, b in par) / len(par)
    cor = sum((a - mx) * (b - my) for a, b in par) / math.sqrt(sum((a - mx) ** 2 for a, _ in par) * sum((b - my) ** 2 for _, b in par))
    r = {"suma_unidades": sum(u), "media_unidades": media, "std_unidades": std,
         "media_precio": sum(p for p in pr if p is not None) / len([p for p in pr if p is not None]),
         "correlacion": cor, "filtro_mayor30": len([x for x in u if x > 30]), "pivot_total": sum(u)}
    for reg in ("Norte", "Sur", "Este", "Oeste"):
        r["unidades_" + reg] = sum(int(f["unidades"]) for f in filas if f["region"] == reg and f["unidades"])
    return r


def cuenta_color(ppm, x, y, w, h, color):
    l, a, px = ler_ppm(ppm)
    px = bytes(px)
    n = 0
    c = bytes(color)
    for fila in range(y, min(a, y + h)):
        base = (fila * l + x) * 3
        trozo = px[base:base + w * 3]
        n += sum(1 for i in range(0, len(trozo) - 2, 3) if trozo[i:i + 3] == c)
    return n


def principal():
    esperado = calculo_anfitrion()
    with Maquina("h479", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ------------------------------------------------------------ 1. ulab y native/viper
        out = m.comando("python /testes/datos_ulab.py", tempo=TEMPO)
        oks = len(re.findall(r"^DATOS ok ", out, re.M))
        verificar("RESULTADO datos_ulab: 0 fallos" in out and "DATOS FALLO" not in out, "ulab: todas las comprobaciones (%d ok)" % oks)
        verificar(oks >= 30, "ulab: al menos 30 comprobaciones (%d)" % oks)
        for clave in ("fft-pico-en-5", "fft-amplitud", "fft-ida-y-vuelta", "det", "inversa", "sistema-lineal", "autovalores",
                      "desviacion", "native-resultado", "viper-resultado", "viper-ptr8", "native-mas-rapido", "viper-mas-rapido"):
            verificar("DATOS ok " + clave in out, "ulab: " + clave)
        med = {k: int(v) for k, v in re.findall(r"^MEDIDA (\w+) (\d+)$", out, re.M)}
        print("  MEDIDA bucle de 30 millones de pasos: Python %(python)d ms, native %(native)d ms, viper %(viper)d ms" % med)
        verificar(med["native"] * 1.5 < med["python"], "native es al menos 1,5 veces más rápido que Python (%s)" % med)
        verificar(med["viper"] * 3 < med["python"], "viper es al menos 3 veces más rápido que Python (%s)" % med)

        # ------------------------------------------------------------ 2. tablas
        out = m.comando("python /testes/datos_tablas.py", tempo=TEMPO)
        oks = len(re.findall(r"^TABLAS ok ", out, re.M))
        verificar("RESULTADO datos_tablas: 0 fallos" in out and "TABLAS FALLO" not in out, "tablas: todas las comprobaciones (%d ok)" % oks)
        verificar(oks >= 60, "tablas: al menos 60 comprobaciones (%d)" % oks)
        for clave in ("xlsx-ida-y-vuelta", "mxs-ida-y-vuelta", "sqlite-consulta", "merge-outer-trae-el-te", "pivot-total",
                      "valkey-no-disponible", "valkey-cliente-propio", "grafico-png-firma", "csv-comillas-y-acentos"):
            verificar("TABLAS ok " + clave in out, "tablas: " + clave)
        v = cifras(out)
        for k, e in esperado.items():
            verificar(k in v and abs(v[k] - e) <= 1e-5 * max(1.0, abs(e)), "cifra %s: SaguiOS %s, anfitrión %s" % (k, v.get(k), e))

        # ------------------------------------------------------------ 3. los ejemplos
        out = m.comando("python /ejemplos/datos/estadistica.py", tempo=TEMPO)
        verificar("media 5.0 mediana 4.5 desviación 2.0" in out or "media 5.0 mediana 4.5 desviaci" in out, "estadistica.py: media, mediana y desviación")
        verificar(re.search(r"x = 1\.0\d* +y = 3\.0", out) is not None or "x = 1.0  y = 3.0" in out, "estadistica.py: el sistema da x=1, y=3")
        verificar(re.search(r"frecuencia 5 amplitud 0\.99999", out) and re.search(r"frecuencia 12 amplitud 0\.5", out), "estadistica.py: la FFT halla 5 y 12")
        out = m.comando("python /ejemplos/datos/ventas.py", tempo=TEMPO)
        verificar("filas y columnas: (36, 5)" in out and "Total por región" in out.replace("regi\xf3n", "región") or "Total por reg" in out, "ventas.py: forma y totales")
        verificar("Panaderia" in out and "Despensa" in out and "Guardado en /tmp" in out, "ventas.py: merge, groupby y guardado")
        out = m.comando("python /ejemplos/datos/rendimiento.py", tempo=TEMPO)
        t = {n: int(ms) for n, ms in re.findall(r"^(Python|native|viper)\s+resultado \d+ en (\d+) ms", out, re.M)}
        verificar(len(t) == 3 and t["viper"] < t["native"] < t["Python"], "rendimiento.py: Python > native > viper (%s)" % t)

        # ------------------------------------------------------------ 4. el gráfico en ventana
        marca = len(m.serial())
        m.digitar("python /ejemplos/datos/grafico.py\n", 0.03)
        j = janela(m, "Tablas", marca)
        time.sleep(2.5)
        ppm = os.path.join(SAIDA, "h479-grafico.ppm")
        if os.path.exists(ppm):
            os.remove(ppm)
        m.monitor("screendump " + ppm)
        for _ in range(50):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.1)
        time.sleep(0.3)
        azul = cuenta_color(ppm, j["x"], j["y"], j["l"], j["a"], AZUL)
        verificar(azul > 1500, "el gráfico dibuja las barras de la serie en azul (%d píxeles)" % azul)
        l, a, px = ler_ppm(ppm)
        px = bytes(px)
        x0, y0 = max(0, j["fx"]), max(0, j["fy"])
        w, h = min(l - x0, j["l"] + 6), min(a - y0, j["a"] + 30)
        rgb = b"".join(px[((y0 + k) * l + x0) * 3:((y0 + k) * l + x0 + w) * 3] for k in range(h))
        _escribir_png(os.path.join(CAPTURAS, "H479-grafico.png"), w, h, rgb)
        os.remove(ppm)
        cerrar(m, j)
        time.sleep(1)
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


if __name__ == "__main__":
    principal()
    print("h479: todo bien")
