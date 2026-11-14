#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H161 (D195) - 2D por la GPU para el escritorio: copiar y rellenar rectángulos.

Con la VMware SVGA II del QEMU (-device vmware-svga, ./run_in_qemu.sh --gpu vmware):

  vmware   el acelerador se detecta por el PCI, pasa la autoprueba del
           arranque (copias solapadas y rellenos comparados con la CPU) y se
           activa; «acel2d probar» repite la autoprueba completa (bordes y
           órdenes al azar: los mismos píxeles que la CPU) y mide la GPU contra
           la CPU; arrastrar una ventana va por la GPU (captura H161-arrastre)
           y la pantalla queda igual, píxel a píxel, que arrastrando con la
           CPU (--sin-acel2d); la medida del arrastre (miles de ciclos por
           cuadro) con y sin la GPU; infopc dice qué acelerador se usa;
  consola  la terminal de pantalla completa (--console) se desplaza con la GPU
           y el texto se ve bien;
  fallos   autoprueba fallida a propósito (--acel2d-prueba=pixel y =tiempo):
           vuelve a la CPU, lo dice el log y el infopc, y el escritorio
           funciona; --acel2d=intel sin tarjeta Intel: la CPU;
  std      con la tarjeta de siempre (-vga std) nada cambia: la CPU.

Velocidad: con el acelerador elegido solo (sin --acel2d=...) el kernel compara al
arrancar la GPU con la CPU y se queda con la CPU si la GPU es más lenta. En TCG la
VMware gana; con KVM la memoria de vídeo está mapeada directamente y la VMware
emulada pierde: entonces la prueba lo comprueba y sigue con --acel2d=vmware.

El QEMU solo rehace el tamaño de la pantalla de la VMware al refrescarla, y sin
ventana (-display none) eso solo pasa al pedir una captura: mientras arranca, la
prueba pide capturas cada cuarto de segundo (como haría una ventana).

Uso: h161_acel2d.py [vmware] [consola] [fallos] [std]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, imagem_console, SAIDA, ler_ppm  # noqa: E402

VMWARE = ["-vga", "none", "-device", "vmware-svga"]
# La ventana del Terminal al arrancar (1024x768): la barra de título y adónde se lleva.
KVM = os.environ.get("SAGUI_KVM") == "1"
TITULO = (560, 94)
DESTINO = (680, 214)
PASSO = 8


def arrancar(m, tempo=180):
    """Arranca hasta el escritorio pidiendo capturas mientras tanto (el QEMU sin
    ventana no refresca la VMware por su cuenta)."""
    ppm = os.path.join(SAIDA, m.nome + "-refresco.ppm")
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()
        if "area de trabalho no ar" in s or "sagui:/$" in s:
            break
        if m.proc.poll() is not None:
            raise FalhaTeste("el QEMU terminó durante el arranque")
        m.monitor("screendump " + ppm)
        time.sleep(0.25)
    else:
        raise FalhaTeste("el escritorio no llegó")
    time.sleep(3)


def linha(serial, padrao):
    m = re.search(padrao, serial)
    return m.group(0) if m else ""


def arrastar_lento(m, x0, y0, x1, y1):
    """Arrastra en pasos pequeños: muchos cuadros para la medida."""
    m.mouse_para(x0, y0)
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    x, y = x0, y0
    while (x, y) != (x1, y1):
        dx = max(-PASSO, min(PASSO, x1 - x))
        dy = max(-PASSO, min(PASSO, y1 - y))
        m.monitor(f"mouse_move {dx} {dy}")
        x += dx
        y += dy
        time.sleep(0.06)
    time.sleep(0.3)
    m.monitor("mouse_button 0")
    time.sleep(1.0)


def medida(serial):
    """(cuadros, kciclos por cuadro, acelerador) del último arrastre."""
    r = re.findall(r"\[GUI\] arrastre: (\d+) cuadros, (\d+) kciclos por cuadro \(2D: (\w+)\)", serial)
    verificar(r, "el gestor de ventanas midió el arrastre")
    q, k, a = r[-1]
    return int(q), int(k), a


def sem_barra(m):
    """Los píxeles de la pantalla, sin la barra superior (el reloj cambia)."""
    ppm = os.path.join(SAIDA, m.nome + "-pixeles.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    l, a, px = ler_ppm(ppm)
    return px[l * 3 * 24:]


def escritorio(nome, opcoes, arrastrar=True):
    """Arranca con la VMware, arrastra el Terminal y devuelve (serial, captura, medida)."""
    img = imagem_de_teste("", ["--vesa"] + opcoes)
    with Maquina(nome, "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        cap = med = None
        if arrastrar:
            arrastar_lento(m, *TITULO, *DESTINO)
            med = medida(m.serial())
            cap = sem_barra(m)
        return m, cap, med


def com_acelerador(m):
    """Con la VMware activa: arrastre, «acel2d probar», «acel2d» e infopc."""
    s = m.serial()
    verificar("[ACEL2D] activo: VMware SVGA II" in s, "el acelerador se activa")
    arrastar_lento(m, *TITULO, *DESTINO)
    q_gpu, k_gpu, a = medida(m.serial())
    verificar(a == "vmware", f"el arrastre va por la GPU ({q_gpu} cuadros, {k_gpu} kciclos por cuadro)")
    cap_gpu = sem_barra(m)
    m.captura("H161-arrastre")
    out = m.comando("acel2d probar", 120)
    print("   ", linha(out, r"acel2d probar: vmware.*"))
    verificar("acel2d probar: BIEN" in out, "la autoprueba completa (bordes, solapes, órdenes al azar) coincide con la CPU")
    mg = re.search(r"medida: GPU (\d+) us, CPU (\d+) us", out)
    verificar(mg, "la orden mide la GPU y la CPU")
    us_gpu, us_cpu = int(mg.group(1)), int(mg.group(2))
    out = m.comando("acel2d", 30)
    mc = re.search(r"copias (\d+), rellenos (\d+)", out)
    verificar("Aceleracion 2D: vmware" in out and mc and int(mc.group(1)) > 0,
              f"«acel2d» dice que la GPU copia ({mc.group(1) if mc else '?'} copias)")
    out = m.comando("infopc -t video", 60)
    verificar("vmware (VMware SVGA II" in out, "infopc muestra el acelerador")
    m.captura("H161-vmware")
    verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones")
    return cap_gpu, k_gpu, q_gpu, us_gpu, us_cpu


def vmware():
    # Con el acelerador elegido solo.
    forzado = []
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h161", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("[ACEL2D] tarjeta de video 15ad:0405" in s and "acelerador vmware" in s,
                  "la VMware SVGA II se detecta por el PCI")
        verificar("[ACEL2D] la pantalla (fd000000) es de la tarjeta 15ad:0405" in s,
                  "la pantalla del VBE es de la VMware (por sus BAR, sin tocar la tarjeta)")
        print("   ", linha(s, r"\[ACEL2D\] vmware: puertos.*"))
        verificar(re.search(r"autoprueba del arranque: \d+ ordenes .*IGUAL que la CPU", s),
                  "la autoprueba del arranque da los mismos píxeles que la CPU")
        print("   ", linha(s, r"\[ACEL2D\] la autoprueba del arranque tardo.*"))
        print("   ", linha(s, r"\[ACEL2D\] velocidad:.*"))
        if "[ACEL2D] activo: VMware SVGA II" in s:
            cap_gpu, k_gpu, q_gpu, us_gpu, us_cpu = com_acelerador(m)
        else:
            verificar(KVM and "es mas lenta que la CPU" in s,
                      "solo con KVM la GPU emulada puede perder contra la CPU (y entonces se queda la CPU)")
            print("    con KVM la VMware es más lenta que la CPU: se sigue con --acel2d=vmware")
            forzado = ["--acel2d=vmware"]
    if forzado:
        img = imagem_de_teste("", ["--vesa"] + forzado)
        with Maquina("h161v", "ide", imagem=img, extras=VMWARE) as m:
            arrancar(m)
            cap_gpu, k_gpu, q_gpu, us_gpu, us_cpu = com_acelerador(m)

    # Lo mismo con la CPU: la pantalla tiene que quedar igual.
    m2, cap_cpu, (q_cpu, k_cpu, a2) = escritorio("h161c", ["--sin-acel2d"])
    print("   ", linha(m2.serial(), r"\[ACEL2D\] desactivado.*"))
    s2 = m2.serial()
    verificar("[ACEL2D] desactivado: la opcion de arranque pide la CPU" in s2 and a2 == "cpu",
              "--sin-acel2d: la CPU, aunque esté la VMware")
    verificar(cap_gpu == cap_cpu,
              "arrastrar con la GPU deja la pantalla igual, píxel a píxel, que con la CPU")
    print(f"    medida del arrastre: GPU {k_gpu} kciclos por cuadro ({q_gpu} cuadros), "
          f"CPU {k_cpu} ({q_cpu} cuadros): {k_cpu / max(k_gpu, 1):.1f} veces")
    print(f"    medida de la orden: {us_gpu} us por la GPU, {us_cpu} us por la CPU")
    if not forzado:
        verificar(k_gpu < k_cpu, "el arrastre por la GPU cuesta menos CPU por cuadro que sin ella")


def consola():
    img = imagem_de_teste("", ["--vesa", "--console"] + (["--acel2d=vmware"] if KVM else []))
    with Maquina("h161t", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        verificar("[ACEL2D] activo" in m.serial(), "la terminal de pantalla completa con el acelerador")
        for i in range(3):
            m.comando("ls /bin", 60)
        m.comando("echo FIN161", 30)
        texto = "\n".join(m.tela_texto())
        verificar("FIN161" in texto and "sagui:/$" in texto, "el texto desplazado por la GPU se lee bien")
        out = m.comando("acel2d", 30)
        mc = re.search(r"copias (\d+), rellenos (\d+)", out)
        verificar(mc and int(mc.group(1)) > 20 and int(mc.group(2)) > 20,
                  f"desplazar la terminal copia y rellena con la GPU ({mc.group(0) if mc else '?'})")
        m.captura("H161-consola")


def fallos():
    for prueba, texto in (("pixel", "pixeles distintos de la CPU"), ("tiempo", "tiempo agotado al final (simulado)")):
        img = imagem_de_teste("", ["--vesa", "--acel2d-prueba=" + prueba])
        with Maquina("h161f" + prueba, "ide", imagem=img, extras=VMWARE) as m:
            arrancar(m)
            s = m.serial()
            verificar("autoprueba fallida" in s and texto in s and "[ACEL2D] activo" not in s,
                      f"autoprueba fallida ({prueba}): vuelve a la CPU y el log dice por qué")
            verificar("modo SVGA apagado" in s, f"({prueba}) la tarjeta vuelve al modo del VBE")
            arrastar_lento(m, *TITULO, *DESTINO)
            q, k, a = medida(m.serial())
            verificar(a == "cpu", f"({prueba}) el escritorio funciona con la CPU")
            m.comando("infopc -t video", 60)
            time.sleep(1)   # la línea puede llegar a la serial después del prompt
            verificar(re.search(r"2D: +CPU \(VMware", m.serial()), f"({prueba}) infopc dice que va la CPU")
            m.comando("acel2d", 30)
            time.sleep(1)
            verificar(re.search(r"Aceleracion 2D: CPU\s+VMware SVGA II: autoprueba fallida", m.serial()),
                      f"({prueba}) «acel2d» dice el motivo")
            if prueba == "pixel":
                m.captura("H161-fallo")
    img = imagem_de_teste("", ["--vesa", "--acel2d=intel"])
    with Maquina("h161i", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        verificar("se pidio el acelerador intel y la pantalla no es de una tarjeta suya" in m.serial(),
                  "--acel2d=intel sin tarjeta Intel: la CPU")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h161s", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s), "-vga std: ninguna tarjeta del acelerador, la CPU")
        arrastar_lento(m, *TITULO, *DESTINO)
        verificar(medida(m.serial())[2] == "cpu", "el arrastre con la tarjeta de siempre va por la CPU")


def main():
    partes = sys.argv[1:] or ["vmware", "consola", "fallos", "std"]
    for p in partes:
        print(f"== {p}")
        {"vmware": vmware, "consola": consola, "fallos": fallos, "std": std}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
