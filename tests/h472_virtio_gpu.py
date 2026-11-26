#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H472 (D472) - virtio-gpu en modo 2D: escritorio en QEMU/Proxmox sin VBE ni tarjeta de Bochs.

  pura     -vga none -device virtio-gpu-pci: sin VBE (la etapa 2 queda en texto), el kernel activa el driver
           virtio-gpu, que crea el recurso 2D, lo asocia a RAM del invitado y lo presenta con
           TRANSFER_TO_HOST_2D + RESOURCE_FLUSH; el escritorio llega y la captura no es negra;
           «pantalla 800x600» cambia de modo en caliente
           (recurso y respaldo nuevos) y la captura cambia de tamaño sin quedar en negro;
  vga      -vga virtio (virtio-vga, con VBE): la etapa 2 puso el modo por la BIOS y el driver se hace cargo
           con el mismo tamaño;
  std      -vga std: nada cambia (ni rastro de virtio-gpu, la tarjeta de siempre);
  mixta    -vga std junto a -device virtio-gpu-pci: la pantalla del arranque (VBE) se respeta.

  virgl    (D473) -vga none -device virtio-vga-gl con -display sdl,gl=on: se negocia VIRGL, se leen los capsets
           (virgl v1 de 308 bytes y virgl2 v2), se crea un contexto 3D con dos recursos, se limpia el destino y
           se dibuja un triángulo con color por vértice (sombreadores TGSI en texto) por SUBMIT_3D con valla; el
           resultado vuelve por TRANSFER_FROM_HOST_3D y el kernel cuenta los píxeles de fondo, rojo, verde, azul
           y mezclados («[VIRGL] autoprueba: BIEN»). Hace falta un QEMU con virglrenderer y una pantalla con GL
           (WSLg, o un X/Wayland): sin ella la parte se omite. No hay captura: con GL el scanout es una textura
           y «screendump» responde «no surface».

Uso: h472_virtio_gpu.py [pura] [vga] [std] [mixta] [virgl]
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, imagem_de_teste, verificar, SAIDA, ler_ppm  # noqa: E402

PURA = ["-vga", "none", "-device", "virtio-gpu-pci"]
VGA = ["-vga", "virtio"]
MIXTA = ["-vga", "std", "-device", "virtio-gpu-pci"]


def arrancar(m, tempo=180):
    m.esperar_serial("area de trabalho no ar", tempo)
    time.sleep(3)


def pantalla(m, nome):
    """(ancho, alto, píxeles distintos del negro, colores distintos) de una captura de la pantalla."""
    ppm = os.path.join(SAIDA, m.nome + "-" + nome + ".ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    l, a, px = ler_ppm(ppm)
    # Se mira una fila de cada 4 y un píxel de cada 2: basta para saber si hay escritorio.
    colores = set()
    no_negro = 0
    for y in range(0, a, 4):
        fila = px[y * l * 3:(y + 1) * l * 3]
        for x in range(0, l, 2):
            c = fila[x * 3:x * 3 + 3]
            colores.add(c)
            if c != b"\x00\x00\x00":
                no_negro += 1
    return l, a, no_negro, len(colores)


def activo(s):
    r = re.search(r"\[VIRTIOGPU\] activo: (\d+)x(\d+), 32 bits, scanout 0, recurso (\d+)", s)
    return (int(r.group(1)), int(r.group(2))) if r else None


def pura():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h472", "ide", imagem=img, extras=PURA) as m:
        arrancar(m)
        s = m.serial()
        verificar("VESA no encontrado" in s or "[VIDEO] virtio-gpu" in s, "sin VBE: no hay modo de la BIOS")
        modo = activo(s)
        verificar(modo == (1024, 768), f"el driver virtio-gpu se activa con 1024x768 ({modo})")
        verificar("[VIRTIOGPU] pantalla 0: activa" in s, "GET_DISPLAY_INFO responde con una pantalla activa")
        verificar("VIRGL no ofrecido" in s and "[VIRGL]" not in s,
                  "sin GL el dispositivo no ofrece VIRGL y no se intenta el 3D")
        verificar("[VIDEO] virtio-gpu: 1024x768" in s, "video.c usa el respaldo del driver como framebuffer")
        l, a, nn, cols = pantalla(m, "pura")
        verificar((l, a) == (1024, 768), f"la captura del QEMU es de 1024x768 ({l}x{a})")
        verificar(nn > 5000 and cols > 8, f"el escritorio se ve en la captura ({nn} puntos no negros, {cols} colores)")
        m.captura("H472-virtio-gpu")
        # El cursor y las ventanas llegan por rectángulos: el contador sube al mover el ratón.
        m.mouse_para(300, 300)
        time.sleep(0.5)
        # Cambio de modo en caliente.
        m.comando("pantalla 800x600", 60)
        time.sleep(1.5)
        s = m.serial()
        verificar("[VIDEO] modo trocado: 800x600, 32 bits, pitch 3200" in s, "«pantalla 800x600» cambia el modo en caliente")
        l, a, nn, cols = pantalla(m, "pura800")
        verificar((l, a) == (800, 600), f"la captura pasa a 800x600 ({l}x{a})")
        verificar(nn > 5000 and cols > 8, f"el escritorio sigue visible tras el cambio ({nn} puntos no negros, {cols} colores)")
        m.comando("pantalla 1280x800", 60)
        time.sleep(1.5)
        l, a, nn, cols = pantalla(m, "pura1280")
        verificar((l, a) == (1280, 800) and nn > 5000, f"y a 1280x800 ({l}x{a}, {nn} no negros)")
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial() and "no responde" not in m.serial(),
                  "sin excepciones ni fallos de la GPU")


def vga():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h472v", "ide", imagem=img, extras=VGA) as m:
        arrancar(m)
        s = m.serial()
        modo = activo(s)
        verificar(modo == (1024, 768), f"-vga virtio: el driver se activa con el modo que dejó el VBE ({modo})")
        l, a, nn, cols = pantalla(m, "vga")
        verificar((l, a) == (1024, 768) and nn > 5000 and cols > 8, f"el escritorio se ve ({l}x{a}, {nn} no negros, {cols} colores)")
        m.comando("pantalla 800x600", 60)
        time.sleep(1.5)
        l, a, nn, cols = pantalla(m, "vga800")
        verificar((l, a) == (800, 600) and nn > 5000, f"cambio en caliente con la virtio-vga ({l}x{a})")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h472s", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar("[VIRTIOGPU]" not in s and "[VIDEO] virtio-gpu" not in s, "-vga std: ni rastro de virtio-gpu")
        verificar(re.search(r"\[VIDEO\] 1024x768, 32 bits", s) is not None, "la tarjeta de siempre, por el VBE")
        l, a, nn, cols = pantalla(m, "std")
        verificar((l, a) == (1024, 768) and nn > 5000, "el escritorio de siempre")


def mixta():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h472m", "ide", imagem=img, extras=MIXTA) as m:
        arrancar(m)
        s = m.serial()
        verificar("[VIRTIOGPU] activo" not in s and re.search(r"\[VIDEO\] 1024x768, 32 bits", s) is not None,
                  "-vga std + virtio-gpu-pci: el VBE del arranque se respeta")
        l, a, nn, cols = pantalla(m, "mixta")
        verificar((l, a) == (1024, 768) and nn > 5000, "el escritorio sigue en la tarjeta del VBE")


def virgl():
    img = imagem_de_teste("", ["--vesa"])
    extras = ["-vga", "none", "-device", "virtio-vga-gl", "-display", "sdl,gl=on"]
    try:
        with Maquina("h472g", "ide", imagem=img, extras=extras) as m:
            try:
                m.esperar_serial("area de trabalho no ar", 120)
            except FalhaTeste:
                err = m.proc.stderr.read().decode("utf-8", "replace") if m.proc.poll() is not None else ""
                if "opengl is not available" in err or "display" in err or "egl" in err.lower():
                    print("    omitida: este QEMU no tiene pantalla con GL (" + err.strip().splitlines()[-1][:80] + ")")
                    return
                raise
            s = m.serial()
            verificar("VIRGL ofrecido y negociado" in s, "VIRGL se ofrece y se negocia con virtio-vga-gl")
            verificar(re.search(r"capset \d+: id 1 \(virgl\), version maxima 1, 308 bytes", s) is not None, "capset virgl v1 (308 bytes)")
            verificar(re.search(r"capset \d+: id 2 \(virgl2\), version maxima 2", s) is not None, "capset virgl2 v2")
            verificar("[VIRGL] contexto 1 creado" in s and "recursos 3D creados" in s, "contexto y recursos 3D")
            verificar(re.search(r"SUBMIT_3D de \d+ palabras: respuesta 1100", s) is not None, "SUBMIT_3D con valla responde OK")
            r = re.search(r"triangulo: fondo (\d+), rojo (\d+), verde (\d+), azul (\d+), mezclados (\d+)", s)
            verificar(r is not None, "la autoprueba cuenta los píxeles que trajo TRANSFER_FROM_HOST_3D")
            f, ro, ve, az, mez = map(int, r.groups())
            verificar(f > 3000 and ro > 20 and ve > 20 and az > 20 and mez > 1000,
                      f"el triángulo tiene los tres colores y el fondo ({f} fondo, {ro} rojo, {ve} verde, {az} azul, {mez} mezclados)")
            verificar("[VIRGL] autoprueba: BIEN" in s, "autoprueba VirGL BIEN")
            verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")
    except OSError as e:
        print("    omitida:", e)


def main():
    partes = sys.argv[1:] or ["pura", "vga", "std", "mixta", "virgl"]
    for p in partes:
        print(f"== {p}")
        {"pura": pura, "vga": vga, "std": std, "mixta": mixta, "virgl": virgl}[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
