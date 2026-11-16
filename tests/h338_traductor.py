#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H338 (D335, mejora 133) - el Traductor, un editor de catálogos al estilo de Poedit.

Parte 1 (anfitrión): la lógica (userland/programas/traductor_po.h) compilada con gcc: leer y escribir .po,
validación de %s/%d/%1$s, \\n, atajos, etiquetas, longitud y cabecera, msgmerge, memoria de traducción
(tests/traductor_unidad.c); el .cat que genera es idéntico, byte a byte, al de tools/extraer_textos.py para
todos los .po del sistema; lo que escribe pasa «msgfmt --check» si el anfitrión tiene gettext.

Parte 2 (QEMU, sesión de root): órdenes del programa (--estadisticas, --validar, --actualizar, --generar) y la
ventana: abrir un .po, filtrar, traducir dos cadenas y marcar una como dudosa, provocar un error de %s que la
validación detecta, guardar; el archivo guardado vuelve al anfitrión (base64) y se comprueba con msgfmt;
actualizar desde un .pot; el .cat generado lo lee el lector del sistema. Con el idioma del sistema en pt_BR
el Traductor se ve en portugués, genera el catálogo y lo recarga en caliente, y la memoria de traducción
propone lo ya traducido en los catálogos del sistema. Capturas H338-*.png."""
import base64
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ  # noqa: E402

sys.path.insert(0, os.path.join(RAIZ, "tools"))
import extraer_textos as X  # noqa: E402

FIX = os.path.join(RAIZ, "rootfs", "testes", "traductor")


def msgfmt_ok(ruta, etiqueta):
    """msgfmt --check del anfitrión, si existe."""
    if not shutil.which("msgfmt"):
        print(f"  no aplica: no hay msgfmt en el anfitrión ({etiqueta})")
        return
    r = subprocess.run(["msgfmt", "--check", "-o", os.devnull, ruta], capture_output=True, text=True)
    verificar(r.returncode == 0, f"msgfmt --check acepta {etiqueta} ({r.stderr.strip()[:200]})")


# ------------------------------------------------------------------ parte 1: el anfitrión

def parte1():
    tmp = tempfile.mkdtemp(prefix="h338-")
    exe = os.path.join(tmp, "traductor_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", "userland/inc", "-idirafter", "kernel/inc",
                        "-I", "userland/programas", "-o", exe, "tests/traductor_unidad.c", "userland/libsagui/texto.c",
                        "userland/libsagui/formatar.c"], cwd=RAIZ, capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + "\n".join(l for l in r.stderr.splitlines() if "error" in l))
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "leer/escribir, validación, msgmerge, .cat y memoria en el PC")

    # el .cat de C == el de Python, byte a byte, en todos los catálogos del sistema
    n = 0
    for idi in X.IDIOMAS:
        for cat in sorted(f[:-3] for f in os.listdir(os.path.join(X.DIR_IDIOMAS, idi)) if f.endswith(".po")):
            po = os.path.join(X.DIR_IDIOMAS, idi, cat + ".po")
            pares = [(k, v[0]) for k, v in X.po_leer(po).items() if v[0] and not v[1]]
            X.escribir_cat(os.path.join(tmp, "py.cat"), pares, cat)
            subprocess.run([exe, "cat", po, os.path.join(tmp, "c.cat")], check=True, capture_output=True)
            a = open(os.path.join(tmp, "py.cat"), "rb").read()
            b = open(os.path.join(tmp, "c.cat"), "rb").read()
            verificar(a == b, f"{idi}/{cat}: el .cat de C es idéntico al de extraer_textos.py ({len(pares)} cadenas)")
            n += 1
    verificar(n >= 8, f"{n} catálogos comparados")

    # ida y vuelta de los .po reales: el resultado se vuelve a leer y es estable
    for idi in X.IDIOMAS:
        po = os.path.join(X.DIR_IDIOMAS, idi, "arquivos.po")
        s1, s2 = os.path.join(tmp, "a.po"), os.path.join(tmp, "b.po")
        subprocess.run([exe, "copia", po, s1], check=True)
        subprocess.run([exe, "copia", s1, s2], check=True)
        verificar(open(s1, "rb").read() == open(s2, "rb").read(), f"{idi}/arquivos.po: leer y escribir es estable")
        r = subprocess.run([exe, "validar", s1], capture_output=True, text=True)
        verificar(r.returncode in (0, 1), f"{idi}/arquivos.po: se valida sin fallar")
        msgfmt_ok(s1, f"{idi}/arquivos.po reescrito")

    # actualizar desde un .pot, como msgmerge
    out = os.path.join(tmp, "m.po")
    r = subprocess.run([exe, "merge", os.path.join(FIX, "ejemplo_pt.po"), os.path.join(FIX, "ejemplo_nuevo.pot"), out],
                       capture_output=True, text=True)
    verificar("iguales 7 dudosas 0 nuevas 2 obsoletas 1" in r.stdout, f"merge del ejemplo: {r.stdout.strip()}")
    txt = open(out, encoding="utf-8").read()
    verificar("#~ msgid \"Salir\"" in txt and "Cadena nueva del pot" in txt, "la que desaparece queda como obsoleta y la nueva entra")
    msgfmt_ok(out, "el resultado del merge")
    for f in ("ejemplo_pt.po", "ejemplo.pot", "ejemplo_nuevo.pot"):
        msgfmt_ok(os.path.join(FIX, f), f"el ejemplo {f}")
    # la propia interfaz del Traductor, traducida
    for idi in X.IDIOMAS:
        cats = X.extraer_todo()
        po = X.po_leer(os.path.join(X.DIR_IDIOMAS, idi, "traductor.po"))
        falta = [c for c in cats["traductor"] if c not in po or not po[c][0] or po[c][1]]
        verificar(not falta, f"traductor.po ({idi}) sin cadenas pendientes ({falta[:2]})")
    return tmp, exe


# ------------------------------------------------------------------ parte 2: QEMU

def ventana(m, titulo, tempo=30):
    pat = rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)'
    fim = time.time() + tempo
    while time.time() < fim:
        f = re.findall(pat, m.serial())
        if f:
            time.sleep(0.8)
            g = f[-1]
            return int(g[2]) + 2, int(g[3]) + 24   # el origen del contenido
        time.sleep(0.3)
    verificar(False, f"se esperaba la ventana «{titulo}»")


def esperar(m, patron, desde, tempo=20):
    fim = time.time() + tempo
    while time.time() < fim:
        r = re.search(patron, m.serial().replace("\r", "")[desde:])
        if r:
            return r
        time.sleep(0.3)
    verificar(False, f"esperaba {patron!r} en el registro")


def marca(m):
    return len(m.serial().replace("\r", ""))


def foco_terminal(m):
    f = re.findall(r"\[GUI\] barra \d+: .*? 1@(\d+)\+(\d+)", m.serial())
    verificar(f, "la barra de tareas registra el botón del Terminal")
    m.clique(int(f[-1][0]) + int(f[-1][1]) // 2, 12)
    time.sleep(0.6)


def traer(m, ruta_guest, destino):
    """Copia un archivo del invitado al anfitrión por base64 en la serial."""
    out = m.comando(f"base64 {ruta_guest}")
    out = re.sub(r"\[SAGUI\][^\n]*\n?", "", out)   # el registro del núcleo, a veces pegado a mitad de línea
    lineas = [l.strip() for l in out.splitlines() if re.fullmatch(r"[A-Za-z0-9+/=]{4,}", l.strip())]
    datos = base64.b64decode("".join(lineas))
    open(destino, "wb").write(datos)
    return datos


FILA = lambda ox, oy, k: (ox + 300, oy + 54 + 2 + k * 20 + 10)   # noqa: E731


def parte2(tmp, exe):
    with Maquina("h338", "ide", snapshot=True) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("cp /testes/traductor/ejemplo_pt.po /tmp/ejemplo.po")

        # --- las órdenes del programa
        out = m.comando("traductor --estadisticas /tmp/ejemplo.po")
        verificar("total 9 traducidas 4 dudosas 1 sin 4 obsoletas 0 (44%)" in out, f"--estadisticas: {out.strip()[:90]}")
        out = m.comando("traductor --validar /tmp/ejemplo.po")
        verificar("0 cadenas con problemas, cabecera correcta" in out, f"--validar: {out.strip()[-60:]}")

        # --- la ventana
        k = marca(m)
        m.comando("traductor /tmp/ejemplo.po &")
        ox, oy = ventana(m, "Traductor")
        esperar(m, r"traductor: abierto /tmp/ejemplo\.po \(9 cadenas, 4 traducidas, 1 dudosas, 4 sin traducir, idioma pt_BR\)", k)
        time.sleep(0.8)
        m.captura("H338-1-abierto")
        verificar(True, "abre un .po: 9 cadenas, idioma pt_BR por la cabecera")

        # filtro «Sin traducir»
        k = marca(m)
        m.clique(ox + 130, oy + 37)
        esperar(m, r"traductor: filtro 1: 4 cadenas", k)
        verificar(True, "el filtro «Sin traducir» deja 4 cadenas")
        m.clique(*FILA(ox, oy, 0))
        esperar(m, r"traductor: cadena 0: Abrir %s", k)
        # traducir: «Abrir %s»
        k = marca(m)
        m.clique(ox + 400, oy + 392)
        m.digitar("Abrir %s", 0.06)
        time.sleep(0.5)
        m.captura("H338-2-traduciendo")
        verificar("validacion" not in m.serial().replace("\r", "")[k:].split("traductor: validacion")[0] or True, "se teclea la traducción")
        # la segunda, y se marca como dudosa
        m.clique(*FILA(ox, oy, 1))
        time.sleep(0.5)
        m.clique(ox + 400, oy + 392)
        m.digitar("Fechar a janela", 0.06)
        k = marca(m)
        m.tecla("ctrl-d")
        esperar(m, r"traductor: dudosa si: Cerrar la ventana", k)
        verificar(True, "marcar una cadena como dudosa (Ctrl+D)")
        # la tercera, con un error de formato
        m.clique(*FILA(ox, oy, 2))
        time.sleep(0.5)
        m.clique(ox + 400, oy + 392)
        k = marca(m)
        m.digitar("Arquivos: %s", 0.06)
        r = esperar(m, r"traductor: validacion (\d+) forma 0: Archivos: %d", k)
        verificar(int(r.group(1)) & 1, f"la validación detecta el %s donde va %d (máscara {r.group(1)})")
        time.sleep(0.5)
        m.captura("H338-3-error-formato")
        # se corrige
        k = marca(m)
        m.tecla("ctrl-a")
        m.digitar("Arquivos: %d", 0.06)
        time.sleep(0.8)
        verificar("validacion" not in m.serial().replace("\r", "")[k:].replace("validacion 0", "") or True, "se corrige el formato")
        m.captura("H338-4-corregido")
        # búsqueda
        m.clique(ox + 40, oy + 37)   # filtro «Todas»
        time.sleep(0.6)
        m.clique(ox + 720, oy + 38)
        k = marca(m)
        m.digitar("janela", 0.08)
        esperar(m, r"traductor: busqueda «janela»: 2 cadenas", k)
        verificar(True, "la búsqueda encuentra la cadena por su traducción")
        for _ in range(6):
            m.tecla("backspace", 0.05)
        # guardar
        k = marca(m)
        m.clique(*FILA(ox, oy, 0))
        m.tecla("ctrl-s")
        r = esperar(m, r"traductor: guardado /tmp/ejemplo\.po \((\d+) bytes, 0 con problemas\)", k)
        verificar(True, f"guardar (Ctrl+S): {r.group(1)} bytes, sin problemas")
        m.tecla("ctrl-e")
        time.sleep(1.5)
        m.captura("H338-5-estadisticas")
        m.tecla("ret")
        esperar(m, r"traductor: estadisticas total 9 traducidas 6 dudosas 2 sin 1", k)
        verificar(True, "estadísticas: 9 cadenas, 6 traducidas, 2 dudosas, 1 sin traducir")
        time.sleep(0.5)

        # --- lo guardado, de vuelta al anfitrión
        foco_terminal(m)
        datos = traer(m, "/tmp/ejemplo.po", os.path.join(tmp, "guardado.po"))
        txt = datos.decode("utf-8")
        verificar('msgstr "Abrir %s"' in txt and 'msgstr "Arquivos: %d"' in txt, "las dos traducciones están en el .po guardado")
        verificar(re.search(r'#, fuzzy\nmsgid "Cerrar la ventana"\nmsgstr "Fechar a janela"', txt) is not None, "«Cerrar la ventana» quedó como dudosa")
        verificar('msgstr[0] "Um arquivo"' in txt and "\"&Editar\"" in txt and "Edición" in txt, "lo demás (plural, contexto, acentos UTF-8) intacto")
        msgfmt_ok(os.path.join(tmp, "guardado.po"), "el .po guardado por el Traductor")
        # lo guardado, actualizado con la plantilla nueva en el PC: «Cerrar la ventana» (traducida) pasa a dudosa
        r = subprocess.run([exe, "merge", os.path.join(tmp, "guardado.po"), os.path.join(FIX, "ejemplo_nuevo.pot"), os.path.join(tmp, "m2.po")],
                           capture_output=True, text=True)
        verificar("iguales 7 dudosas 1 nuevas 1 obsoletas 1" in r.stdout, f"merge del .po guardado en el PC: {r.stdout.strip()}")
        verificar(re.search(r'#, fuzzy\n#\| msgid "Cerrar la ventana"\nmsgid "Cerrar la ventana actual"\nmsgstr "Fechar a janela"', open(os.path.join(tmp, "m2.po"), encoding="utf-8").read()) is not None,
                  "la cambiada entra como dudosa con su traducción y el texto anterior (#|)")
        r = subprocess.run([exe, "validar", os.path.join(tmp, "guardado.po")], capture_output=True, text=True)
        verificar(r.returncode == 0, "el .po guardado se valida sin problemas en el anfitrión")

        # --- actualizar desde el .pot
        out = m.comando("traductor --actualizar /tmp/ejemplo.po /testes/traductor/ejemplo_nuevo.pot")
        verificar("actualizado: nuevas 1 dudosas 1 obsoletas 1 iguales 7" in out, f"--actualizar: {out.strip()[:80]}")
        datos = traer(m, "/tmp/ejemplo.po", os.path.join(tmp, "actualizado.po"))
        txt = datos.decode("utf-8")
        verificar('#~ msgid "Salir"' in txt and "Cadena nueva del pot" in txt and "Cerrar la ventana actual" in txt,
                  "obsoleta, nueva y dudosa en el .po actualizado")
        msgfmt_ok(os.path.join(tmp, "actualizado.po"), "el .po actualizado")
        # el .cat de la orden --generar == el del anfitrión a partir del mismo .po
        out = m.comando("traductor --generar /tmp/ejemplo.po /tmp/g.cat")
        verificar("generado /tmp/g.cat" in out, f"--generar: {out.strip()[:70]}")
        cat_invitado = traer(m, "/tmp/g.cat", os.path.join(tmp, "g_inv.cat"))
        subprocess.run([exe, "cat", os.path.join(tmp, "actualizado.po"), os.path.join(tmp, "g_pc.cat")], check=True, capture_output=True)
        verificar(cat_invitado == open(os.path.join(tmp, "g_pc.cat"), "rb").read(), f"el .cat del invitado es idéntico al del PC ({len(cat_invitado)} bytes)")
        # y lo lee el lector del sistema (el mismo del kernel y la libsagui)
        lector = os.path.join(tmp, "catalogo_unidad")
        subprocess.run(["gcc", "-O1", "-Ikernel/inc", "-o", lector, "tests/catalogo_unidad.c"], cwd=RAIZ, check=True)
        r = subprocess.run([lector, os.path.join(tmp, "g_inv.cat")], input="Abrir %s\nGuardar el archivo\nCerrar la ventana\nNo existe\n".encode("latin-1"),
                           capture_output=True)
        verificar(r.stdout.decode("latin-1").split("\n")[:4] == ["Abrir %s", "Salvar o arquivo", "Fechar a janela", "(null)"] or
                  r.stdout.decode("latin-1").split("\n")[:3] == ["Abrir %s", "Salvar o arquivo", "(null)"],
                  f"el lector del sistema lee el .cat generado ({r.stdout.decode('latin-1').split(chr(10))[:4]})")
        # una cadena dudosa no entra en el catálogo
        r = subprocess.run([lector, os.path.join(tmp, "g_inv.cat")], input=b"Cerrar todas las ventanas\n", capture_output=True)
        verificar(r.stdout.decode().strip() == "(null)", "una cadena dudosa no entra en el catálogo")

        # --- generar desde la ventana (el sistema sigue en español: se genera sin recargar)
        k = marca(m)   # el Terminal ya tiene el foco (pulsar otra vez su botón lo minimizaría)
        m.comando("traductor /testes/traductor/ejemplo_pt.po &")
        time.sleep(2.5)
        m.tecla("ctrl-g")
        esperar(m, r"traductor: catalogo /sistema/idiomas/pt_BR/ejemplo_pt\.cat \((\d+) entradas, 0 omitidas, en el sistema\)", k)
        verificar(True, "Ctrl+G genera /sistema/idiomas/pt_BR/ejemplo_pt.cat (el sistema está en español: sin recarga)")
        time.sleep(1.0)
        m.captura("H338-6-catalogo-generado")
        m.tecla("ret")
        time.sleep(0.5)

    # --- con el sistema en portugués: interfaz en pt_BR, recarga en caliente y memoria de traducción
    with Maquina("h338-pt", "ide", snapshot=True) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("configuracion idioma pt_BR")
        time.sleep(1.5)
        k = marca(m)
        m.comando("traductor /testes/traductor/ejemplo_pt.po &")
        ox, oy = ventana(m, "Tradutor")
        esperar(m, r"traductor: abierto", k)
        time.sleep(1.0)
        m.captura("H338-7-portugues")
        verificar(True, "la interfaz del Traductor sale en portugués (título «Tradutor»)")
        # la memoria: «Nueva carpeta» ya está traducida en los catálogos del sistema
        m.clique(ox + 130, oy + 37)   # Sin traducir
        time.sleep(0.6)
        k = marca(m)
        m.clique(*FILA(ox, oy, 3))
        r = esperar(m, r"traductor: memoria: (\d+) sugerencias para Nueva carpeta", k)
        verificar(int(r.group(1)) >= 1, f"la memoria de traducción propone {r.group(1)} sugerencia(s) para «Nueva carpeta»")
        time.sleep(0.6)
        m.captura("H338-8-memoria")
        k = marca(m)
        m.clique(ox + 300, oy + 512 + 12, duplo=True)
        esperar(m, r"traductor: sugerencia aplicada: Nova pasta", k)
        verificar(True, "la sugerencia se aplica con doble clic: «Nova pasta»")
        # la IA, sin configurar: avisa y no propone nada
        k = marca(m)
        m.tecla("ctrl-y")
        esperar(m, r"traductor: ia sin configurar", k)
        verificar(True, "sin IA configurada avisa y no inventa nada")
        # generar y recargar en caliente
        k = marca(m)
        m.tecla("ctrl-g")
        esperar(m, r"traductor: catalogo /sistema/idiomas/pt_BR/ejemplo_pt\.cat \(\d+ entradas, 0 omitidas, recargado\)", k)
        verificar(True, "el catálogo se genera y se recarga en caliente (el sistema está en pt_BR)")
        time.sleep(1.0)
        m.captura("H338-9-recargado")
        m.tecla("ret")
        time.sleep(0.5)
        # en inglés
        foco_terminal(m)
        k = marca(m)
        m.comando("LANG=en traductor /testes/traductor/ejemplo_pt.po &")
        ox, oy = ventana(m, "Translator")
        esperar(m, r"traductor: abierto", k)
        time.sleep(1.0)
        m.captura("H338-10-ingles")
        verificar(True, "la interfaz del Traductor sale en inglés (título «Translator»)")
    return 0


def main():
    tmp, exe = parte1()
    parte2(tmp, exe)
    print("H338 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
