#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H394 (D387) - volcado del búfer de atrás a la pantalla por la GPU (GART de las Radeon R5xx).

QEMU no emula ninguna tarjeta que lea la RAM del sistema con su motor 2D, así que aquí se prueba
todo lo demás: la lógica pura (en el PC) y el camino del kernel con una «GPU» de software
(--volcado-gpu-simulado) que usa la misma tabla de páginas y las mismas franjas que la GART de la
R5xx y copia CON RETRASO (al esperar o con la siguiente orden del motor), como el motor de verdad.
Si algún camino del escritorio escribe en la pantalla o mueve el búfer de atrás sin esperar a la
GPU, la pantalla queda distinta de la de la CPU.

  unidad    tests/volcado_unidad.c: entradas de la GART (40 bits, PAE), su ventana en el mapa del
            MC (delante o detrás de la VRAM, sin pisar la AGP), franjas y un motor simulado que lee
            píxel a píxel por la tabla con páginas desordenadas (también por encima de 4 GB);
  simulado  -vga std con la GPU simulada: autoprueba superada, volcado activo, arrastrar el Terminal
            deja la pantalla igual, píxel a píxel, que con la CPU (--sin-volcado-gpu); los volcados
            por GPU suben; «acel2d volcado», «acel2d volcado medir» e infopc;
  vmware    lo mismo con la VMware SVGA II y su 2D activo (el arrastre mueve la pantalla con el 2D
            y el búfer de atrás con la CPU: el volcado pendiente tiene que esperar antes);
  fallos    autoprueba con un píxel distinto (no se enciende) y una espera agotada en sesión (el
            vigilante lo apaga y la CPU redibuja: la pantalla sigue igual que con la CPU);
  std       sin la simulada, -vga std: no hay volcado por GPU («sin acelerador 2D activo»), la
            opción volcado_gpu sale en kconfig, y con --sin-volcado-gpu lo dice el log.
  vuelta    D391, la vuelta del cuadro del 3D a la RAM por la GPU, con el 3D simulado y la GART
            simulada: la autoprueba (guardas alrededor, paso torcido) pasa; «sgldemo -g» y la ventana
            del cubo («sgldemo -n») dan la MISMA suma, cuadro a cuadro, con la vuelta y sin ella
            (acel2d vuelta apagar); los cuadros por GPU suben y ninguna escritura cae fuera;
            acel2d vuelta medir, infopc y kconfig vuelta_gpu;
  vfallos   D391: la autoprueba de la vuelta con un píxel distinto (no se enciende) y una espera
            agotada en sesión (se apaga y la CPU lee el cuadro: las sumas siguen iguales).

Uso: h394_volcado_gpu.py [unidad] [simulado] [vmware] [fallos] [std] [vuelta] [vfallos]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
import h161_acel2d as h161  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KVM = os.environ.get("SAGUI_KVM") == "1"
SIM = ["--volcado-gpu-simulado"]


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "volcado_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-o", exe, os.path.join(RAIZ, "tests", "volcado_unidad.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "GART, ventana del MC y franjas como el motor")


def sin_excepciones(s, que):
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones")


def escritorio(nome, opcoes, extras=None, depois=None):
    """Arranca, arrastra el Terminal, captura la pantalla sin la barra y, si se pide, hace más cosas.
    Devuelve (serial, captura, lo que devuelva «depois»)."""
    img = imagem_de_teste("", ["--vesa"] + opcoes)
    with Maquina(nome, "ide", imagem=img, extras=extras or []) as m:
        h161.arrancar(m)
        h161.arrastar_lento(m, *h161.TITULO, *h161.DESTINO)
        cap = h161.sem_barra(m)
        extra = depois(m) if depois else None
        return m.serial(), cap, extra


def estado_volcado(m):
    out = m.comando("acel2d volcado", 30)
    mo = re.search(r"GPU (\d+) volcados \((\d+) kpx\)", out)
    return out, (int(mo.group(1)), int(mo.group(2))) if mo else (0, 0)


def comprobar_activo(m, que):
    s = m.serial()
    verificar("[ACEL2D] volcado: autoprueba IGUAL que la CPU" in s, f"{que}: la autoprueba del volcado da los píxeles de la CPU")
    verificar("[ACEL2D] volcado por la GPU activo: simulado" in s, f"{que}: el volcado simulado se enciende")
    print("   ", h161.linha(s, r"\[ACEL2D\] volcado: autoprueba.*"))
    out, (n, kpx) = estado_volcado(m)
    print("   ", h161.linha(out, r"acel2d volcado: GPU.*"))
    verificar("activo" in out and n > 0 and kpx > 0, f"{que}: los volcados por GPU suben ({n}, {kpx} kpx)")
    return n


def simulado():
    def depois(m):
        n = comprobar_activo(m, "simulado")
        out = m.comando("acel2d volcado medir", 60)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] volcado medir:.*"))
        verificar("[ACEL2D] volcado medir:" in s and "detalle en el log" in out, "«acel2d volcado medir» mide GPU y CPU")
        out = m.comando("infopc -t gpu", 60)
        verificar(re.search(r"Volcados por GPU: (\d+) .*estado: activo \(simulado\)", out), "infopc cuenta los volcados por GPU")
        out = m.comando("acel2d volcado apagar", 30)
        verificar("apagado a mano" in out, "«acel2d volcado apagar»")
        _, (n2, _) = estado_volcado(m)
        out = m.comando("acel2d volcado encender", 60)
        verificar("activo" in out, "«acel2d volcado encender» repite la autoprueba y vuelve")
        sin_excepciones(m.serial(), "simulado")
        return n
    s, cap_gpu, n = escritorio("h394s", SIM, depois=depois)
    s2, cap_cpu, _ = escritorio("h394c", ["--sin-volcado-gpu"])
    verificar("[ACEL2D] volcado: apagado en las opciones del kernel (kconfig volcado_gpu=no)" in s2,
              "--sin-volcado-gpu: el log dice que está apagado por kconfig")
    verificar(cap_gpu == cap_cpu, "arrastrar con el volcado simulado deja la pantalla igual, píxel a píxel, que con la CPU")


def vmware():
    forzar = ["--acel2d=vmware"]   # con KVM la VMware puede perder contra la CPU: así siempre está el 2D
    def depois(m):
        s = m.serial()
        verificar("[ACEL2D] activo: VMware SVGA II" in s, "el 2D de la VMware está activo")
        return comprobar_activo(m, "vmware")
    s, cap_gpu, _ = escritorio("h394v", forzar + SIM, h161.VMWARE, depois)
    sin_excepciones(s, "vmware")
    _, cap_cpu, _ = escritorio("h394w", forzar + ["--sin-volcado-gpu"], h161.VMWARE)
    verificar(cap_gpu == cap_cpu, "con el 2D de la VMware y el volcado simulado, la pantalla queda igual que sin volcado")


def fallos():
    img = imagem_de_teste("", ["--vesa", "--volcado-gpu-prueba=fallo"] + SIM)
    with Maquina("h394f", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] volcado: autoprueba.*"))
        verificar("[ACEL2D] volcado: autoprueba FALLO" in s and "(simulado)" in s, "un píxel distinto: la autoprueba falla")
        verificar("[ACEL2D] volcado por la GPU activo" not in s, "y el volcado no se enciende")
        out, (n, _) = estado_volcado(m)
        verificar("fallo" in out and n == 0, "«acel2d volcado» dice fallo y no hay volcados por GPU")
        sin_excepciones(m.serial(), "fallo")

    def depois(m):
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] volcado: tiempo agotado.*"))
        verificar("[ACEL2D] volcado: tiempo agotado esperando un volcado de la GPU" in s, "la espera agotada se ve en el log")
        verificar("[ACEL2D] volcado por la GPU apagado: tiempo agotado" in s, "el vigilante apaga el volcado")
        out, _ = estado_volcado(m)
        verificar("fallo" in out, "«acel2d volcado» dice fallo")
        sin_excepciones(m.serial(), "cuelgue")
    _, cap_gpu, _ = escritorio("h394g", ["--volcado-gpu-prueba=cuelgue"] + SIM, depois=depois)
    _, cap_cpu, _ = escritorio("h394c2", ["--sin-volcado-gpu"])
    verificar(cap_gpu == cap_cpu, "tras el cuelgue la CPU redibuja: la pantalla queda igual que con la CPU")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h394d", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("[ACEL2D] volcado: sin acelerador 2D activo: copia la CPU" in s, "-vga std sin simulada: copia la CPU")
        out = m.comando("kconfig", 30)
        verificar("volcado_gpu" in out, "la opción volcado_gpu sale en kconfig")
        out = m.comando("infopc -t gpu", 60)
        verificar(re.search(r"Volcados por GPU: 0 ", out), "infopc: ningún volcado por GPU")
        verificar(re.search(r"Vuelta del 3D por GPU: 0 cuadros", out), "infopc: ninguna vuelta por GPU (D391)")
        sin_excepciones(m.serial(), "std")


GPU3D_SIM = ["--vesa", "--gpu3d=simulado"] + SIM


def somas_g(out):
    """Las sumas de los cuadros por la GPU de «sgldemo -g» (una por escena)."""
    return re.findall(r"^gpu (\w+): .*suma gpu ([0-9a-f]{8}): BIEN$", out, re.M)


def estado_vuelta(m):
    out = m.comando("acel2d vuelta", 30)
    mo = re.search(r"acel2d vuelta: GPU (\d+) cuadros \((\d+) kpx\), CPU (\d+)", out)
    return out, tuple(int(x) for x in mo.groups()) if mo else (0, 0, 0)


def vuelta():
    img = imagem_de_teste("", GPU3D_SIM)
    with Maquina("h394r", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] vuelta: autoprueba.*"))
        verificar("[ACEL2D] vuelta: autoprueba IGUAL que la CPU" in s, "la autoprueba de la vuelta (dos destinos con guardas) pasa")
        verificar("[ACEL2D] vuelta por la GPU activa: simulada" in s, "la vuelta simulada se enciende")
        out = m.comando("kconfig", 30)
        verificar("vuelta_gpu" in out, "la opción vuelta_gpu sale en kconfig")
        # 1) sgldemo -g con la vuelta: las escenas fijas y el cubo, cuadros traídos por la GPU.
        out1 = m.comando("sgldemo -g 4", 300)
        print("   ", "\n    ".join(l for l in out1.splitlines() if l.startswith("gpu ") or "sgldemo" in l)[:1500])
        verificar("sgldemo -g: BIEN" in out1, "sgldemo -g con la vuelta: BIEN")
        mo = re.search(r"vuelta del cuadro por la GPU: activa; (\d+) cuadros por la GPU, (\d+) por la CPU", out1)
        verificar(mo and int(mo.group(1)) >= 8 and int(mo.group(2)) == 0,
                  f"todos los cuadros vuelven por la GPU, también los de la memoria del programa ({mo.group(0) if mo else '?'})")
        s1 = somas_g(out1)
        # Las cuatro escenas de siempre y, desde D404 (SaguiGL realista), bilineal, luces y paisaje.
        verificar(len(s1) >= 4, f"una suma por escena de sgldemo -g ({s1})")
        jan1 = m.comando("sgldemo -n 12", 300)
        print("   ", h161.linha(jan1, r"sgldemo: ventana.*"))
        j1 = re.search(r"sgldemo: ventana 320x240, 12 cuadros por la GPU: .* suma ([0-9a-f]{8})", jan1)
        verificar(j1, "la ventana del cubo, 12 cuadros por la GPU")
        _, (n1, kpx1, cpu1) = estado_vuelta(m)
        verificar(n1 >= mo_n(mo) + 12 and kpx1 > 0, f"la ventana también vuelve por la GPU ({n1} cuadros, {kpx1} kpx, {cpu1} por la CPU)")
        out = m.comando("acel2d vuelta medir", 60)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] vuelta medir:.*"))
        verificar("[ACEL2D] vuelta medir:" in s and "detalle en el log" in out, "«acel2d vuelta medir» mide GPU y CPU")
        verificar(re.search(r"region 2 \(escritura\) \d+ paginas, 0 escrituras perdidas", s),
                  "ninguna escritura de la GPU simulada cayó fuera de la región de escritura")
        out = m.comando("infopc -t gpu", 60)
        verificar(re.search(r"Vuelta del 3D por GPU: (\d+) cuadros .*estado: activa", out), "infopc cuenta la vuelta")
        # 2) sin la vuelta: la CPU lee el cuadro; las sumas tienen que ser las mismas.
        out = m.comando("acel2d vuelta apagar", 30)
        verificar("apagada a mano" in out, "«acel2d vuelta apagar»")
        out2 = m.comando("sgldemo -g 4", 300)
        verificar("sgldemo -g: BIEN" in out2 and "vuelta del cuadro por la GPU: no" in out2, "sgldemo -g sin la vuelta: BIEN")
        verificar(somas_g(out2) == s1, f"cada escena da la MISMA suma con la vuelta y sin ella ({s1} / {somas_g(out2)})")
        jan2 = m.comando("sgldemo -n 12", 300)
        j2 = re.search(r"sgldemo: ventana 320x240, 12 cuadros por la GPU: .* suma ([0-9a-f]{8})", jan2)
        verificar(j1 and j2 and j1.group(1) == j2.group(1), f"la ventana del cubo: la misma suma ({j1.group(1) if j1 else '?'} / "
                  f"{j2.group(1) if j2 else '?'})")
        out = m.comando("acel2d vuelta encender", 120)
        verificar("activa" in out, "«acel2d vuelta encender» repite la autoprueba y vuelve")
        out3 = m.comando("sgldemo -g 2", 300)
        verificar(somas_g(out3) == s1, "y vuelve a dar las mismas sumas")
        # 3) la ventana sin pausa (-s) cierra con Esc igual que antes: aquí basta con que -n -s termine.
        jan3 = m.comando("sgldemo -s -n 6", 300)
        verificar(re.search(r"sgldemo: ventana 320x240, 6 cuadros por la GPU", jan3), "sgldemo -s -n 6 (sin pausa)")
        sin_excepciones(m.serial(), "vuelta")
        m.captura("H394-vuelta")
    # 4) apagada en kconfig (--sin-vuelta-gpu): la CPU lee el cuadro y el volcado sigue.
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--sin-vuelta-gpu"] + SIM)
    with Maquina("h394rk", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("[ACEL2D] vuelta: apagada en las opciones del kernel (kconfig vuelta_gpu=no)" in s, "--sin-vuelta-gpu: apagada por kconfig")
        verificar("[ACEL2D] volcado por la GPU activo: simulado" in s, "y el volcado sigue a lo suyo")
        out = m.comando("sgldemo -g 2", 300)
        verificar("sgldemo -g: BIEN" in out and "vuelta del cuadro por la GPU: no; 0 cuadros" in out, "sin la vuelta, la CPU lee el cuadro")
        sin_excepciones(m.serial(), "sin vuelta")


def mo_n(mo):
    return int(mo.group(1)) if mo else 0


def vfallos():
    img = imagem_de_teste("", GPU3D_SIM + ["--volcado-gpu-prueba=vuelta-fallo"])
    with Maquina("h394rf", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] vuelta: autoprueba.*"))
        verificar("[ACEL2D] vuelta: autoprueba FALLO" in s and "(simulado)" in s, "un píxel distinto: la autoprueba de la vuelta falla")
        verificar("[ACEL2D] vuelta por la GPU activa" not in s and "[ACEL2D] volcado por la GPU activo: simulado" in s,
                  "la vuelta no se enciende; el volcado sí")
        out = m.comando("sgldemo -g 2", 300)
        verificar("sgldemo -g: BIEN" in out and "vuelta del cuadro por la GPU: no; 0 cuadros" in out, "la CPU lee el cuadro")
        ref = somas_g(out)
        sin_excepciones(m.serial(), "vuelta-fallo")
    img = imagem_de_teste("", GPU3D_SIM + ["--volcado-gpu-prueba=vuelta-cuelgue"])
    with Maquina("h394rg", "ide", imagem=img) as m:
        h161.arrancar(m)
        out = m.comando("sgldemo -g 2", 300)
        s = m.serial()
        print("   ", h161.linha(s, r"\[ACEL2D\] vuelta: tiempo agotado.*"))
        verificar("[ACEL2D] vuelta: tiempo agotado esperando la vuelta" in s and "[ACEL2D] vuelta por la GPU apagada: tiempo agotado" in s,
                  "la espera agotada apaga la vuelta (el vigilante)")
        verificar("sgldemo -g: BIEN" in out and somas_g(out) == ref, f"tras el cuelgue la CPU lee el cuadro: las mismas sumas ({somas_g(out)})")
        out, _ = estado_vuelta(m)
        verificar("fallo" in out, "«acel2d vuelta» dice fallo")
        sin_excepciones(m.serial(), "vuelta-cuelgue")


def main():
    partes = sys.argv[1:] or ["unidad", "simulado", "vmware", "fallos", "std", "vuelta", "vfallos"]
    for p in partes:
        print(f"== {p}")
        {"unidad": unidad, "simulado": simulado, "vmware": vmware, "fallos": fallos, "std": std, "vuelta": vuelta,
         "vfallos": vfallos}[p]()
    print("H394 OK")


if __name__ == "__main__":
    main()
