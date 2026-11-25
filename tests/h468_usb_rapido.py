#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H468 - Pendrives más rápidos: órdenes grandes, fases encadenadas y lectura anticipada (D468).

Criterios, en QEMU, por el EHCI (alta velocidad) y por el qemu-xhci (USB 3.0):
  1. al conectar, el registro dice el trozo elegido: por el EHCI, 240 KB, o 1 MB si el disco
     del QEMU (que se declara SPC-3) da la página Block Limits (VPD B0); por el xHCI (puerto
     USB 3.0), 1 MB; y el buffer de datos del controlador es de 1 MB;
  2. lectura de un fichero de 24 MB con sha256sum (peticiones de 16 KB: pasan por la caché
     con lectura anticipada) y con cp (peticiones de 1 MB: directas): la suma es igual a la
     del PC, el tamaño medio de las órdenes de lectura pasa de 128 KB y la caché cuenta
     bloques leídos por adelantado («anticipados» crece);
  3. escritura del mismo fichero al pendrive con cp: tamaño medio de las órdenes de escritura
     de más de 128 KB y fases encadenadas («encadenadas» crece);
  4. la escritura sigue siendo segura: tras «sync» se desenchufa el pendrive en caliente
     (device_del, como en h227) y en el PC, con mtools, el fichero escrito es idéntico.
  5. plazo agotado a mitad de una escritura: con el pendrive frenado en el QEMU a 35 KB/s una
     orden de 240 KB o más no acaba a tiempo (la de 128 KB sí); el trozo baja, antes de repetir se hace el reset de
     recuperación, y el fichero queda idéntico en el PC.
Las cifras (órdenes, KB, media, segundos) quedan en build/testes/h468-resultados.txt; el QEMU
no da la velocidad de un pendrive de verdad, pero sí cuántas órdenes y de qué tamaño se mandan.
Uso: h468_usb_rapido.py [ehci] [xhci] [plazo-ehci] [plazo-xhci]   (sin argumentos, todo)
"""
import hashlib
import os
import random
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_console, criar_pendrive, mtools, SAIDA  # noqa: E402

TAM = 24 * 1024 * 1024 + 4321          # no múltiplo de sector ni de clúster
RE_USB = re.compile(r"\[USB\] usb0: lectura (\d+) ordenes, (\d+) KB, media (\d+) KB; escritura (\d+) ordenes, "
                    r"(\d+) KB, media (\d+) KB; mayor (\d+) KB, trozo (\d+) KB, reducciones (\d+), encadenadas (\d+)")
RE_CACHE = re.compile(r"\[CACHE\] bloques \d+/\d+, sucios \d+, aciertos (\d+), fallos (\d+), anticipados (\d+)")

resultados = []


def cronometrar(m, comando, tempo=900):
    """Escribe la orden y devuelve (segundos hasta que acaba, salida), con una marca de fin
    que solo aparece en la salida (el eco lleva comillas en medio)."""
    antes = m.serial()
    m.digitar(comando + " ; echo FI''N468", 0.02)
    m.tecla("ret", 0.02)
    ini = time.time()
    while "\nFIN468" not in m.serial()[len(antes):].replace("\r", ""):
        if time.time() - ini > tempo:
            raise FalhaTeste(f"la orden {comando!r} no ha acabado en {tempo} s")
        if m.proc.poll() is not None:
            raise FalhaTeste("el QEMU ha terminado")
        time.sleep(0.05)
    dt = time.time() - ini
    time.sleep(0.5)
    return dt, m.serial()[len(antes):].replace("\r", "")


def cifras(m):
    """«sync -v»: las cifras del pendrive y de la caché (las últimas que salen)."""
    antes = len(m.serial())
    cronometrar(m, "sync -v", 60)
    fim = time.time() + 20
    while time.time() < fim:
        s = m.serial()[antes:]
        u, c = RE_USB.findall(s), RE_CACHE.findall(s)
        if u and c:
            u = [int(x) for x in u[-1]]
            c = [int(x) for x in c[-1]]
            return {"ord_l": u[0], "kb_l": u[1], "ord_e": u[3], "kb_e": u[4], "maior": u[6], "trozo": u[7],
                    "reducoes": u[8], "encadeadas": u[9], "anticipados": c[2]}
        time.sleep(0.2)
    raise FalhaTeste("sync -v no ha dado las cifras de usb0 y de la caché: " + m.serial()[antes:][-600:])


def media(antes, depois, ordens, kb):
    n = depois[ordens] - antes[ordens]
    return (depois[kb] - antes[kb]) / n if n else 0.0, n


def sha_no_sagui(m, caminho):
    dt, out = cronometrar(m, f"sha256sum {caminho}", 900)
    r = re.search(r"([0-9a-f]{64})\s+" + re.escape(caminho) + r"\s", out)
    return (r.group(1) if r else None), dt


def parte(ctl):
    dados = random.Random(468).randbytes(TAM)
    sha = hashlib.sha256(dados).hexdigest()
    pen = criar_pendrive(f"h468-{ctl}", mb=96, rotulo="RAPIDO", arquivos={"g24.bin": dados})
    if ctl == "ehci":
        extras = ["-device", "usb-ehci,id=ehci", "-drive", f"if=none,id=p1,format=raw,file={pen}",
                  "-device", "usb-storage,bus=ehci.0,drive=p1,removable=on,id=dev-p1"]
        trozos = (240, 1024)
    else:
        extras = ["-device", "qemu-xhci,id=xhci", "-drive", f"if=none,id=p1,format=raw,file={pen}",
                  "-device", "usb-storage,bus=xhci.0,port=1,drive=p1,removable=on,id=dev-p1"]
        trozos = (1024,)
    with Maquina(f"h468-{ctl}", "ide", imagem=imagem_console(), memoria="768M", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        m.esperar_serial("[DISCO] usb0", 60)
        s = m.serial()
        r = re.search(r"\[USB\] armazenamento: ordenes de hasta (\d+) KB \(([^)]*)\)", s)
        verificar(r is not None, f"{ctl}: el registro dice el trozo elegido para el pendrive")
        trozo = int(r.group(1))
        verificar(trozo in trozos, f"{ctl}: trozo de {' o '.join(map(str, trozos))} KB (es {trozo} KB, {r.group(2)})")
        if ctl == "ehci" and trozo == 1024:
            verificar(r.group(2).startswith("VPD B0") and "[USB] VPD B0: transferencia maxima" in s,
                      f"{ctl}: 1 MB en alta velocidad solo porque lo dice la página VPD B0")
        resultados.append(f"{ctl}: trozo {trozo} KB ({r.group(2)})")
        buf = re.search(r"\[(EHCI|XHCI)\] buffer de datos de (\d+) KB", s)
        verificar(buf is not None and int(buf.group(2)) == 1024, f"{ctl}: buffer de datos del controlador de 1 MB")
        out = m.comando("montar usb0", 60)
        verificar("Pendrive montado en /usb" in out, f"{ctl}: pendrive montado en /usb")

        # ---------------- lectura con peticiones pequeñas: lectura anticipada ----------------
        c0 = cifras(m)
        got, dt = sha_no_sagui(m, "/usb/g24.bin")
        c1 = cifras(m)
        verificar(got == sha, f"{ctl}: sha256sum del fichero leído del pendrive igual al del PC")
        med, n = media(c0, c1, "ord_l", "kb_l")
        resultados.append(f"{ctl}: sha256sum de 24 MB: {dt:.2f} s, {n} ordenes de lectura, media {med:.0f} KB, "
                          f"{c1['anticipados'] - c0['anticipados']} bloques anticipados")
        print("  medida:", resultados[-1])
        verificar(med > 128, f"{ctl}: lectura secuencial pequeña: media de las órdenes de más de 128 KB ({med:.0f} KB)")
        verificar(c1["anticipados"] > c0["anticipados"], f"{ctl}: la caché lee por delante (anticipados "
                  f"{c0['anticipados']} -> {c1['anticipados']})")

        # ---------------- lectura con peticiones grandes (cp) ----------------
        dt, out = cronometrar(m, "cp /usb/g24.bin /tmp/g24.bin")
        c2 = cifras(m)
        verificar("cp:" not in out, f"{ctl}: cp del pendrive a /tmp sin error")
        med, n = media(c1, c2, "ord_l", "kb_l")
        resultados.append(f"{ctl}: cp de 24 MB del pendrive: {dt:.2f} s = {TAM / 1024 / dt:.0f} KB/s, {n} ordenes, "
                          f"media {med:.0f} KB, mayor {c2['maior']} KB")
        print("  medida:", resultados[-1])
        verificar(med > 128, f"{ctl}: cp: media de las órdenes de lectura de más de 128 KB ({med:.0f} KB)")
        got, _ = sha_no_sagui(m, "/tmp/g24.bin")
        verificar(got == sha, f"{ctl}: sha256 de la copia en /tmp igual")

        # ---------------- escritura ----------------
        dt, out = cronometrar(m, "cp /tmp/g24.bin /usb/copia.bin")
        verificar("cp:" not in out, f"{ctl}: cp de /tmp al pendrive sin error")
        cronometrar(m, "sync", 120)
        c3 = cifras(m)
        med, n = media(c2, c3, "ord_e", "kb_e")
        resultados.append(f"{ctl}: cp de 24 MB al pendrive: {dt:.2f} s = {TAM / 1024 / dt:.0f} KB/s, {n} ordenes, "
                          f"media {med:.0f} KB, encadenadas {c3['encadeadas']}, reducciones {c3['reducoes']}")
        print("  medida:", resultados[-1])
        verificar(med > 128, f"{ctl}: escritura: media de las órdenes de más de 128 KB ({med:.0f} KB)")
        verificar(c3["encadeadas"] > c0["encadeadas"], f"{ctl}: las fases van encadenadas ({c3['encadeadas']})")
        verificar(c3["reducoes"] == 0, f"{ctl}: el pendrive emulado no obliga a bajar el trozo")
        got, _ = sha_no_sagui(m, "/usb/copia.bin")
        verificar(got == sha, f"{ctl}: sha256 de /usb/copia.bin igual")

        # ---------------- desenchufar en caliente tras sincronizar ----------------
        cronometrar(m, "sync", 120)
        m.monitor("device_del dev-p1")
        m.esperar_serial("usb0 removido", 30)
        time.sleep(1)
        verificar("kernel panic" not in m.serial().lower(), f"{ctl}: sin pánico del kernel tras desenchufar")
        out = m.comando("echo sigue-vivo-468", 30)
        verificar("sigue-vivo-468" in out, f"{ctl}: el terminal responde tras desenchufar")

    destino = os.path.join(SAIDA, f"_h468_{ctl}.bin")
    mtools(pen, "mcopy", "-o", "p:/copia.bin", destino)
    with open(destino, "rb") as f:
        verificar(hashlib.sha256(f.read()).hexdigest() == sha,
                  f"{ctl}: en el PC (mtools), copia.bin grabada antes de desenchufar es idéntica")
    os.remove(destino)


def parte_plazo(ctl):
    tam = 2 * 1024 * 1024 + 777
    dados = random.Random(4680).randbytes(tam)
    sha = hashlib.sha256(dados).hexdigest()
    pen = criar_pendrive(f"h468-plazo-{ctl}", mb=96, rotulo="LENTO", arquivos={"g2.bin": dados})
    dev = "usb-ehci,id=ehci" if ctl == "ehci" else "qemu-xhci,id=xhci"
    bus = "ehci.0" if ctl == "ehci" else "xhci.0"
    extras = ["-device", dev, "-drive", f"if=none,id=p1,format=raw,file={pen},throttling.bps-write=35000",
              "-device", f"usb-storage,bus={bus},drive=p1,removable=on,id=dev-p1"]
    with Maquina(f"h468-plazo-{ctl}", "ide", imagem=imagem_console(), memoria="768M", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        m.esperar_serial("[DISCO] usb0", 60)
        verificar("Pendrive montado en /usb" in m.comando("montar usb0", 60), f"plazo {ctl}: pendrive montado")
        trozo = cifras(m)["trozo"]
        cronometrar(m, "cp /usb/g2.bin /tmp/g2.bin", 300)
        cronometrar(m, "cp /tmp/g2.bin /usb/lenta.bin", 900)
        cronometrar(m, "sync", 900)
        c = cifras(m)
        s = m.serial()
        if trozo >= 1024:
            verificar(c["reducoes"] >= 1 and "una orden de" in s, f"plazo {ctl}: el plazo agotado baja el trozo "
                      f"({trozo} -> {c['trozo']} KB, {c['reducoes']} reducciones)")
        verificar("no se ha podido reiniciar" not in s, f"plazo {ctl}: el endpoint se reinicia sin error")
        got, _ = sha_no_sagui(m, "/tmp/g2.bin")
        verificar(got == sha, f"plazo {ctl}: la lectura anterior está bien")
        verificar("sigue-vivo" in m.comando("echo sigue-vivo", 30), f"plazo {ctl}: el terminal responde")
        resultados.append(f"plazo {ctl}: trozo {trozo} -> {c['trozo']} KB, reducciones {c['reducoes']}")
    destino = os.path.join(SAIDA, f"_h468_plazo_{ctl}.bin")
    mtools(pen, "mcopy", "-o", "p:/lenta.bin", destino)
    with open(destino, "rb") as f:
        verificar(hashlib.sha256(f.read()).hexdigest() == sha, f"plazo {ctl}: en el PC, lenta.bin es idéntica")
    os.remove(destino)


def main():
    todas = ["ehci", "xhci", "plazo-ehci", "plazo-xhci"]
    partes = sys.argv[1:] or todas
    for p in partes:
        if p not in todas:
            print("uso: h468_usb_rapido.py [ehci] [xhci] [plazo-ehci] [plazo-xhci]")
            return 2
    for p in partes:
        parte_plazo(p[6:]) if p.startswith("plazo-") else parte(p)
    os.makedirs(SAIDA, exist_ok=True)
    with open(os.path.join(SAIDA, "h468-resultados.txt"), "w") as f:
        f.write("\n".join(resultados) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
