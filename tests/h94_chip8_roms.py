#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H94 - chip8-roms y chip8-roms-gui: ROM del Chip8 Community Archive (D123).

Sin Internet: un servidor HTTP falso en el anfitrion (127.0.0.1, visto desde
SaguiOS como 10.0.2.2) sirve un programs.json reducido (3 programas, uno de
cada plataforma) y 3 ROM minimas generadas aqui (nada ajeno al repositorio).
El "puente" falla con HTTP 500 la primera vez para probar los reintentos.

Parte A (imagen normal, sin /home persistente -> /tmp/chip8/roms con aviso):
  lista, --plataforma, --buscar, info, bajar, bajar --todas, los archivos y su
  .json con la atribucion, error de clave, jugar (baja y lanza chip8 con el modo
  y la velocidad correctos, visto en el log del emulador), y el selector de
  chip8 con las ROM bajadas (+ Titulo).
Parte B (imagen con disco de datos, /home persistente -> /home/chip8/roms):
  la ventana chip8-roms-gui con el raton: filtro por plataforma, busqueda,
  seleccion con su descripcion, Descargar, Descargar todas, Actualizar indice y
  Jugar; capturas.
Manual (no entra en la bateria), contra GitHub de verdad:
  chip8-roms lista --buscar octojam ; chip8-roms jugar octojam1title
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402
import h68_python_tk as tk68  # noqa: E402  (la sonda y los ayudantes de ventanas)

# ---------------------------------------------------------------- servidor falso

ROMS = {
    "ladrillo": bytes.fromhex("a206d0051204f0909090f0"),
    "puente": bytes.fromhex("a206d0051204206020602060"),
    "aurora2": bytes.fromhex("a206d0051204f0101010f0"),
}
INDICE = {
    "ladrillo": {"title": "Ladrillo", "authors": ["Ana Prueba"], "desc": "Un ladrillo que dibuja un cero.",
                 "event": "PruebaJam", "release": "2020-01-02", "platform": "chip8",
                 "options": {"tickrate": "9", "shiftQuirks": True, "loadStoreQuirks": False, "clipQuirks": False,
                             "fillColor": "#FFAA00", "backgroundColor": "#AA4400"}},
    "puente": {"title": "Puente", "authors": ["Luis Prueba", "Eva Prueba"], "desc": "Un puente en SUPER-CHIP.",
               "event": "PruebaJam", "release": "2021-03-04", "platform": "schip",
               "options": {"tickrate": "40", "loadStoreQuirks": True, "vBlankQuirks": False, "jumpQuirks": True}},
    "aurora2": {"title": "Aurora Boreal", "authors": ["Eva Prueba"], "desc": "Colores de XO-CHIP.",
                "event": "OtraJam", "release": "2022-05-06", "platform": "xochip",
                "options": {"tickrate": "500", "vBlankQuirks": False, "logicQuirks": False}},
}
peticiones = []
falla_puente = [True]


class Manejador(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *a):
        pass

    def do_GET(self):
        peticiones.append(self.path)
        if self.path == "/programs.json":
            cuerpo = json.dumps(INDICE, indent="\t").encode()
        elif self.path.startswith("/roms/") and self.path.endswith(".ch8") and self.path[6:-4] in ROMS:
            clave = self.path[6:-4]
            if clave == "puente" and falla_puente[0]:
                falla_puente[0] = False
                self.send_response(500)
                self.end_headers()
                return
            cuerpo = ROMS[clave]
        else:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(cuerpo)))
        self.end_headers()
        self.wfile.write(cuerpo)


def servidor():
    s = ThreadingHTTPServer(("127.0.0.1", 0), Manejador)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    return s, s.server_address[1]


# ---------------------------------------------------------------- parte A

def parte_a(puerto):
    url = f"--url http://10.0.2.2:{puerto}/"
    cr = f"chip8-roms %s {url}"
    with Maquina("h94a", "ide", extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("chip8-roms")
        verificar("chip8-roms <orden>" in out and "CC0" in out and "chip8Archive" in out,
                  "chip8-roms sin argumentos muestra la ayuda con la licencia CC0")
        out = m.comando(cr % "lista", 120)
        verificar("Ladrillo" in out and "Puente" in out and "Aurora Boreal" in out and "3 programas, 0 descargados" in out
                  and "[ ] ladrillo" in out and "Ana Prueba" in out, "lista: 3 programas con titulo, plataforma y autores")
        verificar(out.index("Aurora Boreal") < out.index("Ladrillo") < out.index("Puente"), "lista ordenada por titulo")
        out = m.comando(cr % "lista --plataforma schip", 60)
        verificar("Puente" in out and "Ladrillo" not in out and "1 programas" in out, "lista --plataforma schip")
        out = m.comando(cr % "lista --buscar eva", 60)
        verificar("Puente" in out and "Aurora" in out and "Ladrillo" not in out, "lista --buscar eva (autores)")
        out = m.comando(cr % "info puente", 60)
        verificar("Luis Prueba, Eva Prueba" in out and "PruebaJam" in out and "2021-03-04" in out
                  and "chip8 -m schip -v 40 -q memoria=fija -q salto=vx -q vblank=no" in out,
                  "info: descripcion, autores, evento, fecha y las opciones traducidas")
        out = m.comando(cr % "info ladrillo", 60)
        verificar("-q desplazar=vx" in out and "-q borde=envuelve" in out and "aviso: el emulador no soporta: backgroundColor, fillColor" in out,
                  "info: quirks del indice y aviso de los colores sin soporte")
        out = m.comando(cr % "info noexiste", 60)
        verificar("no existe 'noexiste'" in out, "info de una clave desconocida da error")

        out = m.comando(cr % "bajar ladrillo", 120)
        verificar("/home no es persistente" in out and "/tmp/chip8" in out and "1 de 1 descargadas" in out,
                  "bajar: aviso de que /home no es persistente y ROM guardada en /tmp/chip8/roms")
        esperado = hashlib.sha256(ROMS["ladrillo"]).hexdigest()
        out = m.comando("sha256sum /tmp/chip8/roms/ladrillo.ch8", 30)
        verificar(esperado in out, "la ROM bajada es identica a la del servidor (SHA-256)")
        out = m.comando("cat /tmp/chip8/roms/ladrillo.json", 30)
        verificar('"titulo": "Ladrillo"' in out and "CC0, Chip8 Community Archive" in out and "Ana Prueba" in out
                  and '"plataforma": "chip8"' in out, "ladrillo.json: titulo, autores y atribucion CC0")
        out = m.comando(cr % "bajar --todas --plataforma schip", 120)
        verificar("reintento 1 de 2" in out and "1 de 1 descargadas" in out, "bajar --todas --plataforma schip: reintenta tras el HTTP 500")
        out = m.comando("ls /tmp/chip8/roms", 30)
        verificar("puente.sc8" in out and "puente.json" in out, "la ROM SUPER-CHIP se guarda como .sc8")
        out = m.comando(cr % "bajar noexiste", 60)
        verificar("no existe 'noexiste'" in out, "bajar de una clave desconocida da error")
        out = m.comando(cr % "lista", 60)
        verificar("[x] ladrillo" in out and "[x] puente" in out and "[ ] aurora2" in out and "3 programas, 2 descargados" in out,
                  "lista marca las descargadas")
        out = m.comando(cr % "bajar --todas", 120)
        verificar("3 de 3 descargadas" in out, "bajar --todas descarga todo")
        out = m.comando("ls /tmp/chip8/roms | wc -l", 30)
        verificar("\n6" in out or " 6" in out, "3 ROM + 3 .json en la carpeta")
        m.comando("rm /tmp/chip8/roms/aurora2.xo8 /tmp/chip8/roms/aurora2.json", 30)

        # el selector de chip8 muestra las ROM bajadas con su titulo
        ini = len(m.serial())
        m.comando("chip8 &")
        j = tk68.janela(m, "CHIP-8", ini, 40)
        time.sleep(2)
        tela = tk68.captura(m, "selector")
        verificar(tk68.tem_texto(tela, tk68.regiao(j), "+ Ladrillo") and tk68.tem_texto(tela, tk68.regiao(j), "+ Puente"),
                  "el selector de chip8 lista las ROM bajadas con su titulo (+ Ladrillo, + Puente)")
        m.captura("H94-chip8-selector")
        tk68.fechar(m, j)
        time.sleep(1)

        # jugar: baja la que falta y lanza el emulador con las opciones del indice
        ini = len(m.serial())
        m.digitar((cr % "jugar aurora2") + "\n", 0.04)
        m.esperar_serial("chip8: rom /tmp/chip8/roms/aurora2.xo8", 120)
        s = m.serial()[ini:]
        verificar("[1/1] aurora2" in s and "listo: /tmp/chip8/roms/aurora2.xo8" in s, "jugar: baja la ROM que falta")
        verificar("chip8 -m xochip -v 500 -q logicas=conserva -q vblank=no /tmp/chip8/roms/aurora2.xo8" in s,
                  "jugar: linea del emulador con modo, velocidad y quirks")
        verificar("modo xochip, 500 instr/cuadro" in s, "el emulador arranca en modo XO-CHIP a 500 instrucciones por cuadro")
        time.sleep(3)
        m.captura("H94-chip8-jugar")
    return 0


# ---------------------------------------------------------------- parte B

def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h94-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    os.makedirs(os.path.join(raiz, "t"))
    with open(os.path.join(raiz, "t", "sonda.py"), "w") as f:
        f.write(tk68.SONDA)
    sgfs = os.path.join(SAIDA, "h94.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h94.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", "32"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def parte_b(puerto):
    img = construir_imagen()
    with Maquina("h94b", "ide", imagem=img, extras=["-nic", "user,model=e1000"], snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        ini = len(m.serial())
        m.digitar(f"python /t/sonda.py /bin/chip8-roms-gui --url http://10.0.2.2:{puerto}/\n", 0.03)
        j = tk68.janela(m, "ROM de CHIP-8", ini, 90)
        tk68.esperar(m, r"chip8-roms-gui: 3 programas, 0 descargados", 90, ini)
        comps, _ = tk68.sonda(m, "ROM de CHIP-8", ini, 60)
        lista = next(v for k, v in comps.items() if "listbox" in k.lower())
        entrada = next(v for k, v in comps.items() if "entry" in k.lower())
        verificar("no es persistente" not in m.serial()[ini:] and not any("/home no es persistente" in v[0] for v in comps.values()),
                  "con disco de datos no sale el aviso de /home")
        time.sleep(1)
        m.captura("H94-gui-inicio")

        def marca():
            return len(m.serial())

        def clic_lista(fila):
            _, x, y, l, a = lista
            m.clique(j["x"] + x + 30, j["y"] + y + 6 + 16 * fila)

        def boton(texto):
            tk68.clicar_em(m, j, comps[texto])

        # filtro por plataforma
        d = marca()
        boton("SUPER-CHIP")
        tk68.esperar(m, r"chip8-roms-gui: 1 programas, 0 descargados", 20, d)
        boton("Todas")
        tk68.esperar(m, r"chip8-roms-gui: 3 programas, 0 descargados", 20, d)
        verificar(True, "filtro por plataforma (SUPER-CHIP -> 1, Todas -> 3)")
        # busqueda
        d = marca()
        tk68.clicar_em(m, j, entrada)
        m.digitar("boreal", 0.1)
        tk68.esperar(m, r"chip8-roms-gui: 1 programas, 0 descargados", 20, d)
        verificar(True, "busqueda 'boreal' deja 1 programa")
        d = marca()
        for _ in range(6):
            m.tecla("backspace", 0.1)
        tk68.esperar(m, r"chip8-roms-gui: 3 programas, 0 descargados", 20, d)
        # seleccion y detalle
        d = marca()
        clic_lista(1)
        tk68.esperar(m, r"chip8-roms-gui: seleccionada ladrillo", 20, d)
        d = marca()
        clic_lista(0)
        tk68.esperar(m, r"chip8-roms-gui: seleccionada aurora2", 20, d)
        time.sleep(1)
        tela = tk68.captura(m, "gui-detalle")
        reg = tk68.regiao(j)
        verificar(tk68.tem_texto(tela, reg, "Colores de XO-CHIP.") and tk68.tem_texto(tela, reg, "Eva Prueba")
                  and tk68.tem_texto(tela, reg, "CC0, Chip8 Community Archive"),
                  "al seleccionar: descripcion, autores y licencia CC0 en el panel")
        m.captura("H94-gui-detalle")
        # descargar
        d = marca()
        boton("Descargar")
        tk68.esperar(m, r"chip8-roms-gui: descargada aurora2", 60, d)
        verificar("chip8-roms-gui: guardada /home/chip8/roms/aurora2.xo8" in m.serial()[d:],
                  "Descargar: la ROM va a /home/chip8/roms (persistente)")
        # descargar todas
        d = marca()
        boton("Descargar todas")
        tk68.esperar(m, r"chip8-roms-gui: terminado: 2 de 2 descargadas", 120, d)
        s = m.serial()[d:]
        verificar("guardada /home/chip8/roms/ladrillo.ch8" in s and "guardada /home/chip8/roms/puente.sc8" in s,
                  "Descargar todas: las 2 que faltaban")
        time.sleep(1)
        m.captura("H94-gui-descargadas")
        # actualizar indice
        d = marca()
        boton("Actualizar indice")
        tk68.esperar(m, r"chip8-roms-gui: 3 programas, 3 descargados", 60, d)
        verificar(peticiones.count("/programs.json") >= 2, "Actualizar indice vuelve a pedir programs.json")
        # jugar
        d = marca()
        boton("Jugar")
        tk68.esperar(m, r"chip8: rom /home/chip8/roms/aurora2.xo8", 60, d)
        s = m.serial()[d:]
        verificar("chip8-roms-gui: lanzo chip8 -m xochip -v 500 -q logicas=conserva -q vblank=no /home/chip8/roms/aurora2.xo8" in s
                  and "modo xochip, 500 instr/cuadro" in s, "Jugar lanza chip8 con modo, velocidad y quirks del indice")
        time.sleep(3)
        m.captura("H94-gui-jugar")
    return 0


def main():
    srv, puerto = servidor()
    try:
        parte_a(puerto)
        parte_b(puerto)
    finally:
        srv.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
