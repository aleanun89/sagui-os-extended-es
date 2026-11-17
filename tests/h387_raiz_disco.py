#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H387 (D379, prototipo de la mejora 035 F2) - la raíz del sistema en el disco con un initramfs mínimo.

Hasta ahora el ramdisk (todo el sistema, ~12 MB) se copia a la RAM al arrancar y es la raíz. Aquí
el ramdisk es un initramfs pequeño (tools/mkinitramfs.py: shell y unas utilidades) cuyo
/etc/raiz.conf dice al kernel que monte la partición SAGUI-RAIZ (ext2, mkimagem.py --raiz con
el árbol del ramdisk de siempre, build/rootfs) en "/" (kernel/fs/raiz.c, montagem.c).

A. Anfitrión: el initramfs pesa menos de 1 MB; la imagen lleva SAGUI-RAIZ (e2fsck limpio) con
   los permisos del ramdisk (/tmp 1777, /home de sagui, /etc/sombra 0600).
B. Arranque (IDE): la raíz se monta desde el disco; /bin es el del disco (no el del initramfs);
   un programa que solo está en el disco funciona; /apps y /sistema/conf se montan dentro de
   la raíz; "umount /" se niega; lo escrito en /home sigue tras reiniciar (y e2fsck limpio).
C. Rescate: si la partición raíz no está, el sistema sigue con el initramfs y lo dice.
D. Sin /etc/raiz.conf (la imagen normal) nada cambia: no hay línea [RAIZ].
E. Instalar (R2): desde la raíz en disco, «instalar hdb» crea SAGUI-RAIZ (con /apps, sin SAGUI-APPS ni
   SAGUI-CONF) copiada por ficheros, SAGUI-DATOS en /home y la cabecera con CAB_RAIZ_OPC = disco;
   arrancado SOLO de hdb, la raíz es la de hdb, /home persiste y e2fsck queda limpio.
F. Live (L1) desde un pendrive (EHCI): la raíz del medio de solo lectura con la capa en la RAM: se
   escribe, se borra, se cambia y se renombra lo de serie; al reiniciar todo vuelve a ser de serie y
   la partición SAGUI-RAIZ es byte a byte la misma (nunca se escribe).
G. Live con persistencia (L2): la capa en SAGUI-PERSIST: los cambios siguen tras reiniciar, la raíz
   sigue intacta y los dos ext2 quedan limpios (e2fsck).
H. Toram (T1) desde el CD: la ISO lleva la raíz dentro de la imagen El Torito; el kernel la copia a
   la RAM (ram0) y la monta en «/»; se saca el CD y el sistema sigue (programas nuevos, escribir).
D (M1). La imagen de serie (build/saguios.img) arranca así, en live, con SAGUI-CONF y sin SAGUI-APPS; una
   imagen con el ramdisk de siempre (clásica) no intenta montar otra raíz.
I. Reinstalar sobre SAGUI-RAIZ (M1, requisito de la revisión): se instala, se pone contraseña a root, se crean
   claves de sshd (con permisos mal puestos a propósito), algo en /root, se cambia /etc/crontab y se escribe en
   /home; se reinstala encima desde una live cuyo /etc/sagshrc es distinto (otra «versión» de serie): root sigue
   entrando con su contraseña (su), las claves son las mismas y quedan 0600, /root 0700 con lo suyo, el crontab
   cambiado se conserva, el sagshrc de serie se actualiza y /home (SAGUI-DATOS) sigue intacta y sin formatear.
J. Raíz que no vale (M1): de otra versión (marca distinta) o sin /bin/sagsh: modo rescate con el motivo, nunca
   un sistema a medias.
K. Instalaciones de antes (P6 A): un disco instalado al estilo 8.5.3 (ramdisk de siempre, SAGUI-APPS, SAGUI-CONF)
   arranca igual con este núcleo; reinstalar encima con la imagen nueva conserva /home y funde SAGUI-CONF en la
   raíz (contraseña de root, /root).
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, e2fsck, debugfs, particoes_img, extrair_particao, opcoes_apps,  # noqa: E402
                  imagem_de_teste, BUILD, RAIZ, SAIDA)

ROOTFS = os.path.join(BUILD, "rootfs")
MAX_INITRAMFS = 1024 * 1024


def gerar_imagem(nome, raiz_conf="LABEL=SAGUI-RAIZ", com_raiz=True, modo="rw", capa="ram", extras_img=(), rootfs=ROOTFS):
    os.makedirs(SAIDA, exist_ok=True)
    initrd = os.path.join(SAIDA, f"{nome}-initramfs.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkinitramfs.py"), ROOTFS, initrd, "--raiz", raiz_conf,
                    "--espera", "5", "--modo", modo, "--capa", capa], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, f"{nome}.img")
    b = BUILD
    cmd = ["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
           "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
           "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
           "--ramdisk", initrd, "--saida", img, "--vesa", "--console", "--sesion-root", *opcoes_apps()]
    if com_raiz:
        cmd += ["--raiz", rootfs]
    if modo == "rw":   # D379 (M1): «instalado»: la raíz con escritura absorbe SAGUI-CONF (sin esto, la imagen la lleva)
        cmd += ["--modo-raiz", "disco"]
    cmd += list(extras_img)
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return img, initrd


def particao_raiz(img):
    """Extrae la partición con la etiqueta SAGUI-RAIZ (la etiqueta ext2 está en el byte 120 del superbloque)."""
    for _num, tipo, inicio, qtd in particoes_img(img):
        if tipo != 0x83:
            continue
        with open(img, "rb") as f:
            f.seek(inicio * 512 + 1024 + 120)
            rotulo = f.read(16).split(b"\0")[0].decode("latin-1", "replace")
        if rotulo == "SAGUI-RAIZ":
            return extrair_particao(img, inicio, qtd // 2048, os.path.join(SAIDA, "h387-raiz.ext2"))
    return None


def parte_a():
    img, initrd = gerar_imagem("h387-raiz")
    tam = os.path.getsize(initrd)
    rd = os.path.getsize(os.path.join(BUILD, "ramdisk.sgfs"))
    print(f"H387: initramfs {tam} bytes (el ramdisk de siempre: {rd} bytes)")
    verificar(tam < MAX_INITRAMFS, f"A: el initramfs pesa {tam // 1024} KB (< {MAX_INITRAMFS // 1024} KB)")
    p = particao_raiz(img)
    verificar(p is not None, "A: la imagen lleva la partición SAGUI-RAIZ")
    cod, txt = e2fsck(p)
    verificar(cod == 0, f"A: e2fsck -fn de SAGUI-RAIZ limpio ({txt.strip().splitlines()[-1] if txt.strip() else ''})")
    st = debugfs(p, "stat /tmp").decode("latin-1")
    verificar(re.search(r"Mode:\s+01777", st), "A: /tmp de la raíz con 1777")
    st = debugfs(p, "stat /home").decode("latin-1")
    verificar(re.search(r"User:\s+1000", st), "A: /home de la raíz es de sagui")
    if os.path.exists(os.path.join(ROOTFS, "etc", "sombra")):
        st = debugfs(p, "stat /etc/sombra").decode("latin-1")
        verificar(re.search(r"Mode:\s+0600", st), "A: /etc/sombra con 0600")
    ls = debugfs(p, "ls -l /bin").decode("latin-1")
    verificar(ls.count("\n") > 100, "A: /bin de la raíz con todos los programas")
    marca = debugfs(p, "cat /sistema/raiz/versao").decode("latin-1")
    verificar(re.match(r"v\d", marca.strip()), f"A: la raíz lleva la marca de versión ({marca.strip()})")
    somas = debugfs(p, "cat /sistema/raiz/etc.sha256").decode("latin-1")
    verificar(re.search(r"^[0-9a-f]{64}  etc/fstab$", somas, re.M), "A: y las sumas del /etc de serie")
    return img


def parte_b(img):
    copia = os.path.join(SAIDA, "h387-raiz-rw.img")
    subprocess.run(["cp", "--sparse=always", img, copia], check=True)
    with Maquina("h387-b1", "ide", imagem=copia, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] hd\w+ \(LABEL=SAGUI-RAIZ, ext2\) montada en /", s), "B: el kernel monta SAGUI-RAIZ en /")
        tiempos = re.findall(r"\[ARRANQUE\] [^\n]+", s)
        print("\n".join(tiempos))
        out = m.comando("cat /etc/raiz.conf")
        verificar("raiz=LABEL" not in out, "B: /etc es el del disco (el raiz.conf del initramfs no se ve)")
        print(m.comando("mem"))
        out = m.comando("ls /bin")
        verificar(len(re.findall(r"\d+ bytes", out)) > 100, "B: /bin es el del disco (más de 100 programas)")
        out = m.comando("uname -a")
        verificar(re.search(r"SaguiOS|Sagui", out), "B: uname (solo en el disco) funciona")
        out = m.comando("mount")
        print(out)
        verificar(re.search(r"\s/\s", out) or re.search(r" on / ", out) or re.search(r"\s/\s*\(", out),
                  "B: «mount» enseña la raíz")
        verificar(not re.search(r"en /apps ", out) and not re.search(r"en /sistema/conf ", out),
                  "B: sin particiones SAGUI-APPS ni SAGUI-CONF (P2 A: la raíz las absorbe)")
        verificar(re.search(r"tmpfs\s+en /tmp\s+tipo ramfs", out), "B: /tmp en la RAM (ramfs)")
        verificar(re.search(r"initrd\s+en /initrd\s+tipo ramfs\s+\(ro", out), "B: el initramfs en /initrd, solo lectura")
        out = m.comando("ls /apps/bin")
        verificar(len(re.findall(r"\d+ bytes", out)) > 5, "B: /apps/bin (dentro de la raíz) se lista")
        out = m.comando("ls /initrd/etc")
        verificar("raiz.conf" in out, "B: /initrd/etc/raiz.conf es el del initramfs")
        out = m.comando("echo x > /initrd/prueba")
        verificar("prueba" not in m.comando("ls /initrd"), "B: /initrd no se puede escribir")
        m.comando("echo temporal > /tmp/h387.tmp")
        verificar("temporal" in m.comando("cat /tmp/h387.tmp"), "B: /tmp (RAM) se escribe")
        s = m.serial()
        verificar("[CONF] /root en la raiz del disco" in s, "B: /root vive en la raíz del disco (SAGUI-CONF fundida)")
        out = m.comando("umount /")
        s = m.serial()
        verificar("[RAIZ]" in s and not re.search(r"desmontado de / ", s), "B: «umount /» no suelta la raíz")
        m.comando("echo persistente-h387 > /home/h387.txt")
        m.comando("sync")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
        verificar(m.proc.poll() is not None, "B: reiniciar")
    with Maquina("h387-b2", "ide", imagem=copia, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        out = m.comando("cat /home/h387.txt")
        verificar("persistente-h387" in out, "B: lo escrito en /home (en la raíz del disco) sigue tras reiniciar")
        out = m.comando("cat /tmp/h387.tmp")
        verificar("temporal" not in out.split("cat /tmp/h387.tmp", 1)[-1], "B: /tmp (RAM) se vacía al reiniciar")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
    p = particao_raiz(copia)
    cod, txt = e2fsck(p)
    verificar(cod == 0, f"B: e2fsck -fn de la raíz limpio tras usarla y reiniciar ({txt.strip().splitlines()[-1]})")
    os.remove(copia)


def parte_c():
    img, _ = gerar_imagem("h387-rescate", raiz_conf="LABEL=SAGUI-NOEXISTE", com_raiz=False)
    with Maquina("h387-c", "ide", imagem=img, memoria="128M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar("[RAIZ] LABEL=SAGUI-NOEXISTE no encontrado" in s, "C: el kernel no encuentra la raíz y lo anota")
        out = m.comando("ls /bin")
        n = len(re.findall(r"bytes\s+programa", out))
        verificar(5 < n < 40, f"C: sigue con el initramfs (/bin con {n} entradas)")
        out = m.comando("cat /etc/rescate.txt")
        verificar("initramfs" in out, "C: el aviso de rescate del initramfs está")


def parte_e(img):
    dest = os.path.join(SAIDA, "h387-dest.img")
    with open(dest, "wb") as f:
        f.truncate(512 * 1024 * 1024)
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h387-e1", "ide", imagem=img, extras=extras, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        out = m.comando("instalar hdb -s", 1800)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"E: instalar hdb termina y verifica ({out.strip()[-400:]})")
        verificar(re.search(r"raíz copiada por ficheros: (\d+) archivos", m.serial()), "E: la raíz se copia por ficheros")
    r = subprocess.run(["sfdisk", "-d", dest], capture_output=True, text=True).stdout
    print(r)
    with open(dest, "rb") as f:
        f.seek(512 + 163)
        verificar(f.read(1) == b"\x02", "E: la cabecera del disco instalado lleva CAB_RAIZ_OPC = disco")
    p = particao_raiz(dest)
    verificar(p is not None, "E: el disco instalado tiene SAGUI-RAIZ")
    rotulos = []
    for _n, tipo, ini, _q in particoes_img(dest):
        with open(dest, "rb") as f:
            f.seek(ini * 512 + 1024 + 120)
            rotulos.append(f.read(16).split(b"\0")[0].decode("latin-1", "replace"))
    rotulos = " ".join(rotulos)
    print("rótulos:", rotulos.split())
    verificar("SAGUI-DATOS" in rotulos and "SAGUI-APPS" not in rotulos and "SAGUI-CONF" not in rotulos,
              "E: SAGUI-DATOS sí; SAGUI-APPS y SAGUI-CONF no (P2 A)")
    ls = debugfs(p, "ls /apps/bin").decode("latin-1")
    verificar(len(ls.split()) > 5, "E: /apps dentro de SAGUI-RAIZ")
    with Maquina("h387-e2", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", s), "E: arranca del disco instalado con su raíz")
        out = m.comando("mount")
        verificar(re.search(r"en /home\s+tipo ext2", out), "E: /home en SAGUI-DATOS")
        verificar(len(re.findall(r"\d+ bytes", m.comando("ls /bin"))) > 100, "E: /bin del disco instalado")
        m.comando("echo instalado-h387 > /home/e.txt")
        m.comando("sync")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
    with Maquina("h387-e3", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar("instalado-h387" in m.comando("cat /home/e.txt"), "E: /home persiste en el instalado")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.5)
    p = particao_raiz(dest)
    cod, txt = e2fsck(p)
    verificar(cod == 0, f"E: e2fsck de la raíz instalada limpio ({txt.strip().splitlines()[-1]})")
    os.remove(dest)


def soma_raiz(img):
    p = particao_raiz(img)
    return hashlib.sha256(open(p, "rb").read()).hexdigest(), p


def reiniciar(m):
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 60
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, "reiniciar")


def tem(m, caminho):
    """¿Existe la ruta? (por «ls» de la carpeta, que en SaguiOS no falla con un error distinto)"""
    pasta, nome = caminho.rsplit("/", 1)
    out = m.comando(f"ls {pasta or '/'}")
    return re.search(rf"^{re.escape(nome)}(/|\s)", out, re.M) is not None


def parte_f():
    img, _ = gerar_imagem("h387-live", modo="live")
    antes, _ = soma_raiz(img)
    with Maquina("h387-f1", "usb", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] usb\d+p\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", s), "F: la raíz de la live se monta desde el pendrive")
        out = m.comando("mount")
        verificar(re.search(r"en /\s+tipo live\s+\(rw", out), "F: «/» es la sobreposición (rw)")
        verificar("tipo ext2" not in out.split("en /  ")[0], "F: el ext2 del medio no se ve montado aparte")
        m.comando("echo nuevo-h387 > /etc/h387.txt")
        verificar("nuevo-h387" in m.comando("cat /etc/h387.txt"), "F: se crea un archivo en /etc (capa)")
        m.comando("echo extra-h387 >> /etc/crontab")
        verificar("extra-h387" in m.comando("cat /etc/crontab"), "F: se cambia un archivo de serie (copy-up)")
        verificar(tem(m, "/bin/yes"), "F: /bin/yes está (de serie)")
        m.comando("rm /bin/yes")
        verificar(not tem(m, "/bin/yes"), "F: borrar un programa de serie (marca de borrado)")
        verificar(len(re.findall(r"\d+ bytes", m.comando("ls /bin"))) > 100, "F: el resto de /bin sigue")
        m.comando("mkdir /etc/carpeta387")
        verificar(tem(m, "/etc/carpeta387"), "F: crear carpeta")
        m.comando("rmdir /etc/carpeta387")
        verificar(not tem(m, "/etc/carpeta387"), "F: borrar carpeta")
        m.comando("mv /etc/usuarios /etc/usuarios387")
        verificar(tem(m, "/etc/usuarios387") and not tem(m, "/etc/usuarios"), "F: renombrar un archivo de serie")
        m.comando("mv /etc/usuarios387 /etc/usuarios")
        verificar(tem(m, "/etc/usuarios"), "F: y devolverlo")
        m.comando("sync")
        reiniciar(m)
    with Maquina("h387-f2", "usb", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar(not tem(m, "/etc/h387.txt"), "F: al reiniciar no queda lo creado (capa en la RAM)")
        verificar(tem(m, "/bin/yes"), "F: al reiniciar vuelve lo borrado")
        verificar("extra-h387" not in m.comando("cat /etc/crontab"), "F: al reiniciar, /etc/crontab de serie")
        reiniciar(m)
    depois, p = soma_raiz(img)
    verificar(antes == depois, "F: SAGUI-RAIZ no se ha escrito nunca (misma suma)")
    cod, txt = e2fsck(p)
    verificar(cod == 0, "F: e2fsck de SAGUI-RAIZ limpio")
    os.remove(img)


def parte_g():
    img, _ = gerar_imagem("h387-persist", modo="live", capa="LABEL=SAGUI-PERSIST", extras_img=["--persist-mb", "32"])
    antes, _ = soma_raiz(img)
    with Maquina("h387-g1", "usb", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar("[SOBRE] capa persistente en" in m.serial(), "G: la capa va en SAGUI-PERSIST")
        m.comando("echo persiste-h387 > /etc/h387p.txt")
        m.comando("rm /bin/yes")
        m.comando("echo extra-h387 >> /etc/crontab")
        m.comando("sync")
        reiniciar(m)
    with Maquina("h387-g2", "usb", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar("persiste-h387" in m.comando("cat /etc/h387p.txt"), "G: lo creado sigue tras reiniciar")
        verificar(not tem(m, "/bin/yes"), "G: lo borrado sigue borrado")
        verificar("extra-h387" in m.comando("cat /etc/crontab"), "G: el cambio a un archivo de serie sigue")
        reiniciar(m)
    depois, p = soma_raiz(img)
    verificar(antes == depois, "G: SAGUI-RAIZ intacta")
    verificar(e2fsck(p)[0] == 0, "G: e2fsck de SAGUI-RAIZ limpio")
    for _n, tipo, ini, qtd in particoes_img(img):
        with open(img, "rb") as f:
            f.seek(ini * 512 + 1024 + 120)
            if f.read(16).split(b"\0")[0] == b"SAGUI-PERSIST":
                pp = extrair_particao(img, ini, qtd // 2048, os.path.join(SAIDA, "h387-persist.ext2"))
                cod, txt = e2fsck(pp)
                verificar(cod == 0, f"G: e2fsck de SAGUI-PERSIST limpio ({txt.strip().splitlines()[-1]})")
                verificar(b".wh.yes" in debugfs(pp, "ls /bin"), "G: la marca de borrado /bin/.wh.yes está en la capa")
    os.remove(img)


def parte_h():
    img, _ = gerar_imagem("h387-cd", modo="toram", extras_img=["--cd"])
    iso = os.path.join(SAIDA, "h387-cd.iso")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img, "--saida", iso],
                   check=True, stdout=subprocess.DEVNULL)
    with Maquina("h387-h", "cd", imagem=iso, memoria="512M") as m:
        m.esperar_serial("sagui:", 240)
        s = m.serial()
        verificar(re.search(r"\[RAM0\] \S+ copiado a la RAM: \d+ MB", s), "H: la raíz del CD se copia a la RAM")
        verificar(re.search(r"\[RAIZ\] \S+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[toram\]", s), "H: la raíz es la copia en RAM")
        out = m.comando("mount")
        verificar(re.search(r"ram0\s+en /\s+tipo ext2\s+\(rw", out), "H: ram0 en «/» con escritura")
        print(m.monitor("eject -f ide1-cd0"))
        time.sleep(3)
        out = m.comando("uname -a")
        verificar(re.search(r"Sagui", out), "H: sin el CD, un programa que no se había abierto funciona")
        m.comando("echo en-ram > /etc/h387cd.txt")
        verificar("en-ram" in m.comando("cat /etc/h387cd.txt"), "H: se escribe en la raíz (RAM)")
        verificar(len(re.findall(r"\d+ bytes", m.comando("ls /apps/bin"))) > 5, "H: /apps, dentro de la raíz, sin el CD")
    os.remove(iso)


def rotulos_de(img):
    """{etiqueta: (inicio, sectores)} de las particiones ext2/FAT32 de una imagen."""
    r = {}
    for _n, tipo, ini, qtd in particoes_img(img):
        if tipo not in (0x83, 0x0B, 0x0C):
            continue
        with open(img, "rb") as f:
            f.seek(ini * 512 + 1024 + 120)
            rot = f.read(16).split(b"\0")[0].decode("latin-1", "replace")
            if tipo in (0x0B, 0x0C):
                f.seek(ini * 512 + 71)
                rot = f.read(11).decode("latin-1", "replace").strip()
        r[rot] = (ini, qtd)
    return r


def apagar_maquina(m):
    m.comando("sync")
    reiniciar(m)


PW_I = "raiz1357"


def su_root(m, pw):
    """¿Entra sagui como root con «su» y esa contraseña? (y no con otra)"""
    m.comando(f"echo {pw}-mal > /tmp/q; echo {pw} > /tmp/p; chmod 644 /tmp/p /tmp/q")
    marca = len(m.serial())
    m.comando("su sagui -c 'su root -c id < /tmp/q'", 60)
    m.comando("echo fin-mal-h387")
    fim = time.time() + 30
    while "fin-mal-h387\n" not in m.serial()[marca:].replace("\r", "") and time.time() < fim:
        time.sleep(0.5)
    mal = "uid=0(root)" in m.serial()[marca:]
    marca = len(m.serial())
    m.comando("su sagui -c 'su root -c id < /tmp/p'", 60)
    fim = time.time() + 30
    while "uid=0(root)" not in m.serial()[marca:] and time.time() < fim:
        time.sleep(0.5)
    return not mal and "uid=0(root)" in m.serial()[marca:]


def instalar_em(img, dest, nome, args="-s", memoria="256M"):
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina(nome, "ide", imagem=img, extras=extras, memoria=memoria) as m:
        m.esperar_serial("sagui:", 180)
        out = m.comando(f"instalar hdb {args}", 1800)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"{nome}: instalar hdb {args} termina y verifica ({out.strip()[-500:]})")
        return out, m.serial()


def rootfs_modificado(nome, cambio):
    """Una copia de build/rootfs con un cambio (otra «versión» del /etc de serie, o sin un archivo)."""
    d = os.path.join(SAIDA, nome)
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(ROOTFS, d, symlinks=True)
    cambio(d)
    return d


def parte_i():
    img, _ = gerar_imagem("h387-i", modo="live")
    dest = os.path.join(SAIDA, "h387-i-dest.img")
    with open(dest, "wb") as f:
        f.truncate(512 * 1024 * 1024)
    instalar_em(img, dest, "h387-i1", f"--clave-root {PW_I} -s")
    rot1 = rotulos_de(dest)
    verificar("SAGUI-RAIZ" in rot1 and "SAGUI-DATOS" in rot1, f"I: primera instalación con SAGUI-RAIZ y SAGUI-DATOS ({sorted(rot1)})")
    with Maquina("h387-i2", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", m.serial()), "I: arranca del disco instalado")
        m.comando("mkdir -p /etc/ssh; ssh-keygen -q -N \"\" -f /etc/ssh/ssh_host_ed25519_key", 120)
        m.comando("chmod 644 /etc/ssh/ssh_host_ed25519_key; chmod 755 /root")
        pub1 = m.comando("cat /etc/ssh/ssh_host_ed25519_key.pub")
        verificar("ssh-ed25519 " in pub1, f"I: clave de sshd creada ({pub1.strip()[-200:]})")
        pub1 = re.search(r"ssh-ed25519 \S+", pub1).group(0)
        m.comando("echo de-root-h387 > /root/marca-i.txt")
        m.comando("echo '# h387-usuario' >> /etc/crontab")
        m.comando("echo datos-h387-i > /home/datos-i.txt")
        sombra1 = re.search(r"^root:\S+", m.comando("grep root /etc/sombra"), re.M)
        verificar(sombra1, "I: root tiene contraseña en /etc/sombra")
        sombra1 = sombra1.group(0)
        apagar_maquina(m)
    # la «versión nueva»: su /etc/sagshrc de serie cambia
    nuevo = rootfs_modificado("h387-i-rootfs", lambda d: open(os.path.join(d, "etc", "sagshrc"), "a").write("# h387-nueva\n"))
    img2, _ = gerar_imagem("h387-i-nueva", modo="live", rootfs=nuevo)
    out, serie = instalar_em(img2, dest, "h387-i3")
    verificar(re.search(r"Se conserva /home \(SAGUI-DATOS hdb\d+", out), "I: el instalador dice que conserva /home")
    verificar(re.search(r"Se conserva la configuración de la instalación anterior \(SAGUI-RAIZ", out),
              "I: el instalador dice que conserva la configuración de la SAGUI-RAIZ anterior")
    verificar(re.search(r"/home \(hdb\d+\) se conserva sin formatear", serie), "I: /home no se formatea")
    verificar(re.search(r"SAGUI-RAIZ de hdb\d+: [1-9]\d* archivo\(s\) de /etc cambiados", serie), "I: se guarda lo cambiado de /etc")
    rot2 = rotulos_de(dest)
    verificar(rot2.get("SAGUI-DATOS") == rot1["SAGUI-DATOS"], f"I: SAGUI-DATOS sigue en su sitio ({rot1['SAGUI-DATOS']} -> {rot2.get('SAGUI-DATOS')})")
    with Maquina("h387-i4", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar("datos-h387-i" in m.comando("cat /home/datos-i.txt"), "I: /home intacta tras reinstalar")
        verificar("de-root-h387" in m.comando("cat /root/marca-i.txt"), "I: /root conserva lo suyo")
        verificar(re.search(r"^drwx------", m.comando("ls -ld /root"), re.M), "I: /root con 0700")
        sombra2 = re.search(r"^root:\S+", m.comando("grep root /etc/sombra"), re.M)
        verificar(sombra2 and sombra2.group(0) == sombra1, "I: la contraseña de root es la misma")
        verificar(re.search(r"^-rw-------", m.comando("ls -l /etc/sombra"), re.M), "I: /etc/sombra con 0600")
        pub2 = m.comando("cat /etc/ssh/ssh_host_ed25519_key.pub")
        verificar(pub1 in pub2, "I: la clave de sshd es la misma")
        verificar(re.search(r"^-rw------- .*ssh_host_ed25519_key\s*$", m.comando("ls -l /etc/ssh"), re.M),
                  "I: la clave privada de sshd queda 0600 (estaba 0644)")
        verificar("h387-usuario" in m.comando("cat /etc/crontab"), "I: /etc/crontab cambiado por la persona se conserva")
        verificar("h387-nueva" in m.comando("cat /etc/sagshrc"), "I: /etc/sagshrc de serie se actualiza al de la versión nueva")
        verificar(su_root(m, PW_I), "I: root entra con su contraseña de antes (su desde sagui)")
        apagar_maquina(m)
    p = particao_raiz(dest)
    verificar(e2fsck(p)[0] == 0, "I: e2fsck de la raíz reinstalada limpio")
    os.remove(dest)
    shutil.rmtree(nuevo, ignore_errors=True)


def rescate_por(nome, img, motivo):
    with Maquina(nome, "ide", imagem=img, memoria="128M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] \S+ \(LABEL=SAGUI-RAIZ\) no vale como raiz: " + motivo, s), f"J: {nome}: el kernel la rechaza ({motivo})")
        verificar(re.search(r"La partici.n ra.z no vale", s), f"J: {nome}: lo dice en pantalla")
        verificar("montada en /" not in s, f"J: {nome}: no se monta en /")
        n = len(re.findall(r"bytes\s+programa", m.comando("ls /bin")))
        verificar(5 < n < 40, f"J: {nome}: sigue con el initramfs (rescate, /bin con {n} entradas)")


def parte_j():
    img, _ = gerar_imagem("h387-j-version", extras_img=["--raiz-versao", "v0.0-h387"])
    rescate_por("h387-j1", img, r"es de la versi.n v0\.0-h387 y este n.cleo es v")
    os.remove(img)
    sin = rootfs_modificado("h387-j-rootfs", lambda d: os.remove(os.path.join(d, "bin", "sagsh")))
    img, _ = gerar_imagem("h387-j-sinsh", rootfs=sin)
    rescate_por("h387-j2", img, r"le falta /bin/sagsh")
    os.remove(img)
    shutil.rmtree(sin, ignore_errors=True)


PW_K = "clasica2468"


def parte_k():
    clasica = imagem_de_teste("", ["--vesa", "--console", *opcoes_apps()], clasica=True)
    dest = os.path.join(SAIDA, "h387-k-dest.img")
    with open(dest, "wb") as f:
        f.truncate(512 * 1024 * 1024)
    instalar_em(clasica, dest, "h387-k1", f"--clave-root {PW_K} -s")
    rot1 = rotulos_de(dest)
    verificar("SAGUI-CONF" in rot1 and "SAGUI-APPS" in rot1 and "SAGUI-RAIZ" not in rot1,
              f"K: instalado al estilo 8.5.3 (SAGUI-APPS y SAGUI-CONF, sin SAGUI-RAIZ: {sorted(rot1)})")
    with Maquina("h387-k2", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar("[RAIZ]" not in s, "K: el instalado de antes arranca como siempre (sin raíz en disco)")
        verificar(re.search(r"/apps\s+tipo ext2", m.comando("mount")), "K: con SAGUI-APPS en /apps")
        verificar(len(re.findall(r"\d+ bytes", m.comando("ls /bin"))) > 100, "K: /bin del ramdisk de siempre")
        m.comando("echo datos-h387-k > /home/datos-k.txt")
        m.comando("echo de-root-k > /root/marca-k.txt")
        sombra1 = re.search(r"^root:\S+", m.comando("grep root /etc/sombra"), re.M)
        verificar(sombra1, "K: root tiene contraseña")
        sombra1 = sombra1.group(0)
        apagar_maquina(m)
    img, _ = gerar_imagem("h387-k-nueva", modo="live")
    out, serie = instalar_em(img, dest, "h387-k3")
    verificar(re.search(r"Se conserva /home \(SAGUI-DATOS hdb\d+", out), "K: reinstalar conserva /home")
    verificar(re.search(r"Se conserva el contenido de SAGUI-CONF", out), "K: y el contenido de SAGUI-CONF")
    rot2 = rotulos_de(dest)
    verificar("SAGUI-RAIZ" in rot2 and "SAGUI-CONF" not in rot2 and "SAGUI-APPS" not in rot2
              and rot2.get("SAGUI-DATOS") == rot1["SAGUI-DATOS"], f"K: ahora SAGUI-RAIZ, la misma SAGUI-DATOS ({sorted(rot2)})")
    with Maquina("h387-k4", "ide", imagem=dest, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", m.serial()), "K: arranca con la raíz en disco")
        verificar("datos-h387-k" in m.comando("cat /home/datos-k.txt"), "K: /home intacta")
        verificar("de-root-k" in m.comando("cat /root/marca-k.txt"), "K: /root de SAGUI-CONF fundido en la raíz")
        sombra2 = re.search(r"^root:\S+", m.comando("grep root /etc/sombra"), re.M)
        verificar(sombra2 and sombra2.group(0) == sombra1, "K: la contraseña de root es la de antes")
        verificar(re.search(r"^-rw-------", m.comando("ls -l /etc/sombra"), re.M), "K: /etc/sombra con 0600")
        verificar(su_root(m, PW_K), "K: root entra con su contraseña (su desde sagui)")
        apagar_maquina(m)
    os.remove(dest)


def parte_d():
    with Maquina("h387-d", "ide", memoria="256M") as m:
        m.esperar_serial("[ARRANQUE] escritorio", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", s),
                  "D: la imagen de serie arranca en live desde SAGUI-RAIZ (M1)")
        verificar(re.search(r"\[CONF\] SAGUI-CONF en /sistema/conf", s), "D: con SAGUI-CONF (la configuración, como siempre)")
        verificar("montado em /apps" not in s and "falta /apps" not in s, "D: sin SAGUI-APPS y sin el aviso de /apps (va dentro de la raíz)")
    img = imagem_de_teste("", ["--vesa"], clasica=True)
    with Maquina("h387-d2", "ide", imagem=img, memoria="256M") as m:
        m.esperar_serial("[ARRANQUE] escritorio", 180)
        verificar("[RAIZ]" not in m.serial(), "D: una imagen con el ramdisk de siempre no intenta montar otra raíz")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k"]
    img = None
    if "a" in partes or "b" in partes or "e" in partes:
        img = parte_a()
    if "b" in partes:
        parte_b(img)
    if "c" in partes:
        parte_c()
    if "d" in partes:
        parte_d()
    if "e" in partes:
        parte_e(img)
    if "f" in partes:
        parte_f()
    if "g" in partes:
        parte_g()
    if "h" in partes:
        parte_h()
    if "i" in partes:
        parte_i()
    if "j" in partes:
        parte_j()
    if "k" in partes:
        parte_k()
    print("H387: OK")
