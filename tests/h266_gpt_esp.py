#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H266 (D266, mejora 062) - el instalador y "particiones" con GPT y partición EFI (ESP).

Parte 1 (instalar --gpt): en un disco vacío de 1,5 GB, con un "cargador UEFI" falso en /sistema/efi
  (un FAT32 de prueba montado encima del de verdad que lleva el ramdisk, D270), "instalar hdb --gpt -s" crea ESP (256 MB FAT32), SaguiOS, /home y
  SAGUI-CONF. Desde el anfitrión se comprueba con Python (CRC de las dos cabeceras y de las entradas,
  copia de respaldo al final, MBR protector, GUID de tipo y únicos, alineación a 1 MiB, sin solapes),
  con sgdisk -v y sfdisk --json; la ESP es un FAT32 sano con EFI/BOOT/BOOTX64.EFI igual que el original;
  el sistema (cabecera de arranque + etapa 2, kernel y ramdisk) está en su partición, byte a byte.
Parte 2 (reinstalar): se pone un fichero en SAGUI-CONF desde el anfitrión; reinstalar con --gpt lo conserva;
  reinstalar con --mbr --esp sobre el disco GPT (GPT -> MBR con partición EFI 0xEF) lo conserva también.
Parte 3 (GPT ajena): un disco GPT hecho por el anfitrión con sgdisk (ESP, Linux, datos de Windows) se lee
  en SaguiOS sin tocarlo (el hash del disco no cambia); después se borra y se crea una partición y el
  resultado sigue siendo una GPT válida con lo demás intacto; con la cabecera principal dañada se lee el
  respaldo (y solo se avisa).
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, SAIDA)  # noqa: E402

MB = 1024 * 1024
DEBUGFS = "/usr/sbin/debugfs"
E2LABEL = "/usr/sbin/e2label"
ESP = bytes.fromhex("28732AC11FF8D211BA4B00A0C93EC93B")
LINUX = bytes.fromhex("AF3DC60F838472478E793D69D8477DE4")
DADOS_MS = bytes.fromhex("A2A0D0EBE5B9443387C068B6B72699C7")
SAGUI = bytes.fromhex("554741534F49534F80015341" "47554953")
ENV_MT = dict(os.environ, MTOOLS_SKIP_CHECK="1")


def crc(b):
    return zlib.crc32(b) & 0xFFFFFFFF


def ler_gpt(img):
    """Valida la GPT de la imagen como lo haría un firmware. Devuelve (entradas, cabecera) o lanza AssertionError."""
    total = os.path.getsize(img) // 512
    with open(img, "rb") as f:
        mbr = f.read(512)
        f.seek(512)
        cab = f.read(512)
        f.seek((total - 1) * 512)
        cab2 = f.read(512)
        assert mbr[510:512] == b"\x55\xAA", "MBR sin firma"
        e = mbr[446:462]
        assert e[4] == 0xEE and struct.unpack("<II", e[8:16]) == (1, min(total - 1, 0xFFFFFFFF)), "MBR protector incorrecto"
        assert all(not x for x in mbr[462:510]), "el MBR protector tiene más entradas"
        res = []
        for nome, h, lba in (("principal", cab, 1), ("respaldo", cab2, total - 1)):
            assert h[:8] == b"EFI PART", f"cabecera {nome}: firma"
            hsz = struct.unpack("<I", h[12:16])[0]
            assert hsz == 92, f"cabecera {nome}: tamaño {hsz}"
            assert crc(h[:16] + b"\0\0\0\0" + h[20:92]) == struct.unpack("<I", h[16:20])[0], f"cabecera {nome}: CRC"
            mi, alt, primeiro, ultimo = struct.unpack("<QQQQ", h[24:56])
            assert mi == lba, f"cabecera {nome}: MyLBA {mi}"
            assert alt == (total - 1 if lba == 1 else 1), f"cabecera {nome}: AlternateLBA {alt}"
            assert primeiro == 34 and ultimo == total - 34, f"cabecera {nome}: zona utilizable {primeiro}-{ultimo}"
            lba_ent, n, tam, crc_ent = struct.unpack("<QIII", h[72:92])
            assert n == 128 and tam == 128, f"cabecera {nome}: {n} entradas de {tam}"
            assert lba_ent == (2 if lba == 1 else total - 33), f"cabecera {nome}: entradas en {lba_ent}"
            f.seek(lba_ent * 512)
            ent = f.read(n * tam)
            assert crc(ent) == crc_ent, f"cabecera {nome}: CRC de las entradas"
            res.append((h, ent))
        assert res[0][1] == res[1][1], "las entradas del respaldo no son las de la principal"
        assert res[0][0][56:72] == res[1][0][56:72], "el GUID del disco difiere entre cabeceras"
        assert any(res[0][0][56:72]), "GUID de disco vacío"
    ents = []
    guids = set()
    ent = res[0][1]
    for i in range(128):
        x = ent[i * 128:(i + 1) * 128]
        if not any(x[:16]):
            continue
        ini, fim = struct.unpack("<QQ", x[32:48])
        nome = x[56:128].decode("utf-16-le").split("\0")[0]
        assert any(x[16:32]) and x[16:32] not in guids, f"entrada {i + 1}: GUID único repetido o vacío"
        guids.add(x[16:32])
        assert 34 <= ini <= fim <= total - 34, f"entrada {i + 1}: fuera de lo utilizable"
        assert ini % 2048 == 0, f"entrada {i + 1}: no alineada a 1 MiB ({ini})"
        ents.append(dict(slot=i, tipo=x[:16], ini=ini, fim=fim, setores=fim - ini + 1, nome=nome))
    for a in range(len(ents)):
        for b in range(a + 1, len(ents)):
            assert ents[a]["fim"] < ents[b]["ini"] or ents[b]["fim"] < ents[a]["ini"], "entradas solapadas"
    return ents, res[0][0]


def sgdisk_ok(img):
    r = subprocess.run(["sgdisk", "-v", img], capture_output=True, text=True)
    return r.returncode == 0 and "No problems found" in r.stdout, r.stdout + r.stderr


def parte_img(img, e, destino):
    return extrair_particao(img, e["ini"], e["setores"] // 2048, destino)


def conf_de(ents):
    """D388: la entrada de configuración (SAGUI-CONF) se busca por nombre: /home va la última."""
    return [e for e in ents if e["nome"] == "SAGUI-CONF"][-1]


def comprobar_gpt_sagui(img, rotulo, origem, efi, com_home=True):
    ents, _ = ler_gpt(img)
    verificar(True, f"[{rotulo}] GPT válida: CRC de las dos cabeceras y de las entradas, respaldo al final, MBR protector, GUID únicos, 1 MiB")
    ok, saida = sgdisk_ok(img)
    verificar(ok, f"[{rotulo}] sgdisk -v: sin problemas ({saida.strip()[-120:]})")
    r = json.loads(subprocess.run(["sfdisk", "--json", img], capture_output=True, text=True).stdout)["partitiontable"]
    verificar(r["label"] == "gpt" and len(r["partitions"]) == len(ents), f"[{rotulo}] sfdisk --json: tabla gpt con {len(ents)} particiones")
    verificar(r["partitions"][0]["type"].upper() == "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", f"[{rotulo}] la primera es la EFI System")
    esperadas = [ESP, SAGUI] + ([LINUX] if com_home else []) + [LINUX]
    verificar([e["tipo"] for e in ents] == esperadas, f"[{rotulo}] GUID de tipo: ESP, SaguiOS, SAGUI-CONF{', /home (la última, D388)' if com_home else ''}")
    verificar(ents[0]["setores"] == 256 * 2048, f"[{rotulo}] la ESP mide 256 MB")
    verificar(conf_de(ents)["setores"] == 16 * 2048, f"[{rotulo}] SAGUI-CONF: 16 MB y su nombre")
    # ESP: FAT32 sano con EFI/BOOT
    p = parte_img(img, ents[0], os.path.join(SAIDA, "h266-esp.part"))
    r = subprocess.run(["fsck.vfat", "-n", p], capture_output=True, text=True)
    verificar(r.returncode == 0, f"[{rotulo}] fsck.vfat da por buena la ESP ({r.stdout.strip()[-100:]})")
    with open(p, "rb") as f:
        b = f.read(512)
    verificar(b[82:90] == b"FAT32   " and b[71:82].rstrip() == b"SAGUI-ESP", f"[{rotulo}] la ESP es FAT32 con etiqueta SAGUI-ESP")
    if efi is not None:
        out = subprocess.run(["mtype", "-i", p, "::/EFI/BOOT/BOOTX64.EFI"], capture_output=True, env=ENV_MT).stdout
        verificar(out == efi, f"[{rotulo}] EFI/BOOT/BOOTX64.EFI está en la ESP, igual que el original")
    # sistema: cabecera en el primer sector de su partición y el blob detrás, igual que en el origen
    sysp = ents[1]
    with open(origem, "rb") as g:
        g.seek(512)
        cab_o = g.read(512)
    ori2, ori_total = struct.unpack("<I", cab_o[8:12])[0], struct.unpack("<I", cab_o[48:52])[0]
    delta = sysp["ini"] - 1
    with open(img, "rb") as f:
        f.seek(sysp["ini"] * 512)
        cab = f.read(512)
        verificar(cab[:4] == b"SGHD" and cab[144:148] == b"SGIN", f"[{rotulo}] la cabecera de arranque (con identificador de instalación) abre la partición de SaguiOS")
        e2, total = struct.unpack("<I", cab[8:12])[0], struct.unpack("<I", cab[48:52])[0]
        verificar(e2 == ori2 + delta and total == ori_total + delta, f"[{rotulo}] las direcciones de la cabecera apuntan dentro de la partición de SaguiOS")
        verificar(total <= sysp["ini"] + sysp["setores"], f"[{rotulo}] etapa 2, kernel y ramdisk caben en su partición")
        f.seek(e2 * 512)
        copiado = f.read((total - e2) * 512)
    with open(origem, "rb") as g:
        g.seek(ori2 * 512)
        orig_blob = g.read((ori_total - ori2) * 512)
    verificar(copiado == orig_blob, f"[{rotulo}] etapa 2, kernel y ramdisk iguales byte a byte al origen")
    # /home y SAGUI-CONF: ext2 sanos
    if com_home:
        h = parte_img(img, ents[-1], os.path.join(SAIDA, "h266-home.part"))
        rc, o = e2fsck(h)
        verificar(rc == 0 and b"SAGUI-DATOS" in subprocess.run([E2LABEL, h], capture_output=True).stdout, f"[{rotulo}] /home: ext2 SAGUI-DATOS sano")
    c = parte_img(img, conf_de(ents), os.path.join(SAIDA, "h266-conf.part"))
    rc, o = e2fsck(c)
    verificar(rc == 0 and b"SAGUI-CONF" in subprocess.run([E2LABEL, c], capture_output=True).stdout, f"[{rotulo}] SAGUI-CONF: ext2 sano con su etiqueta")
    return ents


def poner_en_conf(img, ents, nombre, contenido):
    c = conf_de(ents)
    p = parte_img(img, c, os.path.join(SAIDA, "h266-conf-w.part"))
    tmp = os.path.join(SAIDA, "h266-conf.tmp")
    with open(tmp, "wb") as f:
        f.write(contenido)
    subprocess.run([DEBUGFS, "-w", "-R", f"write {tmp} {nombre}", p], check=True, capture_output=True)
    subprocess.run(["dd", f"of={img}", f"if={p}", "bs=512", f"seek={c['ini']}", "conv=notrunc", "status=none"], check=True)


def leer_de_conf(img, ini, setores, nombre):
    p = extrair_particao(img, ini, setores // 2048, os.path.join(SAIDA, "h266-conf-r.part"))
    return subprocess.run([DEBUGFS, "-R", f"cat {nombre}", p], capture_output=True).stdout


def hash_disco(img):
    h = hashlib.sha256()
    with open(img, "rb") as f:
        while True:
            b = f.read(4 * MB)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def disco_vazio(nome, mb):
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(mb * MB)
    return img


def montar_efi(m):
    m.comando("mkdir /sistema")
    m.comando("mkdir /sistema/efi")
    out = m.comando("mount hdc1 /sistema/efi")
    verificar("error" not in out.lower() or "montad" in out.lower(), f"el cargador UEFI de prueba se monta en /sistema/efi ({out.strip()[-80:]})")
    out = m.comando("ls /sistema/efi/EFI/BOOT")
    verificar("BOOTX64.EFI" in out, "el cargador UEFI de prueba se ve en /sistema/efi/EFI/BOOT")


def partes_instalacion():
    dest = disco_vazio("h266-dest", 1536)
    origem = imagem_console(clasica=True)   # D379: el instalador de siempre (SAGUI-CONF); el nuevo, h387/h149
    efi = os.urandom(70000)
    efi_img, _ = criar_disco("h266-efi", [dict(tipo="fat32", mb=40, rotulo="EFITEST", pastas=["EFI", "EFI/BOOT"], arquivos={"EFI/BOOT/BOOTX64.EFI": efi})])
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw", "-drive", f"file={efi_img},if=ide,index=2,format=raw,snapshot=on"]
    # --- parte 1
    with Maquina("h266-inst1", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        m.digitar("instalar hdb --gpt\n", 0.04)
        m.esperar_serial("para seguir:", 60)
        # D270: el ramdisk lleva el cargador UEFI de verdad en /sistema/efi: no hay aviso.
        verificar("AVISO: no hay cargador UEFI" not in m.serial() and "Tabla de particiones: GPT" in m.serial(),
                  "con el cargador UEFI del ramdisk (/sistema/efi, D270) no hay aviso, y la tabla es GPT")
        out = m.comando("no")
        verificar("No se ha tocado nada" in out, "sin confirmar, no se toca el disco")
        montar_efi(m)
        out = m.comando("instalar hdb --gpt -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"instalar hdb --gpt termina y verifica ({out.strip()[-300:]})")
        verificar("Tabla de particiones: GPT" in out and "Tabla GPT verificada" in out, "el instalador dice que es GPT y que la ha verificado")
        verificar("con el cargador UEFI de /sistema/efi" in out, "la ESP se llena con el cargador de /sistema/efi")
        out = m.comando("particiones hdb")
        print(out)
        verificar("GPT" in out and "SAGUI-ESP" in out and "SAGUI-CONF" in out and "SAGUI-DATOS" in out, "particiones hdb muestra la GPT con ESP, /home y SAGUI-CONF")
        verificar("EFI (sistema)" in out and "SaguiOS (sistema)" in out, "particiones muestra los tipos EFI (sistema) y SaguiOS (sistema)")
        m.captura("H266-1-particiones-gpt")
    ents = comprobar_gpt_sagui(dest, "gpt", origem, efi)
    # --- parte 2: reinstalar conservando SAGUI-CONF
    secreto = os.urandom(3000)
    poner_en_conf(dest, ents, "clave-prueba", secreto)
    verificar(leer_de_conf(dest, conf_de(ents)["ini"], conf_de(ents)["setores"], "/clave-prueba") == secreto, "el fichero de prueba está en SAGUI-CONF (GPT)")
    with Maquina("h266-inst2", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        montar_efi(m)
        out = m.comando("instalar hdb --gpt -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, "la reinstalación GPT termina")
        verificar("Se conserva el contenido de SAGUI-CONF" in out and "contenido anterior conservado" in out, "reinstalar en GPT conserva SAGUI-CONF y lo dice")
    ents = comprobar_gpt_sagui(dest, "gpt-reinstalado", origem, efi)
    verificar(leer_de_conf(dest, conf_de(ents)["ini"], conf_de(ents)["setores"], "/clave-prueba") == secreto, "la reinstalación GPT conserva el fichero de SAGUI-CONF byte a byte")
    # --- GPT -> MBR con ESP, conservando SAGUI-CONF
    with Maquina("h266-inst3", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        montar_efi(m)
        # D388: con una SAGUI-DATOS que se conserva (la última), apps y juegos tienen que caber delante de ella; en este
        # disco de 1,5 GB, ocupado casi todo por /home, no caben: aquí solo se comprueba GPT -> MBR con ESP y SAGUI-CONF.
        out = m.comando("instalar hdb --mbr --esp --sin-apps --sin-jogos -s", 1500)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"instalar --mbr --esp sobre el disco GPT termina ({out.strip()[-300:]})")
        verificar("contenido anterior conservado" in out, "pasar de GPT a MBR conserva SAGUI-CONF")
    t = subprocess.run(["sfdisk", "-d", dest], capture_output=True, text=True).stdout
    verificar("label: dos" in t, "el disco vuelve a ser MBR (sin GPT)")
    with open(dest, "rb") as f:
        mbr = f.read(512)
    tipos = [mbr[446 + 16 * k + 4] for k in range(4)]
    verificar(tipos[0] == 0xDA and 0xEF in tipos and 0x0F in tipos, f"el MBR lleva SaguiOS (0xDA), la partición EFI (0xEF) y la extendida (tipos {[hex(x) for x in tipos]})")
    k = tipos.index(0xEF)
    ini, qtd = struct.unpack("<II", mbr[446 + 16 * k + 8:446 + 16 * k + 16])
    p = extrair_particao(dest, ini, qtd // 2048, os.path.join(SAIDA, "h266-esp-mbr.part"))
    r = subprocess.run(["fsck.vfat", "-n", p], capture_output=True, text=True)
    out = subprocess.run(["mtype", "-i", p, "::/EFI/BOOT/BOOTX64.EFI"], capture_output=True, env=ENV_MT).stdout
    verificar(r.returncode == 0 and out == efi, "la ESP del MBR es un FAT32 sano con el cargador UEFI")
    with open(dest, "rb") as f:
        f.seek(512)
        verificar(f.read(4) == b"SGHD", "el disco MBR tiene la cabecera de arranque en el LBA 1 (como siempre)")
        f.seek(os.path.getsize(dest) - 512)
        verificar(f.read(8) != b"EFI PART", "la cabecera de respaldo de la GPT vieja ya no está")
    # reconocer SAGUI-CONF en el MBR
    from h255_conf_particion import achar_conf  # noqa: E402
    c = achar_conf(dest)
    verificar(c is not None, "SAGUI-CONF está en el disco MBR")
    verificar(leer_de_conf(dest, c[1], c[2], "/clave-prueba") == secreto, "SAGUI-CONF conserva el fichero tras GPT -> MBR")


def disco_gpt_ajeno():
    img = disco_vazio("h266-ajeno", 600)
    subprocess.run(["sgdisk", "-Z", img], capture_output=True)
    subprocess.run(["sgdisk", "-a", "2048", "-n", "1:2048:+64M", "-t", "1:EF00", "-c", "1:EFI de otro", "-n", "2:0:+200M", "-t", "2:8300",
                    "-c", "2:datos-linux", "-n", "3:0:+100M", "-t", "3:0700", "-c", "3:datos-windows", img],
                   check=True, capture_output=True)
    ents, _ = ler_gpt(img)
    secreto = b"contenido ajeno " * 100
    partes = [("fat32", None), ("ext2", "DATOSLINUX"), ("raw", None)]
    # formatos con herramientas del anfitrión, volcados a su sitio
    for e, (tipo, rot) in zip(ents, partes):
        p = os.path.join(SAIDA, "h266-aj.part")
        with open(p, "wb") as f:
            f.truncate(e["setores"] * 512)
        if tipo == "fat32":
            subprocess.run(["mkfs.vfat", "-F", "32", "-n", "ESPAJENA", p], check=True, stdout=subprocess.DEVNULL)
            subprocess.run(["mmd", "-i", p, "::/EFI"], check=True, env=ENV_MT)
        elif tipo == "ext2":
            pasta = os.path.join(SAIDA, "h266-aj.conteudo")
            os.makedirs(pasta, exist_ok=True)
            with open(os.path.join(pasta, "ajeno.txt"), "wb") as f:
                f.write(secreto)
            subprocess.run(["/usr/sbin/mke2fs", "-q", "-F", "-t", "ext2", "-L", rot, "-d", pasta, p], check=True, stdout=subprocess.DEVNULL)
        else:
            continue
        subprocess.run(["dd", f"of={img}", f"if={p}", "bs=512", f"seek={e['ini']}", "conv=notrunc", "status=none"], check=True)
    return img, ents, secreto


def parte_ajena():
    img, ents, secreto = disco_gpt_ajeno()
    verificar(sgdisk_ok(img)[0], "el disco GPT de prueba (sgdisk) es válido")
    antes = hash_disco(img)
    origem = imagem_console(clasica=True)   # D379: el instalador de siempre (SAGUI-CONF); el nuevo, h387/h149
    extras = ["-drive", f"file={img},if=ide,index=1,format=raw"]
    with Maquina("h266-ajeno1", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones hdb")
        print(out)
        verificar("GPT" in out and "EFI (sistema)" in out and "datos de Windows" in out and "Linux" in out, "particiones lee la GPT ajena: EFI, Linux y datos de Windows")
        verificar("EFI de otro" in out and "datos-linux" in out and "datos-windows" in out, "se ven los nombres de las entradas")
        verificar("DATOSLINUX" in out and "ESPAJENA" in out, "se ven las etiquetas de los sistemas de archivos (FAT32 y ext2)")
        out = m.comando("discos")
        verificar("hdb1" in out and "hdb2" in out, "discos lista las particiones de la GPT ajena")
        m.comando("mkdir /mnt/aj")
        out = m.comando("mount -o ro hdb2 /mnt/aj")
        out = m.comando("cat /mnt/aj/ajeno.txt")
        verificar("contenido ajeno" in out, "el ext2 de la GPT ajena se lee en SaguiOS")
        m.comando("umount /mnt/aj")
        m.captura("H266-2-gpt-ajena")
    verificar(hash_disco(img) == antes, "leer una GPT ajena no cambia ni un byte del disco")
    # editar: borrar la tercera y crear una nueva
    sha_p = {}
    for e in ents[:2]:
        sha_p[e["slot"]] = hashlib.sha256(open(extrair_particao(img, e["ini"], e["setores"] // 2048, os.path.join(SAIDA, "h266-aj-x.part")), "rb").read()).hexdigest()
    with Maquina("h266-ajeno2", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones borrar hdb3 -s")
        verificar("borrada hdb3" in out, f"particiones borra hdb3 en la GPT ({out.strip()[-120:]})")
        out = m.comando("particiones crear hdb 100 ext2 --etiqueta NUEVA -s", 120)
        verificar("creada hdb3" in out, f"particiones crea la nueva partición ({out.strip()[-160:]})")
        out = m.comando("particiones crear hdb todo esp -s", 120)
        verificar("creada hdb4" in out or "no hay" in out.lower() or "cuatro" in out, f"crear con esp o aviso claro ({out.strip()[-160:]})")
        out = m.comando("particiones hdb")
        print(out)
        m.captura("H266-3-gpt-editada")
    ents2, _ = ler_gpt(img)
    verificar(True, "tras editar, la GPT sigue válida (CRC de las dos cabeceras y de las entradas)")
    verificar(sgdisk_ok(img)[0], "sgdisk -v: la GPT editada no tiene problemas")
    verificar([e["ini"] for e in ents2[:2]] == [e["ini"] for e in ents[:2]] and [e["nome"] for e in ents2[:2]] == ["EFI de otro", "datos-linux"],
              "las dos primeras entradas siguen igual (sitio y nombre)")
    for e in ents2[:2]:
        h = hashlib.sha256(open(extrair_particao(img, e["ini"], e["setores"] // 2048, os.path.join(SAIDA, "h266-aj-x.part")), "rb").read()).hexdigest()
        verificar(h == sha_p[e["slot"]], f"el contenido de la entrada {e['slot'] + 1} no se ha tocado")
    verificar(len(ents2) >= 3 and ents2[2]["tipo"] == LINUX and ents2[2]["nome"] == "NUEVA", "la entrada nueva es de tipo Linux y está alineada")
    nueva = ents2[2]
    p = extrair_particao(img, nueva["ini"], nueva["setores"] // 2048, os.path.join(SAIDA, "h266-nueva.part"))
    rc, o = e2fsck(p)
    verificar(rc == 0 and b"NUEVA" in subprocess.run([E2LABEL, p], capture_output=True).stdout, "la partición nueva es un ext2 sano con la etiqueta pedida")
    # cabecera principal dañada: se lee el respaldo y no se escribe nada
    with open(img, "r+b") as f:
        f.seek(512 + 16)
        f.write(b"\xDE\xAD\xBE\xEF")   # CRC de la cabecera principal mal
    antes = hash_disco(img)
    with Maquina("h266-ajeno3", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones hdb")
        print(out)
        verificar("principal de la GPT está dañada" in out and "datos-linux" in out, "con la cabecera principal dañada se lee el respaldo y se avisa")
    verificar(hash_disco(img) == antes, "leer una GPT con la principal dañada tampoco escribe")


def main():
    partes_instalacion()
    parte_ajena()
    return 0


if __name__ == "__main__":
    sys.exit(main())
