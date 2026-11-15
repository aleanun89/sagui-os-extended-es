#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H314 (D314, mejora 120) - rsync: servidor por SSH (openrsync, protocolo 27) y copia local.

En QEMU (red de usuario con hostfwd al puerto 22), con el `rsync` y el `ssh` del anfitrión y
una clave ed25519 creada para la prueba (en el /etc/ssh/authorized_keys de la imagen), y la
partición SAGUI-APPS (rsync vive en /apps/bin):
  - subir una carpeta con subcarpetas, un fichero de 1 MB y ficheros de texto: `rsync -av -e ssh`;
    las sumas en SaguiOS (sha256sum) son las del anfitrión;
  - cambiar 10 bytes del fichero de 1 MB y volver a subir: con --stats, lo enviado es una
    fracción pequeña del fichero (el delta funciona);
  - bajar la carpeta de vuelta: idéntica;
  - subir con --delete: borra lo sobrante del destino;
  - copia local dentro de SaguiOS: rsync -a, --dry-run (no toca nada), --delete.
  - v8.4.2: el CLIENTE rsync sobre ssh desde SaguiOS (`rsync -av carpeta/ usuario@host:ruta/` y
    al revés) contra el sshd de SaguiOS por 127.0.0.1 (el anfitrión no suele tener sshd sin root):
    un fichero de 300 KB (mucho más que las tuberías de 512 bytes del kernel: no hay interbloqueo),
    delta con 10 bytes cambiados, bajar, --delete, los modos (chmod 600/755 se conservan) y el
    cliente rechaza rsync:// y origen y destino remotos a la vez. Esta parte no necesita rsync
    ni ssh en el anfitrión.
Si el anfitrión no tiene rsync (o ssh) la prueba se marca «no aplica» y no falla.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste, opcoes_apps  # noqa: E402
import h248_ssh as h248  # noqa: E402  (la imagen con clave y el cliente de OpenSSH)

TRAB = os.path.join(SAIDA, "h314")
h248.TRAB = TRAB


def suma(ruta):
    return hashlib.sha256(open(ruta, "rb").read()).hexdigest()


def arbol(raiz):
    """{ruta relativa: suma o None (carpeta)} de un árbol."""
    r = {}
    for d, ds, fs in os.walk(raiz):
        for n in ds:
            r[os.path.relpath(os.path.join(d, n), raiz)] = None
        for n in fs:
            p = os.path.join(d, n)
            r[os.path.relpath(p, raiz)] = suma(p)
    return r


def construir_imagen(pub):
    """Como la de h248, más la partición de /apps (rsync) y un disco de datos para /home."""
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    with open(os.path.join(raiz, "etc", "ssh", "authorized_keys"), "w") as f:
        f.write("# clave de la prueba h314\n" + open(pub).read())
    sgfs = os.path.join(TRAB, "h314.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h314.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console", "--dados-mb", "16"]
                   + opcoes_apps(), check=True, stdout=subprocess.DEVNULL)
    return img


def preparar_origen(base):
    shutil.rmtree(base, ignore_errors=True)
    org = os.path.join(base, "carpeta")
    os.makedirs(os.path.join(org, "sub", "profundo"))
    os.makedirs(os.path.join(org, "vacia"))
    open(os.path.join(org, "hola.txt"), "w").write("hola desde el PC\n" * 20)
    open(os.path.join(org, "sub", "notas.txt"), "w").write("notas de prueba\n" * 100)
    open(os.path.join(org, "sub", "profundo", "datos.txt"), "w").write("0123456789\n" * 300)
    open(os.path.join(org, "grande.bin"), "wb").write(os.urandom(1024 * 1024))
    return org


def maquina():
    chave = h248.gerar_chave("clave_h314")
    img = construir_imagen(chave + ".pub")
    porta = h248.puerto_libre()
    ssh = (f"ssh -p {porta} -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
           f"-o IdentitiesOnly=yes -o IdentityAgent=none -o BatchMode=yes -o LogLevel=ERROR "
           f"-o ConnectTimeout=20 -i {chave}")
    cl = h248.Cliente(porta, chave)
    base = os.path.join(TRAB, "pc")
    org = preparar_origen(base)
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]

    def rsync(*args, tempo=240):
        return subprocess.run(["rsync", *args], capture_output=True, text=True, timeout=tempo, stdin=subprocess.DEVNULL)

    with Maquina("h314", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        h248.esperar_log(m, "10.0.2.15 (DHCP)", 30)
        out = m.comando("which rsync")
        verificar("/apps/bin/rsync" in out, f"rsync está en /apps/bin ({out.strip()[-60:]})")
        out = m.comando("servicio activar sshd", 30) + m.comando("servicio iniciar sshd", 30)
        h248.esperar_log(m, "sshd: escuchando en el puerto 22 (solo red local)", 40)

        # --- subir
        r = rsync("-av", "--stats", "-e", ssh, org + "/", "root@127.0.0.1:/home/h314/")
        verificar(r.returncode == 0, f"rsync -av -e ssh carpeta/ root@saguios:/home/h314/ termina bien "
                  f"(código {r.returncode}) {r.stderr[-400:]} {r.stdout[-300:]}")
        for rel, esperado in (("grande.bin", suma(os.path.join(org, "grande.bin"))),
                              ("sub/notas.txt", suma(os.path.join(org, "sub", "notas.txt"))),
                              ("sub/profundo/datos.txt", suma(os.path.join(org, "sub", "profundo", "datos.txt"))),
                              ("hola.txt", suma(os.path.join(org, "hola.txt")))):
            out = m.comando(f"sha256sum /home/h314/{rel}", 60)
            verificar(esperado in out, f"subida: la suma de {rel} en SaguiOS es la del PC")
        out = m.comando("ls /home/h314", 30)
        verificar("vacia" in out and "sub" in out, "subida: también las carpetas (con la vacía)")
        n1 = re.search(r"Total bytes sent: ([\d,.]+)", r.stdout)
        enviados1 = int(re.sub(r"[,.]", "", n1.group(1))) if n1 else 0
        verificar(enviados1 > 1000000, f"la primera subida manda el fichero entero ({enviados1} bytes)")

        # --- delta: 10 bytes cambiados en el fichero de 1 MB
        g = os.path.join(org, "grande.bin")
        datos = bytearray(open(g, "rb").read())
        datos[500000:500010] = bytes(b ^ 0xFF for b in datos[500000:500010])
        open(g, "wb").write(datos)
        r = rsync("-av", "--stats", "-e", ssh, org + "/", "root@127.0.0.1:/home/h314/")
        verificar(r.returncode == 0, f"segunda subida termina bien (código {r.returncode}) {r.stderr[-400:]}")
        n2 = re.search(r"Total bytes sent: ([\d,.]+)", r.stdout)
        enviados2 = int(re.sub(r"[,.]", "", n2.group(1))) if n2 else 10**9
        verificar(enviados2 < 40000, f"el delta funciona: con 10 bytes cambiados se mandan {enviados2} bytes (< 40000)")
        out = m.comando("sha256sum /home/h314/grande.bin", 60)
        verificar(suma(g) in out, "tras el delta, la suma del fichero de 1 MB en SaguiOS es la nueva")

        # --- bajar
        bajada = os.path.join(base, "bajada")
        shutil.rmtree(bajada, ignore_errors=True)
        r = rsync("-av", "-e", ssh, "root@127.0.0.1:/home/h314/", bajada + "/")
        verificar(r.returncode == 0, f"rsync -av -e ssh root@saguios:/home/h314/ bajada/ termina bien "
                  f"(código {r.returncode}) {r.stderr[-400:]}")
        verificar(arbol(bajada) == arbol(org), "la carpeta bajada es idéntica a la subida")

        # --- --delete
        cl.ssh("echo sobra > /home/h314/sobra.txt")
        out = m.comando("ls /home/h314", 30)
        verificar("sobra.txt" in out, "se creó un fichero sobrante en el destino")
        r = rsync("-av", "--delete", "-e", ssh, org + "/", "root@127.0.0.1:/home/h314/")
        verificar(r.returncode == 0, f"rsync --delete termina bien (código {r.returncode}) {r.stderr[-400:]}")
        out = m.comando("ls /home/h314", 30)
        verificar("sobra.txt" not in out and "hola.txt" in out, "--delete borra lo sobrante y deja lo demás")

        # --- copia local dentro de SaguiOS
        out = m.comando("rsync -a --dry-run -v /home/h314/ /home/copia/", 60)
        verificar("hola.txt" in out and "grande.bin" in out, f"copia local --dry-run lista lo que copiaría")
        out = m.comando("ls /home/copia", 30)
        verificar("hola.txt" not in out, "--dry-run no copia nada")
        out = m.comando("rsync -a /home/h314/ /home/copia/", 120)
        a = m.comando("sha256sum /home/copia/grande.bin /home/copia/sub/profundo/datos.txt", 60)
        verificar(suma(g) in a and suma(os.path.join(org, "sub", "profundo", "datos.txt")) in a,
                  "copia local: las sumas coinciden")
        m.comando("echo basura > /home/copia/extra.txt", 30)
        out = m.comando("rsync -a --delete --dry-run -v /home/h314/ /home/copia/", 60)
        verificar("deleting extra.txt" in out, "copia local --delete --dry-run anuncia el borrado")
        out = m.comando("ls /home/copia", 30)
        verificar("extra.txt" in out, "--dry-run no borra")
        m.comando("rsync -a --delete /home/h314/ /home/copia/", 60)
        out = m.comando("ls /home/copia", 30)
        verificar("extra.txt" not in out and "hola.txt" in out, "copia local --delete borra lo sobrante")


def esperar_serial(m, texto, desde, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return m.serial()[desde:]
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció {texto!r} en la serie: {m.serial()[desde:][-800:]!r}")


def comando_ate(m, texto, tempo=120):
    """Como m.comando, pero espera a una marca propia (el registro del sshd también contiene «sagui:»)."""
    marca = len(m.serial())
    m.digitar(texto + "; echo fin-de-$((300+14))\n", 0.04)
    s = esperar_serial(m, "\nfin-de-314", marca, tempo)
    return s.replace("\r", "")


def construir_imagen_cliente():
    b = os.path.join(RAIZ, "build")
    img = os.path.join(TRAB, "h314c.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", img, "--vesa", "--console", "--dados-mb", "16"]
                   + opcoes_apps(), check=True, stdout=subprocess.DEVNULL)
    return img


def cliente():
    """El cliente rsync de SaguiOS sobre su propio ssh, contra el sshd de SaguiOS por 127.0.0.1."""
    img = construir_imagen_cliente()
    ssh_e = '-e "ssh -o StrictHostKeyChecking=accept-new"'
    with Maquina("h314c", "ide", imagem=img, snapshot=True, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("ssh-keygen -t ed25519 -N ''", 60)
        verificar("/home/.ssh/id_ed25519.pub" in out, "ssh-keygen crea la clave del cliente")
        m.comando("cp /home/.ssh/id_ed25519.pub /home/.ssh/authorized_keys", 30)
        m.comando("servicio activar sshd", 30)
        m.comando("servicio iniciar sshd", 30)
        fim = time.time() + 40
        while time.time() < fim and "escuchando en el puerto 22" not in m.serial():
            time.sleep(0.3)
        # primera conexión a mano: guarda la huella (con la entrada ocupada por el protocolo no se puede preguntar)
        out = comando_ate(m, "ssh -o StrictHostKeyChecking=accept-new sagui@127.0.0.1 'echo listo'", 90)
        verificar("\nlisto\n" in out, "ssh a 127.0.0.1 funciona (clave pública)")

        # origen: subcarpetas, un fichero de 300 KB (unas 600 tuberías de 512 bytes) y modos distintos
        comando_ate(m, "mkdir -p /home/org/sub /home/org/vacia", 30)
        comando_ate(m, "yes 'linea de relleno para el rsync cliente 0123456789abcdefghijklmnopqrstuvwxyz' | head -n 4000 > /home/org/grande.txt", 90)
        comando_ate(m, "echo hola > /home/org/hola.txt; echo notas > /home/org/sub/notas.txt", 30)
        comando_ate(m, "chmod 600 /home/org/hola.txt; chmod 755 /home/org/sub/notas.txt", 30)
        suma_g = comando_ate(m, "sha256sum /home/org/grande.txt", 90)
        h = re.search(r"([0-9a-f]{64})", suma_g)
        verificar(h is not None, "el fichero de origen existe")
        h = h.group(1)

        # --- subir
        t0 = time.time()
        out = comando_ate(m, f"rsync -av {ssh_e} /home/org/ sagui@127.0.0.1:/home/dst/; echo codigo=$?", 240)
        verificar("\ncodigo=0\n" in out, f"rsync -av carpeta/ sagui@127.0.0.1:/home/dst/ termina bien ({out[-500:]})")
        out = comando_ate(m, "sha256sum /home/dst/grande.txt; ls /home/dst; ls /home/dst/sub", 90)
        verificar(h in out and "vacia" in out and "notas.txt" in out, "subida: la suma del fichero grande y las carpetas llegan")
        verificar(time.time() - t0 < 200, f"la subida no se interbloquea ({time.time() - t0:.0f} s)")

        # --- modos (D282): se conservan con -a
        out = comando_ate(m, "ls -l /home/dst/hola.txt /home/dst/sub/notas.txt", 30)
        verificar(re.search(r"-rw[x-]------ .*hola\.txt", out) is not None, f"el modo 600 de hola.txt se conserva ({out[-300:]})")
        verificar(re.search(r"-rwxr-xr-x.*notas\.txt", out) is not None, f"el modo 755 de notas.txt se conserva ({out[-300:]})")

        # --- delta: unos bytes añadidos al final -> se manda poco
        comando_ate(m, "echo cambio-al-final-0123456789 >> /home/org/grande.txt", 60)
        suma2 = comando_ate(m, "sha256sum /home/org/grande.txt", 90)
        h2 = re.search(r"([0-9a-f]{64})", suma2).group(1)
        verificar(h2 != h, "el fichero de origen cambió")
        out = comando_ate(m, f"rsync -av {ssh_e} /home/org/ sagui@127.0.0.1:/home/dst/; echo codigo=$?", 240)
        verificar("\ncodigo=0\n" in out, f"segunda subida termina bien ({out[-400:]})")
        n = re.search(r"([\d.]+) (B|KB|MB) sent", out)
        enviado = float(n.group(1)) * {"B": 1, "KB": 1024, "MB": 1048576}[n.group(2)] if n else 10**9
        verificar(enviado < 60000, f"el delta funciona por el cliente: se mandan {enviado:.0f} bytes de 320000")
        out = comando_ate(m, "sha256sum /home/dst/grande.txt", 90)
        verificar(h2 in out, "tras el delta, el destino tiene el contenido nuevo")

        # --- bajar
        out = comando_ate(m, f"rsync -av {ssh_e} sagui@127.0.0.1:/home/dst/ /home/back/; echo codigo=$?", 240)
        verificar("\ncodigo=0\n" in out, f"rsync -av sagui@127.0.0.1:/home/dst/ /home/back/ termina bien ({out[-500:]})")
        out = comando_ate(m, "sha256sum /home/back/grande.txt; ls /home/back/sub /home/back", 90)
        verificar(h2 in out and "notas.txt" in out and "vacia" in out, "bajada: llegan el fichero grande y las carpetas")
        out = comando_ate(m, "ls -l /home/back/hola.txt", 30)
        verificar(re.search(r"-rw[x-]------ .*hola\.txt", out) is not None, f"bajada: el modo 600 se conserva ({out[-200:]})")
        comando_ate(m, "rsync -a /home/org/ /home/loc/", 120)
        out = comando_ate(m, "ls -l /home/loc/hola.txt", 30)
        verificar(re.search(r"-rw[x-]------ .*hola\.txt", out) is not None, f"copia local con -a: el modo 600 se conserva ({out[-200:]})")

        # --- --delete
        comando_ate(m, "echo sobra > /home/dst/sobra.txt", 30)
        out = comando_ate(m, f"rsync -a --delete {ssh_e} /home/org/ sagui@127.0.0.1:/home/dst/; echo codigo=$?; ls /home/dst", 240)
        verificar("\ncodigo=0\n" in out and "sobra.txt" not in out.split("codigo=0")[1] and "hola.txt" in out,
                  f"--delete por el cliente borra lo sobrante ({out[-300:]})")

        # --- errores claros
        out = comando_ate(m, "rsync -a rsync://127.0.0.1/mod /home/x/; echo codigo=$?", 30)
        verificar("rsync://" in out and "\ncodigo=1\n" in out, "rsync:// se rechaza con un mensaje")
        out = comando_ate(m, f"rsync -a {ssh_e} a@127.0.0.1:/home/dst/ b@127.0.0.1:/home/x/; echo codigo=$?", 30)
        verificar("remoto a remoto" in out and "\ncodigo=1\n" in out, "remoto a remoto se rechaza")
        out = comando_ate(m, f"rsync -a {ssh_e} /home/org/ sagui@127.0.0.1:/home/dst2/ --rsync-path=noexiste; echo codigo=$?", 90)
        verificar("\ncodigo=1\n" in out, "sin rsync en el otro lado, el cliente termina con error (no se cuelga)")
        m.captura("H314-rsync-cliente", guardar=False)


def main():
    os.makedirs(TRAB, exist_ok=True)
    cliente()
    for prog in ("rsync", "ssh", "ssh-keygen"):
        if not shutil.which(prog):
            print(f"NO APLICA: falta {prog} en el anfitrión (la parte del servidor de h314 necesita rsync y OpenSSH)")
            print("H314 correcto (solo el cliente)")
            return 0
    maquina()
    print("H314 correcto")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
