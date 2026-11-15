#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Gera os PDF de teste do visor (melhora 059, D138).

São três documentos, escritos à mão a partir da norma ISO 32000-1:

  * `simples()`  : uma página com texto (Helvetica, uma das 14 fontes básicas),
                    um retângulo azul e mais texto. Serve para ver que o
                    analisador lê a xref, que o texto sai e que os vetores se
                    desenham.
  * `com_fonte()` : a mesma página, mas com uma fonte TrueType incrustada
                    (com os glifos de verdade). Se o anfitrião não tiver
                    nenhuma fonte TrueType, devolve None e a prova salta essa
                    parte.
  * `cifrado()`   : um PDF protegido com palavra-passe, pelo gestor de
                    segurança padrão da norma (RC4 de 40 bits, V1/R2).

Nada disto vem de uma biblioteca de PDF: é a norma escrita à mão, como o resto
do projeto.
"""
import hashlib
import os
import zlib

import aes256

# O preenchimento da norma (o "padding"), para as chaves de 40 e 128 bits.
PAD = bytes([
    0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56,
    0xFF, 0xFA, 0x01, 0x08, 0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
    0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A,
])

ID_ = b"0123456789abcdef"


# ------------------------------------------------------------ utilitários


def _rc4(chave, dados):
    """O RC4: XOR com a keystream."""
    s = list(range(256))
    j = 0
    for i in range(256):
        j = (j + s[i] + chave[i % len(chave)]) & 0xFF
        s[i], s[j] = s[j], s[i]
    saida = bytearray()
    i = j = 0
    for b in dados:
        i = (i + 1) & 0xFF
        j = (j + s[i]) & 0xFF
        s[i], s[j] = s[j], s[i]
        saida.append(b ^ s[(s[i] + s[j]) & 0xFF])
    return bytes(saida)


def _md5(dados):
    return hashlib.md5(dados).digest()


def _montar(objetos, trailer_extra="", id_=None):
    """Monta um PDF com a xref clássica. `objetos` é uma lista de
    (número, corpo em bytes)."""
    saida = bytearray()
    saida += b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
    posicoes = {}
    for num, corpo in objetos:
        posicoes[num] = len(saida)
        saida += ("%d 0 obj\n" % num).encode()
        saida += corpo if isinstance(corpo, bytes) else corpo.encode("latin-1")
        saida += b"\nendobj\n"
    pos_xref = len(saida)
    quantos = max(posicoes) + 1
    saida += ("xref\n0 %d\n" % quantos).encode()
    saida += b"0000000000 65535 f \n"
    for i in range(1, quantos):
        if i in posicoes:
            saida += ("%010d 00000 n \n" % posicoes[i]).encode()
        else:
            saida += b"0000000000 00000 f \n"
    extra = trailer_extra
    if id_ is not None:
        extra += "/ID [<%s> <%s>] " % (id_.hex(), id_.hex())
    saida += ("trailer\n<< /Size %d /Root 1 0 R %s>>\nstartxref\n%d\n%%%%EOF\n"
              % (quantos, extra, pos_xref)).encode()
    return bytes(saida)


# ------------------------------------------------------------ os documentos


def simples():
    """Uma página: texto, um retângulo e mais texto."""
    conteudo = (
        b"BT /F1 22 Tf 20 150 Td (SaguiOS PDF) Tj ET\n"
        b"0 0 1 rg 20 105 90 28 re f\n"
        b"0 0 0 rg\n"
        b"BT /F1 12 Tf 20 80 Td (pagina um do visor) Tj ET\n"
    )
    objetos = [
        (1, b"<< /Type /Catalog /Pages 2 0 R >>"),
        (2, b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
        (3, b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] "
            b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"),
        (4, b"<< /Length %d >>\nstream\n" % len(conteudo) + conteudo + b"endstream"),
        (5, b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
        (6, b"<< /Title (Documento de teste) /Author (SaguiOS) >>"),
    ]
    return _montar(objetos, trailer_extra="/Info 6 0 R ")


def comprimido():
    """A mesma página, mas com o conteúdo comprimido (FlateDecode).

    O FlateDecode do PDF é um fluxo zlib (RFC 1950): dois bytes de cabeçalho,
    o DEFLATE e o Adler-32 no fim. Serve para ver que o motor sabe
    descomprimir os streams de verdade."""
    conteudo = (
        b"BT /F1 22 Tf 20 150 Td (Conteudo comprimido) Tj ET\n"
        b"0 0 1 rg 20 105 90 28 re f\n"
        b"0 0 0 rg\n"
        b"BT /F1 12 Tf 20 80 Td (pagina comprimida) Tj ET\n"
    )
    dados = zlib.compress(conteudo, 9)
    objetos = [
        (1, b"<< /Type /Catalog /Pages 2 0 R >>"),
        (2, b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
        (3, b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] "
            b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"),
        (4, b"<< /Length %d /Filter /FlateDecode >>\nstream\n" % len(dados) + dados + b"\nendstream"),
        (5, b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
    ]
    return _montar(objetos)


def _fonte_true_type():
    """Uma fonte TrueType do anfitrião, se houver."""
    candidatos = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    ]
    for c in candidatos:
        if os.path.exists(c):
            with open(c, "rb") as f:
                return f.read()
    return None


def com_fonte():
    """A mesma página, mas com uma fonte TrueType incrustada."""
    programa = _fonte_true_type()
    if not programa:
        return None
    conteudo = (
        b"BT /F1 22 Tf 20 150 Td (Fonte incrustada) Tj ET\n"
        b"BT /F1 12 Tf 20 80 Td (glifos de verdade) Tj ET\n"
    )
    objetos = [
        (1, b"<< /Type /Catalog /Pages 2 0 R >>"),
        (2, b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
        (3, b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] "
            b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"),
        (4, b"<< /Length %d >>\nstream\n" % len(conteudo) + conteudo + b"endstream"),
        (5, b"<< /Type /Font /Subtype /TrueType /BaseFont /DejaVuSans "
            b"/FirstChar 32 /LastChar 126 /Widths [%s] /FontDescriptor 6 0 R "
            b"/Encoding /WinAnsiEncoding >>" % (" ".join(["600"] * 95)).encode()),
        (6, b"<< /Type /FontDescriptor /FontName /DejaVuSans /Flags 32 "
            b"/FontBBox [-1021 -463 1793 1536] /ItalicAngle 0 /Ascent 928 "
            b"/Descent -236 /CapHeight 729 /StemV 80 /FontFile2 7 0 R >>"),
        (7, b"<< /Length %d >>\nstream\n" % len(programa) + programa + b"endstream"),
    ]
    return _montar(objetos)


def cifrado(contrasena="sagui"):
    """Um PDF protegido: RC4 de 40 bits, V1/R2 (o gestor padrão da norma)."""
    # As permissões: imprimir e copiar, sem modificar.
    p = -1
    o = _rc4(b"\x00" * 32, PAD)   # o /O é aleatorio na norma; um fixo serve

    # A chave de arquivo (algoritmo 2 da norma).
    senha_pad = (contrasena.encode("latin-1") + PAD)[:32]
    chave = _md5(senha_pad + o + (p & 0xFFFFFF).to_bytes(3, "little") + ID_)[:5]
    u = _rc4(chave, PAD)

    def chave_objeto(num, gen=0):
        h = _md5(chave + num.to_bytes(3, "little") + gen.to_bytes(2, "little"))
        return h[:min(len(chave) + 5, 16)]

    conteudo = (
        b"BT /F1 18 Tf 20 140 Td (PDF protegido) Tj ET\n"
        b"BT /F1 12 Tf 20 100 Td (abre com a palavra-passe) Tj ET\n"
    )
    conteudo_c = _rc4(chave_objeto(4), conteudo)
    objetos = [
        (1, b"<< /Type /Catalog /Pages 2 0 R >>"),
        (2, b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
        (3, b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] "
            b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"),
        (4, b"<< /Length %d >>\nstream\n" % len(conteudo_c) + conteudo_c + b"endstream"),
        (5, b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
        (6, b"<< /Filter /Standard /V 1 /R 2 /O <%s> /U <%s> /P %d >>"
            % (o.hex().encode(), u.hex().encode(), p)),
    ]
    return _montar(objetos, trailer_extra="/Encrypt 6 0 R ", id_=ID_)


def cifrado_aes256(contrasena="sagui"):
    """Um PDF protegido com AES-256: V=5, revisão 6 (a mais moderna).

    Aqui a chave de arquivo não sai do MD5 da norma antiga: o "hash 2.B" da
    ISO 32000-2 dá a chave intermédia, que descifra o /UE, onde vai a chave
    de arquivo. O conteúdo dos streams vai cifrado com AES-256-CBC e um IV
    novo para cada um."""
    pwd = contrasena.encode("utf-8")
    # As chaves e os sais são fixos: o teste tem de dar sempre o mesmo.
    chave_arquivo = hashlib.sha256(b"chave de arquivo do teste do visor").digest()
    sal_validacao = bytes(range(0x10, 0x18))
    sal_chave = bytes(range(0x18, 0x20))
    sal_validacao_dono = bytes(range(0x20, 0x28))
    sal_chave_dono = bytes(range(0x28, 0x30))

    # O /U: o hash da senha com o sal de validação, mais os dois sais.
    hash_u = aes256.hash_2b(pwd, sal_validacao)
    u = hash_u + sal_validacao + sal_chave
    # O /UE: a chave de arquivo cifrada com a chave intermédia.
    intermedia = aes256.hash_2b(pwd, sal_chave)
    ue = aes256.cifrar_cbc(intermedia, b"\x00" * 16, chave_arquivo)

    # O /O e o /OE: o mesmo, mas com a senha de dono e com o /U dentro.
    hash_o = aes256.hash_2b(pwd, sal_validacao_dono, u[:48])
    o = hash_o + sal_validacao_dono + sal_chave_dono
    intermedia_dono = aes256.hash_2b(pwd, sal_chave_dono, u[:48])
    oe = aes256.cifrar_cbc(intermedia_dono, b"\x00" * 16, chave_arquivo)

    conteudo = (
        b"BT /F1 18 Tf 20 140 Td (PDF com AES-256) Tj ET\n"
        b"BT /F1 12 Tf 20 100 Td (revisao 6 do gestor) Tj ET\n"
    )
    iv = bytes(range(0x40, 0x50))
    conteudo_c = iv + aes256.cifrar_cbc(chave_arquivo, iv, aes256.preencher(conteudo))

    objetos = [
        (1, b"<< /Type /Catalog /Pages 2 0 R >>"),
        (2, b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
        (3, b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] "
            b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"),
        (4, b"<< /Length %d >>\nstream\n" % len(conteudo_c) + conteudo_c + b"endstream"),
        (5, b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
        (6, b"<< /Filter /Standard /V 5 /R 6 /Length 256 "
            b"/CF << /StdCF << /CFM /AESV3 /AuthEvent /DocOpen /Length 32 >> >> "
            b"/StmF /StdCF /StrF /StdCF "
            b"/O <%s> /OE <%s> /U <%s> /UE <%s> /P -4 >>"
            % (o.hex().encode(), oe.hex().encode(), u.hex().encode(), ue.hex().encode())),
    ]
    return _montar(objetos, trailer_extra="/Encrypt 6 0 R ", id_=ID_)


if __name__ == "__main__":
    import sys
    destino = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(destino, exist_ok=True)
    with open(os.path.join(destino, "simples.pdf"), "wb") as f:
        f.write(simples())
    fonte = com_fonte()
    if fonte:
        with open(os.path.join(destino, "fonte.pdf"), "wb") as f:
            f.write(fonte)
    with open(os.path.join(destino, "cifrado.pdf"), "wb") as f:
        f.write(cifrado())
    print("PDF de teste em", destino)


def com_indice(paginas=6):
    """Un documento de varias páginas con texto, un índice anidado y destinos
    de los tres tipos (array directo, acción GoTo y destino con nombre), para
    la interfaz del visor (mejora 105).

    Páginas de 300x400 con una frase cada una; la palabra «zorro» sale en las
    páginas 3 y 5 (dos veces en la 3) y «Atención» lleva tilde, para la
    búsqueda sin distinguir mayúsculas ni acentos."""
    frases = [
        b"Capitulo uno: la introduccion",
        b"Seccion uno punto uno del manual",
        b"El zorro rapido salta. Otro zorro corre.",
        b"Atencion: aviso importante del capitulo tres",
        b"Capitulo cuatro habla del zorro dormido",
        b"Ultima pagina del documento",
    ][:paginas]
    objetos = [None, None]
    base = 10
    ids_pag = [base + 2 * i for i in range(paginas)]
    kids = b" ".join(b"%d 0 R" % n for n in ids_pag)
    objetos.append((2, b"<< /Type /Pages /Kids [" + kids + b"] /Count %d >>" % paginas))
    for i, fr in enumerate(frases):
        conteudo = (b"BT /F1 20 Tf 30 340 Td (Pagina %d) Tj ET\n"
                    b"0.2 0.4 0.8 rg 30 250 120 40 re f\n0 g\n"
                    b"BT /F1 12 Tf 30 200 Td (" % (i + 1)) + fr + b") Tj ET\n"
        objetos.append((ids_pag[i], b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 400] "
                        b"/Contents %d 0 R /Resources << /Font << /F1 5 0 R >> >> >>" % (ids_pag[i] + 1)))
        objetos.append((ids_pag[i] + 1, b"<< /Length %d >>\nstream\n" % len(conteudo) + conteudo + b"endstream"))
    objetos.append((5, b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"))
    # El índice: Capitulo 1 (p1) > Seccion 1.1 (p2); Capitulo 2 (destino con
    # nombre, p3); Capitulo 3 (acción GoTo, p4).
    objetos.append((6, b"<< /Type /Outlines /First 21 0 R /Last 24 0 R /Count 4 >>"))
    objetos.append((21, b"<< /Title (Capitulo 1) /Parent 6 0 R /Next 23 0 R /First 22 0 R /Last 22 0 R "
                        b"/Dest [%d 0 R /Fit] >>" % ids_pag[0]))
    objetos.append((22, b"<< /Title (Seccion 1.1) /Parent 21 0 R /Dest [%d 0 R /Fit] >>" % ids_pag[1]))
    objetos.append((23, b"<< /Title (Capitulo 2) /Parent 6 0 R /Prev 21 0 R /Next 24 0 R /Dest /cap2 >>"))
    objetos.append((24, b"<< /Title (Cap\xedtulo 3) /Parent 6 0 R /Prev 23 0 R "
                        b"/A << /S /GoTo /D [%d 0 R /XYZ 0 400 0] >> >>" % ids_pag[3]))
    objetos.append((7, b"<< /cap2 [%d 0 R /Fit] >>" % ids_pag[2]))
    objetos.append((1, b"<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R /Dests 7 0 R >>"))
    objetos = [o for o in objetos if o]
    objetos.sort(key=lambda o: o[0])
    return _montar(objetos, trailer_extra="")
