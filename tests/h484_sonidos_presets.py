#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H484 (D484) - un sonido por acción y presets de sonidos.

Partes (por defecto todas): cli reinicio gui
  cli       HDA con el audio de QEMU a un .wav y /home persistente: migración de un sonidos.conf antiguo (queda como
            preset «anterior»), asignar un archivo propio (tono sintético de 1000 Hz) a «aviso», el de un esquema a
            «error» y «ninguno» a «pregunta»; guardar preset, cambiar, cargar (suena lo esperado en el .wav), volumen
            por acción, renombrar, borrar (se van sus audios), exportar (rutas relativas y audio al lado) e importar.
  reinicio  se reinicia la VM con la misma imagen: configuración y presets siguen y suena el tono propio.
  gui       escritorio: el vigilante toca al abrir y cerrar una ventana (tonos propios distintos), la página
            Configuración > Sonidos del sistema se ve (capturas H484-*).
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu  # noqa: E402
from h70_sonido import gerar_wav, ler_gravacao, trechos, Trecho  # noqa: E402
import h83_configuracion as h83  # noqa: E402


def segmentos(wav):
    esq, dir_ = ler_gravacao(wav)
    return [Trecho(esq, dir_, a, b) for a, b in trechos(esq, dir_)]


def pen(nome, tonos):
    arqs = {n: gerar_wav(48000, 16, 1, 0.5, [f], 0.5) for n, f in tonos.items()}
    return criar_pendrive(nome, rotulo="TONOS", arquivos=arqs)


def parte_cli():
    p = pen("h484-pen", {"propio1.wav": 1000, "propio2.wav": 600})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(p)
    img = h83.imagem_persistente("h484-persistente", False)
    with Maquina("h484-cli", "ide", imagem=img, snapshot=False, extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("montar usb0p1")
        verificar("propio1.wav" in m.comando("ls /usb"), "el pendrive con los tonos sintéticos está montado")
        m.comando("mkdir /home/mis")
        m.comando("cp /usb/propio1.wav /home/mis/propio1.wav", 60)
        m.comando("cp /usb/propio2.wav /home/mis/propio2.wav", 60)
        # 1. migración de un sonidos.conf antiguo (v8.5.5.x: sin «version», sin asignaciones)
        m.comando("mkdir /home/.config")
        for linea in ("activo=1", "volumen=100", "esquema=saguios", "ev_aviso=1", "ev_error=1", "ev_pregunta=1"):
            m.comando(f"echo {linea} >> /home/.config/sonidos.conf")
        out = m.comando("sonidos")
        verificar("Preset: anterior" in out, "la configuración antigua se migra y queda como preset «anterior»")
        out = m.comando("sonidos --presets")
        verificar("anterior" in out and "saguios" in out and "kenney" in out and "(esquema)" in out,
                  "presets: los esquemas de partida (solo lectura) y «anterior»")
        out = m.comando("cat /home/.config/sonidos.conf")
        verificar("version=2" in out and "esquema=saguios" in out, "sonidos.conf pasa al formato nuevo conservando el esquema")
        out = m.comando("sonidos --preset-borrar saguios")
        verificar("no se pudo" in out, "un esquema (preset de partida) no se puede borrar")
        m.comando("sonidos --tocar aviso", 30)                                   # 1: 880 (esquema de la migración)
        time.sleep(0.6)
        # 2. un sonido por acción
        m.comando("sonidos --asignar aviso /home/mis/propio1.wav")
        m.comando("sonidos --asignar error /sistema/sonidos/kenney/error.wav")
        m.comando("sonidos --asignar pregunta -")
        m.comando("sonidos --tocar aviso", 30)                                   # 2: 1000 propio
        time.sleep(0.6)
        m.comando("sonidos --tocar error", 30)                                   # 3: Kenney
        time.sleep(0.6)
        m.comando("sonidos --tocar pregunta", 30)                                # no suena (ninguno)
        time.sleep(0.6)
        out = m.comando("sonidos")
        verificar("/home/mis/propio1.wav" in out and "(ninguno)" in out and "kenney/error.wav" in out, "el estado muestra cada asignación")
        # 3. guardar preset, cambiar, cargar
        out = m.comando("sonidos --preset-guardar mio1")
        verificar("no se pudo" not in out, "guardar preset «mio1»")
        out = m.comando("ls /home/.config/sonidos/audio")
        verificar("mio1.aviso.wav" in out, "el audio propio se copia junto al preset")
        m.comando("sonidos --asignar aviso /home/mis/propio2.wav")
        m.comando("sonidos --tocar aviso", 30)                                   # 4: 600
        time.sleep(0.6)
        m.comando("sonidos --preset-guardar mio2")
        m.comando("sonidos --preset-cargar mio1")
        m.comando("sonidos --tocar aviso", 30)                                   # 5: 1000 otra vez
        time.sleep(0.6)
        m.comando("sonidos --preset-cargar saguios")
        m.comando("sonidos --tocar aviso", 30)                                   # 6: 880 (esquema)
        time.sleep(0.6)
        m.comando("sonidos --vol-evento aviso 50")
        m.comando("sonidos --tocar aviso", 30)                                   # 7: 880 más bajo
        time.sleep(0.6)
        # 4. renombrar y borrar
        out = m.comando("sonidos --preset-renombrar mio2 mio3")
        verificar("no se pudo" not in out, "renombrar mio2 -> mio3")
        out = m.comando("sonidos --presets")
        verificar("mio3" in out and "mio2" not in out and "mio1" in out, "tras renombrar: mio3 y mio1")
        out = m.comando("ls /home/.config/sonidos/audio")
        verificar("mio3.aviso.wav" in out and "mio2.aviso.wav" not in out, "los audios cambian de nombre con el preset")
        m.comando("sonidos --preset-borrar mio3")
        out = m.comando("ls /home/.config/sonidos/audio")
        verificar("mio3" not in out and "mio1.aviso.wav" in out, "borrar quita el preset y sus audios, no los de otro")
        out = m.comando("sonidos --preset-renombrar mio1 kenney")
        verificar("no se pudo" in out, "no se puede renombrar a un esquema")
        # 5. exportar e importar
        m.comando("mkdir /home/exp")
        out = m.comando("sonidos --preset-exportar mio1 /home/exp/mio1.sonidos")
        verificar("no se pudo" not in out, "exportar mio1")
        out = m.comando("ls /home/exp")
        verificar("mio1.sonidos" in out and "mio1.aviso.wav" in out, "exportar deja el .sonidos y el audio propio al lado")
        out = m.comando("cat /home/exp/mio1.sonidos")
        verificar("asig_aviso=mio1.aviso.wav" in out and "asig_error=/sistema/sonidos/kenney/error.wav" in out,
                  "el .sonidos usa rutas relativas para lo propio y absolutas para lo del sistema")
        m.comando("sonidos --preset-borrar mio1")
        out = m.comando("sonidos --preset-importar /home/exp/mio1.sonidos")
        verificar("mio1" in out, "importar devuelve el nombre del preset")
        out = m.comando("sonidos --preset-importar /home/exp/mio1.sonidos")
        verificar("mio1_2" in out, "importar otra vez no pisa: mio1_2")
        m.comando("sonidos --preset-cargar mio1")
        m.comando("sonidos --tocar aviso", 30)                                   # 8: 1000 desde el preset importado
        time.sleep(0.6)
        m.comando("sonidos --tocar pregunta", 30)                                # no suena
        m.comando("sync")
        time.sleep(1)
    seg = segmentos(m.wav)
    print("  trechos:", [(round(s.segundos, 2), s.f_esq, round(s.rms_esq)) for s in seg])
    verificar(len(seg) == 8, f"el .wav tiene los 8 sonidos esperados (ninguno suena de más): hay {len(seg)}")
    f = [s.f_esq for s in seg]
    verificar(abs(f[0] - 880) <= 12, "1: la migración conserva el esquema (880 Hz)")
    verificar(abs(f[1] - 1000) <= 15, f"2: el archivo propio suena en «aviso» ({f[1]} Hz)")
    verificar(abs(f[2] - 1000) > 30 and seg[2].segundos > 0.05, "3: «error» suena distinto, con el archivo de otro esquema")
    verificar(abs(f[3] - 600) <= 12, f"4: otro archivo propio ({f[3]} Hz)")
    verificar(abs(f[4] - 1000) <= 15, f"5: cargar el preset devuelve el sonido guardado ({f[4]} Hz)")
    verificar(abs(f[5] - 880) <= 12, "6: cargar el esquema «saguios» devuelve el aviso de serie")
    verificar(seg[6].rms_esq < seg[5].rms_esq * 0.8, f"7: el volumen por acción baja el nivel ({seg[6].rms_esq:.0f} < {seg[5].rms_esq:.0f})")
    verificar(abs(f[7] - 1000) <= 15, "8: el preset importado suena igual que el exportado")
    return img


def parte_reinicio(img):
    with Maquina("h484-reinicio", "ide", imagem=img, snapshot=False, som="hda") as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("sonidos")
        verificar("activados" in out and "Preset: mio1" in out, "tras reiniciar sigue la configuración y el preset en uso")
        out = m.comando("sonidos --presets")
        verificar("mio1" in out and "mio1_2" in out, "los presets siguen tras reiniciar")
        m.comando("sonidos --preset-cargar mio1_2")
        m.comando("sonidos --tocar aviso", 30)
        time.sleep(0.6)
    seg = segmentos(m.wav)
    verificar(len(seg) == 1 and abs(seg[0].f_esq - 1000) <= 15, f"el audio propio sigue sonando tras reiniciar ({[s.f_esq for s in seg]})")


def parte_gui():
    p = pen("h484-pen-gui", {"abrir.wav": 1000, "cerrar.wav": 600})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(p)
    with Maquina("h484-gui", "ide", memoria="1G", extras=extras, som="hda") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(4)
        m.comando("montar usb0p1")
        m.comando("cp /usb/abrir.wav /tmp/abrir.wav", 60)
        m.comando("cp /usb/cerrar.wav /tmp/cerrar.wav", 60)
        m.comando("sonidos --volumen 100")
        m.comando("sonidos --activar")
        m.comando("sonidos --evento ventana_abrir si")
        m.comando("sonidos --evento ventana_cerrar si")
        m.comando("sonidos --asignar ventana_abrir /tmp/abrir.wav")
        m.comando("sonidos --asignar ventana_cerrar /tmp/cerrar.wav")
        m.comando("sonidos --vigilar &", 10)
        time.sleep(3)
        m.comando("configuracion --ventana sonidos &", 20)
        for _ in range(80):
            if "escritorio vista sonidos" in m.serial():
                break
            time.sleep(0.5)
        verificar("escritorio vista sonidos esquemas=" in m.serial(), "Configuración abre la página «Sonidos del sistema»")
        for _ in range(40):
            if "ventanas abrir=1" in m.serial():
                break
            time.sleep(0.5)
        verificar("ventanas abrir=1" in m.serial(), "el vigilante detecta la ventana nueva")
        time.sleep(3)
        m.captura("H484-1-sonidos-acciones")
        time.sleep(1)
        m.tecla("alt-f4")   # Configuración tiene el foco: se cierra ella
        time.sleep(4)
        verificar("ventanas abrir=0 cerrar=1" in m.serial(), "el vigilante detecta que se cierra")
    seg = segmentos(m.wav)
    print("  trechos:", [(round(s.segundos, 2), s.f_esq) for s in seg])
    verificar(len(seg) >= 2, f"el vigilante toca al abrir y cerrar una ventana: {len(seg)} sonidos")
    verificar(abs(seg[0].f_esq - 1000) <= 15, "abrir una ventana: el tono propio de «abrir» (1000 Hz)")
    verificar(abs(seg[-1].f_esq - 600) <= 12, "cerrar una ventana: el tono propio de «cerrar» (600 Hz)")


PARTES = ["cli", "reinicio", "gui"]


def main():
    pedidas = sys.argv[1:] or PARTES
    img = None
    for p in pedidas:
        print(f"-- {p}")
        if p == "cli":
            img = parte_cli()
        elif p == "reinicio":
            if img is None:
                img = os.path.join(h83.SAIDA, "h484-persistente.img")
            parte_reinicio(img)
        else:
            parte_gui()
    print("H484 ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
