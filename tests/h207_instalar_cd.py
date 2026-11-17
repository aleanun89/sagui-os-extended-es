#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H207 (D207) - arrancar del CD/DVD e instalar SaguiOS en el disco interno.

Lo que pidió el mantenedor: su Fujitsu Amilo Pi 1536 no arranca por USB, pero sí arranca la
ISO (El Torito con emulación de disco duro, D077). Desde el CD, SaguiOS tiene que ver el
lector (ATAPI), leer el ISO 9660, encontrar dentro la imagen de arranque y copiarla al disco.

Parte "ide" (el caso del Amilo: el lector en el IDE/PATA, ATAPI por PIO):
  - la ISO (con terminal a pantalla completa, como la de h159) arranca con -cdrom y un disco
    vacío de 400 MB (hda);
  - el kernel registra cd0 (ISO 9660 "SAGUIOS") y cd0img (la imagen El Torito), reconoce
    cd0img como el disco de arranque (su cabecera es la del arranque) y monta /cd;
  - "mount" y "ls /cd" ven saguios.img; "sha256sum /cd/saguios.img" coincide con el
    archivo del anfitrión (lectura de 16 MB por el ATAPI y el ISO 9660);
  - "discos" e "infopc" muestran el CD; "instalar" toma cd0img como origen, no deja
    instalar en el CD, e "instalar hda -s" termina y verifica;
  - desde el anfitrión: la cabecera del disco instalado es la del CD sin OPC_CD y con el
    identificador de instalación; etapa 2, kernel y ramdisk iguales byte a byte;
  - se arranca SOLO del disco instalado (sin CD): es el disco de arranque, monta /home y
    /apps, la versión (uname -a) es la misma que desde el CD, y /home persiste.
Parte "ahci" (SATAPI): el mismo CD en un puerto del AHCI (ich9-ahci + ide-cd), arranque desde
  él, /cd montado y el archivo leído, e instalación en el disco IDE vacío (y arranque de él).
"""
import hashlib
import os
import re
import subprocess
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, extrair_particao, e2fsck, imagem_de_teste, RAIZ, BUILD, SAIDA, NOME_VERSAO  # noqa: E402

MB = 1024 * 1024
OPC_CD = 8


def construir_iso():
    """La ISO con terminal a pantalla completa (--console), como la de h159 parte C.
    D379 (M1): como la de serie, con el initramfs y la raíz (con /apps) dentro de la imagen El Torito (toram)."""
    os.makedirs(SAIDA, exist_ok=True)
    img = os.path.join(SAIDA, "h207-cd.img")
    iso = os.path.join(SAIDA, "h207.iso")
    shutil.copyfile(imagem_de_teste("", ["--vesa", "--console", "--cd"]), img)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img, "--saida", iso],
                   check=True, stdout=subprocess.DEVNULL)
    return img, iso


def vazio(nome, mb):
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(mb * MB)
    return img


def linha_versao(out):
    """La línea de "uname -a" (la que nombra el sistema), sin el eco ni el prompt."""
    versao = NOME_VERSAO.split()[-1]   # "v7.1.1"
    for l in out.splitlines():
        if l.startswith("SaguiOS ") and versao in l:   # "SaguiOS sagui v7.1.1 #imagen-N i686 SaguiOS"
            # "#imagen-N" cuenta también las particiones de juegos y apps (CAB_SIS_SETORES), que el CD no
            # tiene: no es la versión.
            return re.sub(r"#imagen-\d+ ", "", l.strip())
    return None


def esperar_fim(m, rotulo):
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 60
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, f"[{rotulo}] reiniciar: el QEMU sale (-no-reboot)")


def desde_cd(m, rotulo, img_cd, controlador):
    """Lo común con el sistema arrancado del CD: el lector, /cd y el instalador."""
    m.esperar_serial("sagui:/$", 240)
    s = m.serial()
    print("\n".join(l for l in s.splitlines() if "[ATAPI]" in l or "cd0" in l)[:3000])
    verificar(re.search(r"\[ATAPI\] " + controlador + r".*\(CD/DVD\)", s),
              f"[{rotulo}] el kernel ve el lector ATAPI por el {controlador} (IDENTIFY PACKET)")
    verificar(re.search(r"\[ATAPI\] .* es cd0: ISO 9660.*\"SAGUIOS\"", s), f"[{rotulo}] cd0: ISO 9660 con la etiqueta SAGUIOS")
    verificar("[ATAPI] cd0img: es la imagen de la que ha arrancado SaguiOS" in s,
              f"[{rotulo}] cd0img (El Torito) es el disco de arranque (cabecera igual a la del arranque)")
    verificar("cd0 (iso9660) montado em /cd" in s, f"[{rotulo}] el CD de arranque se monta en /cd al arrancar")
    out = m.comando("mount")
    verificar(re.search(r"cd0\s+en /cd\s+tipo iso9660\s+\(ro", out), f"[{rotulo}] mount: cd0 en /cd, iso9660, solo lectura")
    out = m.comando("ls /cd")
    verificar("saguios.img" in out, f"[{rotulo}] ls /cd muestra saguios.img (nombre 8.3 en minúsculas)")
    esperado = hashlib.sha256(open(img_cd, "rb").read()).hexdigest()
    out = m.comando("sha256sum /cd/saguios.img", 240)
    verificar(esperado in out, f"[{rotulo}] sha256sum /cd/saguios.img es el del archivo (16 MB leídos del CD)")
    m.comando("echo x > /cd/nuevo.txt")
    out = m.comando("ls /cd")
    verificar("nuevo.txt" not in out and "saguios.img" in out, f"[{rotulo}] en el CD no se escribe")
    out = m.comando("discos")
    verificar(re.search(r"CD/DVD.*\n.*cd0\s.*ISO 9660", out) and "cd0img" in out,
              f"[{rotulo}] discos muestra cd0 (ISO 9660) y cd0img")
    out = m.comando("infopc")
    verificar(re.search(r"cd0 \[CD\]: ATAPI", out), f"[{rotulo}] infopc muestra el lector como [CD]")
    versao = linha_versao(m.comando("uname -a"))
    out = m.comando("instalar")
    verificar("Disco de arranque (origen de la copia): cd0img (la imagen de arranque del CD)" in out,
              f"[{rotulo}] instalar toma como origen la imagen El Torito del CD")
    verificar(re.search(r"hda .*se puede instalar", out), f"[{rotulo}] hda se puede elegir")
    verificar(re.search(r"cd0 .*(CD/DVD|disco de arranque)", out) and not re.search(r"cd0\S* .*se puede instalar", out),
              f"[{rotulo}] en el CD no se puede instalar")
    out = m.comando("instalar cd0 -s")
    verificar("no se instala en cd0" in out, f"[{rotulo}] instalar cd0 se niega")
    out = m.comando("instalar hda -s", 1200)
    verificar("SaguiOS instalado en hda y verificado" in out, f"[{rotulo}] instalar hda termina y verifica ({out.strip()[-300:]})")
    verificar("saca el CD" in out, f"[{rotulo}] el final dice que se saque el CD")
    verificar(re.search(r"(Aplicaciones de /apps copiadas|Sistema copiado por ficheros a SAGUI-RAIZ \(\d+ MB, con /apps\)): [1-9]\d* archivos", out),
              f"[{rotulo}] se copian el sistema y las apps de /apps")
    return versao


def comprobar_disco(dest, img_cd, rotulo):
    with open(dest, "rb") as f:
        mbr = f.read(512)
        cab = f.read(512)
    with open(img_cd, "rb") as g:
        orig = g.read()
    verificar(cab[:4] == b"SGHD" and cab[144:148] == b"SGIN", f"[{rotulo}] cabecera con el identificador de instalación")
    op_o = int.from_bytes(orig[512 + 6:512 + 8], "little")
    op_d = int.from_bytes(cab[6:8], "little")
    verificar(op_o & OPC_CD and op_d == op_o & ~OPC_CD, f"[{rotulo}] la cabecera instalada es la del CD sin OPC_CD ({op_o:x} -> {op_d:x})")
    verificar(cab[8:136] == orig[512 + 8:512 + 136], f"[{rotulo}] el resto de la cabecera es la del CD")
    e2, total = int.from_bytes(cab[8:12], "little"), int.from_bytes(cab[48:52], "little")
    with open(dest, "rb") as f:
        f.seek(e2 * 512)
        copiado = f.read((total - e2) * 512)
    verificar(copiado == orig[e2 * 512:total * 512], f"[{rotulo}] etapa 2, kernel y ramdisk iguales byte a byte a los del CD")
    verificar(mbr[:440] == orig[:440] and mbr[510:512] == b"\x55\xAA", f"[{rotulo}] el código del MBR es el del CD")
    r = subprocess.run(["sfdisk", "-d", dest], capture_output=True, text=True).stdout
    t = {int(m.group(1)): (int(m.group(2)), int(m.group(3)), m.group(4))
         for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)", r)}
    verificar(t.get(1) and t[1][2] == "da" and t.get(5), f"[{rotulo}] tabla: SaguiOS (0xDA) y las lógicas de datos")
    home = extrair_particao(dest, t[5][0], t[5][1] // 2048)
    rc, o = e2fsck(home)
    verificar(rc == 0, f"[{rotulo}] e2fsck da por bueno el /home ({o.strip()[-120:]})")
    os.remove(home)


def arrancar_instalado(dest, rotulo, versao):
    with Maquina(f"h207-{rotulo}-hd", "ide", imagem=dest, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("es el disco de arranque (cabecera igual" in s, f"[{rotulo}] el disco instalado arranca y se reconoce como el de arranque")
        verificar("cd=0" in s, f"[{rotulo}] ya no arranca como CD (OPC_CD fuera de la cabecera)")
        out = m.comando("mount")
        verificar(re.search(r"\s/home\s", out) and (re.search(r"\s/apps\s", out) or re.search(r"en /\s+tipo ext2\s+\(rw", out)),
                  f"[{rotulo}] /home montado (por etiqueta) y /apps (partición, o dentro de la raíz en disco, D379)")
        v2 = linha_versao(m.comando("uname -a"))
        verificar(versao and versao == v2, f"[{rotulo}] la versión es la misma que desde el CD ({v2!r})")
        out = m.comando("which sqlite3")
        verificar("/apps/bin/sqlite3" in out, f"[{rotulo}] las apps copiadas a /apps funcionan (which sqlite3)")
        m.comando(f"echo persistente {rotulo} > /home/h207.txt")
        esperar_fim(m, rotulo)
    with Maquina(f"h207-{rotulo}-hd2", "ide", imagem=dest, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 240)
        out = m.comando("cat /home/h207.txt")
        verificar(f"persistente {rotulo}" in out, f"[{rotulo}] /home persiste entre dos arranques del disco instalado")


def parte_ide(img_cd, iso):
    dest = vazio("h207-ide-dest", 400)
    extras = ["-drive", f"file={dest},if=ide,index=0,format=raw"]
    with Maquina("h207-ide", "cd", imagem=iso, extras=extras) as m:
        versao = desde_cd(m, "ide", img_cd, "IDE")
        m.captura("H207-1-instalar-desde-cd")
    comprobar_disco(dest, img_cd, "ide")
    arrancar_instalado(dest, "ide", versao)
    os.remove(dest)


def parte_ahci(img_cd, iso):
    dest = vazio("h207-ahci-dest", 400)
    # El disco vacío en el IDE (hda) y el CD en el AHCI, que es de donde arranca la BIOS.
    extras = ["-device", "ich9-ahci,id=ahci",
              "-drive", f"if=none,id=cdahci,file={iso},media=cdrom,format=raw,readonly=on",
              "-device", "ide-cd,bus=ahci.0,drive=cdahci,bootindex=0"]
    with Maquina("h207-ahci", "ide", imagem=dest, snapshot=False, extras=extras) as m:
        versao = desde_cd(m, "ahci", img_cd, "AHCI")
    comprobar_disco(dest, img_cd, "ahci")
    arrancar_instalado(dest, "ahci", versao)
    os.remove(dest)


def main():
    partes = sys.argv[1:] or ["ide", "ahci"]
    img_cd, iso = construir_iso()
    try:
        if "ide" in partes:
            parte_ide(img_cd, iso)
        if "ahci" in partes:
            parte_ahci(img_cd, iso)
    finally:
        for f in (img_cd, iso):
            if os.path.exists(f):
                os.remove(f)
    print("H207: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
