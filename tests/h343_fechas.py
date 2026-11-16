#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H343 (D341, v8.4.4) - conservar y fijar las fechas de los ficheros.

Un segundo disco IDE hecho en el anfitrión con cuatro particiones: ext2 con inodos de 256 bytes
(con los ficheros de partida: un tar, un zip y uno de referencia, con fechas conocidas), ext4,
FAT32 y exFAT (si hay exfatprogs). En SaguiOS, sobre cada una y sobre /tmp (RAM) y /home (el ext2
de la imagen, inodos de 128 bytes):
  - touch -d @N y touch -t fijan la fecha y stat la lee (FAT: de 2 en 2 segundos);
  - cp -p la conserva; mv dentro del mismo sistema (renombrar) y entre sistemas, también;
  - tar x y unzip la conservan;
  - escribir actualiza la fecha;
  - rsync -t contra un demonio del anfitrión (como h339) la conserva al bajar y al subir.
Permisos: sagui no pone una fecha concreta a un fichero de root, pero sí «ahora» si puede escribirlo.
De vuelta en el anfitrión: debugfs (ext2/ext4), mcopy -m (FAT, hora local = UTC en la máquina
virtual) y exfat_lib (exFAT) ven las mismas fechas; e2fsck -fn, fsck.vfat -n y fsck.exfat -n limpios.
ext4 se escribe solo con la rama de D326/D337 (ESCRITA_EXT34): si se monta de solo lectura se
comprueba que fijar la fecha da error y no cambia nada.
Sin rsync en el anfitrión no aplica (código 77).
"""
import io
import os
import re
import shutil
import signal
import subprocess
import sys
import tarfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste, opcoes_apps, extrair_particao  # noqa: E402
import h314_rsync as h314  # noqa: E402
import h339_rsync_demonio as h339  # noqa: E402
from exfat_lib import ExFat  # noqa: E402

TRAB = os.path.join(SAIDA, "h343")
MB = 1024 * 1024

T_REF = 1600000000      # ref.txt (par)
T_TAR_A = 1400000003    # dtar/a.txt dentro del tar (impar: en FAT queda ...02)
T_TAR_D = 1500000001    # la carpeta dtar/
T_ZIP = 1300000005      # z.txt dentro del zip (campo UT, en UTC)
T_TOUCH = 1234567891    # touch -d @
T_RS_BAJA = 1250000007  # el fichero que se baja por rsync
T_RS_SUBE = 1260000009  # el que se sube
TOCAR_T = "202103040506.09"   # touch -t: 2021-03-04 05:06:09 (hora local; UTC en la máquina virtual)

# (nombre, partición o None, punto de montaje)
PARTES = [("ext2", 1, "/e2"), ("ext4", 2, "/e4"), ("fat", 3, "/fat"), ("exfat", 4, "/exf")]
TAM = {1: 64, 2: 64, 3: 64, 4: 32}


def cmd(m, texto, tempo=120):
    return h314.comando_ate(m, texto, tempo)


def hay(p):
    return shutil.which(p) or os.path.exists("/usr/sbin/" + p) or os.path.exists("/sbin/" + p)


def herramienta(p):
    return shutil.which(p) or ("/usr/sbin/" + p if os.path.exists("/usr/sbin/" + p) else "/sbin/" + p)


def fs_de_2s(t):
    return t - (t % 2)


def preparar_fixtures(base):
    f = os.path.join(base, "fix")
    shutil.rmtree(f, ignore_errors=True)
    os.makedirs(f)
    ref = os.path.join(f, "ref.txt")
    open(ref, "w").write("fichero de referencia\n")
    os.utime(ref, (T_REF, T_REF))
    # tar con una carpeta y un fichero de fechas conocidas
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as t:
        d = tarfile.TarInfo("dtar")
        d.type, d.mode, d.mtime = tarfile.DIRTYPE, 0o755, T_TAR_D
        t.addfile(d)
        datos = b"dentro del tar\n"
        a = tarfile.TarInfo("dtar/a.txt")
        a.size, a.mode, a.mtime = len(datos), 0o644, T_TAR_A
        t.addfile(a, io.BytesIO(datos))
    open(os.path.join(f, "paquete.tar"), "wb").write(buf.getvalue())
    # zip con el campo UT (el zip de Info-ZIP lo pone; TZ=UTC para la hora del DOS)
    zd = os.path.join(base, "zipsrc")
    shutil.rmtree(zd, ignore_errors=True)
    os.makedirs(zd)
    z = os.path.join(zd, "z.txt")
    open(z, "w").write("dentro del zip\n")
    os.utime(z, (T_ZIP, T_ZIP))
    zipf = os.path.join(f, "paquete.zip")
    subprocess.run(["zip", "-q", zipf, "z.txt"], cwd=zd, check=True, env=dict(os.environ, TZ="UTC"))
    return f


def disco_extra(base, fix, con_exfat):
    img = os.path.join(base, "h343-hdb.img")
    total = 1 + sum(TAM.values()) + 1
    with open(img, "wb") as g:
        g.truncate(total * MB)
    tabela, ini, inicios = "", 2048, {}
    for n in (1, 2, 3, 4):
        tipo = {1: "83", 2: "83", 3: "c", 4: "7"}[n]
        tabela += f"start={ini}, size={TAM[n] * 2048}, type={tipo}\n"
        inicios[n] = ini
        ini += TAM[n] * 2048
    subprocess.run([herramienta("sfdisk"), "-q", img], input=tabela, text=True, check=True)
    vols = {}
    for n in (1, 2, 3, 4):
        v = os.path.join(base, f"p{n}.vol")
        with open(v, "wb") as g:
            g.truncate(TAM[n] * MB)
        if n == 1:
            subprocess.run([herramienta("mke2fs"), "-q", "-F", "-t", "ext2", "-I", "256", "-L", "FECHAS2", "-d", fix, v],
                           check=True, stdout=subprocess.DEVNULL)
        elif n == 2:
            subprocess.run([herramienta("mke2fs"), "-q", "-F", "-t", "ext4", "-L", "FECHAS4", "-d", fix, v],
                           check=True, stdout=subprocess.DEVNULL)
        elif n == 3:
            subprocess.run([herramienta("mkfs.vfat"), "-F", "32", "-n", "FECHASF", v], check=True, stdout=subprocess.DEVNULL)
        elif con_exfat:
            subprocess.run([herramienta("mkfs.exfat"), "-L", "FECHASX", v], check=True, capture_output=True)
        vols[n] = v
        subprocess.run(["dd", f"if={v}", f"of={img}", "bs=1M", f"seek={inicios[n] // 2048}", "conv=notrunc", "status=none"],
                       check=True)
    return img, inicios


def imagen():
    b = os.path.join(RAIZ, "build")
    img = os.path.join(TRAB, "h343.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", img, "--vesa", "--console", "--dados-mb", "16"]
                   + opcoes_apps(), check=True, stdout=subprocess.DEVNULL)
    return img


def mtime(m, ruta):
    out = cmd(m, f"stat -c 'Y=%Y' {ruta}", 30)
    r = re.findall(r"\nY=(\d+)", out)
    return int(r[-1]) if r else None


def comprobar(m, nombre, punto, url, fat, escribible):
    """Lo de cada sistema de archivos. Devuelve lo que el anfitrión debe ver después."""
    esperado = (lambda t: fs_de_2s(t)) if fat else (lambda t: t)
    d = f"{punto}/h343"
    cmd(m, f"mkdir -p {d}", 30)
    # touch -d @N y stat
    out = cmd(m, f"touch -d @{T_TOUCH} {d}/t.txt; echo cod=$?", 30)
    verificar("cod=0" in out, f"[{nombre}] touch -d @{T_TOUCH} termina bien ({out[-200:]})")
    verificar(mtime(m, f"{d}/t.txt") == esperado(T_TOUCH), f"[{nombre}] stat lee la fecha de touch -d")
    # touch -t (hora local) y la salida normal de stat
    out = cmd(m, f"touch -t {TOCAR_T} {d}/tt.txt; stat {d}/tt.txt", 30)
    seg = "08" if fat else "09"
    verificar(f"Modificado: 2021-03-04 05:06:{seg}" in out, f"[{nombre}] touch -t y stat: 2021-03-04 05:06:{seg} ({out[-300:]})")
    # cp -p
    cmd(m, f"cp -p /e2/ref.txt {d}/ref.txt", 30)
    verificar(mtime(m, f"{d}/ref.txt") == T_REF, f"[{nombre}] cp -p conserva la fecha")
    cmd(m, f"cp /e2/ref.txt {d}/refsinp.txt", 30)
    verificar((mtime(m, f"{d}/refsinp.txt") or 0) > T_REF + 10**7, f"[{nombre}] cp sin -p pone la hora actual")
    # renombrar dentro del mismo sistema
    cmd(m, f"mv {d}/tt.txt {d}/tt2.txt", 30)
    out = cmd(m, f"stat {d}/tt2.txt", 30)
    verificar(f"Modificado: 2021-03-04 05:06:{seg}" in out, f"[{nombre}] mv (renombrar) conserva la fecha ({out[-200:]})")
    # tar y unzip
    out = cmd(m, f"tar xf /e2/paquete.tar -C {d}/tx; echo cod=$?", 60)
    verificar("cod=0" in out, f"[{nombre}] tar x termina bien ({out[-200:]})")
    verificar(mtime(m, f"{d}/tx/dtar/a.txt") == esperado(T_TAR_A), f"[{nombre}] tar x conserva la fecha del fichero")
    verificar(mtime(m, f"{d}/tx/dtar") == esperado(T_TAR_D), f"[{nombre}] tar x conserva la fecha de la carpeta")
    out = cmd(m, f"unzip -q -o /e2/paquete.zip -d {d}/zx; echo cod=$?", 60)
    verificar("cod=0" in out, f"[{nombre}] unzip termina bien ({out[-200:]})")
    verificar(mtime(m, f"{d}/zx/z.txt") == esperado(T_ZIP), f"[{nombre}] unzip conserva la fecha (campo UT)")
    # rsync -t: bajar
    out = cmd(m, f"rsync -rt {url}/datos/ {d}/rs/; echo cod=$?", 240)
    verificar("\ncod=0\n" in out, f"[{nombre}] rsync -rt rsync://.../datos/ baja bien ({out[-300:]})")
    verificar(mtime(m, f"{d}/rs/baja.txt") == esperado(T_RS_BAJA), f"[{nombre}] rsync -t (bajar) conserva la fecha")
    # rsync -t: subir
    cmd(m, f"mkdir -p {d}/sube; echo para subir > {d}/sube/sube.txt; touch -d @{T_RS_SUBE} {d}/sube/sube.txt", 30)
    out = cmd(m, f"rsync -rt {d}/sube/ {url}/subida/{nombre}/; echo cod=$?", 240)
    verificar("\ncod=0\n" in out, f"[{nombre}] rsync -rt ... rsync://.../subida/ sube bien ({out[-300:]})")
    # escribir actualiza la fecha
    cmd(m, f"echo mas >> {d}/t.txt", 30)
    verificar((mtime(m, f"{d}/t.txt") or 0) > T_TOUCH + 10**7, f"[{nombre}] escribir actualiza la fecha de modificación")
    cmd(m, f"touch -d @{T_TOUCH} {d}/t.txt", 30)   # para que el anfitrión la compruebe
    return {"t.txt": esperado(T_TOUCH), "ref.txt": T_REF, "tx/dtar/a.txt": esperado(T_TAR_A),
            "zx/z.txt": esperado(T_ZIP), "rs/baja.txt": esperado(T_RS_BAJA)}


def debugfs_mtime(vol, ruta):
    r = subprocess.run([herramienta("debugfs"), "-R", f"stat {ruta}", vol], capture_output=True, text=True).stdout
    x = re.search(r"mtime: 0x([0-9a-f]+)", r)
    return int(x.group(1), 16) if x else None


def mcopy_mtime(vol, ruta, base):
    out = os.path.join(base, "mcopy.out")
    if os.path.exists(out):
        os.unlink(out)
    subprocess.run(["mcopy", "-m", "-n", "-i", vol, "::" + ruta, out], check=True, env=dict(os.environ, TZ="UTC", MTOOLS_SKIP_CHECK="1"))
    return int(os.stat(out).st_mtime)


def exfat_mtime(vol, ruta):
    x = ExFat(vol)
    e = x.achar(ruta)
    x.f.close()
    if not e:
        return None
    t = e.mod
    ano, mes, dia = 1980 + (t >> 25), (t >> 21) & 15, (t >> 16) & 31
    h, mi, s = (t >> 11) & 31, (t >> 5) & 63, (t & 31) * 2
    import calendar
    local = calendar.timegm((ano, mes, dia, h, mi, s, 0, 0, 0)) + e.mod_cs // 100
    if e.mod_desv & 0x80:
        o = e.mod_desv & 0x7F
        if o & 0x40:
            o -= 128
        local -= o * 900
    return local


def main():
    os.makedirs(TRAB, exist_ok=True)
    if not shutil.which("rsync"):
        print("NO APLICA: falta rsync en el anfitrión (sudo apt install rsync)")
        return 77
    con_exfat = bool(hay("mkfs.exfat") and hay("fsck.exfat"))
    base = os.path.join(TRAB, "pc")
    shutil.rmtree(base, ignore_errors=True)
    os.makedirs(base)
    fix = preparar_fixtures(base)
    hdb, inicios = disco_extra(base, fix, con_exfat)
    # el demonio: «datos» con un fichero de fecha conocida; «subida» con una carpeta por sistema
    org = os.path.join(base, "datos")
    os.makedirs(org)
    bf = os.path.join(org, "baja.txt")
    open(bf, "w").write("para bajar con rsync -t\n")
    os.utime(bf, (T_RS_BAJA, T_RS_BAJA))
    sub = os.path.join(base, "subida")
    os.makedirs(sub)
    for n in ("ext2", "ext4", "fat", "exfat", "tmp", "home"):
        os.makedirs(os.path.join(sub, n))
    puerto = h339.puerto_libre()
    demonio = h339.lanzar_demonio(base, puerto)
    url = f"rsync://10.0.2.2:{puerto}"
    esperados = {}
    ext4_rw = False
    img = imagen()
    try:
        extras = ["-nic", "user,model=e1000", "-drive", f"file={hdb},if=ide,index=1,format=raw"]
        with Maquina("h343", "ide", imagem=img, snapshot=True, extras=extras) as m:
            m.esperar_serial("sagui:/$", 180)
            out = cmd(m, "mount -o rw hdb1 /e2; mount -o rw hdb2 /e4; mount -o rw hdb3 /fat; date", 60)
            print("MONTAJES:", out[-400:])
            verificar("UTC" in out, f"la máquina virtual está en UTC (la hora del FAT se compara así) ({out[-200:]})")
            if con_exfat:
                cmd(m, "mount -o rw hdb4 /exf", 60)
            out = cmd(m, "ls /e2 /fat; mount", 30)
            verificar("paquete.tar" in out, f"el ext2 del anfitrión se ve en /e2 ({out[-300:]})")
            out = cmd(m, "touch /e4/sonda.txt; echo cod=$?", 30)
            ext4_rw = "cod=0" in out
            for nombre, n, punto in PARTES:
                if nombre == "ext4" and not ext4_rw:
                    out = cmd(m, f"touch -d @{T_TOUCH} /e4/ref.txt; echo cod=$?", 30)
                    verificar("cod=0" not in out and "lectura" in out, f"[ext4 de solo lectura] fijar la fecha da error ({out[-200:]})")
                    verificar(mtime(m, "/e4/ref.txt") == T_REF, "[ext4 de solo lectura] la fecha no cambia")
                    print("  ext4: de solo lectura en esta rama (D326, ESCRITA_EXT34=0): solo se comprueba el error")
                    continue
                if nombre == "exfat" and not con_exfat:
                    print("  exFAT: no aplica (faltan mkfs.exfat/fsck.exfat)")
                    continue
                esperados[nombre] = comprobar(m, nombre, punto, url, nombre == "fat", True)
            esperados["tmp"] = comprobar(m, "tmp", "/tmp", url, False, True)
            esperados["home"] = comprobar(m, "home", "/home", url, False, True)
            # mv entre sistemas (cp -rp y borrar): la fecha se conserva
            out = cmd(m, "cp -p /e2/ref.txt /tmp/mvsrc.txt; mv /tmp/mvsrc.txt /fat/h343/mv.txt; ls /tmp/mvsrc.txt; echo fin", 60)
            verificar(mtime(m, "/fat/h343/mv.txt") == T_REF, "mv de la RAM al FAT conserva la fecha")
            # permisos: sagui no fija una fecha concreta en lo de root, pero sí «ahora» si puede escribirlo
            cmd(m, "echo x > /tmp/deroot.txt; chmod 666 /tmp/deroot.txt; touch -d @1000000000 /tmp/deroot.txt", 30)
            out = cmd(m, "su sagui -c 'touch -d @1100000000 /tmp/deroot.txt'; echo cod=$?", 30)
            verificar(mtime(m, "/tmp/deroot.txt") == 1000000000, f"sagui no pone una fecha concreta a un fichero de root ({out[-200:]})")
            cmd(m, "su sagui -c 'touch /tmp/deroot.txt'", 30)
            verificar((mtime(m, "/tmp/deroot.txt") or 0) > 1700000000, "sagui sí pone «ahora» a un fichero de root que puede escribir")
            cmd(m, "chmod 644 /tmp/deroot.txt; touch -d @1000000000 /tmp/deroot.txt", 30)
            cmd(m, "su sagui -c 'touch /tmp/deroot.txt'", 30)
            verificar(mtime(m, "/tmp/deroot.txt") == 1000000000, "ni «ahora» si no lo puede escribir")
            out = cmd(m, "su sagui -c 'touch -d @1100000000 /home/h343/t.txt; echo cod=$?'", 30)
            verificar("cod=0" in out and mtime(m, "/home/h343/t.txt") == 1100000000, f"sagui fija la fecha de lo suyo en /home ({out[-200:]})")
            cmd(m, f"touch -d @{T_TOUCH} /home/h343/t.txt", 30)
            out = cmd(m, "umount /e2; umount /e4; umount /fat; umount /exf; sync; echo listo", 120)
            print("DESMONTAR:", out[-300:])
    finally:
        demonio.send_signal(signal.SIGTERM)
        demonio.wait(10)

    # rsync -t al subir: el anfitrión tiene las fechas
    for n, esp in esperados.items():
        t = int(os.stat(os.path.join(sub, n, "sube.txt")).st_mtime)
        e = fs_de_2s(T_RS_SUBE) if n == "fat" else T_RS_SUBE
        verificar(t == e, f"[{n}] rsync -t (subir): el anfitrión ve {t} (esperado {e})")

    # el anfitrión lee los volúmenes
    p1 = extrair_particao(hdb, inicios[1], TAM[1], os.path.join(base, "q1.vol"))
    r = subprocess.run([herramienta("e2fsck"), "-fn", p1], capture_output=True, text=True)
    verificar(r.returncode == 0, f"e2fsck -fn del ext2 limpio ({(r.stdout + r.stderr)[-300:]})")
    for ruta, t in esperados["ext2"].items():
        v = debugfs_mtime(p1, "/h343/" + ruta)
        verificar(v == t, f"[ext2] debugfs ve {ruta} con {v} (esperado {t})")
    p2 = extrair_particao(hdb, inicios[2], TAM[2], os.path.join(base, "q2.vol"))
    r = subprocess.run([herramienta("e2fsck"), "-fn", p2], capture_output=True, text=True)
    verificar(r.returncode == 0, f"e2fsck -fn del ext4 limpio ({(r.stdout + r.stderr)[-300:]})")
    if ext4_rw:
        for ruta, t in esperados["ext4"].items():
            v = debugfs_mtime(p2, "/h343/" + ruta)
            verificar(v == t, f"[ext4] debugfs ve {ruta} con {v} (esperado {t})")
    else:
        verificar(debugfs_mtime(p2, "/ref.txt") == T_REF, "[ext4] la fecha de ref.txt sigue intacta")
    p3 = extrair_particao(hdb, inicios[3], TAM[3], os.path.join(base, "q3.vol"))
    r = subprocess.run([herramienta("fsck.vfat"), "-n", p3], capture_output=True, text=True)
    verificar(r.returncode == 0, f"fsck.vfat -n limpio ({(r.stdout + r.stderr)[-300:]})")
    for ruta, t in list(esperados["fat"].items()) + [("mv.txt", T_REF)]:
        v = mcopy_mtime(p3, "/h343/" + ruta, base)
        verificar(v == t, f"[fat] mcopy -m ve {ruta} con {v} (esperado {t}, resolución de 2 s)")
    if con_exfat:
        p4 = extrair_particao(hdb, inicios[4], TAM[4], os.path.join(base, "q4.vol"))
        r = subprocess.run([herramienta("fsck.exfat"), "-n", p4], capture_output=True, text=True)
        verificar(r.returncode == 0, f"fsck.exfat -n limpio ({(r.stdout + r.stderr)[-300:]})")
        for ruta, t in esperados["exfat"].items():
            v = exfat_mtime(p4, "/h343/" + ruta)
            verificar(v == t, f"[exfat] el anfitrión ve {ruta} con {v} (esperado {t})")
    print("H343 correcto")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
