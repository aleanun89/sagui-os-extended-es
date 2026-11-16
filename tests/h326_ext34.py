#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H326 (D326, D337) - ext3/ext4: reproducción del diario JBD2 y escritura.

Parte "unidad" (en el PC, sin QEMU): tests/ext34_unidad.c compila kernel/fs/ext2.c (con la
escritura en ext3/ext4), jbd2.c y crc32c.c contra un disco simulado. Pruebas de unidad de
CRC32c, CRC16, la lista de extents y el diario JBD2; después, sobre volúmenes de mkfs (ext3,
ext4 por defecto con metadata_csum, ext4 sin metadata_csum, ext4 de bloques de 1 KB), una carga
(carpetas, archivos, borrar, grandes fragmentados, renombrar, truncar) con un corte de luz en
cada escritura: cada estado se recupera con nuestro código y con el de Linux y los dos tienen que
dar "e2fsck -fn" limpio. En la batería se comprueba uno de cada 10 cortes; con "completo", todos.
Los volúmenes sin diario (ext4 sin has_journal y ext2) solo se comprueban al final: sin diario un
corte no puede quedar coherente.

Parte "qemu": un pendrive con un ext3 y un ext4 (los dos con una transacción confirmada y sin
aplicar en el diario, hecha con debugfs), un ext4 sin metadata_csum y un ext4 sin diario. El
SaguiOS los monta con escritura, reproduce el diario (a.txt pasa a "DESPUES") y hace en cada uno:
crear, escribir dos archivos grandes a la vez (fragmentados), copiar, renombrar, borrar, truncar,
crear y borrar una carpeta. En el anfitrión: e2fsck -fn limpio y el contenido con debugfs.

Uso: h326_ext34.py [unidad] [qemu] [completo]
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, DEBUGFS, E2FSCK, SAIDA, RAIZ)


# ------------------------------------------------------------ parte unidad

def unidad(completo):
    d = os.path.join(SAIDA, "h326-unidad")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    exe = os.path.join(d, "ext34_unidad")
    r = subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Wno-unused-function", "-fsanitize=address,undefined",
                        "-include", os.path.join(RAIZ, "tests", "ext34_simulado.h"), "-I", os.path.join(RAIZ, "kernel", "inc"),
                        "-o", exe, os.path.join(RAIZ, "tests", "ext34_unidad.c"),
                        os.path.join(RAIZ, "kernel", "fs", "jbd2.c"), os.path.join(RAIZ, "kernel", "lib", "crc32c.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "[unidad] compila ext34_unidad\n" + r.stderr)
    r = subprocess.run([exe, "unidad"], capture_output=True, text=True)
    print(r.stdout[-3000:], r.stderr[-1500:])
    verificar(r.returncode == 0 and "0 fallos" in r.stdout, "[unidad] CRC32c, CRC16, extents y JBD2")
    paso = "1" if completo else "10"
    # Temporales en memoria si se puede (cada corte escribe dos imágenes de 16 MB).
    tmp = "/dev/shm" if os.path.isdir("/dev/shm") else d
    volumenes = [("ext3", "mkfs.ext3", [], True), ("ext4", "mkfs.ext4", [], True),
                 ("ext4-sin-csum", "mkfs.ext4", ["-O", "^metadata_csum"], True),
                 ("ext4-1k", "mkfs.ext4", ["-b", "1024"], True),
                 ("ext4-sin-diario", "mkfs.ext4", ["-O", "^has_journal"], False),
                 ("ext2", "mkfs.ext2", [], False)]
    # Una carpeta con 400 entradas (con índice htree): la carga añade y borra en ella.
    lista = os.path.join(d, "raiz")
    os.makedirs(os.path.join(lista, "lista"))
    for i in range(400):
        with open(os.path.join(lista, "lista", f"e{i:03d}"), "w") as f:
            f.write(f"{i}\n")
    for nome, mkfs, opc, cortes in volumenes:
        img = os.path.join(d, nome + ".img")
        with open(img, "wb") as f:
            f.truncate(16 << 20)
        subprocess.run([shutil.which(mkfs) or "/usr/sbin/" + mkfs, "-q", "-F", *opc, "-d", lista, img], check=True)
        # mke2fs -d no indexa: "e2fsck -D" sí (la carpeta pasa a tener índice htree).
        subprocess.run([E2FSCK, "-fyD", img], capture_output=True)
        verificar("Root node dump" in debugfs(img, "htree /lista").decode(), f"[unidad] {nome}: /lista con índice htree")
        t = os.path.join(tmp, f"sagui-h326-{os.getpid()}-{nome}")
        os.makedirs(t, exist_ok=True)
        r = subprocess.run([exe, "cortes", img, t, E2FSCK, "cortes" if cortes else "sin-cortes", paso],
                           capture_output=True, text=True)
        linhas = [x for x in r.stdout.splitlines() if not x.startswith("  ...")]
        print(f"== {nome}\n" + "\n".join(linhas[-40:]), r.stderr[-2000:])
        resumo = next((x for x in linhas if x.startswith("cortes:")), "")
        verificar(r.returncode == 0 and "RESULTADO:" in r.stdout and ", 0 fallos" in r.stdout,
                  f"[unidad] {nome}: carga y {'cortes de luz' if cortes else 'estado final'} limpios {resumo}")
        final = os.path.join(t, "final.img")
        if nome == "ext4":
            ex = debugfs(final, "ex /d2/grande.bin").decode()
            verificar(re.search(r"^\s*0/\s*1\s", ex, re.M), "[unidad] ext4: el archivo grande tiene árbol de extents con índice")
            ls = debugfs(final, "ls /lista").decode()
            verificar("nuevo.txt" in ls and "e200-renombrado" in ls and "e123 " not in ls and "e122" in ls,
                      "[unidad] ext4: la carpeta de 400 entradas con lo añadido, borrado y renombrado")
            verificar("Root node dump" not in debugfs(final, "htree /lista").decode(), "[unidad] ext4: y sin el índice (quitado de forma válida)")
        shutil.rmtree(t, ignore_errors=True)
    shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------ parte qemu

def pendente(particao):
    """Deja en el diario una transacción confirmada que cambia el bloque de a.txt."""
    b = int(debugfs(particao, "bmap /a.txt 0").decode().split()[-1])
    nuevo = os.path.join(SAIDA, "h326-nuevo.tmp")
    with open(nuevo, "wb") as f:
        f.write(b"DESPUES\n" + bytes(4088))
    cmds = os.path.join(SAIDA, "h326-cmds.tmp")
    with open(cmds, "w") as f:
        f.write(f"jo\njw -b {b} {nuevo}\njc\n")
    subprocess.run([DEBUGFS, "-w", "-f", cmds, particao], check=True, capture_output=True)


def indexar_y_pendente(particao):
    """La carpeta /lista con índice htree (e2fsck -D) y después la transacción pendiente."""
    subprocess.run([E2FSCK, "-fyD", particao], capture_output=True)
    pendente(particao)


def seq(a, b):
    return "".join(f"{i}\n" for i in range(a, b + 1)).encode()


VUELTAS = 8
G1 = seq(1, 4000) * VUELTAS


def carga(m, pto):
    """Las operaciones en el SaguiOS; devuelve la salida de todas."""
    out = ""
    out += m.comando(f"mkdir {pto}/d")
    out += m.comando(f"echo hola ext > {pto}/d/n.txt")
    for _ in range(VUELTAS):   # dos archivos que crecen a la vez: los dos quedan fragmentados
        out += m.comando(f"seq 1 4000 >> {pto}/d/g1.txt")
        out += m.comando(f"seq 1 2500 >> {pto}/d/g2.txt")
    out += m.comando(f"cp {pto}/d/g1.txt {pto}/d/copia.txt", tempo=120)
    out += m.comando(f"mkdir {pto}/x")
    out += m.comando(f"mv {pto}/d/n.txt {pto}/d/m.txt")
    out += m.comando(f"mv {pto}/d/m.txt {pto}/x/m.txt")
    out += m.comando(f"rm {pto}/d/g2.txt")
    out += m.comando(f"truncate -s 100000 {pto}/d/g1.txt")
    out += m.comando(f"truncate -s 5000 {pto}/d/copia.txt")
    out += m.comando(f"mkdir {pto}/vacia")
    out += m.comando(f"rmdir {pto}/vacia")
    out += m.comando(f"cat {pto}/x/m.txt")
    return out


def qemu():
    arq = {"a.txt": "ANTES\n", "viejo/b.txt": "de antes\n"}
    arq4 = dict(arq)
    for i in range(400):
        arq4[f"lista/e{i:03d}"] = f"{i}\n"
    particoes = [dict(tipo="ext3", mb=16, rotulo="TRES", arquivos=arq, depois=pendente),
                 dict(tipo="ext4", mb=32, rotulo="CUATRO", arquivos=arq4, depois=indexar_y_pendente),
                 dict(tipo="ext4", mb=16, rotulo="SINCSUM", arquivos=arq, opcoes=["-O", "^metadata_csum"]),
                 dict(tipo="ext4", mb=16, rotulo="SINDIARIO", arquivos=arq, opcoes=["-O", "^has_journal"])]
    tam = [16, 32, 16, 16]
    img, ini = criar_disco("h326-pen", particoes)
    for k in (0, 1):
        p = extrair_particao(img, ini[k], tam[k])
        verificar("needs_recovery" in subprocess.run(["dumpe2fs", "-h", p], capture_output=True, text=True).stdout,
                  f"partición {k + 1}: diario pendiente (needs_recovery)")
        verificar(b"ANTES" in debugfs(p, "cat /a.txt"), f"partición {k + 1}: a.txt aún dice ANTES")
    p = extrair_particao(img, ini[1], tam[1])
    verificar("Root node dump" in debugfs(p, "htree /lista").decode(), "partición 2: /lista (400 entradas) con índice htree")
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h326", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        for k, pto in enumerate(("/e3", "/e4", "/e4b", "/e4c")):
            dev = f"usb0p{k + 1}"
            out = m.comando(f"mount -o rw {dev} {pto}")
            verificar(f"montado en {pto}" in out and "solo lectura" not in out, f"{dev}: montado con escritura\n{out}")
            if k < 2:
                out = m.comando(f"cat {pto}/a.txt")
                verificar("DESPUE" in out, f"{dev}: el diario se reprodujo (a.txt = DESPUES)")
            out = carga(m, pto)
            if k == 1:   # la carpeta con índice htree
                out += m.comando(f"echo nuevo > {pto}/lista/nuevo.txt")
                out += m.comando(f"rm {pto}/lista/e123")
                out += m.comando(f"ls {pto}/lista | conta")
                verificar("400 líneas" in out, f"{dev}: /lista con una entrada más y una menos")
            verificar("hola ext" in out, f"{dev}: crear, renombrar y leer")
            verificar("solo lectura" not in out and "rror" not in out, f"{dev}: ninguna operación falla\n{out[-1500:]}")
            m.comando(f"umount {pto}")
        s = m.serial()
        verificar(len(re.findall(r"diario reproducido: 1 transacciones", s)) == 2,
                  "el kernel reproduce una transacción en cada volumen con diario")
        verificar(len(re.findall(r"diario ordered", s)) >= 3, "ext3, ext4 y ext4 sin metadata_csum: escritura con diario")
        verificar("Kernel panic" not in s and "PANICO" not in s, "sin pánicos")
    for k in range(4):
        p = extrair_particao(img, ini[k], tam[k], img + f".p{k}-depois")
        n = f"partición {k + 1}"
        cod, txt = e2fsck(p)
        if cod:
            print(txt)
        verificar(cod == 0, f"{n}: e2fsck -fn limpio")
        if k < 2:
            verificar(b"DESPUE" in debugfs(p, "cat /a.txt"), f"{n}: en el disco a.txt = DESPUES")
        verificar(debugfs(p, "cat /x/m.txt") == b"hola ext\n", f"{n}: /x/m.txt (creado y renombrado dos veces)")
        verificar(debugfs(p, "cat /d/g1.txt") == G1[:100000], f"{n}: /d/g1.txt fragmentado y truncado a 100000")
        verificar(debugfs(p, "cat /d/copia.txt") == G1[:5000], f"{n}: /d/copia.txt copiado y truncado a 5000")
        ls = debugfs(p, "ls -l /d").decode() + debugfs(p, "ls -l /").decode()
        verificar("g2.txt" not in ls and "n.txt" not in ls and "vacia" not in ls, f"{n}: lo borrado y renombrado ya no está")
        verificar(debugfs(p, "cat /viejo/b.txt") == b"de antes\n", f"{n}: lo de antes sigue ahí")
        if k == 1:
            ls = debugfs(p, "ls /lista").decode()
            verificar(debugfs(p, "cat /lista/nuevo.txt") == b"nuevo\n" and "e123 " not in ls and "e124" in ls,
                      f"{n}: /lista con nuevo.txt y sin e123")
    return 0


def main():
    partes = set(sys.argv[1:]) or {"unidad", "qemu"}
    completo = "completo" in partes
    if partes == {"completo"}:
        partes = {"unidad", "completo"}
    if "unidad" in partes:
        unidad(completo)
    if "qemu" in partes:
        qemu()
    return 0


if __name__ == "__main__":
    sys.exit(main())
