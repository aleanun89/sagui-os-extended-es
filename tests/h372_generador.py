#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H372 (D370) - generador de imágenes con ventana (tools/generador.py), probado sin pantalla.

  A: `--orden` da las órdenes correctas de cada perfil (Pública, Privada, Personalizada) y opciones;
  B: las reglas de compatibilidad bloquean las combinaciones imposibles y lo bloqueado no sale en la orden;
  C: la entrega privada se rechaza en una carpeta versionada y se acepta en entrega/privada o fuera del repo;
  D: perfiles guardados y última configuración (fichero de configuración temporal);
  E: con --sin-compilar sobre una compilación existente, genera una imagen personalizada en una carpeta
     temporal, con su SHA-256, y arranca en QEMU hasta el escritorio.

La compilación existente se busca en SAGUI_GENERADOR_BUILD o en build/ de esta copia o de la principal
(se lee, no se compila nada)."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ  # noqa: E402

GEN = os.path.join(RAIZ, "tools", "generador.py")
sys.path.insert(0, os.path.join(RAIZ, "tools"))
TMP = tempfile.mkdtemp(prefix="h372-")
CFG = os.path.join(TMP, "cfg.json")
os.environ["SAGUI_GENERADOR_CONFIG"] = CFG
import generador as g  # noqa: E402


def orden(*args):
    r = subprocess.run([sys.executable, GEN, "--orden", *args], capture_output=True, text=True, cwd=RAIZ,
                       env=dict(os.environ, SAGUI_GENERADOR_CONFIG=CFG))
    return r.returncode, r.stdout, r.stderr


def buscar_build():
    cands = [os.environ.get("SAGUI_GENERADOR_BUILD"), os.path.join(RAIZ, "build"),
             os.path.join(os.path.dirname(RAIZ), "sagui-os-main", "build"),
             os.path.join(os.path.dirname(RAIZ), "miniOS_master", "build")]
    for c in cands:
        if c and all(os.path.exists(os.path.join(c, f)) for f in ("kernel.bin", "kernel.elf", "ramdisk.sgfs", "boot/mbr.bin", "boot/estagio2.bin")):
            return c
    return None


def main():
    # ---- A: órdenes
    rc, o, e = orden("--perfil", "publica")
    verificar(rc == 0 and "./gerar_versao.sh" in o and "--todas" in o and "--con-input-share" not in o, "Pública = gerar_versao.sh --todas")
    verificar("--destino " + os.path.join(RAIZ, "entrega/latest") in o, "Pública: destino por defecto entrega/latest")
    rc, o, e = orden("--perfil", "privada")
    verificar(rc == 0 and "make priv_descargas" in o and "--con-input-share" in o and "--todas" in o, "Privada = priv_descargas + --con-input-share")
    verificar("NO PUBLICAR" in e and "entrega/privada" in o, "Privada: aviso y destino entrega/privada")
    rc, o, e = orden("--perfil", "publica", "--set", "g_todas=0", "--set", "g_iso=1", "--set", "g_fat32=1", "--set", "g_fat32_mb=512",
                     "--set", "g_variante=sem-gpl", "--set", "g_otimizacao=-Os", "--set", "g_jobs=4", "--set", "g_probar=1",
                     "--set", "g_jogos_mb=64")
    verificar("--todas" not in o and "--iso" in o and "--fat32 512" in o and "--variante sem-gpl" in o
              and "--otimizacao -Os" in o and "--jobs 4" in o and "--probar" in o and "JOGOS_MB=64" in o, f"opciones sueltas de la Pública: {o}")
    rc, o, e = orden("--perfil", "privada", "--set", "g_publicar=1")
    verificar("--publicar" not in o, "lo privado nunca lleva --publicar")
    rc, o, e = orden("--perfil", "personalizada", "--build", "/x/build", "--set", "g_sin_compilar=1")
    verificar(rc == 2 and "faltan" in e, "Personalizada sin compilación existente: error claro")
    rc, o, e = orden("--perfil", "personalizada", "--set", "mk_smp=1", "--set", "mk_resolucion=1280x720", "--set", "mk_extra_mb=64,32",
                     "--set", "mk_jogos=", "--set", "mk_apps=", "--set", "mk_uefi=")
    verificar(rc == 0 and "make " in o and "tools/mkimagem.py" in o and "--smp" in o and "--resolucao 1280x720" in o
              and o.count("--extra-mb") == 2 and "--jogos" not in o and "--vesa" in o, f"Personalizada: {o}")
    verificar("INPUT_SHARE= " in o and "NETSURF=0" in o, "Personalizada pública compila sin lo privado")

    # ---- B: reglas
    c = g.config_nueva("personalizada")
    c["mk_sem_pae"] = True
    verificar("mk_sem_nx" in g.bloqueos(c), "NX sin PAE queda bloqueado")
    c = g.config_nueva("personalizada")
    c["mk_gpu3d"] = "radeon"
    c["mk_gpu3d_gtt"] = True
    verificar("mk_gpu3d_gtt" in g.bloqueos(c) and not g.efectivo(c)["mk_gpu3d_gtt"], "GTT sin 3D Intel bloqueado y neutralizado")
    c["mk_gpu3d"] = "intel"
    verificar("mk_gpu3d_gtt" not in g.bloqueos(c), "GTT con 3D Intel permitido")
    c = g.config_nueva("personalizada")
    c.update(mk_sem_acpi=True, mk_uacpi=True, mk_sin_swap=True, mk_swap_mb="64", mk_sesion_root=True, mk_sesion_sagui=True)
    b = g.bloqueos(c)
    verificar({"mk_uacpi", "mk_swap_mb", "mk_sesion_sagui"} <= set(b), "uACPI sin ACPI, swap sin swap, sesiones excluyentes")
    _p, _e, _a, _d = g.construir(c)
    texto = g.orden_equivalente(_p)
    verificar("--uacpi" not in texto and "--swap-mb" not in texto and "--sesion-sagui" not in texto, "lo bloqueado no sale en la orden")
    c = g.config_nueva("privada")
    verificar("g_publicar" in g.bloqueos(c) and "g_singpl" in g.bloqueos(c) and "g_variante" in g.bloqueos(c), "Privada: sin --publicar, sin sin-GPL, variante gpl")
    c = g.config_nueva("publica")
    c["g_sin_compilar"] = True
    verificar({"g_limpiar", "g_otimizacao", "g_sin_python", "g_fijar_version"} <= set(g.bloqueos(c)), "--sin-compilar bloquea lo de compilar")
    c = g.config_nueva("publica")
    verificar(all(k in g.bloqueos(c) for k in g.CLAVES_MK), "Pública: las opciones de mkimagem no valen")

    # ---- C: destino privado
    rc, o, e = orden("--perfil", "privada", "--destino", "entrega/v8.5")
    verificar(rc == 2 and "versionados" in e, "privada en entrega/v8.5 (versionada): rechazada")
    rc, o, e = orden("--perfil", "privada", "--destino", "docs/privado")
    verificar(rc == 2, "privada en docs/: rechazada")
    rc, o, e = orden("--perfil", "privada", "--destino", "entrega/privada")
    verificar(rc == 0, "privada en entrega/privada: aceptada")
    rc, o, e = orden("--perfil", "privada", "--destino", os.path.join(TMP, "fuera"))
    verificar(rc == 0, "privada fuera del repositorio: aceptada")
    rc, o, e = orden("--perfil", "privada", "--destino", "/mnt/c/Users/x/86box/roms")
    verificar(rc == 2, "destino en carpetas de 86Box: rechazado")
    rc, o, e = orden("--perfil", "personalizada", "--set", "con_privados=1", "--destino", "tools")
    verificar(rc == 2, "personalizada con datos privados en tools/: rechazada")

    # ---- D: perfiles guardados
    json.dump({"perfiles": {"mio": dict(g.config_nueva("personalizada"), mk_smp=True)}, "ultima": g.config_nueva()}, open(CFG, "w"))
    rc, o, e = orden("--perfil", "mio", "--set", "mk_jogos=", "--set", "mk_apps=", "--set", "mk_uefi=", "--set", "g_sin_compilar=1",
                     "--build", "/nada")
    verificar("--smp" in o or "faltan" in e, "perfil guardado «mio» se carga")
    rc, o, e = orden("--perfil", "mio", "--set", "mk_jogos=", "--set", "mk_apps=", "--set", "mk_uefi=")
    verificar(rc == 0 and "--smp" in o, "perfil guardado: lleva --smp")
    rc, o, e = orden("--perfil", "noexiste")
    verificar(rc != 0, "perfil desconocido: error")

    # ---- D2: cancelar mata al hijo y a sus hijos (por grupo de procesos)
    import threading
    import time
    fin = threading.Event()
    pr = g.Proceso("sleep 300 & sleep 300 & wait", lambda l: None, lambda rc, c: fin.set())
    time.sleep(0.5)
    pr.cancelar()
    verificar(fin.wait(8), "Cancelar para el proceso y sus hijos")

    # ---- E: imagen real y QEMU
    build = buscar_build()
    if not build:
        print("    (sin compilación existente: se omite la parte de QEMU)")
        return
    dest = os.path.join(TMP, "entrega")
    r = subprocess.run([sys.executable, GEN, "--perfil", "personalizada", "--ejecutar", "--build", build, "--destino", dest,
                        "--set", "g_sin_compilar=1", "--set", "mk_jogos=", "--set", "mk_apps=", "--set", "mk_smp=1",
                        "--set", "mk_resolucion=1024x768", "--set", "mk_uefi=" ],
                       capture_output=True, text=True, cwd=RAIZ, env=dict(os.environ, SAGUI_GENERADOR_CONFIG=CFG))
    print(r.stdout[-1500:], r.stderr[-800:])
    img = os.path.join(dest, "saguios-personalizada.img")
    verificar(r.returncode == 0 and os.path.exists(img), "la imagen personalizada se genera")
    sumas = open(os.path.join(dest, "SHA256SUMS.txt")).read()
    verificar(g.sha256_de(img) in sumas, "SHA256SUMS.txt coincide con la imagen")
    res = g.resumen_imagenes(dest, 0)
    verificar(res and res[0][0] == "saguios-personalizada.img" and res[0][2] in sumas, "el resumen lista la imagen con su suma")
    with Maquina("h372", "ide", imagem=img, memoria="512M", nucleos=2) as m:
        m.esperar_serial("Hola! Soy", 120)
        verificar(True, "la imagen personalizada llega al escritorio en QEMU")
    shutil.rmtree(TMP, ignore_errors=True)


if __name__ == "__main__":
    try:
        main()
    finally:
        shutil.rmtree(TMP, ignore_errors=True)
    print("OK h372")
