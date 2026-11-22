#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
diagramas_so.py - dibuja los diagramas del curso «Fundamentos de Sistemas Operativos» de la Ayuda.

Uso: tools/diagramas_so.py [carpeta_de_salida]     (por defecto rootfs/ayuda/sistemas-operativos/img)

Es repetible y no necesita nada más que Python: dibuja con la fuente 8x16 del propio sistema
(tools/fonte/sagui8x16.txt, la misma de la terminal y de la Ayuda), rectángulos, líneas y
flechas, y graba PNG de 760 px de ancho (RGB, filtro «Up» y deflate al máximo; son colores
planos y salen de pocos KB). Los dibujos son propios y describen SaguiOS; el texto va en
español (Latin-1 en la fuente, por eso no hay flechas Unicode: las flechas se dibujan).

Colores (coherentes en todos): verde = kernel, azul = programas de usuario, naranja =
arranque/ cargador, gris = hardware y firmware, blanco con borde = conceptos.
"""
import os
import struct
import sys
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import mkfonte  # noqa: E402

W = 760


def cargar_fuente():
    blocos = mkfonte.ler(os.path.join(AQUI, "fonte", "sagui8x16.txt"))
    caixa = mkfonte.celula(blocos["caixa"])
    f = [list(caixa) for _ in range(256)]
    f[0x20] = f[0xA0] = mkfonte.celula(blocos["espaco"])
    for nome, bloco in blocos.items():
        if len(nome) == 1:
            f[nome.encode("latin-1")[0]] = mkfonte.celula(bloco)
    for cod, (base, ac) in mkfonte.COMPOSTAS.items():
        f[cod] = mkfonte.compor(blocos, base, ac)
    return f


FONTE = cargar_fuente()


def rgb(h):
    return bytes(((h >> 16) & 255, (h >> 8) & 255, h & 255))


FONDO = 0xFFFFFF
TEXTO = 0x1F2933
SUAVE = 0x52606D
# estilo: (relleno, borde)
ESTILOS = {
    "k": (0xDFF1E5, 0x2E7D4F),    # kernel
    "u": (0xE3ECF8, 0x2B5FA8),    # programas de usuario
    "b": (0xFBE9D2, 0xB5651D),    # arranque, cargador
    "h": (0xE6E9ED, 0x5B6672),    # hardware y firmware
    "n": (0xFFFFFF, 0x7B8794),    # concepto neutro
    "a": (0xFFF4C2, 0xA38400),    # destacado (amarillo sobrio)
    "r": (0xF9DEDC, 0xB3362B),    # peligro, excepción
}
FLECHA = 0x3E4C59


class Lienzo:
    def __init__(self, alto, titulo=None, sub=None):
        self.w, self.h = W, alto
        self.px = bytearray(rgb(FONDO) * (W * alto))
        if titulo:
            self.rect(0, 0, W, 38, 0x14532D)
            self.texto(14, 3, titulo, 0xFFFFFF, 2, negrita=True)
        if sub:
            self.texto(14, 44, sub, SUAVE)

    # -- primitivas
    def rect(self, x, y, w, h, color):
        x0, y0, x1, y1 = max(0, x), max(0, y), min(self.w, x + w), min(self.h, y + h)
        if x1 <= x0 or y1 <= y0:
            return
        fila = rgb(color) * (x1 - x0)
        for yy in range(y0, y1):
            o = (yy * self.w + x0) * 3
            self.px[o:o + len(fila)] = fila

    def pt(self, x, y, color):
        if 0 <= x < self.w and 0 <= y < self.h:
            o = (y * self.w + x) * 3
            self.px[o:o + 3] = rgb(color)

    def caja(self, x, y, w, h, estilo="n", lineas=None, color=TEXTO, esc=1, negrita=False, alto_ficha=0):
        fondo, borde = ESTILOS[estilo]
        self.rect(x, y, w, h, borde)
        self.rect(x + 2, y + 2, w - 4, h - 4, fondo)
        for cx, cy in ((x, y), (x + w - 1, y), (x, y + h - 1), (x + w - 1, y + h - 1)):
            self.pt(cx, cy, FONDO)
        if lineas:
            if isinstance(lineas, str):
                lineas = lineas.split("\n")
            for l in lineas:
                if len(l) * 8 * esc > w - 6:
                    print("AVISO: texto ancho en caja: %r (%d > %d)" % (l, len(l) * 8 * esc, w - 6))
            tot = len(lineas) * 16 * esc + (len(lineas) - 1) * 2
            ty = y + (h - tot) // 2
            for i, l in enumerate(lineas):
                neg = negrita and i == 0
                self.texto(x + (w - len(l) * 8 * esc) // 2, ty + i * (16 * esc + 2), l, color, esc, negrita=neg)

    def texto(self, x, y, s, color=TEXTO, esc=1, negrita=False):
        data = s.encode("latin-1", "replace")
        if x + len(data) * 8 * esc > self.w:
            print("AVISO: texto fuera del lienzo: %r" % s)
        c = rgb(color)
        for k, ch in enumerate(data):
            g = FONTE[ch]
            for fila in range(16):
                m = g[fila] << 1
                if negrita:
                    m |= g[fila]
                if not m:
                    continue
                for col in range(9):
                    if m & (0x100 >> col):
                        bx, by = x + (k * 8 + col) * esc, y + fila * esc
                        for a in range(esc):
                            for b in range(esc):
                                self.pt(bx + b, by + a, color)

    def texto_c(self, cx, y, s, color=TEXTO, esc=1, negrita=False):
        self.texto(cx - len(s) * 8 * esc // 2, y, s, color, esc, negrita)

    def lineas_texto(self, x, y, lineas, color=TEXTO, paso=18):
        for i, l in enumerate(lineas):
            self.texto(x, y + i * paso, l, color)

    def linea(self, x0, y0, x1, y1, color=FLECHA, grueso=2, trazos=0):
        """Línea horizontal o vertical (los diagramas son ortogonales)."""
        if x0 == x1:
            ya, yb = sorted((y0, y1))
            self._tramo(x0, ya, grueso, yb - ya + 1, True, color, trazos)
        else:
            xa, xb = sorted((x0, x1))
            self._tramo(xa, y0, xb - xa + 1, grueso, False, color, trazos)

    def _tramo(self, x, y, w, h, vertical, color, trazos):
        if not trazos:
            self.rect(x - (0 if vertical else 0), y, w, h, color)
            return
        largo = h if vertical else w
        p = 0
        while p < largo:
            n = min(trazos, largo - p)
            if vertical:
                self.rect(x, y + p, w, n, color)
            else:
                self.rect(x + p, y, n, h, color)
            p += trazos * 2

    def punta(self, x, y, dir_, color=FLECHA, t=7):
        """Cabeza de flecha con la punta en (x, y); dir_ = u, d, l, r (hacia dónde apunta)."""
        for i in range(t + 1):
            m = t - i   # media anchura: la punta (en x, y) es el extremo estrecho
            if dir_ == "d":
                self.rect(x - m, y - t + i, 2 * m + 1, 1, color)
            elif dir_ == "u":
                self.rect(x - m, y + t - i, 2 * m + 1, 1, color)
            elif dir_ == "r":
                self.rect(x - t + i, y - m, 1, 2 * m + 1, color)
            else:
                self.rect(x + t - i, y - m, 1, 2 * m + 1, color)

    def flecha(self, *pts, color=FLECHA, trazos=0, doble=False):
        """Poligonal ortogonal con punta al final (y al principio si doble)."""
        for (a, b), (c, d) in zip(pts, pts[1:]):
            self.linea(a, b, c, d, color, 2, trazos)
        (a, b), (c, d) = pts[-2], pts[-1]
        self.punta(c, d, "d" if d > b else "u" if d < b else "r" if c > a else "l", color)
        if doble:
            (a, b), (c, d) = pts[1], pts[0]
            self.punta(c, d, "d" if d > b else "u" if d < b else "r" if c > a else "l", color)

    def leyenda(self, y, items):
        x = 14
        for est, txt in items:
            f, b = ESTILOS[est]
            self.rect(x, y, 18, 14, b)
            self.rect(x + 2, y + 2, 14, 10, f)
            self.texto(x + 24, y - 1, txt, SUAVE)
            x += 24 + len(txt) * 8 + 22

    # -- salida
    def guardar(self, ruta):
        return escribir_png(ruta, self.w, self.h, bytes(self.px))


def escribir_png(ruta, w, h, rgb_bytes):
    """PNG RGB de 8 bits, filtro «Up» y deflate al máximo (también lo usa tools/capturas_curso.py)."""
    crudo = bytearray()
    fila_ant = bytes(w * 3)
    for y in range(h):
        fila = bytes(rgb_bytes[y * w * 3:(y + 1) * w * 3])
        crudo.append(2)
        crudo += bytes((a - b) & 255 for a, b in zip(fila, fila_ant))
        fila_ant = fila

    def trozo(tipo, datos):
        c = struct.pack(">I", len(datos)) + tipo + datos
        return c + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += trozo(b"IDAT", zlib.compress(bytes(crudo), 9)) + trozo(b"IEND", b"")
    with open(ruta, "wb") as f:
        f.write(png)
    return len(png)


# ---------------------------------------------------------------- los diagramas

def arranque():
    d = Lienzo(612, "Arranque de SaguiOS", "Cada etapa carga la siguiente. Izquierda: qué es; derecha: dónde está en el código.")
    pasos = [
        ("h", "Firmware: BIOS o UEFI", ["BIOS: copia el primer sector a 0x7C00 y salta.", "UEFI: ejecuta el cargador propio (boot/uefi/cargador.c)."]),
        ("b", "Etapa 1: MBR", ["boot/mbr.asm, 512 bytes, modo real de 16 bits.", "Busca la cabecera de SaguiOS (LBA 1)."]),
        ("b", "Etapa 2", ["boot/estagio2.asm: A20, mapa E820, copia el", "kernel sobre 1 MB y pasa a modo protegido (32 bits)."]),
        ("k", "Kernel", ["kernel/nucleo/principal.c: GDT, IDT, memoria,", "paginación (clásica o PAE), reloj, teclado, discos."]),
        ("b", "Ramdisk (initramfs)", ["Lo mínimo en RAM, formato SGFS (kernel/fs/sgfs.c)."]),
        ("k", "Raíz SAGUI-RAIZ", ["El sistema de archivos real, en un disco (D379)."]),
        ("u", "Servicios", ["El gestor de servicios (kernel/nucleo/supervisor.c)", "arranca los programas en segundo plano."]),
        ("u", "Sesión gráfica", ["kernel/gui: inicio de sesión, escritorio y barra."]),
    ]
    x, w, h, paso, y0 = 24, 250, 44, 69, 72
    for i, (est, nombre, notas) in enumerate(pasos):
        y = y0 + i * paso
        d.caja(x, y, w, h, est, nombre, negrita=True)
        if i:
            d.flecha((x + w // 2, y - (paso - h)), (x + w // 2, y - 1))
        d.lineas_texto(x + w + 24, y + (h - len(notas) * 18) // 2 + 1, notas, TEXTO)
    d.leyenda(y0 + 8 * paso - 12, [("h", "firmware"), ("b", "cargador"), ("k", "kernel"), ("u", "usuario")])
    return d


def anillos():
    d = Lienzo(600, "Anillos de protección y llamadas", "El procesador x86 separa lo que puede tocar todo (anillo 0) de lo que no (anillo 3).")
    d.rect(20, 66, 720, 156, 0x2B5FA8)
    d.rect(22, 68, 716, 152, 0xF2F6FC)
    d.texto(34, 74, "Anillo 3: programas (modo usuario)", 0x2B5FA8, 1, True)
    for i, n in enumerate(["Terminal", "Archivos", "Ayuda", "Juegos"]):
        d.caja(60 + i * 110, 100, 98, 36, "u", n)
    d.caja(60, 160, 440, 48, "u", ["libsagui (userland/libsagui/chamadas.c)", "EAX = nº de llamada, EBX..EDI = argumentos"])
    d.caja(540, 150, 180, 58, "r", ["Excepción del", "programa (división", "por 0, acceso malo)"])
    d.rect(20, 296, 720, 292, 0x2E7D4F)
    d.rect(22, 298, 716, 288, 0xF1F8F3)
    d.texto(34, 304, "Anillo 0: kernel", 0x2E7D4F, 1, True)
    d.caja(40, 330, 680, 56, "k", ["IDT (kernel/arch/idt.c) y tratar_interrupcao(), el despachador general", "un vector por causa: llamadas (0x80), excepciones del procesador e IRQ"])
    d.caja(40, 420, 330, 70, "k", ["Tabla de llamadas (nucleo/sistema.c)", "tabela[n](ebx, ecx, edx, esi, edi)", "SIS_ABRIR, SIS_LER, SIS_FUTEX_ESPERAR..."])
    d.caja(410, 420, 310, 70, "k", ["Servicios del kernel", "procesos y planificador, memoria,", "VFS y drivers, red, ventanas"])
    d.texto(40, 508, "GDT: segmentos de código y datos de cada anillo. TSS: la pila del kernel al entrar.", SUAVE)
    d.texto(40, 528, "Cada llamada comprueba el uid/gid del proceso (usuarios.c, permisos.c).", SUAVE)
    d.texto(40, 548, "El hardware avisa con IRQ; el programa nunca toca el hardware directamente.", SUAVE)
    d.flecha((330, 209), (330, 329))
    d.texto(342, 232, "int 0x80", FLECHA, 1, True)
    d.texto(342, 250, "(entra en el anillo 0)", SUAVE)
    d.flecha((580, 209), (580, 329))
    d.texto(592, 232, "excepción", FLECHA, 1, True)
    d.flecha((205, 387), (205, 419))
    d.flecha((371, 455), (409, 455), doble=True)
    d.caja(600, 254, 126, 30, "h", "Hardware: IRQ")
    d.flecha((663, 285), (663, 329), color=0x5B6672)
    d.flecha((40, 455), (30, 455), (30, 184), (59, 184), color=0x2E7D4F)
    d.texto(44, 232, "iret: vuelve al", 0x2E7D4F, 1, True)
    d.texto(44, 250, "anillo 3 con EAX", 0x2E7D4F, 1, True)
    return d


def procesos_estados():
    d = Lienzo(520, "Estados de un proceso o hilo", "Los cinco estados de processo.h (P_LIVRE ... P_ZUMBI). Un hilo pasa por los mismos.")
    d.caja(24, 120, 170, 50, "n", ["Nuevo o libre", "(P_LIVRE)"], negrita=True)
    d.caja(300, 120, 170, 50, "a", ["Listo", "(P_PRONTO)"], negrita=True)
    d.caja(566, 120, 170, 50, "k", ["Ejecutando", "(P_EXECUTANDO)"], negrita=True)
    d.caja(300, 300, 170, 50, "u", ["Bloqueado", "(P_BLOQUEADO)"], negrita=True)
    d.caja(300, 430, 170, 50, "r", ["Terminado: zombi", "(P_ZUMBI)"], negrita=True)
    d.flecha((194, 145), (299, 145))
    d.texto_c(247, 80, "fork / crear hilo", FLECHA, 1, True)
    d.texto_c(247, 98, "(copia al escribir)", SUAVE)
    d.flecha((470, 135), (565, 135))
    d.texto_c(518, 80, "el planificador", FLECHA, 1, True)
    d.texto_c(518, 98, "le da la CPU", FLECHA, 1, True)
    d.flecha((565, 155), (471, 155))
    d.texto_c(518, 178, "fin del quantum", FLECHA, 1, True)
    d.texto_c(518, 196, "(tic del reloj)", SUAVE)
    d.flecha((690, 171), (690, 325), (471, 325))
    d.texto(520, 258, "espera E/S, dormir,", FLECHA, 1, True)
    d.texto(520, 276, "futex, wait, pipe", FLECHA, 1, True)
    d.flecha((385, 299), (385, 171))
    d.texto(398, 214, "llega el dato,", FLECHA, 1, True)
    d.texto(398, 232, "futex acordar,", FLECHA, 1, True)
    d.texto(398, 250, "señal", FLECHA, 1, True)
    d.flecha((722, 171), (722, 455), (471, 455))
    d.texto(540, 384, "exit, o una señal", FLECHA, 1, True)
    d.texto(540, 402, "que lo termina", FLECHA, 1, True)
    d.flecha((299, 455), (109, 455), (109, 171))
    d.texto(120, 330, "el padre hace wait:", FLECHA, 1, True)
    d.texto(120, 348, "recoge el estado y", FLECHA, 1, True)
    d.texto(120, 366, "libera la entrada", FLECHA, 1, True)
    d.texto(24, 494, "exec no cambia de estado: el proceso sigue ejecutando, pero con otro programa.", SUAVE)
    return d


def planificador():
    d = Lienzo(600, "Planificador", "Quién usa la CPU, cuánto tiempo y en cuál. Código: nucleo/processos.c y planificadores.c.")
    d.caja(24, 70, 230, 56, "h", ["Temporizador", "tic de 10 ms (IRQ del reloj)"], negrita=False)
    d.caja(300, 70, 436, 56, "k", ["escalonar() (processos.c)", "si se acabó la rebanada, cambia de proceso"])
    d.flecha((254, 98), (299, 98))
    d.caja(24, 170, 330, 160, "a", None)
    d.texto(34, 178, "Cola de listos", TEXTO, 1, True)
    d.texto(34, 198, "procesos P_PRONTO de la tabla,", TEXTO)
    d.texto(34, 216, "cada uno con su CPU «casa»", TEXTO)
    for i, n in enumerate(["P7", "P3", "P12", "P5"]):
        d.caja(40 + i * 76, 242, 64, 34, "n", n)
        if i:
            d.flecha((40 + i * 76 - 12, 259), (40 + i * 76 - 1, 259), color=SUAVE)
    d.texto(34, 288, "PLAN_AUDIO: el hilo de audio", 0x7A6400, 1, True)
    d.texto(34, 306, "pasa delante si gasta poco", 0x7A6400, 1, True)
    d.flecha((330, 127), (330, 169))
    d.texto(342, 140, "toma el siguiente listo", FLECHA)
    d.caja(400, 170, 336, 160, "n", None)
    d.texto(410, 178, "Políticas (PLAN_ESCOLHER):", TEXTO, 1, True)
    d.lineas_texto(410, 204, ["rr: por turnos, la de siempre", "afinidad: vuelve a su CPU", "equilibrio: una cola por CPU", "prioridad: nice -20..19 y", "  uso reciente de CPU"], TEXTO, 18)
    d.texto(24, 348, "SMP (opcional, imagen --smp): cada CPU atiende sus llamadas y su tic (D164, D165)", TEXTO, 1, True)
    d.caja(24, 378, 220, 92, "k", ["CPU 0", "kernel, PIT y tareas del", "kernel; ejecuta procesos", "como las demás"])
    for i, n in enumerate(["CPU 1", "CPU 2"]):
        d.caja(262 + i * 162, 378, 150, 92, "u", [n, "procesos e hilos;", "su tic y sus", "llamadas; IRQ"])
    d.caja(586, 378, 150, 92, "n", ["CPU n...", "las que tenga", "el PC"])
    d.rect(24, 492, 712, 36, 0x2E7D4F)
    d.rect(26, 494, 708, 32, 0xDFF1E5)
    d.texto_c(380, 502, "Cerrojo general (D165): entran al kernel de una en una, por turnos (D169)", TEXTO, 1, True)
    for cx in (134, 337, 499, 661):
        d.flecha((cx, 471), (cx, 491), color=0x2E7D4F)
    d.texto(24, 546, "La rebanada son 2 tics de 10 ms (20 ms); quien duerme pronto (interactivo) gana prioridad.", SUAVE)
    d.texto(24, 566, "Afinidad y nice se cambian en caliente (PLAN_AFINIDADE, PLAN_PRIORIDADE).", SUAVE)
    return d


def paginacion():
    d = Lienzo(590, "Paginación y memoria virtual", "Páginas de 4 KB. Modo clásico (2 niveles) o PAE (3 niveles): kernel/mem/paginacao.c.")
    d.texto(24, 70, "Dirección virtual de 32 bits (modo clásico)", TEXTO, 1, True)
    d.caja(24, 96, 240, 40, "u", "directorio (10 bits)")
    d.caja(264, 96, 240, 40, "k", "tabla (10 bits)")
    d.caja(504, 96, 232, 40, "a", "desplazamiento (12 bits)")
    d.texto(24, 142, "bits 31..22", SUAVE)
    d.texto(264, 142, "bits 21..12", SUAVE)
    d.texto(504, 142, "bits 11..0", SUAVE)
    d.caja(24, 200, 150, 54, "h", ["Registro CR3", "(uno por proceso)"])
    d.caja(240, 184, 150, 86, "u", ["Directorio de", "páginas", "1024 entradas"])
    d.caja(450, 184, 130, 86, "k", ["Tabla de", "páginas", "1024 entradas"])
    d.caja(620, 184, 116, 86, "a", ["Marco", "físico", "de 4 KB"])
    d.flecha((174, 227), (239, 227))
    d.flecha((390, 227), (449, 227))
    d.flecha((580, 227), (619, 227))
    d.flecha((144, 137), (144, 170), (315, 170), (315, 183))
    d.flecha((384, 137), (384, 160), (515, 160), (515, 183))
    d.flecha((620, 137), (678, 137), (678, 183))
    d.texto(24, 284, "Cada entrada lleva: presente, escritura, usuario, NX (con PAE), acceso y sucia.", SUAVE)
    d.caja(24, 322, 346, 100, "h", None)
    d.texto(34, 330, "TLB (en el procesador)", TEXTO, 1, True)
    d.lineas_texto(34, 352, ["Guarda las traducciones recientes:", "si acierta, no se recorren las tablas.", "Al cambiar de proceso (CR3) se limpia."], TEXTO, 18)
    d.caja(384, 322, 352, 100, "r", None)
    d.texto(394, 330, "Fallo de página (excepción)", TEXTO, 1, True)
    d.lineas_texto(394, 352, ["No presente o sin permiso: el kernel", "carga la página, la saca de la swap", "comprimida o termina el programa."], TEXTO, 18)
    d.texto(24, 440, "Copia al escribir del fork (kernel/mem/copia_escrita.c)", TEXTO, 1, True)
    d.caja(24, 470, 130, 44, "u", ["Proceso padre", "tabla A"])
    d.caja(24, 530, 130, 44, "u", ["Proceso hijo", "tabla B"])
    d.caja(300, 484, 150, 76, "a", ["Mismo marco", "(solo lectura)"])
    d.flecha((154, 492), (299, 492))
    d.flecha((154, 552), (299, 552))
    d.flecha((450, 498), (519, 498))
    d.caja(520, 470, 216, 56, "k", ["Al escribir uno: fallo,", "se copia el marco y", "pasa a ser suyo"])
    d.caja(520, 538, 216, 36, "a", ["Cada uno con su marco"])
    return d


def mapa_memoria():
    d = Lienzo(592, "Espacio de direcciones de un proceso", "32 bits; arriba, las direcciones altas. El mapa completo: kernel/inc/paginacao.h (D049).")
    x, w = 24, 330
    tramos = [
        (0xFFC00000, "APIC e IOAPIC (solo kernel)", 34, "k"),
        (0xFF400000, "Pila del hilo principal (8 MB)", 52, "u"),
        (0xFF000000, "Guarda de la pila (sin mapa)", 30, "h"),
        (0xF0000000, "Mapas de dispositivos (BAR)", 36, "u"),
        (0xE0000000, "Píxeles de las ventanas", 40, "u"),
        (0xDC000000, "Pilas de los hilos (64 x 1 MB)", 40, "u"),
        (0x80000000, "Programa: código, datos,", 90, "u"),
        (0x78000000, "Ventanas del kernel (vídeo, USB)", 40, "k"),
        (0x00000000, "Kernel: RAM mapeada 1:1 con la física", 80, "k"),
    ]
    y = 70
    for dir_, nombre, alto, est in tramos:
        d.caja(x, y, w, alto, est, nombre if dir_ != 0x80000000 else ["Programa: código, datos,", "BSS, montón y bibliotecas"])
        d.texto(x + w + 10, y + alto - 16, "0x%08X" % dir_, SUAVE)
        y += alto + 4
    d.rect(452, 70, 2, y - 74, 0xC9CED4)
    dx = 480
    d.texto(dx, 72, "Dentro de «Programa»:", TEXTO, 1, True)
    yy = 98
    for est, n in [("a", "montón (crece hacia arriba)"), ("u", "BSS (datos a cero)"), ("u", "datos (.data)"), ("u", "código (.text, ELF32)")]:
        d.caja(dx, yy, 256, 34, est, n)
        yy += 38
    d.lineas_texto(dx, yy + 6, ["Las bibliotecas compartidas", "tienen una sola copia física", "(kernel/mem/imagens.c)."], TEXTO, 18)
    d.lineas_texto(dx, yy + 78, ["Kernel y programa comparten", "un espacio, pero las páginas", "de usuario llevan el bit U:", "el programa no puede tocar", "las que no lo llevan."], TEXTO, 18)
    d.lineas_texto(dx, yy + 184, ["La pila crece sola al tocarla."], TEXTO, 18)
    d.leyenda(y + 6, [("u", "usuario"), ("k", "kernel"), ("h", "sin mapa"), ("a", "dinámico")])
    return d


def vfs():
    d = Lienzo(590, "Archivos: del programa al disco", "Cada capa solo habla con la de abajo. Código en kernel/fs/ y kernel/dispositivos/.")
    d.caja(24, 64, 712, 40, "u", ["Programas: abrir, ler, escrever, listar... (libsagui)"])
    d.flecha((380, 105), (380, 126))
    d.caja(24, 128, 712, 40, "k", ["Llamadas al sistema (kernel/nucleo/sistema.c, kernel/fs/arquivos.c)"])
    d.flecha((380, 169), (380, 190))
    d.caja(24, 192, 712, 44, "k", ["VFS (kernel/fs/vfs.c, montagem.c): rutas, puntos de montaje, descriptores", "elige el sistema de archivos según dónde esté montada la ruta"])
    nombres = [("ext2/3/4", "ext2.c"), ("FAT32", "fat32.c"), ("exFAT", "exfat.c"), ("XFS (ro)", "xfs.c"), ("SGFS", "sgfs.c"), ("ISO 9660", "iso9660.c"), ("UDF", "udf.c")]
    for i, (n, f) in enumerate(nombres):
        x = 24 + i * 102
        d.flecha((x + 47, 237), (x + 47, 258))
        d.caja(x, 260, 96, 52, "k", [n, f])
    d.texto(24, 320, "También: ramfs, pipes, CD de audio (cdda.c) y sobreposicao.c. XFS solo se lee (D446).", SUAVE)
    d.flecha((380, 342), (380, 362))
    d.caja(24, 364, 712, 44, "k", ["Caché de bloques (kernel/fs/cache_disco.c, blocos.c)", "guarda sectores recientes en RAM; discos.c reparte por particiones"])
    d.flecha((380, 409), (380, 430))
    d.caja(24, 432, 712, 34, "k", ["Capa de discos: lectura y escritura de sectores (LBA)"])
    for i, (n, f) in enumerate([("AHCI (SATA)", "ahci.c"), ("NVMe", "nvme.c"), ("ATA / ATAPI", "ata.c, atapi.c"), ("USB", "usb/armazenamento.c")]):
        x = 24 + i * 180
        d.flecha((x + 86, 467), (x + 86, 488))
        d.caja(x, 490, 172, 48, "h", [n, f])
    d.texto(24, 554, "Cada sistema de archivos pide al caché «dame el bloque N» y no sabe qué disco hay debajo.", SUAVE)
    d.texto(24, 572, "Un mismo árbol de carpetas reúne el ramdisk, la raíz y los discos montados.", SUAVE)
    return d


def es_drivers():
    d = Lienzo(560, "Entrada y salida de un dispositivo", "El dispositivo avisa; el kernel atiende rápido y el trabajo largo va fuera de la IRQ.")
    pasos = [
        ("h", ["Dispositivo", "teclado, red,", "disco, USB"]),
        ("h", ["Controlador", "de IRQ", "PIC, APIC, MSI"]),
        ("k", ["IDT", "un vector por", "interrupción"]),
        ("k", ["Manejador", "(código del", "kernel)"]),
        ("k", ["Driver", "dispositivos/", "usb/ rede/"]),
    ]
    xs = [24, 171, 318, 465, 612]
    for (est, ls), x in zip(pasos, xs):
        d.caja(x, 70, 124, 74, est, ls)
    for x in xs[:-1]:
        d.flecha((x + 125, 107), (x + 146, 107))
    d.caja(526, 190, 210, 48, "k", ["Cola o tarea del kernel", "(trabajo largo aparte)"])
    d.flecha((674, 145), (674, 189))
    d.caja(526, 284, 210, 48, "k", ["Llamada al sistema", "SIS_LER, recibir..."])
    d.flecha((631, 239), (631, 283))
    d.caja(526, 378, 210, 48, "u", ["Programa", "recibe los datos"])
    d.flecha((631, 333), (631, 377))
    d.caja(24, 190, 480, 142, "n", None)
    d.texto(34, 198, "Qué pasa en cada paso", TEXTO, 1, True)
    d.lineas_texto(34, 222, [
        "1. El dispositivo levanta su IRQ (o envía un MSI).",
        "2. El APIC la entrega a una CPU y esta salta a la IDT",
        "   (irq_rotas.c elige la línea de PCI con ACPI _PRT).",
        "3. El manejador atiende rápido y avisa al final (EOI).",
        "4. El driver deja el dato en una cola y despierta",
        "   a quien lo espera.",
    ], TEXTO, 18)
    d.caja(24, 346, 480, 104, "a", None)
    d.texto(34, 354, "DMA (acceso directo a memoria)", TEXTO, 1, True)
    d.lineas_texto(34, 376, [
        "El driver prepara un búfer y le da su dirección física",
        "al dispositivo; este lo llena sin gastar CPU (AHCI,",
        "NVMe, red, xHCI) y avisa con una IRQ al terminar.",
    ], TEXTO, 18)
    d.texto(24, 466, "Los puertos de E/S y la memoria de los dispositivos (BAR de PCI) sirven para configurarlos.", SUAVE)
    d.texto(24, 486, "El programa de usuario nunca toca el hardware: lo pide por una llamada al sistema.", SUAVE)
    d.leyenda(522, [("h", "hardware"), ("k", "kernel"), ("u", "usuario"), ("a", "DMA")])
    return d


def pila_red():
    d = Lienzo(550, "Pila de red", "De un programa a la tarjeta. Código en kernel/rede/ (rede.c, socket.c, tcp.c y los drivers).")
    capas = [
        ("u", "Programa (navegador, ssh, ping...)", "libsagui: http.c y sockets"),
        ("k", "Sockets (socket.c)", "TCP y UDP: puertos, conexiones"),
        ("k", "TCP (tcp.c) y UDP (rede.c)", "ICMP (ping), DHCP y DNS"),
        ("k", "IPv4 (rede.c)", "direcciones, rutas, qué interfaz usar"),
        ("k", "ARP y Ethernet (rede.c)", "IP a dirección MAC, tramas"),
    ]
    y = 68
    for est, a, b in capas:
        d.caja(24, y, 400, 46, est, [a, b])
        d.flecha((224, y + 47), (224, y + 62), doble=True)
        y += 62
    d.texto(24, y - 1, "Drivers (hasta 4 interfaces a la vez, D445)", TEXTO, 1, True)
    y += 24
    for i, n in enumerate(["e1000", "RTL8169", "RTL8139", "NE2000", "Wi-Fi", "USB ure"]):
        d.caja(24 + i * 68, y, 64, 36, "k", n)
        d.flecha((56 + i * 68, y + 37), (56 + i * 68, y + 54), doble=True)
        d.caja(24 + i * 68, y + 56, 64, 34, "h", "tarjeta" if i < 5 else "USB")
    d.caja(460, 68, 276, 186, "n", None)
    d.texto(470, 76, "Varias interfaces y rutas", TEXTO, 1, True)
    d.lineas_texto(470, 100, ["Cada interfaz (eth0, eth1,", "wlan0) tiene su IP, su DHCP", "y su ARP. Las rutas salen de", "las interfaces: la red local", "de cada una y una puerta por", "defecto; gana la de menor", "métrica con enlace (cable 100,", "Wi-Fi 600). Si cae, pasa a otra."], TEXTO, 18)
    d.caja(460, 270, 276, 96, "a", None)
    d.texto(470, 278, "Además", TEXTO, 1, True)
    d.lineas_texto(470, 300, ["bloqueo.c: bloqueo de DNS", "tunel.c: túneles (wg0)", "wifi.c y ure.c: USB (tarea ure)"], TEXTO, 18)
    d.caja(460, 382, 276, 70, "h", ["Red física", "cable o aire (Wi-Fi)"])
    d.texto(24, 512, "Un paquete que llega sube por las mismas capas, en orden inverso, hasta el socket.", SUAVE)
    return d


def graficos():
    d = Lienzo(580, "Gráficos: del programa a la pantalla", "2D con CPU o GPU, 3D por software o por la GPU, y todo compuesto en ventanas.")
    d.caja(24, 64, 340, 48, "u", ["Programa", "ventana con sus propios píxeles"])
    d.caja(396, 64, 340, 48, "u", ["libsagui · SDL · SaguiGL", "botones, texto, OpenGL ES (software)"])
    d.flecha((395, 88), (365, 88), doble=True)
    d.flecha((380, 113), (380, 140))
    d.caja(24, 142, 712, 50, "k", ["Compositor de ventanas (kernel/gui/janelas.c)", "mezcla las ventanas con el escritorio y las pone en el framebuffer"])
    d.flecha((200, 193), (200, 224))
    d.flecha((560, 193), (560, 224))
    d.caja(24, 226, 340, 80, "k", ["acel2d (kernel/gui/acel2d.c)", "rellenos, copias de bloques y blits", "con el motor 2D de la GPU"])
    d.caja(396, 226, 340, 80, "k", ["gpu3d (kernel/gui/gpu3d.c)", "v1 y v2: triángulos y texturas", "v3 (D457): vértices en la memoria", "de la GPU, que los transforma"])
    d.flecha((200, 307), (200, 332))
    d.flecha((560, 307), (560, 332))
    d.caja(24, 334, 712, 54, "h", ["GPU (Intel, Radeon, VMware...) y memoria de vídeo", "drivers en kernel/dispositivos/ (acel_*.c, gpu3d_*.c)"])
    d.flecha((200, 389), (200, 416))
    d.caja(24, 418, 340, 44, "h", ["Framebuffer (video.c, bga.c)"])
    d.caja(396, 418, 340, 44, "h", ["Pantalla"])
    d.flecha((365, 440), (395, 440))
    d.rect(24, 480, 712, 70, 0xB5651D)
    d.rect(26, 482, 708, 66, 0xFBE9D2)
    d.texto(36, 488, "Vuelta a la CPU (siempre disponible)", TEXTO, 1, True)
    d.lineas_texto(36, 510, ["Sin driver, o si la GPU falla, el 2D va por la CPU y SaguiGL rasteriza el 3D.", "Si falla la autoprueba de v3, se apaga sola y el 3D sigue por v2 (la CPU transforma)."], TEXTO, 18)
    return d


def dinamico():
    d = Lienzo(560, "Enlace dinámico en /bin (D378, D462)", "Casi todo /bin carga las bibliotecas del sistema al arrancar; lo esencial va estático.")
    d.caja(24, 64, 340, 56, "u", ["Programa de /bin (159 dinámicos)", "ELF con PT_INTERP = /lib/ld-sagui.so.1"])
    d.flecha((194, 121), (194, 150))
    d.caja(24, 152, 340, 56, "k", ["Núcleo: carregador.c", "mapea los PT_LOAD y el intérprete; auxv"])
    d.flecha((194, 209), (194, 238))
    d.caja(24, 240, 340, 56, "u", ["Intérprete ld-sagui.so.1 (usuario)", "busca en /lib y /apps/lib y reubica"])
    d.flecha((194, 297), (194, 326))
    for i, (n, f) in enumerate([("libsagui.so", "base"), ("libc.so", "libc común"), ("medios.so", "audio, vídeo")]):
        d.caja(24 + i * 116, 328, 108, 56, "u", [n, f])
    d.texto(24, 392, "Código de solo lectura compartido entre procesos:", TEXTO, 1, True)
    d.texto(24, 410, "una sola copia en RAM (kernel/mem/imagens.c).", TEXTO)
    d.caja(396, 64, 340, 144, "b", None)
    d.texto(406, 72, "34 estáticos en /bin (BIN_ESTATICOS)", TEXTO, 1, True)
    d.lineas_texto(406, 96, ["los 18 del initramfs (sagsh, ls,", "mount, ps...): corren antes de", "montar la raíz y sin /lib;", "servicios, login, su, passwd;", "instalar, reparar, chmod, kill;", "terminal y sshd."], TEXTO, 18)
    d.caja(396, 226, 340, 70, "a", None)
    d.texto(406, 234, "Por qué estos", TEXTO, 1, True)
    d.lineas_texto(406, 256, ["si una .so se rompe, siempre se", "puede entrar, reparar y arreglar."], TEXTO, 18)
    d.caja(396, 314, 340, 70, "n", None)
    d.texto(406, 322, "Vuelta atrás: DINAMICO=0", TEXTO, 1, True)
    d.lineas_texto(406, 344, ["todo estático, como hasta la", "v8.5.5.2 (make DINAMICO=0)."], TEXTO, 18)
    d.texto(24, 452, "Se ahorra disco (/bin: 9,0 a 5,5 MB) y cada lanzamiento cuesta unos 0,25 ms más.", SUAVE)
    d.texto(24, 472, "A cambio, la RAM sube algo: cada proceso dinámico lleva su BSS y su intérprete.", SUAVE)
    d.texto(24, 492, "El núcleo solo carga el intérprete si /lib y él son de root sin escritura de otros.", SUAVE)
    return d


def sincronizacion():
    d = Lienzo(600, "Sincronización: dos hilos y un recurso", "Sin exclusión mutua, dos hilos que cambian a la vez un dato lo estropean.")
    d.caja(24, 70, 200, 40, "u", "Hilo A", negrita=True)
    d.caja(536, 70, 200, 40, "u", "Hilo B", negrita=True)
    d.caja(280, 70, 200, 40, "n", ["Recurso compartido", "(contador, lista)"])
    d.linea(124, 112, 124, 420, 0x9AA5B1, 2, 6)
    d.linea(636, 112, 636, 420, 0x9AA5B1, 2, 6)
    d.texto(260, 130, "el tiempo va hacia abajo", SUAVE)
    d.flecha((380, 150), (380, 168), color=SUAVE)
    d.caja(30, 184, 188, 36, "k", ["mutex_tomar(&m)", "lo consigue"])
    d.caja(30, 232, 188, 84, "a", ["Sección crítica", "lee, cambia y escribe", "el recurso"])
    d.caja(30, 328, 188, 36, "k", ["mutex_largar(&m)", "despierta a B"])
    d.caja(542, 184, 188, 36, "r", ["mutex_tomar(&m)", "está ocupado"])
    d.caja(542, 232, 188, 84, "u", ["Espera dormido", "futex_esperar", "(SIS_FUTEX_ESPERAR)"])
    d.caja(542, 328, 188, 36, "k", ["Despierta y toma", "el mutex"])
    d.caja(542, 376, 188, 44, "a", ["Sección crítica", "ahora le toca a B"])
    d.flecha((219, 346), (541, 346), color=0x2E7D4F)
    d.texto_c(380, 322, "futex acordar", 0x2E7D4F, 1, True)
    d.caja(24, 444, 350, 138, "n", None)
    d.texto(34, 452, "En el programa (libsagui)", TEXTO, 1, True)
    d.lineas_texto(34, 474, ["mutex_t (sincronia.c): 0 libre, 1", "tomado, 2 tomado y alguien duerme.", "Usa atómicas; gira un poco y si no", "se libra, duerme en el futex.", "Protege a los hilos de un proceso."], TEXTO, 18)
    d.caja(386, 444, 350, 138, "k", None)
    d.texto(396, 452, "En el kernel", TEXTO, 1, True)
    d.lineas_texto(396, 474, ["trava.c: exclusión mutua entre", "procesos y tareas del kernel.", "Cerrojo general (trava_geral.c,", "D165): una CPU a la vez dentro,", "por orden de llegada (ticket lock)."], TEXTO, 18)
    return d


TODOS = [
    ("arranque.png", arranque),
    ("anillos.png", anillos),
    ("procesos-estados.png", procesos_estados),
    ("planificador.png", planificador),
    ("paginacion.png", paginacion),
    ("mapa-memoria.png", mapa_memoria),
    ("vfs.png", vfs),
    ("es-drivers.png", es_drivers),
    ("pila-red.png", pila_red),
    ("graficos.png", graficos),
    ("sincronizacion.png", sincronizacion),
    ("dinamico.png", dinamico),
]


def main():
    raiz = os.path.dirname(AQUI)
    salida = sys.argv[1] if len(sys.argv) > 1 else os.path.join(raiz, "rootfs", "ayuda", "sistemas-operativos", "img")
    os.makedirs(salida, exist_ok=True)
    total = 0
    for nombre, f in TODOS:
        n = f().guardar(os.path.join(salida, nombre))
        total += n
        print("%-24s %6d bytes" % (nombre, n))
    print("total %d bytes" % total)


if __name__ == "__main__":
    main()
