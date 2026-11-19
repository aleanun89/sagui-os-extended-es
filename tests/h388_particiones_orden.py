#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H388 (D388, v8.5.4.10r2) - el orden nuevo de las particiones: /home la última, para ampliarla sin mover nada.

A. Anfitrión (mkimagem.py): con --uefi, la raíz, la capa y /home, el MBR lleva SaguiOS (1), la ESP (2, delante
   de la extendida en el disco) y la extendida (3, la última, hasta el final de la imagen); las lógicas van en el
   orden SAGUI-CONF, SAGUI-RAIZ, SAGUI-PERSIST, (SAGUI-APPS), SAGUI-DATOS y la cadena de EBR es coherente (sfdisk
   la lee igual que nosotros, cada EBR 1 MB antes de su lógica, nada se solapa). Con --orden-antiguo, la de antes
   (/home la primera, la ESP en la entrada 3).
B. QEMU: la imagen live persistente (raíz de solo lectura + SAGUI-PERSIST) grabada en un disco de 2 GB: el primer
   arranque pregunta si ampliar /home; «Ampliar» crece la extendida y /home hasta el final sin mover nada (las
   demás lógicas siguen en su sitio, la raíz byte a byte igual); /home conserva su archivo; e2fsck limpio de
   todas; el segundo arranque es live persistente otra vez, no pregunta y df ve /home grande. Además (D388, avance):
   antes de empezar sale la estimación (grupos nuevos, MB a escribir, velocidad, segundos) y durante el trabajo el
   avance (porcentaje, MB escritos, tiempo restante) en el registro de la serie.
C. QEMU: la misma imagen con la disposición de antes (--orden-antiguo): el primer arranque NO toca nada (ampliar
   habría que mover la raíz y su capa en uso), lo explica y la tabla queda igual.
"""
import hashlib
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, extrair_particao, e2fsck, debugfs, particoes_img, imagem_de_teste, imagem_console,  # noqa: E402
                  SAIDA, BUILD)

MB = 1024 * 1024


def rotulo(img, inicio):
    with open(img, "rb") as f:
        f.seek(inicio * 512 + 1024 + 56)
        if f.read(2) != b"\x53\xef":
            return ""
        f.seek(inicio * 512 + 1024 + 120)
        return f.read(16).split(b"\0")[0].decode("latin-1")


def primarias(img):
    with open(img, "rb") as f:
        mbr = f.read(512)
    r = []
    for k in range(4):
        e = mbr[446 + 16 * k:462 + 16 * k]
        r.append((e[4], int.from_bytes(e[8:12], "little"), int.from_bytes(e[12:16], "little")))
    return r


def ebrs(img, ext):
    """La cadena de EBR: [(lba del EBR, inicio absoluto de la lógica, sectores)]."""
    r, ebr = [], ext
    with open(img, "rb") as f:
        for _ in range(32):
            f.seek(ebr * 512)
            s = f.read(512)
            verificar(s[510:512] == b"\x55\xaa", f"A: el EBR del LBA {ebr} tiene la firma 55AA")
            ini, qtd = int.from_bytes(s[454:458], "little"), int.from_bytes(s[458:462], "little")
            r.append((ebr, ebr + ini, qtd))
            if not s[462 + 4]:
                break
            ebr = ext + int.from_bytes(s[470:474], "little")
    return r


def sfdisk(img):
    out = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True)
    t = {}
    for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)", out.stdout):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4), 16))
    return t, out.stderr


def imagem(nome, extras):
    # Sin nombre de autoprueba (con él, el núcleo no lanza los servicios y ampliar-home no corre); copiada aparte.
    origem = imagem_de_teste("", ["--vesa", "--persist-mb", "64", "--dados-mb", "64"] + extras)
    destino = os.path.join(SAIDA, nome + ".img")
    shutil.copyfile(origem, destino)
    os.remove(origem)
    return destino


def parte_a():
    uefi = os.path.join(BUILD, "boot", "uefi")
    for antiguo in (False, True):
        img = imagem("h388a" + ("-antiguo" if antiguo else ""), ["--uefi", uefi] + (["--orden-antiguo"] if antiguo else []))
        p = primarias(img)
        tam = os.path.getsize(img) // 512
        x = [k for k, e in enumerate(p) if e[0] == 0x0F]
        e = [k for k, v in enumerate(p) if v[0] == 0xEF]
        verificar(p[0][0] == 0xDA and len(x) == 1 and len(e) == 1, f"A{'v' if antiguo else ''}: SaguiOS, una ESP y una extendida ({p})")
        x, e = x[0], e[0]
        verificar(p[e][1] + p[e][2] <= p[x][1], "A: la ESP está delante de la extendida en el disco")
        verificar(p[x][1] + p[x][2] == tam, "A: la extendida llega hasta el final de la imagen (es la última)")
        cadena = ebrs(img, p[x][1])
        nomes = [rotulo(img, ini) for _ebr, ini, _q in cadena]
        fim = p[x][1]
        for ebr, ini, qtd in cadena:
            verificar(ini == ebr + 2048 and ebr >= fim and ini % 2048 == 0, f"A: la lógica de {ini} va 1 MB tras su EBR, alineada y sin solaparse")
            fim = ini + qtd
        verificar(fim == tam, "A: la última lógica llega al final de la extendida")
        t, err = sfdisk(img)
        logicas = [(v[0], v[1]) for n, v in sorted(t.items()) if n >= 5]
        verificar(logicas == [(ini, q) for _e, ini, q in cadena] and not err.strip(),
                  f"A: sfdisk lee las mismas lógicas ({err.strip()[:120]})")
        if not antiguo:
            verificar(e == 1 and x == 2, "A: la ESP en la entrada 2 y la extendida en la 3")
            sis = [n for n in nomes if n != "SAGUI-APPS"]
            verificar(sis == ["SAGUI-CONF", "SAGUI-RAIZ", "SAGUI-PERSIST", "SAGUI-DATOS"], f"A: el orden CONF, RAIZ, PERSIST, (APPS), DATOS ({nomes})")
            verificar(nomes[-1] == "SAGUI-DATOS", "A: /home (SAGUI-DATOS) es la última lógica")
        else:
            verificar(x == 1 and e == 2, "Av: --orden-antiguo: la extendida en la entrada 2 y la ESP en la 3")
            verificar(nomes[0] == "SAGUI-DATOS" and nomes[-1] == "SAGUI-CONF", f"Av: --orden-antiguo: /home la primera y SAGUI-CONF la última ({nomes})")
        os.remove(img)


def particao(img, nome):
    for n, tipo, ini, qtd in particoes_img(img):
        if rotulo(img, ini) == nome:
            return n, ini, qtd
    return None


def soma(img, ini, qtd):
    with open(img, "rb") as f:
        f.seek(ini * 512)
        return hashlib.sha256(f.read(qtd * 512)).hexdigest()


def preparar(nome, antiguo, gb=2):
    img = imagem(nome, ["--orden-antiguo"] if antiguo else [])
    n, ini, qtd = particao(img, "SAGUI-DATOS")
    home0 = extrair_particao(img, ini, qtd // 2048)
    # D388: bloques de 4 KB y GDT reservados para crecer hasta ~2 TB (no 1024 veces: 64 GB)
    dump = subprocess.run(["/usr/sbin/dumpe2fs", "-h", home0], capture_output=True, text=True).stdout
    rg = re.search(r"Reserved GDT blocks:\s+(\d+)", dump)
    verificar("Block size:               4096" in dump and rg and int(rg.group(1)) >= 100,
              f"{nome}: SAGUI-DATOS de la imagen lleva bloques de 4 KB y GDT reservados para ~2 TB ({rg.group(0) if rg else dump[:200]})")
    local = os.path.join(SAIDA, "h388-guardado.txt")
    with open(local, "w") as f:
        f.write("persistente388\n")
    subprocess.run(["debugfs", "-w", "-R", f"write {local} guardado.txt", home0], check=True, capture_output=True)
    with open(home0, "rb") as f, open(img, "r+b") as g:
        g.seek(ini * 512)
        g.write(f.read())
    os.remove(home0)
    with open(img, "r+b") as f:
        f.truncate(gb * 1024 * MB)     # grabada en un disco de gb GB (2 por omisión; disperso)
    return img


def boton(m, s, campo):
    mm = re.search(r"botones y=(\d+)[^\n]*" + campo + r"=(\d+)", s)
    v = re.findall(r'criada por \d+: "Primer arranque: espacio libre" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    jx, jy = int(v[-1][2]) + 2, int(v[-1][3]) + 24
    m.clique(jx + int(mm.group(2)), jy + int(mm.group(1)) + 13)


def apagar(m):
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 60
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.3)


def parte_b():
    img = preparar("h388b", False)
    antes = {n: (ini, qtd) for n, _t, ini, qtd in particoes_img(img)}
    raiz = particao(img, "SAGUI-RAIZ")
    soma_raiz = soma(img, raiz[1], raiz[2])
    with Maquina("h388-b1", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: pregunta", 240)
        time.sleep(2)
        s = m.serial()
        verificar("[SOBRE] capa persistente en" in s, "B: arranca en live con la capa en SAGUI-PERSIST")
        mm = re.search(r"ampliar-home: pregunta: ganar (\d+) MB \((\w+)\)", s)
        verificar(mm and int(mm.group(1)) > 1500 and mm.group(2) == "ampliar", f"B: el primer arranque ofrece ampliar /home ({mm.group(0) if mm else ''})")
        # D388: la estimación sale antes de empezar (MB que escribir, grupos nuevos, velocidad y segundos)
        est = re.search(r"ampliar-home: estimación \(todo\): (\d+) grupos nuevos, (\d+) MB a escribir, (\d+) MB/s, (\d+) s", s)
        verificar(est and int(est.group(1)) >= 5 and int(est.group(2)) >= 10 and int(est.group(3)) >= 3 and int(est.group(4)) > 5,
                  f"B: el diálogo calcula la estimación del trabajo ({est.group(0) if est else ''})")
        grupos, mbest = int(est.group(1)), int(est.group(2))
        m.captura("H388-1-primer-arranque")
        boton(m, s, "ampliar")
        m.esperar_serial("ampliar-home: resultado", 600)
        time.sleep(1.5)
        s = m.serial()
        verificar("ampliar-home: resultado 0" in s, f"B: «Ampliar» termina bien ({s[s.rfind('ampliar-home: resultado'):][:300]})")
        verificar("se copia de" not in s[s.rfind("ampliar-home: empieza"):], "B: no se mueve ninguna partición")
        emp = re.search(r"ampliar-home: empieza \((\d+) MB a escribir, estimado (\d+) s\)", s)
        verificar(emp and int(emp.group(1)) == mbest, f"B: «empieza» repite la estimación ({emp.group(0) if emp else ''})")
        av = [int(x) for x in re.findall(r"ampliar-home: avance (\d+) %", s)]
        verificar(av and av[-1] == 100 and av == sorted(av) and len(set(av)) >= 5, f"B: el avance se anota de 10 en 10 % hasta el 100 % ({av})")
        esc = re.search(r"tablas de inodos nuevas: (\d+) MB escritos", s)
        verificar(esc is None or int(esc.group(1)) <= mbest, f"B: lo escrito de verdad no pasa de la estimación ({esc.group(0) if esc else 'sin dato'} <= {mbest} MB)")
        verificar(re.search(r"ampliar-home: avance 100 % +- +\d+ de \d+ MB escritos +- +quedan", s), "B: el avance lleva los MB escritos y el tiempo que queda")
        m.captura("H388-2-ampliado")
        m.tecla("ret")
        time.sleep(1)
        apagar(m)
    depois = {n: (ini, qtd) for n, _t, ini, qtd in particoes_img(img)}
    n_home = particao(img, "SAGUI-DATOS")
    verificar(n_home and n_home[0] == max(depois) and n_home[1] + n_home[2] == 2048 * 2048 and n_home[2] > 1500 * 2048,
              f"B: /home sigue la última y llega al final del disco de 2 GB ({n_home})")
    verificar(all(depois[n] == antes[n] for n in antes if n != n_home[0]), "B: las demás particiones no se han movido")
    verificar(soma(img, raiz[1], raiz[2]) == soma_raiz, "B: SAGUI-RAIZ byte a byte igual")
    for n, (ini, qtd) in depois.items():
        if n >= 5:
            p = extrair_particao(img, ini, qtd // 2048)
            rc, o = e2fsck(p)
            verificar(rc == 0, f"B: la lógica {n} ({rotulo(img, ini)}): e2fsck -fn limpio")
            if n == n_home[0]:
                verificar(b"persistente388" in debugfs(p, "cat /guardado.txt"), "B: el archivo de /home sigue ahí")
            os.remove(p)
    with Maquina("h388-b2", "ide", imagem=img, snapshot=True) as m:
        m.esperar_serial("sagui:", 240)
        time.sleep(10)
        s = m.serial()
        verificar("[SOBRE] capa persistente en" in s, "B: el segundo arranque vuelve a ser live persistente")
        verificar("ampliar-home: pregunta" not in s, "B: el segundo arranque ya no pregunta")
        out = m.comando("df")
        mm = re.search(r"(\d+)\s+\d+\s+\d+\s+\d+%\s+/home", out)
        verificar(mm and int(mm.group(1)) > 1500 * 1024, f"B: df ve /home de más de 1,5 GB ({mm.group(0) if mm else out[-200:]})")
        verificar("/ es una live" in out, "B: df explica la raíz de la live")
    os.remove(img)


def parte_c():
    img = preparar("h388c", True)
    antes = particoes_img(img)
    with Maquina("h388-c", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: no se puede", 240)
        time.sleep(2)
        s = m.serial()
        verificar("raíz en uso" in s.encode("latin-1", "replace").decode("latin-1") or "en uso" in s,
                  "C: con la disposición de antes en la live no se amplía y se dice por qué")
        m.captura("H388-3-disposicion-antigua")
        boton(m, s, "ahora_no")
        time.sleep(1)
        out = m.comando("ampliar-home")
        verificar("instalar" in out and "imagen nueva" in out, f"C: «ampliar-home» dice que hay que regrabar o reinstalar ({out.strip()[-200:]})")
        apagar(m)
    verificar(particoes_img(img) == antes, "C: la tabla no ha cambiado")
    os.remove(img)


def parte_d():
    """D (D388, avance): «ampliar-home --texto 1G -s» en el terminal: estimación, barra con MB y tiempo restante, e2fsck limpio."""
    img = preparar("h388d", False)
    with Maquina("h388-d", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: pregunta", 240)
        time.sleep(2)
        boton(m, m.serial(), "ahora_no")
        time.sleep(1)
        out = m.comando("ampliar-home --texto 1G -s", 300)
        verificar(re.search(r"Se va a ampliar /home hasta 1024 MB \((\d+) grupos nuevos, unos (\d+) MB que escribir a unos \d+ MB/s: como mucho", out),
                  f"D: el terminal da la estimación antes de empezar ({out[:300]!r})")
        verificar("NO APAGUES EL EQUIPO" in out, "D: el terminal avisa de no apagar")
        verificar(re.search(r"100 %  -  \d+ de \d+ MB escritos  -  quedan", out), "D: la barra llega al 100 % con los MB y el tiempo restante")
        verificar("pasa de" in out or "/home" in out.split("100 %")[-1], "D: dice el resultado")
        apagar(m)
    n_home = particao(img, "SAGUI-DATOS")
    verificar(n_home and 1000 * 2048 < n_home[2] < 1100 * 2048, f"D: /home mide ~1 GB ({n_home})")
    p = extrair_particao(img, n_home[1], n_home[2] // 2048)
    rc, o = e2fsck(p)
    verificar(rc == 0, "D: e2fsck -fn limpio")
    verificar(b"persistente388" in debugfs(p, "cat /guardado.txt"), "D: el archivo de /home sigue ahí")
    os.remove(p)
    os.remove(img)


def parte_e():
    """E (D388): crecimiento grande: /home de 64 MB a un disco disperso de 200 GB, más allá de los 64 GB que dejaban
    los GDT reservados de antes. e2fsck de /home directamente en la imagen (offset), sin copiarla."""
    img = preparar("h388e", False, 200)
    n0, ini0, qtd0 = particao(img, "SAGUI-DATOS")
    with Maquina("h388-e", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: pregunta", 300)
        time.sleep(2)
        s = m.serial()
        est = re.search(r"ampliar-home: estimación \(todo\): (\d+) grupos nuevos, (\d+) MB a escribir", s)
        verificar(est and int(est.group(1)) > 1000, f"E: estimación de un crecimiento grande ({est.group(0) if est else ''})")
        verificar("tabla de grupos no admite" not in s, "E: con la imagen nueva no hay límite que avisar")
        boton(m, s, "ampliar")
        m.esperar_serial("ampliar-home: resultado", 1500)
        time.sleep(1.5)
        s = m.serial()
        verificar("ampliar-home: resultado 0" in s, f"E: ampliar a ~200 GB termina bien ({s[s.rfind('ampliar-home: resultado'):][:300]})")
        m.tecla("ret")
        time.sleep(1)
        apagar(m)
    n_home = particao(img, "SAGUI-DATOS")
    verificar(n_home and n_home[2] > 190 * 1024 * 2048, f"E: /home mide más de 190 GB ({n_home})")
    rc = subprocess.run([E2FSCK_BIN, "-fn", f"{img}?offset={n_home[1] * 512}"], capture_output=True, text=True)
    verificar(rc.returncode == 0, f"E: e2fsck -fn de /home de ~200 GB limpio ({(rc.stdout + rc.stderr)[-300:]})")
    os.remove(img)


def sujar_cola(img, ini, qtd, gb):
    """D388 F: lo que hay detrás de /home hasta el final del disco, con restos de otro sistema (como el disco del
    Amilo, con instalaciones viejas): 64 KB de bytes aleatorios cada 512 KB, y el resto a cero. Así la mayoría de
    los trozos que lee fs_zerar son cero y decide leyendo (no pasa a «escribir sin leer»)."""
    rnd = random.Random(388)
    fim = gb * 1024 * MB
    with open(img, "r+b") as f:
        pos = (ini + qtd) * 512
        pos = (pos + 512 * 1024 - 1) // (512 * 1024) * (512 * 1024)
        while pos + 512 * 1024 <= fim:
            f.seek(pos + 192 * 1024)
            f.write(rnd.randbytes(64 * 1024))
            pos += 512 * 1024


def parte_f():
    """F (D388, ampliación del 2026-11-19): ampliar /home sobre un disco con restos de otro sistema detrás. Las
    tablas de inodos de los grupos nuevos tienen que quedar a cero: e2fsck -fn limpio (antes, con basura en ellas,
    el kernel la dejaba de solo lectura: «un inodo con extents»)."""
    img = preparar("h388f", False)
    n0, ini0, qtd0 = particao(img, "SAGUI-DATOS")
    sujar_cola(img, ini0, qtd0, 2)
    # Un disco sin TRIM (IDENTIFY palabra 169 a cero), como el del Amilo (hasta D388, fs_zerar leía antes de escribir en
    # estos discos y no escribía lo que leía como ceros).
    with Maquina("h388-f", "ide", imagem=img, snapshot=False, extras=["-global", "ide-hd.discard_granularity=0"]) as m:
        m.esperar_serial("ampliar-home: pregunta", 240)
        time.sleep(2)
        boton(m, m.serial(), "ahora_no")
        time.sleep(1)
        out = m.comando("ampliar-home --texto todo -s", 300)
        verificar("100 %" in out, f"F: ampliar /home termina ({out[-300:]!r})")
        s = m.serial()
        esc = re.search(r"tablas de inodos nuevas: (\d+) MB escritos[^\n]*", s)
        verificar(esc and int(esc.group(1)) >= 25,
                  f"F: las tablas de inodos de los grupos nuevos se escriben enteras, sin fiarse de lo leído ({esc.group(0) if esc else 'sin línea de redimensao'})")
        apagar(m)
    n_home = particao(img, "SAGUI-DATOS")
    verificar(n_home and n_home[2] > 1500 * 2048, f"F: /home crece hasta el final del disco ({n_home})")
    p = extrair_particao(img, n_home[1], n_home[2] // 2048)
    rc, o = e2fsck(p)
    verificar(rc == 0, f"F: e2fsck -fn limpio con restos detrás de /home ({o[-400:] if rc else ''})")
    verificar(b"persistente388" in debugfs(p, "cat /guardado.txt"), "F: el archivo de /home sigue ahí")
    os.remove(p)
    os.remove(img)


def parte_g():
    """G (D388, 2026-11-19): lo mismo en un disco SATA (AHCI, como el del Amilo) sin TRIM: un ext2 de 64 MB como el
    /home de las imágenes (4 KB, -i 8192, GDT reservados) en un disco de 2 GB con restos detrás; «particiones ampliar
    hdb1» lo lleva hasta el final y la tabla de inodos de cada grupo nuevo queda a cero (e2fsck -fn limpio)."""
    img = os.path.join(SAIDA, "h388g-sata.img")
    with open(img, "wb") as f:
        f.truncate(2048 * MB)
    mbr = bytearray(512)
    struct.pack_into("<BBBBBBBBII", mbr, 446, 0, 0, 0, 0, 0x83, 0, 0, 0, 2048, 64 * 2048)
    mbr[510:512] = b"\x55\xaa"
    with open(img, "r+b") as f:
        f.write(mbr)
    subprocess.run(["/usr/sbin/mke2fs", "-q", "-F", "-t", "ext2", "-L", "SATA388", "-m", "0", "-b", "4096", "-i", "8192",
                    "-E", f"offset={2048 * 512},lazy_itable_init=1,nodiscard,resize={2 ** 29}", img, "65536k"], check=True)
    sujar_cola(img, 2048, 64 * 2048, 2)
    extras = ["-device", "ich9-ahci,id=ahci",
              "-drive", f"if=none,id=sata0,format=raw,file={img},snapshot=off",
              "-device", "ide-hd,drive=sata0,bus=ahci.0,discard_granularity=0"]
    with Maquina("h388-g", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones ampliar hdb1 -s", 300)
        s = m.serial()
        esc = re.search(r"tablas de inodos nuevas: (\d+) MB escritos[^\n]*", s)
        verificar(esc and int(esc.group(1)) >= 25,
                  f"G: AHCI sin TRIM: las tablas de inodos nuevas se escriben enteras ({esc.group(0) if esc else out[-300:]})")
        apagar(m)
    p = extrair_particao(img, 2048, 2047)
    rc, o = e2fsck(p)
    verificar(rc == 0, f"G: e2fsck -fn limpio tras ampliar en SATA con restos detrás ({o[-400:] if rc else ''})")
    os.remove(p)
    os.remove(img)


def parte_h():
    """H (D388, 2026-11-19): formatear un ext2 en un SSD (AHCI con TRIM, discard=unmap) lleno de restos: el TRIM del
    formateo solo cubre bloques de datos libres, NUNCA los metadatos que se escriben después (superbloques, tablas de
    grupos, mapas, tablas de inodos, raíz y lost+found). Antes se descartaba el volumen entero y luego se escribían los
    metadatos encima: el SSD del Amilo dejó restos de un ext4 viejo en las tablas de inodos. Se comprueba con las
    líneas «[BLOCOS] hdb1: TRIM de N sectores desde S» frente a la geometría que da dumpe2fs, y con e2fsck -fn."""
    img = os.path.join(SAIDA, "h388h-ssd.img")
    with open(img, "wb") as f:
        bloco = b"\xAB" * MB
        for _ in range(320):
            f.write(bloco)
    extras = ["-device", "ich9-ahci,id=ahci",
              "-drive", f"if=none,id=ssd,format=raw,file={img},discard=unmap",
              "-device", "ide-hd,drive=ssd,bus=ahci.0"]
    with Maquina("h388-h", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar(re.search(r"\[AHCI\] puerto 0: .*TRIM", m.serial()), "H: el disco AHCI admite TRIM")
        out = m.comando("particiones crear hdb 300 ext2 --etiqueta SSD388 -s", 300)
        verificar("creada hdb1" in out, f"H: crear un ext2 en el SSD ({out[-200:]!r})")
        s = m.serial()
        apagar(m)
    trims = [(int(a), int(b)) for b, a in re.findall(r"\[BLOCOS\] hdb1: TRIM de (\d+) sectores desde (\d+): 0", s)]
    verificar(len(trims) > 1, f"H: el formateo manda TRIM por grupos ({len(trims)} órdenes)")
    p = extrair_particao(img, 2048, 300)
    rc, o = e2fsck(p)
    verificar(rc == 0, f"H: e2fsck -fn limpio ({o[-400:] if rc else ''})")
    dump = subprocess.run(["/usr/sbin/dumpe2fs", p], capture_output=True, text=True).stdout
    bs = int(re.search(r"Block size:\s+(\d+)", dump).group(1))
    metas = []   # [inicio, fin) en sectores absolutos: del principio del grupo al final de su tabla de inodos
    for g in re.finditer(r"Group (\d+): \(Blocks (\d+)-(\d+)\).*?Inode table at (\d+)-(\d+)", dump, re.S):
        fim = int(g.group(5)) + 1 + (2 if g.group(1) == "0" else 0)
        metas.append((2048 + int(g.group(2)) * bs // 512, 2048 + fim * bs // 512))
    verificar(len(metas) > 5, f"H: dumpe2fs da la geometría ({len(metas)} grupos)")
    solapa = [(t, mt) for t in trims for mt in metas if t[0] < mt[1] and mt[0] < t[0] + t[1]]
    verificar(not solapa, f"H: ningún TRIM cubre metadatos del ext2 ({solapa[:3]})")
    with open(img, "rb") as f:
        f.seek(metas[3][1] * 512 + 64 * 1024)    # datos libres del grupo 3: descartados
        verificar(f.read(4096) == bytes(4096), "H: los datos libres sí se descartan (vuelven a cero)")
    os.remove(p)
    os.remove(img)


E2FSCK_BIN = "/usr/sbin/e2fsck"


def main():
    partes = sys.argv[1:] or ["a", "b", "c", "d", "e", "f", "g", "h"]
    if "a" in partes:
        print("parte A: disposición de mkimagem")
        parte_a()
    if "b" in partes:
        print("parte B: live persistente en un disco grande: ampliar sin mover")
        parte_b()
    if "c" in partes:
        print("parte C: la disposición de antes en la live")
        parte_c()
    if "e" in partes:
        print("parte E: crecimiento grande (64 MB a ~200 GB)")
        parte_e()
    if "d" in partes:
        print("parte D: ampliar-home --texto con estimación y avance")
        parte_d()
    if "f" in partes:
        print("parte F: ampliar sobre un disco con restos de otro sistema detrás")
        parte_f()
    if "g" in partes:
        print("parte G: lo mismo en un disco SATA (AHCI) sin TRIM")
        parte_g()
    if "h" in partes:
        print("parte H: formatear ext2 en un SSD: TRIM solo de los datos libres")
        parte_h()
    print("H388 OK")


if __name__ == "__main__":
    main()
