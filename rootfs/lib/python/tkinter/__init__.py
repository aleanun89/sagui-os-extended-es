# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter - o tkinter do SaguiOS (mejora 033, D084).
#
# Um subconjunto da API do tkinter do CPython, escrito do zero em Python a
# partir da documentação do Tk, sobre o módulo sagui (janelas e desenho em
# C, D083). Não há Tcl nem Tk: cada Tk/Toplevel é uma janela do SaguiOS e
# os componentes são desenhados nela com o aspecto do sistema.
#
# Os nomes públicos são os do tkinter (Button, pack, StringVar...); os
# internos, como o resto do SaguiOS, em português. O que está e o que não
# está: /ayuda/python-tk.md.
#
# Como funciona:
#   - cada componente sabe o tamanho que pede (_natural) e desenha a si
#     mesmo (_desenhar) dentro do retângulo que o gerenciador de geometria
#     (pack, grid ou place) lhe deu;
#   - mudar uma opção marca o componente como "sujo" (redesenhar) ou pede
#     um novo layout da janela; o laço de eventos refaz o layout e redesenha
#     só os sujos quando não há mais nada a fazer, e manda ao kernel só a
#     área que mudou;
#   - o laço dorme no kernel até o próximo evento ou o próximo after().
import sys
import sagui
from tkinter.constants import *

TkVersion = 8.6
TclVersion = 8.6
wantobjects = 1


class TclError(Exception):
    pass


# ------------------------------------------------------------ cores e tema

# Cores do SaguiOS (userland/inc/gui.h e kernel/gui/janelas.c)
FUNDO = 0xF4F4F0
TEXTO = 0x1E2328
FRACO = 0x6A737C
BORDA = 0x8A939C
BOTAO = 0xE2E6EA
BOTAO_ATIVO = 0xECEFF2
APERTADO = 0xC4CBD2
DESTAQUE = 0x0B7A3E
SELECAO = 0x2A6FD8
BRANCO = 0xFFFFFF
ESCURO = 0x5A6470
TRILHO = 0xE4E7EA
POLEGAR = 0x9AA3AC
LETRA_L = 8
LETRA_A = 16

_NOMES_CORES = {
    "white": 0xFFFFFF, "black": 0x000000, "red": 0xFF0000, "green": 0x008000, "blue": 0x0000FF,
    "yellow": 0xFFFF00, "cyan": 0x00FFFF, "magenta": 0xFF00FF, "gray": 0xBEBEBE, "grey": 0xBEBEBE,
    "darkgray": 0xA9A9A9, "darkgrey": 0xA9A9A9, "lightgray": 0xD3D3D3, "lightgrey": 0xD3D3D3,
    "dimgray": 0x696969, "dimgrey": 0x696969, "silver": 0xC0C0C0, "gainsboro": 0xDCDCDC,
    "whitesmoke": 0xF5F5F5, "snow": 0xFFFAFA, "ivory": 0xFFFFF0, "beige": 0xF5F5DC,
    "orange": 0xFFA500, "darkorange": 0xFF8C00, "orangered": 0xFF4500, "gold": 0xFFD700,
    "purple": 0xA020F0, "violet": 0xEE82EE, "pink": 0xFFC0CB, "hotpink": 0xFF69B4,
    "brown": 0xA52A2A, "maroon": 0xB03060, "navy": 0x000080, "navyblue": 0x000080,
    "darkblue": 0x00008B, "lightblue": 0xADD8E6, "skyblue": 0x87CEEB, "lightskyblue": 0x87CEFA,
    "steelblue": 0x4682B4, "royalblue": 0x4169E1, "dodgerblue": 0x1E90FF, "deepskyblue": 0x00BFFF,
    "darkgreen": 0x006400, "lightgreen": 0x90EE90, "lime": 0x00FF00, "limegreen": 0x32CD32,
    "forestgreen": 0x228B22, "seagreen": 0x2E8B57, "olive": 0x808000, "olivedrab": 0x6B8E23,
    "darkred": 0x8B0000, "firebrick": 0xB22222, "crimson": 0xDC143C, "tomato": 0xFF6347,
    "salmon": 0xFA8072, "coral": 0xFF7F50, "khaki": 0xF0E68C, "tan": 0xD2B48C,
    "chocolate": 0xD2691E, "sienna": 0xA0522D, "turquoise": 0x40E0D0, "teal": 0x008080,
    "aquamarine": 0x7FFFD4, "lavender": 0xE6E6FA, "plum": 0xDDA0DD, "orchid": 0xDA70D6,
    "indigo": 0x4B0082, "slategray": 0x708090, "slategrey": 0x708090, "lightyellow": 0xFFFFE0,
    "lightcyan": 0xE0FFFF, "mintcream": 0xF5FFFA, "honeydew": 0xF0FFF0, "azure": 0xF0FFFF,
    "aliceblue": 0xF0F8FF, "linen": 0xFAF0E6, "wheat": 0xF5DEB3, "bisque": 0xFFE4C4,
    "systembuttonface": BOTAO, "systemwindow": BRANCO, "systembuttontext": TEXTO,
    "systemwindowtext": TEXTO, "systemhighlight": SELECAO, "systemhighlighttext": BRANCO,
    "sagui": DESTAQUE,
}
for _i in range(0, 101):
    _NOMES_CORES["gray%d" % _i] = _NOMES_CORES["grey%d" % _i] = (round(_i * 2.55) * 0x010101)

_cache_cores = {}


def _cor(valor):
    """Cor do Tk ('red', '#f00', '#ff0000', 0xRRGGBB) -> inteiro, ou None se vazia."""
    if valor is None or valor == "":
        return None
    if isinstance(valor, int):
        return valor & 0xFFFFFF
    c = _cache_cores.get(valor)
    if c is not None:
        return c
    s = str(valor).strip().lower().replace(" ", "")
    if s.startswith("#"):
        h = s[1:]
        try:
            if len(h) == 3:
                c = int(h[0] * 2 + h[1] * 2 + h[2] * 2, 16)
            elif len(h) == 6:
                c = int(h, 16)
            elif len(h) == 9:
                c = int(h[0:2] + h[3:5] + h[6:8], 16)
            elif len(h) == 12:
                c = int(h[0:2] + h[4:6] + h[8:10], 16)
        except ValueError:
            c = None
    else:
        c = _NOMES_CORES.get(s)
    if c is None:
        raise TclError('unknown color name "%s"' % valor)
    _cache_cores[valor] = c
    return c


def _misturar(c1, c2, t):
    r = ((c1 >> 16) & 255) * (1 - t) + ((c2 >> 16) & 255) * t
    g = ((c1 >> 8) & 255) * (1 - t) + ((c2 >> 8) & 255) * t
    b = (c1 & 255) * (1 - t) + (c2 & 255) * t
    return (int(r) << 16) | (int(g) << 8) | int(b)


def _int(v, padrao=0):
    """Distâncias do Tk: 10, '10', '10p', '1c'... -> pixels."""
    if v is None or v == "":
        return padrao
    if isinstance(v, (int, float)):
        return int(v)
    s = str(v).strip()
    try:
        if s[-1] in "cimp":
            n = float(s[:-1])
            return int(n * {"c": 37.8, "i": 96, "m": 3.78, "p": 1.333}[s[-1]])
        return int(float(s))
    except (ValueError, IndexError):
        raise TclError('bad screen distance "%s"' % v)


def _pares(v):
    """padx=5 ou padx=(2, 8) -> (antes, depois)."""
    if isinstance(v, (tuple, list)):
        if len(v) == 1:
            return _int(v[0]), _int(v[0])
        return _int(v[0]), _int(v[1])
    if isinstance(v, str) and " " in v.strip():
        p = v.split()
        return _int(p[0]), _int(p[1])
    n = _int(v)
    return n, n


def _bool(v):
    if isinstance(v, str):
        return v.strip().lower() in ("1", "yes", "true", "on", "y", "t")
    return bool(v)


def _igual(a, b):
    """Valor de variável contra onvalue/value: 1 == True == "1"."""
    if isinstance(a, (bool, int, float)) or isinstance(b, (bool, int, float)):
        try:
            return float(a) == float(b)
        except (TypeError, ValueError):
            pass
    return str(a) == str(b)


def _letra(c):
    return c.isalpha() or c.isdigit() or c == "_"


def _relatar(e):
    """Exceção num callback: como o Tk, mostra e continua."""
    sys.stderr.write("Exception in Tkinter callback\n")
    sys.print_exception(e)


# ------------------------------------------------------------ desenho comum

def _relevo(v, x, y, w, h, relief, bd, fundo=FUNDO):
    """Borda em relevo com bd pixels: raised, sunken, groove, ridge, solid."""
    if bd <= 0 or relief == "flat" or w <= 0 or h <= 0:
        return
    for i in range(bd):
        externa = i < (bd + 1) // 2
        if relief == "raised":
            cl, ce = (BRANCO, ESCURO) if externa else (_misturar(fundo, BRANCO, 0.5), BORDA)
        elif relief == "sunken":
            cl, ce = (BORDA, BRANCO) if externa else (ESCURO, _misturar(fundo, BRANCO, 0.5))
        elif relief == "groove":
            cl, ce = (BORDA, BRANCO) if externa else (BRANCO, BORDA)
        elif relief == "ridge":
            cl, ce = (BRANCO, BORDA) if externa else (BORDA, BRANCO)
        else:   # solid
            cl = ce = TEXTO
        v.rect(x + i, y + i, w - 2 * i, 1, cl)
        v.rect(x + i, y + i, 1, h - 2 * i, cl)
        v.rect(x + i, y + h - 1 - i, w - 2 * i, 1, ce)
        v.rect(x + w - 1 - i, y + i, 1, h - 2 * i, ce)


def _moldura_campo(v, x, y, w, h, relief, bd, focado):
    """Borda de um campo de texto (Entry, Text, Listbox), como a caixa do
    gui.c: verde com o foco, cinza com uma sombra por dentro sem ele."""
    if focado:
        v.rect(x, y, w, h, DESTAQUE, relleno=False, grosor=2)
    elif relief == "sunken" and bd >= 1:
        v.rect(x, y, w, h, BORDA, relleno=False)
        if bd >= 2:
            v.rect(x + 1, y + 1, w - 2, 1, APERTADO)
            v.rect(x + 1, y + 1, 1, h - 2, APERTADO)
    else:
        _relevo(v, x, y, w, h, relief, bd)


def _quebrar(texto, largura_px):
    """Linhas do texto, quebrando em palavras quando passam de largura_px."""
    linhas = []
    for linha in str(texto).split("\n"):
        if largura_px <= 0 or len(linha) * LETRA_L <= largura_px:
            linhas.append(linha)
            continue
        cabem = max(1, largura_px // LETRA_L)
        atual = ""
        for palavra in linha.split(" "):
            prova = palavra if not atual else atual + " " + palavra
            if len(prova) <= cabem:
                atual = prova
                continue
            if atual:
                linhas.append(atual)
            while len(palavra) > cabem:
                linhas.append(palavra[:cabem])
                palavra = palavra[cabem:]
            atual = palavra
        linhas.append(atual)
    return linhas


def _ancorar(anchor, x, y, w, h, cw, ch):
    """Posição de um conteúdo cw x ch dentro de (x, y, w, h) segundo o anchor."""
    a = anchor or "center"
    if a == "center":
        a = ""
    if "w" in a:
        px = x
    elif "e" in a:
        px = x + w - cw
    else:
        px = x + (w - cw) // 2
    if "n" in a:
        py = y
    elif "s" in a:
        py = y + h - ch
    else:
        py = y + (h - ch) // 2
    return px, py


def _textos(v, linhas, x, y, w, h, anchor, justify, cor):
    """Desenha linhas de texto ancoradas no retângulo."""
    if not linhas:
        return
    maior = max(len(l) for l in linhas) * LETRA_L
    px, py = _ancorar(anchor, x, y, w, h, maior, len(linhas) * LETRA_A)
    for l in linhas:
        lx = px
        if justify == "center":
            lx = px + (maior - len(l) * LETRA_L) // 2
        elif justify == "right":
            lx = px + maior - len(l) * LETRA_L
        v.texto(lx, py, l, cor)
        py += LETRA_A


# ------------------------------------------------------------ variáveis

_cont_var = 0


class Variable:
    _padrao = ""

    def __init__(self, master=None, value=None, name=None):
        global _cont_var
        if name is None:
            name = "PY_VAR%d" % _cont_var
            _cont_var += 1
        self._name = name
        self._valor = self._padrao if value is None else value
        self._traces = []        # [modo, callback, nome]
        self._ouvintes = []      # componentes que mostram esta variável
        self._cont_trace = 0

    def __str__(self):
        return self._name

    def set(self, value):
        self._valor = value
        self._avisar("write")

    initialize = set

    def get(self):
        self._avisar("read")
        return self._valor

    def _avisar(self, modo):
        if modo == "write":
            for w in list(self._ouvintes):
                w._variavel_mudou(self)
        for t in list(self._traces):
            if modo in t[0] or (modo == "write" and "w" in t[0]) or (modo == "read" and "r" in t[0]):
                try:
                    t[1](self._name, "", modo)
                except Exception as e:
                    _relatar(e)

    def trace_add(self, mode, callback):
        modos = (mode,) if isinstance(mode, str) else tuple(mode)
        self._cont_trace += 1
        nome = "%s_trace%d" % (self._name, self._cont_trace)
        self._traces.append([modos, callback, nome])
        return nome

    def trace_remove(self, mode, cbname):
        self._traces = [t for t in self._traces if t[2] != cbname]

    def trace_info(self):
        return [(t[0], t[2]) for t in self._traces]

    def trace_variable(self, mode, callback):
        m = {"w": "write", "r": "read", "u": "unset"}.get(mode, mode)
        return self.trace_add(m, callback)

    trace = trace_variable

    def trace_vdelete(self, mode, cbname):
        self.trace_remove(mode, cbname)

    def trace_vinfo(self):
        return self.trace_info()

    def __eq__(self, other):
        return isinstance(other, Variable) and other._name == self._name

    def __hash__(self):
        return hash(self._name)


class StringVar(Variable):
    _padrao = ""

    def get(self):
        v = Variable.get(self)
        return v if isinstance(v, str) else str(v)


class IntVar(Variable):
    _padrao = 0

    def get(self):
        v = Variable.get(self)
        if isinstance(v, int):
            return v
        try:
            return int(v)
        except (TypeError, ValueError):
            try:
                return int(float(v))
            except (TypeError, ValueError):
                raise TclError('expected integer but got "%s"' % v)


class DoubleVar(Variable):
    _padrao = 0.0

    def get(self):
        v = Variable.get(self)
        try:
            return float(v)
        except (TypeError, ValueError):
            raise TclError('expected floating-point number but got "%s"' % v)


class BooleanVar(Variable):
    _padrao = False

    def set(self, value):
        Variable.set(self, _bool(value))

    def get(self):
        return _bool(Variable.get(self))


def getint(s):
    return int(s)


def getdouble(s):
    return float(s)


def getboolean(s):
    return _bool(s)


# ------------------------------------------------------------ eventos

class EventType:
    KeyPress = "2"
    KeyRelease = "3"
    ButtonPress = "4"
    ButtonRelease = "5"
    Motion = "6"
    Enter = "7"
    Leave = "8"
    FocusIn = "9"
    FocusOut = "10"
    Destroy = "17"
    Configure = "22"
    MouseWheel = "38"
    VirtualEvent = "35"


class Event:
    """O objeto que os callbacks de bind recebem."""

    def __init__(self, tipo, widget, x=0, y=0):
        self.type = tipo
        self.widget = widget
        self.x = x
        self.y = y
        self.x_root = x
        self.y_root = y
        self.num = "??"
        self.char = ""
        self.keysym = "??"
        self.keycode = 0
        self.keysym_num = 0
        self.delta = 0
        self.width = 0
        self.height = 0
        self.state = 0
        self.time = sagui.ticks_ms()
        self.serial = 0
        self.focus = False
        self.send_event = False

    def __repr__(self):
        return "<%s event x=%s y=%s>" % (self.type, self.x, self.y)


# Tipos de evento que o bind entende e o nome interno
_TIPOS = {"button": "ButtonPress", "buttonpress": "ButtonPress", "buttonrelease": "ButtonRelease",
          "motion": "Motion", "key": "KeyPress", "keypress": "KeyPress", "keyrelease": "KeyRelease",
          "enter": "Enter", "leave": "Leave", "configure": "Configure", "mousewheel": "MouseWheel",
          "focusin": "FocusIn", "focusout": "FocusOut", "destroy": "Destroy", "map": "Map",
          "unmap": "Unmap", "visibility": "Visibility", "expose": "Expose"}
_MODS = {"control": "Control", "shift": "Shift", "alt": "Alt", "meta": "Alt", "double": "Double",
         "triple": "Triple", "b1": "B1", "b2": "B2", "b3": "B3", "button1": "B1", "button2": "B2",
         "button3": "B3", "lock": "Lock", "any": "Any", "mod1": "Alt", "command": "Control"}

# Estado (event.state) como no Tk
ESTADO_SHIFT = 1
ESTADO_CONTROL = 4
ESTADO_B1 = 256
ESTADO_B2 = 512
ESTADO_B3 = 1024

_cache_seq = {}


def _analisar(seq):
    """'<Control-Button-1>' -> (tipo, detalhe, mods). Levanta TclError."""
    r = _cache_seq.get(seq)
    if r is not None:
        return r
    s = seq.strip()
    if s.startswith("<<") and s.endswith(">>"):
        r = ("Virtual", s, ())
    elif not s.startswith("<"):
        if len(s) != 1:
            raise TclError('bad event pattern "%s"' % seq)
        r = ("KeyPress", s, ())
    else:
        partes = s[1:-1].split("-")
        mods = []
        tipo = None
        detalhe = None
        for p in partes:
            pl = p.lower()
            if tipo is None and detalhe is None and pl in _MODS:
                mods.append(_MODS[pl])
            elif tipo is None and detalhe is None and pl in _TIPOS:
                tipo = _TIPOS[pl]
            elif detalhe is None:
                detalhe = p
            else:
                raise TclError('bad event pattern "%s"' % seq)
        if tipo is None:
            if detalhe is None:
                raise TclError('bad event pattern "%s"' % seq)
            tipo = "ButtonPress" if detalhe.isdigit() and len(detalhe) == 1 else "KeyPress"
        if tipo in ("ButtonPress", "ButtonRelease") and detalhe is not None:
            if not detalhe.isdigit():
                raise TclError('bad button number "%s"' % detalhe)
            detalhe = int(detalhe)
        if tipo == "KeyPress" and detalhe is not None and len(detalhe) == 1 and detalhe.isalpha() and \
                detalhe.isupper() and "Shift" not in mods:
            pass   # <KeyPress-A> = A maiúsculo (o keysym já distingue)
        r = (tipo, detalhe, tuple(mods))
    _cache_seq[seq] = r
    return r


def _casa(padrao, tipo, detalhe, estado, duplo):
    """Quanto o padrão casa com o evento: -1 = não casa; maior = mais específico."""
    ptipo, pdet, pmods = padrao
    if ptipo != tipo:
        return -1
    pontos = 0
    if pdet is not None:
        if pdet != detalhe:
            return -1
        pontos += 10
    for m in pmods:
        if m == "Double":
            if not duplo:
                return -1
        elif m == "Triple":
            return -1
        elif m == "Control":
            if not estado & ESTADO_CONTROL:
                return -1
        elif m == "Shift":
            if not estado & ESTADO_SHIFT:
                return -1
        elif m == "B1":
            if not estado & ESTADO_B1:
                return -1
        elif m == "B2":
            if not estado & ESTADO_B2:
                return -1
        elif m == "B3":
            if not estado & ESTADO_B3:
                return -1
        elif m == "Alt":
            return -1   # o SaguiOS não entrega Alt nos eventos
        pontos += 1
    return pontos


# Teclas: código do evento do SaguiOS -> keysym do Tk
_KEYSYM_ESPECIAL = {0x100: "Up", 0x101: "Down", 0x102: "Left", 0x103: "Right", 0x104: "Home",
                    0x105: "End", 0x106: "Prior", 0x107: "Next", 0x108: "Insert", 0x109: "Delete",
                    0x116: "Escape", 0x117: "Super_L"}
for _i in range(12):
    _KEYSYM_ESPECIAL[0x10A + _i] = "F%d" % (_i + 1)
_KEYSYM_CHAR = {" ": "space", "!": "exclam", '"': "quotedbl", "#": "numbersign", "$": "dollar",
                "%": "percent", "&": "ampersand", "'": "apostrophe", "(": "parenleft",
                ")": "parenright", "*": "asterisk", "+": "plus", ",": "comma", "-": "minus",
                ".": "period", "/": "slash", ":": "colon", ";": "semicolon", "<": "less",
                "=": "equal", ">": "greater", "?": "question", "@": "at", "[": "bracketleft",
                "\\": "backslash", "]": "bracketright", "^": "asciicircum", "_": "underscore",
                "`": "grave", "{": "braceleft", "|": "bar", "}": "braceright", "~": "asciitilde",
                "\n": "Return", "\r": "Return", "\b": "BackSpace", "\t": "Tab", "\x1b": "Escape",
                "\x7f": "Delete", "ñ": "ntilde", "Ñ": "Ntilde", "á": "aacute", "é": "eacute",
                "í": "iacute", "ó": "oacute", "ú": "uacute", "Á": "Aacute", "É": "Eacute",
                "Í": "Iacute", "Ó": "Oacute", "Ú": "Uacute", "ü": "udiaeresis", "Ü": "Udiaeresis",
                "ç": "ccedilla", "Ç": "Ccedilla", "¡": "exclamdown", "¿": "questiondown",
                "º": "masculine", "ª": "ordfeminine", "·": "periodcentered", "¬": "notsign",
                "€": "EuroSign"}
_CHAR_TECLA = {"\n": "\r", "\b": "\x08"}


def _tecla_evento(e):
    """(keysym, char, estado) de um EV_TECLA."""
    t = e.tecla
    estado = (ESTADO_SHIFT if e.mods & 1 else 0) | (ESTADO_CONTROL if e.mods & 2 else 0)
    if t >= 0x100:
        return _KEYSYM_ESPECIAL.get(t, "??"), "", estado
    c = e.caracter
    if 1 <= t <= 26 and t not in (8, 9, 10, 13):   # Ctrl+letra
        return chr(96 + t), c, estado | ESTADO_CONTROL
    if t == 27:
        return "Escape", "\x1b", estado
    return _KEYSYM_CHAR.get(c, c), _CHAR_TECLA.get(c, c), estado


# ------------------------------------------------------------ laço de eventos

_raiz = None              # a raiz padrão (o primeiro Tk)
_tops = []                # Tk e Toplevel vivos
_por_janela = {}          # id da janela do SaguiOS -> Tk/Toplevel
_tempos = []              # after: [quando, seq, ident, func, args], em ordem
_ociosos = []             # after_idle: [ident, func, args]
_cont_after = 0
_parar = [0]              # quit(): quantos laços devem parar
_modais = []              # diálogos que prendem a entrada (grab_set)
_binds_all = {}           # bind_all
_binds_classe = {}        # bind_class: nome -> {seq: [...]}
_foco = [None]            # componente com o foco do teclado


def _raiz_padrao():
    if _raiz is None:
        Tk()
    return _raiz


def _agendar(ms, func, args):
    global _cont_after
    _cont_after += 1
    ident = "after#%d" % _cont_after
    quando = sagui.ticks_ms() + max(0, int(ms))
    item = [quando, _cont_after, ident, func, args]
    i = len(_tempos)
    while i > 0 and _tempos[i - 1][0] > quando:
        i -= 1
    _tempos.insert(i, item)
    return ident


def _cancelar(ident):
    for lista in (_tempos, _ociosos):
        for i in range(len(lista)):
            if ident in lista[i]:
                del lista[i]
                return


def _rodar_vencidos():
    agora = sagui.ticks_ms()
    n = 0
    while _tempos and _tempos[0][0] <= agora and n < 100:
        item = _tempos.pop(0)
        n += 1
        try:
            item[3](*item[4])
        except Exception as e:
            _relatar(e)


def _ocioso():
    """after_idle, layout e desenho de todas as janelas."""
    while _ociosos:
        item = _ociosos.pop(0)
        try:
            item[1](*item[2])
        except Exception as e:
            _relatar(e)
    for t in list(_tops):
        if not t._destruido:
            t._atualizar_tela()


def _despachar(e):
    j = e.ventana
    top = _por_janela.get(j.id) if j is not None else None
    if top is None or top._destruido:
        return
    try:
        if _modais and e.tipo in (sagui.CLIC, sagui.SOLTAR, sagui.TECLA, sagui.RUEDA, sagui.MOVER, sagui.CERRAR):
            if top is not _modais[-1] and not _modais[-1]._descende_de(top) and not _dentro_do_modal(top):
                if e.tipo == sagui.CLIC:
                    sagui.bip(880, 40)
                return
        top._evento_sistema(e)
    except Exception as ex:
        _relatar(ex)


def _dentro_do_modal(top):
    """Um diálogo aberto por cima do modal (messagebox de um diálogo)."""
    m = _modais[-1]
    w = getattr(top, "_transiente", None)
    while w is not None:
        if w is m:
            return True
        w = getattr(w, "_transiente", None)
    return False


def _alguma_janela():
    for t in _tops:
        if t._v is not None and not t._destruido:
            return True
    return False


def _iteracao(esperar=True):
    """Uma volta do laço: after vencidos, ociosos (desenho), um lote de eventos."""
    _rodar_vencidos()
    _ocioso()
    if esperar:
        prazo = None
        if _ociosos:
            prazo = 0
        elif _tempos:
            prazo = max(0, _tempos[0][0] - sagui.ticks_ms())
    else:
        prazo = 0
    if not _alguma_janela():
        if prazo is None:
            return False
        if prazo:
            sagui.dormir(prazo)
        return True
    e = sagui.evento(prazo)
    n = 0
    while e is not None:
        _despachar(e)
        n += 1
        if n >= 64:
            break
        e = sagui.evento(0)
    return True


def _laco(continuar):
    nivel = _parar[0]
    while continuar() and _parar[0] <= nivel:
        if not _iteracao(True):
            break
    if _parar[0] > nivel:
        _parar[0] -= 1


def mainloop(n=0):
    _raiz_padrao().mainloop(n)


def NoDefaultRoot():
    pass


def image_names():
    return tuple(_imagens.keys())


def image_types():
    return ("photo",)


# ------------------------------------------------------------ base dos componentes

_contagens = {}


class Misc:
    """O que é comum aos componentes e às janelas."""
    _classe = "Misc"
    _padrao = {}
    _aliases = {"bg": "background", "fg": "foreground", "bd": "borderwidth"}
    _opcoes_tamanho = ("text", "textvariable", "width", "height", "font", "padx", "pady", "borderwidth",
                       "image", "compound", "wraplength", "length", "sliderlength", "orient", "label",
                       "showvalue", "highlightthickness", "show", "from", "to", "values", "menu",
                       "selectimage", "underline", "labelanchor", "showhandle")
    _destruido = False
    _foca_no_clique = False

    # ---- opções
    def _iniciar_opcoes(self, cnf, kw):
        op = {}
        for k, v in self._padrao.items():
            op[k] = v
        self._op = op
        if cnf:
            kw = dict(cnf, **kw) if kw else dict(cnf)
        self._configurar(kw, True)

    def _nome_opcao(self, k):
        k = k.lstrip("-")
        if k.endswith("_") and k != "_":
            k = k[:-1]
        return self._aliases.get(k, k)

    def _configurar(self, kw, inicio=False):
        tamanho = False
        for k, v in kw.items():
            n = self._nome_opcao(k)
            if n not in self._op and n not in self._extras():
                raise TclError('unknown option "-%s"' % k)
            antigo = self._op.get(n)
            self._op[n] = v
            if n in ("textvariable", "variable", "listvariable"):
                if isinstance(antigo, Variable) and antigo is not v:
                    try:
                        antigo._ouvintes.remove(self)
                    except ValueError:
                        pass
                if isinstance(v, Variable) and self not in v._ouvintes:
                    v._ouvintes.append(self)
            if n in self._opcoes_tamanho:
                tamanho = True
        if not inicio:
            self._opcoes_mudaram(kw)
            if tamanho:
                self._relayout()
            self._redesenhar()

    def _extras(self):
        return ()

    def _opcoes_mudaram(self, kw):
        pass

    def configure(self, cnf=None, **kw):
        if cnf is None and not kw:
            return {k: (k, k, k.capitalize(), self._padrao.get(k), self._op[k]) for k in self._op}
        if isinstance(cnf, str):
            n = self._nome_opcao(cnf)
            return (n, n, n.capitalize(), self._padrao.get(n), self.cget(n))
        if cnf:
            kw = dict(cnf, **kw)
        self._configurar(kw)

    config = configure

    def cget(self, key):
        n = self._nome_opcao(key)
        if n not in self._op:
            raise TclError('unknown option "-%s"' % key)
        v = self._op[n]
        if n == "text" and isinstance(self._op.get("textvariable"), Variable):
            return self._op["textvariable"].get()
        if n in _OPCOES_COR and isinstance(v, int) and not isinstance(v, bool):
            return "#%06x" % v
        return v

    def __getitem__(self, key):
        return self.cget(key)

    def __setitem__(self, key, value):
        self._configurar({key: value})

    def keys(self):
        return list(self._op.keys())

    def _variavel_mudou(self, var):
        self._relayout()
        self._redesenhar()

    def _texto(self):
        tv = self._op.get("textvariable")
        if isinstance(tv, Variable):
            return str(tv._valor)
        t = self._op.get("text", "")
        return "" if t is None else str(t)

    def _cor_op(self, nome, padrao=None):
        v = _cor(self._op.get(nome))
        return padrao if v is None else v

    # ---- árvore
    def winfo_toplevel(self):
        w = self
        while not isinstance(w, Wm):
            w = w.master
        return w

    def _top(self):
        return self.winfo_toplevel()

    def winfo_children(self):
        return [c for c in self._filhos if not c._destruido]

    def nametowidget(self, name):
        name = str(name)
        w = self._top() if name.startswith(".") else self
        if name == ".":
            return _raiz_padrao()
        for parte in name.strip(".").split("."):
            if not parte:
                continue
            achou = None
            for c in w._filhos:
                if c._name == parte:
                    achou = c
            if achou is None:
                raise KeyError(name)
            w = achou
        return w

    _nametowidget = nametowidget

    def __str__(self):
        return self._w

    def __repr__(self):
        return "<tkinter.%s object %s>" % (type(self).__name__, self._w)

    # ---- after e laço
    def after(self, ms, func=None, *args):
        if func is None:
            sagui.dormir(int(ms))
            return None
        return _agendar(ms, func, args)

    def after_idle(self, func, *args):
        global _cont_after
        _cont_after += 1
        ident = "after#%d" % _cont_after
        _ociosos.append([ident, func, args])
        return ident

    def after_cancel(self, ident):
        if ident:
            _cancelar(ident)

    def after_info(self, ident=None):
        return tuple(t[2] for t in _tempos) + tuple(o[0] for o in _ociosos)

    def update(self):
        for _ in range(4):
            _iteracao(False)

    def update_idletasks(self):
        _ocioso()

    def mainloop(self, n=0):
        top = self._top()
        _laco(lambda: not top._destruido)

    def quit(self):
        _parar[0] += 1

    def wait_window(self, window=None):
        w = self if window is None else window
        _laco(lambda: not w._destruido)

    def wait_variable(self, name):
        var = name
        marca = [False]
        cb = var.trace_add("write", lambda *a: marca.__setitem__(0, True))
        _laco(lambda: not marca[0])
        var.trace_remove("write", cb)

    waitvar = wait_variable

    def wait_visibility(self, window=None):
        self.update()

    def bell(self, displayof=0):
        sagui.bip(880, 80)

    # ---- foco
    def focus_set(self):
        _mudar_foco(self)

    focus = focus_set

    def focus_force(self):
        _mudar_foco(self)

    def focus_get(self):
        f = _foco[0]
        return None if f is None or f._destruido else f

    def focus_displayof(self):
        return self.focus_get()

    def focus_lastfor(self):
        return self.focus_get()

    def tk_focusNext(self):
        return _vizinho_foco(self, 1)

    def tk_focusPrev(self):
        return _vizinho_foco(self, -1)

    def grab_set(self):
        top = self._top()
        if top in _modais:
            _modais.remove(top)
        _modais.append(top)

    grab_set_global = grab_set

    def grab_release(self):
        top = self._top()
        if top in _modais:
            _modais.remove(top)

    def grab_current(self):
        return _modais[-1] if _modais else None

    def grab_status(self):
        return "local" if self._top() in _modais else None

    # ---- bind
    def bind(self, sequence=None, func=None, add=None):
        return _ligar(self._binds, sequence, func, add)

    def unbind(self, sequence, funcid=None):
        _desligar(self._binds, sequence, funcid)

    def bind_all(self, sequence=None, func=None, add=None):
        return _ligar(_binds_all, sequence, func, add)

    def unbind_all(self, sequence):
        _desligar(_binds_all, sequence, None)

    def bind_class(self, className, sequence=None, func=None, add=None):
        d = _binds_classe.setdefault(className, {})
        return _ligar(d, sequence, func, add)

    def unbind_class(self, className, sequence):
        _desligar(_binds_classe.get(className, {}), sequence, None)

    def bindtags(self, tagList=None):
        return (self._w, self._classe, self._top()._w, "all")

    def event_generate(self, sequence, **kw):
        tipo, detalhe, mods = _analisar(sequence)
        ev = Event(tipo if tipo != "Virtual" else EventType.VirtualEvent, self, kw.get("x", 0), kw.get("y", 0))
        for k, v in kw.items():
            setattr(ev, k, v)
        if tipo == "Virtual":
            self._disparar_virtual(sequence, ev)
        else:
            if tipo == "KeyPress":
                ev.keysym = detalhe or "??"
                ev.char = detalhe if detalhe and len(detalhe) == 1 else ""
            if tipo in ("ButtonPress", "ButtonRelease"):
                ev.num = detalhe or 1
            self._disparar(tipo, detalhe, ev, 0, "Double" in mods)

    def event_add(self, virtual, *sequences):
        for s in sequences:
            _virtuais_extra.setdefault(s, []).append(virtual)

    def event_delete(self, virtual, *sequences):
        for s in sequences:
            if virtual in _virtuais_extra.get(s, []):
                _virtuais_extra[s].remove(virtual)

    def event_info(self, virtual=None):
        return ()

    def _disparar_virtual(self, nome, ev):
        ev.type = EventType.VirtualEvent
        for alvo in (self._binds, _binds_classe.get(self._classe, {}), self._top()._binds, _binds_all):
            for padrao, funcs in alvo.values():
                if padrao[0] == "Virtual" and padrao[1] == nome:
                    if _chamar(funcs, ev) == "break":
                        return

    def _disparar(self, tipo, detalhe, ev, estado, duplo=False):
        """Entrega o evento pela ordem dos bindtags: o componente, a classe
        (bind_class e o comportamento próprio do componente), a janela e "all"."""
        ev.state = estado
        alvos = [self._binds, _binds_classe.get(self._classe), _PROPRIO]
        top = self._top()
        if top is not self:
            alvos.append(top._binds)
        alvos.append(_binds_all)
        for alvo in alvos:
            if alvo is _PROPRIO:
                if self._tratar(tipo, ev) == "break":
                    return "break"
                continue
            if not alvo:
                continue
            melhor, pontos = None, -1
            for padrao, funcs in alvo.values():
                p = _casa(padrao, tipo, detalhe, estado, duplo)
                if p > pontos:
                    melhor, pontos = funcs, p
            if melhor is not None and _chamar(melhor, ev) == "break":
                return "break"
        if tipo == "KeyPress" and ev.keysym == "Tab" and self._tab_navega():
            alvo = _vizinho_foco(self, -1 if estado & ESTADO_SHIFT else 1)
            if alvo is not None:
                _mudar_foco(alvo)
        return None

    def _tab_navega(self):
        return True

    def _tratar(self, tipo, ev):
        """Comportamento próprio do componente (a "classe" do Tk)."""
        return None

    # ---- winfo
    def winfo_width(self):
        self._top()._garantir_layout()
        return max(1, self._wl)

    def winfo_height(self):
        self._top()._garantir_layout()
        return max(1, self._wa)

    def winfo_reqwidth(self):
        return self._pedido()[0]

    def winfo_reqheight(self):
        return self._pedido()[1]

    def winfo_x(self):
        self._top()._garantir_layout()
        if isinstance(self, Wm):
            return 0
        return self._ax - self.master._ax

    def winfo_y(self):
        self._top()._garantir_layout()
        if isinstance(self, Wm):
            return 0
        return self._ay - self.master._ay

    def winfo_rootx(self):
        self._top()._garantir_layout()
        return self._ax

    def winfo_rooty(self):
        self._top()._garantir_layout()
        return self._ay

    def winfo_pointerx(self):
        return self._top()._mouse_xy[0]

    def winfo_pointery(self):
        return self._top()._mouse_xy[1]

    def winfo_pointerxy(self):
        return self._top()._mouse_xy

    def winfo_screenwidth(self):
        return sagui.pantalla()[0]

    def winfo_screenheight(self):
        return sagui.pantalla()[1]

    def winfo_vrootwidth(self):
        return self.winfo_screenwidth()

    def winfo_vrootheight(self):
        return self.winfo_screenheight()

    def winfo_exists(self):
        return 0 if self._destruido else 1

    def winfo_ismapped(self):
        return 1 if self._mapeado() else 0

    def winfo_viewable(self):
        return self.winfo_ismapped()

    def winfo_manager(self):
        return self._gestor or ""

    def winfo_class(self):
        return self._classe

    def winfo_name(self):
        return self._name

    def winfo_parent(self):
        return "" if self.master is None else str(self.master)

    def winfo_id(self):
        return id(self)

    def winfo_rgb(self, color):
        c = _cor(color)
        return ((c >> 16) * 257, ((c >> 8) & 255) * 257, (c & 255) * 257)

    def winfo_fpixels(self, n):
        return float(_int(n))

    def winfo_pixels(self, n):
        return _int(n)

    def winfo_containing(self, x, y):
        return self._top()._componente_em(x, y)

    def winfo_geometry(self):
        return "%dx%d+%d+%d" % (self.winfo_width(), self.winfo_height(), self.winfo_x(), self.winfo_y())

    # ---- área de transferência
    def clipboard_clear(self, **kw):
        sagui.portapapeles("")

    def clipboard_append(self, string, **kw):
        sagui.portapapeles(sagui.portapapeles() + str(string))

    def clipboard_get(self, **kw):
        t = sagui.portapapeles()
        if not t:
            raise TclError("CLIPBOARD selection doesn't exist or form \"STRING\" not defined")
        return t

    def selection_get(self, **kw):
        return self.clipboard_get()

    # ---- opções globais (aceitas e ignoradas: não há banco de opções)
    def option_add(self, pattern, value, priority=None):
        pass

    def option_clear(self):
        pass

    def option_get(self, name, className):
        return ""

    def tk_setPalette(self, *args, **kw):
        pass

    def tk_strictMotif(self, boolean=None):
        return 0

    def register(self, func, subst=None, needcleanup=1):
        return func

    _register = register

    def getvar(self, name="PY_VAR"):
        raise TclError("no Tcl in SaguiOS")

    def setvar(self, name="PY_VAR", value="1"):
        raise TclError("no Tcl in SaguiOS")

    def eval(self, *args):
        raise TclError("tkinter de SaguiOS: no hay intérprete Tcl (eval/call no están disponibles)")

    call = eval

    def image_names(self):
        return image_names()

    def lift(self, aboveThis=None):
        self._erguer(True)

    tkraise = lift

    def lower(self, belowThis=None):
        self._erguer(False)

    def _erguer(self, cima):
        m = self.master
        if m is None or isinstance(self, Wm):
            return
        m._filhos.remove(self)
        if cima:
            m._filhos.append(self)
        else:
            m._filhos.insert(0, self)
        m._redesenhar()

    # ---- grid de um contêiner
    def grid_columnconfigure(self, index, cnf={}, **kw):
        return self._grid_conf(self._gcolunas, index, dict(cnf, **kw))

    columnconfigure = grid_columnconfigure

    def grid_rowconfigure(self, index, cnf={}, **kw):
        return self._grid_conf(self._glinhas, index, dict(cnf, **kw))

    rowconfigure = grid_rowconfigure

    def _grid_conf(self, tabela, index, kw):
        if isinstance(index, (list, tuple)):
            for i in index:
                self._grid_conf(tabela, i, kw)
            return None
        if index == "all":
            n = self.grid_size()[0 if tabela is self._gcolunas else 1]
            for i in range(n):
                self._grid_conf(tabela, i, kw)
            return None
        if isinstance(index, Misc):
            info = index._ginfo
            index = info.get("column" if tabela is self._gcolunas else "row", 0)
        index = int(index)
        conf = tabela.setdefault(index, {"weight": 0, "minsize": 0, "pad": 0, "uniform": ""})
        if not kw:
            return dict(conf)
        for k, v in kw.items():
            k = k.lstrip("-")
            if k not in conf:
                raise TclError('bad option "-%s"' % k)
            conf[k] = v if k == "uniform" else _int(v)
        self._relayout()
        return None

    def grid_size(self):
        cols = rows = 0
        for s in self._escravos:
            if s._gestor == "grid":
                cols = max(cols, s._ginfo["column"] + s._ginfo["columnspan"])
                rows = max(rows, s._ginfo["row"] + s._ginfo["rowspan"])
        return cols, rows

    size = grid_size

    def grid_slaves(self, row=None, column=None):
        r = []
        for s in self._escravos:
            if s._gestor != "grid":
                continue
            if row is not None and s._ginfo["row"] != row:
                continue
            if column is not None and s._ginfo["column"] != column:
                continue
            r.append(s)
        r.reverse()
        return r

    def grid_propagate(self, flag=None):
        if flag is None:
            return self._propaga
        self._propaga = _bool(flag)
        self._relayout()

    def grid_anchor(self, anchor=None):
        if anchor is None:
            return self._ancora_grid
        self._ancora_grid = anchor
        self._relayout()

    def grid_bbox(self, column=None, row=None, col2=None, row2=None):
        self._top()._garantir_layout()
        return (0, 0, self._wl, self._wa)

    def pack_slaves(self):
        return [s for s in self._escravos if s._gestor == "pack"]

    slaves = pack_slaves

    def pack_propagate(self, flag=None):
        if flag is None:
            return self._propaga
        self._propaga = _bool(flag)
        self._relayout()

    propagate = pack_propagate

    def place_slaves(self):
        return [s for s in self._escravos if s._gestor == "place"]

    # ---- estado interno (preenchido por _iniciar_misc)
    def _iniciar_misc(self, master, nome):
        self.master = master
        self.children = {}
        self._filhos = []
        self._escravos = []
        self._gcolunas = {}
        self._glinhas = {}
        self._propaga = True
        self._ancora_grid = "nw"
        self._binds = {}
        self._gestor = None
        self._ginfo = {}
        self._ax = self._ay = 0
        self._wl = self._wa = 0
        self._sujo = 2
        self._dentro = False
        if master is None:
            self._name = nome or ""
            self._w = "."
        else:
            if nome is None:
                base = "!" + type(self).__name__.lower()
                n = _contagens.get((id(master), base), 0) + 1
                _contagens[(id(master), base)] = n
                nome = base if n == 1 else "%s%d" % (base, n)
            self._name = nome
            self._w = ("." if master._w == "." else master._w + ".") + nome
            if nome in master.children:
                master.children[nome].destroy()
            master.children[nome] = self
            master._filhos.append(self)

    def destroy(self):
        if self._destruido:
            return
        for c in list(self._filhos):
            c.destroy()
        ev = Event(EventType.Destroy, self)
        try:
            self._disparar("Destroy", None, ev, 0)
        except Exception as e:
            _relatar(e)
        self._destruido = True
        for k in ("textvariable", "variable", "listvariable"):
            v = self._op.get(k) if hasattr(self, "_op") else None
            if isinstance(v, Variable) and self in v._ouvintes:
                v._ouvintes.remove(self)
        if _foco[0] is self:
            _foco[0] = None
        m = self.master
        if m is not None and isinstance(self, Wm):
            if self in m._filhos:
                m._filhos.remove(self)
            if m.children.get(self._name) is self:
                del m.children[self._name]
        elif m is not None:
            if self in m._filhos:
                m._filhos.remove(self)
            if m.children.get(self._name) is self:
                del m.children[self._name]
            if self in m._escravos:
                m._escravos.remove(self)
                m._relayout()
            m._redesenhar()

    def _mapeado(self):
        w = self
        while not isinstance(w, Wm):
            if w._gestor is None or w._destruido:
                return False
            w = w.master
        return not w._destruido and w._v is not None

    # Os desenhos e o layout: implementados em Widget e Wm.
    def _redesenhar(self, nivel=2):
        pass

    def _relayout(self):
        pass


_virtuais_extra = {}
_OPCOES_COR = ("background", "foreground", "activebackground", "activeforeground", "disabledforeground",
               "selectbackground", "selectforeground", "insertbackground", "troughcolor", "selectcolor",
               "highlightbackground", "highlightcolor", "readonlybackground", "disabledbackground",
               "buttonbackground", "inactiveselectbackground")
_PROPRIO = object()   # o passo "classe" dos bindtags: o comportamento do componente


def _ligar(tabela, sequence, func, add):
    if sequence is None:
        return tuple(tabela.keys())
    if func is None:
        r = tabela.get(sequence)
        return "" if r is None else r[1]
    padrao = _analisar(sequence)
    if add and sequence in tabela:
        tabela[sequence][1].append(func)
    else:
        tabela[sequence] = (padrao, [func])
    return "%d%s" % (id(func), getattr(func, "__name__", "func"))


def _desligar(tabela, sequence, funcid):
    if sequence not in tabela:
        return
    if funcid is None:
        del tabela[sequence]
        return
    funcs = tabela[sequence][1]
    for f in list(funcs):
        if "%d%s" % (id(f), getattr(f, "__name__", "func")) == funcid:
            funcs.remove(f)
    if not funcs:
        del tabela[sequence]


def _chamar(funcs, ev):
    r = None
    for f in list(funcs):
        try:
            r = f(ev)
        except Exception as e:
            _relatar(e)
            r = None
        if r == "break":
            return "break"
    return r


def _mudar_foco(w):
    antigo = _foco[0]
    if antigo is w:
        return
    _foco[0] = w
    if antigo is not None and not antigo._destruido:
        antigo._foco_mudou(False)
        antigo._disparar("FocusOut", None, Event("10", antigo), 0)
    if w is not None:
        w._foco_mudou(True)
        w._disparar("FocusIn", None, Event("9", w), 0)


def _focaveis(top, lista):
    for c in top._filhos:
        if c._destruido or c._gestor is None or isinstance(c, Wm):
            continue
        if c._aceita_foco():
            lista.append(c)
        _focaveis(c, lista)
    return lista


def _vizinho_foco(w, passo):
    lista = _focaveis(w._top(), [])
    if not lista:
        return None
    if w in lista:
        return lista[(lista.index(w) + passo) % len(lista)]
    return lista[0]


# ------------------------------------------------------------ geometria

_PACK_PADRAO = {"side": "top", "fill": "none", "expand": False, "anchor": "center", "padx": (0, 0),
                "pady": (0, 0), "ipadx": 0, "ipady": 0}
_GRID_PADRAO = {"row": 0, "column": 0, "rowspan": 1, "columnspan": 1, "sticky": "", "padx": (0, 0),
                "pady": (0, 0), "ipadx": 0, "ipady": 0}
_PLACE_PADRAO = {"x": 0, "y": 0, "relx": 0.0, "rely": 0.0, "width": None, "height": None,
                 "relwidth": None, "relheight": None, "anchor": "nw", "bordermode": "inside"}


def _pack_pedido(escravos):
    largura = altura = maxl = maxa = 0
    for s in escravos:
        rl, ra = s._pedido()
        i = s._ginfo
        sl = rl + i["padx"][0] + i["padx"][1] + 2 * i["ipadx"]
        sa = ra + i["pady"][0] + i["pady"][1] + 2 * i["ipady"]
        if i["side"] in ("top", "bottom"):
            maxl = max(maxl, sl + largura)
            altura += sa
        else:
            maxa = max(maxa, sa + altura)
            largura += sl
    return max(maxl, largura), max(maxa, altura)


def _pack_arranjar(escravos, cx, cy, cl, ca):
    """O empacotador: cada escravo pega uma faixa do lado pedido da cavidade
    que sobra; os com expand dividem entre si o espaço que ninguém pediu."""
    pedidos = []
    for s in escravos:
        rl, ra = s._pedido()
        i = s._ginfo
        pedidos.append((rl, ra, i["padx"][0] + i["padx"][1] + 2 * i["ipadx"], i["pady"][0] + i["pady"][1] + 2 * i["ipady"]))
    for k in range(len(escravos)):
        s = escravos[k]
        i = s._ginfo
        rl, ra, ppx, ppy = pedidos[k]
        vertical = i["side"] in ("top", "bottom")
        if vertical:
            fl = cl
            fa = ra + ppy
            if i["expand"]:
                resto = ca
                n = 0
                for j in range(k, len(escravos)):
                    if escravos[j]._ginfo["side"] in ("top", "bottom"):
                        resto -= pedidos[j][1] + pedidos[j][3]
                        if escravos[j]._ginfo["expand"]:
                            n += 1
                if n and resto > 0:
                    fa += resto // n
            if fa > ca:
                fa = max(0, ca)
            fx = cx
            if i["side"] == "top":
                fy = cy
                cy += fa
            else:
                fy = cy + ca - fa
            ca -= fa
        else:
            fa = ca
            fl = rl + ppx
            if i["expand"]:
                resto = cl
                n = 0
                for j in range(k, len(escravos)):
                    if escravos[j]._ginfo["side"] in ("left", "right"):
                        resto -= pedidos[j][0] + pedidos[j][2]
                        if escravos[j]._ginfo["expand"]:
                            n += 1
                if n and resto > 0:
                    fl += resto // n
            if fl > cl:
                fl = max(0, cl)
            fy = cy
            if i["side"] == "left":
                fx = cx
                cx += fl
            else:
                fx = cx + cl - fl
            cl -= fl
        # o escravo dentro da faixa
        pl0, pl1 = i["padx"]
        pa0, pa1 = i["pady"]
        w = rl + 2 * i["ipadx"]
        h = ra + 2 * i["ipady"]
        if i["fill"] in ("x", "both") or w > fl - pl0 - pl1:
            w = fl - pl0 - pl1
        if i["fill"] in ("y", "both") or h > fa - pa0 - pa1:
            h = fa - pa0 - pa1
        x, y = _ancorar(i["anchor"], fx + pl0, fy + pa0, fl - pl0 - pl1, fa - pa0 - pa1, w, h)
        s._colocar(x, y, max(0, w), max(0, h))


def _grid_tabelas(m):
    """Tamanhos pedidos das colunas e das linhas de um contêiner com grid."""
    escravos = [s for s in m._escravos if s._gestor == "grid"]
    ncol, nlin = m.grid_size()
    for i in m._gcolunas:
        ncol = max(ncol, i + 1) if m._gcolunas[i]["weight"] or m._gcolunas[i]["minsize"] else ncol
    for i in m._glinhas:
        nlin = max(nlin, i + 1) if m._glinhas[i]["weight"] or m._glinhas[i]["minsize"] else nlin
    cols = [m._gcolunas.get(i, {}).get("minsize", 0) for i in range(ncol)]
    lins = [m._glinhas.get(i, {}).get("minsize", 0) for i in range(nlin)]
    for s in escravos:   # primeiro os de uma célula, depois os que atravessam várias
        i = s._ginfo
        rl, ra = s._pedido()
        if i["columnspan"] == 1:
            cols[i["column"]] = max(cols[i["column"]], rl + i["padx"][0] + i["padx"][1] + 2 * i["ipadx"])
        if i["rowspan"] == 1:
            lins[i["row"]] = max(lins[i["row"]], ra + i["pady"][0] + i["pady"][1] + 2 * i["ipady"])
    for s in escravos:
        i = s._ginfo
        rl, ra = s._pedido()
        for tab, ini, n, pedido, conf in ((cols, i["column"], i["columnspan"], rl + i["padx"][0] + i["padx"][1] + 2 * i["ipadx"], m._gcolunas),
                                          (lins, i["row"], i["rowspan"], ra + i["pady"][0] + i["pady"][1] + 2 * i["ipady"], m._glinhas)):
            if n == 1:
                continue
            tem = sum(tab[ini:ini + n])
            if pedido > tem:
                pesos = [conf.get(k, {}).get("weight", 0) for k in range(ini, ini + n)]
                total = sum(pesos)
                falta = pedido - tem
                for k in range(n):
                    parte = (falta * pesos[k] // total) if total else (falta // n + (1 if k < falta % n else 0))
                    tab[ini + k] += parte
                if total:
                    tab[ini + n - 1] += falta - sum(falta * p // total for p in pesos)
    for tab, conf in ((cols, m._gcolunas), (lins, m._glinhas)):
        for k in range(len(tab)):
            tab[k] += conf.get(k, {}).get("pad", 0)
        # uniform: as do mesmo grupo ficam do tamanho da maior (proporcional ao peso)
        grupos = {}
        for k in range(len(tab)):
            u = conf.get(k, {}).get("uniform", "")
            if u:
                grupos.setdefault(u, []).append(k)
        for ks in grupos.values():
            maior = max(tab[k] / max(1, conf.get(k, {}).get("weight", 1)) for k in ks)
            for k in ks:
                tab[k] = int(maior * max(1, conf.get(k, {}).get("weight", 1)))
    return escravos, cols, lins


def _distribuir(tab, conf, total):
    """Ajusta as faixas ao espaço: a sobra (ou a falta) vai para as de peso."""
    soma = sum(tab)
    extra = total - soma
    pesos = [conf.get(k, {}).get("weight", 0) for k in range(len(tab))]
    sp = sum(pesos)
    if extra == 0 or sp == 0:
        return list(tab), extra
    r = list(tab)
    if extra > 0:
        dado = 0
        for k in range(len(r)):
            parte = extra * pesos[k] // sp
            r[k] += parte
            dado += parte
        k = len(r) - 1
        while dado < extra and k >= 0:
            if pesos[k]:
                r[k] += 1
                dado += 1
            k -= 1
        return r, 0
    # falta: encolhe as de peso, sem passar do mínimo
    falta = -extra
    for _ in range(4):
        if falta <= 0:
            break
        ativos = [k for k in range(len(r)) if pesos[k] and r[k] > conf.get(k, {}).get("minsize", 0)]
        if not ativos:
            break
        sp2 = sum(pesos[k] for k in ativos)
        tirado = 0
        for k in ativos:
            parte = min(r[k] - conf.get(k, {}).get("minsize", 0), max(1, falta * pesos[k] // sp2))
            r[k] -= parte
            tirado += parte
        falta -= tirado
    return r, -falta if falta > 0 else 0


def _grid_pedido(m):
    escravos, cols, lins = _grid_tabelas(m)
    return sum(cols), sum(lins)


def _grid_arranjar(m, cx, cy, cl, ca):
    escravos, cols, lins = _grid_tabelas(m)
    cols, sobra_l = _distribuir(cols, m._gcolunas, cl)
    lins, sobra_a = _distribuir(lins, m._glinhas, ca)
    ox, oy = _ancorar(m._ancora_grid, cx, cy, cl, ca, sum(cols), sum(lins))
    xs = [ox]
    for c in cols:
        xs.append(xs[-1] + c)
    ys = [oy]
    for l in lins:
        ys.append(ys[-1] + l)
    for s in escravos:
        i = s._ginfo
        c0, r0 = i["column"], i["row"]
        c1, r1 = c0 + i["columnspan"], r0 + i["rowspan"]
        x0, y0 = xs[c0] + i["padx"][0], ys[r0] + i["pady"][0]
        wl = xs[c1] - xs[c0] - i["padx"][0] - i["padx"][1]
        wa = ys[r1] - ys[r0] - i["pady"][0] - i["pady"][1]
        rl, ra = s._pedido()
        rl += 2 * i["ipadx"]
        ra += 2 * i["ipady"]
        st = i["sticky"]
        w = wl if ("e" in st and "w" in st) else min(rl, wl)
        h = wa if ("n" in st and "s" in st) else min(ra, wa)
        if "w" in st and "e" not in st:
            x = x0
        elif "e" in st and "w" not in st:
            x = x0 + wl - w
        else:
            x = x0 + (wl - w) // 2
        if "n" in st and "s" not in st:
            y = y0
        elif "s" in st and "n" not in st:
            y = y0 + wa - h
        else:
            y = y0 + (wa - h) // 2
        s._colocar(x, y, max(0, w), max(0, h))


def _place_arranjar(m, escravos, cx, cy, cl, ca):
    for s in escravos:
        i = s._ginfo
        rl, ra = s._pedido()
        w = rl
        h = ra
        if i["width"] is not None:
            w = _int(i["width"])
        if i["relwidth"] is not None:
            w = int(float(i["relwidth"]) * cl) + (_int(i["width"]) if i["width"] is not None else 0)
        if i["height"] is not None:
            h = _int(i["height"])
        if i["relheight"] is not None:
            h = int(float(i["relheight"]) * ca) + (_int(i["height"]) if i["height"] is not None else 0)
        x = cx + _int(i["x"]) + int(float(i["relx"]) * cl)
        y = cy + _int(i["y"]) + int(float(i["rely"]) * ca)
        a = i["anchor"]
        if a == "center":
            a = ""
        if "e" in a:
            x -= w
        elif "w" not in a:
            x -= w // 2
        if "s" in a:
            y -= h
        elif "n" not in a:
            y -= h // 2
        s._colocar(x, y, max(0, w), max(0, h))


# ------------------------------------------------------------ Widget

class Widget(Misc):
    """Base de todos os componentes."""
    _classe = "Widget"
    _foca_no_clique = False

    def __init__(self, master=None, cnf={}, **kw):
        if master is None:
            master = _raiz_padrao()
        nome = kw.pop("name", None)
        self._iniciar_misc(master, nome)
        self._iniciar_opcoes(cnf, kw)
        self._iniciar()

    def _iniciar(self):
        pass

    # ---- geometria
    def _pedido(self):
        """Tamanho pedido (requested): o dos escravos se houver e propagar,
        senão o natural do componente."""
        top = self._top()
        cache = top._cache_pedido
        r = cache.get(id(self))
        if r is not None:
            return r
        r = self._natural()
        escravos = [s for s in self._escravos if not s._destruido and s._gestor in ("pack", "grid")]
        if escravos and self._propaga:
            b = self._borda_interna()
            if escravos[0]._gestor == "pack":
                l, a = _pack_pedido(escravos)
            else:
                l, a = _grid_pedido(self)
            px, py = self._padding()
            r = (l + 2 * b + 2 * px, a + 2 * b + 2 * py + self._extra_topo())
        cache[id(self)] = r
        return r

    def _natural(self):
        return (_int(self._op.get("width", 0)), _int(self._op.get("height", 0)))

    def _borda_interna(self):
        return _int(self._op.get("borderwidth", 0)) + _int(self._op.get("highlightthickness", 0))

    def _padding(self):
        return 0, 0

    def _extra_topo(self):
        return 0

    def _area_interna(self):
        b = self._borda_interna()
        px, py = self._padding()
        t = self._extra_topo()
        return (self._ax + b + px, self._ay + b + py + t, self._wl - 2 * b - 2 * px, self._wa - 2 * b - 2 * py - t)

    def _colocar(self, x, y, w, h):
        mudou = (x, y, w, h) != (self._ax, self._ay, self._wl, self._wa)
        tamanho = (w, h) != (self._wl, self._wa)
        self._ax, self._ay, self._wl, self._wa = x, y, w, h
        if mudou:
            self._sujo = 2
            self._top()._houve_mudanca = True
        if tamanho:
            self._top()._configurados.append(self)
        self._arranjar()

    def _arranjar(self):
        escravos = [s for s in self._escravos if not s._destruido]
        if not escravos:
            return
        cx, cy, cl, ca = self._area_interna()
        packs = [s for s in escravos if s._gestor == "pack"]
        if packs:
            _pack_arranjar(packs, cx, cy, cl, ca)
        elif any(s._gestor == "grid" for s in escravos):
            _grid_arranjar(self, cx, cy, cl, ca)
        places = [s for s in escravos if s._gestor == "place"]
        if places:
            b = self._borda_interna()
            _place_arranjar(self, places, self._ax + b, self._ay + b + self._extra_topo(), self._wl - 2 * b,
                            self._wa - 2 * b - self._extra_topo())

    def _gerir(self, gestor, padrao, kw):
        m = kw.pop("in_", None) or kw.pop("in", None) or self.master
        if gestor in ("pack", "grid"):
            for s in m._escravos:
                if s is not self and s._gestor in ("pack", "grid") and s._gestor != gestor:
                    raise TclError("cannot use geometry manager %s inside %s which already has slaves managed by %s"
                                   % (gestor, m._w, s._gestor))
        if self._gestor is not None and (self._gestor != gestor or self._ginfo.get("_in") is not m):
            self._esquecer()
        if self._gestor != gestor:
            info = dict(padrao)
            novo = True
        else:
            info = self._ginfo
            novo = False
        for k, v in kw.items():
            k = k.lstrip("-")
            if k.endswith("_"):
                k = k[:-1]
            if k in ("padx", "pady"):
                info[k] = _pares(v)
            elif k in ("ipadx", "ipady", "row", "column", "rowspan", "columnspan"):
                info[k] = _int(v)
            elif k == "expand":
                info[k] = _bool(v)
            elif k in ("before", "after"):
                info[k] = v
            elif k in padrao:
                info[k] = v
            else:
                raise TclError('bad option "-%s": must be %s' % (k, ", ".join("-" + o for o in padrao)))
        if gestor == "grid":
            info["sticky"] = str(info["sticky"]).lower().replace(",", "").replace(" ", "")
            if novo and "row" not in kw:
                info["row"] = m.grid_size()[1] if "column" not in kw else max(
                    [s._ginfo["row"] + s._ginfo["rowspan"] for s in m._escravos
                     if s._gestor == "grid" and s._ginfo["column"] == info["column"]] or [0])
        info["_in"] = m
        self._gestor = gestor
        self._ginfo = info
        if novo:
            antes = info.pop("before", None)
            depois = info.pop("after", None)
            if antes is not None and antes in m._escravos:
                m._escravos.insert(m._escravos.index(antes), self)
            elif depois is not None and depois in m._escravos:
                m._escravos.insert(m._escravos.index(depois) + 1, self)
            else:
                m._escravos.append(self)
        self._sujo = 2
        m._relayout()

    def _esquecer(self):
        m = self._ginfo.get("_in", self.master)
        if self in m._escravos:
            m._escravos.remove(self)
        self._gestor = None
        m._relayout()
        m._redesenhar()

    def pack_configure(self, cnf={}, **kw):
        self._gerir("pack", _PACK_PADRAO, dict(cnf, **kw))

    pack = pack_configure

    def pack_forget(self):
        if self._gestor == "pack":
            self._esquecer()

    forget = pack_forget

    def pack_info(self):
        d = dict(self._ginfo)
        d["in"] = d.pop("_in", self.master)
        return d

    info = pack_info

    def grid_configure(self, cnf={}, **kw):
        self._gerir("grid", _GRID_PADRAO, dict(cnf, **kw))

    grid = grid_configure

    def grid_forget(self):
        if self._gestor == "grid":
            self._esquecer()

    def grid_remove(self):
        self.grid_forget()

    def grid_info(self):
        d = dict(self._ginfo)
        d["in"] = d.pop("_in", self.master)
        return d

    def place_configure(self, cnf={}, **kw):
        self._gerir("place", _PLACE_PADRAO, dict(cnf, **kw))

    place = place_configure

    def place_forget(self):
        if self._gestor == "place":
            self._esquecer()

    def place_info(self):
        d = dict(self._ginfo)
        d["in"] = d.pop("_in", self.master)
        return d

    def _relayout(self):
        if not self._destruido:
            t = self._top()
            t._layout_pendente = True

    def _redesenhar(self, nivel=2):
        if nivel > self._sujo:
            self._sujo = nivel
        if not self._destruido:
            self._top()._algum_sujo = True

    # ---- desenho
    def _desenhar(self, v):
        self._fundo_e_borda(v)

    def _desenhar_parcial(self, v):
        self._desenhar(v)

    def _fundo_e_borda(self, v, fundo=None, relief=None):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        if fundo is None:
            fundo = self._cor_op("background", FUNDO)
        v.rect(x, y, w, h, fundo)
        _relevo(v, x, y, w, h, relief or self._op.get("relief", "flat"), _int(self._op.get("borderwidth", 0)), fundo)

    def _ativo(self):
        return self._op.get("state", "normal") != "disabled"

    def _aceita_foco(self):
        tf = self._op.get("takefocus", "")
        if tf in (0, "0", False):
            return False
        return self._ativo() and (tf in (1, "1", True) or self._foca_tab())

    def _foca_tab(self):
        return False

    def _foco_mudou(self, ganhou):
        self._redesenhar()

    def _dentro_xy(self, x, y):
        return self._ax <= x < self._ax + self._wl and self._ay <= y < self._ay + self._wa


# ------------------------------------------------------------ janelas (Wm, Tk, Toplevel)

class Wm:
    """Janela de topo: Tk e Toplevel. Uma janela do SaguiOS (sagui.Ventana),
    criada quando é preciso mostrar alguma coisa."""
    _v = None

    def _iniciar_janela(self, dialogo=False):
        self._titulo = "tk"
        self._geometria = None        # (l, a) pedida com geometry() ou pelo usuário
        self._redimensionavel = (True, True)
        self._minimo = (64, 32)
        self._protocolos = {}
        self._menu_barra = None
        self._cache_pedido = {}
        self._layout_pendente = True
        self._algum_sujo = True
        self._houve_mudanca = False
        self._configurados = []
        self._retirada = False
        self._agarrado = None
        self._botao = 0
        self._sob_mouse = None
        self._mouse_xy = (0, 0)
        self._dialogo = dialogo
        self._transiente = None
        _tops.append(self)

    # ---- wm
    def wm_title(self, string=None):
        if string is None:
            return self._titulo
        self._titulo = str(string)
        if self._v is not None:
            self._v.titulo(self._titulo)

    title = wm_title

    def wm_geometry(self, newGeometry=None):
        if newGeometry is None:
            self._garantir_layout()
            return "%dx%d+0+0" % (self._wl, self._wa)
        g = str(newGeometry)
        tam = g.split("+")[0].split("-")[0]
        if "x" in tam:
            l, a = tam.split("x")
            self._geometria = (int(l), int(a))
            self._layout_pendente = True
        return ""

    geometry = wm_geometry

    def wm_resizable(self, width=None, height=None):
        if width is None:
            return self._redimensionavel
        if height is None:
            height = width
        self._redimensionavel = (_bool(width), _bool(height))
        if self._v is not None:
            self._v.estilo(redimensionable=any(self._redimensionavel))

    resizable = wm_resizable

    def wm_minsize(self, width=None, height=None):
        if width is None:
            return self._minimo
        self._minimo = (max(16, _int(width)), max(16, _int(height)))
        if self._v is not None:
            self._v.estilo(minimo=self._minimo)

    minsize = wm_minsize

    def wm_maxsize(self, width=None, height=None):
        return (1000, 700)

    maxsize = wm_maxsize

    def wm_protocol(self, name=None, func=None):
        if name is None:
            return tuple(self._protocolos.keys())
        if func is None:
            return self._protocolos.get(name, "")
        self._protocolos[name] = func
        return ""

    protocol = wm_protocol

    def wm_withdraw(self):
        self._retirada = True
        if self._v is not None:
            del _por_janela[self._v.id]
            self._v.cerrar()
            self._v = None

    withdraw = wm_withdraw

    def wm_deiconify(self):
        self._retirada = False
        self._layout_pendente = True

    deiconify = wm_deiconify

    def wm_iconify(self):
        pass

    iconify = wm_iconify

    def wm_state(self, newstate=None):
        if newstate is None:
            return "withdrawn" if self._retirada else "normal"
        if newstate == "withdrawn":
            self.wm_withdraw()
        elif newstate == "normal":
            self.wm_deiconify()
        elif newstate == "zoomed" and self._v is not None:
            self._v.redimensionar(1000, 700)

    state = wm_state

    def wm_attributes(self, *args, **kw):
        if len(args) == 2 and args[0] == "-fullscreen" and self._v is not None:
            self._v.pantalla_completa(_bool(args[1]))
        for k, v in kw.items():
            if k == "fullscreen" and self._v is not None:
                self._v.pantalla_completa(_bool(v))
        return None

    attributes = wm_attributes

    def wm_transient(self, master=None):
        self._transiente = master

    transient = wm_transient

    def _ignorar(self, *args, **kw):
        return ""

    wm_iconbitmap = iconbitmap = wm_iconphoto = iconphoto = wm_iconname = iconname = _ignorar
    wm_overrideredirect = overrideredirect = wm_group = group = wm_aspect = aspect = _ignorar
    wm_positionfrom = positionfrom = wm_sizefrom = sizefrom = wm_focusmodel = focusmodel = _ignorar
    wm_client = client = wm_command = command = wm_colormapwindows = _ignorar
    wm_iconmask = iconmask = wm_iconposition = iconposition = wm_iconwindow = iconwindow = _ignorar
    wm_frame = frame = _ignorar

    def _descende_de(self, outro):
        return False

    # ---- layout e desenho
    def _extra_topo(self):
        return 22 if self._menu_barra is not None else 0

    def _garantir_layout(self):
        if self._layout_pendente:
            self._fazer_layout()

    def _fazer_layout(self):
        self._layout_pendente = False
        self._cache_pedido = {}
        self._configurados = []
        if self._geometria is not None:
            l, a = self._geometria
        else:
            l, a = self._pedido()
            l = max(l, 1)
            a = max(a, 1)
        l = max(min(l, 1000), 16)
        a = max(min(a, 700), 16)
        if self._v is not None and (l, a) != self._v.tamano():
            if self._geometria is None or self._geometria != self._v.tamano():
                try:
                    self._v.redimensionar(l, a)
                except (ValueError, OSError):
                    l, a = self._v.tamano()
        self._houve_mudanca = False
        self._ax = self._ay = 0
        self._colocar(0, 0, l, a)
        if self._houve_mudanca:
            self._sujo = 2
            self._algum_sujo = True
        conf = self._configurados
        self._configurados = []
        for w in conf:
            if not w._destruido:
                ev = Event(EventType.Configure, w)
                ev.width, ev.height = w._wl, w._wa
                ev.x, ev.y = w.winfo_x(), w.winfo_y()
                w._disparar("Configure", None, ev, 0)

    def _colocar(self, x, y, w, h):
        if (w, h) != (self._wl, self._wa):
            self._configurados.append(self)
        self._wl, self._wa = w, h
        self._houve_mudanca = True
        Widget._arranjar(self)

    def _criar_janela(self):
        l, a = self._wl, self._wa
        self._v = sagui.Ventana(max(l, 16), max(a, 16), self._titulo,
                                redimensionable=any(self._redimensionavel), dialogo=self._dialogo,
                                minimo=self._minimo)
        _por_janela[self._v.id] = self
        self._sujo = 2
        self._algum_sujo = True
        if self._dialogo or isinstance(self, Tk):
            pass

    def _atualizar_tela(self):
        if self._destruido or self._retirada:
            return
        if self._v is None:
            self._garantir_layout()
            self._criar_janela()
        if self._layout_pendente:
            self._fazer_layout()
        if not self._algum_sujo:
            return
        self._algum_sujo = False
        v = self._v
        l, a = v.tamano()
        _desenhar_arvore(v, self, 0, 0, l, a, False)
        if self._menu_barra is not None:
            v.recorte()
            self._menu_barra._desenhar_barra(v, l)
        v.recorte()
        v.actualizar()

    def _desenhar(self, v):
        v.rect(0, 0, self._wl, self._wa, self._cor_op("background", FUNDO))

    _desenhar_parcial = _desenhar

    # ---- eventos do sistema
    def _componente_em(self, x, y):
        w = self
        while True:
            achou = None
            for c in reversed(w._filhos):
                if c._gestor is not None and not c._destruido and not isinstance(c, Wm) and c._dentro_xy(x, y):
                    achou = c
                    break
            if achou is None:
                return w
            w = achou

    def _evento_sistema(self, e):
        t = e.tipo
        if t == sagui.CERRAR:
            f = self._protocolos.get("WM_DELETE_WINDOW")
            if f:
                f()
            else:
                self.destroy()
            return
        if t == sagui.REDIMENSIONAR:
            self._geometria = (e.ancho, e.alto)
            self._layout_pendente = True
            self._sujo = 2
            self._algum_sujo = True
            return
        if t == sagui.TECLA:
            keysym, char, estado = _tecla_evento(e)
            alvo = _foco[0]
            if alvo is None or alvo._destruido or alvo._top() is not self:
                alvo = self
            ev = Event(EventType.KeyPress, alvo)
            ev.keysym, ev.char, ev.keycode = keysym, char, e.tecla
            ev.keysym_num = e.tecla
            alvo._disparar("KeyPress", keysym, ev, estado)
            return
        if t not in (sagui.CLIC, sagui.SOLTAR, sagui.MOVER, sagui.RUEDA):
            return
        x, y = e.x, e.y
        self._mouse_xy = (x, y)
        estado = (ESTADO_SHIFT if e.mods & 1 else 0) | (ESTADO_CONTROL if e.mods & 2 else 0)
        if t == sagui.CLIC:
            num = 3 if e.botones & 2 else (2 if e.botones & 4 else 1)
            if self._menu_barra is not None and y < 22:
                self._menu_barra._clique_barra(x, y)
                return
            alvo = self._componente_em(x, y)
            self._agarrado = alvo
            self._botao = num
            if alvo._foca_no_clique and alvo._ativo():
                _mudar_foco(alvo)
            ev = self._ev_mouse(EventType.ButtonPress, alvo, x, y)
            ev.num = num
            ev._duplo = e.cliques >= 2
            alvo._disparar("ButtonPress", num, ev, estado, e.cliques >= 2)
        elif t == sagui.SOLTAR:
            alvo = self._agarrado or self._componente_em(x, y)
            num = self._botao or 1
            self._agarrado = None
            self._botao = 0
            if alvo._destruido:
                return
            ev = self._ev_mouse(EventType.ButtonRelease, alvo, x, y)
            ev.num = num
            b = {1: ESTADO_B1, 2: ESTADO_B2, 3: ESTADO_B3}.get(num, 0)
            alvo._disparar("ButtonRelease", num, ev, estado | b)
            self._entrar_sair(x, y)
        elif t == sagui.MOVER:
            if self._agarrado is not None and not self._agarrado._destruido and e.botones:
                alvo = self._agarrado
                b = {1: ESTADO_B1, 2: ESTADO_B2, 3: ESTADO_B3}.get(self._botao, 0)
                ev = self._ev_mouse(EventType.Motion, alvo, x, y)
                alvo._disparar("Motion", None, ev, estado | b)
            else:
                self._agarrado = None
                alvo = self._entrar_sair(x, y)
                ev = self._ev_mouse(EventType.Motion, alvo, x, y)
                alvo._disparar("Motion", None, ev, estado)
        elif t == sagui.RUEDA:
            alvo = self._componente_em(x, y)
            ev = self._ev_mouse(EventType.MouseWheel, alvo, x, y)
            ev.delta = -120 * e.cliques
            ev.num = 5 if e.cliques > 0 else 4
            alvo._disparar("MouseWheel", None, ev, estado)

    def _ev_mouse(self, tipo, alvo, x, y):
        ev = Event(tipo, alvo, x - alvo._ax, y - alvo._ay)
        ev.x_root, ev.y_root = x, y
        return ev

    def _entrar_sair(self, x, y):
        alvo = self._componente_em(x, y)
        antes = self._sob_mouse
        if antes is not alvo:
            if antes is not None and not antes._destruido:
                antes._dentro = False
                antes._disparar("Leave", None, self._ev_mouse(EventType.Leave, antes, x, y), 0)
            alvo._dentro = True
            alvo._disparar("Enter", None, self._ev_mouse(EventType.Enter, alvo, x, y), 0)
            self._sob_mouse = alvo
        return alvo

    def destroy(self):
        if self._destruido:
            return
        Misc.destroy(self)
        if self in _modais:
            _modais.remove(self)
        if self._v is not None:
            _por_janela.pop(self._v.id, None)
            self._v.cerrar()
            self._v = None
        if self in _tops:
            _tops.remove(self)
        if isinstance(self, Tk):
            global _raiz
            for t in list(_tops):
                if t._raiz_tk is self:
                    t.destroy()
            if _raiz is self:
                _raiz = None

    def _usa_roda(self):
        return False


def _desenhar_arvore(v, w, x0, y0, x1, y1, forcar):
    """Desenha w (se está sujo ou o pai foi redesenhado) e desce nos filhos,
    cada um recortado pelo interior do pai."""
    if not isinstance(w, Wm):
        if w._gestor is None or w._destruido:
            return
        a0, b0 = max(x0, w._ax), max(y0, w._ay)
        a1, b1 = min(x1, w._ax + w._wl), min(y1, w._ay + w._wa)
        if a0 >= a1 or b0 >= b1:
            w._sujo = 0
            return
    else:
        a0, b0, a1, b1 = x0, y0, x1, y1
    if forcar or w._sujo:
        v.recorte(a0, b0, a1 - a0, b1 - b0)
        try:
            if forcar or w._sujo == 2:
                w._desenhar(v)
            else:
                w._desenhar_parcial(v)
        except Exception as e:
            _relatar(e)
        w._sujo = 0
        forcar = True
    if not w._filhos:
        return
    if isinstance(w, Wm):
        t = w._extra_topo()
        i0, j0, i1, j1 = a0, max(b0, t), a1, b1
    else:
        b = w._borda_interna()
        ix, iy, il, ia = w._ax + b, w._ay + b, w._wl - 2 * b, w._wa - 2 * b
        i0, j0, i1, j1 = max(a0, ix), max(b0, iy), min(a1, ix + il), min(b1, iy + ia)
    if i0 >= i1 or j0 >= j1:
        return
    for c in w._filhos:
        if not isinstance(c, Wm):
            _desenhar_arvore(v, c, i0, j0, i1, j1, forcar)


class Tk(Wm, Misc):
    """A janela principal."""
    _classe = "Tk"
    _padrao = {"background": FUNDO, "borderwidth": 0, "relief": "flat", "menu": None, "width": 0,
               "height": 0, "highlightthickness": 0, "cursor": "", "padx": 0, "pady": 0,
               "takefocus": 0, "class": "Tk", "screen": "", "use": "", "container": 0,
               "colormap": "", "visual": "", "highlightbackground": "", "highlightcolor": ""}

    def __init__(self, screenName=None, baseName=None, className="Tk", useTk=True, sync=False, use=None):
        global _raiz
        self._iniciar_misc(None, None)
        self._iniciar_opcoes(None, {})
        self._iniciar_janela()
        self._raiz_tk = self
        self.tk = self
        if baseName is None:
            import os
            nome = sys.argv[0] if sys.argv and sys.argv[0] else "tk"
            baseName = os.path.splitext(os.path.basename(nome))[0] or "tk"
        self._titulo = baseName
        if _raiz is None:
            _raiz = self

    def report_callback_exception(self, exc, val, tb):
        _relatar(val)

    def _opcoes_mudaram(self, kw):
        if "menu" in kw:
            m = self._op["menu"]
            self._menu_barra = m if m else None
            if m:
                m._barra_de = self
            self._layout_pendente = True
            self._algum_sujo = True
            self._sujo = 2

    def _redesenhar(self, nivel=2):
        if nivel > self._sujo:
            self._sujo = nivel
        self._algum_sujo = True

    def _relayout(self):
        self._layout_pendente = True

    def _pedido(self):
        return Widget._pedido(self)

    def _natural(self):
        return (_int(self._op.get("width", 0)) or 200, _int(self._op.get("height", 0)) or 200)

    def _borda_interna(self):
        return _int(self._op.get("borderwidth", 0))

    def _padding(self):
        return _int(self._op.get("padx", 0)), _int(self._op.get("pady", 0))

    def _area_interna(self):
        return Widget._area_interna(self)

    def _arranjar(self):
        Widget._arranjar(self)

    def _ativo(self):
        return True

    def _aceita_foco(self):
        return False

    def _foco_mudou(self, ganhou):
        pass

    def _dentro_xy(self, x, y):
        return True

    def readprofile(self, baseName, className):
        pass

    def loadtk(self):
        pass

    def wantobjects(self):
        return 1

    def createcommand(self, name, func):
        pass

    def deletecommand(self, name):
        pass

    def splitlist(self, s):
        return tuple(str(s).split())

    def getboolean(self, s):
        return _bool(s)

    def getint(self, s):
        return int(s)

    def getdouble(self, s):
        return float(s)


class Toplevel(Wm, Widget):
    """Outra janela do mesmo programa."""
    _classe = "Toplevel"
    _padrao = dict(Tk._padrao)

    def __init__(self, master=None, cnf={}, **kw):
        if master is None:
            master = _raiz_padrao()
        dialogo = kw.pop("_dialogo", False)
        nome = kw.pop("name", None)
        kw.pop("class_", None)
        self._iniciar_misc(master, nome)
        self._iniciar_opcoes(cnf, kw)
        self._iniciar_janela(dialogo)
        self._raiz_tk = master._top()._raiz_tk if not isinstance(master, Tk) else master
        self._titulo = master._top()._titulo if master is not None else "tk"

    _opcoes_mudaram = Tk._opcoes_mudaram
    _redesenhar = Tk._redesenhar
    _relayout = Tk._relayout
    _natural = Tk._natural
    _borda_interna = Tk._borda_interna
    _padding = Tk._padding
    _ativo = Tk._ativo
    _aceita_foco = Tk._aceita_foco
    _foco_mudou = Tk._foco_mudou
    _dentro_xy = Tk._dentro_xy
    _desenhar = Wm._desenhar
    _colocar = Wm._colocar
    destroy = Wm.destroy
    _extra_topo = Wm._extra_topo

    def _descende_de(self, outro):
        w = self._transiente
        while w is not None:
            if w is outro:
                return True
            w = getattr(w, "_transiente", None)
        return False

    def _top(self):
        return self

    winfo_toplevel = _top


Tk._top = lambda self: self
Tk.winfo_toplevel = Tk._top


# ------------------------------------------------------------ Frame e LabelFrame

class Frame(Widget):
    _classe = "Frame"
    _padrao = {"background": FUNDO, "borderwidth": 0, "relief": "flat", "width": 0, "height": 0,
               "padx": 0, "pady": 0, "highlightthickness": 0, "highlightbackground": "",
               "highlightcolor": "", "cursor": "", "takefocus": 0, "class": "Frame",
               "colormap": "", "container": 0, "visual": ""}

    def _padding(self):
        return _int(self._op.get("padx", 0)), _int(self._op.get("pady", 0))


class LabelFrame(Frame):
    _classe = "Labelframe"
    _padrao = dict(Frame._padrao, borderwidth=2, relief="groove", text="", foreground=TEXTO,
                   labelanchor="nw", font=None, labelwidget=None)

    def _extra_topo(self):
        return LETRA_A // 2 if self._texto() else 0

    def _natural(self):
        l, a = Frame._natural(self)
        return max(l, len(self._texto()) * LETRA_L + 24), a

    def _pedido(self):
        l, a = Widget._pedido(self)
        return max(l, len(self._texto()) * LETRA_L + 24), a

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fundo = self._cor_op("background", FUNDO)
        v.rect(x, y, w, h, fundo)
        t = self._texto()
        m = LETRA_A // 2 if t else 0
        _relevo(v, x, y + m, w, h - m, self._op.get("relief", "groove"), _int(self._op.get("borderwidth", 2)), fundo)
        if t:
            la = self._op.get("labelanchor", "nw")
            tl = len(t) * LETRA_L
            tx = x + 8 if "w" in la or la == "n" and False else (x + w - tl - 8 if "e" in la else x + (w - tl) // 2)
            if la in ("nw", "w", "wn"):
                tx = x + 8
            v.rect(tx - 2, y, tl + 4, LETRA_A, fundo)
            v.texto(tx, y, t, self._cor_op("foreground", TEXTO))


Labelframe = LabelFrame


# ------------------------------------------------------------ Label, Message

class Label(Widget):
    _classe = "Label"
    _padrao = {"text": "", "textvariable": None, "background": FUNDO, "foreground": TEXTO,
               "borderwidth": 2, "relief": "flat", "padx": 1, "pady": 1, "width": 0, "height": 0,
               "anchor": "center", "justify": "center", "wraplength": 0, "font": None, "image": "",
               "compound": "none", "state": "normal", "disabledforeground": FRACO,
               "activebackground": BOTAO_ATIVO, "activeforeground": TEXTO, "highlightthickness": 0,
               "highlightbackground": "", "highlightcolor": "", "cursor": "", "takefocus": 0,
               "underline": -1, "bitmap": ""}

    def _linhas(self):
        return _quebrar(self._texto(), _int(self._op.get("wraplength", 0)))

    def _conteudo(self):
        """(largura, altura) do texto e/ou da imagem."""
        img = self._imagem()
        linhas = self._linhas()
        tl = max([len(l) for l in linhas] or [0]) * LETRA_L
        ta = len(linhas) * LETRA_A if self._texto() or not img else 0
        if not img:
            return tl, ta
        il, ia = img.width(), img.height()
        c = self._op.get("compound", "none")
        if c in ("none", "") or not self._texto():
            return il, ia
        if c in ("left", "right"):
            return il + 4 + tl, max(ia, ta)
        if c in ("top", "bottom"):
            return max(il, tl), ia + 2 + ta
        return max(il, tl), max(ia, ta)

    def _imagem(self):
        img = self._op.get("image")
        if not img:
            return None
        if isinstance(img, str):
            img = _imagens.get(img)
        return img

    def _natural(self):
        b = _int(self._op.get("borderwidth", 0)) + _int(self._op.get("highlightthickness", 0))
        px, py = _int(self._op.get("padx", 0)), _int(self._op.get("pady", 0))
        cl, ca = self._conteudo()
        wo, ho = _int(self._op.get("width", 0)), _int(self._op.get("height", 0))
        if self._imagem() is not None:
            if wo:
                cl = wo
            if ho:
                ca = ho
        else:
            if wo:
                cl = wo * LETRA_L
            if ho:
                ca = ho * LETRA_A
            if not cl:
                cl = 0
            if not ca:
                ca = LETRA_A
        return cl + 2 * (b + px), ca + 2 * (b + py)

    def _cores(self):
        fundo = self._cor_op("background", FUNDO)
        frente = self._cor_op("foreground", TEXTO)
        if not self._ativo():
            frente = self._cor_op("disabledforeground", FRACO)
        return fundo, frente

    def _desenhar(self, v):
        fundo, frente = self._cores()
        self._fundo_e_borda(v, fundo)
        self._desenhar_conteudo(v, frente)

    def _desenhar_conteudo(self, v, frente, dx=0, dy=0):
        b = _int(self._op.get("borderwidth", 0)) + _int(self._op.get("highlightthickness", 0))
        px, py = _int(self._op.get("padx", 0)), _int(self._op.get("pady", 0))
        x, y = self._ax + b + px + dx, self._ay + b + py + dy
        w, h = self._wl - 2 * (b + px), self._wa - 2 * (b + py)
        anchor = self._op.get("anchor", "center")
        img = self._imagem()
        texto = self._texto()
        if img is None:
            _textos(v, self._linhas(), x, y, w, h, anchor, self._op.get("justify", "center"), frente)
            return
        cl, ca = self._conteudo()
        ox, oy = _ancorar(anchor, x, y, w, h, cl, ca)
        c = self._op.get("compound", "none")
        il, ia = img.width(), img.height()
        if c in ("none", "") or not texto:
            img._desenhar_em(v, ox, oy)
            return
        linhas = self._linhas()
        tl = max(len(l) for l in linhas) * LETRA_L
        ta = len(linhas) * LETRA_A
        if c == "left":
            img._desenhar_em(v, ox, oy + (ca - ia) // 2)
            _textos(v, linhas, ox + il + 4, oy, tl, ca, "w", "left", frente)
        elif c == "right":
            _textos(v, linhas, ox, oy, tl, ca, "w", "left", frente)
            img._desenhar_em(v, ox + tl + 4, oy + (ca - ia) // 2)
        elif c == "top":
            img._desenhar_em(v, ox + (cl - il) // 2, oy)
            _textos(v, linhas, ox, oy + ia + 2, cl, ta, "n", "center", frente)
        elif c == "bottom":
            _textos(v, linhas, ox, oy, cl, ta, "n", "center", frente)
            img._desenhar_em(v, ox + (cl - il) // 2, oy + ta + 2)
        else:
            img._desenhar_em(v, ox + (cl - il) // 2, oy + (ca - ia) // 2)
            _textos(v, linhas, ox, oy, cl, ca, "center", "center", frente)


class Message(Label):
    _classe = "Message"
    _padrao = dict(Label._padrao, width=0, aspect=150, justify="left", anchor="center")

    def _linhas(self):
        largura = _int(self._op.get("width", 0))
        if not largura:
            t = self._texto()
            largura = max(80, int((len(t) * LETRA_L * LETRA_A * self._op.get("aspect", 150) / 100) ** 0.5))
        return _quebrar(self._texto(), largura)

    def _natural(self):
        linhas = self._linhas()
        return max([len(l) for l in linhas] or [0]) * LETRA_L + 8, len(linhas) * LETRA_A + 8


# ------------------------------------------------------------ Button

class Button(Label):
    _classe = "Button"
    _foca_no_clique = False
    _padrao = dict(Label._padrao, borderwidth=2, relief="raised", padx=6, pady=2, background=BOTAO,
                   activebackground=BOTAO_ATIVO, command=None, default="disabled", overrelief="",
                   repeatdelay=0, repeatinterval=0, takefocus="")

    def _iniciar(self):
        self._apertado = False

    def _foca_tab(self):
        return True

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fundo, frente = self._cores()
        if self._apertado:
            fundo = APERTADO
        elif self._dentro and self._ativo():
            fundo = self._cor_op("activebackground", BOTAO_ATIVO)
        v.rect(x, y, w, h, fundo)
        rel = self._op.get("relief", "raised")
        if self._apertado:
            rel = "sunken"
        if rel in ("raised", "sunken") and _int(self._op.get("borderwidth", 2)) >= 1:
            # o botão do SaguiOS: moldura escura e relevo de um pixel (gui.c)
            v.rect(x, y, w, h, ESCURO, relleno=False)
            _relevo(v, x + 1, y + 1, w - 2, h - 2, rel, 1, fundo)
        else:
            _relevo(v, x, y, w, h, rel, _int(self._op.get("borderwidth", 2)), fundo)
        d = 1 if self._apertado else 0
        self._desenhar_conteudo(v, frente, d, d)
        if _foco[0] is self:
            v.rect(x + 3, y + 3, w - 6, h - 6, DESTAQUE, relleno=False)

    def _tratar(self, tipo, ev):
        if not self._ativo():
            return None
        if tipo == "ButtonPress" and ev.num == 1:
            self._apertado = True
            self._redesenhar()
        elif tipo == "ButtonRelease" and ev.num == 1 and self._apertado:
            self._apertado = False
            self._redesenhar()
            if 0 <= ev.x < self._wl and 0 <= ev.y < self._wa:
                self.invoke()
        elif tipo == "Motion" and self._apertado:
            pass
        elif tipo in ("Enter", "Leave"):
            self._redesenhar()
        elif tipo == "KeyPress" and ev.keysym in ("space", "Return"):
            self.invoke()
            return "break"
        return None

    def invoke(self):
        if not self._ativo():
            return None
        c = self._op.get("command")
        if c:
            return c()
        return None

    def flash(self):
        pass


# ------------------------------------------------------------ Checkbutton e Radiobutton

class Checkbutton(Label):
    _classe = "Checkbutton"
    _padrao = dict(Label._padrao, anchor="center", justify="left", padx=1, pady=1, borderwidth=2,
                   command=None, variable=None, onvalue=1, offvalue=0, indicatoron=1,
                   selectcolor=BRANCO, offrelief="raised", overrelief="", selectimage="",
                   tristateimage="", tristatevalue="", takefocus="")
    _CAIXA = 13

    def _iniciar(self):
        if self._op.get("variable") is None:
            v = IntVar(value=self._op["offvalue"]) if isinstance(self._op["onvalue"], int) else StringVar(value=self._op["offvalue"])
            self._op["variable"] = v
            v._ouvintes.append(self)

    def _foca_tab(self):
        return True

    def _var(self):
        v = self._op.get("variable")
        if isinstance(v, str):
            v = _vars_por_nome.get(v)
        return v

    def _marcado(self):
        v = self._var()
        if v is None:
            return False
        return _igual(v._valor, self._op["onvalue"])

    def _natural(self):
        l, a = Label._natural(self)
        return l + self._CAIXA + 6, max(a, self._CAIXA + 4)

    def _desenhar(self, v):
        fundo, frente = self._cores()
        if self._dentro and self._ativo():
            fundo = self._cor_op("activebackground", fundo) if self._op.get("activebackground") != BOTAO_ATIVO else fundo
        self._fundo_e_borda(v, fundo)
        c = self._CAIXA
        cl, ca = self._conteudo()
        b = _int(self._op.get("borderwidth", 0))
        px = _int(self._op.get("padx", 0))
        tot = c + 6 + cl
        x0, y0 = _ancorar(self._op.get("anchor", "center"), self._ax + b + px, self._ay + b,
                          self._wl - 2 * (b + px), self._wa - 2 * b, tot, max(ca, c))
        cy = y0 + (max(ca, c) - c) // 2
        self._indicador(v, x0, cy, frente)
        b0 = _int(self._op.get("borderwidth", 0)) + _int(self._op.get("highlightthickness", 0)) + _int(self._op.get("padx", 0))
        # o texto à direita do indicador
        sx = self._ax
        sw = self._wl
        self._ax = x0 + c + 6 - b0
        self._wl = cl + 2 * b0
        try:
            Label._desenhar_conteudo(self, v, frente)
        finally:
            self._ax, self._wl = sx, sw
        if _foco[0] is self:
            v.rect(x0 + c + 4, y0 - 1, cl + 4, max(ca, c) + 2, DESTAQUE, relleno=False)

    def _indicador(self, v, x, y, frente):
        c = self._CAIXA
        v.rect(x, y, c, c, self._cor_op("selectcolor", BRANCO) if self._ativo() else BOTAO)
        _relevo(v, x, y, c, c, "sunken", 1)
        if self._marcado():
            cor = DESTAQUE if self._ativo() else FRACO
            pts = ((3, 6), (5, 8), (9, 3))
            for k in range(2):
                v.linea(x + pts[0][0], y + pts[0][1] + k, x + pts[1][0], y + pts[1][1] + k, cor)
                v.linea(x + pts[1][0], y + pts[1][1] + k, x + pts[2][0], y + pts[2][1] + k, cor)

    def _tratar(self, tipo, ev):
        if not self._ativo():
            return None
        if tipo == "ButtonRelease" and ev.num == 1 and 0 <= ev.x < self._wl and 0 <= ev.y < self._wa:
            self.invoke()
        elif tipo in ("Enter", "Leave"):
            self._redesenhar()
        elif tipo == "KeyPress" and ev.keysym == "space":
            self.invoke()
            return "break"
        return None

    def select(self):
        v = self._var()
        if v is not None:
            v.set(self._op["onvalue"])

    def deselect(self):
        v = self._var()
        if v is not None:
            v.set(self._op["offvalue"])

    def toggle(self):
        if self._marcado():
            self.deselect()
        else:
            self.select()

    def invoke(self):
        if not self._ativo():
            return None
        self.toggle()
        c = self._op.get("command")
        if c:
            return c()
        return None

    def flash(self):
        pass

    def _variavel_mudou(self, var):
        self._redesenhar()


class Radiobutton(Checkbutton):
    _classe = "Radiobutton"
    _padrao = dict(Checkbutton._padrao, value="")
    del _padrao["onvalue"]
    del _padrao["offvalue"]

    def _iniciar(self):
        if self._op.get("variable") is None:
            v = _raiz_padrao()._var_radio if hasattr(_raiz_padrao(), "_var_radio") else None
            if v is None:
                v = StringVar(value="")
                _raiz_padrao()._var_radio = v
            self._op["variable"] = v
            v._ouvintes.append(self)

    def _marcado(self):
        v = self._var()
        return v is not None and _igual(v._valor, self._op.get("value"))

    def _indicador(self, v, x, y, frente):
        c = self._CAIXA
        v.elipse(x, y, c, c, self._cor_op("selectcolor", BRANCO) if self._ativo() else BOTAO)
        v.elipse(x, y, c, c, BORDA, relleno=False)
        if self._marcado():
            v.elipse(x + 3, y + 3, c - 6, c - 6, DESTAQUE if self._ativo() else FRACO)

    def select(self):
        v = self._var()
        if v is not None:
            v.set(self._op.get("value"))

    def deselect(self):
        v = self._var()
        if v is not None and self._marcado():
            v.set("")

    def invoke(self):
        if not self._ativo():
            return None
        self.select()
        c = self._op.get("command")
        if c:
            return c()
        return None


_vars_por_nome = {}


# ------------------------------------------------------------ Entry

class Entry(Widget):
    _classe = "Entry"
    _foca_no_clique = True
    _padrao = {"background": BRANCO, "foreground": TEXTO, "borderwidth": 2, "relief": "sunken",
               "width": 20, "textvariable": None, "show": "", "state": "normal", "justify": "left",
               "font": None, "insertbackground": TEXTO, "selectbackground": SELECAO,
               "selectforeground": BRANCO, "disabledbackground": BOTAO, "disabledforeground": FRACO,
               "readonlybackground": FUNDO, "highlightthickness": 0, "highlightbackground": "",
               "highlightcolor": "", "cursor": "", "takefocus": "", "exportselection": 1,
               "validate": "none", "validatecommand": None, "invalidcommand": None,
               "xscrollcommand": None, "insertwidth": 2, "insertontime": 600, "insertofftime": 300,
               "insertborderwidth": 0, "selectborderwidth": 0}
    _aliases = dict(Misc._aliases, vcmd="validatecommand", invcmd="invalidcommand")
    _PAD = 4

    def _iniciar(self):
        self._valor = ""
        self._cursor = 0
        self._sel = None          # (início, fim) ou None
        self._ancora = 0
        self._desloc = 0          # primeiro caractere visível
        tv = self._op.get("textvariable")
        if isinstance(tv, Variable):
            self._valor = str(tv._valor)

    def _foca_tab(self):
        return True

    def _natural(self):
        b = _int(self._op.get("borderwidth", 2))
        return _int(self._op.get("width", 20)) * LETRA_L + 2 * (b + self._PAD), LETRA_A + 2 * (b + 2) + 2

    # ---- texto
    def _definir(self, t, avisar=True):
        if t == self._valor:
            return
        self._valor = t
        tv = self._op.get("textvariable")
        if avisar and isinstance(tv, Variable):
            tv._valor = t
            tv._avisar("write")
        self._redesenhar()

    def _variavel_mudou(self, var):
        if var is self._op.get("textvariable"):
            self._valor = str(var._valor)
            self._cursor = min(self._cursor, len(self._valor))
            self._sel = None
            self._redesenhar()

    def get(self):
        return self._valor

    def index(self, idx):
        if isinstance(idx, int):
            return max(0, min(idx, len(self._valor)))
        s = str(idx)
        if s == "end":
            return len(self._valor)
        if s == "insert":
            return self._cursor
        if s == "anchor":
            return self._ancora
        if s in ("sel.first", "sel.last"):
            if not self._sel:
                raise TclError("selection isn't in widget %s" % self._w)
            return self._sel[0] if s == "sel.first" else self._sel[1]
        if s.startswith("@"):
            return self._indice_x(int(s[1:]) + self._ax)
        try:
            return max(0, min(int(s), len(self._valor)))
        except ValueError:
            raise TclError('bad entry index "%s"' % idx)

    def insert(self, index, string):
        i = self.index(index)
        s = str(string)
        self._definir(self._valor[:i] + s + self._valor[i:])
        if self._cursor >= i:
            self._cursor += len(s)
        self._mostrar_cursor()

    def delete(self, first, last=None):
        a = self.index(first)
        b = a + 1 if last is None else self.index(last)
        if b <= a:
            return
        self._definir(self._valor[:a] + self._valor[b:])
        if self._cursor > b:
            self._cursor -= b - a
        elif self._cursor > a:
            self._cursor = a
        self._sel = None
        self._mostrar_cursor()

    def icursor(self, index):
        self._cursor = self.index(index)
        self._mostrar_cursor()
        self._redesenhar()

    def select_range(self, start, end):
        a, b = self.index(start), self.index(end)
        self._sel = (a, b) if a < b else None
        self._redesenhar()

    selection_range = select_range

    def select_clear(self):
        self._sel = None
        self._redesenhar()

    selection_clear = select_clear

    def select_present(self):
        return self._sel is not None

    selection_present = select_present

    def select_adjust(self, index):
        i = self.index(index)
        self.select_range(min(self._ancora, i), max(self._ancora, i))

    selection_adjust = select_adjust

    def select_from(self, index):
        self._ancora = self.index(index)

    selection_from = select_from

    def select_to(self, index):
        self.select_adjust(index)

    selection_to = select_to

    def xview(self, *args):
        if not args:
            n = max(1, len(self._valor))
            return (self._desloc / n, min(1.0, (self._desloc + self._cabem()) / n))
        if args[0] == "moveto":
            self._desloc = int(float(args[1]) * len(self._valor))
        elif args[0] == "scroll":
            self._desloc += int(args[1]) * (1 if args[2] == "units" else self._cabem())
        else:
            self._desloc = self.index(args[0])
        self._desloc = max(0, min(self._desloc, len(self._valor)))
        self._redesenhar()

    def xview_moveto(self, fraction):
        self.xview("moveto", fraction)

    def xview_scroll(self, number, what):
        self.xview("scroll", number, what)

    # ---- desenho
    def _cabem(self):
        b = _int(self._op.get("borderwidth", 2))
        return max(1, (self._wl - 2 * (b + self._PAD)) // LETRA_L)

    def _mostrar_cursor(self):
        cabem = self._cabem() if self._wl else 20
        if self._cursor < self._desloc:
            self._desloc = self._cursor
        elif self._cursor > self._desloc + cabem - 1:
            self._desloc = self._cursor - cabem + 1
        self._desloc = max(0, self._desloc)

    def _visivel(self):
        s = self._valor
        if self._op.get("show"):
            s = str(self._op["show"])[0] * len(s)
        return s

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        estado = self._op.get("state", "normal")
        if estado == "disabled":
            fundo = self._cor_op("disabledbackground", BOTAO)
            frente = self._cor_op("disabledforeground", FRACO)
        elif estado == "readonly":
            fundo = self._cor_op("readonlybackground", FUNDO)
            frente = self._cor_op("foreground", TEXTO)
        else:
            fundo = self._cor_op("background", BRANCO)
            frente = self._cor_op("foreground", TEXTO)
        v.rect(x, y, w, h, fundo)
        b = _int(self._op.get("borderwidth", 2))
        focado = _foco[0] is self
        _moldura_campo(v, x, y, w, h, self._op.get("relief", "sunken"), b, focado and estado != "disabled")
        self._mostrar_cursor()
        s = self._visivel()
        cabem = self._cabem()
        tx = x + b + self._PAD
        ty = y + (h - LETRA_A) // 2
        parte = s[self._desloc:self._desloc + cabem]
        just = self._op.get("justify", "left")
        if just != "left" and len(s) <= cabem:
            sobra = (cabem - len(s)) * LETRA_L
            tx += sobra // 2 if just == "center" else sobra
        if self._sel and focado:
            a, z = max(self._sel[0], self._desloc), min(self._sel[1], self._desloc + cabem)
            if a < z:
                v.texto(tx, ty, parte[:a - self._desloc], frente)
                v.texto(tx + (a - self._desloc) * LETRA_L, ty, parte[a - self._desloc:z - self._desloc],
                        self._cor_op("selectforeground", BRANCO), self._cor_op("selectbackground", SELECAO))
                v.texto(tx + (z - self._desloc) * LETRA_L, ty, parte[z - self._desloc:], frente)
            else:
                v.texto(tx, ty, parte, frente)
        else:
            v.texto(tx, ty, parte, frente)
        if focado and estado == "normal":
            cx = tx + (self._cursor - self._desloc) * LETRA_L
            v.rect(cx - 1, ty, 2, LETRA_A, self._cor_op("insertbackground", TEXTO))

    def _indice_x(self, xa):
        b = _int(self._op.get("borderwidth", 2))
        col = (xa - self._ax - b - self._PAD + LETRA_L // 2) // LETRA_L
        return max(0, min(len(self._valor), self._desloc + col))

    # ---- edição
    def _validar(self, novo, acao, indice, texto):
        modo = self._op.get("validate", "none")
        vc = self._op.get("validatecommand")
        if modo in ("none", None) or vc is None or modo not in ("key", "all"):
            return True
        if isinstance(vc, (tuple, list)):
            f = vc[0]
            subs = {"%P": novo, "%s": self._valor, "%d": str(acao), "%i": str(indice), "%S": texto,
                    "%v": modo, "%V": "key", "%W": self._w}
            args = [subs.get(a, a) for a in vc[1:]]
            ok = f(*args)
        else:
            ok = vc()
        if not ok and self._op.get("invalidcommand"):
            ic = self._op["invalidcommand"]
            (ic[0] if isinstance(ic, (tuple, list)) else ic)()
        return bool(ok)

    def _apagar_sel(self):
        if self._sel:
            a, b = self._sel
            novo = self._valor[:a] + self._valor[b:]
            if not self._validar(novo, 0, a, self._valor[a:b]):
                return True
            self._definir(novo)
            self._cursor = a
            self._sel = None
            return True
        return False

    def _digitar(self, s):
        if self._sel:
            a, b = self._sel
        else:
            a = b = self._cursor
        novo = self._valor[:a] + s + self._valor[b:]
        if not self._validar(novo, 1, a, s):
            return
        self._definir(novo)
        self._cursor = a + len(s)
        self._sel = None

    def _tratar(self, tipo, ev):
        estado = self._op.get("state", "normal")
        if estado == "disabled":
            return None
        if tipo == "ButtonPress" and ev.num == 1:
            i = self._indice_x(ev.x + self._ax)
            if ev.state & ESTADO_SHIFT:
                self.select_adjust(i)
            elif getattr(ev, "_duplo", False):   # duplo clique: a palavra
                a = b = min(i, len(self._valor))
                while a > 0 and _letra(self._valor[a - 1]):
                    a -= 1
                while b < len(self._valor) and _letra(self._valor[b]):
                    b += 1
                self._sel = (a, b) if a < b else None
                self._cursor = b
                self._ancora = a
            else:
                self._cursor = self._ancora = i
                self._sel = None
            self._redesenhar()
        elif tipo == "Motion" and ev.state & ESTADO_B1:
            i = self._indice_x(ev.x + self._ax)
            self._cursor = i
            a, b = min(self._ancora, i), max(self._ancora, i)
            self._sel = (a, b) if a < b else None
            self._redesenhar()
        elif tipo == "KeyPress":
            return self._tecla(ev, estado == "readonly")
        return None

    def _tecla(self, ev, so_ler):
        k = ev.keysym
        ctrl = ev.state & ESTADO_CONTROL
        shift = ev.state & ESTADO_SHIFT
        n = len(self._valor)
        antes = self._cursor
        mover = None
        if k == "Left":
            mover = max(0, self._cursor - 1) if not (self._sel and not shift) else self._sel[0]
        elif k == "Right":
            mover = min(n, self._cursor + 1) if not (self._sel and not shift) else self._sel[1]
        elif k == "Home":
            mover = 0
        elif k == "End":
            mover = n
        if mover is not None:
            if shift:
                if not self._sel:
                    self._ancora = antes
                a, b = min(self._ancora, mover), max(self._ancora, mover)
                self._sel = (a, b) if a < b else None
            else:
                self._sel = None
            self._cursor = mover
            self._redesenhar()
            return None
        if ctrl and k in ("a", "slash"):
            self._sel = (0, n) if n else None
            self._ancora = 0
            self._cursor = n
            self._redesenhar()
            return "break"
        if ctrl and k == "c":
            if self._sel and not self._op.get("show"):
                sagui.portapapeles(self._valor[self._sel[0]:self._sel[1]])
            return "break"
        if so_ler:
            return None
        if ctrl and k == "x":
            if self._sel and not self._op.get("show"):
                sagui.portapapeles(self._valor[self._sel[0]:self._sel[1]])
                self._apagar_sel()
                self._redesenhar()
            return "break"
        if ctrl and k == "v":
            t = sagui.portapapeles().split("\n")[0]
            if t:
                self._digitar(t)
                self._redesenhar()
            return "break"
        if k == "BackSpace":
            if not self._apagar_sel() and self._cursor > 0:
                novo = self._valor[:self._cursor - 1] + self._valor[self._cursor:]
                if self._validar(novo, 0, self._cursor - 1, self._valor[self._cursor - 1]):
                    self._definir(novo)
                    self._cursor -= 1
            self._redesenhar()
            return "break"
        if k == "Delete":
            if not self._apagar_sel() and self._cursor < n:
                novo = self._valor[:self._cursor] + self._valor[self._cursor + 1:]
                if self._validar(novo, 0, self._cursor, self._valor[self._cursor]):
                    self._definir(novo)
            self._redesenhar()
            return "break"
        c = ev.char
        if c and not ctrl and len(c) == 1 and c >= " " and c != "\x7f":
            self._digitar(c)
            self._redesenhar()
            return None
        return None

    def _foco_mudou(self, ganhou):
        if not ganhou:
            pass
        self._redesenhar()


# ------------------------------------------------------------ Spinbox

class Spinbox(Entry):
    _classe = "Spinbox"
    _padrao = dict(Entry._padrao, to=0.0, increment=1.0, values=(), wrap=0, command=None,
                   format="", buttonbackground=BOTAO, buttonuprelief="raised", buttondownrelief="raised",
                   repeatdelay=400, repeatinterval=100)
    _padrao["from"] = 0.0
    _BOTAO = 16

    def _iniciar(self):
        Entry._iniciar(self)
        if not self._valor:
            vals = self._valores()
            if vals:
                self._valor = str(vals[0])
            else:
                self._valor = self._formatar(float(self._op.get("from", 0)))
            tv = self._op.get("textvariable")
            if isinstance(tv, Variable) and str(tv._valor) == "":
                tv._valor = self._valor

    def _valores(self):
        v = self._op.get("values") or ()
        if isinstance(v, str):
            v = v.split()
        return list(v)

    def _formatar(self, x):
        f = self._op.get("format")
        if f:
            try:
                return f % x
            except Exception:
                pass
        inc = float(self._op.get("increment", 1))
        if float(self._op.get("from", 0)) == int(float(self._op.get("from", 0))) and inc == int(inc):
            return str(int(round(x)))
        return str(x)

    def _natural(self):
        l, a = Entry._natural(self)
        return l + self._BOTAO, a

    def _cabem(self):
        b = _int(self._op.get("borderwidth", 2))
        return max(1, (self._wl - self._BOTAO - 2 * (b + self._PAD)) // LETRA_L)

    def _desenhar(self, v):
        Entry._desenhar(self, v)
        x = self._ax + self._wl - self._BOTAO - 2
        y = self._ay + 2
        h = (self._wa - 4) // 2
        for k, (yy, cima) in enumerate(((y, True), (y + h, False))):
            v.rect(x, yy, self._BOTAO, h, BOTAO)
            _relevo(v, x, yy, self._BOTAO, h, "raised", 1)
            cx, cy = x + self._BOTAO // 2, yy + h // 2
            if cima:
                v.poligono([(cx - 4, cy + 2), (cx + 4, cy + 2), (cx, cy - 2)], TEXTO)
            else:
                v.poligono([(cx - 4, cy - 2), (cx + 4, cy - 2), (cx, cy + 2)], TEXTO)

    def _passo(self, d):
        vals = self._valores()
        if vals:
            try:
                i = vals.index(self._valor)
            except ValueError:
                i = -1 if d > 0 else 0
            i += d
            if i < 0:
                i = len(vals) - 1 if self._op.get("wrap") else 0
            elif i >= len(vals):
                i = 0 if self._op.get("wrap") else len(vals) - 1
            novo = str(vals[i])
        else:
            de, ate = float(self._op.get("from", 0)), float(self._op.get("to", 0))
            inc = float(self._op.get("increment", 1))
            try:
                x = float(self._valor)
            except ValueError:
                x = de
            x += d * inc
            if x > ate:
                x = de if self._op.get("wrap") else ate
            if x < de:
                x = ate if self._op.get("wrap") else de
            novo = self._formatar(x)
        self._definir(novo)
        self._cursor = len(novo)
        self._sel = None
        c = self._op.get("command")
        if c:
            c()

    def invoke(self, element):
        self._passo(1 if element == "buttonup" else -1)

    def _tratar(self, tipo, ev):
        if self._op.get("state", "normal") == "disabled":
            return None
        if tipo == "ButtonPress" and ev.num == 1 and ev.x >= self._wl - self._BOTAO - 2:
            self._passo(1 if ev.y < self._wa // 2 else -1)
            return None
        if tipo == "KeyPress" and ev.keysym in ("Up", "Down"):
            self._passo(1 if ev.keysym == "Up" else -1)
            return "break"
        if tipo == "MouseWheel":
            self._passo(1 if ev.delta > 0 else -1)
            return "break"
        return Entry._tratar(self, tipo, ev)

    def _usa_roda(self):
        return True

    def identify(self, x, y):
        if x >= self._wl - self._BOTAO - 2:
            return "buttonup" if y < self._wa // 2 else "buttondown"
        return "entry"


# ------------------------------------------------------------ Scale

class Scale(Widget):
    _classe = "Scale"
    _padrao = {"from": 0.0, "to": 100.0, "resolution": 1.0, "orient": "vertical", "length": 100,
               "sliderlength": 30, "width": 15, "showvalue": 1, "label": "", "variable": None,
               "command": None, "background": FUNDO, "foreground": TEXTO, "troughcolor": TRILHO,
               "borderwidth": 1, "relief": "flat", "state": "normal", "digits": 0, "tickinterval": 0,
               "font": None, "highlightthickness": 0, "highlightbackground": "", "highlightcolor": "",
               "activebackground": BOTAO_ATIVO, "sliderrelief": "raised", "bigincrement": 0,
               "repeatdelay": 300, "repeatinterval": 100, "cursor": "", "takefocus": ""}

    def _nome_opcao(self, k):
        n = Widget._nome_opcao(self, k)
        return "from" if n == "from_" else n

    def _iniciar(self):
        var = self._op.get("variable")
        if isinstance(var, Variable):
            try:
                self._valor = float(var._valor)
            except (TypeError, ValueError):
                self._valor = float(self._op["from"])
        else:
            self._valor = float(self._op["from"])
        self._valor = self._ajustar(self._valor)
        self._arrastando = None

    def _foca_tab(self):
        return True

    def _horizontal(self):
        return str(self._op.get("orient", "vertical")).startswith("h")

    def _ajustar(self, x):
        de, ate = float(self._op["from"]), float(self._op["to"])
        lo, hi = min(de, ate), max(de, ate)
        x = max(lo, min(hi, x))
        r = float(self._op.get("resolution", 1))
        if r > 0:
            x = round((x - de) / r) * r + de
        return x

    def _texto_valor(self, x=None):
        x = self._valor if x is None else x
        r = float(self._op.get("resolution", 1))
        d = _int(self._op.get("digits", 0))
        if d:
            return ("%%.%df" % max(0, d - len(str(int(abs(x)))))) % x
        if r >= 1 and r == int(r):
            return str(int(round(x)))
        casas = 0
        while r < 1 and casas < 6:
            r *= 10
            casas += 1
        return ("%%.%df" % casas) % x

    def _medidas(self):
        """(texto do valor, altura do rótulo, espessura do trilho)."""
        tv = 0
        if _bool(self._op.get("showvalue", 1)):
            tv = max(len(self._texto_valor(float(self._op["from"]))), len(self._texto_valor(float(self._op["to"])))) * LETRA_L
        lab = LETRA_A if self._op.get("label") else 0
        return tv, lab, _int(self._op.get("width", 15))

    def _natural(self):
        tv, lab, esp = self._medidas()
        comp = _int(self._op.get("length", 100))
        if self._horizontal():
            return comp + 4, lab + (LETRA_A + 2 if tv else 0) + esp + 6
        return (tv + 6 if tv else 0) + esp + 6 + (len(str(self._op.get("label", ""))) * LETRA_L if lab else 0), comp + 4 + lab

    def _trilho(self):
        """(x, y, comprimento, espessura) do trilho em coordenadas da janela."""
        tv, lab, esp = self._medidas()
        if self._horizontal():
            return self._ax + 2, self._ay + lab + (LETRA_A + 2 if tv else 0) + 3, self._wl - 4, esp
        return self._ax + (tv + 6 if tv else 0) + 3, self._ay + lab + 2, self._wa - lab - 4, esp

    def _pos(self, x):
        tx, ty, comp, esp = self._trilho()
        sl = _int(self._op.get("sliderlength", 30))
        de, ate = float(self._op["from"]), float(self._op["to"])
        f = 0 if ate == de else (x - de) / (ate - de)
        return int(f * max(0, comp - sl))

    def _valor_em(self, p):
        tx, ty, comp, esp = self._trilho()
        sl = _int(self._op.get("sliderlength", 30))
        de, ate = float(self._op["from"]), float(self._op["to"])
        f = 0 if comp <= sl else max(0.0, min(1.0, p / (comp - sl)))
        return self._ajustar(de + f * (ate - de))

    def _desenhar(self, v):
        fundo = self._cor_op("background", FUNDO)
        frente = self._cor_op("foreground", TEXTO) if self._ativo() else FRACO
        self._fundo_e_borda(v, fundo)
        tv, lab, esp = self._medidas()
        if lab:
            v.texto(self._ax + 2, self._ay, str(self._op["label"]), frente)
        tx, ty, comp, _ = self._trilho()
        sl = _int(self._op.get("sliderlength", 30))
        p = self._pos(self._valor)
        tc = self._cor_op("troughcolor", TRILHO)
        if self._horizontal():
            v.rect(tx, ty, comp, esp, tc)
            _relevo(v, tx, ty, comp, esp, "sunken", 1)
            v.rect(tx + p + 1, ty + 1, sl - 2, esp - 2, APERTADO if self._arrastando is not None else BOTAO)
            _relevo(v, tx + p + 1, ty + 1, sl - 2, esp - 2, "raised", 1)
            v.rect(tx + p + sl // 2 - 1, ty + 3, 2, esp - 6, BORDA)
            if tv:
                t = self._texto_valor()
                v.texto(max(self._ax, min(tx + p + (sl - len(t) * LETRA_L) // 2, self._ax + self._wl - len(t) * LETRA_L)),
                        ty - LETRA_A - 2, t, frente)
        else:
            v.rect(tx, ty, esp, comp, tc)
            _relevo(v, tx, ty, esp, comp, "sunken", 1)
            v.rect(tx + 1, ty + p + 1, esp - 2, sl - 2, APERTADO if self._arrastando is not None else BOTAO)
            _relevo(v, tx + 1, ty + p + 1, esp - 2, sl - 2, "raised", 1)
            v.rect(tx + 3, ty + p + sl // 2 - 1, esp - 6, 2, BORDA)
            if tv:
                t = self._texto_valor()
                v.texto(tx - 4 - len(t) * LETRA_L, ty + p + (sl - LETRA_A) // 2, t, frente)
        if _foco[0] is self:
            v.rect(self._ax, self._ay, self._wl, self._wa, DESTAQUE, relleno=False)

    def get(self):
        r = float(self._op.get("resolution", 1))
        if r >= 1 and r == int(r) and float(self._op["from"]) == int(float(self._op["from"])):
            return int(round(self._valor))
        return self._valor

    def set(self, value):
        self._mudar(float(value), avisar_cmd=False)

    def _mudar(self, x, avisar_cmd=True):
        x = self._ajustar(x)
        if x == self._valor:
            return
        self._valor = x
        var = self._op.get("variable")
        if isinstance(var, Variable):
            var._valor = self.get() if isinstance(var, (IntVar, BooleanVar)) else x
            if isinstance(var, IntVar):
                var._valor = int(round(x))
            var._avisar("write")
        self._redesenhar()
        c = self._op.get("command")
        if c and avisar_cmd:
            c(self._texto_valor())

    def _variavel_mudou(self, var):
        try:
            x = self._ajustar(float(var._valor))
        except (TypeError, ValueError):
            return
        if x != self._valor:
            self._valor = x
            self._redesenhar()

    def coords(self, value=None):
        tx, ty, comp, esp = self._trilho()
        p = self._pos(self._valor if value is None else float(value)) + _int(self._op.get("sliderlength", 30)) // 2
        if self._horizontal():
            return (tx + p - self._ax, ty + esp // 2 - self._ay)
        return (tx + esp // 2 - self._ax, ty + p - self._ay)

    def identify(self, x, y):
        return "slider"

    def _tratar(self, tipo, ev):
        if not self._ativo():
            return None
        tx, ty, comp, esp = self._trilho()
        sl = _int(self._op.get("sliderlength", 30))
        ax, ay = ev.x + self._ax, ev.y + self._ay
        d = (ax - tx) if self._horizontal() else (ay - ty)
        if tipo == "ButtonPress" and ev.num == 1:
            p = self._pos(self._valor)
            if p <= d < p + sl:
                self._arrastando = d - p
                self._redesenhar()
            else:
                r = float(self._op.get("resolution", 1)) or 1
                big = float(self._op.get("bigincrement", 0)) or max(r, abs(float(self._op["to"]) - float(self._op["from"])) / 10)
                sentido = 1 if d >= p + sl else -1
                if float(self._op["to"]) < float(self._op["from"]):
                    sentido = -sentido
                self._mudar(self._valor + sentido * big)
        elif tipo == "Motion" and self._arrastando is not None and ev.state & ESTADO_B1:
            self._mudar(self._valor_em(d - self._arrastando))
        elif tipo == "ButtonRelease" and self._arrastando is not None:
            self._arrastando = None
            self._redesenhar()
        elif tipo == "KeyPress" and ev.keysym in ("Left", "Right", "Up", "Down"):
            r = float(self._op.get("resolution", 1)) or 1
            self._mudar(self._valor + (r if ev.keysym in ("Right", "Down") else -r))
            return "break"
        elif tipo == "MouseWheel":
            r = float(self._op.get("resolution", 1)) or 1
            self._mudar(self._valor + (-r if ev.delta > 0 else r))
            return "break"
        return None

    def _usa_roda(self):
        return True


# ------------------------------------------------------------ Scrollbar

class Scrollbar(Widget):
    _classe = "Scrollbar"
    _padrao = {"orient": "vertical", "command": None, "width": 16, "background": BOTAO,
               "troughcolor": TRILHO, "borderwidth": 1, "relief": "sunken", "activebackground": BOTAO_ATIVO,
               "elementborderwidth": 1, "highlightthickness": 0, "highlightbackground": "",
               "highlightcolor": "", "jump": 0, "repeatdelay": 300, "repeatinterval": 100,
               "takefocus": 0, "cursor": "", "activerelief": "raised"}
    _SETA = 16

    def _iniciar(self):
        self._ini, self._fim = 0.0, 1.0
        self._arrasto = None

    def _horizontal(self):
        return str(self._op.get("orient", "vertical")).startswith("h")

    def _natural(self):
        w = _int(self._op.get("width", 16))
        return (60, w) if self._horizontal() else (w, 60)

    def set(self, first, last):
        a, b = float(first), float(last)
        if (a, b) != (self._ini, self._fim):
            self._ini, self._fim = a, b
            self._redesenhar()

    def get(self):
        return (self._ini, self._fim)

    def _geom(self):
        """(comprimento útil, início do trilho, posição e tamanho do polegar)."""
        comp = (self._wl if self._horizontal() else self._wa) - 2 * self._SETA
        p0 = int(self._ini * comp)
        tam = max(10, int((self._fim - self._ini) * comp))
        if p0 + tam > comp:
            p0 = max(0, comp - tam)
        return comp, p0, tam

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        v.rect(x, y, w, h, self._cor_op("troughcolor", TRILHO))
        comp, p0, tam = self._geom()
        s = self._SETA
        hor = self._horizontal()
        for k in range(2):
            if hor:
                bx, by, bw, bh = (x, y, s, h) if k == 0 else (x + w - s, y, s, h)
            else:
                bx, by, bw, bh = (x, y, w, s) if k == 0 else (x, y + h - s, w, s)
            v.rect(bx, by, bw, bh, BOTAO)
            _relevo(v, bx, by, bw, bh, "raised", 1)
            cx, cy = bx + bw // 2, by + bh // 2
            if hor:
                pts = [(cx + 2, cy - 4), (cx + 2, cy + 4), (cx - 2, cy)] if k == 0 else [(cx - 2, cy - 4), (cx - 2, cy + 4), (cx + 2, cy)]
            else:
                pts = [(cx - 4, cy + 2), (cx + 4, cy + 2), (cx, cy - 2)] if k == 0 else [(cx - 4, cy - 2), (cx + 4, cy - 2), (cx, cy + 2)]
            v.poligono(pts, TEXTO if self._fim - self._ini < 1 else FRACO)
        if self._fim - self._ini < 1:
            if hor:
                v.rect(x + s + p0, y + 2, tam, h - 4, POLEGAR)
            else:
                v.rect(x + 2, y + s + p0, w - 4, tam, POLEGAR)

    def _comando(self, *args):
        c = self._op.get("command")
        if c:
            c(*args)

    def _tratar(self, tipo, ev):
        hor = self._horizontal()
        d = ev.x if hor else ev.y
        comp, p0, tam = self._geom()
        s = self._SETA
        if tipo == "ButtonPress" and ev.num == 1:
            if d < s:
                self._comando("scroll", -1, "units")
            elif d >= s + comp:
                self._comando("scroll", 1, "units")
            elif d - s < p0:
                self._comando("scroll", -1, "pages")
            elif d - s >= p0 + tam:
                self._comando("scroll", 1, "pages")
            else:
                self._arrasto = (d, self._ini)
        elif tipo == "Motion" and self._arrasto is not None and ev.state & ESTADO_B1:
            d0, ini0 = self._arrasto
            if comp > 0:
                self._comando("moveto", max(0.0, min(1.0, ini0 + (d - d0) / comp)))
        elif tipo == "ButtonRelease":
            self._arrasto = None
        elif tipo == "MouseWheel":
            self._comando("scroll", -1 if ev.delta > 0 else 1, "units")
            return "break"
        return None

    def _usa_roda(self):
        return True

    def activate(self, index=None):
        return ""

    def delta(self, deltax, deltay):
        comp = (self._wl if self._horizontal() else self._wa) - 2 * self._SETA
        return (deltax if self._horizontal() else deltay) / max(1, comp)

    def fraction(self, x, y):
        comp = (self._wl if self._horizontal() else self._wa) - 2 * self._SETA
        d = (x if self._horizontal() else y) - self._SETA
        return max(0.0, min(1.0, d / max(1, comp)))

    def identify(self, x, y):
        return "slider"


# ------------------------------------------------------------ rolagem em comum (Listbox, Text, Canvas)

class XView:
    def xview(self, *args):
        return self._xview(*args)

    def xview_moveto(self, fraction):
        return self._xview("moveto", fraction)

    def xview_scroll(self, number, what):
        return self._xview("scroll", number, what)

    def _xview(self, *args):
        return (0.0, 1.0)


class YView:
    def yview(self, *args):
        return self._yview(*args)

    def yview_moveto(self, fraction):
        return self._yview("moveto", fraction)

    def yview_scroll(self, number, what):
        return self._yview("scroll", number, what)


# ------------------------------------------------------------ Listbox

class Listbox(Widget, XView, YView):
    _classe = "Listbox"
    _foca_no_clique = True
    _padrao = {"background": BRANCO, "foreground": TEXTO, "borderwidth": 2, "relief": "sunken",
               "width": 20, "height": 10, "selectmode": "browse", "selectbackground": SELECAO,
               "selectforeground": BRANCO, "font": None, "listvariable": None, "yscrollcommand": None,
               "xscrollcommand": None, "state": "normal", "activestyle": "dotbox",
               "disabledforeground": FRACO, "exportselection": 1, "highlightthickness": 0,
               "highlightbackground": "", "highlightcolor": "", "setgrid": 0, "justify": "left",
               "takefocus": "", "cursor": "", "selectborderwidth": 0}
    _ALTURA = 18

    def _iniciar(self):
        self._itens = []
        self._cfg_itens = []
        self._sel = set()
        self._topo = 0
        self._ativo_i = 0
        self._ancora = 0
        lv = self._op.get("listvariable")
        if isinstance(lv, Variable):
            self._da_variavel(lv)

    def _foca_tab(self):
        return True

    def _da_variavel(self, lv):
        v = lv._valor
        if isinstance(v, str):
            v = v.split()
        self._itens = [str(x) for x in v]
        self._cfg_itens = [None] * len(self._itens)

    def _para_variavel(self):
        lv = self._op.get("listvariable")
        if isinstance(lv, Variable):
            lv._valor = tuple(self._itens)
            lv._avisar("write")

    def _variavel_mudou(self, var):
        self._da_variavel(var)
        self._sel = set(i for i in self._sel if i < len(self._itens))
        self._mudou_rolagem()

    def _natural(self):
        b = _int(self._op.get("borderwidth", 2))
        return _int(self._op.get("width", 20)) * LETRA_L + 2 * b + 4, _int(self._op.get("height", 10)) * self._ALTURA + 2 * b + 2

    def _visiveis(self):
        b = _int(self._op.get("borderwidth", 2))
        return max(1, (self._wa - 2 * b - 2) // self._ALTURA)

    # ---- índices
    def index(self, index):
        if isinstance(index, int):
            return index
        s = str(index)
        if s == "end":
            return len(self._itens)
        if s == "active":
            return self._ativo_i
        if s == "anchor":
            return self._ancora
        if s.startswith("@"):
            x, y = s[1:].split(",")
            return self.nearest(int(y))
        try:
            return int(s)
        except ValueError:
            raise TclError('bad listbox index "%s": must be active, anchor, end, @x,y, or a number' % index)

    def _faixa(self, first, last):
        a = self.index(first)
        b = a if last is None else self.index(last)
        if str(last) == "end":
            b = len(self._itens) - 1
        return a, b

    # ---- itens
    def insert(self, index, *elements):
        i = self.index(index)
        i = max(0, min(i, len(self._itens)))
        for k, e in enumerate(elements):
            self._itens.insert(i + k, str(e))
            self._cfg_itens.insert(i + k, None)
        n = len(elements)
        self._sel = set(s + n if s >= i else s for s in self._sel)
        self._para_variavel()
        self._mudou_rolagem()

    def delete(self, first, last=None):
        a, b = self._faixa(first, last)
        a = max(0, a)
        b = min(b, len(self._itens) - 1)
        if b < a:
            return
        del self._itens[a:b + 1]
        del self._cfg_itens[a:b + 1]
        n = b - a + 1
        self._sel = set(s if s < a else s - n for s in self._sel if not (a <= s <= b))
        if self._ativo_i > b:
            self._ativo_i -= n
        self._ativo_i = max(0, min(self._ativo_i, len(self._itens) - 1))
        self._topo = max(0, min(self._topo, len(self._itens) - 1))
        self._para_variavel()
        self._mudou_rolagem()

    def get(self, first, last=None):
        a, b = self._faixa(first, last)
        if last is None:
            if 0 <= a < len(self._itens):
                return self._itens[a]
            return ""
        return tuple(self._itens[max(0, a):b + 1])

    def size(self):
        return len(self._itens)

    def curselection(self):
        return tuple(sorted(self._sel))

    def selection_set(self, first, last=None):
        a, b = self._faixa(first, last)
        for i in range(max(0, a), min(b, len(self._itens) - 1) + 1):
            self._sel.add(i)
        self._redesenhar()

    select_set = selection_set

    def selection_clear(self, first, last=None):
        a, b = self._faixa(first, last)
        for i in range(a, b + 1):
            self._sel.discard(i)
        self._redesenhar()

    select_clear = selection_clear

    def selection_includes(self, index):
        return self.index(index) in self._sel

    select_includes = selection_includes

    def selection_anchor(self, index):
        self._ancora = self.index(index)

    select_anchor = selection_anchor

    def activate(self, index):
        self._ativo_i = max(0, min(self.index(index), len(self._itens) - 1))
        self._redesenhar()

    def see(self, index):
        i = self.index(index)
        vis = self._visiveis()
        if i < self._topo:
            self._topo = i
        elif i >= self._topo + vis:
            self._topo = i - vis + 1
        self._mudou_rolagem()

    def nearest(self, y):
        b = _int(self._op.get("borderwidth", 2))
        i = self._topo + (int(y) - b - 1) // self._ALTURA
        return max(0, min(i, len(self._itens) - 1))

    def bbox(self, index):
        i = self.index(index)
        if not self._topo <= i < self._topo + self._visiveis():
            return None
        b = _int(self._op.get("borderwidth", 2))
        return (b + 2, b + 1 + (i - self._topo) * self._ALTURA, len(self._itens[i]) * LETRA_L, self._ALTURA)

    def itemconfigure(self, index, cnf=None, **kw):
        i = self.index(index)
        if cnf:
            kw = dict(cnf, **kw)
        c = self._cfg_itens[i] or {}
        for k, v in kw.items():
            c[self._nome_opcao(k)] = v
        self._cfg_itens[i] = c
        self._redesenhar()

    itemconfig = itemconfigure

    def itemcget(self, index, option):
        c = self._cfg_itens[self.index(index)] or {}
        return c.get(self._nome_opcao(option), "")

    # ---- rolagem
    def _yview(self, *args):
        n = len(self._itens)
        vis = self._visiveis() if self._wa else _int(self._op.get("height", 10))
        if not args:
            if n == 0:
                return (0.0, 1.0)
            return (self._topo / n, min(1.0, (self._topo + vis) / n))
        if args[0] == "moveto":
            self._topo = int(round(float(args[1]) * n))
        elif args[0] == "scroll":
            k = int(args[1])
            self._topo += k * (vis if str(args[2]).startswith("page") else 1)
        else:
            self._topo = self.index(args[0])
        self._mudou_rolagem()

    def _mudou_rolagem(self):
        vis = self._visiveis() if self._wa else _int(self._op.get("height", 10))
        self._topo = max(0, min(self._topo, len(self._itens) - vis))
        self._redesenhar()
        cmd = self._op.get("yscrollcommand")
        if cmd:
            a, b = self._yview()
            cmd(a, b)

    def _colocar(self, x, y, w, h):
        Widget._colocar(self, x, y, w, h)
        self.after_idle(self._mudou_rolagem)

    # ---- desenho
    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fundo = self._cor_op("background", BRANCO)
        frente = self._cor_op("foreground", TEXTO) if self._ativo() else self._cor_op("disabledforeground", FRACO)
        v.rect(x, y, w, h, fundo)
        b = _int(self._op.get("borderwidth", 2))
        focado = _foco[0] is self
        _moldura_campo(v, x, y, w, h, self._op.get("relief", "sunken"), b, focado)
        ant = v.recorte()
        ix, iy = x + b, y + b
        iw, ih = w - 2 * b, h - 2 * b
        _recorte_dentro(v, ant, ix, iy, iw, ih)
        vis = self._visiveis() + 1
        sb, sf = self._cor_op("selectbackground", SELECAO), self._cor_op("selectforeground", BRANCO)
        for k in range(vis):
            i = self._topo + k
            if i >= len(self._itens):
                break
            ly = iy + 1 + k * self._ALTURA
            cfg = self._cfg_itens[i]
            cf = frente
            if cfg:
                if cfg.get("background"):
                    v.rect(ix, ly, iw, self._ALTURA, _cor(cfg["background"]))
                if cfg.get("foreground"):
                    cf = _cor(cfg["foreground"])
            if i in self._sel:
                v.rect(ix, ly, iw, self._ALTURA, sb)
                cf = sf
            v.texto(ix + 3, ly + 1, self._itens[i], cf)
            if focado and i == self._ativo_i and self._op.get("activestyle") != "none":
                v.rect(ix, ly, iw, self._ALTURA, DESTAQUE if i not in self._sel else BRANCO, relleno=False)
        v.recorte(*ant)

    # ---- eventos
    def _selecionar(self, i, ev):
        modo = self._op.get("selectmode", "browse")
        antes = set(self._sel)
        if modo in ("browse", "single"):
            self._sel = {i}
        elif modo == "multiple":
            if i in self._sel:
                self._sel.discard(i)
            else:
                self._sel.add(i)
        else:   # extended
            if ev is not None and ev.state & ESTADO_SHIFT:
                a, b = min(self._ancora, i), max(self._ancora, i)
                self._sel = set(range(a, b + 1))
            elif ev is not None and ev.state & ESTADO_CONTROL:
                if i in self._sel:
                    self._sel.discard(i)
                else:
                    self._sel.add(i)
                self._ancora = i
            else:
                self._sel = {i}
                self._ancora = i
        if modo != "extended":
            self._ancora = i
        self._ativo_i = i
        self._redesenhar()
        if self._sel != antes:
            self.event_generate("<<ListboxSelect>>")

    def _tratar(self, tipo, ev):
        if not self._ativo():
            return None
        if tipo == "ButtonPress" and ev.num == 1 and self._itens:
            i = self.nearest(ev.y)
            if self._topo <= i:
                self._selecionar(i, ev)
        elif tipo == "Motion" and ev.state & ESTADO_B1 and self._itens and \
                self._op.get("selectmode", "browse") in ("browse", "extended"):
            i = self.nearest(ev.y)
            if i != self._ativo_i:
                if self._op.get("selectmode") == "extended":
                    ev.state |= ESTADO_SHIFT
                self._selecionar(i, ev)
                self.see(i)
        elif tipo == "MouseWheel":
            self._yview("scroll", -3 if ev.delta > 0 else 3, "units")
            return "break"
        elif tipo == "KeyPress" and self._itens:
            k = ev.keysym
            i = self._ativo_i
            if k == "Up":
                i -= 1
            elif k == "Down":
                i += 1
            elif k == "Home":
                i = 0
            elif k == "End":
                i = len(self._itens) - 1
            elif k == "Prior":
                i -= self._visiveis()
            elif k == "Next":
                i += self._visiveis()
            elif k == "space":
                self._selecionar(self._ativo_i, ev)
                return "break"
            else:
                return None
            i = max(0, min(i, len(self._itens) - 1))
            if self._op.get("selectmode", "browse") in ("browse", "extended"):
                self._selecionar(i, ev)
            else:
                self._ativo_i = i
                self._redesenhar()
            self.see(i)
            return "break"
        return None

    def _usa_roda(self):
        return True


def _recorte_dentro(v, ant, x, y, w, h):
    """Recorta em (x, y, w, h) sem sair do recorte atual."""
    ax, ay, aw, ah = ant
    x0, y0 = max(ax, x), max(ay, y)
    x1, y1 = min(ax + aw, x + w), min(ay + ah, y + h)
    v.recorte(x0, y0, max(0, x1 - x0), max(0, y1 - y0))


# ------------------------------------------------------------ PhotoImage

_imagens = {}
_cont_img = [0]


class Image:
    pass


class PhotoImage(Image):
    """Imagem para Label, Button e Canvas: file= com BMP, PNG ou PPM/PGM
    (sagui.Imagen); width/height cria uma imagem vazia."""

    def __init__(self, name=None, cnf={}, master=None, **kw):
        kw = dict(cnf, **kw)
        _cont_img[0] += 1
        self.name = name or "pyimage%d" % _cont_img[0]
        self._img = None
        self._arquivo = None
        arq = kw.pop("file", None)
        dados = kw.pop("data", None)
        l, a = _int(kw.pop("width", 0)), _int(kw.pop("height", 0))
        kw.pop("format", None)
        kw.pop("gamma", None)
        kw.pop("palette", None)
        if arq is not None:
            try:
                self._img = sagui.Imagen.cargar(str(arq))
            except (OSError, ValueError) as e:
                raise TclError('couldn\'t open "%s": %s' % (arq, e))
            self._arquivo = str(arq)
        elif dados:
            raise TclError("PhotoImage(data=...) no está disponible en SaguiOS: usa file=")
        else:
            self._img = sagui.Imagen.nueva(max(1, l), max(1, a), 0xFFFFFF) if (l or a) else None
            self._vazia = True
        _imagens[self.name] = self

    def __str__(self):
        return self.name

    def width(self):
        return self._img.ancho if self._img else 0

    def height(self):
        return self._img.alto if self._img else 0

    def type(self):
        return "photo"

    def _desenhar_em(self, v, x, y):
        if self._img is not None:
            v.imagen(x, y, self._img)

    def cget(self, option):
        if option == "file":
            return self._arquivo or ""
        if option == "width":
            return self.width()
        if option == "height":
            return self.height()
        return ""

    __getitem__ = cget

    def configure(self, **kw):
        if "file" in kw:
            self._img = sagui.Imagen.cargar(str(kw["file"]))
            self._arquivo = str(kw["file"])

    config = configure

    def copy(self):
        p = PhotoImage()
        if self._img is not None:
            p._img = self._img.recortar(0, 0, self._img.ancho, self._img.alto)
        return p

    def zoom(self, x, y=""):
        y = x if y == "" else y
        p = PhotoImage()
        if self._img is not None:
            p._img = self._img.escalar(self._img.ancho * int(x), self._img.alto * int(y))
        return p

    def subsample(self, x, y=""):
        y = x if y == "" else y
        p = PhotoImage()
        if self._img is not None:
            p._img = self._img.escalar(max(1, self._img.ancho // int(x)), max(1, self._img.alto // int(y)))
        return p

    def get(self, x, y):
        c = self._img.pixel(int(x), int(y))
        return ((c >> 16) & 255, (c >> 8) & 255, c & 255)

    def put(self, data, to=None):
        if self._img is None:
            return
        if isinstance(data, str) and not data.startswith("{"):
            c = _cor(data)
            if to is None:
                self._img.rect(0, 0, self._img.ancho, self._img.alto, c)
            elif len(to) == 2:
                self._img.pixel(int(to[0]), int(to[1]), c)
            else:
                self._img.rect(int(to[0]), int(to[1]), int(to[2]) - int(to[0]), int(to[3]) - int(to[1]), c)
            return
        # linhas de cores: "{#f00 #0f0} {#00f #fff}" ou [[cor, cor], ...]
        x0, y0 = (int(to[0]), int(to[1])) if to else (0, 0)
        if isinstance(data, str):
            linhas = [l.strip().split() for l in data.replace("}", "{").split("{") if l.strip()]
        else:
            linhas = data
        for j, linha in enumerate(linhas):
            for i, c in enumerate(linha):
                self._img.pixel(x0 + i, y0 + j, _cor(c))

    def blank(self):
        if self._img is not None:
            self._img.limpiar(0xFFFFFF)

    def write(self, filename, format=None, from_coords=None):
        self._img.guardar(str(filename))

    def transparency_get(self, x, y):
        return False

    def transparency_set(self, x, y, boolean):
        pass


class BitmapImage(Image):
    def __init__(self, *args, **kw):
        raise TclError("BitmapImage no está disponible en SaguiOS: usa PhotoImage")


# ------------------------------------------------------------ o resto

from tkinter._menu import Menu, OptionMenu, Menubutton   # noqa: E402
from tkinter._texto import Text                          # noqa: E402
from tkinter._tela import Canvas                         # noqa: E402


class PanedWindow(Frame):
    """Simplificado: os painéis ficam lado a lado (sem arrastar o divisor)."""
    _classe = "Panedwindow"
    _padrao = dict(Frame._padrao, orient="horizontal", sashwidth=4, showhandle=0, sashrelief="flat",
                   sashpad=0, handlesize=8, handlepad=8, opaqueresize=1, proxybackground="",
                   proxyborderwidth=2, proxyrelief="flat", sashcursor="")

    def add(self, child, **kw):
        lado = "left" if str(self._op.get("orient", "horizontal")).startswith("h") else "top"
        child.pack(side=lado, fill="both", expand=True, padx=(0, 4) if lado == "left" else 0,
                   pady=(0, 4) if lado == "top" else 0)

    def forget(self, child):
        child.pack_forget()

    remove = forget

    def panes(self):
        return self.pack_slaves()


class Grid:
    pass


class Pack:
    pass


class Place:
    pass
