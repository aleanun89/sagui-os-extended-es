; SPDX-License-Identifier: GPL-3.0-only
; entrada.asm - primera instrucción del kernel de SaguiOS.
;
; La etapa 2 llega aquí en modo protegido de 32 bits, con EAX = firma del
; cargador y EBX = dirección de boot_info. Antes de cualquier código C:
;   1. poner a cero la BSS (el binario plano no la contiene; el C espera ceros);
;   2. cambiar a una pila propia, dentro de la BSS;
;   3. llamar a inicio_kernel(firma, boot_info).

section .entrada
global _inicio
extern inicio_kernel
extern _bss_inicio, _bss_fim

_inicio:
        cli
        mov esi, eax                    ; guarda los dos valores del cargador
        mov edx, ebx                    ; (la BSS y la pila van a ser borradas)
        cld
        mov edi, _bss_inicio
        mov ecx, _bss_fim
        sub ecx, edi
        xor eax, eax
        rep stosb
        mov esp, pilha_kernel_topo
        xor ebp, ebp                    ; marca el fin de la cadena de marcos de pila
        push edx
        push esi
        call inicio_kernel
.parado:
        cli
        hlt
        jmp .parado

section .bss
align 16
pilha_kernel_base:
        resb 16384
pilha_kernel_topo:
