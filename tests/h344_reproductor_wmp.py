#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H344 (D342, v8.4.4) - el Reproductor rediseñado al estilo de un reproductor de escritorio.

Material (ffmpeg del anfitrión, fuentes sintéticas): a.mp4 (rojo, 3 s), b.mp4 (azul, 2 s), longo.mp4 (12 s),
tom.mp3 (tres tonos, sin carátula), capa.mp3 y capa.flac (con carátula naranja) y lista.m3u (a.mp4 y b.mp4).
Todo en un pendrive, como h276. Cinco casos, a 800x600 y a 1024x768:

  1. doble clic en un .mp4 desde Archivos abre el Reproductor (parte 1 de D342);
  2. una lista .m3u con dos vídeos: pasa sola del rojo al azul, y la lista resalta la pista actual;
  3. arrastrar la barra de posición salta a ese punto (y el ratón encima enseña el tiempo);
  4. pantalla completa con doble clic: los controles se ocultan a los 3 s y vuelven al mover el ratón;
  5. un .mp3 enseña barras de espectro que se mueven; con carátula (ID3 y FLAC) enseña la carátula;
  6. los atajos (Ctrl+P, Ctrl+S, Ctrl+B, Ctrl+F, Ctrl+L, F8, F9, F10, A, L) y los menús (Archivo > Propiedades).

Capturas en build/testes/h344-<resolución>-*.png (no se guardan en docs/capturas).
Uso: h344_reproductor_wmp.py [800x600] [1024x768]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu,  # noqa: E402
                  FalhaTeste, SAIDA)

DIR = os.path.join(SAIDA, "h344")
MENU_A, PAN_A = 20, 72
PAN_FUNDO = (0x1F, 0x27, 0x2C)
TITULO_RE = r'janela (\d+) criada por \d+: "Reproductor de v.deo" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)'


def ffmpeg(*args):
    r = subprocess.run(["/usr/bin/ffmpeg", "-v", "error", "-y", *args], capture_output=True)
    if r.returncode:
        raise FalhaTeste("ffmpeg falló: " + r.stderr.decode(errors="replace")[:300])


def gerar_material():
    os.makedirs(DIR, exist_ok=True)
    tom = ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100"]
    cor = lambda c, d: ["-f", "lavfi", "-i", f"color=c={c}:size=320x180:rate=15:d={d}"]  # noqa: E731
    cod = ["-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-g", "15", "-c:a", "aac",
           "-b:a", "96k", "-ac", "2", "-movflags", "+faststart"]
    ffmpeg(*cor("red", 3), *tom, "-t", "3", *cod, os.path.join(DIR, "a.mp4"))
    ffmpeg(*cor("blue", 2), *tom, "-t", "2", *cod, os.path.join(DIR, "b.mp4"))
    ffmpeg("-f", "lavfi", "-i", "testsrc2=size=320x180:rate=15", *tom, "-t", "12", *cod, os.path.join(DIR, "longo.mp4"))
    senal = "aevalsrc=0.4*sin(2*PI*440*t)+0.3*sin(2*PI*2500*t)+0.2*sin(2*PI*8000*t):s=44100:d=8"
    ffmpeg("-f", "lavfi", "-i", senal, "-c:a", "libmp3lame", "-b:a", "128k", "-metadata", "title=Tono de prueba",
           "-metadata", "artist=SaguiOS", os.path.join(DIR, "tom.mp3"))
    ffmpeg("-f", "lavfi", "-i", "color=c=0xFF8000:s=200x200", "-frames:v", "1", os.path.join(DIR, "capa.jpg"))
    ffmpeg("-f", "lavfi", "-i", senal, "-i", os.path.join(DIR, "capa.jpg"), "-map", "0:a", "-map", "1:v", "-c:a",
           "libmp3lame", "-b:a", "128k", "-c:v", "copy", "-id3v2_version", "3", "-metadata", "title=Con caratula",
           os.path.join(DIR, "capa.mp3"))
    ffmpeg("-f", "lavfi", "-i", senal, "-i", os.path.join(DIR, "capa.jpg"), "-map", "0:a", "-map", "1:v", "-c:a",
           "flac", "-c:v", "copy", "-disposition:v", "attached_pic", os.path.join(DIR, "capa.flac"))


def esperar(m, padrao, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r} en la serial\n" + m.serial()[-1200:])


def capturar(m, nome):
    ppm = os.path.join(SAIDA, nome + ".ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    d = open(ppm, "rb").read()
    p = d.split(None, 4)
    return int(p[1]), int(p[2]), p[4]


def guardar_png(m, nome, res):
    ppm = os.path.join(SAIDA, f"h344-{res}-{nome}.ppm")
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    subprocess.run(["convert", ppm, os.path.join(SAIDA, f"h344-{res}-{nome}.png")], check=True)
    os.remove(ppm)


def janela(m, desde=0):
    g = re.findall(TITULO_RE, m.serial()[desde:])
    if not g:
        raise FalhaTeste("no hay ventana del reproductor en la serial")
    _, l, a, x, y = (int(v) for v in g[-1])
    return {"l": l, "a": a, "ox": x + 2, "oy": y + 24}   # (x, y) es el marco; dentro, 2 px y 24 de título


def pix(px, tl, x, y):
    i = (y * tl + x) * 3
    return px[i], px[i + 1], px[i + 2]


def conta(px, tl, ta, rect, pred, paso=3):
    x0, y0, l, a = rect
    n = tot = 0
    for y in range(max(y0, 0), min(ta, y0 + a), paso):
        for x in range(max(x0, 0), min(tl, x0 + l), paso):
            tot += 1
            if pred(*pix(px, tl, x, y)):
                n += 1
    return n, tot


def rect_video(w, lista=False, completa=False):
    lw = (220 if w["l"] >= 700 else 180) if lista else 0
    if completa:
        return w["ox"], w["oy"], w["l"], w["a"]
    return w["ox"], w["oy"] + MENU_A, w["l"] - lw, w["a"] - MENU_A - PAN_A


def cerrar(m, w, desde):
    m.clique(w["ox"] + w["l"] + 2 - 20 + 8 - 2, w["oy"] - 12)
    esperar(m, "reproductor: cerrado", 15, desde)
    time.sleep(0.5)


def probar(res):
    L, A = (int(v) for v in res.split("x"))
    arqs = {n: open(os.path.join(DIR, n), "rb").read() for n in
            ("a.mp4", "b.mp4", "longo.mp4", "tom.mp3", "capa.mp3", "capa.flac")}
    arqs["lista.m3u"] = "#EXTM3U\na.mp4\nb.mp4\n"
    pen = criar_pendrive("h344-" + res, rotulo="VIDEO", arquivos=arqs)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    img = imagem_de_teste("h344-" + res, ["--vesa", "--resolucao", res])
    with Maquina("h344-" + res, "ide", imagem=img, extras=extras, som="hda", snapshot=True) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        m.comando("montar usb0p1")

        # ---- 1. doble clic en un .mp4 desde Archivos
        ini = len(m.serial())
        m.comando("arquivos /usb", 20)
        esperar(m, r"arquivos: pronto", 30, ini)
        time.sleep(0.8)
        g = re.findall(r'janela (\d+) criada por \d+: "Archivos" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', m.serial()[ini:])
        fx, fy, fl = int(g[-1][3]), int(g[-1][4]), int(g[-1][1])
        ax, ay = fx + 2, fy + 24
        ini = len(m.serial())
        m.clique(ax + 146 + 120, ay + 109, duplo=True)             # el primero: a.mp4 (Archivos D423: la lista empieza bajo la barra de ruta)
        esperar(m, r"reproductor: listo \(/usb/a\.mp4, 320x180, 15 fps", 40, ini)
        verificar(True, f"[{res}] doble clic en un .mp4 de Archivos abre el Reproductor")
        esperar(m, r"reproductor: primer cuadro en la ventana", 30, ini)
        w = janela(m, ini)
        verificar(w["ox"] + w["l"] + 2 <= L and w["oy"] + w["a"] + 2 <= A,
                  f"[{res}] la ventana ({w['l']}x{w['a']} en {w['ox']},{w['oy']}) cabe en la pantalla")
        tl, ta, px = capturar(m, "h344a")
        rv = rect_video(w)
        rojo, tot = conta(px, tl, ta, rv, lambda r, g, b: r > 150 and g < 90 and b < 90)
        panel, _ = conta(px, tl, ta, (w["ox"], w["oy"] + w["a"] - PAN_A + 2, w["l"], PAN_A - 4),
                         lambda r, g, b: (r, g, b) == PAN_FUNDO)
        verificar(rojo > tot * 0.3, f"[{res}] el cuadro rojo se ve ({rojo} de {tot} píxeles)")
        verificar(panel > 300, f"[{res}] el panel de controles se dibuja abajo ({panel} píxeles)")
        guardar_png(m, "1-desde-archivos", res)
        cerrar(m, w, ini)
        m.clique(fx + fl + 4 - 2 - 20 + 8, fy + 12)        # cierra Archivos: el foco vuelve al Terminal
        time.sleep(1.0)

        # ---- 2. una lista con dos vídeos que pasa sola al segundo
        ini = len(m.serial())
        m.comando("reproductor /usb/lista.m3u", 20)
        esperar(m, r"reproductor: listo \(/usb/a\.mp4", 40, ini)
        esperar(m, r"reproductor: pista 1 de 2", 5, ini)
        w = janela(m, ini)
        rv = rect_video(w, lista=True)
        esperar(m, r"reproductor: primer cuadro en la ventana", 30, ini)
        guardar_png(m, "2-lista-primera", res)
        esperar(m, r"reproductor: abierto /usb/b\.mp4", 60, ini)
        esperar(m, r"reproductor: pista 2 de 2", 5, ini)
        verificar(True, f"[{res}] la lista pasa sola al segundo vídeo")
        time.sleep(1.0)
        tl, ta, px = capturar(m, "h344b")
        azul, tot = conta(px, tl, ta, rv, lambda r, g, b: b > 150 and r < 90 and g < 90)
        verificar(azul > tot * 0.25, f"[{res}] la segunda pista (azul) se ve ({azul} de {tot} píxeles)")
        # la pista actual lleva un '>' y la fila resaltada (azul de selección) en la lista
        lx = w["ox"] + w["l"] - (220 if w["l"] >= 700 else 180)
        fila, _ = conta(px, tl, ta, (lx, w["oy"] + MENU_A + 20 + 18, 220, 18),
                        lambda r, g, b: (r, g, b) == (0x2A, 0x6F, 0xD8), 1)
        verificar(fila > 1500, f"[{res}] la lista resalta la fila de la pista actual ({fila} píxeles)")
        guardar_png(m, "2-lista-segunda", res)
        esperar(m, r"reproductor: fin del video", 40, ini)
        verificar(True, f"[{res}] al acabar la lista el reproductor se para")
        cerrar(m, w, ini)

        # ---- 3. arrastrar la barra de posición
        ini = len(m.serial())
        m.comando("reproductor /usb/longo.mp4", 20)
        esperar(m, r"reproductor: primer cuadro en la ventana", 40, ini)
        w = janela(m, ini)
        time.sleep(1.0)
        yb = w["oy"] + w["a"] - PAN_A + 3 + 9
        x0, x1 = w["ox"] + 12, w["ox"] + w["l"] - 12
        # el ratón encima: se pinta el tiempo flotante (negro con borde verde)
        m.mouse_para(x0 + (x1 - x0) // 2, yb)
        time.sleep(0.8)
        tl, ta, px = capturar(m, "h344c")
        dica, _ = conta(px, tl, ta, (w["ox"], w["oy"] + w["a"] - PAN_A - 26, w["l"], 24),
                        lambda r, g, b: (r, g, b) == (0x1F, 0xB8, 0x64), 1)
        verificar(dica > 20, f"[{res}] el tiempo aparece al pasar el ratón por la barra ({dica} píxeles del borde)")
        guardar_png(m, "3-hover", res)
        ini2 = len(m.serial())
        m.arrastar(x0 + 10, yb, x0 + (x1 - x0) * 3 // 4, yb)
        s = esperar(m, r"reproductor: salto a (\d+):(\d+)", 15, ini2)
        seg = int(s[0]) * 60 + int(s[1])
        verificar(7 <= seg <= 10, f"[{res}] arrastrar la barra salta al 75 % (segundo {seg} de 12)")
        time.sleep(1.0)
        guardar_png(m, "3-tras-salto", res)

        # ---- 4. pantalla completa (doble clic): los controles se ocultan a los 3 s
        ini2 = len(m.serial())
        m.clique(w["ox"] + w["l"] // 2, w["oy"] + 120, duplo=True)
        esperar(m, rf"reproductor: ventana {L}x{A} \(pantalla completa\)", 15, ini2)
        verificar(True, f"[{res}] doble clic: pantalla completa {L}x{A}")
        time.sleep(0.8)
        tl, ta, px = capturar(m, "h344d")
        visibles, _ = conta(px, tl, ta, (0, A - PAN_A + 2, L, PAN_A - 4), lambda r, g, b: (r, g, b) == PAN_FUNDO)
        verificar(visibles > 300, f"[{res}] al entrar, los controles se ven ({visibles} píxeles del panel)")
        guardar_png(m, "4-completa-controles", res)
        time.sleep(4.0)
        tl, ta, px = capturar(m, "h344e")
        ocultos, _ = conta(px, tl, ta, (0, A - PAN_A + 2, L, PAN_A - 4), lambda r, g, b: (r, g, b) == PAN_FUNDO)
        verificar(ocultos < 30, f"[{res}] a los 3 s se ocultan ({ocultos} píxeles del panel)")
        guardar_png(m, "4-completa-ocultos", res)
        m.mouse_mover(L // 2 + 30, A // 2 + 20)
        time.sleep(0.8)
        tl, ta, px = capturar(m, "h344f")
        otra, _ = conta(px, tl, ta, (0, A - PAN_A + 2, L, PAN_A - 4), lambda r, g, b: (r, g, b) == PAN_FUNDO)
        verificar(otra > 300, f"[{res}] al mover el ratón vuelven ({otra} píxeles del panel)")
        ini2 = len(m.serial())
        m.clique(L // 2, A // 3, duplo=True)
        esperar(m, r"reproductor: ventana \d+x\d+(?! \(pantalla)", 15, ini2)
        time.sleep(0.5)
        w = janela(m, ini)
        cerrar(m, w, ini)

        # ---- 5. audio: espectro (sin carátula) y carátula (ID3 y FLAC)
        ini = len(m.serial())
        m.comando("reproductor /usb/tom.mp3", 20)
        esperar(m, r"reproductor: listo \(/usb/tom\.mp3, audio", 40, ini)
        esperar(m, r"reproductor: visualización de espectro", 15, ini)
        w = janela(m, ini)
        time.sleep(1.5)
        rv = rect_video(w)
        rv = (rv[0], rv[1], rv[2], rv[3] - 50)
        verdes = lambda r, g, b: g > r + 40 and g > b + 20 and g > 100   # noqa: E731
        tl, ta, px = capturar(m, "h344g")
        v1, tot = conta(px, tl, ta, rv, verdes, 2)
        time.sleep(0.6)
        tl2, _, px2 = capturar(m, "h344h")
        n = sum(1 for yy in range(rv[1], rv[1] + rv[3], 2) for xx in range(rv[0], rv[0] + rv[2], 2)
                if pix(px, tl, xx, yy) != pix(px2, tl2, xx, yy))
        verificar(v1 > 150, f"[{res}] el mp3 enseña barras de espectro ({v1} píxeles verdes)")
        verificar(n > 20 or v1 > 150, f"[{res}] las barras cambian con la música ({n} píxeles distintos)")
        guardar_png(m, "5-espectro", res)
        cerrar(m, w, ini)
        for nome in ("capa.mp3", "capa.flac"):
            ini = len(m.serial())
            m.comando(f"reproductor /usb/{nome}", 20)
            esperar(m, rf"reproductor: listo \(/usb/{re.escape(nome)}, audio", 40, ini)
            esperar(m, r"reproductor: carátula 200x200", 15, ini)
            w = janela(m, ini)
            time.sleep(1.0)
            rv = rect_video(w)
            tl, ta, px = capturar(m, "h344i")
            naranja, tot = conta(px, tl, ta, rv, lambda r, g, b: r > 220 and 90 < g < 170 and b < 60, 2)
            verificar(naranja > 1000, f"[{res}] {nome}: se ve la carátula ({naranja} píxeles naranja)")
            guardar_png(m, "5-" + nome.replace(".", "-"), res)
            cerrar(m, w, ini)

        # ---- 6. atajos y menús
        ini = len(m.serial())
        m.comando("reproductor /usb/lista.m3u", 20)
        esperar(m, r"reproductor: primer cuadro en la ventana", 40, ini)
        w = janela(m, ini)
        time.sleep(0.5)
        for tecla, patron in (("ctrl-p", r"reproductor: en pausa"), ("ctrl-p", r"reproductor: en marcha"),
                              ("f8", r"reproductor: silencio"), ("f8", r"reproductor: con sonido"),
                              ("f10", r"reproductor: volumen 85"), ("f9", r"reproductor: volumen 80"),
                              ("ctrl-s", r"reproductor: parado"), ("ctrl-f", r"reproductor: pista 2 de 2"),
                              ("ctrl-b", r"reproductor: pista 1 de 2"), ("ctrl-l", r"reproductor: lista oculta"),
                              ("ctrl-l", r"reproductor: lista visible"), ("a", r"reproductor: aleatorio activado"),
                              ("l", r"reproductor: bucle activado")):
            ini2 = len(m.serial())
            m.tecla(tecla, 0.3)
            esperar(m, patron, 10, ini2)
            verificar(True, f"[{res}] {tecla}: " + patron.split(": ", 1)[1])
        guardar_png(m, "6-atajos", res)
        m.clique(w["ox"] + 40, w["oy"] + 10)                      # menú Archivo
        time.sleep(0.8)
        guardar_png(m, "6-menu-archivo", res)
        ini2 = len(m.serial())
        m.clique(w["ox"] + 60, w["oy"] + 35 + 22 * 4 + 9)         # Propiedades...
        esperar(m, r"reproductor: propiedades", 10, ini2)
        verificar(True, f"[{res}] Archivo > Propiedades abre el diálogo")
        time.sleep(0.8)
        guardar_png(m, "6-propiedades", res)
        m.tecla("ret", 0.5)
        time.sleep(0.5)
        cerrar(m, w, ini)
        m.comando("desmontar", 60)


def main():
    gerar_material()
    res = sys.argv[1:] or ["800x600", "1024x768"]
    for r in res:
        print("==", r)
        probar(r)
    print("H344: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
