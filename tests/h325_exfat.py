#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H325 (D325) - exFAT con lectura y escritura.

Volúmenes creados en el anfitrión con mkfs.exfat (clústeres de 4 KB y de 128 KB) y
poblados con tests/exfat_lib.py (carpetas anidadas, nombres largos con acentos, un archivo
fragmentado, otro contiguo sin cadena). En SaguiOS: montar, listar, leer (sumas
SHA-256), escribir, crear, borrar, renombrar y ampliar. Vuelta al anfitrión:
`fsck.exfat -n` limpio y el contenido comparado byte a byte. Además: un pendrive exFAT
montado con «montar», y un volumen marcado sucio (solo lectura con aviso; con «-o rw»
a propósito se monta para escribir).

Sin exfatprogs (mkfs.exfat/fsck.exfat) la prueba se marca «no aplica» y sale con 0:
    sudo apt install exfatprogs      (o: tools/entorno-local.sh, que lo trae sin root)
"""
import hashlib
import os
import random
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, pendrive_qemu, extrair_particao, SAIDA  # noqa: E402
from exfat_lib import ExFat  # noqa: E402

random.seed(325)
FRAG = bytes(random.getrandbits(8) for _ in range(300 * 1024 + 77))      # fragmentado
GRANDE = bytes(random.getrandbits(8) for _ in range(1536 * 1024 + 5))    # contiguo, sin cadena
LARGO = "un nombre largo con acentos áéíóú y ñ de más de quince caracteres.txt"   # 66 caracteres: se recorta a 63


def sha(b):
    return hashlib.sha256(b).hexdigest()


def hay_exfatprogs():
    return shutil.which("mkfs.exfat") and shutil.which("fsck.exfat")


def volume(nome, mb, cluster, rotulo, popular):
    """Un volumen exFAT suelto (build/testes/<nome>.vol) ya poblado."""
    os.makedirs(SAIDA, exist_ok=True)
    caminho = os.path.join(SAIDA, nome + ".vol")
    with open(caminho, "wb") as f:
        f.truncate(mb * 1024 * 1024)
    subprocess.run(["mkfs.exfat", "-c", str(cluster), "-L", rotulo, caminho], check=True, capture_output=True)
    x = ExFat(caminho, 0, True)
    popular(x)
    x.f.close()
    return caminho


def popular(x):
    x.add_arq("/hola.txt", "Canción del ñandú: acción.\n".encode("latin-1"))
    x.mkdir("/Música")
    x.mkdir("/Música/álbum ñandú")
    x.add_arq("/Música/álbum ñandú/" + LARGO, b"nombre largo\n")
    x.add_arq("/frag.bin", FRAG, fragmentado=True)
    x.add_arq("/grande.bin", GRANDE)
    x.add_arq("/MAYUS.TXT", b"mayusculas\n")
    x.mkdir("/borrar")
    x.add_arq("/borrar/a.txt", b"a\n")
    x.add_arq("/borrar/b.txt", b"b\n")


def disco(nome, vols):
    """Disco MBR con un volumen exFAT por partición (tipo 0x07); devuelve (img, [inicios])."""
    img = os.path.join(SAIDA, nome + ".img")
    total = 1 + sum(os.path.getsize(v) // (1024 * 1024) for v in vols) + 1
    with open(img, "wb") as f:
        f.truncate(total * 1024 * 1024)
    tabela, ini, inicios = "", 2048, []
    for v in vols:
        mb = os.path.getsize(v) // (1024 * 1024)
        tabela += f"start={ini}, size={mb * 2048}, type=7\n"
        inicios.append(ini)
        ini += mb * 2048
    subprocess.run(["sfdisk", "-q", img], input=tabela, text=True, check=True)
    for v, i in zip(vols, inicios):
        subprocess.run(["dd", f"if={v}", f"of={img}", "bs=1M", f"seek={i // 2048}", "conv=notrunc", "status=none"], check=True)
    return img, inicios


def fsck(p):
    r = subprocess.run(["fsck.exfat", "-n", p], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def parte2():
    """Pendrive exFAT con «montar», volumen sucio, volumen lleno, formateo propio y archivo de más de 4 GB."""
    import struct
    v1 = volume("h325-u1", 40, 32768, "UNICO", popular)
    v2 = volume("h325-u2", 16, 4096, "SUCIO", lambda x: x.add_arq("/viejo.txt", b"estaba aqui\n"))
    x = ExFat(v2, 0, True)
    x.poner_sujo(True)
    x.f.close()
    v3 = volume("h325-u3", 8, 4096, "PEQUENO", lambda x: x.add_arq("/semilla.txt", b"s\n"))
    v4 = os.path.join(SAIDA, "h325-u4.vol")
    with open(v4, "wb") as f:
        f.truncate(16 * 1024 * 1024)
    img, ini = disco("h325-pen2", [v1, v2, v3, v4])
    # Segundo pendrive: un exFAT de 5,5 GB (imagen dispersa) con un archivo de 4,5 GB (más que el VFS).
    GB = 1024 * 1024 * 1024
    vg = os.path.join(SAIDA, "h325-grande.vol")
    with open(vg, "wb") as f:
        f.truncate(11 * GB // 2)
    subprocess.run(["mkfs.exfat", "-c", "131072", "-L", "ENORME", vg], check=True, capture_output=True)
    x = ExFat(vg, 0, True)
    x.add_arq_esparso("/enorme.bin", 9 * GB // 2, {0: b"PRINCIPIO-DEL-ENORME", 4 * GB - 100: b"CERCA-DEL-LIMITE-4GB-"})
    x.add_arq("/pequeno.txt", b"junto al enorme\n")
    x.f.close()
    imgg = os.path.join(SAIDA, "h325-pen3.img")
    with open(imgg, "wb") as f:
        f.truncate(6 * GB)
    subprocess.run(["sfdisk", "-q", imgg], input="start=2048, type=7\n", text=True, check=True)
    subprocess.run(["dd", f"if={vg}", f"of={imgg}", "bs=1M", "seek=1", "conv=notrunc,sparse", "status=none"], check=True)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img) + pendrive_qemu(imgg, "pen2")
    with Maquina("h325b", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        # ---- montar (la forma de los FAT32): el primer volumen, en /usb
        out = m.comando("montar")
        verificar("/usb" in out, "montar: el pendrive exFAT en /usb")
        out = m.comando("ls /usb")
        verificar("hola.txt" in out and "frag.bin" in out, "ls /usb del pendrive exFAT")
        m.comando("echo en el pendrive > /usb/nota.txt")
        m.comando("cp /usb/frag.bin /usb/Música/copia.bin")
        out = m.comando("sha256sum /usb/Música/copia.bin", 60)
        verificar(sha(FRAG) in out, "copia de un archivo fragmentado dentro de otra carpeta")
        out = m.comando("desmontar")
        verificar("Ya puedes retirar el pendrive." in out or "Desmontado" in out, "desmontar libera el pendrive")
        # ---- volumen sucio
        out = m.comando("mount usb0p2 /mnt/sucio")
        verificar("solo lectura" in out or "(ro" in out, "volumen marcado sucio: se monta en solo lectura")
        out = m.comando("cat /mnt/sucio/viejo.txt")
        verificar("estaba aqui" in out, "el volumen sucio se puede leer")
        m.comando("echo prohibido > /mnt/sucio/nuevo.txt")
        out = m.comando("ls /mnt/sucio")
        verificar("nuevo.txt" not in out, "en solo lectura no se escribe")
        m.comando("umount /mnt/sucio")
        out = m.comando("mount -o rw usb0p2 /mnt/sucio")
        verificar("montado en /mnt/sucio" in out and "solo lectura" not in out, "con -o rw a propósito se monta para escribir")
        m.comando("echo forzado > /mnt/sucio/forzado.txt")
        m.comando("umount /mnt/sucio")
        # ---- volumen lleno: la copia que no cabe no deja clústeres perdidos
        m.comando("mount -t exfat usb0p3 /mnt/pequeno")
        m.comando("mount -t exfat usb0p1 /mnt/unico")
        for k in range(8):
            m.comando(f"cp /mnt/unico/grande.bin /mnt/pequeno/g{k}.bin", 90)
        out = m.comando("df")
        m.comando("rm /mnt/pequeno/g0.bin")
        m.comando("rm /mnt/pequeno/g1.bin")
        m.comando("cp /mnt/unico/grande.bin /mnt/pequeno/nuevo-tras-liberar.bin", 90)
        out = m.comando("ls /mnt/pequeno")
        verificar("nuevo-tras-liberar.bin" in out, "tras borrar, vuelve a caber un archivo (el espacio se liberó de verdad)")
        m.comando("umount /mnt/pequeno")
        m.comando("umount /mnt/unico")
        # ---- formatear con particiones (mkfs exFAT propio)
        out = m.comando("particiones formatear usb0p4 exfat NUEVO -s", 120)
        verificar("exFAT" in out, "particiones formatear usb0p4 exfat")
        out = m.comando("discos")
        verificar(re.search(r"usb0p4\s+16 MB\s+exFAT\s+\"NUEVO\"", out), "discos: la partición formateada sale exFAT con su etiqueta")
        out = m.comando("mount usb0p4 /mnt/nuevo")
        verificar("montado en /mnt/nuevo" in out, "montar lo que formateó SaguiOS")
        m.comando("echo recien formateado > /mnt/nuevo/primero.txt")
        m.comando("mkdir /mnt/nuevo/carpeta")
        m.comando("cp /leia-me.txt /mnt/nuevo/carpeta/")
        m.comando("umount /mnt/nuevo")
        # ---- más de 4 GB
        out = m.comando("mount -t exfat usb1p1 /mnt/enorme")
        verificar("montado en /mnt/enorme" in out, "montar un exFAT de 5,5 GB")
        out = m.comando("ls /mnt/enorme")
        verificar("enorme.bin" in out, "el archivo de 4,5 GB sale en el listado")
        out = m.comando("head -c 20 /mnt/enorme/enorme.bin")
        verificar("PRINCIPIO-DEL-ENORME" in out, "se lee el principio de un archivo de más de 4 GB")
        m.comando("echo escritura >> /mnt/enorme/enorme.bin")
        m.comando("echo escritura > /mnt/enorme/enorme.bin")
        out = m.comando("cat /mnt/enorme/pequeno.txt")
        verificar("junto al enorme" in out, "el volumen sigue bien tras rechazar escribir en el grande")
        m.comando("umount /mnt/enorme")
        verificar("[TESTE] falha" not in m.serial() and "PANICO" not in m.serial(), "ningún error en el kernel (parte 2)")
    # ---- anfitrión
    for k, (inicio, mb, nome) in enumerate(((ini[0], 40, "montar"), (ini[1], 16, "sucio"), (ini[2], 8, "lleno"), (ini[3], 16, "formateado"))):
        p = extrair_particao(img, inicio, mb, os.path.join(SAIDA, f"h325-q{k}.vol"))
        cod, txt = fsck(p)
        if cod:
            print(txt)
        x = ExFat(p)
        if k == 1:
            verificar(x.sujo(), "[sucio] sigue marcado sucio (no se limpia lo que no se comprobó)")
            verificar(x.ler_arq("/forzado.txt") == b"forzado\n" and x.ler_arq("/viejo.txt") == b"estaba aqui\n", "[sucio] lo escrito con -o rw está y lo viejo también")
            verificar(not x.achar("/nuevo.txt"), "[sucio] nada se escribió mientras estuvo en solo lectura")
            continue
        verificar(cod == 0, f"fsck.exfat -n [{nome}]: limpio (código {cod})")
        verificar(not x.erros and not x.sujo(), f"[{nome}] sumas correctas y VolumeDirty limpio")
        t = x.arvore()
        if k == 0:
            verificar(t["/nota.txt"] == b"en el pendrive\n" and t["/Música/copia.bin"] == FRAG, "[montar] nota y copia fragmentada")
        if k == 2:
            grandes = [n for n in t if n.startswith("/g")]
            completos = [n for n in grandes if t[n] == GRANDE]
            verificar(len(completos) >= 1 and all(t[n] == GRANDE[:len(t[n])] for n in grandes),
                      f"[lleno] {len(completos)} copias completas idénticas; la que no cupo es un prefijo (sin basura)")
            verificar("/nuevo-tras-liberar.bin" in t and t["/nuevo-tras-liberar.bin"] == GRANDE, "[lleno] la copia tras liberar espacio es idéntica")
        if k == 3:
            verificar(x.rotulo == "NUEVO", f"[formateado] etiqueta {x.rotulo!r}")
            verificar(t["/primero.txt"] == b"recien formateado\n" and "/carpeta/leia-me.txt" in t, "[formateado] lo escrito sobre el volumen formateado por SaguiOS")
    p = os.path.join(SAIDA, "h325-g.vol")
    subprocess.run(["dd", f"if={imgg}", f"of={p}", "bs=1M", "skip=1", "count=5632", "conv=sparse", "status=none"], check=True)
    cod, txt = fsck(p)
    if cod:
        print(txt)
    verificar(cod == 0, f"fsck.exfat -n [enorme]: limpio (código {cod})")
    x = ExFat(p)
    e = x.achar("/enorme.bin")
    verificar(e and e.tam == 9 * GB // 2, "[enorme] el archivo de 4,5 GB conserva su tamaño: no se truncó ni corrompió")
    verificar(x.ler(x.pos_cluster(e.primeiro), 20) == b"PRINCIPIO-DEL-ENORME", "[enorme] su contenido sigue ahí")
    return 0


def main():
    if not hay_exfatprogs():
        print("   NO APLICA: faltan mkfs.exfat/fsck.exfat (sudo apt install exfatprogs, o tools/entorno-local.sh)")
        return 0
    if sys.argv[1:] == ["2"]:      # solo la parte 2 (para desarrollar)
        return parte2()
    v1 = volume("h325-v1", 24, 4096, "EXF4K", popular)
    v2 = volume("h325-v2", 48, 131072, "EXF128K", popular)
    img, ini = disco("h325-pen", [v1, v2])
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h325", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("discos")
        verificar(re.search(r"usb0p1\s+24 MB\s+exFAT\s+\"EXF4K\"", out) and re.search(r"usb0p2\s+48 MB\s+exFAT\s+\"EXF128K\"", out),
                  "discos: las dos particiones exFAT con su etiqueta")
        out = m.comando("mount -t exfat usb0p1 /mnt/e4")
        verificar("montado en /mnt/e4" in out and "solo lectura" not in out, "mount -t exfat (lectura y escritura)")
        out = m.comando("mount -o rw LABEL=EXF128K /mnt/e128")
        verificar("montado en /mnt/e128" in out, "mount por LABEL= del exFAT de clústeres de 128 KB")
        out = m.comando("mount")
        verificar("tipo exfat" in out, "mount sin argumentos: tipo exfat")
        for pto, nome in (("/mnt/e4", "4 KB"), ("/mnt/e128", "128 KB")):
            out = m.comando(f"ls {pto}")
            verificar("hola.txt" in out and "Música" in out and "frag.bin" in out and "borrar" in out, f"[{nome}] ls de la raíz")
            out = m.comando(f"cat {pto}/hola.txt")
            verificar("Canción del ñandú: acción." in out, f"[{nome}] lectura con acentos (UTF-16 a Latin-1)")
            out = m.comando(f"cat {pto}/HOLA.TXT")
            verificar("Canción del ñandú" in out, f"[{nome}] búsqueda sin distinguir mayúsculas (tabla del volumen)")
            out = m.comando(f"cat {pto}/mayus.txt")
            verificar("mayusculas" in out, f"[{nome}] MAYUS.TXT se abre en minúsculas")
            out = m.comando(f"ls \"{pto}/Música/álbum ñandú\"")
            verificar(LARGO[:30] in out, f"[{nome}] nombre largo en carpeta anidada")
            out = m.comando(f"sha256sum {pto}/frag.bin {pto}/grande.bin", 120)
            verificar(sha(FRAG) in out, f"[{nome}] archivo fragmentado (con cadena) leído completo")
            verificar(sha(GRANDE) in out, f"[{nome}] archivo contiguo (NoFatChain) de 1,5 MB leído completo")
        out = m.comando("df")
        verificar("/mnt/e4" in out, "df muestra el volumen exFAT")
        # ---- escritura en los dos
        for pto in ("/mnt/e4", "/mnt/e128"):
            m.comando(f"echo creado desde SaguiOS > {pto}/nuevo.txt")
            m.comando(f"echo otra linea >> {pto}/nuevo.txt")
            m.comando(f"echo reemplazado > {pto}/hola.txt")
            m.comando(f"cp {pto}/frag.bin {pto}/frag-copia.bin")
            m.comando(f"cp {pto}/grande.bin {pto}/grande-copia.bin")
            m.comando(f"mkdir \"{pto}/Carpeta ñ nueva\"")
            m.comando(f"echo dentro > \"{pto}/Carpeta ñ nueva/archivo dentro.txt\"")
            m.comando(f"mkdir {pto}/vacia")
            m.comando(f"rmdir {pto}/vacia")
            m.comando(f"mv {pto}/nuevo.txt \"{pto}/Carpeta ñ nueva\"")
            m.comando(f"mv {pto}/MAYUS.TXT {pto}/minus.txt")
            m.comando(f"mv {pto}/Música {pto}/Canciones")
            m.comando(f"truncate -s 100 {pto}/grande-copia.bin")
            m.comando(f"truncate -s 70000 {pto}/frag-copia.bin")       # crece: se lee con ceros
            m.comando(f"echo cola >> {pto}/frag-copia.bin")
            m.comando(f"rm {pto}/borrar/a.txt")
            m.comando(f"rm {pto}/borrar/b.txt")
            m.comando(f"rmdir {pto}/borrar")
            # una carpeta con muchas entradas: la carpeta crece de clúster (4 KB: 128 entradas por clúster)
            m.comando(f"mkdir {pto}/muchos")
        # la carpeta de muchos ficheros: bucle en el intérprete
        for i in range(0, 60):
            m.comando(f"echo n{i} > /mnt/e4/muchos/archivo-con-nombre-largo-numero-{i:03d}.txt", 20)
        out = m.comando("ls /mnt/e4/muchos | conta")
        verificar("60 líneas" in out, "carpeta que creció de clúster: 60 archivos con nombre largo listados")
        out = m.comando("cat /mnt/e4/muchos/archivo-con-nombre-largo-numero-059.txt")
        verificar("n59" in out, "el último de la carpeta ampliada se lee")
        out = m.comando("cat \"/mnt/e4/Carpeta ñ nueva/nuevo.txt\"")
        verificar("creado desde SaguiOS" in out and "otra linea" in out, "SaguiOS relee lo que grabó")
        out = m.comando("rmdir /mnt/e4/Canciones")
        verificar("no está vacía" in out, "rmdir de carpeta con contenido: rechazado")
        out = m.comando("umount /mnt/e4")
        verificar("Desmontado: /mnt/e4" in out, "umount del volumen de 4 KB")
        m.comando("umount /mnt/e128")
        out = m.comando("ls /mnt/e4")
        verificar("no existe" in out, "la carpeta del montaje desaparece")
        verificar("[TESTE] falha" not in m.serial() and "PANICO" not in m.serial(), "ningún error en el kernel")
    # ---- de vuelta en el anfitrión
    for k, (inicio, mb, nome) in enumerate(((ini[0], 24, "4 KB"), (ini[1], 48, "128 KB"))):
        p = extrair_particao(img, inicio, mb, os.path.join(SAIDA, f"h325-p{k}.vol"))
        cod, txt = fsck(p)
        if cod:
            print(txt)
        verificar(cod == 0, f"fsck.exfat -n [{nome}]: limpio (código {cod})")
        x = ExFat(p)
        verificar(not x.erros, f"[{nome}] sumas (arranque, grupos, NameHash, mayúsculas) correctas {x.erros}")
        verificar(not x.sujo(), f"[{nome}] VolumeDirty limpio tras desmontar")
        t = x.arvore()
        verificar(t.get("/hola.txt") == b"reemplazado\n", f"[{nome}] hola.txt sobrescrito")
        verificar(t.get("/Carpeta ñ nueva/nuevo.txt") == b"creado desde SaguiOS\notra linea\n", f"[{nome}] nuevo.txt creado, ampliado y movido")
        verificar(t.get("/Carpeta ñ nueva/archivo dentro.txt") == b"dentro\n", f"[{nome}] carpeta con ñ y espacios")
        verificar(t.get("/frag.bin") == FRAG and t.get("/grande.bin") == GRANDE, f"[{nome}] originales intactos")
        copia = t.get("/frag-copia.bin")
        verificar(copia == FRAG[:70000] + b"cola\n", f"[{nome}] copia fragmentada truncada a 70000 y ampliada con una cola")
        verificar(t.get("/grande-copia.bin") == GRANDE[:100], f"[{nome}] grande-copia truncada a 100 bytes")
        verificar(t.get("/minus.txt") == b"mayusculas\n" and "/MAYUS.TXT" not in t, f"[{nome}] renombrado MAYUS.TXT -> minus.txt")
        verificar("/Canciones" in t and "/Música" not in t and any(k.startswith("/Canciones/álbum") for k in t), f"[{nome}] carpeta renombrada con su contenido")
        verificar("/borrar" not in t and "/vacia" not in t, f"[{nome}] carpetas borradas")
        if k == 0:
            muchos = [n for n in t if n.startswith("/muchos/")]
            verificar(len(muchos) == 60 and t["/muchos/archivo-con-nombre-largo-numero-007.txt"] == b"n7\n", f"[{nome}] carpeta de 60 archivos (creció de clúster)")
    return parte2()


if __name__ == "__main__":
    sys.exit(main())
