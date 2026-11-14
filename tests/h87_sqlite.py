#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H87 (D116, mejora 064) - SQLite 3 en SaguiOS: programa sqlite3, módulo Python y llamadas
truncar/sincronizar.

Un pendrive con una partición ext2 y otra FAT32 (las dos se montan, como /home persistente). En
cada una: el guion SQL de tests/recursos/sqlite/guion.sql (tablas, índices, JOIN, vista, ROLLBACK,
PRAGMA integrity_check, VACUUM, modos csv y column) se ejecuta con el sqlite3 de SaguiOS y la salida
tiene que ser idéntica byte a byte a la del sqlite3 del anfitrión; una base creada en el anfitrión se
lee y se modifica desde SaguiOS (y el anfitrión la relee); un guion de Python (import sqlite3,
parámetros, fetchall, commit, rollback, excepciones) da lo mismo que el sqlite3 de CPython del
anfitrión. Se reinicia con el mismo pendrive y se relee. Al final: e2fsck, fsck.vfat y
PRAGMA integrity_check en el anfitrión sobre las bases escritas por SaguiOS.
"""
import os
import sqlite3
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, RAIZ)

REC = os.path.join(RAIZ, "tests", "recursos", "sqlite")
GUION = open(os.path.join(REC, "guion.sql"), "rb").read()
PRUEBA = open(os.path.join(REC, "prueba.py"), "rb").read()
PRUEBA2 = open(os.path.join(REC, "prueba2.py"), "rb").read()   # row_factory, liberación y cerrojos (D118)
ENV_FAT = dict(os.environ, MTOOLS_SKIP_CHECK="1")


def sqlite_host(base, guion=None, args=()):
    """Salida del sqlite3 del anfitrión (o del módulo de Python si no hay programa)."""
    r = subprocess.run(["sqlite3", *args, base], input=guion, capture_output=True)
    if r.returncode != 0:
        print(r.stderr.decode())
    verificar(r.returncode == 0, "el sqlite3 del anfitrión ejecuta el guion")
    return r.stdout


def base_anfitrion(ruta):
    """Base creada en el anfitrión que SaguiOS leerá y modificará."""
    if os.path.exists(ruta):
        os.remove(ruta)
    con = sqlite3.connect(ruta)
    con.executescript("""
        CREATE TABLE notas(id INTEGER PRIMARY KEY, texto TEXT, valor REAL);
        CREATE INDEX idx_valor ON notas(valor);
    """)
    con.executemany("INSERT INTO notas(texto, valor) VALUES (?, ?)", [(f"nota {i} del anfitrión", i * 0.25) for i in range(300)])
    con.commit()
    con.close()


def ejecutar_en(m, v, tipo):
    """Todo lo que se hace en un volumen montado en v. Las salidas van a archivos del volumen."""
    m.comando(f"sqlite3 {v}/s.db \".read {v}/guion.sql\" > {v}/salida.txt", tempo=240)
    out = m.comando(f"sqlite3 {v}/host.db \"select count(*), round(sum(valor),2) from notas\" > {v}/lee.txt")
    m.comando(f"sqlite3 {v}/host.db \"insert into notas(texto, valor) values ('desde SaguiOS', 999)\" \"delete from notas where id <= 10\" "
              f"\"select count(*) from notas\" > {v}/lee2.txt")
    m.comando(f"sqlite3 {v}/host.db \"pragma integrity_check\" > {v}/ic.txt")
    m.comando(f"python {v}/prueba.py {v} > {v}/py.txt", tempo=120)
    m.comando(f"rm {v}/p.db")
    m.comando(f"python-x87 {v}/prueba.py {v} > {v}/py87.txt", tempo=120)   # la variante sin SSE2 trae el mismo módulo
    # D118: row_factory, liberación automática y cerrojo entre dos procesos (el segundo escribe mientras el primero tiene la transacción)
    m.comando(f"python {v}/prueba2.py {v} > {v}/py2.txt", tempo=240)
    m.comando(f"python {v}/prueba2.py {v} escritor > {v}/pe.txt & python {v}/prueba2.py {v} intento > {v}/pi.txt; wait", tempo=240)
    out = m.comando(f"ls {v}")
    verificar("l.db-lock" not in out, f"[{tipo}] sin cerrojos huérfanos tras los dos procesos")


def main():
    base_anfitrion(os.path.join(tempfile.gettempdir(), "h87_host.db"))
    host_db = open(os.path.join(tempfile.gettempdir(), "h87_host.db"), "rb").read()
    arq = {"guion.sql": GUION, "prueba.py": PRUEBA, "prueba2.py": PRUEBA2, "host.db": host_db}
    particoes = [
        dict(tipo="ext2", mb=24, rotulo="SQEXT", arquivos=dict(arq), opcoes=["-b", "4096", "-I", "256"]),
        dict(tipo="fat32", mb=40, rotulo="SQFAT", arquivos=dict(arq)),
    ]
    img, ini = criar_disco("h87-pen", particoes)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h87a", "ide", imagem=imagem_console(apps=True), extras=extras) as m:   # D194: sqlite3 en /apps
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("mount usb0p1 /home")
        verificar("montado en /home" in out, "ext2 montado en /home")
        out = m.comando("mount -t fat32 usb0p2 /fat")
        verificar("montado en /fat" in out, "FAT32 montado en /fat")
        out = m.comando("sqlite3 :memory: \"select 1+1, 'hola'\"")
        verificar("2|hola" in out, "sqlite3 en la línea de órdenes (base en memoria)")
        for v in ("/tmp", "/home", "/fat"):   # SIS_TRUNCAR y SIS_SINCRONIZAR: RAM, ext2 y FAT32
            out = m.comando(f"/testes/truncar {v}/tr.bin")
            verificar("truncar: OK" in out, f"truncar y sincronizar en {v}")
        for v, tipo in (("/home", "ext2"), ("/fat", "fat32")):
            ejecutar_en(m, v, tipo)
        m.captura("H87-sqlite")
        verificar("[TESTE] falha" not in m.serial() and "PANICO" not in m.serial(), "nenhum erro no kernel")
        m.comando("umount /home")
        m.comando("umount /fat")
    with Maquina("h87b", "ide", imagem=imagem_console(apps=True), extras=extras) as m:   # "reiniciar": mismo pendrive
        m.esperar_serial("sagui:/$", 120)
        m.comando("mount usb0p1 /home")
        m.comando("mount -t fat32 usb0p2 /fat")
        for v in ("/home", "/fat"):
            m.comando(f"sqlite3 {v}/s.db \"pragma integrity_check\" \"select count(*), min(id), max(id) from pedidos\" \".tables\" > {v}/b.txt")
        out = m.comando("sqlite3 /fat/s.db \"select count(*) from clientes\"")
        verificar("500" in out, "tras reiniciar, la base de la FAT32 se relee")
        m.comando("umount /home")
        m.comando("umount /fat")

    # ---- el anfitrión comprueba
    with tempfile.TemporaryDirectory() as t:
        esperado = sqlite_host(os.path.join(t, "ref.db"), GUION)
        r = subprocess.run([sys.executable, "-I", os.path.join(REC, "prueba.py"), t], capture_output=True)   # el mismo guion en CPython
        verificar(r.returncode == 0, "el guion de Python corre en el CPython del anfitrión")
        esperado_py = r.stdout
        r = subprocess.run([sys.executable, "-I", os.path.join(REC, "prueba2.py"), t], capture_output=True)
        verificar(r.returncode == 0, "prueba2.py (row_factory, liberación) corre en el CPython del anfitrión")
        esperado_py2 = r.stdout
        p1 = extrair_particao(img, ini[0], 24)
        p2 = extrair_particao(img, ini[1], 40)

        def de_ext2(ruta):
            return debugfs(p1, f"cat {ruta}")

        def de_fat(ruta):
            r = subprocess.run(["mtype", "-i", p2, f"::/{ruta}"], capture_output=True, env=ENV_FAT)
            return r.stdout

        def bajar(ruta, destino):
            if destino.endswith(".e"):
                open(destino, "wb").write(de_ext2(ruta))
            else:
                r = subprocess.run(["mcopy", "-o", "-i", p2, f"::/{ruta}", destino], capture_output=True, env=ENV_FAT)
                verificar(r.returncode == 0, "mcopy")
            return destino

        for nombre, leer, ext in (("ext2", de_ext2, ".e"), ("FAT32", de_fat, ".f")):
            ruta = "/" if nombre == "ext2" else ""
            sal = leer(ruta + "salida.txt") if nombre == "ext2" else leer("salida.txt")
            if sal != esperado:
                open(os.path.join(t, "esperado.txt"), "wb").write(esperado)
                open(os.path.join(t, "obtenido.txt"), "wb").write(sal)
                subprocess.run(["diff", os.path.join(t, "esperado.txt"), os.path.join(t, "obtenido.txt")])
            verificar(sal == esperado, f"[{nombre}] la salida del guion SQL es idéntica a la del sqlite3 del anfitrión ({len(esperado)} bytes)")
            py = leer(ruta + "py.txt") if nombre == "ext2" else leer("py.txt")
            if py != esperado_py:
                print("esperado:\n", esperado_py.decode(), "\nobtenido:\n", py.decode(errors="replace"))
            verificar(py == esperado_py, f"[{nombre}] Python: import sqlite3, parámetros, fetchall, commit, rollback = CPython")
            py2 = leer(ruta + "py2.txt") if nombre == "ext2" else leer("py2.txt")
            if py2 != esperado_py2:
                print("esperado:\n", esperado_py2.decode(), "\nobtenido:\n", py2.decode(errors="replace"))
            verificar(py2 == esperado_py2, f"[{nombre}] row_factory (sqlite3.Row, funciones), liberación automática de 200 conexiones y finalizadores = CPython")
            pe = leer(ruta + "pe.txt") if nombre == "ext2" else leer("pe.txt")
            pi = leer(ruta + "pi.txt") if nombre == "ext2" else leer("pi.txt")
            if b"database is locked" not in pi or b"intento largo: ok" not in pi:
                print("escritor:\n", pe.decode(errors="replace"), "\nintento:\n", pi.decode(errors="replace"))
            verificar(b"escritor: libre" in pe, f"[{nombre}] el proceso escritor termina y suelta el cerrojo")
            verificar(b"intento corto: database is locked" in pi, f"[{nombre}] un segundo proceso que escribe recibe 'database is locked'")
            verificar(b"intento largo: ok 2" in pi, f"[{nombre}] con timeout largo, el segundo proceso espera y escribe después")
            py87 = leer(ruta + "py87.txt") if nombre == "ext2" else leer("py87.txt")
            verificar(py87 == esperado_py, f"[{nombre}] python-x87: la misma salida que CPython")
            ic = leer(ruta + "ic.txt") if nombre == "ext2" else leer("ic.txt")
            verificar(ic.strip() == b"ok", f"[{nombre}] PRAGMA integrity_check de la base del anfitrión modificada en SaguiOS")
            lee = leer(ruta + "lee.txt") if nombre == "ext2" else leer("lee.txt")
            verificar(lee.strip() == b"300|" + str(round(sum(i * 0.25 for i in range(300)), 2)).encode(),
                      f"[{nombre}] SaguiOS lee la base creada en el anfitrión")
            lee2 = leer(ruta + "lee2.txt") if nombre == "ext2" else leer("lee2.txt")
            verificar(lee2.strip() == b"291", f"[{nombre}] insertar y borrar en la base del anfitrión desde SaguiOS")
            b = leer(ruta + "b.txt") if nombre == "ext2" else leer("b.txt")
            verificar(b.startswith(b"ok\n") and b"1000|1|1999" in b, f"[{nombre}] tras reiniciar: integrity_check ok y los datos siguen")
            # bases escritas por SaguiOS, abiertas por el sqlite del anfitrión
            for base in ("s.db", "host.db", "p.db"):
                dest = os.path.join(t, nombre + "_" + base)
                if nombre == "ext2":
                    open(dest, "wb").write(de_ext2("/" + base))
                else:
                    bajar(base, dest)
                con = sqlite3.connect(dest)
                res = con.execute("pragma integrity_check").fetchone()[0]
                verificar(res == "ok", f"[{nombre}] el anfitrión abre {base} escrita por SaguiOS: integrity_check = ok")
                if base == "host.db":
                    verificar(con.execute("select count(*) from notas where texto='desde SaguiOS'").fetchone()[0] == 1,
                              f"[{nombre}] el anfitrión ve la fila insertada en SaguiOS")
                if base == "s.db":
                    verificar(con.execute("select count(*) from clientes").fetchone()[0] == 500, f"[{nombre}] el anfitrión lee s.db")
                con.close()
        cod, txt = e2fsck(p1)
        if cod:
            print(txt)
        verificar(cod == 0, "e2fsck -fn: ext2 íntegro tras las bases")
        r = subprocess.run(["fsck.vfat", "-n", p2], capture_output=True, text=True)
        verificar(r.returncode == 0, "fsck.vfat: FAT32 íntegra tras las bases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
