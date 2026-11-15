#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H68 - MicroPython: módulo sagui, tkinter y Python sin SSE2 (mejora 033,
D082-D084).

Criterio:
  1. Módulo sagui: una Ventana con rect (lleno y moldura), elipse, línea
     gruesa, polígono, texto con fondo y acentos, una Imagen BMP escalada,
     recorte y copiar_area, comprobados píxel a píxel en la captura; 10.000
     rectángulos y un cuadro de 640x480 desde Python con tiempos razonables;
     eventos de clic, soltar, tecla (también ñ), rueda, el menú contextual
     con submenú (elegido con el teclado), evento(timeout) que devuelve None
     y el X de la ventana (CERRAR).
  2. tkinter, con los ejemplos de /ejemplos/tk sin cambios, manejados con el
     ratón y el teclado (una "sonda" imprime dónde quedó cada componente):
     - calculadora (grid y Button): 7 × 6 = 42, también tecleado;
     - tareas (Entry, Listbox, Scrollbar, botones, Menu y messagebox):
       añadir con Intro y con el botón, copiar y pegar en el Entry, marcar
       una tarea, borrarla confirmando con askyesno, vaciar desde el menú
       Archivo de la barra;
     - formulario (Checkbutton, Radiobutton, Scale, Spinbox, OptionMenu,
       showerror y showinfo);
     - dibujo (Canvas con bind en B1-Motion, menú emergente con tk_popup y
       submenú de radiobuttons): trazos negros y rojos en la captura;
     - reloj (after): varios segundos distintos en la salida;
     - Text (teclear, Intro, Retroceso, flechas y get), PhotoImage con BMP y
       PNG en un Label y en un Canvas; el juego y /ejemplos/sagui_ventana.py
       corren sin errores;
     - geometría: pack (side, fill, expand, padx), grid (sticky, columnspan,
       weight) y place (relx/rely/anchor) con las posiciones esperadas; y
       redimensionar la ventana con la geometría que acompaña.
  3. Python x87: con -cpu pentium3 (sin SSE2) /apps/bin/python ejecuta la
     variante /apps/bin/python-x87 y da los mismos resultados que con SSE2
     (math.sqrt(2), 1/3, flotante.py y mandelbrot.py).
"""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, imagem_console, RAIZ, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24

# ------------------------------------------------------------ scripts em /t

SONDA = r'''
# sonda.py - executa um programa tkinter e imprime onde ficou cada componente.
import sys, tkinter

def despejar(top):
    def rec(w):
        for c in w.winfo_children():
            if isinstance(c, tkinter.Wm) or c._gestor is None:
                continue
            t = ""
            try:
                t = str(c.cget("text"))
            except Exception:
                pass
            print("sonda: w|%s|%s|%s|%s|%d|%d|%d|%d" % (top.title(), c, c.winfo_class(), t.replace("\n", " "),
                  c.winfo_rootx(), c.winfo_rooty(), c.winfo_width(), c.winfo_height()))
            rec(c)
    rec(top)
    m = top._menu_barra
    if m is not None:
        for x0, x1, i in m._posicoes:
            print("sonda: m|%s|%s|%d|%d" % (top.title(), m._itens[i]["label"], x0, x1))
    print("sonda: fim|%s|%d|%d" % (top.title(), top._wl, top._wa))

orig = tkinter.Wm._criar_janela
def criar(self):
    orig(self)
    self.after_idle(despejar, self)
tkinter.Wm._criar_janela = criar
orig_layout = tkinter.Wm._fazer_layout
def layout(self):
    novo = self._v is not None and self._geometria is not None and (self._wl, self._wa) != self._geometria
    orig_layout(self)
    if novo:
        self.after_idle(despejar, self)
tkinter.Wm._fazer_layout = layout
sys.argv.pop(0)
exec(compile(open(sys.argv[0], encoding="utf-8").read(), sys.argv[0], "exec"), {"__name__": "__main__", "__file__": sys.argv[0]})
print("sonda: salio")
'''

SAGUI_PRUEBA = r'''
import sagui, time
v = sagui.Ventana(660, 500, "Prueba sagui", redimensionable=False)
# rendimiento: 10.000 rectángulos
t = sagui.ticks_ms()
for i in range(10000):
    v.rect((i * 37) % 640, (i * 53) % 480, 12, 12, (i * 2654435761) & 0xFFFFFF)
v.actualizar()
print("sagui: 10000 rect %d ms" % (sagui.ticks_ms() - t))
# un cuadro de 640x480 calculado en Python (una franja por línea) y volcado de una vez
img = sagui.Imagen.nueva(640, 480)
t = sagui.ticks_ms()
mv = memoryview(img)
for y in range(480):
    c = (y * 255 // 479) << 8 | 0x40
    base = y * 640
    for x in range(0, 640, 4):
        mv[base + x] = c | (x * 255 // 639) << 16
        mv[base + x + 1] = c
        mv[base + x + 2] = c
        mv[base + x + 3] = c
v.imagen(0, 0, img)
v.actualizar()
print("sagui: cuadro 640x480 %d ms" % (sagui.ticks_ms() - t))
t = sagui.ticks_ms()
for k in range(20):
    v.imagen(0, 0, img)
    v.actualizar()
print("sagui: 20 volcados %d ms" % (sagui.ticks_ms() - t))
# el dibujo que se comprueba en la captura
v.limpiar(0xFFFFFF)
v.rect(10, 10, 50, 30, 0xFF0000)
v.rect(70, 10, 50, 30, (0, 0, 255), relleno=False, grosor=2)
v.elipse(130, 10, 60, 40, 0x00AA00)
v.linea(10, 60, 200, 60, 0x000000, 3)
v.poligono([(220, 10), (300, 10), (260, 50)], 0xFFAA00)
v.texto(10, 80, "Hola, señor", 0x000000, 0xFFFF00)
logo = sagui.Imagen.cargar("/imagenes/sagui.bmp")
v.imagen(10, 100, logo, ancho=64, alto=64)
v.recorte(200, 100, 40, 40)
v.rect(180, 80, 100, 100, 0x800080)
v.recorte()
v.copiar_area(10, 10, 50, 30, 250, 200)
v.pixel(5, 5, 0x123456)
v.actualizar()
print("sagui: dibujado %s %dx%d %s px=%06x" % (v.tamano(), logo.ancho, logo.alto, logo.formato, v.pixel(5, 5)))
peq = logo.escalar(32, 32)
peq.guardar("/tmp/peq.bmp")
r = sagui.Imagen.cargar("/tmp/peq.bmp")
print("sagui: guardado %dx%d %s %s" % (r.ancho, r.alto, r.formato, all(r.pixel(x, y) == peq.pixel(x, y) for x in range(0, 32, 3) for y in range(0, 32, 3))))
nulos = 0
while True:
    e = v.evento(400)
    if e is None:
        nulos += 1
        if nulos <= 2:
            print("sagui: nada en 400 ms")
        continue
    print("sagui: evento %d %d %d %d %d %d %r" % (e.tipo, e.x, e.y, e.botones, e.tecla, e.cliques, e.caracter))
    if e.tipo == sagui.CLIC and e.botones == sagui.BOTON_DERECHO:
        r = v.menu_contextual(e.x, e.y, ["Uno", "-", ("Sub", ["A", "B"]), "Fin"])
        print("sagui: menu %r" % r)
    if e.tipo == sagui.CERRAR:
        print("sagui: cerrar")
        break
v.cerrar()
'''

GEOMETRIA = r'''
import tkinter as tk
r = tk.Tk()
r.title("Geometria")
r.geometry("400x300")
a = tk.Label(r, text="arriba", bg="red")
a.pack(side="top", fill="x")
b = tk.Label(r, text="izq", bg="green", width=6)
b.pack(side="left", fill="y", padx=(4, 8))
f = tk.Frame(r, bg="white")
f.pack(side="left", fill="both", expand=True)
g1 = tk.Button(f, text="g1")
g1.grid(row=0, column=0, sticky="nsew")
g2 = tk.Button(f, text="g2")
g2.grid(row=0, column=1, sticky="w")
g3 = tk.Label(f, text="g3", bg="yellow")
g3.grid(row=1, column=0, columnspan=2, sticky="ew", pady=5)
f.columnconfigure(0, weight=1)
f.rowconfigure(2, weight=1)
p = tk.Label(r, text="centro", bg="blue", fg="white")
p.place(relx=0.5, rely=0.5, anchor="center")
q = tk.Label(r, text="se", bg="orange")
q.place(relx=1.0, rely=1.0, x=-5, y=-5, anchor="se")
def mostrar(ev=None):
    r.update_idletasks()
    for w in (a, b, f, g1, g2, g3, p, q):
        print("geo: %s %d %d %d %d" % (w.cget("text") if w is not f else "f", w.winfo_rootx(), w.winfo_rooty(), w.winfo_width(), w.winfo_height()))
    print("geo: fin %d %d" % (r.winfo_width(), r.winfo_height()))
r.after(500, mostrar)
r.bind("<Configure>", lambda e: print("geo: configure", e.width, e.height) if e.widget is r else None)
r.bind("<F5>", mostrar)
r.mainloop()
'''

EXTRAS = r'''
import tkinter as tk
r = tk.Tk()
r.title("Extras")
foto = tk.PhotoImage(file="/imagenes/sagui.bmp")
peq = foto.subsample(2)
png = tk.PhotoImage(file="/imagenes/hojas.png")
tk.Label(r, image=peq, text="BMP/2", compound="top").pack(side="left", padx=4)
c = tk.Canvas(r, width=140, height=140, bg="white")
c.pack(side="left")
c.create_image(70, 70, image=foto)
t = tk.Text(r, width=24, height=6)
t.pack(side="left", padx=4)
t.insert("1.0", "linea uno\n")
tk.Button(r, text="Leer", command=lambda: print("texto: " + t.get("1.0", "end-1c").replace("\n", "|"))).pack(side="left")
print("fotos: %d %d %d %d %s" % (foto.width(), foto.height(), peq.width(), peq.height(), png.width() > 0))
r.mainloop()
'''

SCRIPTS = {"sonda.py": SONDA, "sagui_prueba.py": SAGUI_PRUEBA, "geometria.py": GEOMETRIA, "extras.py": EXTRAS}


def imagem(console=False):
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h68-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    os.makedirs(os.path.join(raiz, "t"), exist_ok=True)
    for nome, texto in SCRIPTS.items():
        with open(os.path.join(raiz, "t", nome), "w", encoding="utf-8") as f:
            f.write(texto.lstrip())
    sgfs = os.path.join(SAIDA, "h68.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h68%s.img" % ("-console" if console else ""))
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa"] + (["--console"] if console else []),
                   check=True, stdout=subprocess.DEVNULL)
    return img


# ------------------------------------------------------------ ajudas

def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def janela(m, titulo, desde=0, tempo=30):
    g = esperar(m, rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', tempo, desde)
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO,
                fx=int(g[3]), fy=int(g[4]))


def sonda(m, titulo, desde=0, tempo=40):
    """Os componentes impressos pela sonda: {texto ou caminho: (x, y, l, a)} (relativos à janela)."""
    esperar(m, rf"sonda: fim\|{re.escape(titulo)}\|", tempo, desde)
    s = m.serial()[desde:]
    ultimo = s.rfind("sonda: fim|%s|" % titulo)
    ini = s.rfind("sonda: fim|", 0, ultimo)
    s = s[ini + 1 if ini >= 0 else 0:ultimo]
    comps, menus = {}, {}
    for linha in s.splitlines():
        p = linha.strip().split("|")
        if p[0] == "sonda: w" and p[1] == titulo and len(p) >= 9:
            caixa = tuple(int(v) for v in p[5:9])
            comps[p[2]] = (p[3],) + caixa
            if p[4] and p[4] not in comps:
                comps[p[4]] = (p[3],) + caixa
        elif p[0] == "sonda: m" and p[1] == titulo:
            menus[p[2]] = (int(p[3]), int(p[4]))
    return comps, menus


def clicar_em(m, j, caixa, dx=None, dy=None, botao=1, duplo=False):
    _, x, y, l, a = caixa
    m.clique(j["x"] + x + (l // 2 if dx is None else dx), j["y"] + y + (a // 2 if dy is None else dy), botao=botao, duplo=duplo)


def fechar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def captura(m, nome):
    ppm = os.path.join(SAIDA, f"h68-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    return ler_ppm(ppm)


def cor(tela, x, y):
    l, a, px = tela
    i = (y * l + x) * 3
    return (px[i] << 16) | (px[i + 1] << 8) | px[i + 2]


_GLIFOS = None


def glifos():
    global _GLIFOS
    if _GLIFOS is None:
        src = open(os.path.join(RAIZ, "build", "gerado", "fonte8x16.c")).read()
        linhas = [l for l in src.splitlines() if l.strip().startswith("{")]
        _GLIFOS = [bytes(int(v, 16) for v in re.findall(r"0x([0-9A-F]{2})", l.split("/*")[0])) for l in linhas]
    return _GLIFOS


def tem_texto(tela, regiao, texto):
    """O texto (fonte 8x16 do sistema, uma cor) aparece em algum lugar da região?"""
    l, a, px = tela
    gs = [glifos()[b] for b in texto.encode("latin-1")]
    x0, y0, x1, y1 = regiao
    x1, y1 = min(x1, l - 8 * len(gs)), min(y1, a - 16)
    lin0 = next(r for r in range(16) if gs[0][r])
    col0 = next(c for c in range(8) if gs[0][lin0] & (0x80 >> c))
    for y in range(max(0, y0), y1):
        for x in range(max(0, x0), x1):
            i = ((y + lin0) * l + x + col0) * 3
            frente = px[i:i + 3]
            ok = True
            for k, g in enumerate(gs):
                for r in range(16):
                    base = ((y + r) * l + x + 8 * k) * 3
                    bits = g[r]
                    for c in range(8):
                        igual = px[base + 3 * c:base + 3 * c + 3] == frente
                        if igual != bool(bits & (0x80 >> c)):
                            ok = False
                            break
                    if not ok:
                        break
                if not ok:
                    break
            if ok:
                return True
    return False


def regiao(j, extra=0):
    return (j["x"] - extra, j["y"] - extra, j["x"] + j["l"] + extra, j["y"] + j["a"] + extra)


def saida(m, desde):
    return m.serial()[desde:].replace("\r", "")


# ------------------------------------------------------------ 1. módulo sagui

def parte_sagui(m):
    ini = len(m.serial())
    m.digitar("python /t/sagui_prueba.py\n", 0.03)
    j = janela(m, "Prueba sagui", ini, 60)
    esperar(m, r"sagui: dibujado", 120, ini)
    s = saida(m, ini)
    t_rect = int(re.search(r"10000 rect (\d+) ms", s).group(1))
    t_cuadro = int(re.search(r"cuadro 640x480 (\d+) ms", s).group(1))
    t_volcado = int(re.search(r"20 volcados (\d+) ms", s).group(1))
    print(f"  medidas: 10.000 rect {t_rect} ms, cuadro 640x480 en Python {t_cuadro} ms, 20 volcados {t_volcado} ms")
    verificar(t_rect < 8000 and t_cuadro < 30000 and t_volcado < 8000,
              f"rendimiento razonable en QEMU (10.000 rect {t_rect} ms, 640x480 {t_cuadro} ms)")
    verificar("(660, 500) 128x128 BMP 8 bits px=123456" in s, "tamano(), Imagen.cargar (BMP 128x128), pixel()")
    esperar(m, r"sagui: guardado", 20, ini)
    verificar("sagui: guardado 32x32 BMP 24 bits True" in saida(m, ini), "Imagen.escalar, guardar (BMP) y cargar de nuevo")
    time.sleep(0.5)
    tela = captura(m, "sagui")
    x, y = j["x"], j["y"]
    pontos = {(10 + 25, 10 + 15): 0xFF0000, (70, 10 + 15): 0x0000FF, (71, 20): 0x0000FF, (75, 20): 0xFFFFFF,
              (160, 30): 0x00AA00, (131, 11): 0xFFFFFF, (100, 60): 0x000000, (100, 62): 0xFFFFFF,
              (260, 20): 0xFFAA00, (290, 45): 0xFFFFFF, (199, 120): 0xFFFFFF, (205, 120): 0x800080,
              (239, 139): 0x800080, (241, 120): 0xFFFFFF, (275, 215): 0xFF0000, (5, 5): 0x123456,
              (12, 82): 0xFFFF00}
    errados = [(p, f"{cor(tela, x + p[0], y + p[1]):06x}", f"{c:06x}") for p, c in pontos.items() if cor(tela, x + p[0], y + p[1]) != c]
    verificar(not errados, f"rect, moldura, elipse, línea gruesa, polígono, recorte, copiar_area y pixel en la captura {errados}")
    verificar(tem_texto(tela, (x + 5, y + 75, x + 120, y + 100), "Hola, señor"), "texto con fondo y acentos (fuente 8x16) en la captura")
    logo = [cor(tela, x + 10 + k, y + 100 + k) for k in range(0, 64, 8)]
    verificar(len(set(logo)) > 2 and 0xFFFFFF in [cor(tela, x + 80, y + 120)], "Imagen escalada a 64x64 dibujada")
    m.captura("H68-sagui")

    # eventos
    ini = len(m.serial())
    esperar(m, r"sagui: nada en 400 ms", 20, 0)
    m.clique(x + 50, y + 150)
    esperar(m, r"sagui: evento 1 50 150 1 0 1", 10, ini)
    esperar(m, r"sagui: evento 2 50 150", 10, ini)
    verificar(True, "evento CLIC y SOLTAR con x, y y botones")
    m.tecla("a")
    m.tecla("semicolon")
    esperar(m, r"sagui: evento 4 \d+ \d+ 0 97 \d+ 'a'", 10, ini)
    esperar(m, r"sagui: evento 4 \d+ \d+ 0 241 \d+ '\\xf1'", 10, ini)
    verificar(True, "evento TECLA con el código y el carácter (también ñ)")
    m.mouse_mover(x + 100, y + 160)
    esperar(m, r"sagui: evento 3 100 160", 10, ini)
    m.monitor("mouse_move 0 0 1")
    esperar(m, r"sagui: evento 11 100 160 0 0 -?1 ", 10, ini)
    verificar(True, "eventos MOVER y RUEDA")
    m.clique(x + 120, y + 170, botao=2)
    esperar(m, r"\[GUI\] menu aberto", 10, ini)
    time.sleep(0.4)
    m.captura("H68-sagui-menu")
    for t in ("down", "down", "right", "down", "ret"):
        m.tecla(t, 0.3)
    r = esperar(m, r"sagui: menu (\S+)", 10, ini)
    verificar(r == "4", f"menu_contextual con submenú: elegido Sub > B (índice {r})")
    fechar(m, j)
    esperar(m, r"sagui: cerrar", 10, ini)
    verificar(True, "evento(timeout) devuelve None y el X de la ventana llega como CERRAR")
    m.esperar_serial("(python) terminou com codigo 0", 20)


# ------------------------------------------------------------ 2. tkinter

def abrir(m, programa, titulo):
    ini = len(m.serial())
    m.digitar(f"python /t/sonda.py {programa}\n", 0.03)
    j = janela(m, titulo, ini, 60)
    comps, menus = sonda(m, titulo, ini, 60)
    time.sleep(0.5)
    return ini, j, comps, menus


def parte_calculadora(m):
    ini, j, c, _ = abrir(m, "/ejemplos/tk/calculadora.py", "Calculadora tk")
    for t in ("7", "×", "6", "="):
        clicar_em(m, j, c[t])
    esperar(m, r"calculadora: 42", 15, ini)
    time.sleep(0.5)
    tela = captura(m, "calc")
    verificar(tem_texto(tela, regiao(j), "42"), "calculadora: 7 × 6 = 42 con el ratón (grid y Button), visible en la pantalla")
    m.captura("H68-tk-calculadora")
    m.digitar("12+30\n", 0.1)
    esperar(m, r"calculadora: 12\r?\ncalculadora: 42", 15, ini)
    verificar(True, "calculadora: 12+30 Intro con el teclado (bind <Key>)")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)


def parte_tareas(m):
    m.comando("rm -f /tmp/tareas.txt")
    ini, j, c, menus = abrir(m, "/ejemplos/tk/tareas.py", "Tareas")
    entrada = next(v for k, v in c.items() if v[0] == "Entry")
    lista = next(v for k, v in c.items() if v[0] == "Listbox")
    clicar_em(m, j, entrada)
    m.digitar("Comprar pan", 0.06)
    m.tecla("ret")
    esperar(m, r"tareas: añadida Comprar pan", 15, ini)
    m.digitar("Regar el jardín", 0.06)
    clicar_em(m, j, c["Añadir"])
    esperar(m, r"tareas: añadida Regar el jardín", 15, ini)
    m.digitar("Llamar", 0.06)
    m.tecla("ctrl-a")
    m.tecla("ctrl-c")
    m.tecla("ret")
    esperar(m, r"tareas: añadida Llamar", 15, ini)
    m.tecla("ctrl-v")
    m.digitar(" otra vez", 0.06)
    m.tecla("ret")
    esperar(m, r"tareas: añadida Llamar otra vez", 15, ini)
    verificar(True, "Entry: Intro, botón Añadir, Ctrl+A/Ctrl+C/Ctrl+V (portapapeles)")
    time.sleep(0.5)
    tela = captura(m, "tareas")
    verificar(tem_texto(tela, regiao(j), "[ ] Regar el jard\xedn") and tem_texto(tela, regiao(j), "4 tareas, 0 hechas"),
              "Listbox y Label en la captura")
    clicar_em(m, j, lista, dy=2 + 18 + 9)   # segunda línea
    clicar_em(m, j, c["Hecha"])
    esperar(m, r"tareas: marcada \[x\] Regar el jardín", 15, ini)
    verificar(True, "Listbox: seleccionar con el ratón y marcar con el botón")
    clicar_em(m, j, lista, dy=2 + 9)
    ini2 = len(m.serial())
    clicar_em(m, j, c["Borrar"])
    d = janela(m, "Borrar", ini2, 20)
    dc, _ = sonda(m, "Borrar", ini2, 20)
    time.sleep(0.5)
    tela = captura(m, "tareas-borrar")
    verificar(tem_texto(tela, regiao(d), "\xbfBorrar \xabComprar pan\xbb?"), "messagebox.askyesno con el texto")
    m.captura("H68-tk-askyesno")
    clicar_em(m, d, dc["Sí"])
    esperar(m, r"tareas: borrada Comprar pan", 15, ini)
    verificar(True, "askyesno: «Sí» confirma el borrado")
    # menú de la barra: Archivo > Vaciar la lista, y askokcancel con Intro
    x0, x1 = menus["Archivo"]
    ini3 = len(m.serial())
    m.clique(j["x"] + (x0 + x1) // 2, j["y"] + 10)
    esperar(m, r"\[GUI\] menu aberto", 10, ini3)
    time.sleep(0.4)
    m.captura("H68-tk-menu")
    m.tecla("down", 0.3)
    m.tecla("ret", 0.3)
    janela(m, "Vaciar", ini3, 20)
    sonda(m, "Vaciar", ini3, 20)
    m.tecla("ret")
    esperar(m, r"tareas: lista vaciada", 15, ini)
    verificar(True, "Menu de la barra: Archivo > Vaciar la lista, askokcancel aceptado con Intro")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)
    time.sleep(0.5)
    s = m.comando("cat /tmp/tareas.txt; echo fin-tareas")
    verificar("Regar" not in s and "fin-tareas" in s, "la lista vacía se guardó en /tmp/tareas.txt")


def parte_formulario(m):
    ini, j, c, _ = abrir(m, "/ejemplos/tk/formulario.py", "Formulario")
    ini2 = len(m.serial())
    clicar_em(m, j, c["Enviar"])
    d = janela(m, "Formulario", ini2, 20)
    time.sleep(0.8)
    tela = captura(m, "form-error")
    verificar(tem_texto(tela, regiao(d), "Falta el nombre."), "showerror sin nombre")
    m.tecla("ret")
    time.sleep(0.8)
    entrada = next(v for k, v in c.items() if v[0] == "Entry")
    clicar_em(m, j, entrada)
    m.digitar("Tito", 0.06)
    clicar_em(m, j, c["C"], dx=6)
    clicar_em(m, j, c["Acepto las condiciones"], dx=6)
    clicar_em(m, j, c["Recibir el boletín"], dx=6)
    escala = next(v for k, v in c.items() if v[0] == "Scale")
    clicar_em(m, j, escala, dx=escala[3] - 6)     # en el carril, a la derecha del cursor: +1
    clicar_em(m, j, escala, dx=escala[3] - 6)
    spin = next(v for k, v in c.items() if v[0] == "Spinbox")
    clicar_em(m, j, spin, dx=spin[3] - 6, dy=4)   # flecha de arriba
    clicar_em(m, j, spin, dx=spin[3] - 6, dy=4)
    time.sleep(0.5)
    m.captura("H68-tk-formulario")
    ini3 = len(m.serial())
    clicar_em(m, j, c["Enviar"])
    r = esperar(m, r"formulario: (.*)", 15, ini3)
    verificar("Tito, 3 años" in r and "Lenguaje: C" in r and "Boletín: sí" in r and "Nivel: 7" in r and "Sistema: SaguiOS" in r,
              f"Entry, Spinbox, Radiobutton, Checkbutton, Scale y OptionMenu: {r.strip()}")
    d = janela(m, "Enviado", ini3, 20)
    time.sleep(0.8)
    tela = captura(m, "form-info")
    verificar(tem_texto(tela, regiao(d), "Lenguaje: C"), "showinfo con el resumen")
    m.captura("H68-tk-showinfo")
    m.tecla("ret")
    time.sleep(0.5)
    clicar_em(m, j, c["Salir"])
    m.esperar_serial("sonda: salio", 20)


def parte_dibujo(m):
    ini, j, c, _ = abrir(m, "/ejemplos/tk/dibujo.py", "Dibujo")
    lienzo = next(v for k, v in c.items() if v[0] == "Canvas")
    _, lx, ly, ll, la = lienzo
    x0, y0 = j["x"] + lx, j["y"] + ly
    m.arrastar(x0 + 40, y0 + 60, x0 + 200, y0 + 60)
    esperar(m, r"dibujo: trazo 1 terminado en \(200, 60\) con black", 15, ini)
    # menú emergente: Color > red
    m.clique(x0 + 300, y0 + 200, botao=3 - 1)
    esperar(m, r"\[GUI\] menu aberto", 10, ini)
    for t in ("down", "right", "down", "ret"):
        m.tecla(t, 0.3)
    time.sleep(0.5)
    m.arrastar(x0 + 40, y0 + 120, x0 + 200, y0 + 160)
    esperar(m, r"dibujo: trazo 2 terminado en \(200, 160\) con red", 15, ini)
    verificar(True, "Canvas: bind de Button-1, B1-Motion y ButtonRelease; tk_popup con submenú de radiobuttons")
    time.sleep(0.5)
    tela = captura(m, "dibujo")
    negro = sum(1 for x in range(60, 190, 10) if cor(tela, x0 + x, y0 + 60) == 0x000000)
    # el ratón va en pasos de 40 en diagonal y después en horizontal: de (40,120) a (80,160) y a (200,160)
    rojo = sum(1 for x in range(90, 200, 10) if cor(tela, x0 + x, y0 + 160) == 0xFF0000)
    verificar(negro >= 12 and rojo >= 10, f"trazos en la captura: {negro} puntos negros y {rojo} rojos")
    m.captura("H68-tk-dibujo")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)


def parte_reloj(m):
    ini, j, c, _ = abrir(m, "/ejemplos/tk/reloj.py", "Reloj")
    time.sleep(4)
    horas = set(re.findall(r"reloj: (\d\d:\d\d:\d\d)", saida(m, ini)))
    verificar(len(horas) >= 3, f"after(): el reloj avanza ({len(horas)} segundos distintos)")
    m.captura("H68-tk-reloj")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)


def parte_geometria(m):
    ini = len(m.serial())
    m.digitar("python /t/geometria.py\n", 0.03)
    j = janela(m, "Geometria", ini, 60)
    esperar(m, r"geo: fin", 30, ini)
    g = dict((l.split()[1], tuple(int(v) for v in l.split()[2:6])) for l in re.findall(r"geo: (?!fin|configure).*", saida(m, ini)))
    verificar(j["l"] == 400 and j["a"] == 300, "geometry('400x300')")
    verificar(g["arriba"] == (0, 0, 400, 22), f"pack side=top fill=x: {g['arriba']}")
    verificar(g["izq"][0] == 4 and g["izq"][1] == 22 and g["izq"][3] == 278, f"pack side=left fill=y padx=(4, 8): {g['izq']}")
    fx = 4 + g["izq"][2] + 8
    verificar(g["f"] == (fx, 22, 400 - fx, 278), f"pack fill=both expand=True: {g['f']}")
    verificar(g["g1"][0] == fx and g["g1"][2] == 400 - fx - g["g2"][2], f"grid weight y sticky=nsew: {g['g1']}")
    verificar(g["g3"][2] == 400 - fx and g["g3"][1] == 22 + g["g1"][3] + 5, f"grid columnspan=2 sticky=ew pady=5: {g['g3']}")
    p = g["centro"]
    verificar(abs(p[0] + p[2] / 2 - 200) <= 1 and abs(p[1] + p[3] / 2 - 150) <= 1, f"place relx/rely 0.5 anchor=center: {p}")
    q = g["se"]
    verificar(q[0] + q[2] == 395 and q[1] + q[3] == 295, f"place relx/rely 1 anchor=se con x/y: {q}")
    time.sleep(0.5)
    m.captura("H68-tk-geometria")
    # redimensionar arrastrando la esquina: el layout acompaña
    ini2 = len(m.serial())
    cx, cy = j["fx"] + j["l"] + 2 * BORDA - 6, j["fy"] + j["a"] + TITULO + BORDA - 6   # la esquina (como en h42)
    m.arrastar(cx, cy, cx + 100, cy + 60)
    esperar(m, r"geo: configure (\d+) (\d+)", 15, ini2)
    m.tecla("f5")
    esperar(m, r"geo: fin (5\d\d) (3\d\d)", 15, ini2)
    g2 = dict((l.split()[1], tuple(int(v) for v in l.split()[2:6])) for l in re.findall(r"geo: (?!fin|configure).*", saida(m, ini2)))
    lw, lh = (int(v) for v in re.findall(r"geo: fin (\d+) (\d+)", saida(m, ini2))[-1])
    verificar(g2["arriba"][2] == lw and g2["f"][2] == lw - fx and abs(g2["centro"][0] + g2["centro"][2] / 2 - lw / 2) <= 1,
              f"redimensionada a {lw}x{lh}: <Configure> y el layout se rehace")
    fechar(m, dict(j, l=lw, a=lh))
    m.esperar_serial("(python) terminou com codigo 0", 20)


def parte_extras(m):
    ini, j, c, _ = abrir(m, "/t/extras.py", "Extras")
    verificar("fotos: 128 128 64 64 True" in saida(m, ini), "PhotoImage(file=) con BMP y PNG, subsample")
    texto = next(v for k, v in c.items() if v[0] == "Text")
    clicar_em(m, j, texto, dx=texto[3] - 10, dy=texto[4] - 10)   # después del final
    m.digitar("segunda linea", 0.06)
    m.tecla("ret")
    m.digitar("tercera", 0.06)
    m.tecla("backspace")
    m.tecla("left")
    m.digitar("X", 0.06)
    clicar_em(m, j, c["Leer"])
    r = esperar(m, r"texto: (.*)", 15, ini).strip()
    verificar(r == "linea uno|segunda linea|terceXr", f"Text: clic, teclear, Intro, Retroceso y flechas ({r})")
    time.sleep(0.5)
    tela = captura(m, "extras")
    canvas = next(v for k, v in c.items() if v[0] == "Canvas")
    cores = set(cor(tela, j["x"] + canvas[1] + 70 + d, j["y"] + canvas[2] + 70 + d) for d in range(-40, 40, 4))
    rot = next(v for k, v in c.items() if v[0] == "Label")
    cores2 = set(cor(tela, j["x"] + rot[1] + rot[3] // 2 + d, j["y"] + rot[2] + 10 + d) for d in range(0, 40, 4))
    verificar(len(cores) > 3 and len(cores2) > 3, f"imágenes en el Canvas y en el Label ({len(cores)} y {len(cores2)} colores)")
    verificar(tem_texto(tela, regiao(j), "segunda linea"), "Text en la captura")
    m.captura("H68-tk-imagen-texto")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)


def parte_juego(m):
    ini, j, c, _ = abrir(m, "/ejemplos/tk/juego.py", "Lluvia de bananas")
    for _ in range(6):
        m.tecla("left", 0.15)
    for _ in range(3):
        m.tecla("right", 0.15)
    time.sleep(3)
    m.captura("H68-tk-juego")
    s = saida(m, ini)
    verificar("Traceback" not in s and "Exception" not in s, "el juego del Canvas corre unos segundos sin errores")
    fechar(m, j)
    m.esperar_serial("sonda: salio", 20)
    ini = len(m.serial())
    m.digitar("python /ejemplos/sagui_ventana.py\n", 0.03)
    j = janela(m, "sagui: ventana de ejemplo", ini, 60)
    time.sleep(1.5)
    m.arrastar(j["x"] + 100, j["y"] + 120, j["x"] + 220, j["y"] + 200)
    m.tecla("q")
    m.esperar_serial("(python) terminou com codigo 0", 20)
    verificar("Traceback" not in saida(m, ini), "/ejemplos/sagui_ventana.py: dibujar arrastrando y salir con q")


# ------------------------------------------------------------ 3. x87

def parte_x87():
    img = imagem(console=True)
    saidas = {}
    for cpu in ("max", "pentium3"):
        with Maquina("h68-" + cpu, "ide", imagem=img, extras=["-cpu", cpu]) as m:
            m.esperar_serial("sagui:/$", 120)
            # D216: el supervisor arranca cron justo después del prompt; su línea del log
            # se colaba en el eco de la orden tecleada en un sitio distinto en cada CPU
            for _ in range(50):
                if "executar /bin/cron" in m.serial():
                    break
                time.sleep(0.2)
            time.sleep(0.5)
            r = []
            r.append(m.comando("python -c 'import math; print(\"vals\", math.sqrt(2.0), 1/3, math.sin(1.0), math.exp(1), round(2.675, 2), 0.1 + 0.2)'"))
            r.append(m.comando("python /ejemplos/mandelbrot.py", tempo=120))
            r.append(m.comando("python /ejemplos/flotante.py", tempo=120))
            saidas[cpu] = r
            if cpu == "pentium3":
                verificar("executar /apps/bin/python-x87" in m.serial(), "pentium3: /apps/bin/python ejecuta /apps/bin/python-x87")
                m.captura("H68-python-x87")

    def limpar(t):
        return [l for l in t.splitlines() if not l.startswith("[") and " ms" not in l and "python" not in l]
    v = re.search(r"vals .*", saidas["pentium3"][0]).group(0)
    verificar(v == "vals 1.414213562373095 0.3333333333333333 0.8414709848078965 2.718281828459046 2.68 0.3",
              f"pentium3: {v}")
    for k, nome in enumerate(("print", "mandelbrot.py", "flotante.py")):
        a, b = limpar(saidas["max"][k]), limpar(saidas["pentium3"][k])
        verificar(a == b and len(a) >= 1, f"pentium3 (x87) = SSE2 en {nome} ({len(a)} líneas)")


def main():
    img = imagem()
    with Maquina("h68", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        parte_sagui(m)
        parte_calculadora(m)
        parte_tareas(m)
        parte_formulario(m)
        parte_dibujo(m)
        parte_reloj(m)
        parte_geometria(m)
        parte_extras(m)
        parte_juego(m)
    parte_x87()
    return 0


if __name__ == "__main__":
    sys.exit(main())
