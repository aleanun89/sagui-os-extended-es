#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H422 (D422) - energía y pantalla: salvapantallas, bloqueo y apagado de la pantalla por inactividad, batería y
«no molestar».

  unidad    tests/ocio_unidad.c en el PC (con ASan/UBSan si hay): los cuatro efectos del salvapantallas dibujan,
            son deterministas, no escriben fuera del lienzo ni con 20 000 cuadros, ni en tamaños raros;
  sim       QEMU con la sesión de sagui y el driver de frecuencia simulado. La inactividad se simula con
            «energia simular» (la cuenta real son minutos). Salvapantallas: sale a su hora, la primera tecla solo
            lo quita (no llega al Terminal); los cuatro efectos con captura. Bloqueo: la cuenta sin contraseña no
            se bloquea; con contraseña se bloquea, pide y entra (también «al salir del salvapantallas»).
            Pantalla apagada (en negro) con batería y enchufado, con valores distintos. «No molestar» de un
            programa (y se suelta al acabar). Batería simulada: aviso de baja y crítica, perfil del gobernador al
            enchufar y desenchufar, apagado ordenado al llegar a crítico (y su cancelación). Configuración >
            Energía y pantalla con capturas, y un cambio que guarda /etc/energia.conf pidiendo permisos.

Uso: h422_energia.py [unidad] [sim]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, RAIZ  # noqa: E402
import h323_conf_aceleracion as h323  # noqa: E402
import conf_arbol as ca  # noqa: E402

PW = h323.CLAVE      # la contraseña de root (la misma que usan los diálogos de permisos de las pruebas)
CL = "sagui135"      # la de sagui


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def orden(m, texto, tempo=90):
    orden.n += 1
    marca = f"FIN-{orden.n:03d}"
    antes = len(m.serial())
    m.digitar(f"{texto}; echo FIN-'{orden.n:03d}'", 0.03)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}\b", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def adm(m, texto, tempo=90):
    return orden(m, f"echo {PW} | admin {texto}", tempo)


def conf(m, **kv):
    """Escribe /etc/energia.conf (como root) y se lo dice al kernel."""
    lineas = "".join(f"{k}={v}\\n" for k, v in kv.items())
    orden(m, f"printf '{lineas}' > /tmp/e.conf")
    adm(m, "cp /tmp/e.conf /etc/energia.conf")
    out = orden(m, "energia recargar")
    verificar("recargar: hecho" in out, "energia recargar")


def simular(m, *tramos):
    """Suma segundos de inactividad como administrador, en tramos con 3 s entre uno y otro, todo desde un guion: lo
    tecleado reinicia la cuenta, así que entre tramo y tramo no puede haber teclas."""
    guion = "".join(("sleep 3\\n" if i else "") + f"energia simular {t}\\n" for i, t in enumerate(tramos))
    orden(m, f"printf '{guion}' > /tmp/s.sh")
    adm(m, "sh /tmp/s.sh")


def estado(m):
    out = orden(m, "energia")
    g = re.search(r"energia: estado (.+?) inactivo (\d+) s no_molestar (\d+) bloqueada (\d+)", out)
    verificar(g, f"«energia» da su estado ({out[-300:]!r})")
    return g.group(1), int(g.group(2)), int(g.group(3)), int(g.group(4)), out


def _leer_png(p):
    """Un lector mínimo de PNG de 8 bits RGB/RGBA (lo que dan las capturas): (ancho, alto, bytes de píxel, bytes por píxel)."""
    import struct
    import zlib
    d = open(p, "rb").read()
    i, idat = 8, b""
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            l, a, _, tipo = struct.unpack(">IIBB", c[:10])
        elif t == b"IDAT":
            idat += c
    bpp = {2: 3, 6: 4, 0: 1, 4: 2}[tipo]
    raw, st, out, prev, k = zlib.decompress(idat), l * bpp, bytearray(), bytearray(l * bpp), 0
    for _ in range(a):
        f = raw[k]
        lin = bytearray(raw[k + 1:k + 1 + st])
        k += 1 + st
        for x in range(st):
            esq = lin[x - bpp] if x >= bpp else 0
            ar = prev[x]
            ae = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                lin[x] = (lin[x] + esq) & 255
            elif f == 2:
                lin[x] = (lin[x] + ar) & 255
            elif f == 3:
                lin[x] = (lin[x] + (esq + ar) // 2) & 255
            elif f == 4:
                pp = esq + ar - ae
                pa, pb, pc = abs(pp - esq), abs(pp - ar), abs(pp - ae)
                lin[x] = (lin[x] + (esq if pa <= pb and pa <= pc else ar if pb <= pc else ae)) & 255
        out += lin
        prev = lin
    return l, a, bytes(out), bpp


def encendidos(png):
    """Cuántos píxeles de la captura no son negros (el cursor del ratón son unos 80)."""
    l, a, px, bpp = _leer_png(png)
    return sum(1 for i in range(0, len(px), bpp) if px[i] | px[i + 1] | px[i + 2])


def contar(m, patron):
    return len(re.findall(patron, m.serial()))


def despertar(m, tecla="q"):
    pos = len(m.serial())
    m.tecla(tecla)
    esperar(m, r"\[OCIO\] despierta", 15, pos)
    time.sleep(1)


def sin_excepciones(m, que):
    s = m.serial()
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones")


# ------------------------------------------------------------------ unidad

def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "ocio_unidad")
    base = ["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Ikernel/inc", "-o", exe, "tests/ocio_unidad.c"]
    r = subprocess.run(base[:1] + ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"] + base[1:], capture_output=True, text=True, cwd=RAIZ)
    if r.returncode != 0:   # sin las bibliotecas de los sanitizadores
        r = subprocess.run(base, capture_output=True, text=True, cwd=RAIZ)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "ocio_unidad: OK" in r.stdout, "efectos del salvapantallas: dibujan, deterministas, sin salirse")


# ------------------------------------------------------------------ sim

def sim():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2", "--frecuencia-simulada"])
    with Maquina("h422", "ide", imagem=img) as m:
        esperar(m, r"\[SAGUI\] terminal: \d+", 200)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(4)
        # --- estado de serie
        g, ino, nm, blq, out = estado(m)
        verificar(g == "activo" and "configuracion por defecto" in out, "de serie: activo y sin /etc/energia.conf")
        verificar("salvapantallas 0/10 min bloqueo 0/15 min pantalla bateria 10 red 30" in out,
                  "de serie: salvapantallas y bloqueo apagados, pantalla a los 10/30 min")
        # root necesita su primera contraseña para escribir /etc y simular
        desde = len(m.serial())
        m.digitar("admin id", 0.04); m.tecla("ret")
        for p in ("Contraseña nueva:", "Repítela:"):
            esperar(m, re.escape(p), 60, desde)
            desde = len(m.serial())
            m.digitar(PW, 0.04); m.tecla("ret")
        time.sleep(3)
        out = orden(m, "energia simular 5")
        verificar("necesita permisos de administrador" in out, "simular, sin ser administrador, se niega")
        out = adm(m, "energia simular 5")
        verificar("simular: hecho" in out, "simular, como administrador")

        # --- salvapantallas
        conf(m, salvapantallas=1, salvapantallas_min=1, pantalla_red_min=0, pantalla_bateria_min=0)
        g, ino, nm, blq, out = estado(m)
        verificar("salvapantallas 1/1 min" in out and "leida" in out, "la configuración se lee")
        pos = len(m.serial())
        simular(m, 20, 50)
        esperar(m, r"\[OCIO\] salvapantallas: efecto 1", 20, pos)
        s = m.serial()[pos:]
        verificar(s.index("+20 s de inactividad") < s.index("+50 s de inactividad") < s.index("salvapantallas: efecto 1"),
                  "con 20 s aún no, con 70 s (1 min) sí")
        time.sleep(1.5)
        a = m.captura("H422-salvapantallas-estrellas")
        time.sleep(1)
        b = m.captura("H422-salvapantallas-estrellas-2", guardar=False)
        ba = encendidos(a)
        verificar(300 < ba < 40000, f"el salvapantallas dibuja estrellas sobre negro ({ba} píxeles encendidos)")
        verificar(open(a, "rb").read() != open(b, "rb").read(), "el salvapantallas se mueve")
        despertar(m)
        out = orden(m, "echo OK1")
        verificar("OK1" in out and "qecho" not in out, "la primera tecla solo despierta: no llega al Terminal")
        verificar(estado(m)[0] == "activo", "tras despertar, activo")
        # los cuatro efectos (probar los muestra ya)
        for ef, nombre in ((2, "logotipo"), (3, "tuberias"), (4, "reloj")):
            pos = len(m.serial())
            orden(m, f"energia probar {ef}")
            esperar(m, rf"\[OCIO\] salvapantallas: efecto {ef}", 15, pos)
            time.sleep(4 if ef == 3 else 1.5)
            png = m.captura(f"H422-salvapantallas-{nombre}")
            verificar(encendidos(png) > 300, f"efecto {nombre}: dibuja")
            despertar(m)
        pos = len(m.serial())
        orden(m, "energia probar 0")
        esperar(m, r"\[OCIO\] salvapantallas: efecto 0", 15, pos)
        time.sleep(1)
        verificar(encendidos(m.captura("H422-negro", guardar=False)) < 150, "efecto 0: pantalla negra")
        despertar(m)

        # --- bloqueo con la cuenta sin contraseña: no bloquea
        conf(m, salvapantallas=0, bloqueo=1, bloqueo_min=1, pantalla_red_min=0, pantalla_bateria_min=0)
        pos = len(m.serial())
        adm(m, "energia simular 70")
        esperar(m, r"\[OCIO\] no se bloquea: la cuenta sagui no tiene contrase", 20, pos)
        time.sleep(1)
        verificar("[SESION] pantalla bloqueada" not in m.serial()[pos:], "sin contraseña, no se bloquea")
        g, ino, nm, blq, out = estado(m)
        verificar(blq == 0 and g == "activo", "sin contraseña: sigue activo y sin bloqueo")

        # --- pantalla apagada (enchufado)
        conf(m, salvapantallas=0, bloqueo=0, pantalla_red_min=2, pantalla_bateria_min=0)
        pos = len(m.serial())
        simular(m, 110, 20)
        esperar(m, r"\[OCIO\] pantalla apagada", 20, pos)
        s = m.serial()[pos:]
        verificar(s.index("+110 s de inactividad") < s.index("+20 s de inactividad") < s.index("pantalla apagada (en negro)"),
                  "a los 110 s aún encendida, a los 130 s (2 min) apagada")
        time.sleep(1)
        verificar(encendidos(m.captura("H422-pantalla-apagada", guardar=False)) < 150, "la pantalla apagada está en negro")
        despertar(m)
        out = orden(m, "echo OK2")
        verificar("OK2" in out and "qecho" not in out, "la tecla que despierta la pantalla no llega al Terminal")

        # --- «no molestar» de un programa
        pos = len(m.serial())
        m.comando("energia nomolestar 45 &")
        esperar(m, r"\[OCIO\] no molestar: pide el pid", 20, pos)
        n0 = contar(m, r"\[OCIO\] pantalla apagada")
        pos2 = len(m.serial())
        simular(m, 600, 600)
        verificar("pantalla apagada" not in m.serial()[pos2:] and contar(m, r"\[OCIO\] pantalla apagada") == n0,
                  "con «no molestar», 20 minutos de inactividad no apagan la pantalla")
        g, ino, nm, blq, out = estado(m)
        verificar(g == "activo" and nm == 1 and contar(m, r"\[OCIO\] pantalla apagada") == n0,
                  f"con «no molestar» ni se apaga la pantalla ni corre la inactividad ({g}, nm {nm}, {ino} s)")
        esperar(m, r"\[OCIO\] no molestar: suelta el pid", 70, pos)
        time.sleep(1)
        verificar(estado(m)[2] == 0, "al acabar el programa, «no molestar» se suelta")
        pos = len(m.serial())
        adm(m, "energia simular 130")
        esperar(m, r"\[OCIO\] pantalla apagada", 20, pos)
        despertar(m)
        # un programa que acaba sin soltarlo: lo suelta el kernel
        pos = len(m.serial())
        orden(m, "energia nomolestar 3")
        esperar(m, r"\[OCIO\] no molestar: suelta el pid", 20, pos)

        # --- batería simulada
        conf(m, salvapantallas=0, bloqueo=0, pantalla_red_min=0, pantalla_bateria_min=1, bateria_aviso=20, bateria_critica=8,
             bateria_accion=0, gobernador_bateria=1, gobernador_red=0)
        pos = len(m.serial())
        out = adm(m, "energia bateria 50 desenchufada")
        verificar("bateria: hecho" in out, "batería simulada")
        esperar(m, r"\[OCIO\] gobernador: perfil ahorro \(con bateria\) -> 0", 20, pos)
        esperar(m, r"\[FREQ\] politica: ahorro", 10, pos)
        out = orden(m, "energia")
        verificar("bateria si (simulada) pct 50 desenchufada" in out, "el estado enseña la batería simulada")
        pos = len(m.serial())
        adm(m, "energia simular 70")
        esperar(m, r"\[OCIO\] pantalla apagada", 20, pos)
        despertar(m)
        pos = len(m.serial())
        adm(m, "energia bateria 18 desenchufada")
        esperar(m, r"\[OCIO\] bateria baja: 18 %", 20, pos)
        adm(m, "energia bateria 6 desenchufada")
        esperar(m, r"\[OCIO\] bateria critica: 6 %", 20, pos)
        time.sleep(3)
        verificar("apagado ordenado" not in m.serial()[pos:], "sin la opción, la batería crítica solo avisa")
        pos = len(m.serial())
        adm(m, "energia bateria 6 enchufada")
        esperar(m, r"\[OCIO\] gobernador: perfil rendimiento \(enchufado\) -> 0", 20, pos)
        esperar(m, r"\[FREQ\] politica: rendimiento", 10, pos)
        # con la acción: se puede cancelar enchufando
        conf(m, salvapantallas=0, bloqueo=0, pantalla_red_min=0, pantalla_bateria_min=0, bateria_aviso=20, bateria_critica=8,
             bateria_accion=1, gobernador_bateria=3, gobernador_red=3)
        adm(m, "energia bateria 50 desenchufada")
        time.sleep(4)
        pos = len(m.serial())
        adm(m, "energia bateria 6 desenchufada")
        esperar(m, r"\[OCIO\] bateria critica: 6 %", 20, pos)
        adm(m, "energia bateria 6 enchufada")
        esperar(m, r"\[OCIO\] apagado por bateria cancelado", 20, pos)
        time.sleep(1)
        sin_excepciones(m, "batería")

        # --- bloqueo con contraseña
        adm(m, "energia bateria real")
        out = orden(m, f"printf '{CL}\\n{CL}\\n' > /tmp/p; passwd < /tmp/p; rm /tmp/p")
        verificar("contraseña de sagui cambiada" in out, f"sagui elige su contraseña ({out[-200:]!r})")
        conf(m, salvapantallas=0, bloqueo=1, bloqueo_min=1, pantalla_red_min=0, pantalla_bateria_min=0, bateria_accion=0,
             gobernador_bateria=3, gobernador_red=3)
        pos = len(m.serial())
        adm(m, "energia simular 70")
        esperar(m, r"\[OCIO\] bloqueo por inactividad \(1 min\)", 20, pos)
        esperar(m, r"\[SESION\] pantalla bloqueada \(sagui\)", 20, pos)
        time.sleep(1)
        m.captura("H422-bloqueo-inactividad")
        pos = len(m.serial())
        m.digitar("mala999", 0.05); m.tecla("ret")
        esperar(m, r"inicio de sesion \(pedido \d+\) de uid 1000: NEGADO", 30, pos)
        verificar("pantalla desbloqueada" not in m.serial()[pos:], "con la contraseña mala sigue bloqueada")
        time.sleep(3)
        m.digitar(CL, 0.05); m.tecla("ret")
        esperar(m, r"\[SESION\] pantalla desbloqueada", 30, pos)
        time.sleep(1)
        out = orden(m, "id")
        verificar("uid=1000(sagui)" in out, "tras desbloquear, el Terminal sigue ahí")
        # «al salir del salvapantallas»
        conf(m, salvapantallas=2, salvapantallas_min=1, bloqueo=2, pantalla_red_min=0, pantalla_bateria_min=0)
        pos = len(m.serial())
        adm(m, "energia simular 70")
        esperar(m, r"\[OCIO\] salvapantallas: efecto 2", 20, pos)
        verificar("pantalla bloqueada" not in m.serial()[pos:], "con el salvapantallas en marcha aún no se ha bloqueado")
        pos = len(m.serial())
        m.tecla("q")
        esperar(m, r"\[OCIO\] bloqueo al salir del salvapantallas", 20, pos)
        esperar(m, r"\[SESION\] pantalla bloqueada \(sagui\)", 20, pos)
        time.sleep(1.5)
        m.captura("H422-bloqueo-al-salir")
        m.digitar(CL, 0.05); m.tecla("ret")
        esperar(m, r"\[SESION\] pantalla desbloqueada", 30, pos)
        time.sleep(1)
        out = orden(m, "echo OK3")
        verificar("OK3" in out, "tras el bloqueo al salir, el Terminal responde")
        sin_excepciones(m, "bloqueo")

        # --- Configuración > Energía y pantalla
        conf(m, salvapantallas=2, salvapantallas_min=5, bloqueo=1, bloqueo_min=10, pantalla_red_min=30, pantalla_bateria_min=10)
        n = len(m.serial())
        m.comando("configuracion --ventana &", 20)
        esperar(m, r'"Configuraci.n"', 30, n)
        time.sleep(1.5)
        v = ca.ventana(m, n)
        ca.ir(m, v, "energia")
        esperar(m, r"configuracion: pantalla vista energia salvapantallas=2/5 bloqueo=1/10 pantalla=10/30", 15, n)
        time.sleep(1.5)
        m.captura("H422-energia-1")
        ca.al_final(m, v)
        time.sleep(1)
        m.captura("H422-energia-2")
        ca.al_inicio(m, v)
        time.sleep(1)
        # clic en «Estrellas» (segunda opción de la primera fila): cambia, pide permisos y guarda
        px, py = ca.panel(v, 30 + 190 + 8, 26 + 6)
        pos = len(m.serial())
        m.clique(px, py)
        h323.conceder(m, pos)
        esperar(m, r"configuracion: energia guardado 0", 30, pos)
        time.sleep(1)
        m.captura("H422-energia-3")
        h323.cerrar_ventana(m, v)
        time.sleep(1)
        out = orden(m, "energia")
        verificar("salvapantallas 1/5 min bloqueo 1/10 min" in out, f"Configuración guardó el salvapantallas en /etc/energia.conf ({out[-400:]!r})")
        out = orden(m, "cat /etc/energia.conf")
        verificar("salvapantallas=1" in out and "bloqueo_min=10" in out, "el archivo de la máquina lleva lo guardado")
        sin_excepciones(m, "Configuración")

        # --- apagado ordenado por batería crítica (lo último: apaga la máquina)
        conf(m, salvapantallas=0, bloqueo=0, pantalla_red_min=0, pantalla_bateria_min=0, bateria_aviso=20, bateria_critica=8,
             bateria_accion=1, gobernador_bateria=3, gobernador_red=3)
        adm(m, "energia bateria 50 desenchufada")
        time.sleep(4)
        pos = len(m.serial())
        adm(m, "energia bateria 5 desenchufada")
        esperar(m, r"\[OCIO\] bateria critica: apagado ordenado", 60, pos)
        verificar(True, "batería crítica con la acción: apagado ordenado a los 30 s")


def main():
    partes = sys.argv[1:] or ["unidad", "sim"]
    if "unidad" in partes:
        unidad()
    if "sim" in partes:
        sim()
    print("H422 OK")


if __name__ == "__main__":
    main()
