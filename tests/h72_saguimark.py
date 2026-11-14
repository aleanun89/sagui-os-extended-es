#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H72 (D093, mejora 051) - SaguiMark: benchmark com pontuação.

Critério:
  - `saguimark --rapido` termina com todas as provas verificadas («ok»),
    código 0, total e grupos com pontos, a comparação com as referências de
    /lib/saguimark/referencias.txt e o resultado gravado em /tmp (sem /home);
  - com KVM (SAGUI_KVM=1), duas execuções do modo preciso dão totais que não
    diferem mais de 15 %;
  - com `-cpu pentium3` (sem SSE2) o grupo SIMD sai «n/d», o resto funciona e
    o total sai marcado como parcial;
  - a aba Benchmark do infopc roda o modo rápido (tecla R), mostra a barra de
    progresso e o resultado (capturas H72-*.png);
  - grupo Gráficos 2D (D102, versão 2): no terminal de tela cheia (sem
    escritório) as 4 provas saem «n/d» e o total, parcial; no escritório a
    janela de 128x128 aparece num canto (embaixo à esquerda, fora do infopc) (captura H72-saguimark-2d), as 4
    provas terminam verificadas e os valores esperados batem com a
    implementação de referência em Python (tests/saguimark_ref2d.py).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, cor_do_pixel, tamanho_png  # noqa: E402
import saguimark_ref2d  # noqa: E402

PROVAS = 21        # na tabela; no terminal sem escritório as 4 do grupo 2D saem n/d
PROVAS_2D = 4


def linhas_prova(out):
    return re.findall(r"^(Enteros|FPU x87|SIMD \(SSE2\)|Memoria|Gráficos 2D|Sistema)\s{2,}(.+?)\s{2,}.*?\s(ok \([0-9a-f]+\)|ERROR.*|n/d.*)$",
                      out, re.M)


def total(out):
    t = re.search(r"^TOTAL:\s+(\d+)", out, re.M)
    return int(t.group(1)) if t else None


def main():
    kvm = os.environ.get("SAGUI_KVM") == "1"
    console = imagem_console()
    with Maquina("h72", "ide", imagem=console) as m:
        m.esperar_serial("sagui:/$", 120)
        ini = time.time()
        out = m.comando("saguimark --rapido", 600)
        print(f"  (saguimark --rapido: {time.time() - ini:.0f} s{' con KVM' if kvm else ', TCG'})")
        p = linhas_prova(out)
        verificar(len(p) == PROVAS, f"{PROVAS} pruebas en la tabla ({len(p)})")
        mal = [f"{g}/{n}: {v}" for g, n, v in p if not v.startswith("ok") and g != "Gráficos 2D"]
        verificar(not mal, f"todas las pruebas verificadas {mal}")
        d2 = [v for g, n, v in p if g == "Gráficos 2D"]
        verificar(len(d2) == PROVAS_2D and all("n/d: sin escritorio" in v for v in d2),
                  "terminal sin escritorio: las 4 pruebas 2D salen «n/d»")
        verificar(re.search(r"^  Gráficos 2D\s+n/d$", out, re.M), "terminal sin escritorio: grupo 2D n/d")
        verificar("terminou com codigo 0" in out, "saguimark termina con código 0")
        t = total(out)
        verificar(t and t > 0 and re.search(r"^TOTAL:\s+\d+  \(parcial", out, re.M), f"total {t}, parcial sin 2D")
        for g in ("Enteros", "FPU x87", "SIMD \\(SSE2\\)", "Memoria"):
            verificar(re.search(rf"^  {g}\s+\d+$", out, re.M), f"grupo {g.replace(chr(92), '')} con puntos")
        verificar("SaguiMark 2, modo rápido (3 repeticiones, mediana)" in out and "versiones distintas del benchmark NO se comparan" in out,
                  "versión 2 del benchmark y aviso de que no se comparan versiones")
        verificar(re.search(r"QEMU/KVM, PC de desarrollo \(referencia p\s+1000  #+", out),
                  "comparación con la referencia provisional (barras)")
        if not kvm:
            verificar("QEMU sin KVM (TCG" in out, "aviso: TCG no es representativo")
        g = re.search(r"Resultado guardado en (/tmp/saguimark-\d{8}-\d{4}\.txt)", out)
        verificar(g, "resultado guardado en /tmp/saguimark-AAAAMMDD-HHMM.txt (no hay /home)")
        arq = m.comando(f"cat {g.group(1)}", 30)
        verificar("TOTAL:" in arq and re.search(r"^2;QEMU Virtual CPU[^;]*;\d+;\d+;\d+;\d+;\d+;\d+$", arq, re.M),
                  "el archivo tiene el resultado y la línea para referencias.txt")
        m.captura("H72-saguimark-texto")
        out = m.comando("infopc --informe /tmp/inf.txt", 120)
        out = m.comando("cat /tmp/inf.txt", 60)
        verificar("== SaguiMark (último resultado) ==" in out and "Archivo: /tmp/saguimark-" in out,
                  "infopc --informe incluye el último resultado")

        if kvm:
            out1 = m.comando("saguimark --preciso --no-guardar", 300)
            out2 = m.comando("saguimark --preciso --no-guardar", 300)
            t1, t2 = total(out1), total(out2)
            verificar(t1 and t2, f"KVM: dos ejecuciones con total ({t1}, {t2})")
            dif = abs(t1 - t2) * 100 / max(t1, t2)
            verificar(dif <= 15, f"KVM: totales estables ({t1} y {t2}, {dif:.1f} %)")
        else:
            print("  (sin KVM: la estabilidad entre ejecuciones se comprueba con SAGUI_KVM=1)")

    with Maquina("h72p3", "ide", imagem=console, extras=["-cpu", "pentium3"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("saguimark --rapido --no-guardar", 600)
        p = linhas_prova(out)
        simd = [v for g, n, v in p if g.startswith("SIMD")]
        verificar(len(simd) == 3 and all("n/d: la CPU no tiene SSE2" in v for v in simd),
                  "-cpu pentium3: las pruebas SIMD salen «n/d»")
        resto = [v for g, n, v in p if not g.startswith("SIMD") and g != "Gráficos 2D"]
        verificar(len(resto) == PROVAS - 3 - PROVAS_2D and all(v.startswith("ok") for v in resto),
                  "-cpu pentium3: el resto de pruebas, verificadas")
        verificar(re.search(r"^  SIMD \(SSE2\)\s+n/d$", out, re.M), "-cpu pentium3: grupo SIMD n/d")
        verificar(re.search(r"^TOTAL:\s+\d+  \(parcial", out, re.M), "-cpu pentium3: total parcial")
        verificar("terminou com codigo 0" in out, "-cpu pentium3: código 0")

    with Maquina("h72d", "ide") as m:
        # Grupo Gráficos 2D no escritório: janela 128x128 num canto (embaixo à esquerda, fora do infopc) e provas verificadas.
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        antes = len(m.serial())
        m.digitar("saguimark --solo 2d --rapido --no-guardar", 0.04)
        m.tecla("ret", 0.04)
        m.esperar_serial("saguimark: ventana 2D abierta", 60)
        png = m.captura("H72-saguimark-2d")
        larg, alt = tamanho_png(png)
        # a moldura (132 x 154) fica encostada no canto de baixo à esquerda; a barra de título é verde
        r, g, b = cor_do_pixel(png, 60, alt - 154 + 4)
        verificar((r, g, b) == (11, 122, 62), f"la ventana 2D aparece en la esquina inferior izquierda (barra {r},{g},{b})")
        m.esperar_serial("(saguimark) terminou com codigo", 120)   # no el de configuracion --aplicar del arranque (D112)
        out = m.serial()[antes:].replace("\r", "")
        verificar(re.search(r"janela \d+ criada por \d+: \"SaguiMark 2D\" 128x128", out), "el kernel registra la ventana de 128x128")
        p = linhas_prova(out)
        verificar(len(p) == PROVAS_2D and all(g == "Gráficos 2D" and v.startswith("ok") for g, n, v in p),
                  f"las 4 pruebas 2D verificadas ({[v for g, n, v in p]})")
        esp = saguimark_ref2d.esperados()
        quer = [esp[0], esp[1], esp[2][0], esp[3][0]]
        obt = [int(x, 16) for x in re.findall(r"\bok \(([0-9a-f]+)\)", out)]
        verificar(obt == quer, f"sumas del búfer iguales a las de la implementación en Python ({[hex(x) for x in obt]})")
        verificar(re.search(r"líneas/s, \d+ cuadros/s", out) and re.search(r"copias/s, \d+ píxeles/s", out),
                  "se muestran líneas/s, cuadros/s y píxeles/s")
        verificar("EXCECAO" not in m.serial(), "ningún error del kernel")
        out = m.comando("saguimark --sin-2d --solo memoria --no-guardar", 120)
        verificar("ventana 2D" not in out, "--sin-2d: no se abre la ventana 2D")

    with Maquina("h72g", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("infopc")
        m.esperar_serial("infopc: janela aberta", 30)
        m.digitar("7", 0.08)
        m.esperar_serial("infopc: secao benchmark", 15)
        verificar(True, "tecla 7: pestaña Benchmark")
        time.sleep(0.5)
        m.captura("H72-infopc-benchmark")
        m.digitar("r", 0.08)
        m.esperar_serial("infopc: saguimark inicio rapido", 15)
        m.esperar_serial("saguimark: ventana 2D abierta", 60)
        m.captura("H72-infopc-2d")
        m.esperar_serial("infopc: saguimark 6/21", 300)
        m.captura("H72-infopc-progreso", guardar=False)
        m.esperar_serial("infopc: saguimark total", 600)
        g = re.findall(r"infopc: saguimark total (\d+) erro (\d) grupos (\d+) (\d+) (\d+) (\d+) (\d+) guardado (\S+)", m.serial())[-1]
        verificar(int(g[0]) > 0 and g[1] == "0" and all(int(x) > 0 for x in g[2:7]),
                  f"infopc: medida rápida con total {g[0]} y los cinco grupos (incluido Gráficos 2D)")
        verificar(g[7].startswith("/tmp/saguimark-"), f"infopc: resultado guardado en {g[7]}")
        m.esperar_serial("infopc: secao benchmark", 15)
        time.sleep(1)
        m.captura("H72-infopc-resultado")
        m.tecla("esc")
        m.esperar_serial("(infopc) terminou com codigo 0", 15)
        verificar("EXCECAO" not in m.serial(), "ningún error del kernel")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
