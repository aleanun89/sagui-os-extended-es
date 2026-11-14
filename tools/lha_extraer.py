#!/usr/bin/env python3
"""lha_extraer.py - extrae un archivo LHA (métodos -lh0- y -lh5-) (D229).

    lha_extraer.py ARCHIVO DESTINO

El shareware de Quake (quake106.zip) trae sus ficheros dentro de resource.1,
un autoextraíble de DOS hecho con LHA («LHA's SFX 2.13S»). Para no depender
de un programa lha en el PC, este es un descompresor propio, escrito para el
SaguiOS: lee las cabeceras de nivel 0 y 1 (lo que hay en ese fichero; se
saltan el código del autoextraíble buscando la primera cabecera), descomprime
el -lh5- (LZSS con ventana de 8 KB y códigos de Huffman estáticos por
bloque, el formato de LHA 2.x) y comprueba el CRC-16 de cada fichero.

Se niega a escribir fuera de DESTINO (rutas absolutas o con «..»). Los
separadores «\\» de DOS pasan a «/». Solo la biblioteca estándar de Python.
"""
import os
import sys


class ErroLHA(Exception):
    pass


# ------------------------------------------------------------------ CRC-16 (el de LHA: polinomio 0xA001)

_TAB_CRC = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ 0xA001 if _c & 1 else _c >> 1
    _TAB_CRC.append(_c)


def crc16(dados):
    crc = 0
    tab = _TAB_CRC
    for b in dados:
        crc = (crc >> 8) ^ tab[(crc ^ b) & 0xFF]
    return crc


# ------------------------------------------------------------------ cabeceras

def cabecalhos(d):
    """(nome, método, dados comprimidos, tamaño original, crc) de cada fichero."""
    i = d.find(b'-lh')
    if i < 2:
        raise ErroLHA('no hay ninguna cabecera LHA')
    i -= 2
    while i < len(d) and d[i] != 0:
        if i + 22 > len(d):
            raise ErroLHA('cabecera cortada')
        tam = d[i]
        metodo = d[i + 2:i + 7]
        if metodo[:3] != b'-lh' or metodo[4:5] != b'-':
            raise ErroLHA('cabecera no válida en %d' % i)
        comprimido = int.from_bytes(d[i + 7:i + 11], 'little')
        original = int.from_bytes(d[i + 11:i + 15], 'little')
        nivel = d[i + 20]
        nl = d[i + 21]
        nome = d[i + 22:i + 22 + nl].decode('latin-1')
        crc = int.from_bytes(d[i + 22 + nl:i + 24 + nl], 'little')
        inicio = i + 2 + tam
        if nivel == 1:
            # Las cabeceras extendidas van detrás y cuentan en el tamaño comprimido.
            p = inicio
            ext = int.from_bytes(d[p - 2:p], 'little')
            while ext:
                p += ext
                ext = int.from_bytes(d[p - 2:p], 'little')
            comprimido -= p - inicio
            inicio = p
        elif nivel != 0:
            raise ErroLHA('cabecera de nivel %d no admitida' % nivel)
        if inicio + comprimido > len(d):
            raise ErroLHA('%s: datos cortados' % nome)
        yield nome, metodo, d[inicio:inicio + comprimido], original, crc
        i = inicio + comprimido


# ------------------------------------------------------------------ -lh5-

class Bits:
    def __init__(self, b):
        self.b = bytes(b) + b'\0\0\0\0'
        self.p = 0

    def ver(self, n):
        q = self.p >> 3
        v = int.from_bytes(self.b[q:q + 4], 'big')
        return (v >> (32 - (self.p & 7) - n)) & ((1 << n) - 1)

    def ler(self, n):
        if not n:
            return 0
        v = self.ver(n)
        self.p += n
        return v


def tabela(comprimentos):
    """Tabla de 16 bits -> (símbolo, longitud) del código canónico."""
    t = [None] * 65536
    codigo = 0
    for L in range(1, 17):
        for s, l in enumerate(comprimentos):
            if l == L:
                n = 1 << (16 - L)
                base = codigo << (16 - L)
                if base + n > 65536:
                    raise ErroLHA('código de Huffman no válido')
                t[base:base + n] = [(s, L)] * n
                codigo += 1
        codigo <<= 1
    return t


def tabela_unica(simbolo):
    return [(simbolo, 0)] * 65536


def ler_pt(bs, nn, nbit, especial):
    n = bs.ler(nbit)
    if n == 0:
        return tabela_unica(bs.ler(nbit))
    if n > nn:
        raise ErroLHA('tabla de longitudes no válida')
    comp = [0] * nn
    i = 0
    while i < n:
        c = bs.ler(3)
        if c == 7:
            while bs.ler(1):
                c += 1
        comp[i] = c
        i += 1
        if i == especial:
            for _ in range(bs.ler(2)):
                if i < nn:
                    comp[i] = 0
                i += 1
    return tabela(comp)


def ler_c(bs, pt):
    n = bs.ler(9)
    if n == 0:
        return tabela_unica(bs.ler(9))
    if n > 510:
        raise ErroLHA('tabla de caracteres no válida')
    comp = [0] * 510
    i = 0
    while i < n:
        e = pt[bs.ver(16)]
        if e is None:
            raise ErroLHA('código no válido')
        s, L = e
        bs.p += L
        if s <= 2:
            i += 1 if s == 0 else (bs.ler(4) + 3 if s == 1 else bs.ler(9) + 20)
        else:
            comp[i] = s - 2
            i += 1
    return tabela(comp)


def lh5(dados, original):
    bs = Bits(dados)
    limite = len(dados) * 8 + 32
    saida = bytearray()
    bloco = 0
    ct = pp = None
    while len(saida) < original:
        if bs.p > limite:
            raise ErroLHA('datos comprimidos cortados')
        if bloco == 0:
            bloco = bs.ler(16)
            pt = ler_pt(bs, 19, 5, 3)
            ct = ler_c(bs, pt)
            pp = ler_pt(bs, 14, 4, -1)
        bloco -= 1
        e = ct[bs.ver(16)]
        if e is None:
            raise ErroLHA('código no válido')
        c, L = e
        bs.p += L
        if c < 256:
            saida.append(c)
            continue
        comprimento = c - 253
        e = pp[bs.ver(16)]
        if e is None:
            raise ErroLHA('código no válido')
        j, L = e
        bs.p += L
        if j:
            j = (1 << (j - 1)) + bs.ler(j - 1)
        ini = len(saida) - j - 1
        if ini < 0:
            raise ErroLHA('distancia fuera de la ventana')
        for k in range(comprimento):
            saida.append(saida[ini + k])
    return bytes(saida[:original])


# ------------------------------------------------------------------ extraer

def nome_seguro(nome):
    partes = [p for p in nome.replace('\\', '/').split('/') if p]
    if not partes or nome.startswith(('/', '\\')) or any(p in ('.', '..') for p in partes) or ':' in nome:
        raise ErroLHA('nombre peligroso en el archivo: %r' % nome)
    return os.path.join(*partes)


def extrair(arquivo, destino):
    d = open(arquivo, 'rb').read()
    n = 0
    for nome, metodo, dados, original, crc in cabecalhos(d):
        if metodo == b'-lh5-':
            conteudo = lh5(dados, original)
        elif metodo == b'-lh0-':
            conteudo = dados
        else:
            raise ErroLHA('%s: método %s no admitido' % (nome, metodo.decode('latin-1')))
        if len(conteudo) != original or crc16(conteudo) != crc:
            raise ErroLHA('%s: el CRC no coincide' % nome)
        c = os.path.join(destino, nome_seguro(nome))
        os.makedirs(os.path.dirname(c) or destino, exist_ok=True)
        with open(c, 'wb') as f:
            f.write(conteudo)
        print('lha_extraer: %s (%d bytes)' % (nome.replace('\\', '/'), original))
        n += 1
    if not n:
        raise ErroLHA('el archivo no tiene ficheros')
    return n


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    try:
        extrair(sys.argv[1], sys.argv[2])
    except ErroLHA as e:
        sys.stderr.write('lha_extraer: %s\n' % e)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
