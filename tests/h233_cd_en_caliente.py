#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H233 (D233) - meter y sacar un CD/DVD con el sistema en marcha; UDF.

El fallo que vio el mantenedor: «cuando meto un CD/DVD no puedo montarlo y ver su contenido».
Las pruebas del D207 arrancaban siempre con el disco ya dentro. Aquí se arranca con el lector
VACÍO y los discos se meten y se sacan por el monitor del QEMU (change / eject), en el IDE
(ATAPI por PIO) y en el AHCI (SATAPI):

  - sin disco: "discos" dice "sin disco" y "mount cd0 /cd" lo explica (no "formato no soportado");
  - se mete un ISO 9660 + Joliet: el kernel lo ve solo (tarea "cdrom"), lo monta en /cd, avisa;
    "ls /cd" con nombres largos y un archivo de 300 KB con su SHA-256;
  - un "umount /cd" a mano no se deshace solo;
  - "eject" (START STOP UNIT) desmonta y abre la bandeja; se mete OTRO disco y se monta el nuevo;
  - "eject" por el monitor (como sacarlo con la mano) desmonta solo y avisa;
  - un disco SOLO UDF (2.01, con directorios y archivos dentro de su File Entry y un archivo en
    tres trozos con la lista de extensiones en un Allocation Extent Descriptor): se monta (udf);
  - un UDF bridge con un ISO de nombres 8.3: se monta el UDF (nombres largos); "mount -t iso9660"
    monta también el ISO (nombres 8.3); el mismo bridge con Joliet: se prefiere el ISO;
  - cambiar el disco montado por otro directamente (change sin eject): se ve el nuevo.
Las imágenes las hace tools/mkcd_dados.py (no hay genisoimage ni mkudffs en el entorno).

Parte "unidad" (en el PC, sin QEMU): el CD de audio (sistema de archivos cdda). El QEMU no emula
pistas de audio (-cdrom y los .cue solo dan datos al ATAPI), así que tests/cdda_unidad.c compila
kernel/fs/cdda.c y cdda_logica.c con un lector simulado: tabla de contenido (audio, mixto, Enhanced
CD), CD-TEXT, nombres, cabecera WAV, lecturas con desplazamientos raros, la caché de 13 sectores,
el disco sacado, y que Sintonía (sintonia_logica.h) lee Lista.m3u y la duración. Aquí, además, el
WAV que sale se abre con el módulo "wave" de Python.

Uso: h233_cd_en_caliente.py [unidad] [ide] [ahci]
"""
import hashlib
import os
import random
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA, FalhaTeste  # noqa: E402

GRANDE = "docs/sub/grande.bin"
LARGO = "nombre largo de un archivo con espacios para probar.txt"


def arvore(destino, semente, texto):
    """El árbol de archivos de un disco: lo mismo en todos, con contenido distinto por disco."""
    if os.path.exists(destino):
        shutil.rmtree(destino)
    os.makedirs(os.path.join(destino, "docs", "sub"))
    os.makedirs(os.path.join(destino, "vacia"))
    r = random.Random(semente)
    arqs = {
        "leeme.txt": (texto + "\n").encode(),
        "docs/" + LARGO: b"linea larga\n" * 50,
        "docs/canción.txt": "canción con acentos\n".encode("latin-1"),
        GRANDE: bytes(r.getrandbits(8) for _ in range(300000)),
        "vacio.txt": b"",
    }
    for n, d in arqs.items():
        with open(os.path.join(destino, n), "wb") as f:
            f.write(d)
    return {n: hashlib.sha256(d).hexdigest() for n, d in arqs.items()}


def disco(nome, semente, texto, *opcoes, rotulo):
    pasta = os.path.join(SAIDA, f"h233-{nome}")
    hashes = arvore(pasta, semente, texto)
    iso = os.path.join(SAIDA, f"h233-{nome}.iso")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkcd_dados.py"), "--pasta", pasta, "--saida", iso,
                    "--rotulo", rotulo, *opcoes], check=True, stdout=subprocess.DEVNULL)
    shutil.rmtree(pasta)
    return iso, hashes


def esperar_novo(m, texto, desde, tempo=40):
    """Espera a que "texto" salga en la serial después de la posición "desde"."""
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()
        if texto in s[desde:]:
            return s[desde:]
        if m.proc.poll() is not None:
            raise FalhaTeste("el QEMU ha terminado")
        time.sleep(0.3)
    return None


def meter(m, disp, iso):
    desde = len(m.serial())
    r = m.monitor(f"change {disp} {iso} raw")
    verificar("rror" not in r, f"monitor: change {os.path.basename(iso)} ({r.strip()[:120]})")
    return desde


def comprobar_contenido(m, rotulo, hashes, nomes_longos=True):
    out = m.comando("ls /cd")
    verificar("leeme.txt" in out and "docs" in out, f"[{rotulo}] ls /cd: leeme.txt y docs")
    out = m.comando("ls /cd/docs")
    if nomes_longos:
        verificar("nombre largo de un archivo con espacios" in out, f"[{rotulo}] ls /cd/docs: nombre largo")
        verificar("canción.txt" in out, f"[{rotulo}] ls /cd/docs: nombre con acento (Latin-1)")
    out = m.comando(f"sha256sum /cd/{GRANDE}", 120)
    verificar(hashes[GRANDE] in out, f"[{rotulo}] sha256sum /cd/{GRANDE} (300 KB) coincide con el original")


def parte(nome, extras, disp, discos):
    a_iso, a_h = discos["a"]
    b_iso, b_h = discos["b"]
    u_iso, u_h = discos["udf"]
    p_iso, p_h = discos["bridge"]
    j_iso, _ = discos["bridgej"]
    with Maquina(f"h233-{nome}", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        print("\n".join(l for l in s.splitlines() if "[ATAPI]" in l)[:2000])
        verificar(re.search(r"\[ATAPI\] .* es cd0: sin disco", s), f"[{nome}] arranca con el lector vacío: cd0 sin disco")
        verificar("[ATAPI] vigilancia de 1 lector" in s, f"[{nome}] la tarea de vigilancia del lector está en marcha")
        out = m.comando("discos")
        verificar(re.search(r"cd0 .*sin disco", out), f"[{nome}] discos: cd0 sin disco")
        out = m.comando("mount cd0 /cd")
        verificar("mount: cd0: sin disco" in out, f"[{nome}] mount sin disco lo dice ({out.strip()[-80:]})")

        # --- 1. se mete un ISO 9660 + Joliet: se monta solo en /cd
        desde = meter(m, disp, a_iso)
        s = esperar_novo(m, 'aviso: CD/DVD "DATOS-A" montado en /cd', desde)
        verificar(s is not None, f"[{nome}] el disco metido en caliente se monta solo en /cd y se avisa")
        if s:
            verificar("se ha metido un disco" in s, f"[{nome}] log: se ha metido un disco")
        out = m.comando("mount")
        verificar(re.search(r"cd0\s+en /cd\s+tipo iso9660\s+\(ro", out), f"[{nome}] mount: cd0 en /cd, iso9660, solo lectura")
        comprobar_contenido(m, f"{nome}/iso", a_h)
        out = m.comando("cat /cd/leeme.txt")
        verificar("disco A" in out, f"[{nome}] cat /cd/leeme.txt es el del disco A")
        out = m.comando("discos")
        verificar(re.search(r"cd0 .*Joliet.*\"DATOS-A\".*montado en /cd", out), f"[{nome}] discos: cd0 con su etiqueta, montado")

        # --- 2. umount a mano: no se vuelve a montar solo
        m.comando("umount /cd")
        time.sleep(5)
        out = m.comando("mount")
        verificar("/cd " not in out and "cd0" not in out, f"[{nome}] un umount a mano no se deshace solo")
        out = m.comando("mount cd0 /cd")
        verificar("montado en /cd" in out, f"[{nome}] mount cd0 /cd a mano otra vez")

        # --- 3. eject desde el sistema (START STOP UNIT): desmonta y abre la bandeja
        desde = len(m.serial())
        out = m.comando("eject")
        verificar("cd0: disco expulsado" in out, f"[{nome}] eject: disco expulsado ({out.strip()[-80:]})")
        verificar("abrir la bandeja: hecho" in m.serial()[desde:], f"[{nome}] el lector ha abierto la bandeja")
        out = m.comando("mount")
        verificar("cd0" not in out, f"[{nome}] tras eject, nada montado del CD")
        info = m.monitor("info block")
        verificar(re.search(r"[Tt]ray open", info) or "not inserted" in info, f"[{nome}] el QEMU ve la bandeja abierta")

        # --- 4. otro disco distinto
        desde = meter(m, disp, b_iso)
        s = esperar_novo(m, 'aviso: CD/DVD "DATOS-B" montado en /cd', desde)
        verificar(s is not None, f"[{nome}] el segundo disco se monta solo en /cd")
        out = m.comando("cat /cd/leeme.txt")
        verificar("disco B" in out, f"[{nome}] cat /cd/leeme.txt es el del disco B")
        out = m.comando(f"sha256sum /cd/{GRANDE}", 120)
        verificar(b_h[GRANDE] in out and a_h[GRANDE] not in out, f"[{nome}] sha256sum del disco B (no del A)")

        # --- 5. sacarlo "con la mano" (eject del monitor): se desmonta solo
        desde = len(m.serial())
        m.monitor(f"eject -f {disp}")
        s = esperar_novo(m, "aviso: CD/DVD retirado", desde)
        verificar(s is not None, f"[{nome}] al sacar el disco, se desmonta solo y se avisa")
        out = m.comando("mount")
        verificar("cd0" not in out, f"[{nome}] tras sacarlo, nada montado del CD")
        out = m.comando("ls /cd")
        verificar("leeme.txt" not in out, f"[{nome}] /cd ya no muestra el disco")

        # --- 6. un disco solo UDF
        desde = meter(m, disp, u_iso)
        s = esperar_novo(m, 'aviso: CD/DVD "Datos UDF" montado en /cd', desde)
        verificar(s is not None, f"[{nome}] el disco solo UDF se monta solo en /cd")
        out = m.comando("mount")
        verificar(re.search(r"cd0\s+en /cd\s+tipo udf\s+\(ro", out), f"[{nome}] mount: cd0 en /cd, tipo udf")
        comprobar_contenido(m, f"{nome}/udf", u_h)
        out = m.comando("cat /cd/leeme.txt")
        verificar("disco UDF" in out, f"[{nome}] UDF: archivo pequeño dentro de su File Entry (embebido)")
        out = m.comando("ls /cd/vacia")
        verificar("rror" not in out, f"[{nome}] UDF: directorio vacío")
        out = m.comando("discos")
        verificar(re.search(r"cd0 .*UDF 2\.01.*\"Datos UDF\"", out), f"[{nome}] discos: UDF 2.01 con su etiqueta")

        # --- 7. cambiar el disco montado por otro sin eject: UDF bridge (ISO 8.3 + UDF) -> UDF
        desde = meter(m, disp, p_iso)
        s = esperar_novo(m, 'aviso: CD/DVD "Datos bridge" montado en /cd', desde)
        verificar(s is not None, f"[{nome}] cambiar el disco montado: se desmonta el viejo y se monta el nuevo")
        out = m.comando("mount")
        verificar(re.search(r"cd0\s+en /cd\s+tipo udf", out), f"[{nome}] bridge con ISO 8.3: se monta el UDF (nombres largos)")
        comprobar_contenido(m, f"{nome}/bridge", p_h)
        m.comando("umount /cd")
        out = m.comando("mount -t iso9660 cd0 /isocd")
        verificar("montado en /isocd" in out, f"[{nome}] mount -t iso9660 del bridge ({out.strip()[-80:]})")
        out = m.comando("ls /isocd")
        verificar("leeme.txt" in out, f"[{nome}] el ISO del bridge (nombres 8.3 en minúsculas)")
        out = m.comando(f"sha256sum /isocd/{GRANDE}", 120)
        verificar(p_h[GRANDE] in out, f"[{nome}] el ISO del bridge comparte los datos del UDF")
        m.comando("umount /isocd")

        # --- 8. bridge con Joliet: se prefiere el ISO 9660
        desde = meter(m, disp, j_iso)
        s = esperar_novo(m, "aviso: CD/DVD", desde)
        verificar(s is not None and "montado en /cd" in s, f"[{nome}] bridge con Joliet: montado en /cd")
        out = m.comando("mount")
        verificar(re.search(r"cd0\s+en /cd\s+tipo iso9660", out), f"[{nome}] bridge con Joliet: se prefiere el ISO 9660")
        out = m.comando("discos")
        verificar(re.search(r"cd0 .*Joliet \+ UDF", out), f"[{nome}] discos: ISO 9660 + Joliet + UDF")

        verificar("Kernel panic" not in m.serial() and "PANICO" not in m.serial(), f"[{nome}] sin pánicos")
        m.captura(f"H233-{nome}", guardar=False)


def unidad():
    """El CD de audio en el PC: kernel/fs/cdda.c y cdda_logica.c con un lector simulado."""
    d = os.path.join(SAIDA, "h233-cdda")
    os.makedirs(d, exist_ok=True)
    k = ["-O1", "-g", "-ffreestanding", "-fno-builtin", "-nostdinc", "-fPIE", "-fno-stack-protector", "-Wall", "-Wextra",
         "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-DSAGUI_KERNEL", "-I", os.path.join(RAIZ, "kernel", "inc")]
    san = ["-fsanitize=address,undefined"]
    ent = dict(os.environ, CPATH="")   # D191: nada de la glibc del anfitrión en lo que es del kernel
    objs = []
    for fonte, extra in (("kernel/fs/cdda.c", []), ("kernel/fs/cdda_logica.c", []), ("tests/cdda_unidad.c", ["-DCDDA_PONTE"])):
        o = os.path.join(d, os.path.basename(fonte) + (".ponte" if extra else "") + ".o")
        r = subprocess.run(["gcc", *k, *san, *extra, "-c", os.path.join(RAIZ, fonte), "-o", o], capture_output=True, text=True, env=ent)
        verificar(r.returncode == 0, f"[unidad] compila {fonte}\n" + r.stderr)
        objs.append(o)
    o = os.path.join(d, "anfitriao.o")
    r = subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", *san, "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-c", os.path.join(RAIZ, "tests", "cdda_unidad.c"), "-o", o], capture_output=True, text=True)
    verificar(r.returncode == 0, "[unidad] compila la parte del PC\n" + r.stderr)
    exe = os.path.join(d, "cdda_unidad")
    r = subprocess.run(["gcc", *san, *objs, o, "-o", exe], capture_output=True, text=True)
    verificar(r.returncode == 0, "[unidad] enlaza\n" + r.stderr)
    r = subprocess.run([exe, d], capture_output=True, text=True, errors="replace")
    print(r.stdout[-1500:], r.stderr[-1500:])
    verificar(r.returncode == 0 and "0 fallos" in r.stdout and "ERROR" not in r.stderr, "[unidad] cdda_unidad: todas las comprobaciones")
    import wave
    with wave.open(os.path.join(d, "Pista01.wav")) as w:
        verificar((w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()) == (2, 2, 44100, 40 * 588),
                  "[unidad] Python abre Pista01.wav: estéreo, 16 bits, 44100 Hz, 40 sectores de 588 cuadros")
    shutil.rmtree(d)


def main():
    partes = sys.argv[1:] or ["unidad", "ide", "ahci"]
    if "unidad" in partes:
        unidad()
        if partes == ["unidad"]:
            print("H233: OK")
            return 0
    os.makedirs(SAIDA, exist_ok=True)
    discos = {
        "a": disco("a", 1, "disco A", "--iso", "--joliet", rotulo="DATOS-A"),
        "b": disco("b", 2, "disco B", "--iso", "--joliet", rotulo="DATOS-B"),
        "udf": disco("udf", 3, "disco UDF", "--udf", "2.01", "--embutir", "--fragmentar", GRANDE, rotulo="Datos UDF"),
        "bridge": disco("bridge", 4, "disco bridge", "--iso", "--udf", "1.02", rotulo="Datos bridge"),
        "bridgej": disco("bridgej", 5, "disco bridge Joliet", "--iso", "--joliet", "--udf", "2.01", rotulo="Datos J"),
    }
    try:
        if "ide" in partes:
            extras = ["-drive", "if=none,id=cdide,media=cdrom",
                      "-device", "ide-cd,bus=ide.1,unit=0,drive=cdide"]
            parte("ide", extras, "cdide", discos)
        if "ahci" in partes:
            extras = ["-device", "ich9-ahci,id=ahci",
                      "-drive", "if=none,id=cdahci,media=cdrom",
                      "-device", "ide-cd,bus=ahci.0,drive=cdahci"]
            parte("ahci", extras, "cdahci", discos)
    finally:
        for iso, _ in discos.values():
            if os.path.exists(iso):
                os.remove(iso)
    print("H233: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
