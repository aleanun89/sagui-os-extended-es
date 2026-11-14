# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/filedialog.py - escolher arquivos (mejora 033, D084).
#
# askopenfilename, asksaveasfilename, askopenfilenames, askdirectory,
# askopenfile e asksaveasfile, com um diálogo próprio e simples: a pasta
# atual, a lista de pastas e arquivos (as pastas primeiro, com "/"), o nome
# e um filtro pelos filetypes. Duplo clique numa pasta entra nela; num
# arquivo, escolhe.
import os
import tkinter
from tkinter import messagebox


def _padroes(filetypes):
    """[("Python", "*.py"), ("Todo", "*")] -> [("Python (*.py)", [".py"]), ...]"""
    r = []
    for item in filetypes or ():
        desc, pads = item[0], item[1]
        if isinstance(pads, str):
            pads = pads.split()
        exts = []
        for p in pads:
            p = p.strip()
            if p in ("*", "*.*", ""):
                exts = None
                break
            exts.append(p[1:].lower() if p.startswith("*") else p.lower())
        r.append(("%s (%s)" % (desc, " ".join(pads)), exts))
    return r


class _Escolher:
    def __init__(self, titulo, pasta, nome, filetypes, salvar, so_pastas, extensao, parent):
        if parent is None:
            parent = tkinter._raiz_padrao()
            if parent._v is None and not parent._escravos:
                parent.withdraw()
        self.resultado = ""
        self.salvar = salvar
        self.so_pastas = so_pastas
        self.extensao = extensao or ""
        self.filtros = _padroes(filetypes)
        self.filtro = self.filtros[0][1] if self.filtros else None
        pasta = pasta or os.getcwd()
        if not os.path.isdir(pasta):
            pasta = "/"
        self.pasta = os.path.abspath(pasta)
        d = self.d = tkinter.Toplevel(parent, _dialogo=True)
        d.title(titulo or ("Guardar como" if salvar else ("Elegir carpeta" if so_pastas else "Abrir")))
        d.transient(parent)
        d.minsize(360, 260)
        alto = tkinter.Frame(d, padx=8, pady=6)
        alto.pack(fill="x")
        tkinter.Label(alto, text="Carpeta:").pack(side="left")
        tkinter.Button(alto, text="Subir", command=self.subir).pack(side="right")
        self.rotulo = tkinter.Label(alto, text=self.pasta, anchor="w", relief="sunken", bg="white")
        self.rotulo.pack(side="left", fill="x", expand=True, padx=6)
        meio = tkinter.Frame(d, padx=8)
        meio.pack(fill="both", expand=True)
        self.lista = tkinter.Listbox(meio, width=46, height=12)
        barra = tkinter.Scrollbar(meio, command=self.lista.yview)
        self.lista.config(yscrollcommand=barra.set)
        barra.pack(side="right", fill="y")
        self.lista.pack(side="left", fill="both", expand=True)
        self.lista.bind("<<ListboxSelect>>", self.selecionou)
        self.lista.bind("<Double-Button-1>", self.ativou)
        self.lista.bind("<Return>", self.ativou)
        baixo = tkinter.Frame(d, padx=8, pady=6)
        baixo.pack(fill="x")
        tkinter.Label(baixo, text="Nombre:" if not so_pastas else "Carpeta:").grid(row=0, column=0, sticky="w")
        self.nome = tkinter.Entry(baixo)
        self.nome.grid(row=0, column=1, sticky="ew", padx=6)
        if nome:
            self.nome.insert(0, nome)
        if len(self.filtros) > 1:
            self.tipo = tkinter.StringVar(value=self.filtros[0][0])
            tkinter.Label(baixo, text="Tipo:").grid(row=1, column=0, sticky="w", pady=(4, 0))
            om = tkinter.OptionMenu(baixo, self.tipo, *[f[0] for f in self.filtros], command=self.mudou_filtro)
            om.grid(row=1, column=1, sticky="w", padx=6, pady=(4, 0))
        baixo.columnconfigure(1, weight=1)
        botoes = tkinter.Frame(d, padx=8, pady=8)
        botoes.pack(fill="x")
        tkinter.Button(botoes, text="Cancelar", width=10, command=d.destroy).pack(side="right", padx=4)
        tkinter.Button(botoes, text="Guardar" if salvar else ("Elegir" if so_pastas else "Abrir"), width=10,
                       command=self.aceitar).pack(side="right", padx=4)
        d.bind("<Escape>", lambda e: d.destroy())
        self.nome.bind("<Return>", lambda e: self.aceitar())
        self.encher()
        (self.nome if salvar else self.lista).focus_set()
        d.grab_set()
        d.wait_window()

    def mudou_filtro(self, valor):
        for desc, exts in self.filtros:
            if desc == valor:
                self.filtro = exts
        self.encher()

    def encher(self):
        self.rotulo.config(text=self.pasta)
        self.lista.delete(0, "end")
        try:
            nomes = sorted(os.listdir(self.pasta))
        except OSError:
            nomes = []
        pastas, arquivos = [], []
        for n in nomes:
            if os.path.isdir(os.path.join(self.pasta, n)):
                pastas.append(n + "/")
            elif not self.so_pastas:
                if self.filtro is None or any(n.lower().endswith(e) for e in self.filtro):
                    arquivos.append(n)
        self.itens = pastas + arquivos
        for n in self.itens:
            self.lista.insert("end", n)

    def subir(self):
        self.pasta = os.path.dirname(self.pasta.rstrip("/")) or "/"
        self.encher()

    def selecionou(self, ev=None):
        s = self.lista.curselection()
        if s and not self.itens[s[0]].endswith("/"):
            self.nome.delete(0, "end")
            self.nome.insert(0, self.itens[s[0]])

    def ativou(self, ev=None):
        s = self.lista.curselection()
        if not s:
            return
        n = self.itens[s[0]]
        if n.endswith("/"):
            self.pasta = os.path.join(self.pasta, n[:-1])
            self.encher()
        else:
            self.aceitar()

    def aceitar(self):
        n = self.nome.get().strip()
        if self.so_pastas:
            self.resultado = os.path.join(self.pasta, n) if n else self.pasta
            self.d.destroy()
            return
        if not n:
            return
        caminho = n if n.startswith("/") else os.path.join(self.pasta, n)
        if os.path.isdir(caminho):
            self.pasta = caminho
            self.nome.delete(0, "end")
            self.encher()
            return
        if self.salvar:
            if self.extensao and "." not in os.path.basename(caminho):
                caminho += self.extensao
            if os.path.exists(caminho) and not messagebox.askyesno(
                    "Confirmar", "%s ya existe.\n¿Quieres reemplazarlo?" % os.path.basename(caminho), parent=self.d):
                return
        elif not os.path.exists(caminho):
            messagebox.showwarning("Abrir", "No se encuentra el archivo:\n%s" % caminho, parent=self.d)
            return
        self.resultado = caminho
        self.d.destroy()


def _abrir(salvar, so_pastas, options):
    return _Escolher(options.get("title"), options.get("initialdir"), options.get("initialfile"),
                     options.get("filetypes"), salvar, so_pastas, options.get("defaultextension"),
                     options.get("parent")).resultado


def askopenfilename(**options):
    return _abrir(False, False, options)


def askopenfilenames(**options):
    r = _abrir(False, False, options)
    return (r,) if r else ()


def asksaveasfilename(**options):
    return _abrir(True, False, options)


def askdirectory(**options):
    return _abrir(False, True, options)


def askopenfile(mode="r", **options):
    r = askopenfilename(**options)
    return open(r, mode) if r else None


def askopenfiles(mode="r", **options):
    return [open(r, mode) for r in askopenfilenames(**options)]


def asksaveasfile(mode="w", **options):
    r = asksaveasfilename(**options)
    return open(r, mode) if r else None


class FileDialog:
    def __init__(self, master, title=None):
        self.master = master
        self.title = title

    def go(self, dir_or_file=os.curdir, pattern="*", default="", key=None):
        return askopenfilename(title=self.title, initialdir=dir_or_file, parent=self.master)


LoadFileDialog = FileDialog


class SaveFileDialog(FileDialog):
    def go(self, dir_or_file=os.curdir, pattern="*", default="", key=None):
        return asksaveasfilename(title=self.title, initialdir=dir_or_file, initialfile=default, parent=self.master)
