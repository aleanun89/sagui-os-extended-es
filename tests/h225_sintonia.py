#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H225 (D225) - Sintonía, el reproductor de música de /apps (userland/programas/sintonia.c).

El audio es el que genera h86 (tonos distintos por canal: un WAV 440/660 Hz, un MP3 con etiquetas ID3
600/1500 Hz y un FLAC con comentarios Vorbis 800/1700 Hz, de 6 s; hace falta ffmpeg). Partes:

  unidad  en el PC (tests/sintonia_unidad.c con sintonia_logica.h): FFT y espectro, listas M3U y PLS (leer,
          escribir y releer), metadatos (ID3v1, ID3v2.2/2.3/2.4, Vorbis de FLAC, WAV, duración Xing y por
          tasa), filtro ICY de la radio, corte de MP3 en cuadros, temas, orden aleatorio;
  os      QEMU con tarjeta Intel HDA grabando la salida en un .wav y la partición SAGUI-APPS (D194):
            - `sintonia` está en /apps/bin y `ajuda sintonia` explica el uso;
            - abrir una lista M3U de 3 pistas (WAV, MP3, FLAC) las carga, elige la primera y la toca; el
              visualizador dibuja barras de espectro (píxeles del color de las barras) y el pie dice
              «Reproduciendo | pista 1 de 3»; las etiquetas ID3 y Vorbis salen en la lista;
            - siguiente (B) abre la pista 2 (MP3); pausa (C) y seguir (C) sin perder el sitio; un clic en la
              barra de posición salta; modo mini (M) redimensiona la ventana a 520x64 y vuelve;
            - guardar la lista (Ctrl+S) y volver a abrirla (O) da otra vez 3 pistas;
            - aleatorio (S) y repetir (R); clic en el visualizador cambia a osciloscopio;
            - el ecualizador (E) y su preajuste «Voz» aplican [AUDIO] ecualizador activo: -3 -1 3 3 -2 dB;
            - en la grabación suenan el tono del WAV y el del MP3 (y cada uno con su canal derecho);
            - capturas de pantalla (normal, mini, ecualizador).

Uso: h225_sintonia.py [unidad] [os]
"""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu, opcoes_apps,  # noqa: E402
                  RAIZ, SAIDA, FalhaTeste)
import h70_sonido as h70  # noqa: E402
import h86_musica as h86  # noqa: E402

DIR = os.path.join(SAIDA, "h225")

COR_BARRAS = (57, 224, 127)        # tema.barras (0x39E07F)
COR_PANEL = (14, 26, 20)           # tema.panel (0x0E1A14)
COR_ACENTO = (47, 208, 112)        # tema.acento (0x2FD070)
COR_OSC = (125, 240, 174)          # tema.osciloscopio (0x7DF0AE)


# ------------------------------------------------------------ unidad

def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "sintonia_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DSINTONIA_ANFITRION", "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-o", exe, os.path.join(RAIZ, "tests", "sintonia_unidad.c"), "-lm"], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout,
              "FFT, M3U/PLS, ID3/Vorbis/WAV/Xing, ICY, corte de MP3, tema y aleatorio")


# ------------------------------------------------------------ dentro de SaguiOS

def contar(t, caja, color, tol=8):
    """Píxeles de la captura t dentro de caja = (x, y, l, a) cuyo color está a menos de tol de color."""
    larg, _, px = t
    x0, y0, l, a = caja
    n = 0
    for y in range(y0, y0 + a):
        base = (y * larg + x0) * 3
        for x in range(l):
            p = px[base + x * 3: base + x * 3 + 3]
            if abs(p[0] - color[0]) <= tol and abs(p[1] - color[1]) <= tol and abs(p[2] - color[2]) <= tol:
                n += 1
    return n


def pie(m, jm):
    t = h86.tela(m)
    return h86.texto_em(t, jm["x"] + 8, jm["y"] + jm["a"] - 20 + 3, 60)


def escribir_ruta(m, borrar, texto):
    """En el cuadro de diálogo (que ya trae una ruta): borra `borrar` letras y escribe `texto`."""
    time.sleep(1.0)
    for _ in range(borrar):
        m.tecla("backspace", 0.05)
    m.digitar(texto, 0.05)
    m.tecla("ret")


def provar_os(arqs):
    if not opcoes_apps():
        raise FalhaTeste("falta build/apps/raiz: compila con make (la partición SAGUI-APPS)")
    verificar(os.path.isfile(os.path.join(RAIZ, "build", "apps", "raiz", "bin", "sintonia")), "sintonia está en build/apps/raiz/bin")
    nombres = ("a_tono.wav", "b_tono.mp3", "c_tono.flac")
    lista = "".join(f"/tmp/m/{n}\n" for n in nombres)
    pen = criar_pendrive("h225-pen", rotulo="SINTONIA", arquivos=dict(arqs, **{"lista.m3u": lista.encode()}))
    imagem = imagem_de_teste("", ["--vesa"] + opcoes_apps())
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h225", "ide", imagem=imagem, extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("which sintonia")
        verificar("/apps/bin/sintonia" in out, f"sintonia está en /apps/bin ({out.strip()[-60:]})")
        out = m.comando("ayuda sintonia")
        verificar("sintonia [archivo | carpeta | lista.m3u" in out, "ajuda sintonia explica el uso")
        m.comando("montar usb0p1")
        m.comando("mkdir /tmp/m")
        for n in list(nombres) + ["lista.m3u"]:
            m.comando(f"cp /usb/{n} /tmp/m/{n}", 120)

        # ---- abrir una lista M3U: carga, elige y toca la primera
        ini = len(m.serial())
        m.comando("sintonia /tmp/m/lista.m3u &")
        h86.esperar(m, r"sintonia: pista 1 a_tono.wav: WAV PCM 44100 Hz 2 canales 6000 ms", 40, ini)
        h86.esperar(m, r"sintonia: reproduciendo a_tono.wav", 10, ini)
        h86.esperar(m, r"sintonia: pronto \(3 pistas", 10, ini)
        verificar(True, "la lista M3U carga 3 pistas, elige la primera y la toca")
        jm = h86.janela(m.serial()[ini:], "Sinton.a")
        verificar(jm["l"] == 520 and jm["a"] == 410, f"ventana de {jm['l']}x{jm['a']}")
        time.sleep(1.5)
        m.captura("h225-sintonia-tocando")
        t = h86.tela(m)
        verificar(h86.perto(h86.pixel(t, jm["x"] + 300, jm["y"] + 14), COR_PANEL, 6), "el panel principal es oscuro (tema de fábrica)")
        vis = (jm["x"] + 112, jm["y"] + 32, 394, 50)
        n = contar(t, vis, COR_BARRAS)
        verificar(n >= 15, f"el visualizador dibuja barras de espectro ({n} píxeles de color de barra)")
        verificar(h86.perto(h86.pixel(t, jm["x"] + 20, jm["y"] + 99), COR_ACENTO, 8), "la barra de posición se llena")
        verificar(h86.perto(h86.pixel(t, jm["x"] + 500, jm["y"] + 99), COR_PANEL, 8), "y el resto sigue vacío")
        p = pie(m, jm)
        verificar("Reproduciendo" in p and "pista 1 de 3" in p, f"el pie dice «{p}»")
        time.sleep(1.5)
        t = h86.tela(m)
        fila3 = h86.texto_em(t, jm["x"] + 12, jm["y"] + 258, 34)
        verificar("Tono FLAC" in fila3, f"la lista muestra las etiquetas Vorbis del FLAC: «{fila3}»")
        fila2 = h86.texto_em(t, jm["x"] + 12, jm["y"] + 240, 34)
        verificar("Tono MP3" in fila2, f"y las ID3 del MP3: «{fila2}»")

        # ---- siguiente, pausa y seguir, salto
        marca = len(m.serial())
        m.tecla("b")
        h86.esperar(m, r"sintonia: pista 2 b_tono.mp3: MP3 \(MPEG-1 Layer III\) 44100 Hz 2 canales \d+ ms", 15, marca)
        h86.esperar(m, r"sintonia: reproduciendo b_tono.mp3", 10, marca)
        verificar(True, "B: siguiente abre y toca el MP3")
        time.sleep(2.5)
        m.tecla("c")
        pausa = int(h86.esperar(m, r"sintonia: pausa en (\d+) ms", 10, marca))
        verificar(1500 <= pausa <= 4500, f"pausa en {pausa} ms")
        time.sleep(0.5)
        p = pie(m, jm)
        verificar("En pausa" in p, f"el pie dice «{p}»")
        m.captura("h225-sintonia-pausa")
        m.tecla("c")
        sigue = int(h86.esperar(m, r"sintonia: sigue en (\d+) ms", 10, marca))
        verificar(abs(sigue - pausa) <= 60, f"seguir continúa donde se quedó ({pausa} -> {sigue} ms)")
        time.sleep(0.5)
        m.clique(jm["x"] + 8 + int(504 * 0.75), jm["y"] + 99)
        salto = int(h86.esperar(m, r"sintonia: salto a (\d+) ms", 10, marca))
        verificar(abs(salto - 4500) < 250, f"clic en la barra al 75 %: salto a {salto} ms (esperado ~4500)")
        time.sleep(1.0)

        # ---- visualizador: un clic cambia a osciloscopio
        m.clique(jm["x"] + 112 + 200, jm["y"] + 32 + 25)
        time.sleep(0.8)
        m.captura("h225-sintonia-osciloscopio")
        t = h86.tela(m)
        n = contar(t, vis, COR_OSC)
        verificar(n >= 40, f"el osciloscopio dibuja la onda ({n} píxeles)")

        # ---- ecualizador del sistema (D201): preajuste Voz
        marca = len(m.serial())
        m.tecla("e")
        h86.esperar(m, r"sintonia: paneles eq=1 lista=1", 10, marca)
        time.sleep(0.8)
        m.clique(jm["x"] + 330 + 45, jm["y"] + 166 + 6 + 2 * 24 + 10)
        h86.esperar(m, r"sintonia: ecualizador voz", 10, marca)
        h86.esperar(m, r"\[AUDIO\] ecualizador activo: -3 -1 3 3 -2 dB", 10, marca)
        verificar(True, "el preajuste Voz aplica el ecualizador del sistema (-3 -1 3 3 -2 dB)")
        time.sleep(0.5)
        m.captura("h225-sintonia-ecualizador")
        m.tecla("e")
        h86.esperar(m, r"sintonia: paneles eq=0 lista=1", 10, marca)

        # ---- aleatorio y repetir
        m.tecla("s")
        h86.esperar(m, r"sintonia: aleatorio 1", 10, marca)
        m.tecla("r")
        h86.esperar(m, r"sintonia: repetir 1", 10, marca)
        m.tecla("s")
        h86.esperar(m, r"sintonia: aleatorio 0", 10, marca)
        verificar(True, "S y R: aleatorio y repetir")

        # ---- guardar la lista en M3U y volver a abrirla
        marca = len(m.serial())
        m.tecla("ctrl-s")
        escribir_ruta(m, 4, "2.m3u")     # /tmp/m/lista.m3u -> /tmp/m/lista2.m3u
        h86.esperar(m, r"sintonia: lista guardada en /tmp/m/lista2.m3u \(3 pistas, M3U\)", 20, marca)
        verificar(True, "Ctrl+S guarda la lista en M3U")
        time.sleep(1.0)
        marca = len(m.serial())
        m.tecla("o")
        escribir_ruta(m, 0, "/lista2.m3u")   # propone la carpeta (/tmp/m)
        h86.esperar(m, r"sintonia: lista vaciada", 20, marca)
        h86.esperar(m, r"sintonia: 3 pistas añadidas desde /tmp/m/lista2.m3u", 20, marca)
        h86.esperar(m, r"sintonia: pista 1 a_tono.wav", 20, marca)
        verificar(True, "la lista guardada se vuelve a abrir con sus 3 pistas")
        time.sleep(1.0)

        # ---- modo mini
        marca = len(m.serial())
        m.tecla("m")
        h86.esperar(m, r"sintonia: modo mini", 10, marca)
        h86.esperar(m, rf"janela {jm['id']} redimensionada para 520x64", 10, marca)
        time.sleep(1.2)
        m.captura("h225-sintonia-mini")
        t = h86.tela(m)
        caja = (jm["x"] + 236, jm["y"] + 28, 224, 20)
        n = contar(t, caja, COR_BARRAS) + contar(t, caja, COR_OSC)   # el modo (barras u osciloscopio) se conserva del clic anterior
        verificar(n >= 5, f"en modo mini el visualizador sigue ({n} píxeles)")
        verificar(h86.perto(h86.pixel(t, jm["x"] + 400, jm["y"] + 12), COR_PANEL, 6), "y el panel es una sola barra oscura")
        m.tecla("m")
        h86.esperar(m, r"sintonia: modo normal", 10, marca)
        h86.esperar(m, rf"janela {jm['id']} redimensionada para 520x410", 10, marca)
        verificar(True, "M: mini (520x64) y vuelta a la ventana normal")
        time.sleep(1.0)
    # ---- la grabación: suenan los tonos del WAV y del MP3, cada uno con su canal derecho
    esq, dir_ = h70.ler_gravacao(m.wav)
    pares = {440: 660, 600: 1500}
    dur = {f: 0.0 for f in pares}
    mal = 0
    for k in range(0, len(esq) - 4800, 4800):
        x, y = esq[k:k + 4800], dir_[k:k + 4800]
        if h70.rms(x) < 1000:
            continue
        f = max(pares, key=lambda f: h70.goertzel(x, f))
        if h70.goertzel(y, pares[f]) < 0.5 * max(h70.goertzel(y, g) for g in pares.values()):
            mal += 1
        dur[f] += 0.1
    print(f"  segundos por tono: {dur} (ventanas dudosas: {mal})")
    verificar(dur[440] >= 1.0, f"suena el WAV (440/660 Hz): {dur[440]:.1f} s")
    verificar(dur[600] >= 1.5, f"suena el MP3 (600/1500 Hz): {dur[600]:.1f} s")
    verificar(mal <= 4, f"cada tono izquierdo va con su tono derecho ({mal} ventanas dudosas, las de los cambios de pista)")


def main():
    quais = sys.argv[1:] or ["unidad", "os"]
    if "unidad" in quais:
        unidad()
    if "os" in quais:
        provar_os(h86.preparar())
    print("H225 ok")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print(f"FALHOU: {e}")
        sys.exit(1)
