#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H07 - teclado PS/2 com ABNT2 e ES.

Critério: no QEMU, teclas enviadas pelo monitor aparecem corretas na tela,
incluindo ç (e ñ no ES). O teste cobre todas as teclas da tabela do PRD 4.4,
as teclas mortas (combinações válidas e inválidas), Shift, Caps Lock, AltGr
e Ctrl, nos dois layouts (autotestes "teclado" = ABNT2 e "teclado_es").

O QEMU nomeia as teclas pela posição física. Num teclado ABNT2 a tecla na
posição do ';' americano é o ç, a do '[' é o acento agudo etc.; no ES a
mesma tecla é o ñ, e o acento agudo fica na posição do apóstrofo.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

# (teclas do QEMU, o que o SaguiOS deve produzir em Latin-1, descrição)
ROTEIRO = [
    (["o", "l", "a"], "ola", "letras"),
    (["shift-a"], "A", "Shift"),
    (["semicolon"], "ç", "ç (0x27)"),
    (["shift-semicolon"], "Ç", "Ç"),
    (["bracket_left", "a"], "á", "´ + a"),
    (["bracket_left", "e"], "é", "´ + e"),
    (["bracket_left", "i", "bracket_left", "o", "bracket_left", "u"], "íóú", "´ + i o u"),
    (["shift-bracket_left", "a"], "à", "` + a"),
    (["shift-apostrophe", "a", "shift-apostrophe", "e", "shift-apostrophe", "o"], "âêô", "^ + a e o"),
    (["apostrophe", "a", "apostrophe", "o"], "ãõ", "~ + a o"),
    (["shift-6", "u"], "ü", "¨ + u"),
    (["bracket_left", "x"], "´x", "combinação inválida mostra acento e letra"),
    (["apostrophe", "spc"], "~", "morta + espaço = o próprio acento"),
    (["shift-bracket_left", "shift-a"], "À", "` + A"),
    (["grave_accent", "shift-grave_accent"], "'\"", "' e \" (0x29)"),
    (["bracket_right", "shift-bracket_right"], "[{", "[ { (0x1B)"),
    (["backslash", "shift-backslash"], "]}", "] } (0x2B)"),
    (["less", "shift-less"], "\\|", "\\ | (0x56)"),
    (["comma", "shift-comma", "dot", "shift-dot"], ",<.>", ", < . > (0x33, 0x34)"),
    (["slash", "shift-slash"], ";:", "; : (0x35)"),
    (["ro", "shift-ro"], "/?", "/ ? (0x73)"),
    (["shift-1", "shift-2", "shift-7", "minus", "shift-equal"], "!@&-+", "símbolos com Shift"),
    (["caps_lock", "b", "semicolon", "shift-c", "caps_lock", "d"], "BÇcd", "Caps Lock (letras e ç; Shift inverte)"),
    (["alt_r-q", "alt_r-e"], "/°", "AltGr (/ e °) = Alt da direita"),
    (["ctrl-c"], "\x03", "Ctrl+C vira o código 3"),
]

ROTEIRO_ES = [
    (["o", "l", "a"], "ola", "letras"),
    (["shift-a"], "A", "Shift"),
    (["semicolon", "shift-semicolon"], "ñÑ", "ñ Ñ (0x27)"),
    (["backslash", "shift-backslash"], "çÇ", "ç Ç (0x2B)"),
    (["apostrophe", "a", "apostrophe", "e", "apostrophe", "i", "apostrophe", "o", "apostrophe", "u"], "áéíóú", "´ + vogais"),
    (["apostrophe", "shift-a"], "Á", "´ + A"),
    (["bracket_left", "a"], "à", "` + a"),
    (["shift-bracket_left", "o"], "ô", "^ + o"),
    (["shift-apostrophe", "u"], "ü", "¨ + u"),
    (["alt_r-4", "a", "alt_r-4", "n"], "ãñ", "~ morto (AltGr+4) + a n"),
    (["apostrophe", "x"], "´x", "combinação inválida mostra acento e letra"),
    (["bracket_left", "spc"], "`", "morta + espaço = o próprio acento"),
    (["minus", "shift-minus"], "'?", "' ? (0x0C)"),
    (["equal", "shift-equal"], "¡¿", "¡ ¿ (0x0D)"),
    (["grave_accent", "shift-grave_accent"], "ºª", "º ª (0x29)"),
    (["shift-2", "shift-3", "shift-7", "shift-0"], "\"·/=", "símbolos com Shift"),
    (["bracket_right", "shift-bracket_right"], "+*", "+ * (0x1B)"),
    (["less", "shift-less"], "<>", "< > (0x56)"),
    (["comma", "shift-comma", "dot", "shift-dot"], ",;.:", ", ; . : (0x33, 0x34)"),
    (["slash", "shift-slash"], "-_", "- _ (0x35)"),
    (["alt_r-2", "alt_r-3", "alt_r-1", "alt_r-grave_accent"], "@#|\\", "AltGr: @ # | \\"),
    (["alt_r-bracket_left", "alt_r-bracket_right", "alt_r-apostrophe", "alt_r-backslash"], "[]{}", "AltGr: [ ] { }"),
    (["caps_lock", "b", "semicolon", "shift-c", "caps_lock", "d"], "BÑcd", "Caps Lock (letras e ñ; Shift inverte)"),
    (["ctrl-c"], "\x03", "Ctrl+C vira o código 3"),
]


def rodar(autoteste, roteiro, captura):
    img = imagem_de_teste(autoteste)
    with Maquina("h07", "ide", imagem=img) as m:
        m.esperar_serial("[TESTE] teclado pronto", 60)
        esperado = ""
        for teclas, saida, _ in roteiro:
            for t in teclas:
                m.tecla(t, 0.05)
            esperado += saida
        m.captura(captura)
        m.tecla("ret")
        m.esperar_serial("[TESTE] teclado linha=", 30)
        linha = m.serial().split("[TESTE] teclado linha=")[1].split()[0]
        obtido = bytes.fromhex(linha).decode("latin-1")
        # Confere item a item para a mensagem de erro apontar a tecla certa.
        pos = 0
        for teclas, saida, desc in roteiro:
            pedaco = obtido[pos:pos + len(saida)]
            verificar(pedaco == saida, f"{autoteste}: {desc}: {saida!r} (obtido {pedaco!r})")
            pos += len(saida)
        verificar(obtido == esperado, f"{autoteste}: linha completa idêntica à esperada")


def main():
    rodar("teclado", ROTEIRO, "H07-teclado-abnt2")
    rodar("teclado_es", ROTEIRO_ES, "H07-teclado-es")
    return 0


if __name__ == "__main__":
    sys.exit(main())
