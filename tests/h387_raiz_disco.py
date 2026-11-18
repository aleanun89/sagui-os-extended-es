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
J (v8.5.4.2). La marca de versión se compara con la línea entera: «<versión> beta» no vale para el núcleo
   <versión> (antes se cortaba en el primer espacio y valía); con espacios y \r al final, sí.
L (v8.5.4.2). Dos discos SaguiOS: el instalado lleva su identificador en la cabecera y en /sistema/raiz/id; con
   una copia del disco cuya raíz tiene otro id delante (hda), el núcleo la rechaza y monta la suya (hdb); con
   un pendrive live conectado, arranca del instalado sin tocar el pendrive y, arrancando del pendrive, la live
   no toca el instalado; con dos copias idénticas del instalado, ninguna se monta con escritura (live y aviso).
M (v8.5.4.2). La tecla M al arrancar abre el menú del modo (live, todo a la RAM, instalado, rescate); kconfig
   raiz_modo lo guarda y --por-defecto lo quita. N (v8.5.4.2). La capa de la live en la RAM tiene tope
   (kconfig capa_ram_pct): al llenarse, «no queda espacio» con aviso, y el sistema sigue.
O (v8.5.4.2). fsck básico: un ext2 con los mapas mal y sin el bit de limpio se corrige al montarlo con escritura
   (e2fsck limpio después, los datos intactos); la raíz instalada tras un corte de luz, igual.
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
from qemu import (Maquina, verificar, e2fsck, debugfs, particoes_img, extrair_particao, opcoes_apps, DEBUGFS, criar_disco,  # noqa: E402
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


def versao_nucleo():
    with open(os.path.join(RAIZ, "kernel", "inc", "versao.h"), encoding="utf-8") as f:
        return re.search(r'^#define\s+SAGUI_VERSAO\s+"([^"]+)"', f.read(), re.M).group(1)


def mkimagem_sem_debugfs():
    """v8.5.4.2: sin debugfs, mkimagem.py no genera una SAGUI-RAIZ (dueños root obligatorios, D378 §14)."""
    import importlib.util
    import tempfile
    spec = importlib.util.spec_from_file_location("mkimagem_h387", os.path.join(RAIZ, "tools", "mkimagem.py"))
    mk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mk)
    mk.achar_ferramenta = lambda nome: None
    with tempfile.TemporaryDirectory() as d:
        open(os.path.join(d, "x"), "w").close()
        try:
            mk.normalizar_donos(os.path.join(d, "no-existe.img"), 0, d, 0, 0, "/")
            falla = False
        except SystemExit as e:
            falla = "debugfs" in str(e)
        verificar(falla, "A: sin debugfs, mkimagem.py se niega a generar SAGUI-RAIZ")
        mk.normalizar_donos(os.path.join(d, "no-existe.img"), 0, d, 1000, 1000, "/home")   # otra: solo avisa


def parte_a():
    mkimagem_sem_debugfs()
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
        verificar("saguios-completo.img" in s, "C: el aviso recomienda saguios-completo.img (#36)")
        verificar(re.search(r"Discos que ve SaguiOS:\s*\n\s+hda\s", s), "C: y lista los discos que ve")


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
        # v8.5.4.2: casos raros de renombrar, como el ext2 (a una carpeta, dentro; sin fundir ni tapar)
        m.comando("mkdir /etc/c9; echo c9-h387 > /etc/c9/f9; mv /etc/c9 /etc/ssh")
        verificar("c9-h387" in m.comando("cat /etc/ssh/c9/f9") and tem(m, "/etc/ssh/sshd.conf") and not tem(m, "/etc/c9"),
                  "F: una carpeta de arriba a una carpeta de serie va dentro (no se funde)")
        m.comando("rm /etc/bloqueo/ejemplo.txt; rmdir /etc/bloqueo; mkdir /etc/d9; echo d9-h387 > /etc/d9/g9; mv /etc/d9 /etc/bloqueo")
        out = m.comando("ls /etc/bloqueo")
        verificar("g9" in out and "ejemplo.txt" not in out,
                  f"F: una carpeta llevada al nombre de una de serie borrada queda opaca ({out.strip()[-200:]})")
        m.comando("echo h9-h387 > /etc/h9; mv /etc/h9 /etc/vnc")
        verificar("h9-h387" in m.comando("cat /etc/vnc/h9") and tem(m, "/etc/vnc/vncd.conf"),
                  "F: un archivo sobre una carpeta de serie va dentro (no la tapa)")
        m.comando("mkdir /etc/e9; mv /etc/e9 /etc/crontab")
        verificar(tem(m, "/etc/e9") and "extra-h387" in m.comando("cat /etc/crontab"),
                  "F: una carpeta encima de un archivo: error, los dos siguen")
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
        t = re.search(r"\[RAM0\] \S+ copiado a la RAM: (\d+) MB en (\d+) ms", s)
        print(f"H387: toram desde la raíz comprimida: {t.group(1)} MB en {t.group(2)} ms" if t else "H387: (sin tiempo de copia)")
    verificar(re.search(r"\[RZ\] \S+: raiz comprimida SAGUI-RAIZ", s), "H: (P5) la raíz del CD va comprimida (rz0)")
    # P5: la misma ISO sin comprimir, para comparar el tamaño
    img2, _ = gerar_imagem("h387-cd-cru", modo="toram", extras_img=["--cd", "--raiz-sin-comprimir"])
    iso2 = os.path.join(SAIDA, "h387-cd-cru.iso")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img2, "--saida", iso2],
                   check=True, stdout=subprocess.DEVNULL)
    t1, t2 = os.path.getsize(iso), os.path.getsize(iso2)
    print(f"H387: ISO con la raíz comprimida {t1 / 1048576:.1f} MB; sin comprimir {t2 / 1048576:.1f} MB")
    verificar(t1 < t2 * 0.7, f"H: (P5) la ISO comprimida es bastante más pequeña ({t1 >> 20} MB frente a {t2 >> 20} MB)")
    os.remove(iso2)
    os.remove(img2)
    # P5: con poca RAM, live desde el CD, descomprimiendo lo que se lee
    with Maquina("h387-h2", "cd", imagem=iso, memoria="96M") as m:
        m.esperar_serial("sagui:", 240)
        s = m.serial()
        verificar("no cabe en la RAM" in s, "H: con 96 MB no cabe en la RAM y lo dice")
        verificar(re.search(r"\[RAIZ\] rz0 \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", s), "H: y arranca en live desde el CD comprimido")
        verificar(re.search(r"Sagui", m.comando("uname -a")), "H: un programa se lee del CD comprimido")
        m.comando("echo en-capa > /etc/h387cd2.txt")
        verificar("en-capa" in m.comando("cat /etc/h387cd2.txt"), "H: y se escribe en la capa")
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


def donos_ld(p, que):
    """D378: /lib, /apps/lib, el intérprete y las .so de la raíz instalada: de root, sin escritura de otros."""
    rutas = ["/lib", "/lib/ld-sagui.so.1", "/apps/lib"]
    for pasta in ("/lib", "/apps/lib"):
        rutas += [f"{pasta}/{n}" for n in re.findall(r"(\S+\.so\.\d+)", debugfs(p, f"ls {pasta}").decode("latin-1"))]
    malas = []
    for r in rutas:
        st = debugfs(p, f'stat "{r}"').decode("latin-1")
        u, mo = re.search(r"User:\s+(\d+)", st), re.search(r"Mode:\s+0?([0-7]+)", st)
        if not u or not mo or int(u.group(1)) != 0 or int(mo.group(1), 8) & 0o022:
            malas.append(r)
    verificar(len(rutas) >= 5 and not malas, f"{que}: /lib, ld-sagui.so.1 y las .so de root sin escritura de otros ({malas or len(rutas)})")


def instalar_em(img, dest, nome, args="-s", memoria="256M", antes=()):
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina(nome, "ide", imagem=img, extras=extras, memoria=memoria) as m:
        m.esperar_serial("sagui:", 180)
        for c in antes:
            if isinstance(c, tuple):   # (orden, expresión que tiene que salir, qué se comprueba)
                o = m.comando(c[0])
                verificar(re.search(c[1], o), f"{nome}: {c[2]} ({o.strip()[-300:]})")
            else:
                m.comando(c)
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
    # v8.5.4.2: la línea entera (antes «<v> beta» valía: se cortaba en el primer espacio)
    v = versao_nucleo()
    img, _ = gerar_imagem("h387-j-espacio", extras_img=["--raiz-versao", v + " beta"])
    rescate_por("h387-j3", img, r"es de la versi.n " + re.escape(v + " beta") + r" y este n.cleo es " + re.escape(v))
    os.remove(img)
    img, _ = gerar_imagem("h387-j-final", extras_img=["--raiz-versao", v + "  \t\r"])
    with Maquina("h387-j4", "ide", imagem=img, memoria="128M") as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", m.serial()),
                  "J: con espacios y \\r al final de la marca, la raíz vale")
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


def copiar_particao_de_volta(img, inicio, arquivo):
    with open(arquivo, "rb") as f, open(img, "r+b") as g:
        g.seek(inicio * 512)
        shutil.copyfileobj(f, g)


def id_raiz(img):
    p = particao_raiz(img)
    return debugfs(p, "cat /sistema/raiz/id").decode("latin-1").strip(), p


def parte_l():
    img, _ = gerar_imagem("h387-l", modo="live")
    dest = os.path.join(SAIDA, "h387-l-dest.img")
    with open(dest, "wb") as f:
        f.truncate(512 * 1024 * 1024)
    sesion = ["echo de-la-sesion > /etc/sesion387.txt", "echo '# sesion-387' >> /etc/sagshrc"]
    out, serie = instalar_em(img, dest, "h387-l1", antes=sesion)
    verificar("Se copia el sistema de serie, sin lo cambiado en esta sesi" in out, "L: el instalador dice que copia el sistema de serie")
    p = particao_raiz(dest)
    verificar(b"sesion387" not in debugfs(p, "ls /etc") and b"sesion-387" not in debugfs(p, "cat /etc/sagshrc"),
              "L: (punto 7) lo cambiado en la sesión live no se instala por defecto")
    donos_ld(p, "L: (D378) de serie")
    ident, _ = id_raiz(dest)
    verificar(re.fullmatch(r"[0-9a-f]{16}", ident), f"L: la raíz instalada lleva su identificador ({ident})")
    with open(dest, "rb") as f:
        f.seek(512 + 144)
        cab = f.read(12)
    verificar(cab[:4] == b"SGIN" and f"{int.from_bytes(cab[4:8], 'little'):08x}{int.from_bytes(cab[8:12], 'little'):08x}" == ident,
              "L: y es el mismo que el de la cabecera del disco (CAB_ID_INSTALACAO)")
    # 1. Una copia del disco con otra raíz (otro id) delante: hda es la copia, hdb el bueno; arranca de hda
    #    (las cabeceras son iguales), y la raíz tiene que ser la de hdb.
    otro = os.path.join(SAIDA, "h387-l-otro.img")
    shutil.copyfile(dest, otro)
    _, p = id_raiz(otro)
    with open(os.path.join(SAIDA, "h387-l-id"), "w") as f:
        f.write("0123456789abcdef\n")
    subprocess.run([DEBUGFS, "-w", "-R", "rm /sistema/raiz/id", p], check=True, capture_output=True)
    subprocess.run([DEBUGFS, "-w", "-R", f"write {os.path.join(SAIDA, 'h387-l-id')} /sistema/raiz/id", p], check=True, capture_output=True)
    ini = rotulos_de(otro)["SAGUI-RAIZ"][0]
    copiar_particao_de_volta(otro, ini, p)
    verificar(id_raiz(otro)[0] == "0123456789abcdef", "L: la copia lleva otro identificador")
    with Maquina("h387-l2", "ide", imagem=otro, extras=["-drive", f"file={dest},if=ide,index=1,format=raw,snapshot=on"],
                 memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ\) no vale como raiz: es de otra instalaci.n", s),
                  "L: la raíz de la copia (otro id) se rechaza")
        verificar(re.search(r"\[RAIZ\] hdb\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", s),
                  "L: y se monta la de esta instalación (hdb), con escritura")
    os.remove(otro)
    # 2. El instalado con un pendrive live conectado: arranca del instalado, el pendrive no se toca.
    antes, _ = soma_raiz(img)
    with Maquina("h387-l3", "ide", imagem=dest, snapshot=False, memoria="256M", extras=pendrive_extra(img)) as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[rw\]", m.serial()),
                  "L: con el pendrive live puesto, la raíz es la del instalado")
        m.comando("echo l3 > /root/l3.txt")
        apagar_maquina(m)
    verificar(soma_raiz(img)[0] == antes, "L: el pendrive live no se ha escrito")
    # 3. Del pendrive live con el instalado conectado: la live no toca el instalado.
    antes_d, _ = soma_raiz(dest)
    with Maquina("h387-l4", "usb", imagem=img, memoria="256M",
                 extras=["-drive", f"file={dest},if=ide,index=0,format=raw,snapshot=on"]) as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"\[RAIZ\] usb\d+p\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", m.serial()),
                  "L: arrancando del pendrive, la raíz es la del pendrive (live)")
    verificar(soma_raiz(dest)[0] == antes_d, "L: el instalado no se ha escrito")
    # 4. Dos copias idénticas del instalado: no se sabe cuál es: ninguna con escritura.
    gemelo = os.path.join(SAIDA, "h387-l-gemelo.img")
    shutil.copyfile(dest, gemelo)
    with Maquina("h387-l5", "ide", imagem=dest, memoria="256M",
                 extras=["-drive", f"file={gemelo},if=ide,index=1,format=raw"]) as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar("Hay dos discos con este mismo SaguiOS instalado" in s, "L: con dos copias idénticas, lo avisa")
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", s),
                  "L: y monta la raíz sin escribir en ella (live)")
    os.remove(gemelo)
    # 5. (punto 7) Reinstalar «con los cambios de la sesión».
    # D378 (I1): dos bibliotecas seguidas de otra cuenta en /lib (y una en /apps/lib): ninguna llega al disco.
    ajenas = ["echo x > /lib/zz387a.so.1", "echo x > /lib/zz387b.so.1", "echo x > /apps/lib/zz387c.so.1",
              "chown sagui /lib/zz387a.so.1 /lib/zz387b.so.1 /apps/lib/zz387c.so.1",
              ("ls -l /lib/zz387a.so.1 /lib/zz387b.so.1 /apps/lib/zz387c.so.1",
               r"(?s)(sagui.*zz387[abc]\.so\.1.*){3}", "(D378) en la sesión hay tres .so de sagui en /lib y /apps/lib")]
    out, serie = instalar_em(img, dest, "h387-l6", args="--con-cambios -s", antes=list(sesion) + ajenas)
    verificar("con lo cambiado en esta sesi" in out, "L: con --con-cambios, el instalador lo dice")
    p = particao_raiz(dest)
    verificar(b"sesion387" in debugfs(p, "ls /etc") and b"sesion-387" in debugfs(p, "cat /etc/sagshrc"),
              "L: y lo cambiado en la sesión live va al disco")
    verificar(b"zz387" not in debugfs(p, "ls /lib") and b"zz387" not in debugfs(p, "ls /apps/lib"),
              "L: (D378) las bibliotecas de otra cuenta en /lib y /apps/lib no se instalan (ninguna de las dos seguidas)")
    donos_ld(p, "L: (D378) con --con-cambios")
    verificar(id_raiz(dest)[0] != ident, "L: la reinstalación lleva un identificador nuevo")
    os.remove(dest)
    os.remove(img)


def parte_m():
    """v8.5.4.2 (punto 6): la tecla M al arrancar abre el menú del modo de la raíz; «kconfig raiz_modo» lo guarda."""
    img, _ = gerar_imagem("h387-m")   # «instalado»: rw
    # Sin KVM: con KVM la BIOS llega a la etapa 2 en unas décimas y QEMU no repite la tecla como un
    # teclado de verdad (que, mantenida, la vuelve a mandar), así que la M no llega a tiempo.
    kvm = os.environ.pop("SAGUI_KVM", None)
    m = Maquina("h387-m1", "ide", imagem=img, snapshot=False, memoria="256M")
    try:
        m.ligar()
    finally:
        if kvm is not None:
            os.environ["SAGUI_KVM"] = kvm
    try:
        # La etapa 2 solo mira si ya hay una M en el búfer del teclado de la BIOS: se manda muy seguido
        # desde que arranca la máquina (la BIOS borra el búfer al iniciar el teclado), y luego se mira la pantalla.
        fim = time.time() + 8
        while time.time() < fim:
            m.monitor("sendkey m")
            time.sleep(0.02)
        visto = False
        fim = time.time() + 20
        while time.time() < fim and not visto:
            visto = any("M pulsada: modo de arranque" in l for l in m.tela_texto())
            time.sleep(0.3)
        verificar(visto, "M: con la M pulsada, la etapa 2 enseña el menú del modo")
        time.sleep(0.5)
        m.monitor("sendkey 1")
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar("[RAIZ] modo 3 elegido al arrancar (tecla M)" in s, "M: el núcleo recibe lo elegido (live)")
        verificar(re.search(r"\[RAIZ\] hda\d+ \(LABEL=SAGUI-RAIZ, ext2\) montada en / \[live\]", s), "M: y arranca en live")
        out = m.comando("kconfig raiz_modo=toram", 30)
        verificar(re.search(r"raiz_modo", m.comando("kconfig", 30)), f"M: kconfig conoce raiz_modo ({out.strip()[-200:]})")
        m.comando("sync")
        reiniciar(m)
    finally:
        m.desligar()
    with Maquina("h387-m2", "ide", imagem=img, snapshot=False, memoria="512M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        # v8.5.4.2 (revisión): en un disco instalado, lo guardado no se aplica (se perdería lo escrito)
        verificar("[RAIZ] modo 4 guardado con kconfig: no se aplica a un disco instalado" in s,
                  "M: en un disco instalado, raiz_modo de kconfig no se aplica")
        verificar(re.search(r"montada en / \[rw\]", s), "M: y sigue arrancando con escritura")
        m.comando("kconfig --por-defecto", 30)
        reiniciar(m)
    with Maquina("h387-m3", "ide", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        verificar(re.search(r"montada en / \[rw\]", m.serial()), "M: con --por-defecto, vuelve el modo de la imagen")
    os.remove(img)


def parte_n():
    """v8.5.4.2 (punto 8): la capa de la live en la RAM tiene tope: ENOSPC limpio y el sistema sigue."""
    img, _ = gerar_imagem("h387-n", modo="live")
    with Maquina("h387-n1", "ide", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        m.comando("kconfig capa_ram_pct=10", 30)
        m.comando("sync")
        reiniciar(m)
    with Maquina("h387-n2", "ide", imagem=img, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        out = m.comando("df /", 30)
        print(out)
        out = m.comando("truncate -s 64M /etc/grande387; echo fin-$?", 60)
        verificar(re.search(r"no queda espacio|No queda espacio", out), f"N: más allá del tope: «no queda espacio» ({out.strip()[-300:]})")
        verificar("[RAMFS] la capa sin sitio" in m.serial() and "No queda sitio en la RAM para la capa" in m.serial(), "N: con aviso en el registro y en pantalla")
        m.comando("rm /etc/grande387")
        out = m.comando("truncate -s 4M /etc/peq387; echo hecho-387", 60)
        verificar("hecho-387" in out and "espacio" not in out, "N: lo que cabe se sigue escribiendo")
        verificar("vivo-387" in m.comando("echo vivo-387"), "N: el sistema sigue en pie")
        # v8.5.4.2 (revisión): guardados atómicos (temporal + renombrar encima): el uso no sube (antes, ENOSPC falso)
        m.comando("rm /etc/peq387")
        for pasta in ("/tmp", "/etc"):
            for vuelta in range(4):
                orden = "; ".join(f"truncate -s 2M {pasta}/g387.tmp; mv {pasta}/g387.tmp {pasta}/g387.dat" for _ in range(8))
                out = m.comando(orden + "; echo vuelta-387", 120)
                verificar("espacio" not in out, f"N: {pasta}: 8 guardados atómicos de 2 MB más (vuelta {vuelta + 1}) sin ENOSPC")
            out = m.comando(f"df {pasta}", 30)
            usado = re.search(r"\s(\d+)\s+(\d+)\s+(\d+)\s+\d+%", out)
            verificar(usado and int(usado.group(2)) < 8 * 1024, f"N: {pasta}: tras 32 guardados de 2 MB el uso es el de un archivo ({out.strip()[-160:]})")
    os.remove(img)


def parte_o():
    """v8.5.4.2 (punto 10, F): un ext2 que no se desmontó bien pasa el fsck básico antes de escribir en él."""
    # 1. Un ext2 con los mapas mal a propósito (un bloque en uso marcado libre, uno libre marcado en uso)
    #    y sin el bit de limpio.
    datos = "".join(f"linea {i} del h387\n" for i in range(4000))
    disco, inis = criar_disco("h387-o-datos", [{"tipo": "ext2", "mb": 32, "rotulo": "O387", "arquivos": {"a.txt": datos}}])
    p = extrair_particao(disco, inis[0], 32, os.path.join(SAIDA, "h387-o-datos.ext2"))
    blocos = debugfs(p, "blocks /a.txt").decode().split()
    total = int(re.search(r"Block count:\s+(\d+)", subprocess.run([DEBUGFS, "-R", "stats", p], capture_output=True, text=True).stdout).group(1))
    for c in (f"freeb {blocos[0]}", f"setb {total - 20}", "ssv state 0"):
        subprocess.run([DEBUGFS, "-w", "-R", c, p], check=True, capture_output=True)
    verificar(e2fsck(p)[0] != 0, "O: el ext2 de prueba está mal (e2fsck lo ve)")
    copiar_particao_de_volta(disco, inis[0], p)
    img, _ = gerar_imagem("h387-o")
    with Maquina("h387-o1", "ide", imagem=img, memoria="256M",
                 extras=["-drive", f"file={disco},if=ide,index=1,format=raw"]) as m:
        m.esperar_serial("sagui:", 180)
        m.comando("mkdir /m1; mount -o rw hdb1 /m1")
        s = m.serial()
        r = re.search(r"\[EXT2\] hdb1: no se desmonto bien: fsck basico hecho \((\d+) correcciones", s)
        verificar(r and int(r.group(1)) >= 2, f"O: al montarlo con escritura, el fsck básico lo corrige ({r.group(0) if r else 'nada'})")
        m.comando("echo nuevo-h387 > /m1/nuevo.txt")
        verificar(m.comando("md5sum /m1/a.txt").split()[0:1] != [], "O: el archivo de antes se lee")
        m.comando("umount /m1; sync")
    extrair_particao(disco, inis[0], 32, p)
    cod, txt = e2fsck(p)
    verificar(cod == 0, f"O: tras el fsck básico y escribir, e2fsck -fn limpio ({txt.strip().splitlines()[-1] if txt.strip() else ''})")
    verificar(debugfs(p, "cat /a.txt").decode() == datos, "O: el archivo de antes sigue intacto")
    os.remove(disco)
    # 1b. (revisión) s_inodes_count que no cuadra con grupos * inodos por grupo: con más, no se monta (antes el
    #     fsck escribía fuera de su memoria); con menos, solo lectura.
    disco, inis = criar_disco("h387-o-inodos", [{"tipo": "ext2", "mb": 8, "rotulo": "MAS387"},
                                                 {"tipo": "ext2", "mb": 8, "rotulo": "MENOS387"}])
    for k, delta in ((0, +64), (1, -8)):
        pk = extrair_particao(disco, inis[k], 8, os.path.join(SAIDA, f"h387-o-inodos{k}.ext2"))
        st = subprocess.run([DEBUGFS, "-R", "stats", pk], capture_output=True, text=True).stdout
        n = int(re.search(r"Inode count:\s+(\d+)", st).group(1))
        for c in (f"ssv inodes_count {n + delta}", "ssv state 0"):
            subprocess.run([DEBUGFS, "-w", "-R", c, pk], check=True, capture_output=True)
        copiar_particao_de_volta(disco, inis[k], pk)
    with Maquina("h387-o1b", "ide", imagem=img, memoria="256M",
                 extras=["-drive", f"file={disco},if=ide,index=1,format=raw"]) as m:
        m.esperar_serial("sagui:", 180)
        m.comando("mkdir /m2 /m3; mount -o rw hdb1 /m2; mount -o rw hdb2 /m3")
        s = m.serial()
        verificar(re.search(r"hdb1: ext: m.s inodos que grupos", s), "O: con más inodos que grupos * inodos por grupo, no se monta")
        verificar(re.search(r"hdb2: la cuenta de inodos no cuadra con los grupos", s), "O: con menos, solo lectura")
        verificar("vivo-o387" in m.comando("echo vivo-o387"), "O: y el sistema sigue")
    os.remove(disco)
    # 2. La raíz instalada con un corte de luz: al arrancar, el fsck básico y la raíz con escritura.
    with Maquina("h387-o2", "ide", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        m.comando("mkdir /root/corte; echo antes > /root/corte/a.txt; sync")
        m.comando("echo durante > /root/corte/b.txt; rm /root/corte/a.txt")
        m.monitor("quit")   # el corte: sin desmontar
    with Maquina("h387-o3", "ide", imagem=img, snapshot=False, memoria="256M") as m:
        m.esperar_serial("sagui:", 180)
        s = m.serial()
        verificar(re.search(r"\[EXT2\] hda\d+: no se desmonto bien: fsck basico hecho", s), "O: tras el corte, la raíz pasa el fsck básico")
        verificar(re.search(r"montada en / \[rw\];", s), "O: y se monta con escritura")
        m.comando("echo despues > /root/corte/c.txt")
        apagar_maquina(m)
    cod, txt = e2fsck(particao_raiz(img))
    verificar(cod == 0, f"O: tras el corte y un apagado limpio, e2fsck -fn de la raíz limpio ({txt.strip()[-300:]})")
    os.remove(img)


def pendrive_extra(img):
    return ["-device", "usb-ehci,id=ehci", "-drive", f"if=none,id=pen,format=raw,file={img}",
            "-device", "usb-storage,bus=ehci.0,drive=pen"]


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
    partes = sys.argv[1:] or ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o"]
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
    if "l" in partes:
        parte_l()
    if "m" in partes:
        parte_m()
    if "n" in partes:
        parte_n()
    if "o" in partes:
        parte_o()
    print("H387: OK")
