#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""AES-256 em modo CBC, escrito à mão (só para os testes do visor de PDF).

O projeto não pode depender de bibliotecas de terceiros, e os PDF de teste
precisam de um PDF cifrado com AES-256 (V=5, revisão 6, o "hash 2.B" da
ISO 32000-2). Por isso aqui vai um AES escrito de raiz: a tabela S sai da
inversa multiplicativa em GF(2^8) com a transformação afim, e a cifragem é o
cifrador de blocos da norma FIPS 197.

Só faz falta cifrar (é o que usa o teste), não decifrar.
"""
import os
import sys

def _gf_mul(a, b):
    """O produto em GF(2^8) com o polinómio irredutível x^8+x^4+x^3+x+1."""
    p = 0
    for _ in range(8):
        if b & 1:
            p ^= a
        alto = a & 0x80
        a = (a << 1) & 0xFF
        if alto:
            a ^= 0x1B
        b >>= 1
    return p


def _inversa(a):
    """A inversa multiplicativa (a^254)."""
    if a == 0:
        return 0
    r = 1
    for _ in range(254):
        r = _gf_mul(r, a)
    return r


def _tabela_s():
    """A tabela S do AES: a inversa seguida da transformação afim."""
    tabela = []
    for x in range(256):
        b = _inversa(x)
        s = b
        for _ in range(4):
            b = ((b << 1) | (b >> 7)) & 0xFF
            s ^= b
        tabela.append(s ^ 0x63)
    return tabela


S = _tabela_s()


def _expandir_chave(chave):
    """A expansão da chave do AES: 15 chaves de ronda para 256 bits, 11 para 128."""
    nk = len(chave) // 4
    nr = nk + 6
    palavras = [list(chave[4 * i:4 * i + 4]) for i in range(nk)]
    rcon = 1
    for i in range(nk, 4 * (nr + 1)):
        temp = list(palavras[i - 1])
        if i % nk == 0:
            temp = temp[1:] + temp[:1]                 # RotWord
            temp = [S[b] for b in temp]                # SubWord
            temp[0] ^= rcon
            rcon = _gf_mul(rcon, 2)
        elif i % nk == 4:
            temp = [S[b] for b in temp]
        palavras.append([palavras[i - nk][j] ^ temp[j] for j in range(4)])
    return [sum(palavras[4 * r:4 * r + 4], []) for r in range(nr + 1)]


def _cifrar_bloco(chave_rondas, bloco):
    """Cifra um bloco de 16 bytes (o estado é por colunas)."""
    estado = list(bloco)
    for i in range(16):
        estado[i] ^= chave_rondas[0][i]
    for r in range(1, len(chave_rondas)):
        estado = [S[b] for b in estado]
        # ShiftRows: a linha k roda k posições para a esquerda.
        novo = [0] * 16
        for c in range(4):
            for k in range(4):
                novo[4 * c + k] = estado[4 * ((c + k) % 4) + k]
        estado = novo
        if r != len(chave_rondas) - 1:
            # MixColumns.
            novo = [0] * 16
            for c in range(4):
                a = estado[4 * c:4 * c + 4]
                s = a[0] ^ a[1] ^ a[2] ^ a[3]
                novo[4 * c + 0] = a[0] ^ s ^ _gf_mul(0x02, a[0] ^ a[1])
                novo[4 * c + 1] = a[1] ^ s ^ _gf_mul(0x02, a[1] ^ a[2])
                novo[4 * c + 2] = a[2] ^ s ^ _gf_mul(0x02, a[2] ^ a[3])
                novo[4 * c + 3] = a[3] ^ s ^ _gf_mul(0x02, a[3] ^ a[0])
            estado = novo
        for i in range(16):
            estado[i] ^= chave_rondas[r][i]
    return bytes(estado)


def cifrar_cbc(chave, iv, dados):
    """AES-256-CBC sem preenchimento (o padding põe-se à parte)."""
    if len(dados) % 16:
        raise ValueError("o bloco tem de ser múltiplo de 16")
    rondas = _expandir_chave(chave)
    saida = bytearray()
    anterior = bytes(iv)
    for i in range(0, len(dados), 16):
        bloco = bytes(dados[i:i + 16])
        x = bytes(a ^ b for a, b in zip(bloco, anterior))
        cif = _cifrar_bloco(rondas, x)
        saida += cif
        anterior = cif
    return bytes(saida)


def hash_2b(senha, sal, dados=b"", nbytes=16):
    """O "hash 2.B" da ISO 32000-2 (revisão 6): o ciclo de 64 voltas."""
    import hashlib
    k = hashlib.sha256(senha + sal + dados).digest()
    bloco = 32
    i = 0
    while True:
        conteudo = (senha + k[:bloco] + dados) * 64
        e = cifrar_cbc(k[:16], k[16:32], conteudo)
        resto = int.from_bytes(e[:nbytes], "big") % 3
        if resto == 0:
            k = hashlib.sha256(e).digest()
            bloco = 32
        elif resto == 1:
            k = hashlib.sha384(e).digest()
            bloco = 48
        else:
            k = hashlib.sha512(e).digest()
            bloco = 64
        i += 1
        if not (i < 64 or i - 32 < e[-1]):
            break
    return k[:32]


def preencher(dados):
    """O preenchimento PKCS#5: um byte com quantos faltam."""
    falta = 16 - (len(dados) % 16)
    return dados + bytes([falta]) * falta


if __name__ == "__main__":
    # A prova: o mesmo resultado que uma biblioteca de referência.
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    chave = bytes(range(32))
    iv = bytes(range(16))
    dados = preencher(b"Macaco eXcel Sheet" * 5)
    meu = cifrar_cbc(chave, iv, dados)
    c = Cipher(algorithms.AES(chave), modes.CBC(iv)).encryptor()
    deles = c.update(dados) + c.finalize()
    print("meu  :", meu.hex())
    print("deles:", deles.hex())
    print("iguais" if meu == deles else "DIFERENTES")
    # E o hash 2.B contra o ficheiro de teste do PDFium.
    u = bytes.fromhex("0798ab4b1c93d360f96b8ba41d1add5b7eaf4b110f014de88a57615fdd6f677c"
                      "1a5e059d15ed6eed88d94c0349583f86")
    h = hash_2b("hôtel".encode("utf-8"), u[32:40])
    print("hash 2.B:", h.hex())
    print("confere" if h == u[:32] else "DIFERENTE")
