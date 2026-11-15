#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H92 (D121) - utilidad gráfica «Discos»: cuánto ocupa cada disco, a la vista.

Un pendrive USB con dos particiones (FAT32 con 20 MB de datos y ext2 con 8 MB) y un disco
interno IDE con un ext2 de 4 MB. Parte 1 (terminal): `discos` muestra usado/total y el
porcentaje junto a cada partición montada, y `discos -u` la barra de texto. Parte 2
(escritorio): el icono «Discos» abre la ventana; la prueba lee del log serie las filas que
dibuja (porcentaje y KB) y las compara con lo que dicen mkfs/fsck/dumpe2fs del anfitrión
(±1 %); captura de la ventana; desmontar y montar desde los botones; Actualizar con F5;
refresco automático al conectar otro pendrive; el menú contextual; Abrir en Archivos.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, pendrive_qemu, RAIZ,  # noqa: E402
                  cor_do_pixel, SAIDA, BUILD, imagem_de_teste, opcoes_apps)


def pct_ext2(part):
    r = subprocess.run(["/usr/sbin/dumpe2fs", "-h", part], capture_output=True, text=True).stdout
    bc = int(re.search(r"Block count:\s+(\d+)", r).group(1))
    fb = int(re.search(r"Free blocks:\s+(\d+)", r).group(1))
    return (bc - fb) * 100.0 / bc


def pct_fat(part):
    r = subprocess.run(["fsck.fat", "-n", "-v", part], capture_output=True, text=True).stdout
    u, t = re.search(r"(\d+)/(\d+) clusters", r).groups()
    return int(u) * 100.0 / int(t)


def linhas_janela(serial):
    """Última foto de las filas que registró la ventana: {nombre: (estado, pct, total_kb, ponto)}."""
    res = {}
    for m in re.finditer(r"discos: vol (\S+) (montado|no_montado) uso=(\d+)% total_kb=(\d+) livre_kb=(\d+) ponto=(\S*) y=\d+", serial):
        res[m.group(1)] = (m.group(2), int(m.group(3)), int(m.group(4)), m.group(6))
    return res


def main():
    pen, ini = criar_disco("h92-pen", [
        dict(tipo="fat32", mb=40, rotulo="FATPEN", arquivos={"grande.bin": os.urandom(20_000_000), "oi.txt": "hola\n"}),
        dict(tipo="ext2", mb=24, rotulo="EXTPEN", arquivos={"datos.bin": os.urandom(8 * 1024 * 1024)}),
    ])
    hd, ini_hd = criar_disco("h92-hd", [
        dict(tipo="ext2", mb=16, rotulo="INTERNO", arquivos={"doc.bin": os.urandom(4 * 1024 * 1024)}),
    ])
    fat = extrair_particao(pen, ini[0], 40)
    ext = extrair_particao(pen, ini[1], 24)
    interno = extrair_particao(hd, ini_hd[0], 16)
    esp = {"usb0p1": pct_fat(fat), "usb0p2": pct_ext2(ext), "hdb1": pct_ext2(interno)}
    print("esperado:", {k: round(v, 1) for k, v in esp.items()})

    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen) + ["-drive", f"file={hd},if=ide,index=1,format=raw"]

    # ---------------- parte 1: terminal ----------------
    with Maquina("h92-texto", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("discos")
        verificar(re.search(r"usb0p1\s+40 MB\s+FAT32\s+\"FATPEN\"\s+\(no montado\)\n", out), "discos sin montar: salida de siempre, sin uso")
        m.comando("mount usb0p1 /datos")
        m.comando("mount usb0p2 /extra")
        m.comando("mount hdb1 /interno")
        out = m.comando("discos")
        for nome, pct in esp.items():
            r = re.search(rf"{nome}\s+\d+ MB\s+\S+\s+\"\w+\"\s+\(montado en [^)]*\)\s+([\d,]+ \w+)/([\d,]+ \w+) (\d+)%", out)
            verificar(r is not None, f"discos: {nome} muestra usado/total y porcentaje")
            if r:
                verificar(abs(int(r.group(3)) - pct) <= 1.0, f"discos: {nome} {r.group(3)}% (esperado {pct:.1f}% +-1)")
        out = m.comando("discos -u")
        # 3 montadas a mano + /apps (hda5) y SAGUI-CONF (hda6), que ahora van montadas de serie (D324, D255)
        verificar(len(re.findall(r"\[#*\.*\] \d+ %", out)) == 5, "discos -u: una barra de texto por partición montada")
        r = re.search(r"usb0p1.*\n?.*\[(#+)(\.*)\] (\d+) %", out)
        verificar(r and len(r.group(1)) + len(r.group(2)) == 10 and abs(len(r.group(1)) - round(int(r.group(3)) / 10)) <= 1,
                  "discos -u: la barra tiene 10 casillas y concuerda con el porcentaje")
        out = m.comando("discos -t")
        verificar("[#" not in out and "usb0p1" in out, "discos -t: texto sin barras")

    # ---------------- parte 2: escritorio ----------------
    # D257: con SAGUI-CONF hay más filas de las que caben: la ventana se desplaza y la prueba
    # comprueba que todas (también SAGUI-CONF y hdb1) son alcanzables con las teclas.
    jogos = os.path.join(BUILD, "jogos", "raiz")
    img_gui = imagem_de_teste("", ["--vesa"] + (["--jogos", jogos] if os.path.isdir(jogos) else []) + opcoes_apps())
    with Maquina("h92-gui", "ide", imagem=img_gui, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        # el icono «Discos» es el 14.º (índice 13): segunda columna, fila 6
        m.clique(20 + 104 + 48, 40 + 5 * 84 + 32, duplo=True)
        m.esperar_serial("discos: janela aberta", 20)
        verificar("lanzado por el escritorio" in m.serial(), "el icono lanza discos sin argumentos y lo detecta (ventana)")
        time.sleep(1)
        s = m.serial()
        f = linhas_janela(s)
        verificar(all(k in f for k in ("ramfs", "usb0p1", "usb0p2", "hdb1")), f"la ventana muestra RAM, las dos particiones del pendrive y el disco interno ({sorted(f)})")
        verificar(f["usb0p1"][0] == "no_montado" and f["usb0p2"][0] == "no_montado", "las particiones del pendrive empiezan sin montar (barra rayada)")
        m.captura("H92-1-ventana-sin-montar")
        jan = re.findall(r'criada por \d+: "Discos" (\d+)x(\d+) em \((\d+),(\d+)\)', s)[-1]
        jl, ja, jx, jy = (int(v) for v in jan)
        ox, oy = jx + 2, jy + 24   # origen del contenido

        ys = {m_.group(1): int(m_.group(2)) for m_ in re.finditer(r"discos: vol (\S+) .* y=(\d+)", s[s.rindex("lanzado por el escritorio"):])}

        def rolagem_actual():
            r = re.findall(r"discos: rolagem (\d+)", m.serial())
            return int(r[-1]) if r else 0

        def y_boton(nome):
            # D257: si la fila no se ve entera, se desplaza (AvPag) hasta que se vea
            for _ in range(6):
                y = ys[nome] - rolagem_actual()
                if 0 <= y and y + 72 <= ja - 24 - 38:
                    break
                m.tecla("pgdn" if y > 0 else "pgup")
                time.sleep(0.6)
            return ys[nome] - rolagem_actual() + 2

        # x de los botones: ancho - 12 - 14 - 2*92 - 8
        bx = jl - 12 - 14 - 2 * 92 - 8
        # Montar usb0p1 con el botón
        yb = y_boton("usb0p1")
        m.clique(ox + bx + 92 + 8 + 40, oy + yb + 13)
        m.esperar_serial("discos: montar usb0p1 -> 0", 20)
        time.sleep(1.5)
        f = linhas_janela(m.serial())
        verificar(f["usb0p1"][0] == "montado" and abs(f["usb0p1"][1] - esp["usb0p1"]) <= 1,
                  f"botón Montar: usb0p1 montado, {f['usb0p1'][1]}% (esperado {esp['usb0p1']:.1f}% +-1)")
        m.captura("H92-2-fat-montada")
        # Montar usb0p2 con F5 despues de montarla a mano por la consola de la serie? usa el menu contextual
        yb2 = y_boton("usb0p2")
        m.clique(ox + 100, oy + yb2 + 20, botao=2)
        time.sleep(0.8)
        m.captura("H92-3-menu-contextual")
        m.tecla("esc")
        time.sleep(0.5)
        m.clique(ox + bx + 92 + 8 + 40, oy + yb2 + 13)
        m.esperar_serial("discos: montar usb0p2 -> 0", 20)
        time.sleep(1.5)
        f = linhas_janela(m.serial())
        verificar(f["usb0p2"][0] == "montado" and abs(f["usb0p2"][1] - esp["usb0p2"]) <= 1,
                  f"usb0p2 (ext2) montado, {f['usb0p2'][1]}% (esperado {esp['usb0p2']:.1f}% +-1)")
        # disco interno: montar solo lectura por defecto
        yb3 = y_boton("hdb1")
        m.clique(ox + bx + 92 + 8 + 40, oy + yb3 + 13)
        m.esperar_serial("discos: montar hdb1 -> 0", 20)
        time.sleep(1.5)
        verificar("hdb1 montado (solo lectura)" in m.serial() or re.search(r"vol hdb1 montado .* ponto=/mnt/hdb1", m.serial()),
                  "disco interno: Montar lo deja en /mnt/hdb1")
        f = linhas_janela(m.serial())
        verificar(abs(f["hdb1"][1] - esp["hdb1"]) <= 1, f"hdb1 {f['hdb1'][1]}% (esperado {esp['hdb1']:.1f}% +-1)")
        m.captura("H92-4-todo-montado")
        # D257: la última fila (SAGUI-CONF, hda7) es alcanzable desplazando
        verificar("hda7" in ys, "la fila de SAGUI-CONF (hda7) existe en la ventana")
        y_boton("hda7")
        verificar(0 <= ys["hda7"] - rolagem_actual() and ys["hda7"] - rolagem_actual() + 72 <= ja - 24 - 38,
                  f"SAGUI-CONF visible tras desplazar (rolagem {rolagem_actual()})")
        m.captura("H92-4b-desplazada-sagui-conf")
        # Desmontar usb0p1
        m.clique(ox + bx + 92 + 8 + 40, oy + y_boton("usb0p1") + 13)
        m.esperar_serial("discos: desmontar usb0p1 -> 0", 20)
        time.sleep(1.5)
        f = linhas_janela(m.serial())
        verificar(f["usb0p1"][0] == "no_montado", "botón Desmontar: usb0p1 vuelve a «no montada»")
        # Abrir en Archivos
        m.clique(ox + bx + 40, oy + y_boton("usb0p2") + 13)
        time.sleep(2)
        verificar("arquivos: pasta " in m.serial()[m.serial().rindex("discos: desmontar"):], "botón Abrir: se abre Archivos en la carpeta del montaje")
        m.captura("H92-5-abrir-archivos")
        # F5
        m.clique(ox + 12, oy + 20)   # el botón Actualizar (trae la ventana al frente)
        verificar("discos: actualizado (boton)" in m.serial(), "botón Actualizar")
        antes = m.serial().count("discos: actualizado (F5)")
        m.tecla("f5")
        time.sleep(1)
        verificar(m.serial().count("discos: actualizado (F5)") == antes + 1, "F5 actualiza")
        # refresco automático: conectar otro pendrive
        otro = os.path.join(SAIDA, "h92-otro.img")
        with open(otro, "wb") as fh:
            fh.truncate(16 * 1024 * 1024)
        subprocess.run(["mkfs.vfat", "-F", "16", "-n", "OTRO", otro], check=True, stdout=subprocess.DEVNULL)
        m.monitor(f"drive_add 0 if=none,id=p9,format=raw,file={otro}")
        m.monitor("device_add usb-storage,bus=ehci.0,drive=p9,id=dev-p9,removable=on")
        t0 = time.time()
        while time.time() - t0 < 12 and "usb1" not in linhas_janela(m.serial()):
            time.sleep(0.5)
        verificar("usb1" in linhas_janela(m.serial()), "refresco automático: el pendrive nuevo aparece sin pulsar nada")
        m.captura("H92-6-pendrive-nuevo")
    return 0


if __name__ == "__main__":
    sys.exit(main())
