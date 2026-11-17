#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H323 (D323) - Configuración > Pantalla > «Aceleración gráfica».

QEMU con la VMware SVGA II (2D por la GPU), el driver 3D simulado (--gpu3d=simulado), SAGUI-CONF y la sesión
como sagui (así el diálogo de administrador de D269 sale de verdad):

  1. La ventana: Pantalla > «Aceleración gráfica» enseña el estado (tarjeta, 2D, 3D) y las opciones.
  2. Cambia el 3D a «Activado», el 2D a «Desactivado», la SDL 2 a «Activado» y el disco a «Desactivado» con el
     diálogo de administrador (la SDL 2 no lo pide); «Probar ahora» del 3D. Lo aplicado se ve en `acel2d`,
     `gpu3d`, /tmp/acel-sdl2 y en sagui.conf, y la cabecera de arranque lleva lo del 2D y el disco.
  3. Tras reiniciar, `configuracion --aplicar` lo vuelve a aplicar (2D CPU, 3D activo, SDL 2, DMA apagado).
  4. «Restablecer valores seguros»: todo en automático (la cabecera también).
  5. La orden `configuracion aceleracion [2d|3d|sdl|disco] VALOR` (sin permisos: pide admin).
  6. Un arranque que no llegó al escritorio (la marca /home/.config/acel-arranque sigue puesta): el siguiente
     arranque restablece los valores seguros, lo anota y avisa (se ve en la ventana).
  7. Modo seguro pedido al arrancar (Mayús; aquí --acel-segura): 2D por la CPU, 3D por software y la
     configuración guardada no se aplica.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

BORDA, TITULO = 2, 24
Y_PAG = 50
CLAVE = "root1234"
TITULOS_PAG = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red",
               "Escritorio remoto", "Mandos", "Usuarios")
VMWARE = ["-vga", "none", "-device", "vmware-svga"]
import conf_arbol as ca  # noqa: E402


def lc(v, xl, yl):
    """Punto LOCAL del panel (D367) -> relativo al área de cliente de la ventana."""
    px, py, pw, ph = ca.geometria(v["l"], v["a"])
    return px - 16 + xl, py + yl


def boton(v, fila, col):
    """La opción `col` (0 automático, 1 activado, 2 desactivado) de la fila: grupos de 98 px, la opción a 124 px una de otra."""
    return lc(v, 30 + col * 124 + 10, 26 + 98 * fila + 6)


def probar(v, fila):
    px, py, pw, ph = ca.geometria(v["l"], v["a"])
    return lc(v, (pw + 22 - 36) - 130 + 65, 26 + 98 * fila + 7)


def restablecer(v):
    """«Predeterminado» (abajo a la izquierda del panel): los valores seguros."""
    x, y = ca.boton_pie(dict(x=0, y=0, l=v["l"], a=v["a"]), "predeterminado")
    return x, y


def aceptar(v):
    return ca.boton_pie(dict(x=0, y=0, l=v["l"], a=v["a"]), "aceptar")


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def ventana(m, titulo, desde=0):
    g = re.findall(r'janela (\d+) criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), m.serial()[desde:])
    verificar(g, f"ventana «{titulo}» creada")
    g = g[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def clic(m, v, p):
    m.clique(v["x"] + p[0], v["y"] + p[1])


def pestana(nombre):
    """Como traducir_rotulos() de configuracion.c (D145, issue #43, D340)."""
    suma = sum(len(t) * 8 for t in TITULOS_PAG)
    relleno = 10
    while relleno > 1 and 10 + suma + len(TITULOS_PAG) * (relleno + 1) > 788 - 4:
        relleno -= 1
    x = 4 if 10 + suma + len(TITULOS_PAG) * (relleno + 1) > 788 - 4 else 10
    for t in TITULOS_PAG:
        l = len(t) * 8 + relleno
        if t == nombre:
            return (x + l // 2, 24)
        x += l + 1


def conceder(m, desde, primera=False):
    """Atiende el diálogo de permisos del gestor de ventanas (D269)."""
    esperar(m, r"dialogo de permisos: configuracion", 40, desde)
    time.sleep(1)
    m.digitar(CLAVE, 0.05)
    if primera:
        m.tecla("tab")
        m.digitar(CLAVE, 0.05)
    m.tecla("ret")
    esperar(m, r"ELEVACION concedida: configuracion", 40, desde)
    time.sleep(1.5)


def abrir_ventana(m):
    n = len(m.serial())
    m.comando("configuracion --ventana &", 20)
    esperar(m, r'"Configuraci.n"', 30, n)
    time.sleep(1.5)
    v = ventana(m, "Configuración", n)
    m.clique(*ca.arbol(v, "aceleracion"))
    esperar(m, r"configuracion: pantalla vista aceleracion", 15, n)
    time.sleep(1)
    return v, n


def cerrar_ventana(m, v):
    clic(m, v, aceptar(v))
    time.sleep(1.5)


def admin(m, orden, tempo=90):
    return m.comando(f"echo {CLAVE} | admin {orden}", tempo)


def arrancar(m):
    m.esperar_serial("sagui:/$", 180)
    esperar(m, r"supervisor en marcha", 60)
    time.sleep(3)


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--gpu3d=simulado", "--acel2d=vmware", "--dados-mb", "16"])
    # ---------------------------------------------------------------- primer arranque
    with Maquina("h323", "ide", imagem=img, snapshot=False, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("[PERMISOS] sesion como sagui" in s, "la sesión es sagui")
        verificar(re.search(r"\[GPU3D\] simulado", s), "el driver 3D simulado está")
        out = m.comando("configuracion aceleracion")
        verificar("Aceleración gráfica" in out and "3D (simulado)" in out, f"la orden enseña el estado ({out!r})")
        out = m.comando("configuracion aceleracion 3d si")
        verificar("hace falta ser administrador" in out, f"sin permisos, la orden lo pide ({out!r})")

        v, n0 = abrir_ventana(m)
        verificar(v["l"] >= 700 and v["a"] >= 440, f"la ventana tiene su tamaño ({v['l']}x{v['a']})")
        m.captura("H323-1-estado")

        # 3D: Activado (diálogo de administrador; la primera vez root elige su contraseña)
        pos = len(m.serial())
        clic(m, v, boton(v, 1, 1))
        conceder(m, pos, True)
        esperar(m, r"configuracion: aceleracion fila 1 -> si: 3D: activo", 30, pos)
        m.captura("H323-2-3d-activado")
        # Probar ahora (3D)
        pos = len(m.serial())
        clic(m, v, probar(v, 1))   # el permiso queda recordado 5 minutos: sin diálogo
        esperar(m, r"configuracion: aceleracion probar 3d -> 1: BIEN", 60, pos)
        time.sleep(1)
        m.captura("H323-3-3d-probado")
        # 2D: Desactivado
        pos = len(m.serial())
        clic(m, v, boton(v, 0, 2))
        esperar(m, r"configuracion: aceleracion fila 0 -> no: 2D: ", 30, pos)
        # SDL 2: Activado (sin diálogo)
        pos = len(m.serial())
        clic(m, v, boton(v, 2, 1))
        esperar(m, r"configuracion: aceleracion fila 2 -> si", 30, pos)
        verificar("dialogo de permisos" not in m.serial()[pos:], "la SDL 2 no pide permisos")
        # Disco: Desactivado
        pos = len(m.serial())
        clic(m, v, boton(v, 3, 1))
        esperar(m, r"configuracion: aceleracion fila 3 -> no: Disco", 30, pos)
        time.sleep(1)
        m.captura("H323-4-todo-elegido")
        verificar(re.search(r"\[VIDEO\] cabecera 143 &03=03 grabada en \S+: 0", m.serial()), "la cabecera del disco lleva DMA y caché apagados")
        verificar(re.search(r"\[VIDEO\] cabecera 135 &0f=01 grabada en \S+: 0", m.serial()), "la cabecera lleva el 2D en CPU")
        cerrar_ventana(m, v)

        out = m.comando("acel2d")
        verificar("Aceleracion 2D: CPU" in out, f"acel2d: la CPU hace todo ({out[:300]!r})")
        out = m.comando("gpu3d")
        verificar("3D por GPU: activo" in out, f"gpu3d: activo ({out[:300]!r})")
        out = m.comando("cat /tmp/acel-sdl2")
        verificar(re.search(r"(^|\n)1", out), f"/tmp/acel-sdl2 vale 1 ({out!r})")
        out = m.comando("cat /home/.config/sagui.conf")
        for k in ("acel2d=no", "gpu3d=si", "sdl2_gpu=si", "disco_dma_cache=no"):
            verificar(k in out, f"sagui.conf lleva {k} ({out!r})")
        m.comando("sync")
        time.sleep(1)

    # ---------------------------------------------------------------- reinicio: se recuerda
    with Maquina("h323-b", "ide", imagem=img, snapshot=False, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("[ATA] DMA desactivado" in s, "tras reiniciar: el disco va por PIO (cabecera)")
        verificar("[CACHE] caché de bloques desactivada" in s, "tras reiniciar: sin caché de disco")
        verificar(re.search(r"configuracion: acel2d no -> 0", s), "configuracion --aplicar: 2D desactivado")
        verificar(re.search(r"configuracion: gpu3d si -> 0 \(3D: activo\)", s), "configuracion --aplicar: 3D activado")
        verificar(re.search(r"configuracion: sdl2_gpu si -> 0", s), "configuracion --aplicar: SDL 2 por la GPU")
        verificar("la marca de arranque seguia puesta" not in s, "arranque normal: sin marca vieja")
        verificar("aceleracion aplicada, marca de arranque borrada" in s, "la marca se borró al llegar al escritorio")
        out = m.comando("acel2d")
        verificar("Aceleracion 2D: CPU" in out, "sobrevive al reinicio: 2D en la CPU")
        out = m.comando("gpu3d")
        verificar("3D por GPU: activo" in out, "sobrevive al reinicio: 3D activo")
        out = m.comando("ls /home/.config")
        verificar("acel-arranque" not in out, "ya no queda la marca")
        v, n0 = abrir_ventana(m)
        time.sleep(1)
        m.captura("H323-5-tras-reiniciar")

        # Restablecer valores seguros
        pos = len(m.serial())
        clic(m, v, restablecer(v))
        conceder(m, pos)
        esperar(m, r"configuracion: aceleracion restablecida a los valores seguros", 30, pos)
        time.sleep(1)
        m.captura("H323-6-restablecido")
        cerrar_ventana(m, v)
        out = m.comando("cat /home/.config/sagui.conf")
        for k in ("acel2d=auto", "gpu3d=auto", "sdl2_gpu=auto", "disco_dma_cache=auto"):
            verificar(k in out, f"restablecer: sagui.conf lleva {k}")
        verificar(re.search(r"\[VIDEO\] cabecera 135 &0f=00 grabada en \S+: 0", m.serial()), "restablecer: la cabecera vuelve a 2D automático")
        verificar("[ACEL2D] encender a mano" in m.serial()[pos:], "restablecer: el 2D se vuelve a elegir")
        out = m.comando("ls /tmp")
        verificar("acel-sdl2" not in out, "restablecer: la SDL 2 vuelve a automático")

        # La orden de terminal
        out = admin(m, "configuracion aceleracion 3d no")
        verificar("apagado" in out, f"orden: 3d no ({out[-200:]!r})")
        out = admin(m, "configuracion aceleracion 3d probar")
        verificar("BIEN" in out, f"orden: 3d probar ({out[-200:]!r})")
        out = admin(m, "configuracion aceleracion 3d si")
        verificar("activo" in out, f"orden: 3d si ({out[-200:]!r})")
        out = m.comando("configuracion aceleracion sdl no")
        verificar("Desactivado" in out and "Guardado" in out, f"orden: sdl no no pide permisos ({out[-200:]!r})")
        out = m.comando("configuracion aceleracion")
        verificar("3D si" in out and "SDL 2 no" in out, f"orden: el estado dice lo elegido ({out[-300:]!r})")
        # Un cuelgue simulado: la marca de arranque en curso y el 3D activado a mano
        out = admin(m, "configuracion aceleracion --simular-cuelgue")
        verificar("Marca de arranque puesta" in out, "marca puesta")
        m.comando("sync")
        time.sleep(1)
        # el QEMU muere aquí (como un cuelgue antes del escritorio)

    # ---------------------------------------------------------------- arranque tras el cuelgue
    with Maquina("h323-c", "ide", imagem=img, snapshot=False, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("la marca de arranque seguia puesta" in s, "la marca sigue puesta: valores seguros")
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("gpu3d=auto" in out and "gpu3d=si" not in out and "sdl2_gpu=auto" in out, f"el archivo vuelve a automático ({out!r})")
        out = m.comando("cat /tmp/acel-aviso")
        verificar("sin llegar al escritorio" in out, f"el aviso queda escrito ({out!r})")
        out = m.comando("ls /home/.config")
        verificar("acel-arranque" not in out, "la marca se borra")
        v, n0 = abrir_ventana(m)
        time.sleep(1)
        m.captura("H323-7-aviso-arranque-fallido")
        cerrar_ventana(m, v)

    # ---------------------------------------------------------------- modo seguro (Mayús al arrancar)
    img2 = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--gpu3d=simulado", "--acel2d=vmware", "--acel-segura", "--dados-mb", "16"])
    with Maquina("h323-d", "ide", imagem=img2, extras=VMWARE) as m:
        arrancar(m)
        s = m.serial()
        verificar("MODO SEGURO pedido al arrancar" in s, "modo seguro: lo anota")
        out = m.comando("acel2d")
        verificar("Aceleracion 2D: CPU" in out and "modo seguro" in out, f"modo seguro: 2D por la CPU ({out[:250]!r})")
        out = m.comando("gpu3d")
        verificar("desactivado" in out and "software" in out, f"modo seguro: 3D por software ({out[:250]!r})")
        v, n0 = abrir_ventana(m)
        time.sleep(1)
        m.captura("H323-8-modo-seguro")
        cerrar_ventana(m, v)

    print("H323: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
