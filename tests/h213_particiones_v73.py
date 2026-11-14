#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H213 (D213, v7.3) - imágenes con una sola primaria y todo lo demás en una extendida;
Discos > Particiones: ampliar la extendida, crear lógicas y redimensionar con el disco en uso.

Parte 1 (en el anfitrión, sin QEMU): userland/libsagui/redimensao.c y formatacao.c compilados
  para el PC (tests/recursos/redimensao_anfitriao) sobre volúmenes de mke2fs y mkfs.vfat:
  - ext2 con bloques de 1, 2 y 4 KB, con y sin resize_inode: crecer (también gastando bloques
    reservados de la tabla de grupos) y encoger varias veces; e2fsck -fn limpio después de cada
    paso y los archivos siguen (debugfs);
  - encoger por encima de un bloque en uso al final se niega y no cambia ni un byte;
  - sin resize_inode no crece más de lo que da su tabla (y lo dice);
  - FAT32 de mkimagem (con la FAT dimensionada) y de SaguiOS: crecer y fsck.vfat -n limpio.
Parte 2 (QEMU, IDE, el disco de arranque):
  - la imagen de mkimagem: con sfdisk -d, la de SaguiOS (0xDA, LBA 1) es la única primaria y
    SAGUI-DATOS, TMP, LOGS (FAT32) y APPS son lógicas de una extendida (0x0F);
  - arranca y monta /home, /tmp, /logs y /apps;
  - la imagen se agranda (qemu-img resize, como grabarla en un pendrive mayor) y desde SaguiOS,
    con todo montado: "particiones ampliar", crear una lógica ext2 y otra FAT32, montarlas y
    escribir; la FAT32 crece de 34 a 60 MB;
  - lo montado no se toca (redimensionar /home montado se niega) y la de SaguiOS tampoco;
  - /home desmontado: su ext2 encoge (64 -> 40 MB) y vuelve a crecer (-> 56 MB);
  - se apaga, vuelve a arrancar y todo se monta igual (/home desde la partición redimensionada);
  - desde el PC: la tabla nueva con sfdisk, e2fsck -fn y fsck.vfat -n limpios y los archivos.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, extrair_particao, e2fsck, debugfs, particoes_img, opcoes_apps,  # noqa: E402
                  RAIZ, SAIDA, BUILD)

MB = 1024 * 1024
E2 = "/usr/sbin"


def herramienta(nome):
    for d in (E2, "/sbin", os.path.expanduser("~/.local/saguitools/root/usr/sbin"), os.path.expanduser("~/.local/saguitools/root/sbin")):
        if os.path.exists(os.path.join(d, nome)):
            return os.path.join(d, nome)
    return shutil.which(nome) or nome


MKE2FS, MKFS_VFAT, FSCK_VFAT = herramienta("mke2fs"), herramienta("mkfs.vfat"), herramienta("fsck.vfat")


# ------------------------------------------------------------ parte 1: en el anfitrión

def compilar():
    d = os.path.join(SAIDA, "h213-anfitriao")
    os.makedirs(d, exist_ok=True)
    rec = os.path.join(RAIZ, "tests", "recursos", "redimensao_anfitriao")
    shutil.copy(os.path.join(rec, "sagui.h"), d)
    shutil.copy(os.path.join(RAIZ, "userland", "inc", "particoes.h"), d)
    exe = os.path.join(d, "redimensao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I", d, "-o", exe, os.path.join(rec, "principal.c"),
                    os.path.join(RAIZ, "userland", "libsagui", "redimensao.c"),
                    os.path.join(RAIZ, "userland", "libsagui", "formatacao.c")], check=True)
    return exe, d


def rodar(exe, *args):
    r = subprocess.run([exe, *map(str, args)], capture_output=True, text=True)
    return r.returncode, r.stdout.strip()


def fsck_ext(img):
    r = subprocess.run([os.path.join(E2, "e2fsck"), "-fn", img], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def fsck_vfat(img):
    r = subprocess.run([FSCK_VFAT, "-n", img], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def sha(f):
    return hashlib.sha256(open(f, "rb").read()).hexdigest()


def parte1():
    exe, d = compilar()
    conteudo = os.path.join(d, "conteudo")
    os.makedirs(os.path.join(conteudo, "sub"), exist_ok=True)
    with open(os.path.join(conteudo, "hola.txt"), "w") as f:
        f.write("hola desde el anfitrion\n")
    with open(os.path.join(conteudo, "sub", "grande.bin"), "wb") as f:
        f.write(bytes(range(256)) * 4096 * 3)                       # 3 MB
    img = os.path.join(d, "ext.img")
    pasos = (300, 900, 100, 40, 200, 24, 64)
    for opts, nome in ((["-b", "1024"], "1 KB con resize_inode"), (["-b", "2048"], "2 KB"), (["-b", "4096"], "4 KB"),
                       (["-b", "1024", "-O", "^resize_inode"], "1 KB sin resize_inode")):
        if os.path.exists(img):
            os.remove(img)
        with open(img, "wb") as f:
            f.truncate(64 * MB)
        subprocess.run([MKE2FS, "-q", "-F", "-t", "ext2", *opts, "-m", "1", "-L", "H213", "-d", conteudo, img], check=True)
        bien = True
        for mb in pasos:
            if mb * MB > os.path.getsize(img):
                with open(img, "r+b") as f:
                    f.truncate(mb * MB)                              # la partición crece antes
            rc, out = rodar(exe, "redimensionar", img, mb * 2048)
            sin_sitio = "no tiene sitio" in out
            if rc == 0 and mb * MB < os.path.getsize(img):
                with open(img, "r+b") as f:
                    f.truncate(mb * MB)                              # ...y encoge después
            rcf, outf = fsck_ext(img)
            dato = debugfs(img, "cat /hola.txt")
            gran = debugfs(img, "cat /sub/grande.bin")
            ok = (rc == 0 or sin_sitio) and rcf == 0 and b"hola desde" in dato and gran == bytes(range(256)) * 4096 * 3
            if not ok:
                print(f"    [{nome}] {mb} MB: rc={rc} {out}\n{outf[-600:]}")
            bien = bien and ok
            if "sin resize_inode" in nome and mb == 300:
                verificar(sin_sitio and "256 MB" in out, f"[{nome}] sin bloques reservados no crece más de lo que da su tabla ({out})")
        verificar(bien, f"[{nome}] crecer y encoger {pasos} MB: e2fsck -fn limpio y los archivos intactos en cada paso")
    # Encoger por encima de un bloque en uso: se niega sin tocar nada.
    os.remove(img)
    with open(img, "wb") as f:
        f.truncate(64 * MB)
    subprocess.run([MKE2FS, "-q", "-F", "-t", "ext2", "-b", "1024", "-d", conteudo, img], check=True)
    subprocess.run([os.path.join(E2, "debugfs"), "-w", "-R", "setb 60000", img], capture_output=True, check=True)
    antes = sha(img)
    rc, out = rodar(exe, "redimensionar", img, 40 * 2048)
    verificar(rc != 0 and "hay datos al final" in out and "no mueve datos" in out, f"encoger por encima de un bloque en uso se niega ({out})")
    verificar(sha(img) == antes, "y el volumen no ha cambiado ni un byte")
    # debugfs setb marca el mapa pero no la cuenta del grupo: la comprobación lo ve.
    rc, out = rodar(exe, "verificar", img)
    verificar(rc != 0 and "pásale e2fsck" in out, f"la comprobación de solo lectura ve un mapa que no cuadra ({out})")
    subprocess.run([os.path.join(E2, "debugfs"), "-w", "-R", "freeb 60000", img], capture_output=True, check=True)
    rc, out = rodar(exe, "verificar", img)
    verificar(rc == 0 and "comprobado" in out, f"la comprobación de solo lectura del ext2 sano ({out})")
    # FAT32: el de mkimagem (FAT dimensionada) y el de SaguiOS crecen.
    fat = os.path.join(d, "fat.img")
    for origem in ("mkimagem", "SaguiOS"):
        if os.path.exists(fat):
            os.remove(fat)
        with open(fat, "wb") as f:
            f.truncate(40 * MB)
        if origem == "mkimagem":
            sys.path.insert(0, os.path.join(RAIZ, "tools"))
            import mkimagem   # noqa: E402
            with __import__("tempfile").TemporaryDirectory() as tmp:
                vol = mkimagem.fat32_com_margem(tmp, 40 * 2048, "H213", MKFS_VFAT)
                verificar(vol is not None, "mkimagem hace el FAT32 con la FAT dimensionada para crecer")
                shutil.copy(vol, fat)
        else:
            rc, out = rodar(exe, "formatear-fat32", fat, "H213")
            verificar(rc == 0, f"el formateo FAT32 de SaguiOS ({out})")
        rc, out = fsck_vfat(fat)
        verificar(rc == 0, f"[{origem}] fsck.vfat -n da por bueno el FAT32 recién hecho")
        with open(fat, "r+b") as f:
            f.truncate(70 * MB)
        rc, out = rodar(exe, "crecer-fat32", fat)
        verificar(rc == 0 and "ha crecido de 40 a" in out, f"[{origem}] el FAT32 crece ({out})")
        rcf, outf = fsck_vfat(fat)
        rcv, outv = rodar(exe, "verificar", fat)
        verificar(rcf == 0 and rcv == 0, f"[{origem}] fsck.vfat -n y la comprobación de SaguiOS, limpios tras crecer ({outv})")


# ------------------------------------------------------------ parte 2: QEMU

def sfdisk(img):
    r = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True).stdout
    t = {}
    for m in re.finditer(r"\.img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)(, bootable)?", r):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), m.group(4), bool(m.group(5)))
    return t


def criar_imagem():
    img = os.path.join(SAIDA, "h213.img")
    b = BUILD
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", f"{b}/boot/mbr.bin", "--estagio2", f"{b}/boot/estagio2.bin", "--kernel", f"{b}/kernel.bin",
                    "--kernel-elf", f"{b}/kernel.elf", "--ramdisk", f"{b}/ramdisk.sgfs", "--saida", img,
                    "--vesa", "--console", "--dados-mb", "64", "--tmp-mb", "8", "--logs-mb", "40", "--logs-fs", "fat32",
                    *opcoes_apps()], check=True, stdout=subprocess.DEVNULL)
    return img


def apagar(m):
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 40
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.2)
    verificar(m.proc.poll() is not None, "apagar-pc: QEMU termina solo (apagado ordenado)")


def parte2():
    img = criar_imagem()
    t = sfdisk(img)
    print("tabla:", t)
    verificar(t.get(1) and t[1][0] == 1 and t[1][2] == "da" and t[1][3], "sfdisk: la 1 es la de SaguiOS (0xDA, LBA 1, activa)")
    verificar(t.get(2) and t[2][2] == "f" and 3 not in t and 4 not in t, "sfdisk: la 2 es una extendida y no hay más primarias")
    con_apps = bool(opcoes_apps())
    tipos = [t.get(n, (0, 0, ""))[2] for n in range(5, 9 if con_apps else 8)]
    verificar(tipos == (["83", "83", "c", "83"] if con_apps else ["83", "83", "c"]),
              f"sfdisk: /home, /tmp, /logs (FAT32){' y /apps' if con_apps else ''} son lógicas de la extendida ({tipos})")
    ext_fim = t[2][0] + t[2][1]
    verificar(ext_fim * 512 == os.path.getsize(img), "la extendida llega hasta el final de la imagen")
    # Como grabarla en un pendrive mayor.
    subprocess.run(["qemu-img", "resize", "-f", "raw", img, f"{os.path.getsize(img) + 256 * MB}"], check=True,
                   stdout=subprocess.DEVNULL)

    with Maquina("h213a", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[FSTAB\] arranque -> /home \(auto, rw\): 0", s), "el fstab monta /home (la primera de datos, lógica)")
        for etq, ponto in (("SAGUI-TMP", "/tmp"), ("SAGUI-LOGS", "/logs")):
            verificar(re.search(rf"\[FSTAB\] LABEL={etq} -> {ponto} \(auto, rw\): 0", s), f"el fstab monta {ponto} por etiqueta")
        out = m.comando("mount")
        for ponto in ("/home", "/tmp", "/logs") + (("/apps",) if con_apps else ()):
            verificar(ponto in out, f"mount lista {ponto}")
        m.comando("echo dato-de-home > /home/h213.txt")
        out = m.comando("particiones hda")
        verificar(re.search(r"libre .*\(primaria\)", out), "particiones ve el espacio libre nuevo al final del disco")
        out = m.comando("particiones crear hda 10 ext2 --logica -s")
        verificar("disco de arranque" in out and "creada" not in out, "sin --arranque, el disco de arranque no se toca")
        out = m.comando("particiones ampliar hda --arranque -s", 120)
        verificar("la extendida de hda pasa de" in out, f"particiones ampliar: la extendida llega al final ({out.strip()[-160:]})")
        out = m.comando("particiones crear hda 100 ext2 --logica --etiqueta NUEVA --arranque -s", 300)
        nova = re.search(r"creada (hda\d+): 100 MB", out)
        verificar(nova, f"crear una lógica ext2 de 100 MB en el hueco nuevo con todo montado ({out.strip()[-160:]})")
        nova = nova.group(1)
        out = m.comando(f"mount -o rw {nova} /m213")
        m.comando("echo en-la-nueva > /m213/x.txt")
        out = m.comando("cat /m213/x.txt")
        verificar("en-la-nueva" in out, f"la lógica nueva ({nova}) se monta y se escribe")
        m.comando("umount /m213")
        out = m.comando("particiones crear hda 34 fat32 --logica --etiqueta NUEVAFAT --arranque -s", 300)
        nfat = re.search(r"creada (hda\d+): 34 MB", out)
        verificar(nfat, f"crear una lógica FAT32 de 34 MB ({out.strip()[-120:]})")
        nfat = nfat.group(1)
        out = m.comando(f"particiones redimensionar {nfat} 60 --arranque -s", 300)
        verificar(f"{nfat} pasa de 34 a 60 MB" in out and "ha crecido" in out, f"el FAT32 nuevo crece de 34 a 60 MB ({out.strip()[-200:]})")
        out = m.comando(f"mount -o rw {nfat} /m213f")
        m.comando("echo en-la-fat > /m213f/y.txt")
        m.comando("umount /m213f")
        # Lo montado y la de SaguiOS no se tocan.
        out = m.comando("particiones redimensionar hda5 40 --arranque -s")
        verificar("está montada en /home" in out and "pasa de" not in out, "redimensionar /home montado se niega")
        out = m.comando("particiones borrar hda1 --arranque -s")
        verificar("partición de SaguiOS" in out and "borrada" not in out, "la partición de SaguiOS no se borra")
        # /home desmontado: encoger y crecer su ext2.
        m.comando("cd /")
        out = m.comando("umount /home")
        out = m.comando("particiones comprobar hda5", 120)
        verificar("comprobado" in out, f"particiones comprobar hda5 ({out.strip()[-120:]})")
        out = m.comando("particiones redimensionar hda5 40 --arranque -s", 300)
        verificar("hda5 pasa de 64 a 40 MB" in out and "el ext2 de hda5 pasa de" in out, f"/home: su ext2 encoge de 64 a 40 MB ({out.strip()[-200:]})")
        out = m.comando("particiones redimensionar hda5 56 --arranque -s", 300)
        verificar("hda5 pasa de 40 a 56 MB" in out and "el ext2 de hda5 pasa de" in out, f"/home: y vuelve a crecer a 56 MB ({out.strip()[-200:]})")
        out = m.comando("particiones redimensionar hda7 20 --arranque -s")
        verificar("montada" in out or "solo crece" in out, "un FAT32 no se encoge por debajo de lo que ocupa (o está montado)")
        out = m.comando("mount -o rw hda5 /home")
        out = m.comando("cat /home/h213.txt")
        verificar("dato-de-home" in out, "/home vuelve a montarse y conserva sus archivos")
        s = m.serial()
        verificar("gravacao recusada" not in s, "ninguna escritura rechazada por el cerrojo de D040")
        verificar(not re.search(r"\[BLOCOS\] .*(cambiaría o quitaría|rechazada)", s),
                  "el kernel no tuvo que rechazar ninguna escritura de particiones (las comprobaciones se hacen antes)")
        m.captura("H213-1-terminal")
        print(m.comando("particiones hda"))
        apagar(m)

    with Maquina("h213b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[FSTAB\] arranque -> /home \(auto, rw\): 0", s), "segundo arranque: /home se monta (redimensionada)")
        out = m.comando("cat /home/h213.txt")
        verificar("dato-de-home" in out, "segundo arranque: /home conserva sus archivos")
        out = m.comando("mount")
        verificar("/logs" in out and "/tmp" in out, "segundo arranque: /tmp y /logs siguen montados")
        apagar(m)

    t = sfdisk(img)
    print("tabla final:", t)
    verificar(t[2][0] + t[2][1] > ext_fim + 200 * 2048, "anfitrión: la extendida ocupa el disco agrandado")
    verificar(t[5][1] == 56 * 2048, "anfitrión: /home (la 5) mide 56 MB")
    num = {p[0]: p for p in particoes_img(img)}
    n_nova, n_fat = int(re.sub(r"\D", "", nova)), int(re.sub(r"\D", "", nfat))
    verificar(num[n_nova][1] == 0x83 and num[n_nova][3] == 100 * 2048, "anfitrión: la lógica ext2 nueva, de 100 MB")
    verificar(num[n_fat][1] == 0x0C and num[n_fat][3] == 60 * 2048, "anfitrión: la lógica FAT32 nueva, ya de 60 MB")
    for n, nombre in ((5, "/home"), (6, "/tmp"), (n_nova, "la nueva ext2")):
        p = extrair_particao(img, num[n][2], num[n][3] // 2048)
        rc, o = e2fsck(p)
        if n == 5:
            verificar(b"dato-de-home" in debugfs(p, "cat /h213.txt"), "anfitrión: /home/h213.txt en la partición redimensionada")
        if n == n_nova:
            verificar(b"en-la-nueva" in debugfs(p, "cat /x.txt"), "anfitrión: el archivo de la lógica nueva")
        os.remove(p)
        verificar(rc == 0, f"anfitrión: e2fsck -fn limpio en {nombre}" + ("" if rc == 0 else "\n" + o[-600:]))
    for n, nombre in ((7, "/logs"), (n_fat, "la nueva FAT32")):
        p = extrair_particao(img, num[n][2], num[n][3] // 2048)
        rc, o = fsck_vfat(p)
        if n == n_fat:
            r = subprocess.run(["mtype", "-i", p, "::/y.txt"], capture_output=True, text=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
            verificar("en-la-fat" in r.stdout, "anfitrión: el archivo de la FAT32 nueva")
        os.remove(p)
        verificar(rc == 0, f"anfitrión: fsck.vfat -n limpio en {nombre}" + ("" if rc == 0 else "\n" + o[-400:]))


def main():
    parte1()
    if os.environ.get("H213_SOLO_ANFITRIAO"):
        return 0
    parte2()
    print("H213 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
