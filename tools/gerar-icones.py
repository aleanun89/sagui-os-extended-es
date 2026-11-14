#!/usr/bin/env python3
"""gerar-icones.py - genera la biblioteca de iconos del SaguiOS (D135).

Baja los SVG de los iconos "Fluent UI System Icons" de Microsoft (licencia MIT,
servidos por Iconify) y los rasteriza a pixel art de 16x16, en el formato del
proyecto: filas de 16 caracteres con una paleta compartida. Escribe:

  - userland/libsagui/icones.c      (la tabla de iconos y la paleta)
  - userland/terceros/fluentui/ICONES.md  (la lista de iconos usados y su origen)

No hace falta ImageMagick ni librerías de SVG: el rasterizador es de aquí
(muestreo 4x4 por píxel, relleno por regla par-impar, curvas y arcos aplanados
y degradados lineales y radiales).

Uso:
  python3 tools/gerar-icones.py            # genera los ficheros
  python3 tools/gerar-icones.py --ver      # sólo comprueba, no escribe nada
  python3 tools/gerar-icones.py --limpar   # borra la caché de los SVG
"""
import argparse
import math
import os
import re
import sys
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAIDA_C = os.path.join(RAIZ, "userland", "libsagui", "icones.c")
SAIDA_LISTA = os.path.join(RAIZ, "userland", "terceros", "fluentui", "ICONES.md")
CACHE = os.path.join(RAIZ, "build", "icones", "svg")

TAM = 16          # lado del pixel art, en píxeles
MUESTRAS = 4      # muestras por píxel y lado (16 por píxel)
UMBRAL = 0.5      # cobertura mínima para que un píxel esté pintado

# La API de Iconify responde 404 si la petición no lleva un User-Agent de
# navegador: hay que mandar uno.
AGENTE = "Mozilla/5.0 (X11; Linux x86_64)"

# La paleta que comparten todos los iconos. 'P' es el contorno oscuro (como en
# los iconos del escritorio) y el resto son los tonos de relleno; '.' es
# transparente (lo entiende g_desenho de la libsagui).
PALETA = [
    ("P", 0x202428),   # contorno
    ("D", 0x4A5460),   # gris oscuro
    ("B", 0xFFFFFF),   # blanco
    ("C", 0x9AA3AC),   # gris
    ("L", 0xE3E8EE),   # gris claro
    ("V", 0x2A6FD8),   # azul
    ("T", 0x22B8D6),   # cian
    ("G", 0x3CDC6E),   # verde
    ("Y", 0xFFE070),   # amarillo
    ("A", 0xF2C230),   # ámbar
    ("O", 0xF2762D),   # naranja
    ("R", 0xE05252),   # rojo
    ("M", 0x9B6BD6),   # morado
    ("K", 0x8A6239),   # marrón
]
# El contorno no se usa como tono de relleno: un color muy oscuro se mapea al
# gris oscuro 'D' y así el contorno sigue viéndose.
TONOS = [p for p in PALETA if p[0] != "P"]

# Iconos de la biblioteca: (clave, prefijo de Iconify, nombre, color de relleno
# para los que son monocromos). La clave es el nombre con el que se buscan
# desde un programa; los identificadores van en portugués.
ICONES = [
    # ---- sistema y escritorio (a color) ----
    ("inicio", "fluent-color", "home-16", None),
    ("casa", "fluent-color", "building-home-16", None),
    ("edificio", "fluent-color", "building-16", None),
    ("aplicativos", "fluent-color", "apps-16", None),
    ("quadro", "fluent-color", "board-16", None),
    ("ajustes", "fluent-color", "settings-16", None),
    ("pessoas", "fluent-color", "people-16", None),
    ("grupo", "fluent-color", "people-team-16", None),
    ("pessoa", "fluent-color", "person-16", None),
    ("pessoa-nova", "fluent-color", "person-add-20", None),
    ("calendario", "fluent-color", "calendar-16", None),
    ("relogio", "fluent-color", "clock-16", None),
    ("despertador", "fluent-color", "clock-alarm-16", None),
    ("coracao", "fluent-color", "heart-16", None),
    ("estrela", "fluent-color", "star-16", None),
    ("luz", "fluent-color", "lightbulb-16", None),
    ("presente", "fluent-color", "gift-16", None),
    ("maleta", "fluent-color", "briefcase-16", None),
    ("ferramentas", "fluent-color", "toolbox-16", None),
    ("chave-inglesa", "fluent-color", "wrench-16", None),
    ("bandeira", "fluent-color", "flag-16", None),
    ("marcador", "fluent-color", "bookmark-16", None),
    ("nuvem", "fluent-color", "cloud-16", None),
    ("nuvem-erro", "fluent-color", "cloud-dismiss-16", None),
    ("tarefa", "fluent-color", "clipboard-task-16", None),
    ("prancheta", "fluent-color", "clipboard-16", None),
    ("cofre", "fluent-color", "vault-16", None),
    ("sincronizar", "fluent-color", "arrow-sync-16", None),
    ("historico", "fluent-color", "history-16", None),
    ("trophy", "fluent-color", "trophy-16", None),
    ("puzzle", "fluent-color", "puzzle-piece-16", None),
    ("robo", "fluent-color", "bot-16", None),
    ("medidor", "fluent-color", "gauge-24", None),
    ("grafico", "fluent-color", "chart-multiple-16", None),
    ("tendencia", "fluent-color", "data-trending-16", None),

    # ---- ficheros ----
    ("documento", "fluent-color", "document-16", None),
    ("documento-texto", "fluent-color", "document-text-16", None),
    ("documento-pasta", "fluent-color", "document-folder-16", None),
    ("documento-editar", "fluent-color", "document-edit-16", None),
    ("documento-novo", "fluent-color", "document-add-16", None),
    ("documento-bloqueado", "fluent-color", "document-lock-16", None),
    ("notas", "fluent-color", "drafts-16", None),
    ("biblioteca", "fluent-color", "library-16", None),
    ("livro", "fluent-color", "book-16", None),
    ("caderno", "fluent-color", "notebook-16", None),
    ("lista", "fluent-color", "text-bullet-list-square-16", None),
    ("texto", "fluent-color", "text-edit-style-16", None),
    ("tabela", "fluent-color", "table-16", None),
    ("base", "fluent-color", "database-16", None),

    # ---- red y correo ----
    ("correio", "fluent-color", "mail-16", None),
    ("correio-alerta", "fluent-color", "mail-alert-16", None),
    ("correio-muitos", "fluent-color", "mail-multiple-16", None),
    ("conversa", "fluent-color", "chat-16", None),
    ("conversas", "fluent-color", "chat-multiple-16", None),
    ("telefone", "fluent-color", "phone-16", None),
    ("globo", "fluent-color", "globe-24", None),
    ("globo-escudo", "fluent-color", "globe-shield-24", None),
    ("wifi", "fluent-color", "wifi-24", None),
    ("wifi-aviso", "fluent-color", "wifi-warning-24", None),
    ("local", "fluent-color", "location-ripple-16", None),
    ("alfinete", "fluent-color", "pin-16", None),
    ("ligacao", "fluent-color", "link-24", None),
    ("enviar", "fluent-color", "send-16", None),
    ("codigo", "fluent-color", "code-16", None),
    ("cartao-contato", "fluent-color", "contact-card-16", None),

    # ---- multimedia ----
    ("camera", "fluent-color", "camera-16", None),
    ("video", "fluent-color", "video-16", None),
    ("imagem", "fluent-color", "image-16", None),
    ("microfone", "fluent-color", "mic-16", None),
    ("fones", "fluent-color", "headphones-24", None),
    ("sol", "fluent-color", "weather-sunny-low-24", None),
    ("neve", "fluent-color", "weather-snowflake-24", None),
    ("jogo", "fluent-color", "game-chat-20", None),
    ("desporto", "fluent-color", "sport-16", None),
    ("recibo", "fluent-color", "receipt-16", None),
    ("poupanca", "fluent-color", "savings-16", None),
    ("formulario", "fluent-color", "form-24", None),

    # ---- avisos y seguridad ----
    ("escudo", "fluent-color", "shield-16", None),
    ("escudo-verificado", "fluent-color", "shield-checkmark-16", None),
    ("cadeado", "fluent-color", "lock-closed-16", None),
    ("alerta", "fluent-color", "alert-16", None),
    ("aviso", "fluent-color", "warning-16", None),
    ("erro", "fluent-color", "error-circle-16", None),
    ("duvida", "fluent-color", "question-circle-16", None),
    ("certo", "fluent-color", "checkmark-circle-16", None),
    ("fechar", "fluent-color", "dismiss-circle-16", None),

    # ---- los que el juego de color no trae: la variante monocroma ----
    ("pasta", "fluent", "folder-16-filled", 0x8494A8),
    ("guardar", "fluent", "save-16-filled", 0x2A6FD8),
    ("abrir", "fluent", "open-16-filled", 0x2A6FD8),
    ("procurar", "fluent", "search-16-filled", 0x8494A8),
    ("tocar", "fluent", "play-16-filled", 0x3CDC6E),
    ("pausar", "fluent", "pause-16-filled", 0x3CDC6E),
    ("copiar", "fluent", "copy-16-filled", 0x8494A8),
    ("colar", "fluent", "clipboard-16-filled", 0x8494A8),
    ("excluir", "fluent", "delete-16-filled", 0xE05252),
    ("editar", "fluent", "edit-16-filled", 0x8494A8),
    ("voltar", "fluent", "arrow-left-16-filled", 0x8494A8),
    ("avancar", "fluent", "arrow-right-16-filled", 0x8494A8),
    ("subir", "fluent", "arrow-up-16-filled", 0x8494A8),
    ("descer", "fluent", "arrow-down-16-filled", 0x8494A8),
    ("janela", "fluent", "window-16-filled", 0x8494A8),
    ("disco", "fluent", "storage-16-filled", 0x8494A8),
    ("imprimir", "fluent", "print-16-filled", 0x8494A8),
    ("olho", "fluent", "eye-16-filled", 0x8494A8),
    ("terminal", "fluent", "window-dev-tools-16-filled", 0x8494A8),
    ("som", "fluent", "speaker-0-16-filled", 0x2A6FD8),
    ("musica", "fluent", "music-note-2-16-filled", 0x9B6BD6),
    ("informacao", "fluent", "info-16-filled", 0x2A6FD8),
    ("ajuda", "fluent", "question-circle-16-filled", 0x2A6FD8),
    ("bateria", "fluent", "battery-10-16-filled", 0x3CDC6E),
    ("ambiente", "fluent", "desktop-16-filled", 0x8494A8),
    ("desenhar", "fluent", "paint-brush-16-filled", 0x9B6BD6),
]


# --------------------------------------------------------------------- color

def _cor(txt):
    """(r, g, b, a) de un color de CSS, o None si no se entiende."""
    if not txt:
        return None
    txt = txt.strip()
    if txt.lower() in ("none", "transparent"):
        return None
    if txt.startswith("#"):
        h = txt[1:]
        if len(h) == 3:
            h = "".join(c * 2 for c in h)
        if len(h) == 6:
            return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 1.0)
        if len(h) == 8:
            return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), int(h[6:8], 16) / 255.0)
        return None
    m = re.match(r"rgba?\(([^)]*)\)", txt, re.I)
    if m:
        partes = [p.strip() for p in m.group(1).split(",")]
        try:
            r, g, b = (int(float(p)) for p in partes[:3])
        except ValueError:
            return None
        a = float(partes[3]) if len(partes) > 3 else 1.0
        return (r, g, b, a)
    nome = _CORES_CSS.get(txt.lower())
    return nome


_CORES_CSS = {
    "black": (0, 0, 0, 1.0),
    "white": (255, 255, 255, 1.0),
    "red": (255, 0, 0, 1.0),
    "green": (0, 128, 0, 1.0),
    "blue": (0, 0, 255, 1.0),
    "gray": (128, 128, 128, 1.0),
    "grey": (128, 128, 128, 1.0),
    "silver": (192, 192, 192, 1.0),
    "navy": (0, 0, 128, 1.0),
    "teal": (0, 128, 128, 1.0),
    "orange": (255, 165, 0, 1.0),
    "purple": (128, 0, 128, 1.0),
    "yellow": (255, 255, 0, 1.0),
}


# ------------------------------------------------------------------ curvas

def _bezier3(p0, p1, p2, p3, n=16):
    """Aplana una curva cúbica de Bézier en n+1 puntos."""
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1.0 - t
        x = u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0]
        y = u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]
        pts.append((x, y))
    return pts


def _bezier2(p0, p1, p2, n=12):
    """Aplana una curva cuadrática de Bézier en n+1 puntos."""
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1.0 - t
        x = u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0]
        y = u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]
        pts.append((x, y))
    return pts


def _arco(p0, rx, ry, phi, grande, sentido, p1):
    """Convierte un arco (endpoint -> centro) y lo aplana en puntos."""
    if rx == 0 or ry == 0 or p0 == p1:
        return [p0, p1]
    rx, ry = abs(rx), abs(ry)
    phi = math.radians(phi % 360.0)
    dx2 = (p0[0] - p1[0]) / 2.0
    dy2 = (p0[1] - p1[1]) / 2.0
    x1p = math.cos(phi) * dx2 + math.sin(phi) * dy2
    y1p = -math.sin(phi) * dx2 + math.cos(phi) * dy2
    lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry)
    if lam > 1:
        s = math.sqrt(lam)
        rx *= s
        ry *= s
    num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p
    den = rx * rx * y1p * y1p + ry * ry * x1p * x1p
    c = math.sqrt(max(0.0, num / den))
    if grande == sentido:
        c = -c
    cxp = c * rx * y1p / ry
    cyp = -c * ry * x1p / rx
    cx = math.cos(phi) * cxp - math.sin(phi) * cyp + (p0[0] + p1[0]) / 2.0
    cy = math.sin(phi) * cxp + math.cos(phi) * cyp + (p0[1] + p1[1]) / 2.0

    def _ang(ux, uy, vx, vy):
        d = math.atan2(vy, vx) - math.atan2(uy, ux)
        while d > math.pi:
            d -= 2 * math.pi
        while d < -math.pi:
            d += 2 * math.pi
        return d

    t1 = _ang(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry)
    dt = _ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry)
    if not sentido and dt > 0:
        dt -= 2 * math.pi
    elif sentido and dt < 0:
        dt += 2 * math.pi
    n = max(2, int(abs(dt) / (math.pi / 12.0)) + 1)
    pts = []
    for i in range(n + 1):
        t = t1 + dt * i / n
        px = math.cos(phi) * rx * math.cos(t) - math.sin(phi) * ry * math.sin(t) + cx
        py = math.sin(phi) * rx * math.cos(t) + math.cos(phi) * ry * math.sin(t) + cy
        pts.append((px, py))
    return pts


# ------------------------------------------------------------------- SVG

_RE_TOKEN = re.compile(r"([MmLlHhVvCcSsQqTtAaZz])|([-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?)")


def _tokens(d):
    for m in _RE_TOKEN.finditer(d):
        if m.group(1):
            yield ("c", m.group(1))
        else:
            yield ("n", float(m.group(2)))


def _caminho(d):
    """Devuelve los subcontornos (listas de puntos) del atributo d."""
    toks = list(_tokens(d))
    subs = []
    atual = []
    x = y = sx = sy = 0.0
    ctrl = None       # último punto de control de una C o S
    qctrl = None      # último punto de control de una Q o T
    i = 0
    cmd = None

    def numeros(n):
        nonlocal i
        vals = []
        for _ in range(n):
            if i >= len(toks) or toks[i][0] != "n":
                raise ValueError("faltan números en el trazo")
            vals.append(toks[i][1])
            i += 1
        return vals

    while i < len(toks):
        if toks[i][0] == "c":
            cmd = toks[i][1]
            i += 1
        if cmd is None:
            raise ValueError("trazo SVG sin comando")
        rel = cmd.islower()
        c = cmd.upper()
        if c == "M":
            v = numeros(2)
            x, y = (x + v[0], y + v[1]) if rel else (v[0], v[1])
            if len(atual) > 1:
                subs.append(atual)
            atual = [(x, y)]
            sx, sy = x, y
            ctrl = qctrl = None
            cmd = "l" if rel else "L"
        elif c == "L":
            v = numeros(2)
            x, y = (x + v[0], y + v[1]) if rel else (v[0], v[1])
            atual.append((x, y))
            ctrl = qctrl = None
        elif c == "H":
            v = numeros(1)
            x = x + v[0] if rel else v[0]
            atual.append((x, y))
            ctrl = qctrl = None
        elif c == "V":
            v = numeros(1)
            y = y + v[0] if rel else v[0]
            atual.append((x, y))
            ctrl = qctrl = None
        elif c == "C":
            v = numeros(6)
            if rel:
                v = [v[0] + x, v[1] + y, v[2] + x, v[3] + y, v[4] + x, v[5] + y]
            ctrl = (v[2], v[3])
            atual.extend(_bezier3((x, y), (v[0], v[1]), ctrl, (v[4], v[5]))[1:])
            x, y = v[4], v[5]
            qctrl = None
        elif c == "S":
            v = numeros(4)
            if rel:
                v = [v[0] + x, v[1] + y, v[2] + x, v[3] + y]
            p1 = (2 * x - ctrl[0], 2 * y - ctrl[1]) if ctrl else (x, y)
            ctrl = (v[0], v[1])
            atual.extend(_bezier3((x, y), p1, ctrl, (v[2], v[3]))[1:])
            x, y = v[2], v[3]
            qctrl = None
        elif c == "Q":
            v = numeros(4)
            if rel:
                v = [v[0] + x, v[1] + y, v[2] + x, v[3] + y]
            qctrl = (v[0], v[1])
            atual.extend(_bezier2((x, y), qctrl, (v[2], v[3]))[1:])
            x, y = v[2], v[3]
            ctrl = None
        elif c == "T":
            v = numeros(2)
            if rel:
                v = [v[0] + x, v[1] + y]
            p1 = (2 * x - qctrl[0], 2 * y - qctrl[1]) if qctrl else (x, y)
            qctrl = p1
            atual.extend(_bezier2((x, y), p1, (v[0], v[1]))[1:])
            x, y = v[0], v[1]
            ctrl = None
        elif c == "A":
            v = numeros(7)
            if rel:
                v[5] += x
                v[6] += y
            atual.extend(_arco((x, y), v[0], v[1], v[2], bool(v[3]), bool(v[4]), (v[5], v[6]))[1:])
            x, y = v[5], v[6]
            ctrl = qctrl = None
        elif c == "Z":
            if len(atual) > 1:
                subs.append(atual)
            atual = []
            x, y = sx, sy
            ctrl = qctrl = None
            cmd = None
        else:
            raise ValueError("comando SVG desconocido: " + cmd)
    if len(atual) > 1:
        subs.append(atual)
    return subs


def _tag(el):
    return el.tag.rsplit("}", 1)[-1]


def _forma(el, tag):
    """Los subcontornos de una forma del SVG."""
    if tag == "path":
        return _caminho(el.get("d", ""))
    if tag == "rect":
        x = float(el.get("x", 0))
        y = float(el.get("y", 0))
        w = float(el.get("width", 0))
        h = float(el.get("height", 0))
        return [[(x, y), (x + w, y), (x + w, y + h), (x, y + h)]]
    if tag in ("circle", "ellipse"):
        cx = float(el.get("cx", 0))
        cy = float(el.get("cy", 0))
        if tag == "circle":
            rx = ry = float(el.get("r", 0))
        else:
            rx = float(el.get("rx", 0))
            ry = float(el.get("ry", 0))
        k = 0.5522847498
        pts = []
        for ex, ey, px, py in ((cx + rx, cy, cx, cy + k * ry), (cx, cy + ry, cx - k * rx, cy),
                               (cx - rx, cy, cx, cy - k * ry), (cx, cy - ry, cx + k * rx, cy)):
            pts.append((ex, ey))
        subs = []
        for i in range(4):
            p0 = pts[i]
            p3 = pts[(i + 1) % 4]
            c1 = (p0[0] + (p3[0] - pts[(i - 1) % 4][0]) / 6, p0[1] + (p3[1] - pts[(i - 1) % 4][1]) / 6)
            c2 = (p3[0] - (pts[(i + 1) % 4][0] - p0[0]) / 6, p3[1] - (pts[(i + 1) % 4][1] - p0[1]) / 6)
            subs.append(_bezier3(p0, c1, c2, p3))
        return [subs[0] + subs[1][1:] + subs[2][1:] + subs[3][1:]]
    if tag in ("polygon", "polyline"):
        nums = [float(v) for v in re.findall(r"[-+]?(?:\d*\.\d+|\d+\.?)", el.get("points", ""))]
        pts = [(nums[i], nums[i + 1]) for i in range(0, len(nums) - 1, 2)]
        return [pts] if len(pts) > 1 else []
    return []


def _degradientes(raiz):
    """Los degradados del SVG, indexados por id."""
    por_id = {}
    for el in raiz.iter():
        if _tag(el) in ("linearGradient", "radialGradient"):
            if el.get("id"):
                por_id[el.get("id")] = el
    grads = {}
    for gid, el in por_id.items():
        paradas = []
        for st in el:
            if _tag(st) != "stop":
                continue
            off = st.get("offset", "0")
            off = float(off[:-1]) / 100.0 if off.endswith("%") else float(off)
            cor = _cor(st.get("stop-color", "#000000"))
            op = st.get("stop-opacity")
            if cor is None:
                continue
            if op is not None:
                cor = (cor[0], cor[1], cor[2], cor[2] and cor[3] * float(op))
            paradas.append((off, cor))
        # Un degradado puede heredar las paradas de otro (xlink:href)
        if not paradas:
            href = el.get("{http://www.w3.org/1999/xlink}href") or el.get("href")
            if href and href.startswith("#") and href[1:] in por_id:
                outro = por_id[href[1:]]
                for st in outro:
                    if _tag(st) != "stop":
                        continue
                    off = st.get("offset", "0")
                    off = float(off[:-1]) / 100.0 if off.endswith("%") else float(off)
                    cor = _cor(st.get("stop-color", "#000000"))
                    if cor:
                        paradas.append((off, cor))
        paradas.sort(key=lambda p: p[0])
        grads[gid] = {
            "tipo": "radial" if _tag(el) == "radialGradient" else "lineal",
            "x1": float(el.get("x1", 0)), "y1": float(el.get("y1", 0)),
            "x2": float(el.get("x2", 1)), "y2": float(el.get("y2", 0)),
            "cx": float(el.get("cx", 0.5)), "cy": float(el.get("cy", 0.5)),
            "r": float(el.get("r", 0.5)),
            "unidades": el.get("gradientUnits", "objectBoundingBox"),
            "paradas": paradas,
        }
    return grads


def _cor_degradiente(g, x, y, caixa):
    """El color del degradado en el punto (x, y)."""
    paradas = g["paradas"]
    if not paradas:
        return None
    if g["tipo"] == "lineal":
        x1, y1, x2, y2 = g["x1"], g["y1"], g["x2"], g["y2"]
        if g["unidades"] != "userSpaceOnUse":
            bx, by, bw, bh = caixa
            x1, y1 = bx + x1 * bw, by + y1 * bh
            x2, y2 = bx + x2 * bw, by + y2 * bh
        dx, dy = x2 - x1, y2 - y1
        d2 = dx * dx + dy * dy
        t = 0.0 if d2 == 0 else ((x - x1) * dx + (y - y1) * dy) / d2
    else:
        cx, cy, r = g["cx"], g["cy"], g["r"]
        if g["unidades"] != "userSpaceOnUse":
            bx, by, bw, bh = caixa
            cx, cy, r = bx + cx * bw, by + cy * bh, r * max(bw, bh)
        t = 0.0 if r == 0 else math.hypot(x - cx, y - cy) / r
    t = min(1.0, max(0.0, t))
    if t <= paradas[0][0]:
        return paradas[0][1]
    if t >= paradas[-1][0]:
        return paradas[-1][1]
    for i in range(len(paradas) - 1):
        o0, c0 = paradas[i]
        o1, c1 = paradas[i + 1]
        if o0 <= t <= o1:
            u = 0.0 if o1 == o0 else (t - o0) / (o1 - o0)
            return tuple(c0[k] + (c1[k] - c0[k]) * u for k in range(4))
    return paradas[-1][1]


def _forma_cor(est, grads, subs, cor_mono):
    """La función que da el color de la forma en cada punto (o None si no se rellena)."""
    relleno = est.get("fill", "black")
    if relleno.strip().lower() == "none":
        return None
    op = float(est.get("opacity", 1.0)) * float(est.get("fill-opacity", 1.0))
    if op <= 0:
        return None
    xs = [p[0] for sub in subs for p in sub]
    ys = [p[1] for sub in subs for p in sub]
    caixa = (min(xs), min(ys), max(xs) - min(xs), max(ys) - min(ys))
    m = re.match(r"url\(#([^)]+)\)", relleno.strip())
    if m:
        g = grads.get(m.group(1))
        if g is None:
            return None
        return lambda px, py: _mezcla(_cor_degradiente(g, px, py, caixa), op)
    if relleno.strip().lower() == "currentcolor":
        cor = _cor_int(cor_mono)
    else:
        cor = _cor(relleno)
    if cor is None:
        return None
    return lambda px, py: _mezcla(cor, op)


def _cor_int(valor):
    """Un color 0xRRGGBB como (r, g, b, a)."""
    return ((valor >> 16) & 0xFF, (valor >> 8) & 0xFF, valor & 0xFF, 1.0)


def _mezcla(cor, op):
    if cor is None:
        return None
    return (cor[0], cor[1], cor[2], cor[3] * op)


def _svg_formas(texto, cor_mono):
    """Las formas rellenas del SVG, en orden (de abajo arriba)."""
    raiz = ET.fromstring(texto)
    grads = _degradientes(raiz)
    formas = []

    def visitar(el, est):
        tag = _tag(el)
        if tag in ("defs", "clipPath", "mask", "title", "desc", "metadata", "linearGradient", "radialGradient"):
            return
        novo = dict(est)
        for chave in ("fill", "fill-opacity", "opacity"):
            if el.get(chave) is not None:
                novo[chave] = el.get(chave)
        if tag in ("path", "rect", "circle", "ellipse", "polygon", "polyline"):
            subs = _forma(el, tag)
            if subs:
                cor = _forma_cor(novo, grads, subs, cor_mono)
                if cor is not None:
                    formas.append({"subs": subs, "cor": cor})
            return
        for filho in el:
            visitar(filho, novo)

    visitar(raiz, {"fill": "black", "fill-opacity": "1", "opacity": "1"})
    return formas


# -------------------------------------------------------------- rasterizado

def _dentro(subs, x, y):
    """Regla par-impar: ¿el punto cae dentro de los subcontornos?"""
    dentro = False
    for pts in subs:
        n = len(pts)
        j = n - 1
        for i in range(n):
            xi, yi = pts[i]
            xj, yj = pts[j]
            if (yi > y) != (yj > y):
                t = (y - yi) / (yj - yi)
                if x < xi + t * (xj - xi):
                    dentro = not dentro
            j = i
    return dentro


def _rasteriza(formas, tam=TAM, muestras=MUESTRAS):
    """Devuelve una matriz tam x tam con el color de cada píxel (o None)."""
    total = muestras * muestras
    mat = []
    for py in range(tam):
        linha = []
        for px in range(tam):
            soma_r = soma_g = soma_b = soma_a = 0.0
            for sy in range(muestras):
                for sx in range(muestras):
                    x = px + (sx + 0.5) / muestras
                    y = py + (sy + 0.5) / muestras
                    r = g = b = a = 0.0
                    for forma in formas:
                        if not _dentro(forma["subs"], x, y):
                            continue
                        cor = forma["cor"](x, y)
                        if cor is None or cor[3] <= 0:
                            continue
                        ca = cor[3]
                        r = cor[0] * ca + r * (1 - ca)
                        g = cor[1] * ca + g * (1 - ca)
                        b = cor[2] * ca + b * (1 - ca)
                        a = ca + a * (1 - ca)
                    if a > 0:
                        soma_r += r * a
                        soma_g += g * a
                        soma_b += b * a
                        soma_a += a
            if soma_a / total >= UMBRAL and soma_a > 0:
                linha.append((soma_r / soma_a, soma_g / soma_a, soma_b / soma_a))
            else:
                linha.append(None)
        mat.append(linha)
    return mat


def _tomo_mais_proximo(cor):
    """La letra de la paleta que más se parece al color."""
    melhor = None
    dist_min = None
    for letra, valor in TONOS:
        r = (valor >> 16) & 0xFF
        g = (valor >> 8) & 0xFF
        b = valor & 0xFF
        d = (r - cor[0]) ** 2 + (g - cor[1]) ** 2 + (b - cor[2]) ** 2
        if dist_min is None or d < dist_min:
            dist_min = d
            melhor = letra
    return melhor


def _pixel_art(mat, contorno="P"):
    """Convierte la matriz de colores en filas de caracteres de la paleta."""
    linhas = []
    for y in range(len(mat)):
        linha = []
        for x in range(len(mat[0])):
            if mat[y][x] is None:
                linha.append(".")
            else:
                linha.append(_tomo_mais_proximo(mat[y][x]))
        linhas.append(linha)
    # El contorno: un píxel pintado cuyo vecino de al lado no lo está.
    for y in range(len(linhas)):
        for x in range(len(linhas[0])):
            if linhas[y][x] == ".":
                continue
            fora = False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or ny < 0 or nx >= len(linhas[0]) or ny >= len(linhas):
                    fora = True
                    break
                if linhas[ny][nx] == ".":
                    fora = True
                    break
            if fora:
                linhas[y][x] = contorno
    return ["".join(l) for l in linhas]


# ------------------------------------------------------------------ descarga

def _baixa(prefijo, nome):
    """El texto del SVG, de la caché o de la API de Iconify."""
    os.makedirs(CACHE, exist_ok=True)
    ficheiro = os.path.join(CACHE, "%s-%s.svg" % (prefijo, nome))
    if os.path.exists(ficheiro):
        with open(ficheiro, encoding="utf-8") as f:
            return f.read()
    url = "https://api.iconify.design/%s/%s.svg" % (prefijo, nome)
    # La API aguanta pocas peticiones seguidas: si contesta 429 (demasiadas),
    # se espera y se repite. Entre descarga y descarga hay una pausa.
    espera = 2
    for tentativa in range(5):
        pedido = urllib.request.Request(url, headers={"User-Agent": AGENTE})
        try:
            with urllib.request.urlopen(pedido, timeout=30) as r:
                texto = r.read().decode("utf-8")
            break
        except urllib.error.HTTPError as e:
            if e.code == 429 and tentativa < 4:
                time.sleep(espera)
                espera *= 2
                continue
            raise SystemExit("o icono %s/%s no se ha podido bajar (HTTP %d)" % (prefijo, nome, e.code))
        except urllib.error.URLError as e:
            raise SystemExit("no hay red para bajar %s/%s: %s" % (prefijo, nome, e))
    time.sleep(0.9)
    with open(ficheiro, "w", encoding="utf-8") as f:
        f.write(texto)
    return texto


# ------------------------------------------------------------------ salida

def _variavel(chave):
    return "ic_" + re.sub(r"[^A-Za-z0-9]", "_", chave)


def _c_codigo(icones):
    """El texto de userland/libsagui/icones.c."""
    out = []
    out.append("/* icones.c - biblioteca de iconos del SaguiOS (D135).")
    out.append(" *")
    out.append(" * Generado por tools/gerar-icones.py: no editar a mano. Los iconos vienen")
    out.append(" * de los \"Fluent UI System Icons\" de Microsoft (licencia MIT), descargados")
    out.append(" * del Iconify y rasterizados a 16x16 píxeles. El formato es el del proyecto:")
    out.append(" * cada línea tiene 16 caracteres y cada carácter es un tono de la paleta")
    out.append(" * compartida (abajo).")
    out.append(" *")
    out.append(" * La licencia y el aviso de copyright están en userland/terceros/fluentui/")
    out.append(" * (vea docs/terceros.md). */")
    out.append('#include "icones.h"')
    out.append("")
    for chave, linhas in icones:
        out.append("static const char *const %s[] = {" % _variavel(chave))
        for i in range(0, len(linhas), 4):
            grupo = linhas[i:i + 4]
            out.append("    " + ", ".join('"%s"' % l for l in grupo) + ",")
        out.append("};")
        out.append("")
    out.append("/* La tabla: nombre del icono -> dibujo. */")
    out.append("static const icone_t tabela[] = {")
    for chave, _ in icones:
        out.append('    { "%s", %s },' % (chave, _variavel(chave)))
    out.append("};")
    out.append("")
    out.append("#define QTD_ICONES (int)(sizeof tabela / sizeof tabela[0])")
    out.append("")
    out.append("/* La paleta compartida: '.' es transparente, los demás son tonos. */")
    out.append("uint32_t icones_cor(char c)")
    out.append("{")
    out.append("    switch (c) {")
    for letra, valor in PALETA:
        out.append("    case '%s': return 0x%06X;" % (letra, valor))
    out.append("    default: return 0xFF000000;   /* transparente */")
    out.append("    }")
    out.append("}")
    out.append("")
    out.append("int icones_total(void)")
    out.append("{")
    out.append("    return QTD_ICONES;")
    out.append("}")
    out.append("")
    out.append("const icone_t *icones_todos(void)")
    out.append("{")
    out.append("    return tabela;")
    out.append("}")
    out.append("")
    out.append("const icone_t *icones_procurar(const char *chave)")
    out.append("{")
    out.append("    if (!chave) return NULL;")
    out.append("    for (int i = 0; i < QTD_ICONES; i++)")
    out.append("        if (compara_texto(tabela[i].chave, chave) == 0) return &tabela[i];")
    out.append("    return NULL;")
    out.append("}")
    out.append("")
    out.append("/* Dibuja un icono por su nombre. Si no existe, no dibuja nada. */")
    out.append("void icone_desenhar(jan_t *j, int x, int y, const char *chave, int escala)")
    out.append("{")
    out.append("    const icone_t *ic = icones_procurar(chave);")
    out.append("    if (!ic) return;")
    out.append("    g_desenho(j, x, y, ic->linhas, ICONES_LADO, ICONES_LADO, icones_cor, escala);")
    out.append("}")
    out.append("")
    out.append("/* Dibuja un icono por su posición en la tabla. */")
    out.append("void icone_desenhar_indice(jan_t *j, int x, int y, int indice, int escala)")
    out.append("{")
    out.append("    if (indice < 0 || indice >= QTD_ICONES) return;")
    out.append("    g_desenho(j, x, y, tabela[indice].linhas, ICONES_LADO, ICONES_LADO, icones_cor, escala);")
    out.append("}")
    out.append("")
    return "\n".join(out) + "\n"


def _c_lista(icones):
    """El texto de userland/terceros/fluentui/ICONES.md."""
    out = []
    out.append("# Iconos usados en la biblioteca del SaguiOS (D135)")
    out.append("")
    out.append("Todos estos iconos son del juego **Fluent UI System Icons** de Microsoft")
    out.append("(Copyright (c) 2020 Microsoft Corporation, licencia MIT), servido por Iconify.")
    out.append("El rasterizado a 16x16 y el paso a pixel art los hace `tools/gerar-icones.py`")
    out.append("de este proyecto; el resultado es `userland/libsagui/icones.c`.")
    out.append("")
    out.append("El texto de la licencia está en `LICENSE` (en esta misma carpeta).")
    out.append("")
    out.append("## Conjuntos usados")
    out.append("")
    out.append("| Prefijo de Iconify | Qué es | |")
    out.append("| --- | --- | --- |")
    out.append("| `fluent-color` | Fluent UI System Color Icons: los iconos a color "
               "(con degradados), 199 iconos | https://icon-sets.iconify.design/fluent-color/ |")
    out.append("| `fluent` | Fluent UI System Icons: la variante monocroma (relleno sólido), "
               "de donde salen los iconos que el juego a color no trae | "
               "https://icon-sets.iconify.design/fluent/ |")
    out.append("")
    out.append("## Iconos de la biblioteca")
    out.append("")
    out.append("| Clave | Origen | SVG |")
    out.append("| --- | --- | --- |")
    for chave, prefijo, nome, _ in ICONES:
        out.append("| `%s` | %s | https://api.iconify.design/%s/%s.svg |" % (chave, prefijo, prefijo, nome))
    out.append("")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description="genera la biblioteca de iconos del SaguiOS")
    ap.add_argument("--ver", action="store_true", help="sólo comprueba, no escribe")
    ap.add_argument("--limpar", action="store_true", help="borra la caché de los SVG")
    args = ap.parse_args()

    if args.limpar:
        import shutil
        shutil.rmtree(CACHE, ignore_errors=True)
        print("caché borrada")
        return 0

    icones = []
    for chave, prefijo, nome, cor_mono in ICONES:
        texto = _baixa(prefijo, nome)
        formas = _svg_formas(texto, cor_mono if cor_mono is not None else 0x8494A8)
        mat = _rasteriza(formas)
        arte = _pixel_art(mat)
        if all(l == "." * TAM for l in arte):
            raise SystemExit("el icono %s (%s/%s) ha salido vacío" % (chave, prefijo, nome))
        icones.append((chave, arte))
        print("%-20s %s/%s" % (chave, prefijo, nome))

    codigo = _c_codigo(icones)
    lista = _c_lista(icones)
    if args.ver:
        print("ok: %d iconos" % len(icones))
        return 0
    with open(SAIDA_C, "w", encoding="utf-8") as f:
        f.write(codigo)
    os.makedirs(os.path.dirname(SAIDA_LISTA), exist_ok=True)
    with open(SAIDA_LISTA, "w", encoding="utf-8") as f:
        f.write(lista)
    print("escrito %s (%d iconos) y %s" % (SAIDA_C, len(icones), SAIDA_LISTA))
    return 0


if __name__ == "__main__":
    sys.exit(main())
