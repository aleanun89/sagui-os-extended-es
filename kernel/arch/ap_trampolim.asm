; SPDX-License-Identifier: GPL-2.0-or-later
; ap_trampolim.asm - el trampolín de arranque de los procesadores secundarios (D157, D164).
;
; Cuando el procesador principal envía la SIPI, el secundario arranca en modo
; real en la página que indica el vector (0x7000, donde el principal copia este
; código). El trampolín hace lo mínimo para llegar a C:
;
;   1. carga una GDT plana local y pasa a modo protegido de 32 bits;
;   2. carga DS, ES, FS, GS y SS con el segmento de datos de esa GDT local
;      (base 0, límite 4 GB);
;   3. copia el CR4 y el CR0 del principal (PSE, PAE, PGE, OSFXSR, caché activa...),
;      pone EFER.NXE si el principal usa NX (D349), carga el CR3 del kernel (el
;      directorio, o el PDPT con PAE) y activa la paginación;
;   4. salta a smp_ap_entrada() con la pila de kernel de esta CPU.
;
; La GDT propia de la CPU, su TSS, su GS, la IDT y la FPU se preparan después,
; en C (gdt_carregar, idt_carregar y fpu_iniciar_secundaria), con la paginación
; activa: el mismo camino que
; usa el procesador principal (D164). Antes se cargaban aquí y, en KVM y en el
; hardware real, el procesador secundario provocaba un triple fault que
; reiniciaba la máquina; el TCG del QEMU no lo notaba.
;
; Diagnóstico: en cada paso se escribe un número en _ap_progresso (una escritura
; en memoria, que no toca ningún registro de segmento ni de E/S). El principal
; lo lee mientras espera y, si la CPU no llega, el log dice en qué paso se quedó.
;
; El bloque se copia a la memoria baja (0x7000): todas las direcciones son
; relativas a esa base, por eso el código de 32 bits usa EBX como base y las
; instrucciones con dirección absoluta se montan a mano. Los "huecos" del final
; los escribe el principal antes de cada SIPI.

bits 16
section .aptramp
global _ap_trampolim
global _ap_tam
global _ap_gdt_local
global _ap_gdt_base
global _ap_cr0
global _ap_cr3
global _ap_cr4
global _ap_nxe
global _ap_xd
global _ap_pilha
global _ap_entrada
global _ap_indice
global _ap_progresso

; La base del bloque copiado. Tiene que ser la misma que SMP_TRAMPOLIM (kernel/inc/smp.h).
%define BASE 0x7000
; El desplazamiento de una etiqueta dentro del bloque (constante de ensamblado).
%define OFF(x) ((x) - $$)
; Anota el paso en el que va el procesador (solo en modo protegido: usa EBX).
%macro PASO 1
        mov dword [ebx + OFF(_ap_progresso)], %1
%endmacro

; ---------------------------------------------------------------- modo real
_ap_trampolim:
        cli
        cld
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7000               ; pila provisional (no se usa)
        mov word [BASE + OFF(_ap_progresso)], 1   ; paso 1: el procesador corre
        ; lgdt [disp16]: en modo real la dirección cabe en 16 bits.
        db 0x0F, 0x01, 0x16
        dw BASE + OFF(_ap_gdt_local_desc)
        mov eax, cr0
        or eax, 1                    ; PE
        mov cr0, eax
        ; salto lejano al código de 32 bits (selector 0x08 de la GDT local)
        db 0xEA
        dw BASE + OFF(protegido)
        dw 0x0008

bits 32
; ------------------------------------------------------------ modo protegido
protegido:
        ; Los segmentos de datos aún tienen la caché del modo real (límite
        ; 64 KB): se cargan ya con el segmento plano de la GDT local, antes de
        ; tocar memoria por encima de 64 KB.
        mov ax, 0x10
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov ebx, BASE
        mov esp, BASE                ; pila provisional bajo el trampolín
        PASO 2                       ; modo protegido con segmentos planos

        ; CR4 del principal: PSE (el mapa del kernel usa páginas de 4 MB), PGE,
        ; OSFXSR y OSXMMEXCPT. Tiene que ir antes que la paginación.
        mov eax, [ebx + OFF(_ap_cr4)]
        mov cr4, eax
        ; D349: con NX las tablas del kernel llevan el bit 63, que sin EFER.NXE es
        ; un bit reservado: hay que ponerlo antes de encender la paginación.
        ; Esta CPU puede no tener NX aunque la principal sí (la BIOS puede dejar
        ; XD_DISABLE puesto solo en algunas): el wrmsr de NXE daría un #GP sin IDT
        ; (triple fallo, reinicio). Se mira CPUID antes y, si falta, la CPU se
        ; queda parada en el paso 9 y el kernel no la cuenta.
        cmp dword [ebx + OFF(_ap_nxe)], 0
        je .sem_nx
        cmp dword [ebx + OFF(_ap_xd)], 0
        je .ver_nx
        ; La principal quitó XD_DISABLE (IA32_MISC_ENABLE bit 34, Intel): lo mismo
        ; aquí. El MSR existe: es el mismo modelo de CPU que la principal.
        mov ecx, 0x1A0               ; IA32_MISC_ENABLE
        rdmsr
        and edx, 0xFFFFFFFB          ; bit 34 = bit 2 de EDX
        wrmsr
.ver_nx:
        push ebx                     ; CPUID pisa EBX (la base del bloque)
        mov eax, 0x80000000
        cpuid
        cmp eax, 0x80000001
        jb .nx_falta
        mov eax, 0x80000001
        cpuid
        test edx, 0x100000           ; bit 20 = NX
        jz .nx_falta
        pop ebx
        mov ecx, 0xC0000080          ; IA32_EFER
        rdmsr
        or eax, 0x800                ; NXE
        wrmsr
        jmp .sem_nx
.nx_falta:
        pop ebx
        PASO 9                       ; sin NX en esta CPU: no se arranca
        jmp parado
.sem_nx:
        mov eax, [ebx + OFF(_ap_cr3)]
        mov cr3, eax
        PASO 3                       ; CR4 y CR3 cargados

        ; CR0 del principal (PE, MP, NE, WP, PG; sin EM, CD ni NW), sin TS:
        ; tras el INIT el procesador trae CD y NW (caché desactivada); con el
        ; CR0 del principal queda igual que él. smp_iniciar() corre después de
        ; fpu_iniciar(), así que ese CR0 ya no lleva EM.
        mov eax, [ebx + OFF(_ap_cr0)]
        and eax, 0xFFFFFFF7          ; sin TS: la FPU es suya desde el principio
        mov cr0, eax
        jmp short paginado           ; vacía la cola de instrucciones
paginado:
        PASO 4                       ; paginación activa

        mov esp, [ebx + OFF(_ap_pilha)]    ; la pila de kernel de esta CPU
        push dword [ebx + OFF(_ap_indice)] ; argumento: el número de esta CPU
        PASO 5                       ; salto a C
        mov eax, [ebx + OFF(_ap_entrada)]
        call eax
parado:
        cli
        hlt
        jmp parado

; --------------------------------------------------------------- los huecos
align 4
_ap_cr0:       dd 0                   ; el CR0 del principal
_ap_cr3:       dd 0                   ; el CR3 (directorio de páginas del kernel)
_ap_cr4:       dd 0                   ; el CR4 del principal
_ap_nxe:       dd 0                   ; D349: 1 = poner EFER.NXE (el principal usa NX)
_ap_xd:        dd 0                   ; D349: 1 = quitar XD_DISABLE antes (la principal lo hizo)
_ap_pilha:     dd 0                   ; el tope de la pila de kernel de la CPU
_ap_entrada:   dd 0                   ; la función de entrada del kernel
_ap_indice:    dd 0                   ; el número de esta CPU
_ap_progresso: dd 0                   ; el último paso alcanzado (diagnóstico)

; ------------------------------------------------- GDT local (solo para llegar
; a C; la GDT de verdad de la CPU se carga en gdt_carregar)
align 8
_ap_gdt_local:
        dq 0x0000000000000000        ; nulo
        dq 0x00CF9A000000FFFF        ; código: base 0, límite 4 GB
        dq 0x00CF92000000FFFF        ; datos: base 0, límite 4 GB
_ap_gdt_local_fim:
align 4
_ap_gdt_local_desc:
        dw _ap_gdt_local_fim - _ap_gdt_local - 1
_ap_gdt_base:
        dd 0                        ; la base la escribe el principal
_ap_tam:
