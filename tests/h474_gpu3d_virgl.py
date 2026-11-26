#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H474 (D474) - gpu3d por VirGL: los programas 3D de siempre por la GPU del anfitrión en QEMU/Proxmox.

  fuentes  lo que tiene que estar en el árbol: el driver gpu3d_virgl.c con sus funciones, la capa que lo elige con
           VIRGL negociado, el codificador exportado de virgl.c, la decisión D474 y docs/gpus.md;
  virgl    -vga none -device virtio-vga-gl con -display sdl,gl=on: la capa elige «virgl» al arrancar sin encenderlo;
           «gpu3d probar» enciende el contexto 3D y pasa las ocho pruebas de la autoprueba común (comparadas píxel a
           píxel con lo que debe salir) y la medida; «sgldemo -g 4» dibuja sus siete escenas por software y por la
           GPU y salen iguales; «sgldemo -n» dibuja la ventana por la GPU; «sgldemo -p --mem --gpu» (el paisaje)
           por la GPU y por software, con los fps de cada uno; la SDL 2 a pantalla completa presenta por la GPU
           (D432: los cuadros vuelven del anfitrión y la CPU los escala al respaldo del escritorio): el patrón fijo
           se lee en la RAM del respaldo con «xp» del monitor (con GL no hay «screendump»); «gpu3d apagar» y
           «gpu3d encender» vuelven a probarlo. Sin QEMU con pantalla GL, se omite;
  pura     -device virtio-gpu-pci sin GL: no hay VIRGL y la capa no elige el driver (SaguiGL por software);
  std      -vga std: ni rastro del driver.

Uso: h474_gpu3d_virgl.py [fuentes] [virgl] [pura] [std]
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, FalhaTeste, imagem_de_teste, verificar, APPS_RAIZ  # noqa: E402

GL = ["-vga", "none", "-device", "virtio-vga-gl", "-display", "sdl,gl=on"]


def ler(caminho):
    with open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def sano(s, que):
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones ni pánicos")


def fuentes():
    d = ler("kernel/dispositivos/gpu3d_virgl.c")
    for f in ("v_iniciar", "v_alvo", "v_limpar", "v_textura", "v_desenhar", "v_esperar", "v_ler", "v_apresentar", "v_desligar"):
        verificar(f"static int {f}(" in d or f"static void {f}(" in d, f"gpu3d_virgl.c: {f}")
    verificar("const gpu3d_driver_t gpu3d_virgl" in d and "virgl_caps(" in d and "VC_ESCRIBIR" in d,
              "gpu3d_virgl.c: el driver, los formatos del capset y los vértices dentro del flujo")
    verificar("while (1)" not in d and "for (;;)" not in d, "gpu3d_virgl.c: ninguna espera sin plazo")
    capa = ler("kernel/gui/gpu3d.c")
    verificar("virgl_disponible()" in capa and "drv = &gpu3d_virgl" in capa, "gpu3d.c: la capa elige el driver con VIRGL negociado")
    h = ler("kernel/inc/virgl.h")
    for f in ("virgl_ctx_crear", "virgl_recurso_crear", "virgl_transferir", "virgl_enviar", "virgl_sombreador", "virgl_caps"):
        verificar(f in h, f"virgl.h: {f}")
    verificar("## D474." in ler("docs/decisoes.md"), "la decisión D474 está en docs/decisoes.md")
    verificar("VirGL" in ler("docs/gpus.md") and "D474" in ler("docs/gpus.md"), "docs/gpus.md habla del 3D por VirGL (D474)")


def respaldo(s):
    r = re.search(r"\[VIDEO\] virtio-gpu: (\d+)x(\d+), 32 bits, pitch (\d+), respaldo ([0-9a-f]+)", s)
    verificar(r is not None, "el registro da el respaldo del escritorio")
    return int(r.group(1)), int(r.group(2)), int(r.group(3)), int(r.group(4), 16)


def pixel(m, base, pitch, x, y):
    out = m.monitor("xp /1wx 0x%x" % (base + y * pitch + x * 4))
    r = re.search(r":\s*0x([0-9a-f]+)", out)
    return int(r.group(1), 16) & 0xFFFFFF if r else None


def parecido(a, b, tol=8):
    return a is not None and all(abs(((a >> s) & 255) - ((b >> s) & 255)) <= tol for s in (0, 8, 16))


def virgl():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    try:
        with Maquina("h474g", "ide", imagem=img, extras=GL) as m:
            try:
                m.esperar_serial("area de trabalho no ar", 150)
            except FalhaTeste:
                err = m.proc.stderr.read().decode("utf-8", "replace") if m.proc.poll() is not None else ""
                if "opengl is not available" in err or "display" in err or "egl" in err.lower():
                    print("    omitida: este QEMU no tiene pantalla con GL (" + err.strip().splitlines()[-1][:80] + ")")
                    return
                raise
            time.sleep(3)
            s = m.serial()
            verificar("[VIRGL] autoprueba: BIEN" in s, "la autoprueba de VirGL del arranque (D473) sigue bien")
            verificar("[GPU3D] virgl: virtio-gpu VirGL: 3D por la GPU del anfitrion sin probar" in s and "[GPU3D] encender" not in s,
                      "la capa elige el driver virgl al arrancar y no lo enciende hasta el primer uso")
            out = m.comando("gpu3d probar", 300)
            print("   ", "\n    ".join(l for l in out.splitlines() if "gpu3d probar" in l)[:1500])
            verificar("gpu3d probar: BIEN" in out, "«gpu3d probar»: autoprueba y medida por VirGL bien")
            verificar(len(re.findall(r"gpu3d probar:\s+\d \S.*\bbien\s*$", out, re.M)) == 8, "las ocho pruebas de la autoprueba común")
            s = m.serial()
            verificar(re.search(r"\[GPU3D\] virgl: formatos del anfitrion: .*Z de 24 bits con el formato (19|20|21|18)", s),
                      "el Z de 24 bits sale de un formato que el anfitrión tiene")
            verificar("[GPU3D] activo: virtio-gpu VirGL" in s, "el 3D queda activo")
            out = m.comando("sgldemo -g 4", 300)
            print("   ", "\n    ".join(l for l in out.splitlines() if l.startswith("gpu ") or "sgldemo" in l)[:1500])
            for cena in ("triangulo", "cubo", "zcruzado", "textura", "bilineal", "luces", "paisaje"):
                verificar(re.search(rf"^gpu {cena}: .*: BIEN", out, re.M), f"«sgldemo -g»: la escena {cena} por VirGL es la de software")
            verificar("sgldemo -g: BIEN" in out and "cuadros por la GPU" in out, "«sgldemo -g»: veredicto BIEN")
            out = m.comando("sgldemo -n 60", 200)
            verificar(re.search(r"sgldemo: ventana 320x240, 60 cuadros por la GPU", out), "«sgldemo -n»: la ventana del cubo por la GPU")
            fps = {}
            for modo, extra in (("gpu", " --gpu"), ("software", "")):
                out = m.comando(f"sgldemo -p 100 --mem{extra} -r 640x480", 300)
                r = re.search(r"sgldemo paisaje: 640x480, 100 cuadros por (la GPU|software): \d+ ms, ([\d.]+) fps", out)
                verificar(r and (r.group(1) == "la GPU") == (modo == "gpu"), f"el paisaje de 640x480 por {modo} ({out.strip()[-200:]})")
                fps[modo] = float(r.group(2))
            print(f"    paisaje 640x480: {fps['gpu']} fps por VirGL, {fps['software']} por software")
            # D432: la SDL 2 a pantalla completa presenta por la GPU; el patrón fijo queda en el respaldo del escritorio.
            W, H, pitch, base = respaldo(m.serial())
            marca = len(m.serial())
            m.digitar("/apps/bin/sdl2presentar -n 30 -l 320x180 -e 8", 0.04)
            m.tecla("ret", 0.04)
            limite = time.time() + 90
            while time.time() < limite and "[SDL2PRES] patron fijo" not in m.serial()[marca:]:
                time.sleep(0.3)
            verificar("[SDL2PRES] patron fijo" in m.serial()[marca:], "sdl2presentar llega al patrón fijo")
            time.sleep(2)
            da = W * 180 // 320
            dy = (H - da) // 2
            quer = ((W // 4, dy + da // 4, 0xFF0000), (3 * W // 4, dy + da // 4, 0x00FF00),
                    (W // 4, dy + 3 * da // 4, 0x0000FF), (3 * W // 4, dy + 3 * da // 4, 0xFFFFFF), (W // 2, dy // 2, 0))
            vistos = [(x, y, c, pixel(m, base, pitch, x, y)) for x, y, c in quer]
            mal = [v for v in vistos if not parecido(v[3], v[2])]
            verificar(not mal, f"el patrón escalado y las bandas negras están en el respaldo del escritorio ({mal})")
            limite = time.time() + 60
            while time.time() < limite and "[SDL2PRES] fin:" not in m.serial()[marca:]:
                time.sleep(0.5)
            s = m.serial()[marca:]
            r = re.search(r"\[SDL2PRES\] medida: (\d+) cuadros en (\d+) ms: ([\d.]+) fps; directos (\d+), por la RAM (\d+)", s)
            verificar(r and int(r.group(4)) == 30 and int(r.group(5)) == 0, "los 30 cuadros de la medida van directos a la pantalla")
            print(f"    SDL 2 por la GPU a pantalla completa (320x180 -> {W}x{da}): {r.group(3)} fps")
            verificar(re.search(r"\[GUI\] pantalla exclusiva: cedida a la ventana \d+", s) and re.search(r"la devuelve tras \d+ cuadros", s),
                      "el compositor cede la pantalla y la recupera al salir")
            out = m.comando("gpu3d", 30)
            verificar("3D por GPU: activo" in out and "driver virgl" in out and "motor libre" in out, "«gpu3d»: activo, driver virgl, motor libre")
            t = re.search(r"fallos (\d+)", out)
            verificar(t and int(t.group(1)) == 0, "sin fallos de la GPU")
            out = m.comando("gpu3d apagar", 30)
            verificar("apagado a mano" in out, "«gpu3d apagar»")
            out = m.comando("sgldemo -g 2", 200)
            verificar("sin 3D por GPU, SaguiGL por software" in out, "apagado: SaguiGL por software")
            out = m.comando("gpu3d encender", 120)
            verificar("gpu3d encender: activo" in out, "«gpu3d encender»: contexto nuevo, autoprueba otra vez y activo")
            out = m.comando("sgldemo -g 2", 300)
            verificar("sgldemo -g: BIEN" in out, "y sigue dibujando igual")
            sano(m.serial(), "virgl")
    except OSError as e:
        print("    omitida:", e)


def pura():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h474p", "ide", imagem=img, extras=["-vga", "none", "-device", "virtio-gpu-pci"]) as m:
        m.esperar_serial("area de trabalho no ar", 150)
        time.sleep(2)
        s = m.serial()
        verificar("[GPU3D] software:" in s and "gpu3d_virgl" not in s and "[GPU3D] virgl" not in s,
                  "virtio-gpu sin GL: no hay VIRGL y la capa sigue por software")
        out = m.comando("gpu3d", 30)
        verificar("sin GPU con driver 3D" in out, "«gpu3d» lo dice")
        sano(m.serial(), "pura")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h474s", "ide", imagem=img) as m:
        m.esperar_serial("area de trabalho no ar", 150)
        time.sleep(2)
        s = m.serial()
        verificar("[GPU3D] virgl" not in s and "[GPU3D] software:" in s, "-vga std: ni rastro del driver virgl")
        sano(s, "std")


def main():
    partes = sys.argv[1:] or ["fuentes", "virgl", "pura", "std"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "virgl": virgl, "pura": pura, "std": std}[p]()
    print("H474: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
