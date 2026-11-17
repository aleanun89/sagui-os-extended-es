#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""generador.py - generador de imágenes de SaguiOS con ventana (D370).

Herramienta del PC (no de SaguiOS): una ventana Tkinter para montar la orden de
gerar_versao.sh (perfiles «Pública» y «Privada completa») o de tools/mkimagem.py
(perfil «Personalizada»), elegir la carpeta de destino, ejecutarla con turnos y ver
el resultado. Sin ventana:

    tools/generador.py --orden --perfil privada
    tools/generador.py --orden --perfil personalizada --set mk_smp=1 --destino /tmp/x
    tools/generador.py --perfil publica --ejecutar

Ver docs/generador.md.
"""
import argparse
import copy
import hashlib
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import threading
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.environ.get("SAGUI_GENERADOR_CONFIG") or os.path.expanduser("~/.config/saguios-generador.json")
ENV_SH = os.path.expanduser("~/.local/saguitools/env.sh")

# ---------------------------------------------------------------------------
# Tabla de opciones
# ---------------------------------------------------------------------------
# Opciones de gerar_versao.sh (perfiles Pública y Privada, y compilación de la Personalizada).
# (clave, tipo, valor por defecto, etiqueta, ayuda)   tipos: bool, int, str, choice:a|b|c
OPC_GERAR = [
    ("g_todas", "bool", False, "Todas las imágenes (--todas)", "Principal, SMP, uACPI, completo, ISO, VDI, persistentes, particiones y sin GPL."),
    ("g_smp", "bool", False, "Multinúcleo (saguios-smp.img)", "--smp: todos los procesadores (D164)."),
    ("g_uacpi", "bool", False, "uACPI (batería)", "--uacpi: intérprete de AML; indicador de batería en portátiles (D192)."),
    ("g_completo", "bool", False, "Completo (saguios-completo.img)", "--completo: las aplicaciones dentro del ramdisk (D324)."),
    ("g_iso", "bool", False, "ISO de CD", "--iso (D077). La ISO no lleva la partición de juegos."),
    ("g_fat32", "bool", False, "Persistente FAT32", "--fat32: /home persistente en FAT32 (se lee desde Windows)."),
    ("g_fat32_mb", "int", "", "   tamaño FAT32 (MB)", "Por defecto 2048."),
    ("g_ext2", "bool", False, "Persistente ext2", "--ext2: /home persistente en ext2."),
    ("g_ext2_mb", "int", "", "   tamaño ext2 (MB)", "Por defecto 64."),
    ("g_particiones", "bool", False, "Disco con particiones", "--particiones: /home, /tmp y /logs en particiones (D120), comprimido."),
    ("g_singpl", "bool", False, "Variante sin GPL (saguios-sin-gpl.img)", "--sin-gpl (D109)."),
    ("g_variante", "choice:gpl|sem-gpl|sem-firmware", "gpl", "Variante de la imagen principal",
     "gpl (por defecto), sem-gpl (sin ports GPL) o sem-firmware (sin firmware)."),
    ("g_sin_python", "bool", False, "Sin MicroPython", "--sin-python: una imagen bastante más pequeña."),
    ("g_otimizacao", "choice:-O0|-O1|-O2|-O3|-Os", "-O2", "Optimización", "Nivel de optimización del compilador (por defecto -O2)."),
    ("g_jobs", "int", "", "Trabajos de make (--jobs)", "Vacío = los núcleos del PC."),
    ("g_limpiar", "bool", False, "Compilar desde cero (--limpiar)", "Borra la carpeta de compilación antes."),
    ("g_sin_compilar", "bool", False, "No compilar (--sin-compilar)", "Usa lo que ya haya en build/."),
    ("g_probar", "bool", False, "Probar al terminar (--probar)", "Arranca la imagen en QEMU y comprueba que llega al escritorio."),
    ("g_fijar_version", "str", "", "Fijar versión (--fijar-version)", "p. ej. v8.6; escribe kernel/inc/versao.h."),
    ("g_notas", "str", "", "Fichero de notas (--notas)", "Se añade al final de VERSION.txt."),
    ("g_publicar", "bool", False, "Copiar también a entrega/v<versión> (--publicar)", "Esa carpeta está en git: nunca con la entrega privada."),
    ("g_home_mb", "int", "", "HOME_MB (disco con particiones)", "Variable de entorno; por defecto 2048."),
    ("g_tmp_mb", "int", "", "TMP_MB", "Por defecto 256."),
    ("g_logs_mb", "int", "", "LOGS_MB", "Por defecto 128."),
    ("g_jogos_mb", "int", "", "JOGOS_MB (partición de juegos)", "Por defecto 800; 0 = lo justo."),
    ("g_apps_mb", "int", "", "APPS_MB (partición de aplicaciones)", "Por defecto 0 = lo justo más margen."),
    ("g_part_fs", "choice:|fat32|ext2", "", "PART_FS (sistema de las particiones)", "Vacío = el de por defecto (fat32)."),
]

# Opciones de tools/mkimagem.py (perfil Personalizada), agrupadas por pestaña.
# (clave, flag, tipo, por defecto, pestaña, etiqueta, ayuda)
# tipos: bool, int, str, path, choice:a|b|c  (choice con "" = omitir). "{build}" se sustituye por la carpeta de compilación.
OPC_MK = [
    # --- Pantalla
    ("mk_vesa", "--vesa", "bool", True, "Pantalla", "Modo gráfico VESA", "El estágio 2 cambia al modo gráfico. Es lo normal."),
    ("mk_consola", "--console", "bool", False, "Pantalla", "Solo consola (sin escritorio)", "Terminal de pantalla completa."),
    ("mk_resolucion", "--resolucao", "str", "", "Pantalla", "Resolución preferida (LxA)", "p. ej. 1280x720. Vacío = automática (1024x768)."),
    ("mk_bpp", "--bpp", "choice:|0|15|16|24|32", "", "Pantalla", "Profundidad de color", "Vacío = automática (32 > 24 > 16 > 15)."),
    ("mk_sem_wc", "--sem-wc", "bool", False, "Pantalla", "Sin write-combining en el framebuffer", "D057."),
    # --- Memoria
    ("mk_sem_pae", "--sem-pae", "bool", False, "Memoria", "Sin PAE", "Paginación clásica de 2 niveles aunque la CPU tenga PAE: hasta 4 GB, sin NX (D349)."),
    ("mk_sem_nx", "--sem-nx", "bool", False, "Memoria", "Sin NX", "No activar NX aunque la CPU lo tenga (D349)."),
    ("mk_sin_swap", "--sin-swap", "bool", False, "Memoria", "Sin swap comprimida", "Si se acaba la memoria, el programa falla (D352)."),
    ("mk_swap_mb", "--swap-mb", "int", "", "Memoria", "Tope de la swap (MB, 1-1024)", "Vacío = automático."),
    ("mk_sem_cache_disco", "--sem-cache-disco", "bool", False, "Memoria", "Sin caché de bloques", "Cada lectura y grabación va al disco (D281)."),
    ("mk_sin_escritura_diferida", "--sin-escritura-diferida", "bool", False, "Memoria", "Sin escritura diferida", "La caché graba al instante (D281)."),
    ("mk_cache_disco_baja", "--cache-disco-baja", "bool", False, "Memoria", "Caché de disco en memoria baja", "Hasta 64 MB aunque haya memoria alta (D350)."),
    # --- CPU
    ("mk_smp", "--smp", "bool", False, "CPU", "Multinúcleo (SMP)", "Enciende los procesadores secundarios (D164)."),
    ("mk_ioapic", "--ioapic", "bool", False, "CPU", "IRQ por el IOAPIC", "Sin esto, el PIC 8259 (D166)."),
    ("mk_planificador", "--planificador", "choice:|afinidad|equilibrio|prioridad|rr", "", "CPU", "Planificador", "Vacío = el de por defecto (rr)."),
    ("mk_uacpi", "--uacpi", "bool", False, "CPU", "uACPI (intérprete de AML)", "Controlador embebido e indicador de batería (D192)."),
    ("mk_sem_acpi", "--sem-acpi", "bool", False, "CPU", "Sin ACPI", "Apagar solo avisa, sin botón de energía (D113)."),
    ("mk_sin_frecuencia", "--sin-frecuencia", "bool", False, "CPU", "Sin driver de frecuencia", "La CPU como la deja la BIOS (D106)."),
    # --- Aceleración
    ("mk_sin_acel2d", "--sin-acel2d", "bool", False, "Aceleración", "2D siempre por la CPU", "Sin acelerador de la GPU (D195)."),
    ("mk_acel2d", "--acel2d", "choice:|auto|cpu|intel|mach64|nvidia|radeon|s3|tdfx|vmware", "", "Aceleración", "Acelerador 2D",
     "Vacío/auto = por el PCI (lo normal)."),
    ("mk_gpu3d", "--gpu3d", "choice:|auto|intel|no|radeon|simulado", "", "Aceleración", "3D por la GPU",
     "auto (Intel Gen3 por el PCI), no (SaguiGL por software), intel, radeon (sin probar) o simulado (QEMU)."),
    ("mk_gpu3d_anillo", "--gpu3d-anillo", "bool", False, "Aceleración", "3D Intel: órdenes por el anillo", "En vez de batch buffers."),
    ("mk_gpu3d_gtt", "--gpu3d-gtt", "bool", False, "Aceleración", "3D Intel Gen3: texturas en el GTT", "Sin probar en hardware (D358)."),
    ("mk_sgl_motor", "--sgl-motor", "choice:|auto|software", "", "Aceleración", "Motor 3D (SaguiGL)", "software desactiva toda la aceleración por hardware."),
    ("mk_acel_segura", "--acel-segura", "bool", False, "Aceleración", "Arrancar en modo seguro de la aceleración", "2D por la CPU y 3D por software (D323)."),
    # --- Disco y particiones
    ("mk_home_mb", "--dados-mb", "int", "", "Disco", "/home persistente (MB)", "Vacío = sin partición de datos."),
    ("mk_home_fs", "--dados-fs", "choice:|ext2|fat32", "", "Disco", "Sistema de /home", "fat32 se lee desde Windows."),
    ("mk_tmp_mb", "--tmp-mb", "int", "", "Disco", "/tmp en partición (MB)", "SAGUI-TMP."),
    ("mk_tmp_fs", "--tmp-fs", "choice:|ext2|fat32", "", "Disco", "Sistema de /tmp", ""),
    ("mk_logs_mb", "--logs-mb", "int", "", "Disco", "/logs en partición (MB)", "SAGUI-LOGS."),
    ("mk_logs_fs", "--logs-fs", "choice:|ext2|fat32", "", "Disco", "Sistema de /logs", ""),
    ("mk_extra_mb", "--extra-mb", "str", "", "Disco", "Particiones extra (MB, separadas por comas)", "p. ej. 128,64: SAGUI-X1, SAGUI-X2."),
    ("mk_part_fs", "--part-fs", "choice:|ext2|fat32", "", "Disco", "Sistema de las demás particiones", "Por defecto ext2."),
    ("mk_esquema", "--esquema", "choice:|logicas|primarias|mixto", "", "Disco", "Esquema de particiones", "Vacío = lógicas en una extendida (D213)."),
    ("mk_jogos", "--jogos", "path", "{build}/jogos/raiz", "Disco", "Carpeta de juegos (SAGUI-JOGOS)", "Un juego por carpeta; vacío = sin la partición. Por defecto la de build/."),
    ("mk_jogos_mb", "--jogos-mb", "int", "", "Disco", "   tamaño de juegos (MB)", "Vacío/0 = lo justo."),
    ("mk_apps", "--apps", "path", "{build}/apps/raiz", "Disco", "Carpeta de aplicaciones (SAGUI-APPS)", "bin/ y una carpeta por app; vacío = sin la partición."),
    ("mk_apps_mb", "--apps-mb", "int", "", "Disco", "   tamaño de aplicaciones (MB)", "Vacío/0 = lo justo más margen."),
    ("mk_sin_conf", "--sin-conf", "bool", False, "Disco", "Sin la partición SAGUI-CONF", "Por defecto va en todas las imágenes de disco."),
    ("mk_uefi", "--uefi", "path", "{build}/boot/uefi", "Disco", "Arranque UEFI (carpeta con BOOTX64.EFI)", "Imagen híbrida BIOS+UEFI (D264); vacío = solo BIOS."),
    ("mk_esp_mb", "--esp-mb", "int", "", "Disco", "   tamaño de la ESP (MB)", "Vacío = lo justo más 8 MB (al menos 40)."),
    # --- Sesión
    ("mk_sesion_root", "--sesion-root", "bool", False, "Sesión", "Sesión como root", "El escritorio y el Terminal como root en vez de sagui (D255)."),
    ("mk_sesion_sagui", "--sesion-sagui", "bool", False, "Sesión", "Sesión como sagui", "Lo contrario de la anterior."),
    ("mk_sin_servicios", "--sin-servicios", "bool", False, "Sesión", "Sin servicios ni cron", "No lanza el gestor de servicios al arrancar (D216)."),
    ("mk_rescate", "--rescate", "bool", False, "Sesión", "Arrancar en modo rescate", "Consola con shell de root, sin escritorio ni servicios (D255)."),
    # --- USB y varios
    ("mk_sem_dma", "--sem-dma", "bool", False, "USB y varios", "IDE por PIO (sin Bus Master)", "Para comparar o si falla el DMA (D281)."),
    ("mk_usb_zip", "--usb-zip", "bool", False, "USB y varios", "MBR como disco Zip", "Geometría 64x32 y SaguiOS en la partición 4 (D203)."),
    ("mk_usb_bpb", "--usb-bpb", "choice:|cero|valido|sin", "", "USB y varios", "Bytes 0-0x59 del MBR", "cero (por defecto), valido (BPB con la geometría) o sin (sin firma FAT)."),
    ("mk_ne2k_isa", "--ne2k-isa", "bool", False, "USB y varios", "Sondear NE2000 ISA", "Puertos 0x300/0x280/0x320/0x340 (D329)."),
    ("mk_ne2k_irq", "--ne2k-irq", "choice:|0|3|4|5|7|9|10|11|12|15", "", "USB y varios", "IRQ de la NE2000 ISA", "0 = autodetectarla."),
]
PESTANAS_MK = ["Pantalla", "Memoria", "CPU", "Aceleración", "Disco", "Sesión", "USB y varios"]

# Otras claves de la configuración (no son opciones de la orden).
EXTRAS = {
    "perfil": "publica",          # publica | privada | personalizada
    "destino": "",                # vacío = el de por defecto del perfil
    "build": "build",             # carpeta de compilación (Personalizada)
    "nombre": "saguios-personalizada.img",
    "con_privados": False,        # Personalizada: lleva input_share/datos de juegos/NetSurf
}
PERFILES = ("publica", "privada", "personalizada")
DESTINO_DEFECTO = {"publica": "entrega/latest", "privada": "entrega/privada", "personalizada": "entrega/personalizada"}
ETIQUETA_PERFIL = {"publica": "Pública", "privada": "Privada completa", "personalizada": "Personalizada"}


def defecto(clave):
    for t in OPC_GERAR:
        if t[0] == clave:
            return t[2]
    for t in OPC_MK:
        if t[0] == clave:
            return t[3]
    return EXTRAS[clave]


def config_nueva(perfil="publica"):
    cfg = {t[0]: t[2] for t in OPC_GERAR}
    cfg.update({t[0]: t[3] for t in OPC_MK})
    cfg.update(EXTRAS)
    aplicar_perfil(cfg, perfil)
    return cfg


def aplicar_perfil(cfg, perfil):
    """Pone los valores típicos del perfil (sin tocar el resto)."""
    ant = cfg.get("perfil")
    cfg["perfil"] = perfil
    if perfil == "publica":
        cfg["g_todas"] = True
        cfg["con_privados"] = False
    elif perfil == "privada":
        cfg["g_todas"] = True
        cfg["con_privados"] = True
        cfg["g_variante"] = "gpl"
        cfg["g_publicar"] = False
    else:
        cfg["g_todas"] = False
    if cfg.get("destino") in ("", *DESTINO_DEFECTO.values()):
        cfg["destino"] = DESTINO_DEFECTO[perfil]
    return ant


# ---------------------------------------------------------------------------
# Reglas de compatibilidad (tabla local; no hay otra en main)
# ---------------------------------------------------------------------------
# (clave bloqueada, condición(cfg) -> bool, motivo)
def _es_intel3d(c):
    return c["mk_gpu3d"] in ("", "auto", "intel")


REGLAS = [
    # mkimagem
    ("mk_sem_nx", lambda c: c["mk_sem_pae"], "sin PAE ya implica sin NX (D349)"),
    ("mk_swap_mb", lambda c: c["mk_sin_swap"], "no hay swap que dimensionar"),
    ("mk_sin_escritura_diferida", lambda c: c["mk_sem_cache_disco"], "sin caché de bloques no hay escritura diferida"),
    ("mk_cache_disco_baja", lambda c: c["mk_sem_cache_disco"], "sin caché de bloques no hay dónde ponerla"),
    ("mk_gpu3d_gtt", lambda c: not _es_intel3d(c), "el GTT es del 3D Intel Gen3: elige 3D auto o intel"),
    ("mk_gpu3d_anillo", lambda c: not _es_intel3d(c), "el anillo es del 3D Intel: elige 3D auto o intel"),
    ("mk_gpu3d_gtt", lambda c: c["mk_sin_acel2d"] or c["mk_acel_segura"] or c["mk_sgl_motor"] == "software",
     "con el 2D por la CPU / modo seguro / motor por software no hay 3D por la GPU"),
    ("mk_gpu3d_anillo", lambda c: c["mk_sin_acel2d"] or c["mk_acel_segura"] or c["mk_sgl_motor"] == "software",
     "con el 2D por la CPU / modo seguro / motor por software no hay 3D por la GPU"),
    ("mk_acel2d", lambda c: c["mk_sin_acel2d"], "«2D siempre por la CPU» ya fija el acelerador"),
    ("mk_uacpi", lambda c: c["mk_sem_acpi"], "uACPI necesita el ACPI"),
    ("mk_sesion_sagui", lambda c: c["mk_sesion_root"], "ya has elegido la sesión de root"),
    ("mk_sesion_root", lambda c: c["mk_rescate"] or c["mk_consola"], "el rescate/la consola ya van sin escritorio"),
    ("mk_sesion_sagui", lambda c: c["mk_rescate"] or c["mk_consola"], "el rescate/la consola ya van sin escritorio"),
    ("mk_sin_servicios", lambda c: c["mk_rescate"], "el rescate ya no lanza servicios"),
    ("mk_ne2k_irq", lambda c: not c["mk_ne2k_isa"], "solo vale con «Sondear NE2000 ISA»"),
    ("mk_esp_mb", lambda c: not c["mk_uefi"], "sin arranque UEFI no hay ESP"),
    ("mk_jogos_mb", lambda c: not c["mk_jogos"], "sin carpeta de juegos"),
    ("mk_apps_mb", lambda c: not c["mk_apps"], "sin carpeta de aplicaciones"),
    ("mk_home_fs", lambda c: not c["mk_home_mb"], "sin partición /home"),
    ("mk_tmp_fs", lambda c: not c["mk_tmp_mb"], "sin partición /tmp"),
    ("mk_logs_fs", lambda c: not c["mk_logs_mb"], "sin partición /logs"),
    ("mk_planificador", lambda c: False, ""),
    # gerar_versao.sh
    ("g_fat32_mb", lambda c: not c["g_fat32"] and not c["g_todas"], "elige la imagen persistente FAT32"),
    ("g_ext2_mb", lambda c: not c["g_ext2"] and not c["g_todas"], "elige la imagen persistente ext2"),
    ("g_sin_python", lambda c: c["g_variante"] != "gpl", "no se combina con una variante distinta de gpl"),
    ("g_variante", lambda c: c["g_sin_python"], "no se combina con --sin-python"),
    ("g_variante", lambda c: c["perfil"] == "privada" or c["con_privados"], "lo privado lleva NetSurf (GPL): solo variante gpl"),
    ("g_singpl", lambda c: c["perfil"] == "privada" or c["con_privados"], "la entrega privada no lleva la variante sin GPL"),
    ("g_limpiar", lambda c: c["g_sin_compilar"], "no se compila"),
    ("g_fijar_version", lambda c: c["g_sin_compilar"], "no se compila"),
    ("g_otimizacao", lambda c: c["g_sin_compilar"], "no se compila"),
    ("g_sin_python", lambda c: c["g_sin_compilar"], "no se compila"),
    ("g_variante", lambda c: c["g_sin_compilar"], "no se compila"),
    ("g_publicar", lambda c: es_sensible(c), "entrega/v<versión> está en git: lo privado no se publica nunca"),
    ("g_publicar", lambda c: c["perfil"] == "personalizada", "solo para las entregas de gerar_versao.sh"),
    ("con_privados", lambda c: c["perfil"] != "personalizada", "lo fija el perfil"),
    ("nombre", lambda c: c["perfil"] != "personalizada", "solo en la imagen personalizada"),
]
# Las pestañas de mkimagem solo valen en la Personalizada.
CLAVES_MK = {t[0] for t in OPC_MK}
CLAVES_G = {t[0] for t in OPC_GERAR}
CLAVES_G_PERSONALIZADA = {"g_jobs", "g_limpiar", "g_sin_compilar", "g_sin_python", "g_otimizacao"}


def bloqueos(cfg):
    """Devuelve {clave: motivo} de las opciones que no se pueden usar con la configuración actual."""
    res = {}
    c = copy.deepcopy(cfg)
    for _ in range(3):   # un bloqueo puede liberar otro: se aplica hasta estabilizar
        res = {}
        for clave, cond, motivo in REGLAS:
            if motivo and cond(c) and clave not in res:
                res[clave] = motivo
        for clave in res:
            c[clave] = defecto(clave)
    if cfg["perfil"] != "personalizada":
        for k in CLAVES_MK:
            res[k] = "solo vale en el perfil Personalizada"
        for k in CLAVES_G:
            pass
    else:
        for k in CLAVES_G - CLAVES_G_PERSONALIZADA:
            res.setdefault(k, "solo vale en las entregas de gerar_versao.sh (Pública/Privada)")
    return res


def efectivo(cfg):
    """La configuración con las opciones bloqueadas puestas a su valor por defecto."""
    c = copy.deepcopy(cfg)
    for _ in range(3):
        for clave in bloqueos(c):
            c[clave] = defecto(clave)
    return c


# ---------------------------------------------------------------------------
# Seguridad
# ---------------------------------------------------------------------------
def es_sensible(cfg):
    """¿La entrega lleva input_share/datos de juegos/NetSurf (uso propio, NO publicar)?"""
    if cfg["perfil"] == "privada":
        return True
    if cfg["perfil"] == "personalizada":
        if cfg.get("con_privados"):
            return True
        for k in ("mk_jogos", "mk_apps"):
            if "input_share" in str(cfg.get(k, "")):
                return True
    return False


def ruta_destino(cfg):
    d = cfg.get("destino") or DESTINO_DEFECTO[cfg["perfil"]]
    d = os.path.expanduser(d)
    if not os.path.isabs(d):
        d = os.path.join(RAIZ, d)
    return os.path.normpath(d)


def _dentro(ruta, base):
    ruta, base = os.path.realpath(ruta), os.path.realpath(base)
    return ruta == base or ruta.startswith(base + os.sep)


def ignorado_por_git(ruta):
    """True si git ignora esa carpeta (comprobando un fichero dentro de ella)."""
    rel = os.path.relpath(os.path.realpath(ruta), os.path.realpath(RAIZ))
    r = subprocess.run(["git", "check-ignore", "-q", "--", os.path.join(rel, ".generador-prueba")],
                       cwd=RAIZ, capture_output=True)
    return r.returncode == 0


def versionada(ruta):
    """True si la carpeta es de una entrega congelada (entrega/v*) o ya tiene ficheros versionados en git."""
    rel = os.path.relpath(os.path.realpath(ruta), os.path.realpath(RAIZ))
    partes = rel.split(os.sep)
    if len(partes) >= 2 and partes[0] == "entrega" and partes[1].startswith("v"):
        return True
    r = subprocess.run(["git", "ls-files", "--", rel], cwd=RAIZ, capture_output=True, text=True)
    return bool(r.stdout.strip())


def validar_destino(cfg):
    """Lista de errores del destino (vacía si vale)."""
    errores = []
    d = ruta_destino(cfg)
    if os.path.exists(d) and not os.path.isdir(d):
        errores.append(f"el destino {d} existe y no es una carpeta")
    if "86box" in d.lower():
        errores.append("el destino no puede estar en las carpetas de 86Box (ROM prohibidas)")
    if _dentro(d, RAIZ):
        if os.path.realpath(d) == os.path.realpath(RAIZ):
            errores.append("el destino no puede ser la raíz del repositorio")
        elif es_sensible(cfg) and (not ignorado_por_git(d) or versionada(d)):
            errores.append("la entrega privada no puede ir a una carpeta del repositorio que git no ignore o que ya tenga ficheros versionados "
                           "(usa entrega/privada, entrega/latest o una carpeta fuera del repositorio)")
        else:
            rel = os.path.relpath(os.path.realpath(d), os.path.realpath(RAIZ)).split(os.sep)
            if rel[0] in ("userland", "kernel", "docs", "tools", "tests", "boot", "rootfs") and es_sensible(cfg):
                errores.append("el destino es una carpeta del código fuente")
    return errores


# ---------------------------------------------------------------------------
# Construcción de la orden
# ---------------------------------------------------------------------------
def _num(v):
    v = str(v).strip()
    return v if v.isdigit() else ""


def _resolver(cfg, valor):
    return str(valor).replace("{build}", cfg["build"] or "build")


def opciones_gerar(c, destino):
    """Argumentos de gerar_versao.sh y variables de entorno."""
    a = ["--destino", destino]
    if c["g_todas"]:
        a.append("--todas")
    for clave, flag in (("g_smp", "--smp"), ("g_uacpi", "--uacpi"), ("g_completo", "--completo"), ("g_iso", "--iso"),
                        ("g_particiones", "--particiones"), ("g_singpl", "--sin-gpl")):
        if c[clave] and not c["g_todas"]:
            a.append(flag)
    for clave, flag in (("g_fat32", "--fat32"), ("g_ext2", "--ext2")):
        mb = _num(c[clave + "_mb"])
        if (c[clave] and not c["g_todas"]) or (c["g_todas"] and mb):
            a += [flag] + ([mb] if mb else [])
    if c["g_variante"] != "gpl":
        a += ["--variante", c["g_variante"]]
    if c["g_sin_python"]:
        a.append("--sin-python")
    if c["g_otimizacao"] != "-O2":
        a += ["--otimizacao", c["g_otimizacao"]]
    if _num(c["g_jobs"]):
        a += ["--jobs", _num(c["g_jobs"])]
    for clave, flag in (("g_limpiar", "--limpiar"), ("g_sin_compilar", "--sin-compilar"), ("g_probar", "--probar"),
                        ("g_publicar", "--publicar")):
        if c[clave]:
            a.append(flag)
    if c["g_fijar_version"].strip():
        a += ["--fijar-version", c["g_fijar_version"].strip()]
    if c["g_notas"].strip():
        a += ["--notas", c["g_notas"].strip()]
    if c["perfil"] == "privada":
        a.append("--con-input-share")
    env = []
    for clave, var in (("g_home_mb", "HOME_MB"), ("g_tmp_mb", "TMP_MB"), ("g_logs_mb", "LOGS_MB"),
                       ("g_jogos_mb", "JOGOS_MB"), ("g_apps_mb", "APPS_MB")):
        if _num(c[clave]):
            env.append(f"{var}={_num(c[clave])}")
    if c["g_part_fs"]:
        env.append(f"PART_FS={c['g_part_fs']}")
    return a, env


def opciones_mkimagem(c, avisos):
    """Argumentos de mkimagem.py según OPC_MK (solo lo que difiere de hoy)."""
    a = []
    for clave, flag, tipo, dfl, _p, _l, _h in OPC_MK:
        v = c[clave]
        if tipo == "bool":
            if v:
                a.append(flag)
        elif tipo == "int":
            if _num(v):
                a += [flag, _num(v)]
        elif tipo == "path":
            v = _resolver(c, v).strip()
            if not v:
                continue
            p = v if os.path.isabs(v) else os.path.join(RAIZ, v)
            if not os.path.isdir(p):
                if v == _resolver(c, dfl):
                    avisos.append(f"{flag}: no existe {v}; se omite (compila antes o pon la carpeta)")
                    continue
                avisos.append(f"{flag}: no existe la carpeta {v}")
            a += [flag, v]
        elif tipo == "str":
            if v.strip():
                if flag == "--extra-mb":
                    for n in v.split(","):
                        if n.strip().isdigit():
                            a += [flag, n.strip()]
                else:
                    a += [flag, v.strip()]
        else:
            if v:
                a += [flag, v]
    return a


def construir(cfg):
    """Devuelve (pasos, errores, avisos, destino). pasos = lista de (título, [palabras de la orden])."""
    errores, avisos = [], []
    c = efectivo(cfg)
    destino = ruta_destino(c)
    errores += validar_destino(c)
    if c["perfil"] not in PERFILES:
        errores.append(f"perfil desconocido: {c['perfil']}")
        return [], errores, avisos, destino
    sensible = es_sensible(c)
    pasos = []
    if c["perfil"] in ("publica", "privada"):
        if c["perfil"] == "privada" and not c["g_sin_compilar"]:
            pasos.append(("Descargas de lo privado (NetSurf, GNUBoy, datos de Quake y OpenJazz)", ["make", "priv_descargas"]))
        a, env = opciones_gerar(c, destino)
        pasos.append(("Generar la entrega", env + ["./gerar_versao.sh"] + a))
        if c["perfil"] == "privada" and not os.path.isdir(os.path.join(RAIZ, "input_share")):
            avisos.append("no hay input_share/: los juegos propios no entrarán (ver docs/input_share.md)")
    else:
        build = c["build"] or "build"
        bpath = build if os.path.isabs(build) else os.path.join(RAIZ, build)
        if not c["g_sin_compilar"]:
            m = ["make"]
            if build != "build":
                m.append(f"B={build}")
            if _num(c["g_jobs"]):
                m.append(f"-j{_num(c['g_jobs'])}")
            if c["g_sin_python"]:
                m.append("MICROPYTHON=0")
            if c["g_otimizacao"] != "-O2":
                m.append(f"OTIMIZACAO={c['g_otimizacao']}")
            if c["g_limpiar"]:
                pasos.append(("Limpiar", ["make"] + ([f"B={build}"] if build != "build" else []) + ["limpar"]))
            if sensible:
                pasos.append(("Descargas de lo privado", ["make", "priv_descargas"]))
            else:
                m += ["INPUT_SHARE=", "OPENJAZZ_DATOS=0", "QUAKE_DATOS=0", "NETSURF=0", "GNUBOY=0", "PUBLICABLE=1"]
            pasos.append(("Compilar", m + ["all"]))
        else:
            faltan = [f for f in ("kernel.bin", "kernel.elf", "ramdisk.sgfs", "boot/mbr.bin", "boot/estagio2.bin")
                      if not os.path.exists(os.path.join(bpath, f))]
            if faltan:
                errores.append(f"con «No compilar» hacen falta los ficheros de {build}/: faltan {', '.join(faltan)}")
        nombre = os.path.basename(c["nombre"].strip() or "saguios-personalizada.img")
        if not re.match(r"^[\w.+-]+$", nombre):
            errores.append(f"nombre de imagen no válido: {nombre}")
        salida = os.path.join(destino, nombre)
        mk = ["python3", "tools/mkimagem.py", "--mbr", f"{build}/boot/mbr.bin", "--estagio2", f"{build}/boot/estagio2.bin",
              "--kernel", f"{build}/kernel.bin", "--kernel-elf", f"{build}/kernel.elf", "--ramdisk", f"{build}/ramdisk.sgfs",
              "--saida", salida] + opciones_mkimagem(c, avisos)
        pasos.append(("Crear la carpeta de destino", ["mkdir", "-p", destino]))
        pasos.append(("Crear la imagen", mk))
        pasos.append(("Suma SHA-256", ["bash", "-c", f"cd {shlex.quote(destino)} && sha256sum {shlex.quote(nombre)} | tee SHA256SUMS.txt"]))
        if c["con_privados"] and c["g_sin_compilar"]:
            avisos.append("«No compilar» con datos privados: se usa lo que ya haya en la carpeta de compilación")
    if sensible:
        avisos.insert(0, "USO PROPIO: NO PUBLICAR (lleva input_share, datos de juegos y/o NetSurf; D228, D258)")
    if any("86box" in w.lower() and "roms" in w.lower() for _t, p in pasos for w in p):
        errores.append("nunca se copian ROM de 86Box")
    return pasos, errores, avisos, destino


def a_texto(palabras):
    return " ".join(w if re.match(r"^[\w@%+=:,./-]+$", w) else shlex.quote(w) for w in palabras)


def orden_equivalente(pasos):
    return "\n".join(a_texto(p) for _t, p in pasos)


def guion_ejecucion(pasos):
    """Guion bash que se ejecuta bajo turno.sh: cada paso precedido de un marcador de fase."""
    n = len(pasos)
    lineas = ["set -e"]
    for i, (t, p) in enumerate(pasos, 1):
        lineas.append(f"echo '== FASE {i}/{n}: {t} =='")
        lineas.append(a_texto(p))
    lineas.append("echo '== FIN =='")
    return "\n".join(lineas)


def orden_shell(pasos):
    """Orden completa, tal como se ejecuta: entorno local + turno.sh."""
    pre = f". {shlex.quote(ENV_SH)} && " if os.path.exists(ENV_SH) else ""
    return pre + "SAGUI_TURNOS=6 tools/turno.sh bash -c " + shlex.quote(guion_ejecucion(pasos))


# ---------------------------------------------------------------------------
# Configuración y perfiles guardados
# ---------------------------------------------------------------------------
def cargar_estado():
    try:
        with open(CONFIG, encoding="utf-8") as f:
            e = json.load(f)
        if isinstance(e, dict):
            return e
    except (OSError, ValueError):
        pass
    return {}


def guardar_estado(e):
    try:
        os.makedirs(os.path.dirname(CONFIG), exist_ok=True)
        with open(CONFIG, "w", encoding="utf-8") as f:
            json.dump(e, f, indent=1, ensure_ascii=False)
    except OSError:
        pass


def fusionar(base, otra):
    """Copia las claves conocidas de «otra» sobre «base» (ignora claves viejas o desconocidas)."""
    for k in base:
        if k in otra and type(otra[k]) is type(base[k]):
            base[k] = otra[k]
        elif k in otra and isinstance(base[k], str) and isinstance(otra[k], (int, float)):
            base[k] = str(otra[k])
    return base


# ---------------------------------------------------------------------------
# Ejecución
# ---------------------------------------------------------------------------
def sha256_de(ruta):
    h = hashlib.sha256()
    with open(ruta, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def resumen_imagenes(destino, desde):
    """Imágenes nuevas (modificadas desde «desde») en el destino: [(nombre, bytes, sha256)]."""
    sumas = {}
    f = os.path.join(destino, "SHA256SUMS.txt")
    if os.path.exists(f):
        for l in open(f, encoding="utf-8", errors="replace"):
            p = l.split()
            if len(p) == 2:
                sumas[p[1].lstrip("*")] = p[0]
    res = []
    if not os.path.isdir(destino):
        return res
    for n in sorted(os.listdir(destino)):
        p = os.path.join(destino, n)
        if not (n.startswith("saguios") and os.path.isfile(p)) or os.path.getmtime(p) < desde - 2:
            continue
        res.append((n, os.path.getsize(p), sumas.get(n) or sha256_de(p)))
    return res


def tam_humano(n):
    for u in ("B", "KB", "MB", "GB"):
        if n < 1024 or u == "GB":
            return f"{n:.0f} {u}" if u == "B" else f"{n / 1:.1f} {u}"
        n /= 1024


class Proceso:
    """Un proceso hijo en su propio grupo, para poder cancelarlo con todos sus hijos."""

    def __init__(self, orden, al_linea, al_fin):
        self.p = subprocess.Popen(["bash", "-c", orden], cwd=RAIZ, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  start_new_session=True, text=True, encoding="utf-8", errors="replace")
        self.cancelado = False
        self.al_linea, self.al_fin = al_linea, al_fin
        threading.Thread(target=self._leer, daemon=True).start()

    def _leer(self):
        for l in self.p.stdout:
            self.al_linea(l.rstrip("\n"))
        self.p.wait()
        self.al_fin(self.p.returncode, self.cancelado)

    def cancelar(self):
        self.cancelado = True
        try:
            os.killpg(self.p.pid, signal.SIGTERM)
        except OSError:
            return

        def rematar():
            time.sleep(5)
            try:
                os.killpg(self.p.pid, signal.SIGKILL)
            except OSError:
                pass
        threading.Thread(target=rematar, daemon=True).start()


def lanzar_qemu(imagen):
    """run_in_qemu.sh con esa imagen, en segundo plano y sin bloquear."""
    pre = f". {shlex.quote(ENV_SH)} && " if os.path.exists(ENV_SH) else ""
    return subprocess.Popen(["bash", "-c", pre + f"./run_in_qemu.sh --imagen {shlex.quote(imagen)}"], cwd=RAIZ,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)


def abrir_carpeta(ruta):
    """Abre la carpeta en el explorador (explorer.exe con wslpath -w en WSL; si no, xdg-open)."""
    try:
        if os.path.exists("/proc/sys/fs/binfmt_misc/WSLInterop") or "microsoft" in os.uname().release.lower():
            w = subprocess.run(["wslpath", "-w", ruta], capture_output=True, text=True).stdout.strip()
            if w:
                subprocess.Popen(["explorer.exe", w])
                return True
        subprocess.Popen(["xdg-open", ruta])
        return True
    except OSError:
        return False


FASES = [(r"Descargas|priv_descargas", 3), (r"Borrando", 5), (r"Compilando", 8), (r"vdi|\.vdi", 55), (r"\.iso|mkiso", 62),
         (r"persistente", 72), (r"particoes|particiones", 82), (r"sin GPL|sem-gpl", 88),
         (r"VERSION\.txt|SHA256|Publicando", 95), (r"Arrancando la imagen", 97)]


def progreso_de(linea, actual, total_fases=None):
    m = re.match(r"== FASE (\d+)/(\d+)", linea)
    if m:
        i, n = int(m.group(1)), int(m.group(2))
        return max(actual, int(100 * (i - 1) / n))
    if linea.startswith("== FIN"):
        return 100
    for pat, v in FASES:
        if re.search(pat, linea) and v > actual:
            return v
    return actual


# ---------------------------------------------------------------------------
# Modo sin ventana
# ---------------------------------------------------------------------------
def cfg_desde_args(a):
    est = cargar_estado()
    perfil = a.perfil
    if perfil in PERFILES:
        cfg = config_nueva(perfil)
    elif perfil and perfil in est.get("perfiles", {}):
        cfg = fusionar(config_nueva(est["perfiles"][perfil].get("perfil", "publica")), est["perfiles"][perfil])
    elif perfil:
        sys.exit(f"generador: perfil desconocido: {perfil} (publica, privada, personalizada o uno guardado)")
    else:
        cfg = fusionar(config_nueva(), est.get("ultima", {}))
    for kv in a.set or []:
        k, _, v = kv.partition("=")
        if k not in cfg:
            sys.exit(f"generador: clave desconocida: {k}")
        if isinstance(cfg[k], bool):
            v = v.lower() in ("1", "true", "si", "sí", "yes", "on")
        cfg[k] = v
    if a.destino:
        cfg["destino"] = a.destino
    if a.build:
        cfg["build"] = a.build
    if a.nombre:
        cfg["nombre"] = a.nombre
    return cfg


def principal_cli(a):
    cfg = cfg_desde_args(a)
    pasos, errores, avisos, destino = construir(cfg)
    for e in errores:
        print("ERROR:", e, file=sys.stderr)
    for v in avisos:
        print("AVISO:", v, file=sys.stderr)
    if errores:
        return 2
    if a.orden:
        print(orden_equivalente(pasos))
        if not a.ejecutar:
            return 0
    desde = time.time()
    fin = threading.Event()
    res = {}

    def linea(l):
        print(l, flush=True)

    def terminado(rc, canc):
        res["rc"] = rc
        fin.set()
    Proceso(orden_shell(pasos), linea, terminado)
    fin.wait()
    for n, tam, suma in resumen_imagenes(destino, desde):
        print(f"{n}  {tam_humano(tam)}  {suma}")
    return res["rc"]


def main():
    ap = argparse.ArgumentParser(description="Generador de imágenes de SaguiOS (ventana Tkinter; sin ventana con --orden/--ejecutar).")
    ap.add_argument("--perfil", help="publica, privada, personalizada o el nombre de un perfil guardado")
    ap.add_argument("--orden", action="store_true", help="imprime la orden equivalente y sale (sin ventana)")
    ap.add_argument("--ejecutar", action="store_true", help="ejecuta sin ventana")
    ap.add_argument("--set", action="append", metavar="CLAVE=VALOR", help="cambia una opción (g_*, mk_*, perfil...)")
    ap.add_argument("--destino", help="carpeta de destino")
    ap.add_argument("--build", help="carpeta de compilación (Personalizada; por defecto build)")
    ap.add_argument("--nombre", help="nombre del fichero de la imagen personalizada")
    ap.add_argument("--claves", action="store_true", help="lista las claves de --set y sale")
    a = ap.parse_args()
    if a.claves:
        for k in config_nueva():
            print(k)
        return 0
    if a.orden or a.ejecutar:
        return principal_cli(a)
    from generador_gui import abrir
    return abrir(a)


if __name__ == "__main__":
    sys.exit(main())
