#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H270 - UEFI (D264) con el instalador GPT/ESP (D266): discos instalados que arrancan por UEFI (D270).

Criterio:
  - el ramdisk lleva el cargador UEFI en /sistema/efi/EFI/BOOT (los mismos BOOTX64.EFI y BOOTIA32.EFI
    de la ESP de la imagen) y el MBR de SaguiOS en /sistema/arranque/mbr.bin;
  - modo automático: arrancado por BIOS, "instalar hdb -s" crea un MBR; arrancado por UEFI (OVMF),
    sobre ese mismo disco, crea una GPT (ESP + SaguiOS + /home + SAGUI-CONF) y conserva SAGUI-CONF;
    la ESP lleva el cargador del ramdisk;
  - ese disco GPT, solo, arranca con OVMF: el cargador lee la cabecera del primer sector de la
    partición de SaguiOS y el sistema del disco; el kernel sabe que es su disco de arranque, monta
    SAGUI-CONF (el fichero puesto en ella está en /etc) y "pantalla 800x600 --guardar" graba la
    cabecera de la partición, que el cargador usa en el arranque siguiente (x64, hasta el escritorio,
    y IA32);
  - desde ese disco GPT, "instalar hdb --mbr" (el segundo disco) hace un disco MBR (con el MBR del ramdisk) que arranca
    por BIOS;
  - NVMe: instalar en nvme0 desde el arranque UEFI (GPT automática) y arrancar el NVMe con OVMF;
  - SMP por UEFI: imagen --smp con OVMF y 4 CPU: arrancan los secundarios;
  - la variante persistente (--dados-mb) con --uefi arranca por UEFI y monta /home;
  - la ISO (mkiso.py --efi) arranca por UEFI x64 e IA32 (El Torito, sección 0xEF con EFI.IMG; el
    cargador lee el sistema de la imagen de disco de dentro del CD) y sigue arrancando por BIOS.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, extrair_particao, achar_ovmf, FalhaTeste,  # noqa: E402
                  BUILD, SAIDA)
from h266_gpt_esp import ler_gpt, sgdisk_ok, disco_vazio, ESP, SAGUI, LINUX, ENV_MT  # noqa: E402

UEFI_DIR = os.path.join(BUILD, "boot", "uefi")
DEBUGFS = "/usr/sbin/debugfs"
OPC_CONSOLE = 2
# OVMF pone las BAR de 64 bits de los dispositivos del bus raíz (el NVMe de QEMU) por encima de 4 GB. Desde D283
# el kernel las recoloca por debajo de 4 GB (kernel/dispositivos/pci_realoc.c): ya no hace falta apagar la ventana
# de 64 bits de OVMF (antes, X-PciMmio64Mb=0).
NVME_FW = []
SECRETO = b"h270: SAGUI-CONF conservada en el paso de MBR a GPT\n"


def ler(img, setor, n=1):
    with open(img, "rb") as f:
        f.seek(setor * 512)
        return f.read(n * 512)


def escrever(img, setor, dados):
    with open(img, "r+b") as f:
        f.seek(setor * 512)
        f.write(dados)


def particoes(img):
    """(inicio, setores) de cada partición (MBR con lógicas o GPT), con sfdisk."""
    import json
    r = subprocess.run(["sfdisk", "--json", img], capture_output=True, text=True)
    t = json.loads(r.stdout)["partitiontable"]
    return [(p["start"], p["size"]) for p in t["partitions"]]


def achar_rotulo(img, rotulo):
    for ini, qtd in particoes(img):
        sb = ler(img, ini + 2)
        if sb[56:58] == b"\x53\xef" and sb[120:136].split(b"\0")[0] == rotulo.encode():
            return ini, qtd
    return None


def achar_conf(img):
    """D379 (M1): la configuración del sistema instalado: SAGUI-CONF, o /etc dentro de SAGUI-RAIZ."""
    return achar_rotulo(img, "SAGUI-CONF") or achar_rotulo(img, "SAGUI-RAIZ")


def conf_escrever(img, nome, conteudo):
    ini, qtd = achar_conf(img)
    p = extrair_particao(img, ini, qtd // 2048, os.path.join(SAIDA, "h270-conf.part"))
    tmp = os.path.join(SAIDA, "h270-conf.tmp")
    with open(tmp, "wb") as f:
        f.write(conteudo)
    subprocess.run([DEBUGFS, "-w", "-R", "mkdir etc", p], capture_output=True)
    subprocess.run([DEBUGFS, "-w", "-R", f"write {tmp} {nome}", p], check=True, capture_output=True)
    subprocess.run(["dd", f"of={img}", f"if={p}", "bs=512", f"seek={ini}", "conv=notrunc", "status=none"], check=True)


def conf_ler(img, nome):
    ini, qtd = achar_conf(img)
    p = extrair_particao(img, ini, qtd // 2048, os.path.join(SAIDA, "h270-conf-r.part"))
    return subprocess.run([DEBUGFS, "-R", f"cat {nome}", p], capture_output=True).stdout


def cabecera_gpt(img):
    """El sector de la cabecera de arranque en un disco GPT instalado (primer sector de la partición SaguiOS)."""
    ents, _ = ler_gpt(img)
    s = [e for e in ents if e["tipo"] == SAGUI]
    verificar(len(s) == 1, "hay una partición de tipo SaguiOS en la GPT")
    return s[0]["ini"]


def quitar_console(img, setor):
    cab = bytearray(ler(img, setor))
    op = struct.unpack_from("<H", cab, 6)[0]
    struct.pack_into("<H", cab, 6, op & ~OPC_CONSOLE)
    escrever(img, setor, bytes(cab))


def esperar_escritorio(m, rot, tempo=240):
    fim = time.time() + tempo
    while "area de trabalho no ar" not in m.serial():
        if "no puede arrancar por UEFI" in m.serial() or time.time() > fim:
            print(m.serial()[-3000:])
            raise FalhaTeste(f"{rot}: el cargador UEFI se paró o no se llegó al escritorio")
        time.sleep(0.5)


def esperar_prompt(m, rot, tempo=240):
    fim = time.time() + tempo
    while "sagui:/$" not in m.serial():
        if "no puede arrancar por UEFI" in m.serial() or time.time() > fim:
            print(m.serial()[-3000:])
            raise FalhaTeste(f"{rot}: no se llegó al terminal")
        time.sleep(0.5)


def conferir_ramdisk():
    for nome in ("BOOTX64.EFI", "BOOTIA32.EFI"):
        a = os.path.join(BUILD, "rootfs", "sistema", "efi", "EFI", "BOOT", nome)
        verificar(os.path.isfile(a) and open(a, "rb").read() == open(os.path.join(UEFI_DIR, nome), "rb").read(),
                  f"el ramdisk lleva /sistema/efi/EFI/BOOT/{nome} (el mismo de la ESP)")
    m = os.path.join(BUILD, "rootfs", "sistema", "arranque", "mbr.bin")
    verificar(os.path.isfile(m) and open(m, "rb").read() == open(os.path.join(BUILD, "boot", "mbr.bin"), "rb").read(),
              "el ramdisk lleva el MBR de SaguiOS en /sistema/arranque/mbr.bin")


def gpt_desde_uefi(origem):
    """BIOS -> MBR automático; luego UEFI -> GPT automática sobre el mismo disco, conservando SAGUI-CONF."""
    dest = disco_vazio("h270-dest", 1536)
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h270-bios-auto", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar hdb --sin-jogos --sin-apps -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"[BIOS] instalar termina ({out.strip()[-200:]})")
        verificar("Tabla de particiones: MBR" in out and "automática: se arrancó por BIOS" in out,
                  "[BIOS] el modo automático elige MBR al arrancar por BIOS")
        verificar("con el cargador UEFI de /sistema/efi" not in out, "[BIOS] sin --esp no hay partición EFI")
    verificar(ler(dest, 1)[:4] == b"SGHD", "[BIOS] el disco MBR tiene la cabecera en el LBA 1")
    conf_escrever(dest, "etc/prueba-h270.txt", SECRETO)
    verificar(conf_ler(dest, "/etc/prueba-h270.txt") == SECRETO, "el fichero de prueba está en la SAGUI-CONF del disco MBR")
    with Maquina("h270-uefi-auto", "ide", imagem=origem, extras=extras, firmware="x64") as m:
        esperar_prompt(m, "[UEFI] origen")
        verificar("firmware=uefi-x64" in m.serial(), "[UEFI] el origen arrancó por UEFI x64")
        # D379 (M1): reinstalar conserva /home en su sitio; de MBR a GPT no cabe (la ESP va antes): se pide borrarla
        out = m.comando("instalar hdb -s", 300)
        verificar("no deja sitio en su lugar" in out and "--formatear-home" in out,
                  f"[UEFI] sin --formatear-home no se borra el /home de la instalación anterior ({out.strip()[-300:]})")
        out = m.comando("instalar hdb --formatear-home -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"[UEFI] instalar termina ({out.strip()[-300:]})")
        verificar("Tabla de particiones: GPT" in out and "automática: se arrancó por UEFI" in out,
                  "[UEFI] el modo automático elige GPT al arrancar por UEFI")
        verificar("con el cargador UEFI de /sistema/efi" in out and "AVISO: no hay cargador" not in out,
                  "[UEFI] la ESP lleva el cargador del ramdisk")
        verificar("contenido anterior conservado" in out or "Configuración de la instalación anterior conservada" in out,
                  "[UEFI] se conserva SAGUI-CONF (de MBR a GPT; D379: la configuración de la raíz instalada)")
    ents, _ = ler_gpt(dest)
    verificar(sgdisk_ok(dest)[0], "la GPT creada es válida (sgdisk -v)")
    verificar([e["tipo"] for e in ents] == [ESP, SAGUI, LINUX, LINUX], "GPT: ESP, SaguiOS, /home y SAGUI-CONF")
    p = extrair_particao(dest, ents[0]["ini"], ents[0]["setores"] // 2048, os.path.join(SAIDA, "h270-esp.part"))
    for nome in ("BOOTX64.EFI", "BOOTIA32.EFI"):
        out = subprocess.run(["mtype", "-i", p, f"::/EFI/BOOT/{nome}"], capture_output=True, env=ENV_MT).stdout
        verificar(out == open(os.path.join(UEFI_DIR, nome), "rb").read(), f"la ESP instalada tiene EFI/BOOT/{nome} (el del build)")
    lista = subprocess.run(["mdir", "-i", p, "-/", "-b", "::/"], capture_output=True, env=ENV_MT, text=True).stdout
    verificar("SAGUI" not in lista.upper().replace("EFI/BOOT", ""), "la ESP instalada no lleva copia del sistema (va en su partición)")
    verificar(conf_ler(dest, "/etc/prueba-h270.txt") == SECRETO, "la SAGUI-CONF de la GPT conserva el fichero byte a byte")
    return dest


def arrancar_gpt(dest):
    setor = cabecera_gpt(dest)
    # 1) en modo terminal (la cabecera viene de la imagen de origen, --console): disco, SAGUI-CONF y pantalla --guardar
    terceiro = disco_vazio("h270-mbr-desde-gpt", 700)
    with Maquina("h270-gpt-term", "ide", imagem=dest, firmware="x64", snapshot=False,
                 extras=["-drive", f"file={terceiro},if=ide,index=2,format=raw"]) as m:
        esperar_prompt(m, "[GPT x64]")
        s = m.serial()
        verificar(f"la de la particion de SaguiOS (GPT, sector {setor})" in s and "sistema leido del disco" in s,
                  f"[GPT x64] el cargador lee la cabecera del sector {setor} y el sistema del disco")
        verificar(re.search(r"\[DISCO\] hda: es el disco de arranque \(GPT: cabecera en hda\d, sector %d\)" % setor, s) is not None,
                  "[GPT x64] el kernel reconoce su disco de arranque (cabecera en la partición GPT)")
        verificar("[CONF] SAGUI-CONF en" in s or "[CONF] /root en la raiz del disco" in s,
                  "[GPT x64] SAGUI-CONF montada (D379: o fundida en la raíz instalada)")
        out = m.comando("cat /etc/prueba-h270.txt")
        verificar("conservada en el paso de MBR a GPT" in out, "[GPT x64] el fichero de SAGUI-CONF está en /etc")
        out = m.comando("discos")
        verificar("SaguiOS" in out, f"discos muestra la partición de SaguiOS ({out.strip()[-200:]})")
        out = m.comando("pantalla 800x600 --guardar", 60)
        print(out)
        # D270: instalar desde un disco GPT (cabecera fuera del LBA 1) a un disco MBR
        out = m.comando("instalar hdb --mbr --sin-jogos --sin-apps -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"[GPT x64] instalar desde el disco GPT a un MBR termina ({out.strip()[-300:]})")
    cab = ler(dest, setor)
    verificar(struct.unpack_from("<HH", cab, 128) == (800, 600), "pantalla --guardar grabó 800x600 en la cabecera de la partición GPT")
    verificar(ler(dest, 1)[:8] == b"EFI PART" and sgdisk_ok(dest)[0], "la GPT sigue intacta después de guardar")
    verificar(ler(terceiro, 1)[:4] == b"SGHD" and ler(terceiro, 0)[:440] == open(os.path.join(BUILD, "boot", "mbr.bin"), "rb").read()[:440],
              "el disco MBR hecho desde el GPT lleva la cabecera en el LBA 1 y el MBR de SaguiOS")
    with Maquina("h270-mbr-bios", "ide", imagem=terceiro) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar("es el disco de arranque (cabecera igual" in m.serial(), "el disco MBR hecho desde el GPT arranca por BIOS y se reconoce")
    # 2) escritorio por UEFI x64 (con la resolución guardada) e IA32
    quitar_console(dest, setor)
    for fw in ("x64", "ia32"):
        if not achar_ovmf(fw):
            print(f"AVISO: sin OVMF {fw}: se salta")
            continue
        with Maquina(f"h270-gpt-{fw}", "ide", imagem=dest, firmware=fw) as m:
            esperar_escritorio(m, f"[GPT {fw}]")
            s = m.serial()
            verificar(f"firmware=uefi-{fw}" in s, f"[GPT {fw}] arranca por UEFI {fw} hasta el escritorio")
            verificar("es el disco de arranque (GPT" in s, f"[GPT {fw}] el kernel reconoce su disco")
            verificar(re.search(r"gop=800x600", s) is not None, f"[GPT {fw}] el cargador usa la resolución guardada (800x600)")
            m.captura(f"H270-gpt-{fw}", guardar=(fw == "x64"))


def nvme(origem):
    nv = disco_vazio("h270-nvme", 800)
    extras = ["-drive", f"file={nv},if=none,id=nv0,format=raw", "-device", "nvme,drive=nv0,serial=h270nvme"] + NVME_FW
    with Maquina("h270-nvme-inst", "ide", imagem=origem, extras=extras, firmware="x64") as m:
        esperar_prompt(m, "[NVMe] origen")
        out = m.comando("instalar nvme0 -s", 1500)
        verificar("SaguiOS instalado en nvme0 y verificado" in out and "Tabla de particiones: GPT" in out,
                  f"[NVMe] instalar en nvme0 por UEFI: GPT automática ({out.strip()[-300:]})")
    setor = cabecera_gpt(nv)
    verificar(sgdisk_ok(nv)[0], "[NVMe] la GPT del NVMe es válida")
    quitar_console(nv, setor)
    vazio = disco_vazio("h270-vazio", 2)
    extras = ["-drive", f"file={nv},if=none,id=nv0,format=raw", "-device", "nvme,drive=nv0,serial=h270nvme,bootindex=0"] + NVME_FW
    with Maquina("h270-nvme-boot", "ide", imagem=vazio, extras=extras, firmware="x64") as m:
        esperar_escritorio(m, "[NVMe boot]")
        s = m.serial()
        verificar("es el disco de arranque (GPT: cabecera en nvme0p" in s, "[NVMe] arranca del NVMe por UEFI y el kernel lo reconoce")
        verificar("[CONF] SAGUI-CONF en" in s or "[CONF] /root en la raiz del disco" in s,
                  "[NVMe] SAGUI-CONF del NVMe montada (D379: o fundida en la raíz instalada)")


def smp_uefi():
    img = imagem_de_teste("", ["--vesa", "--smp", "--uefi", UEFI_DIR])
    with Maquina("h270-smp", "ide", imagem=img, firmware="x64", nucleos=4) as m:
        esperar_escritorio(m, "[SMP UEFI]")
        m.esperar_serial("[GUI] relogio:", 30)
        s = m.serial()
        for i in (1, 2, 3):
            verificar(f"CPU {i} arrancou" in s, f"[SMP UEFI] el procesador secundario {i} arrancó")
        verificar("EXCECAO" not in s, "[SMP UEFI] sin excepciones")


def persistente_uefi():
    img = imagem_de_teste("", ["--vesa", "--uefi", UEFI_DIR, "--dados-mb", "64"])
    with Maquina("h270-pers", "ide", imagem=img, firmware="x64") as m:
        esperar_escritorio(m, "[persistente UEFI]")
        s = m.serial()
        verificar("la del disco (LBA 1)" in s, "[persistente UEFI] arranca por UEFI con la cabecera del LBA 1")
        verificar(re.search(r"\[MONTAGEM\] \S+ \(ext2\) montado em /home", s) is not None, "[persistente UEFI] /home montado")


def iso_uefi():
    from h207_instalar_cd import construir_iso  # noqa: E402
    img, _ = construir_iso()
    iso = os.path.join(SAIDA, "h270.iso")
    subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tools", "mkiso.py"),
                    "--imagem", img, "--saida", iso, "--efi", UEFI_DIR], check=True, stdout=subprocess.DEVNULL)
    r = subprocess.run(["xorriso", "-indev", iso, "-report_el_torito", "plain"], capture_output=True, text=True) \
        if subprocess.run(["which", "xorriso"], capture_output=True).returncode == 0 else None
    if r is not None:
        print(r.stdout + r.stderr)
        verificar("UEFI" in r.stdout and "BIOS" in r.stdout, "xorriso ve las dos entradas de El Torito (BIOS y UEFI)")
    for fw in ("x64", "ia32", None):
        if fw and not achar_ovmf(fw):
            continue
        rot = f"[ISO {fw or 'BIOS'}]"
        with Maquina(f"h270-iso-{fw or 'bios'}", "cd", imagem=iso, firmware=fw) as m:
            esperar_prompt(m, rot)
            s = m.serial()
            if fw:
                verificar("la de la imagen del CD (El Torito)" in s and f"firmware=uefi-{fw}" in s,
                          f"{rot} el cargador UEFI lee la cabecera y el sistema de la imagen El Torito")
            verificar("[ATAPI] cd0img: es la imagen de la que ha arrancado SaguiOS" in s, f"{rot} cd0img es el disco de arranque")
            verificar("cd0 (iso9660) montado em /cd" in s, f"{rot} el CD se monta en /cd")


def main():
    if not achar_ovmf("x64"):
        print("AVISO: sin OVMF x64 (bash tools/entorno-local.sh): se salta H270")
        return 0
    conferir_ramdisk()
    origem = imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI_DIR])
    dest = gpt_desde_uefi(origem)
    arrancar_gpt(dest)
    nvme(origem)
    smp_uefi()
    persistente_uefi()
    iso_uefi()
    return 0


if __name__ == "__main__":
    sys.exit(main())
