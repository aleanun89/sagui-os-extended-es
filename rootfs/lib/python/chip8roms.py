# chip8roms.py - lógica común de chip8-roms (terminal) y chip8-roms-gui (D123).
#
# Descarga las ROM del Chip8 Community Archive de John Earnest
# (https://github.com/JohnEarnest/chip8Archive, carpeta roms/), cuyo contenido
# está bajo la licencia CC0. SaguiOS NO incluye ninguna: se bajan al ejecutar.
#
# El índice es programs.json (un diccionario clave -> título, autores, desc,
# evento, fecha, plataforma y opciones). Cada ROM está en roms/<clave>.ch8.
# Con otra URL base (--url) se usa un espejo o un servidor de pruebas.
import os
import sys
import json
import time

try:
    import requests
except ImportError:
    requests = None

URL_BASE = "https://raw.githubusercontent.com/JohnEarnest/chip8Archive/master/"
LICENCIA = "CC0, Chip8 Community Archive"
EXTENSION = {"chip8": ".ch8", "schip": ".sc8", "xochip": ".xo8"}
PLATAFORMAS = ("chip8", "schip", "xochip")

# Opciones del índice que el emulador SaguiOS no tiene (se avisa al jugar).
SIN_SOPORTE = {
    "fillColor": "color", "fillColor2": "color", "blendColor": "color",
    "backgroundColor": "color", "buzzColor": "color", "quietColor": "color",
    "vfOrderQuirks": "orden de VF", "screenRotation": "rotacion de pantalla",
    "maxSize": "tamano maximo", "displayScale": "escala", "fontStyle": "tipografia",
    "touchInputMode": "entrada tactil", "enableXO": "extensiones XO",
}


# ------------------------------------------------------------ carpetas

def _existe(ruta):
    try:
        os.stat(ruta)
        return True
    except OSError:
        return False


def _crear(ruta):
    """mkdir -p."""
    partes = [p for p in ruta.split("/") if p]
    actual = ""
    for p in partes:
        actual += "/" + p
        if not _existe(actual):
            os.mkdir(actual)


def carpeta_base(forzada=None):
    """(carpeta, persistente): /home/chip8 si /home existe (disco montado) y
    /tmp/chip8 si no, donde todo se pierde al apagar."""
    if forzada:
        return forzada.rstrip("/"), True
    try:
        if "home" in os.listdir("/"):
            return "/home/chip8", True
    except OSError:
        pass
    return "/tmp/chip8", False


def carpeta_roms(forzada=None):
    base, _ = carpeta_base(forzada)
    return base + "/roms"


def aviso_persistencia(forzada=None):
    """Texto de aviso si las ROM se perderán, o None."""
    base, persistente = carpeta_base(forzada)
    if persistente:
        return None
    return ("Aviso: /home no es persistente; las ROM se guardan en %s y se perderán al apagar." % base)


# ------------------------------------------------------------ red

def _base(url):
    if not url:
        return URL_BASE
    return url if url.endswith("/") else url + "/"


def _bajar_bytes(url, intentos=3, espera=1, timeout=30, aviso=None):
    if requests is None:
        raise OSError("no esta el modulo requests")
    ultimo = None
    for n in range(1, intentos + 1):
        try:
            r = requests.get(url, timeout=timeout)
            if r.status_code == 200:
                return r.content
            ultimo = "HTTP %s" % r.status_code
            if r.status_code == 404:
                break               # reintentar no arregla un 404
        except Exception as e:      # noqa: BLE001
            ultimo = str(e) or e.__class__.__name__
        if n < intentos:
            if aviso:
                aviso("reintento %d de %d (%s)" % (n, intentos - 1, ultimo))
            time.sleep(espera)
    raise OSError("%s: %s" % (url, ultimo))


def clave_valida(clave):
    if not clave or len(clave) > 60:
        return False
    for c in clave:
        if not (c.isalpha() or c.isdigit() or c in "_-") or ord(c) > 127:
            return False
    return True


# ------------------------------------------------------------ índice

def ruta_indice(forzada=None):
    return carpeta_base(forzada)[0] + "/programs.json"


def cargar_indice(url=None, forzar=False, forzada=None, aviso=None):
    """Devuelve el diccionario del índice; lo baja si no está en caché o con
    forzar=True, y si no hay red usa la caché."""
    ruta = ruta_indice(forzada)
    datos = None
    if forzar or not _existe(ruta):
        try:
            datos = _bajar_bytes(_base(url) + "programs.json", aviso=aviso)
            _crear(carpeta_base(forzada)[0])
            with open(ruta, "wb") as f:
                f.write(datos)
        except OSError:
            if not _existe(ruta):
                raise
            if aviso:
                aviso("sin red: uso el indice en cache")
            datos = None
    if datos is None:
        with open(ruta, "rb") as f:
            datos = f.read()
    return json.loads(datos.decode("utf-8"))


def plataforma(prog):
    p = prog.get("platform", "chip8")
    return p if p in EXTENSION else "chip8"


def _ascii(s):
    return "".join(c if ord(c) < 128 else "?" for c in str(s))


def ruta_rom(clave, prog, forzada=None):
    return carpeta_roms(forzada) + "/" + clave + EXTENSION[plataforma(prog)]


def esta_bajada(clave, prog, forzada=None):
    return _existe(ruta_rom(clave, prog, forzada))


def filtrar(indice, plat=None, texto=None):
    """Lista ordenada de (clave, programa) por título, filtrada."""
    t = texto.lower() if texto else None
    res = []
    for clave in indice:
        p = indice[clave]
        if plat and plataforma(p) != plat:
            continue
        if t:
            h = (clave + " " + p.get("title", "") + " " + " ".join(p.get("authors", [])) + " " + p.get("desc", "")).lower()
            if t not in h:
                continue
        res.append((clave, p))
    res.sort(key=lambda x: x[1].get("title", x[0]).lower())
    return res


def autores(prog):
    return ", ".join(prog.get("authors", [])) or "?"


# ------------------------------------------------------------ descarga

def bajar_rom(clave, prog, url=None, forzada=None, aviso=None):
    """Baja <clave> a la carpeta de ROM con su .json (metadatos y atribución).
    Devuelve la ruta de la ROM."""
    if not clave_valida(clave):
        raise OSError("clave no valida: %r" % clave)
    datos = _bajar_bytes(_base(url) + "roms/" + clave + ".ch8", aviso=aviso)
    if not datos:
        raise OSError("%s: ROM vacia" % clave)
    _crear(carpeta_roms(forzada))
    ruta = ruta_rom(clave, prog, forzada)
    with open(ruta, "wb") as f:
        f.write(datos)
    meta = {
        "clave": clave,
        "title": prog.get("title", clave),
        "autores": prog.get("authors", []),
        "desc": prog.get("desc", ""),
        "evento": prog.get("event", ""),
        "fecha": prog.get("release", ""),
        "plataforma": plataforma(prog),
        "opciones": prog.get("options", {}),
        "licencia": LICENCIA,
        "atribucion": "%s - %s" % (autores(prog), LICENCIA),
        "origen": _base(url) + "roms/" + clave + ".ch8",
        "bytes": len(datos),
    }
    with open(carpeta_roms(forzada) + "/" + clave + ".json", "w") as f:
        # una línea "titulo" con ese formato exacto: la lee el selector de chip8
        titulo = _ascii(prog.get("title", clave)).replace("\\", "").replace('"', "'")
        f.write('{"titulo": "%s",\n' % titulo)
        resto = json.dumps(meta)
        f.write(resto[1:] + "\n")
    return ruta


# ------------------------------------------------------------ emulador

def _si(v):
    return v is True or str(v).lower() == "true"


def opciones_emulador(prog):
    """Traduce las opciones del índice a las de /apps/bin/chip8.
    Devuelve (argumentos, avisos)."""
    o = prog.get("options", {})
    args = ["-m", plataforma(prog)]
    avisos = []
    if "tickrate" in o:
        try:
            args += ["-v", str(int(o["tickrate"]))]
        except ValueError:
            avisos.append("tickrate no valido: %r" % (o["tickrate"],))
    quirks = (
        ("shiftQuirks", "desplazar", "vx", "vy"),
        ("loadStoreQuirks", "memoria", "fija", "incrementa"),
        ("jumpQuirks", "salto", "vx", "v0"),
        ("clipQuirks", "borde", "recorta", "envuelve"),
        ("logicQuirks", "logicas", "reinicia", "conserva"),
        ("vBlankQuirks", "vblank", "espera", "no"),
    )
    for clave, nombre, si, no in quirks:
        if clave in o:
            args += ["-q", "%s=%s" % (nombre, si if _si(o[clave]) else no)]
    sin = []
    for clave in sorted(o):
        if clave in SIN_SOPORTE and clave != "enableXO":
            sin.append(clave)
    if sin:
        avisos.append("el emulador no soporta: " + ", ".join(sin))
    return args, avisos


def linea_emulador(clave, prog, forzada=None):
    args, avisos = opciones_emulador(prog)
    return "chip8 " + " ".join(args) + " " + ruta_rom(clave, prog, forzada), avisos


def jugar(clave, prog, forzada=None, fondo=False):
    """Lanza el emulador; con fondo=True no espera (la ventana gráfica sigue viva)."""
    linea, avisos = linea_emulador(clave, prog, forzada)
    return os.system(linea + (" &" if fondo else "")), linea, avisos
