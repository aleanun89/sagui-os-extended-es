#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
mkimagem.py - monta la imagen de disco de SaguiOS (build/saguios.img).

Distribución (PRD 4.1):
  LBA 0        MBR (etapa 1) + tabla de particiones
  LBA 1        cabecera de arranque (formato en boot/cabecalho.inc)
  LBA 2...     etapa 2
  a continuación kernel (binario plano)
  a continuación ramdisk (SGFS)
  opcional     hasta 3 particiones de datos (--home-mb/--tmp-mb/--logs-mb; antes --dados-mb),
               alineadas en 1 MB: SaguiOS las monta en /home, /tmp y /logs (D040, D120)
  opcional     la partición de juegos (--jogos PASTA, D181, D228): ext2 de lectura y
               escritura con etiqueta SAGUI-JOGOS; el /etc/fstab la monta en /jogos
  opcional     la partición de aplicaciones (--apps PASTA, D194): ext2 de lectura y
               escritura con etiqueta SAGUI-APPS; el /etc/fstab la monta en /apps
  opcional     la raíz del sistema en disco (--raiz PASTA, D379, prototipo): ext2 SAGUI-RAIZ con
               todo el sistema; el ramdisk es entonces un initramfs pequeño (tools/mkinitramfs.py)
               cuyo /etc/raiz.conf dice al kernel que monte esta partición en "/"
  por defecto  la partición de configuración SAGUI-CONF (D256, mejora 098): ext2 de 16 MB con
               un LEEME, la última de todas (--sin-conf la quita; la ISO no la lleva)

La meta de tamaño (META_BYTES, D181) cuenta los bytes útiles de la imagen del
sistema: el área de arranque (MBR, cabecera, etapa 2, kernel y ramdisk) más las
particiones del sistema (la de juegos y la de aplicaciones). Las de datos del usuario (/home,
/tmp, /logs y extras) no cuentan: su tamaño lo elige quien hace la imagen.

Con --cd la imagen es la que va dentro de la ISO (tools/mkiso.py, D077): la
cabecera lleva OPC_CD y el archivo se completa hasta un cilindro entero de
255 cabezas x 63 sectores, porque la BIOS, en la emulación de disco duro de El
Torito, toma la geometría del CHS final de la partición 1.

Todos los LBA y direcciones se calculan aquí y se graban en la cabecera.
La dirección del ramdisk en memoria sale del símbolo _kernel_fim del ELF del kernel
(fin de la BSS, alineado a 4 KB): así la BSS puesta a cero por el kernel nunca borra el
ramdisk.
"""
META_BYTES = 38 * 1024 * 1024  # objetivo de tamaño de la imagen (D181; antes 16 MB, D122; 8 MB, D111; 6 MB, D050; 1,44 MB, un disquete)
META_MB = META_BYTES // (1024 * 1024)
ROTULO_JOGOS = "SAGUI-JOGOS"   # la partición de juegos (D181); el /etc/fstab la monta por esta etiqueta
ROTULO_APPS = "SAGUI-APPS"     # la partición de aplicaciones (D194); el /etc/fstab la monta en /apps
# UUID y semilla fijos de cada partición del sistema: dos compilaciones dan la misma imagen.
MODOS_RAIZ = {"auto": 0, "ram": 1, "disco": 2, "live": 3, "toram": 4}   # CAB_RAIZ_OPC (D379)
ROTULO_PERSIST = "SAGUI-PERSIST"   # D379 (L2): la capa persistente de la live
ROTULO_RAIZ = "SAGUI-RAIZ"     # D379: la raíz del sistema en disco (prototipo); el initramfs la monta en "/"
ROTULO_CONF = "SAGUI-CONF"     # la configuración del sistema (mejora 098, D256): usuarios, claves, red; 16 MB ext2
CONF_MB = 16
UUIDS_SISTEMA = {ROTULO_CONF: ("5a6a0005-5a61-434f-8000-434f4e460001", "5a6a0005-5a61-434f-8000-434f4e460002"),
                 ROTULO_JOGOS: ("5a6a0005-5a61-4a4f-8000-4a4f474f5301", "5a6a0005-5a61-4a4f-8000-4a4f474f5302"),
                 ROTULO_APPS: ("5a6a0005-5a61-4150-8000-415050530001", "5a6a0005-5a61-4150-8000-415050530002"),
                 ROTULO_RAIZ: ("5a6a0005-5a61-5241-8000-5241495a0001", "5a6a0005-5a61-5241-8000-5241495a0002"),
                 ROTULO_PERSIST: ("5a6a0005-5a61-5045-8000-504552530001", "5a6a0005-5a61-5045-8000-504552530002")}
import argparse
import os
import re
import shutil
import struct
import tempfile
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mksgfs import validar_sgfs, EXT_TEXTO   # la misma validación del ramdisk (D147)

SETOR = 512
KERNEL_ENDERECO = 0x100000
OPC_VESA = 1
OPC_SEM_WC = 4   # boot/cabecalho.inc
OPC_CD = 8       # imagen para la ISO: el disco de arranque es solo de lectura (D077)
OPC_SEM_ACPI = 16   # no usar el ACPI: ni modo ACPI ni botón de encendido; apagar solo avisa (D113)
OPC_SMP = 64            # encender los procesadores secundarios; sin esto, uno solo (D164)
OPC_IOAPIC = 128        # las IRQ por el IOAPIC; sin esto, el PIC 8259 (D166)
OPC_TURNOS = 256        # el cerrojo general por turnos también bajo un hipervisor (D169)
OPC_UACPI = 4096        # arrancar el intérprete de AML (uACPI): EC y batería; sin esto, ni AML ni batería (D192)
OPC_SEM_SERVICOS = 0x2000   # no lanzar el gestor de servicios /bin/servicios (D216)
OPC_RESCATE = 0x4000        # modo rescate: consola con un shell de root, sin servicios (D255)
OPC_SESION_RAIZ = 0x8000    # imagen de prueba: el escritorio y el Terminal como root (D255)
# El planificador al arrancar (D174): bits 9-11, con el número de abi.h (PLANIFICADOR_*).
OPC_PLANIFICADOR_DESL = 9
PLANIFICADORES = {"rr": 1, "afinidad": 2, "equilibrio": 3, "prioridad": 4}
OPC_SGL_SOFTWARE = 32   # solo el motor por software: la GPU no acelera nada (SGL_MOTOR=software, D158)
# D211: el byte CAB_IRQ_PRUEBA (kernel/inc/irq_rotas.h, IRQ_PRUEBA_*).
IRQ_PRUEBA = {"prt-errada": 1, "isa-errada": 2, "tormenta": 4}


def irq_prueba_byte(texto):
    v = 0
    for parte in filter(None, (p.strip() for p in texto.split(","))):
        if parte not in IRQ_PRUEBA:
            sys.exit(f"mkimagem: --irq-prueba desconocido: {parte} (opciones: {', '.join(IRQ_PRUEBA)})")
        v |= IRQ_PRUEBA[parte]
    return v


# D227: el byte CAB_USB_PRUEBA (kernel/inc/usb.h, USB_PRUEBA_*).
USB_PRUEBA = {"primer-intento": 1, "siempre": 2}


def usb_prueba_byte(texto):
    v = 0
    for parte in filter(None, (p.strip() for p in texto.split(","))):
        if parte not in USB_PRUEBA:
            sys.exit(f"mkimagem: --usb-prueba desconocido: {parte} (opciones: {', '.join(USB_PRUEBA)})")
        v |= USB_PRUEBA[parte]
    return v


# D195: el byte CAB_ACEL2D de la cabecera (kernel/inc/acel2d.h).
ACEL2D_MODOS = {"auto": 0, "cpu": 1, "vmware": 2, "intel": 3, "radeon": 4, "nvidia": 6, "tdfx": 7,
                "mach64": 8, "s3": 9}
ACEL2D_PRUEBA = {"": 0, "pixel": 0x10, "tiempo": 0x20}


def acel2d_byte(a):
    modo = ACEL2D_MODOS["cpu"] if a.sin_acel2d else ACEL2D_MODOS[a.acel2d]
    return modo | ACEL2D_PRUEBA[a.acel2d_prueba] | (0x80 if a.acel_segura else 0)   # 0x80: CAB_ACEL_SEGURO (D323)


# D285: el byte CAB_GPU3D de la cabecera (kernel/inc/gpu3d.h).
GPU3D_MODOS = {"auto": 0, "no": 1, "simulado": 2, "intel": 3, "radeon": 4}
GPU3D_PRUEBA = {"": 0, "pixel": 0x10, "tiempo": 0x20}


def gpu3d_byte(a):
    return (GPU3D_MODOS[a.gpu3d] | (0x08 if a.gpu3d_anillo else 0) | GPU3D_PRUEBA[a.gpu3d_prueba]
            | (0x40 if a.gpu3d_gtt else 0))   # 0x40: GPU3D_CAB_GTT (D358)


TIPO_PARTICAO = 0xDA  # "datos sin sistema de archivos": ningún sistema intenta montar
GEOM_CABECAS, GEOM_SETORES = 255, 63
# D203: disposición del MBR. Por defecto, como un disco duro (USB-HDD): la partición
# de SaguiOS en la entrada 1 y las de datos en la 2-4. Con --usb-zip, como un disco
# Zip (USB-ZIP, lo único que arrancan algunas BIOS de 2005-2008): geometría 64x32 y
# la de SaguiOS en la entrada 4; las de datos pasan a la 1-3 (se montan por etiqueta).
# D213 (v7.3): con el esquema por defecto solo hay otra entrada, la extendida con todas las
# de datos y del sistema como lógicas: la 2 (USB-HDD) o la 1 (USB-ZIP).
SLOT_SAGUIOS, SLOT_DADOS = 0x1BE, 0x1CE
ALINHAMENTO_ARQUIVO = 64 * SETOR  # archivo final múltiplo de 32 KB...
TAMANHO_MINIMO = 1024 * 1024      # ...y con al menos 1 MB (ver D004)


def setores(n):
    return (n + SETOR - 1) // SETOR


def preencher_setores(dados):
    return dados + b"\0" * ((-len(dados)) % SETOR)


def simbolos(elf):
    saida = subprocess.run(["nm", elf], check=True, capture_output=True, text=True).stdout
    tab = {}
    for linha in saida.splitlines():
        partes = linha.split()
        if len(partes) == 3:
            tab[partes[2]] = int(partes[0], 16)
    return tab


def chs(lba):
    """Convierte LBA al CHS de 3 bytes de la tabla de particiones (255 cabezas, 63 sectores)."""
    c = lba // (GEOM_CABECAS * GEOM_SETORES)
    h = (lba // GEOM_SETORES) % GEOM_CABECAS
    s = lba % GEOM_SETORES + 1
    if c > 1023:
        c, h, s = 1023, 254, 63
    return bytes([h, s | ((c >> 2) & 0xC0), c & 0xFF])


def soma(dados):
    return sum(dados) & 0xFFFFFFFF


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mbr", required=True)
    ap.add_argument("--estagio2", required=True)
    ap.add_argument("--kernel", required=True)
    ap.add_argument("--kernel-elf", required=True)
    ap.add_argument("--ramdisk", required=True)
    ap.add_argument("--saida", required=True)
    ap.add_argument("--vesa", action="store_true", help="estágio 2 troca para o modo gráfico VESA")
    ap.add_argument("--console", action="store_true", help="sem área de trabalho: terminal de tela cheia")
    ap.add_argument("--teste", default="", help="nome de um autoteste do kernel (só para os testes automáticos)")
    ap.add_argument("--resolucao", default="", help="resolução preferida LxA (D058); vazio = automática (1024x768)")
    ap.add_argument("--sem-wc", action="store_true", help="não ligar write-combining no framebuffer (D057)")
    ap.add_argument("--sem-pae", action="store_true",
                    help="paginación clásica de 2 niveles aunque la CPU tenga PAE (D349): hasta 4 GB, sin NX")
    ap.add_argument("--sem-nx", action="store_true", help="no activar NX aunque la CPU lo tenga (D349)")
    ap.add_argument("--sin-swap", action="store_true",
                    help="D352: sin swap comprimida en RAM (cuando se acaba la memoria, el programa falla como antes)")
    ap.add_argument("--swap-mb", type=int, default=0,
                    help="D352: tope del almacén de la swap comprimida en MB (1-1024); 0 = automático "
                         "(la mitad de la RAM con memoria alta, un cuarto sin ella)")
    ap.add_argument("--bpp", type=int, default=0, choices=(0, 15, 16, 24, 32),
                    help="profundidade de cor preferida (D078); 0 = automática (32 > 24 > 16 > 15)")
    ap.add_argument("--smp", action="store_true",
                    help="encender los procesadores secundarios (D164); sin esta opción el sistema usa uno solo")
    ap.add_argument("--ioapic", action="store_true",
                    help="las IRQ de los dispositivos por el IOAPIC (D166); sin esta opción, el PIC 8259")
    ap.add_argument("--trava-turnos", action="store_true",
                    help="el cerrojo general del kernel por turnos también bajo un hipervisor (D169, para pruebas)")
    ap.add_argument("--planificador", choices=sorted(PLANIFICADORES),
                    help="el planificador de procesos al arrancar (D174); sin esta opción, el de por defecto (rr)")
    ap.add_argument("--sem-acpi", action="store_true", help="não usar o ACPI: apagar só avisa, sem botão de energia (D113)")
    ap.add_argument("--sin-servicios", action="store_true",
                    help="no lanzar el gestor de servicios ni el cron al arrancar (D216)")
    ap.add_argument("--rescate", action="store_true",
                    help="arrancar en modo rescate: consola con un shell de root, sin escritorio ni servicios "
                         "(lo mismo que mantener Ctrl pulsado al arrancar, D255)")
    ap.add_argument("--sesion-root", action="store_true",
                    default=os.environ.get("SAGUI_SESION_ROOT") == "1",
                    help="imagen de prueba: el escritorio y el Terminal como root en vez de sagui (D255). "
                         "Las pruebas lo ponen con SAGUI_SESION_ROOT=1")
    ap.add_argument("--sin-pedir-root", action="store_true", default=os.environ.get("SAGUI_PRUEBA_USUARIOS") == "1",
                    help="D371: el primer arranque con SAGUI-CONF no pide la contraseña de root en su pantalla "
                         "(para las pruebas, que ponen SAGUI_PRUEBA_USUARIOS=1; las de prueba la llevan salvo con --pedir-root). "
                         "Además: la consola no pide la cuenta y se entra sin la pantalla (inicio automático)")
    ap.add_argument("--pedir-root", dest="sin_pedir_root", action="store_false",
                    help="lo contrario de --sin-pedir-root")
    ap.add_argument("--sin-migrar-home", action="store_true", default=os.environ.get("SAGUI_PRUEBA_USUARIOS") == "1",
                    help="D371: no pasar los archivos de sagui a /home/sagui en el primer arranque (para las pruebas; "
                         "las imágenes de prueba la llevan salvo con --migrar-home)")
    ap.add_argument("--migrar-home", dest="sin_migrar_home", action="store_false", help="lo contrario de --sin-migrar-home")
    ap.add_argument("--sin-serie-terminal", action="store_true", default=os.environ.get("SAGUI_PRUEBA_USUARIOS") != "1",
                    help="D385: el Terminal no copia su salida al puerto serie (en un PC con COM1, esa copia limita la salida "
                         "a unos 3,8 KB/s). Lo llevan todas las imágenes salvo las de prueba (SAGUI_PRUEBA_USUARIOS=1), que "
                         "leen el Terminal por el serie; desde el sistema, «kconfig serie_terminal=si|no»")
    ap.add_argument("--serie-terminal", dest="sin_serie_terminal", action="store_false",
                    help="lo contrario de --sin-serie-terminal: el Terminal copia su salida al puerto serie")
    ap.add_argument("--sesion-sagui", dest="sesion_root", action="store_false",
                    help="lo contrario de --sesion-root (aunque SAGUI_SESION_ROOT=1)")
    ap.add_argument("--uacpi", action="store_true",
                    help="arrancar el intérprete de AML (uACPI): controlador embebido e indicador de batería. "
                         "Desactivado por defecto; el apagado y el botón no cambian (D192)")
    ap.add_argument("--sgl-motor", default="auto", choices=("auto", "software"),
                    help="motor 3D: 'auto' deja accelerate com a GPU; 'software' e SGL_MOTOR=software "
                         "(desactiva toda a aceleração por hardware, D158)")
    ap.add_argument("--sin-acel2d", action="store_true",
                    help="2D del escritorio siempre con la CPU, sin acelerador de la GPU (D195)")
    ap.add_argument("--acel2d", default="auto", choices=sorted(ACEL2D_MODOS),
                    help="acelerador 2D: auto (por el PCI, lo normal), cpu, o forzar vmware, intel, radeon, nvidia, "
                         "tdfx, mach64 o s3 (D195; radeon incluye las R6xx, que solas no se eligen, D223; nvidia, D224; "
                         "tdfx: 3dfx Banshee/Voodoo3/Voodoo4/5, D305; mach64: ATI Mach64/Rage, s3: S3 Trio64/ViRGE, D308)")
    ap.add_argument("--irq-prueba", default="",
                    help="D211 (solo pruebas): fallos simulados de la autoprueba de las IRQ, separados por comas: "
                         "prt-errada (GSI del _PRT equivocada), isa-errada (la entrada ISA no entrega), tormenta (D244: simula una en la IRQ 11 a los 2 s)")
    ap.add_argument("--ne2k-isa", action="store_true",
                    help="D329: sondear tarjetas NE2000 ISA en los puertos 0x300/0x280/0x320/0x340 (apagado por defecto)")
    ap.add_argument("--ne2k-sin-isa", action="store_true", help="D329: sin efecto (el sondeo ISA ya está apagado)")
    ap.add_argument("--ne2k-irq", type=int, default=0, choices=[0] + [3, 4, 5, 7, 9, 10, 11, 12, 15],
                    help="D329: IRQ de la NE2000 ISA; 0 = autodetectarla")
    ap.add_argument("--mandos-prueba", action="store_true",
                    help="D307: registra dos mandos de prueba (Dual Action por el analizador HID y Xbox 360) "
                         "y acepta informes inyectados (solo para las pruebas)")
    ap.add_argument("--usb-prueba", default="",
                    help="D227 (solo pruebas): fallos simulados de la enumeración USB, separados por comas: "
                         "primer-intento (el primer intento de cada conexión falla), siempre (fallan todos)")
    ap.add_argument("--sem-dma", action="store_true",
                    help="D281: el IDE por PIO aunque el controlador tenga Bus Master (para comparar y por si falla)")
    ap.add_argument("--sem-cache-disco", action="store_true",
                    help="D281: sin caché de bloques común (cada lectura y grabación va al disco)")
    ap.add_argument("--sin-escritura-diferida", action="store_true",
                    help="D281: la caché de bloques graba al instante (sin escritura diferida)")
    ap.add_argument("--cache-disco-baja", action="store_true",
                    help="D350: la caché de bloques en la memoria baja (hasta 64 MB) aunque haya memoria alta")
    ap.add_argument("--acel-segura", action="store_true",
                    help="D323: arrancar en modo seguro de la aceleración (2D por la CPU, 3D por software, sin la configuración guardada); "
                         "es lo que hace Mayús pulsado al arrancar")
    ap.add_argument("--acel2d-prueba", default="", choices=("", "pixel", "tiempo"),
                    help="solo pruebas: la autoprueba del acelerador falla a propósito (un píxel distinto o tiempo agotado)")
    ap.add_argument("--gpu3d", default="auto", choices=sorted(GPU3D_MODOS),
                    help="3D por la GPU (D285): auto (por el PCI: Intel Gen3), no (SaguiGL siempre por software), "
                         "intel (aunque el 2D haya fallado), radeon (D284/D287: el 3D de las Radeon R7xx y R5xx, sin probar en hardware, se prueba "
                         "con el primer uso; sin esto, solo con «gpu3d probar») o simulado (driver de pruebas por la CPU, para QEMU)")
    ap.add_argument("--gpu3d-anillo", action="store_true",
                    help="3D Intel: órdenes escritas en el anillo en vez de lotes (batch buffers); para comparar en hardware")
    ap.add_argument("--gpu3d-gtt", action="store_true",
                    help="3D Intel Gen3 (D358): el destino y las texturas en RAM del sistema mapeada en el GTT desde el "
                         "primer uso (escribe entradas del GTT; sin probar en hardware)")
    ap.add_argument("--gpu3d-prueba", default="", choices=("", "pixel", "tiempo"),
                    help="solo pruebas: la autoprueba 3D ve un píxel distinto, o la GPU deja de responder después")
    ap.add_argument("--usb-zip", action="store_true",
                    help="MBR como un disco Zip (USB-ZIP): geometría 64x32 y SaguiOS en la partición 4 (D203)")
    ap.add_argument("--usb-bpb", choices=["cero", "valido", "sin"], default="cero",
                    help="los bytes 0-0x59 del MBR: 'cero' (salto + BPB a cero, como siempre), "
                         "'valido' (BPB con la geometría) o 'sin' (sin la firma de sector FAT) (D203)")
    ap.add_argument("--cd", action="store_true", help="imagem para a ISO de CD (tools/mkiso.py, D077)")
    ap.add_argument("--sin-frecuencia", action="store_true",
                    help="arrancar con el driver de frecuencia desactivado: la CPU como la deja la BIOS (D106)")
    ap.add_argument("--frecuencia-simulada", action="store_true",
                    help="driver de frecuencia simulado, sin hardware (solo pruebas, D106)")
    ap.add_argument("--dados-mb", "--home-mb", dest="home_mb", type=int, default=0,
                    help="acrescenta uma partição de dados com N MB (SAGUI-DATOS, montada em /home, D040)")
    ap.add_argument("--dados-fs", "--home-fs", dest="home_fs", default=None, choices=("ext2", "fat32"),
                    help="sistema da partição /home: ext2 (D040) ou fat32 (legível no Windows)")
    ap.add_argument("--tmp-mb", type=int, default=0, help="partição SAGUI-TMP (montada em /tmp, D120) com N MB")
    ap.add_argument("--tmp-fs", default=None, choices=("ext2", "fat32"), help="sistema da partição /tmp")
    ap.add_argument("--logs-mb", type=int, default=0, help="partição SAGUI-LOGS (montada em /logs, D120) com N MB")
    ap.add_argument("--logs-fs", default=None, choices=("ext2", "fat32"), help="sistema da partição /logs")
    ap.add_argument("--extra-mb", type=int, action="append", default=[],
                    help="partição de dados extra (SAGUI-X1...) de N MB, sem carpeta fixa; pode repetir-se; implica --extendida (D120)")
    ap.add_argument("--esquema", default="logicas", choices=("logicas", "primarias", "mixto"),
                    help="D213: dónde van las particiones de datos y del sistema. logicas (por defecto): todas "
                         "como lógicas dentro de una extendida, y la de SaguiOS es la única primaria; primarias: "
                         "el formato de antes de la v7.3 (primarias 2-4; con más de 3, mixto); mixto: la primera "
                         "de datos primaria y el resto lógicas (el antiguo --extendida, D120)")
    ap.add_argument("--extendida", action="store_true",
                    help="D213: ya es lo de por defecto (todas en la extendida); se acepta por compatibilidad")
    ap.add_argument("--primarias", dest="esquema", action="store_const", const="primarias",
                    help="lo mismo que --esquema primarias (el formato de antes de la v7.3)")
    ap.add_argument("--jogos", default=None, metavar="PASTA",
                    help="partición de juegos (D181, D228): ext2 SAGUI-JOGOS con el contenido de la PASTA "
                         "(un juego por carpeta); el /etc/fstab la monta en /jogos, de lectura y escritura")
    ap.add_argument("--jogos-mb", type=int, default=0,
                    help="tamanho da partição de jogos em MB; 0 = o justo para o conteúdo")
    ap.add_argument("--apps", default=None, metavar="PASTA",
                    help="partición de aplicaciones (D194): ext2 SAGUI-APPS, de lectura y escritura, con el "
                         "contenido de la PASTA (bin/ y una carpeta por app); el /etc/fstab la monta en /apps")
    ap.add_argument("--apps-mb", type=int, default=0,
                    help="tamaño de la partición de aplicaciones en MB; 0 = lo justo más un margen "
                         "para instalar otras (el doble, al menos 4 MB más)")
    ap.add_argument("--raiz", default=None, metavar="PASTA",
                    help="D379 (prototipo): partición SAGUI-RAIZ (ext2) con la raíz del sistema (el árbol del ramdisk, "
                         "build/rootfs); el --ramdisk tiene que ser un initramfs (tools/mkinitramfs.py) que la monte")
    ap.add_argument("--modo-raiz", default="auto", choices=sorted(MODOS_RAIZ),
                    help="D379: fuerza el modo de la raíz (auto = lo que diga /etc/raiz.conf del initramfs; ram = el "
                         "initramfs en modo rescate; disco; live; toram)")
    ap.add_argument("--persist-mb", type=int, default=0,
                    help="D379 (L2): partición SAGUI-PERSIST (ext2 vacía) de N MB: la capa persistente de la live")
    ap.add_argument("--raiz-mb", type=int, default=0, help="tamaño de SAGUI-RAIZ en MB (0 = lo justo y margen)")
    ap.add_argument("--raiz-sin-comprimir", action="store_true",
                    help="v8.5.4.2 (D379 P5): con --cd, la raíz va como ext2 tal cual (por defecto va comprimida por "
                         "bloques de 64 KB, partición 0x5A, kernel/fs/disco_comprimido.c)")
    ap.add_argument("--raiz-versao", default=None,
                    help="D379 (M1, solo pruebas): la marca de versión que se escribe en la raíz (/sistema/raiz/versao); "
                         "por defecto, SAGUI_VERSAO de kernel/inc/versao.h (la del núcleo)")
    ap.add_argument("--part-fs", default="ext2", choices=("ext2", "fat32"),
                    help="sistema das partições sem --*-fs próprio (padrão ext2)")
    ap.add_argument("--sin-conf", action="store_true",
                    help="sin la partición de configuración SAGUI-CONF (ext2, 16 MB, con un LEEME; D256): por "
                         "defecto va en todas las imágenes de disco, la última; la ISO (--cd) no la lleva")
    ap.add_argument("--uefi", default=None, metavar="CARPETA",
                    help="imagen híbrida BIOS+UEFI (D264): añade una ESP FAT32 (SAGUI-EFI, tipo 0xEF, primaria) "
                         "con EFI/BOOT/BOOTX64.EFI y BOOTIA32.EFI de la CARPETA y el kernel, el ramdisk y la "
                         "cabecera en SAGUI/; el arranque por BIOS no cambia")
    ap.add_argument("--esp-mb", type=int, default=0,
                    help="tamaño de la ESP en MB (D264); 0 = lo justo más 8 MB (al menos 40: FAT32)")
    ap.add_argument("--mke2fs", default="/usr/sbin/mke2fs")
    ap.add_argument("--mkfs-vfat", default=achar_ferramenta("mkfs.vfat"))
    a = ap.parse_args()

    mbr = bytearray(open(a.mbr, "rb").read())
    e2 = open(a.estagio2, "rb").read()
    kern = open(a.kernel, "rb").read()
    rd = open(a.ramdisk, "rb").read()
    if len(mbr) != SETOR or mbr[510:512] != b"\x55\xAA":
        sys.exit("mkimagem: MBR inválido")
    global GEOM_CABECAS, GEOM_SETORES, SLOT_SAGUIOS, SLOT_DADOS
    if a.usb_zip:
        if a.cd:
            sys.exit("mkimagem: --usb-zip no combina con --cd")
        GEOM_CABECAS, GEOM_SETORES = 64, 32
        SLOT_SAGUIOS, SLOT_DADOS = 0x1EE, 0x1BE
    if mbr[0] != 0xEB or mbr[1] != 0x58 or mbr[2] != 0x90:
        sys.exit("mkimagem: el MBR no empieza por 'jmp short 0x5A; nop'")
    if setores(len(e2)) > 64:
        sys.exit("mkimagem: estágio 2 maior que 32 KB")
    # D147: o ramdisk tem de estar inteiro. Se a construção se cortou a meio
    # (ou duas construções ao mesmo tempo), pode ficar um SGFS sem entradas:
    # o kernel arrancava sem um único programa e nenhuma aplicação abria.
    # Aqui falha-se, com o motivo, em vez de gravar uma imagem inútil.
    entradas_rd, _total_rd = validar_sgfs(rd)

    jogos_mb = 0
    if a.jogos:
        if a.cd:
            sys.exit("mkimagem: --cd não combina com a partição de jogos (o kernel não lê o CD, D077/D181)")
        if not os.path.isdir(a.jogos) or not os.listdir(a.jogos):
            sys.exit(f"mkimagem: a pasta de jogos {a.jogos} não existe ou está vazia")
        jogos_mb = a.jogos_mb or mb_para_conteudo(a.jogos)
    apps_mb = 0
    if a.apps and a.raiz:
        pass   # D379 (P2 A): con la raíz en disco, /apps va dentro de SAGUI-RAIZ (sin partición propia)
    elif a.apps:
        if a.cd:
            sys.exit("mkimagem: --cd no combina con la partición de aplicaciones (el kernel no lee el CD, D077/D194)")
        if not os.path.isdir(a.apps) or not os.listdir(a.apps):
            sys.exit(f"mkimagem: la carpeta de aplicaciones {a.apps} no existe o está vacía")
        justo = mb_para_conteudo(a.apps)
        apps_mb = a.apps_mb or justo + max(4, justo)
        if apps_mb < justo:
            sys.exit(f"mkimagem: --apps-mb {apps_mb} no basta para {a.apps} (hacen falta {justo} MB)")
    raiz_mb = 0
    tmp_raiz = None
    if a.raiz:
        # D379 (T1): con --cd la raíz va dentro de la imagen El Torito; el kernel la ve en cd0img y,
        # en modo toram, la copia a la RAM (ram0).
        if not os.path.isdir(a.raiz) or not os.listdir(a.raiz):
            sys.exit(f"mkimagem: la carpeta raíz {a.raiz} no existe o está vacía")
        tmp_raiz = tempfile.TemporaryDirectory()
        preparar_raiz(a.raiz, tmp_raiz.name, a.raiz_versao or versao_do_nucleo())
        if a.apps:   # D379 (P2 A): SAGUI-RAIZ absorbe /apps (tal cual, como la partición SAGUI-APPS)
            if not os.path.isdir(a.apps) or not os.listdir(a.apps):
                sys.exit(f"mkimagem: la carpeta de aplicaciones {a.apps} no existe o está vacía")
            shutil.copytree(a.apps, os.path.join(tmp_raiz.name, "apps"), dirs_exist_ok=True, symlinks=True)
        justo = mb_para_conteudo(tmp_raiz.name)
        # D379 (M1): en la live la raíz es de solo lectura (lo que se cambia va a la capa) y en el CD se copia
        # entera a la RAM: un margen pequeño (lo instalado lo dimensiona el instalador, no esto).
        raiz_mb = a.raiz_mb or justo + max(8, justo // 16)
        if raiz_mb < justo:
            sys.exit(f"mkimagem: --raiz-mb {raiz_mb} no basta para {a.raiz} (hacen falta {justo} MB)")
    # D379 (P2 A): con la raíz en disco con escritura (--modo-raiz disco), SAGUI-CONF se funde en ella (/etc y
    # /root persistentes). En la live (las imágenes de serie, M1) la raíz es de solo lectura y SAGUI-CONF sigue
    # guardando la configuración como siempre (el kernel la superpone sobre el /etc de la capa).
    conf_mb = 0 if (a.cd or a.sin_conf or (a.raiz and a.modo_raiz == "disco")) else CONF_MB
    # D379 (M1, P4/P7 A): el CD con la raíz dentro arranca en toram (la raíz copiada a la RAM; si no cabe, live
    # desde el CD) diga lo que diga el raiz.conf del initramfs (el mismo de las imágenes de disco).
    if a.cd and a.raiz and a.modo_raiz == "auto":
        a.modo_raiz = "toram"
    sis_mb = jogos_mb + apps_mb + raiz_mb + conf_mb

    sim = simbolos(a.kernel_elf)
    if sim.get("_kernel_inicio") != KERNEL_ENDERECO:
        sys.exit("mkimagem: kernel não está ligado em 1 MB")
    kernel_fim = sim["_kernel_fim"]
    entrada = sim["_inicio"]
    rd_endereco = (kernel_fim + 0xFFF) & ~0xFFF

    e2_lba = 2
    k_lba = e2_lba + setores(len(e2))
    r_lba = k_lba + setores(len(kern))
    total = r_lba + setores(len(rd))

    opcoes = (OPC_VESA if a.vesa else 0) | (2 if a.console else 0) | (OPC_SEM_WC if a.sem_wc else 0) | (OPC_CD if a.cd else 0) \
        | (OPC_SEM_ACPI if a.sem_acpi else 0) \
        | (OPC_UACPI if a.uacpi else 0) \
        | (OPC_SEM_SERVICOS if a.sin_servicios else 0) \
        | (OPC_RESCATE if a.rescate else 0) \
        | (OPC_SESION_RAIZ if a.sesion_root else 0) \
        | (OPC_SGL_SOFTWARE if a.sgl_motor == "software" else 0) \
        | (OPC_SMP if a.smp else 0) \
        | (OPC_IOAPIC if a.ioapic else 0) \
        | (OPC_TURNOS if a.trava_turnos else 0) \
        | (PLANIFICADORES[a.planificador] << OPC_PLANIFICADOR_DESL if a.planificador else 0)
    res_l = res_a = 0
    if a.resolucao:
        try:
            res_l, res_a = (int(v) for v in a.resolucao.lower().split("x"))
        except ValueError:
            sys.exit("mkimagem: --resolucao precisa ser LxA, por exemplo 1280x1024")
    cab = struct.pack("<IHHIIIIIIIIIIIIII",
                      0x44484753, 1, opcoes,
                      e2_lba, setores(len(e2)),
                      k_lba, setores(len(kern)), len(kern), KERNEL_ENDERECO,
                      r_lba, setores(len(rd)), len(rd), rd_endereco,
                      total, entrada, soma(kern), soma(rd))
    cab += a.teste.encode("latin-1")[:63].ljust(64, b"\0")
    cab += struct.pack("<HH", res_l, res_a)   # CAB_RES_LARG, CAB_RES_ALT (desplazamiento 128)
    cab += struct.pack("<B", a.bpp)           # CAB_BPP_PREF (desplazamiento 132, D078)
    cab += struct.pack("<B", (1 if a.sin_frecuencia else 0) | (2 if a.frecuencia_simulada else 0))   # CAB_FREQ (133, D106)
    cab += struct.pack("<B", 1 if a.sgl_motor == "software" else 0)   # CAB_MOTOR (134, D158): 1 = solo software
    cab += struct.pack("<B", acel2d_byte(a))                          # CAB_ACEL2D (135, D195): 2D por la GPU
    cab += struct.pack("<I", sis_mb * 2048)   # CAB_SIS_SETORES (136, D181): sectores de las particiones del sistema
    cab += struct.pack("<B", irq_prueba_byte(a.irq_prueba))   # CAB_IRQ_PRUEBA (140, D211): fallos simulados
    cab += struct.pack("<B", usb_prueba_byte(a.usb_prueba))   # CAB_USB_PRUEBA (141, D227): fallos simulados
    cab += struct.pack("<B", gpu3d_byte(a))                   # CAB_GPU3D (142, D285): 3D por la GPU
    assert len(cab) == 143, "CAB_DISCO_OPC tiene que caer en el byte 143"
    cab += struct.pack("<B", (1 if a.sem_dma else 0) | (2 if a.sem_cache_disco else 0) | (4 if a.sin_escritura_diferida else 0) | (8 if a.cache_disco_baja else 0))   # CAB_DISCO_OPC (143, D281)
    # 144-155 = CAB_ID_INSTALACAO (D179: lo escribe «instalar»).
    cab = cab.ljust(156, b"\0")
    cab += struct.pack("<B", 1 if a.mandos_prueba else 0)    # CAB_MANDOS_PRUEBA (156, D307): mandos de prueba
    cab += struct.pack("<BB", 1 if a.ne2k_isa else 0, a.ne2k_irq)   # CAB_NE2K_OPC (157), CAB_NE2K_IRQ (158) (D329)
    assert len(cab) == 159, "CAB_MEM_OPC tiene que caer en el byte 159"
    cab += struct.pack("<B", (1 if a.sem_pae else 0) | (2 if a.sem_nx else 0) | (4 if a.sin_swap else 0))   # CAB_MEM_OPC (159, D349; bit 2 D352)
    if not 0 <= a.swap_mb <= 1024:
        sys.exit("mkimagem: --swap-mb tiene que estar entre 1 y 1024 (0 = automático)")
    cab += struct.pack("<H", a.swap_mb)   # CAB_TROCA_MB (160, D352): tope de la swap comprimida en MB
    cab += struct.pack("<B", (1 if a.sin_pedir_root else 0) | (2 if a.sin_migrar_home else 0))   # CAB_USU_OPC (162, D371)
    assert len(cab) == 163, "CAB_RAIZ_OPC tiene que caer en el byte 163"
    cab += struct.pack("<B", MODOS_RAIZ[a.modo_raiz])   # CAB_RAIZ_OPC (163, D379)
    assert len(cab) == 164, "CAB_TERM_OPC tiene que caer en el byte 164"
    cab += struct.pack("<B", 1 if a.sin_serie_terminal else 0)   # CAB_TERM_OPC (164, D385): bit 0 sin copia al serie
    # 165-191 libres. 192-255 = CAB_KCONF (D353): lo que guarda «kconfig» desde el sistema;
    # aquí se deja a cero (preencher_setores): los valores de la imagen son los bits de arriba.

    # Tamaño del archivo: redondeado a múltiplo de 32 KB. La partición cubre
    # desde el LBA 1 hasta el fin del archivo y es la única, marcada como activa (0x80),
    # porque algunas BIOS solo tratan un pendrive como disco duro si hay
    # una partición activa válida.
    tam_arquivo = ((total * SETOR) + ALINHAMENTO_ARQUIVO - 1) // ALINHAMENTO_ARQUIVO * ALINHAMENTO_ARQUIVO
    setores_arquivo = setores(max(tam_arquivo, TAMANHO_MINIMO))
    if a.cd:
        if a.home_mb or a.tmp_mb or a.logs_mb:
            sys.exit("mkimagem: --cd não combina com partições de dados (o CD não se grava)")
        cilindro = GEOM_CABECAS * GEOM_SETORES
        setores_arquivo = (setores_arquivo + cilindro - 1) // cilindro * cilindro
    p_inicio, p_qtd = 1, setores_arquivo - 1
    entrada_part = (bytes([0x80]) + chs(p_inicio) + bytes([TIPO_PARTICAO]) + chs(p_inicio + p_qtd - 1)
                    + struct.pack("<II", p_inicio, p_qtd))
    mbr[SLOT_SAGUIOS:SLOT_SAGUIOS + 16] = entrada_part
    if a.usb_bpb == "sin":
        # Sin la firma de un sector FAT (EB xx 90): hay BIOS que, al verla, tratan el
        # pendrive como un disquete (USB-FDD) y leen la geometría del BPB. 'nop; jmp
        # short 0x5A' salta al mismo sitio.
        mbr[0:3] = bytes([0x90, 0xEB, 0x57])
    elif a.usb_bpb == "valido":
        # BPB de disco duro con la geometría de la tabla (como el de un FAT de verdad),
        # para las BIOS que leen la geometría de aquí. No describe ningún FAT: sin FAT
        # (0 copias) y sin directorio raíz; nadie lo monta.
        bpb = struct.pack("<HBHBHHBHHHII", SETOR, 1, 1, 0, 0, 0, 0xF8, 0, GEOM_SETORES, GEOM_CABECAS,
                          0, setores_arquivo)
        mbr[0x0B:0x0B + len(bpb)] = bpb
        mbr[0x24] = 0x80                                   # número de unidad (BPB de FAT16)
    if a.usb_zip or a.usb_bpb != "cero":
        print(f"mkimagem: MBR {'USB-ZIP (64x32, SaguiOS en la partición 4)' if a.usb_zip else 'USB-HDD'}, "
              f"BPB {a.usb_bpb}")

    img = bytes(mbr) + preencher_setores(cab) + preencher_setores(e2) + preencher_setores(kern) + preencher_setores(rd)
    assert len(img) == total * SETOR
    util = len(img)
    img += b"\0" * (setores_arquivo * SETOR - len(img))
    with open(a.saida, "wb") as f:
        f.write(img)
    tabela = [("SAGUI-DATOS", a.home_mb, a.home_fs or a.part_fs, "/home"),
              ("SAGUI-TMP", a.tmp_mb, a.tmp_fs or a.part_fs, "/tmp"),
              ("SAGUI-LOGS", a.logs_mb, a.logs_fs or a.part_fs, "/logs")]
    tabela += [(f"SAGUI-X{k + 1}", mb, a.part_fs, "uma carpeta à tua escolha") for k, mb in enumerate(a.extra_mb)]
    # Las del sistema van las últimas (juegos y luego aplicaciones): así /home sigue siendo
    # la primera de datos (D040, D120) y las imágenes de antes no cambian de orden.
    conteudos = {}
    if jogos_mb:
        tabela.append((ROTULO_JOGOS, jogos_mb, "ext2", "/jogos"))
        conteudos[ROTULO_JOGOS] = a.jogos
    if apps_mb:
        tabela.append((ROTULO_APPS, apps_mb, "ext2", "/apps"))
        conteudos[ROTULO_APPS] = a.apps
    if raiz_mb:   # D379: la raíz, justo antes de la de configuración
        tabela.append((ROTULO_RAIZ, raiz_mb, "ext2", "/"))
        conteudos[ROTULO_RAIZ] = tmp_raiz.name
    tmp_persist = tempfile.TemporaryDirectory()
    if a.persist_mb:   # D379 (L2): vacía (sin leeme: lo que hay en ella aparece en la raíz de la live)
        tabela.append((ROTULO_PERSIST, a.persist_mb, "ext2", "/persist"))
        conteudos[ROTULO_PERSIST] = tmp_persist.name
    # D256: la de configuración es la última de todas; lleva solo un LEEME.
    tmp_conf = tempfile.TemporaryDirectory()
    if conf_mb:
        with open(os.path.join(tmp_conf.name, "LEEME"), "w", encoding="utf-8") as g:
            g.write(TEXTO_LEEME_CONF)
        tabela.append((ROTULO_CONF, conf_mb, "ext2", "/conf"))
        conteudos[ROTULO_CONF] = tmp_conf.name
    esquema = a.esquema
    if a.cd and raiz_mb:
        esquema = "primarias"   # v8.5.4.2: en el CD la raíz es la única partición (primaria): se comprime en su sitio
    if esquema == "primarias" and a.extendida:
        esquema = "mixto"           # --primarias --extendida: el antiguo --extendida
    # D264: la ESP va justo detrás del área de arranque (alineada a 1 MB) y antes de la
    # extendida; su entrada, en la primera libre de la tabla tras las de datos.
    fim_arranque = len(img)
    esp = None
    if a.uefi:
        if a.cd:
            sys.exit("mkimagem: --uefi no combina con --cd todavía (la ISO híbrida UEFI está pendiente, D264)")
        esp = montar_esp(a, a.saida, len(img), preencher_setores(cab), kern, rd)
        fim_arranque = (esp[0] + esp[1]) * SETOR
    acrescentar_particoes(a.saida, fim_arranque, [t for t in tabela if t[1] > 0], a.mke2fs, a.mkfs_vfat,
                          esquema, conteudos=conteudos)
    if esp:
        with open(a.saida, "r+b") as f:
            tab = bytearray(f.read(512))
            livres = [o for o in (0x1BE, 0x1CE, 0x1DE, 0x1EE) if o != SLOT_SAGUIOS and tab[o + 4] == 0 and o > SLOT_DADOS - 1]
            livres += [o for o in (0x1BE, 0x1CE, 0x1DE, 0x1EE) if o != SLOT_SAGUIOS and tab[o + 4] == 0 and o not in livres]
            if not livres:
                sys.exit("mkimagem: --uefi necesita una entrada libre en la tabla MBR para la ESP "
                         "(con --esquema primarias y 3 particiones de datos no queda ninguna)")
            f.seek(livres[0])
            f.write(entrada_mbr(0xEF, esp[0], esp[1]))
        print(f"mkimagem: ESP SAGUI-EFI (FAT32, tipo 0xEF) de {esp[1] // 2048} MB en el LBA {esp[0]}, "
              f"entrada {(livres[0] - 0x1BE) // 16 + 1} (D264)")
    if a.cd and raiz_mb and not a.raiz_sin_comprimir:
        comprimir_raiz(a.saida, ROTULO_RAIZ)
    if a.cd and raiz_mb:
        # D379 (T1): con la raíz dentro, la imagen del CD también tiene que acabar en un cilindro entero
        # (la BIOS, en la emulación de disco de El Torito, toma la geometría de la tabla).
        cil = GEOM_CABECAS * GEOM_SETORES * SETOR
        tam = os.path.getsize(a.saida)
        with open(a.saida, "r+b") as f:
            f.truncate((tam + cil - 1) // cil * cil)
    tmp_conf.cleanup()
    if tmp_raiz:
        tmp_raiz.cleanup()
    tmp_persist.cleanup()
    util_sis = util + sis_mb * 1024 * 1024
    # tamanho.txt: la imagen entera que cuenta «Acerca de» (arranque y particiones del
    # sistema). tamanho-base.txt: la meta de 38 MB (D181), que desde la v7.4 es la del
    # sistema base (arranque y /apps): la partición de juegos no cuenta (D228), porque es
    # de 800 MB de lectura y escritura o lleva lo que el que compila mete en input_share.
    # D306 (mantenedor, 2026-11-15): la meta de 38 MB es la del ARRANQUE (lo que se carga en la RAM al
    # arrancar: etapa 2, kernel y ramdisk), para que no se pase de RAM. Las aplicaciones de SAGUI-APPS
    # se leen del disco al usarlas y pueden ocupar lo que haga falta: no cuentan.
    util_base = util
    pasta = os.path.dirname(os.path.abspath(a.saida))
    with open(os.path.join(pasta, "tamanho.txt"), "w") as f:
        f.write(str(util_sis))
    with open(os.path.join(pasta, "tamanho-base.txt"), "w") as f:
        f.write(str(util_base))

    print(f"mkimagem: estágio 2 LBA {e2_lba} ({setores(len(e2))} set), kernel LBA {k_lba} "
          f"({len(kern)} B em 0x{KERNEL_ENDERECO:X}, fim BSS 0x{kernel_fim:X}), "
          f"ramdisk LBA {r_lba} ({len(rd)} B em 0x{rd_endereco:X}, {entradas_rd} entradas)")
    print(f"mkimagem: {util_sis} bytes úteis; sistema base {util_base} ({100.0 * util_base / META_BYTES:.1f}% da meta de {META_MB} MB): "
          f"arranque {util} (ramdisk {len(rd)}), partições do sistema {sis_mb * 1024 * 1024}"
          f"{f' ({ROTULO_JOGOS} {jogos_mb} MB)' if jogos_mb else ''}"
          f"{f' ({ROTULO_APPS} {apps_mb} MB)' if apps_mb else ''}"
          f"{f' ({ROTULO_CONF} {conf_mb} MB)' if conf_mb else ''}; arquivo {len(img)} bytes -> {a.saida}")


ESP_ROTULO = "SAGUI-EFI"
ESP_VOLUME_ID = "5A6AEF10"   # fijo: dos compilaciones dan la misma ESP


def montar_esp(a, saida, tam, cab, kern, rd):
    """D264: la partición del sistema EFI de la imagen híbrida. FAT32 (lo que todo firmware
    UEFI lee) con:
        EFI/BOOT/BOOTX64.EFI    cargador para firmware de 64 bits
        EFI/BOOT/BOOTIA32.EFI   cargador para firmware de 32 bits
        SAGUI/CABECERA.BIN      la cabecera de arranque (512 bytes, igual que el LBA 1)
        SAGUI/KERNEL.BIN        el kernel (binario plano, el mismo que va tras la etapa 2)
        SAGUI/RAMDISK.SGF       el ramdisk SGFS
    Se monta aparte y se copia en su sitio (disperso). Devuelve (LBA de inicio, sectores)."""
    efis = [os.path.join(a.uefi, n) for n in ("BOOTX64.EFI", "BOOTIA32.EFI")]
    for e in efis:
        if not os.path.isfile(e):
            sys.exit(f"mkimagem: --uefi: falta {e} (make uefi)")
    mkfs_vfat = exigir_ferramenta(a.mkfs_vfat, "mkfs.vfat", "dosfstools")
    mcopy = exigir_ferramenta(achar_ferramenta("mcopy"), "mcopy", "mtools")
    mmd = exigir_ferramenta(achar_ferramenta("mmd"), "mmd", "mtools")
    conteudo = sum(os.path.getsize(e) for e in efis) + len(cab) + len(kern) + len(rd)
    mb = a.esp_mb or max(40, (conteudo + (1 << 20) - 1) // (1 << 20) + 8)
    if mb < 34:
        sys.exit("mkimagem: --esp-mb: una FAT32 necesita al menos 34 MB")
    inicio = (tam // SETOR + 2047) // 2048 * 2048
    qtd = mb * 2048
    env = dict(os.environ, MTOOLS_SKIP_CHECK="1")
    with tempfile.TemporaryDirectory() as tmp:
        vol = os.path.join(tmp, "esp.img")
        with open(vol, "wb") as f:
            f.truncate(qtd * SETOR)
        # -s 1: clústeres de 512 bytes, para que haya más de 65525 (si no, no sería FAT32 de verdad);
        # -h: sectores ocultos = el inicio de la partición, como un FAT32 hecho en el disco.
        subprocess.run([mkfs_vfat, "-F", "32", "-s", "1", "-h", str(inicio), "-n", ESP_ROTULO, "-i", ESP_VOLUME_ID, vol],
                       check=True, stdout=subprocess.DEVNULL)
        for d in ("::/EFI", "::/EFI/BOOT", "::/SAGUI"):
            subprocess.run([mmd, "-i", vol, d], check=True, env=env)
        arq_cab = os.path.join(tmp, "CABECERA.BIN")
        arq_k = os.path.join(tmp, "KERNEL.BIN")
        arq_r = os.path.join(tmp, "RAMDISK.SGF")
        for caminho, dados in ((arq_cab, cab), (arq_k, kern), (arq_r, rd)):
            with open(caminho, "wb") as f:
                f.write(dados)
        for origem, destino in ((efis[0], "::/EFI/BOOT/BOOTX64.EFI"), (efis[1], "::/EFI/BOOT/BOOTIA32.EFI"),
                                (arq_cab, "::/SAGUI/CABECERA.BIN"), (arq_k, "::/SAGUI/KERNEL.BIN"),
                                (arq_r, "::/SAGUI/RAMDISK.SGF")):
            subprocess.run([mcopy, "-i", vol, origem, destino], check=True, env=env)
        with open(saida, "r+b") as f:
            f.truncate(max(os.path.getsize(saida), (inicio + qtd) * SETOR))
        copiar_disperso(vol, saida, inicio * SETOR)
    return inicio, qtd


RZ_BLOCO = 65536
RZ_TIPO = 0x5A   # kernel/inc/disco_comprimido.h: DISCO_TIPO_RAIZ_COMPRIMIDA


def comprimir_raiz(saida, rotulo):
    """v8.5.4.2 (D379 P5): la partición ext2 «rotulo» de la imagen, comprimida por bloques de 64 KB con
    DEFLATE en su mismo sitio (tipo 0x5A); la imagen se recorta detrás. El núcleo la lee con
    kernel/fs/disco_comprimido.c (formato descrito allí). Solo para la imagen del CD: es de solo lectura."""
    import zlib
    with open(saida, "r+b") as f:
        mbr = bytearray(f.read(SETOR))
        achada = None
        for o in (0x1BE, 0x1CE, 0x1DE, 0x1EE):
            tipo = mbr[o + 4]
            ini, qtd = struct.unpack_from("<II", mbr, o + 8)
            if tipo != 0x83 or not qtd:
                continue
            f.seek(ini * SETOR + 1024 + 120)
            if f.read(16).split(b"\0")[0].decode("latin-1") == rotulo:
                achada = (o, ini, qtd)
        if not achada:
            sys.exit(f"mkimagem: no se encuentra la partición {rotulo} para comprimirla")
        o, ini, qtd = achada
        # v8.5.4.2 (revisión): se recorta la imagen detrás de la raíz: nada puede ir detrás (p. ej. --persist-mb)
        for o2 in (0x1BE, 0x1CE, 0x1DE, 0x1EE):
            ini2, qtd2 = struct.unpack_from("<II", mbr, o2 + 8)
            if o2 != o and mbr[o2 + 4] and qtd2 and ini2 >= ini:
                sys.exit(f"mkimagem: con --cd, {rotulo} tiene que ser la última partición para comprimirla "
                         f"(hay otra en el LBA {ini2}); quita las de detrás o usa --raiz-sin-comprimir")
        f.seek(ini * SETOR)
        ext2 = f.read(qtd * SETOR)
    n = (len(ext2) + RZ_BLOCO - 1) // RZ_BLOCO
    tam_tab = n * 12
    dados0 = SETOR + (tam_tab + SETOR - 1) // SETOR * SETOR
    tabela = bytearray()
    corpo = bytearray()
    for i in range(n):
        bloco = ext2[i * RZ_BLOCO:(i + 1) * RZ_BLOCO]
        crc = zlib.crc32(bloco) & 0xFFFFFFFF
        if not bloco.strip(b"\0"):
            tabela += struct.pack("<III", 0, 0, crc)
            continue
        c = zlib.compressobj(9, zlib.DEFLATED, -15)
        z = c.compress(bloco) + c.flush()
        # v8.5.4.2 (revisión): cada bloque se comprueba al generarlo (se descomprime y se compara con su CRC)
        if (zlib.crc32(zlib.decompress(z, -15)) & 0xFFFFFFFF) != crc:
            sys.exit(f"mkimagem: el bloque {i} de {rotulo} no se descomprime igual: no se genera la imagen")
        cru = len(z) >= len(bloco)
        if cru:
            z = bloco
        tabela += struct.pack("<III", dados0 + len(corpo), len(z) | (0x80000000 if cru else 0), crc)
        corpo += z
    cab = bytearray(SETOR)
    struct.pack_into("<4sIIIII", cab, 0, b"SGRZ", 1, RZ_BLOCO, n, qtd, SETOR)
    cab[24:40] = rotulo.encode("latin-1")[:16].ljust(16, b"\0")
    struct.pack_into("<I", cab, 40, zlib.crc32(tabela) & 0xFFFFFFFF)
    tudo = bytes(cab) + bytes(tabela).ljust(dados0 - SETOR, b"\0") + bytes(corpo)
    novo = (len(tudo) + SETOR - 1) // SETOR
    with open(saida, "r+b") as f:
        f.seek(ini * SETOR)
        f.write(tudo.ljust(novo * SETOR, b"\0"))
        f.truncate((ini + novo) * SETOR)
        mbr[o:o + 16] = entrada_mbr(RZ_TIPO, ini, novo)
        f.seek(0)
        f.write(mbr)
    print(f"mkimagem: {rotulo} comprimida (D379 P5): {qtd // 2048} MB -> {novo * SETOR / 1048576:.1f} MB "
          f"en {n} bloques de 64 KB (tipo 0x{RZ_TIPO:02X})")


def versao_do_nucleo():
    """SAGUI_VERSAO de kernel/inc/versao.h (la que el núcleo compara con la marca de la raíz, D379)."""
    import re
    h = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "kernel", "inc", "versao.h")
    with open(h, encoding="utf-8") as f:
        m = re.search(r'^#define\s+SAGUI_VERSAO\s+"([^"]+)"', f.read(), re.M)
    if not m:
        sys.exit(f"mkimagem: no se encuentra SAGUI_VERSAO en {h}")
    return m.group(1)


# D379 (M1): lo que identifica la raíz. "versao": la versión del núcleo con el que va (el kernel no usa una raíz
# de otra versión, raiz.c). "etc.sha256": las sumas del /etc de serie, para que el instalador, al reinstalar,
# sepa qué archivos de /etc cambió la persona (esos se conservan) y cuáles son de serie (esos se actualizan).
MARCA_RAIZ = "sistema/raiz/versao"
SOMAS_ETC = "sistema/raiz/etc.sha256"


def marcar_raiz(destino, versao):
    import hashlib
    os.makedirs(os.path.join(destino, "sistema", "raiz"), exist_ok=True)
    linhas = []
    etc = os.path.join(destino, "etc")
    for raiz, pastas, arqs in os.walk(etc):
        pastas.sort()
        for n in sorted(arqs):
            real = os.path.join(raiz, n)
            if os.path.islink(real):
                continue
            rel = os.path.relpath(real, destino).replace(os.sep, "/")
            with open(real, "rb") as f:
                linhas.append(f"{hashlib.sha256(f.read()).hexdigest()}  {rel}\n")
    with open(os.path.join(destino, SOMAS_ETC), "w", encoding="latin-1") as f:
        f.write("".join(linhas))
    with open(os.path.join(destino, MARCA_RAIZ), "w", encoding="latin-1") as f:
        f.write(versao + "\n")


def preparar_raiz(origem, destino, versao):
    """D379: copia el árbol de la raíz como lo vería el kernel en el ramdisk: los textos
    (EXT_TEXTO de mksgfs) pasan de UTF-8 a Latin-1 y se quitan los .gitkeep. Al final, la
    marca de versión y las sumas de /etc (marcar_raiz)."""
    for raiz, pastas, arqs in os.walk(origem):
        rel = os.path.relpath(raiz, origem)
        dest = os.path.join(destino, rel) if rel != "." else destino
        os.makedirs(dest, exist_ok=True)
        for n in arqs:
            if n == ".gitkeep":
                continue
            src = os.path.join(raiz, n)
            if os.path.islink(src):
                continue
            with open(src, "rb") as f:
                dados = f.read()
            if dados[:4] != b"\x7fELF" and os.path.splitext(n)[1].lower() in EXT_TEXTO:
                try:
                    dados = dados.decode("utf-8").encode("latin-1")
                except (UnicodeDecodeError, UnicodeEncodeError):
                    pass
            with open(os.path.join(dest, n), "wb") as g:
                g.write(dados)
            shutil.copymode(src, os.path.join(dest, n))
    # Las carpetas que en el árbol en RAM crea el kernel al usarlas (/home, D371) o los montajes:
    # en la raíz en disco tienen que existir ya (los puntos de montaje de /etc/fstab).
    for p in ("home", "tmp", "apps", "apps/vps", "jogos", "logs", "sistema/conf", "root", "initrd"):
        os.makedirs(os.path.join(destino, p), exist_ok=True)
    marcar_raiz(destino, versao)


# D379: los permisos que el kernel da al árbol del ramdisk (vfs.c, meta_inicial), en la raíz en disco.
def ordens_permisos_raiz(conteudo):
    ordens = []
    for raiz, pastas, arqs in os.walk(conteudo):
        for n in sorted(pastas) + sorted(arqs):
            real = os.path.join(raiz, n)
            rel = os.path.relpath(real, conteudo).replace(os.sep, "/")
            if os.path.islink(real):
                continue
            if '"' in rel or "\n" in rel:
                sys.exit(f"mkimagem: nombre con comillas o salto de línea en la raíz: /{rel!r}")
            pasta = os.path.isdir(real)
            if rel == "home" or rel.startswith("home/"):
                ordens += [f'sif "/{rel}" uid 1000', f'sif "/{rel}" gid 1000']
            if rel.startswith("sistema/") and not (rel == "sistema/idiomas" or rel.startswith("sistema/idiomas/")):
                ordens.append(f'sif "/{rel}" mode {"040700" if pasta else "0100700"}')
            if rel == "etc/sombra":
                ordens.append(f'sif "/{rel}" mode 0100600')
    for rel in PASTAS_DE_SAGUI["/apps"]:   # /apps/vps de sagui también dentro de la raíz (D297)
        if os.path.isdir(os.path.join(conteudo, "apps" + rel)):
            ordens += [f'sif "/apps{rel}" uid 1000', f'sif "/apps{rel}" gid 1000']
    for rel, modo in (("tmp", "041777"), ("sistema", "040711"), ("root", "040700")):
        if os.path.isdir(os.path.join(conteudo, rel)):
            ordens.append(f'sif "/{rel}" mode {modo}')
    return ordens


def mb_para_conteudo(pasta):
    """MB de una ext2 (bloques de 1 KB) donde quepa la pasta: los datos redondeados a
    bloques, un 5 % para los metadatos de cada fichero y 768 KB de tablas fijas."""
    kb = 0
    for raiz, dirs, arquivos in os.walk(pasta):
        kb += 1 + len(dirs)
        for n in arquivos:
            if n.lower().endswith(".gguf"):   # D380: los modelos de IA nunca van en las imágenes
                sys.exit("mkimagem: hay un modelo de IA (%s) en lo que va a la imagen: los modelos van "
                         "en /home/compartido/modelos (input_share/modelos no se copia; ver docs/llm.md)"
                         % os.path.join(raiz, n))
            kb += (os.path.getsize(os.path.join(raiz, n)) + 1023) // 1024 + 1
    # Metadatos: bloques indirectos (~1/256 de los datos con bloques de 1 KB), mapas de
    # bits y descriptores de grupo, y la tabla de inodos, que mke2fs dimensiona por el
    # tamaño de la partición (un inodo de 256 B cada 4 KB: 1/16). Con un 5 % y 768 KB
    # fijos no cabían 51 MB de juegos (Jazz y el pak de Quake, D228/D229).
    kb = kb * 105 // 100
    kb = kb + kb // 16 + kb // 128 + 1024
    return max(2, (kb + 1023) // 1024)


def achar_ferramenta(nome):
    """Busca una herramienta en el PATH, en /usr/sbin y /sbin y en las instaladas sin sudo
    (~/.local/saguitools, mejora 005), para que funcione aunque no se haya cargado env.sh."""
    raiz = os.path.expanduser("~/.local/saguitools/root")
    for c in (shutil.which(nome), f"/usr/sbin/{nome}", f"/sbin/{nome}",
              f"{raiz}/sbin/{nome}", f"{raiz}/usr/sbin/{nome}", f"{raiz}/usr/bin/{nome}"):
        if c and os.path.exists(c):
            return c
    return None


def exigir_ferramenta(caminho, nome, pacote):
    if not caminho:
        sys.exit(f"mkimagem: no se encuentra {nome}. Instala el paquete {pacote} "
                 f"(sudo apt install {pacote}) o carga las herramientas sin sudo con "
                 f"'. ~/.local/saguitools/env.sh'.")
    return caminho


TEXTO_LEEME_CONF = (
    "SAGUI-CONF: la configuración del sistema de SaguiOS.\n"
    "\n"
    "Esta partición guarda lo que el sistema necesita recordar entre arranques:\n"
    "usuarios y contraseñas, claves de root y del servidor SSH, la contraseña\n"
    "del VNC, los servicios cambiados y la red.\n"
    "\n"
    "No la borres ni la formatees: sin ella SaguiOS arranca con los valores de\n"
    "fábrica y se pierden las contraseñas y las claves.\n")


def texto_leeme(ponto, tipo):
    """Texto del leeme.txt de cada partición de datos (D120)."""
    return (f"Esta carpeta ({ponto}) está en la partición {tipo} {ponto} del disco de arranque de SaguiOS.\n"
            "Lo que guardes aquí sigue aquí después de reiniciar SaguiOS.\n")


# D282: dueños de las particiones ext2 (mke2fs -d copia el uid de quien compila, que
# en otro PC no es 1000). /home y /jogos son de sagui (la persona escribe ahí); el
# resto, de root (/apps: solo root instala programas). El kernel da a /tmp y /logs el
# 1777 por su cuenta, sea cual sea el sistema de archivos.
def dono_da_particao(ponto):
    return (1000, 1000) if ponto in ("/home", "/jogos") else (0, 0)


# D282: carpetas de sagui dentro de una partición de root (VisualPythonS genera en /apps/vps, D297).
PASTAS_DE_SAGUI = {"/apps": ("/vps",)}


def normalizar_donos(saida, inicio, conteudo, uid, gid, ponto=""):
    """D282: deja lo copiado con -d con el dueño de la partición y permisos limpios
    (carpetas 0755; archivos 0755 si eran ejecutables, si no 0644), con debugfs. Si
    no está, se queda como lo dejó mke2fs (el uid de quien compila) y se avisa."""
    debugfs = achar_ferramenta("debugfs")
    if not debugfs:
        if ponto == "/":
            # v8.5.4.2 (D378 §14, D379): en la raíz los dueños root son obligatorios: sin ellos /bin, /lib,
            # /etc y /apps serían de quien compila (el uid 1000 es sagui dentro de SaguiOS: podría cambiar
            # los programas del sistema y las bibliotecas). Nunca se genera una SAGUI-RAIZ así.
            sys.exit("mkimagem: no se encuentra debugfs (paquete e2fsprogs): hace falta para dejar la raíz "
                     "(SAGUI-RAIZ) de root. Instálalo (sudo apt install e2fsprogs) o carga "
                     "'. ~/.local/saguitools/env.sh'.")
        print("mkimagem: aviso: sin debugfs, los archivos de la partición quedan con el dueño de quien compila")
        return
    ordens = []
    for raiz, pastas, arqs in os.walk(conteudo):
        for n in sorted(pastas) + sorted(arqs):
            real = os.path.join(raiz, n)
            if os.path.islink(real):
                continue
            rel = "/" + os.path.relpath(real, conteudo).replace(os.sep, "/")
            if '"' in rel or "\n" in rel:
                if ponto == "/":
                    # D378: en la raíz no se deja nada sin dueño root: un nombre que debugfs no sabe escribir para.
                    sys.exit(f"mkimagem: nombre con comillas o salto de línea en la raíz: {rel!r}")
                continue
            if os.path.isdir(real):
                modo = 0o40755
            else:
                modo = 0o100755 if os.stat(real).st_mode & 0o111 else 0o100644
            ordens += [f'sif "{rel}" uid {uid}', f'sif "{rel}" gid {gid}', f'sif "{rel}" mode 0{modo:o}']
    # /apps es de root, pero VisualPythonS (D297) guarda ahí, como sagui, las aplicaciones que genera:
    # /apps/vps se crea ya de sagui (si no existe, «mkdir» falla sin más y el «sif» lo corrige).
    if ponto == "/":
        ordens += ordens_permisos_raiz(conteudo)
    for rel in PASTAS_DE_SAGUI.get(ponto, ()):
        ordens += [f'mkdir "{rel}"', f'sif "{rel}" uid 1000', f'sif "{rel}" gid 1000', f'sif "{rel}" mode 040755']
    if not ordens:
        return
    with tempfile.NamedTemporaryFile("w", suffix=".debugfs", delete=False) as g:
        g.write("\n".join(ordens) + "\n")
        nome = g.name
    try:
        p = subprocess.run([debugfs, "-w", "-f", nome, f"{saida}?offset={inicio * SETOR}"], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, errors="replace",
                           env=dict(os.environ, E2FSPROGS_FAKE_TIME=os.environ.get("SOURCE_DATE_EPOCH", "1762905600")))
    finally:
        os.unlink(nome)
    # debugfs sale con 0 aunque falle un «sif»: sus errores se muestran y, en la raíz, se comprueba lo que importa.
    erros = [l for l in p.stderr.splitlines() if l.strip() and not l.startswith("debugfs ")
             and "mkdir: Ext2 directory already exists" not in l and "File exists" not in l]
    if erros:
        print("mkimagem: debugfs avisó:\n  " + "\n  ".join(erros[:20]))
    if ponto == "/":
        comprovar_donos_raiz(debugfs, saida, inicio, conteudo)


def donos_ext2(debugfs, saida, inicio, rel):
    """(uid, modo) de una ruta de la ext2 en el desplazamiento «inicio» (sectores), o None si no está."""
    p = subprocess.run([debugfs, "-R", f'stat "{rel}"', f"{saida}?offset={inicio * SETOR}"],
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors="replace")
    mu = re.search(r"User:\s+(\d+)", p.stdout)
    mm = re.search(r"Mode:\s+(0?[0-7]+)", p.stdout)
    return (int(mu.group(1)), int(mm.group(1), 8)) if mu and mm else None


def comprovar_donos_raiz(debugfs, saida, inicio, conteudo):
    """D378: /, /lib, /apps, /apps/lib, el intérprete y las .so de la raíz tienen que quedar de root sin
    escritura de grupo ni de otros; si no, el kernel no carga /lib/ld-sagui.so.1 (o el ligador las rechaza)."""
    rutas = ["/", "/lib", "/apps", "/apps/lib", "/bin"]
    for pasta in ("lib", "apps/lib"):
        d = os.path.join(conteudo, pasta)
        if os.path.isdir(d):
            rutas += [f"/{pasta}/{n}" for n in sorted(os.listdir(d)) if ".so" in n]
    for rel in rutas:
        if rel != "/" and not os.path.exists(os.path.join(conteudo, rel.lstrip("/"))):
            continue
        r = donos_ext2(debugfs, saida, inicio, rel)
        if not r or r[0] != 0 or r[1] & 0o022:
            sys.exit(f"mkimagem: {rel} no quedó de root sin escritura de otros en la raíz (uid/modo: {r}); "
                     "no se genera la imagen")


def formatar_particao(saida, inicio, mb, fs, etiqueta, ponto, mke2fs, mkfs_vfat, mcopy, conteudo_pronto=None):
    """Formatea una partición dentro del archivo (desplazamiento en sectores) y copia su leeme.txt.
    Con conteudo_pronto (solo ext2, las particiones del sistema: juegos, D181, y aplicaciones,
    D194) copia esa carpeta en vez del leeme.txt, con UUID y semilla fijos para que dos
    compilaciones den la misma tabla.
    Devuelve el tipo MBR."""
    qtd = mb * 2048
    with open(saida, "r+b") as f:
        f.truncate(max(os.path.getsize(saida), (inicio + qtd) * SETOR))
    with tempfile.TemporaryDirectory() as tmp:
        uid, gid = dono_da_particao(ponto)
        if fs == "ext2" and conteudo_pronto:
            uuid, semente = UUIDS_SISTEMA[etiqueta]
            subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-L", etiqueta, "-m", "0", "-b", "1024",
                            "-U", uuid, "-d", conteudo_pronto,
                            "-E", f"offset={inicio * SETOR},hash_seed={semente},root_owner={uid}:{gid},"
                                  "lazy_itable_init=0,nodiscard", saida, f"{mb * 1024}k"],
                           check=True, stdout=subprocess.DEVNULL,
                           env=dict(os.environ, E2FSPROGS_FAKE_TIME=os.environ.get("SOURCE_DATE_EPOCH", "1762905600")))
            normalizar_donos(saida, inicio, conteudo_pronto, uid, gid, ponto)
            return 0x83
        if fs == "ext2":
            conteudo = os.path.join(tmp, "conteudo")
            os.makedirs(conteudo)
            with open(os.path.join(conteudo, "leeme.txt"), "w", encoding="utf-8") as g:
                g.write(texto_leeme(ponto, "ext2"))
            subprocess.run([mke2fs, "-q", "-F", "-t", "ext2", "-L", etiqueta, "-m", "0", "-d", conteudo,
                            "-E", f"offset={inicio * SETOR},root_owner={uid}:{gid},lazy_itable_init=1,nodiscard",
                            saida, f"{mb * 1024}k"],
                           check=True, stdout=subprocess.DEVNULL)
            normalizar_donos(saida, inicio, conteudo, uid, gid, ponto)
            return 0x83
        leeme = os.path.join(tmp, "leeme.txt")
        with open(leeme, "w", encoding="latin-1", newline="\r\n") as g:
            g.write(texto_leeme(ponto, "FAT32") + "Se ve también desde Windows.\n")
        vol = fat32_com_margem(tmp, qtd, etiqueta, mkfs_vfat)
        if vol:
            # D213: el volumen hecho aparte (con la FAT dimensionada para crecer) se copia en su sitio.
            subprocess.run([mcopy, "-i", vol, leeme, "::/leeme.txt"], check=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
            copiar_disperso(vol, saida, inicio * SETOR)
            return 0x0C
        subprocess.run([mkfs_vfat, "-F", "32", "-n", etiqueta, "--offset", str(inicio), saida, str(qtd // 2)],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run([mcopy, "-i", f"{saida}@@{inicio * SETOR}", leeme, "::/leeme.txt"], check=True,
                       env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
        return 0x0C


# D213: la FAT de las particiones FAT32 de las imágenes, con sitio para que el volumen crezca
# (Discos > Particiones > Tamaño, o "particiones redimensionar") hasta FAT_MARGEM veces su
# tamaño: crecer un FAT32 sin mover nada solo se puede hasta donde dé su FAT. Cuesta poco
# (con clústeres de 4 KB, un 0,2 % del volumen por cada vez) y fsck.vfat y Windows lo aceptan:
# el número de clústeres sale del tamaño del volumen, y la FAT solo tiene que tener sitio.
FAT_MARGEM = 4


def fat32_com_margem(tmp, qtd, etiqueta, mkfs_vfat):
    """Un FAT32 de qtd sectores con la FAT de uno FAT_MARGEM veces mayor (mismo tamaño de
    clúster que elegiría mkfs.vfat), en un archivo aparte. None si no se puede (muy pequeño)."""
    real = os.path.join(tmp, "fat-real.img")
    with open(real, "wb") as f:
        f.truncate(qtd * SETOR)
    subprocess.run([mkfs_vfat, "-F", "32", "-n", etiqueta, real], check=True, stdout=subprocess.DEVNULL)
    with open(real, "rb") as f:
        spc = f.read(512)[13]
    os.remove(real)
    vol = os.path.join(tmp, "fat-margem.img")
    with open(vol, "wb") as f:
        f.truncate(qtd * SETOR * FAT_MARGEM)
    subprocess.run([mkfs_vfat, "-F", "32", "-n", etiqueta, "-s", str(spc), vol], check=True, stdout=subprocess.DEVNULL)
    with open(vol, "r+b") as f:
        bs = bytearray(f.read(512))
        rsvd, nf, fatsz = struct.unpack("<H", bs[14:16])[0], bs[16], struct.unpack("<I", bs[36:40])[0]
        if bs[13] != spc:
            return None
        clusters = (qtd - rsvd - nf * fatsz) // spc
        if clusters < 65525:
            return None
        bs[32:36] = struct.pack("<I", qtd)
        for setor in (0, 6):
            f.seek(setor * SETOR)
            f.write(bs)
        fsinfo = struct.unpack("<H", bs[48:50])[0]
        for setor in (fsinfo, fsinfo + 6):
            f.seek(setor * SETOR)
            fi = bytearray(f.read(512))
            if fi[0:4] == b"RRaA":
                fi[488:492] = struct.pack("<I", clusters - 1)   # libres: todos menos la raíz
                f.seek(setor * SETOR)
                f.write(fi)
        f.truncate(qtd * SETOR)
    return vol


def copiar_disperso(origem, destino, desl):
    """Copia un archivo dentro de otro a partir de desl, sin escribir los trozos a cero
    (la imagen sigue dispersa)."""
    with open(origem, "rb") as o, open(destino, "r+b") as d:
        pos = 0
        while True:
            bloco = o.read(1024 * 1024)
            if not bloco:
                break
            if bloco.count(0) != len(bloco):
                d.seek(desl + pos)
                d.write(bloco)
            pos += len(bloco)


def entrada_mbr(tipo, inicio, qtd):
    return bytes([0x00]) + chs(inicio) + bytes([tipo]) + chs(inicio + qtd - 1) + struct.pack("<II", inicio, qtd)


def acrescentar_particoes(saida, tam, lista, mke2fs, mkfs_vfat, esquema="logicas", conteudos=None):
    """Particiones de datos (D040, D120): una por elemento de lista = (etiqueta, MB, sistema,
    carpeta). Cada una alineada en 1 MB tras la anterior, tipo 0x83 (ext2) o 0x0C (FAT32 LBA).
    Se formatea directo en el archivo, con desplazamiento: queda disperso (una imagen de
    varios GB ocupa unos pocos MB en el PC).
    D213 (v7.3), esquema "logicas" (por defecto): todas son lógicas dentro de una extendida
    (tipo 0x0F) que empieza tras la de SaguiOS y llega al final de la imagen; cada lógica va tras
    su EBR (1 MB). La de SaguiOS es la única primaria: así queda sitio en la tabla y en la
    extendida para crear y redimensionar particiones desde SaguiOS (Discos > Particiones).
    Esquema "primarias" (el de antes): primarias 2 a 4 (máximo 3 de datos; con más, "mixto").
    Esquema "mixto" (el antiguo --extendida, D120): la primera de datos es primaria y el resto,
    lógicas en una extendida."""
    if not lista:
        return
    if esquema == "primarias" and len(lista) > 3:
        esquema = "mixto"
    if len(lista) > 16 + (1 if esquema == "mixto" else 0):
        sys.exit("mkimagem: demasiadas particiones (máximo 16 lógicas)")
    mke2fs = mke2fs if os.path.exists(mke2fs) else achar_ferramenta("mke2fs")
    if any(t[2] == "ext2" for t in lista):
        mke2fs = exigir_ferramenta(mke2fs, "mke2fs", "e2fsprogs")
    mcopy = None
    if any(t[2] == "fat32" for t in lista):
        mkfs_vfat = exigir_ferramenta(mkfs_vfat, "mkfs.vfat", "dosfstools")
        mcopy = exigir_ferramenta(achar_ferramenta("mcopy"), "mcopy", "mtools")
    for etiqueta, mb, fs, ponto in lista:
        if fs == "fat32" and mb < 33:
            sys.exit(f"mkimagem: FAT32 precisa de pelo menos 33 MB ({etiqueta} tem {mb})")
        if fs == "ext2" and mb < 2:
            sys.exit(f"mkimagem: ext2 precisa de pelo menos 2 MB ({etiqueta} tem {mb})")
    inicio = (tam // SETOR + 2047) // 2048 * 2048
    entradas = []
    primarias = {"logicas": [], "primarias": lista, "mixto": lista[:1]}[esquema]
    logicas = lista[len(primarias):]
    with open(saida, "r+b") as f:
        f.truncate(tam)
    for etiqueta, mb, fs, ponto in primarias:
        tipo = formatar_particao(saida, inicio, mb, fs, etiqueta, ponto, mke2fs, mkfs_vfat, mcopy,
                                 (conteudos or {}).get(etiqueta))
        entradas.append(entrada_mbr(tipo, inicio, mb * 2048))
        print(f"mkimagem: partição {etiqueta} ({fs}) de {mb} MB no LBA {inicio}, para {ponto}")
        inicio += mb * 2048
    if logicas:
        ext = inicio
        ebr = ext
        for k, (etiqueta, mb, fs, ponto) in enumerate(logicas):
            qtd = mb * 2048
            tipo = formatar_particao(saida, ebr + 2048, mb, fs, etiqueta, ponto, mke2fs, mkfs_vfat, mcopy,
                                     (conteudos or {}).get(etiqueta))
            sector = bytearray(SETOR)
            sector[446:462] = entrada_mbr(tipo, ebr + 2048, qtd)[:8] + struct.pack("<II", 2048, qtd)
            proximo = ebr + 2048 + qtd
            if k + 1 < len(logicas):
                tam_seg = 2048 + logicas[k + 1][1] * 2048
                sector[462:478] = bytes([0]) + chs(proximo) + bytes([0x05]) + chs(proximo + tam_seg - 1) + \
                    struct.pack("<II", proximo - ext, tam_seg)
            sector[510:512] = b"\x55\xAA"
            with open(saida, "r+b") as f:
                f.seek(ebr * SETOR)
                f.write(sector)
            print(f"mkimagem: partição lógica {etiqueta} ({fs}) de {mb} MB no LBA {ebr + 2048} (EBR {ebr}), para {ponto}")
            ebr = proximo
        entradas.append(entrada_mbr(0x0F, ext, ebr - ext))
        print(f"mkimagem: partição extendida de {(ebr - ext) // 2048} MB no LBA {ext} con {len(logicas)} lógicas")
    with open(saida, "r+b") as f:
        for k, e in enumerate(entradas):
            f.seek(SLOT_DADOS + 16 * k)
            f.write(e)


if __name__ == "__main__":
    main()
