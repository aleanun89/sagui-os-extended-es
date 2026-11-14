; SPDX-License-Identifier: GPL-3.0-only
; vetores.asm - puerta de entrada de las 256 interrupciones.
;
; El procesador no dice al código qué vector disparó, y solo algunas excepciones
; apilan un código de error. Por eso cada vector tiene un trozo propio,
; generado abajo por repetición, que:
;   1. apila un código de error 0 cuando el procesador no apiló ninguno,
;      para que el marco tenga siempre el mismo formato;
;   2. apila el número del vector;
;   3. salta al trozo común.
; El trozo común guarda todos los registros, entra en los segmentos del kernel
; y llama a tratar_interrupcao(quadro). El C devuelve la dirección del marco a
; reanudar: normalmente el mismo, pero en la fase 3 el planificador devuelve el marco
; de otro proceso, y eso es todo lo que necesita un cambio de contexto.
;
; La tabela_vetores (dirección de cada trozo) la lee idt.c para montar la IDT.

section .text
extern tratar_interrupcao
extern kernel_saida
global tabela_vetores
global retomar_quadro
global espera_sem_trava
global ponto_hlt
global ponto_depois_hlt

%assign v 0
%rep 256
vetor_%[v]:
%if !(v == 8 || (v >= 10 && v <= 14) || v == 17 || v == 21 || v == 29 || v == 30)
        push dword 0
%endif
        push dword v
        jmp entrada_comum
%assign v v+1
%endrep

entrada_comum:
        pushad
        push ds
        push es
        push fs
        push gs
        mov ax, 0x10                    ; segmentos de datos del kernel
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov ax, 0x30                    ; SEL_GS_CPU: los datos de esta CPU (D157)
        mov gs, ax
        cld                             ; el C supone la flag de dirección a cero
        push esp
        call tratar_interrupcao
        mov esp, eax                    ; marco a reanudar (puede ser otro)
        ; Ya estamos en la pila del marco a reanudar: si ese marco no tenía el
        ; cerrojo general (modo usuario o la espera de cpu_espera), se suelta
        ; aquí, y salen las IPI pendientes (D165). Por debajo del marco la pila
        ; está libre, así que la llamada no pisa nada.
        push esp
        call kernel_saida
        add esp, 4
; retomar_quadro: también usado para empezar un proceso nuevo a partir de un
; marco montado a mano (fase 3). Espera ESP apuntando a un quadro_t.
retomar_quadro:
        pop gs
        pop fs
        pop es
        pop ds
        popad
        add esp, 8                      ; descarta vector y código de error
        iretd

; espera_sem_trava: duerme hasta la próxima interrupción sin el cerrojo
; general (D165). Se llama con las interrupciones apagadas y el cerrojo ya
; suelto, y vuelve con las interrupciones apagadas. El "sti" retrasa las
; interrupciones una instrucción, así que ninguna llega antes del "hlt". Un
; marco interrumpido aquí tiene el EIP en ponto_hlt o ponto_depois_hlt: la
; salida del kernel lo reconoce y suelta el cerrojo antes de volver.
espera_sem_trava:
        sti
ponto_hlt:
        hlt
ponto_depois_hlt:
        cli
        ret

section .rodata
align 4
tabela_vetores:
%assign v 0
%rep 256
        dd vetor_%[v]
%assign v v+1
%endrep
