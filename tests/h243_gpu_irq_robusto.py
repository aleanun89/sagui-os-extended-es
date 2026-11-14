#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H243 - GPU e IRQ robustas (D244, v8.0b).

  vigilante  con --acel2d-prueba=tiempo la autoprueba no termina a tiempo: el acelerador queda
             apagado (línea «[ACEL2D] CPU: ... tiempo agotado»), el escritorio sigue y «acel2d» dice
             «por autoprueba» y el motivo; «acel2d encender» repite la autoprueba (y vuelve a fallar).
  manual     VMware normal: «acel2d» dice «confirmada»; «acel2d apagar» deja la CPU con la línea
             «[ACEL2D] apagado: a mano»; «acel2d encender» vuelve a activarla.
  tormenta   --irq-prueba tormenta: una tormenta simulada en la IRQ 11 se enmascara, se registra con
             el texto «[IRQ] tormenta en la IRQ 11 (...): enmascarada; los drivers de esa linea pasan a
             sondeo», se reintenta («desenmascarada») y el sistema sigue respondiendo; infopc -t pci
             enseña las interrupciones por segundo.
Uso: h243_gpu_irq_robusto.py [vigilante] [manual] [tormenta]
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402
from h161_acel2d import arrancar, VMWARE  # noqa: E402


def vigilante():
    img = imagem_de_teste("", ["--vesa", "--acel2d-prueba=tiempo"])
    with Maquina("h243v", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("tiempo agotado" in s and "[ACEL2D] activo" not in s, "autoprueba con tiempo agotado: CPU y log")
        out = m.comando("acel2d", 30).replace("\r", "")
        verificar(re.search(r"Aceleracion 2D: CPU", out) and "tiempo agotado" in out, "«acel2d» dice CPU y el motivo")
        verificar("por autoprueba" in out or "confirmada" in out, "«acel2d» dice si la tarjeta está confirmada")
        out = m.comando("acel2d encender", 60).replace("\r", "")
        verificar("la CPU hace todo" in out, "«acel2d encender» repite la autoprueba y vuelve a fallar")
        out = m.comando("echo vivo243", 20)
        verificar("vivo243" in out, "el sistema sigue respondiendo")


def manual():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h243m", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        out = m.comando("acel2d", 30).replace("\r", "")
        if "Aceleracion 2D: CPU" in out:   # KVM: la VMware emulada pierde contra la CPU
            print("  aviso: la VMware quedó en la CPU (velocidad); se salta la parte manual")
            return
        verificar("tarjeta: confirmada" in out, "la VMware SVGA II figura como confirmada")
        verificar(re.search(r"plazo de la GPU en sesion: \d+ ms", out), "el plazo de la GPU en sesión es de unos ms")
        out = m.comando("acel2d apagar", 30).replace("\r", "")
        time.sleep(1)
        verificar("[ACEL2D] apagado: a mano" in m.serial(), "apagar: línea [ACEL2D] apagado en el log")
        verificar("la CPU hace todo" in out, "apagar: la CPU hace todo")
        out = m.comando("acel2d", 30).replace("\r", "")
        verificar("apagado a mano" in out, "«acel2d» enseña que está apagado a mano")
        out = m.comando("acel2d encender", 60).replace("\r", "")
        verificar("acelerador activo" in out, "encender: vuelve el acelerador")
        out = m.comando("echo vivo243", 20)
        verificar("vivo243" in out, "el sistema sigue respondiendo")


def tormenta():
    img = imagem_de_teste("", ["--irq-prueba", "tormenta"])
    with Maquina("h243t", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        m.esperar_serial("[IRQ] tormenta en la IRQ 11", 60)
        s = m.serial()
        verificar(re.search(r"\[IRQ\] tormenta en la IRQ 11 \(\d+/s\).*: enmascarada; los drivers de esa linea pasan a sondeo", s),
                  "la tormenta se registra y la línea se enmascara")
        out = m.comando("echo vivo243", 20)
        verificar("vivo243" in out, "el sistema responde con la línea enmascarada")
        m.esperar_serial("[IRQ] IRQ 11 desenmascarada", 30)
        out = m.comando("infopc -t pci", 60)
        verificar(re.search(r"\d+ atendidas, \d+/s", out), "infopc muestra las interrupciones por segundo")
        verificar("tormenta de interrupciones" in out or "IRQ 11 " not in out, "infopc anota la tormenta (si la IRQ 11 tiene filas)")
        out = m.comando("echo vivo243b", 20)
        verificar("vivo243b" in out, "el sistema sigue después del reintento")


def main():
    pasos = sys.argv[1:] or ["vigilante", "manual", "tormenta"]
    for p in pasos:
        print("== " + p)
        {"vigilante": vigilante, "manual": manual, "tormenta": tormenta}[p]()
    print("H243 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
