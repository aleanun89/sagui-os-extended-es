; SPDX-License-Identifier: GPL-2.0-or-later
; prueba-xochip.asm - prova do XO-CHIP e dos quirks do perfil "xochip"
; (Octo): 64 KB e F000 NNNN, 5XY2/5XY3, os dois planos (FN01), 00E0 e
; rolamento so nos planos escolhidos, 00DN, sprite que da a volta na borda,
; F002 e FX3A; deslocamento usa VY e FX55/FX65 incrementam I.
;
; Resultado esperado com "chip8 -t 60 prueba-xochip.xo8": vE = 16 provas
; (0x10), vD = 0, vB = 0, tono 64 e sonido 250 Hz (o padrao 00FF repetido
; sobe 8 vezes em 128 bits: 4000 / 16). Ver comum.asm.

    ld vB, 0
    ld vD, 0
    ld vE, 0

; 1: F000 NNNN e 5XY2/5XY3 acima de 4 KB (sem mexer em I)
    add vE, 1
    ld i, long 0x8000
    ld v0, 0x5A
    ld v1, 0xA5
    save v0 - v1
    ld v0, 0
    ld v1, 0
    load v0 - v1
    se v0, 0x5A
    call falhou
    se v1, 0xA5
    call falhou
    ld v0, 0
    ld v0, [i]              ; I continua em 0x8000
    se v0, 0x5A
    call falhou

; 2: um pulo passa por cima dos 4 bytes do F000 NNNN
    add vE, 1
    ld i, long dado_a
    ld v0, 0
    se v0, v0
    ld i, long dado_b
    ld v0, [i]
    se v0, 0x11
    call falhou
    ld i, long dado_a
    sne v0, 0x55
    ld i, long dado_b
    ld v0, [i]
    se v0, 0x11
    call falhou

; 3: 5XY2 com X > Y grava ao contrario
    add vE, 1
    ld v1, 1
    ld v2, 2
    ld v3, 3
    ld i, buffer
    save v3 - v1
    load v4 - v6
    se v4, 3
    call falhou
    se v6, 1
    call falhou

; 4: o sprite da a volta na borda direita (quirk borde=envuelve)
    add vE, 1
    cls
    ld i, um_byte
    ld v0, 60
    ld v1, 0
    drw v0, v1, 1
    ld i, pixel
    ld v0, 0
    drw v0, v1, 1
    se vF, 1
    call falhou

; 5: e na de baixo
    add vE, 1
    cls
    ld i, dois
    ld v0, 10
    ld v1, 31
    drw v0, v1, 2
    ld i, pixel
    ld v1, 0
    drw v0, v1, 1
    se vF, 1
    call falhou

; 6: cada plano tem os seus pixels
    add vE, 1
    plane 3
    cls
    plane 1
    ld i, pixel
    ld v0, 5
    ld v1, 5
    drw v0, v1, 1
    plane 2
    drw v0, v1, 1
    se vF, 0
    call falhou
    drw v0, v1, 1
    se vF, 1
    call falhou

; 7: com os dois planos, o sprite tem uma parte para cada um
    add vE, 1
    plane 3
    cls
    ld i, dois_planos
    ld v0, 6
    ld v1, 6
    drw v0, v1, 1
    plane 2
    ld i, pixel
    drw v0, v1, 1
    se vF, 0
    call falhou
    plane 1
    drw v0, v1, 1
    se vF, 1
    call falhou

; 8: 00E0 apaga so o plano escolhido
    add vE, 1
    plane 3
    cls
    ld i, dois
    ld v0, 7
    ld v1, 7
    drw v0, v1, 1
    plane 1
    cls
    plane 2
    ld i, pixel
    drw v0, v1, 1
    se vF, 1
    call falhou
    plane 1
    drw v0, v1, 1
    se vF, 0
    call falhou

; 9: 00DN rola para cima (em 128x64)
    add vE, 1
    plane 3
    high
    cls
    plane 1
    ld i, pixel
    ld v0, 20
    ld v1, 20
    drw v0, v1, 1
    scu 5
    ld v1, 15
    drw v0, v1, 1
    se vF, 1
    call falhou

; 10: rolar so mexe no plano escolhido
    add vE, 1
    plane 3
    cls
    plane 2
    ld v0, 30
    ld v1, 30
    drw v0, v1, 1
    plane 1
    scd 3
    plane 2
    drw v0, v1, 1
    se vF, 1
    call falhou

; 11: 8XY6 desloca VY (quirk desplazar=vy)
    add vE, 1
    plane 1
    ld v0, 0xFF
    ld v1, 5
    shr v0, v1
    se v0, 2
    call falhou

; 12: FX55/FX65 incrementam I (quirk memoria=incrementa)
    add vE, 1
    ld i, buffer
    ld v0, 7
    ld v1, 8
    ld [i], v1
    ld v0, 9
    ld [i], v0
    ld i, buffer
    ld v2, [i]
    se v2, 9
    call falhou

; 13: 8XY2 nao zera VF (quirk logicas=conserva)
    add vE, 1
    ld vF, 6
    ld v0, 3
    ld v1, 1
    and v0, v1
    se vF, 6
    call falhou

; 14: FX75/FX85 com mais de 8 registros
    add vE, 1
    ld vA, 0x5A
    ld v9, 0x59
    ld r, vA
    ld vA, 0
    ld v9, 0
    ld vA, r
    se vA, 0x5A
    call falhou
    se v9, 0x59
    call falhou

; 15: F002 e FX3A
    add vE, 1
    ld i, long padrao
    audio
    ld v0, 64
    pitch v0

; 16: BNNN soma V0 (quirk salto=v0)
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

    plane 3
    cls
    plane 1                 ; a fonte tem um plano so
    call resultado
    ld v0, 255              ; som ligado: o "-t" mostra a frequencia
    ld st, v0
fim:
    jp fim

tabela_salto:
    ld v1, 1
    jp volta_salto
    ld v1, 2
    jp volta_salto

dois_planos: db %X......., %........

include "comum.asm"

    org 0x1000
dado_a:  db 0x11
dado_b:  db 0x22
padrao:  db 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF
         db 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF
