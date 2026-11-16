#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Conversor de vídeo para SaguiOS (se ejecuta en el PC, no en SaguiOS).

Prepara vídeos en los formatos que entiende el Reproductor de SaguiOS (reproductor.c, D129/D276) usando
ffmpeg por debajo, con una interfaz gráfica sencilla (tkinter: Windows, Linux y macOS).

Formatos de salida:
  - MP4 H.264 Main + AAC-LC: el que menos ocupa; el recomendado.
  - MP4 H.264 Baseline + AAC-LC: más ligero de decodificar (sin CABAC ni cuadros B), para PC antiguos.
  - AVI MJPEG + PCM: ocupa mucho, pero es el más ligero de decodificar (Pentium II y anteriores).
  - MPEG-1 (MPEG-PS): solo vídeo; el reproductor aún no decodifica su audio.

Vídeos con varias pistas de audio (audio dual): se elige una, por posición o por idioma («Ver pistas...»
enseña las que trae cada vídeo y cuál se usará). Si un vídeo no tiene la pista pedida, se usa la primera.

Uso:
  python3 tools/conversor_video_pc.py            la ventana
  python3 tools/conversor_video_pc.py --ffmpeg RUTA   si ffmpeg no está en el PATH

Requisitos: Python 3 con tkinter (en Windows viene con el instalador de python.org) y ffmpeg
(https://ffmpeg.org/download.html; en Windows basta con poner ffmpeg.exe junto a este fichero).
"""
import os
import queue
import re
import shutil
import subprocess
import sys
import threading

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:  # pragma: no cover
    print("Hace falta tkinter (en Debian/Ubuntu: sudo apt install python3-tk)")
    sys.exit(1)

# ------------------------------------------------------------------ formatos y calidades

FORMATOS = {
    "MP4 H.264 Main + AAC (recomendado)": {
        "ext": ".mp4",
        "video": ["-c:v", "libx264", "-profile:v", "main", "-level", "3.1", "-pix_fmt", "yuv420p",
                  "-preset", "medium"],
        "audio": ["-c:a", "aac", "-ac", "2", "-ar", "44100"],
        "extra": ["-movflags", "+faststart"],
        "calidad": "crf",
    },
    "MP4 H.264 Baseline + AAC (PC antiguos)": {
        "ext": ".mp4",
        "video": ["-c:v", "libx264", "-profile:v", "baseline", "-level", "3.0", "-pix_fmt", "yuv420p",
                  "-preset", "medium"],
        "audio": ["-c:a", "aac", "-ac", "2", "-ar", "44100"],
        "extra": ["-movflags", "+faststart"],
        "calidad": "crf",
    },
    "AVI MJPEG + PCM (el más ligero de decodificar)": {
        "ext": ".avi",
        "video": ["-c:v", "mjpeg", "-pix_fmt", "yuvj420p"],
        "audio": ["-c:a", "pcm_s16le", "-ac", "2", "-ar", "22050"],
        "extra": ["-f", "avi"],
        "calidad": "q",
    },
    "MPEG-1 (solo vídeo, sin sonido)": {
        "ext": ".mpg",
        "video": ["-c:v", "mpeg1video", "-pix_fmt", "yuv420p"],
        "audio": ["-an"],
        "extra": ["-f", "mpeg"],
        "calidad": "q",
    },
}

# calidad -> (crf de x264, q de mjpeg/mpeg1, kbps de audio AAC)
CALIDADES = {
    "Alta": (21, 3, 160),
    "Media": (25, 5, 128),
    "Baja (ocupa poco)": (29, 8, 96),
}

RESOLUCIONES = {
    "La original": None,
    "320 de ancho (muy ligero)": 320,
    "480 de ancho": 480,
    "640 de ancho (recomendado)": 640,
    "800 de ancho": 800,
    "1024 de ancho": 1024,
}

# Pista de audio (vídeos con audio dual): por posición o por idioma (código ISO 639-2 de la pista).
PISTAS = {
    "La primera": ("pos", 0),
    "La segunda": ("pos", 1),
    "La tercera": ("pos", 2),
    "Por idioma: español": ("idioma", ("spa", "es", "esp", "cas")),
    "Por idioma: inglés": ("idioma", ("eng", "en")),
    "Por idioma: portugués": ("idioma", ("por", "pt")),
    "Por idioma: catalán": ("idioma", ("cat", "ca")),
    "Por idioma: francés": ("idioma", ("fre", "fra", "fr")),
    "Por idioma: japonés": ("idioma", ("jpn", "ja")),
}

FPS = {"Los originales": None, "15": 15, "20": 20, "24": 24, "25": 25, "30": 30}

VIDEOS = ("*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.mpg", "*.mpeg", "*.wmv", "*.flv", "*.m4v", "*.3gp", "*.ts")


def buscar_ffmpeg(dado=None):
    """ffmpeg y ffprobe: el dado, el que está junto a este fichero o el del PATH."""
    if dado and os.path.isfile(dado):
        ff = dado
    else:
        aqui = os.path.dirname(os.path.abspath(__file__))
        ff = None
        for n in ("ffmpeg.exe", "ffmpeg"):
            c = os.path.join(aqui, n)
            if os.path.isfile(c):
                ff = c
                break
        ff = ff or shutil.which("ffmpeg")
    if not ff:
        return None, None
    d = os.path.dirname(ff)
    fp = None
    for n in ("ffprobe.exe", "ffprobe"):
        c = os.path.join(d, n)
        if os.path.isfile(c):
            fp = c
            break
    return ff, fp or shutil.which("ffprobe")


def duracion(ffprobe, ruta):
    """La duración en segundos (o None) para la barra de progreso."""
    if not ffprobe:
        return None
    try:
        r = subprocess.run([ffprobe, "-v", "error", "-show_entries", "format=duration", "-of",
                            "default=nw=1:nk=1", ruta], capture_output=True, text=True, timeout=30,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return float(r.stdout.strip())
    except (ValueError, OSError, subprocess.SubprocessError):
        return None


def pistas_audio(ffprobe, ruta):
    """Las pistas de audio del vídeo: [{'n': posición entre las de audio, 'idioma', 'titulo', 'codec', 'canales'}]."""
    if not ffprobe:
        return []
    try:
        r = subprocess.run([ffprobe, "-v", "error", "-select_streams", "a", "-show_entries",
                            "stream=codec_name,channels:stream_tags=language,title", "-of", "json", ruta],
                           capture_output=True, text=True, timeout=30,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        import json
        res = []
        for n, st in enumerate(json.loads(r.stdout or "{}").get("streams", [])):
            tags = st.get("tags", {})
            res.append({"n": n, "idioma": (tags.get("language") or "").lower(), "titulo": tags.get("title") or "",
                        "codec": st.get("codec_name") or "?", "canales": st.get("channels") or 0})
        return res
    except (ValueError, OSError, subprocess.SubprocessError):
        return []


def elegir_pista(pistas, criterio):
    """La posición de la pista de audio a usar según el criterio de PISTAS, o 0 (la primera) si no hay."""
    tipo, v = PISTAS[criterio]
    if not pistas:
        return 0
    if tipo == "pos":
        return v if v < len(pistas) else 0
    for p in pistas:
        if p["idioma"] in v:
            return p["n"]
    return 0


def describir_pistas(pistas):
    if not pistas:
        return "sin pistas de audio (o no se pudo leer: falta ffprobe)"
    nombres = {1: "mono", 2: "estéreo", 6: "5.1", 8: "7.1"}
    return "\n".join("%d. %s %s %s%s" % (p["n"] + 1, p["idioma"] or "(sin idioma)", p["codec"],
                                          nombres.get(p["canales"], "%d canales" % p["canales"]),
                                          (" - " + p["titulo"]) if p["titulo"] else "") for p in pistas)


def orden_ffmpeg(ffmpeg, entrada, salida, formato, calidad, ancho, fps, pista=0):
    f = FORMATOS[formato]
    crf, q, kbps = CALIDADES[calidad]
    cmd = [ffmpeg, "-hide_banner", "-y", "-i", entrada, "-map", "0:v:0", "-map", "0:a:%d?" % pista]
    filtros = []
    if ancho:
        # sin agrandar vídeos más pequeños; alto par (lo pide el H.264 4:2:0)
        filtros.append("scale='min(%d,iw)':-2" % ancho)
    if fps:
        filtros.append("fps=%d" % fps)
    if filtros:
        cmd += ["-vf", ",".join(filtros)]
    cmd += f["video"]
    if f["calidad"] == "crf":
        cmd += ["-crf", str(crf)]
    else:
        cmd += ["-q:v", str(q)]
    cmd += f["audio"]
    if f["audio"][:2] == ["-c:a", "aac"]:
        cmd += ["-b:a", "%dk" % kbps]
    cmd += f["extra"] + ["-progress", "pipe:1", "-nostats", salida]
    return cmd


# ------------------------------------------------------------------ la ventana

class Ventana:
    def __init__(self, raiz, ffmpeg, ffprobe):
        self.raiz, self.ffmpeg, self.ffprobe = raiz, ffmpeg, ffprobe
        self.proc = None
        self.cancelado = False
        self.cola = queue.Queue()
        raiz.title("Conversor de vídeo para SaguiOS")
        raiz.minsize(620, 440)
        m = ttk.Frame(raiz, padding=10)
        m.pack(fill="both", expand=True)

        ttk.Label(m, text="Vídeos a convertir:").grid(row=0, column=0, sticky="w")
        self.lista = tk.Listbox(m, height=8, selectmode="extended")
        self.lista.grid(row=1, column=0, columnspan=3, sticky="nsew")
        b = ttk.Frame(m)
        b.grid(row=1, column=3, sticky="n", padx=(8, 0))
        ttk.Button(b, text="Añadir...", command=self.anadir).pack(fill="x")
        ttk.Button(b, text="Quitar", command=self.quitar).pack(fill="x", pady=4)
        ttk.Button(b, text="Vaciar", command=lambda: self.lista.delete(0, "end")).pack(fill="x")
        ttk.Button(b, text="Ver pistas...", command=self.ver_pistas).pack(fill="x", pady=(4, 0))

        self.formato = self._combo(m, 2, "Formato:", list(FORMATOS), 0)
        self.calidad = self._combo(m, 3, "Calidad:", list(CALIDADES), 1)
        self.resol = self._combo(m, 4, "Tamaño:", list(RESOLUCIONES), 3)
        self.fps = self._combo(m, 5, "Cuadros por segundo:", list(FPS), 0)
        self.pista = self._combo(m, 6, "Pista de audio:", list(PISTAS), 0)

        ttk.Label(m, text="Carpeta de destino:").grid(row=7, column=0, sticky="w", pady=(8, 0))
        self.destino = tk.StringVar(value="")
        ttk.Entry(m, textvariable=self.destino).grid(row=7, column=1, columnspan=2, sticky="ew", pady=(8, 0))
        ttk.Button(m, text="...", width=3, command=self.elegir_destino).grid(row=7, column=3, sticky="w",
                                                                           padx=(8, 0), pady=(8, 0))
        ttk.Label(m, text="(vacío = la misma carpeta que cada vídeo)", foreground="gray").grid(
            row=8, column=1, columnspan=2, sticky="w")

        self.estado = tk.StringVar(value="Listo." if ffmpeg else "No se encontró ffmpeg.")
        ttk.Label(m, textvariable=self.estado).grid(row=9, column=0, columnspan=4, sticky="w", pady=(10, 2))
        self.barra = ttk.Progressbar(m, maximum=100)
        self.barra.grid(row=10, column=0, columnspan=4, sticky="ew")

        bb = ttk.Frame(m)
        bb.grid(row=11, column=0, columnspan=4, sticky="e", pady=(10, 0))
        self.b_conv = ttk.Button(bb, text="Convertir", command=self.convertir)
        self.b_conv.pack(side="left")
        self.b_canc = ttk.Button(bb, text="Cancelar", command=self.cancelar, state="disabled")
        self.b_canc.pack(side="left", padx=6)

        m.columnconfigure(1, weight=1)
        m.rowconfigure(1, weight=1)
        raiz.after(100, self.atender)

    def _combo(self, m, fila, rotulo, valores, defecto):
        ttk.Label(m, text=rotulo).grid(row=fila, column=0, sticky="w", pady=(6, 0))
        c = ttk.Combobox(m, values=valores, state="readonly")
        c.current(defecto)
        c.grid(row=fila, column=1, columnspan=2, sticky="ew", pady=(6, 0))
        return c

    def anadir(self):
        rutas = filedialog.askopenfilenames(title="Elegir vídeos",
                                            filetypes=[("Vídeos", " ".join(VIDEOS)), ("Todos", "*.*")])
        for r in rutas:
            if r not in self.lista.get(0, "end"):
                self.lista.insert("end", r)

    def quitar(self):
        for i in reversed(self.lista.curselection()):
            self.lista.delete(i)

    def ver_pistas(self):
        sel = self.lista.curselection()
        rutas = [self.lista.get(i) for i in sel] or list(self.lista.get(0, "end"))[:1]
        if not rutas:
            messagebox.showinfo("Pistas de audio", "Añade (y elige) un vídeo.")
            return
        texto = []
        for r in rutas[:5]:
            ps = pistas_audio(self.ffprobe, r)
            elegida = elegir_pista(ps, self.pista.get())
            texto.append("%s\n%s\n-> se usará la %d" % (os.path.basename(r), describir_pistas(ps), elegida + 1))
        messagebox.showinfo("Pistas de audio", "\n\n".join(texto))

    def elegir_destino(self):
        d = filedialog.askdirectory(title="Carpeta de destino")
        if d:
            self.destino.set(d)

    def salida_de(self, entrada, ext):
        base = os.path.splitext(os.path.basename(entrada))[0] + "-sagui" + ext
        return os.path.join(self.destino.get() or os.path.dirname(entrada), base)

    def convertir(self):
        if not self.ffmpeg:
            messagebox.showerror("Falta ffmpeg", "No se encontró ffmpeg. Instálalo o pon ffmpeg.exe junto a "
                                 "este programa.")
            return
        entradas = list(self.lista.get(0, "end"))
        if not entradas:
            messagebox.showinfo("Sin vídeos", "Añade al menos un vídeo.")
            return
        fmt = self.formato.get()
        trabajos = [(e, self.salida_de(e, FORMATOS[fmt]["ext"])) for e in entradas]
        existen = [s for _, s in trabajos if os.path.exists(s)]
        if existen and not messagebox.askyesno("Ya existen", "%d fichero(s) de destino ya existen. "
                                               "¿Sobrescribirlos?" % len(existen)):
            return
        if fmt.startswith("MPEG-1"):
            messagebox.showinfo("MPEG-1", "El Reproductor de SaguiOS aún no decodifica el audio del MPEG-1: "
                                "el vídeo se verá sin sonido.")
        opc = (fmt, self.calidad.get(), RESOLUCIONES[self.resol.get()], FPS[self.fps.get()], self.pista.get())
        self.cancelado = False
        self.b_conv.config(state="disabled")
        self.b_canc.config(state="normal")
        threading.Thread(target=self.hilo, args=(trabajos, opc), daemon=True).start()

    def cancelar(self):
        self.cancelado = True
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()

    def hilo(self, trabajos, opc):
        fmt, cal, ancho, fps, criterio = opc
        hechos, fallos = 0, []
        for n, (ent, sal) in enumerate(trabajos, 1):
            if self.cancelado:
                break
            total = duracion(self.ffprobe, ent)
            self.cola.put(("estado", "(%d/%d) %s" % (n, len(trabajos), os.path.basename(ent))))
            self.cola.put(("barra", 0))
            pista = elegir_pista(pistas_audio(self.ffprobe, ent), criterio)
            cmd = orden_ffmpeg(self.ffmpeg, ent, sal, fmt, cal, ancho, fps, pista)
            try:
                self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            except OSError as e:
                fallos.append("%s: %s" % (os.path.basename(ent), e))
                continue
            err = []
            t = threading.Thread(target=lambda: err.extend(self.proc.stderr.readlines()), daemon=True)
            t.start()
            for linha in self.proc.stdout:
                m = re.match(r"out_time_us=(\d+)", linha)
                if m and total:
                    self.cola.put(("barra", min(100.0, int(m.group(1)) / 1e6 / total * 100)))
            self.proc.wait()
            t.join(timeout=2)
            if self.cancelado:
                if os.path.exists(sal):
                    try:
                        os.remove(sal)
                    except OSError:
                        pass
                break
            if self.proc.returncode == 0:
                hechos += 1
            else:
                fallos.append("%s: %s" % (os.path.basename(ent), "".join(err[-3:]).strip()[-300:]))
        self.cola.put(("fin", (hechos, len(trabajos), fallos)))

    def atender(self):
        try:
            while True:
                tipo, v = self.cola.get_nowait()
                if tipo == "estado":
                    self.estado.set(v)
                elif tipo == "barra":
                    self.barra["value"] = v
                elif tipo == "fin":
                    hechos, total, fallos = v
                    self.b_conv.config(state="normal")
                    self.b_canc.config(state="disabled")
                    if self.cancelado:
                        self.estado.set("Cancelado (%d de %d convertidos)." % (hechos, total))
                    else:
                        self.barra["value"] = 100
                        self.estado.set("Hecho: %d de %d convertidos." % (hechos, total))
                    if fallos:
                        messagebox.showerror("Errores", "\n\n".join(fallos))
        except queue.Empty:
            pass
        self.raiz.after(100, self.atender)


def main():
    dado = None
    if "--ffmpeg" in sys.argv:
        i = sys.argv.index("--ffmpeg")
        dado = sys.argv[i + 1] if i + 1 < len(sys.argv) else None
    ffmpeg, ffprobe = buscar_ffmpeg(dado)
    raiz = tk.Tk()
    Ventana(raiz, ffmpeg, ffprobe)
    if not ffmpeg:
        raiz.after(300, lambda: messagebox.showwarning(
            "Falta ffmpeg", "No se encontró ffmpeg. Descárgalo de https://ffmpeg.org/download.html y pon "
            "ffmpeg.exe (y ffprobe.exe) junto a este programa, o usa --ffmpeg RUTA."))
    raiz.mainloop()


if __name__ == "__main__":
    main()
