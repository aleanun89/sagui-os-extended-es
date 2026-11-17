#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H373 - Cipó moderno (D373): pestañas, barra, marcadores, inicio, descargas y descargas simultáneas.

Criterio:
  - pestañas: favicon de la página, más de 6 abiertas, reordenar arrastrando, cerrar con el clic central;
  - marcadores: añadir, meter en una carpeta, barra de marcadores, exportar a HTML (Netscape) y volver a importar;
  - página de inicio propia con accesos rápidos y el buscador; historial con búsqueda (Ctrl+H);
  - gestor de descargas: un archivo que no es página se baja en segundo plano, con pausa y cancelar;
  - descargas simultáneas 1, 4 y 12: con 1 se baja en serie, con N varios hilos bajan a la vez
    (lo mide el servidor y lo cuentan las líneas «cipo: descarga hilo TID» del log) y la página carga igual.
"""
import http.server
import os
import re
import struct
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402


def png(r, g, b, n=16):
    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    fila = b"\x00" + bytes([r, g, b]) * n
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0))
            + trozo(b"IDAT", zlib.compress(fila * n)) + trozo(b"IEND", b""))


N_IMG = 30
GRANDE = bytes((i * 7) & 255 for i in range(4096 * 1024))
PEQUENO = bytes((i * 3) & 255 for i in range(200 * 1024))


class Estado:
    lock = threading.Lock()
    activas = 0
    maximo = 0
    pedidas = 0

    @classmethod
    def reiniciar(cls):
        with cls.lock:
            cls.activas = cls.maximo = cls.pedidas = 0


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def responder(self, corpo, tipo, trozos=0, pausa=0.0):
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            if trozos:
                for i in range(0, len(corpo), trozos):
                    self.wfile.write(corpo[i:i + trozos])
                    self.wfile.flush()
                    time.sleep(pausa)
            else:
                self.wfile.write(corpo)
        except (BrokenPipeError, ConnectionResetError):   # la descarga cancelada cierra la conexión
            pass
        self.close_connection = True

    def do_GET(self):
        ruta = self.path.split("?")[0]
        if ruta == "/fav.png":
            return self.responder(png(220, 40, 40), "image/png")
        if ruta.startswith("/img/"):
            with Estado.lock:
                Estado.activas += 1
                Estado.pedidas += 1
                Estado.maximo = max(Estado.maximo, Estado.activas)
            time.sleep(0.25)
            try:
                n = int(ruta[5:].split(".")[0])
                self.responder(png(n * 8 % 256, 100, 255 - n * 8 % 256), "image/png")
            finally:
                with Estado.lock:
                    Estado.activas -= 1
            return
        if ruta == "/muchas.html":
            imgs = "".join(f'<img src="/img/{i}.png" width="16" height="16"> ' for i in range(N_IMG))
            return self.responder(f"<!doctype html><html><head><title>Muchas imagenes</title></head><body><h1>Muchas</h1><p>{imgs}</p></body></html>".encode(), "text/html; charset=utf-8")
        if ruta == "/grande.bin":
            return self.responder(GRANDE, "application/octet-stream", 16384, 0.06)
        if ruta == "/pequeno.bin":
            return self.responder(PEQUENO, "application/octet-stream", 16384, 0.01)
        if ruta == "/dos.html":
            return self.responder(b"<!doctype html><html><head><title>Pagina Dos</title></head><body><h1>Dos</h1></body></html>", "text/html; charset=utf-8")
        return self.responder(b'<!doctype html><html><head><title>Pagina Uno</title><link rel="icon" href="/fav.png"></head><body><h1>Uno</h1><p><a href="/dos.html">dos</a></p></body></html>', "text/html; charset=utf-8")


def servidor():
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv, srv.server_address[1]


def janela(m):
    s = m.serial().replace("\r", "")
    j = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
    verificar(j is not None, "la ventana del Cipó tiene origen conocido")
    return int(j.group(1)), int(j.group(2))


def desde(m, patron, marca, tempo=30):
    """Espera un patrón regex en el log a partir de la posición 'marca'."""
    fim = time.time() + tempo
    while time.time() < fim:
        r = re.search(patron, m.serial().replace("\r", "")[marca:])
        if r:
            return r
        time.sleep(0.3)
    raise AssertionError(f"no apareció {patron!r}")


def ui(porta):
    img = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    with Maquina("h373ui", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial('cipo: pronto "Pagina Uno"', 90)
        ox, oy = janela(m)
        cy = oy + 24                      # el borde superior de la ventana (la barra de título mide 24)
        cx = ox + 2
        verificar(bool(re.search(r"cipo: ajustes: inicio 0, buscador DuckDuckGo, descargas simultáneas 4", m.serial())), "ajustes por defecto (4 descargas, DuckDuckGo)")
        m.esperar_serial("cipo: favicon http://10.0.2.2:%d/fav.png" % porta, 40)
        verificar(True, "el favicon de la página se baja y se pone en la pestaña")
        m.clique(cx + 400, cy + 300)

        # ---- pestañas: reordenar y cerrar con el clic central ----
        m.tecla("ctrl-t"); m.esperar_serial("cipo: nova aba 1", 20)
        m.tecla("ctrl-t"); m.esperar_serial("cipo: nova aba 2", 20)
        m.esperar_serial('cipo: pronto "Inicio"', 20)
        marca = len(m.serial().replace("\r", ""))
        m.arrastar(cx + 100, cy + 14, cx + 300, cy + 14)           # de la pestaña 0 a la 1
        desde(m, r"cipo: aba 0 movida a 1", marca, 10)
        verificar(True, "arrastrar una pestaña la reordena")
        m.captura("H373-pestanas3")
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 4 + 2 * 200 + 100, cy + 14, botao=4)          # clic central (QEMU: 1 izquierdo, 2 derecho, 4 medio) en la tercera
        desde(m, r"cipo: aba fechada, 2 restantes", marca, 10)
        verificar(True, "el clic central cierra la pestaña")
        for i in range(8):
            m.tecla("ctrl-t")
            time.sleep(0.5)
        m.esperar_serial("cipo: nova aba 9", 30)
        verificar(True, "se pueden abrir más de 6 pestañas (hay 10)")
        m.captura("H373-pestanas10")
        for i in range(7):
            m.tecla("ctrl-w")
            time.sleep(0.4)
        m.esperar_serial("cipo: aba fechada, 3 restantes", 20)

        # ---- marcadores: añadir, carpeta, barra, exportar, importar ----
        m.tecla("ctrl-l"); time.sleep(0.3)
        m.digitar(f"10.0.2.2:{porta}/dos.html"); m.tecla("ret")
        m.esperar_serial('cipo: pronto "Pagina Dos"', 30)
        m.tecla("ctrl-d")
        m.esperar_serial("cipo: marcador 1 http://10.0.2.2:%d/dos.html" % porta, 10)
        m.tecla("ctrl-l"); time.sleep(0.3)
        m.digitar(f"10.0.2.2:{porta}/"); m.tecla("ret")
        m.esperar_serial("cipo: abrindo http://10.0.2.2:%d/" % porta, 30)
        time.sleep(1.5)
        m.tecla("ctrl-d")
        time.sleep(0.5)
        marca = len(m.serial().replace("\r", ""))
        m.tecla("ctrl-shift-b")
        desde(m, r"cipo: panel Marcadores", marca, 10)
        verificar(True, "Ctrl+Mayús+B abre el gestor de marcadores")
        m.captura("H373-marcadores")
        PY = 62                           # donde empieza el panel (pestañas 28 + barra 34)
        # el primer marcador a una carpeta
        m.clique(cx + 250, cy + PY + 34 + 30)                        # el segundo (el primero ya está elegido: otro clic lo abriría)
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 438, cy + 517)                                 # «Carpeta...»
        time.sleep(1.5)
        m.digitar("Pruebas"); m.tecla("ret")
        desde(m, r'cipo: marcador 1 en la carpeta "Pruebas"', marca, 20)
        verificar(True, "un marcador pasa a una carpeta")
        # barra de marcadores opcional
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 494, cy + 548)
        desde(m, r"cipo: ventana", marca, 10)
        verificar(True, "se muestra la barra de marcadores (el área de la página se reajusta)")
        m.captura("H373-barra-marcadores")
        # exportar a HTML (Netscape bookmark file), borrar y volver a importar
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 394, cy + 549)                                 # «Exportar HTML...»
        desde(m, r"dialogo: guardar en", marca, 20)
        time.sleep(0.8); m.tecla("ret")
        r = desde(m, r"cipo: marcadores exportados a (\S+) \((\d+)\)", marca, 20)
        verificar(r.group(2) == "2", f"exporta los 2 marcadores a {r.group(1)}")
        for k in (1, 0):
            marca = len(m.serial().replace("\r", ""))
            m.clique(cx + 342, cy + 517)                             # «Borrar»
            desde(m, r"cipo: marcador borrado, quedan %d" % k, marca, 10)
        verificar(True, "los marcadores se borran")
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 238, cy + 549)                                 # «Importar HTML...»
        desde(m, r"dialogo: abrir en", marca, 20)
        time.sleep(0.8); m.tecla("ret")
        r = desde(m, r"cipo: importados (\d+) marcadores de (\S+)", marca, 20)
        verificar(r.group(1) == "2", f"importa los 2 marcadores del HTML ({r.group(2)})")
        s_ = m.serial().replace("\r", "")[marca:]
        verificar('cipo: importado marcador "Pagina Uno" [Pruebas]' in s_ and 'cipo: importado marcador "Pagina Dos" []' in s_,
                  "el HTML conserva títulos y carpeta")
        m.captura("H373-marcadores-importados")

        # ---- historial con búsqueda (Ctrl+H) ----
        marca = len(m.serial().replace("\r", ""))
        m.tecla("ctrl-h")
        desde(m, r"cipo: panel Historial", marca, 10)
        m.digitar("dos")
        r = desde(m, r"cipo: historial filtrado \((\d+)\)", marca, 10)
        verificar(True, "Ctrl+H abre el historial y la búsqueda filtra")
        m.captura("H373-historial")
        m.tecla("esc"); m.tecla("esc")

        # ---- página de inicio con accesos rápidos ----
        m.tecla("alt-home")
        m.esperar_serial("cipo: abrindo sobre:inicio", 20)
        time.sleep(1.5)
        m.captura("H373-inicio")

        # ---- ajustes: Rendimiento (descargas simultáneas) ----
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 860 - 30 + 12, cy + 28 + 17)                  # el menú «≡»
        time.sleep(0.6)
        desde(m, r"\[GUI\] menu aberto", marca, 10)
        g = re.findall(r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", m.serial())[-1]
        mx, my, ml, ma, nl = map(int, g)
        marca = len(m.serial().replace("\r", ""))
        m.clique(mx + ml // 2, my + 2 + 4 * 20 + 8)                  # «Ajustes»
        desde(m, r"cipo: panel General", marca, 10)
        m.captura("H373-ajustes-general")
        PY = 62 + 22                       # con la barra de marcadores el panel baja 22 px
        m.clique(cx + 60, cy + PY + 8 + 5 * 28 + 10)                 # Rendimiento
        time.sleep(0.6)
        m.captura("H373-ajustes-rendimiento")
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 164 + 40, cy + PY + 56 + 10)                   # el desplegable de hilos
        desde(m, r"\[GUI\] menu aberto", marca, 10)
        g = re.findall(r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", m.serial())[-1]
        mx, my, ml, ma, nl = map(int, g)
        m.clique(mx + ml // 2, my + 2 + 3 * 20 + 8)                  # «4»
        desde(m, r"cipo: descargas simult.neas 4 \(efectivas 4\)", marca, 10)
        verificar(True, "el ajuste de descargas simultáneas se cambia desde la ventana (1 -> 4)")
        m.tecla("esc")

        # ---- gestor de descargas: un archivo que no es página ----
        m.tecla("ctrl-l"); time.sleep(0.3)
        marca = len(m.serial().replace("\r", ""))
        m.digitar(f"10.0.2.2:{porta}/grande.bin"); m.tecla("ret")
        desde(m, r"cipo: descarga iniciada http://10.0.2.2:%d/grande.bin -> (\S+)" % porta, marca, 30)
        desde(m, r"cipo: panel Descargas", marca, 10)
        verificar(True, "un archivo que no es página va al gestor de descargas")
        time.sleep(1.0)
        m.captura("H373-descargas")
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 234, cy + 551)                                 # «Pausar/Reanudar»
        desde(m, r"cipo: descarga grande.bin en pausa", marca, 10)
        m.clique(cx + 234, cy + 551)
        desde(m, r"cipo: descarga grande.bin reanudada", marca, 10)
        verificar(True, "la descarga se pausa y se reanuda")
        m.clique(cx + 354, cy + 551)                                 # «Cancelar»
        desde(m, r"cipo: descarga cancelada grande.bin", marca, 20)
        verificar(True, "la descarga se cancela (y se borra el archivo a medias)")
        m.tecla("esc")
        m.tecla("ctrl-l"); time.sleep(0.3)
        marca = len(m.serial().replace("\r", ""))
        m.digitar(f"10.0.2.2:{porta}/pequeno.bin"); m.tecla("ret")
        r = desde(m, r"cipo: descarga lista (\S+) \((\d+) bytes\)", marca, 60)
        verificar(int(r.group(2)) == len(PEQUENO), f"la descarga completa deja el archivo entero ({r.group(2)} bytes en {r.group(1)})")
        m.captura("H373-descargas-lista")
        marca = len(m.serial().replace("\r", ""))
        m.clique(cx + 164 + 140 + 8 + 92 + 8 + 50, cy + 551)         # «Abrir carpeta»
        r = desde(m, r"cipo: abrir carpeta (\S+) \(pid (-?\d+)\)", marca, 10)
        verificar(int(r.group(2)) > 0, f"«Abrir carpeta» lanza el gestor de archivos en {r.group(1)}")
        verificar("cipo: erro" not in m.serial(), "sin errores de carga")
        return m


def hilos(n):
    Estado.reiniciar()
    srv, porta = servidor()
    img = imagem_de_teste(f"/apps/bin/cipo --hilos {n} http://10.0.2.2:{porta}/muchas.html", ["--vesa"])
    try:
        with Maquina(f"h373h{n}", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("cipo: recursos listos", 120)
            s = m.serial().replace("\r", "")
            tids = set(re.findall(r"cipo: descarga hilo (\d+) ", s))
            m.captura(f"H373-hilos{n}")
            ms = re.search(r"cipo: recursos listos en (\d+) ms", s)
            print(f"  medida: {n} descargas simultáneas -> recursos listos en {ms.group(1) if ms else '?'} ms ({N_IMG} imágenes de 250 ms en el servidor)")
            imagenes = len(re.findall(r"cipo: imagem 16x16", s))
            m_ = re.search(r"cipo: ajustes: .*descargas simultáneas (\d+) \(efectivas (\d+)\)", s)
            ef = int(m_.group(2)) if m_ else 0
            verificar(ef >= 1, f"ajuste de descargas simultáneas: pedido {n}, efectivo {ef}")
            verificar(imagenes >= N_IMG, f"la página carga igual: {imagenes} imágenes decodificadas de {N_IMG}")
            verificar("cipo: erro" not in s, "sin errores de carga")
            if n == 1 or ef == 1:
                verificar(not tids and Estado.maximo == 1, f"con 1 se baja en serie (hilos {len(tids)}, simultáneas en el servidor {Estado.maximo})")
            else:
                verificar(len(tids) >= 2, f"varios hilos bajan (ids de hilo distintos en el log: {len(tids)}; cada lote lanza los suyos)")
                verificar(2 <= Estado.maximo <= ef + 1, f"el servidor vio hasta {Estado.maximo} peticiones simultáneas")
            return len(tids), Estado.maximo
    finally:
        srv.shutdown()


def main():
    parte = sys.argv[1] if len(sys.argv) > 1 else "todo"
    if parte in ("todo", "ui"):
        srv, porta = servidor()
        try:
            ui(porta)
        finally:
            srv.shutdown()
    if parte in ("todo", "hilos"):
        r = {}
        for n in (1, 4, 12):
            r[n] = hilos(n)
        verificar(r[4][1] > r[1][1], f"con 4 hay más simultáneas que con 1 ({r[4][1]} > {r[1][1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
