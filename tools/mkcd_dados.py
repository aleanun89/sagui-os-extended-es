#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkcd_dados.py - imágenes de CD/DVD de datos para las pruebas (D233): ISO 9660 (con o sin
Joliet), UDF (1.02 a 2.01) o los dos a la vez ("UDF bridge", lo normal en un DVD), sin
genisoimage, xorriso ni mkudffs (no están en el entorno del proyecto).

Uso:
  mkcd_dados.py --pasta DIR --saida disco.iso [--iso] [--joliet] [--udf 2.01] [--rotulo NOMBRE]
                [--embutir] [--fragmentar RUTA] [--sessao N]

  --iso         árbol ISO 9660 (nombres 8.3 en mayúsculas, ";1")
  --joliet      además, el árbol Joliet (nombres largos en UCS-2)
  --udf REV     árbol UDF de esa revisión (1.02, 1.50, 2.00 o 2.01)
  --embutir     UDF: los directorios y los archivos pequeños dentro de su File Entry
  --fragmentar  UDF: ese archivo en tres trozos separados, con la lista de extensiones en un
                Allocation Extent Descriptor (para probar la continuación)
  --sessao N    simula un disco multisesión: los descriptores (ISO y UDF) se escriben a partir
                del bloque N (el lector diría N en READ TOC formato 1); los datos, absolutos

Distribución (bloques de 2048):
  0-15     área de sistema
  16...    ISO: descriptor primario, Joliet, terminador; UDF: BEA01, NSR0x, TEA01
  32-37    UDF: secuencia principal de descriptores (PVD, IUVD, PD, LVD, USD, TD)
  48-53    UDF: la de reserva;  64: Logical Volume Integrity Descriptor (y su terminador)
  256      UDF: Anchor Volume Descriptor Pointer (también en el último bloque)
  257...   la partición UDF (FSD, entradas, directorios y los datos de los archivos, que el
           ISO 9660 comparte) y, detrás, los directorios y las tablas de rutas del ISO
Con --sessao, todo lo anterior se desplaza N bloques (salvo los datos de los archivos, que
están donde estén: los registros de directorio y las extensiones UDF son absolutos).
"""
import argparse
import os
import struct
import sys
import time

BLOCO = 2048


# ------------------------------------------------------------ árbol de entrada

class No:
    def __init__(self, nome, pasta, dados=b"", pai=None):
        self.nome = nome          # str (el nombre largo)
        self.pasta = pasta
        self.dados = dados
        self.pai = pai
        self.filhos = []
        # rellenado al distribuir
        self.lba_dados = 0        # primer bloque de los datos (archivo)
        self.extensoes = []       # UDF: [(bloque físico, bytes)] de un archivo fragmentado
        self.fe_lbn = 0           # UDF: bloque lógico de la File Entry
        self.dir_udf = b""        # UDF: los FID del directorio
        self.dir_udf_lbn = 0
        self.iso_lba = 0          # ISO: bloque del directorio
        self.iso_tam = 0
        self.jol_lba = 0          # Joliet: bloque del directorio
        self.jol_tam = 0
        self.nome83 = b""


def ler_arvore(raiz):
    r = No("", True)
    def andar(no, caminho):
        for n in sorted(os.listdir(caminho)):
            c = os.path.join(caminho, n)
            if os.path.isdir(c):
                f = No(n, True, pai=no)
                andar(f, c)
            else:
                f = No(n, False, open(c, "rb").read(), pai=no)
            no.filhos.append(f)
    andar(r, raiz)
    return r


def todos(no):
    yield no
    for f in no.filhos:
        yield from todos(f)


def caminho(no):
    p = []
    while no.pai:
        p.append(no.nome)
        no = no.pai
    return "/".join(reversed(p))


# ------------------------------------------------------------ utilidades

def blocos(n):
    return (n + BLOCO - 1) // BLOCO


def ambos16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def ambos32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def crc_itu(dados):
    """CRC de ECMA-167 7.2.6 (CRC-ITU-T, polinomio 0x1021, valor inicial 0)."""
    crc = 0
    for b in dados:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


class Disco:
    def __init__(self, total):
        self.b = bytearray(total * BLOCO)

    def pon(self, lba, dados):
        self.b[lba * BLOCO:lba * BLOCO + len(dados)] = dados


# ------------------------------------------------------------ UDF

def etiqueta(ident, versao, corpo, local):
    """Descriptor completo: la etiqueta de 16 bytes (suma y CRC) + el cuerpo."""
    crc = crc_itu(corpo)
    t = bytearray(struct.pack("<HHBBHHHI", ident, versao, 0, 0, 1, crc, len(corpo), local))
    t[4] = (sum(t[0:4]) + sum(t[5:16])) & 0xFF
    return bytes(t) + corpo


def regid(ident, sufixo=b""):
    return bytes([0]) + ident.encode("ascii").ljust(23, b"\0") + sufixo.ljust(8, b"\0")


def charspec():
    return bytes([0]) + b"OSTA Compressed Unicode".ljust(63, b"\0")


def cs0(nome):
    """d-characters OSTA CS0: 8 bits si todo cabe en Latin-1; si no, UTF-16 big-endian."""
    if all(ord(c) < 256 for c in nome):
        return bytes([8]) + nome.encode("latin-1")
    return bytes([16]) + nome.encode("utf-16-be")


def dstring(nome, tam):
    d = cs0(nome) if nome else b""
    d = d[:tam - 1]
    return d.ljust(tam - 1, b"\0") + bytes([len(d)])


def timestamp(t):
    return struct.pack("<HhBBBBBBBB", (1 << 12) | 0, t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0, 0, 0)


def long_ad(tam, lbn, ref=0):
    return struct.pack("<IIH", tam, lbn, ref) + b"\0" * 6


def short_ad(tam, pos, tipo=0):
    return struct.pack("<II", (tipo << 30) | tam, pos)


# ------------------------------------------------------------ ISO 9660

def nome_83(nome, usados):
    base, _, ext = nome.rpartition(".") if "." in nome else (nome, "", "")
    def limpa(s, n):
        s = "".join(c if (c.isascii() and (c.isalnum() or c == "_")) else "_" for c in s.upper())
        return s[:n]
    b, e = limpa(base, 8) or "_", limpa(ext, 3)
    k = 1
    cand = b + ("." + e if e else "")
    while cand in usados:
        sfx = "~%d" % k
        cand = b[:8 - len(sfx)] + sfx + ("." + e if e else "")
        k += 1
    usados.add(cand)
    return cand


def data_registro(t):
    return bytes([t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, 0])


def registro(lba, tam, nome, pasta, t):
    corpo = (bytes([0]) + ambos32(lba) + ambos32(tam) + data_registro(t) + bytes([2 if pasta else 0, 0, 0])
             + ambos16(1) + bytes([len(nome)]) + nome)
    if (len(corpo) + 1) % 2:   # byte de relleno tras un nombre de longitud par (el tamaño lo incluye)
        corpo += b"\0"
    return bytes([len(corpo) + 1]) + corpo


def dir_iso(no, t, joliet):
    """Los registros del directorio, sin cruzar bloques (con el tamaño que quedará)."""
    def lba(n):
        return n.jol_lba if joliet else n.iso_lba
    def tam(n):
        return n.jol_tam if joliet else n.iso_tam
    pai = no.pai or no
    regs = [registro(lba(no), tam(no), b"\0", True, t), registro(lba(pai), tam(pai), b"\1", True, t)]
    for f in no.filhos:
        if joliet:
            nome = f.nome.encode("utf-16-be")[:128]
        else:
            nome = f.nome83.encode("ascii") + (b"" if f.pasta else b";1")
        if f.pasta:
            regs.append(registro(lba(f), tam(f), nome, True, t))
        else:
            regs.append(registro(f.lba_dados, len(f.dados), nome, False, t))
    out = bytearray()
    for r in regs:
        if len(out) % BLOCO + len(r) > BLOCO:
            out += b"\0" * (BLOCO - len(out) % BLOCO)
        out += r
    return bytes(out) + b"\0" * ((-len(out)) % BLOCO)


def tabela_rotas(pastas, grande, joliet):
    fmt = ">" if grande else "<"
    out = bytearray()
    idx = {id(p): i + 1 for i, p in enumerate(pastas)}
    for p in pastas:
        if p.pai is None:
            nome = b"\0"
        else:
            nome = p.nome.encode("utf-16-be") if joliet else p.nome83.encode("ascii")
        lba = p.jol_lba if joliet else p.iso_lba
        pai = idx[id(p.pai)] if p.pai else 1
        out += bytes([len(nome), 0]) + struct.pack(fmt + "I", lba) + struct.pack(fmt + "H", pai) + nome
        if len(nome) % 2:
            out += b"\0"
    return bytes(out)


# ------------------------------------------------------------ montaje

def montar(raiz, saida, iso=True, joliet=False, udf=None, rotulo="DADOS", embutir=False, fragmentar=None,
           sessao=0, data=None):
    t = time.gmtime(data if data is not None else int(os.environ.get("SOURCE_DATE_EPOCH", "1700000000")))
    nos = list(todos(raiz))
    pastas = [n for n in nos if n.pasta]
    # Orden de las tablas de rutas: por niveles (la raíz primero).
    pastas.sort(key=lambda p: caminho(p).count("/") + (0 if p.pai is None else 1))
    arquivos = [n for n in nos if not n.pasta]
    for p in pastas:
        usados = set()
        for f in p.filhos:
            f.nome83 = nome_83(f.nome, usados)

    rev = {"1.02": 0x0102, "1.50": 0x0150, "2.00": 0x0200, "2.01": 0x0201}[udf] if udf else 0
    vtag = 3 if rev >= 0x0200 else 2
    base = sessao                 # descriptores de esta "sesión"
    PART = base + 257             # inicio de la partición UDF
    prox = PART                   # siguiente bloque físico libre
    lbn = lambda fis: fis - PART  # noqa: E731

    # --- UDF: FSD, su terminador y las File Entry de todo (una por bloque)
    if udf:
        fsd_lbn, prox = 0, PART + 2
        for n in nos:
            n.fe_lbn = lbn(prox)
            prox += 1
        aed_lbn = None
        frag = None
        if fragmentar:
            frag = next((n for n in arquivos if caminho(n) == fragmentar), None)
            if not frag:
                sys.exit(f"mkcd_dados: no está {fragmentar}")
            aed_lbn = lbn(prox)
            prox += 1
    # --- datos de los archivos (compartidos por el ISO y el UDF)
    for f in arquivos:
        if udf and embutir and len(f.dados) <= BLOCO - 176:
            f.lba_dados = 0        # dentro de la File Entry (el ISO no puede verlo: va también aparte)
            continue
        if udf and fragmentar and f is frag:
            # Tres trozos (bloques enteros salvo el último) con un bloque libre entre ellos.
            n = blocos(len(f.dados))
            cortes = [0, max(1, n // 3), max(2, 2 * n // 3), n]
            f.extensoes = []
            for i in range(3):
                nb = cortes[i + 1] - cortes[i]
                if nb <= 0:
                    continue
                ini = cortes[i] * BLOCO
                fim = min(len(f.dados), cortes[i + 1] * BLOCO)
                f.extensoes.append((prox, fim - ini))
                prox += nb + 1
            f.lba_dados = f.extensoes[0][0]   # el ISO lo verá como contiguo: no se pone en el ISO
            continue
        f.lba_dados = prox
        prox += max(1, blocos(len(f.dados)))
    # --- directorios UDF (FID); con --embutir, dentro de la File Entry si caben
    if udf:
        def fid(car, nome, alvo):
            ident = cs0(nome) if nome else b""
            corpo = struct.pack("<HBB", 1, car, len(ident)) + long_ad(BLOCO, alvo.fe_lbn) + struct.pack("<H", 0) + ident
            corpo += b"\0" * ((-(16 + len(corpo))) % 4)
            return corpo
        for p in pastas:
            p._fids = [(0x0A, "", p.pai or p)] + [((0x02 if f.pasta else 0x00), f.nome, f) for f in p.filhos]
            tam = sum(16 + len(fid(c, nm, a)) for c, nm, a in p._fids)
            p._tam_dir = tam
            if embutir and tam <= BLOCO - 176:
                p.dir_udf_lbn = None
            else:
                p.dir_udf_lbn = lbn(prox)
                prox += blocos(tam)
    # --- ISO 9660 (y Joliet): directorios y tablas de rutas
    if iso:
        for p in pastas:
            p.iso_tam = BLOCO   # provisional para medir
        for p in pastas:
            p.iso_tam = len(dir_iso(p, t, False))
        for p in pastas:
            p.iso_lba = prox
            prox += blocos(p.iso_tam)
        tl = tabela_rotas(pastas, False, False)
        tab_l, prox = prox, prox + blocos(len(tl))
        tab_m, prox = prox, prox + blocos(len(tl))
        if joliet:
            for p in pastas:
                p.jol_tam = BLOCO
            for p in pastas:
                p.jol_tam = len(dir_iso(p, t, True))
            for p in pastas:
                p.jol_lba = prox
                prox += blocos(p.jol_tam)
            tj = tabela_rotas(pastas, False, True)
            jtab_l, prox = prox, prox + blocos(len(tj))
            jtab_m, prox = prox, prox + blocos(len(tj))
        # Archivos embutidos o fragmentados en el UDF: el ISO necesita sus datos seguidos.
        for f in arquivos:
            if f.lba_dados == 0 or f.extensoes:
                f.iso_dados = prox
                prox += max(1, blocos(len(f.dados)))
    total = prox + (1 if udf else 0)   # el último bloque: la segunda ancla
    if total < base + 300:
        total = base + 300
    d = Disco(total)

    # --- datos
    for f in arquivos:
        if f.extensoes:
            pos = 0
            for fis, tam in f.extensoes:
                d.pon(fis, f.dados[pos:pos + tam])
                pos += tam
        elif f.lba_dados:
            d.pon(f.lba_dados, f.dados)
        if iso and hasattr(f, "iso_dados"):
            d.pon(f.iso_dados, f.dados)

    # --- ISO 9660
    if iso:
        # Lo que el ISO ve de cada archivo: sus datos seguidos.
        for f in arquivos:
            if hasattr(f, "iso_dados"):
                f._lba_udf, f.lba_dados = f.lba_dados, f.iso_dados
        for p in pastas:
            d.pon(p.iso_lba, dir_iso(p, t, False))
            if joliet:
                d.pon(p.jol_lba, dir_iso(p, t, True))
        d.pon(tab_l, tl)
        d.pon(tab_m, tabela_rotas(pastas, True, False))
        if joliet:
            d.pon(jtab_l, tj)
            d.pon(jtab_m, tabela_rotas(pastas, True, True))
        for f in arquivos:
            if hasattr(f, "_lba_udf"):
                f.lba_dados = f._lba_udf
        def vd(tipo, nome_vol, rp, tl_tam, ptl, ptm, esc=b""):
            v = bytearray(BLOCO)
            v[0:7] = bytes([tipo]) + b"CD001\x01"
            if tipo == 2:
                v[40:72] = nome_vol.encode("utf-16-be")[:32].ljust(32, b"\0")
                v[88:91] = esc
            else:
                v[40:72] = nome_vol.encode("ascii")[:32].ljust(32, b" ")
            v[8:40] = b" " * 32 if tipo == 1 else b"\0" * 32
            v[80:88] = ambos32(total)
            v[120:124] = ambos16(1)
            v[124:128] = ambos16(1)
            v[128:132] = ambos16(BLOCO)
            v[132:140] = ambos32(tl_tam)
            v[140:144] = struct.pack("<I", ptl)
            v[148:152] = struct.pack(">I", ptm)
            v[156:190] = rp
            stamp = time.strftime("%Y%m%d%H%M%S00", t).encode("ascii") + b"\0"
            v[813:830] = stamp
            v[830:847] = stamp
            v[847:864] = b"0" * 16 + b"\0"
            v[864:881] = b"0" * 16 + b"\0"
            v[881] = 1
            return bytes(v)
        r = pastas[0]
        vds = [vd(1, rotulo.upper(), registro(r.iso_lba, r.iso_tam, b"\0", True, t), len(tl), tab_l, tab_m)]
        if joliet:
            vds.append(vd(2, rotulo, registro(r.jol_lba, r.jol_tam, b"\0", True, t), len(tj), jtab_l, jtab_m, b"%/E"))
        fim = bytearray(BLOCO)
        fim[0:7] = b"\xffCD001\x01"
        vds.append(bytes(fim))
    else:
        vds = []
    # Volume Recognition Sequence del UDF, detrás de los descriptores del ISO.
    if udf:
        for ident in (b"BEA01", b"NSR03" if vtag == 3 else b"NSR02", b"TEA01"):
            v = bytearray(BLOCO)
            v[0:7] = b"\x00" + ident + b"\x01"
            vds.append(bytes(v))
    for i, v in enumerate(vds):
        d.pon(base + 16 + i, v)
    assert base + 16 + len(vds) <= base + 32

    # --- UDF
    if udf:
        dom = regid("*OSTA UDF Compliant", struct.pack("<HB", rev, 0))
        impl = regid("*SaguiOS mkcd_dados")
        part_tam = total - 1 - PART
        def seq(ini):
            lvid_loc = base + 64
            pvd = (struct.pack("<II", 1, 0) + dstring(rotulo, 32) + struct.pack("<HHHHII", 1, 1, 2, 3, 1, 1)
                   + dstring(rotulo + " conjunto", 128) + charspec() + charspec() + b"\0" * 16 + impl
                   + timestamp(t) + impl + b"\0" * 64 + struct.pack("<IH", 0, 0) + b"\0" * 22)
            iuvd = struct.pack("<I", 2) + regid("*UDF LV Info", struct.pack("<H", rev)) + charspec() + dstring(rotulo, 128) \
                + dstring("", 36) * 3 + impl + b"\0" * 128
            pd = (struct.pack("<IHH", 3, 1, 0) + regid("+NSR03" if vtag == 3 else "+NSR02") + b"\0" * 128
                  + struct.pack("<III", 1, PART, part_tam) + impl + b"\0" * 128 + b"\0" * 156)
            lvd = (struct.pack("<I", 4) + charspec() + dstring(rotulo, 128) + struct.pack("<I", BLOCO) + dom
                   + long_ad(BLOCO, fsd_lbn) + struct.pack("<II", 6, 1) + impl + b"\0" * 128
                   + struct.pack("<II", BLOCO * 2, lvid_loc) + struct.pack("<BBHH", 1, 6, 1, 0))
            usd = struct.pack("<II", 5, 0)
            td = b"\0" * 496
            return [(1, pvd), (4, iuvd), (5, pd), (6, lvd), (7, usd), (8, td)], ini
        for ini in (base + 32, base + 48):
            descs, _ = seq(ini)
            for i, (ident, corpo) in enumerate(descs):
                d.pon(ini + i, etiqueta(ident, vtag, corpo, ini + i))
        # LVID (cerrado) y su terminador
        nfich, ndirs = len(arquivos), len(pastas)
        lvid = (timestamp(t) + struct.pack("<I", 1) + b"\0" * 8 + struct.pack("<Q", 16 + len(nos)) + b"\0" * 24
                + struct.pack("<II", 1, 46) + struct.pack("<I", 0) + struct.pack("<I", part_tam)
                + impl + struct.pack("<IIHHH", nfich, ndirs, rev, rev, rev))
        d.pon(base + 64, etiqueta(9, vtag, lvid, base + 64))
        d.pon(base + 65, etiqueta(8, vtag, b"\0" * 496, base + 65))
        avdp = struct.pack("<IIII", 6 * BLOCO, base + 32, 6 * BLOCO, base + 48) + b"\0" * 480
        d.pon(base + 256, etiqueta(2, vtag, avdp, base + 256))
        d.pon(total - 1, etiqueta(2, vtag, avdp, total - 1))
        # FSD y terminador
        raiz_no = pastas[0]
        fsd = (timestamp(t) + struct.pack("<HHIIII", 3, 3, 1, 1, 0, 0) + charspec() + dstring(rotulo, 128) + charspec()
               + dstring(rotulo, 32) + dstring("", 32) + dstring("", 32) + long_ad(BLOCO, raiz_no.fe_lbn) + dom
               + b"\0" * 16 + b"\0" * 16 + b"\0" * 32)
        d.pon(PART + fsd_lbn, etiqueta(256, vtag, fsd, fsd_lbn))
        d.pon(PART + fsd_lbn + 1, etiqueta(8, vtag, b"\0" * 496, fsd_lbn + 1))
        unico = [16]
        def fe(no, ads, tipo_ad, tam_info, gravados):
            perm = 0x14A5 if no.pasta else 0x1084
            ligacoes = 1 + sum(1 for f in no.filhos if f.pasta) if no.pasta else 1
            icb = struct.pack("<IHHHBB", 0, 4, 0, 1, 0, 4 if no.pasta else 5) + b"\0" * 6 + struct.pack("<H", tipo_ad)
            u = 0 if no.pai is None else unico[0]
            unico[0] += 1
            corpo = (icb + struct.pack("<IIIHBBI", 0xFFFFFFFF, 0xFFFFFFFF, perm, ligacoes, 0, 0, 0)
                     + struct.pack("<QQ", tam_info, gravados) + timestamp(t) * 3 + struct.pack("<I", 1)
                     + b"\0" * 16 + impl + struct.pack("<QII", u, 0, len(ads)) + ads)
            return etiqueta(261, vtag, corpo, no.fe_lbn)
        for p in pastas:
            dados = bytearray()
            pos_lbn = p.dir_udf_lbn
            for c, nm, alvo in p._fids:
                ident = cs0(nm) if nm else b""
                corpo = struct.pack("<HBB", 1, c, len(ident)) + long_ad(BLOCO, alvo.fe_lbn) + struct.pack("<H", 0) + ident
                corpo += b"\0" * ((-(16 + len(corpo))) % 4)
                local = (pos_lbn + len(dados) // BLOCO) if pos_lbn is not None else p.fe_lbn
                dados += etiqueta(257, vtag, corpo, local)
            if pos_lbn is None:
                d.pon(PART + p.fe_lbn, fe(p, bytes(dados), 3, len(dados), 0))
            else:
                d.pon(PART + pos_lbn, bytes(dados))
                d.pon(PART + p.fe_lbn, fe(p, short_ad(len(dados), pos_lbn), 0, len(dados), blocos(len(dados))))
        for f in arquivos:
            if f.extensoes:
                ads = b"".join(short_ad(tam, lbn(fis)) for fis, tam in f.extensoes)
                aed = struct.pack("<II", 0, len(ads)) + ads
                d.pon(PART + aed_lbn, etiqueta(258, vtag, aed, aed_lbn))
                d.pon(PART + f.fe_lbn, fe(f, short_ad(BLOCO, aed_lbn, 3), 0, len(f.dados), blocos(len(f.dados))))
            elif embutir and len(f.dados) <= BLOCO - 176:
                d.pon(PART + f.fe_lbn, fe(f, f.dados, 3, len(f.dados), 0))
            elif not f.dados:
                d.pon(PART + f.fe_lbn, fe(f, b"", 0, 0, 0))
            else:
                ud = getattr(f, "_lba_udf", f.lba_dados)
                d.pon(PART + f.fe_lbn, fe(f, short_ad(len(f.dados), lbn(ud)), 0, len(f.dados), blocos(len(f.dados))))
    with open(saida, "wb") as s:
        s.write(d.b)
    return total


def main():
    ap = argparse.ArgumentParser(description="imágenes de CD/DVD de datos (ISO 9660, Joliet, UDF) para las pruebas")
    ap.add_argument("--pasta", required=True)
    ap.add_argument("--saida", required=True)
    ap.add_argument("--iso", action="store_true")
    ap.add_argument("--joliet", action="store_true")
    ap.add_argument("--udf", choices=["1.02", "1.50", "2.00", "2.01"])
    ap.add_argument("--rotulo", default="DADOS")
    ap.add_argument("--embutir", action="store_true")
    ap.add_argument("--fragmentar")
    ap.add_argument("--sessao", type=int, default=0)
    a = ap.parse_args()
    if not a.iso and not a.udf:
        sys.exit("mkcd_dados: hace falta --iso, --udf o los dos")
    total = montar(ler_arvore(a.pasta), a.saida, iso=a.iso, joliet=a.joliet, udf=a.udf, rotulo=a.rotulo,
                   embutir=a.embutir, fragmentar=a.fragmentar, sessao=a.sessao)
    print(f"mkcd_dados: {a.saida}: {total} bloques de 2048")


if __name__ == "__main__":
    main()
