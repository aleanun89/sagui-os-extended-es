#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H401 (D399) - energía de la GPU: relojes, estados de la BIOS ATOM, temperatura, VRAM y modos.

QEMU no emula ninguna Radeon: aquí se prueba todo lo demás. El driver de verdad (energia_radeon.c) solo
se puede probar en el Amilo (/home/aanun/SO/private/amilo-energia/ORDENES.txt).

  unidad    tests/gpu_energia_unidad.c en el PC: las tablas de datos ATOM sobre una BIOS de juguete
            escrita a mano, el intérprete ATOM (copia de Linux, MIT) con registros de mentira, imágenes
            rotas al azar (con ASan/UBSan si el compilador los tiene), el PLL, «bajo»/«alto» y el
            gobernador «auto»;
  sim       --gpu-energia-simulada (la misma BIOS de juguete en el intérprete DENTRO del kernel): «acel2d
            energia» (estados, relojes, temperatura «sin sensor», VRAM), leer, bajo, alto, simular (ensayo
            sin escribir), bios (vuelta a los del arranque), auto (baja en reposo y sube con el 3D simulado
            trabajando: sgldemo), kconfig gpu_energia, infopc -t gpu, Tareas (Rendimiento > GPU) y
            Configuración (Pantalla > Aceleración), con capturas;
  fallo     la BIOS de juguete escribe mal el reloj: la comprobación lo ve, vuelve a la BIOS y se bloquea;
            desbloquear deja intentarlo otra vez (y vuelve a fallar igual);
  cuelgue   SetEngineClock no termina: el vigilante del intérprete la corta, vuelta a la BIOS y bloqueo,
            sin colgar el sistema;
  std       sin la simulada (-vga std): «sin driver de energía», nada se toca, la opción sale en kconfig y
            Tareas/Configuración lo dicen.

Uso: h401_gpu_energia.py [unidad] [sim] [fallo] [cuelgue] [std]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
import h323_conf_aceleracion as h323  # noqa: E402
import h370_tareas_moderno as h370  # noqa: E402
import conf_arbol as ca  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = ["--vesa", "--gpu-energia-simulada"]
ESTADO = re.compile(r"acel2d energia: modo (\S+) estado (-?\d+) motor (\d+) memoria (\d+) fallos (\d+)( bloqueado)?")


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "gpu_energia_unidad")
    fuentes = [os.path.join(RAIZ, "tests", "gpu_energia_unidad.c"), os.path.join(RAIZ, "kernel", "terceros", "mit", "atom", "atom.c")]
    base = ["gcc", "-O1", "-g", "-Wall", "-Wextra", "-DATOM_ANFITRIAO", "-o", exe] + fuentes
    r = subprocess.run(base[:1] + ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"] + base[1:], capture_output=True, text=True)
    if r.returncode != 0:   # sin las bibliotecas de los sanitizadores
        r = subprocess.run(base, capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "gpu_energia_unidad: OK" in r.stdout, "tablas ATOM, intérprete, imágenes rotas, PLL y gobernador")


def sin_excepciones(s, que):
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones")


def estado(m, orden="acel2d energia", tempo=60):
    out = m.comando(orden, tempo)
    g = ESTADO.findall(out)
    verificar(g, f"«{orden}» da su línea de estado ({out[-300:]!r})")
    modo, est, s, k, f, b = g[-1]
    return out, modo, int(est), int(s), int(k), int(f), bool(b)


def arrancar(m):
    m.esperar_serial("sagui:/$", 240)
    h323.esperar(m, r"supervisor en marcha", 60)
    time.sleep(2)


def tareas_gpu(m, captura):
    """Abre el Administrador de tareas en Rendimiento > GPU y devuelve la línea «tareas: gpu energia»."""
    n0 = len(m.serial())
    m.comando("tareas &")
    m.esperar_serial("tareas: pronto", 60)
    j = h370.janela(m.serial(), "Administrador de tareas")
    m.clique(j["x"] + h370.ABA_X["rendimiento"], j["y"] + h370.TAB_Y)
    m.esperar_serial("tareas: pestana rendimiento", 10)
    n = h370.contar(m, r"tareas: recurso gpu")
    m.clique(j["x"] + h370.SB_X, j["y"] + h370.SB_Y0 + h370.RECURSOS.index("gpu") * h370.SB_A + 22)
    h370.esperar_novo(m, r"tareas: recurso gpu", n, 10)
    g = h370.esperar_novo(m, r"tareas: gpu energia soporte (\d+) modo (\d+) estado (-?\d+) motor (\d+) memoria (\d+) kHz "
                             r"temp (-?\d+) zonas (\d+) vram (\d+)/(\d+) KB", h370.contar(m, r"tareas: gpu energia"), 20)
    time.sleep(1.5)
    m.captura(captura)
    # el foco vuelve al Terminal (su barra de título), como en h370
    t = h370.janela(m.serial()[:n0] if "Terminal" in m.serial()[:n0] else m.serial(), "Terminal")
    m.clique(t["fx"] + 30, t["fy"] + 8)
    time.sleep(1)
    return [int(x) for x in g]


def configuracion(m, captura):
    v, n0 = h323.abrir_ventana(m)
    g = re.findall(r"configuracion: gpu energia soporte=(\d+) modo=(\d+) estado=(-?\d+) motor=(\d+) memoria=(\d+) temp=(-?\d+) "
                   r"zonas=(\d+) vram=(\d+)/(\d+)", m.serial()[n0:])
    verificar(g, "Configuración registra el bloque de energía de la tarjeta")
    # el bloque está debajo de los selectores: hasta el final con la barra (como h369)
    ca.al_final(m, v)
    time.sleep(1)
    m.captura(captura)
    h323.cerrar_ventana(m, v)
    return [int(x) for x in g[-1]]


def sim():
    img = imagem_de_teste("", SIM + ["--gpu3d=simulado"])
    with Maquina("h401s", "ide", imagem=img, memoria="512M") as m:
        arrancar(m)
        s = m.serial()
        verificar("[GPUENERGIA] simulado: BIOS «SAGUI», 3 estados, 445000/250000 kHz por defecto" in s,
                  "el driver simulado lee las tablas de la BIOS de juguete")
        verificar(re.search(r"\[GPUENERGIA\] simulado: .*kconfig gpu_energia=0 -> auto", s), "automático: la simulada está confirmada -> auto")
        out, modo, est, sclk, mclk, f, b = estado(m)
        print("   ", "\n    ".join(out.strip().splitlines()[:16]))
        verificar("Energia de la GPU: simulado (control)" in out and "BIOS de video: SAGUI" in out, "«acel2d energia»: driver y BIOS")
        verificar("temperatura: sin sensor (tarjeta simulada)" in out, "la temperatura dice que no hay sensor (no se inventa)")
        verificar(re.search(r"VRAM: 16 MB; SaguiOS usa unos \d+ MB", out), "VRAM total y usada")
        verificar(re.search(r"0: 100 MHz / 100 MHz ahorro, sin pantalla", out) and re.search(r"2: 445 MHz / 250 MHz por defecto", out),
                  "los tres estados de la tabla")
        # kconfig automático: a los 5 s se pone el modo «auto» y, en reposo (sin 2D por la GPU), baja al estado 1
        fin = time.time() + 40
        while time.time() < fin and est != 1:
            time.sleep(2)
            out, modo, est, sclk, mclk, f, b = estado(m)
        verificar(modo == "auto" and est == 1 and sclk == 200000, f"auto en reposo: baja al estado 1 (modo {modo}, estado {est}, {sclk} kHz)")
        s = m.serial()
        verificar("[GPUENERGIA] relojes del arranque (por la BIOS): motor 445000 kHz, memoria 250000 kHz" in s,
                  "antes del primer cambio lee los relojes del arranque por la BIOS (GetEngineClock en el intérprete)")
        verificar("[GPUENERGIA] comprobacion tras el cambio:" in s, "comprueba después de cambiar")
        # el 3D simulado trabajando: sube
        m.comando("sgldemo -g 400", 300)   # sostenido: unos 2 s de 3D (4 cuadros duran 20 ms: no basta, y está bien)
        out, modo, est, sclk, mclk, f, b = estado(m)
        verificar(re.search(r"auto: sube: estado 2", m.serial()), "auto: con el 3D trabajando sube al estado 2")
        # a mano
        out, modo, est, sclk, mclk, f, b = estado(m, "acel2d energia bajo")
        verificar(modo == "bajo" and est == 1 and sclk == 200000 and mclk == 200000, f"bajo: estado 1 (200/200 MHz) ({modo} {est} {sclk})")
        out, modo, est, sclk, mclk, f, b = estado(m, "acel2d energia alto")
        verificar(modo == "alto" and est == 2 and sclk == 445000 and mclk == 250000, "alto: el de por defecto")
        out = m.comando("acel2d energia estado 0", 60)
        verificar("NO" in out and "no vale con 1 pantalla" in out, "el estado «sin pantalla» se rechaza con la pantalla encendida")
        out = m.comando("acel2d energia simular 1", 60)
        verificar("bien" in out and "ensayo del estado 1" in out, "simular: ensayo sin escribir")
        verificar(re.search(r"\[GPUENERGIA\] simulado: ensayo: PLL 30 <- 4e20 \(no se escribe\)", m.serial()),
                  "el ensayo enseña lo que la BIOS escribiría (PLL 0x30 = 20000)")
        out, modo, est, sclk, mclk, f, b = estado(m)
        verificar(est == 2 and sclk == 445000, "el ensayo no cambió nada")
        out, modo, est, sclk, mclk, f, b = estado(m, "acel2d energia bios")
        verificar(modo == "bios" and est == -1 and sclk == 445000 and mclk == 250000, "bios: vuelve a los relojes del arranque")
        out = m.comando("acel2d energia detalle", 30)
        verificar("[GPUENERGIA] simulado: PLL 30 " in m.serial(), "detalle al registro")
        out = m.comando("acel2d energia leer", 30)
        verificar("relojes leidos por la BIOS: motor 445 MHz, memoria 250 MHz" in out, "leer por la BIOS")
        # «auto» a mano (la tarea ya existe desde el arranque) y de vuelta a la BIOS
        out, modo, est, sclk, mclk, f, b = estado(m, "acel2d energia auto")
        verificar(modo == "auto", "auto a mano")
        fin = time.time() + 30
        while time.time() < fin and est != 1:
            time.sleep(2)
            out, modo, est, sclk, mclk, f, b = estado(m)
        verificar(est == 1, "auto a mano: baja en reposo")
        out, modo, est, sclk, mclk, f, b = estado(m, "acel2d energia bios")
        verificar(modo == "bios" and est == -1, "y vuelve a la BIOS")
        out = m.comando("kconfig", 30)
        verificar("gpu_energia" in out and "auto|bios|bajo|alto|dinamico" in m.comando("kconfig -h", 30), "la opción gpu_energia en kconfig")
        out = m.comando("kconfig gpu_energia=bajo", 30)
        out = m.comando("kconfig", 30)
        verificar(re.search(r"gpu_energia.*bajo", out), f"kconfig gpu_energia=bajo se guarda ({out[-400:]!r})")
        m.comando("kconfig gpu_energia=defecto", 30)
        out = m.comando("infopc -t gpu", 60)
        verificar("Energía de la GPU: simulado (control)" in out and "Estados de la BIOS (bajo = 1, alto = 2)" in out, "infopc -t gpu")
        g = tareas_gpu(m, "H401-tareas-gpu-simulada")
        verificar(g[0] == 2 and g[3] > 0 and g[5] == -300000 and g[8] == 16384, f"Tareas: soporte, reloj, «sin sensor» y VRAM ({g})")
        g = configuracion(m, "H401-configuracion-simulada")
        verificar(g[0] == 2 and g[3] > 0 and g[8] == 16384, f"Configuración: el bloque de la tarjeta trae la energía ({g})")
        sin_excepciones(m.serial(), "sim")


def con_prueba(nome, prueba, que):
    img = imagem_de_teste("", SIM + [f"--gpu-energia-prueba={prueba}"])
    with Maquina(nome, "ide", imagem=img) as m:
        arrancar(m)
        # el automático pone «auto» a los 5 s y, en reposo, intenta bajar: tiene que fallar y bloquearse
        fin = time.time() + 60
        b = False
        while time.time() < fin and not b:
            time.sleep(2)
            out, modo, est, sclk, mclk, f, b = estado(m)
        s = m.serial()
        print("   ", "\n    ".join(l for l in s.splitlines() if "[GPUENERGIA]" in l)[-1500:])
        verificar(b and modo == "bios" and est == -1 and f >= 1, f"{que}: fallo, vuelta a la BIOS y bloqueado ({modo} {est} {f})")
        verificar(sclk == 445000 and mclk == 250000, f"{que}: los relojes del arranque ({sclk}/{mclk})")
        verificar("vuelta a la BIOS bien; bloqueado hasta reiniciar" in s, f"{que}: la vuelta a la BIOS sale bien")
        if prueba == "cuelgue":
            verificar("[GPUENERGIA] simulado: tabla 10 cortada (motivo 1)" in s, "cuelgue: el vigilante corta el bucle de SetEngineClock")
        else:
            verificar("la BIOS dice 220000/200000 kHz y se pidieron 200000/200000" in s, "fallo: la comprobación ve el reloj mal puesto")
        out = m.comando("acel2d energia alto", 60)
        verificar("NO" in out and "bloqueado" in out, f"{que}: bloqueado, no se reintenta solo")
        out = m.comando("acel2d energia desbloquear", 30)
        verificar("bien" in out, f"{que}: desbloquear a mano")
        out, modo, est, sclk, mclk, f2, b = estado(m, "acel2d energia bajo", 90)
        verificar(b and f2 == f + 1 and est == -1, f"{que}: otra vez falla y vuelve a bloquearse ({f} -> {f2})")
        out = m.comando("echo vivo", 30)
        verificar("vivo" in out, f"{que}: el sistema sigue respondiendo")
        sin_excepciones(m.serial(), que)


def fallo():
    con_prueba("h401f", "fallo", "fallo")


def cuelgue():
    con_prueba("h401c", "cuelgue", "cuelgue")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h401n", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar("[GPUENERGIA] sin driver de energia para esta tarjeta" in s, "-vga std: sin driver de energía")
        out = m.comando("acel2d energia", 30)
        verificar("Energia de la GPU: - (sin driver)" in out, f"«acel2d energia» lo dice ({out[-200:]!r})")
        out = m.comando("acel2d energia bajo", 30)
        verificar("NO" in out, "sin driver no se cambia nada")
        out = m.comando("kconfig", 30)
        verificar("gpu_energia" in out, "la opción sale en kconfig")
        g = tareas_gpu(m, "H401-tareas-gpu-sin-driver")
        verificar(g[0] == 0, f"Tareas: sin driver ({g})")
        g = configuracion(m, "H401-configuracion-sin-driver")
        verificar(g[0] == 0, f"Configuración: sin driver ({g})")
        sin_excepciones(m.serial(), "std")


def main():
    pedidos = sys.argv[1:] or ["unidad", "sim", "fallo", "cuelgue", "std"]
    for p in pedidos:
        print(f"== {p}")
        globals()[p]()
    print("H401 OK")


if __name__ == "__main__":
    main()
