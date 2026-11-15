#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""EMERGENCIA - batería rápida para comprobar una versión antes de probarla en hardware real.

NO es una prueba hNN y NO entra en la batería (rodar_*.py solo recogen h<dos dígitos>*.py y b<dígito>_*.py).
Arranca la imagen, en serie y una máquina cada vez (cada una envuelta con tools/turno.sh):

  1. 86Box, preset p2-450-voodoo3500 (Pentium II 450, Voodoo3 3500, SB16, RTL8139)
  2. 86Box, preset p3-rage          (placa de P3 con Deschutes, ATI Mach64, ES1371)
  3. QEMU con 1 núcleo y 512 MB (KVM si /dev/kvm es accesible; si no, TCG)

En cada una mide el tiempo hasta el escritorio y recoge CPU, RAM, GPU y 2D, sonido, red, discos (DMA y
caché), /apps, USB, teclado, y abre pluma, macaco, paint, python y la Configuración desde /apps/bin.
Al final saca una tabla resumen. Con --referencia (una imagen anterior) repite todo con ella y marca como
REGRESIÓN lo que antes estaba bien y ahora no, con las líneas de ambas versiones.

    . ~/.local/saguitools/env.sh
    python3 tests/emergencia.py [--imagen RUTA] [--referencia RUTA] [--salida DIR] [--maquinas a,b,c]

Salida 0 si todo bien; 1 si algún elemento falla o hay regresión. Logs en build/emergencia/ (o --salida):
<maquina>-<etiqueta>.json (todo lo recogido), .txt (órdenes y salida) y -serie.txt (el serie entero),
más las capturas. Las ROM de 86Box no se copian a ninguna parte (las lee emu86box de ~/.local/saguitools).
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, AQUI)

MAQUINAS = ["p2-450-voodoo3500", "p3-rage", "qemu-1n-512m"]
TEXTO = ["infopc -t", "mount", "df", "acel2d", "configuracion aceleracion", "sonido", "ifconfig", "free",
         "ls /apps/bin", "which pluma", "which python", "python --version"]
APPS = ["pluma", "macaco", "paint", "python", "configuracion"]   # las ventanas, al final (roban el foco)


# ---------------------------------------------------------------- recogida (una máquina)
def _linea(texto, rx, grupo=0):
    m = re.search(rx, texto, re.M)
    return m.group(grupo).strip() if m else ""


def analizar(c):
    """Del serie y de las órdenes recogidas, los elementos detectados (cadenas) y sus comprobaciones."""
    s, o = c["serie"], c["ordenes"]
    info = o.get("infopc -t", "")
    d = {}
    d["escritorio"] = f"{c['t_escritorio']:.0f} s" if c.get("t_escritorio") else ""
    kern = _linea(s, r"\[ARRANQUE\] escritorio: (\d+) ms", 1)
    d["escritorio_kernel"] = f"{int(kern) / 1000:.1f} s" if kern else ""
    d["cpu"] = " | ".join(x for x in (_linea(info, r"^Microarquitectura: (.*)"), _linea(info, r"^Frecuencia \(TSC\): (\d+)", 1) + " MHz"
                                      if _linea(info, r"^Frecuencia \(TSC\): (\d+)", 1) else "",
                                      _linea(info, r"^Fabricante: +(\S+)")) if x)
    d["ram"] = " | ".join(x for x in (_linea(info, r"^RAM usable detectada: +(.*)"), _linea(info, r"^Memoria: +(libre .*)")) if x)
    d["gpu"] = " | ".join(x for x in (_linea(info, r"^Producto: +(.*)"), _linea(info, r"^Resolución: +(.*)"),
                                      _linea(info, r"^Modo gráfico: +(.*)")) if x)
    ac = _linea(info, r"^Aceleración 2D: +(.*)")
    d["2d"] = ac[:90]
    d["3d"] = _linea(info, r"^ +3D por GPU: +(.*)")[:90]
    d["sonido"] = " | ".join(x for x in (_linea(info, r"^Sonido: +(.*)"), _linea(info, r"^ +Códec: +(.*)")) if x)
    d["red"] = _linea(o.get("ifconfig", ""), r"^eth0: (.*)")
    ata = re.findall(r"\[ATA\] [^\n]*(?:setores|sem LBA|DMA)[^\n]*|Discos internos ATA: \d+|\[CACHE\][^\n]*", s)
    d["disco"] = " ; ".join(a.strip() for a in ata)[:300]
    d["acel_disco"] = _linea(o.get("configuracion aceleracion", ""), r"disco \(DMA y caché\) (\w+)")
    mnt = o.get("mount", "")
    d["montajes"] = " ; ".join(l.strip() for l in mnt.splitlines() if l.strip() and not l.startswith(("mount", "[SAGUI]", "sagui")))[:300]
    d["apps"] = "montado" if re.search(r"/apps\b", mnt) else ""
    d["which_pluma"] = _linea(o.get("which pluma", ""), r"^(/\S+)$")
    d["usb"] = " | ".join(_linea(info, rx) for rx in (r"^ +(Controladores USB 2\.0[^\n]*)", r"^ +(Controladores USB 1\.1[^\n]*)") if _linea(info, rx))
    d["teclado_raton"] = " | ".join(x for x in (_linea(info, r"^Teclado: +(.*)"),
                                                _linea(s, r"^(\[[^\]]*\][^\n]*(?:PS/2|ratón|raton|mouse)[^\n]*)")) if x)[:200]
    d["python"] = _linea(o.get("python --version", ""), r"^(MicroPython[^\n]*)")
    for a in APPS:
        sl = c.get("lanz", {}).get(a, "")
        r = re.search(r"executar (/\S*/%s) -> processo (\d+)" % a, sl)
        ok = False
        if r:
            ok = re.search(r"janela \d+ criada por %s:" % r.group(2), sl) is not None or a == "python"
        d["app_" + a] = (r.group(1) + (" (ventana)" if ok and a != "python" else "")) if r and ok else ""
    d = {k: re.sub(r" {2,}", " ", v) for k, v in d.items()}
    d["excepciones"] = "SI" if re.search(r"EXCECAO|PANICO|PÁNICO", s) else ""
    return d


# elemento -> (descripción, función que dice si está bien, a partir de d y de la máquina)
def comprobaciones(d, maquina):
    en_vm = not maquina.startswith("qemu")
    return [
        ("escritorio", bool(d["escritorio"])),
        ("cpu", bool(d["cpu"])),
        ("ram", bool(d["ram"])),
        ("gpu y modo", bool(d["gpu"])),
        ("2D activo", bool(d["2d"]) and not re.search(r"software|CPU\)|desactivad|no disponible", d["2d"], re.I)
         if en_vm else bool(d["2d"])),
        ("sonido", bool(d["sonido"]) and "ninguno" not in d["sonido"].lower() if en_vm else True),
        ("red", bool(d["red"])),
        ("disco IDE", bool(re.search(r"setores", d["disco"])) and "sem LBA" not in d["disco"]),
        ("disco con DMA y caché", "DMA (Bus Master" in d["disco"] and "[CACHE] caché de bloques activa" in d["disco"]),
        ("/apps montado", d["apps"] == "montado"),
        ("which pluma = /apps/bin", d["which_pluma"] == "/apps/bin/pluma"),
        ("abre pluma", bool(d["app_pluma"])),
        ("abre macaco", bool(d["app_macaco"])),
        ("abre paint", bool(d["app_paint"])),
        ("python", bool(d["python"]) or bool(d["app_python"])),
        ("abre Configuración", bool(d["app_configuracion"])),
        ("sin excepciones", not d["excepciones"]),
    ]


def recoger(maquina, imagen, etiqueta, salida):
    """Arranca una máquina, recoge todo y escribe <salida>/<maquina>-<etiqueta>.json (+ .txt, serie, capturas)."""
    nombre = f"{maquina}-{etiqueta}"
    base = os.path.join(salida, nombre)
    os.makedirs(salida, exist_ok=True)
    log = open(base + ".txt", "w", encoding="utf-8")

    def W(t):
        log.write(t + "\n")
        log.flush()
        print(t, flush=True)

    c = {"maquina": maquina, "imagen": imagen, "ordenes": {}, "t_escritorio": None, "lanz": {}}
    if maquina.startswith("qemu"):
        import qemu
        if os.access("/dev/kvm", os.R_OK | os.W_OK):
            os.environ["SAGUI_KVM"] = "1"
        c["kvm"] = os.environ.get("SAGUI_KVM") == "1"
        m = qemu.Maquina(f"emerg-{etiqueta}", imagem=imagen, memoria="512M", nucleos=1)
        esperar_desktop, tmax = 150, 120
        bios = lambda: None  # noqa: E731
    else:
        import emu86box
        emu86box.TECLAS_SIGNOS.update({"(": "shift+8", ")": "shift+9", '"': "shift+2"})
        m = emu86box.Maquina86Box(maquina, imagen, nome=f"emerg-{nombre}", salida=f"/tmp/emerg-{nombre}")
        esperar_desktop, tmax = 240, 30
        bios = lambda: m.pasar_bios(tempo=90)  # noqa: E731
    t0 = time.time()
    with m:
        try:
            bios()
            for marca, tmo in (("area de trabalho no ar", esperar_desktop), ("sagui:", tmax)):
                try:
                    m.esperar_serial(marca, tmo)
                    if marca != "sagui:":
                        c["t_escritorio"] = time.time() - t0
                    W(f"{marca!r}: {time.time() - t0:.0f} s")
                except Exception as e:  # noqa: BLE001
                    W(f"NO apareció {marca!r}: {e}")
            time.sleep(3)
            _captura(m, f"{nombre}-escritorio", salida)
            for orden in (TEXTO if c["t_escritorio"] else []):   # sin escritorio no hay terminal: no se pierde el tiempo
                try:
                    out = m.comando(orden, 120 if orden == "infopc -t" else 60)
                    c["ordenes"][orden] = out
                    W(f"$ {orden}\n{out}")
                except Exception as e:  # noqa: BLE001
                    c["ordenes"][orden] = ""
                    W(f"$ {orden}  FALLO: {e}")
                    time.sleep(2)
            for app in (APPS if c["t_escritorio"] else []):
                orden = {"python": "python --version", "configuracion": "configuracion"}.get(app, app)
                if app == "python":
                    continue   # ya ejecutado como orden de texto: lo da «python --version»
                antes = len(m.serial())
                try:
                    m.comando(orden, 20)
                except Exception:  # noqa: BLE001 - una ventana puede no devolver el prompt: se mira el serie
                    pass
                time.sleep(3)
                c["lanz"][app] = m.serial()[antes:]
                W(f"abrir {app}: " + (_linea(c["lanz"][app], r"executar (/\S*/%s) -> processo \d+" % app, 0) or "sin rastro en el serie"))
                _captura(m, f"{nombre}-{app}", salida)
                # la ventana se queda con el foco: se cierra con Alt+F4 para que el siguiente arranque llegue al shell
                if not re.search(r"janela \d+ criada por", c["lanz"][app]):
                    continue   # no se abrió ventana: Alt+F4 cerraría el Terminal
                try:
                    m.tecla("alt-f4" if maquina.startswith("qemu") else "alt+F4")
                except Exception:  # noqa: BLE001
                    pass
                time.sleep(1)
        except Exception as e:  # noqa: BLE001
            W(f"FALLO: {e}")
        c["serie"] = m.serial().replace("\x00", "")
        c["serie_bruto"] = m.serial_bruto().replace("\x00", "")
    c["t_total"] = time.time() - t0
    with open(base + "-serie.txt", "w", encoding="utf-8", errors="replace") as f:
        f.write(c["serie_bruto"])
    del c["serie_bruto"]
    c["detectado"] = analizar(c)
    c["checks"] = [[k, bool(v)] for k, v in comprobaciones(c["detectado"], maquina)]
    with open(base + ".json", "w", encoding="utf-8") as f:
        json.dump(c, f, ensure_ascii=False, indent=1)
    W(f"total {c['t_total']:.0f} s")
    return 0


def _captura(m, nombre, salida):
    try:
        ruta = m.captura(nombre, guardar=False) if hasattr(m, "dir_capturas") else m.captura(nombre, guardar=False)
        if ruta and os.path.exists(ruta):
            subprocess.run(["cp", ruta, os.path.join(salida, nombre + ".png")], check=False)
    except Exception as e:  # noqa: BLE001
        print("  (sin captura:", e, ")", flush=True)


# ---------------------------------------------------------------- orquestador
def lanzar(maquina, imagen, etiqueta, salida):
    """Una máquina en un subproceso envuelto con turno.sh (así cuenta como una más para el coordinador)."""
    cmd = [os.path.join(RAIZ, "tools", "turno.sh"), sys.executable, os.path.abspath(__file__), "--una", maquina,
           "--imagen", imagen, "--etiqueta", etiqueta, "--salida", salida]
    env = dict(os.environ)
    env.setdefault("SAGUI_TURNOS", "2")
    subprocess.run(cmd, env=env, cwd=RAIZ)
    ruta = os.path.join(salida, f"{maquina}-{etiqueta}.json")
    if os.path.exists(ruta):
        with open(ruta, encoding="utf-8") as f:
            return json.load(f)
    return None


def tabla(resultados, referencia):
    """Escribe la tabla resumen y devuelve (texto, fallos, regresiones)."""
    L, fallos, regr = [], [], []
    for maq, cur in resultados.items():
        ref = referencia.get(maq) if referencia else None
        L.append(f"\n== {maq}   imagen: {os.path.basename(cur['imagen']) if cur else '?'}")
        if not cur:
            L.append("  NO SE RECOGIÓ NADA (la máquina no arrancó o el trabajador falló)")
            fallos.append((maq, "máquina", "sin datos"))
            continue
        refck = dict(ref["checks"]) if ref else {}
        L.append(f"  {'elemento':26} {'ahora':7} " + ("ref.    " if ref else "") + "detectado")
        dkey = {"escritorio": "escritorio", "cpu": "cpu", "ram": "ram", "gpu y modo": "gpu", "2D activo": "2d",
                "sonido": "sonido", "red": "red", "disco IDE": "disco", "disco con DMA y caché": "disco", "/apps montado": "montajes",
                "which pluma = /apps/bin": "which_pluma", "abre pluma": "app_pluma", "abre macaco": "app_macaco",
                "abre paint": "app_paint", "python": "python", "abre Configuración": "app_configuracion",
                "sin excepciones": "excepciones"}
        for nombre, ok in cur["checks"]:
            r = ""
            if ref:
                r = ("ok" if refck.get(nombre) else "FALLA").ljust(8)
            marca = "ok" if ok else "FALLA"
            if not ok:
                fallos.append((maq, nombre, cur["detectado"].get(dkey[nombre], "")))
            if ref and refck.get(nombre) and not ok:
                marca = "REGRES"
                regr.append((maq, nombre))
            det = cur["detectado"].get(dkey[nombre], "") if nombre != "sin excepciones" else ("sí" if cur["detectado"]["excepciones"] else "no")
            L.append(f"  {nombre:26} {marca:7} {r}{det[:110]}")
        L.append(f"  tiempo hasta el escritorio: {cur['detectado']['escritorio'] or 'NO LLEGÓ'} (kernel: {cur['detectado']['escritorio_kernel'] or '?'})"
                 + (f"   (referencia: {ref['detectado']['escritorio'] or 'NO LLEGÓ'})" if ref else ""))
        if ref:
            for k in ("cpu", "ram", "gpu", "2d", "3d", "sonido", "red", "disco", "usb", "teclado_raton", "montajes"):
                a, b = ref["detectado"].get(k, ""), cur["detectado"].get(k, "")
                if a != b:
                    L.append(f"  diff {k}:\n      ref:   {a}\n      ahora: {b}")
    return "\n".join(L), fallos, regr


def principal():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--imagen", default=os.path.join(RAIZ, "build", "saguios.img"))
    ap.add_argument("--referencia", default="", help="imagen anterior para el diff de lo detectado")
    ap.add_argument("--salida", default=os.path.join(RAIZ, "build", "emergencia"))
    ap.add_argument("--maquinas", default=",".join(MAQUINAS))
    ap.add_argument("--una", default="", help="(interno) recoge solo esta máquina")
    ap.add_argument("--etiqueta", default="ahora", help="(interno)")
    ap.add_argument("--resumen", action="store_true", help="no arranca nada: rehace la tabla con los json de --salida")
    a = ap.parse_args()
    a.salida = os.path.abspath(a.salida)
    a.imagen = os.path.abspath(a.imagen)
    if a.una:
        return recoger(a.una, a.imagen, a.etiqueta, a.salida)
    if not os.path.exists(a.imagen):
        print("no existe la imagen", a.imagen)
        return 1
    maquinas = [x for x in a.maquinas.split(",") if x]
    res, ref = {}, {}
    for maq in maquinas:
        if a.referencia:
            print(f"### {maq} con la referencia {a.referencia}", flush=True)
            ref[maq] = (_leer(a.salida, maq, "ref") if a.resumen else lanzar(maq, os.path.abspath(a.referencia), "ref", a.salida))
        print(f"### {maq} con {a.imagen}", flush=True)
        res[maq] = _leer(a.salida, maq, "ahora") if a.resumen else lanzar(maq, a.imagen, "ahora", a.salida)
    texto, fallos, regr = tabla(res, ref if a.referencia else None)
    print("\n================ RESUMEN DE LA BATERÍA DE EMERGENCIA ================" + texto)
    with open(os.path.join(a.salida, "resumen.txt"), "w", encoding="utf-8") as f:
        f.write(texto + "\n")
    print(f"\nfallos: {len(fallos)}   regresiones: {len(regr)}   (logs en {a.salida})")
    for r in regr:
        print("  REGRESIÓN:", r[0], "-", r[1])
    return 1 if (fallos or regr) else 0


def _leer(salida, maq, etiqueta):
    ruta = os.path.join(salida, f"{maq}-{etiqueta}.json")
    if os.path.exists(ruta):
        with open(ruta, encoding="utf-8") as f:
            return json.load(f)
    return None


if __name__ == "__main__":
    sys.exit(principal())
