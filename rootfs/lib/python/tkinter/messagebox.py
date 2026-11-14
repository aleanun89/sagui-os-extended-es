# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/messagebox.py - diálogos de mensagem (mejora 033, D084).
#
# showinfo, showwarning, showerror, askquestion, askokcancel, askyesno,
# askyesnocancel e askretrycancel, com a interface do tkinter e o aspecto
# do SaguiOS: uma janela de diálogo (fora da barra de tarefas, D065), um
# ícone, o texto e os botões em espanhol. Esperam a resposta (modais).
import tkinter

ERROR = "error"
INFO = "info"
QUESTION = "question"
WARNING = "warning"
ABORTRETRYIGNORE = "abortretryignore"
OK = "ok"
OKCANCEL = "okcancel"
RETRYCANCEL = "retrycancel"
YESNO = "yesno"
YESNOCANCEL = "yesnocancel"
ABORT = "abort"
RETRY = "retry"
IGNORE = "ignore"
CANCEL = "cancel"
YES = "yes"
NO = "no"

_ROTULOS = {"ok": "Aceptar", "cancel": "Cancelar", "yes": "Sí", "no": "No", "retry": "Reintentar",
            "abort": "Abortar", "ignore": "Ignorar"}
_BOTOES = {OK: ("ok",), OKCANCEL: ("ok", "cancel"), YESNO: ("yes", "no"), YESNOCANCEL: ("yes", "no", "cancel"),
           RETRYCANCEL: ("retry", "cancel"), ABORTRETRYIGNORE: ("abort", "retry", "ignore")}


def _icone(master, tipo):
    c = tkinter.Canvas(master, width=36, height=36, highlightthickness=0)
    if tipo == WARNING:
        c.create_polygon(18, 2, 35, 33, 1, 33, fill="#FFD52E", outline="#7A5C00")
        c.create_text(18, 22, text="!", fill="#1E2328")
    else:
        cor = {ERROR: "#D83A30", QUESTION: "#2A6FD8"}.get(tipo, "#0B7A3E")
        c.create_oval(2, 2, 33, 33, fill=cor, outline="")
        c.create_text(18, 18, text={ERROR: "x", QUESTION: "?"}.get(tipo, "i"), fill="white")
    return c


def _janela_dialogo(parent, titulo):
    if parent is None:
        parent = tkinter._raiz_padrao()
        if parent._v is None and not parent._escravos:
            parent.withdraw()   # programa só com diálogos: a raiz vazia não aparece
    d = tkinter.Toplevel(parent, _dialogo=True)
    d.title(titulo or "")
    d.resizable(False, False)
    d.transient(parent._top() if parent is not None else None)
    return d


def _mostrar(title=None, message=None, icon=INFO, type=OK, default=None, parent=None, detail=None, **kw):
    botoes = _BOTOES.get(type, ("ok",))
    if default is None:
        default = botoes[0]
    d = _janela_dialogo(parent, title)
    resposta = [None]

    def responder(r):
        resposta[0] = r
        d.destroy()

    corpo = tkinter.Frame(d, padx=12, pady=12)
    corpo.pack(fill="both", expand=True)
    _icone(corpo, icon).pack(side="left", anchor="n", padx=(0, 12))
    texto = str(message or "")
    if detail:
        texto += "\n\n" + str(detail)
    tkinter.Label(corpo, text=texto, justify="left", anchor="w", wraplength=360).pack(side="left", fill="both", expand=True)
    barra = tkinter.Frame(d, padx=8, pady=8)
    barra.pack(fill="x")
    foco = None
    for b in reversed(botoes):
        w = tkinter.Button(barra, text=_ROTULOS[b], width=max(8, len(_ROTULOS[b]) + 2),
                           command=lambda b=b: responder(b))
        w.pack(side="right", padx=4)
        if b == default:
            foco = w
    cancelar = "cancel" if "cancel" in botoes else ("no" if "no" in botoes else botoes[-1])
    d.bind("<Return>", lambda e: responder(default) if not isinstance(tkinter._foco[0], tkinter.Button) else None)
    d.bind("<Escape>", lambda e: responder(cancelar))
    d.protocol("WM_DELETE_WINDOW", lambda: responder(cancelar if len(botoes) > 1 else botoes[0]))
    if foco is not None:
        foco.focus_set()
    d.grab_set()
    d.wait_window()
    return resposta[0] if resposta[0] is not None else cancelar


class Message:
    """tkinter.messagebox.Message(title=..., message=..., icon=..., type=...).show()"""
    command = None

    def __init__(self, master=None, **options):
        self.master = master
        self.options = options

    def show(self, **options):
        o = dict(self.options, **options)
        return _mostrar(parent=self.master, **o)


def showinfo(title=None, message=None, **options):
    return _mostrar(title, message, INFO, OK, **options)


def showwarning(title=None, message=None, **options):
    return _mostrar(title, message, WARNING, OK, **options)


def showerror(title=None, message=None, **options):
    return _mostrar(title, message, ERROR, OK, **options)


def askquestion(title=None, message=None, **options):
    return _mostrar(title, message, QUESTION, YESNO, **options)


def askokcancel(title=None, message=None, **options):
    return _mostrar(title, message, QUESTION, OKCANCEL, **options) == "ok"


def askyesno(title=None, message=None, **options):
    return _mostrar(title, message, QUESTION, YESNO, **options) == "yes"


def askyesnocancel(title=None, message=None, **options):
    r = _mostrar(title, message, QUESTION, YESNOCANCEL, **options)
    if r == "cancel":
        return None
    return r == "yes"


def askretrycancel(title=None, message=None, **options):
    return _mostrar(title, message, WARNING, RETRYCANCEL, **options) == "retry"
