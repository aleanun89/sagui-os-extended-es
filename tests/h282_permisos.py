#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H282 (D282, mejora 098, fase 2) - permisos de archivos.

Una imagen de consola con la sesión como root y, además del ramdisk (SGFS) y de la
partición SAGUI-CONF (ext2, en /sistema/conf):
  - /home en ext2 (SAGUI-DATOS) y /logs en ext2 (SAGUI-LOGS, que el kernel deja en 1777);
  - dos FAT32: SAGUI-X1 en /fatroot con «uid=root,gid=root,umask=022» y SAGUI-X2 en
    /fatsagui con las opciones de serie (de sagui, 022), las dos por el /etc/fstab.

1. /testes/permisos --matriz: como root prepara archivos y carpetas con permisos
   conocidos (y comprueba que root lo puede todo y que el FAT32 no guarda permisos);
   luego se convierte en sagui y recorre la matriz: leer, escribir, crear, borrar,
   renombrar, mkdir, rmdir, listar, entrar, ejecutar, chmod, chown, chgrp, umask y el
   pegajoso, en RAM, ext2 y FAT32; y lo que protegía D255 (/, /etc, /bin, /sistema,
   la sombra) sigue protegido.
2. En el shell: ls -l (rwxr-xr-x dueño grupo), umask (y que lo heredan los hijos),
   chmod en octal y con letras, chown y chgrp.
3. Reinicio: el ext2 guarda dueño, grupo y modo.
"""
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402

TRAB = os.path.join(SAIDA, "h282")
B = os.path.join(RAIZ, "build")


def construir_imagen():
    os.makedirs(TRAB, exist_ok=True)
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(B, "rootfs"), raiz)
    verificar(os.path.exists(os.path.join(raiz, "testes", "permisos")), "/testes/permisos está en el ramdisk")
    with open(os.path.join(raiz, "etc", "fstab"), "a") as f:
        f.write("# prueba h282\n"
                "LABEL=SAGUI-X1   /fatroot   auto   rw,uid=root,gid=root,umask=022\n"
                "LABEL=SAGUI-X2   /fatsagui  auto   rw\n")
    sgfs = os.path.join(TRAB, "h282.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h282.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(B, "boot", "mbr.bin"), "--estagio2", os.path.join(B, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--console", "--sesion-root",
                    "--home-mb", "16", "--home-fs", "ext2", "--logs-mb", "16", "--logs-fs", "ext2",
                    "--part-fs", "fat32", "--extra-mb", "40", "--extra-mb", "40"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def linha_ls(out, nome):
    for l in out.splitlines():
        if l.rstrip().endswith(nome):
            return l
    return ""


def main():
    img = construir_imagen()
    with Maquina("h282", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/", 120)
        s = m.serial()
        verificar("[PERMISOS] sesion como root" in s, "la sesión de la prueba es root")
        verificar(re.search(r"\[FSTAB\] LABEL=SAGUI-X1 -> /fatroot \(auto, rw,uid=root,gid=root,umask=022\): 0", s),
                  "fstab: el FAT32 con uid=root,gid=root,umask=022 se monta en /fatroot")
        verificar(re.search(r"\[FSTAB\] LABEL=SAGUI-X2 -> /fatsagui .*: 0", s), "fstab: el otro FAT32 en /fatsagui")
        verificar(re.search(r"\[MONTAGEM\] \S+ \(ext2\) montado em /home", s), "/home es ext2")
        verificar(re.search(r"\[MONTAGEM\] \S+ \(ext2\) montado em /logs", s), "/logs es ext2")

        # --- 1. la matriz
        out = m.comando("/testes/permisos --matriz", 120)
        g = re.search(r"permisos: matriz ok=(\d+) mal=(\d+)", out)
        verificar(g and g.group(2) == "0" and int(g.group(1)) > 120,
                  f"matriz de permisos como root y como sagui ({g and g.group(0)}){'' if g and g.group(2) == '0' else chr(10) + out[-3000:]}")
        verificar("[PERMISOS] denegado a permisos" in m.serial(), "las negativas se anotan en el registro")

        # --- 2. el shell
        out = m.comando("ls -l --color=never /")
        l = linha_ls(out, "bin/")
        verificar(re.match(r"drwxr-xr-x root +root ", l), f"ls -l /: bin es drwxr-xr-x root root ({l!r})")
        l = linha_ls(out, "sistema/")
        verificar(re.match(r"drwx--x--x root +root ", l), f"ls -l /: sistema es drwx--x--x (D313: se atraviesa hasta /sistema/idiomas) ({l!r})")
        l = linha_ls(out, "tmp/")
        verificar(re.match(r"drwxrwxrwt root +root ", l), f"ls -l /: tmp es drwxrwxrwt ({l!r})")
        l = linha_ls(out, "home/")
        verificar(re.match(r"drwxr-xr-x sagui +sagui ", l), f"ls -l /: home es de sagui ({l!r})")
        out = m.comando("umask")
        verificar(re.search(r"^0022$", out, re.M), "umask de serie: 0022")
        out = m.comando("umask 027; sagsh -c umask")
        verificar(re.search(r"^0027$", out, re.M), "un hijo hereda la umask (0027)")
        m.comando("umask 077; echo hola > /tmp/u.txt; umask 022")
        out = m.comando("ls -l --color=never /tmp/u.txt")
        verificar(re.search(r"^-rw------- root +root ", out, re.M), f"con umask 077 nace -rw------- ({out.strip()[-120:]!r})")
        m.comando("chmod 640 /tmp/u.txt")
        out = m.comando("ls -l --color=never /tmp/u.txt")
        verificar(re.search(r"^-rw-r----- ", out, re.M), "chmod 640")
        m.comando("chmod u+x,o+r /tmp/u.txt")
        out = m.comando("ls -l --color=never /tmp/u.txt")
        verificar(re.search(r"^-rwxr--r-- ", out, re.M), "chmod u+x,o+r")
        m.comando("chown sagui:sagui /tmp/u.txt")
        out = m.comando("ls -l --color=never /tmp/u.txt")
        verificar(re.search(r"^-rwxr--r-- sagui +sagui ", out, re.M), "chown sagui:sagui")
        m.comando("chgrp root /tmp/u.txt")
        out = m.comando("ls -l --color=never /tmp/u.txt")
        verificar(re.search(r"^-rwxr--r-- sagui +root ", out, re.M), "chgrp root")
        out = m.comando("chmod 999 /tmp/u.txt")
        verificar("modo no válido" in out, "chmod con un modo malo")
        out = m.comando("chmod 600 /fatroot/f")
        verificar("chmod: /fatroot/f:" in out, "chmod en FAT32: error")
        out = m.comando("ls -l --color=never /fatsagui")
        verificar(re.search(r"rwxr-xr-x sagui +sagui ", out) or "leeme" not in out, "ls -l en FAT32 de sagui")
    # --- 3. el ext2 los guarda
    with Maquina("h282b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/", 120)
        out = m.comando("/testes/permisos --ver /home/persiste")
        verificar("permisos: /home/persiste 0751 1000 1000 1" in out, f"tras reiniciar, /home/persiste 0751 sagui ({out.strip()[-80:]!r})")
        out = m.comando("/testes/permisos --ver /logs/persiste")
        verificar("permisos: /logs/persiste 0640 1000 100 1" in out, f"tras reiniciar, /logs/persiste 0640 1000:100 ({out.strip()[-80:]!r})")
        out = m.comando("/testes/permisos --ver /logs")
        verificar("permisos: /logs 1777 0 0 1" in out, "/logs: 1777 de root")
    print("H282 OK")


if __name__ == "__main__":
    main()
