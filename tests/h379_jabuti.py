#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H379 - Jabuti, programación con bloques para niños (D377, mejora 137).

Parte 0, en el anfitrión: pruebas de unidad del motor (tests/jabuti_unidad.c: números en punto fijo, el texto del
proyecto, el intérprete con todos los bloques, la concurrencia y los siete ejemplos).
Parte 1, sin ventana: los ejemplos se abren y corren sin errores con resultados comprobables (el dibujo con el lápiz
vuelve al punto de partida, la conversación usa la respuesta, el cohete cuenta atrás...), el proyecto se guarda como ZIP
con proyecto.txt y se vuelve a abrir igual.
Parte 2, con ventana: se arrastran bloques de la paleta («al pulsar la bandera verde», «repetir 10», «mover 10 pasos»,
«girar 15 grados») hasta formar el programa, se edita una ranura, se pulsa la bandera y se comprueba la posición final del
personaje en el registro; se guarda con el diálogo, se cierra, se reabre y se repite. Capturas en docs/capturas/H379-*.png.
"""
import math
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA, RAIZ, tamanho_png  # noqa: E402


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "jabuti_unidad")
    d = os.path.join(RAIZ, "userland/jabuti")
    subprocess.run(["gcc", "-O1", "-Wall", "-Wextra", "-Wno-unused-parameter", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland/inc"),
                    "-idirafter", os.path.join(RAIZ, "kernel/inc"), "-I", d, "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-o", exe,
                    os.path.join(RAIZ, "tests/jabuti_unidad.c")] + [os.path.join(d, f) for f in ("jb_datos.c", "jb_modelo.c", "jb_motor.c", "jb_escena.c")] + ["-lm"],
                   check=True)
    r = subprocess.run([exe, os.path.join(RAIZ, "rootfs/ejemplos/jabuti")], capture_output=True)
    salida = r.stdout.decode("latin-1")
    verificar(r.returncode == 0 and "todo bien" in salida, "unidad del motor en el PC: " + repr(salida.strip().splitlines()[-3:]))
    n = re.search(r"(\d+) comprobaciones, (\d+) fallos", salida)
    verificar(n and int(n.group(1)) > 80, "la unidad hace más de 80 comprobaciones (%s)" % (n.group(1) if n else "?"))


def esperar_desde(m, texto, desde, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return True
        time.sleep(0.2)
    verificar(False, "esperaba %r en la serie" % texto)


def janela(m, titulo, desde=0):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
    verificar(bool(g), "ventana %r creada" % titulo)
    g = g[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def borrar(m, n=24):
    for _ in range(n):
        m.tecla("backspace", 0.03)


def lanzar(m, comando):
    pos = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    return pos


def rect(s, patron):
    """Última coincidencia de «patron x=.. y=.. w=.. h=..» en s."""
    r = re.findall(patron + r" x=(-?\d+) y=(-?\d+) w=(-?\d+) h=(-?\d+)", s)
    verificar(bool(r), "en el registro: %s" % patron)
    x, y, w, h = [int(v) for v in r[-1][-4:]]
    return x, y, w, h


def punto(s, patron):
    r = re.findall(patron + r" x=(-?\d+) y=(-?\d+)", s)
    verificar(bool(r), "en el registro: %s" % patron)
    return int(r[-1][-2]), int(r[-1][-1])


def personaje(texto, nombre):
    r = re.findall(r"jabuti: personaje %s x=(-?[\d.]+) y=(-?[\d.]+) dir=(-?[\d.]+) tam=(\d+) vis=(\d) traje=(\d+) decir=([^\n]*)" % re.escape(nombre), texto)
    verificar(bool(r), "estado del personaje %s en el registro" % nombre)
    x, y, d, tam, vis, traje, decir = r[-1]
    return dict(x=float(x), y=float(y), dir=float(d), tam=int(tam), vis=int(vis), traje=int(traje), decir=decir)


def esperado_giros(n=10, paso=10.0, giro=15.0, x=0.0, y=0.0, d=90.0):
    for _ in range(n):
        x += paso * math.sin(math.radians(d))
        y += paso * math.cos(math.radians(d))
        d += giro
    d = (d + 180) % 360 - 180
    return x, y, d


def parte1(m):
    ej = "/ejemplos/jabuti/"
    out = m.comando("ls " + ej, 30)
    for n in ("01-el-sagui-que-salta", "02-atrapa-la-fruta", "03-dibujar-formas", "04-conversacion", "05-carrera-con-mensajes", "06-cuenta-atras-del-cohete", "07-musica-y-trajes"):
        verificar(n + ".jbt" in out, "el ejemplo %s está en /ejemplos/jabuti" % n)
    # todos se abren y corren sin errores
    for n in ("01-el-sagui-que-salta", "02-atrapa-la-fruta", "03-dibujar-formas", "04-conversacion", "05-carrera-con-mensajes", "06-cuenta-atras-del-cohete", "07-musica-y-trajes"):
        out = m.comando("jabuti --ejecutar %s%s.jbt --cuadros 400 --respuesta 7" % (ej, n), 120)
        verificar("jabuti: ejecucion ok" in out and "error" not in out and "aviso" not in out, "%s corre sin errores: %s" % (n, out.strip().splitlines()[-4:]))
    # resultados comprobables
    out = m.comando("jabuti --ejecutar %s03-dibujar-formas.jbt --cuadros 400" % ej, 120)
    p = personaje(out, "Sagui")
    verificar(abs(p["x"] - 120) < 1.5 and abs(p["y"] - 50) < 1.5 and abs(p["dir"] - 90) < 0.5, "las formas del lápiz vuelven al punto de partida (120, 50): %s" % p)
    out = m.comando("jabuti --ejecutar %s04-conversacion.jbt --cuadros 100 --respuesta 7" % ej, 120)
    p = personaje(out, "Sagui")
    verificar(p["decir"].startswith("Dentro de 10 a") and p["decir"].endswith("17"), "la conversación usa la respuesta: %r" % p["decir"])
    verificar("variable edad = 7" in out, "la respuesta queda en la variable edad")
    out = m.comando("jabuti --ejecutar %s06-cuenta-atras-del-cohete.jbt --cuadros 400" % ej, 120)
    p = personaje(out, "Cohete")
    verificar("variable cuenta = 0" in out and p["y"] > 150 and p["traje"] == 1, "el cohete cuenta atrás y despega: %s" % p)
    out = m.comando("jabuti --ejecutar %s05-carrera-con-mensajes.jbt --cuadros 400" % ej, 120)
    verificar(abs(personaje(out, "Pelota")["x"] - 190) < 0.6 and abs(personaje(out, "Pez")["x"] - 190) < 0.6, "los dos corredores llegan a la meta a la vez que el sagui espera")
    out = m.comando("jabuti --ejecutar %s01-el-sagui-que-salta.jbt --cuadros 80 --tecla espacio@10" % ej, 120)
    p = personaje(out, "Sagui")
    verificar(abs(p["y"] + 110) < 0.5, "el sagui salta y vuelve al suelo: y=%s" % p["y"])
    out = m.comando("jabuti --ejecutar %s01-el-sagui-que-salta.jbt --cuadros 120 --tecla derecha@70" % ej, 120)
    verificar(personaje(out, "Sagui")["x"] > 40, "con la flecha derecha pulsada avanza: x=%s" % personaje(out, "Sagui")["x"])
    # guardar como ZIP y volver a abrir
    out = m.comando("jabuti --abrir %s03-dibujar-formas.jbt --guardar /tmp/h379.jbt" % ej, 60)
    verificar("jabuti: guardado /tmp/h379.jbt" in out, "guardado desde la línea de órdenes")
    out = m.comando("unzip -l /tmp/h379.jbt", 30)
    verificar("proyecto.txt" in out and "leeme.txt" in out, "el .jbt es un ZIP con proyecto.txt y leeme.txt: %r" % out[-200:])
    out = m.comando("jabuti --abrir /tmp/h379.jbt --texto", 60)
    verificar("JABUTI 1" in out and "repetir 36" in out and 'lapiz-color "verde"' in out and "jabuti: abrir ok" in out, "el ZIP se abre y el texto del proyecto es legible")
    m.comando("cp /ejemplos/jabuti/03-dibujar-formas.jbt /tmp/viejo.blq", 30)
    out = m.comando("jabuti --abrir /tmp/viejo.blq", 30)
    verificar("jabuti: abrir ok" in out, "la extensión .blq de las primeras versiones sigue abriéndose")
    out = m.comando("jabuti --abrir /ejemplos/jabuti/no-existe.jbt", 30)
    verificar("error al abrir" in out and "no se encuentra" in out, "un proyecto que no existe da un mensaje claro: %r" % out.strip()[-120:])
    m.comando("echo hola > /tmp/malo.jbt", 30)
    out = m.comando("jabuti --abrir /tmp/malo.jbt", 30)
    verificar("error al abrir" in out and "no es un proyecto" in out, "un archivo que no es de Jabuti da un mensaje claro: %r" % out.strip()[-120:])


def parte5(m):
    """Listas desplegables, variables y bloques redondos dentro de huecos, con el ratón y el teclado."""
    pos = lanzar(m, "jabuti")
    esperar_desde(m, "jabuti: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Jabuti", pos)
    s = m.serial()[pos:]
    ax, ay, aw, ah = rect(s, r"jabuti: area")
    k = arrastrar_de_paleta(m, w, 0, "tecla", (ax + 24, ay + 50), pos)
    s = m.serial()[k:]
    mx, my, mw, mh = rect(s, r"jabuti: menu tecla 0")
    # la lista desplegable de la tecla: flecha derecha es la quinta
    k = len(m.serial())
    m.clique(w["x"] + mx + mw // 2, w["y"] + my + mh // 2)
    time.sleep(0.8)
    for _ in range(5):
        m.tecla("down", 0.15)
    m.tecla("ret")
    esperar_desde(m, "jabuti: valor tecla 0 = flecha derecha", k, 20)
    # «mover» debajo del sombrero
    cx, cy = punto(s, r"jabuti: conexion sig tecla")
    k = arrastrar_de_paleta(m, w, 1, "mover", (cx, cy), pos)
    s = m.serial()[k:]
    verificar('tecla "flecha derecha"; mover 10' in s, "el sombrero lleva la tecla elegida y «mover» debajo: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    rx, ry, rw, rh = rect(s, r"jabuti: ranura mover 0")
    # crear una variable con su diálogo
    cxx, cyy, cww, chh = rect(m.serial()[pos:], r"jabuti: categoria 8")
    k = len(m.serial())
    m.clique(w["x"] + cxx + 40, w["y"] + cyy + 14)
    esperar_desde(m, "jabuti: categoria elegida 8", k, 20)
    time.sleep(0.5)
    bx, by, bw_, bh_ = rect(m.serial()[k:], r"jabuti: paleta-boton 1")
    k = len(m.serial())
    m.clique(w["x"] + bx + bw_ // 2, w["y"] + by + bh_ // 2)
    time.sleep(1.2)
    m.digitar("puntos\n", 0.05)
    esperar_desde(m, "jabuti: variable creada puntos", k, 20)
    time.sleep(0.6)
    vx, vy, vw, vh = rect(m.serial()[k:], r"jabuti: paleta var")
    # el bloque «puntos» (redondo) cae dentro del hueco de «mover»
    k = len(m.serial())
    m.arrastar(w["x"] + vx + 14, w["y"] + vy + vh // 2, w["x"] + rx + rw // 2, w["y"] + ry + rh // 2)
    esperar_desde(m, "jabuti: guion 0:", k, 20)
    time.sleep(0.5)
    s = m.serial()[k:]
    verificar('mover (var "puntos")' in s, "la variable cayó dentro del hueco de «mover»: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    m.captura("H379-13-menus-y-variables")
    # el tamaño grande de los bloques (A+): el texto sale al doble
    zx, zy, zw, zh = rect(m.serial()[pos:], r"jabuti: boton zoom\+")
    k = len(m.serial())
    m.clique(w["x"] + zx + zw // 2, w["y"] + zy + zh // 2)
    esperar_desde(m, "jabuti: zoom 2", k, 20)
    time.sleep(0.8)
    m.captura("H379-14-jabuti-grandes")
    m.clique(w["x"] + zx - 40 + 4, w["y"] + zy + zh // 2)     # A-: vuelve al tamaño normal
    time.sleep(0.6)
    # deshacer (Ctrl+Z) devuelve el hueco con su número
    k = len(m.serial())
    m.tecla("ctrl-z")
    esperar_desde(m, "jabuti: deshacer", k, 20)
    time.sleep(0.5)
    cerrar(m, w, pos)


def parte6(m):
    """Importar una imagen, guardar con el traje pintado y medir lo que cuesta un cuadro."""
    out = m.comando("jabuti --importar /imagenes/hojas.png --guardar /tmp/imp.jbt", 90)
    mm = re.search(r"traje con (\d+) píxeles opacos de (\d+) colores", out)
    verificar(mm is not None and int(mm.group(1)) > 100 and int(mm.group(2)) >= 3, "la imagen importada da un traje con píxeles y colores: %r" % ((mm.groups() if mm else out[-200:]),))
    out = m.comando("jabuti --abrir /tmp/imp.jbt --texto", 60)
    filas = re.findall(r"^    ([.0-9a-u]{32})$", out, re.M)
    verificar('personaje "Importado"' in out and 'traje "importado" pintado' in out and len(filas) == 96 and len(set(filas)) > 5, "el traje importado se guarda con sus 32 filas (3 trajes en el proyecto): %d filas, %d distintas" % (len(filas), len(set(filas))))
    out = m.comando("jabuti --medir", 120)
    verificar("jabuti: medida ok" in out, "la medida de rendimiento corre: %r" % out.strip()[-300:])
    for l in re.findall(r"jabuti: medida [^\n]*", out):
        print("  medida:", l.strip())


def cerrar(m, w, desde):
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    time.sleep(1.0)
    if "jabuti: fechado" not in m.serial()[desde:]:
        m.tecla("ret")
        time.sleep(0.8)
    esperar_desde(m, "jabuti: fechado", desde, 20)


def arrastrar_de_paleta(m, w, cat, clave, destino, k):
    """Elige la categoría, localiza el bloque en la paleta y lo suelta de modo que su esquina caiga en destino."""
    cx, cy, cw, ch = rect(m.serial()[k:], r"jabuti: categoria %d" % cat)
    k2 = len(m.serial())
    m.clique(w["x"] + cx + 40, w["y"] + cy + 14)
    esperar_desde(m, "jabuti: categoria elegida %d" % cat, k2, 20)
    time.sleep(0.5)
    bx, by, bw, bh = rect(m.serial()[k2:], r"jabuti: paleta %s" % clave)
    k3 = len(m.serial())
    oy = bh // 2 if bh < 24 else 12
    m.arrastar(w["x"] + bx + 14, w["y"] + by + oy, w["x"] + destino[0] + 14, w["y"] + destino[1] + oy)
    esperar_desde(m, "jabuti: bloque %s" % clave, k3, 20)
    time.sleep(0.5)
    return k3


def parte2(m):
    pos = lanzar(m, "jabuti")
    esperar_desde(m, "jabuti: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Jabuti", pos)
    verificar(w["l"] == 780 and w["a"] == 520, "ventana de Jabuti de 780x520 (cabe en 800x600): %dx%d" % (w["l"], w["a"]))
    s = m.serial()[pos:]
    ax, ay, aw, ah = rect(s, r"jabuti: area")
    ex, ey, ew, eh = rect(s, r"jabuti: escenario")
    verificar(aw >= 220 and ew >= 160 and ex > ax + aw, "las tres zonas caben en 800x600: área %dx%d, escenario %dx%d" % (aw, ah, ew, eh))
    bx, by, bw_, bh_ = rect(s, r"jabuti: boton bandera")
    m.captura("H379-1-inicio")

    # 1) al pulsar la bandera verde
    k = arrastrar_de_paleta(m, w, 0, "bandera", (ax + 24, ay + 50), pos)
    s = m.serial()[k:]
    verificar("jabuti: guion 0: bandera" in s, "el sombrero «bandera» está en el área de programa: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    hx, hy, hw, hh = rect(s, r"jabuti: bloque bandera")
    m.captura("H379-2-paleta-y-sombrero")
    # 2) repetir (Control): debajo del sombrero
    cx, cy = punto(s, r"jabuti: conexion sig bandera")
    k = arrastrar_de_paleta(m, w, 5, "repetir", (cx, cy), pos)
    s = m.serial()[k:]
    verificar("bandera; repetir 10 {" in s, "«repetir 10» encajó debajo de la bandera: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    # 3) mover (Movimiento): dentro del brazo de repetir
    cx, cy = punto(s, r"jabuti: conexion c1 repetir")
    k = arrastrar_de_paleta(m, w, 1, "mover", (cx, cy), pos)
    s = m.serial()[k:]
    verificar("repetir 10 {mover 10}" in s, "«mover 10» encajó dentro de «repetir»: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    # 4) girar a la derecha: debajo de mover
    cx, cy = punto(s, r"jabuti: conexion sig mover")
    k = arrastrar_de_paleta(m, w, 1, "girar-der", (cx, cy), pos)
    s = m.serial()[k:]
    verificar("repetir 10 {mover 10; girar-der 15}" in s, "«girar 15 grados» encajó debajo de «mover»: %r" % re.findall(r"jabuti: guion[^\n]*", s))
    m.captura("H379-3-programa-armado")
    # editar la ranura de «repetir»: 10 -> 12 -> 10 (comprueba la edición en el sitio)
    rx, ry, rw, rh = rect(s, r"jabuti: ranura repetir 0")
    k = len(m.serial())
    m.clique(w["x"] + rx + rw // 2, w["y"] + ry + rh // 2)
    time.sleep(0.5)
    borrar(m, 3)
    m.digitar("12", 0.05)
    m.tecla("ret")
    esperar_desde(m, "jabuti: editado repetir 0 = 12", k, 20)
    k = len(m.serial())
    m.clique(w["x"] + rx + 8, w["y"] + ry + rh // 2)
    time.sleep(0.5)
    borrar(m, 4)
    m.digitar("10", 0.05)
    m.tecla("ret")
    esperar_desde(m, "jabuti: editado repetir 0 = 10", k, 20)
    time.sleep(0.5)
    # la bandera verde: el personaje se mueve
    k = len(m.serial())
    m.clique(w["x"] + bx + bw_ // 2, w["y"] + by + bh_ // 2)
    esperar_desde(m, "jabuti: fin", k, 40)
    time.sleep(0.8)
    p = personaje(m.serial()[k:], "Sagui")
    ex_, ey_, ed_ = esperado_giros()
    verificar(abs(p["x"] - ex_) < 0.3 and abs(p["y"] - ey_) < 0.3, "posición final tras repetir 10 {mover 10; girar 15}: (%s, %s) esperado (%.2f, %.2f)" % (p["x"], p["y"], ex_, ey_))
    verificar(abs(p["dir"] - ed_) < 0.1, "dirección final %s esperado %.1f" % (p["dir"], ed_))
    m.captura("H379-4-ejecutado")

    # guardar con el diálogo
    k = len(m.serial())
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    borrar(m)
    m.digitar("/tmp/h379b\n", 0.05)
    esperar_desde(m, "jabuti: guardado /tmp/h379b.jbt", k, 30)
    time.sleep(0.8)
    # «Guardar como» (Ctrl+Mayús+S) con extensión .txt: el programa como texto legible
    k = len(m.serial())
    m.tecla("ctrl-shift-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    borrar(m)
    m.digitar("/tmp/h379t.txt\n", 0.05)
    esperar_desde(m, "jabuti: texto guardado /tmp/h379t.txt", k, 30)
    time.sleep(0.8)
    # cerrar y reabrir
    cerrar(m, w, pos)
    out = m.comando("cat /tmp/h379t.txt", 30)
    verificar("JABUTI 1" in out and "repetir 10" in out and "girar-der 15" in out, "el texto legible tiene el programa")
    out = m.comando("jabuti --abrir /tmp/h379t.txt", 30)
    verificar("jabuti: abrir ok" in out and "1 guiones" in out, "el texto legible se vuelve a abrir: %r" % out.strip()[-160:])
    pos2 = lanzar(m, "jabuti /tmp/h379b.jbt")
    esperar_desde(m, "jabuti: pronto", pos2, 60)
    esperar_desde(m, "jabuti: abierto /tmp/h379b.jbt (1 personajes)", pos2, 20)
    time.sleep(1.5)
    w = janela(m, "Jabuti", pos2)
    s = m.serial()[pos2:]
    verificar("bandera; repetir 10 {mover 10; girar-der 15}" in s, "el proyecto guardado se reabre con el mismo programa")
    ps = personaje(s, "Sagui")
    verificar(abs(ps["x"] - ex_) < 0.3 and abs(ps["y"] - ey_) < 0.3 and abs(ps["dir"] - ed_) < 0.1, "el estado del personaje se guarda y se recupera: %s" % ps)
    bx, by, bw_, bh_ = rect(s, r"jabuti: boton bandera")
    k = len(m.serial())
    m.clique(w["x"] + bx + bw_ // 2, w["y"] + by + bh_ // 2)
    esperar_desde(m, "jabuti: fin", k, 40)
    time.sleep(0.8)
    p2 = personaje(m.serial()[k:], "Sagui")
    e2 = esperado_giros(x=ps["x"], y=ps["y"], d=ps["dir"])
    verificar(abs(p2["x"] - e2[0]) < 0.4 and abs(p2["y"] - e2[1]) < 0.4, "tras reabrir, ejecutar da la posición esperada: (%s, %s) esperado (%.2f, %.2f)" % (p2["x"], p2["y"], e2[0], e2[1]))
    m.captura("H379-5-reabierto")

    # el editor de trajes: pintar un píxel
    k = len(m.serial())
    m.clique(w["x"] + 6 + 92 + 3 + 30, w["y"] + 46 + 12)
    time.sleep(0.8)
    m.captura("H379-6-trajes")
    cerrar(m, w, pos2)


def parte3(m):
    """Los ejemplos con ventana: se abren desde la línea de órdenes, se pulsa la bandera y se capturan."""
    for i, (n, cuadros) in enumerate((("01-el-sagui-que-salta", 1.5), ("02-atrapa-la-fruta", 4), ("03-dibujar-formas", 5), ("04-conversacion", 1.5))):
        pos = lanzar(m, "jabuti /ejemplos/jabuti/%s.jbt" % n)
        esperar_desde(m, "jabuti: pronto", pos, 60)
        time.sleep(1.5)
        w = janela(m, "Jabuti", pos)
        s = m.serial()[pos:]
        bx, by, bw_, bh_ = rect(s, r"jabuti: boton bandera")
        m.clique(w["x"] + bx + bw_ // 2, w["y"] + by + bh_ // 2)
        time.sleep(cuadros)
        if n.startswith("04"):
            m.digitar("Ana\n", 0.05)
            time.sleep(1.5)
        verificar("error" not in m.serial()[pos:], "el ejemplo %s corre en la ventana sin errores" % n)
        m.captura("H379-%d-ejemplo-%s" % (7 + i, n))
        cerrar(m, w, pos)
        time.sleep(0.5)


def parte4(m):
    """800x600 (equipos antiguos): la ventana cabe entera; la pantalla completa del escenario."""
    out = m.comando("pantalla 800x600", 60)
    verificar("Pantalla cambiada a 800x600." in out, "pantalla 800x600")
    time.sleep(1)
    pos = lanzar(m, "jabuti /ejemplos/jabuti/05-carrera-con-mensajes.jbt")
    esperar_desde(m, "jabuti: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Jabuti", pos)
    s = m.serial()[pos:]
    ax, ay, aw, ah = rect(s, r"jabuti: area")
    ex, ey, ew, eh = rect(s, r"jabuti: escenario")
    verificar(w["x"] >= 0 and w["x"] + w["l"] + 2 <= 800 and w["y"] + w["a"] + 2 <= 600, "la ventana cabe en 800x600: (%d,%d) %dx%d" % (w["x"], w["y"], w["l"], w["a"]))
    verificar(aw >= 220 and ex + ew <= w["l"], "área de programa %dx%d y escenario %dx%d dentro de la ventana" % (aw, ah, ew, eh))
    bx, by, bw_, bh_ = rect(s, r"jabuti: boton bandera")
    m.clique(w["x"] + bx + bw_ // 2, w["y"] + by + bh_ // 2)
    time.sleep(2.0)
    png = m.captura("H379-11-800x600")
    verificar(tamanho_png(png) == (800, 600), "captura de 800x600")
    # pantalla completa del escenario
    px, py, pw, ph = rect(s, r"jabuti: boton pantalla")
    k = len(m.serial())
    m.clique(w["x"] + px + pw // 2, w["y"] + py + ph // 2)
    esperar_desde(m, "jabuti: pantalla completa 1", k, 20)
    time.sleep(1.0)
    m.captura("H379-12-pantalla-completa")
    k = len(m.serial())
    m.tecla("f11")
    esperar_desde(m, "jabuti: pantalla completa 0", k, 20)
    time.sleep(1.0)
    cerrar(m, w, pos)
    out = m.comando("pantalla 1024x768", 60)
    verificar("Pantalla cambiada a 1024x768." in out, "de vuelta a 1024x768")


def main():
    unidad()
    solo = os.environ.get("H379_SOLO", "")
    with Maquina("h379", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        if not solo or solo == "1":
            parte1(m)
        if not solo or solo == "2":
            parte2(m)
        if not solo or solo == "3":
            parte3(m)
        if not solo or solo == "4":
            parte4(m)
        if not solo or solo == "5":
            parte5(m)
        if not solo or solo == "6":
            parte6(m)
    print("H379 OK")


if __name__ == "__main__":
    main()
