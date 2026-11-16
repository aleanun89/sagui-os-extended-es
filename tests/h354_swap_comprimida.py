#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H354 - swap comprimida en RAM (D352, fase 3b de la mejora 125; fase 3 de la 069).

Partes (H354_SO=parte1,parte2 para elegir):
  auto      la autoprueba del kernel 'troca' (compresor, sacar, traer, permisos, tirar, el
            almacén vuelve vacío) con 3G (almacén en la alta) y 512M (sin alta, en la baja);
  1G        sin memoria alta: un proceso de 1300 MB y dos de 700 MB a la vez en 1 GB, varias
            vueltas comprobando todas las palabras: sin errores, la swap se usó, y al terminar
            vuelve la memoria y el almacén se vacía; free, mem e infopc muestran la swap;
  512M      un proceso de 700 MB en 512 MB (almacén en la baja, tope pequeño);
  sinpae    CPU sin PAE (qemu32,-pae) con 3G: cuatro procesos de 1000 MB (4 GB en 3 GB);
  smp       --smp, 4 CPU, 1G: cuatro hilos tocan a la vez las mismas páginas mientras otro las
            saca (MEM_SACAR) y la tarea de la swap las saca y las trae; y el lector;
  lector    1 CPU, sin caché de disco: un hilo lee del disco a un búfer que otro hilo saca a la
            swap sin parar; el kernel encuentra el búfer fuera al copiar tras el DMA y lo trae
            en su propio fallo de página (sin pantalla de error, mismos datos);
  sinswap   --sin-swap: free dice «desactivada» y un proceso de 1300 MB en 1G no obtiene la
            memoria (como antes); el sistema sigue respondiendo.
Con SAGUI_KVM=1 se prueba con KVM (la CPU es la del anfitrión).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

ARQUIVO_LECTOR = "/apps/bin/dosbox"


def imagem(*extras):
    return imagem_de_teste("", ["--vesa", "--console", *extras])


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")
    verificar("ERRO memalta" not in s, f"[{et}] sin errores de las ventanas temporales")
    verificar("liberando pagina fisica invalida" not in s, f"[{et}] ninguna página liberada dos veces")
    verificar("[SWAP] ERRO" not in s and "[SWAP] AVISO" not in s, f"[{et}] ningún trozo de la swap corrupto o inválido")


def free_m(m):
    out = m.comando("free -m")
    r = {}
    for nome, rot in (("mem", r"Mem:"), ("swap", r"Swap \(RAM\):")):
        g = re.search(rot + r"\s+(\d+)\s+(\d+)\s+(\d+)", out)
        r[nome] = tuple(int(x) for x in g.groups()) if g else (-1, -1, -1)
    return r, out


def info_swap(m):
    out = m.comando("/testes/troca info")
    r = []
    for rot in ("tope KB", "datos KB", "comprimidos KB", "almacen KB", "salidas", "entradas"):
        g = re.search(r"swap " + rot + r": (\d+)", out)
        if not g:
            return None
        r.append(int(g.group(1)))
    return tuple(r)


def maximo(out, rot):
    v = [int(x) for x in re.findall(rot, out)]
    return max(v) if v else -1


def autoteste():
    for mem in ("3G", "512M"):
        with Maquina(f"h354-auto-{mem}", "ide", imagem=imagem_de_teste("troca"), memoria=mem) as m:
            m.esperar_serial("[TESTE] troca ", 180)
            s = m.serial()
            verificar("[TESTE] troca ok" in s and "[TESTE] falha" not in s, f"[auto {mem}] autoprueba 'troca' pasa")
            verificar("compresor 40/40" in s, f"[auto {mem}] el compresor devuelve las 40 páginas iguales")
            verificar("volta 2: 51 paginas na swap" in s, f"[auto {mem}] 51 de 64 páginas a la swap (las aleatorias no)")
            sem_erros(s, f"auto {mem}")


def sem_alta_1g():
    et = "1G"
    with Maquina("h354-1G", "ide", imagem=imagem(), memoria="1G") as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        verificar("[SWAP] swap comprimida: tope" in s and "sin alta" in s, f"[{et}] la swap arranca en la baja (sin alta)")
        f0, out = free_m(m)
        verificar(150 <= f0["swap"][0] <= 300 and f0["swap"][1] == 0, f"[{et}] free: swap de {f0['swap'][0]} MB, vacía")
        out = m.comando("/testes/troca llenar 1300 2", 1800)
        verificar("llenar 1300 MB, 2 vueltas, 1 hilos: ok (0 errores)" in out,
                  f"[{et}] un proceso de 1300 MB en 1 GB: todas las palabras bien en las 2 vueltas")
        sw = maximo(out, r"swap maxima (\d+) MB")
        verificar(sw > 200, f"[{et}] la swap se usó ({sw} MB como mucho)")
        out = m.comando("/testes/troca varios 2 700 2", 1800)
        verificar("varios: 2 de 2 procesos con 700 MB cada uno: todos ok" in out, f"[{et}] dos procesos de 700 MB a la vez")
        out = m.comando("free -m")
        verificar(re.search(r"en la swap:.*salidas \d{4,}", out) is not None, f"[{et}] free muestra la swap usada")
        out = m.comando("mem")
        verificar("Swap comprimida:" in out and "tope de" in out, f"[{et}] mem muestra la swap")
        out = m.comando("infopc -t memoria", 120)
        verificar("Swap comprimida en RAM:" in out, f"[{et}] infopc muestra la swap")
        f1, _ = free_m(m)
        verificar(abs(f1["mem"][2] - f0["mem"][2]) <= 8, f"[{et}] al terminar vuelve la memoria ({f0['mem'][2]} -> {f1['mem'][2]} MB libres)")
        i = info_swap(m)
        verificar(i is not None and i[3] <= 2048, f"[{et}] el almacén queda casi vacío ({i[3] if i else -1} KB: páginas de los servicios en espera)")
        sem_erros(m.serial(), et)
        m.captura("H354-1G")


def sem_alta_512m():
    et = "512M"
    with Maquina("h354-512M", "ide", imagem=imagem(), memoria="512M") as m:
        m.esperar_serial("sagui:/$", 300)
        f0, _ = free_m(m)
        verificar(80 <= f0["swap"][0] <= 160, f"[{et}] tope pequeño: {f0['swap'][0]} MB (un cuarto de la RAM)")
        out = m.comando("/testes/troca llenar 700 2", 1800)
        verificar("llenar 700 MB, 2 vueltas, 1 hilos: ok (0 errores)" in out, f"[{et}] 700 MB en 512 MB, sin errores")
        out = m.comando("/testes/memoria 200", 300)
        verificar("patron ok en 200 MB" in out, f"[{et}] después, un programa normal de 200 MB")
        sem_erros(m.serial(), et)


def sin_pae():
    et = "3G sin PAE"
    with Maquina("h354-sinpae", "ide", imagem=imagem(), memoria="3G", extras=["-cpu", "qemu32,-pae"]) as m:
        m.esperar_serial("sagui:/$", 300)
        s = m.serial()
        verificar("modo sem PAE" in s, f"[{et}] paginación sin PAE")
        f0, _ = free_m(m)
        verificar(f0["swap"][0] >= 1000, f"[{et}] tope de {f0['swap'][0]} MB (la mitad de la RAM, como mucho 1 GB)")
        out = m.comando("/testes/troca varios 4 1000 1", 2400)
        verificar("varios: 4 de 4 procesos con 1000 MB cada uno: todos ok" in out,
                  f"[{et}] cuatro procesos de 1000 MB (4 GB en 3 GB): todas las palabras bien")
        verificar(maximo(out, r"swap maxima (\d+) MB") > 500, f"[{et}] la swap se usó")
        f1, _ = free_m(m)
        verificar(abs(f1["mem"][2] - f0["mem"][2]) <= 8, f"[{et}] al terminar vuelve la memoria")
        sem_erros(m.serial(), et)


def smp():
    et = "smp"
    with Maquina("h354-smp", "ide", imagem=imagem("--smp", "--sem-cache-disco"), memoria="1G", nucleos=4) as m:
        m.esperar_serial("sagui:/$", 300)
        out = m.comando("/testes/troca llenar 1200 2 4", 2400)
        verificar("llenar 1200 MB, 2 vueltas, 4 hilos: ok (0 errores)" in out,
                  f"[{et}] cuatro hilos en las mismas páginas mientras se sacan y se traen: sin errores")
        g = re.search(r"MEM_SACAR (\d+) veces", out)
        verificar(g and int(g.group(1)) > 10, f"[{et}] el hilo que saca trabajó a la vez ({g.group(1) if g else 0} veces)")
        out = m.comando(f"/testes/troca lector {ARQUIVO_LECTOR} 8", 300)
        verificar(re.search(r"lector: \d+ lecturas, \d+ veces sacado el bufer, 0 errores: ok", out) is not None,
                  f"[{et}] lecturas del disco a un búfer que otra CPU saca a la swap: mismos datos")
        sem_erros(m.serial(), et)


def lector():
    et = "lector"
    with Maquina("h354-lector", "ide", imagem=imagem("--sem-cache-disco"), memoria="1G") as m:
        m.esperar_serial("sagui:/$", 300)
        out = m.comando(f"/testes/troca lector {ARQUIVO_LECTOR} 10", 300)
        verificar(re.search(r"lector: \d+ lecturas, \d+ veces sacado el bufer, 0 errores: ok", out) is not None,
                  f"[{et}] lecturas del disco a un búfer que se saca a la swap mientras se lee: mismos datos")
        s = m.serial()
        verificar("[SWAP] fallo de pagina del kernel resuelto" in s,
                  f"[{et}] el kernel encontró el búfer en la swap a mitad de la lectura y lo trajo")
        sem_erros(s, et)


def sin_swap():
    et = "--sin-swap"
    with Maquina("h354-sinswap", "ide", imagem=imagem("--sin-swap"), memoria="1G") as m:
        m.esperar_serial("sagui:/$", 300)
        verificar("swap comprimida desactivada (--sin-swap)" in m.serial(), f"[{et}] el kernel no activa la swap")
        _, out = free_m(m)
        verificar(re.search(r"Swap \(RAM\):\s+desactivada", out) is not None, f"[{et}] free: desactivada")
        out = m.comando("/testes/troca llenar 1300 1", 600)
        verificar(re.search(r"el kernel solo dio \d+ de 1300 MB", out) is not None,
                  f"[{et}] 1300 MB en 1 GB: el kernel no los da (como antes de la swap)")
        out = m.comando("/testes/memoria 200", 300)
        verificar("patron ok en 200 MB" in out, f"[{et}] el sistema sigue: otro programa de 200 MB")
        sem_erros(m.serial(), et)


def main():
    so = os.environ.get("H354_SO", "")
    partes = [("auto", autoteste), ("1G", sem_alta_1g), ("512M", sem_alta_512m), ("sinpae", sin_pae),
              ("smp", smp), ("lector", lector), ("sinswap", sin_swap)]
    for nome, f in partes:
        if so and nome not in so.split(","):
            continue
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
