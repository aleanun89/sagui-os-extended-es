#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H397 (D397, v8.5.4.10r2) - «reparar»: el reparador de ext2 de SaguiOS.

Se generan en el PC ext2 de bloques de 1 KB y de 4 KB con mke2fs y archivos (pequeños, uno con indirecto
doble, carpetas con muchas entradas) y se ESTROPEAN a propósito, como tras un cuelgue: un inodo que apunta
fuera del volumen, un bloque que usan dos archivos, el mapa de bits desfasado (un bloque en uso marcado
libre y uno libre marcado en uso), los contadores de libres mal (grupo y superbloque), una entrada de
carpeta a un inodo borrado, un inodo vivo sin carpeta, enlaces mal (archivo y carpeta), un inodo en uso
con fecha de borrado, «..» mal, una cadena de entradas rota en una carpeta y la marca de «no se desmontó
bien». e2fsck -fn del PC tiene que ver errores antes.

A. Anfitrión: userland/libsagui/reparo_ext2.c compilado para el PC (como h318): analizar no cambia nada
   (sha256 de la imagen igual) y ve cada cosa; reparar deja e2fsck -fn limpio, los archivos sanos intactos
   (sha256), el huérfano en /lost+found con su contenido, los dos del bloque compartido legibles; la copia
   de seguridad devuelve la imagen exacta de antes («deshacer») y una segunda reparación vuelve a dejarla
   limpia. Un superbloque principal destruido se rehace con su copia.
B. QEMU: un pendrive con las dos particiones estropeadas: «reparar -n» lo detecta sin escribir (sha256 de
   la imagen igual), «reparar -y» lo arregla; e2fsck -fn limpio, archivos sanos intactos, y el kernel la
   monta con escritura (sin «solo lectura») y lee un archivo.
F. Anfitrión: el formateador ext2 propio (formatacao.c, el de «particiones formatear», Discos y el instalador)
   ya deja resize_inode como mke2fs (bloques de la tabla de grupos reservados e inodo 7): e2fsck -fn limpio, «reparar»
   no ve nada, dumpe2fs lo declara y el volumen crece luego con fs_redimensionar_ext2 (varias veces el tamaño) sin
   rehacerlo (D397: lo que /home necesita para ampliar-home).
C. QEMU: el caso del Amilo: /home (SAGUI-DATOS, disco de arranque) estropeada (un bloque de dos archivos y «no se
   desmontó bien»); el kernel la monta de solo lectura y el aviso dice «reparar»; el servicio del arranque
   (reparar --arranque, modo -p) la repara SOLO, sin perder nada, y /home vuelve a estar en rw.
D. Anfitrión: tablas de inodos con basura en grupos vacíos (geometría del Amilo): se ponen a cero sin tocar archivos
   de verdad (ver parte_d).
E. QEMU: lo mismo que C pero con algo arriesgado (una entrada de carpeta a un inodo borrado: habría que descartar un
   nombre): el servicio no toca nada, /home sigue de solo lectura y el aviso dice «ejecuta reparar hdaN»; a
   mano, «reparar -y» la arregla y la monta con escritura.
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
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, particoes_img, imagem_de_teste, SAIDA, RAIZ, MKE2FS, DEBUGFS)

random.seed(397)
SANOS = {
    "sano.txt": b"Este archivo no se toca: tiene que seguir igual.\n" * 40,
    "grande.bin": bytes(random.getrandbits(8) for _ in range(2 * 1024 * 1024 + 77)),   # indirecto doble con 1 KB
    "docs/carta.txt": "Canción del ñandú: acción, corazón.\n".encode("utf-8") * 30,
    "docs/sub/hondo.txt": b"muy dentro\n" * 100,
}
for i in range(120):
    SANOS[f"muchos/f{i:03d}.txt"] = f"archivo {i} de la carpeta grande\n".encode() * 3
ROTOS = {
    "fuera.bin": bytes(random.getrandbits(8) for _ in range(30 * 1024)),
    "comp_a.bin": b"A" * 9000,
    "comp_b.bin": b"B" * 9000,
    "borrado.txt": b"este inodo se borra pero su entrada se queda\n" * 10,
    "d1/huerfano.txt": b"me quedo sin carpeta pero sigo vivo\n" * 50,
    "enlaces.txt": b"enlaces mal\n" * 20,
    "fechado.txt": b"en uso con fecha de borrado\n" * 20,
    "d2/x.txt": b"x\n",
    "rota/a.txt": b"a\n", "rota/b.txt": b"b\n", "rota/c.txt": b"c\n",
}


def sha(datos):
    return hashlib.sha256(datos).hexdigest()


def sha_arq(p):
    with open(p, "rb") as f:
        return sha(f.read())


def dfs(img, *ordenes):
    """debugfs -w con varias órdenes (del PC: solo para estropear la imagen de prueba)."""
    r = subprocess.run([DEBUGFS, "-w", "-f", "-", img], input="\n".join(ordenes) + "\n", capture_output=True, text=True)
    return r.stdout + r.stderr


def stat(img, ruta):
    return debugfs(img, f"stat {ruta}").decode("utf-8", "replace")


def inodo(img, ruta):
    m = re.search(r"Inode:\s*(\d+)", stat(img, ruta))
    return int(m.group(1))


def blocos(img, ruta):
    return [int(x) for x in debugfs(img, f"blocks {ruta}").decode().split()]


def sb_campo(img, off, fmt):
    with open(img, "rb") as f:
        f.seek(1024 + off)
        return struct.unpack(fmt, f.read(struct.calcsize(fmt)))[0]


def criar_ext2(destino, bs, mb):
    pasta = destino + ".conteudo"
    shutil.rmtree(pasta, ignore_errors=True)
    for nome, dados in list(SANOS.items()) + list(ROTOS.items()):
        p = os.path.join(pasta, nome)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "wb") as f:
            f.write(dados)
    if os.path.exists(destino):
        os.remove(destino)
    with open(destino, "wb") as f:
        f.truncate(mb * 1024 * 1024)
    subprocess.run([MKE2FS, "-q", "-F", "-t", "ext2", "-b", str(bs), "-L", f"REP{bs // 1024}K", "-d", pasta, destino],
                   check=True, env=dict(os.environ, E2FSPROGS_FAKE_TIME="1700000000"))
    shutil.rmtree(pasta)


def estropear(img, bs):
    """Lo que deja un cuelgue (y algo más). Devuelve el número de inodo del huérfano."""
    total = sb_campo(img, 4, "<I")
    huerfano = inodo(img, "/d1/huerfano.txt")
    a, b = blocos(img, "/comp_a.bin"), blocos(img, "/comp_b.bin")
    sano = blocos(img, "/sano.txt")
    d2 = inodo(img, "/d2")
    rota = blocos(img, "/rota")[0]
    salida = dfs(img,
                 f"sif /fuera.bin block[2] {total + 5000}",       # fuera del volumen
                 f"sif /comp_b.bin block[1] {a[1]}",              # un bloque de dos archivos
                 f"freeb {sano[0]}",                             # en uso pero marcado libre
                 f"setb {total - 3}",                            # libre pero marcado en uso
                 "set_bg 0 free_blocks_count 7",                 # contadores mal
                 "ssv free_blocks_count 12345",
                 "ssv free_inodes_count 99",
                 "sif /borrado.txt links_count 0",               # inodo borrado con su entrada aún
                 "sif /borrado.txt dtime 1700000500",
                 "unlink /d1/huerfano.txt",                      # vivo sin carpeta
                 "sif /enlaces.txt links_count 7",               # enlaces mal
                 "sif /docs links_count 9",
                 "sif /fechado.txt dtime 1700000600",            # en uso con fecha de borrado
                 "ssv state 0")                                  # no se desmontó bien
    # «..» de /d2 apuntando a /docs, y una entrada rota en /rota (rec_len imposible en la segunda de sus
    # entradas normales): a mano, en el primer bloque de cada carpeta.
    docs = inodo(img, "/docs")
    d2b = blocos(img, "/d2")[0]
    with open(img, "r+b") as f:
        f.seek(d2b * bs + 12)
        f.write(struct.pack("<I", docs))
        f.seek(rota * bs)
        blk = bytearray(f.read(bs))
        off, n = 0, 0
        while off < bs:
            rl = struct.unpack_from("<H", blk, off + 4)[0]
            if n == 3:   # «.», «..», a.txt, (esta)
                struct.pack_into("<H", blk, off + 4, 3)    # rec_len 3: imposible
                break
            off += rl
            n += 1
        f.seek(rota * bs)
        f.write(blk)
    verificar("rror" not in salida or "No such" not in salida, f"debugfs aplicó las averías ({salida[-200:]})")
    _ = d2
    return huerfano


def compilar():
    d = os.path.join(SAIDA, "h397-anfitriao")
    os.makedirs(d, exist_ok=True)
    rec = os.path.join(RAIZ, "tests", "recursos", "reparo_anfitriao")
    shutil.copy(os.path.join(rec, "sagui.h"), d)
    shutil.copy(os.path.join(RAIZ, "userland", "inc", "particoes.h"), d)
    exe = os.path.join(d, "reparo")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I", d, "-o", exe, os.path.join(rec, "principal.c"),
                    os.path.join(RAIZ, "userland", "libsagui", "reparo_ext2.c")], check=True)
    return exe


def comprobar_reparado(img, huerfano, donde):
    cod, txt = e2fsck(img)
    if cod:
        print(txt)
    verificar(cod == 0, f"{donde}: e2fsck -fn limpio tras reparar (código {cod})")
    for nome, dados in SANOS.items():
        if debugfs(img, f"cat /{nome}") != dados:
            verificar(False, f"{donde}: el archivo sano /{nome} sigue igual")
    verificar(True, f"{donde}: los {len(SANOS)} archivos sanos siguen iguales (contenido byte a byte)")
    verificar(debugfs(img, f"cat /lost+found/#{huerfano}") == ROTOS["d1/huerfano.txt"],
              f"{donde}: el huérfano está en /lost+found/#{huerfano} con su contenido")
    ca, cb = debugfs(img, "cat /comp_a.bin"), debugfs(img, "cat /comp_b.bin")
    verificar(ca == ROTOS["comp_a.bin"] and len(cb) == 9000, f"{donde}: los dos archivos del bloque compartido se leen enteros")
    verificar(set(blocos(img, "/comp_a.bin")).isdisjoint(blocos(img, "/comp_b.bin")), f"{donde}: ya no comparten bloques")
    ls = debugfs(img, "ls /").decode("utf-8", "replace")
    verificar("borrado.txt" not in ls, f"{donde}: la entrada del inodo borrado ya no está")
    verificar(debugfs(img, "cat /enlaces.txt") == ROTOS["enlaces.txt"] and "Links: 1" in stat(img, "/enlaces.txt"),
              f"{donde}: enlaces.txt con 1 enlace y su contenido")
    verificar(debugfs(img, "cat /fechado.txt") == ROTOS["fechado.txt"], f"{donde}: fechado.txt sigue vivo con su contenido")
    verificar(sb_campo(img, 58, "<H") == 1, f"{donde}: marcado como desmontado bien")


def parte_a():
    exe = compilar()
    for bs, mb in ((1024, 24), (4096, 48)):
        img = os.path.join(SAIDA, f"h397a-{bs}.img")
        criar_ext2(img, bs, mb)
        huerfano = estropear(img, bs)
        cod, txt = e2fsck(img)
        verificar(cod != 0, f"A {bs}: e2fsck -fn ve el volumen estropeado (código {cod})")
        antes = sha_arq(img)
        r = subprocess.run([exe, "analisar", img], capture_output=True, text=True)
        print(r.stdout[-3000:])
        verificar(r.returncode == 1 and sha_arq(img) == antes, f"A {bs}: analizar ve problemas y no cambia la imagen (sha256 igual)")
        for frase in ("fuera del volumen", "lo usa también otro archivo", "el mapa de bloques no cuadra", "inodo que no está en uso",
                      "no está en ninguna carpeta", "enlaces y tiene", "con fecha de borrado", "«..» apunta", "entradas rotas",
                      "no se desmontó bien", "libres"):
            verificar(frase in r.stdout, f"A {bs}: analizar ve «{frase}»")
        copia = img + ".copia"
        r = subprocess.run([exe, "reparar", img, copia], capture_output=True, text=True)
        print(r.stdout[-1500:])
        verificar(r.returncode == 1, f"A {bs}: reparar termina bien ({r.stdout[-200:]})")
        comprobar_reparado(img, huerfano, f"A {bs}")
        r = subprocess.run([exe, "analisar", img], capture_output=True, text=True)
        verificar(r.returncode == 0 and "nada que reparar" in r.stdout, f"A {bs}: una segunda pasada no encuentra nada ({r.stdout[-300:]})")
        r = subprocess.run([exe, "deshacer", img, copia], capture_output=True, text=True)
        verificar(r.returncode == 0 and sha_arq(img) == antes, f"A {bs}: deshacer con la copia deja la imagen exacta de antes ({r.stdout.strip()})")
        r = subprocess.run([exe, "reparar", img], capture_output=True, text=True)
        verificar(r.returncode == 1, f"A {bs}: se repara otra vez")
        comprobar_reparado(img, huerfano, f"A {bs} (segunda vez)")
        # El superbloque principal destruido: se usa una copia (solo con varios grupos: con uno no hay copias).
        if bs != 1024:
            for p in (img, copia):
                if os.path.exists(p):
                    os.remove(p)
            continue
        with open(img, "r+b") as f:
            f.seek(1024)
            f.write(b"\0" * 1024)
        r = subprocess.run([exe, "reparar", img], capture_output=True, text=True)
        cod, txt = e2fsck(img)
        verificar(r.returncode == 1 and "copia del grupo" in r.stdout and cod == 0,
                  f"A {bs}: superbloque principal a cero: se rehace con una copia y e2fsck queda limpio ({cod} {r.stdout[-200:]})")
        for p in (img, copia):
            if os.path.exists(p):
                os.remove(p)


def montar_disco(nome, partes):
    """Un disco MBR con las particiones (imágenes ya hechas) una detrás de otra desde 1 MB."""
    img = os.path.join(SAIDA, nome + ".img")
    mbs = [os.path.getsize(p) // (1024 * 1024) for p in partes]
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate((2 + sum(mbs)) * 1024 * 1024)
    tabela, ini, inicios = "", 2048, []
    for mb in mbs:
        tabela += f"start={ini}, size={mb * 2048}, type=83\n"
        inicios.append(ini)
        ini += mb * 2048
    subprocess.run(["sfdisk", "-q", img], input=tabela, text=True, check=True)
    for p, i in zip(partes, inicios):
        subprocess.run(["dd", f"if={p}", f"of={img}", "bs=1M", f"seek={i // 2048}", "conv=notrunc", "status=none"], check=True)
    return img, inicios, mbs


def parte_b():
    partes, huerfanos = [], []
    for bs, mb in ((1024, 24), (4096, 48)):
        p = os.path.join(SAIDA, f"h397b-{bs}.part")
        criar_ext2(p, bs, mb)
        huerfanos.append(estropear(p, bs))
        partes.append(p)
    img, inicios, mbs = montar_disco("h397b-pen", partes)
    for p in partes:
        os.remove(p)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h397-b", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        for k, vol in enumerate(("usb0p1", "usb0p2")):
            antes = sha_arq(img)
            out = m.comando(f"reparar -n {vol}", 300)
            print(out[-2500:])
            for frase in ("fuera del volumen", "lo usa también otro archivo", "no está en ninguna carpeta", "entradas rotas",
                          "no se desmontó bien", "Con -n no se ha cambiado nada"):
                verificar(frase in out, f"B {vol}: «reparar -n» ve «{frase}»")
            verificar(sha_arq(img) == antes, f"B {vol}: «reparar -n» no escribe nada (sha256 del disco igual)")
            out = m.comando(f"reparar -y {vol}", 300)
            print(out[-1500:])
            verificar("reparado:" in out and "Comprobado otra vez: todo cuadra" in out, f"B {vol}: «reparar -y» lo arregla y la segunda pasada sale limpia")
            verificar("Para deshacer: reparar --deshacer /tmp/reparar/" in out, f"B {vol}: guarda la copia de lo que cambia en /tmp/reparar")
            out = m.comando(f"reparar -n {vol}", 300)
            verificar("nada que reparar" in out, f"B {vol}: otra vez «reparar -n»: nada que reparar")
            out = m.comando(f"mount -o rw {vol} /mnt/r{k}")
            verificar(f"montado en /mnt/r{k}" in out and "solo lectura" not in out, f"B {vol}: el kernel la monta con escritura ({out.strip()[-160:]})")
            out = m.comando(f"cat /mnt/r{k}/docs/sub/hondo.txt")
            verificar("muy dentro" in out, f"B {vol}: se lee un archivo sano")
            m.comando(f"echo escrito tras reparar > /mnt/r{k}/nuevo.txt")
            out = m.comando(f"umount /mnt/r{k}")
        m.captura("H397-reparar")
        verificar("PANICO" not in m.serial(), "B: ningún pánico del kernel")
    for k, bs in enumerate((1024, 4096)):
        p = extrair_particao(img, inicios[k], mbs[k])
        comprobar_reparado(p, huerfanos[k], f"B {bs}")
        verificar(debugfs(p, "cat /nuevo.txt") == b"escrito tras reparar\n", f"B {bs}: lo escrito después de reparar está")
        os.remove(p)
    os.remove(img)


# ------------------------------------------------------------ D: tablas de inodos con basura (el /home del Amilo)

def geometria(img):
    """(bloques por grupo, inodos por grupo, tamaño de inodo, [bloque de la tabla de inodos de cada grupo])."""
    with open(img, "rb") as f:
        f.seek(1024)
        sb = f.read(1024)
        bpg, ipg, isz = struct.unpack_from("<I", sb, 32)[0], struct.unpack_from("<I", sb, 40)[0], struct.unpack_from("<H", sb, 88)[0]
        grupos = (struct.unpack_from("<I", sb, 4)[0] + bpg - 1) // bpg
        f.seek(4096)
        gdt = f.read(grupos * 32)
    return bpg, ipg, isz, [struct.unpack_from("<I", gdt, g * 32 + 8)[0] for g in range(grupos)]


def inodo_cru(img, n, isz, ipg, its, novo=None):
    g, i = (n - 1) // ipg, (n - 1) % ipg
    with open(img, "r+b" if novo is not None else "rb") as f:
        f.seek(its[g] * 4096 + i * isz)
        if novo is None:
            return f.read(isz)
        f.write(novo)


def parte_d():
    """D (D397, 2026-11-19): un ext2 como el /home del Amilo tras ampliarlo sobre restos de otro sistema: bloques de
    4 KB, 8192 inodos por grupo, archivos de verdad en el grupo 0 y, en las tablas de inodos de los grupos altos (que
    según sus mapas no tienen ningún inodo en uso), inodos de un ext4 viejo y bloques de bytes aleatorios a partir de
    la mitad de la tabla. Además:
      - grupo 6: un inodo basura «creíble» (archivo normal, sin fecha de borrado) en el índice 300 cuyo indirecto
        simple es el bloque de un archivo de verdad casi todo ceros con 3 valores «fuera del volumen»: el reparador de
        antes lo tomaba por un archivo y «arreglaba» ese bloque (cambiaba el archivo de verdad);
      - grupo 7: un archivo de verdad cuyo inodo está en el índice 0 sin carpeta ni mapa (lo que deja un corte justo al
        crearlo): el grupo no se toma por vacío y el archivo va a /lost+found con su contenido;
      - grupo 5: una carpeta de verdad apunta a un inodo del grupo (copia de un archivo de verdad): el análisis se
        repite sin fiarse de ese grupo y el archivo se conserva.
    reparar: e2fsck -fn limpio, los archivos de verdad iguales (sha256), la basura a cero, ningún bloque de más en uso;
    deshacer deja la imagen exacta; una segunda pasada no ve nada."""
    exe = compilar()
    img = os.path.join(SAIDA, "h397d-lixo.img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(1024 * 1024 * 1024)
    subprocess.run([MKE2FS, "-q", "-F", "-t", "ext2", "-L", "SAGUI-DATOS", "-m", "0", "-b", "4096", "-i", "16384", "-I", "256",
                    "-E", f"lazy_itable_init=0,nodiscard,resize={2 ** 29}", img], check=True)
    rnd = random.Random(3971)
    reais = {f"docs/r{i:02d}.bin": rnd.randbytes(rnd.randint(100, 200000)) for i in range(20)}
    reais["grande.bin"] = rnd.randbytes(3 * 1024 * 1024 + 11)
    reais["orfao7.bin"] = b"creado justo antes del corte\n" * 300
    reais["resgate.bin"] = b"una carpeta de verdad apunta a mi inodo\n" * 200
    tmp = img + ".d"
    os.makedirs(tmp, exist_ok=True)
    ordens = ["mkdir docs"]
    for nome, dados in reais.items():
        p = os.path.join(tmp, nome.replace("/", "_"))
        with open(p, "wb") as f:
            f.write(dados)
        ordens.append(f"write {p} {nome}")
    dfs(img, *ordens)
    # El archivo «casi todo ceros» cuyo bloque usará como indirecto el inodo basura creíble.
    total = sb_campo(img, 4, "<I")
    alvo = blocos(img, "/docs/r00.bin")
    esp = bytearray(4096)
    for k, v in ((10, 0xFFFFFFF0), (200, 0xFFFFFF00), (700, 0xF0000000), (30, alvo[0]), (31, alvo[1] if len(alvo) > 1 else alvo[0])):
        struct.pack_into("<I", esp, k * 4, v)
    p = os.path.join(tmp, "esparso")
    with open(p, "wb") as f:
        f.write(esp)
    dfs(img, f"write {p} esparso.bin")
    reais["esparso.bin"] = bytes(esp)
    shutil.rmtree(tmp)
    bpg, ipg, isz, its = geometria(img)
    verificar(len(its) == 8 and ipg == 8192, f"D: 8 grupos de 8192 inodos ({len(its)}, {ipg})")
    # Un ext4 viejo con archivos: de ahí salen inodos «de verdad» de otro sistema (extents, carpetas indexadas).
    e4 = img + ".ext4"
    with open(e4, "wb") as f:
        f.truncate(64 * 1024 * 1024)
    subprocess.run([MKE2FS, "-q", "-F", "-t", "ext4", "-b", "4096", "-I", str(isz), e4], check=True)
    dfs(e4, "mkdir a", *[f"symlink a/s{i} /destino/{i}" for i in range(50)], *[f"mknod a/d{i} c 1 {i}" for i in range(50)])
    tab4 = open(e4, "rb").read()[int(re.search(r"Inode table at (\d+)", subprocess.run(["/usr/sbin/dumpe2fs", e4], capture_output=True, text=True).stdout).group(1)) * 4096:][:64 * 4096]
    os.remove(e4)
    with open(img, "r+b") as f:
        for g in range(3, 8):
            f.seek((its[g] + 128) * 4096)
            f.write(tab4)
            for k in range(200, ipg * isz // 4096, 37):
                f.seek((its[g] + k) * 4096)
                f.write(rnd.randbytes(4096))
    # grupo 6: el inodo basura creíble (índice 300) con el bloque de esparso.bin como indirecto simple
    cri = bytearray(isz)
    struct.pack_into("<HHIIIIIHHI", cri, 0, 0x81A4, 0, 5 * 1024 * 1024, 1, 1, 1, 0, 0, 1, 16)
    struct.pack_into("<I", cri, 40 + 12 * 4, blocos(img, "/esparso.bin")[0])
    inodo_cru(img, 6 * ipg + 301, isz, ipg, its, bytes(cri))
    # grupo 7: el archivo creado justo antes del corte, en el índice 0, sin carpeta ni mapa
    n_or = inodo(img, "/orfao7.bin")
    orf = 7 * ipg + 1
    inodo_cru(img, orf, isz, ipg, its, inodo_cru(img, n_or, isz, ipg, its))
    # grupo 5: una carpeta de verdad apunta a un inodo de la tabla con basura
    n_rs = inodo(img, "/resgate.bin")
    resg = 5 * ipg + 101
    inodo_cru(img, resg, isz, ipg, its, inodo_cru(img, n_rs, isz, ipg, its))
    dfs(img, f"ln <{resg}> /resgatado.bin", "unlink /resgate.bin", f"clri <{n_rs}>", "unlink /orfao7.bin", f"clri <{n_or}>",
        f"freei <{n_rs}>", f"freei <{n_or}>", "ssv state 0")
    reais["resgatado.bin"] = reais.pop("resgate.bin")
    orfao = reais.pop("orfao7.bin")
    cod, _ = e2fsck(img)
    verificar(cod != 0, f"D: e2fsck -fn ve el volumen estropeado (código {cod})")
    usados_antes = sb_campo(img, 4, "<I") - sb_campo(img, 12, "<I")
    antes = sha_arq(img)
    r = subprocess.run([exe, "analisar", img], capture_output=True, text=True)
    print(r.stdout[-2500:])
    verificar(r.returncode == 1 and sha_arq(img) == antes, "D: analizar ve problemas y no cambia la imagen")
    verificar("Tablas de inodos con basura en grupos vacíos: 3 grupos" in r.stdout,
              "D: los grupos 3, 4 y 6 se toman por vacíos con basura; el 5 (tras repetir) y el 7, no")
    verificar("grupo 7: su mapa dice que no tiene inodos en uso, pero el primero" in r.stdout, "D: el grupo 7 se comprueba inodo a inodo")
    verificar("se repite el análisis" in r.stdout, "D: la entrada de carpeta al grupo 5 hace repetir el análisis")
    copia = img + ".copia"
    r = subprocess.run([exe, "reparar", img, copia], capture_output=True, text=True)
    print(r.stdout[-1200:])
    verificar(r.returncode == 1, f"D: reparar termina bien ({r.stdout[-200:]})")
    cod, txt = e2fsck(img)
    if cod:
        print(txt[-3000:])
    verificar(cod == 0, f"D: e2fsck -fn limpio tras reparar (código {cod})")
    for nome, dados in reais.items():
        verificar(debugfs(img, f"cat /{nome}") == dados, f"D: {nome} intacto (sha256)")
    verificar(debugfs(img, f"cat /lost+found/#{orf}") == orfao, "D: el archivo del índice 0 del grupo 7 está en /lost+found con su contenido")
    for g in (3, 4, 6):
        with open(img, "rb") as f:
            f.seek(its[g] * 4096)
            verificar(f.read(ipg * isz) == bytes(ipg * isz), f"D: la tabla de inodos del grupo {g} queda a cero")
    usados = sb_campo(img, 4, "<I") - sb_campo(img, 12, "<I")
    verificar(usados <= usados_antes + 2, f"D: no hay bloques de más en uso ({usados_antes} -> {usados}; +1 de lost+found como mucho)")
    r = subprocess.run([exe, "analisar", img], capture_output=True, text=True)
    verificar(r.returncode == 0, f"D: una segunda pasada no encuentra nada ({r.stdout[-300:]})")
    r = subprocess.run([exe, "deshacer", img, copia], capture_output=True, text=True)
    verificar(r.returncode == 0 and sha_arq(img) == antes, f"D: deshacer deja la imagen exacta de antes ({r.stdout.strip()})")
    for p in (img, copia):
        os.remove(p)


def rotulo(img, inicio):
    with open(img, "rb") as f:
        f.seek(inicio * 512 + 1024 + 120)
        return f.read(16).split(b"\0")[0].decode("latin-1")


def apagar(m):
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 60
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.3)


def parte_c(arriscado=False):
    """El caso del Amilo: /home (SAGUI-DATOS) en el disco de arranque, estropeada tras un cuelgue.
    Sin «arriscado» (un bloque de dos archivos + «no se desmontó bien»): el servicio del arranque la repara
    solo (reparar -p) y la deja en rw. Con él (además, una entrada de carpeta a un inodo borrado: se
    descartaría un nombre): el servicio NO la toca, sigue en solo lectura con el aviso, y luego se arregla
    a mano con «reparar -y»."""
    t = "E" if arriscado else "C"
    origem = imagem_de_teste("", ["--vesa", "--console", "--dados-mb", "32", "--dados-fs", "ext2"])
    img = os.path.join(SAIDA, f"h397{t.lower()}.img")
    shutil.copyfile(origem, img)
    num = ini = qtd = None
    for n, _t, i, q in particoes_img(img):
        if rotulo(img, i) == "SAGUI-DATOS":
            num, ini, qtd = n, i, q
    verificar(num is not None, f"{t}: la imagen lleva /home en SAGUI-DATOS")
    home = extrair_particao(img, ini, qtd // 2048)
    local = os.path.join(SAIDA, f"h397{t.lower()}")
    os.makedirs(local, exist_ok=True)
    nombres = [("sano.txt", SANOS["sano.txt"]), ("comp_a.bin", ROTOS["comp_a.bin"]), ("comp_b.bin", ROTOS["comp_b.bin"])]
    if arriscado:
        nombres.append(("borrado.txt", ROTOS["borrado.txt"]))
    for nome, dados in nombres:
        with open(os.path.join(local, nome), "wb") as f:
            f.write(dados)
        dfs(home, f"write {os.path.join(local, nome)} {nome}")
    a = blocos(home, "/comp_a.bin")
    ordenes = [f"sif /comp_b.bin block[1] {a[1]}", "ssv state 0"]   # un bloque de dos archivos, sin desmontar bien
    if arriscado:
        ordenes[:0] = ["sif /borrado.txt links_count 0", "sif /borrado.txt dtime 1700000500"]
    dfs(home, *ordenes)
    cod, _txt = e2fsck(home)
    verificar(cod != 0, f"{t}: /home estropeada (e2fsck lo ve)")
    with open(home, "rb") as f, open(img, "r+b") as g:
        g.seek(ini * 512)
        g.write(f.read())
    os.remove(home)
    vol = f"hda{num}"
    with Maquina(f"h397-{t.lower()}", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar(f"ejecuta «reparar {vol}»" in s or f"reparar {vol}" in s, f"{t}: el kernel avisa «reparar {vol}»")
        if not arriscado:
            m.esperar_serial("reparar: arranque:", 120)
            m.esperar_serial(f"{vol}: comprobación de después", 120)
            time.sleep(3)
            s = m.serial()
            verificar(f"reparar: {vol} estaba sin desmontar bien y se ha reparado" in s, f"{t}: el servicio del arranque la repara solo")
            out = m.comando("mount")
            verificar(re.search(rf"{vol}\s+en /home.*\(rw", out) is not None, f"{t}: /home vuelve a estar rw sin que nadie haga nada ({out.strip()[-300:]})")
        else:
            m.esperar_serial("reparar: arranque:", 120)
            m.esperar_serial(f"{vol}: -p:", 120)
            time.sleep(3)
            s = m.serial()
            verificar(f"reparar: {vol} sigue de solo lectura: ejecuta «reparar {vol}»" in s, f"{t}: el servicio no toca lo arriesgado y avisa")
            out = m.comando("mount")
            verificar(re.search(rf"{vol}\s+en /home.*\(ro", out) is not None, f"{t}: /home sigue de solo lectura ({out.strip()[-300:]})")
            out = m.comando(f"reparar -y {vol}", 300)
            print(out[-2000:])
            verificar("Comprobado otra vez: todo cuadra" in out and "montada otra vez en /home, con escritura" in out,
                      f"{t}: «reparar -y» a mano la arregla y la monta con escritura")
        m.comando("echo despues de reparar > /home/nuevo.txt")
        out = m.comando("cat /home/nuevo.txt")
        verificar("despues de reparar" in out, f"{t}: se escribe en /home")
        m.comando("sync")
        apagar(m)
    home = extrair_particao(img, ini, qtd // 2048)
    cod, txt = e2fsck(home)
    if cod:
        print(txt)
    verificar(cod == 0, f"{t}: e2fsck -fn limpio de /home después (código {cod})")
    verificar(debugfs(home, "cat /sano.txt") == SANOS["sano.txt"], f"{t}: el archivo sano de /home sigue igual")
    verificar(debugfs(home, "cat /nuevo.txt") == b"despues de reparar\n", f"{t}: lo escrito después está en /home")
    os.remove(home)
    os.remove(img)


def parte_f():
    exe_reparo = compilar()
    d = os.path.join(SAIDA, "h397-formatear")
    os.makedirs(d, exist_ok=True)
    rec = os.path.join(RAIZ, "tests", "recursos", "redimensao_anfitriao")
    shutil.copy(os.path.join(rec, "sagui.h"), d)
    shutil.copy(os.path.join(RAIZ, "userland", "inc", "particoes.h"), d)
    exe = os.path.join(d, "redimensao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I", d, "-o", exe, os.path.join(rec, "principal.c"),
                    os.path.join(RAIZ, "userland", "libsagui", "redimensao.c"),
                    os.path.join(RAIZ, "userland", "libsagui", "formatacao.c")], check=True)
    dumpe2fs = os.path.join(os.path.dirname(DEBUGFS), "dumpe2fs")
    for mb, bs in ((24, 1024), (600, 4096)):
        img = os.path.join(d, f"f{mb}.img")
        if os.path.exists(img):
            os.remove(img)
        with open(img, "wb") as f:
            f.truncate(mb * 1024 * 1024)
        r = subprocess.run([exe, "formatear-ext2", img, "SAGUI-DATOS"], capture_output=True, text=True)
        verificar(r.returncode == 0, f"F {mb} MB: el formateador termina bien ({r.stdout.strip()[-120:]})")
        cod, txt = e2fsck(img)
        if cod:
            print(txt[-1500:])
        verificar(cod == 0, f"F {mb} MB ({bs}): e2fsck -fn limpio con resize_inode (código {cod})")
        h = subprocess.run([dumpe2fs, "-h", img], capture_output=True, text=True).stdout
        verificar("resize_inode" in h and re.search(r"Reserved GDT blocks:\s+[1-9]", h) is not None, f"F {mb} MB: resize_inode y bloques GDT reservados ({bs})")
        a = subprocess.run([exe_reparo, "analisar", img], capture_output=True, text=True)
        verificar(a.returncode == 0 and "nada que reparar" in a.stdout, f"F {mb} MB: «reparar» no ve nada ({a.stdout[-200:]})")
        rodar = subprocess.run([exe, "minimo", img], capture_output=True, text=True)
        _ = rodar
        for fator in (3, 16):
            novo = mb * 2048 * fator
            with open(img, "r+b") as f:
                f.truncate(novo * 512)
            r = subprocess.run([exe, "redimensionar", img, str(novo)], capture_output=True, text=True)
            cod, txt = e2fsck(img)
            if cod:
                print(txt[-1500:])
            verificar(r.returncode == 0 and cod == 0, f"F {mb} MB: crece x{fator} (con los reservados) y e2fsck queda limpio ({r.stdout.strip()[-150:]})")
        os.remove(img)


def main():
    solo = os.environ.get("H397_PARTES", "abcdef")
    if "f" in solo:
        parte_f()
    if "a" in solo:
        parte_a()
    if "b" in solo:
        parte_b()
    if "c" in solo:
        parte_c()
    if "d" in solo:
        parte_d()
    if "e" in solo:
        parte_c(True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
