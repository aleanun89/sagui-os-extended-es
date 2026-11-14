; SPDX-License-Identifier: GPL-2.0-or-later
; ramas.asm - RAMAS, jogo de CHIP-8 (64x32) feito para o SaguiOS.
;
; O sagui anda pelo chao da mata. Das ramas caem bananas (pegue: +1 ponto)
; e galhos secos (fuja: -1 vida). Banana que chega ao chao tambem custa uma
; vida. A cada 10 bananas tudo cai mais depressa. Sao 3 vidas.
;
; Teclas: A e D (7 e 9 do CHIP-8) andam; as setas tambem servem.
; Monte com:  chip8asm ramas.asm -o ramas.ch8
;
; Registros: v0-v2 temporarios (ld v2, [i] carrega os tres), v3 x do sagui,
; v4/v5 banana (x, y), v6/v7 galho (x, y), v8 pontos, v9-vB texto e
; desenho, vC vidas, vD velocidade da banana (o galho cai vD + 1).
;
; O codigo nao depende dos quirks: depois de "ld v2, [i]" o I e sempre
; recarregado, e os deslocamentos nao sao usados.

SAGUI_Y = 25
NASCE_Y = 9
PASSO_DT = 4            ; um passo do jogo a cada 4 quadros (no maximo)

inicio:
    call titulo
jogo:
    cls
    call cenario
    ld v3, 28
    ld v8, 0
    ld vC, 3
    ld vD, 1
    call desenhar_sagui
    call nova_banana
    call novo_galho
    ld vA, 0
    ld vB, 0
    call desenhar_pontos
    call desenhar_vidas

laco:
    ld v0, dt           ; o ritmo vem do temporizador, nao da velocidade da CPU
    se v0, 0
    jp laco
    ld v0, PASSO_DT
    ld dt, v0

    ld v0, 7            ; A: esquerda
    sknp v0
    call esquerda
    ld v0, 9            ; D: direita
    sknp v0
    call direita

    call mover_banana
    call mover_galho
    se vC, 0
    jp laco
    jp fim_de_jogo

; ---------------------------------------------------------------- sagui

desenhar_sagui:
    ld i, spr_sagui
    ld v0, SAGUI_Y
    drw v3, v0, 6
    ret

esquerda:
    sne v3, 0
    ret
    call desenhar_sagui     ; apaga (o desenho e XOR)
    add v3, -4
    jp desenhar_sagui

direita:
    sne v3, 56
    ret
    call desenhar_sagui
    add v3, 4
    jp desenhar_sagui

; ---------------------------------------------------------------- banana

nova_banana:
    rnd v4, 0x3F
    ld v0, 56
    sub v0, v4          ; VF = 1 se x <= 56
    se vF, 1
    add v4, -8
    ld v5, NASCE_Y
    ld i, spr_banana
    drw v4, v5, 5
    ret

mover_banana:
    ld i, spr_banana
    drw v4, v5, 5       ; apaga
    add v5, vD
    drw v4, v5, 5
    ld v0, 20
    sub v0, v5          ; VF = 1 se y <= 20: ainda longe do sagui
    se vF, 0
    ret
    ld v0, v4           ; alinhada? (x_banana - x_sagui + 5) entre 0 e 12
    sub v0, v3
    add v0, 5
    ld v1, 12
    sub v1, v0
    se vF, 0
    jp pegou_banana
    ld v0, 25
    sub v0, v5          ; VF = 1 se y <= 25: ainda pode pegar
    se vF, 1
    jp perdeu_banana
    ret

pegou_banana:
    ld i, spr_banana
    drw v4, v5, 5
    ld vA, 0
    ld vB, 0
    call desenhar_pontos    ; apaga o placar velho
    add v8, 1
    ld vA, 0
    call desenhar_pontos
    ld v0, 2
    ld st, v0           ; bip curto
    ld v0, 10           ; com 10 pontos, mais rapido
    sne v8, v0
    ld vD, 2
    ld v0, 30
    sne v8, v0
    ld vD, 3
    jp nova_banana

perdeu_banana:
    ld i, spr_banana
    drw v4, v5, 5
    call perder_vida
    jp nova_banana

; ---------------------------------------------------------------- galho

novo_galho:
    rnd v6, 0x3F
    ld v0, 56
    sub v0, v6
    se vF, 1
    add v6, -8
    ld v7, NASCE_Y
    ld i, spr_galho
    drw v6, v7, 3
    ret

mover_galho:
    ld i, spr_galho
    drw v6, v7, 3
    add v7, vD
    add v7, 1
    drw v6, v7, 3
    ld v0, 22
    sub v0, v7          ; VF = 1 se y <= 22
    se vF, 0
    ret
    ld v0, v6           ; encostou no sagui? (x_galho - x_sagui + 7) entre 0 e 14
    sub v0, v3
    add v0, 7
    ld v1, 14
    sub v1, v0
    se vF, 0
    jp levou_galho
    ld v0, 26
    sub v0, v7          ; VF = 1 se y <= 26
    se vF, 1
    jp sumiu_galho
    ret

levou_galho:
    call sumiu_galho
    jp perder_vida

sumiu_galho:
    ld i, spr_galho
    drw v6, v7, 3
    jp novo_galho

; ---------------------------------------------------------------- placar

; Pontos (v8) em tres digitos a partir de (vA, vB).
desenhar_pontos:
    ld i, bcd
    ld b, v8
    ld v2, [i]
    ld f, v0
    drw vA, vB, 5
    add vA, 5
    ld f, v1
    drw vA, vB, 5
    add vA, 5
    ld f, v2
    drw vA, vB, 5
    ret

; Um coracao por vida, da direita para a esquerda (XOR: chamar de novo apaga).
desenhar_vidas:
    ld v0, vC
    ld vA, 61
    ld vB, 0
    ld i, spr_coracao
dv_laco:
    sne v0, 0
    ret
    drw vA, vB, 3
    add vA, -4
    add v0, -1
    jp dv_laco

perder_vida:
    call desenhar_vidas
    add vC, -1
    call desenhar_vidas
    ld v0, 20
    ld st, v0           ; bip longo
    ret

cenario:
    ld vA, 0
    ld vB, 6
    ld i, spr_copa
ce_copa:
    drw vA, vB, 3
    add vA, 8
    se vA, 64
    jp ce_copa
    ld vA, 0
    ld vB, 31
    ld i, spr_chao
ce_chao:
    drw vA, vB, 1
    add vA, 8
    se vA, 64
    jp ce_chao
    ret

; ---------------------------------------------------------------- telas

titulo:
    cls
    ld v9, t_titulo - textos
    ld vA, 22
    ld vB, 1
    call escrever
    ld i, spr_sagui
    ld vA, 8
    ld vB, 0
    drw vA, vB, 6
    ld i, spr_banana
    ld vA, 48
    drw vA, vB, 5
    ld v9, t_mover - textos
    ld vA, 0
    ld vB, 8
    call escrever
    ld v9, t_banana - textos
    ld vA, 0
    ld vB, 14
    call escrever
    ld v9, t_galho - textos
    ld vA, 0
    ld vB, 20
    call escrever
    ld v9, t_tecla - textos
    ld vA, 2
    ld vB, 26
    call escrever
    ld v0, k
    ret

fim_de_jogo:
    ld v0, 40
    ld st, v0
    cls
    ld v9, t_fim - textos
    ld vA, 6
    ld vB, 3
    call escrever
    ld v9, t_pontos - textos
    ld vA, 10
    ld vB, 12
    call escrever
    ld vA, 38
    ld vB, 12
    call desenhar_pontos
    ld v0, 60           ; um segundo antes de aceitar tecla (quem jogava ainda aperta A ou D)
    ld dt, v0
fj_espera:
    ld v0, dt
    se v0, 0
    jp fj_espera
    ld v9, t_tecla - textos
    ld vA, 2
    ld vB, 24
    call escrever
    ld v0, k
    jp jogo

; Escreve o texto que comeca em textos + v9 (terminado em 0) a partir de
; (vA, vB), 4 pixels por letra. Usa v0.
escrever:
    ld i, textos
    add i, v9
    ld v0, [i]
    sne v0, 0
    ret
    add v0, -32
    sne v0, 0
    jp esc_espaco       ; o espaco nao gasta um desenho
    ld i, letras
    add i, v0           ; 5 bytes por letra: I += 5 * v0 sem multiplicar
    add i, v0
    add i, v0
    add i, v0
    add i, v0
    drw vA, vB, 5
esc_espaco:
    add vA, 4
    add v9, 1
    jp escrever

; ---------------------------------------------------------------- dados

textos:
t_titulo:   db "RAMAS", 0
t_mover:    db "A D: ANDAR", 0
t_banana:   db "BANANA: +1", 0
t_galho:    db "PALO: -1 VIDA", 0
t_tecla:    db "PULSA UNA TECLA", 0
t_fim:      db "FIN DEL JUEGO", 0
t_pontos:   db "PUNTOS", 0

spr_sagui:
    db %X.X..X.X
    db %.XXXXXX.
    db %.X.XX.X.
    db %..XXXX..
    db %.XXXXX.X
    db %.X..X.X.

spr_banana:
    db %......X.
    db %.....XX.
    db %X...XX..
    db %.XXXX...
    db %..XX....

spr_galho:
    db %X.....X.
    db %.XXXXX.X
    db %X..X....

spr_coracao:
    db %X.X.....
    db %XXX.....
    db %.X......

spr_copa:
    db %X.XX.XX.
    db %XXXXXXXX
    db %..X...X.

spr_chao:
    db %XXXXXXXX

bcd:
    ds 3

include "letras.asm"
