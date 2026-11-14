; SPDX-License-Identifier: GPL-2.0-or-later
; comum.asm - o que as ROMs de prova do emulador CHIP-8 compartilham.
;
; Convencao: vE = numero da prova atual, vD = quantas falharam, vB = a
; primeira que falhou (0 = nenhuma), vC e temporario de "falhou". As provas
; usam v0-vA e vF. Cada verificacao e "se/sne ... / call falhou": o salto
; pula a chamada quando o resultado esta certo. "chip8 -t" mostra os
; registros no fim; a tela mostra vE, vD e vB em hexadecimal e um OK.

; Anota a falha da prova vE (sem mexer em v0).
falhou:
    sne vD, 0
    ld vB, vE
    ld vC, v0
    ld i, falhas
    add i, vD
    ld v0, vE
    ld [i], v0
    ld v0, vC
    add vD, 1
    ret

; Escreve v0 em hexadecimal (dois algarismos) em (vA, v9). Usa v1.
hex2:
    ld v1, v0
    shr v1
    shr v1
    shr v1
    shr v1
    ld f, v1
    drw vA, v9, 5
    add vA, 5
    ld v1, 15
    and v1, v0
    ld f, v1
    drw vA, v9, 5
    add vA, 7
    ret

; Tela final: provas, falhas, primeira falha e, se nao houve falha, "OK".
resultado:
    cls
    ld vA, 0
    ld v9, 0
    ld v0, vE
    call hex2
    ld v0, vD
    call hex2
    ld v0, vB
    call hex2
    se vD, 0
    ret
    ld i, spr_ok
    ld vA, 0
    ld v9, 8
    drw vA, v9, 5
    ld vA, 6
    ld i, spr_ok + 5
    drw vA, v9, 5
    ret

spr_ok:
    db %.XXX...., %X...X..., %X...X..., %X...X..., %.XXX....
    db %X...X..., %X..X...., %XXX....., %X..X...., %X...X...

pixel:     db %X.......
dois:      db %X......., %X.......
um_byte:   db %XXXXXXXX
dados:     db 0x11, 0x22, 0x33, 0x44
bloco16:   dw 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
           dw 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
buffer:    ds 16
falhas:    ds 64
