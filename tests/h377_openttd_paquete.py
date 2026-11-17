#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H377 - OpenTTD como paquete aparte (D375, prueba de concepto): se instala en /apps y arranca.

OpenTTD (GPL-2.0 solo) vive en otro repositorio y no va en las imágenes públicas. Esta prueba usa el
paquete que deja tools/openttd_paquete.sh (en SAGUI_PAQUETES, por defecto build/paquetes, o el que diga
H377_PAQUETE); sin paquete se salta con un aviso.

Partes (por defecto todas; o las que se pidan: h377_openttd_paquete.py paquete licencia os):
  paquete   el manifiesto (formato 1, licencia GPL-2.0-only, origen con commit, datos libres con SHA-256),
            las sumas de todos los ficheros, que va el ejecutable, la LICENSE y los baseset de OpenGFX,
            OpenSFX y OpenMSX; que tools/paquete_instalar.py lo extrae en una carpeta de /apps.
  licencia  docs/openttd.md, la decisión D375 y las filas de docs/terceros.md.
  os        en SaguiOS: se instala en una copia de build/apps/raiz, imagen privada de prueba (nunca la
            pública), Inicio > Juegos > OpenTTD, el menú principal (capturas H377-*.png; son capturas
            del juego, no código de OpenTTD), «New Game» y partida rápida generada.
"""
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

PAQUETES = os.environ.get("SAGUI_PAQUETES") or os.path.join(RAIZ, "build", "paquetes")


def buscar_paquete():
    p = os.environ.get("H377_PAQUETE")
    if p:
        return p if os.path.isfile(p) else None
    if os.path.isdir(PAQUETES):
        c = sorted((os.path.join(PAQUETES, f) for f in os.listdir(PAQUETES)
                    if f.startswith("openttd-") and f.endswith("-saguios.tar")), key=os.path.getmtime)
        if c:
            return c[-1]
    return None


def parte_paquete(paquete):
    from qemu import verificar
    t = tarfile.open(paquete)
    man = json.loads(t.extractfile("sagui-paquete.json").read().decode("utf-8"))
    verificar(man["formato"] == 1 and man["nombre"] == "openttd", "manifiesto de formato 1 y nombre openttd")
    verificar(man["licencia"] == "GPL-2.0-only", "licencia GPL-2.0-only declarada")
    verificar(len(man["origen"]["commit"]) == 40 and man["origen"]["url"].startswith("https://github.com/OpenTTD/"),
              "origen exacto (URL y commit de 40 caracteres)")
    verificar(man["instalacion"]["destino"] == "/apps" and man["menu"]["grupo"] == "Juegos"
              and man["menu"]["ejecutable"] == "/apps/bin/openttd", "dónde se instala y entrada de menú")
    nombres = {d["nombre"] for d in man["datos_externos"]}
    verificar(nombres == {"OpenGFX", "OpenSFX", "OpenMSX"} and all(len(d["sha256"]) == 64 for d in man["datos_externos"]),
              "datos libres OpenGFX, OpenSFX y OpenMSX con SHA-256")
    rutas = {f["ruta"] for f in man["ficheros"]}
    for r in ("apps/bin/openttd", "LICENSE", "ORIGEN-FUENTES.txt", "icono.png"):
        verificar(r in rutas, "el paquete lleva " + r)
    for pref in ("apps/openttd/baseset/opengfx-", "apps/openttd/baseset/opensfx-"):
        verificar(any(r.startswith(pref) for r in rutas), "baseset " + pref.split("/")[-1])
    verificar("apps/openttd/baseset/openmsx-0.4.2/openmsx.obm" in rutas,
              "OpenMSX va extraído en una carpeta (OpenTTD 1.10 no busca la música dentro de un tar)")
    elf = t.extractfile("apps/bin/openttd").read(20)
    verificar(elf[:4] == b"\x7fELF" and elf[4] == 1, "el ejecutable es un ELF de 32 bits")
    verificar(b"version 2" in t.extractfile("LICENSE").read(), "LICENSE es la GPL versión 2")
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "paquete_instalar.py"), "--info", paquete],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "paquete_instalar --info lee el paquete")
    d = tempfile.mkdtemp(prefix="h377-")
    try:
        r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "paquete_instalar.py"), paquete, d],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, "paquete_instalar extrae y comprueba las sumas: " + r.stdout.strip() + r.stderr.strip())
        verificar(os.path.isfile(os.path.join(d, "bin", "openttd")) and os.access(os.path.join(d, "bin", "openttd"), os.X_OK),
                  "/apps/bin/openttd queda ejecutable")
    finally:
        shutil.rmtree(d, ignore_errors=True)
    print("  ok: paquete %s (%d ficheros)" % (man["paquete"], len(man["ficheros"])))


def parte_licencia():
    from qemu import verificar
    p = os.path.join(RAIZ, "docs", "openttd.md")
    verificar(os.path.isfile(p), "docs/openttd.md existe")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D375" in d and "paquete" in d.split("## D375", 1)[1].split("\n## D", 1)[0].lower(),
              "D375 en docs/decisoes.md menciona el paquete")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("OpenTTD" in t, "docs/terceros.md registra OpenTTD")
    s = open(os.path.join(RAIZ, "tools", "openttd_paquete.sh"), encoding="utf-8").read()
    # El Makefile no compila nada de OpenTTD: solo el SDK externo y, en las imágenes PRIVADAS (priv_descargas),
    # la llamada a la receta que genera e instala el paquete en input_share/apps (D375).
    mk = [l for l in open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read().lower().splitlines()
          if "openttd" in l.replace("sdk_externo", "")]
    permitidas = ("openttd_paquete.sh", "openttd_repo", "openttd_url", "# d375", "@# d375", "@# aparte", "openttd (gpl-2.0")
    sobran = [l for l in mk if not any(p in l for p in permitidas)]
    verificar(not sobran, "el Makefile no compila OpenTTD (solo el SDK externo y la receta en priv_descargas)" + ("".join("\n    " + l for l in sobran)))
    verificar("OPENTTD_URL" in s, "la receta admite el repositorio público por OPENTTD_URL")
    print("  ok: documentación y decisión registradas")


def parte_os(paquete):
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, cor_do_pixel
    from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui
    if not os.path.isdir(os.path.join(APPS_RAIZ, "bin")):
        print("AVISO: no hay build/apps/raiz (make): se salta la parte «os»")
        return
    apps = tempfile.mkdtemp(prefix="h377-apps-")
    try:
        shutil.rmtree(apps)
        shutil.copytree(APPS_RAIZ, apps)
        subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "paquete_instalar.py"), paquete, apps], check=True,
                       stdout=subprocess.DEVNULL)
        img = imagem_de_teste("", ["--vesa", "--apps", apps])
        with Maquina("h377-openttd", "ide", imagem=img, memoria=os.environ.get("H377_MEM", "512M")) as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            marca = len(m.serial())
            m.clique(40, 12)
            p = painel(m, marca)
            sub = submenu_pelo_mouse(m, p, "Juegos", "OpenTTD")
            verificar("OpenTTD" in sub["linhas"], "Inicio > Juegos tiene OpenTTD (%s)" % ", ".join(sub["linhas"]))
            time.sleep(0.4)
            m.captura("H377-1-menu-juegos")
            marca = len(m.serial())
            clicar_aqui(m)
            esperar(m, r"inicio -> /apps/bin/openttd", 30, desde=marca)
            esperar(m, r"\[SDL2\] primer cuadro dibujado", 240, desde=marca)
            time.sleep(8)
            png = m.captura("H377-2-menu-principal")
            azul = cor_do_pixel(png, 226, 146)   # el agua del fondo del menú, donde luego irá la barra de herramientas
            ser = m.serial()[marca:]
            verificar("Failed to find" not in ser and "terminou com codigo" not in ser,
                      "OpenTTD encuentra OpenGFX, OpenSFX y OpenMSX y sigue en marcha")
            # «New Game» (la ventana de 640x480 nace en (218,110): el botón queda en (459,294))
            m.clique(*PUNTOS["nueva"])
            time.sleep(5)
            m.captura("H377-3-nueva-partida")
            # «Generate»: mapa de 256x256 (sin hilos: el juego se queda generando y luego abre la partida)
            m.clique(*PUNTOS["generar"])
            for _ in range(40):
                time.sleep(5)
                png = m.captura("H377-4-partida")
                # la barra de herramientas del juego (botón de pausa, naranja) sustituye al azul del menú
                if cor_do_pixel(png, 226, 146) != azul:
                    break
            else:
                verificar(False, "la partida se genera y aparece la barra de herramientas")
            time.sleep(6)
            m.captura("H377-4-partida")
            ser = m.serial()[marca:]
            verificar("terminou com codigo" not in ser, "OpenTTD sigue en marcha con la partida (no se cae)")
            verificar("falta de pagina" not in ser.lower() and "page fault" not in ser.lower(), "sin faltas de página")
            print("  ok: menú principal, partida nueva generada y en marcha")
    finally:
        shutil.rmtree(apps, ignore_errors=True)


# Coordenadas (pantalla) con la ventana de 640x480 en (218,110) del escritorio de 1024x768.
PUNTOS = {"nueva": (459, 294), "generar": (660, 447)}


if __name__ == "__main__":
    partes = sys.argv[1:] or ["paquete", "licencia", "os"]
    paquete = buscar_paquete()
    for p in partes:
        print("== " + p)
        if p == "licencia":
            parte_licencia()
        elif paquete is None:
            print("AVISO: no hay paquete de OpenTTD (tools/openttd_paquete.sh): se salta la parte «%s»" % p)
        elif p == "paquete":
            parte_paquete(paquete)
        elif p == "os":
            parte_os(paquete)
    print("H377 ok")
