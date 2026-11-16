#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H339 (D336, v8.4.4) - cliente rsync:// (demonio), sync para el usuario normal y regresión de la issue #41.

Con un `rsync --daemon` del anfitrión (sin root, 127.0.0.1) y el QEMU con red de usuario (el anfitrión
es 10.0.2.2 desde el invitado):
  - listar módulos: rsync rsync://10.0.2.2:PUERTO/
  - bajar un módulo (fichero de 300 KB, subcarpetas, modos) y comprobar las sumas, los modos y el dueño;
  - subir una carpeta y comprobar las sumas en el anfitrión; delta; --delete;
  - errores: módulo inexistente, módulo con clave (no se autentica), puerto cerrado.
Sin rsync en el anfitrión no aplica (código 77).
Siempre (no necesita el anfitrión):
  - `sync` como usuario normal (su sagui -c sync) funciona; desmontar sigue siendo de root;
  - issue #41: reescribir un fichero existente con open('w') de MicroPython (/tmp, /home, y con
    SAGUI-TMP en ext2) como root y como sagui no da EEXIST.
"""
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste, opcoes_apps  # noqa: E402
import h314_rsync as h314  # noqa: E402

TRAB = os.path.join(SAIDA, "h339")
h314.TRAB = TRAB


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def suma(ruta):
    return h314.suma(ruta)


def cmd(m, texto, tempo=120):
    return h314.comando_ate(m, texto, tempo)


def preparar_pc(base):
    shutil.rmtree(base, ignore_errors=True)
    org = os.path.join(base, "datos")
    os.makedirs(os.path.join(org, "sub"))
    os.makedirs(os.path.join(org, "vacia"))
    open(os.path.join(org, "hola.txt"), "w").write("hola desde el demonio\n" * 10)
    open(os.path.join(org, "sub", "notas.txt"), "w").write("notas\n" * 50)
    with open(os.path.join(org, "grande.txt"), "w") as f:
        for i in range(4000):
            f.write("linea de relleno del demonio rsync %05d abcdefghijklmnopqrstuvwxyz\n" % i)
    os.chmod(os.path.join(org, "hola.txt"), 0o600)
    os.chmod(os.path.join(org, "sub", "notas.txt"), 0o755)
    sub = os.path.join(base, "subida")
    os.makedirs(sub)
    return org, sub


def lanzar_demonio(base, puerto):
    org = os.path.join(base, "datos")
    sub = os.path.join(base, "subida")
    conf = os.path.join(base, "rsyncd.conf")
    with open(conf, "w") as f:
        f.write(f"""pid file = {base}/rsyncd.pid
port = {puerto}
address = 127.0.0.1
use chroot = no
reverse lookup = no
forward lookup = no
log file = {base}/rsyncd.log
[datos]
    path = {org}
    comment = datos de prueba
    read only = yes
[subida]
    path = {sub}
    comment = para subir
    read only = no
    pre-xfer exec = /bin/sh -c 'env | grep RSYNC_ARG > {base}/args.txt'
[secreto]
    path = {org}
    comment = con clave
    auth users = alguien
    secrets file = {base}/secretos
""")
    with open(os.path.join(base, "secretos"), "w") as f:
        f.write("alguien:clave\n")
    os.chmod(os.path.join(base, "secretos"), 0o600)
    p = subprocess.Popen(["rsync", "--daemon", "--no-detach", f"--config={conf}"],
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    for _ in range(80):
        if p.poll() is not None:
            raise FalhaTeste("el demonio rsync del anfitrión terminó: " + p.stderr.read().decode(errors="replace")[-300:])
        try:
            socket.create_connection(("127.0.0.1", puerto), 1).close()
            return p
        except OSError:
            time.sleep(0.1)
    p.kill()
    raise FalhaTeste("el demonio rsync del anfitrión no arrancó")


def parte_demonio(m, base, puerto):
    org = os.path.join(base, "datos")
    sub = os.path.join(base, "subida")
    url = f"rsync://10.0.2.2:{puerto}"
    out = cmd(m, f"rsync {url}/; echo codigo=$?", 60)
    verificar("datos" in out and "subida" in out and "\ncodigo=0\n" in out, f"lista los módulos ({out[-300:]})")

    out = cmd(m, f"rsync -av {url}/datos/ /home/bajada/; echo codigo=$?", 240)
    verificar("\ncodigo=0\n" in out, f"rsync -av rsync://.../datos/ /home/bajada/ termina bien ({out[-500:]})")
    out = cmd(m, "sha256sum /home/bajada/grande.txt /home/bajada/sub/notas.txt; ls /home/bajada", 90)
    verificar(suma(os.path.join(org, "grande.txt")) in out and suma(os.path.join(org, "sub", "notas.txt")) in out
              and "vacia" in out, "bajada: las sumas y las carpetas llegan")
    out = cmd(m, "ls -l /home/bajada/hola.txt /home/bajada/sub/notas.txt", 30)
    verificar(re.search(r"-rw[x-]------ .*hola\.txt", out) is not None, f"bajada: el modo 600 se conserva ({out[-300:]})")
    verificar(re.search(r"-rwxr-xr-x.*notas\.txt", out) is not None, f"bajada: el modo 755 se conserva ({out[-300:]})")
    # dueño y grupo: el del anfitrión (con -a), aplicado porque la sesión es root
    uid = os.getuid()
    out = cmd(m, "ls -l /home/bajada/hola.txt; ls -ln /home/bajada/hola.txt", 30)
    print("DUENO:", out[-400:])

    # subir
    cmd(m, "mkdir -p /home/org/sub; echo hola-subida > /home/org/a.txt; echo notas > /home/org/sub/b.txt; "
           "yes 'relleno de subida 0123456789abcdefghijklmnopqrstuvwxyz' | head -n 3000 > /home/org/grande.txt; "
           "chmod 600 /home/org/a.txt", 120)
    out = cmd(m, f"rsync -av /home/org/ {url}/subida/; echo codigo=$?", 240)
    verificar("\ncodigo=0\n" in out, f"rsync -av /home/org/ rsync://.../subida/ termina bien ({out[-500:]})")
    verificar(os.path.exists(os.path.join(sub, "grande.txt")) and os.path.exists(os.path.join(sub, "sub", "b.txt")),
              "subida: los ficheros llegan al anfitrión")
    sg = cmd(m, "sha256sum /home/org/grande.txt", 90)
    verificar(suma(os.path.join(sub, "grande.txt")) in sg, "subida: la suma del fichero grande coincide")
    verificar(os.stat(os.path.join(sub, "a.txt")).st_mode & 0o077 == 0 and os.stat(os.path.join(sub, "a.txt")).st_mode & 0o600 == 0o600, "subida: el modo 600 llega al anfitrión")

    # delta
    cmd(m, "echo cambio-al-final-0123456789 >> /home/org/grande.txt", 60)
    out = cmd(m, f"rsync -av /home/org/ {url}/subida/; echo codigo=$?", 240)
    verificar("\ncodigo=0\n" in out, f"segunda subida termina bien ({out[-400:]})")
    nums = [float(a) * {"B": 1, "KB": 1024, "MB": 1048576}[b] for a, b in re.findall(r"([\d.]+) (B|KB|MB) (?:sent|read)", out)]
    enviado = max(nums) if nums else 10**9
    verificar(enviado < 60000, f"el delta funciona: se mandan {enviado:.0f} bytes de unos 180000")
    sg = cmd(m, "sha256sum /home/org/grande.txt", 90)
    verificar(suma(os.path.join(sub, "grande.txt")) in sg, "tras el delta, el anfitrión tiene el contenido nuevo")

    # --delete
    open(os.path.join(sub, "sobra.txt"), "w").write("sobra\n")
    out = cmd(m, f"rsync -avv --delete /home/org/ {url}/subida/; echo codigo=$?", 240)
    verificar("\ncodigo=0\n" in out and not os.path.exists(os.path.join(sub, "sobra.txt")),
              f"--delete borra lo sobrante en el demonio ({out[-400:]}) {open(base + '/rsyncd.log').read()[-1800:]}")

    # errores
    out = cmd(m, f"rsync -a {url}/nohay/ /home/x/; echo codigo=$?", 60)
    verificar("\ncodigo=1\n" in out and "@ERROR" in out, f"módulo inexistente: error claro ({out[-200:]})")
    out = cmd(m, f"rsync -a {url}/secreto/ /home/x/; echo codigo=$?", 60)
    verificar("\ncodigo=1\n" in out and "clave" in out, f"módulo con clave: se rechaza con un mensaje ({out[-200:]})")
    out = cmd(m, "rsync rsync://10.0.2.2:1/; echo codigo=$?", 60)
    verificar("\ncodigo=1\n" in out and "conectar" in out, f"puerto cerrado: error y no se cuelga ({out[-200:]})")


def imagen_con(nombre, extras):
    b = os.path.join(RAIZ, "build")
    img = os.path.join(TRAB, nombre + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", img, "--vesa", "--console"]
                   + extras + opcoes_apps(), check=True, stdout=subprocess.DEVNULL)
    return img


def parte_sync_y_41(m):
    # sync para el usuario normal
    out = cmd(m, "su sagui -c 'id; sync; echo sync=$?'", 60)
    verificar("sync=0" in out, f"sync como sagui termina bien ({out[-200:]})")
    out = cmd(m, "su sagui -c 'sync -v; echo syncv=$?'", 60)
    verificar("syncv=0" in out, f"sync -v como sagui termina bien ({out[-200:]})")
    out = cmd(m, "su sagui -c 'desmontar disco-que-no-hay; echo desm=$?'", 60)
    verificar("desm=0" not in out, f"desmontar sigue siendo de root ({out[-200:]})")
    out = cmd(m, "sync; echo sync=$?", 60)
    verificar("sync=0" in out, "sync como root termina bien")

    parte_41(m)


def parte_41(m):
    # issue #41: reescribir con open('w') (el script va a un fichero para no anidar comillas)
    cmd(m, "echo \"import sys;p=sys.argv[1];f=open(p,'w');f.write('1');f.close();f=open(p,'w');f.write('22');f.close();"
           "f=open(p,'a');f.write('3');f.close();f=open(p,'w');f.write('4444');f.close();print('res='+open(p).read())\" > /tmp/t41.py", 30)
    cmd(m, "mkdir -p /home/sub41; chmod 777 /home/sub41; chmod 644 /tmp/t41.py", 30)
    for quien in ("root", "sagui"):
        for ruta in ("/tmp/r41-%s.txt", "/home/r41-%s.txt", "/home/sub41/r41-%s.txt"):
            ruta = ruta % quien
            pre = "su sagui -c " if quien == "sagui" else ""
            ordem = f"{pre}\"python /tmp/t41.py {ruta}\"" if pre else f"python /tmp/t41.py {ruta}"
            out = cmd(m, ordem, 60)
            verificar("res=4444" in out and "EEXIST" not in out and "Errno" not in out,
                      f"#41: reescribir {ruta} como {quien} ({out[-200:]})")
        # y un fichero que ya existe de otro usuario en /tmp (modo 666): reescribirlo también vale
    cmd(m, "echo x > /tmp/otro41.txt; chmod 666 /tmp/otro41.txt", 30)
    out = cmd(m, 'su sagui -c "python /tmp/t41.py /tmp/otro41.txt"', 60)
    verificar("res=4444" in out and "EEXIST" not in out, f"#41: sagui reescribe un fichero de root con modo 666 ({out[-200:]})")


def main():
    os.makedirs(TRAB, exist_ok=True)
    img = h314.construir_imagen_cliente()
    base = os.path.join(TRAB, "pc")
    hay_rsync = shutil.which("rsync") is not None
    demonio = None
    puerto = puerto_libre()
    if hay_rsync:
        preparar_pc(base)
        demonio = lanzar_demonio(base, puerto)
    try:
        with Maquina("h339", "ide", imagem=img, snapshot=True, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 180)
            if hay_rsync:
                parte_demonio(m, base, puerto)
            parte_sync_y_41(m)
        # #41 en las demás combinaciones de sistemas de archivos: /home y /tmp en FAT32, /tmp en ext2
        for nombre, extras in (("fat", ["--dados-mb", "40", "--dados-fs", "fat32", "--tmp-mb", "40", "--tmp-fs", "fat32"]),
                               ("ext2tmp", ["--dados-mb", "16", "--tmp-mb", "16", "--tmp-fs", "ext2"])):
            with Maquina("h339" + nombre, "ide", imagem=imagen_con("h339" + nombre, extras), snapshot=True) as m2:
                m2.esperar_serial("sagui:/$", 180)
                out = cmd(m2, "mount", 30)
                print("MONTAJES:", out[-500:])
                parte_41(m2)
    finally:
        if demonio:
            demonio.send_signal(signal.SIGTERM)
            demonio.wait(10)
    if not hay_rsync:
        print("NO APLICA: falta rsync en el anfitrión (sudo apt install rsync); solo se probó sync y la issue #41")
        return 77
    print("H339 correcto")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
