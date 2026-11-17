#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H318 (D318, v8.4.3) - el asistente de instalación, cambiar el tamaño de las particiones y el
diálogo «Ampliar /home» del primer arranque.

Parte 1 (anfitrión, sin QEMU): la libsagui de particiones compilada para el PC
  (tests/recursos/instalador_anfitriao y redimensao_anfitriao) sobre archivos-disco:
  - FAT32 con datos: encoger hasta su mínimo (fsck.fat -n limpio, archivos iguales por suma),
    negarse con datos al final (sin cambiar un byte) y por debajo de 65.525 clústeres;
  - un disco como el de una imagen persistente (SaguiOS, extendida con /home, juegos y
    SAGUI-CONF) grabado en uno mayor: el plan mueve las dos lógicas de detrás al final y
    «tamano 5 0» lo hace; e2fsck -fn limpio en las tres y los archivos iguales;
  - reducir una primaria FAT32 con datos (pt_executar) y que NTFS se niegue.
Parte 2 (QEMU, escritorio, dos discos): el asistente de punta a punta con «Usar todo el disco»,
  /tmp en FAT32, contraseña de root, nombre del equipo, sshd y configuración copiada; capturas
  H318-* de cada paso. El disco instalado: tabla, e2fsck/fsck.fat, la configuración en /home y en
  SAGUI-CONF; arranca solo (BIOS) con su nombre de equipo y, con OVMF, por UEFI.
Parte 3 (QEMU, escritorio): «Junto a lo que hay» sobre un disco con una FAT32 con datos: se
  reduce (cuadro de MB), se instala en el hueco y los datos siguen intactos (fsck.fat, sumas).
Parte 4 (QEMU): una imagen persistente pequeña grabada en un disco grande: el diálogo del primer
  arranque, «Ampliar» (mueve juegos, /apps y SAGUI-CONF al final), /home crece y conserva sus
  archivos; el segundo arranque ya no pregunta; e2fsck limpio de todas desde el PC.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, extrair_particao, e2fsck, debugfs, opcoes_apps,  # noqa: E402
                  RAIZ, SAIDA, BUILD, IMAGEM)

MB = 1024 * 1024
E2 = "/usr/sbin"


def herramienta(nome):
    for d in (E2, "/sbin", os.path.expanduser("~/.local/saguitools/root/usr/sbin"),
              os.path.expanduser("~/.local/saguitools/root/sbin"), os.path.expanduser("~/.local/saguitools/root/usr/bin")):
        if os.path.exists(os.path.join(d, nome)):
            return os.path.join(d, nome)
    return shutil.which(nome) or nome


MKFS_VFAT, FSCK_VFAT, MCOPY, MTYPE, MMD, MDEL = (herramienta(n) for n in ("mkfs.vfat", "fsck.vfat", "mcopy", "mtype", "mmd", "mdel"))
MKE2FS = herramienta("mke2fs")


def sha(dados):
    return hashlib.sha256(dados).hexdigest()


def sfdisk(img):
    r = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True).stdout
    t = {}
    for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)(, bootable)?", r):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), m.group(4), bool(m.group(5)))
    return t


def fsck_fat(img):
    r = subprocess.run([FSCK_VFAT, "-n", img], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def fsck_ext(img):
    r = subprocess.run([os.path.join(E2, "e2fsck"), "-fn", img], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def mtype(img, ruta, offset=None):
    alvo = f"{img}@@{offset}" if offset is not None else img
    return subprocess.run([MTYPE, "-i", alvo, "::" + ruta], capture_output=True).stdout


# ------------------------------------------------------------ parte 1: anfitrión

def compilar():
    d = os.path.join(SAIDA, "h318-anfitriao")
    os.makedirs(d, exist_ok=True)
    rec = os.path.join(RAIZ, "tests", "recursos")
    lib = os.path.join(RAIZ, "userland", "libsagui")
    inc = os.path.join(RAIZ, "userland", "inc")
    # el de un volumen (redimensao_anfitriao) y el de un disco entero (instalador_anfitriao)
    dv, dd = os.path.join(d, "vol"), os.path.join(d, "disco")
    for x in (dv, dd):
        os.makedirs(x, exist_ok=True)
    shutil.copy(os.path.join(rec, "redimensao_anfitriao", "sagui.h"), dv)
    shutil.copy(os.path.join(inc, "particoes.h"), dv)
    vol = os.path.join(d, "volume")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I", dv, "-o", vol,
                    os.path.join(rec, "redimensao_anfitriao", "principal.c"),
                    os.path.join(lib, "redimensao.c"), os.path.join(lib, "formatacao.c")], check=True)
    shutil.copy(os.path.join(rec, "instalador_anfitriao", "sagui.h"), dd)
    for h in ("particoes.h", "gpt.h", "compressao.h"):
        shutil.copy(os.path.join(inc, h), dd)
    disco = os.path.join(d, "disco-bin")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-I", dd, "-o", disco,
                    os.path.join(rec, "instalador_anfitriao", "principal.c"),
                    *(os.path.join(lib, f) for f in ("particoes.c", "particoes_acoes.c", "particoes_tamanho.c", "gpt.c",
                                                      "redimensao.c", "formatacao.c"))], check=True)
    return vol, disco, d


def rodar(exe, *args, tmp=None):
    env = dict(os.environ)
    if tmp:
        env["DISCO_TMP"] = tmp
    r = subprocess.run([exe, *map(str, args)], capture_output=True, text=True, env=env)
    return r.returncode, r.stdout.strip()


def parte1():
    vol, disco, d = compilar()
    tmp = os.path.join(d, "deshacer")
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    datos = {f"a{i}.bin": os.urandom(700_000 + i * 1000) for i in range(4)}
    for k, v in datos.items():
        with open(os.path.join(d, k), "wb") as f:
            f.write(v)

    # --- FAT32 con datos: encoger sin mover nada
    fat = os.path.join(d, "fat.img")
    if os.path.exists(fat):
        os.remove(fat)
    with open(fat, "wb") as f:
        f.truncate(400 * MB)
    subprocess.run([MKFS_VFAT, "-F", "32", "-n", "H318", fat], check=True, capture_output=True)
    for k in datos:
        subprocess.run([MCOPY, "-i", fat, os.path.join(d, k), "::/" + k], check=True)
    rc, out = rodar(vol, "minimo-fat32", fat)
    minimo = int(out)
    verificar(rc == 0 and 2048 * 256 <= minimo < 300 * 2048, f"FAT32 de 400 MB con 3 MB: el mínimo es el de 65.525 clústeres ({minimo // 2048} MB)")
    rc, out = rodar(vol, "reducir-fat32", fat, minimo)
    verificar(rc == 0 and "ha encogido de 399 a" in out, f"el FAT32 encoge hasta su mínimo ({out})")
    with open(fat, "r+b") as f:
        f.truncate(minimo * 512)                       # ...y la partición después
    rcf, outf = fsck_fat(fat)
    iguales = all(sha(mtype(fat, "/" + k)) == sha(v) for k, v in datos.items())
    verificar(rcf == 0 and iguales, f"fsck.fat -n limpio y los cuatro archivos iguales por suma tras encoger ({outf.strip()[-80:]})")
    rc, out = rodar(vol, "reducir-fat32", fat, 100 * 2048)
    verificar(rc != 0 and "dejaría de ser FAT32" in out, f"por debajo de 65.525 clústeres se niega ({out})")
    # datos al final: se niega sin tocar un byte
    if os.path.exists(fat):
        os.remove(fat)
    with open(fat, "wb") as f:
        f.truncate(600 * MB)
    subprocess.run([MKFS_VFAT, "-F", "32", "-n", "H318", fat], check=True, capture_output=True)
    relleno = os.path.join(d, "relleno.bin")
    with open(relleno, "wb") as f:
        f.truncate(450 * MB)
    subprocess.run([MCOPY, "-i", fat, relleno, "::/relleno.bin"], check=True)
    subprocess.run([MCOPY, "-i", fat, os.path.join(d, "a0.bin"), "::/final.bin"], check=True)
    subprocess.run([MDEL, "-i", fat, "::/relleno.bin"], check=True)
    antes = sha(open(fat, "rb").read())
    rc, out = rodar(vol, "reducir-fat32", fat, 300 * 2048)
    verificar(rc != 0 and "hay datos al final" in out, f"con datos al final no encoge ({out})")
    verificar(sha(open(fat, "rb").read()) == antes, "y el volumen no ha cambiado ni un byte")

    # --- un disco como el de una imagen persistente, grabado en uno mayor
    img = os.path.join(d, "persistente.img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(300 * MB)
    disposicion = ("label: dos\nstart=1, size=40959, type=da, bootable\nstart=40960, type=f\n"
                   "start=43008, size=131072, type=83\nstart=176128, size=65536, type=83\nstart=243712, size=32768, type=83\n")
    subprocess.run(["sfdisk", "-q", img], input=disposicion, text=True, check=True)
    cont = os.path.join(d, "conteudo")
    shutil.rmtree(cont, ignore_errors=True)
    os.makedirs(os.path.join(cont, "sub"))
    grande = os.urandom(5 * MB)
    with open(os.path.join(cont, "sub", "grande.bin"), "wb") as f:
        f.write(grande)
    with open(os.path.join(cont, "hola.txt"), "w") as f:
        f.write("hola desde h318\n")
    for ini, mb, rot in ((43008, 64, "SAGUI-DATOS"), (176128, 32, "SAGUI-JOGOS"), (243712, 16, "SAGUI-CONF")):
        subprocess.run([MKE2FS, "-q", "-F", "-t", "ext2", "-b", "1024", "-L", rot, "-d", cont, "-E", f"offset={ini * 512}",
                        img, f"{mb * 1024}"], check=True)
    with open(img, "r+b") as f:
        f.truncate(1024 * MB)                             # «grabada en un disco mayor»
    rc, out = rodar(disco, "planear", img, 5, 0, "mover", tmp=tmp)
    verificar(rc == 0 and "nmover=2" in out and "ext_fim=2097152" in out, f"el plan: la extendida hasta el final y dos lógicas al final ({out})")
    rc, out = rodar(disco, "planear", img, 5, 0, tmp=tmp)
    verificar(rc == 0 and "nmover=0" in out and "depois=131072" in out, f"sin mover, /home no puede crecer ({out})")
    rc, out = rodar(disco, "tamano", img, 5, 0, tmp=tmp)
    verificar(rc == 0 and "se han movido al final" in out, f"«tamano 5 0»: /home crece moviendo las de detrás ({out.splitlines()[1] if len(out.splitlines()) > 1 else out})")
    t = sfdisk(img)
    print("    tabla:", t)
    verificar(t[5][1] > 900 * 2048 and t[6][0] > t[5][0] + t[5][1] and t[7][0] + t[7][1] <= 2097152,
              "la tabla: /home de casi 1 GB y juegos y SAGUI-CONF al final del disco")
    for n, rot in ((5, "SAGUI-DATOS"), (6, "SAGUI-JOGOS"), (7, "SAGUI-CONF")):
        p = extrair_particao(img, t[n][0], t[n][1] // 2048)
        rc, o = fsck_ext(p)
        ok = rc == 0 and debugfs(p, "cat /sub/grande.bin") == grande and b"hola desde h318" in debugfs(p, "cat /hola.txt")
        lab = subprocess.run([os.path.join(E2, "e2label"), p], capture_output=True, text=True).stdout.strip()
        verificar(ok and lab == rot, f"{rot}: e2fsck -fn limpio y sus archivos iguales ({o.strip()[-60:] if rc else 'limpio'})")

    # --- una primaria FAT32 con datos que ocupa el disco: se reduce con pt_executar
    img = os.path.join(d, "fat-disco.img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(800 * MB)
    subprocess.run(["sfdisk", "-q", img], input="label: dos\nstart=2048, type=c\n", text=True, check=True)
    t = sfdisk(img)
    part = extrair_particao(img, t[1][0], t[1][1] // 2048)
    subprocess.run([MKFS_VFAT, "-F", "32", "-n", "DATOS", part], check=True, capture_output=True)
    for k in datos:
        subprocess.run([MCOPY, "-i", part, os.path.join(d, k), "::/" + k], check=True)
    with open(part, "rb") as f, open(img, "r+b") as g:
        g.seek(t[1][0] * 512)
        g.write(f.read())
    rc, out = rodar(disco, "reducir", img, 1, 400, tmp=tmp)
    verificar(rc == 0 and "pasa de" in out, f"pt_executar reduce la FAT32 con datos a 400 MB ({out.splitlines()[0]})")
    t = sfdisk(img)
    verificar(t[1][1] <= 400 * 2048, f"la tabla dice 400 MB ({t[1][1] // 2048})")
    part = extrair_particao(img, t[1][0], t[1][1] // 2048)
    rcf, outf = fsck_fat(part)
    iguales = all(sha(mtype(part, "/" + k)) == sha(v) for k, v in datos.items())
    verificar(rcf == 0 and iguales, "fsck.fat -n limpio y los archivos iguales tras reducir la partición")
    # NTFS (firma en el sector de arranque): no se encoge
    with open(img, "r+b") as f:
        f.seek(t[1][0] * 512 + 3)
        f.write(b"NTFS    ")
        f.seek(t[1][0] * 512 + 82)                      # sin la firma de FAT32: un NTFS de verdad no la lleva
        f.write(b"\0" * 8)
    subprocess.run(["sfdisk", "-q", "--part-type", img, "1", "7"], check=True, capture_output=True)
    antes = sha(open(img, "rb").read())
    rc, out = rodar(disco, "reducir", img, 1, 200, tmp=tmp)
    verificar(rc != 0 and "NTFS" in out, f"una NTFS no se reduce ({out.splitlines()[0] if out else ''})")
    verificar(sha(open(img, "rb").read()) == antes, "y el disco no ha cambiado")


# ------------------------------------------------------------ el asistente en QEMU

class Asistente:
    """Maneja la ventana del asistente con las zonas que escribe en el registro."""

    def __init__(self, m, titulo="Instalar SaguiOS"):
        self.m = m
        self.titulo = titulo
        self.ox = self.oy = None

    def origen(self):
        s = self.m.serial()
        v = re.findall(rf'criada por \d+: "{self.titulo}" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        if not v:
            return False
        jl, ja, jx, jy = (int(x) for x in v[-1])
        self.ox, self.oy = jx + 2, jy + 24
        return True

    def esperar_pagina(self, n, tempo=60):
        fim = time.time() + tempo
        while time.time() < fim:
            s = self.m.serial()
            ult = s.rfind("instalar: pagina ")
            if ult >= 0 and s.startswith(f"instalar: pagina {n} ", ult) and f"instalar: fin de zonas {n}" in s[ult:]:
                time.sleep(0.6)
                return
            time.sleep(0.3)
        raise AssertionError(f"el asistente no llega a la página {n}")

    def esperar_zona(self, nome, tempo=20):
        fim = time.time() + tempo
        while time.time() < fim:
            if nome in self.zonas():
                time.sleep(0.3)
                return
            time.sleep(0.3)
        raise AssertionError(f"no aparece «{nome}» en el asistente")

    def zonas(self):
        s = self.m.serial()
        ult = s.rfind("instalar: pagina ")
        z = {}
        for mm in re.finditer(r"instalar: zona (\S+) (-?\d+) (-?\d+) (\d+) (\d+)", s[ult:]):
            z[mm.group(1)] = tuple(int(x) for x in mm.group(2, 3, 4, 5))
        return z

    def pulsar(self, nome, dx=None):
        if self.ox is None:
            self.origen()
        z = self.zonas()
        if nome not in z:
            raise AssertionError(f"el asistente no tiene «{nome}» en esta página ({sorted(z)})")
        x, y, l, a = z[nome]
        cx = x + (dx if dx is not None else l // 2)
        self.m.clique(self.ox + cx, self.oy + y + a // 2)
        time.sleep(0.4)

    def escribir(self, nome, texto):
        self.pulsar(nome)
        for _ in range(12):
            self.m.tecla("backspace", 0.03)
        self.m.digitar(texto, 0.05)
        time.sleep(0.3)


def disco_vazio(nome, mb):
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(mb * MB)
    return img


def preparar_sesion(m):
    """La configuración de la sesión en vivo que se copiará."""
    for c in ("mkdir /home", "mkdir /home/.config", "mkdir /home/.ssh", "echo red=CasaH318 > /home/.config/wifi.conf",
              "echo ssh-ed25519 AAAAH318 prueba > /home/.ssh/authorized_keys", "echo documento h318 > /home/carta318.txt",
              "echo fondo=azul > /home/.config/prueba318.conf"):
        m.comando(c)


def parte2():
    dest = disco_vazio("h318-entero", 2048)
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h318-gui", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        preparar_sesion(m)
        a = Asistente(m)
        m.digitar("instalar --ventana\n", 0.04)
        a.esperar_pagina(0)
        a.origen()
        verificar("instalar: zona siguiente" in m.serial(), "el asistente abre en la bienvenida")
        m.captura("H318-1-bienvenida")
        a.pulsar("siguiente")
        a.esperar_pagina(1)
        z = a.zonas()
        verificar("disco1" in z and "disco0" not in z, "el disco de arranque (hda) no se puede elegir; hdb sí")
        a.pulsar("disco1")
        m.captura("H318-2-disco")
        a.pulsar("siguiente")
        a.esperar_pagina(2)
        verificar("tipo-entero" in a.zonas(), "tipo de instalación: las tres opciones")
        a.pulsar("tipo-entero")
        a.escribir("confirmar", "hdb")
        m.captura("H318-3-tipo-entero")
        a.pulsar("siguiente")
        a.esperar_pagina(3)
        a.pulsar("usa-tmp")
        a.escribir("mb-tmp", "64")
        a.pulsar("fs-tmp")
        time.sleep(0.5)
        verificar(re.search(r"instalar: plan ok", m.serial()[m.serial().rfind("instalar: pagina 3"):]), "los tamaños dan un plan válido")
        m.captura("H318-4-tamanos")
        a.pulsar("siguiente")
        a.esperar_pagina(4)
        a.pulsar("ar-ambos")
        a.escribir("nombre", "h318pc")
        a.escribir("clave-root", "clave318")
        a.escribir("clave-root2", "clave318")
        a.pulsar("sshd")
        m.captura("H318-5-opciones")
        a.pulsar("siguiente")
        a.esperar_pagina(5)
        a.pulsar("cc-ssh")
        a.pulsar("cc-docs")
        m.captura("H318-6-configuracion")
        a.pulsar("siguiente")
        a.esperar_pagina(6)
        m.captura("H318-7-resumen")
        a.pulsar("siguiente")                 # «Instalar»
        time.sleep(1.5)
        m.captura("H318-8-confirmar")
        m.tecla("ret")
        m.esperar_serial("instalar: fase 3", 300)
        time.sleep(0.5)
        m.captura("H318-9-progreso")
        m.esperar_serial("instalar: resultado", 1200)
        time.sleep(1.5)
        s = m.serial()
        verificar("instalar: resultado 0" in s, f"el asistente instala sin errores ({s[s.rfind('instalar: resultado'):][:300]})")
        m.captura("H318-10-terminado")
        verificar("instalar: zona reiniciar" in s[s.rfind("instalar: pagina 8"):], "al final, «Reiniciar ahora»")
    t = sfdisk(dest)
    print("    tabla:", t)
    verificar(t[1][2] == "da" and t[1][3] and any(v[2] == "ef" for v in t.values()), "SaguiOS (0xDA, activa) y la partición EFI (BIOS y UEFI)")
    logicas = {n: v for n, v in t.items() if n >= 5}
    rotulos, tipos = {}, {}
    for n, v in logicas.items():
        p = extrair_particao(dest, v[0], v[1] // 2048)
        if v[2] == "c":
            rc, o = fsck_fat(p)
            rot = subprocess.run([herramienta("fatlabel"), p], capture_output=True, text=True).stdout.strip()
        else:
            rc, o = fsck_ext(p)
            rot = subprocess.run([os.path.join(E2, "e2label"), p], capture_output=True, text=True).stdout.strip()
        rotulos[rot] = p
        tipos[rot] = v[2]
        verificar(rc == 0, f"{rot} ({'FAT32' if v[2] == 'c' else 'ext2'}) sano en el anfitrión")
    verificar(tipos.get("SAGUI-TMP") == "c", "/tmp en FAT32, como se eligió")
    home, conf = rotulos["SAGUI-DATOS"], rotulos["SAGUI-CONF"]
    verificar(b"CasaH318" in debugfs(home, "cat /.config/wifi.conf") and b"fondo=azul" in debugfs(home, "cat /.config/prueba318.conf"),
              "/home instalado: preferencias y Wi-Fi copiadas")
    verificar(b"AAAAH318" in debugfs(home, "cat /.ssh/authorized_keys") and b"documento h318" in debugfs(home, "cat /carta318.txt"),
              "/home instalado: claves SSH y documentos copiados")
    st = debugfs(home, "stat /carta318.txt") + debugfs(home, "stat /.config")
    verificar(len(re.findall(rb"User:\s+1000\b", st)) == 2, "D282: lo copiado a /home y lo que crea el instalador es de sagui (uid 1000)")
    if "SAGUI-APPS" in rotulos:
        verificar(re.search(rb"User:\s+1000\b", debugfs(rotulos["SAGUI-APPS"], "stat /vps")) and re.search(rb"User:\s+0\b", debugfs(rotulos["SAGUI-APPS"], "stat /")),
                  "D282: /apps es de root y /apps/vps de sagui")
    verificar(re.search(rb"Mode:\s+0600", debugfs(conf, "stat /etc/sombra")), "D282: la sombra de SAGUI-CONF en 0600")
    verificar(b"h318pc" in debugfs(conf, "cat /etc/hostname"), "SAGUI-CONF: el nombre del equipo")
    sombra = debugfs(conf, "cat /etc/sombra")
    verificar(re.search(rb"^root:pbkdf2-sha256\$\d+\$[0-9a-f]{32}\$[0-9a-f]{64}$", sombra, re.M) and b"clave318" not in sombra,
              "SAGUI-CONF: la contraseña de root como hash PBKDF2 (nunca en claro)")
    verificar(b"sshd=si" in debugfs(conf, "cat /etc/servicios/activados"), "SAGUI-CONF: sshd activado")
    # El disco instalado arranca solo (BIOS).
    with Maquina("h318-gui-arranca", "ide", imagem=dest, snapshot=True) as m:
        m.esperar_serial("area de trabalho no ar", 240)
        time.sleep(3)
        s = m.serial()
        verificar("es el disco de arranque (cabecera igual" in s, "el disco instalado arranca por BIOS y se reconoce como el de arranque")
        out = m.comando("hostname", 60)
        verificar("h318pc" in out, "con el nombre de equipo elegido")
        m.captura("H318-11-arranca")
    # Y por UEFI (OVMF), si está.
    try:
        with Maquina("h318-gui-uefi", "ide", imagem=dest, snapshot=True, firmware="x64") as m:
            m.esperar_serial("area de trabalho no ar", 300)
            verificar("es el disco de arranque" in m.serial(), "el disco instalado (MBR + ESP) arranca también por UEFI (OVMF)")
    except Exception as e:   # sin OVMF en el anfitrión
        if "OVMF" in str(e):
            print("    (sin OVMF: no se prueba el arranque UEFI)", e)
        else:
            raise


def parte3():
    datos = {f"foto{i}.bin": os.urandom(400_000 + i) for i in range(3)}
    dest = disco_vazio("h318-junto", 1536)
    subprocess.run(["sfdisk", "-q", dest], input="label: dos\nstart=2048, type=c\n", text=True, check=True)
    t = sfdisk(dest)
    part = extrair_particao(dest, t[1][0], t[1][1] // 2048)
    subprocess.run([MKFS_VFAT, "-F", "32", "-n", "WINDATOS", part], check=True, capture_output=True)
    d = os.path.join(SAIDA, "h318-junto-datos")
    os.makedirs(d, exist_ok=True)
    for k, v in datos.items():
        with open(os.path.join(d, k), "wb") as f:
            f.write(v)
        subprocess.run([MCOPY, "-i", part, os.path.join(d, k), "::/" + k], check=True)
    with open(part, "rb") as f, open(dest, "r+b") as g:
        g.seek(t[1][0] * 512)
        g.write(f.read())
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h318-junto", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        a = Asistente(m)
        m.digitar("instalar --ventana\n", 0.04)
        a.esperar_pagina(0)
        a.origen()
        a.pulsar("siguiente")
        a.esperar_pagina(1)
        a.pulsar("disco1")
        a.pulsar("siguiente")
        a.esperar_pagina(2)
        z = a.zonas()
        verificar("reducir-mb" in z and "tirador" in z, "«junto a lo que hay» propone reducir la FAT32 (tirador y cuadro de MB)")
        a.escribir("reducir-mb", "600")
        time.sleep(0.5)
        m.captura("H318-12-junto")
        a.pulsar("tipo-avanzado")
        a.esperar_zona("parte1")
        a.pulsar("parte1")
        time.sleep(0.5)
        m.captura("H318-13-avanzado")
        a.pulsar("tipo-junto")
        a.escribir("reducir-mb", "600")
        a.pulsar("siguiente")
        a.esperar_pagina(3)
        a.pulsar("siguiente")
        a.esperar_pagina(4)
        a.pulsar("ar-bios")
        a.escribir("clave-root", "clave318")   # D371 (P1 b): la de root es obligatoria
        a.escribir("clave-root2", "clave318")
        a.pulsar("siguiente")
        a.esperar_pagina(5)
        a.pulsar("siguiente")
        a.esperar_pagina(6)
        s = m.serial()
        m.captura("H318-14-resumen-junto")
        a.pulsar("siguiente")
        time.sleep(1.5)
        m.tecla("ret")
        m.esperar_serial("instalar: resultado", 1200)
        time.sleep(1)
        s = m.serial()
        verificar("instalar: resultado 0" in s, f"instalar junto a la FAT32 termina bien ({s[s.rfind('instalar: resultado'):][:300]})")
    t = sfdisk(dest)
    print("    tabla:", t)
    verificar(t[1][2] == "c" and t[1][1] <= 600 * 2048 and any(v[2] == "da" for v in t.values()),
              "la FAT32 mide 600 MB y SaguiOS está en el hueco que dejó")
    part = extrair_particao(dest, t[1][0], t[1][1] // 2048)
    rc, o = fsck_fat(part)
    iguales = all(sha(mtype(part, "/" + k)) == sha(v) for k, v in datos.items())
    verificar(rc == 0 and iguales, "la FAT32 reducida: fsck.fat -n limpio y sus archivos iguales por suma")


def parte4():
    img = os.path.join(SAIDA, "h318-pequena.img")
    b = BUILD
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", f"{b}/boot/mbr.bin", "--estagio2", f"{b}/boot/estagio2.bin", "--kernel", f"{b}/kernel.bin",
                    "--kernel-elf", f"{b}/kernel.elf", "--ramdisk", f"{b}/ramdisk.sgfs", "--saida", img,
                    "--vesa", "--dados-mb", "64", "--sin-pedir-root", "--sin-migrar-home",   # D371: como las demás de prueba
                    *opcoes_apps()], check=True, stdout=subprocess.DEVNULL)
    t0 = sfdisk(img)
    # Un archivo en /home antes de arrancar (escrito desde el PC en la partición de datos).
    home0 = extrair_particao(img, t0[5][0], t0[5][1] // 2048)
    local = os.path.join(SAIDA, "h318-guardado.txt")
    with open(local, "w") as f:
        f.write("persistente318\n")
    subprocess.run([os.path.join(E2, "debugfs"), "-w", "-R", f"write {local} guardado.txt", home0], check=True, capture_output=True)
    with open(home0, "rb") as f, open(img, "r+b") as g:
        g.seek(t0[5][0] * 512)
        g.write(f.read())
    with open(img, "r+b") as f:
        f.truncate(2048 * MB)                         # la imagen, grabada en un disco de 2 GB
    with Maquina("h318-ampliar", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("ampliar-home: pregunta", 180)
        time.sleep(2)
        s = m.serial()
        mm = re.search(r"ampliar-home: pregunta: ganar (\d+) MB \((\w+)\)[^\n]*botones y=(\d+) ampliar=(\d+)", s)
        verificar(mm and int(mm.group(1)) > 1500 and mm.group(2) == "ampliar", f"el primer arranque pregunta si ampliar /home ({mm.group(0) if mm else ''})")
        v = re.findall(r'criada por \d+: "Primer arranque: espacio libre" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        jx, jy = int(v[-1][2]) + 2, int(v[-1][3]) + 24
        m.captura("H318-15-primer-arranque")
        m.clique(jx + int(mm.group(4)), jy + int(mm.group(3)) + 13)
        m.esperar_serial("ampliar-home: resultado", 600)
        time.sleep(1.5)
        s = m.serial()
        verificar("ampliar-home: resultado 0" in s, f"«Ampliar» termina bien ({s[s.rfind('ampliar-home: resultado'):][:300]})")
        m.captura("H318-16-ampliado")
        m.tecla("ret")                                  # cierra el diálogo
        time.sleep(1)
        verificar(re.search(r"\[MONTAGEM\] hda5 \(ext2\) montado em /home", s[s.rfind("ampliar-home: empieza"):]),
                  "/home se vuelve a montar al acabar")
        m.digitar("apagar-pc\n", 0.05)
        fim = time.time() + 60
        while time.time() < fim and m.proc.poll() is None:
            time.sleep(0.3)
    t = sfdisk(img)
    print("    antes:", t0, "\n    después:", t)
    home = [n for n, v in t.items() if n >= 5 and v[0] == t0[5][0]]
    verificar(home and t[home[0]][1] > 1500 * 2048, "en la tabla, /home pasa de 64 MB a más de 1,5 GB")
    for n, v in t.items():
        if n < 5:
            continue
        p = extrair_particao(img, v[0], v[1] // 2048)
        rc, o = fsck_ext(p)
        verificar(rc == 0, f"la lógica {n} ({v[1] // 2048} MB): e2fsck -fn limpio")
    p = extrair_particao(img, t[home[0]][0], t[home[0]][1] // 2048)
    verificar(b"persistente318" in debugfs(p, "cat /guardado.txt"), "el archivo de /home sigue ahí")
    with Maquina("h318-ampliar2", "ide", imagem=img, snapshot=True) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(15)
        verificar("ampliar-home: pregunta" not in m.serial(), "el segundo arranque ya no pregunta")


def main():
    partes = sys.argv[1:] or ["1", "2", "3", "4"]
    if "1" in partes:
        print("parte 1: anfitrión")
        parte1()
    if "2" in partes:
        print("parte 2: asistente, usar todo el disco")
        parte2()
    if "3" in partes:
        print("parte 3: asistente, junto a lo que hay")
        parte3()
    if "4" in partes:
        print("parte 4: ampliar /home en el primer arranque")
        parte4()
    print("H318 OK")


if __name__ == "__main__":
    main()
