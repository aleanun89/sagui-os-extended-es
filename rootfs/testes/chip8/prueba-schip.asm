; SPDX-License-Identifier: GPL-2.0-or-later
; prueba-schip.asm - prova do SUPER-CHIP 1.1 e dos quirks do perfil "schip"
; (HP-48): deslocamento usa VX, FX55/FX65 nao mexem em I, BXNN soma VX,
; logicas nao zeram VF; e das instrucoes novas: 00FF/00FE, 00CN, 00FB,
; 00FC, sprite 16x16, FX30, FX75/FX85 e 00FD (a ultima instrucao).
;
; Resultado esperado com "chip8 -t 300 prueba-schip.sc8": estado terminado
; (00FD), vE = 18 provas (0x12), vD = 0, vB = 0. Ver comum.asm.

    ld vB, 0
    ld vD, 0
    ld vE, 0

; 1: 8XY6 desloca o proprio VX (quirk desplazar=vx)
    add vE, 1
    ld v0, 5
    ld v1, 0x80
    shr v0, v1
    se v0, 2
    call falhou
    se vF, 1
    call falhou

; 2: 8XYE desloca o proprio VX
    add vE, 1
    ld v0, 0x81
    ld v1, 0
    shl v0, v1
    se v0, 2
    call falhou
    se vF, 1
    call falhou

; 3: FX55/FX65 nao mexem em I (quirk memoria=fija)
    add vE, 1
    ld i, buffer
    ld v0, 0x31
    ld [i], v0
    ld v0, 0x77
    ld [i], v0
    ld i, buffer
    ld v1, [i]
    se v0, 0x77
    call falhou
    se v1, 0
    call falhou

; 4: BXNN soma VX, com X = algarismo alto do endereco (quirk salto=vx).
; V0 = 0 e V1-V9 = 4: quem somar V0 cai em +0.
    add vE, 1
    ld v0, 0
    ld v1, 4
    ld v2, 4
    ld v3, 4
    ld v4, 4
    ld v5, 4
    ld v6, 4
    ld v7, 4
    ld v8, 4
    ld v9, 4
    jp v0, tabela_salto
volta_salto:
    se v1, 2
    call falhou

; 5: 8XY1 nao zera VF (quirk logicas=conserva)
    add vE, 1
    ld vF, 5
    ld v0, 1
    ld v1, 2
    or v0, v1
    se vF, 5
    call falhou
    se v0, 3
    call falhou

; 6: 00FF liga 128x64: (100, 50) e (36, 18) sao pixels diferentes
    add vE, 1
    high
    cls
    ld i, pixel
    ld v0, 100
    ld v1, 50
    drw v0, v1, 1
    ld v0, 36
    ld v1, 18
    drw v0, v1, 1
    se vF, 0
    call falhou

; 7: sprite de 16x16 (DXY0)
    add vE, 1
    cls
    ld i, bloco16
    ld v0, 0
    ld v1, 0
    drw v0, v1, 0
    se vF, 0
    call falhou
    ld i, pixel
    ld v0, 15
    ld v1, 15
    drw v0, v1, 1
    se vF, 1
    call falhou
    ld v0, 16
    drw v0, v1, 1
    se vF, 0
    call falhou

; 8: 00CN rola para baixo
    add vE, 1
    cls
    ld i, pixel
    ld v0, 10
    ld v1, 10
    drw v0, v1, 1
    scd 4
    ld v1, 14
    drw v0, v1, 1
    se vF, 1
    call falhou
    ld v1, 10
    drw v0, v1, 1
    se vF, 0
    call falhou

; 9: 00FB rola 4 pixels para a direita
    add vE, 1
    cls
    ld v0, 10
    ld v1, 10
    drw v0, v1, 1
    scr
    ld v0, 14
    drw v0, v1, 1
    se vF, 1
    call falhou

; 10: 00FC rola 4 pixels para a esquerda
    add vE, 1
    cls
    ld v0, 20
    ld v1, 10
    drw v0, v1, 1
    scl
    ld v0, 16
    drw v0, v1, 1
    se vF, 1
    call falhou

; 11: FX30 (fonte grande: o 1 comeca com 0x18)
    add vE, 1
    ld v0, 1
    ld hf, v0
    ld v0, [i]
    se v0, 0x18
    call falhou

; 12: FX75 e FX85
    add vE, 1
    ld v0, 0x11
    ld v1, 0x22
    ld v2, 0x33
    ld v3, 0x44
    ld r, v3
    ld v0, 0
    ld v1, 0
    ld v2, 0
    ld v3, 0
    ld v3, r
    se v0, 0x11
    call falhou
    se v3, 0x44
    call falhou

; 13: 00FE volta a 64x32: (100, 50) da a volta e cai em (36, 18)
    add vE, 1
    low
    cls
    ld i, pixel
    ld v0, 100
    ld v1, 50
    drw v0, v1, 1
    ld v0, 36
    ld v1, 18
    drw v0, v1, 1
    se vF, 1
    call falhou

; 14: no modo baixo o sprite tambem e cortado na borda
    add vE, 1
    cls
    ld i, um_byte
    ld v0, 60
    ld v1, 0
    drw v0, v1, 1
    ld i, pixel
    ld v0, 0
    drw v0, v1, 1
    se vF, 0
    call falhou

; 15: DXY0 no modo baixo tambem e 16x16
    add vE, 1
    cls
    ld i, bloco16
    ld v0, 0
    ld v1, 0
    drw v0, v1, 0
    ld i, pixel
    ld v0, 15
    ld v1, 15
    drw v0, v1, 1
    se vF, 1
    call falhou

; 16: 8XY4 e 8XY5 continuam iguais
    add vE, 1
    ld v0, 200
    ld v1, 100
    add v0, v1
    se v0, 44
    call falhou
    se vF, 1
    call falhou
    ld v0, 10
    ld v1, 20
    sub v0, v1
    se vF, 0
    call falhou

; 17: FX29 continua apontando a fonte pequena
    add vE, 1
    ld v0, 0xF
    ld f, v0
    ld v0, [i]
    se v0, 0xF0
    call falhou

; 18: 00EE e 2NNN aninhados
    add vE, 1
    ld v0, 0
    call sub_dupla
    se v0, 2
    call falhou

    high
    call resultado
    exit

sub_dupla:
    add v0, 1
    call sub_um
    ret
sub_um:
    add v0, 1
    ret

tabela_salto:
    ld v1, 1                ; +0: quem somou V0
    jp volta_salto
    ld v1, 2                ; +4: quem somou VX = 4
    jp volta_salto

include "comum.asm"
