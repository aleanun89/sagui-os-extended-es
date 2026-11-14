; SPDX-License-Identifier: GPL-3.0-only
; anel3_testes.asm - tres programitas para la prueba de H12.
;
; Todavía no hay cargador de ELF (H13). Estos trozos son independientes de
; posición: la autoprueba copia cada uno a USUARIO_INICIO en un espacio de direcciones
; nuevo y lo ejecuta en el anillo 3. Números de las llamadas: kernel/inc/abi.h.

SIS_SAIR     equ 3
SIS_PID      equ 4
SIS_ESCREVER equ 12

section .rodata
bits 32

; 1) Escribe "Ola do ring 3!" con la llamada escrever y sale. Antes de salir,
;    intenta escribir a partir de una dirección del kernel (1 MB): el kernel tiene que
;    rechazarlo. Código de salida 42 = lo rechazó (bien); 1 = lo aceptó (mal).
global anel3_ola, anel3_ola_fim
anel3_ola:
        call .aqui
.aqui:  pop esi                         ; ESI = dirección de .aqui (sin direcciones fijas)
        mov eax, SIS_ESCREVER
        mov ebx, 1
        lea ecx, [esi + (.msg - .aqui)]
        mov edx, .msg_fim - .msg
        int 0x80
        mov eax, SIS_ESCREVER
        mov ebx, 1
        mov ecx, 0x00100000             ; memoria del kernel
        mov edx, 16
        int 0x80
        mov ebx, 1
        test eax, eax
        jns .sai
        mov ebx, 42
.sai:   mov eax, SIS_SAIR
        int 0x80
.msg:   db "Ola do ring 3!", 10
.msg_fim:
anel3_ola_fim:

; 2) Ejecuta una instrucción privilegiada (cli). En el anillo 3 eso genera protección
;    general, y el kernel tiene que terminar solo este proceso.
global anel3_cli, anel3_cli_fim
anel3_cli:
        cli
        mov eax, SIS_SAIR
        xor ebx, ebx
        int 0x80
anel3_cli_fim:

; 3) Intenta leer la memoria del kernel directamente. Las páginas del kernel no tienen
;    el bit de usuario: fallo de página y el proceso es terminado.
global anel3_espiao, anel3_espiao_fim
anel3_espiao:
        mov eax, [0x00100000]
        mov eax, SIS_SAIR
        xor ebx, ebx
        int 0x80
anel3_espiao_fim:
