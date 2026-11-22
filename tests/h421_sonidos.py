#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H421 (D421) - sonidos del sistema: esquemas «SaguiOS» y «Kenney», eventos y Configuración > Sonidos del sistema.

Partes (por defecto todas): audio apagado sinsonido gui
  audio    tarjeta HDA con el audio de QEMU a un .wav: de serie no suena nada; con los sonidos activados suena el
           aviso (A5 = 880 Hz) y, pedido, el de arranque; «ninguno» calla; el esquema Kenney suena; --max corta un
           sonido largo; el vigilante toca conectar y desconectar USB (hot-plug por el monitor de QEMU).
  apagado  «apagar-pc» con el sonido de apagado activado: suena antes de apagar (se oye en el .wav) y QEMU termina.
  sinsonido  sin tarjeta de sonido y con todo activado: «apagar-pc» no se cuelga y los sonidos terminan en silencio.
  gui      Configuración > Sonidos del sistema se ve (captura sintética H421-*).
Los esquemas PRIVADOS (Windows, Pixabay) no entran nunca en la batería: aquí se comprueba que NO están en la imagen.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste  # noqa: E402
from h70_sonido import ler_gravacao, trechos, Trecho  # noqa: E402


def segmentos(wav):
    esq, dir_ = ler_gravacao(wav)
    return [Trecho(esq, dir_, a, b) for a, b in trechos(esq, dir_)]


def esperar_fin(m, tempo):
    try:
        m.proc.wait(timeout=tempo)
        return True
    except Exception:
        return False


def parte_audio():
    extras = ["-device", "usb-ehci,id=ehci"]
    with Maquina("h421-audio", "ide", imagem=imagem_console(apps=True), extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sonidos")
        verificar("desactivados" in out and "(de serie)" in out, "de serie los sonidos están desactivados")
        out = m.comando("sonidos --listar")
        verificar("saguios" in out and "kenney" in out, "esquemas instalados: saguios y kenney")
        verificar("(privado)" not in out and "windows" not in out.lower() and "pixabay" not in out.lower(),
                  "ningún esquema privado en la imagen de la batería (D421)")
        out = m.comando("ls /apps/sonidos")
        verificar("windows" not in out.lower() and "pixabay" not in out.lower(), "no hay /apps/sonidos privados")
        out = m.comando("cat /sistema/sonidos/kenney/CREDITOS.txt")
        verificar("kenney.nl" in out and "CC0" in out, "los créditos de Kenney (CC0) van junto a los sonidos")
        out = m.comando("cat /sistema/sonidos/saguios/CREDITOS.txt")
        verificar("GPL v3" in out, "créditos del esquema propio: generado por SaguiOS-Extended, GPL v3")
        out = m.comando("cat /etc/servicios/sonidos.conf")
        verificar("--vigilar" in out, "servicio «sonidos» instalado")

        m.comando("sonidos --tocar aviso", 30)        # 1: desactivado -> no suena
        m.comando("sonidos --esquema saguios")
        m.comando("sonidos --volumen 100")
        m.comando("sonidos --activar")
        m.comando("sonidos --tocar aviso", 30)        # 2: suena (A5)
        time.sleep(0.6)
        m.comando("sonidos --tocar arranque", 30)     # no: el arranque está apagado de serie
        time.sleep(0.6)
        m.comando("sonidos --evento arranque si")
        m.comando("sonidos --tocar arranque", 30)     # 3: suena largo
        time.sleep(0.6)
        m.comando("sonidos --esquema ninguno")
        m.comando("sonidos --tocar aviso", 30)        # no
        time.sleep(0.6)
        m.comando("sonidos --esquema kenney")
        m.comando("sonidos --tocar error", 30)        # 4: suena (Kenney)
        time.sleep(0.6)
        m.comando("sonidos --esquema saguios")
        m.comando("sonidos --tocar arranque --max 500", 30)   # 5: cortado a ~0,5 s
        time.sleep(0.6)
        m.comando("sonidos --evento aviso no")
        m.comando("sonidos --tocar aviso", 30)        # no: ese evento está apagado
        time.sleep(0.6)
        m.comando("sonidos --tocar aviso --siempre", 30)   # 6: el botón ▶ ignora los interruptores
        time.sleep(0.6)
        # El vigilante: conectar y desconectar un USB (hot-plug por el monitor de QEMU).
        m.comando("sonidos --vigilar &", 10)
        time.sleep(3)
        m.monitor("device_add usb-mouse,bus=ehci.0,id=rat1")
        time.sleep(7)
        m.monitor("device_del rat1")
        time.sleep(7)
        m.comando("cat /home/.config/sonidos.conf")
    seg = segmentos(m.wav)
    print("  trechos:", seg)
    verificar(len(seg) == 7, f"el .wav tiene justo los 7 sonidos esperados (los apagados/«ninguno» no suenan): hay {len(seg)}")
    a = seg[0]
    verificar(abs(a.f_esq - 880) <= 12, f"el aviso del esquema SaguiOS es el La agudo (880 Hz): {a.f_esq} Hz")
    verificar(a.segundos > 0.4, f"el aviso dura {a.segundos:.2f} s")
    verificar(seg[1].segundos > 0.9, f"el arranque es largo: {seg[1].segundos:.2f} s")
    verificar(seg[2].segundos > 0.1, "el esquema Kenney suena (error)")
    verificar(seg[3].segundos < 1.0, f"--max 500 corta el arranque de 1,8 s: {seg[3].segundos:.2f} s")
    verificar(abs(seg[4].f_esq - 880) <= 12, "«--siempre» toca aunque el evento esté apagado")
    verificar(abs(seg[5].f_esq - 880) <= 12, "el vigilante toca al conectar el USB (E5 y A5, sube)")
    verificar(abs(seg[6].f_esq - 659) <= 12, "el vigilante toca al desconectar el USB (A5 y E5, baja)")


def parte_apagado():
    with Maquina("h421-apagado", "ide", imagem=imagem_console(apps=True), som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("sonidos --esquema saguios")
        m.comando("sonidos --volumen 100")
        m.comando("sonidos --activar")
        m.comando("sonidos --evento apagar si")
        m.digitar("apagar-pc\n", 0.05)
        verificar(esperar_fin(m, 40), "apagar-pc con el sonido activado: QEMU termina solo")
    seg = segmentos(m.wav)
    print("  trechos:", seg)
    verificar(len(seg) >= 1 and seg[0].segundos > 0.5, f"suena el sonido de apagado antes de apagar ({seg[0].segundos if seg else 0:.2f} s)")


def parte_sinsonido():
    with Maquina("h421-sinsonido", "ide", imagem=imagem_console(apps=True)) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("sonidos --esquema saguios")
        m.comando("sonidos --activar")
        m.comando("sonidos --evento apagar si")
        t0 = time.time()
        out = m.comando("sonidos --tocar aviso", 20)
        verificar(time.time() - t0 < 15, "sin tarjeta, un sonido termina enseguida y en silencio")
        verificar("error" not in out.lower(), "sin tarjeta no se queja")
        m.digitar("apagar-pc\n", 0.05)
        t0 = time.time()
        verificar(esperar_fin(m, 40), "sin tarjeta de sonido, apagar-pc no se cuelga")
        print(f"  apagado sin tarjeta: {time.time() - t0:.1f} s")


def parte_gui():
    with Maquina("h421-gui", "ide", memoria="1G") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(4)
        m.comando("configuracion --ventana sonidos &", 20)
        for _ in range(80):
            if "escritorio vista sonidos" in m.serial():
                break
            time.sleep(0.5)
        verificar("escritorio vista sonidos esquemas=" in m.serial(), "Configuración abre la sección «Sonidos del sistema»")
        verificar(re.search(r"esquemas=[2-9]", m.serial()), "la sección ve los esquemas instalados (saguios y kenney)")
        time.sleep(2)
        m.captura("H421-1-sonidos")


PARTES = {"audio": parte_audio, "apagado": parte_apagado, "sinsonido": parte_sinsonido, "gui": parte_gui}


def main():
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        print(f"-- {p}")
        PARTES[p]()
    print("H421 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
