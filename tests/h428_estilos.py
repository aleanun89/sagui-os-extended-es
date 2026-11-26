#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H428 (D428, v8.5.5.2) - estilos de apariencia para todo el sistema.

Parte estática (sin QEMU, tests/estilos_unidad.c con el mismo generador que usan el kernel y la libsagui):
  (1) hay doce esquemas incluidos con los nombres pedidos y cada elemento editable cambia su color;
  (2) «SaguiOS» da, valor por valor, los colores de siempre (COR_*, W98_*, mod_tema, barra de título, menús);
  (3) contraste: texto sobre fondo, texto de títulos, selección, menús, barra y campos llegan a 4,5:1 (7:1 en los de
      alto contraste).
Parte QEMU (1024x768, imagen con /home persistente):
  (4) en Configuración > Estilos, un clic real en «Desierto», «Oscuro» y «Alto contraste negro» cambia los colores del
      escritorio, de la barra de tareas, del título activo y del menú abierto (se leen píxeles) y se hacen capturas de
      Archivos, un menú y Configuración;
  (5) «SaguiOS» deja la pantalla idéntica a la de antes de tocar nada (misma imagen, salvo el reloj y los widgets);
  (6) un esquema personalizado (color de un elemento + guardarlo con nombre) persiste: tras reiniciar la sesión
      (VM con la misma imagen) el arranque lo aplica y sus colores están en pantalla;
  (7) las aplicaciones abiertas reciben EV_TEMA: Archivos y Configuración se repintan con los colores nuevos.
Capturas docs/capturas/H428-*.png."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import estilos_png as png  # noqa: E402
from qemu import Maquina, verificar, imagem_de_teste, cor_do_pixel, RAIZ, SAIDA  # noqa: E402
import conf_arbol as ca  # noqa: E402

NOMBRES = ("SaguiOS", "SaguiOS clásico", "Desierto", "Berenjena", "Pizarra", "Marino", "Ladrillo", "Lluvia", "Bosque",
           "Alto contraste negro", "Alto contraste blanco", "Oscuro", "Arce", "Lavanda", "Océano", "Cereza", "Grafito",
           "Nórdico", "Terminal", "Solarizado claro", "Solarizado oscuro", "Alto contraste amarillo")
ALTO_CONTRASTE = (9, 10, 21)
KVM = os.environ.get("SAGUI_KVM") == "1"


# ------------------------------------------------------------ parte estática

def volcar():
    exe = os.path.join(RAIZ, "build", "testes", "estilos_unidad")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                    os.path.join(RAIZ, "tests", "estilos_unidad.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0 and "FALLO" not in r.stdout, "estilos_unidad: la edición de elementos funciona")
    esq = {}
    nombres = {}
    for l in r.stdout.splitlines():
        p = l.split("|")
        if len(p) == 4 and p[0].isdigit():
            esq.setdefault(int(p[0]), {})[p[2]] = int(p[3], 16) if p[2] not in ("escuro", "barra_propia", "escritorio_liso") else int(p[3])
            nombres[int(p[0])] = p[1]
    return esq, nombres


# Los colores de siempre (antes de D428): COR_* y W98_* de gui.h, la barra de título, el menú contextual y mod_tema
SAGUIOS = dict(fundo=0xF4F4F0, texto=0x1E2328, texto_fraco=0x6A737C, borda=0x8A939C, botao=0xE2E6EA, botao_apertado=0xC4CBD2,
               destaque=0x0B7A3E, selecao=0x2A6FD8, cara=0xC0C0C0, luz=0xFFFFFF, sombra=0x808080, oscuro=0x404040,
               azul=0x000080, negro=0, titulo_a1=0x0B7A3E, titulo_a2=0x0B7A3E, titulo_i1=0x5A6470, titulo_i_texto=0xD0D4D8,
               contorno=0x101010, cerrar_a=0xD03C32, cerrar_i=0x8A5A56, menu_fundo=0xF4F4F0, menu_texto=0x1E2328,
               menu_sel=0x0B7A3E, menu_borde=0x101010, menu_sep=0xC4CBD2, menu_atalho=0x6A737C, texto_dis=0x9AA3AC,
               barra=0x18202A, acento=0x0B7A3E, foco=0x0A8F96, scroll_pista=0xE4E7EA, scroll_polegar=0x9AA3AC,
               sel_suave=0xB9D3F2, inicio_a=0x2A5FC8, inicio_b=0x1B3F94)
# TEMA_CLARO de gui_moderno.c -> campo del esquema
MOD = dict(acento="destaque", acento_hover="m_acento_hover", acento_press="m_acento_press", acento_suave="m_acento_suave",
           sobre_acento="m_sobre_acento", foco="foco", superficie="m_superficie", barra="m_barra", campo="campo_fundo",
           pista="m_pista", borde="m_borde", borde_forte="m_borde_forte", hover="m_hover", press="m_press", texto="texto",
           texto_fraco="m_texto_fraco", desab_fundo="m_desab_fundo", desab_texto="m_desab_texto", peligro="m_peligro",
           peligro_suave="m_peligro_suave", aviso="m_aviso", ok="m_ok", dica_fundo="m_dica_fundo", dica_texto="m_dica_texto")


def parte_estatica():
    esq, nombres = volcar()
    verificar(tuple(nombres[i] for i in range(len(NOMBRES))) == NOMBRES and len(esq) == len(NOMBRES), f"los esquemas incluidos: {[nombres[i] for i in sorted(nombres)]}")
    for k, v in SAGUIOS.items():
        verificar(esq[0][k] == v, f"SaguiOS.{k} = {v:06X} (da {esq[0][k]:06X})")
    src = open(os.path.join(RAIZ, "userland", "libsagui", "gui_moderno.c"), encoding="utf-8").read()
    bloque = src[src.index("#define TEMA_CLARO"):src.index("mod_tema_t mod_tema")]
    for campo, nombre in MOD.items():
        g = re.search(r"\." + campo + r" = 0x([0-9A-Fa-f]{6})", bloque)
        verificar(g and int(g.group(1), 16) == esq[0][nombre], f"mod_tema.{campo} de SaguiOS = TEMA_CLARO ({g.group(1) if g else '?'})")
    # contraste (WCAG): 4,5:1 para el texto normal; 7:1 en los de alto contraste; 3:1 en el título inactivo; el texto
    # deshabilitado solo se pide que se distinga (2,3:1; 4,5:1 en los de alto contraste). «SaguiOS» es el aspecto de siempre.
    pares = (("texto", "fundo"), ("titulo_a_texto", "titulo_a1"), ("titulo_a_texto", "titulo_a2"), ("titulo_i_texto", "titulo_i1"),
             ("titulo_i_texto", "titulo_i2"), ("selecao_texto", "selecao"), ("menu_texto", "menu_fundo"),
             ("menu_sel_texto", "menu_sel"), ("barra_texto", "barra"), ("campo_texto", "campo_fundo"),
             ("info_texto", "info_fundo"), ("m_sobre_acento", "acento"), ("escritorio_texto", "escritorio_a"),
             ("escritorio_texto", "escritorio_b"), ("texto_fraco", "fundo"), ("menu_atalho", "menu_fundo"),
             ("cerrar_x", "cerrar_a"), ("texto_dis", "fundo"))
    for i in range(1, len(NOMBRES)):
        e = esq[i]
        hc = i in ALTO_CONTRASTE
        for a, b in pares:
            minimo = 3.0 if a == "titulo_i_texto" else (4.5 if hc else 2.3) if a == "texto_dis" else 7.0 if hc else 4.5
            c = png.contraste(e[a], e[b])
            verificar(c >= minimo, f"[{NOMBRES[i]}] {a} sobre {b}: {c:.1f}:1 (mínimo {minimo})")
    return esq


# ------------------------------------------------------------ parte QEMU

def esperar(m, patron, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(patron, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-1200:])


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def hex3(c):
    return (c >> 16, (c >> 8) & 255, c & 255)


def mistura(a, b, num, den):
    r = 0
    for s in (16, 8, 0):
        ca_, cb = (a >> s) & 255, (b >> s) & 255
        r |= (ca_ + int((cb - ca_) * num / den)) << s   # C trunca hacia cero
    return r


def main():
    esq = parte_estatica()
    print("parte estática: ok")
    if os.environ.get("SAGUI_SIN_QEMU"):
        return
    parte_qemu(esq)


ANCHO, ALTO = 1280, 1024
# lo que se ignora al comparar dos capturas del mismo estado: el reloj de la barra y la bandeja
# El botón «Adelante» de Archivos (582..609, 590..617) conserva el realce de «ratón encima» de un paso anterior de la prueba
# (el ratón pasó por allí al abrir la ventana sin foco) y se repinta normal más tarde: depende del ratón, no del estilo.
IGNORAR = ((1000, 0, ANCHO, 24), (580, 588, 612, 620))


def distintos(a, b):
    """Píxeles distintos entre dos capturas (fuera del reloj y la bandeja)."""
    l, _al, ra = a
    rb = b[2]
    n = 0
    for y in range(a[1]):
        fa, fb = ra[y * l * 3:(y + 1) * l * 3], rb[y * l * 3:(y + 1) * l * 3]
        if fa == fb:
            continue
        for x in range(l):
            if fa[x * 3:x * 3 + 3] != fb[x * 3:x * 3 + 3] and not any(r[0] <= x < r[2] and r[1] <= y < r[3] for r in IGNORAR):
                n += 1
    return n


def mover(m, frame, destino, agarre=(300, 12)):
    """Arrastra una ventana por su título para que su esquina de arriba a la izquierda quede en `destino`."""
    m.clique(frame[0] + agarre[0], frame[1] + agarre[1])   # primero se le da el foco: en una ventana sin foco el clic solo la sube
    m.arrastar(frame[0] + agarre[0], frame[1] + agarre[1], destino[0] + agarre[0], destino[1] + agarre[1])


def mistura_pct(a, b, pct):
    """esq_mix de esquemas.h: a + (b - a) * pct / 100 por canal (división entera hacia cero, como en C)."""
    r = 0
    for sh in (16, 8, 0):
        x, y = (a >> sh) & 255, (b >> sh) & 255
        r |= (x + int((y - x) * pct / 100)) << sh
    return r


def esperado_escritorio(e, y):
    if e["escritorio_liso"]:
        return mistura(e["escritorio_a"], e["escritorio_b"], y, ALTO - 1)
    h = ALTO // 2
    return mistura(0x0B7A3E, 0xE0B92A, y, h) if y < h else mistura(0xE0B92A, 0x14337E, y - h, ALTO - h)


def parte_qemu(esq):
    img_path = imagem_de_teste("h428", ["--vesa", "--resolucao", f"{ANCHO}x{ALTO}", "--dados-mb", "16"])
    with Maquina("h428", "ide", imagem=img_path, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(3)
        n0 = len(m.serial())
        m.comando("configuracion --ventana estilos & arquivos &", 20)
        esperar(m, r'criada por \d+: "Archivos"', 30, n0)
        esperar(m, r"configuracion: estilos lista x=", 30, n0)
        time.sleep(2.5)
        sr = m.serial()
        v = janela(sr, "Configuración")
        a = janela(sr, "Archivos")
        g = re.findall(r"configuracion: estilos lista x=(\d+) y=(\d+)", sr)[-1]
        lx, ly = int(g[0]), int(g[1])
        # Archivos abajo a la derecha y Configuración arriba a la izquierda: se ven los dos y sus listas no se tapan.
        # Primero Archivos (su título queda a la vista encima de la de Configuración al lanzarlas en este orden).
        mover(m, (a["x"] - 2, a["y"] - 24), (540, 560))
        mover(m, (v["x"] - 2, v["y"] - 24), (20, 40))
        v = dict(v, x=22, y=64)
        a = dict(a, x=542, y=584)
        time.sleep(1)
        # --- 4: clic real en la lista de Configuración y menú abierto sobre una carpeta de Archivos
        base = {}

        topo = [0]   # primera fila visible de la lista (10 filas a la vista, con barra de desplazamiento)

        def fila(i):
            return ca.panel(v, lx + 60, ly + 2 + (i - topo[0]) * 20 + 10)

        def elegir(i):
            """Elige la fila i: clic si se ve; si no, clic en la última (o primera) visible y flechas, como una persona."""
            vis = i - topo[0]
            if 0 <= vis <= 9:
                m.clique(*fila(i))
                m.clique(*fila(i))   # por si el primer clic solo subió la ventana
            elif vis > 9:
                m.clique(*fila(topo[0] + 9))
                for _ in range(i - topo[0] - 9):
                    m.tecla("down", 0.5)
                topo[0] = i - 9
            else:
                m.clique(*fila(topo[0]))
                for _ in range(topo[0] - i):
                    m.tecla("up", 0.5)
                topo[0] = i

        def estilo(i, nombre, ver=False):
            """Elige el estilo i en la lista, abre el menú de una carpeta de Archivos y devuelve la captura."""
            n = len(m.serial())
            elegir(i)
            esperar(m, r"configuracion: estilo '[^']*' aplicado", 20, n)
            time.sleep(2)
            fx, fy = 1000, 735   # una carpeta de Archivos que Configuración no tapa
            m.clique(fx, fy, botao=2)   # menú de contexto de la carpeta bajo el cursor
            time.sleep(1.2)
            ruta = m.captura("H428-" + nombre)
            m.tecla("esc")
            time.sleep(0.5)
            return png.leer(ruta), (fx, fy)

        resultados = {}
        for i, nombre in ((0, "1-saguios"), (2, "2-desierto"), (11, "3-oscuro"), (9, "4-alto-contraste-negro"),
                          (12, "10-arce"), (16, "11-grafito"), (18, "12-terminal"), (19, "13-solarizado-claro"),
                          (21, "14-alto-contraste-amarillo"), (0, "5-saguios-otra-vez")):
            img, (fx, fy) = estilo(i, nombre)
            resultados[nombre] = img
            e = esq[i]
            # barra de tareas, escritorio, títulos, menú, selección de la lista y fondo de la ventana
            barra = e["barra"] if e["barra_propia"] else 0x18202A
            verificar(png.pixel(img, 900, 4) == hex3(barra), f"[{NOMBRES[i]}] barra de tareas {png.pixel(img, 900, 4)} = {barra:06X}")
            verificar(png.pixel(img, 10, 960) == hex3(esperado_escritorio(e, 960)),
                      f"[{NOMBRES[i]}] escritorio {png.pixel(img, 10, 960)} = {esperado_escritorio(e, 960):06X}")
            k = 960 - (a["x"] - 2)   # Archivos (la activa, tras el clic derecho): un punto libre de su título, con su degradado
            lf = a["l"] + 4
            ta = mistura_pct(e["titulo_a1"], e["titulo_a2"], k * 100 // (lf - 1))
            ty = a["y"] - 24 + 4
            verificar(png.pixel(img, 960, ty) == hex3(ta), f"[{NOMBRES[i]}] título activo (degradado) {png.pixel(img, 960, ty)} = {ta:06X}")
            cx, cy = v["x"] - 2 + 3, v["y"] - 24 + 3   # Configuración (inactiva)
            verificar(png.pixel(img, cx, cy) == hex3(e["titulo_i1"]), f"[{NOMBRES[i]}] título inactivo {png.pixel(img, cx, cy)} = {e['titulo_i1']:06X}")
            tf = mistura_pct(e["titulo_i1"], e["titulo_i2"], (700 - (v["x"] - 2)) * 100 // (v["l"] + 3))   # y su degradado (ancho del marco = l + 4)
            verificar(png.pixel(img, 700, cy) == hex3(tf), f"[{NOMBRES[i]}] título inactivo (degradado) {png.pixel(img, 700, cy)} = {tf:06X}")
            verificar(png.pixel(img, 1006, 430) == hex3(e["menu_fundo"]), f"[{NOMBRES[i]}] fondo del menú {png.pixel(img, 1006, 430)} = {e['menu_fundo']:06X}")
            sx, sy = ca.panel(v, lx + 4, ly + 2 + (i - topo[0]) * 20 + 3)
            verificar(png.pixel(img, sx, sy) == hex3(e["selecao"]), f"[{NOMBRES[i]}] fila elegida {png.pixel(img, sx, sy)} = {e['selecao']:06X}")
            verificar(png.pixel(img, v["x"] + 3, v["y"] + 300) == hex3(e["fundo"]), f"[{NOMBRES[i]}] fondo de Configuración repintado {png.pixel(img, v['x'] + 3, v['y'] + 300)} = {e['fundo']:06X}")
        # --- 5: «SaguiOS» deja la pantalla como estaba
        d = distintos(resultados["1-saguios"], resultados["5-saguios-otra-vez"])
        verificar(d == 0, f"«SaguiOS» tras pasar por otros estilos: {d} píxeles distintos de la captura inicial")
        # --- 8: la fuente del sistema se cambia en vivo, la de siempre vuelve idéntica y las ventanas se repintan
        base = resultados["5-saguios-otra-vez"]
        zona = (20, 40, 400, 64)   # el título de Configuración (inactiva): el texto «Configuración» con la fuente del sistema
        def region(img):
            l, _a, rgb = img
            return b"".join(rgb[(y * l + zona[0]) * 3:(y * l + zona[2]) * 3] for y in range(zona[1], zona[3]))
        m.clique(150, 12)   # el botón del Terminal
        time.sleep(1)
        n = len(m.serial())
        m.comando("configuracion estilo fuente 2", 30)
        esperar(m, r"configuracion: estilo 'SaguiOS' aplicado", 20, n)
        time.sleep(1.5)
        img_f = png.leer(m.captura("H428-9-fuente-terminus-negrita"))
        verificar(region(img_f) != region(base), "con la fuente «Terminus negrita» el título de Configuración se dibuja distinto")
        m.comando("configuracion estilo fuente 3", 30)
        time.sleep(1.5)
        m.clique(520, 12)   # Configuración al frente: se ve la fuente en el árbol y en la página
        time.sleep(1.2)
        m.captura("H428-9-fuente-fixed")
        m.clique(150, 12)   # y de vuelta al Terminal
        time.sleep(1)
        m.comando("configuracion estilo fuente 0", 30)
        time.sleep(1.5)
        verificar(region(png.leer(m.captura("H428-9-fuente-normal", guardar=False))) == region(base), "con la fuente de siempre el título vuelve idéntico píxel a píxel")
        # --- 6: un esquema personalizado (la barra y el título activo con colores propios) se guarda con nombre
        n = len(m.serial())   # el Terminal ya tiene el foco (el paso anterior)
        m.comando("configuracion estilo color 19 123456", 30)    # EE_BARRA
        m.comando("configuracion estilo color 2 654321", 30)     # EE_TIT_A1
        m.comando("configuracion estilo guardar Prueba", 30)
        esperar(m, r"configuracion: estilo 'Prueba' aplicado", 20, n)
        time.sleep(1)
        ruta = m.captura("H428-6-personalizado")
        img = png.leer(ruta)
        verificar(png.pixel(img, 900, 4) == hex3(0x123456), f"personalizado: la barra de tareas es la elegida ({png.pixel(img, 900, 4)})")
        out = m.comando("cat /home/.config/estilo.conf", 20)
        verificar("estilo=personal" in out and "nombre=Prueba" in out, "el archivo de la cuenta lleva el esquema personalizado con su nombre")
        out = m.comando("configuracion estilo lista", 20)
        verificar("Prueba (guardado)" in out, "el esquema aparece en la lista de los guardados")
        m.comando("sync", 30)
    # --- 6 (cont.): tras reiniciar (la misma imagen), el arranque lo aplica
    with Maquina("h428", "ide", imagem=img_path, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(3)
        verificar("configuracion: estilo 'Prueba' ->" in m.serial(), "al arrancar, «configuracion --aplicar» aplica el esquema guardado")
        ruta = m.captura("H428-7-reinicio")
        img = png.leer(ruta)
        verificar(png.pixel(img, 900, 4) == hex3(0x123456), f"tras reiniciar: la barra de tareas sigue siendo la elegida ({png.pixel(img, 900, 4)})")
        sr = m.serial()
        t = janela(sr, "Terminal")
        verificar(png.pixel(img, t["x"] - 2 + 3, t["y"] - 24 + 3) == hex3(0x654321), f"tras reiniciar: el título activo es el elegido ({png.pixel(img, t['x'] - 2 + 3, t['y'] - 24 + 3)})")
        # --- 7: las aplicaciones abiertas se repintan al cambiar de esquema (EV_TEMA)
        n = len(m.serial())
        m.comando("calculadora &", 20)
        esperar(m, r'criada por \d+: "Calculadora"', 20, n)
        time.sleep(1.5)
        c = janela(m.serial(), "Calculadora")
        mover(m, (c["x"] - 2, c["y"] - 24), (930, 300), agarre=(60, 12))   # fuera del Terminal, que va a quedar encima
        c = dict(c, x=932, y=324)
        antes = png.leer(m.captura("H428-8-antes", guardar=False))
        px = (c["x"] + c["l"] - 6, c["y"] + 200)   # el margen de la derecha, que el Terminal no tapa
        m.clique(150, 12)   # el botón del Terminal
        time.sleep(1)
        e_sag = esq[0]
        verificar(png.pixel(antes, *px) == hex3(e_sag["fundo"]), f"la Calculadora arranca con el fondo de siempre ({png.pixel(antes, *px)})")
        m.comando("configuracion estilo oscuro", 30)
        time.sleep(2)
        despues = png.leer(m.captura("H428-8-oscuro-con-calculadora"))
        verificar(png.pixel(despues, *px) == hex3(esq[11]["fundo"]), f"la Calculadora abierta se repinta con el esquema Oscuro ({png.pixel(despues, *px)})")


if __name__ == "__main__":
    main()
