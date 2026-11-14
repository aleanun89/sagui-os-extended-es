; SPDX-License-Identifier: GPL-2.0-or-later
; prueba-chip8.asm - prova das 35 instrucoes do CHIP-8 e dos quirks do perfil
; "chip8" (COSMAC VIP): deslocamento usa VY, FX55/FX65 incrementam I, BNNN
; soma V0, o sprite e cortado na borda, 8XY1/2/3 zeram VF.
;
; Resultado esperado com "chip8 -t 600 prueba-chip8.ch8": vE = 33 provas
; (0x21), vD = 0 falhas, vB = 0. Com outros quirks, falham provas
; conhecidas (o teste h63 confere quais). Ver comum.asm.

    ld vB, 0
    ld vD, 0
    ld vE, 0

; 1: 6XNN e 7XNN (7XNN nao mexe em VF)
    add vE, 1
    ld vF, 7
    ld v0, 5
    add v0, 250
    se v0, 255
    call falhou
    add v0, 2
    se v0, 1
    call falhou
    se vF, 7
    call falhou

; 2: 3XNN pula quando igual, e so entao
    add vE, 1
    ld v0, 0x42
    ld v1, 0
    se v0, 0x42
    ld v1, 1
    se v1, 0
    call falhou
    ld v1, 0
    se v0, 0x43
    ld v1, 1
    se v1, 1
    call falhou

; 3: 4XNN
    add vE, 1
    ld v1, 0
    sne v0, 0x43
    ld v1, 1
    se v1, 0
    call falhou
    ld v1, 0
    sne v0, 0x42
    ld v1, 1
    se v1, 1
    call falhou

; 4: 5XY0
    add vE, 1
    ld v0, 9
    ld v1, 9
    ld v2, 0
    se v0, v1
    ld v2, 1
    se v2, 0
    call falhou
    ld v1, 8
    ld v2, 0
    se v0, v1
    ld v2, 1
    se v2, 1
    call falhou

; 5: 9XY0
    add vE, 1
    ld v2, 0
    sne v0, v1
    ld v2, 1
    se v2, 0
    call falhou
    ld v1, 9
    ld v2, 0
    sne v0, v1
    ld v2, 1
    se v2, 1
    call falhou

; 6: 8XY0
    add vE, 1
    ld v1, 0x5A
    ld v0, v1
    se v0, 0x5A
    call falhou

; 7: 8XY1 (e VF zerado)
    add vE, 1
    ld v0, 0x0F
    ld v1, 0xF0
    ld vF, 5
    or v0, v1
    se v0, 0xFF
    call falhou
    se vF, 0
    call falhou

; 8: 8XY2
    add vE, 1
    ld v0, 0x3C
    ld v1, 0x0F
    ld vF, 5
    and v0, v1
    se v0, 0x0C
    call falhou
    se vF, 0
    call falhou

; 9: 8XY3
    add vE, 1
    ld v0, 0x3C
    ld v1, 0x0F
    ld vF, 5
    xor v0, v1
    se v0, 0x33
    call falhou
    se vF, 0
    call falhou

; 10: 8XY4 com e sem vai-um
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
    add v0, v1
    se v0, 30
    call falhou
    se vF, 0
    call falhou

; 11: 8XY4 com X = F: vale o vai-um
    add vE, 1
    ld vF, 200
    ld v1, 100
    add vF, v1
    se vF, 1
    call falhou

; 12: 8XY5
    add vE, 1
    ld v0, 10
    ld v1, 20
    sub v0, v1
    se v0, 246
    call falhou
    se vF, 0
    call falhou
    ld v0, 20
    ld v1, 10
    sub v0, v1
    se v0, 10
    call falhou
    se vF, 1
    call falhou
    ld v0, 5
    ld v1, 5
    sub v0, v1
    se v0, 0
    call falhou
    se vF, 1
    call falhou

; 13: 8XY7
    add vE, 1
    ld v0, 10
    ld v1, 20
    subn v0, v1
    se v0, 10
    call falhou
    se vF, 1
    call falhou
    ld v0, 20
    ld v1, 10
    subn v0, v1
    se v0, 246
    call falhou
    se vF, 0
    call falhou

; 14: 8XY6 desloca VY (quirk desplazar=vy)
    add vE, 1
    ld v0, 0xFF
    ld v1, 5
    shr v0, v1
    se v0, 2
    call falhou
    se vF, 1
    call falhou

; 15: 8XYE desloca VY
    add vE, 1
    ld v0, 0
    ld v1, 0x81
    shl v0, v1
    se v0, 2
    call falhou
    se vF, 1
    call falhou

; 16: ANNN, FX1E e FX65 de um registro
    add vE, 1
    ld i, dados
    ld v0, 3
    add i, v0
    ld v0, [i]
    se v0, 0x44
    call falhou

; 17: FX55/FX65 deixam I depois do ultimo registro (quirk memoria=incrementa)
    add vE, 1
    ld i, buffer
    ld v0, 1
    ld v1, 2
    ld v2, 3
    ld [i], v2
    ld v0, 0x99
    ld [i], v0
    ld i, buffer
    ld v3, [i]
    se v0, 1
    call falhou
    se v3, 0x99
    call falhou

; 18: FX33
    add vE, 1
    ld v0, 254
    ld i, buffer
    ld b, v0
    ld v2, [i]
    se v0, 2
    call falhou
    se v1, 5
    call falhou
    se v2, 4
    call falhou

; 19: FX29 (fonte pequena)
    add vE, 1
    ld v0, 1
    ld f, v0
    ld v0, [i]
    se v0, 0x20
    call falhou
    ld v0, 0xF
    ld f, v0
    ld v0, [i]
    se v0, 0xF0
    call falhou

; 20: 2NNN e 00EE
    add vE, 1
    ld v0, 0
    call sub_42
    se v0, 0x42
    call falhou

; 21: BNNN soma V0 (quirk salto=v0); os outros registros valem 0
    add vE, 1
    ld v0, 4
    ld v1, 0
    ld v2, 0
    ld v3, 0
    ld v4, 0
    ld v5, 0
    ld v6, 0
    ld v7, 0
    ld v8, 0
    ld v9, 0
    jp v0, tabela_salto
volta_salto:
    se v1, 2
    call falhou

; 22: CXNN
    add vE, 1
    rnd v0, 0
    se v0, 0
    call falhou
    rnd v0, 0x0F
    ld v1, 0xF0
    and v1, v0
    se v1, 0
    call falhou

; 23: DXYN e colisao
    add vE, 1
    cls
    ld i, um_byte
    ld v0, 0
    ld v1, 0
    drw v0, v1, 1
    se vF, 0
    call falhou
    drw v0, v1, 1
    se vF, 1
    call falhou

; 24: sprite cortado na borda direita (quirk borde=recorta)
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

; 25: e na borda de baixo
    add vE, 1
    cls
    ld i, dois
    ld v0, 10
    ld v1, 31
    drw v0, v1, 2
    ld i, pixel
    ld v1, 0
    drw v0, v1, 1
    se vF, 0
    call falhou

; 26: o ponto de partida da a volta (69 -> 5, 34 -> 2)
    add vE, 1
    cls
    ld i, pixel
    ld v0, 69
    ld v1, 34
    drw v0, v1, 1
    ld v0, 5
    ld v1, 2
    drw v0, v1, 1
    se vF, 1
    call falhou

; 27: FX15 e FX07: o retardo desce sozinho
    add vE, 1
    ld v0, 3
    ld dt, v0
    ld v1, 0
espera_dt:
    ld v0, dt
    add v1, 1
    se v0, 0
    jp espera_dt
    ld v2, 1
    sub v2, v1              ; VF = 1 se v1 <= 1: nao esperou nada
    se vF, 0
    call falhou

; 28: FX18
    add vE, 1
    ld v0, 2
    ld st, v0

; 29: EX9E e EXA1 sem tecla apertada
    add vE, 1
    ld v0, 5
    ld v1, 0
    skp v0
    ld v1, 1
    se v1, 1
    call falhou
    ld v1, 0
    sknp v0
    ld v1, 1
    se v1, 0
    call falhou

; 30: 0NNN e ignorado no CHIP-8
    add vE, 1
    ld v0, 7
    sys 0x123
    se v0, 7
    call falhou

; 31: 8XY5 com X = F: vale o "nao emprestou"
    add vE, 1
    ld vF, 20
    ld v1, 5
    sub vF, v1
    se vF, 1
    call falhou

; 32: FX1E soma em I sem mexer em VF
    add vE, 1
    ld vF, 9
    ld i, dados
    ld v0, 1
    add i, v0
    se vF, 9
    call falhou
    ld v0, [i]
    se v0, 0x22
    call falhou

; 33: 00E0 apaga a tela
    add vE, 1
    ld i, pixel
    ld v0, 3
    ld v1, 3
    drw v0, v1, 1
    cls
    drw v0, v1, 1
    se vF, 0
    call falhou

    call resultado
fim:
    jp fim

sub_42:
    ld v0, 0x42
    ret

tabela_salto:
    ld v1, 1                ; +0: aqui chega quem somou outro registro
    jp volta_salto
    ld v1, 2                ; +4: aqui chega quem somou V0 = 4
    jp volta_salto

include "comum.asm"
