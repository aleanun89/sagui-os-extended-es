#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H355 - opciones del kernel guardadas desde el sistema y aplicadas al reiniciar (D353, kconfig).

Una sola imagen (consola, sesión sagui, frecuencia simulada, 6 GB y CPU qemu32 con PAE) que se
arranca varias veces sin instantánea (lo grabado en la cabecera de arranque sigue ahí):
  A. `kconfig` lista las opciones; como sagui, `kconfig pae=no` se niega (hace falta administrador) y
     la cabecera no cambia; con `admin kconfig ...` se guardan pae=no, cache_alta=no, dma=no,
     cache_max_mb=32, vaciado_s=5 y gobernador=ahorro (este, también al momento: `frecuencia`);
     un valor fuera de rango (cache_max_mb=5000) se rechaza sin guardar nada.
  B. Al reiniciar el kernel las aplica: paginación sin PAE (registro, `free`, `mem`), caché en la baja y
     con tope de 32 MB, IDE por PIO, gobernador ahorro; `kconfig` lo enseña. `gobernador=defecto`
     quita solo esa.
  C. Salvavidas: el modo rescate (el bit que pone Ctrl al arrancar, puesto desde el anfitrión) NO aplica
     lo guardado (PAE, DMA y caché alta vuelven); `kconfig --por-defecto` desde el shell de root lo
     borra (la zona queda a cero en el disco).
  D. Sin rescate: los valores de la imagen; con una zona escrita a mano con vaciado_s=99 (fuera de
     rango) y sucios_max_mb=2, el kernel ignora la primera con aviso en el registro y aplica la segunda;
     una zona con la suma mala se ignora entera (E).
  G. La swap comprimida (D352): `admin kconfig swap=no swap_mb=64` (swap_mb=2 se rechaza: desde 4); al
     reiniciar el kernel no la activa y `free` dice «desactivada»; con `swap=defecto` vuelve con el tope
     de 64 MB guardado; `--por-defecto` lo borra. Necesita A (la contraseña de root).
  F. La ventana: Configuración > Pantalla > Kernel con la sesión sagui; «Cambiar» en PAE pide administrador
     (diálogo de D269) y lo guarda; «De la imagen» lo quita. Capturas H355-4 y H355-5.
Con SAGUI_KVM=1 se prueba con KVM.
"""
import os
import re
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

CLAVE = "root1234"
MEM = "6G"
CAB = 512                 # LBA 1 en el archivo de la imagen
CAB_OPCOES = 6
OPC_RESCATE = 0x4000
ZONA, PRES, VALS, SOMA = 192, 196, 200, 248
HUECO = {"pae": 0, "nx": 1, "cache_disco": 2, "cache_alta": 3, "escritura_diferida": 4, "dma": 5,
         "cache_max_mb": 6, "sucios_max_mb": 7, "vaciado_s": 8, "frecuencia": 9, "gobernador": 10,
         "swap": 11, "swap_mb": 12}


def ler_cab(img):
    with open(img, "rb") as f:
        f.seek(CAB)
        return bytearray(f.read(512))


def gravar_cab(img, c):
    with open(img, "r+b") as f:
        f.seek(CAB)
        f.write(bytes(c))


def zona(c):
    """{nombre: valor} de lo guardado, o None sin firma; "MALA" si la suma no cuadra."""
    if c[ZONA:ZONA + 4] != b"SGKC":
        return None
    if struct.unpack_from("<I", c, SOMA)[0] != sum(c[ZONA:SOMA]):
        return "MALA"
    pres = struct.unpack_from("<I", c, PRES)[0]
    return {n: struct.unpack_from("<H", c, VALS + 2 * h)[0] for n, h in HUECO.items() if pres >> h & 1}


def escrever_zona(c, valores, suma_buena=True):
    c[ZONA:256] = bytes(256 - ZONA)
    c[ZONA:ZONA + 4] = b"SGKC"
    pres = 0
    for n, v in valores.items():
        pres |= 1 << HUECO[n]
        struct.pack_into("<H", c, VALS + 2 * HUECO[n], v)
    struct.pack_into("<I", c, PRES, pres)
    struct.pack_into("<I", c, SOMA, sum(c[ZONA:SOMA]) + (0 if suma_buena else 1))


def rescate(img, sim):
    c = ler_cab(img)
    op = struct.unpack_from("<H", c, CAB_OPCOES)[0]
    op = (op | OPC_RESCATE) if sim else (op & ~OPC_RESCATE)
    struct.pack_into("<H", c, CAB_OPCOES, op)
    gravar_cab(img, c)


def admin(m, orden, tempo=90):
    return m.comando(f"echo {CLAVE} | admin {orden}", tempo)


def apagar(m, et):
    m.digitar("apagar-pc\n", 0.05)
    fim = time.time() + 120
    while time.time() < fim and m.proc.poll() is None:
        time.sleep(0.3)
    verificar(m.proc.poll() is not None, f"[{et}] apagar-pc apaga el equipo")


def modo_pag(s):
    g = re.findall(r"paginacao: modo (PAE|sem PAE)", s)
    return g[-1] if g else "?"


def sem_erros(s, et):
    verificar("EXCECAO" not in s, f"[{et}] sin excepciones del kernel")


def parte_a(img):
    with Maquina("h355-a", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar("[PERMISOS] sesion como sagui" in s, "[A] la sesión es sagui")
        verificar(modo_pag(s) == "PAE", f"[A] arranque normal con PAE ({modo_pag(s)})")
        verificar("[KCONF]" not in s, "[A] sin nada guardado, el kernel no anota nada de kconfig")
        out = m.comando("kconfig")
        verificar("pae" in out and "cache_max_mb" in out and "gobernador" in out and "AHORA" in out,
                  f"[A] kconfig lista las opciones ({out[-600:]!r})")
        antes = bytes(ler_cab(img))
        out = m.comando("kconfig pae=no")
        verificar("hace falta ser administrador" in out, f"[A] sin administrador, kconfig no guarda ({out!r})")
        verificar("[KCONF] guardado" not in m.serial(), "[A] el kernel no grabó nada para sagui")
        out = m.comando("kconfig cache_max_mb=5000")
        verificar("fuera de rango" in out, f"[A] un valor fuera de rango se rechaza ({out!r})")
        # La primera vez root elige su contraseña (dos veces) en la sesión local.
        out = m.comando(f"echo {CLAVE} > /tmp/p; echo {CLAVE} >> /tmp/p; admin echo listo < /tmp/p; rm /tmp/p", 120)
        verificar("listo" in out, f"[A] root elige contraseña ({out[-300:]!r})")
        out = admin(m, "kconfig pae=no cache_alta=no dma=no cache_max_mb=32 vaciado_s=5 gobernador=ahorro")
        verificar("pae: guardado no" in out and "se aplica al reiniciar" in out, f"[A] admin kconfig guarda ({out!r})")
        verificar(re.search(r"\[KCONF\] guardado gobernador=2 en \S+: 0", m.serial()) is not None,
                  "[A] el kernel grabó el gobernador en el disco de arranque")
        verificar(re.search(r"\[KCONF\] gobernador aplicado al momento: 0", m.serial()) is not None,
                  "[A] el gobernador se aplica al momento")
        out = m.comando("frecuencia")
        verificar("ahorro" in out, f"[A] frecuencia dice ahorro sin reiniciar ({out[-300:]!r})")
        out = admin(m, "kconfig cache_max_mb=5000")
        verificar("fuera de rango" in out, f"[A] fuera de rango también como administrador ({out!r})")
        out = m.comando("kconfig")
        verificar("cambia al reiniciar" in out, f"[A] kconfig avisa de que hace falta reiniciar ({out[-900:]!r})")
        m.captura("H355-1-kconfig")
        sem_erros(m.serial(), "A")
        apagar(m, "A")
    verificar(ler_cab(img)[:192] == antes[:192], "[A] los bytes de la imagen (0-191) no cambian")
    z = zona(ler_cab(img))
    esperado = {"pae": 0, "cache_alta": 0, "dma": 0, "cache_max_mb": 32, "vaciado_s": 5, "gobernador": 2}
    verificar(z == esperado, f"[A] la zona guardada en el disco es la pedida ({z})")


def parte_b(img):
    with Maquina("h355-b", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar(re.search(r"\[KCONF\] aplicada la configuracion guardada: pae=0 cache_alta=0 dma=0 cache_max_mb=32 "
                            r"vaciado_s=5 gobernador=2", s) is not None, "[B] el kernel anota lo que aplica")
        verificar(modo_pag(s) == "sem PAE", f"[B] paginación sin PAE ({modo_pag(s)})")
        verificar("[ATA] DMA desactivado" in s, "[B] el disco IDE va por PIO")
        verificar("[CACHE] datos en la memoria alta" not in s, "[B] la caché no va a la alta")
        verificar(re.search(r"\[CACHE\] kconfig: hasta 32 MB, sucios hasta \d+ KB, un bloque sucio se graba a los 5 s", s)
                  is not None, "[B] caché con tope de 32 MB y vaciado a los 5 s")
        verificar("[FREQ] politica ahorro (kconfig)" in s, "[B] el gobernador ahorro al arrancar")
        out = m.comando("free -m")
        verificar("sin PAE (desactivado" in out, f"[B] free dice sin PAE ({out[-400:]!r})")
        out = m.comando("mem")
        verificar("sin PAE" in out, "[B] mem dice sin PAE")
        out = m.comando("frecuencia")
        verificar("ahorro" in out, "[B] frecuencia: ahorro")
        out = m.comando("kconfig")
        verificar(re.search(r"\npae\s+no\s+s\S\s+no", out) is not None, f"[B] kconfig: pae no ahora, sí en la imagen ({out!r})")
        verificar("cambia al reiniciar" not in out, "[B] nada pendiente de reiniciar")
        out = admin(m, "kconfig gobernador=defecto")
        verificar("gobernador: vuelve al de la imagen" in out, f"[B] gobernador=defecto quita solo esa ({out!r})")
        m.captura("H355-2-aplicado")
        sem_erros(m.serial(), "B")
        apagar(m, "B")
    z = zona(ler_cab(img))
    verificar(isinstance(z, dict) and "gobernador" not in z and z.get("pae") == 0, f"[B] en el disco, todo menos el gobernador ({z})")


def parte_c(img):
    rescate(img, True)   # lo que hace Ctrl al arrancar (OPC_RESCATE): el mismo bit en boot_info
    with Maquina("h355-c", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/#", 400)
        s = m.serial()
        verificar("[SAGUI] MODO RESCATE" in s, "[C] modo rescate")
        verificar("[KCONF] modo rescate: la configuracion guardada del kernel NO se aplica" in s,
                  "[C] el rescate no aplica lo guardado (registro)")
        verificar(modo_pag(s) == "PAE" and "[ATA] DMA desactivado" not in s and "[CACHE] datos en la memoria alta" in s,
                  f"[C] valores de la imagen: PAE ({modo_pag(s)}), DMA y caché alta")
        out = m.comando("kconfig")
        verificar("no aplicado: modo rescate" in out and "Modo rescate" in out, f"[C] kconfig lo dice ({out[-900:]!r})")
        out = m.comando("kconfig --por-defecto")
        verificar("borrada" in out, f"[C] kconfig --por-defecto desde el shell de root ({out!r})")
        m.captura("H355-3-rescate")
        sem_erros(m.serial(), "C")
        apagar(m, "C")
    c = ler_cab(img)
    verificar(c[ZONA:256] == bytes(256 - ZONA), "[C] la zona guardada queda a cero en el disco")
    rescate(img, False)


def parte_d(img):
    c = ler_cab(img)
    escrever_zona(c, {"vaciado_s": 99, "sucios_max_mb": 2})
    gravar_cab(img, c)
    with Maquina("h355-d", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar(modo_pag(s) == "PAE" and "[ATA] DMA desactivado" not in s and "[CACHE] datos en la memoria alta" in s,
                  f"[D] tras --por-defecto, los valores de la imagen: PAE ({modo_pag(s)}), DMA y caché alta")
        verificar(re.search(r"\[KCONF\] AVISO: vaciado_s=99 fuera de rango \(1 a 30", s) is not None,
                  "[D] el valor fuera de rango se ignora con aviso")
        verificar("[KCONF] aplicada la configuracion guardada: sucios_max_mb=2" in s, "[D] el resto sí se aplica")
        verificar(re.search(r"\[CACHE\] kconfig: hasta \d+ MB, sucios hasta 2048 KB, un bloque sucio se graba a los 3 s", s)
                  is not None, "[D] sucios hasta 2 MB y el vaciado de serie (3 s)")
        out = m.comando("kconfig")
        verificar("guardado fuera de rango" in out, f"[D] kconfig lo enseña ({out[-900:]!r})")
        out = admin(m, "kconfig --por-defecto")
        verificar("borrada" in out, f"[D] admin kconfig --por-defecto ({out!r})")
        sem_erros(m.serial(), "D")
        apagar(m, "D")
    c = ler_cab(img)
    verificar(c[ZONA:256] == bytes(256 - ZONA), "[D] la zona queda a cero")
    # E: una zona con la suma mala no se aplica (ni lo que parezca bueno).
    escrever_zona(c, {"pae": 0}, suma_buena=False)
    gravar_cab(img, c)
    with Maquina("h355-e", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar("[KCONF] la configuracion guardada del kernel no pasa la suma" in s, "[E] suma mala: anotada")
        verificar(modo_pag(s) == "PAE", f"[E] y no se aplica: PAE ({modo_pag(s)})")
        sem_erros(m.serial(), "E")


def parte_g(img):
    with Maquina("h355-g1", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar(re.search(r"\[SWAP\] swap comprimida: tope \d+ MB \(la mitad de la RAM\)", s) is not None,
                  "[G] sin nada guardado, la swap de la imagen (automática)")
        out = admin(m, "kconfig swap_mb=2")
        verificar("fuera de rango" in out, f"[G] swap_mb=2 se rechaza (desde 4) ({out!r})")
        out = admin(m, "kconfig swap=no swap_mb=64")
        verificar("swap: guardado no" in out and "swap_mb: guardado 64 MB" in out, f"[G] admin kconfig swap=no ({out!r})")
        sem_erros(m.serial(), "G1")
        apagar(m, "G1")
    z = zona(ler_cab(img))
    verificar(z == {"swap": 0, "swap_mb": 64}, f"[G] en el disco, swap=0 y swap_mb=64 ({z})")
    with Maquina("h355-g2", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar("[SWAP] swap comprimida desactivada (kconfig swap=no)" in s, "[G] al reiniciar, el kernel no activa la swap")
        out = m.comando("free")
        verificar(re.search(r"Swap \(RAM\):\s+desactivada", out) is not None, f"[G] free: desactivada ({out[-400:]!r})")
        out = admin(m, "kconfig swap=defecto")
        verificar("swap: vuelve al de la imagen" in out, f"[G] swap=defecto ({out!r})")
        sem_erros(m.serial(), "G2")
        apagar(m, "G2")
    with Maquina("h355-g3", "ide", imagem=img, memoria=MEM, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        s = m.serial()
        verificar("[SWAP] swap comprimida: tope 64 MB (kconfig swap_mb)" in s, "[G] la swap vuelve, con el tope guardado de 64 MB")
        out = admin(m, "kconfig --por-defecto")
        verificar("borrada" in out, f"[G] admin kconfig --por-defecto ({out!r})")
        sem_erros(m.serial(), "G3")
        apagar(m, "G3")
    verificar(ler_cab(img)[ZONA:256] == bytes(256 - ZONA), "[G] la zona queda a cero")


def parte_f():
    from h323_conf_aceleracion import esperar, ventana, clic, pestana, conceder, cerrar_ventana
    y_pag = 50
    sub_kernel = (708 + 37, y_pag + 2 + 13)
    fila0 = y_pag + 74 + 11            # KC_Y0 de configuracion.c, la fila de «pae»
    cambiar, imagen = (528 + 48, fila0), (628 + 65, fila0)
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h355-f", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 400)
        esperar(m, r"supervisor en marcha", 90)
        time.sleep(3)
        n = len(m.serial())
        m.comando("configuracion --ventana &", 20)
        esperar(m, r'"Configuraci.n"', 30, n)
        time.sleep(1.5)
        v = ventana(m, "Configuración", n)
        clic(m, v, pestana("Pantalla"))
        time.sleep(0.8)
        clic(m, v, sub_kernel)
        esperar(m, r"configuracion: pantalla vista kernel", 15, n)
        time.sleep(1)
        m.captura("H355-4-ventana")
        n = len(m.serial())
        clic(m, v, cambiar)
        conceder(m, n, primera=True)
        esperar(m, r"\[KCONF\] guardado pae=0 en \S+: 0", 30, n)
        verificar(True, "[F] «Cambiar» en PAE, con el diálogo de administrador, guarda pae=0")
        time.sleep(1.5)
        m.captura("H355-5-ventana-cambiado")
        n = len(m.serial())
        clic(m, v, imagen)
        fim = time.time() + 8
        while time.time() < fim and "dialogo de permisos: configuracion" not in m.serial()[n:] and "[KCONF] pae vuelve" not in m.serial()[n:]:
            time.sleep(0.3)
        if "dialogo de permisos: configuracion" in m.serial()[n:]:
            conceder(m, n)
        esperar(m, r"\[KCONF\] pae vuelve al valor de la imagen en \S+: 0", 30, n)
        verificar(True, "[F] «De la imagen» quita lo guardado")
        cerrar_ventana(m, v)
        sem_erros(m.serial(), "F")


def main():
    img = imagem_de_teste("", ["--vesa", "--console", "--sesion-sagui", "--frecuencia-simulada", "--dados-mb", "16"])
    c = ler_cab(img)
    verificar(zona(c) is None, "la imagen recién hecha no lleva nada guardado")
    so = os.environ.get("H355_SO", "abcdgf")
    if "a" in so:
        parte_a(img)
    if "b" in so:
        parte_b(img)
    if "c" in so:
        parte_c(img)
    if "d" in so:
        parte_d(img)
    if "g" in so:
        parte_g(img)
    if "f" in so:
        parte_f()
    print("H355 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
