#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H304 - Bases de datos en SaguiOS: la biblioteca `basedatos`, los componentes de VisualPythonS y el
Explorador de bases de datos (mejora 127, D304).

Criterio:
  1. una base SQLite de ejemplo con datos (/ejemplos/bd/crear_ejemplo.py);
  2. la autoprueba de `basedatos` y de ConectorBD + RejillaDatos + controles enlazados (con la agenda de
     ejemplo): CRUD con parámetros enlazados, transacciones, paginación, orden, filtro, edición de celdas;
  3. el Explorador: abrir una base (sin ejecutar nada), el árbol, los datos, editar una celda y
     comprobarlo en la base, la pestaña Estructura, una consulta SQL con su resultado, la confirmación de
     un DELETE sin WHERE, exportar a CSV y abrirlo en Macaco;
  4. lite-monkey: el Explorador ofrece arrancar el servicio, se insertan documentos con Python y se ven;
  5. la agenda de VisualPythonS (ConectorBD + RejillaDatos + ficha enlazada): se edita en la ficha, se
     cambia de fila y queda guardado en la base.
Capturas: docs/capturas/H304-*.png
"""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA, RAIZ, BUILD, opcoes_apps  # noqa: E402

BORDA, TITULO = 2, 24
EJEMPLO = "/tmp/ejemplo.db"


def janela(m, titulo, desde=0, tempo=60):
    """La última ventana con ese título (empieza por): dict con la posición del contenido."""
    fim = time.time() + tempo
    while time.time() < fim:
        achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
        if achada:
            g = achada[-1]
            return dict(l=int(g[0]), a=int(g[1]), fx=int(g[2]), fy=int(g[3]), x=int(g[2]) + BORDA, y=int(g[3]) + TITULO)
        time.sleep(0.3)
    print(m.serial()[desde:][-2500:])
    m.captura("H304-fallo", guardar=False)
    raise AssertionError(f"no apareció la ventana {titulo!r}")


def limpio(t):
    """El serial sin las líneas de órdenes del shell, que se cuelan en medio de lo que escribe un programa
    en segundo plano."""
    return re.sub(r"(?:(?:root|sagui)@saguios|sagui):/[#$] \x1b?\[K\r?\n?", "", t)


def esperar(m, texto, desde=0, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in limpio(m.serial()[desde:]):
            return True
        time.sleep(0.3)
    print(m.serial()[desde:][-2500:])
    m.captura("H304-fallo", guardar=False)
    raise AssertionError(f"tiempo agotado esperando {texto!r}")


def cerrar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def sonda(m, w):
    """F12 en el Explorador: {clave: [campos]} con las posiciones que imprime, ya en pantalla (la sonda las da
    relativas al contenido de la ventana `w`)."""
    marca = len(m.serial())
    m.tecla("f12", 0.8)
    esperar(m, "sonda: fin", marca, 20)
    s = limpio(m.serial()[marca:])
    s = s[s.rfind("vps-sonda: inicio"):]
    r = {}
    rej = None
    for linea in s.splitlines():
        if not linea.startswith("sonda: ") or linea.startswith("sonda: fin"):
            continue
        p = linea[len("sonda: "):].rstrip("\r").split("|")
        k = p[0]
        pos = {"nodo": (2, 3), "boton": (2, 3), "cab": (2, 3), "celda": (3, 4), "texto": (1, 2), "arbol": (1, 2)}.get(k)
        if pos:
            p[pos[0]] = str(int(p[pos[0]]) + w["x"])
            p[pos[1]] = str(int(p[pos[1]]) + w["y"])
        if k.startswith("rejilla"):
            rej = k
            r.setdefault("_" + k, {})
        elif k == "celda":
            r["_" + rej][(int(p[1]), int(p[2]))] = p[5] if len(p) > 5 else ""
            if rej == "rejilla":
                r.setdefault("celda", []).append(p[1:])
        elif k == "cab":
            if rej == "rejilla":
                r.setdefault("cab", []).append(p[1:])
        else:
            r.setdefault(k, []).append(p[1:])
    return r


def celda(s, i, j):
    for p in s["celda"]:
        if int(p[0]) == i and int(p[1]) == j:
            return int(p[2]), int(p[3]), p[4] if len(p) > 4 else ""
    raise AssertionError(f"la sonda no tiene la celda ({i}, {j})")


def celda_de(s, rejilla, i, j):
    """El texto de una celda de otra rejilla (rejilla_res, rejilla_cols...): se localiza por orden."""
    # la sonda imprime las celdas de cada rejilla seguidas; se reparten por el orden en que salen
    return s["_" + rejilla][(i, j)]


def boton(s, texto):
    for p in s["boton"]:
        if p[0] == texto:
            return int(p[1]), int(p[2])
    raise AssertionError(f"la sonda no tiene el botón {texto!r}: {[b[0] for b in s['boton']]}")


def python_c(m, codigo, tempo=60):
    """Ejecuta `python -c` en el terminal y devuelve lo que imprime."""
    return m.comando("python -c \"%s\"" % codigo, tempo)


def imagen_con_home():
    """La imagen normal con un disco de datos que se monta en /home (como h88): el servicio lite-monkey guarda
    ahí sus datos (/home/litemonkey) y, sin /home, no puede arrancar."""
    sgfs = os.path.join(SAIDA, "h304.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), os.path.join(BUILD, "rootfs"), sgfs],
                   check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h304.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", "32", *opcoes_apps()],   # D324: python vive en /apps
                   check=True, stdout=subprocess.DEVNULL)
    return img


def principal():
    os.makedirs(SAIDA, exist_ok=True)
    with Maquina("h304", "ide", imagem=imagen_con_home()) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. la base de ejemplo
        out = m.comando("python /ejemplos/bd/crear_ejemplo.py %s" % EJEMPLO, 90)
        verificar("contactos (60 filas) y productos (4 filas)" in out, "el ejemplo crea la base SQLite")

        # ---------------------------------------------------------- 2. la autoprueba
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/autoprueba.py\n", 0.03)
        esperar(m, "bd-autoprueba: fin", marca, 170)
        s = re.sub(r"\[SAGUI\][^\n]*\n", "", m.serial()[marca:])       # el registro del kernel parte alguna línea
        fallos = re.findall(r"bd-autoprueba: FALLO (.*)", s)
        oks = re.findall(r"bd-autoprueba: ok (.*)", s)
        verificar(not fallos, "la autoprueba no tiene fallos: %s" % fallos)
        verificar(len(oks) >= 50, "la autoprueba hizo %d comprobaciones" % len(oks))
        for clave in ("un valor hostil es solo un dato", "la transaccion con error se deshace", "editar una celda escribe en la base",
                      "paginacion: ir_a(60)", "guardar() inserta", "agenda-bd: elegir una fila"):
            verificar(any(clave in o for o in oks), "autoprueba: " + clave)
        print("H304: %d comprobaciones de la autoprueba" % len(oks))

        # ---------------------------------------------------------- 3. el Explorador con SQLite
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/explorador.py %s &\n" % EJEMPLO, 0.03)
        ex = janela(m, "Explorador de bases de datos", marca)
        esperar(m, "exploradorbd: Abierta ejemplo.db (2 tablas)", marca)
        time.sleep(1.5)
        m.captura("H304-1-explorador")
        s = sonda(m, ex)
        verificar(any("contactos" in n[3] for n in s["nodo"]) and any("productos" in n[3] for n in s["nodo"]),
                  "el árbol enseña las dos tablas de la base abierta")

        def nodo(s, texto):
            for n in s["nodo"]:
                if texto in n[3]:
                    return int(n[1]), int(n[2])
            raise AssertionError("no hay el nodo " + texto)

        # elegir «contactos»: sus datos
        marca = len(m.serial())
        m.clique(*nodo(s, "contactos"))
        esperar(m, "exploradorbd: contactos: 60 filas", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar(celda(s, 0, 1)[2] == "Ana 1" and celda(s, 1, 2)[2] == "Sevilla", "la pestaña Datos enseña la tabla (60 filas)")
        m.captura("H304-2-datos")
        # doble clic: se despliegan las columnas y los índices
        m.clique(*nodo(s, "contactos"), duplo=True)
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(any("nombre  TEXT" in n[3] for n in s["nodo"]) and any(n[3].startswith("id  INTEGER") for n in s["nodo"]),
                  "doble clic en una tabla: el árbol enseña sus columnas")
        m.captura("H304-3-arbol")

        # editar una celda: Ana 1 -> Ana Editada
        marca = len(m.serial())
        x, y, _ = celda(s, 0, 1)
        m.clique(x, y, duplo=True)
        time.sleep(0.6)
        m.tecla("ctrl-a", 0.1)
        m.digitar("Ana Editada", 0.08)
        m.tecla("ret", 0.5)
        esperar(m, "fila actualizada (nombre = Ana Editada)", marca)
        # ordenar por «nombre»: la primera fila pasa a ser «Ana 2»
        s = sonda(m, ex)
        cab = [c for c in s["cab"] if c[0] == "nombre"][0]
        m.clique(int(cab[1]), int(cab[2]))
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(celda(s, 0, 1)[2] == "Ana 2", "un clic en la cabecera ordena por esa columna (y la celda editada cambió de sitio)")
        m.captura("H304-4-editar-ordenar")

        # la pestaña Estructura
        m.clique(*boton(s, "Estructura"))
        time.sleep(1.0)
        s = sonda(m, ex)
        verificar(celda_de(s, "rejilla_cols", 0, 0) == "id" and celda_de(s, "rejilla_cols", 1, 1) == "TEXT", "la pestaña Estructura enseña las columnas y sus tipos")
        m.captura("H304-5-estructura")

        # la pestaña Consulta: SQL con resultado
        m.clique(*boton(s, "Consulta"))
        time.sleep(0.8)
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*[int(v) for v in s["texto"][0]])
        m.tecla("ctrl-a", 0.1)
        m.digitar("SELECT ciudad, count(*) AS n FROM contactos GROUP BY ciudad ORDER BY ciudad", 0.05)
        time.sleep(0.5)
        m.tecla("f5", 0.5)
        esperar(m, "exploradorbd: 4 filas en", marca)
        time.sleep(0.6)
        s = sonda(m, ex)
        verificar(celda_de(s, "rejilla_res", 0, 0) == "Bogota" and celda_de(s, "rejilla_res", 0, 1) == "15", "la consulta SQL enseña su resultado en la rejilla")
        m.captura("H304-6-consulta")

        # un DELETE sin WHERE pide confirmación; se contesta que no
        marca = len(m.serial())
        m.clique(*[int(v) for v in s["texto"][0]])
        m.tecla("ctrl-a", 0.1)
        m.digitar("DELETE FROM productos", 0.05)
        m.tecla("f5", 0.5)
        dlg = janela(m, "Confirmar", marca)
        time.sleep(0.8)
        m.captura("H304-7-confirmar")
        m.tecla("esc", 0.8)
        esperar(m, "exploradorbd: Consulta cancelada", marca)

        # exportar el resultado de la consulta a CSV y abrirlo en Macaco
        s = sonda(m, ex)
        marca = len(m.serial())
        m.clique(*boton(s, "Exportar resultado..."))
        janela(m, "Exportar a CSV", marca)
        time.sleep(0.8)
        m.tecla("ctrl-a", 0.1)
        m.digitar("/tmp/ciudades.csv", 0.06)
        m.tecla("ret", 1.0)
        janela(m, "Exportar", marca)                      # «¿Abrirlo en Macaco?»
        m.tecla("ret", 1.0)
        esperar(m, "exploradorbd: Abriendo /tmp/ciudades.csv en Macaco", marca)
        mac = janela(m, "Macaco", marca, 90)
        time.sleep(3)
        m.captura("H304-8-macaco")
        cerrar(m, mac)
        time.sleep(1.5)
        cerrar(m, ex)
        time.sleep(1.5)
        out = m.comando("cat /tmp/ciudades.csv")
        verificar("ciudad,n" in out and "Bogota,15" in out and "Sevilla,15" in out, "el CSV exportado tiene las columnas y los datos")
        out = python_c(m, "import basedatos;b=basedatos.conectar('sqlite:%s');print('EDIT=',b.consultar('contactos',{'id':1})[0]['nombre']);print('PROD=',b.contar('productos'))" % EJEMPLO)
        verificar("EDIT= Ana Editada" in out, "la celda editada en el Explorador está en la base")
        verificar("PROD= 4" in out, "el DELETE sin WHERE que se canceló no borró nada")

        # ---------------------------------------------------------- 4. lite-monkey
        out = m.comando("servicio listar")
        verificar(not re.search(r"^litemonkey\s+(en marcha|activo|corriendo)", out, re.M), "lite-monkey no está arrancado de serie")
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/explorador.py litemonkey://127.0.0.1:7979/tienda &\n", 0.03)
        ex2 = janela(m, "Explorador de bases de datos", marca)
        janela(m, "lite-monkey", marca)                                  # «no está arrancado: ¿arrancarlo?»
        time.sleep(0.8)
        m.captura("H304-10a-lmk-arrancar")
        m.tecla("ret", 1.0)                                              # Sí
        esperar(m, "exploradorbd: Arrancando el servicio litemonkey", marca)
        esperar(m, "exploradorbd: Abierta 127.0.0.1:7979/tienda", marca, 120)
        verificar(True, "el Explorador ofrece arrancar el servicio y conecta")
        time.sleep(1)
        cerrar(m, ex2)
        time.sleep(1.5)
        out = m.comando("servicio listar")
        print(out)
        out = m.comando("lmk --base tienda --eval \"db.productos.insertMany([{'nombre':'cafe','precio':4.5,'tags':['bebida']},{'nombre':'te','precio':3.2},{'nombre':'pan','precio':1.2}])\"", 60)
        out = m.comando("lmk --base tienda --eval \"db.productos.countDocuments({})\"", 60)
        verificar("3" in out, "lmk insertó tres documentos")
        marca = len(m.serial())
        m.digitar("python /lib/python/basedatos/explorador.py litemonkey://127.0.0.1:7979/tienda &\n", 0.03)
        ex3 = janela(m, "Explorador de bases de datos", marca)
        esperar(m, "exploradorbd: Abierta 127.0.0.1:7979/tienda (1 tablas)", marca, 60)
        time.sleep(1.5)
        s = sonda(m, ex3)
        marca = len(m.serial())
        m.clique(*nodo(s, "productos"))
        esperar(m, "exploradorbd: productos: 3 filas", marca)
        time.sleep(0.6)
        s = sonda(m, ex3)
        textos = list(s["_rejilla"].values())
        verificar("cafe" in textos and "te" in textos and "pan" in textos, "los documentos insertados con lmk salen en el Explorador: %s" % textos[:12])
        cols = [c[0] for c in s["cab"]]
        verificar(cols[0] == "_id" and "nombre" in cols and "precio" in cols and "tags" in cols, "las columnas son la unión de los campos: %s" % cols)
        m.captura("H304-10-lmk")
        m.clique(*boton(s, "Estructura"))
        time.sleep(1.0)
        s = sonda(m, ex3)
        tipos = list(s["_rejilla_cols"].values())
        verificar("entero|real" in tipos or "real" in tipos, "la pestaña Estructura enseña los campos deducidos: %s" % tipos)
        m.captura("H304-11-lmk-estructura")
        cerrar(m, ex3)
        time.sleep(1.5)

        # ---------------------------------------------------------- 5. la agenda de VisualPythonS
        m.comando("rm -f /home/agenda.db /tmp/agenda.db")
        marca = len(m.serial())
        m.digitar("python /ejemplos/vps/agenda-bd/lanzador.py &\n", 0.03)
        ag = janela(m, "Agenda de contactos", marca)
        time.sleep(3)
        gx, gy = ag["x"] + 8, ag["y"] + 8
        m.clique(gx + 40, gy + 20 + 18 + 9)                              # la segunda fila: Beto Lopez
        time.sleep(1.0)
        m.captura("H304-9-agenda")
        m.clique(ag["x"] + 8 + 2 + 84 + 85, ag["y"] + 216 + 2 + 8 + 14 + 11)   # el cuadro «Nombre» de la ficha
        time.sleep(0.4)
        m.tecla("ctrl-a", 0.1)
        m.digitar("Beto Editado", 0.08)
        time.sleep(0.4)
        m.clique(gx + 40, gy + 20 + 36 + 9)                              # otra fila: guarda lo escrito
        time.sleep(1.2)
        m.captura("H304-12-agenda-guardada")
        cerrar(m, ag)
        time.sleep(1.5)
        out = python_c(m, "import os,basedatos;p='/home/agenda.db' if os.path.isdir('/home') else '/tmp/agenda.db';b=basedatos.conectar('sqlite:'+p);print('FICHA=',b.consultar('contactos',{'id':2})[0]['nombre'])")
        verificar("FICHA= Beto Editado" in out, "lo escrito en la ficha enlazada se guardó en la base al cambiar de fila")


if __name__ == "__main__":
    principal()
    print("H304: OK")
