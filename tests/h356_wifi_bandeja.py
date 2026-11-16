#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H356 (D354) - la ventana de redes Wi-Fi (icono de la bandeja) y conectar sin ser root.

QEMU no tiene el pincho RTL8188EU: la ventana se prueba con `wifi --ventana --simulado` (cinco redes de
mentira y un punto de acceso que acepta la contraseña «sagui1234»); lo de verdad (permisos del kernel,
el clic de la bandeja, las redes recordadas por usuario) se mira en las fuentes y con el programa de
pruebas /testes/usuarios.

  1. fuentes: permisos.c deja WIFI_CONECTAR y WIFI_DESCONECTAR a quien no es un servicio; barra.c abre
     `wifi --ventana` al hacer clic en el icono Wi-Fi; el kernel lee la lista recordada de la sesión
     (wifi.c); catálogos pt_BR y en completos.
  2. QEMU (escritorio, sesión como sagui): /testes/usuarios --llamadas (conectar/desconectar libres para
     la sesión local, sin ERRO_ACESSO); clic en el icono de red (sin Wi-Fi: sigue el panel de la red);
     la ventana simulada: lista con las cinco redes, contraseña mala (error), buena (se guarda en
     /home/.config/wifi.conf con la PMK correcta), «Mostrar», desconectar, reconectar sin contraseña
     (recordada), olvidar.
El «servicio no root es negado» lo vigila h254 (usuarios --servicio)."""
import hashlib
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, imagem_de_teste  # noqa: E402

BORDA, TITULO = 2, 24


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def esperar(m, patron, tempo=40, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def fuentes():
    perm = ler("kernel/nucleo/permisos.c")
    verificar(re.search(r"a == WIFI_CONECTAR \|\| a == WIFI_DESCONECTAR\) && !eu->de_servico", perm),
              "permisos.c: conectar y desconectar, para quien no es un servicio")
    barra = ler("kernel/gui/barra.c")
    verificar("bandeja_wifi()" in barra and '"--ventana"' in barra and 'gui_abrir_unico("wifi"' in barra,
              "barra.c: el clic en el icono Wi-Fi abre la ventana de redes")
    verificar("permisos_sessao_caminho" in ler("kernel/rede/wifi.c"), "wifi.c: las recordadas de la sesión")
    verificar("--ventana" in ler("userland/programas/wifi.c"), "wifi.c: --ventana")
    for lang in ("pt_BR", "en"):
        po = ler(f"idiomas/{lang}/wifi.po")
        vacios = re.findall(r'msgid "(.+)"\nmsgstr ""\n', po)
        verificar(not vacios, f"{lang}/wifi.po completo ({vacios[:2]})")
    usu = ler("userland/testes/usuarios.c")
    verificar("livre(SIS_WIFI, WIFI_CONECTAR" in usu and "conectar el Wi-Fi desde un servicio" in usu,
              "usuarios.c prueba sesión local (libre) y servicio (negado)")
    print("OK fuentes")


def ventana(m, titulo, desde=0):
    g = re.findall(r'janela (\d+) criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), m.serial()[desde:])
    verificar(g, f"ventana «{titulo}» creada")
    g = g[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def clic(m, v, cx, cy, **kw):
    m.clique(v["x"] + cx, v["y"] + cy, **kw)


def foco(m, id_):
    """Da el foco a una ventana por su botón de la barra de tareas (el teclado va a la que lo tiene)."""
    linha = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    x, l, e = re.findall(rf" {id_}@(\d+)\+(\d+)([fm]?)", linha)[0]
    if e != "f":
        m.clique(int(x) + int(l) // 2, 12)
        time.sleep(0.8)


def term_cmd(m, term, win, orden, tempo=20):
    foco(m, term)
    out = m.comando(orden, tempo)
    foco(m, win)
    return out


def pmk(clave, ssid):
    return hashlib.pbkdf2_hmac("sha1", clave.encode(), ssid.encode(), 4096, 32).hex()


def fila(v, i):
    return 12 + 80, 46 + 2 + 20 * i + 10


def boton(v, n):   # 0 conectar, 1 desconectar, 2 olvidar
    return (12 + 55, v["a"] - 124 + 13) if n == 0 else (130 + 55, v["a"] - 124 + 13) if n == 1 else (248 + 45, v["a"] - 124 + 13)


def maquina():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h356", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(3)
        verificar("[PERMISOS] sesion como sagui" in m.serial(), "la sesión es sagui")
        term = int(re.findall(r'janela (\d+) criada por \d+: "Terminal"', m.serial())[-1])

        # permisos: la sesión local, sin ser root, ya no recibe ERRO_ACESSO al conectar/desconectar
        out = m.comando("/testes/usuarios --llamadas", 90)
        g = re.search(r"usuarios: llamadas ok=(\d+) mal=(\d+)", out)
        verificar(g and int(g[2]) == 0, f"usuarios --llamadas sin fallos ({out[-300:]!r})")

        # el icono de red sin Wi-Fi sigue abriendo el panel de la red (no la ventana)
        r = re.findall(r"\[GUI\] bandeja (\d+)\+(\d+):([^\r\n]*)", m.serial())[-1]
        ic = {n: int(x) for n, x in re.findall(r"(\w+)@(\d+)", r[2])}
        pos = len(m.serial())
        m.clique(ic["red"] + 8, 12)
        esperar(m, r"bandeja: painel rede aberto", 10, pos)
        verificar("ventana de redes" not in m.serial()[pos:], "sin adaptador Wi-Fi el clic abre el panel de la red")
        m.tecla("esc")
        time.sleep(0.5)

        # la ventana (simulada)
        n = len(m.serial())
        m.digitar("wifi --ventana --simulado &", 0.04)
        m.tecla("ret")
        esperar(m, r"wifi: lista 5 redes \(0 recordadas\)", 30, n)
        v = ventana(m, "Redes Wi-Fi", n)
        time.sleep(2)
        m.captura("H356-1-lista-de-redes")

        # contraseña mala: error
        clic(m, v, *fila(v, 0))                 # SaguiNet (la más fuerte)
        clic(m, v, *boton(v, 0))
        esperar(m, r"wifi: pide la contrasena de SaguiNet", 10, n)
        time.sleep(0.5)
        m.captura("H356-2-pide-contrasena")
        m.digitar("mala12345", 0.05)
        m.tecla("ret")
        esperar(m, r"wifi: error al conectar: contrase", 20, n)
        time.sleep(1)
        m.captura("H356-3-contrasena-mala")
        verificar("mala12345" not in m.serial(), "la contraseña no sale en el registro")

        # contraseña buena, con «Mostrar la contraseña»
        clic(m, v, *fila(v, 0))
        clic(m, v, *boton(v, 0))
        esperar(m, r"wifi: pide la contrasena de SaguiNet", 10, n + 1)
        m.digitar("sagui1234", 0.05)
        clic(m, v, 12 + 7, v["a"] - 124 + 58 + 7)   # casilla «Mostrar»
        time.sleep(0.5)
        m.captura("H356-4-contrasena-visible")
        m.tecla("ret")
        esperar(m, r"wifi: conectado a SaguiNet", 30, n)
        time.sleep(1.5)
        m.captura("H356-5-conectada")
        out = term_cmd(m, term, v["id"], "cat /home/.config/wifi.conf")
        hexa = "SaguiNet".encode().hex()
        verificar(re.search(rf"^rede {hexa} {pmk('sagui1234', 'SaguiNet')}\s*$", out, re.M),
                  f"wifi.conf guarda SaguiNet con la PMK correcta ({out!r})")
        verificar("sagui1234" not in out, "la contraseña no está en claro en wifi.conf")
        out = term_cmd(m, term, v["id"], "wifi recordadas")
        verificar("SaguiNet (WPA/WPA2)" in out, f"«wifi recordadas» la lista ({out!r})")

        # desconectar y reconectar sin contraseña (recordada)
        pos = len(m.serial())
        clic(m, v, *boton(v, 1))
        esperar(m, r"wifi: desconectar -> 0", 10, pos)
        time.sleep(1)
        m.captura("H356-6-desconectada")
        pos = len(m.serial())
        clic(m, v, *fila(v, 0))
        clic(m, v, *boton(v, 0))
        esperar(m, r"wifi: conectando a SaguiNet", 10, pos)
        esperar(m, r"wifi: conectado a SaguiNet", 30, pos)
        verificar("pide la contrasena" not in m.serial()[pos:], "la recordada conecta sin pedir la contraseña")

        # red abierta: conecta sin contraseña
        pos = len(m.serial())
        time.sleep(1)
        clic(m, v, *fila(v, 1))                 # CafeLibre
        clic(m, v, *boton(v, 0))
        esperar(m, r"wifi: conectado a CafeLibre", 30, pos)
        verificar("pide la contrasena" not in m.serial()[pos:], "la abierta conecta sin contraseña")
        time.sleep(1)
        m.captura("H356-7-abierta")

        # olvidar SaguiNet
        pos = len(m.serial())
        clic(m, v, *fila(v, 1))                 # la conectada (CafeLibre) va primero: SaguiNet es la segunda
        clic(m, v, *boton(v, 2))
        time.sleep(1.5)
        m.captura("H356-8-confirmar-olvidar")
        m.tecla("ret")
        time.sleep(2)
        out = term_cmd(m, term, v["id"], "cat /home/.config/wifi.conf")
        verificar(hexa not in out, f"SaguiNet ya no está en wifi.conf ({out!r})")
        esperar(m, r"wifi: olvidada SaguiNet", 5, pos)
        m.captura("H356-9-olvidada")
    print("OK ventana")


def main():
    fuentes()
    maquina()
    print("H356 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
