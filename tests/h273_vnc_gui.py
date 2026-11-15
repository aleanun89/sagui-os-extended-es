#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H273 (D273, mejora 104) - interfaz gráfica del VNC: ventana de conexión del cliente y pestaña del servidor.

QEMU (escritorio, sesión como sagui, con SAGUI-CONF):
  - Configuración > «Escritorio remoto»: fija la contraseña y activa el servicio `vncd` con el diálogo de
    permisos (D269: la primera vez root elige contraseña), cambia «Solo ver» y «Escuchar» (se guardan en
    /etc/vnc/vncd.conf y el servicio se reinicia) y los vuelve a dejar;
  - `vnc` sin argumentos: ventana de conexión; se rellena el formulario (equipo, nombre, contraseña,
    «Recordar») y se guarda en /home/.config/vnc.conf (la contraseña va ofuscada, nunca en claro);
  - se reabre `vnc`: la conexión sale en la lista, se elige y se conecta contra el `vncd` local;
  - mientras hay alguien conectado: `vncd --estado` lo muestra, aparece la ventana indicadora «Escritorio
    remoto» y la pestaña del servidor lo lista; «Desconectar» de la pestaña corta la sesión (el cliente lo
    dice, «R» reconecta con la contraseña recordada) y el botón «Desconectar» de la barra del cliente
    vuelve a la ventana de conexión; la ventana indicadora se cierra sola."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

BORDA, TITULO = 2, 24
Y_PAG = 50
CLAVE_ROOT = "root1234"
CLAVE_VNC = "clave123"
TABS = ["Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red",
        "Escritorio remoto"]


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2000:])


def ventana(m, titulo, desde=0):
    """Origen del área cliente (x, y) y tamaño de la última ventana con ese título."""
    g = re.findall(r'janela (\d+) criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), m.serial()[desde:])
    verificar(g, f"ventana «{titulo}» creada")
    g = g[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def clic(m, v, cx, cy, **kw):
    m.clique(v["x"] + cx, v["y"] + cy, **kw)


def pestana(v):
    """Centro de la pestaña «Escritorio remoto» (cada una mide su título + 10 y hay 2 px entre ellas)."""
    x = 10
    for t in TABS[:-1]:
        x += len(t) * 8 + 10 + 2
    return x + (len(TABS[-1]) * 8 + 10) // 2, 10 + 14


def lanzar_fondo(m, orden):
    n = len(m.serial())
    m.comando(orden + " &", 20)
    return n


def ident(m, titulo, desde=0):
    """Identificador de la última ventana creada con ese título."""
    g = re.findall(r'janela (\d+) criada por \d+: "%s" ' % titulo, m.serial()[desde:])
    verificar(g, f"ventana «{titulo}» creada")
    return int(g[-1])


def foco(m, ident_):
    """Da el foco a una ventana por su botón de la barra de tareas (el teclado va a la que tiene el foco)."""
    linha = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    x, l, e = re.findall(rf" {ident_}@(\d+)\+(\d+)([fm]?)", linha)[0]
    if e != "f":
        m.clique(int(x) + int(l) // 2, 12)
        time.sleep(0.8)


def en_barra(m, ident_):
    linha = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    return re.search(rf" {ident_}@", linha) is not None


def conceder(m, desde, primera):
    """Atiende el diálogo de permisos del gestor de ventanas (D269)."""
    esperar(m, r"dialogo de permisos: configuracion", 40, desde)
    time.sleep(1)
    m.digitar(CLAVE_ROOT, 0.05)
    if primera:
        m.tecla("tab")
        m.digitar(CLAVE_ROOT, 0.05)
    m.tecla("ret")
    esperar(m, r"ELEVACION concedida: configuracion", 40, desde)
    time.sleep(1)


def comando(m, term, orden, tempo=20):
    foco(m, term)
    return m.comando(orden, tempo)


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h273", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(3)
        verificar("[PERMISOS] sesion como sagui" in m.serial(), "la sesión es sagui")
        term = ident(m, "Terminal")

        # ===================== servidor: pestaña «Escritorio remoto» de Configuración
        n = lanzar_fondo(m, "configuracion --ventana")
        cfg = ident(m, "Configuraci.n", n)
        g = re.findall(r'janela \d+ criada por \d+: "Configuraci.n" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[n:])[-1]
        v = dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + BORDA, y=int(g[3]) + TITULO)
        verificar(v["l"] >= 780, f"la ventana de Configuración ensanchada ({v['l']} px)")
        time.sleep(1)
        clic(m, v, *pestana(v))
        esperar(m, r"configuracion: pagina Escritorio remoto", 10, n)
        time.sleep(1)
        m.captura("H273-1-servidor-parado")

        # la contraseña se fija con «Fijar contraseña» y el diálogo de permisos (root elige la suya)
        m.digitar(CLAVE_VNC, 0.05)
        pos = len(m.serial())
        clic(m, v, 294 + 20, Y_PAG + 122 + 14)
        conceder(m, pos, True)
        time.sleep(3)
        m.captura("H273-2-contrasena-fijada")
        verificar(CLAVE_VNC not in m.serial()[pos:], "la contraseña de VNC no aparece en la serie")

        pos = len(m.serial())
        clic(m, v, 24 + 20, Y_PAG + 58 + 14)   # Activar y arrancar (permiso recordado: sin diálogo)
        esperar(m, r"vncd: escuchando en el puerto 5900|vncd.*escuchando", 40, pos)
        time.sleep(4)
        m.captura("H273-3-servidor-activo")
        out = comando(m, term, "servicio estado vncd")
        verificar("marcha" in out, f"el servicio vncd está en marcha ({out!r})")
        out = m.comando("ls /etc/vnc", 20)
        verificar("vncd.passwd" in out, f"la contraseña quedó en la configuración del sistema ({out!r})")

        # «Solo ver» y «Escuchar»: se guardan en /etc/vnc/vncd.conf y reinician el servicio
        def alternar(cx, cy, esperado, nombre):
            foco(m, cfg)
            clic(m, v, cx, cy)
            time.sleep(9)
            o = comando(m, term, "cat /etc/vnc/vncd.conf")
            verificar(esperado in o, f"{nombre}: {esperado} guardado ({o!r})")
        alternar(24 + 20, Y_PAG + 170 + 14, "solo_ver=si", "Solo ver")
        alternar(284 + 20, Y_PAG + 170 + 14, "escuchar=red-local", "Escuchar")
        foco(m, cfg)
        time.sleep(1)
        m.captura("H273-4-solo-ver-y-red-local")
        alternar(24 + 20, Y_PAG + 170 + 14, "solo_ver=no", "Solo ver")
        alternar(284 + 20, Y_PAG + 170 + 14, "escuchar=local", "Escuchar")
        out = comando(m, term, "servicio estado vncd")
        verificar("marcha" in out, "tras los reinicios el servicio sigue en marcha")

        # ===================== cliente: ventana de conexión
        n = lanzar_fondo(m, "vnc")
        esperar(m, r"vnc: menu lista=", 30, n)
        linea = re.findall(r"vnc: menu (.*)", m.serial()[n:])[-1]
        pos_ = {k: tuple(int(a) for a in v_.split(",")) for k, v_ in re.findall(r"(\w+)=\(([-\d,]+)\)", linea)}
        lx, ly = pos_["lista"][:2]
        vn1 = ident(m, "Escritorio remoto \\(VNC\\)", n)
        v = ventana(m, "Escritorio remoto (VNC)", n)
        time.sleep(1)
        m.captura("H273-5-vnc-ventana-conexion")
        # formulario: equipo (ya con el foco), nombre, contraseña, Recordar
        m.digitar("127.0.0.1", 0.05)
        clic(m, v, pos_["equipo"][0] + 20, pos_["equipo"][1] - 56 + 10)     # la caja del nombre está 56 px por encima
        m.digitar("local", 0.05)
        clic(m, v, pos_["clave"][0] + 20, pos_["clave"][1] + 10)
        m.digitar(CLAVE_VNC, 0.05)
        clic(m, v, pos_["recordar"][0] + 6, pos_["recordar"][1] + 6)
        time.sleep(0.5)
        m.captura("H273-6-formulario-relleno")
        clic(m, v, pos_["guardar"][0] + 20, pos_["guardar"][1] + 14)
        time.sleep(1)
        out = comando(m, term, "cat /home/.config/vnc.conf")
        foco(m, vn1)
        m.captura("H273-7-conexion-guardada")
        verificar(re.search(r"^local\|127\.0\.0\.1\|5900\|32\|-1\|0\|0\|[0-9a-f]{16}\s*$", out, re.M),
                  f"vnc.conf lleva la conexión con la clave ofuscada ({out!r})")
        verificar(CLAVE_VNC not in out, "la contraseña no está en claro en vnc.conf")
        m.tecla("alt-f4")
        esperar(m, r"vnc: cerrado", 10, n)

        # reabrir: la conexión sale en la lista; elegirla y Conectar
        n = lanzar_fondo(m, "vnc")
        esperar(m, r"vnc: menu lista=", 30, n)
        vn2 = ident(m, "Escritorio remoto \\(VNC\\)", n)
        v = ventana(m, "Escritorio remoto (VNC)", n)
        time.sleep(1)
        clic(m, v, lx + 40, ly + 10)       # primera fila
        time.sleep(0.5)
        m.captura("H273-8-lista-con-conexion")
        pos = len(m.serial())
        clic(m, v, pos_["conectar"][0] + 40, pos_["conectar"][1] + 14)     # Conectar
        esperar(m, r"vnc: conectado, escritorio (\d+)x(\d+)", 40, pos)
        time.sleep(4)
        m.captura("H273-9-sesion-con-barra")
        s = m.serial()[pos:]
        verificar("127.0.0.1:5900" in s, "conectó con lo guardado")
        verificar("contraseña incorrecta" not in s, "la contraseña recordada sirvió")

        # servidor: --estado, ventana indicadora y la pestaña
        out = comando(m, term, "vncd --estado")
        verificar("127.0.0.1" in out and "sesion" in out, f"vncd --estado lista al cliente ({out!r})")
        ind = ident(m, "Escritorio remoto", pos)
        verificar(en_barra(m, ind), "la ventana indicadora está en la barra de tareas mientras hay un cliente")
        foco(m, ind)
        time.sleep(1)
        m.captura("H273-10-indicador")
        foco(m, cfg)
        time.sleep(2)
        m.captura("H273-11-pestana-conectados")
        g = re.findall(r'janela \d+ criada por \d+: "Configuraci.n" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
        vcfg = dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + BORDA, y=int(g[3]) + TITULO)
        clic(m, vcfg, 24 + 100, Y_PAG + 238 + 10)      # primera fila de la lista
        pos = len(m.serial())
        clic(m, vcfg, 24 + 562 + 10 + 40, Y_PAG + 238 + 14)   # Desconectar
        esperar(m, r"vnc: desconectado: ", 30, pos)
        time.sleep(1)
        m.captura("H273-12-desconectado-desde-servidor")

        # el cliente reconecta con «R» (contraseña recordada) y su barra «Desconectar» vuelve a la ventana de conexión
        foco(m, vn2)
        pos = len(m.serial())
        m.tecla("r")
        esperar(m, r"vnc: conectado, escritorio", 40, pos)
        time.sleep(2)
        m.captura("H273-13-reconectado")
        bar_y = v["a"] - 26 + 1 + 12
        clic(m, v, 8 + 76 + 8 + 120 + 8 + 150 + 8 + 50, bar_y)   # Desconectar (cuarto botón)
        esperar(m, r"vnc: desconectado por la persona", 20, pos)
        time.sleep(1)
        m.captura("H273-14-vuelta-a-conexiones")
        out = comando(m, term, "vncd --estado")
        verificar("nadie conectado" in out, f"sin clientes: {out!r}")
        fim = time.time() + 15
        while en_barra(m, ind) and time.time() < fim:
            time.sleep(1)
        verificar(not en_barra(m, ind), "la ventana indicadora se cerró sola al irse el último cliente")
    print("h273: todo bien")


if __name__ == "__main__":
    main()
