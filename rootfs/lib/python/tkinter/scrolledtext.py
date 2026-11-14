# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/scrolledtext.py - um Text com barra de rolagem (mejora 033, D084).
import tkinter


class ScrolledText(tkinter.Text):
    def __init__(self, master=None, **kw):
        self.frame = tkinter.Frame(master)
        self.vbar = tkinter.Scrollbar(self.frame)
        self.vbar.pack(side="right", fill="y")
        kw["yscrollcommand"] = self.vbar.set
        tkinter.Text.__init__(self, self.frame, **kw)
        self.pack(side="left", fill="both", expand=True)
        self.vbar["command"] = self.yview

    def pack(self, cnf={}, **kw):
        if self.master is self.frame and not getattr(self, "_dentro_pronto", False):
            self._dentro_pronto = True
            return tkinter.Text.pack(self, cnf, **kw)
        return self.frame.pack(cnf, **kw)

    def grid(self, cnf={}, **kw):
        return self.frame.grid(cnf, **kw)

    def place(self, cnf={}, **kw):
        return self.frame.place(cnf, **kw)

    def pack_forget(self):
        self.frame.pack_forget()

    def grid_forget(self):
        self.frame.grid_forget()

    def destroy(self):
        tkinter.Text.destroy(self)
        self.frame.destroy()
