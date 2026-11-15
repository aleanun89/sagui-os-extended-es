#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H242 - infopc: pestaña «GPU» y benchmark 2D (D243, v8.0b).

Criterio:
  * QEMU std: `infopc -t gpu` muestra la GPU 1234:1111, la memoria de vídeo, el modo, «por software»
    y «3D por GPU: no disponible: ninguna tarjeta con driver 3D» (D285); `infopc --bench-gpu` termina con las seis pruebas
    con suma correcta, una puntuación 2D mayor que cero, y guarda saguimark-gpu-AAAAMMDD-HHMM.txt;
    `infopc --informe` lleva la sección GPU; el resumen de SaguiMark no confunde ese archivo con el suyo.
  * La ventana: la tecla 9 abre la pestaña GPU y la tecla M mide (capturas).
  * Con la VMware SVGA II (acelerador 2D): salen los dos caminos, la GPU y la CPU, con las sumas
    iguales (o, si QEMU/KVM lo deja a la CPU, se dice por qué).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402
from h161_acel2d import arrancar, VMWARE  # noqa: E402


def comprobar_bench(out, rotulo, camino_gpu):
    out = out.replace("\r", "")
    filas = re.findall(r"^(?:Rellenos de rect|Copias de bloques|Copias solapadas|Líneas|Texto|Ventana).*$", out, re.M)
    verificar(len(filas) == 6, f"{rotulo}: las seis pruebas ({len(filas)})")
    verificar(all(re.search(r"\sOK\s+\d+$", f) for f in filas), f"{rotulo}: todas con suma correcta (OK)")
    t = re.search(r"Puntuación 2D total: (\d+)$", out, re.M)
    verificar(t and int(t.group(1)) > 0, f"{rotulo}: puntuación total {t.group(1) if t else '?'}")
    verificar(re.search(r"^Camino GPU: " + camino_gpu, out, re.M), f"{rotulo}: camino GPU «{camino_gpu}»")
    return int(t.group(1))


def main():
    with Maquina("h242", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("infopc -t gpu", 60)
        verificar(re.search(r"^GPU:\s+1234:1111", out, re.M), "std: GPU 1234:1111")
        verificar(re.search(r"^  Subsistema:", out, re.M), "std: subsistema y revisión")
        verificar(re.search(r"^Memoria de vídeo:", out, re.M), "std: memoria de vídeo")
        verificar(re.search(r"^Modo de vídeo:\s+1024x768, 32 bits", out, re.M), "std: modo 1024x768")
        verificar(re.search(r"^Acelerador 2D:\s+por software \(CPU\)", out, re.M), "std: acelerador 2D por software (CPU)")
        verificar(re.search(r"3D por GPU:\s+no disponible", out) and "ninguna tarjeta con driver 3D" in out,
                  "std: 3D por GPU no disponible (sin tarjeta con driver 3D, D285)")
        verificar("SaguiGL por software" in out, "std: SaguiGL por software")
        out = m.comando("infopc --bench-gpu", 600)
        comprobar_bench(out, "std", "no hay acelerador 2D")
        g = re.findall(r"gpu bench total \d+ erros \d+ acel -?\d+ guardado (/\w+/saguimark-gpu-\d{8}-\d{4}\.txt)", m.serial())
        g = re.search(r"(.+)", g[-1]) if g else None
        verificar(g, "std: resultado guardado en saguimark-gpu-AAAAMMDD-HHMM.txt")
        out = m.comando("cat " + g.group(1), 20)
        verificar("Puntuación 2D total:" in out, "std: el archivo guardado tiene la puntuación")
        out = m.comando("infopc --informe /tmp/h242-informe.txt", 120)
        out = m.comando("cat /tmp/h242-informe.txt", 30)
        verificar("== GPU ==" in out and "Puntuación 2D total" in out, "el informe incluye la sección GPU con el último benchmark")
        verificar(not re.search(r"^Archivo: \S*saguimark-gpu-", out, re.M), "el informe no toma el archivo GPU por el de SaguiMark")
        out = m.comando("infopc -t gpu", 60)
        verificar("Último resultado guardado" in out, "-t gpu enseña el último resultado guardado")

        # La ventana.
        m.comando("infopc")
        m.esperar_serial("infopc: janela aberta", 30)
        m.digitar("9", 0.08)
        m.esperar_serial("infopc: secao gpu", 15)
        time.sleep(0.5)
        m.captura("H242-infopc-gpu")
        m.digitar("m", 0.08)
        m.esperar_serial("infopc: gpu bench inicio", 15)
        m.esperar_serial("infopc: gpu bench total", 600)
        t = re.findall(r"infopc: gpu bench total (\d+) erros (\d+) acel (-?\d+) guardado (\S+)", m.serial())[-1]
        verificar(int(t[0]) > 0 and t[1] == "0" and t[2] == "0", f"ventana: total {t[0]}, errores {t[1]}, sin acelerador")
        m.esperar_serial("infopc: secao gpu", 15)
        time.sleep(1)
        m.captura("H242-infopc-gpu-resultado")
        m.tecla("esc")
        m.esperar_serial("(infopc) terminou com codigo 0", 15)
        verificar("EXCECAO" not in m.serial(), "std: ningún error del kernel")

    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h242v", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        out = m.comando("infopc -t gpu", 60)
        verificar(re.search(r"^GPU:\s+15ad:0405", out, re.M), "vmware: GPU 15ad:0405")
        out = m.comando("infopc --bench-gpu", 900)
        comprobar_bench(out, "vmware", r"(vmware|el acelerador FALL|no hay acelerador)")
        if re.search(r"^Camino GPU: vmware", out, re.M):
            verificar(re.search(r"^(Rellenos|Copias de bloques).*x\d+,\d\d\s+OK", out, re.M), "vmware: las dos columnas y la ganancia (×)")
        else:
            print("  (aviso) con este QEMU/KVM el acelerador VMware no quedó en uso:", re.search(r"^Camino GPU:.*$", out, re.M).group(0))
        verificar("EXCECAO" not in m.serial(), "vmware: ningún error del kernel")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
