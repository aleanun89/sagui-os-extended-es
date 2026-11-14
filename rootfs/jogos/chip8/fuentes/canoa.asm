; SPDX-License-Identifier: GPL-2.0-or-later
; canoa.asm - CANOA, jogo de SUPER-CHIP (128x64) feito para o SaguiOS.
;
; O sagui desce o rio numa canoa. A agua corre para baixo (a tela rola com
; 00C1, o "scd" do SUPER-CHIP) e de vez em quando aparecem pedras e troncos;
; e preciso desviar com A e D (7 e 9 do CHIP-8, ou as setas). A distancia
; percorrida vira METROS, mostrados no fim com a fonte grande (FX30).
; A correnteza fica mais rapida e os obstaculos mais frequentes com a
; distancia. No fim, V (tecla F) sai do jogo com 00FD.
;
; Usa do SUPER-CHIP: high (128x64), scd, sprites de 16x16 (drw ..., 0),
; a fonte grande e exit.
;
; Registros: v0-v2 temporarios, v3 x da canoa, v4 linha (desenho das
; margens), v5 linhas ate o proximo obstaculo, v6 metros, v7 quadros por
; passo, v8 linhas entre obstaculos, v9-vB texto, vC-vD coordenadas,
; vE linhas do metro atual.

CANOA_Y = 50
ESQ_MIN = 8
DIR_MAX = 112

inicio:
    high
    call titulo
jogo:
    cls
    ld v3, 60
    ld v4, 0
    ld v5, 30
    ld v6, 0
    ld v7, 4
    ld v8, 20
    ld vE, 0
    ld v1, 64               ; margens na tela inteira antes de comecar
jg_margens:
    scd 1
    call margens
    add v4, 1
    add v1, -1
    se v1, 0
    jp jg_margens
    call desenhar_canoa

laco:
    ld v0, dt
    se v0, 0
    jp laco
    ld dt, v7
    call desenhar_canoa     ; apaga (XOR) na posicao antiga
    ld v0, 7
    sknp v0
    call esquerda
    ld v0, 9
    sknp v0
    call direita
    scd 1                   ; o rio desce um pixel
    call margens
    call obstaculo
    call desenhar_canoa
    se vF, 0
    jp bateu
    add v4, 1
    add vE, 1
    se vE, 16
    jp laco
    ld vE, 0
    se v6, 255
    add v6, 1
    ; a correnteza aperta com a distancia
    sne v6, 10
    ld v7, 3
    sne v6, 25
    ld v8, 14
    sne v6, 40
    ld v7, 2
    sne v6, 60
    ld v8, 10
    sne v6, 100
    ld v7, 1
    jp laco

desenhar_canoa:
    ld i, spr_canoa
    ld vD, CANOA_Y
    drw v3, vD, 12
    ret

esquerda:
    sne v3, ESQ_MIN
    ret
    add v3, -2
    ret

direita:
    sne v3, DIR_MAX
    ret
    add v3, 2
    ret

; Uma linha nova de cada margem no alto da tela (8 desenhos que se repetem).
margens:
    ld v0, v4
    ld v2, 7
    and v0, v2
    ld i, margem_esq
    add i, v0
    ld vC, 0
    ld vD, 0
    drw vC, vD, 1
    ld i, margem_dir
    add i, v0
    ld vC, 120
    drw vC, vD, 1
    ret

; A cada v8 linhas, uma pedra ou um tronco num lugar sorteado do rio.
obstaculo:
    add v5, -1
    se v5, 0
    ret
    ld v5, v8
    rnd v0, 0x7F
    ld v2, 96
    sub v2, v0              ; VF = 1 se x <= 96
    se vF, 1
    add v0, -32
    add v0, 8               ; x de 8 a 104
    ld vC, 0
    rnd v2, 1
    se v2, 0
    jp ob_tronco
    ld i, spr_pedra
    drw v0, vC, 6
    ret
ob_tronco:
    ld i, spr_tronco
    drw v0, vC, 0           ; sprite de 16x16 do SUPER-CHIP
    ret

bateu:
    ld v0, 30
    ld st, v0
    ld v1, 6
ba_pisca:
    call desenhar_canoa
    ld v0, 6
    ld dt, v0
ba_espera:
    ld v0, dt
    se v0, 0
    jp ba_espera
    add v1, -1
    se v1, 0
    jp ba_pisca
    ; tela do fim: metros com a fonte grande
    cls
    ld v9, t_fim - textos
    ld vA, 38
    ld vB, 6
    call escrever
    ld v9, t_metros - textos
    ld vA, 52
    ld vB, 16
    call escrever
    ld i, bcd
    ld b, v6
    ld v2, [i]
    ld vA, 49
    ld vB, 24
    ld hf, v0
    drw vA, vB, 10
    add vA, 10
    ld hf, v1
    drw vA, vB, 10
    add vA, 10
    ld hf, v2
    drw vA, vB, 10
    ld v0, 60
    ld dt, v0
ba_pausa:
    ld v0, dt
    se v0, 0
    jp ba_pausa
    ld v9, t_sair - textos
    ld vA, 48
    ld vB, 42
    call escrever
    ld v9, t_outra - textos
    ld vA, 30
    ld vB, 52
    call escrever
    ld v0, k
    sne v0, 0xF
    exit
    jp jogo

; ---------------------------------------------------------------- titulo

titulo:
    cls
    ld v9, t_titulo - textos
    ld vA, 54
    ld vB, 2
    call escrever
    ld i, spr_arte
    ld vA, 56
    ld vB, 10
    drw vA, vB, 0
    ld v9, t_reg1 - textos
    ld vA, 20
    ld vB, 30
    call escrever
    ld v9, t_reg2 - textos
    ld vA, 8
    ld vB, 38
    call escrever
    ld v9, t_teclas - textos
    ld vA, 44
    ld vB, 46
    call escrever
    ld v9, t_tecla - textos
    ld vA, 34
    ld vB, 56
    call escrever
    ld v0, k
    ret

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
t_titulo:  db "CANOA", 0
t_reg1:    db "BAJA EL RIO SIN CHOCAR", 0
t_reg2:    db "CON LAS ROCAS NI LOS TRONCOS", 0
t_teclas:  db "A D: REMAR", 0
t_tecla:   db "PULSA UNA TECLA", 0
t_fim:     db "FIN DEL VIAJE", 0
t_metros:  db "METROS", 0
t_sair:    db "V: SALIR", 0
t_outra:   db "OTRA TECLA: OTRO VIAJE", 0

margem_esq:
    db %XXXXXX.., %XXXXX..., %XXXXXX.., %XXXXXXX., %XXXXXX.., %XXXXX..., %XXXX...., %XXXXX...
margem_dir:
    db %..XXXXXX, %...XXXXX, %..XXXXXX, %.XXXXXXX, %..XXXXXX, %...XXXXX, %....XXXX, %...XXXXX

spr_canoa:
    db %...XX...
    db %..X..X..
    db %..X..X..
    db %.X.XX.X.
    db %.X.XX.X.
    db %.X....X.
    db %.XX..XX.
    db %.X.XX.X.
    db %.X....X.
    db %..X..X..
    db %..X..X..
    db %...XX...

spr_pedra:
    db %..XXX...
    db %.XX.XX..
    db %XX.XXXX.
    db %XXXXX.XX
    db %.XXXXXX.
    db %..XXXX..

spr_tronco:
    dw %.XXXXXXXXXXXXXX.
    dw %X..X......X...XX
    dw %X....X..X.....XX
    dw %.XXXXXXXXXXXXXX.
    ds 24

spr_arte:
    dw %................
    dw %......X..X......
    dw %.....XXXXXX.....
    dw %.....X.XX.X.....
    dw %.....XXXXXX.....
    dw %......XXXX......
    dw %....XXXXXXXX....
    dw %...XX.XXXX.XX..X
    dw %..X...XXXX...XX.
    dw %XXXXXXXXXXXXXXX.
    dw %.X............X.
    dw %..XXXXXXXXXXXX..
    dw %................
    dw %.X.X.X.X.X.X.X.X
    dw %X.X.X.X.X.X.X.X.
    dw %................

bcd: ds 3

include "letras.asm"
