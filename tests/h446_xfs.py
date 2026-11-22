#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H446 (D446) - XFS en solo lectura (fase 1 de docs/xfs.md).

Imágenes creadas en el PC con mkfs.xfs (xfsprogs) y llenadas con un fichero «proto» (sin montar
nada), más tests/xfs_lib.py para lo que mkfs no hace solo (un fichero con más de 100 extents en
un árbol bmbt, huecos y extents sin escribir; el diario sucio). Variantes:
  A  v5 con CRC, bloque de 4 KB (GPT, tipo Linux): las cinco formas de carpeta (shortform, block,
     leaf, node y node con bmbt, 10 000 ficheros), nombres UTF-8, enlaces cortos y largos, huecos;
  B  v4 antigua, sin CRC (MBR 0x83), con su propio fichero fragmentado;
  C  v5 con bloque de 1 KB y bloques de carpeta de 4 KB (cada bloque de carpeta en 4 del disco);
  D  diario sucio: se monta con aviso;
  E  un bit incompatible desconocido: no se monta y se dice por qué;
  F  un inodo con la suma CRC32C mal: ese fichero da error y los demás se leen.
Primero el propio driver (kernel/fs/xfs.c) corre en el PC sobre las imágenes (tools/xfs_anfitriao.c):
recorre todo, busca cada nombre y compara cada fichero. Después, en QEMU: discos, mount, df, ls,
sumas SHA-256 iguales a las del PC, enlaces, y que escribir, crear, borrar o renombrar dé «solo
lectura». Al final las particiones no cambiaron ni un byte y xfs_repair -n sigue limpio.

Sin xfsprogs (mkfs.xfs, xfs_db, xfs_repair) la prueba se marca «no aplica» y sale con 0:
    bash tools/entorno-local.sh      (lo trae sin root; o sudo apt install xfsprogs)
"""
import hashlib
import os
import random
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, extrair_particao, SAIDA  # noqa: E402
import xfs_lib as X  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MB = 320                      # mkfs.xfs no hace volúmenes de menos de 300 MB (imágenes dispersas)
rnd = random.Random(446)
HOLA = "Canción del ñandú: acción y corazón.\n".encode("utf-8")
MEDIO = bytes(rnd.getrandbits(8) for _ in range(1536 * 1024 + 7))
CANCION = "letra con eñes: ñoño\n".encode("utf-8")
EURO = "precio_€.txt"     # un nombre UTF-8 que no cabe en Latin-1 (se ve con sus bytes)
LARGO = "./" * 450 + "hola.txt"           # 908 bytes: enlace en bloques (remoto)
ABSOLUTO = "/" + "x" * 900                # destino absoluto: se lee como texto
MUCHOS = 10000


def sha(b):
    return hashlib.sha256(b).hexdigest()


def bloco(bs):
    return lambda i: hashlib.sha256(f"bloque {i}".encode()).digest() * (bs // 32)


def plano(n, semente):
    r = random.Random(semente)
    return [(r.choice([0, 0, 1, 2]), r.choice([1, 1, 2, 3]), r.random() < 0.15) for _ in range(n)]


def imagem_a(caminho):
    arv = {
        "hola.txt": HOLA,
        "vacio.txt": b"",
        "medio.bin": MEDIO,
        "Música": {"álbum_ñandú": {"canción.txt": CANCION}, "subir": X.Ligacao("../hola.txt")},
        EURO: b"euro\n",
        "carpetas": {
            "sf": {f"s{i}.txt": f"s{i}\n".encode() for i in range(3)},
            "block": {f"b{i:02d}.txt": f"b{i}\n".encode() for i in range(40)},
            "leaf": {f"hoja_{i:03d}.txt": f"h{i}\n".encode() for i in range(300)},
            "node": {f"nodo_{i:04d}.txt": b"" for i in range(1500)},
        },
        "muchos": {f"f{i:05d}_archivo.txt": b"igual\n" for i in range(MUCHOS)},
        "corto": X.Ligacao("hola.txt"),
        "largo": X.Ligacao(LARGO),
        "absoluto": X.Ligacao(ABSOLUTO),
        "a_carpeta": X.Ligacao("carpetas/sf"),
        "frag.bin": b"f" * (700 * 4096),
        "poco.bin": b"p" * (30 * 4096),
    }
    X.crear(caminho, MB, ["-m", "rmapbt=0"], arv, rotulo="DATOS_XFS")
    esp, n, _ = X.fragmentar(caminho, "/frag.bin", plano(300, 1), bloco(4096), buraco_final=3)
    esp2, n2, _ = X.fragmentar(caminho, "/poco.bin", [(1, 2, False), (0, 3, True), (2, 1, False), (0, 2, False)], bloco(4096))
    return {"frag.bin": esp, "poco.bin": esp2}, n


def imagem_b(caminho):
    arv = {"hola.txt": HOLA, "medio.bin": MEDIO, "leaf": {f"v4_{i:03d}": b"4\n" for i in range(300)},
           "largo": X.Ligacao(LARGO), "ñandú.txt": CANCION, "frag.bin": b"f" * (500 * 4096)}
    X.crear(caminho, MB, ["-m", "crc=0"], arv, rotulo="VIEJO_V4")
    esp, n, _ = X.fragmentar(caminho, "/frag.bin", plano(200, 2), bloco(4096), buraco_final=1)
    return {"frag.bin": esp}, n


def imagem_c(caminho):
    arv = {"hola.txt": HOLA, "medio.bin": MEDIO,
           "block": {f"k{i:02d}": b"k\n" for i in range(40)},
           "leaf": {f"k{i:03d}": b"k\n" for i in range(300)},
           "node": {f"k{i:04d}": b"" for i in range(1500)},
           "largo": X.Ligacao(LARGO)}
    X.crear(caminho, MB, ["-b", "size=1024", "-n", "size=4096"], arv, rotulo="KILO")
    return {}, 0


def imagem_simples(caminho, rotulo):
    arv = {"hola.txt": HOLA, "otro.txt": b"otro fichero\n", "roto.txt": b"este inodo tendra la suma mal\n"}
    X.crear(caminho, MB, [], arv, rotulo=rotulo)


def estragar_inodo(caminho, ruta):
    """Cambia un byte del inodo (di_gen) sin rehacer la suma: el CRC32C ya no cuadra."""
    ino = X.numero_inodo(caminho, ruta)
    x = X.Img(caminho)
    off = x.ino_byte(ino) + 92
    b = bytearray(x.ler(off, 1))
    b[0] ^= 0x5A
    x.gravar(off, bytes(b))
    x.fechar()


def disco(nome, vols, gpt):
    """Un disco con un volumen por partición: GPT (tipo «Linux filesystem») o MBR (0x83)."""
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate((2 + len(vols) * MB) * 1024 * 1024)
    tipo = "0FC63DAF-8483-4772-8E79-3D69D8477DE4" if gpt else "83"
    tabela = ("label: gpt\n" if gpt else "") + "".join(
        f"start={2048 + i * MB * 2048}, size={MB * 2048}, type={tipo}\n" for i in range(len(vols)))
    subprocess.run(["sfdisk", "-q", img], input=tabela, text=True, check=True)
    for i, v in enumerate(vols):
        subprocess.run(["dd", f"if={v}", f"of={img}", "bs=1M", f"seek={1 + i * MB}", "conv=notrunc,sparse", "status=none"],
                       check=True)
    return img, [2048 + i * MB * 2048 for i in range(len(vols))]


def anfitriao():
    """Compila el driver del kernel para el PC (32 bits, como el kernel)."""
    saida = os.path.join(SAIDA, "xfs_anfitriao")
    r = os.path.expanduser("~/.local/saguitools/root")
    gcc = os.path.join(r, "usr/lib/gcc/x86_64-linux-gnu/13/32")
    cmd = ["gcc", "-m32", "-static", "-O1", "-fno-builtin", "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1",
           "-I" + os.path.join(RAIZ, "kernel/inc"), "-o", saida]
    if os.path.isdir(gcc):
        cmd += ["-B" + gcc + "/", "-B" + os.path.join(r, "usr/lib32") + "/", "-L" + os.path.join(r, "usr/lib32"), "-L" + gcc]
    cmd += [os.path.join(RAIZ, p) for p in ("tools/xfs_anfitriao.c", "kernel/fs/xfs.c", "kernel/lib/crc32c.c",
                                              "kernel/lib/formatar.c")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    verificar(r.returncode == 0, "el driver XFS se compila para el PC (tools/xfs_anfitriao.c)" +
              ("" if r.returncode == 0 else ": " + r.stderr[-800:]))
    return saida


def no_pc(xa, img, esperados):
    """El driver en el PC: recorre todo (y busca cada nombre) y lee cada fichero."""
    r = subprocess.run([xa, img, "arbol"], capture_output=True)
    linhas = r.stdout.decode("latin-1").splitlines()
    erros = [l for l in r.stderr.decode("latin-1").splitlines() if not re.match(r"klog: \[XFS\] (montado|desmontado)", l)]
    verificar(r.returncode == 0 and not erros, f"{os.path.basename(img)}: recorrido completo en el PC ({len(linhas)} entradas)"
              + ("" if not erros else ": " + " | ".join(erros[:5])))
    for ruta, cont in esperados.items():
        o = subprocess.run([xa, img, "cat", ruta.encode("latin-1")], capture_output=True).stdout
        verificar(o == cont, f"{os.path.basename(img)}: {ruta} leído en el PC igual que el original ({len(cont)} bytes)")
    return linhas


def main():
    if not X.hay_xfsprogs():
        print("H446: no aplica (no hay xfsprogs: bash tools/entorno-local.sh)")
        return 0
    os.makedirs(SAIDA, exist_ok=True)
    va, vb, vc, vd, ve, vf = (os.path.join(SAIDA, f"h446-{x}.vol") for x in "abcdef")
    frag_a, na = imagem_a(va)
    frag_b, nb = imagem_b(vb)
    imagem_c(vc)
    imagem_simples(vd, "SUCIO")
    X.diario_sucio(vd)
    imagem_simples(ve, "RARO")
    out = X.xfs_db(ve, "sb 0", "p features_incompat")
    inc = int(re.search(r"features_incompat = (0x[0-9a-f]+)", out).group(1), 16)
    # «write -d»: deja escribir un valor que xfs_db cree malo, con la suma bien (solo falla el bit)
    X.xfs_db(ve, "sb 0", f"write -d features_incompat {inc | 0x80000000:#x}", escrita=True)
    c = X.campos(ve, "sb 0", "p features_incompat crc")
    verificar(int(c["features_incompat"], 16) & 0x80000000 and "correct" in c["crc"], "E: bit 0x80000000 puesto con la suma bien")
    imagem_simples(vf, "ROTO")
    estragar_inodo(vf, "/roto.txt")

    # ---- en el PC: lo que dice xfsprogs
    for v, nome in ((va, "A (v5)"), (vb, "B (v4)"), (vc, "C (1 KB)")):
        cod, txt = X.repair(v)
        verificar(cod == 0, f"xfs_repair -n limpio en {nome}" + ("" if cod == 0 else ": " + txt[-600:]))
    formas = {r: X.forma_pasta(va, r) for r in ("/carpetas/sf", "/carpetas/block", "/carpetas/leaf", "/carpetas/node", "/muchos")}
    verificar(sorted(formas.values()) == sorted(["shortform", "block", "leaf", "node", "btree"]),
              f"las cinco formas de carpeta en A: {formas}")
    verificar(X.forma_pasta(vb, "/leaf") == "leaf" and X.forma_pasta(vc, "/node") == "node",
              "carpetas leaf en v4 y node con bloques de 1 KB")
    for v, n in ((va, na), (vb, nb)):
        c = X.campos(v, "path /frag.bin", "p core.format core.nextents")
        verificar(c.get("core.format", "").startswith("3") and int(c["core.nextents"]) == n and n > 100,
                  f"{os.path.basename(v)}: frag.bin con {n} extents en un árbol bmbt (según xfs_db)")
    verificar(X.campos(va, "path /largo", "p core.format").get("core.format", "").startswith("2"),
              "el enlace largo está en bloques (remoto)")
    verificar(X.campos(vb, "sb 0", "p versionnum").get("versionnum", "").endswith("4"), "B es XFS v4")
    log = subprocess.run(["xfs_logprint", "-t", vd], capture_output=True, text=True).stdout
    verificar("CLEAN" not in log, "D: xfs_logprint ya no ve el diario limpio")

    # ---- el driver del kernel en el PC
    xa = anfitriao()
    esp_a = {"hola.txt": HOLA, "medio.bin": MEDIO, "Música/álbum_ñandú/canción.txt": CANCION, "frag.bin": frag_a["frag.bin"],
             "poco.bin": frag_a["poco.bin"], "corto": HOLA, "largo": HOLA, "Música/subir": HOLA,
             "absoluto": ABSOLUTO.encode(), "vacio.txt": b"", "carpetas/leaf/hoja_299.txt": b"h299\n",
             "muchos/f09999_archivo.txt": b"igual\n", "a_carpeta/s2.txt": b"s2\n"}
    lin = no_pc(xa, va, esp_a)
    verificar(sum(1 for l in lin if l.startswith("f 6 muchos/")) == MUCHOS, f"A: las {MUCHOS} entradas de /muchos en el PC")
    no_pc(xa, vb, {"hola.txt": HOLA, "medio.bin": MEDIO, "largo": HOLA, "ñandú.txt": CANCION, "frag.bin": frag_b["frag.bin"]})
    no_pc(xa, vc, {"hola.txt": HOLA, "medio.bin": MEDIO, "largo": HOLA, "node/k1499": b""})
    r = subprocess.run([xa, vd, "cat", "hola.txt"], capture_output=True)
    verificar(r.stdout == HOLA and b"diario no esta vacio" in r.stderr, "D (diario sucio): se monta con aviso en el PC")
    r = subprocess.run([xa, ve, "montar"], capture_output=True)
    verificar(r.returncode == 1 and b"funciones desconocidas 0x80000000" in r.stderr, "E (bit desconocido): no se monta en el PC")
    r = subprocess.run([xa, vf, "cat", "roto.txt"], capture_output=True)
    r2 = subprocess.run([xa, vf, "cat", "otro.txt"], capture_output=True)
    verificar(r.returncode == 2 and b"suma CRC32C mal" in r.stderr and r2.stdout == b"otro fichero\n",
              "F: el inodo con la suma mal da error de E/S; los demás se leen")
    r = subprocess.run([xa, va, "escribir", "hola.txt"], capture_output=True, text=True)
    verificar(r.stdout.count(": -16") == 4, "en el PC, escribir/crear/carpeta/borrar dan ERRO_SO_LEITURA (-16)")

    # ---- en QEMU
    d1, ini1 = disco("h446-gpt", [va, vf], True)
    d2, ini2 = disco("h446-mbr", [vb, vc, vd, ve], False)
    antes = [sha(open(extrair_particao(d1, ini1[0], MB, d1 + ".pa"), "rb").read()),
             sha(open(extrair_particao(d2, ini2[0], MB, d2 + ".pb"), "rb").read())]
    extras = ["-drive", f"file={d1},if=ide,index=1,format=raw", "-drive", f"file={d2},if=ide,index=2,format=raw"]
    with Maquina("h446", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("discos")
        verificar(re.search(r"hdb1\s.*XFS \(solo lectura\)", out) and "DATOS_XFS" in out, "discos: la GPT con XFS (tipo Linux)")
        verificar(re.search(r"hd[cd]1\s.*XFS \(solo lectura\)", out) and "VIEJO_V4" in out, "discos: el XFS v4 en MBR 0x83")
        verificar("XFS: funciones desconocidas 0x80000000" in out, "discos: el XFS con un bit desconocido dice por qué no se lee")
        hd2 = "hdc" if re.search(r"hdc1\s", out) else "hdd"
        out = m.comando("mount hdb1 /xa")
        verificar("(xfs, solo lectura) montado en /xa" in out, "mount de XFS v5: siempre solo lectura")
        out = m.comando("mount")
        verificar("tipo xfs (solo lectura" in out, "mount lista «xfs (solo lectura)»")
        out = m.comando("df")
        verificar(re.search(r"hdb1\s+xfs\s", out), "df muestra el tipo xfs")
        out = m.comando("infopc | grep XFS")
        verificar("XFS (solo lectura)" in out, "infopc enseña el XFS")
        out = m.comando("cat /xa/hola.txt")
        verificar("Canción del ñandú" in out, "cat en XFS v5 (nombres y texto UTF-8 a Latin-1)")
        sumas = {"medio.bin": MEDIO, "frag.bin": frag_a["frag.bin"], "poco.bin": frag_a["poco.bin"]}
        for nome, cont in sumas.items():
            out = m.comando(f"sha256sum /xa/{nome}", 180)
            verificar(sha(cont) in out, f"SHA-256 de {nome} en QEMU igual que en el PC ({len(cont)} bytes)")
        out = m.comando("cat /xa/Música/álbum_ñandú/canción.txt")
        verificar("ñoño" in out, "carpetas y nombres con acentos")
        out = m.comando("cat /xa/precio_*")
        verificar("euro" in out, "un nombre UTF-8 que no cabe en Latin-1 se lista y se abre")
        for pasta, n in (("carpetas/sf", 3), ("carpetas/block", 40), ("carpetas/leaf", 300), ("carpetas/node", 1500), ("muchos", MUCHOS)):
            out = m.comando(f"ls /xa/{pasta} | conta", 240)
            verificar(f"{n} líneas" in out, f"ls de una carpeta {formas['/' + pasta]} ({n} entradas)")
        out = m.comando("cat /xa/muchos/f07777_archivo.txt /xa/carpetas/leaf/hoja_123.txt /xa/carpetas/node/nodo_1499.txt")
        verificar("igual" in out and "h123" in out and "no existe" not in out, "búsqueda por hash en las formas leaf, node y btree")
        out = m.comando("cat /xa/muchos/no_esta.txt")
        verificar("no existe" in out, "un nombre que no está: no existe")
        out = m.comando("cat /xa/corto /xa/largo /xa/Música/subir")
        verificar(out.count("Canción del ñandú") == 3, "enlaces simbólicos: corto, largo (en bloques) y con ..")
        out = m.comando("wc /xa/absoluto")
        verificar(f"{len(ABSOLUTO)} letras" in out, "un enlace con destino absoluto se lee como su texto")
        out = m.comando("ls /xa/a_carpeta")
        verificar("s0.txt" in out and "s2.txt" in out, "un enlace a una carpeta se recorre")
        out = m.comando("wc /xa/vacio.txt")
        verificar("0 letras" in out, "fichero vacío")
        # ---- solo lectura: EROFS
        out = m.comando("echo x > /xa/nuevo.txt")
        verificar("solo lectura" in out, "crear un fichero en XFS: solo lectura")
        out = m.comando("echo x >> /xa/hola.txt")
        verificar("solo lectura" in out, "añadir a un fichero: solo lectura")
        out = m.comando("mkdir /xa/carpeta")
        verificar("solo lectura" in out, "mkdir: solo lectura")
        out = m.comando("rm /xa/hola.txt")
        verificar("solo lectura" in out, "rm: solo lectura")
        out = m.comando("mv /xa/hola.txt /xa/otro.txt")
        verificar("solo lectura" in out, "mv: solo lectura")
        # ---- v4, 1 KB, diario sucio, bit desconocido, suma mal
        out = m.comando(f"mount -o rw {hd2}1 /xb")
        verificar("(xfs, solo lectura) montado en /xb" in out and "solo sabe leer XFS" in out,
                  "XFS v4 con -o rw: se monta en solo lectura y se avisa")
        for nome, cont in (("medio.bin", MEDIO), ("frag.bin", frag_b["frag.bin"])):
            out = m.comando(f"sha256sum /xb/{nome}", 180)
            verificar(sha(cont) in out, f"v4: SHA-256 de {nome} igual que en el PC")
        out = m.comando("cat /xb/ñandú.txt /xb/largo")
        verificar("ñoño" in out and "Canción del ñandú" in out, "v4: nombre UTF-8 y enlace largo")
        out = m.comando("ls /xb/leaf | conta")
        verificar("300 líneas" in out, "v4: carpeta leaf")
        out = m.comando(f"mount {hd2}2 /xc")
        verificar("montado en /xc" in out, "XFS con bloques de 1 KB")
        out = m.comando("sha256sum /xc/medio.bin", 180)
        verificar(sha(MEDIO) in out, "1 KB: SHA-256 igual que en el PC")
        for pasta, n in (("block", 40), ("leaf", 300), ("node", 1500)):
            out = m.comando(f"ls /xc/{pasta} | conta", 240)
            verificar(f"{n} líneas" in out, f"1 KB: carpeta {pasta} con bloques de carpeta de 4 KB ({n})")
        out = m.comando("cat /xc/node/k0777 /xc/leaf/k123 /xc/largo")
        verificar("no existe" not in out and "Canción del ñandú" in out, "1 KB: búsqueda y enlace largo en bloques")
        out = m.comando(f"mount {hd2}3 /xd")
        verificar("montado en /xd" in out, "XFS con el diario sucio: se monta")
        out = m.comando("dmesg diario")
        verificar("el diario no esta vacio" in out, "diario sucio: el aviso queda en el registro")
        out = m.comando("cat /xd/hola.txt")
        verificar("Canción del ñandú" in out, "diario sucio: se lee")
        out = m.comando(f"mount {hd2}4 /xe")
        verificar("funciones desconocidas 0x80000000" in out and "montado en" not in out,
                  "un bit incompatible desconocido: no se monta y se dice por qué")
        out = m.comando("mount hdb2 /xf")
        verificar("montado en /xf" in out, "XFS con un inodo estropeado: se monta")
        out = m.comando("cat /xf/roto.txt")
        verificar("este inodo" not in out and "cat:" in out, "el inodo con la suma mal da error")
        out = m.comando("cat /xf/otro.txt")
        verificar("otro fichero" in out, "los demás ficheros del mismo volumen se leen")
        out = m.comando("dmesg CRC32C")
        verificar("suma CRC32C mal en inodo" in out, "la suma mal queda en el registro")
        for p in ("/xa", "/xb", "/xc", "/xd", "/xf"):
            m.comando(f"umount {p}")
    depois = [sha(open(extrair_particao(d1, ini1[0], MB, d1 + ".pa2"), "rb").read()),
              sha(open(extrair_particao(d2, ini2[0], MB, d2 + ".pb2"), "rb").read())]
    verificar(depois == antes, "las particiones XFS no cambiaron ni un byte")
    cod, txt = X.repair(d1 + ".pa2")
    verificar(cod == 0, "xfs_repair -n sigue limpio después de SaguiOS")
    for f in (d1 + ".pa", d1 + ".pa2", d2 + ".pb", d2 + ".pb2", va, vb, vc, vd, ve, vf):
        if os.path.exists(f):
            os.remove(f)
    return 0


if __name__ == "__main__":
    sys.exit(main())
