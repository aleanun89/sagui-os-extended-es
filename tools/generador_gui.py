# SPDX-License-Identifier: GPL-3.0-only
"""generador_gui.py - la ventana Tkinter del generador de imágenes (D370). La lógica está en generador.py."""
import os
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

import generador as g

COL_FONDO = "#f6f7f9"
COL_TARJETA = "#ffffff"
COL_ACENTO = "#2f6fdb"
COL_TEXTO = "#1f2933"
COL_SUAVE = "#6b7785"
COL_AVISO = "#b42318"


def _estilo(raiz):
    s = ttk.Style(raiz)
    try:
        s.theme_use("clam")
    except tk.TclError:
        pass
    raiz.configure(bg=COL_FONDO)
    s.configure(".", background=COL_FONDO, foreground=COL_TEXTO, font=("Sans", 10))
    s.configure("TFrame", background=COL_FONDO)
    s.configure("Tarjeta.TFrame", background=COL_TARJETA)
    s.configure("TLabel", background=COL_FONDO)
    s.configure("Tarjeta.TLabel", background=COL_TARJETA)
    s.configure("Suave.TLabel", background=COL_TARJETA, foreground=COL_SUAVE, font=("Sans", 9))
    s.configure("Aviso.TLabel", background="#fdecea", foreground=COL_AVISO, font=("Sans", 10, "bold"), padding=6)
    s.configure("TCheckbutton", background=COL_TARJETA)
    s.configure("TRadiobutton", background=COL_FONDO)
    s.configure("TNotebook", background=COL_FONDO, borderwidth=0)
    s.configure("TNotebook.Tab", padding=(12, 6), background="#e4e8ee")
    s.map("TNotebook.Tab", background=[("selected", COL_TARJETA)])
    s.configure("Accion.TButton", background=COL_ACENTO, foreground="white", padding=(14, 6), font=("Sans", 10, "bold"))
    s.map("Accion.TButton", background=[("active", "#2559b3"), ("disabled", "#a9bde4")])
    s.configure("TButton", padding=(10, 5))
    s.configure("Horizontal.TProgressbar", troughcolor="#e4e8ee", background=COL_ACENTO)


class Ventana:
    def __init__(self, raiz, args):
        self.raiz = raiz
        raiz.title("Generador de imágenes de SaguiOS")
        raiz.geometry("1000x820")
        raiz.minsize(860, 640)
        _estilo(raiz)
        self.estado = g.cargar_estado()
        self.cfg = g.fusionar(g.config_nueva(), self.estado.get("ultima", {}))
        if args.perfil in g.PERFILES:
            g.aplicar_perfil(self.cfg, args.perfil)
        self.vars = {}
        self.filas = {}          # clave -> (widgets, etiqueta de motivo)
        self.proc = None
        self.destino_ult = None
        self.inicio = 0
        self.progreso = 0
        self.imagenes = []
        self._ocupado = False
        self._construir()
        self._refrescar()
        raiz.protocol("WM_DELETE_WINDOW", self._cerrar)

    # ----- construcción -----
    def _var(self, clave, tipo):
        v = tk.BooleanVar() if tipo == "bool" else tk.StringVar()
        v.set(self.cfg[clave])
        v.trace_add("write", lambda *_: self._cambio())
        self.vars[clave] = v
        return v

    def _fila(self, padre, clave, tipo, etiqueta, ayuda):
        f = ttk.Frame(padre, style="Tarjeta.TFrame")
        f.pack(fill="x", padx=12, pady=3)
        v = self._var(clave, tipo.split(":")[0])
        ws = []
        if tipo == "bool":
            w = ttk.Checkbutton(f, text=etiqueta, variable=v)
            w.pack(anchor="w")
            ws.append(w)
        else:
            ttk.Label(f, text=etiqueta, style="Tarjeta.TLabel").pack(anchor="w")
            if tipo.startswith("choice:"):
                w = ttk.Combobox(f, textvariable=v, values=tipo[7:].split("|"), state="readonly", width=24)
            elif tipo == "path":
                sub = ttk.Frame(f, style="Tarjeta.TFrame")
                sub.pack(fill="x")
                w = ttk.Entry(sub, textvariable=v)
                w.pack(side="left", fill="x", expand=True)
                b = ttk.Button(sub, text="Elegir…", command=lambda v=v: self._elegir_dir(v))
                b.pack(side="left", padx=(6, 0))
                ws.append(b)
                w.pack_configure()
            else:
                w = ttk.Entry(f, textvariable=v, width=18 if tipo == "int" else 50)
            if tipo != "path":
                w.pack(anchor="w")
            ws.append(w)
        info = ttk.Label(f, text=ayuda, style="Suave.TLabel", wraplength=780, justify="left")
        info.pack(anchor="w")
        self.filas[clave] = (ws, info, ayuda)

    def _pestana(self, nb, titulo, nota=None):
        marco = ttk.Frame(nb, style="Tarjeta.TFrame")
        canvas = tk.Canvas(marco, bg=COL_TARJETA, highlightthickness=0)
        sb = ttk.Scrollbar(marco, orient="vertical", command=canvas.yview)
        interior = ttk.Frame(canvas, style="Tarjeta.TFrame")
        interior.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        win = canvas.create_window((0, 0), window=interior, anchor="nw")
        canvas.bind("<Configure>", lambda e: canvas.itemconfigure(win, width=e.width))
        canvas.configure(yscrollcommand=sb.set)
        canvas.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        canvas.bind("<Enter>", lambda e: canvas.bind_all("<Button-4>", lambda ev: canvas.yview_scroll(-2, "units")))
        canvas.bind("<Enter>", lambda e: (canvas.bind_all("<Button-4>", lambda ev: canvas.yview_scroll(-2, "units")),
                                          canvas.bind_all("<Button-5>", lambda ev: canvas.yview_scroll(2, "units"))), add="+")
        nb.add(marco, text=titulo)
        if nota:
            ttk.Label(interior, text=nota, style="Suave.TLabel", wraplength=780).pack(anchor="w", padx=12, pady=(8, 2))
        return interior

    def _construir(self):
        r = self.raiz
        sup = ttk.Frame(r)
        sup.pack(fill="x", padx=12, pady=(10, 4))
        ttk.Label(sup, text="Perfil:", font=("Sans", 10, "bold")).pack(side="left")
        self.v_perfil = tk.StringVar(value=self.cfg["perfil"])
        for p in g.PERFILES:
            ttk.Radiobutton(sup, text=g.ETIQUETA_PERFIL[p], value=p, variable=self.v_perfil,
                            command=self._cambio_perfil).pack(side="left", padx=8)
        ttk.Button(sup, text="Borrar", command=self._borrar_perfil).pack(side="right")
        ttk.Button(sup, text="Guardar…", command=self._guardar_perfil).pack(side="right", padx=4)
        ttk.Button(sup, text="Cargar", command=self._cargar_perfil).pack(side="right")
        self.v_guardado = tk.StringVar()
        self.cb_guardado = ttk.Combobox(sup, textvariable=self.v_guardado, state="readonly", width=18,
                                        values=sorted(self.estado.get("perfiles", {})))
        self.cb_guardado.pack(side="right", padx=6)
        ttk.Label(sup, text="Perfiles guardados:").pack(side="right")

        self.aviso = ttk.Label(r, text="", style="Aviso.TLabel", wraplength=960)

        nb = ttk.Notebook(r)
        nb.pack(fill="both", expand=True, padx=12, pady=4)
        self.nb = nb
        t = self._pestana(nb, "Entrega", "Qué imágenes y cómo se compilan (gerar_versao.sh; la Personalizada usa solo compilación).")
        for clave, tipo, _d, etq, ayuda in g.OPC_GERAR:
            self._fila(t, clave, tipo, etq, ayuda)
        self.pest_mk = []
        for p in g.PESTANAS_MK:
            t = self._pestana(nb, p, "Opciones de la imagen (tools/mkimagem.py). Solo valen en el perfil Personalizada; "
                                     "lo que no se toca queda como en la imagen de hoy.")
            self.pest_mk.append(t)
            for clave, _flag, tipo, _d, pest, etq, ayuda in g.OPC_MK:
                if pest == p:
                    self._fila(t, clave, tipo, etq, ayuda)
        t = self._pestana(nb, "Personalizada", "Datos de la imagen personalizada.")
        for clave, tipo, etq, ayuda in (
                ("nombre", "str", "Nombre del fichero de la imagen", "Se crea en la carpeta de destino."),
                ("build", "path", "Carpeta de compilación", "De ahí salen el kernel, el ramdisk y el cargador (por defecto build/)."),
                ("con_privados", "bool", "Lleva datos privados (input_share, juegos, NetSurf)",
                 "Márcalo si lo que hay en la carpeta de compilación o de juegos es privado: activa el aviso y las comprobaciones.")):
            self._fila(t, clave, tipo, etq, ayuda)

        # destino
        d = ttk.Frame(r)
        d.pack(fill="x", padx=12, pady=2)
        ttk.Label(d, text="Carpeta de destino:", font=("Sans", 10, "bold")).pack(side="left")
        self.v_destino = tk.StringVar(value=self.cfg["destino"])
        self.v_destino.trace_add("write", lambda *_: self._cambio())
        ttk.Entry(d, textvariable=self.v_destino).pack(side="left", fill="x", expand=True, padx=6)
        ttk.Button(d, text="Elegir…", command=lambda: self._elegir_dir(self.v_destino)).pack(side="left")
        ttk.Button(d, text="Windows (/mnt/c)", command=self._elegir_windows).pack(side="left", padx=4)
        self.err = ttk.Label(r, text="", foreground=COL_AVISO, wraplength=960)
        self.err.pack(fill="x", padx=12)
        self.aviso.pack(fill="x", padx=12, pady=2, before=self.nb)

        # orden
        o = ttk.Frame(r)
        o.pack(fill="x", padx=12, pady=2)
        ttk.Label(o, text="Orden equivalente:").pack(side="left")
        ttk.Button(o, text="Copiar", command=self._copiar).pack(side="right")
        self.txt_orden = tk.Text(r, height=4, wrap="word", bg="#eef1f5", relief="flat", font=("Monospace", 9))
        self.txt_orden.pack(fill="x", padx=12)

        # botones y progreso
        b = ttk.Frame(r)
        b.pack(fill="x", padx=12, pady=6)
        self.b_gen = ttk.Button(b, text="Generar", style="Accion.TButton", command=self._generar)
        self.b_gen.pack(side="left")
        self.b_canc = ttk.Button(b, text="Cancelar", command=self._cancelar, state="disabled")
        self.b_canc.pack(side="left", padx=6)
        self.b_qemu = ttk.Button(b, text="Probar en QEMU", command=self._probar, state="disabled")
        self.b_qemu.pack(side="left", padx=6)
        self.b_abrir = ttk.Button(b, text="Abrir carpeta", command=self._abrir, state="disabled")
        self.b_abrir.pack(side="left")
        self.barra = ttk.Progressbar(b, maximum=100, length=260)
        self.barra.pack(side="right")
        self.lb_estado = ttk.Label(b, text="Listo")
        self.lb_estado.pack(side="right", padx=8)

        self.log = tk.Text(r, height=12, wrap="none", bg="#14181f", fg="#d6dde6", insertbackground="#d6dde6",
                           relief="flat", font=("Monospace", 9), state="disabled")
        self.log.tag_configure("err", foreground="#ff6b6b")
        self.log.tag_configure("fase", foreground="#7cc4ff", font=("Monospace", 9, "bold"))
        self.log.tag_configure("ok", foreground="#7ee787")
        self.log.pack(fill="both", expand=True, padx=12, pady=(0, 10))

    # ----- estado <-> widgets -----
    def _leer(self):
        for k, v in self.vars.items():
            self.cfg[k] = v.get() if not isinstance(self.cfg[k], bool) else bool(v.get())
        self.cfg["perfil"] = self.v_perfil.get()
        self.cfg["destino"] = self.v_destino.get().strip()

    def _volcar(self):
        self._bloq = True
        for k, v in self.vars.items():
            v.set(self.cfg[k])
        self.v_perfil.set(self.cfg["perfil"])
        self.v_destino.set(self.cfg["destino"])
        self._bloq = False

    def _cambio(self):
        if not getattr(self, "_bloq", False):
            self._refrescar()

    def _cambio_perfil(self):
        self._leer()
        g.aplicar_perfil(self.cfg, self.v_perfil.get())
        self._volcar()
        self._refrescar()

    def _refrescar(self):
        self._leer()
        bl = g.bloqueos(self.cfg)
        for k, (ws, info, ayuda) in self.filas.items():
            m = bl.get(k)
            for w in ws:
                try:
                    if isinstance(w, ttk.Combobox):
                        w.configure(state="disabled" if m else "readonly")
                    else:
                        w.configure(state="disabled" if m else "normal")
                except tk.TclError:
                    pass
            info.configure(text=(f"No disponible: {m}" if m else ayuda), foreground=("#b54708" if m else COL_SUAVE))
        pasos, errores, avisos, destino = g.construir(self.cfg)
        self.err.configure(text="\n".join(errores))
        if g.es_sensible(self.cfg):
            self.aviso.configure(text="USO PROPIO: NO PUBLICAR. Esta entrega lleva input_share, datos de juegos y/o NetSurf.")
            self.aviso.pack(fill="x", padx=12, pady=2, before=self.nb)
        else:
            self.aviso.pack_forget()
        self.txt_orden.delete("1.0", "end")
        self.txt_orden.insert("1.0", g.orden_equivalente(pasos) + ("\n# Avisos: " + "; ".join(avisos) if avisos else ""))
        self.pasos, self.errores = pasos, errores
        self._act_botones()
        self._guardar_ultima()

    def _act_botones(self):
        self.b_gen.configure(state="disabled" if (self._ocupado or self.errores) else "normal")

    def _guardar_ultima(self):
        self.estado["ultima"] = self.cfg
        g.guardar_estado(self.estado)

    # ----- perfiles guardados -----
    def _guardar_perfil(self):
        n = simpledialog.askstring("Guardar perfil", "Nombre del perfil:", parent=self.raiz)
        if n and n.strip():
            self._leer()
            self.estado.setdefault("perfiles", {})[n.strip()] = dict(self.cfg)
            g.guardar_estado(self.estado)
            self.cb_guardado.configure(values=sorted(self.estado["perfiles"]))
            self.v_guardado.set(n.strip())

    def _cargar_perfil(self):
        n = self.v_guardado.get()
        p = self.estado.get("perfiles", {}).get(n)
        if p:
            self.cfg = g.fusionar(g.config_nueva(), p)
            self._volcar()
            self._refrescar()

    def _borrar_perfil(self):
        n = self.v_guardado.get()
        if n and n in self.estado.get("perfiles", {}) and messagebox.askyesno("Borrar", f"¿Borrar el perfil «{n}»?"):
            del self.estado["perfiles"][n]
            g.guardar_estado(self.estado)
            self.cb_guardado.configure(values=sorted(self.estado["perfiles"]))
            self.v_guardado.set("")

    # ----- carpetas -----
    def _elegir_dir(self, var):
        ini = var.get() or g.RAIZ
        ini = ini if os.path.isdir(ini) else g.RAIZ
        d = filedialog.askdirectory(initialdir=ini, parent=self.raiz, title="Elige una carpeta")
        if d:
            var.set(d)

    def _elegir_windows(self):
        base = "/mnt/c/Users" if os.path.isdir("/mnt/c/Users") else "/"
        d = filedialog.askdirectory(initialdir=base, parent=self.raiz, title="Carpeta de Windows (/mnt/c/Users/...)")
        if d:
            self.v_destino.set(d)

    def _abrir(self):
        if self.destino_ult:
            g.abrir_carpeta(self.destino_ult)

    def _copiar(self):
        self.raiz.clipboard_clear()
        self.raiz.clipboard_append(g.orden_equivalente(self.pasos))

    # ----- ejecución -----
    def _escribir(self, texto, etiqueta=None):
        self.log.configure(state="normal")
        self.log.insert("end", texto + "\n", etiqueta)
        self.log.see("end")
        self.log.configure(state="disabled")

    def _generar(self):
        self._leer()
        self.pasos, self.errores, avisos, destino = g.construir(self.cfg)
        if self.errores:
            return
        if g.es_sensible(self.cfg) and not messagebox.askyesno(
                "Uso propio", "Esta entrega es PRIVADA (input_share, datos de juegos, NetSurf).\n"
                "No se puede publicar ni subir a GitHub.\n\n¿Continuar?"):
            return
        self.destino_ult = destino
        self.inicio = time.time()
        self.progreso = 0
        self.barra["value"] = 0
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        self._escribir("$ " + g.orden_equivalente(self.pasos).replace("\n", "\n$ "), "fase")
        self._ocupado = True
        self.b_canc.configure(state="normal")
        self.b_qemu.configure(state="disabled")
        self.b_abrir.configure(state="disabled")
        self.lb_estado.configure(text="Generando…")
        self._act_botones()
        self.proc = g.Proceso(g.orden_shell(self.pasos),
                              lambda l: self.raiz.after(0, self._linea, l),
                              lambda rc, c: self.raiz.after(0, self._fin, rc, c))

    def _linea(self, l):
        etq = None
        if l.startswith("== "):
            etq = "fase"
        elif any(p in l.lower() for p in ("error", "erro:", "fatal", "traceback", "aviso:")) and "-werror" not in l.lower():
            etq = "err"
        self._escribir(l, etq)
        nuevo = g.progreso_de(l, self.progreso)
        if nuevo != self.progreso:
            self.progreso = nuevo
            self.barra["value"] = nuevo
            self.lb_estado.configure(text=f"Generando… {nuevo}%")

    def _fin(self, rc, cancelado):
        self._ocupado = False
        self.b_canc.configure(state="disabled")
        self._act_botones()
        if cancelado:
            self.lb_estado.configure(text="Cancelado")
            self._escribir("Cancelado.", "err")
            return
        if rc != 0:
            self.lb_estado.configure(text=f"Falló (código {rc})")
            self._escribir(f"La generación terminó con error (código {rc}).", "err")
            return
        self.barra["value"] = 100
        self.lb_estado.configure(text="Terminado")
        self.b_abrir.configure(state="normal")
        self._escribir("")
        self._escribir("Resumen de lo generado en " + self.destino_ult, "ok")
        self.lb_estado.configure(text="Calculando sumas…")
        threading.Thread(target=self._resumen, daemon=True).start()

    def _resumen(self):
        try:
            self.imagenes = g.resumen_imagenes(self.destino_ult, self.inicio)
        except OSError:
            self.imagenes = []
        self.raiz.after(0, self._mostrar_resumen)

    def _mostrar_resumen(self):
        for n, tam, suma in self.imagenes:
            self._escribir(f"  {n:<40} {g.tam_humano(tam):>10}  {suma}", "ok")
        if not self.imagenes:
            self._escribir("  (no se encontraron imágenes nuevas)", "err")
        else:
            self.b_qemu.configure(state="normal")
        if g.es_sensible(self.cfg):
            self._escribir("USO PROPIO: NO PUBLICAR.", "err")
        self.lb_estado.configure(text="Terminado")

    def _cancelar(self):
        if self.proc:
            self.proc.cancelar()
            self.lb_estado.configure(text="Cancelando…")

    def _probar(self):
        if not self.imagenes:
            return
        nombres = [n for n, _t, _s in self.imagenes if n.endswith(".img") and not n.endswith(".xz")]
        if not nombres:
            return
        elegido = nombres[0]
        if len(nombres) > 1:
            elegido = self._elegir_imagen(nombres) or ""
        if elegido:
            g.lanzar_qemu(os.path.join(self.destino_ult, elegido))
            self._escribir(f"QEMU lanzado con {elegido} (en segundo plano).", "ok")

    def _elegir_imagen(self, nombres):
        w = tk.Toplevel(self.raiz)
        w.title("Imagen a probar")
        v = tk.StringVar(value=nombres[0])
        res = {}
        for n in nombres:
            ttk.Radiobutton(w, text=n, value=n, variable=v).pack(anchor="w", padx=14, pady=2)

        def ok():
            res["n"] = v.get()
            w.destroy()
        ttk.Button(w, text="Probar", style="Accion.TButton", command=ok).pack(pady=8)
        w.transient(self.raiz)
        w.grab_set()
        self.raiz.wait_window(w)
        return res.get("n")

    def _cerrar(self):
        if self._ocupado and not messagebox.askyesno("Salir", "Hay una generación en marcha. ¿Cancelarla y salir?"):
            return
        if self.proc and self._ocupado:
            self.proc.cancelar()
        self._leer()
        self._guardar_ultima()
        self.raiz.destroy()


def abrir(args):
    raiz = tk.Tk()
    Ventana(raiz, args)
    raiz.mainloop()
    return 0
