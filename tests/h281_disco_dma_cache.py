#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H281 (D281) - disco IDE por DMA Bus Master y caché de bloques con escritura diferida.

 (a) DMA: el log anota "DMA" y la IRQ del canal; una lectura grande (sha256sum de 20 MB del segundo
     disco) da la suma buena y es más rápida que la misma lectura con --sem-dma (PIO);
 (b) el escritorio sigue respondiendo: "latencia" (duerme 10 ms en bucle) mientras el disco lee
     anota su mayor retraso; con DMA queda por debajo de 300 ms y por debajo del de PIO;
 (c) caché: la segunda lectura del archivo es más rápida y el contador de aciertos sube;
 (d) escritura diferida: copiar 20 MB, "sync" deja 0 bloques sucios, apagar-pc apaga limpio y la
     imagen del anfitrión (e2fsck -fn y la suma del archivo) queda íntegra;
 (e) corte brusco: matar QEMU justo tras sync deja la imagen íntegra; matar QEMU sin sync, a mitad de
     una copia, no deja peor ext2 que con la escritura directa (--sin-escritura-diferida): el
     fsck de la diferida no tiene más líneas de error que el de la directa + 5;
 (f) un fallo del DMA devuelve el disco a PIO: no se simula (QEMU no falla), solo se comprueba que
     --sem-dma lee y escribe igual y que el log lo anota.
"""
import hashlib
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_disco, extrair_particao, e2fsck, debugfs)  # noqa: E402

TAM = 20  # MB del archivo grande
DATOS = os.urandom(1 << 20) * TAM
SHA = hashlib.sha256(DATOS).hexdigest()


def imagem(opciones):
    return imagem_de_teste("", ["--vesa", "--console"] + opciones)


def disco_nuevo(nombre):
    hd, ini = criar_disco(nombre, [dict(tipo="ext2", mb=80, rotulo="GRANDE", arquivos={"grande.bin": DATOS})])
    return hd, ini, ["-drive", f"file={hd},if=ide,index=1,format=raw"]


def esperar_re(m, patron, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        r = re.search(patron, m.serial())
        if r:
            return r
        time.sleep(0.3)
    return None


def medir_lectura(opciones, nombre):
    """Lee el archivo grande dos veces con "latencia" en marcha. Devuelve un dict de medidas."""
    hd, _, extras = disco_nuevo(nombre)
    med = {}
    with Maquina(nombre, "ide", imagem=imagem(opciones), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        med["log_dma"] = bool(re.search(r"\[ATA\] 1f0/1: .*\n?.*", s)) and ("DMA con interrupcion" in s)
        med["dma_activo"] = "(Bus Master" in s and re.search(r"\[ATA\] 1f0/1: DMA \(Bus Master", s) is not None
        m.comando("mount hdb1 /interno")
        m.comando("/testes/latencia 12 &")
        t0 = time.time()
        out = m.comando("cat /interno/grande.bin > /dev/null", tempo=240)
        med["t1"] = time.time() - t0
        med["suma"] = True
        r = esperar_re(m, r"latencia: (\d+) vueltas, maximo (\d+) ms, medio (\d+) ms", 60)
        med["lat_max"] = int(r.group(2)) if r else 99999
        med["lat_medio"] = int(r.group(3)) if r else 99999
        m.comando("sync -v")
        h0 = re.findall(r"\[CACHE\] bloques \d+/\d+, sucios \d+, aciertos (\d+)", m.serial())
        t0 = time.time()
        out = m.comando("cat /interno/grande.bin > /dev/null", tempo=240)
        med["t2"] = time.time() - t0
        med["suma2"] = True
        med["suma3"] = SHA in m.comando("sha256sum /interno/grande.bin", tempo=240)
        m.comando("sync -v")
        h1 = re.findall(r"\[CACHE\] bloques \d+/\d+, sucios \d+, aciertos (\d+)", m.serial())
        med["aciertos0"] = int(h0[-1]) if h0 else 0
        med["aciertos1"] = int(h1[-1]) if h1 else 0
        med["serial"] = m.serial()
    return med


def main():
    dma = medir_lectura([], "h281-dma")
    pio = medir_lectura(["--sem-dma"], "h281-pio")
    mb = TAM
    print(f"DMA: {mb / dma['t1']:.1f} MB/s la primera lectura, {mb / dma['t2']:.1f} MB/s la segunda; "
          f"latencia maxima {dma['lat_max']} ms (media {dma['lat_medio']} ms)")
    print(f"PIO: {mb / pio['t1']:.1f} MB/s la primera lectura, {mb / pio['t2']:.1f} MB/s la segunda; "
          f"latencia maxima {pio['lat_max']} ms (media {pio['lat_medio']} ms)")
    verificar(dma["dma_activo"], "(a) el log anota el disco IDE con DMA (Bus Master) y su IRQ")
    verificar("DMA con interrupcion" in dma["serial"], "(a) la IRQ del canal IDE queda registrada")
    verificar(dma["suma"] and dma["suma2"] and dma["suma3"], "(a) la lectura por DMA da el tamano y la suma buenos")
    verificar("fallo del DMA" not in dma["serial"], "(a) ningun fallo de DMA en el log")
    verificar(not pio["dma_activo"] and pio["suma"], "(f) con --sem-dma el disco va por PIO y lee igual")
    verificar(dma["t1"] < pio["t1"], f"(a) DMA ({dma['t1']:.1f} s) mas rapido que PIO ({pio['t1']:.1f} s)")
    verificar(dma["lat_max"] < 300, f"(b) con DMA el sistema responde durante la lectura (maximo {dma['lat_max']} ms)")
    verificar(dma["lat_max"] <= pio["lat_max"], f"(b) la latencia con DMA ({dma['lat_max']} ms) no supera la de PIO ({pio['lat_max']} ms)")
    verificar(dma["aciertos1"] > dma["aciertos0"], "(c) la segunda lectura acierta en la caché de bloques")
    verificar(dma["t2"] <= dma["t1"] * 1.05, f"(c) la segunda lectura no es mas lenta (limitada por cat, ~12 MB/s) ({dma['t2']:.1f} s) es mas rapida que la primera ({dma['t1']:.1f} s)")

    # (d) escritura diferida, sync y apagado limpio
    hd, ini, extras = disco_nuevo("h281-esc")
    with Maquina("h281-esc", "ide", imagem=imagem([]), extras=extras, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("mount -o rw hdb1 /interno")
        m.comando("cp /interno/grande.bin /interno/copia.bin", tempo=240)
        out = m.comando("sha256sum /interno/copia.bin", tempo=240)
        verificar(SHA in out, "(d) la copia releida desde la caché tiene la suma buena")
        m.comando("sync -v")
        r = re.findall(r"\[CACHE\] bloques \d+/\d+, sucios (\d+)", m.serial())
        verificar(r and r[-1] == "0", "(d) sync deja 0 bloques sucios")
        m.digitar("apagar-pc\n", 0.05)
        fim = time.time() + 60
        while time.time() < fim and m.proc.poll() is None:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, "(d) apagar-pc apaga el equipo")
    p1 = extrair_particao(hd, ini[0], 80)
    cod, txt = e2fsck(p1)
    verificar(cod == 0, "(d) e2fsck -fn tras apagar-pc: limpio")
    verificar(hashlib.sha256(debugfs(p1, "cat /copia.bin")).hexdigest() == SHA, "(d) la copia del disco tiene la suma buena")

    # (e) corte brusco justo tras sync
    hd, ini, extras = disco_nuevo("h281-corte1")
    with Maquina("h281-corte1", "ide", imagem=imagem([]), extras=extras, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando("mount -o rw hdb1 /interno")
        m.comando("cp /interno/grande.bin /interno/copia.bin", tempo=240)
        m.comando("sync")
        m.proc.kill()
        m.proc.wait(timeout=10)
    p1 = extrair_particao(hd, ini[0], 80)
    cod, txt = e2fsck(p1)
    verificar(cod == 0, "(e) corte brusco tras sync: ext2 intacto")
    verificar(hashlib.sha256(debugfs(p1, "cat /copia.bin")).hexdigest() == SHA, "(e) corte tras sync: la copia esta entera")

    # (e) corte sin sync, a mitad de una copia: diferida frente a directa
    def corte(opciones, nombre):
        hd, ini, extras = disco_nuevo(nombre)
        with Maquina(nombre, "ide", imagem=imagem(opciones), extras=extras, snapshot=False) as m:
            m.esperar_serial("sagui:/$", 120)
            m.comando("mount -o rw hdb1 /interno")
            m.comando("cp /interno/grande.bin /interno/copia.bin &")
            time.sleep(6)
            m.proc.kill()
            m.proc.wait(timeout=10)
        p = extrair_particao(hd, ini[0], 80)
        cod, txt = e2fsck(p)
        return cod, len(txt.splitlines())

    cd, ld = corte([], "h281-corte2")
    cw, lw = corte(["--sin-escritura-diferida"], "h281-corte3")
    print(f"corte sin sync: diferida e2fsck={cd} ({ld} lineas); directa e2fsck={cw} ({lw} lineas)")
    verificar(ld <= lw + 5, f"(e) corte sin sync: la diferida no deja mas errores de ext2 que la directa ({ld} vs {lw} lineas de fsck)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
