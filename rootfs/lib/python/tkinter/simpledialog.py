# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/simpledialog.py - askstring, askinteger, askfloat e Dialog
# (mejora 033, D084), com o aspecto do SaguiOS.
import tkinter
from tkinter import messagebox


class Dialog(tkinter.Toplevel):
    """Base de diálogos: redefina body(master), validate() e apply()."""

    def __init__(self, parent, title=None):
        if parent is None:
            parent = tkinter._raiz_padrao()
            if parent._v is None and not parent._escravos:
                parent.withdraw()
        tkinter.Toplevel.__init__(self, parent, _dialogo=True)
        self.transient(parent)
        if title:
            self.title(title)
        self.resizable(False, False)
        self.parent = parent
        self.result = None
        corpo = tkinter.Frame(self, padx=10, pady=10)
        self.initial_focus = self.body(corpo)
        corpo.pack(fill="both", expand=True)
        self.buttonbox()
        self.protocol("WM_DELETE_WINDOW", self.cancel)
        (self.initial_focus or self).focus_set()
        self.grab_set()
        self.wait_window(self)

    def body(self, master):
        return None

    def buttonbox(self):
        caixa = tkinter.Frame(self, padx=8, pady=8)
        tkinter.Button(caixa, text="Cancelar", width=10, command=self.cancel).pack(side="right", padx=4)
        tkinter.Button(caixa, text="Aceptar", width=10, command=self.ok).pack(side="right", padx=4)
        self.bind("<Return>", self.ok)
        self.bind("<Escape>", self.cancel)
        caixa.pack(fill="x")

    def ok(self, event=None):
        if not self.validate():
            (self.initial_focus or self).focus_set()
            return
        try:
            self.apply()
        finally:
            self.cancel()

    def cancel(self, event=None):
        self.destroy()

    def validate(self):
        return 1

    def apply(self):
        pass


class _QueryDialog(Dialog):
    def __init__(self, title, prompt, initialvalue=None, minvalue=None, maxvalue=None, parent=None, show=None):
        self.prompt = prompt
        self.minvalue = minvalue
        self.maxvalue = maxvalue
        self.initialvalue = initialvalue
        self.show = show
        Dialog.__init__(self, parent, title)

    def body(self, master):
        tkinter.Label(master, text=self.prompt, justify="left", anchor="w").pack(fill="x", pady=(0, 6))
        self.entry = tkinter.Entry(master, width=36, show=self.show or "")
        self.entry.pack(fill="x")
        if self.initialvalue is not None:
            self.entry.insert(0, str(self.initialvalue))
            self.entry.select_range(0, "end")
        return self.entry

    def validate(self):
        try:
            r = self.getresult()
        except ValueError:
            messagebox.showwarning("Valor no válido", self.errormessage + "\nInténtalo de nuevo.", parent=self)
            return 0
        if self.minvalue is not None and r < self.minvalue:
            messagebox.showwarning("Demasiado pequeño", "El valor mínimo es %s.\nInténtalo de nuevo." % self.minvalue, parent=self)
            return 0
        if self.maxvalue is not None and r > self.maxvalue:
            messagebox.showwarning("Demasiado grande", "El valor máximo es %s.\nInténtalo de nuevo." % self.maxvalue, parent=self)
            return 0
        self.result = r
        return 1


class _QueryString(_QueryDialog):
    errormessage = "No es un texto."

    def getresult(self):
        return self.entry.get()


class _QueryInteger(_QueryDialog):
    errormessage = "No es un número entero."

    def getresult(self):
        return int(self.entry.get().strip())


class _QueryFloat(_QueryDialog):
    errormessage = "No es un número."

    def getresult(self):
        return float(self.entry.get().strip().replace(",", "."))


def askstring(title, prompt, **kw):
    return _QueryString(title, prompt, **kw).result


def askinteger(title, prompt, **kw):
    return _QueryInteger(title, prompt, **kw).result


def askfloat(title, prompt, **kw):
    return _QueryFloat(title, prompt, **kw).result
