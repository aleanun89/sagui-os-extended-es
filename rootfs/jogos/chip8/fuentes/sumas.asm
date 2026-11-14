; SPDX-License-Identifier: GPL-2.0-or-later
; sumas.asm - SUMAS, quebra-cabeca de CHIP-8 (64x32) feito para o SaguiOS.
;
; Um tabuleiro de 4x4 casas com algarismos de 1 a 9 e uma META. Cada tecla
; do teclado hexadecimal marca ou desmarca a casa na mesma posicao
; (1234 / QWER / ASDF / ZXCV no PC). Quando a soma das casas marcadas
; chega exatamente a meta, passa de nivel; se passar da meta, perde uma
; vida e as marcas se apagam. Nos niveis mais altos a meta pede mais casas
; (2, depois 3, depois 4). Sempre ha solucao: a meta e a soma de casas
; sorteadas.
;
; Registros: v0-v2 temporarios, v3 meta, v4 soma, v5 vidas, v6 nivel,
; v7 tecla, v8 casa, v9-vB texto e numeros, vC/vD canto da casa, vE contador.

inicio:
    call titulo
jogo:
    ld v5, 3
    ld v6, 1
nivel:
    call novo_tabuleiro
    call desenhar_tudo

laco:
    ld v7, k                ; espera uma tecla (conta quando e solta)
    ld i, mapa
    add i, v7
    ld v0, [i]
    ld v8, v0               ; v8 = casa da tecla
    call marcar
    ld v0, v3
    sub v0, v4              ; VF = 1 se meta >= soma
    se vF, 1
    jp passou
    se v3, v4
    jp laco
    ; acertou
    ld v0, 8
    ld st, v0
    ld v0, 40
    call esperar
    ld v0, 99
    se v6, v0
    add v6, 1
    jp nivel

passou:
    ld v0, 30
    ld st, v0
    ld v0, 30
    call esperar
    ld vA, 58
    ld vB, 16
    ld vE, v5
    call desenhar_digito    ; apaga as vidas velhas
    add v5, -1
    ld vE, v5
    call desenhar_digito
    se v5, 0
    jp limpar_marcas
    jp fim_de_jogo

; Desmarca tudo e zera a soma.
limpar_marcas:
    ld v8, 0
lm_laco:
    ld i, marcado
    add i, v8
    ld v0, [i]
    se v0, 0
    call marcar
    add v8, 1
    se v8, 16
    jp lm_laco
    jp laco

; Marca ou desmarca a casa v8: inverte o quadrado, atualiza a soma e o placar.
marcar:
    ld vA, 53
    ld vB, 8
    ld vE, v4
    call desenhar_numero    ; apaga a soma velha (antes: ele usa v0-v2)
    call canto
    ld i, spr_caixa
    drw vC, vD, 7
    ld i, marcado
    add i, v8
    ld v0, [i]
    ld v1, 1
    xor v0, v1
    ld v2, v0               ; v2 = novo estado
    ld i, marcado
    add i, v8
    ld [i], v0
    call valor              ; v0 = algarismo da casa
    se v2, 0
    add v4, v0
    sne v2, 0
    sub v4, v0
    ld vA, 53
    ld vB, 8
    ld vE, v4
    jp desenhar_numero

; v0 = algarismo da casa v8 (so carrega v0: nao mexe em v1 e v2).
valor:
    ld i, tabuleiro
    add i, v8
    ld v0, [i]
    ret

; vC, vD = canto de cima a esquerda da casa v8 (colunas e linhas de 8 pixels).
canto:
    ld vC, v8
    ld v0, 3
    and vC, v0
    shl vC
    shl vC
    shl vC
    ld vD, v8
    shr vD
    shr vD
    shl vD
    shl vD
    shl vD
    ret

; ---------------------------------------------------------------- tabuleiro

novo_tabuleiro:
    ld v8, 0
nt_laco:
    rnd v0, 15              ; algarismo de 1 a 9: sorteia 0-15 e repete se nao serve
    sne v0, 0
    jp nt_laco
    ld v1, 9
    sub v1, v0
    se vF, 1
    jp nt_laco
    ld i, tabuleiro
    add i, v8
    ld [i], v0
    ld i, marcado
    add i, v8
    ld v0, 0
    ld [i], v0
    add v8, 1
    se v8, 16
    jp nt_laco
    ; A meta: casas a, b = a + 1..8, c = b + 1..4, d = c + 1..2 (modulo 16),
    ; que nunca se repetem.
    ld v4, 0
    rnd v8, 15
    call valor
    ld v3, v0
    rnd v1, 7
    add v1, 1
    call proxima_casa
    ld v0, 2
    sub v0, v6              ; VF = 1 se nivel <= 2
    se vF, 0
    ret
    rnd v1, 3
    add v1, 1
    call proxima_casa
    ld v0, 5
    sub v0, v6              ; VF = 1 se nivel <= 5
    se vF, 0
    ret
    rnd v1, 1
    add v1, 1
    ; cai em proxima_casa

; v8 += v1 (modulo 16) e soma o algarismo dessa casa na meta.
proxima_casa:
    add v8, v1
    ld v1, 15
    and v8, v1
    call valor
    add v3, v0
    ret

desenhar_tudo:
    cls
    ld v8, 0
dt_laco:
    call canto
    add vC, 2
    add vD, 1
    call valor
    ld f, v0
    drw vC, vD, 5
    add v8, 1
    se v8, 16
    jp dt_laco
    ld i, spr_linha         ; separador vertical
    ld vA, 33
    ld vB, 0
    drw vA, vB, 15
    ld vB, 15
    drw vA, vB, 15
    ld vB, 30
    drw vA, vB, 2
    ld v9, t_meta - textos
    ld vA, 36
    ld vB, 0
    call escrever
    ld v9, t_suma - textos
    ld vA, 36
    ld vB, 8
    call escrever
    ld v9, t_vida - textos
    ld vA, 36
    ld vB, 16
    call escrever
    ld v9, t_nivel - textos
    ld vA, 36
    ld vB, 24
    call escrever
    ld vA, 53
    ld vB, 0
    ld vE, v3
    call desenhar_numero
    ld vA, 53
    ld vB, 8
    ld vE, v4
    call desenhar_numero
    ld vA, 58
    ld vB, 16
    ld vE, v5
    call desenhar_digito
    ld vA, 53
    ld vB, 24
    ld vE, v6
    jp desenhar_numero

; Dois algarismos de vE (0-99) em (vA, vB); XOR: desenhar de novo apaga.
desenhar_numero:
    ld i, bcd
    ld b, vE
    ld v2, [i]
    ld v0, v1               ; dezenas
    call dn_um
    add vA, 5
    ld v0, v2
dn_um:
    ld f, v0
    drw vA, vB, 5
    ret

desenhar_digito:
    ld f, vE
    drw vA, vB, 5
    ret

; Espera v0 quadros.
esperar:
    ld dt, v0
es_laco:
    ld v0, dt
    se v0, 0
    jp es_laco
    ret

; ---------------------------------------------------------------- telas

titulo:
    cls
    ld v9, t_titulo - textos
    ld vA, 0
    ld vB, 0
    call escrever
    ld v9, t_reg1 - textos
    ld vA, 0
    ld vB, 6
    call escrever
    ld v9, t_reg2 - textos
    ld vA, 0
    ld vB, 12
    call escrever
    ld v9, t_reg3 - textos
    ld vA, 0
    ld vB, 18
    call escrever
    ld v9, t_k1 - textos
    ld vA, 48
    ld vB, 0
    call escrever
    ld v9, t_k2 - textos
    ld vA, 48
    ld vB, 6
    call escrever
    ld v9, t_k3 - textos
    ld vA, 48
    ld vB, 12
    call escrever
    ld v9, t_k4 - textos
    ld vA, 48
    ld vB, 18
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
    ld v9, t_nivel_fim - textos
    ld vA, 14
    ld vB, 12
    call escrever
    ld vA, 38
    ld vB, 12
    ld vE, v6
    call desenhar_numero
    ld v0, 60
    call esperar
    ld v9, t_tecla - textos
    ld vA, 2
    ld vB, 24
    call escrever
    ld v0, k
    jp jogo

; Escreve textos + v9 (terminado em 0) a partir de (vA, vB). Usa v0.
escrever:
    ld i, textos
    add i, v9
    ld v0, [i]
    sne v0, 0
    ret
    add v0, -32
    sne v0, 0
    jp esc_espaco
    ld i, letras
    add i, v0
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
t_titulo:    db "SUMAS", 0
t_reg1:      db "SUMA LA", 0
t_reg2:      db "META CON", 0
t_reg3:      db "CASILLAS", 0
t_k1:        db "1234", 0
t_k2:        db "QWER", 0
t_k3:        db "ASDF", 0
t_k4:        db "ZXCV", 0
t_tecla:     db "PULSA UNA TECLA", 0
t_meta:      db "META", 0
t_suma:      db "SUMA", 0
t_vida:      db "VIDA", 0
t_nivel:     db "NIV.", 0
t_fim:       db "FIN DEL JUEGO", 0
t_nivel_fim: db "NIVEL", 0

; Tecla do CHIP-8 -> casa (o teclado hexadecimal e 1 2 3 C / 4 5 6 D /
; 7 8 9 E / A 0 B F).
mapa:
    db 13, 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 14, 3, 7, 11, 15

spr_caixa:
    db %XXXXXXX., %XXXXXXX., %XXXXXXX., %XXXXXXX., %XXXXXXX., %XXXXXXX., %XXXXXXX.

spr_linha:
    db %X......., %........, %X......., %........, %X......., %........, %X......., %........
    db %X......., %........, %X......., %........, %X......., %........, %X.......

tabuleiro: ds 16
marcado:   ds 16
bcd:       ds 3

include "letras.asm"
