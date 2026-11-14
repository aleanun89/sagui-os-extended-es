; parpadeo.asm - dibuja una letra y la borra (CHIP-8)
; F5: chip8asm parpadeo.asm -o /tmp/parpadeo.ch8 && chip8 /tmp/parpadeo.ch8
espera = 30

inicio:
    cls
    ld v0, 10          ; x
    ld v1, 12          ; y
    ld v2, 0xA
    ld f, v2           ; I = la letra A de la fuente
bucle:
    drw v0, v1, 5
    ld v3, espera
    ld dt, v3
pausa:
    ld v3, dt
    se v3, 0
    jp pausa
    jp bucle
