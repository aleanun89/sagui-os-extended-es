#!/usr/bin/env python3
"""Genera userland/libsagui/audio_mp3_tabelas.c: las tablas numéricas del
decodificador MP3 (ISO/IEC 11172-3, mejora 053).

Casi todo se calcula aquí (cosenos, ventanas, potencias); solo dos cosas
son tablas puras de la norma, sin fórmula, y están transcritas como DATOS en
tools/dados/ (D109: el script ya no lee ninguna biblioteca del anfitrión):

  - tools/dados/mp3_huffman_b7.txt: las tablas de Huffman (anexo B, tabla
    B.7): longitudes y códigos, en el orden de la propia norma (tablas 1..33);
  - tools/dados/mp3_janela_b3.txt: la ventana de la síntesis polifásica (tabla
    B.3/C.1), 257 de los 512 coeficientes; el resto sale de la simetría.

Los datos pasan por verificaciones propias (Kraft completo o casi, código
sin prefijos, ventana múltiplo de 2^-16 y simétrica), y el decodificador
entero se compara con ffmpeg en tests/h75_codecs_audio.py.

Uso: tools/gerar_tabelas_mp3.py [saida.c]
"""
import math
import os
import sys

DADOS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dados")
SAIDA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "userland", "libsagui", "audio_mp3_tabelas.c")


def linhas_de_dados(nome):
    """Líneas útiles (sin comentarios ni vacías) de un archivo de tools/dados/."""
    with open(os.path.join(DADOS, nome), encoding="utf-8") as f:
        return [l.split() for l in f if l.strip() and not l.lstrip().startswith("#")]


def huffman():
    """Tablas de Huffman de mp3_huffman_b7.txt: {t: (xlen, ylen, linbits, códigos, longitudes)}."""
    fontes, tabelas = {}, {}
    atual = None
    for p in linhas_de_dados("mp3_huffman_b7.txt"):
        if p[0] == "tabela":
            t, xl, yl, lb, f = (int(x) for x in p[1:6])
            atual = t
            fontes[t] = (xl, yl, lb, f)
            if f == t:
                tabelas[t] = []
        else:
            x, y, hl = int(p[0]), int(p[1]), int(p[2])
            xl, yl = fontes[atual][:2]
            assert len(tabelas[atual]) == x * yl + y, "tabla %d: filas fuera de orden" % atual
            assert (len(p[3]) if p[3] != "-" else 0) == hl, "tabla %d: longitud del código" % atual
            tabelas[atual].append((int(p[3], 2) if hl else 0, hl))
    res = {}
    for t, (xl, yl, lb, f) in fontes.items():
        filas = tabelas[f]
        assert len(filas) == xl * yl, "tabla %d: faltan filas" % t
        res[t] = (xl, yl, lb, [c for c, _ in filas], [l for _, l in filas])
    return res


def verificar_huffman(tabelas):
    for t, (xl, yl, lb, cod, com) in tabelas.items():
        kraft = sum(2.0 ** -c for c in com if c)
        palavras = [(format(cd, "0%db" % c) if c else "") for cd, c in zip(cod, com)]
        for i, a in enumerate(palavras):
            assert not a or cod[i] < (1 << com[i]), (t, i)
            for j, b in enumerate(palavras):
                if i != j and a and b and b.startswith(a):
                    raise SystemExit("tabela %d: código %d é prefixo de %d" % (t, i, j))
        assert kraft <= 1.0 + 1e-12, (t, kraft)
        print("  tabela %2d: %3d códigos, soma de Kraft %.6f" % (t, len(com), kraft), file=sys.stderr)


def janela():
    """Los 512 coeficientes D[i] x 2^16 de la ventana, desde los 257 de mp3_janela_b3.txt."""
    D = [int(x) for p in linhas_de_dados("mp3_janela_b3.txt") for x in p]
    assert len(D) == 257, "mp3_janela_b3.txt: se esperaban 257 coeficientes"
    for k in range(1, 256):
        # simetría de la norma: D[512-k] = -D[k], salvo en los múltiplos de 64
        D.append(D[k] if k % 64 == 0 else -D[k])
    D = D[:257] + D[257:][::-1]
    assert D[256] == 75038 and D[0] == 0 and D[1] == -1
    assert len(D) == 512
    return D


def vetor(nome, tipo, valores, por_linha=8, hexa=False):
    linhas = []
    for i in range(0, len(valores), por_linha):
        fatia = valores[i:i + por_linha]
        linhas.append("    " + ", ".join(("0x%X" % v) if hexa else str(v) for v in fatia) + ",")
    return "const %s %s[%d] = {\n%s\n};\n" % (tipo, nome, len(valores), "\n".join(linhas))


Q = 1 << 30


def q30(x):
    return int(round(x * Q))


def main():
    print("Huffman (de tools/dados/mp3_huffman_b7.txt):", file=sys.stderr)
    hf = huffman()
    verificar_huffman(hf)
    D = janela()
    o = []
    o.append("""/* audio_mp3_tabelas.c - tablas numéricas del decodificador MP3 (mejora 053, D099).
 *
 * ARCHIVO GENERADO por tools/gerar_tabelas_mp3.py: no editar a mano.
 * Los cosenos, las ventanas y las potencias los calcula el script en
 * coma fija; las tablas de Huffman y la ventana de la síntesis polifásica
 * son las de la norma ISO/IEC 11172-3 (anexo B), ver la cabecera del script. */
#include "audio_mp3_tabelas.h"
""")
    # Huffman: un vector de longitudes y otro de códigos por tabla distinta
    distintos = [1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 24, 32, 33]
    for t in distintos:
        xl, yl, lb, cod, com = hf[t]
        o.append(vetor("mp3_hc%d" % t, "uint16_t", cod, 16))
        o.append(vetor("mp3_hl%d" % t, "uint8_t", com, 16))
    # la tabla de descriptores: 0..31 pares, 32 y 33 cuádruplas
    uso = {}
    for t in range(32):
        if t in hf:
            xl, yl, lb, cod, com = hf[t]
            fonte = t
            if 16 <= t <= 23:
                fonte = 16
            if 24 <= t <= 31:
                fonte = 24
            uso[t] = (fonte, xl, lb)
    o.append("const mp3_huf_t mp3_huf[34] = {")
    for t in range(34):
        if t in uso:
            f, xl, lb = uso[t]
            o.append("    { %d, %d, mp3_hc%d, mp3_hl%d }," % (xl, lb, f, f))
        elif t >= 32:
            o.append("    { 0, 0, mp3_hc%d, mp3_hl%d }," % (t, t))
        else:
            o.append("    { 0, 0, 0, 0 },")
    o.append("};\n")
    o.append(vetor("mp3_janela_d", "int32_t", D, 8))
    # x^(4/3): mantisa de 32 bits normalizada y exponente (valor = mant * 2^exp)
    mant, expo = [0], [0]
    for i in range(1, 8207):
        v = i ** (4.0 / 3.0)
        e = math.floor(math.log2(v)) - 31
        m = int(round(v / 2.0 ** e))
        if m >= 1 << 32:
            m >>= 1
            e += 1
        assert (1 << 31) <= m < (1 << 32)
        mant.append(m)
        expo.append(e)
    o.append(vetor("mp3_pot43_mant", "uint32_t", mant, 8, hexa=True))
    o.append(vetor("mp3_pot43_exp", "int8_t", expo, 24))
    o.append(vetor("mp3_ganho4", "uint32_t", [q30(2 ** (r / 4.0)) for r in range(4)], 4))
    # IMDCT de 36 puntos (18 entradas) y de 12 puntos (6 entradas), Q30
    c36 = [q30(math.cos(math.pi / 72 * (2 * i + 1 + 18) * (2 * k + 1))) for i in range(36) for k in range(18)]
    c12 = [q30(math.cos(math.pi / 24 * (2 * i + 1 + 6) * (2 * k + 1))) for i in range(12) for k in range(6)]
    o.append(vetor("mp3_imdct36", "int32_t", c36, 9))
    o.append(vetor("mp3_imdct12", "int32_t", c12, 6))
    # ventanas ya multiplicadas después: normal, inicio, fin (36) y corta (12)
    def sen36(i):
        return math.sin(math.pi / 36 * (i + 0.5))

    def sen12(i):
        return math.sin(math.pi / 12 * (i + 0.5))

    normal = [sen36(i) for i in range(36)]
    inicio = [sen36(i) if i < 18 else 1.0 if i < 24 else sen12(i - 18) if i < 30 else 0.0 for i in range(36)]
    fim = [0.0 if i < 6 else sen12(i - 6) if i < 12 else 1.0 if i < 18 else sen36(i) for i in range(36)]
    o.append(vetor("mp3_janela_longa", "int32_t", [q30(x) for w in (normal, inicio, fim) for x in w], 9))
    o.append(vetor("mp3_janela_curta", "int32_t", [q30(sen12(i)) for i in range(12)], 6))
    # reducción de alias
    ci = [-0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037]
    o.append(vetor("mp3_alias_cs", "int32_t", [q30(1 / math.sqrt(1 + c * c)) for c in ci], 8))
    o.append(vetor("mp3_alias_ca", "int32_t", [q30(c / math.sqrt(1 + c * c)) for c in ci], 8))
    # estéreo por intensidad (MPEG-1): is_pos 0..6 -> L y R
    esq, dire = [], []
    for p in range(7):
        if p == 6:
            esq.append(Q)
            dire.append(0)
        else:
            r = math.tan(p * math.pi / 12)
            esq.append(q30(r / (1 + r)))
            dire.append(q30(1 / (1 + r)))
    o.append(vetor("mp3_is_esq", "int32_t", esq, 7))
    o.append(vetor("mp3_is_dir", "int32_t", dire, 7))
    # MPEG-2: 2^(-n/4) y 2^(-n/2), n = 0..31 (n par/impar de la posición)
    o.append(vetor("mp3_is2_q", "int32_t", [q30(2 ** (-n / 4.0)) for n in range(32)], 8))
    o.append("const int32_t mp3_raiz_meio = %d;\n" % q30(math.sqrt(0.5)))
    # matriz de la síntesis polifásica: N[i][k] = cos((16+i)(2k+1)pi/64), 64x32, Q30
    nm = [q30(math.cos((16 + i) * (2 * k + 1) * math.pi / 64)) for i in range(64) for k in range(32)]
    o.append(vetor("mp3_sintese_n", "int32_t", nm, 8))
    saida = sys.argv[1] if len(sys.argv) > 1 else SAIDA
    open(saida, "w").write("\n".join(o))
    print("escrito", os.path.normpath(saida), file=sys.stderr)


main()
