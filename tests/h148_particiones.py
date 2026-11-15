#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H148 (D179) - gestión de particiones: MBR con extendida y lógicas, formateo, deshacer, GPT y TRIM.

Parte 1 (terminal, IDE): un segundo disco vacío (hdb) y uno GPT (hdc).
  - crear primaria FAT32, extendida, lógica ext2 y lógica FAT32; marcar activa;
  - redimensionar: un ext2 encoge y vuelve a crecer con su partición (D213); agrandar un FAT32 vale;
  - crear y borrar una primaria, deshacer (vuelve) y borrar otra vez;
  - una partición montada no se toca (D213: el resto del disco sí); el disco de arranque, sin --arranque, tampoco;
  - la GPT se muestra y no se toca salvo que se pida crear, borrar o formatear (D266, h266): aquí solo se leen
    y se rechaza redimensionar (el fichero no cambia ni un byte);
  - desde el anfitrión: la tabla con sfdisk, los FAT32 con fsck.vfat y mtools, el ext2 con e2fsck,
    y los archivos que SaguiOS escribió en ellos.
Parte 2 (AHCI con discard=unmap): el disco empieza lleno de 0xAB; formatear manda TRIM (la zona
  de datos vuelve a cero en el anfitrión) y "particiones trim" descarta el espacio libre; lo que no
  es de nadie (el primer MB) no se toca.
Parte 3 (escritorio): la vista Particiones de la ventana Discos (capturas H148-*), borrar desde el
  botón con su diálogo y deshacer desde el botón.
"""
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, SAIDA  # noqa: E402

MB = 1024 * 1024


def sfdisk(img):
    r = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True).stdout
    t = {}
    for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)(, bootable)?", r):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), m.group(4), bool(m.group(5)))
    return t


def fsck_vfat(part):
    r = subprocess.run(["fsck.vfat", "-n", part], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def mtype(part, caminho):
    r = subprocess.run(["mtype", "-i", part, "::" + caminho], capture_output=True, text=True,
                       env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    return r.stdout


def sha(f):
    return hashlib.sha256(open(f, "rb").read()).hexdigest()


def parte1():
    hdb = os.path.join(SAIDA, "h148-hdb.img")
    with open(hdb, "wb") as f:
        f.truncate(256 * MB)
    gpt = os.path.join(SAIDA, "h148-gpt.img")
    with open(gpt, "wb") as f:
        f.truncate(64 * MB)
    subprocess.run(["sfdisk", "-q", gpt], input="label: gpt\nstart=2048, size=40960, type=L, name=\"datos linux\"\n"
                   "start=43008, size=20480, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7\n", text=True, check=True)
    hash_gpt = sha(gpt)
    extras = ["-drive", f"file={hdb},if=ide,index=1,format=raw", "-drive", f"file={gpt},if=ide,index=2,format=raw"]
    with Maquina("h148-ide", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar(re.search(r"\[DISCO\] hda: es el disco de arranque", m.serial()), "el kernel reconoce el disco de arranque por la cabecera")
        out = m.comando("particiones hdb")
        verificar("sin tabla" in out, "un disco vacío se ve sin tabla")
        out = m.comando("particiones crear hdb 64 fat32 --etiqueta DATOS -s", 300)
        verificar("creada hdb1: 64 MB" in out and "formateada en FAT32" in out, "crear una primaria FAT32 de 64 MB")
        out = m.comando("particiones crear hdb 100 extendida -s")
        verificar("creada hdb2: 100 MB, extendida" in out, "crear una extendida de 100 MB")
        out = m.comando("particiones crear hdb 40 ext2 --logica --etiqueta LINUX -s", 300)
        verificar("creada hdb5: 40 MB" in out and "ext2" in out, "crear una lógica ext2 (hdb5)")
        out = m.comando("particiones crear hdb 34 fat32 --logica -s", 300)
        verificar("creada hdb6: 34 MB" in out, "crear una segunda lógica FAT32 (hdb6)")
        out = m.comando("particiones activa hdb1")
        verificar("es ahora la activa" in out, "marcar activa la hdb1")
        out = m.comando("particiones redimensionar hdb5 20 -s", 120)
        verificar("hdb5 pasa de 40 a 20 MB" in out and "el ext2 de hdb5 pasa de" in out, f"D213: el ext2 encoge con su partición ({out.strip()[-160:]})")
        out = m.comando("particiones redimensionar hdb5 40 -s", 120)
        verificar("hdb5 pasa de 20 a 40 MB" in out and "el ext2 de hdb5 pasa de" in out, f"D213: y vuelve a crecer ({out.strip()[-160:]})")
        out = m.comando("particiones redimensionar hdb6 50 -s", 120)
        verificar("hdb6 pasa de 34 a 50 MB" in out, f"agrandar la lógica FAT32 de 34 a 50 MB ({out.strip()[-160:]})")
        out = m.comando("particiones crear hdb 30 -s")
        verificar("creada hdb3: 30 MB" in out, "crear una primaria sin formato (hdb3)")
        out = m.comando("particiones borrar hdb3 -s")
        verificar("borrada hdb3" in out, "borrar la hdb3")
        verificar(not re.search(r"3\s+hdb3", m.comando("particiones hdb")), "la hdb3 ya no está en la tabla")
        out = m.comando("particiones deshacer hdb")
        verificar("deshecho" in out, "deshacer el borrado")
        out = m.comando("particiones hdb")
        verificar(re.search(r"3\s+hdb3", out), "deshacer: la hdb3 vuelve a la tabla")
        out = m.comando("particiones borrar hdb3 -s")
        verificar("borrada hdb3" in out, "borrar la hdb3 otra vez")
        # Montado: se niega. Y lo escrito por SaguiOS en los volúmenes nuevos.
        m.comando("mount -o rw hdb1 /m1")
        m.comando("echo escrito por SaguiOS > /m1/hola.txt")
        out = m.comando("particiones borrar hdb1 -s")
        verificar("está montada en /m1" in out and "borrada" not in out, "una partición montada no se borra (D213)")
        m.comando("umount /m1")
        m.comando("mount -o rw hdb5 /m5")
        m.comando("echo en el ext2 nuevo > /m5/hola.txt")
        m.comando("umount /m5")
        out = m.comando("particiones crear hda 10 -s")
        verificar("disco de arranque" in out and "creada" not in out, "el disco de arranque se niega sin --arranque")
        # GPT
        out = m.comando("particiones hdc")
        verificar("GPT" in out and "datos linux" in out and "datos de Windows" in out,
                  "la GPT se muestra con sus dos entradas, su tipo y su nombre")
        # D266: en una GPT se puede crear, borrar y formatear (h266); lo demás sigue sin estar disponible
        out = m.comando("particiones redimensionar hdc1 20 -s")
        verificar("GPT" in out and "no está disponible" in out and "pasa de" not in out, "en un disco GPT no se redimensiona (D266: solo crear, borrar y formatear)")
        m.captura("H148-1-terminal")
        out = m.comando("particiones hdb")
        print(out)
        verificar("gravacao recusada" not in m.serial(), "ninguna escritura rechazada por el cerrojo de D040")
    t = sfdisk(hdb)
    print("tabla:", t)
    verificar(t.get(1) == (2048, 131072, "c", True), "anfitrión: hdb1 FAT32 de 64 MB en 1 MB, activa")
    verificar(t.get(2) and t[2][0] == 133120 and t[2][1] == 204800 and t[2][2] == "f", "anfitrión: hdb2 extendida de 100 MB")
    verificar(t.get(5) and t[5][0] == 135168 and t[5][1] == 81920 and t[5][2] == "83", "anfitrión: hdb5 lógica Linux de 40 MB")
    verificar(t.get(6) and t[6][1] == 102400 and t[6][2] == "c", "anfitrión: hdb6 lógica FAT32, ya de 50 MB")
    verificar(3 not in t and 4 not in t, "anfitrión: la hdb3 borrada no está")
    p1 = extrair_particao(hdb, 2048, 64)
    rc, o = fsck_vfat(p1)
    verificar(rc == 0, f"fsck.vfat da por bueno el FAT32 de hdb1 ({o.strip()[-120:]})")
    verificar("escrito por SaguiOS" in mtype(p1, "/hola.txt"), "mtools lee lo que SaguiOS escribió en hdb1")
    p6 = extrair_particao(hdb, t[6][0], 50)
    rc, o = fsck_vfat(p6)
    verificar(rc == 0, f"fsck.vfat da por bueno el FAT32 agrandado de hdb6 ({o.strip()[-120:]})")
    p5 = extrair_particao(hdb, 135168, 40)
    rc, o = e2fsck(p5)
    verificar(rc == 0, f"e2fsck -fn da por bueno el ext2 de hdb5 ({o.strip()[-160:]})")
    r = subprocess.run(["/usr/sbin/debugfs", "-R", "cat /hola.txt", p5], capture_output=True, text=True).stdout
    verificar("en el ext2 nuevo" in r, "debugfs lee lo que SaguiOS escribió en el ext2 formateado por SaguiOS")
    verificar(sha(gpt) == hash_gpt, "la GPT no ha cambiado ni un byte")


def parte2():
    sata = os.path.join(SAIDA, "h148-ssd.img")
    with open(sata, "wb") as f:
        bloco = b"\xAB" * MB
        for _ in range(128):
            f.write(bloco)
    extras = ["-device", "ich9-ahci,id=ahci",
              "-drive", f"if=none,id=ssd,format=raw,file={sata},discard=unmap",
              "-device", "ide-hd,drive=ssd,bus=ahci.0"]
    with Maquina("h148-trim", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar(re.search(r"\[AHCI\] puerto 0: .*TRIM", m.serial()), "el disco AHCI dice que admite TRIM (IDENTIFY 169)")
        out = m.comando("particiones hdb")
        verificar("TRIM: sí" in out, "particiones muestra TRIM: sí")
        out = m.comando("particiones crear hdb 64 fat32 --etiqueta SSD -s", 300)
        verificar("creada hdb1" in out, "crear un FAT32 en el disco AHCI")
        s = m.serial()
        verificar(re.search(r"\[BLOCOS\] hdb1: TRIM de \d+ sectores desde \d+: 0", s), "formatear manda TRIM y el disco lo acepta")
        out = m.comando("particiones trim hdb -s")
        verificar("descartado (TRIM) el espacio libre de hdb" in out, "particiones trim descarta el espacio libre")
        verificar("comando 6 falló" not in m.serial(), "ningún DATA SET MANAGEMENT falló")
    with open(sata, "rb") as f:
        f.seek(512 * 1024)                 # dentro del primer MB, de nadie: no se toca
        verificar(f.read(4096) == b"\xAB" * 4096, "el hueco antes de la primera partición no se ha descartado")
        f.seek(1 * MB + 40 * MB)           # zona de datos del FAT32: descartada al formatear
        verificar(f.read(65536) == b"\0" * 65536, "la zona de datos del FAT32 vuelve a cero (TRIM al formatear)")
        f.seek(100 * MB)                   # espacio libre: descartado con particiones trim
        verificar(f.read(65536) == b"\0" * 65536, "el espacio libre vuelve a cero (particiones trim)")
    rc, o = fsck_vfat(extrair_particao(sata, 2048, 64))
    verificar(rc == 0, "el FAT32 del disco AHCI está bien")


def parte3():
    hd, ini = criar_disco("h148-gui", [dict(tipo="fat32", mb=64, rotulo="DATOS", arquivos={"a.txt": "a\n"}),
                                       dict(tipo="ext2", mb=40, rotulo="LINUX")], mb=300)
    extras = ["-drive", f"file={hd},if=ide,index=1,format=raw"]
    with Maquina("h148-gui", "ide", extras=extras, snapshot=True) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        m.digitar("discos --particiones hdb\n", 0.04)
        m.esperar_serial("discos: botones", 30)
        time.sleep(1.5)
        s = m.serial()
        jan = re.findall(r'criada por \d+: "Discos" (\d+)x(\d+) em \((\d+),(\d+)\)', s)[-1]
        jl, ja, jx, jy = (int(v) for v in jan)
        ox, oy = jx + 2, jy + 24
        segs = {int(g.group(2)): (int(g.group(3)), int(g.group(4)), int(g.group(5)))
                for g in re.finditer(r"discos: seg (\d+) num=(\d+) x=(\d+) l=(\d+) y=(\d+)", s)}
        bt = re.findall(r"discos: botones y=(\d+) x0=(\d+) paso=(\d+)", s)[-1]
        by, bx0, passo = (int(v) for v in bt)
        verificar(1 in segs and 2 in segs and 0 in segs, f"la barra tiene hdb1, hdb2 y el espacio libre ({segs})")
        m.captura("H148-2-vista-particiones")
        x, l, y = segs[2]
        m.clique(ox + x + l // 2, oy + y + 20)
        m.esperar_serial("discos: particion elegida", 10)
        m.clique(ox + x + l // 2, oy + y + 20, botao=2)
        time.sleep(1)
        m.captura("H148-3-menu-contextual")
        m.tecla("esc")
        time.sleep(0.5)
        m.clique(ox + bx0 + 1 * passo + 40, oy + by + 12)     # Borrar
        time.sleep(1.5)
        m.captura("H148-4-confirmar-borrar")
        m.tecla("ret")
        m.esperar_serial("discos: particiones op 1 en hdb -> 0", 60)
        time.sleep(1.5)
        m.captura("H148-5-borrada")
        verificar("borrada hdb2" in m.serial(), "el botón Borrar, tras el diálogo, borra la hdb2")
        m.clique(ox + bx0 + 6 * passo + 40, oy + by + 12)     # Deshacer
        time.sleep(1.5)
        m.tecla("ret")
        m.esperar_serial("discos: particiones op 6 en hdb -> 0", 60)
        time.sleep(1.5)
        s = m.serial()
        ult = s[s.rindex("op 6 en hdb"):]
        verificar(re.search(r"discos: seg \d+ num=2 ", ult), "Deshacer: la hdb2 vuelve a la barra")
        m.captura("H148-6-deshecho")


def main():
    parte1()
    parte2()
    parte3()
    return 0


if __name__ == "__main__":
    sys.exit(main())
