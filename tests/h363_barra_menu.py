#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H363 (D361) - barra de tareas y menú Inicio personalizables.

Criterio (imagen gráfica con /home persistente, 1024x768):
  - la barra de serie es la de siempre (arriba, 24 px, color oscuro) pero sin el lema ni la versión del sistema;
    «Acerca de» (sobre-o-sistema) sigue mostrando la versión;
  - los estilos del menú Inicio (Clásico, Dos columnas, Moderno) se eligen con `configuracion barra inicio_menu=N`
    y se ven en capturas (docs/capturas/H363-*.png): franja lateral, cabecera con usuario y columnas, y el buscador
    del Moderno (se filtra al teclear, arriba o abajo, translúcido o con degradado);
  - la barra abajo: el rectángulo de la barra, el menú Inicio que sube desde ella, y la altura y el «ocultar sola»
    cambian el área de las ventanas maximizadas (el kernel pide el tamaño nuevo);
  - ocultar sola: aparece al llevar el ratón al borde y se va al dejarla;
  - reloj, segundos, bandeja, rótulos, agrupar, botón Inicio y tema de color cambian lo que dibuja la barra;
  - desde la ventana «Configuración» (Escritorio > Barra y menú Inicio) se cambia la posición y el estilo con clics;
  - se guarda en sagui.conf y el arranque lo aplica de nuevo (reinicio con la misma imagen).
"""
import os
import re
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, RAIZ, SAIDA, opcoes_apps  # noqa: E402

BARRA = (0x18, 0x20, 0x2A)
DADOS_MB = 16
L, A = 1024, 768
LB_X0, LB_Y0, LB_PASSO, Y_PAG = 176, 84, 22, 50   # como configuracion.c


def imagem_persistente(nome):
    b = os.path.join(RAIZ, "build")
    os.makedirs(SAIDA, exist_ok=True)
    destino = os.path.join(SAIDA, nome + ".img")
    cmd = ["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
           "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
           "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
           "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--dados-mb", str(DADOS_MB),
           *opcoes_apps()]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return destino


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


_png_cache = {}


def _ler_png(caminho):
    """Lee un PNG de 8 bits (RGB, RGBA o gris) sin ImageMagick: (ancho, alto, bytes RGB)."""
    chave = (caminho, os.path.getmtime(caminho))
    if chave in _png_cache:
        return _png_cache[chave]
    d = open(caminho, "rb").read()
    i, idat = 8, b""
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            larg, alt, _prof, tipo = struct.unpack(">IIBB", c[:10])
        elif t == b"IDAT":
            idat += c
    bpp = {2: 3, 6: 4, 0: 1, 4: 2}[tipo]
    raw, st, k = zlib.decompress(idat), larg * bpp, 0
    prev, rgb = bytearray(st), bytearray()
    for _ in range(alt):
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
        prev = lin
        for j in range(larg):
            px = lin[j * bpp:(j + 1) * bpp]
            rgb += bytes(px[:3]) if bpp >= 3 else bytes([px[0]] * 3)
    _png_cache[chave] = (larg, alt, bytes(rgb))
    return _png_cache[chave]


def regiao(png, x, y, l, a):
    """Cuenta los colores de una región de la captura: {(r, g, b): n}."""
    larg, alt, rgb = _ler_png(png)
    cont = {}
    for j in range(y, min(y + a, alt)):
        for i in range(x, min(x + l, larg)):
            c = tuple(rgb[(j * larg + i) * 3:(j * larg + i) * 3 + 3])
            cont[c] = cont.get(c, 0) + 1
    return cont


def perto(a, b, tol):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def mostrar(m):
    out = m.comando("configuracion barra mostrar")
    v = dict(re.findall(r"^(\w+)=(\d+)$", out, re.M))
    g = re.search(r"visible (\d+) en \((-?\d+),(-?\d+)\) (\d+)x(\d+); menu Inicio abierto (\d+) en \((-?\d+),(-?\d+)\) (\d+)x(\d+)", out)
    verificar(g is not None, f"barra mostrar da el estado ({out.strip()[-120:]})")
    n = [int(x) for x in g.groups()]
    return {k: int(x) for k, x in v.items()}, dict(visible=n[0], x=n[1], y=n[2], l=n[3], a=n[4], menu=n[5],
                                                      mx=n[6], my=n[7], ml=n[8], ma=n[9])


def ajustar(m, *pares, espera=1.0):
    out = m.comando("configuracion barra " + " ".join(pares))
    verificar("barra: aplicada" in out, f"configuracion barra {' '.join(pares)}")
    time.sleep(espera)


def abrir_inicio(m, estilo, nome):
    k = len(m.serial())
    m.clique(30, 12 if m.pos_barra_arriba else A - 12)
    esperar(m, rf"menu Inicio aberto \(mouse, estilo {estilo}\)", 15, k)
    time.sleep(0.8)
    return m.captura(f"H363-{nome}")


def painel(m, desde=0):
    """El rectángulo del panel de columnas abierto, por el log del kernel (con el panel abierto el teclado es suyo)."""
    g = re.findall(r"inicio painel aberto estilo \d em \((-?\d+),(-?\d+)\) (\d+)x(\d+)", m.serial()[desde:])[-1]
    return dict(mx=int(g[0]), my=int(g[1]), ml=int(g[2]), ma=int(g[3]), menu=1)


def solapan(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def comprobar_diseno(m, nome, moderno):
    """El buscador y los botones de energía no se solapan, ni entre sí ni con la lista, y cada uno queda en su columna."""
    g = re.findall(r"inicio diseno busca=\(([-\d,]+)\) lista=\(([-\d,]+)\) dir=\(([-\d,]+)\) botoes=(.*)", m.serial())[-1]
    busca, lista, dire = (tuple(int(v) for v in x.split(",")) for x in g[:3])
    botoes = [tuple(int(v) for v in b.split(",")) for b in re.findall(r"\(([-\d,]+)\)", g[3])]
    verificar(len(botoes) >= 2, f"{nome}: hay botones de energía ({len(botoes)})")
    for i, b in enumerate(botoes):
        verificar(b[2] == b[3] <= 30, f"{nome}: el botón {i} es cuadrado y pequeño ({b[2]}x{b[3]})")
        verificar(not solapan(b, lista), f"{nome}: el botón {i} no pisa la lista")
        for c in botoes[i + 1:]:
            verificar(not solapan(b, c) and abs(b[0] - c[0]) >= b[2] + 4, f"{nome}: los botones no se tocan y tienen separación")
        if moderno:
            verificar(dire[0] <= b[0] and b[0] + b[2] <= dire[0] + dire[2], f"{nome}: el botón {i} está dentro de la columna derecha")
            verificar(not solapan(b, busca), f"{nome}: el botón {i} no pisa el buscador")
    if moderno:
        verificar(lista[0] <= busca[0] and busca[0] + busca[2] <= lista[0] + lista[2] - 4, f"{nome}: el buscador queda dentro de la columna izquierda, con margen")
        verificar(busca[0] + busca[2] < dire[0], f"{nome}: el buscador no invade la columna derecha")


def cfg_log(m, desde):
    """(posición, altura, menú, tema) de la última línea «[GUI] barra: posicion ...» del kernel (la ventana Configuración tiene el foco)."""
    g = re.findall(r"\[GUI\] barra: posicion (\d+) altura (\d+) autoocultar \d+ menu (\d+) tema (\d+)", m.serial()[desde:])[-1]
    return tuple(int(x) for x in g)


def foco(m):
    """Las ventanas nuevas se llevan el foco: se vuelve al Terminal con su botón de la barra (el primero)."""
    m.clique(110, m.cy)
    time.sleep(0.6)


def cerrar_menu(m):
    m.tecla("esc")
    time.sleep(0.6)


def pedido(m, desde):
    g = re.findall(r"pedido de tamanho (\d+)x(\d+)", m.serial()[desde:])
    return (int(g[-1][0]), int(g[-1][1])) if g else None


def principal():
    img = imagem_persistente("h363")
    with Maquina("h363", "ide", imagem=img, snapshot=False) as m:
        m.pos_barra_arriba = True
        m.cy = 12
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        s = m.serial()

        # ---- 1. la barra de serie, sin lema ni versión
        verificar(re.search(r"\[GUI\] barra 1024: inicio 2\+84", s) is not None, "barra de serie: Inicio en 2+84")
        png = m.captura("H363-0-barra-de-serie")
        verificar(cor_do_pixel(png, 300, 2) == BARRA, "barra de serie arriba y oscura")
        cont = regiao(png, 300, 2, 380, 19)
        verificar(set(cont) == {BARRA}, f"sin el lema ni la versión en la barra ({len(cont)} colores entre las tareas y la bandeja)")
        v, r = mostrar(m)
        verificar((r["y"], r["a"], r["visible"]) == (0, 24, 1), f"rectángulo de serie {r}")
        verificar(v["barra_posicion"] == 0 and v["barra_altura"] == 24 and v["inicio_menu"] == 0, "ajustes de serie")
        k = len(m.serial())
        m.comando("sobre-o-sistema &")
        esperar(m, r"sobre: versao \S+", 20, k)
        verificar(True, "«Acerca de» sigue mostrando la versión (sobre: versao ...)")
        time.sleep(1.0)
        foco(m)

        # ---- 2. los estilos del menú Inicio
        ajustar(m, "inicio_menu=1")
        png = abrir_inicio(m, 1, "menu-clasico")
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+)", 5, 0)
        mx, my, ml, ma = (int(x) for x in g)
        verificar(my == 24 and mx == 2, f"clásico: el menú cuelga de la barra ({mx},{my})")
        verificar(cor_do_pixel(png, mx + 60, my + 3) == (0xD4, 0xD0, 0xC8), "clásico: fondo gris clásico")
        c = cor_do_pixel(png, mx + 8, my + ma - 14)
        verificar(c[2] > c[0] + 40 and c[0] < 60, f"clásico: franja lateral azul marino abajo {c}")
        cont = regiao(png, mx + 2, my + 2, 22, ma - 4)
        verificar(cont.get((255, 255, 255), 0) > 60, "clásico: el nombre del sistema en vertical (texto blanco) en la franja")
        cerrar_menu(m)

        ajustar(m, "inicio_menu=2")
        k = len(m.serial())
        png = abrir_inicio(m, 2, "menu-dos-columnas")
        r = painel(m)
        verificar(r["menu"] == 1 and (r["mx"], r["my"]) == (2, 24) and r["ml"] == 470, f"dos columnas: rectángulo {r}")
        mx, my, ml, ma = r["mx"], r["my"], r["ml"], r["ma"]
        c = cor_do_pixel(png, mx + 300, my + 10)
        verificar(c[2] > c[0] + 60, f"dos columnas: cabecera azul {c}")
        verificar(cor_do_pixel(png, mx + 350, my + 120) == (0xD3, 0xE5, 0xFA), "dos columnas: columna de la derecha clara")
        comprobar_diseno(m, "dos columnas", False)
        verificar(cor_do_pixel(png, mx + 20, my + 300) == (0xFF, 0xFF, 0xFF), "dos columnas: columna de programas blanca")
        cont = regiao(png, mx + 8, my + 8, 38, 38)
        verificar(len(cont) > 6, "dos columnas: la imagen de la cuenta (el sagui) en la cabecera")
        # «Todos los programas» (clic) y un grupo
        k = len(m.serial())
        m.clique(mx + 60, my + 54 + 4 * 24 + 9 + 14)
        esperar(m, r"inicio: todos os programas", 10, k)
        time.sleep(0.6)
        m.captura("H363-menu-dos-columnas-todos")
        k = len(m.serial())
        m.clique(mx + 60, my + 54 + 28 + 11)
        esperar(m, r"inicio: grupo 0 aberto", 10, k)
        time.sleep(0.6)
        m.captura("H363-menu-dos-columnas-grupo")
        m.tecla("esc")   # vuelve a los más usados
        time.sleep(0.5)
        k = len(m.serial())
        m.tecla("down")
        m.tecla("ret")
        esperar(m, r"inicio -> /bin/\w+ \(pid \d+\)", 15, k)
        verificar("inicio painel fechado" in m.serial()[k:], "dos columnas: Intro lanza el programa y el panel se cierra")
        time.sleep(1.5)
        foco(m)

        ajustar(m, "inicio_menu=3")
        png = abrir_inicio(m, 3, "menu-moderno")
        r = painel(m)
        verificar(r["menu"] == 1 and r["ml"] == 510, f"moderno: rectángulo {r}")
        mx, my, ml, ma = r["mx"], r["my"], r["ml"], r["ma"]
        verificar(cor_do_pixel(png, mx + 200, my + ma - 15) == (255, 255, 255), "moderno: buscador abajo (caja blanca)")
        comprobar_diseno(m, "moderno (buscador abajo)", True)
        k = len(m.serial())
        m.digitar("calc", 0.12)
        n = int(esperar(m, r"inicio busca 'calc': (\d+) resultados", 10, k))
        verificar(n >= 1, f"moderno: la búsqueda «calc» da {n} resultado(s)")
        time.sleep(0.6)
        m.captura("H363-menu-moderno-busqueda")
        k = len(m.serial())
        m.tecla("ret")
        esperar(m, r"inicio -> /bin/calculadora", 15, k)
        time.sleep(1.5)
        foco(m)
        # sin translúcido y con el buscador arriba
        ajustar(m, "inicio_translucido=0", "inicio_buscador=0")
        png = abrir_inicio(m, 3, "menu-moderno-oscuro")
        r = painel(m)
        mx, my, ma = r["mx"], r["my"], r["ma"]
        verificar(cor_do_pixel(png, mx + 200, my + 18) == (255, 255, 255), "moderno: buscador arriba")
        comprobar_diseno(m, "moderno (buscador arriba)", True)
        c = cor_do_pixel(png, mx + 440, my + 6)
        verificar(perto(c, (0x1E, 0x32, 0x50), 12), f"moderno sin translucidez: degradado oscuro {c}")
        cerrar_menu(m)
        ajustar(m, "inicio_translucido=1")
        abrir_inicio(m, 3, "menu-moderno-translucido")
        cerrar_menu(m)
        ajustar(m, "inicio_menu=0")

        # ---- 3. posición, altura y área de las ventanas maximizadas
        k = len(m.serial())
        m.comando("arquivos &")
        esperar(m, r'criada por \d+: "[^"]*" \d+x\d+ em \(\d+,\d+\)', 20, k)
        time.sleep(1.5)
        g = re.findall(r'criada por \d+: "[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[k:])[-1]
        wx, wy = int(g[2]), int(g[3])
        k = len(m.serial())
        m.clique(wx + 150, wy + 10, duplo=True)   # doble clic en el título: maximiza
        time.sleep(1.5)
        p_arriba = pedido(m, k)
        foco(m)
        verificar(p_arriba is not None and p_arriba[0] == L - 4, f"maximizada con la barra arriba: pide {p_arriba}")
        verificar(p_arriba[1] == A - 24 - 24 - 2, f"... y deja los 24 px de la barra ({p_arriba})")
        k = len(m.serial())
        ajustar(m, "barra_posicion=1")
        m.cy = A - 12
        p = pedido(m, k)
        verificar(p == p_arriba, f"barra abajo: la maximizada se reajusta al mismo tamaño ({p})")
        v, r = mostrar(m)
        verificar((r["y"], r["a"], r["l"]) == (A - 24, 24, L), f"barra abajo: rectángulo {r}")
        png = m.captura("H363-barra-abajo")
        verificar(cor_do_pixel(png, 300, A - 23) == BARRA, "barra abajo: pintada en la parte de abajo")
        verificar(cor_do_pixel(png, 300, 4) != BARRA, "barra abajo: arriba ya no hay barra")
        m.pos_barra_arriba = False
        png = abrir_inicio(m, 0, "menu-sagui-con-barra-abajo")
        g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+)", 5, k)
        verificar(int(g[1]) + int(g[3]) == A - 24, f"barra abajo: el menú sube desde la barra ({g})")
        cerrar_menu(m)
        ajustar(m, "inicio_menu=2")
        abrir_inicio(m, 2, "menu-dos-columnas-con-barra-abajo")
        r = painel(m)
        verificar(r["my"] + r["ma"] == A - 24, f"barra abajo: el panel de dos columnas sube desde la barra ({r})")
        cerrar_menu(m)
        ajustar(m, "inicio_menu=0")
        k = len(m.serial())
        ajustar(m, "barra_altura=40")
        p = pedido(m, k)
        verificar(p == (p_arriba[0], p_arriba[1] - 16), f"altura 40: la maximizada pierde 16 px más ({p})")
        v, r = mostrar(m)
        verificar((r["y"], r["a"]) == (A - 40, 40), f"altura 40: rectángulo {r}")
        m.captura("H363-barra-abajo-alta")
        k = len(m.serial())
        ajustar(m, "barra_altura=24", "barra_autoocultar=1")
        p = pedido(m, k)
        verificar(p == (p_arriba[0], p_arriba[1] + 24), f"ocultar sola: la maximizada ocupa toda el área ({p})")
        m.mouse_para(500, 300)   # el ratón estaba sobre la barra (el último clic): fuera de ella se esconde
        time.sleep(1.5)
        v, r = mostrar(m)
        verificar(r["visible"] == 0, "ocultar sola: sin el ratón en el borde, la barra no está")
        m.mouse_para(500, A - 1)
        time.sleep(1.0)
        v, r = mostrar(m)
        verificar(r["visible"] == 1, "ocultar sola: el ratón en el borde la trae")
        png = m.captura("H363-barra-ocultar-sola")
        m.mouse_para(500, 300)
        time.sleep(1.5)
        v, r = mostrar(m)
        verificar(r["visible"] == 0, "ocultar sola: al irse el ratón vuelve a esconderse")
        k = len(m.serial())
        ajustar(m, "barra_autoocultar=0", "barra_posicion=0")
        p = pedido(m, k)
        verificar(p == p_arriba, f"barra arriba otra vez: la maximizada vuelve a su tamaño ({p})")
        m.pos_barra_arriba = True
        m.cy = 12

        # ---- 4. reloj, bandeja, rótulos, agrupar, botón Inicio y tema
        def ultimo_barra(desde):
            g = re.findall(r"\[GUI\] barra 1024: inicio (\d+)\+(\d+), (\d+) tarefas:([^\r\n]*?), relogio (\d+)", m.serial()[desde:])
            return g[-1] if g else None

        k = len(m.serial())
        ajustar(m, "barra_segundos=1")
        a = ultimo_barra(k)
        k = len(m.serial())
        ajustar(m, "barra_segundos=0")
        b = ultimo_barra(k)
        verificar(a and b and int(b[4]) - int(a[4]) == 24, f"segundos: el reloj se ensancha 24 px ({a and a[4]} -> {b and b[4]})")
        ajustar(m, "barra_reloj=0")
        png = m.captura("H363-sin-reloj")
        cont = regiao(png, 850, 2, 120, 19)
        verificar(cont.get((255, 255, 255), 0) == 0, "sin reloj: no hay texto blanco en su sitio")
        ajustar(m, "barra_reloj=1")
        png = m.captura("H363-con-reloj")
        verificar(regiao(png, 850, 2, 120, 19).get((255, 255, 255), 0) > 30, "con reloj: el texto de la hora")
        k = len(m.serial())
        ajustar(m, "bandeja_volumen=0")
        g = re.findall(r"\[GUI\] bandeja \d+\+\d+:([^\r\n]*)", m.serial()[k:])
        verificar(g and "vol@" not in g[-1] and "red@" in g[-1], f"bandeja sin volumen ({g[-1] if g else None})")
        ajustar(m, "bandeja_volumen=1")
        k = len(m.serial())
        ajustar(m, "inicio_texto=1")
        verificar(ultimo_barra(k)[1] == "28", f"botón Inicio solo logo: 28 px ({ultimo_barra(k)})")
        m.captura("H363-inicio-solo-logo")
        k = len(m.serial())
        ajustar(m, "inicio_texto=2")
        verificar(ultimo_barra(k)[1] == "64", f"botón Inicio solo texto: 64 px ({ultimo_barra(k)})")
        ajustar(m, "inicio_texto=0")
        k = len(m.serial())
        ajustar(m, "barra_rotulos=1")
        t = ultimo_barra(k)
        anchos = re.findall(r"\d+@\d+\+(\d+)", t[3])
        verificar(anchos and all(int(x) == 28 for x in anchos), f"solo iconos: botones de 28 px ({anchos})")
        m.captura("H363-solo-iconos")
        ajustar(m, "barra_rotulos=0")
        k = len(m.serial())
        m.comando("arquivos &")
        time.sleep(2.0)
        foco(m)
        sin = ultimo_barra(k)
        n_sin = len(re.findall(r"\d+@\d+\+\d+", sin[3]))
        k = len(m.serial())
        ajustar(m, "barra_agrupar=1")
        con = ultimo_barra(k)
        n_con = len(re.findall(r"\d+@\d+\+\d+", con[3]))
        verificar(n_con < n_sin, f"agrupar: las ventanas «Archivos» pasan a un solo botón ({n_sin} -> {n_con} botones)")
        m.captura("H363-agrupar")
        ajustar(m, "barra_agrupar=0")
        ajustar(m, "barra_tema=1")
        png = m.captura("H363-tema-claro")
        verificar(cor_do_pixel(png, 300, 2) == (0xD8, 0xDE, 0xE4), "tema claro")
        ajustar(m, "barra_tema=4")
        png = m.captura("H363-tema-azul")
        verificar(cor_do_pixel(png, 300, 2) == (0x0E, 0x1A, 0x38), "tema azul noche")
        ajustar(m, "barra_tema=2")
        png = m.captura("H363-tema-acento")
        verificar(perto(cor_do_pixel(png, 300, 2), (5, 49, 25), 3), f"tema del acento {cor_do_pixel(png, 300, 2)}")
        ajustar(m, "barra_tema=0")

        # ---- 5. la ventana «Configuración»: Escritorio > Barra y menú Inicio
        k = len(m.serial())
        m.comando("configuracion &")
        esperar(m, r'criada por \d+: "Configuraci\S+" \d+x\d+ em \(\d+,\d+\)', 30, k)
        time.sleep(1.5)
        g = re.findall(r'criada por \d+: "Configuraci\S+" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[k:])[-1]
        ox, oy = int(g[2]) + 2, int(g[3]) + 24
        import conf_arbol as ca
        v = dict(x=ox, y=oy, l=int(g[0]), a=int(g[1]))
        k = len(m.serial())
        ca.ir(m, v, "barra")                             # Escritorio > Barra y menú Inicio (D367: sección del árbol)
        esperar(m, r"configuracion: escritorio vista barra", 10, k)
        time.sleep(0.6)
        m.captura("H363-config-barra-menu-inicio")
        k = len(m.serial())
        # Posición: primera fila bajo la vista previa (y local 206); «Abajo» es la segunda opción
        m.clique(*ca.panel(v, 30 + 150 + 78 + 10, 206 + 6))
        esperar(m, r"configuracion: barra barra_posicion=1", 10, k)
        time.sleep(1.0)
        c = cfg_log(m, k)
        verificar(c[0] == 1, f"Configuración: «Abajo» puso la barra abajo ({c})")
        k = len(m.serial())
        # Menú Inicio > Moderno: la última fila del panel; con el estilo de serie (buscador y translúcido en gris con su motivo)
        # el contenido mide 748 (a 900 de ancho las cuatro opciones caben en una fila) y hay que desplazarlo al final
        ca.al_final(m, v)
        rol = ca.rol_max(748, v["a"])
        m.clique(*ca.panel(v, 180 + 94 + 94 + 134 + 10, 636 + 6, rol))
        esperar(m, r"configuracion: barra inicio_menu=3", 10, k)
        time.sleep(0.8)
        m.captura("H363-config-barra-moderno")
        c = cfg_log(m, k)
        verificar(c[2] == 3, f"Configuración: «Moderno» cambia el estilo del menú ({c})")
        # «Predeterminado» (restablecer lo de serie)
        k = len(m.serial())
        m.clique(*ca.boton_pie(v, "predeterminado"))
        esperar(m, r"configuracion: barra restablecida", 10, k)
        time.sleep(1.0)
        c = cfg_log(m, k)
        verificar(c[0] == 0 and c[2] == 0 and c[1] == 24, f"«Restablecer lo de serie» vuelve a lo de siempre ({c})")
        foco(m)

        # ---- 6. se guarda y el arranque lo aplica
        ajustar(m, "barra_posicion=1", "barra_altura=32", "inicio_menu=3", "barra_tema=4", "inicio_buscador=0")
        out = m.comando("cat /home/.config/sagui.conf")
        for kclave in ("barra_posicion=1", "barra_altura=32", "inicio_menu=3", "barra_tema=4", "inicio_buscador=0"):
            verificar(kclave in out, f"sagui.conf tiene {kclave}")
        m.comando("sync")
        time.sleep(1.0)

    with Maquina("h363-b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        verificar("configuracion: barra posicion 1 altura 32 menu 3 tema 4 -> 0" in m.serial(), "el arranque aplicó la barra guardada")
        v, r = mostrar(m)
        verificar((r["y"], r["a"]) == (A - 32, 32) and v["inicio_menu"] == 3 and v["barra_tema"] == 4,
                  f"tras reiniciar: barra abajo de 32 px, menú Moderno, tema azul ({r})")
        png = m.captura("H363-despues-de-reiniciar")
        verificar(cor_do_pixel(png, 300, A - 16) == (0x0E, 0x1A, 0x38), "tras reiniciar: la barra azul está abajo")
        m.comando("configuracion barra restablecer")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
