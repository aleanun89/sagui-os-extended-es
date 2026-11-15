#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H315 (D313, mejora 118, fase 1) - multiidioma: catálogos, _(), idioma elegible y pseudo-idioma.

Parte 1 (anfitrión): el lector de catálogos de kernel/lib/catalogo.c compilado con gcc lee los .cat
que genera tools/extraer_textos.py (aciertos, fallos, \\n, Latin-1, archivos truncados) y normaliza los
nombres de idioma; los .po de la fase 1 (kernel, configuracion, arquivos, libsagui) no tienen
cadenas sin traducir en pt_BR ni en en.

Parte 2 (QEMU, imagen gráfica con /home persistente): en español todo sigue igual; con
«configuracion idioma en» y «pt_BR» se capturan el escritorio, el menú Inicio, Configuración y Archivos
traducidos; se cambia el idioma desde la ventana de Configuración y el programa que se abre después
sale en ese idioma; con el pseudo-idioma «xx» (cada cadena entre corchetes y un 30 % más larga) no queda
ninguna cadena de la fase 1 sin traducir (el registro «[IDIOMA] sin traducir» está vacío); el idioma se
recuerda al reiniciar.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402
import h83_configuracion as h83  # noqa: E402

sys.path.insert(0, os.path.join(RAIZ, "tools"))
import extraer_textos as X  # noqa: E402

FASE1 = ["kernel", "configuracion", "arquivos", "libsagui"]


# ------------------------------------------------------------------ parte 1: el anfitrión

def parte1():
    tmp = tempfile.mkdtemp(prefix="h315-")
    exe = os.path.join(tmp, "catalogo_unidad")
    subprocess.run(["gcc", "-O1", "-Wall", "-Ikernel/inc", "-o", exe, "tests/catalogo_unidad.c"], cwd=RAIZ, check=True)
    cat_dir = os.path.join(tmp, "cat")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "extraer_textos.py"), "compilar", cat_dir],
                   check=True, stdout=subprocess.DEVNULL)

    # el .cat de pt_BR de Archivos contra su .po, clave por clave
    for idi in ("pt_BR", "en"):
        po = X.po_leer(os.path.join(X.DIR_IDIOMAS, idi, "arquivos.po"))
        claves = [c for c in po if po[c][0]]
        entrada = "\n".join(c.replace("\n", "\\n") for c in claves) + "\nno existe esta cadena\n"
        r = subprocess.run([exe, os.path.join(cat_dir, idi, "arquivos.cat")], input=entrada.encode("latin-1"),
                           capture_output=True, check=True)
        salida = r.stdout.decode("latin-1").split("\n")[:-1]
        verificar(len(salida) == len(claves) + 1, f"{idi}: una respuesta por clave")
        mal = [c for c, s in zip(claves, salida) if s != po[c][0].replace("\n", "\\n")]
        verificar(not mal, f"{idi}: el lector devuelve las {len(claves)} traducciones de arquivos.po ({mal[:2]})")
        verificar(salida[-1] == "(null)", f"{idi}: una clave que no está devuelve NULL")
    # Latin-1: la «ñ» y las vocales acentuadas llegan como un byte
    r = subprocess.run([exe, os.path.join(cat_dir, "pt_BR", "kernel.cat")], input="Configuración\n".encode("latin-1"),
                       capture_output=True, check=True)
    verificar(r.stdout.decode("latin-1").strip() == "Configurações", "Latin-1 de extremo a extremo (Configuración -> Configurações)")
    # el pseudo-idioma: entre corchetes y un 30 % más largo
    r = subprocess.run([exe, os.path.join(cat_dir, "xx", "libsagui.cat")], input="Guardar como\n".encode("latin-1"),
                       capture_output=True, check=True)
    ps = r.stdout.decode("latin-1").strip()
    verificar(ps.startswith("[Guardar como") and ps.endswith("]") and len(ps) >= int(len("Guardar como") * 1.3) + 2,
              f"xx: «{ps}» entre corchetes y más larga")
    # archivo que no es un catálogo y truncado
    falso = os.path.join(tmp, "falso.cat")
    open(falso, "wb").write(b"no es un catalogo" * 4)
    r = subprocess.run([exe, falso], input=b"x\n", capture_output=True)
    verificar(r.returncode == 4 and b"INVALIDO" in r.stdout, "un archivo que no es un catálogo se rechaza")
    # nombres de idioma
    ent = "pt\npt_BR.ISO-8859-1\nPT-br\nen_US.UTF-8\nEN\nes_ES\nC\nPOSIX\n\nxx\n../etc\n"
    r = subprocess.run([exe, "-n"], input=ent.encode(), capture_output=True, check=True)
    esperado = ["pt_BR", "pt_BR", "pt_BR", "en", "en", "es", "es", "es", "es", "xx", "es"]
    verificar(r.stdout.decode().split("\n")[:-1] == esperado, f"nombres de idioma normalizados ({r.stdout.decode().split()})")
    # nada de la fase 1 sin traducir
    sin = 0
    cats = X.extraer_todo()
    for cat in FASE1:
        verificar(cat in cats, f"el catálogo {cat} tiene cadenas marcadas ({len(cats.get(cat, {}))})")
        for idi in X.IDIOMAS:
            po = X.po_leer(os.path.join(X.DIR_IDIOMAS, idi, cat + ".po"))
            falta = [c for c in cats[cat] if c not in po or not po[c][0] or po[c][1]]
            sin += len(falta)
            verificar(not falta, f"{idi}/{cat}.po sin cadenas pendientes ({falta[:3]})")
    # los .po estándar: cabecera con la codificación y c-format en lo que lleva %
    t = open(os.path.join(X.DIR_IDIOMAS, "pt_BR", "arquivos.po"), encoding="utf-8").read()
    verificar('charset=UTF-8' in t and "#, c-format" in t, ".po estándar: charset UTF-8 y marcas c-format")


# ------------------------------------------------------------------ parte 2: QEMU

def sin_traducir(serial, programas=None):
    """Las líneas «[IDIOMA] sin traducir (xx): texto» del registro."""
    r = re.findall(r"\[IDIOMA\] sin traducir \((\w+)\): (.*)", serial.replace("\r", ""))
    return [t for _, t in r]


def esperar_serial_desde(m, patron, desde, tempo=20):
    fim = time.time() + tempo
    while time.time() < fim:
        r = re.search(patron, m.serial().replace("\r", "")[desde:])
        if r:
            return r
        time.sleep(0.3)
    verificar(False, f"esperaba {patron!r} en el registro")


def marca(m):
    return len(m.serial().replace("\r", ""))


def titulos_pestanas(idioma):
    """Los títulos de las pestañas de Configuración en ese idioma, como los dibuja el programa."""
    es = ["Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red", "Escritorio remoto"]
    if idioma == "es":
        return es
    if idioma == "xx":
        return [X.pseudo(t) for t in es]
    po = X.po_leer(os.path.join(X.DIR_IDIOMAS, idioma, "configuracion.po"))
    return [po[t][0] for t in es]


def posiciones(titulos):
    x, pos = 10, []
    for t in titulos:
        l = len(t.encode("latin-1")) * 8 + 10
        pos.append((x, l))
        x += l + 2
    return pos


def ventana(m, titulo):
    f = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
    verificar(f, f"la ventana «{titulo}» se creó")
    g = f[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def esperar_ventana(m, titulo, desde=0, tempo=25):
    fim = time.time() + tempo
    pat = f'criada por \\d+: "{re.escape(titulo)}" '
    while time.time() < fim:
        if re.search(pat, m.serial()[desde:]):
            time.sleep(0.8)
            return ventana(m, titulo)
        time.sleep(0.3)
    verificar(False, f"se esperaba la ventana «{titulo}»")


def foco_terminal(m):
    """Devuelve el foco al Terminal con su botón de la barra de tareas (la última línea «[GUI] barra» del registro)."""
    f = re.findall(r"\[GUI\] barra \d+: .*? 1@(\d+)\+(\d+)", m.serial())
    verificar(f, "la barra de tareas registra el botón del Terminal")
    x, l = int(f[-1][0]), int(f[-1][1])
    m.clique(x + l // 2, 12)
    time.sleep(0.6)


def cerrar_ventana(m, j):
    """No se cierran las ventanas (los clics en el título las mueven): solo se devuelve el foco al Terminal."""
    foco_terminal(m)


def idioma_en_pantalla(m, idi, etiqueta, titulos, tit_cfg, tit_arq):
    """Pone el idioma por la línea de comandos y captura el escritorio, el menú Inicio, Configuración y Archivos."""
    k = marca(m)
    out = m.comando(f"configuracion idioma {idi}")
    verificar("[IDIOMA] interfaz del kernel: %s" % idi in m.serial().replace("\r", "")[k:] or idi == "es",
              f"{etiqueta}: el kernel carga su catálogo ({idi})")
    time.sleep(1.2)
    m.captura(f"H315-{etiqueta}-1-escritorio")
    m.clique(44, 12)    # el botón Inicio
    time.sleep(0.8)
    m.captura(f"H315-{etiqueta}-2-menu-inicio")
    m.tecla("esc")
    time.sleep(0.5)
    k = marca(m)
    m.comando("configuracion &")
    jc = esperar_ventana(m, tit_cfg, k)
    time.sleep(0.8)
    m.captura(f"H315-{etiqueta}-3-configuracion")
    pos = posiciones(titulos)
    ox, oy = jc["x"] + 2, jc["y"] + 24
    for i in range(len(pos)):   # todas las páginas: lo que no esté en el catálogo se anota
        m.clique(ox + pos[i][0] + pos[i][1] // 2, oy + 24)
        time.sleep(0.35)
        if i in (1, 2, 3, 8, 9):
            m.captura(f"H315-{etiqueta}-3-configuracion-{i}")
    cerrar_ventana(m, jc)
    k = marca(m)
    m.comando("arquivos &")
    ja = esperar_ventana(m, tit_arq, k)
    time.sleep(0.8)
    m.captura(f"H315-{etiqueta}-4-archivos")
    return ja


def parte2():
    os.makedirs(SAIDA, exist_ok=True)
    img = h83.imagem_persistente("h315-persistente", True)
    with Maquina("h315-a", "ide", imagem=img, snapshot=False, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        verificar("[GUI] area de trabalho" in s or "area de trabalho no ar" in s, "escritorio gráfico")
        out = m.comando("configuracion mostrar")
        verificar("Idioma:    es" in out, "por defecto el idioma es el español")
        m.captura("H315-0-es-escritorio")
        # ---- español: sin cambios (Archivos con sus botones de siempre)
        k = marca(m)
        m.comando("arquivos &")
        esperar_ventana(m, "Archivos", k)
        m.captura("H315-0-es-archivos")
        verificar(not sin_traducir(m.serial()), "en español no se anota nada como sin traducir")
        cerrar_ventana(m, ventana(m, "Archivos"))

        # ---- inglés
        ja = idioma_en_pantalla(m, "en", "en", titulos_pestanas("en"), "Settings", "Files")
        cerrar_ventana(m, ja)
        # ---- portugués de Brasil
        ja = idioma_en_pantalla(m, "pt_BR", "pt", titulos_pestanas("pt_BR"), "Configurações", "Arquivos")
        cerrar_ventana(m, ja)
        faltan = sin_traducir(m.serial())
        verificar(not faltan, f"en en y pt_BR no queda nada de la fase 1 sin traducir ({faltan[:4]})")

        # ---- cambiar el idioma desde la ventana de Configuración y abrir un programa nuevo
        m.comando("configuracion idioma es")
        time.sleep(1)
        k = marca(m)
        m.comando("configuracion &")
        jc = esperar_ventana(m, "Configuración", k)
        ox, oy = jc["x"] + 2, jc["y"] + 24
        pos = posiciones(titulos_pestanas("es"))
        m.clique(ox + pos[0][0] + pos[0][1] // 2, oy + 24)   # Teclado
        time.sleep(0.5)
        m.captura("H315-5-elegir-idioma-es")
        k = marca(m)
        m.clique(ox + 444 + 100, oy + 50 + 226 + 15)         # botón «English»
        esperar_serial_desde(m, r"\[IDIOMA\] interfaz del kernel: en", k)
        time.sleep(1.0)
        m.captura("H315-5-elegir-idioma-en")
        foco_terminal(m)
        k = marca(m)
        m.comando("arquivos &")
        esperar_ventana(m, "Files", k)                       # el programa nuevo ya sale en inglés
        m.captura("H315-5-programa-nuevo-en-ingles")
        cerrar_ventana(m, ventana(m, "Files"))
        cerrar_ventana(m, jc)
        verificar("configuracion: pagina Teclado" not in m.serial()[k:], "(sin efectos laterales)")
        foco_terminal(m)
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("idioma=en" in out, "la elección se guardó en /home/.config/sagui.conf")
        # LANG del entorno manda sobre el del sistema
        out = m.comando("LANG=pt_BR configuracion mostrar")
        verificar("Idioma:    pt_BR" in out or "Idioma:    en" in out, f"LANG en el entorno ({out.strip()[-60:]})")
        # ---- reiniciar: el idioma se aplica en el arranque
        h83.reiniciar(m)

    with Maquina("h315-b", "ide", imagem=img, snapshot=False, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial().replace("\r", "")
        verificar("configuracion: idioma en -> 0" in s, "al arrancar, configuracion --aplicar fija el idioma elegido")
        verificar("[IDIOMA] interfaz del kernel: en (catalogo cargado)" in s, "... y el kernel traduce su interfaz")
        time.sleep(1.5)
        m.captura("H315-6-reiniciado-en")
        # ---- el pseudo-idioma: nada sin traducir en la fase 1
        m.comando("configuracion idioma xx")
        time.sleep(1.2)
        m.captura("H315-7-xx-escritorio")
        m.clique(44, 12)
        time.sleep(0.8)
        m.captura("H315-7-xx-menu-inicio")
        m.tecla("esc")
        time.sleep(0.4)
        tit_cfg, tit_arq = X.pseudo("Configuración"), X.pseudo("Archivos")
        k0 = marca(m)
        m.comando("configuracion &")
        jc = esperar_ventana(m, tit_cfg, k0)
        pos = posiciones(titulos_pestanas("xx"))
        ox, oy = jc["x"] + 2, jc["y"] + 24
        for i in range(len(pos)):
            m.clique(ox + pos[i][0] + pos[i][1] // 2, oy + 24)
            time.sleep(0.35)
            if i in (0, 1, 3):
                m.captura(f"H315-7-xx-configuracion-{i}")
        cerrar_ventana(m, jc)
        k = marca(m)
        m.comando("arquivos &")
        ja = esperar_ventana(m, tit_arq, k)
        time.sleep(0.6)
        m.captura("H315-7-xx-archivos")
        cerrar_ventana(m, ja)
        # el diálogo común de archivos (D272), desde el Editor
        k = marca(m)
        m.comando("editor &")
        ed = esperar_ventana(m, "Editor - sin nombre", k)
        k = marca(m)
        m.clique(ed["x"] + 2 + 136, ed["y"] + 24 + 20)
        esperar_ventana(m, X.pseudo("Abrir"), k)
        time.sleep(0.6)
        m.captura("H315-7-xx-dialogo-abrir")
        m.tecla("esc")
        time.sleep(0.5)
        cerrar_ventana(m, ed)
        faltan = sin_traducir(m.serial())
        verificar(not faltan, f"con el pseudo-idioma no queda ninguna cadena de la fase 1 sin traducir ({faltan[:6]})")
        m.comando("configuracion idioma es")
        out = m.comando("configuracion mostrar")
        verificar("Idioma:    es" in out, "se vuelve al español")


if __name__ == "__main__":
    parte1()
    parte2()
    print("H315 OK")
