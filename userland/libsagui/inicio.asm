; SPDX-License-Identifier: GPL-3.0-only
; inicio.asm - ponto de entrada de todo programa do SaguiOS.
;
; O kernel começa o programa com [esp] = argc, [esp+4] = argv e [esp+8] =
; envp, o ambiente (ver kernel/nucleo/carregador.c e D037). Guardamos envp
; em "environ", llamamos a los constructores globales (mejora 094),
; chamamos principal(argc, argv) e encerramos o processo com o valor devolvido.
section .text.inicio
global _inicio
extern principal
extern sair
extern environ
global __argv_inicial
extern __init_array_start
extern __init_array_end

_inicio:
        xor ebp, ebp                    ; fim da cadeia de quadros
        mov ebx, esp                    ; argc, argv e envp ficam onde o kernel os pôs
        mov edx, [ebx + 8]
        mov [environ], edx
        mov ecx, [ebx + 4]
        mov [__argv_inicial], ecx       ; para el detector de la GUI (D300): nombre del programa
        ; Mejora 094 (punto 10): los constructores globales (.init_array) antes
        ; de principal, con la pila alineada a 16 en cada llamada. EBX y ESI
        ; los conserva quien es llamado (ABI de 32 bits).
        and esp, ~15
        mov esi, __init_array_start
.constructor:
        cmp esi, __init_array_end
        jae .principal
        call [esi]
        add esi, 4
        jmp .constructor
.principal:
        mov eax, [ebx]
        mov ecx, [ebx + 4]
        and esp, ~15                    ; pilha alinhada para o C:
        sub esp, 8                      ; 16 bytes exatos no "call" (o SSE exige, D032)
        push ecx
        push eax
        call principal
        push eax
        call sair
.nunca: jmp .nunca

section .bss
__argv_inicial: resd 1
