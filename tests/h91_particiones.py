#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H91 (D120, mejora 068) - disco de arranque con varias particiones de datos.

Genera con tools/mkimagem.py una imagen con SaguiOS + /home (SAGUI-DATOS) + /tmp (SAGUI-TMP) +
/logs (SAGUI-LOGS), una vez en FAT32 y otra en ext2, y la arranca en QEMU como pendrive USB:
  - el fstab monta las tres donde toca (LABEL= solo en el disco que tiene SaguiOS);
  - se escribe en /home, /tmp y /logs; en /logs ya hay un kernel-AAAAMMDD-HHMM.log del montaje;
  - apagado ordenado (apagar-pc): QEMU termina solo y el registro final queda en /logs;
  - se arranca otra vez: lo escrito persiste y hay dos registros (uno por arranque);
  - el anfitrión comprueba el contenido y fsck.vfat / e2fsck limpios.
Y sin las particiones nuevas (imagen sin /tmp ni /logs): /tmp sigue en la RAM y no existe /logs.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, extrair_particao, e2fsck, debugfs, particoes_img, RAIZ, SAIDA  # noqa: E402

B = os.path.join(RAIZ, "build")
ENV_FAT = dict(os.environ, MTOOLS_SKIP_CHECK="1")
TAM = {"fat32": (40, 34, 34), "ext2": (8, 8, 8)}


def criar_imagem(nome, fs, partes=(True, True, True)):
    os.makedirs(SAIDA, exist_ok=True)
    img = os.path.join(SAIDA, nome + ".img")
    h, t, l = TAM[fs]
    cmd = [sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
           "--mbr", f"{B}/boot/mbr.bin", "--estagio2", f"{B}/boot/estagio2.bin", "--kernel", f"{B}/kernel.bin",
           "--kernel-elf", f"{B}/kernel.elf", "--ramdisk", f"{B}/ramdisk.sgfs", "--saida", img,
           "--vesa", "--console", "--part-fs", fs, "--home-mb", str(h),
           "--sin-conf"]      # D256: SAGUI-CONF la prueba h255; aquí solo /home, /tmp y /logs
    if partes[1]:
        cmd += ["--tmp-mb", str(t)]
    if partes[2]:
        cmd += ["--logs-mb", str(l)]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    return img


# D388: orden de la tabla sin ESP: SaguiOS, SAGUI-TMP, SAGUI-LOGS y SAGUI-DATOS (/home) la última.
K_TMP, K_LOGS, K_HOME = 1, 2, 3


def tabela(img):
    """[(inicio, sectores)]: la de SaguiOS y luego las de datos (D213: lógicas de la extendida)."""
    return [(ini, qtd) for _n, _t, ini, qtd in particoes_img(img)]


def ler_host(img, fs, k, ruta):
    """Contenido de un archivo de la partición k (índice en la tabla) leído desde el anfitrión."""
    ini, qtd = tabela(img)[k]
    p = extrair_particao(img, ini, qtd // 2048)
    try:
        if fs == "ext2":
            return debugfs(p, f"cat /{ruta}")
        return subprocess.run(["mtype", "-i", p, f"::/{ruta}"], capture_output=True, env=ENV_FAT).stdout
    finally:
        os.remove(p)


def listar_host(img, fs, k):
    ini, qtd = tabela(img)[k]
    p = extrair_particao(img, ini, qtd // 2048)
    try:
        if fs == "ext2":
            return debugfs(p, "ls -p /").decode()
        return subprocess.run(["mdir", "-b", "-i", p, "::/"], capture_output=True, env=ENV_FAT).stdout.decode()
    finally:
        os.remove(p)


def apagar(m):
    marca = len(m.serial())
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 30
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.2)
    verificar(m.proc.poll() is not None, "apagar-pc: QEMU termina solo (apagado ordenado)")
    verificar("[LOGS] apagado o reinicio: kernel-" in m.serial()[marca:], "el kernel guarda su registro en /logs al apagar")


def probar(fs):
    print(f"--- {fs}")
    img = criar_imagem(f"h91-{fs}", fs)
    verificar(len(tabela(img)) == 4, f"[{fs}] imagen con 4 particiones (arranque, /home, /tmp, /logs)")

    with Maquina("h91a", "usb", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        for disp, ponto in (("arranque", "/home"), ("LABEL=SAGUI-TMP", "/tmp"), ("LABEL=SAGUI-LOGS", "/logs")):
            verificar(re.search(rf"\[FSTAB\] {re.escape(disp)} -> {ponto} \(auto, rw\): 0", s) is not None,
                      f"[{fs}] el fstab monta {disp} en {ponto}")
        out = m.comando("mount")
        for ponto in ("/home", "/tmp", "/logs"):
            verificar(ponto in out, f"[{fs}] mount lista {ponto}")
        out = m.comando("discos")
        verificar("usb0p7" in out and "usb0p2" in out, f"[{fs}] discos lista la extendida y sus tres lógicas (D213)")
        verificar("Pendrive conectado" not in s, f"[{fs}] ni avisos de pendrive ni /usb: es el disco de arranque")
        out = m.comando("ls /logs")
        verificar("kernel-" in out and ".log" in out, f"[{fs}] ya hay un kernel-*.log tras montar /logs")
        out = m.comando("cat /home/leeme.txt")
        verificar("/home" in out, f"[{fs}] /home trae su leeme.txt")
        m.comando("echo en-home > /home/h91.txt")
        m.comando("echo en-tmp > /tmp/h91.txt")
        m.comando("echo en-logs > /logs/h91.txt")
        out = m.comando("cat /home/h91.txt /tmp/h91.txt /logs/h91.txt")
        verificar(out.count("en-home") and "en-tmp" in out and "en-logs" in out, f"[{fs}] se escribe y se lee en las tres")
        out = m.comando("infopc --informe", tempo=120)
        verificar("Copia también en /logs/informe-" in out, f"[{fs}] infopc --informe deja también una copia en /logs")
        apagar(m)

    with Maquina("h91b", "usb", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("cat /home/h91.txt /tmp/h91.txt /logs/h91.txt")
        verificar("en-home" in out and "en-tmp" in out and "en-logs" in out, f"[{fs}] tras reiniciar todo persiste (/tmp también)")
        out = m.comando("ls /logs")
        verificar(len(re.findall(r"kernel-\d{8}-\d{4}[-0-9]*\.log", out)) >= 2, f"[{fs}] dos registros: uno por arranque")
        apagar(m)

    # El anfitrión comprueba
    nombres = re.findall(r"kernel-\d{8}-\d{4}[-0-9]*\.log", listar_host(img, fs, K_LOGS))
    verificar(len(set(nombres)) >= 2, f"[{fs}] el anfitrión ve los registros del kernel en la partición")
    txt = ler_host(img, fs, K_LOGS, sorted(set(nombres))[0]).decode("latin-1")
    verificar("[FSTAB]" in txt and "[SAGUI] apagando" in txt, f"[{fs}] el registro guardado contiene el arranque y el apagado")
    verificar(ler_host(img, fs, K_HOME, "h91.txt").strip() == b"en-home", f"[{fs}] /home/h91.txt en la partición")
    verificar(ler_host(img, fs, K_TMP, "h91.txt").strip() == b"en-tmp", f"[{fs}] /tmp/h91.txt en la partición")
    for k in range(1, 4):
        ini, qtd = tabela(img)[k]
        p = extrair_particao(img, ini, qtd // 2048)
        if fs == "ext2":
            rc, out = e2fsck(p)
        else:
            r = subprocess.run(["fsck.vfat", "-n", p], capture_output=True, text=True)
            rc, out = r.returncode, r.stdout + r.stderr
        os.remove(p)
        verificar(rc == 0, f"[{fs}] fsck limpio en la partición {k + 1}" + ("" if rc == 0 else "\n" + out))


def sin_particiones():
    print("--- sin /tmp ni /logs")
    img = criar_imagem("h91-solo-home", "fat32", (True, False, False))
    with Maquina("h91c", "usb", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("mount")
        verificar("/home" in out and "/logs" not in out, "sin SAGUI-LOGS no se monta /logs")
        out = m.comando("echo ram > /tmp/x.txt; cat /tmp/x.txt")
        verificar("ram" in out, "/tmp sigue funcionando en la RAM")
        out = m.comando("ls /logs")
        verificar("kernel-" not in out, "sin partición de logs no se escribe ningún registro")
        apagar_sin_log(m)


def apagar_sin_log(m):
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 30
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.2)
    verificar(m.proc.poll() is not None and "[LOGS]" not in m.serial(), "apagado sin escribir registros")


def main():
    probar("fat32")
    probar("ext2")
    sin_particiones()
    print("H91 OK")


if __name__ == "__main__":
    main()
