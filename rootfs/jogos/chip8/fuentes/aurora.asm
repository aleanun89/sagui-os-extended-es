; SPDX-License-Identifier: GPL-2.0-or-later
; aurora.asm - AURORA, demonstracao de XO-CHIP (128x64, 4 cores) feita para
; o SaguiOS.
;
; Uma aurora corre pelo ceu (plano 2, rolando para a esquerda com 00FC e
; uma fatia nova a cada passo), as estrelas piscam (plano 1), as colinas
; ficam no plano 1 e o sagui usa os dois planos (as quatro cores). Uma
; melodia toca com o audio do XO-CHIP: um padrao de 16 bytes (F002) e a
; altura de cada nota (FX3A). A melodia e o padrao ficam acima de 4 KB e
; sao lidos com "ld i, long" (F000 NNNN) e "load v0 - v1" (5XY3).
;
; Teclas: W/S sobem e descem o tom, A/D mudam o vento, E liga e desliga a
; musica, V (tecla F) sai (00FD).
;
; Registros: v0-v2 temporarios, v3 fase da onda, v4/v5 contador e periodo
; do vento, v8 posicao na melodia, v9 quadros da nota, vA-vB coordenadas,
; vC musica ligada, vD transposicao, vE espera entre teclas, v6/v7 texto.

SAGUI_X = 56
SAGUI_Y = 36

inicio:
    high
    call titulo
    plane 3
    cls
    ld i, long padrao
    audio
    call colinas
    plane 3
    ld i, spr_sagui
    ld vA, SAGUI_X
    ld vB, SAGUI_Y
    drw vA, vB, 0           ; 16x16 nos dois planos: 64 bytes
    ld v3, 0
    ld v4, 0
    ld v5, 2
    ld v8, 0
    ld v9, 1
    ld vC, 1
    ld vD, 0
    ld vE, 0

quadro:
    ld v0, dt
    se v0, 0
    jp quadro
    ld v0, 1
    ld dt, v0
    add v4, 1
    se v4, v5
    jp qu_resto
    ld v4, 0
    call aurora
qu_resto:
    call estrela
    call musica
    call teclas
    jp quadro

; A aurora: o plano 2 anda 4 pixels para a esquerda e entra uma fatia nova
; na direita, na altura da onda. O sagui tambem tem pixels no plano 2:
; sai antes de rolar e volta depois (o desenho e XOR).
aurora:
    plane 2
    call sagui_plano2
    scl
    ld i, onda
    ld v0, v3
    ld v1, 31
    and v0, v1
    add i, v0
    ld v0, [i]
    ld vA, 124
    ld i, fatia
    drw vA, v0, 15
    add v3, 1
    jp sagui_plano2

sagui_plano2:
    ld i, spr_sagui + 32
    ld vA, SAGUI_X
    ld vB, SAGUI_Y
    drw vA, vB, 0
    ret

; Uma das 16 estrelas troca de estado (plano 1).
estrela:
    plane 1
    rnd v0, 15
    ld i, estrelas
    add i, v0
    add i, v0
    load v0 - v1            ; x e y, sem mexer em I
    ld i, ponto
    drw v0, v1, 1
    ret

; A proxima nota quando a atual acaba. Cada nota: altura, quadros.
musica:
    add v9, -1
    se v9, 0
    ret
    ld i, long melodia
    add i, v8
    load v0 - v1
    sne v0, 0xFF
    jp mu_volta
    add v8, 2
    add v0, vD
    pitch v0
    ld v9, v1
    add v1, -3              ; um respiro entre as notas
    se vC, 0
    ld st, v1
    ret
mu_volta:
    ld v8, 0
    ld v9, 1
    ret

teclas:
    se vE, 0
    jp te_espera
    ld v0, 5                ; W: mais agudo
    skp v0
    jp te_s
    se vD, 32
    add vD, 4
    jp te_feito
te_s:
    ld v0, 8                ; S: mais grave
    skp v0
    jp te_a
    se vD, 0xF0
    add vD, -4
    jp te_feito
te_a:
    ld v0, 7                ; A: vento mais fraco
    skp v0
    jp te_d
    se v5, 6
    add v5, 1
    ld v4, 0
    jp te_feito
te_d:
    ld v0, 9                ; D: vento mais forte
    skp v0
    jp te_e
    se v5, 1
    add v5, -1
    ld v4, 0
    jp te_feito
te_e:
    ld v0, 6                ; E: musica
    skp v0
    jp te_v
    ld v0, 1
    xor vC, v0
    jp te_feito
te_v:
    ld v0, 15               ; V (F): sair
    sknp v0
    exit
    ret
te_feito:
    ld vE, 12
    ret
te_espera:
    add vE, -1
    ret

colinas:
    plane 1
    ld vA, 0
    ld vB, 48
co_laco:
    ld i, colina_a
    drw vA, vB, 0
    add vA, 16
    ld i, colina_b
    drw vA, vB, 0
    add vA, 16
    se vA, 128
    jp co_laco
    ret

; ---------------------------------------------------------------- titulo

titulo:
    plane 3
    cls
    plane 1
    ld v6, t_titulo - textos
    ld vA, 52
    ld vB, 4
    call escrever
    plane 2
    ld v6, t_demo - textos
    ld vA, 10
    ld vB, 13
    call escrever
    plane 1
    ld v6, t_tono - textos
    ld vA, 40
    ld vB, 25
    call escrever
    plane 2
    ld v6, t_vento - textos
    ld vA, 40
    ld vB, 32
    call escrever
    plane 1                 ; cor 3: o mesmo texto nos dois planos
    ld v6, t_musica - textos
    ld vA, 40
    ld vB, 39
    call escrever
    plane 2
    ld v6, t_musica - textos
    ld vA, 40
    call escrever
    plane 1
    ld v6, t_sair - textos
    ld vA, 40
    ld vB, 46
    call escrever
    plane 3
    ld i, spr_sagui
    ld vA, 16
    ld vB, 28
    drw vA, vB, 0
    plane 2
    ld v6, t_tecla - textos
    ld vA, 34
    ld vB, 57
    call escrever
    ld v0, k
    ret

; Escreve textos + v6 (terminado em 0) a partir de (vA, vB) nos planos
; ligados (um de cada vez: com os dois, cada letra pediria 10 bytes).
escrever:
    ld i, textos
    add i, v6
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
    add v6, 1
    jp escrever

; ---------------------------------------------------------------- dados

textos:
t_titulo: db "AURORA", 0
t_demo:   db "DEMO DE XO-CHIP A 4 COLORES", 0
t_tono:   db "W S: TONO", 0
t_vento:  db "A D: VIENTO", 0
t_musica: db "E: MUSICA", 0
t_sair:   db "V: SALIR", 0
t_tecla:  db "PULSA UNA TECLA", 0

onda:
    db 6, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8
    db 7, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5

fatia:
    db %X.X....., %.X.X...., %X.X.X..., %.XXX...., %XXXX...., %XXXX...., %XXXX...., %XXXX....
    db %XXXX...., %.XXX...., %X.XX...., %.X.X...., %X.X....., %.X......, %X.......

ponto:
    db %X.......

; x, y das estrelas (todas no ceu, acima das colinas e do sagui)
estrelas:
    db 5, 3, 19, 17, 30, 8, 41, 26, 50, 2, 63, 20, 70, 11, 82, 28
    db 90, 5, 99, 22, 108, 14, 115, 1, 122, 25, 12, 29, 36, 19, 77, 31

colina_a:
    dw %................, %................, %................, %................
    dw %.....XXXXXX....., %...XXXXXXXXXX..., %..XXXXXXXXXXXX.., %.XXXXXXXXXXXXXX.
    dw %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX
    dw %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX

colina_b:
    dw %................, %................, %................, %................
    dw %................, %................, %................, %................
    dw %X..............X, %XX............XX, %XXXX......XXXXXX, %XXXXXXXXXXXXXXXX
    dw %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX, %XXXXXXXXXXXXXXXX

; O sagui: 32 bytes do plano 1 (corpo) e 32 do plano 2 (tufos, olhos,
; focinho e os aneis do rabo). Onde os dois se cruzam sai a cor 3.
spr_sagui:
    dw %................, %....XXXXXXXX...., %...XXXXXXXXXX..., %...XX.XXXX.XX...
    dw %...XXXXXXXXXX..., %....XXXXXXXX...., %.....XXXXXX....., %....XXXXXXXX....
    dw %...XXXXXXXXXX..., %...XXXXXXXXXX..X, %...XXXXXXXXXX.X., %....XXXXXXXX.X..
    dw %....XX....XXX..., %...XXX....XX...., %................, %................
    dw %.XX..........XX., %XXX..........XXX, %.X............X., %.....X....X.....
    dw %................, %......XXXX......, %................, %................
    dw %................, %...............X, %..............X., %.............X..
    dw %................, %................, %................, %................

include "letras.asm"

; Acima de 4 KB: so o XO-CHIP alcanca (ld i, long).
    org 0x1000
padrao:
    db 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF
    db 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF

; altura (64 + 4 por semitom), quadros; 0xFF volta ao comeco
melodia:
    db 92, 12, 100, 12, 112, 24, 100, 12, 92, 12, 80, 24
    db 72, 12, 80, 12, 92, 12, 80, 12, 72, 24, 64, 24
    db 72, 12, 80, 12, 92, 12, 100, 12, 92, 36, 64, 36
    db 0xFF, 0
