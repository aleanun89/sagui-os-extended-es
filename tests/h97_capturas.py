#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H97 (D125) - capturas de pantalla (v6.0 beta 3), versión reducida para la beta.

Criterio:
  - `captura -r x,y,l,a --verificar` guarda un PNG, lo relee con el lector de la libsagui y
    coincide pixel a pixel con lo capturado; el hash coincide con el `screendump` de QEMU;
  - `captura -v` captura la ventana activa con su marco (ancho + 4, alto + 26);
  - Impr Pant guarda captura-AAAAMMDD-HHMMSS.png en /home/capturas (o /tmp/capturas) con aviso;
    Alt+Impr Pant también (modo ventana).
  - Windows+Mayús+S, arrastre con el ratón: sale la vista previa con el tamaño arrastrado.
Pendiente: probar Guardar/Abrir en Paint por ratón, pegar imagen en Paint.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, SAIDA  # noqa: E402


def fnv(pixeles):
    h = 2166136261
    for p in pixeles:
        h = ((h ^ p) * 16777619) & 0xFFFFFFFF
    return h


def region_ppm(ppm, x, y, l, a):
    with open(ppm, "rb") as f:
        d = f.read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", d)
    W = int(m.group(1))
    body = d[m.end():]
    px = []
    for yy in range(y, y + a):
        for xx in range(x, x + l):
            o = (yy * W + xx) * 3
            px.append((body[o] << 16) | (body[o + 1] << 8) | body[o + 2])
    return px


def main():
    with Maquina("h97", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---- recorte por el terminal, comparado con el screendump de QEMU
        ppm = os.path.join(SAIDA, "h97-pantalla.ppm")
        if os.path.exists(ppm):
            os.remove(ppm)
        m.monitor(f"screendump {ppm}")
        time.sleep(1)
        out = m.comando("captura -r 900,700,100,60 --verificar /tmp/a.png")
        verificar("verificar: OK 100x60 en 900,700" in out, "captura -r guarda un PNG que el lector relee igual")
        h = re.search(r"hash ([0-9a-f]{8})", out)
        verificar(h and int(h.group(1), 16) == fnv(region_ppm(ppm, 900, 700, 100, 60)),
                  "los píxeles coinciden con el screendump de QEMU")

        # ---- PNG comprimido: tamaño pequeño y lectura en el anfitrión con zlib
        out = m.comando("captura -r 0,60,320,200 --verificar --hex /tmp/c.png", tempo=120)
        verificar("verificar: OK 320x200" in out, "el PNG comprimido se relee igual en el invitado")
        hx = "".join(re.findall(r"HX:([0-9a-f]+)", out))
        try:
            dados = bytes.fromhex(hx)
        except ValueError:
            open(os.path.join(SAIDA, "h97-hex.txt"), "w").write(out)
            raise
        fim = re.search(r"HEXFIM (\d+)", out)
        open(os.path.join(SAIDA, "h97-hex.txt"), "w").write(out)
        verificar(fim and int(fim.group(1)) == len(dados), f"HEX completo ({fim and fim.group(1)} vs {len(dados)})")
        verificar(dados[:8] == b"\x89PNG\r\n\x1a\n", "firma PNG")
        # trozos y zlib
        import zlib
        pos, idat = 8, b""
        while pos < len(dados):
            n = int.from_bytes(dados[pos:pos + 4], "big")
            tipo = dados[pos + 4:pos + 8]
            cuerpo = dados[pos + 8:pos + 8 + n]
            crc = int.from_bytes(dados[pos + 8 + n:pos + 12 + n], "big")
            verificar(zlib.crc32(tipo + cuerpo) == crc, f"CRC del trozo {tipo.decode()}")
            if tipo == b"IDAT":
                idat += cuerpo
            pos += 12 + n
        raw = zlib.decompress(idat)
        verificar(len(raw) == 200 * (1 + 320 * 3), "zlib descomprime el tamaño esperado")
        print(f"  PNG de 320x200: {len(dados)} bytes (sin comprimir: {len(raw)})")
        verificar(len(dados) < len(raw) // 4, "el PNG pesa menos de la cuarta parte")

        # ---- ajuste de Configuración: carpeta de las capturas
        out = m.comando("echo capturas_carpeta=/tmp/mis > /tmp/capturas.conf; captura")
        verificar("Captura guardada en /tmp/mis/captura-" in out, "la carpeta de capturas de Configuración se respeta")
        m.comando("rm /tmp/capturas.conf")

        # ---- ventana activa con su marco
        out = m.comando("captura -v --verificar /tmp/v.png")
        g = re.search(r"verificar: OK (\d+)x(\d+) en", out)
        verificar(g is not None, "captura -v guarda y relee la ventana activa")
        t = re.findall(r'janela \d+ criada por \d+: "Terminal" (\d+)x(\d+)', m.serial())[-1]
        verificar((int(g.group(1)), int(g.group(2))) == (int(t[0]) + 4, int(t[1]) + 26),
                  f"tamaño de la ventana con marco ({g.group(1)}x{g.group(2)} vs {t[0]}x{t[1]}+marco)")

        # ---- teclas
        marca = len(m.serial())
        m.tecla("print")
        fim = time.time() + 20
        while time.time() < fim and "Captura guardada" not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("Captura guardada" in m.serial()[marca:], "Impr Pant guarda la captura y avisa")
        out = m.comando("ls /home/capturas /tmp/capturas")
        verificar(re.search(r"captura-\d{8}-\d{6}\.png", out) is not None, "el archivo captura-AAAAMMDD-HHMMSS.png existe")
        marca = len(m.serial())
        m.tecla("alt-sysrq")
        fim = time.time() + 20
        while time.time() < fim and "captura -v" not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("captura -v" in m.serial()[marca:], "Alt+Impr Pant lanza la captura de la ventana activa")
        # ---- recorte: Windows+Mayús+S, arrastrar, Guardar con nombre, Copiar y Abrir en Paint
        ppm0 = os.path.join(SAIDA, "h97-antes.ppm")
        if os.path.exists(ppm0):
            os.remove(ppm0)
        m.monitor(f"screendump {ppm0}")
        time.sleep(1)
        marca = len(m.serial())
        m.tecla("meta_l-shift-s", 0.2)
        fim = time.time() + 20
        while time.time() < fim and '"Recortes"' not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar('"Recortes"' in m.serial()[marca:], "Windows+Mayús+S abre Recortes")
        time.sleep(1.5)
        m.arrastar(300, 300, 500, 420)
        time.sleep(1.5)
        pat = r'janela \d+ criada por \d+: "Recortes" (\d+)x(\d+) em \((\d+),(\d+)\)'
        g = re.findall(pat, m.serial()[marca:])
        verificar(len(g) >= 2, f"tras soltar sale la ventana de vista previa ({g})")
        pw, ph, px_, py_ = (int(v) for v in g[-1])
        verificar(abs((ph - 72) - 120) <= 4, f"el recorte tiene la altura arrastrada (~120): {ph - 72}")
        ox, oy = px_ + 2, py_ + 24   # origen del contenido

        # Copiar: el portapapeles de imagen del kernel
        marca = len(m.serial())
        m.clique(ox + 110 + 45, oy + ph - 40 + 14)
        fim = time.time() + 10
        while time.time() < fim and "[PORTAPAPELES] imagen 201x121" not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("[PORTAPAPELES] imagen 201x121" in m.serial()[marca:], "Copiar guarda la imagen 201x121 en el portapapeles")

        # Guardar con nombre
        marca = len(m.serial())
        m.clique(ox + 12 + 45, oy + ph - 40 + 14)
        time.sleep(1.5)
        for _ in range(70):
            m.tecla("backspace", 0.02)
        m.digitar("/tmp/rec1.png", 0.05)
        m.tecla("ret")
        fim = time.time() + 20
        while time.time() < fim and "Guardado en /tmp/rec1.png" not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("Guardado en /tmp/rec1.png" in m.serial()[marca:], "Guardar con nombre escribe el archivo y avisa")

        # El aviso es pulsable: abre el Visor con el archivo (el aviso de Guardar sigue unos segundos)
        marca = len(m.serial())
        m.clique(1024 - 420 - 12 + 100, 28 + 10 + 30)
        fim = time.time() + 15
        # (con el diálogo común de guardar, mejora 103, el aviso de la captura anterior aún puede estar
        # apilado: vale pulsar cualquiera de los dos)
        pat_aviso = r"aviso pulsado: visor /tmp/(rec1|capturas/captura-\S+)\.png"
        while time.time() < fim and not re.search(pat_aviso, m.serial()[marca:]):
            time.sleep(0.3)
        verificar(re.search(pat_aviso, m.serial()[marca:]) is not None, "pulsar el aviso abre el archivo en el Visor")
        time.sleep(2)
        m.tecla("alt-f4", 0.3)   # cierra el Visor para volver a la vista previa
        time.sleep(1)

        # Abrir en Paint
        marca = len(m.serial())
        m.clique(ox + 208 + 60, oy + ph - 40 + 14)
        fim = time.time() + 20
        while time.time() < fim and 'paint:' not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("paint:" in m.serial()[marca:], "Abrir en Paint lanza Paint con el recorte")
        time.sleep(1.5)
        m.tecla("ctrl-v", 0.3)
        fim = time.time() + 10
        while time.time() < fim and "paint: pegada imagen 201x121" not in m.serial()[marca:]:
            time.sleep(0.3)
        verificar("paint: pegada imagen 201x121" in m.serial()[marca:], "Ctrl+V en Paint pega la imagen del portapapeles")
        m.captura("h97-recortes", guardar=False)

        # Píxeles del recorte guardado contra la pantalla de antes (volviendo al terminal por su barra de título)
        tx, ty = (int(v) for v in re.findall(r'janela \d+ criada por \d+: "Terminal" \d+x\d+ em \((\d+),(\d+)\)', m.serial())[-1])
        m.clique(tx + 150, ty + 10)
        time.sleep(1)
        out = m.comando("visor -i /tmp/rec1.png 0 0 100 60 200 120")
        verificar("201 x 121" in out, "el archivo guardado mide 201 x 121")
        buenos = 0
        for (x, y) in ((0, 0), (100, 60), (200, 120)):
            esperado = region_ppm(ppm0, 300 + x, 300 + y, 1, 1)[0]
            r = re.search(rf"pixel {x},{y}: ([0-9a-f]{{6}})", out)
            if r and int(r.group(1), 16) == esperado:
                buenos += 1
        verificar(buenos >= 2, f"los píxeles del recorte coinciden con la pantalla ({buenos}/3)")

    print("h97: PASSOU")


if __name__ == "__main__":
    main()
