#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H54 (D040) - /etc/fstab também vale para pendrives conectados a quente.

Acrescenta uma linha ao /etc/fstab (na camada em RAM), conecta com o
sistema ligado um pendrive cuja partição ext2 tem o rótulo da linha, e o
SaguiOS monta sozinho na pasta pedida. Desconectar sem desmontar avisa.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco  # noqa: E402


def main():
    img, _ = criar_disco("h54-pen", [dict(tipo="ext2", mb=16, rotulo="QUENTE", arquivos={"hola.txt": "montado por el fstab\n"})])
    with Maquina("h54", "ide", imagem=imagem_console(), extras=["-device", "usb-ehci,id=ehci"]) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("cat /etc/fstab")
        verificar("arranque" in out and "/home" in out, "/etc/fstab padrão tem a linha da partição de dados")
        m.comando("echo LABEL=QUENTE /mnt/quente ext2 rw >> /etc/fstab")
        m.monitor(f"drive_add 0 if=none,id=q,format=raw,file={img}")
        m.monitor("device_add usb-storage,bus=ehci.0,drive=q,id=dev-q,removable=on")
        m.esperar_serial("[FSTAB] LABEL=QUENTE -> /mnt/quente", 30)
        verificar("montado em /mnt/quente, leitura e escrita" in m.serial(), "conectado a quente: montado sozinho pelo fstab")
        verificar("Pendrive montado en /mnt/quente" in m.serial(), "aviso de montagem para a área de trabalho")
        out = m.comando("cat /mnt/quente/hola.txt")
        verificar("montado por el fstab" in out, "o conteúdo está lá")
        out = m.comando("discos")
        verificar("montado en /mnt/quente" in out, "discos mostra onde está montado")
        m.monitor("device_del dev-q")
        m.esperar_serial("sin desmontarlo: pueden haberse perdido datos", 20)
        verificar(True, "retirado sem desmontar: aviso")
        out = m.comando("ls /mnt/quente")
        verificar("no existe" in out, "a montagem some com o pendrive")
    return 0


if __name__ == "__main__":
    sys.exit(main())
