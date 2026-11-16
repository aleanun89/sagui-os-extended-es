#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H352 - la caché de disco en la memoria alta o superior (D350, fase 3a de la mejora 125).

Partes (por defecto todas; o las de H352_SO, separadas por comas):
  6G      BIOS, CPU por defecto (qemu32, PAE): la caché pone sus datos en la alta/superior;
  3G      CPU sin PAE (qemu32,-pae): la caché en la alta bajo 4 GB;
  smp     6G con --smp y 4 CPU: lo mismo, con los programas que piden memoria en varias CPU;
  512M    sin memoria alta: la caché sigue en la baja como siempre (D281);
  opcion  3G con la imagen hecha con --cache-disco-baja: la caché en la baja aunque haya alta.
En cada una:
  - el auditor (tools/auditar_memalta.py) no encuentra direcciones físicas usadas como puntero;
  - un disco IDE con un ext2 y un archivo grande al azar: se lee dos veces con sha256sum (las dos
    sumas, iguales a la del anfitrión); en la segunda, casi todo son aciertos de la caché ([CACHE]
    de `sync -v`), y con alta casi todos los bloques están en la alta (también en `free` y `mem`);
  - con alta: varios procesos (/testes/memoria varios) piden más memoria que la alta libre: la
    caché cede bloques (cedidos > 0, menos bloques), los programas terminan bien y sus sumas no
    cambian; a la vez, un sha256sum del archivo en segundo plano da la suma buena; después el
    archivo se vuelve a leer con la suma buena;
  - escritura diferida desde bloques altos: cp del archivo, sha256sum de la copia, `sync -v` deja
    0 sucios, apagar-pc, y en el anfitrión e2fsck limpio y la copia con la suma buena;
  - sin excepciones del kernel ni errores de las ventanas.
Con SAGUI_KVM=1 se prueba con KVM.
"""
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, imagem_de_teste, criar_disco,  # noqa: E402
                  extrair_particao, e2fsck, debugfs)

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAM_MB = 128                       # el archivo grande
PART_MB = 2 * TAM_MB + 64          # ext2 con sitio para la copia
DATOS = os.urandom(1 << 20) * TAM_MB
SHA = hashlib.sha256(DATOS).hexdigest()
BLOCOS = TAM_MB * 256              # bloques de 4 KB del archivo
RE_CACHE = re.compile(r"\[CACHE\] bloques (\d+)/(\d+), sucios (\d+), aciertos (\d+), fallos (\d+), anticipados (\d+), "
                      r"grabados (\d+), vaciados (\d+), directos (\d+), errores (\d+), altos (\d+), cedidos (\d+), "
                      r"reclamos (\d+) \(ocupada (\d+)\)")
CAMPOS = ("blocos", "max", "sujos", "acertos", "falhas", "anticipados", "gravados", "vaciados", "diretos", "erros",
          "altos", "cedidos", "reclamos", "ocupada")


def estado(m):
    """Las cifras de la caché tras `sync -v` (un dict con CAMPOS)."""
    antes = len(RE_CACHE.findall(m.serial()))
    m.comando("sync -v", 120)
    fim = time.time() + 30
    while time.time() < fim:
        r = RE_CACHE.findall(m.serial())
        if len(r) > antes:
            return dict(zip(CAMPOS, (int(x) for x in r[-1])))
        time.sleep(0.3)
    return dict.fromkeys(CAMPOS, -1)


def free_m(m):
    """{"mem", "baja", "alta"}: (total, usado, libre) en MB, y la caché (total, en la alta), de `free -m`."""
    out = m.comando("free -m")
    r = {}
    for nome, rot in (("mem", r"Mem:"), ("baja", r"baja:"), ("alta", r"alta:")):
        g = re.search(rot + r"\s+(\d+)\s+(\d+)\s+(\d+)", out)
        r[nome] = tuple(int(x) for x in g.groups()) if g else (-1, -1, -1)
    g = re.search(r"Cach. disco:\s+(\d+)\s+\(en la alta (\d+)", out)
    r["cache"] = (int(g.group(1)), int(g.group(2))) if g else (-1, -1)
    return r, out


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("ERRO memalta" not in s, f"[{et}] sin errores de las ventanas temporales")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")
    verificar("AVISO: kfree" not in s, f"[{et}] ningún kfree incorrecto")


def ler_soma(m, caminho, et, rotulo):
    t0 = time.time()
    out = m.comando(f"sha256sum {caminho}", 900)
    dt = time.time() - t0
    verificar(SHA in out, f"[{et}] {rotulo}: la suma de {caminho} es la del anfitrión ({dt:.1f} s)")
    return dt


def parte(nome, et, memoria, alta, imagem=None, extras=None, nucleos=1):
    hd, ini = criar_disco(nome, [dict(tipo="ext2", mb=PART_MB, rotulo="GRANDE", arquivos={"grande.bin": DATOS})])
    discos = ["-drive", f"file={hd},if=ide,index=1,format=raw"]
    with Maquina(nome, "ide", imagem=imagem or imagem_console(), memoria=memoria, nucleos=nucleos,
                 extras=(extras or []) + discos, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        g = re.search(r"\[CACHE\] caché de bloques activa: hasta (\d+) bloques", s)
        verificar(g is not None, f"[{et}] la caché de bloques está activa")
        g2 = re.search(r"\[CACHE\] datos en la memoria alta: hasta (\d+) MB, (\d+) MB altos siempre libres", s)
        if alta:
            verificar(g2 is not None and int(g2.group(1)) >= 256,
                      f"[{et}] la caché va a la memoria alta ({g2.group(1) if g2 else '?'} MB como mucho, "
                      f"reserva {g2.group(2) if g2 else '?'} MB)")
        else:
            if memoria != "512M":
                verificar("datos en la memoria baja (--cache-disco-baja)" in s, f"[{et}] el registro dice --cache-disco-baja")
            verificar(g2 is None and g is not None and int(g.group(1)) <= 16384,
                      f"[{et}] sin alta: la caché sigue en la baja, como mucho 64 MB ({g.group(1) if g else '?'} bloques)")
        m.comando("mount -o rw hdb1 /interno")
        e0 = estado(m)

        # Dos lecturas: la segunda, de la caché.
        t1 = ler_soma(m, "/interno/grande.bin", et, "primera lectura")
        e1 = estado(m)
        t2 = ler_soma(m, "/interno/grande.bin", et, "segunda lectura")
        e2 = estado(m)
        ac, fa = e2["acertos"] - e1["acertos"], e2["falhas"] - e1["falhas"]
        print(f"[{et}] lecturas: {t1:.1f} s y {t2:.1f} s; segunda: {ac} aciertos, {fa} fallos; "
              f"bloques {e2['blocos']} (altos {e2['altos']})")
        if alta:
            verificar(ac >= BLOCOS * 0.95 and fa <= BLOCOS * 0.02,
                      f"[{et}] la segunda lectura va de la caché ({ac} aciertos, {fa} fallos de {BLOCOS} bloques)")
            verificar(e2["blocos"] >= BLOCOS * 0.95 and e2["altos"] >= e2["blocos"] * 0.95,
                      f"[{et}] los bloques están en la alta ({e2['altos']} de {e2['blocos']})")
            verificar(t2 <= t1 * 1.10, f"[{et}] la segunda lectura no es más lenta ({t2:.1f} s frente a {t1:.1f} s)")
        else:
            verificar(e2["altos"] == 0 and e2["blocos"] <= e2["max"], f"[{et}] ningún bloque en la alta ({e2['altos']})")
        f0, out = free_m(m)
        verificar(f0["cache"][0] >= 0, f"[{et}] free muestra la caché ({f0['cache'][0]} MB, {f0['cache'][1]} en la alta)")
        if alta:
            verificar(f0["cache"][1] >= TAM_MB * 0.9, f"[{et}] free: {f0['cache'][1]} MB de caché en la alta")
        out = m.comando("mem")
        verificar(re.search(r"Cach. de disco:\s+\d+ MB \(en la alta \d+ MB\)", out) is not None, f"[{et}] mem muestra la caché")

        if alta:
            # Programas que piden más que la alta libre: la caché tiene que ceder.
            libre_alta, cache_alta, libre_baja = f0["alta"][2], f0["cache"][1], f0["baja"][2]
            objetivo = libre_alta + int(cache_alta * 0.7)
            k = max(2, -(-objetivo // 1200))
            mb = -(-objetivo // k)
            verificar(k * mb < libre_alta + cache_alta + libre_baja - 300, f"[{et}] el reparto cabe ({k} x {mb} MB)")
            print(f"[{et}] alta libre {libre_alta} MB, caché en la alta {cache_alta} MB: {k} procesos de {mb} MB")
            # A la vez, una lectura del disco: la caché tiene que ceder también mientras la usan.
            m.comando("sha256sum /interno/grande.bin > /tmp/suma_fondo.txt &")
            out = m.comando(f"/testes/memoria varios {k} {mb}", 3600)
            verificar(f"varios: {k} de {k} procesos con {mb} MB cada uno: todos ok" in out,
                      f"[{et}] {k} procesos de {mb} MB (más que la alta libre): ninguno falla")
            verificar(out.count("igual, 0 errores") == k, f"[{et}] las {k} sumas de los programas coinciden")
            e3 = estado(m)
            print(f"[{et}] tras los programas: bloques {e3['blocos']} (altos {e3['altos']}), cedidos {e3['cedidos']}, "
                  f"reclamos {e3['reclamos']} (con la caché ocupada {e3['ocupada']})")
            verificar(e3["cedidos"] > e2["cedidos"] and e3["blocos"] < e2["blocos"],
                      f"[{et}] la caché cedió memoria a los programas ({e3['cedidos'] - e2['cedidos']} bloques; "
                      f"{e2['blocos']} -> {e3['blocos']})")
            fim = time.time() + 300
            fundo = ""
            while time.time() < fim and SHA not in fundo:
                fundo = m.comando("cat /tmp/suma_fondo.txt", 30)
                if SHA not in fundo:
                    time.sleep(2)
            verificar(SHA in fundo, f"[{et}] la lectura simultánea a los programas da la suma buena")
            ler_soma(m, "/interno/grande.bin", et, "tras ceder memoria")

        # Escritura diferida con los bloques en la alta (o en la baja, sin alta).
        m.comando("cp /interno/grande.bin /interno/copia.bin", 900)
        ler_soma(m, "/interno/copia.bin", et, "la copia releída")
        e4 = estado(m)
        verificar(e4["sujos"] == 0 and e4["erros"] == 0, f"[{et}] sync deja 0 bloques sucios y sin errores")
        sem_erros(m.serial(), et)
        m.captura(f"H352-{nome}")
        m.digitar("apagar-pc\n", 0.05)
        fim = time.time() + 90
        while time.time() < fim and m.proc.poll() is None:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, f"[{et}] apagar-pc apaga el equipo")
    p1 = extrair_particao(hd, ini[0], PART_MB)
    cod, txt = e2fsck(p1)
    verificar(cod == 0, f"[{et}] e2fsck -fn tras apagar: limpio")
    verificar(hashlib.sha256(debugfs(p1, "cat /copia.bin")).hexdigest() == SHA,
              f"[{et}] la copia grabada desde la caché tiene la suma buena en el disco")
    for f in (p1, hd):
        try:
            os.remove(f)
        except OSError:
            pass


def main():
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "auditar_memalta.py")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "auditor: ninguna dirección física usada como puntero" +
              ("" if r.returncode == 0 else "\n" + r.stdout))
    so = os.environ.get("H352_SO", "")
    partes = [
        ("6G", lambda: parte("h352-6G", "6G PAE", "6G", True)),
        ("3G", lambda: parte("h352-3G", "3G sin PAE", "3G", True, extras=["-cpu", "qemu32,-pae"])),
        ("512M", lambda: parte("h352-512M", "512M", "512M", False)),
        ("smp", lambda: parte("h352-smp", "6G --smp", "6G", True, nucleos=4,
                              imagem=imagem_de_teste("", ["--vesa", "--console", "--smp"]))),
        ("opcion", lambda: parte("h352-opcion", "3G --cache-disco-baja", "3G", False,
                                 imagem=imagem_de_teste("", ["--vesa", "--console", "--cache-disco-baja"]))),
    ]
    for n, f in partes:
        if not so or n in so.split(","):
            f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
