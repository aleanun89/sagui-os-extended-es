#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H83 (D112, mejoras 007 y 047) - la aplicación «Configuración» y su persistencia.

Parte 1 (imagen sin /home persistente, modo consola): "configuracion teclado/acento" avisa de
que no se recordará; "--aplicar" sin archivo no hace nada.

Parte 2 (imagen gráfica con partición ext2 de datos, sonido AC'97): se cambian el teclado
(ABNT2), la zona horaria, el color de acento, el volumen y el fondo (PNG estirado) con la
línea de comandos del programa; se comprueba el color de la barra de título y el fondo en la
captura; se REINICIA la VM con la misma imagen y se comprueba que el arranque lo aplica todo
(teclado, volumen, zona, acento, fondo) leyendo /home/.config/sagui.conf.

Parte 3: la ventana «Configuración» (una captura de cada sección y un clic de verdad en el
color de acento y en el botón del teclado), más los modos centrado y mosaico del fondo.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, imagem_console, RAIZ, SAIDA, opcoes_apps  # noqa: E402

ACENTO = (0x2A, 0x6F, 0xD8)
DADOS_MB = 16

# As abas da janela «Configuración»: cada uma é da largura do seu título (D145).
TITULOS_PAG = ("Teclado", "Fecha y hora", "Pantalla", "Sonido", "Escritorio", "Capturas", "IA", "Fuentes", "Red", "Escritorio remoto", "Mandos",
               "Usuarios")


def pos_paginas():
    """Devolve a posição e a largura de cada aba, como as calcula o programa."""
    pos = []
    suma = sum(len(t) * 8 for t in TITULOS_PAG)
    relleno = 10   # como traducir_rotulos(): menos si no caben en los 788 px (issue #43; hasta 1 con Usuarios, D340)
    while relleno > 1 and 10 + suma + len(TITULOS_PAG) * (relleno + 1) > 788 - 4:
        relleno -= 1
    x = 4 if 10 + suma + len(TITULOS_PAG) * (relleno + 1) > 788 - 4 else 10
    for t in TITULOS_PAG:
        l = len(t) * 8 + relleno
        pos.append((x, l))
        x += l + 1
    return pos


def imagem_persistente(nome, grafica):
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, nome + ".img")
    cmd = ["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
           "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
           "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
           "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--dados-mb", str(DADOS_MB),
           *opcoes_apps()]   # D324: con /apps, para que no salga el aviso de «falta /apps» sobre las ventanas
    if not grafica:
        cmd.append("--console")
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return destino


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]), y=int(g[3]))


def perto(a, b, tol):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def esperado_estirado(bmp, x, y, L=1024, A=768):
    """El pixel (x, y) de un BMP de 24 bits estirado a L x A (bilineal, como la libsagui)."""
    d = open(bmp, "rb").read()
    off = int.from_bytes(d[10:14], "little")
    w = int.from_bytes(d[18:22], "little")
    h = int.from_bytes(d[22:26], "little", signed=True)
    cima_abajo = h < 0
    h = abs(h)
    fila = (w * 3 + 3) & ~3

    def px(sx, sy):
        sx = min(max(sx, 0), w - 1)
        sy = min(max(sy, 0), h - 1)
        r = sy if cima_abajo else h - 1 - sy
        o = off + r * fila + sx * 3
        return (d[o + 2], d[o + 1], d[o])

    fx = min(max((x + 0.5) * w / L - 0.5, 0), w - 1)
    fy = min(max((y + 0.5) * h / A - 0.5, 0), h - 1)
    x0, y0 = int(fx), int(fy)
    wx, wy = fx - x0, fy - y0
    a, b, c, e = px(x0, y0), px(x0 + 1, y0), px(x0, y0 + 1), px(x0 + 1, y0 + 1)
    return tuple(round((a[k] * (1 - wx) + b[k] * wx) * (1 - wy) + (c[k] * (1 - wx) + e[k] * wx) * wy) for k in range(3))


def reiniciar(m):
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 60
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.3)
    verificar(m.proc.poll() is not None, "reiniciar: a máquina reiniciou")


def parte1():
    with Maquina("h83-p1", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("configuracion: sin /home/.config/sagui.conf" in m.serial() or "configuracion --aplicar" in m.serial(),
                  "o arranque executa configuracion --aplicar")
        out = m.comando("configuracion teclado abnt2")
        verificar("Aviso: /home no es un disco persistente" in out, "sem /home persistente: avisa que não se recordará")
        out = m.comando("teclado")
        verificar("abnt2" in out, "o teclado mudou na hora, mesmo sem persistência")
        out = m.comando("configuracion acento 2A6FD8")
        verificar("Aviso" in out, "o acento também avisa")
        out = m.comando("configuracion mostrar")
        verificar("Acento:    2a6fd8" in out and "no existe" in out, "mostrar: acento aplicado, sem arquivo")
        out = m.comando("configuracion acento ZZZ")
        verificar("RRGGBB" in out, "acento inválido: mensagem de uso")


def registou_pagina(m, nome, depois):
    """Espera que o serie registe «configuracion: pagina <nome>» depois da posição «depois».
    Devolve a posição da marca no serie, ou -1 se não aparece."""
    marca = f"configuracion: pagina {nome}"
    fim = time.time() + 6
    while True:
        p = m.serial().find(marca, depois)
        if p >= 0 or time.time() > fim:
            return p
        time.sleep(0.3)


def parte2e3():
    img = imagem_persistente("h83-persistente", True)
    ref = os.path.join(RAIZ, "rootfs", "imagenes", "degradado.bmp")
    with Maquina("h83-a", "ide", imagem=img, snapshot=False, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("montado em /home, leitura e escrita" in s, "o fstab montou /home (rw)")
        verificar("[GUI] area de trabalho" in s or "area de trabalho no ar" in s, "escritório gráfico")
        out = m.comando("configuracion acento 2A6FD8")
        verificar("Guardado" in out, "acento: guardado")
        out = m.comando("configuracion volumen 37")
        verificar("Guardado" in out, "volumen: guardado")
        out = m.comando("configuracion zona America/Sao_Paulo")
        verificar("Guardado" in out, f"zona: guardada ({out.strip()[-120:]})")
        out = m.comando("configuracion fondo /imagenes/degradado.bmp estirado")
        verificar("Guardado" in out and "Error" not in out, f"fondo: guardado ({out.strip()[-80:]})")
        out = m.comando("cat /home/.config/sagui.conf")
        for k in ("volumen=37", "zona=America/Sao_Paulo", "acento=2a6fd8",
                  "fondo=/imagenes/degradado.bmp", "fondo_modo=estirado"):
            verificar(k in out, f"sagui.conf tem {k}")
        # el teclado va el último: desde aquí las teclas del test salen con la distribución ABNT2
        out = m.comando("configuracion teclado abnt2")
        verificar("Guardado en /home/.config/sagui.conf" in out, "teclado: guardado em /home")
        time.sleep(1.0)
        png = m.captura("H83-1-acento-fondo")
        t = janela(m.serial(), "Terminal")
        cor = cor_do_pixel(png, t["x"] + t["l"] - 120, t["y"] + 8)
        verificar(perto(cor, ACENTO, 2), f"a barra de título do Terminal usa o acento {cor}")
        for (x, y) in ((1000, 740), (700, 600)):
            c = cor_do_pixel(png, x, y)
            e = esperado_estirado(ref, x, y)
            verificar(perto(c, e, 40), f"fundo estirado em ({x},{y}): {c} ~ {e}")
        out = m.comando("configuracion mostrar")
        verificar("teclado 1, volumen 37" in out, "o arquivo tem o teclado ABNT2")
        reiniciar(m)

    with Maquina("h83-b", "ide", imagem=img, snapshot=False, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("configuracion: teclado 1 -> 0" in s, "depois de reiniciar o arranque aplicou o teclado")
        verificar("configuracion: acento 2a6fd8" in s, "... o acento")
        verificar("configuracion: volumen 37" in s, "... o volume")
        verificar("configuracion: fondo /imagenes/degradado.bmp (estirado) -> 0" in s, "... o fundo")
        out = m.comando("teclado")
        verificar("abnt2" in out, "teclado ABNT2 depois de reiniciar")
        out = m.comando("volumen")
        verificar("37 %" in out, f"volume 37 % depois de reiniciar ({out.strip()[:60]})")
        out = m.comando("zona")
        verificar("America/Sao_Paulo" in out, "zona America/Sao_Paulo depois de reiniciar")
        out = m.comando("configuracion mostrar")
        verificar("teclado 1, volumen 37, zona 'America/Sao_Paulo', acento 2a6fd8" in out, "mostrar: o arquivo tem tudo")
        m.comando("configuracion teclado es")   # (as teclas do teste são as de um teclado ES)
        time.sleep(1.0)
        png = m.captura("H83-2-depois-de-reiniciar")
        t = janela(m.serial(), "Terminal")
        verificar(perto(cor_do_pixel(png, t["x"] + t["l"] - 120, t["y"] + 8), ACENTO, 2), "acento depois de reiniciar")
        c = cor_do_pixel(png, 1000, 740)
        verificar(perto(c, esperado_estirado(ref, 1000, 740), 40), "fundo depois de reiniciar")

        # modos do fundo
        m.comando("configuracion fondo /imagenes/mandelbrot.png centrado")
        time.sleep(0.8)
        png = m.captura("H83-3-fondo-centrado")
        verificar(cor_do_pixel(png, 1000, 740) == (0, 0, 0), "centrado: fora da imagem fica preto")
        m.comando("configuracion fondo /imagenes/mandelbrot.png mosaico")
        time.sleep(0.8)
        m.captura("H83-4-fondo-mosaico")
        m.comando("configuracion fondo /imagenes/atardecer.png ajustado")
        time.sleep(0.8)
        m.captura("H83-5-fondo-ajustado")

        # a janela
        m.comando("configuracion &")
        fim = time.time() + 20
        while time.time() < fim and 'criada por' not in m.serial().rsplit('"Configuración"', 1)[-1] and '"Configuración"' not in m.serial():
            time.sleep(0.3)
        time.sleep(1.0)
        j = janela(m.serial(), "Configuración")
        ox, oy = j["x"] + 2, j["y"] + 24
        m.captura("H83-6-janela-teclado")
        pos = pos_paginas()
        for i, nome in enumerate(("teclado", "zona", "pantalla", "sonido", "escritorio")):
            m.clique(ox + pos[i][0] + pos[i][1] // 2, oy + 24)
            time.sleep(0.5)
            m.captura(f"H83-7-janela-{nome}")
        # Capturas: antes del arreglo (Capturas, v7.3.1) la ventana se quedaba atrapada
        # en esta pestaña y no volvía a cambiar. Se comprueba por el serie.
        m.clique(ox + pos[5][0] + pos[5][1] // 2, oy + 24)
        time.sleep(0.5)
        m.captura("H83-7b-janela-capturas")
        p_cap = registou_pagina(m, "Capturas", 0)
        verificar(p_cap >= 0, "pestaña Capturas: el serie registra la página")
        m.clique(ox + pos[0][0] + pos[0][1] // 2, oy + 24)   # sale de Capturas
        p_tec = registou_pagina(m, "Teclado", max(p_cap, 0))
        verificar(p_cap >= 0 and p_tec > p_cap, "pestaña Capturas: al pulsar Teclado la página cambia")
        # y todas las pestañas, en orden, para que ninguna se quede atrapada
        cursor = max(p_tec, 0)
        for i, nome in enumerate(TITULOS_PAG):
            m.clique(ox + pos[i][0] + pos[i][1] // 2, oy + 24)
            time.sleep(0.3)
            cursor = registou_pagina(m, nome, cursor + 1)   # una marca nueva, posterior al clic
            verificar(cursor >= 0, f"pestaña {nome}: la página cambia")
            if cursor < 0:
                break
        # el recorrido acaba en la última pestaña: se vuelve a Escritorio para los colores
        m.clique(ox + pos[4][0] + pos[4][1] // 2, oy + 24)
        time.sleep(0.5)
        # clique de verdade no segundo swatch (verde -> azul ya activo): elegimos Rojo (índice 2)
        m.clique(ox + 24 + 2 * 70 + 20, oy + 50 + 28 + 10)
        time.sleep(0.6)
        png = m.captura("H83-8-acento-rojo")
        t = janela(m.serial(), "Terminal")
        c = cor_do_pixel(png, j["x"] + j["l"] - 150, j["y"] + 8)
        verificar(perto(c, (0xC0, 0x39, 0x2B), 2), f"clique no color Rojo: a barra da janela ativa ficou {c}")
        out = ""
        m.clique(ox + pos[0][0] + pos[0][1] // 2, oy + 24)   # volta ao teclado
        m.clique(ox + 24 + 100, oy + 50 + 56 + 15)   # botão "Español (ES)"
        time.sleep(0.6)
        m.captura("H83-9-teclado-es")
        verificar("configuracion: ventana abierta (persistente 1)" in m.serial(), "a janela sabe que /home é persistente")
        # D218: la pestaña Red, con el interruptor del bloqueo de publicidad por DNS.
        m.clique(ox + pos[8][0] + pos[8][1] // 2, oy + 24)
        time.sleep(0.5)
        m.captura("H83-10-red")
        m.clique(ox + 24 + 80, oy + 50 + 100 + 14)   # botón "Activar"
        fim = time.time() + 30
        while time.time() < fim and "[BLOQUEO] activo:" not in m.serial():
            time.sleep(0.3)
        verificar("[BLOQUEO] activo:" in m.serial(), "Red > Activar: el kernel carga la tabla del bloqueo")
        time.sleep(0.8)
        m.captura("H83-11-red-bloqueo-activado")
        m.clique(ox + 24 + 80, oy + 50 + 100 + 14)   # ahora dice "Desactivar"
        time.sleep(2)
        m.digitar("", 0.01)


if __name__ == "__main__":
    parte1()
    parte2e3()
    print("H83 OK")
