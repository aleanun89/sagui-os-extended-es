#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H340 (D340, mejora 098, fase 3) - varias cuentas: inicio de sesión gráfico, Configuración > Usuarios,
carpeta personal por cuenta, cerrar sesión.

Imagen con escritorio, la sesión como sagui (no la de pruebas, que es root), /home en ext2 y SAGUI-CONF,
sin instantánea (lo escrito sigue tras «reiniciar»):

1. Arranque con una sola cuenta sin contraseña: entra directo, como antes (sin pantalla de inicio).
   sagui deja un archivo suyo en /home (secreto.txt).
2. Configuración > Usuarios (configuracion --usuarios): «Nueva cuenta...» con nombre «ana», descripción,
   contraseña dos veces y sin administrador; el diálogo de permisos (primera contraseña de root) lo
   autoriza. /etc/usuarios tiene a ana, /home/ana es de ana con 0700 y sagui no puede entrar.
3. Reinicio: con dos cuentas aparece la pantalla de inicio de sesión. ana con contraseña mala: «incorrecta»;
   con la buena entra. Su /home es /home/ana: ve su carpeta, no los archivos de sagui por ningún nombre
   (/home/secreto.txt, /home/../home/secreto.txt, /home/sagui), no lee /etc/sombra, lo que escribe en
   /home queda en /home/ana, y Configuración guarda su sagui.conf en su carpeta.
4. Cerrar sesión (ejecutar --cerrar-sesion): se cierran sus programas y vuelve la pantalla; entra sagui
   (sin contraseña); no puede leer los archivos de ana.
5. Como administrador (diálogo de permisos con la contraseña de root), Configuración > Usuarios borra a
   ana con sus archivos: ni la cuenta ni /home/ana.
En la serie no aparece ninguna contraseña."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

CLAVE_ROOT = "raiz2468"
CLAVE_ANA = "ana13579"
BORDA, TITULO = 2, 24
Y_PAG = 50


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def orden(m, texto, tempo=60):
    """Escribe una orden en el Terminal y espera a que termine (una marca propia, sea cual sea el prompt)."""
    orden.n += 1
    marca = f"FIN{orden.n:03d}"
    antes = len(m.serial())
    m.digitar(f"{texto}; echo {marca}", 0.04)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def ventana(m, titulo, desde=0):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)',
                   m.serial()[desde:])
    verificar(g, f"se abre la ventana «{titulo}»")
    g = g[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]))


def esperar_ventana(m, titulo, desde, tempo=40):
    esperar(m, rf'janela \d+ criada por \d+: "{re.escape(titulo)}"', tempo, desde)
    time.sleep(0.8)


def clic_en(m, v, x, y):
    """Clic en (x, y) del contenido de la ventana v."""
    m.clique(v["x"] + BORDA + x, v["y"] + TITULO + y)


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2"])
    # ---------------------------------------------------------------- 1 y 2
    with Maquina("h340", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"supervisor en marcha", 120)
        s = m.serial()
        verificar("[SESION] una sola cuenta sin contrasena (sagui): entrada directa" in s,
                  "una sola cuenta sin contraseña: entrada directa, sin pantalla")
        verificar("pantalla de inicio de sesion" not in s, "no aparece la pantalla de inicio de sesión")
        esperar(m, r"\[SAGUI\] terminal: \d+", 30)
        time.sleep(3)
        out = orden(m, "id; echo SECRETO-SAGUI > /home/secreto.txt; ls /home")
        verificar("uid=1000" in out, "el Terminal es sagui")

        pos = len(m.serial())
        m.digitar("configuracion --usuarios", 0.04)
        m.tecla("ret")
        esperar_ventana(m, "Configuración", pos)
        v = ventana(m, "Configuración", pos)
        m.captura("H340-1-usuarios")
        # «Nueva cuenta...»: botón en (500, Y_PAG + 34), 260x28
        pos = len(m.serial())
        clic_en(m, v, 630, Y_PAG + 48)
        esperar_ventana(m, "Nueva cuenta", pos)
        m.digitar("ana", 0.05)
        m.tecla("ret")
        time.sleep(1.0)
        m.digitar("Ana Prueba", 0.05)
        m.tecla("ret")
        time.sleep(1.0)
        m.digitar(CLAVE_ANA, 0.05)
        m.tecla("ret")
        time.sleep(1.0)
        m.digitar(CLAVE_ANA, 0.05)
        m.tecla("ret")
        time.sleep(1.0)
        m.tecla("esc")   # ¿administrador? No
        # Diálogo de permisos: root aún no tiene contraseña: se elige (dos veces)
        esperar(m, r"dialogo de permisos: configuracion \(pid \d+\), primera contraseña", 40, pos)
        time.sleep(1)
        m.captura("H340-2-permisos")
        m.digitar(CLAVE_ROOT, 0.05)
        m.tecla("tab")
        m.digitar(CLAVE_ROOT, 0.05)
        m.tecla("ret")
        g = esperar(m, r"configuracion: usuarios: crear ana -> (-?\d+)", 90, pos)
        verificar(g.group(1) == "0", f"Configuración crea la cuenta ana ({g.group(0)})")
        time.sleep(1)
        m.captura("H340-3-ana-creada")
        m.tecla("alt-f4")
        time.sleep(1)
        out = orden(m, "cat /etc/usuarios; ls -l --color=never /home; ls /home/ana")
        verificar(re.search(r"^ana:1001:1001:/home/ana:/bin/sagsh:Ana Prueba$", out, re.M),
                  "/etc/usuarios: ana:1001:1001:/home/ana")
        verificar(re.search(r"^drwx------ ana +ana .* ana/?$", out, re.M), f"/home/ana es de ana y 0700 ({out[-600:]!r})")
        verificar(re.search(r"ls: .*/home/ana", out) or "acceso" in out.lower() or "permitid" in out,
                  "sagui no puede entrar en /home/ana")
        out = orden(m, "cat /etc/grupos")
        verificar(not re.search(r"admin:\d+:.*ana", out), "ana no es administradora")
        verificar(CLAVE_ANA not in m.serial() and CLAVE_ROOT not in m.serial(), "ninguna contraseña en la serie")
        reiniciar(m)
    # ---------------------------------------------------------------- 3
    with Maquina("h340b", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"\[SESION\] pantalla de inicio de sesion: 2 cuenta\(s\)", 120)
        verificar("entrada directa" not in m.serial(), "con dos cuentas no se entra directo")
        verificar(not re.search(r"\[SAGUI\] terminal: \d+", m.serial()), "sin sesión no se abre el Terminal")
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(2)
        m.captura("H340-4-inicio-de-sesion")
        m.tecla("down")          # sagui -> ana
        pos = len(m.serial())
        m.digitar("mala1234", 0.05)
        m.tecla("ret")
        g = esperar(m, r"inicio de sesion \(pedido \d+\) de uid 1001: (\S+)", 40, pos)
        verificar(g.group(1) == "NEGADO", "contraseña mala: no entra")
        time.sleep(1)
        m.captura("H340-5-contrasena-mala")
        pos = len(m.serial())
        m.digitar(CLAVE_ANA, 0.05)
        m.tecla("ret")
        esperar(m, r"\[SESION\] entra ana \(uid 1001\)", 60, pos)
        esperar(m, r"\[SAGUI\] terminal: \d+", 60, pos)
        verificar("sesion local: uid 1001 gid 1001, carpeta /home/ana" in m.serial()[pos:], "la sesión es ana con /home/ana")
        time.sleep(4)
        m.captura("H340-6-sesion-ana")
        out = orden(m, "id; pwd; cd /home; pwd; ls -a /home")
        verificar("uid=1001(ana)" in out, f"el Terminal es ana ({out[-300:]!r})")
        verificar(re.search(r"^/home$", out, re.M), "ana ve su carpeta en /home")
        verificar("secreto.txt" not in out, "ana no ve los archivos de sagui en /home")
        for ruta in ("/home/secreto.txt", "/home/../home/secreto.txt", "//home/secreto.txt", "/home/sagui/secreto.txt",
                     "/home/ana/secreto.txt"):
            out = orden(m, f"cat {ruta}")
            verificar("SECRETO-SAGUI" not in out, f"ana no lee {ruta}")
        out = orden(m, "ls /home/sagui")
        verificar("SECRETO" not in out and "secreto" not in out, "ana no puede leer /home/sagui")
        out = orden(m, "cat /etc/sombra")
        verificar("pbkdf2" not in out and re.search(r"cat: .*sombra", out), f"ana no lee /etc/sombra ({out[-200:]!r})")
        out = orden(m, "cat /sistema/conf/etc/sombra")
        verificar("pbkdf2" not in out, "ni la de SAGUI-CONF")
        out = orden(m, "echo de-ana > /home/nota.txt; cat /home/nota.txt; configuracion acento 2A6FD8")
        verificar("de-ana" in out, "ana escribe en su /home")
        # ---------------------------------------------------------------- 4
        pos = len(m.serial())
        m.digitar("ejecutar --cerrar-sesion", 0.04)
        m.tecla("ret")
        esperar_ventana(m, "Cerrar sesión", pos)
        m.tecla("ret")   # Sí
        esperar(m, r"\[SESION\] cerrar sesion pedido por pid \d+", 30, pos)
        esperar(m, r"\[SESION\] \d+ programa\(s\) de la sesion cerrados", 30, pos)
        esperar(m, r"\[SESION\] pantalla de inicio de sesion: 2 cuenta\(s\)", 30, pos)
        verificar(re.search(r"processo \d+ \(terminal\) terminou", m.serial()[pos:]), "al cerrar la sesión se cierra su Terminal")
        time.sleep(2)
        m.captura("H340-7-sesion-cerrada")
        m.tecla("up")    # ana -> sagui (sin contraseña)
        pos = len(m.serial())
        m.tecla("ret")
        esperar(m, r"\[SESION\] entra sagui \(uid 1000\)", 60, pos)
        esperar(m, r"\[SAGUI\] terminal: \d+", 60, pos)
        time.sleep(4)
        out = orden(m, "id; cat /home/secreto.txt; cat /home/ana/nota.txt; ls /home/ana")
        verificar("uid=1000(sagui)" in out, "vuelve a ser sagui")
        verificar("SECRETO-SAGUI" in out, "sagui ve su /home de siempre")
        verificar("de-ana" not in out, "sagui no lee los archivos de ana")
        # ---------------------------------------------------------------- 5
        pos = len(m.serial())
        m.digitar("configuracion --usuarios", 0.04)
        m.tecla("ret")
        esperar_ventana(m, "Configuración", pos)
        v = ventana(m, "Configuración", pos)
        clic_en(m, v, 80, Y_PAG + 34 + 20 + 10)   # la segunda cuenta de la lista: ana
        time.sleep(0.5)
        pos = len(m.serial())
        clic_en(m, v, 630, Y_PAG + 192)             # «Borrar la cuenta...»
        esperar_ventana(m, "Borrar la cuenta", pos)
        m.tecla("ret")   # ¿Borrar la cuenta ana? Sí
        time.sleep(1.5)
        m.tecla("ret")   # ¿Y su carpeta? Sí
        esperar(m, r"dialogo de permisos: configuracion \(pid \d+\)", 40, pos)
        time.sleep(1)
        m.digitar(CLAVE_ROOT, 0.05)
        m.tecla("ret")
        g = esperar(m, r"configuracion: usuarios: borrar ana \(archivos (\d)\) -> (-?\d+)", 90, pos)
        verificar(g.group(1) == "1" and g.group(2) == "0", f"el administrador borra a ana con sus archivos ({g.group(0)})")
        time.sleep(1)
        m.captura("H340-8-ana-borrada")
        m.tecla("alt-f4")
        time.sleep(1)
        out = orden(m, "cat /etc/usuarios; ls /home")
        verificar(not re.search(r"^ana:", out, re.M), "ana ya no está en /etc/usuarios")
        verificar(not re.search(r"^ana$", out, re.M) and "ana " not in out.split("ls /home")[-1], "ni su carpeta")
        verificar(CLAVE_ANA not in m.serial() and CLAVE_ROOT not in m.serial() and "mala1234" not in m.serial(),
                  "ninguna contraseña en la serie")
    print("H340 OK")


def reiniciar(m):
    """«reiniciar» en el Terminal: el QEMU de las pruebas sale al reiniciar (-no-reboot)."""
    m.digitar("reiniciar", 0.04)
    m.tecla("ret")
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, "reinicia")


if __name__ == "__main__":
    main()
