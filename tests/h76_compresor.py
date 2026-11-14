#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H76 - Compresor: tar, gzip y zip (mejora 056, D103).

Criterio, con datos generados y un pendrive FAT32 compartido con el anfitrión:
  1. lo que crean tar, gzip, zip, tarfile y zipfile del anfitrión (GNU tar, ustar,
     pax, nombres largos y con acentos, stored y deflate, gzip de varios miembros)
     se extrae en SaguiOS con los mismos SHA-256;
  2. lo que crea SaguiOS (tar, tar -z, gzip -1/-6/-9, zip -r, zip -0) se abre en el
     anfitrión con tar, gzip, unzip, tarfile y zipfile, con el mismo contenido;
  3. archivos corruptos o truncados dan un error claro (y código distinto de 0);
  4. las rutas "../" y absolutas no salen de la carpeta de destino.
"""
import gzip
import hashlib
import io
import os
import random
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402

TRABAJO = os.path.join(SAIDA, "h76")
LARGO = "d1_" + "x" * 37 + "/d2_" + "y" * 37 + "/archivo_largo.txt"   # data/ + esto = 105 caracteres (el límite del sistema es 128 con el destino)


def sha(b):
    return hashlib.sha256(b).hexdigest()


def generar():
    """El árbol de datos (data/) y sus extras; devuelve {ruta: bytes o None si es carpeta}."""
    r = random.Random(76)
    palabras = ["alfa", "beta", "gamma", "delta", "sagui", "kernel", "disco", "\n", "tar", "zip"]
    t = {
        "data/a.txt": " ".join(r.choice(palabras) for _ in range(12000)).encode(),
        "data/b.bin": r.randbytes(200_000),
        "data/vazio": b"",
        "data/um": b"x",
        "data/sub/c.txt": b"hola\n" * 3000,
        "data/sub/deep/d.bin": r.randbytes(70_000),
        "data/pv": None,
        "data/" + LARGO: b"nombre largo\n" * 50,
        "data/cero.bin": bytes(100_000),
    }
    return t


def escribir_arbol(destino, arbol):
    shutil.rmtree(destino, ignore_errors=True)
    for ruta, c in arbol.items():
        p = os.path.join(destino, ruta)
        if c is None:
            os.makedirs(p, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as f:
                f.write(c)


def hashes(arbol, prefijo=""):
    return {prefijo + k: sha(v) for k, v in arbol.items() if v is not None}


def leer_arbol(raiz):
    out = {}
    for d, ds, fs in os.walk(raiz):
        for x in ds:
            out[os.path.relpath(os.path.join(d, x), raiz)] = None
        for x in fs:
            with open(os.path.join(d, x), "rb") as f:
                out[os.path.relpath(os.path.join(d, x), raiz)] = sha(f.read())
    return out


def sh(*cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def host_archivos(arbol, extra):
    """Crea en TRABAJO/arch los archivos hechos por el anfitrión."""
    base = os.path.join(TRABAJO, "src")
    escribir_arbol(base, arbol)
    arch = os.path.join(TRABAJO, "arch")
    shutil.rmtree(arch, ignore_errors=True)
    os.makedirs(arch)
    sh("tar", "-cf", f"{arch}/h.tar", "-C", base, "data")
    sh("tar", "-czf", f"{arch}/h.tar.gz", "-C", base, "data")
    sh("tar", "--format=ustar", "-cf", f"{arch}/hu.tar", "-C", base, "data/a.txt", "data/sub", "data/pv")
    sh("zip", "-q", "-r", f"{arch}/h.zip", "data", cwd=base)
    sh("zip", "-q", "-0", "-r", f"{arch}/hz0.zip", "data", cwd=base)
    # Python: pax por defecto (acentos), GNU para nombres largos, zip con UTF-8
    ext = dict(arbol)
    ext.update(extra)
    with tarfile.open(f"{arch}/p.tar", "w", format=tarfile.PAX_FORMAT) as t:
        for ruta, c in ext.items():
            ti = tarfile.TarInfo(ruta)
            ti.mtime = 1700000000
            if c is None:
                ti.type = tarfile.DIRTYPE
                ti.mode = 0o755
                t.addfile(ti)
            else:
                ti.size = len(c)
                t.addfile(ti, io.BytesIO(c))
    with tarfile.open(f"{arch}/g.tar.gz", "w:gz", format=tarfile.GNU_FORMAT) as t:
        for ruta, c in ext.items():
            if c is None:
                continue
            ti = tarfile.TarInfo(ruta)
            ti.size = len(c)
            t.addfile(ti, io.BytesIO(c))
    with zipfile.ZipFile(f"{arch}/pz.zip", "w") as z:
        for i, (ruta, c) in enumerate(ext.items()):
            if c is None:
                z.writestr(ruta + "/", b"")
            else:
                z.writestr(ruta, c, compress_type=zipfile.ZIP_STORED if i % 4 == 3 else zipfile.ZIP_DEFLATED)
    b = arbol["data/b.bin"]
    a = arbol["data/a.txt"]
    sh("gzip", "-9", "-c", f"{base}/data/b.bin", stdout=open(f"{arch}/h_b.gz", "wb"))
    with open(f"{arch}/hm.gz", "wb") as f:   # dos miembros seguidos
        f.write(gzip.compress(a, 6, mtime=1) + gzip.compress(b, 1, mtime=2))
    # maliciosos
    with tarfile.open(f"{arch}/evil.tar", "w", format=tarfile.GNU_FORMAT) as t:
        for n in ("good.txt", "../escape.txt", "../../escape2.txt", "ok/../../escape3.txt", "/abs_evil.txt"):
            ti = tarfile.TarInfo(n)
            ti.size = 5
            t.addfile(ti, io.BytesIO(b"mal!\n" if n != "good.txt" else b"bien\n"))
    with zipfile.ZipFile(f"{arch}/evil.zip", "w") as z:
        for n in ("good.txt", "../escape.txt", "../../escape2.txt", "ok/../../escape3.txt", "/abs_evil.txt"):
            z.writestr(n, b"mal!\n" if n != "good.txt" else b"bien\n")
    # corruptos
    gz = open(f"{arch}/h_b.gz", "rb").read()
    open(f"{arch}/trunc.gz", "wb").write(gz[: len(gz) // 2])
    mal = bytearray(gz)
    mal[len(mal) // 2] ^= 0x55
    open(f"{arch}/bad.gz", "wb").write(bytes(mal))
    z = open(f"{arch}/h.zip", "rb").read()
    open(f"{arch}/trunc.zip", "wb").write(z[: len(z) // 2])
    zz = bytearray(open(f"{arch}/hz0.zip", "rb").read())
    zz[400] ^= 0xFF   # dentro de los datos de data/a.txt (stored): falla el CRC
    open(f"{arch}/bad.zip", "wb").write(bytes(zz))
    tg = open(f"{arch}/h.tar.gz", "rb").read()
    open(f"{arch}/trunc.tgz", "wb").write(tg[: len(tg) // 2])
    open(f"{arch}/garbage.tar", "wb").write(random.Random(5).randbytes(5000))
    return arch


def comandos(arbol, extra):
    """El script que ejecuta SaguiOS (sh /usb/t1.sh)."""
    L = []
    h_base = [f"{k}" for k, v in arbol.items() if v is not None]
    h_ext = h_base + [k for k, v in extra.items() if v is not None]

    def shas(pref, nombres):
        L.append("sha256sum " + " ".join(f'"{pref}/{n}"' for n in nombres))

    def extraer(titulo, cmd, pref, nombres):
        L.append(f"echo == {titulo}")
        L.append(f"mkdir {pref}")
        L.append(cmd)
        L.append(f"echo RC_{titulo}=$?")
        shas(pref, nombres)
        L.append(f"ls {pref}/data/pv")
        L.append(f"echo RCPV_{titulo}=$?")

    extraer("htar", "tar -xf /usb/h.tar -C /tmp/x1", "/tmp/x1", h_base)
    extraer("htgz", "tar -xzf /usb/h.tar.gz -C /tmp/x2", "/tmp/x2", h_base)
    extraer("hutar", "tar -xf /usb/hu.tar -C /tmp/x3", "/tmp/x3", ["data/a.txt", "data/sub/c.txt", "data/sub/deep/d.bin"])
    extraer("ptar", "tar -xf /usb/p.tar -C /tmp/x4", "/tmp/x4", h_ext)
    extraer("gtgz", "tar -xf /usb/g.tar.gz -C /tmp/x5", "/tmp/x5", h_ext)   # sin -z: se detecta por el contenido
    extraer("hzip", "unzip -q -d /tmp/x6 /usb/h.zip", "/tmp/x6", h_base)
    extraer("hz0", "unzip -q -d /tmp/x7 /usb/hz0.zip", "/tmp/x7", h_base)
    extraer("pzip", "unzip -q -d /tmp/x8 /usb/pz.zip", "/tmp/x8", h_ext)
    L.append("echo == ls")
    L.append("tar -tvf /usb/h.tar.gz")
    L.append("unzip -l /usb/h.zip")
    L.append("unzip -t /usb/pz.zip")
    # gzip del anfitrión
    L.append("echo == gz")
    L.append("gunzip -c /usb/h_b.gz | sha256sum")
    L.append("zcat /usb/hm.gz | sha256sum")
    L.append("gzip -dc /usb/hm.gz > /tmp/hm.out")
    L.append("sha256sum /tmp/hm.out")
    L.append("cp /usb/h_b.gz /tmp/k.gz")
    L.append("gunzip -k /tmp/k.gz")
    L.append("echo RC_gunzipk=$?")
    L.append("sha256sum /tmp/k")
    L.append("ls /tmp/k.gz")
    L.append("gunzip /tmp/k.gz")   # sin -k borra el .gz (y /tmp/k ya existe: no sobrescribe)
    L.append("echo RC_gunzipexiste=$?")
    L.append("gunzip -f /tmp/k.gz")
    L.append("echo RC_gunzipf=$?")
    L.append("ls /tmp/k.gz")
    # lo que crea SaguiOS
    L.append("echo == crea")
    L.append("mkdir /usb/out")
    L.append("tar -cf /usb/out/s.tar -C /usb data")
    L.append("echo RC_star=$?")
    L.append("tar -czf /usb/out/s.tgz -C /usb data")
    L.append("echo RC_stgz=$?")
    L.append("tar cvf /usb/out/s2.tar -C /usb data/sub data/a.txt")
    L.append("cd /usb")
    L.append("zip -rq out/s.zip data")
    L.append("echo RC_szip=$?")
    L.append("zip -0rq out/s0.zip data")
    L.append("zip -r out/sv.zip data/sub")
    L.append("zip out/solo.zip data/a.txt data/vazio data/um")
    for n in (1, 6, 9):
        L.append(f"gzip -{n} -c /usb/data/a.txt > /usb/out/a{n}.gz")
        L.append(f"gzip -{n} -c /usb/data/b.bin > /usb/out/b{n}.gz")
    L.append("gzip -c /usb/data/vazio > /usb/out/vazio.gz")
    L.append("gzip -c /usb/data/cero.bin > /usb/out/cero.gz")
    L.append("cp /usb/data/a.txt /tmp/ga.txt")
    L.append("gzip -k /tmp/ga.txt")
    L.append("echo RC_gzipk=$?")
    L.append("cp /tmp/ga.txt.gz /usb/out/ga.gz")
    L.append("ls /tmp/ga.txt")
    # ida y vuelta dentro de SaguiOS
    L.append("echo == vuelta")
    L.append("mkdir /tmp/v1")
    L.append("tar -xzf /usb/out/s.tgz -C /tmp/v1")
    L.append("sha256sum " + " ".join(f"/tmp/v1/{n}" for n in h_base))
    L.append("mkdir /tmp/v2")
    L.append("unzip -q -d /tmp/v2 /usb/out/s.zip")
    L.append("sha256sum " + " ".join(f"/tmp/v2/{n}" for n in h_base))
    # seguridad
    L.append("echo == evil")
    L.append("mkdir /tmp/ev")
    L.append("tar -xf /usb/evil.tar -C /tmp/ev")
    L.append("echo RC_evtar=$?")
    L.append("mkdir /tmp/ez")
    L.append("unzip -q -d /tmp/ez /usb/evil.zip")
    L.append("echo RC_evzip=$?")
    L.append("sha256sum /tmp/ev/good.txt /tmp/ev/abs_evil.txt /tmp/ez/good.txt /tmp/ez/abs_evil.txt")
    L.append("echo == lsraiz")
    L.append("ls /tmp")
    L.append("echo == lsraiz2")
    L.append("ls /")
    L.append("echo == lsfin")
    # corruptos
    L.append("echo == corrupto")
    for n, cmd in (("badgz", "gunzip -c /usb/bad.gz"), ("truncgz", "gunzip -c /usb/trunc.gz"),
                   ("truncgzip", "gzip -t /usb/trunc.gz"),
                   ("truntgz", "tar -tzf /usb/trunc.tgz"), ("truncgzp", "tar -xf /usb/trunc.tgz -C /tmp/ct"),
                   ("badzip", "unzip -t /usb/bad.zip"), ("truncz", "unzip -l /usb/trunc.zip"),
                   ("garbage", "tar -tf /usb/garbage.tar"), ("garbz", "unzip -t /usb/garbage.tar"),
                   ("nogz", "gunzip -c /usb/garbage.tar"), ("noexiste", "tar -tf /usb/no_existe.tar")):
        L.append(f"{cmd} > /tmp/lixo")
        L.append(f"echo RC_{n}=$?")
    L.append("echo == fin")
    return "\n".join(L) + "\n"


def main():
    os.makedirs(TRABAJO, exist_ok=True)
    arbol = generar()
    extra = {"extra/canción.txt": "acentos ñ á é\n".encode(), "extra/".rstrip("/"): None}
    # carpeta "extra" y un archivo con acento (UTF-8 en el archivo, Latin-1 en SaguiOS)
    arch = host_archivos(arbol, extra)
    h_ext = hashes(arbol)
    h_ext.update(hashes(extra))
    src = os.path.join(TRABAJO, "src")
    # pendrive con data/ y los archivos del anfitrión en la raíz
    pen = criar_pendrive("h76", mb=96, rotulo="H76")
    mtools(pen, "mcopy", "-s", "-o", f"{src}/data", "p:/")
    for f in sorted(os.listdir(arch)):
        mtools(pen, "mcopy", "-o", os.path.join(arch, f), f"p:/{f}")
    script = comandos(arbol, extra)
    ruta = os.path.join(TRABAJO, "t1.sh")
    with open(ruta, "wb") as f:
        f.write(script.encode("latin-1", "replace").replace("canción".encode("latin-1", "replace"), "canción".encode("latin-1")))
    mtools(pen, "mcopy", "-o", ruta, "p:/t1.sh")

    with Maquina("h76", "ide", imagem=imagem_console(), memoria="768M",
                 extras=["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen, "p1")) as m:
        m.esperar_serial("sagui:/$", 120)
        m.esperar_serial("[DISCO] usb0", 30)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "pendrive montado en /usb")
        out = m.comando("sh /usb/t1.sh", tempo=900)
        for p in ("tar", "gzip", "gunzip", "zcat", "zip", "unzip"):
            verificar(f"{p}: comando no encontrado" not in out and f"{p}: not found" not in out, f"existe el comando {p}")
        m.comando("cd /", 10)
        o2 = m.comando("desmontar usb0", 120)
        verificar("Ya puedes retirar el pendrive." in o2, "desmontar graba todo")
    # el script termina en "== fin"
    verificar("== fin" in out, "el script de pruebas llegó hasta el final")
    sal = out
    shaP = dict((p, h) for h, p in re.findall(r"([0-9a-f]{64})\s+(\S+)", sal))

    def seccion(nombre, sig):
        a = sal.index(f"== {nombre}")
        b = sal.index(f"== {sig}", a)
        return sal[a:b]

    def rc(n):
        mm = re.search(rf"RC_{n}=(\d+)", sal)
        return int(mm.group(1)) if mm else None

    # ---- 1. lo del anfitrión, extraído en SaguiOS ----
    esperado = dict(hashes(arbol))
    esperado_ext = dict(esperado)
    esperado_ext.update(hashes(extra))
    casos = [("htar", "x1", esperado), ("htgz", "x2", esperado), ("ptar", "x4", esperado_ext),
             ("gtgz", "x5", esperado_ext), ("hzip", "x6", esperado), ("hz0", "x7", esperado), ("pzip", "x8", esperado_ext)]
    for nombre, d, exp in casos:
        verificar(rc(nombre) == 0, f"{nombre}: la extracción termina bien (rc {rc(nombre)})")
        faltan = [k for k, v in exp.items() if shaP.get(f"/tmp/{d}/{k.replace('canción', 'canción')}") != v]
        verificar(not faltan, f"{nombre}: SHA-256 iguales en SaguiOS ({len(exp)} archivos)" + (f" FALLAN {faltan[:3]}" if faltan else ""))
        if nombre != "gtgz":   # ese tar de Python no lleva entradas de carpeta
            verificar(re.search(rf"RCPV_{nombre}=0", sal) is not None, f"{nombre}: la carpeta vacía data/pv existe")
    verificar(rc("hutar") == 0, "hutar: ustar de GNU tar se extrae")
    for k in ("data/a.txt", "data/sub/c.txt", "data/sub/deep/d.bin"):
        verificar(shaP.get("/tmp/x3/" + k) == esperado[k], f"ustar: {k} igual")
    s = seccion("ls", "gz")
    verificar("data/a.txt" in s and "-rw-" in s and "data/sub/" in s, "tar -tv lista nombres y permisos")
    verificar("data/b.bin" in s and "200000" in s and "archivos" in s, "unzip -l lista tamaños y total")
    verificar("No errors detected in compressed data of /usb/pz.zip" in s, "unzip -t del zip de Python: sin errores")
    s = seccion("gz", "crea")
    verificar(s.count(esperado["data/b.bin"]) >= 2, "gunzip -c / zcat de un .gz del anfitrión")
    ab = sha(arbol["data/a.txt"] + arbol["data/b.bin"])
    verificar(ab in s, "gzip de dos miembros seguidos (sha de a+b)")
    verificar(rc("gunzipk") == 0 and shaP.get("/tmp/k") == esperado["data/b.bin"], "gunzip -k deja el resultado correcto")
    verificar(rc("gunzipexiste") == 1 and "ya existe" in s, "gunzip no sobrescribe sin -f")
    verificar(rc("gunzipf") == 0, "gunzip -f sobrescribe y borra el .gz")

    # ---- 2. lo que crea SaguiOS, abierto en el anfitrión ----
    for r in ("star", "stgz", "szip", "gzipk"):
        verificar(rc(r) == 0, f"SaguiOS: {r} termina bien")
    out_dir = os.path.join(TRABAJO, "out")
    shutil.rmtree(out_dir, ignore_errors=True)
    os.makedirs(out_dir)
    mtools(pen, "mcopy", "-s", "-o", "p:/out", TRABAJO)
    exp_arbol = {k[len("data/"):]: v for k, v in hashes(arbol).items()}
    exp_arbol_data = dict(hashes(arbol))
    carp = {k for k, v in arbol.items() if v is None}
    # tar y tar.gz
    for nombre in ("s.tar", "s.tgz"):
        p = os.path.join(out_dir, nombre)
        r = subprocess.run(["tar", "-tf", p], capture_output=True, text=True)
        verificar(r.returncode == 0, f"tar del anfitrión lista {nombre}: {r.stderr.strip()[:80]}")
        x = os.path.join(TRABAJO, "x_" + nombre)
        shutil.rmtree(x, ignore_errors=True)
        os.makedirs(x)
        sh("tar", "-xf", p, "-C", x)
        got = leer_arbol(x)
        for k, v in exp_arbol_data.items():
            verificar(got.get(k) == v, f"{nombre}: {k} igual (tar del anfitrión)") if k.endswith(("a.txt", "b.bin", "vazio", "um")) else None
        verificar(all(got.get(k) == v for k, v in exp_arbol_data.items()), f"{nombre}: todos los archivos con el mismo SHA-256 (tar -x)")
        verificar("data/pv" in got and got["data/pv"] is None, f"{nombre}: la carpeta vacía llega")
        with tarfile.open(p) as t:
            nombres = t.getnames()
            verificar(any(len(n) > 100 for n in nombres), f"{nombre}: el nombre largo (>100) se lee con tarfile")
            for mi in t:
                if mi.isfile():
                    verificar(sha(t.extractfile(mi).read()) == exp_arbol_data[mi.name], f"tarfile: {mi.name} igual") if mi.name.endswith(("sub/c.txt", "deep/d.bin", LARGO[-20:])) else None
    with tarfile.open(os.path.join(out_dir, "s2.tar")) as t:
        verificar(sorted(t.getnames()) == sorted(["data/sub/", "data/sub/c.txt", "data/sub/deep/", "data/sub/deep/d.bin", "data/a.txt"])
                  or set(n.rstrip("/") for n in t.getnames()) == {"data/sub", "data/sub/c.txt", "data/sub/deep", "data/sub/deep/d.bin", "data/a.txt"},
                  "tar cvf de estilo antiguo con dos rutas")
    # zip
    for nombre in ("s.zip", "s0.zip"):
        p = os.path.join(out_dir, nombre)
        r = subprocess.run(["unzip", "-tq", p], capture_output=True, text=True)
        verificar(r.returncode == 0, f"unzip del anfitrión comprueba {nombre}: {r.stdout.strip()[-80:]}")
        with zipfile.ZipFile(p) as z:
            verificar(z.testzip() is None, f"zipfile.testzip de {nombre}")
            got = {i.filename: sha(z.read(i)) for i in z.infolist() if not i.is_dir()}
            verificar(all(got.get(k) == v for k, v in exp_arbol_data.items()), f"{nombre}: SHA-256 iguales (zipfile)")
            dirs = [i.filename for i in z.infolist() if i.is_dir()]
            verificar("data/pv/" in dirs, f"{nombre}: carpeta vacía con barra final")
            if nombre == "s0.zip":
                verificar(all(i.compress_type == zipfile.ZIP_STORED for i in z.infolist()), "zip -0: todo stored")
            else:
                verificar(any(i.compress_type == zipfile.ZIP_DEFLATED for i in z.infolist()), "zip: usa deflate")
        x = os.path.join(TRABAJO, "x_" + nombre)
        shutil.rmtree(x, ignore_errors=True)
        os.makedirs(x)
        sh("unzip", "-q", p, "-d", x)
        got = leer_arbol(x)
        verificar(all(got.get(k) == v for k, v in exp_arbol_data.items()), f"{nombre}: unzip del anfitrión extrae igual")
    with zipfile.ZipFile(os.path.join(out_dir, "solo.zip")) as z:
        verificar(sorted(z.namelist()) == ["data/a.txt", "data/um", "data/vazio"], "zip sin -r de varios archivos")
    # gzip
    origen = {"a": arbol["data/a.txt"], "b": arbol["data/b.bin"]}
    tam = {}
    for n in (1, 6, 9):
        for k in ("a", "b"):
            p = os.path.join(out_dir, f"{k}{n}.gz")
            data = open(p, "rb").read()
            r = subprocess.run(["gzip", "-dc", p], capture_output=True)
            verificar(r.returncode == 0 and r.stdout == origen[k], f"gzip -{n}: {k}{n}.gz lo abre el gzip del anfitrión")
            verificar(gzip.decompress(data) == origen[k], f"gzip -{n}: {k}{n}.gz lo abre Python")
            tam[(k, n)] = len(data)
    verificar(tam[("a", 9)] <= tam[("a", 1)], f"el nivel 9 no es peor que el 1 ({tam[('a', 9)]} <= {tam[('a', 1)]})")
    verificar(tam[("a", 6)] < len(origen["a"]) * 0.5, f"el texto se comprime (a6: {tam[('a', 6)]} de {len(origen['a'])})")
    verificar(tam[("b", 6)] < len(origen["b"]) + 200, "los datos aleatorios casi no crecen (se guardan)")
    ref = len(subprocess.run(["gzip", "-6", "-c"], input=origen["a"], capture_output=True).stdout)
    verificar(tam[("a", 6)] < ref * 1.15, f"tamaño a6 cerca del gzip del anfitrión ({tam[('a', 6)]} vs {ref})")
    verificar(gzip.decompress(open(os.path.join(out_dir, "vazio.gz"), "rb").read()) == b"", "gzip de un archivo vacío")
    cero = open(os.path.join(out_dir, "cero.gz"), "rb").read()
    verificar(gzip.decompress(cero) == bytes(100_000) and len(cero) < 400, f"100000 ceros -> {len(cero)} bytes")
    verificar(gzip.decompress(open(os.path.join(out_dir, "ga.gz"), "rb").read()) == arbol["data/a.txt"], "gzip -k conserva y comprime bien")
    with gzip.open(os.path.join(out_dir, "ga.gz")) as g:
        verificar(g.read() == arbol["data/a.txt"], "gzip.open lee el .gz de SaguiOS")

    # ---- ida y vuelta en SaguiOS ----
    s = seccion("vuelta", "evil")
    shaV = dict((p, h) for h, p in re.findall(r"([0-9a-f]{64})\s+(\S+)", s))
    verificar(all(shaV.get(f"/tmp/v1/{k}") == v for k, v in exp_arbol_data.items()), "ida y vuelta tar -z en SaguiOS")
    verificar(all(shaV.get(f"/tmp/v2/{k}") == v for k, v in exp_arbol_data.items()), "ida y vuelta zip -r en SaguiOS")

    # ---- 4. seguridad ----
    s = seccion("evil", "lsraiz")
    verificar(s.count("ruta peligrosa") >= 6, f"las rutas con .. se rechazan con mensaje (tar y zip): {s.count('ruta peligrosa')}")
    verificar(rc("evtar") == 1 and rc("evzip") == 1, "los archivos maliciosos dan código 1")
    good = sha(b"bien\n")
    verificar(shaP.get("/tmp/ev/good.txt") == good and shaP.get("/tmp/ez/good.txt") == good, "lo bueno sí se extrae")
    verificar(shaP.get("/tmp/ev/abs_evil.txt") is not None and shaP.get("/tmp/ez/abs_evil.txt") is not None,
              "la ruta absoluta queda dentro del destino")
    a = sal.index("== lsraiz")
    b = sal.index("== lsfin")
    verificar("escape" not in sal[a:b], "nada de escape*.txt fuera del destino (ls /tmp y ls /)")

    # ---- 3. corruptos ----
    s = seccion("corrupto", "fin")
    for n in ("badgz", "truncgz", "truncgzip", "truntgz", "truncgzp", "badzip", "truncz", "garbage", "garbz", "nogz", "noexiste"):
        verificar(rc(n) == 1, f"{n}: error con código 1 (fue {rc(n)})")
    for frag in ("CRC", "truncado", "no es un archivo zip", "no es un archivo tar", "no existe"):
        verificar(frag in s, f"mensaje claro: «{frag}»")
    print("H76 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
