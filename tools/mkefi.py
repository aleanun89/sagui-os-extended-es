#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkefi.py - convierte el cargador UEFI enlazado como ELF en una aplicación EFI
(PE32+ para BOOTX64.EFI, PE32 para BOOTIA32.EFI). D264.

Por qué así: el gcc del sistema genera ELF, y el formato de UEFI es PE/COFF. Con
'objcopy --target=efi-app-*' las reubicaciones no pasan al PE (gnu-efi lo resuelve
con un código que se reubica a sí mismo al empezar); aquí se hace en el PC:

  1. el ELF viene enlazado independiente de la posición (-pie) a partir de 0x1000
     con boot/uefi/ligacao.ld: dos segmentos de carga (código y datos) alineados a
     página, y las reubicaciones dinámicas en .rela.dyn (x64) o .rel.dyn (i386);
  2. solo se aceptan reubicaciones R_*_RELATIVE ("suma la base"): cualquier otra
     querría un enlazador dinámico y la conversión falla, con el motivo;
  3. cada segmento pasa a una sección del PE en la misma dirección relativa; cada
     RELATIVE se aplica con la ImageBase del PE y se apunta en la sección .reloc
     (DIR64 en x64, HIGHLOW en IA32), que el firmware usa si carga la imagen en otra
     dirección (lo normal).

Uso: mkefi.py ENTRADA.elf SALIDA.efi
"""
import struct
import sys

PAGINA = 0x1000
ALINH_ARQ = 0x200
IMAGE_BASE = 0x10000000
R_RELATIVE = 8          # R_X86_64_RELATIVE y R_386_RELATIVE tienen el mismo número
SHT_RELA, SHT_REL = 4, 9
SHF_ALLOC = 2
PT_LOAD, PT_DYNAMIC, PT_INTERP = 1, 2, 3


def alinhar(v, a):
    return (v + a - 1) // a * a


def falhar(msg):
    sys.exit(f"mkefi: {msg}")


def ler_elf(dados):
    if dados[:4] != b"\x7fELF" or dados[5] != 1:
        falhar("no es un ELF little-endian")
    classe = dados[4]
    if classe == 2:
        (e_type, e_machine, _v, e_entry, e_phoff, e_shoff, _f, _eh, e_phentsize, e_phnum, e_shentsize, e_shnum,
         e_shstrndx) = struct.unpack_from("<HHIQQQIHHHHHH", dados, 16)
    else:
        (e_type, e_machine, _v, e_entry, e_phoff, e_shoff, _f, _eh, e_phentsize, e_phnum, e_shentsize, e_shnum,
         e_shstrndx) = struct.unpack_from("<HHIIIIIHHHHHH", dados, 16)
    if (classe, e_machine) not in ((2, 62), (1, 3)):
        falhar(f"arquitectura no soportada (clase {classe}, máquina {e_machine})")
    segs = []
    dinamico = False
    for k in range(e_phnum):
        o = e_phoff + k * e_phentsize
        if classe == 2:
            p_type, p_flags, p_offset, p_vaddr, _pa, p_filesz, p_memsz, _al = struct.unpack_from("<IIQQQQQQ", dados, o)
        else:
            p_type, p_offset, p_vaddr, _pa, p_filesz, p_memsz, p_flags, _al = struct.unpack_from("<IIIIIIII", dados, o)
        if p_type == PT_INTERP:
            falhar("el ELF pide un intérprete dinámico (falta --no-dynamic-linker)")
        dinamico |= p_type == PT_DYNAMIC
        if p_type == PT_LOAD and p_memsz:
            segs.append((p_vaddr, p_offset, p_filesz, p_memsz, p_flags))
    secs = []
    for k in range(e_shnum):
        o = e_shoff + k * e_shentsize
        if classe == 2:
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, _l, _i, _a, sh_entsize = \
                struct.unpack_from("<IIQQQQIIQQ", dados, o)
        else:
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, _l, _i, _a, sh_entsize = \
                struct.unpack_from("<IIIIIIIIII", dados, o)
        secs.append((sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_entsize))
    return classe, dinamico, e_entry, segs, secs


def main():
    if len(sys.argv) != 3:
        falhar("uso: mkefi.py ENTRADA.elf SALIDA.efi")
    dados = open(sys.argv[1], "rb").read()
    classe, dinamico, entrada, segs, secs = ler_elf(dados)
    x64 = classe == 2
    # Un PIE cuyo primer segmento no está en 0 sale marcado como ET_EXEC (ld), pero sigue
    # enlazado como PIE: lo que lo delata es la sección dinámica con sus reubicaciones.
    if not dinamico:
        falhar("el ELF no es independiente de la posición (enlazar con -pie)")
    if not segs:
        falhar("sin segmentos de carga")
    segs.sort()
    if segs[0][0] < PAGINA:
        falhar("la imagen tiene que empezar en 0x1000 o más arriba (las cabeceras PE van en la primera página)")
    for (va, _o, _fs, ms, _fl), sig in zip(segs, segs[1:] + [None]):
        if va % PAGINA:
            falhar(f"segmento en 0x{va:x} no alineado a página")
        if sig and va + ms > sig[0]:
            falhar("segmentos solapados")
    fim = alinhar(max(va + ms for va, _o, _fs, ms, _fl in segs), PAGINA)
    img = bytearray(fim)
    for va, off, fs, ms, _fl in segs:
        img[va:va + fs] = dados[off:off + fs]

    # Reubicaciones dinámicas: solo RELATIVE.
    relocs = []
    for _n, tipo, flags, _addr, off, tam, ent in secs:
        if tipo not in (SHT_RELA, SHT_REL) or not (flags & SHF_ALLOC) or not tam:
            continue
        if x64 and tipo == SHT_RELA:
            for o in range(off, off + tam, ent or 24):
                r_offset, r_info, r_addend = struct.unpack_from("<QQq", dados, o)
                t = r_info & 0xFFFFFFFF
                if t == 0:
                    continue
                if t != R_RELATIVE:
                    falhar(f"reubicación de tipo {t} en 0x{r_offset:x}: solo se admiten RELATIVE")
                struct.pack_into("<Q", img, r_offset, IMAGE_BASE + r_addend)
                relocs.append(r_offset)
        elif not x64 and tipo == SHT_REL:
            for o in range(off, off + tam, ent or 8):
                r_offset, r_info = struct.unpack_from("<II", dados, o)
                t = r_info & 0xFF
                if t == 0:
                    continue
                if t != R_RELATIVE:
                    falhar(f"reubicación de tipo {t} en 0x{r_offset:x}: solo se admiten RELATIVE")
                (v,) = struct.unpack_from("<I", img, r_offset)
                struct.pack_into("<I", img, r_offset, (v + IMAGE_BASE) & 0xFFFFFFFF)
                relocs.append(r_offset)
        else:
            falhar("sección de reubicaciones de un tipo inesperado")

    # Sección .reloc: bloques por página.
    tipo_reloc = 10 if x64 else 3      # IMAGE_REL_BASED_DIR64 / HIGHLOW
    paginas = {}
    for r in sorted(set(relocs)):
        paginas.setdefault(r & ~(PAGINA - 1), []).append((tipo_reloc << 12) | (r & (PAGINA - 1)))
    if not paginas:
        paginas[segs[0][0]] = []       # un bloque vacío: hay firmware que quiere al menos uno
    reloc = bytearray()
    for pag, ents in sorted(paginas.items()):
        if len(ents) % 2 or not ents:
            ents = ents + [0] * (2 - len(ents) % 2 if ents else 2)
        reloc += struct.pack("<II", pag, 8 + 2 * len(ents)) + struct.pack(f"<{len(ents)}H", *ents)

    # Secciones del PE: una por segmento y la .reloc.
    secoes = []
    for va, _off, _fs, ms, fl in segs:
        exe = fl & 1
        nome = b".text" if exe else b".data"
        carac = 0x60000020 if exe else 0xC0000040
        secoes.append([nome, va, ms, bytes(img[va:va + ms]), carac])
    reloc_va = fim
    secoes.append([b".reloc", reloc_va, len(reloc), bytes(reloc), 0x42000040])
    tam_imagem = alinhar(reloc_va + len(reloc), PAGINA)

    n_sec = len(secoes)
    tam_opc = 240 if x64 else 224
    cabecalhos = alinhar(0x40 + 4 + 20 + tam_opc + 40 * n_sec, ALINH_ARQ)
    pos = cabecalhos
    for s in secoes:
        bruto = alinhar(len(s[3]), ALINH_ARQ)
        s.append(pos)
        s.append(bruto)
        pos += bruto

    codigo = sum(s[6] for s in secoes if s[4] & 0x20)
    inic = sum(s[6] for s in secoes if not s[4] & 0x20)
    base_codigo = next((s[1] for s in secoes if s[4] & 0x20), 0)
    base_dados = next((s[1] for s in secoes if not s[4] & 0x20), 0)

    dos = bytearray(0x40)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x40)
    carac = 0x0002 | 0x0004 | 0x0008 | 0x0200 | (0x0020 if x64 else 0x0100)
    coff = struct.pack("<HHIIIHH", 0x8664 if x64 else 0x014C, n_sec, 0, 0, 0, tam_opc, carac)
    if x64:
        opc = struct.pack("<HBBIIIII", 0x20B, 1, 0, codigo, inic, 0, entrada, base_codigo)
        opc += struct.pack("<QII", IMAGE_BASE, PAGINA, ALINH_ARQ)
        opc += struct.pack("<HHHHHHI", 0, 0, 0, 0, 0, 0, 0)
        opc += struct.pack("<IIIHH", tam_imagem, cabecalhos, 0, 10, 0)
        opc += struct.pack("<QQQQII", 0x10000, 0x10000, 0x10000, 0x1000, 0, 16)
    else:
        opc = struct.pack("<HBBIIIIII", 0x10B, 1, 0, codigo, inic, 0, entrada, base_codigo, base_dados)
        opc += struct.pack("<III", IMAGE_BASE, PAGINA, ALINH_ARQ)
        opc += struct.pack("<HHHHHHI", 0, 0, 0, 0, 0, 0, 0)
        opc += struct.pack("<IIIHH", tam_imagem, cabecalhos, 0, 10, 0)
        opc += struct.pack("<IIIIII", 0x10000, 0x10000, 0x10000, 0x1000, 0, 16)
    diretorios = [(0, 0)] * 16
    diretorios[5] = (reloc_va, len(reloc))
    opc += b"".join(struct.pack("<II", *d) for d in diretorios)
    assert len(opc) == tam_opc
    tabela = b""
    for nome, va, vs, _b, c, ptr, bruto in secoes:
        tabela += struct.pack("<8sIIIIIIHHI", nome, vs, va, bruto, ptr, 0, 0, 0, 0, c)
    cab = bytes(dos) + b"PE\0\0" + coff + opc + tabela
    saida = bytearray(cab.ljust(cabecalhos, b"\0"))
    for _n, _va, _vs, b, _c, _ptr, bruto in secoes:
        saida += b.ljust(bruto, b"\0")
    with open(sys.argv[2], "wb") as f:
        f.write(saida)
    print(f"mkefi: {sys.argv[2]}: {'PE32+ x64' if x64 else 'PE32 IA32'}, {len(saida)} bytes, "
          f"entrada 0x{entrada:x}, {len(relocs)} reubicaciones")


if __name__ == "__main__":
    main()
