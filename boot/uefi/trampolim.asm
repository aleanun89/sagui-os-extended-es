; SPDX-License-Identifier: GPL-2.0-or-later
; trampolim.asm - del cargador UEFI al kernel de SaguiOS en modo protegido de 32 bits (D264).
;
; El cargador (boot/uefi/cargador.c) copia este bloque a la zona de transferencia
; (memoria propia por debajo de 4 GB, de tipo LoaderCode, que el firmware mapea
; 1:1 y deja ejecutar) y salta a él después de ExitBootServices. Es código
; independiente de la posición: solo usa direcciones relativas (RIP o la base que
; da un call/pop), porque la zona puede quedar en cualquier sitio.
;
; Dos entradas, una por firmware:
;   entrada64 - firmware x64: modo largo con paginación. Carga la GDT propia, pasa al
;               segmento de código de 32 bits (modo de compatibilidad) con un retorno
;               lejano, apaga la paginación (eso sale del modo largo) y borra EFER.LME.
;   entrada32 - firmware IA32: modo protegido de 32 bits, quizá con paginación. Carga la
;               GDT y apaga la paginación.
; Las dos siguen en "comum", que deja la CPU como la deja la etapa 2 por BIOS:
; segmentos planos 0x08/0x10, sin paginación, CR4 a cero, sin IDT; el APIC local en
; "cable virtual" (LINT0 = ExtINT, LINT1 = NMI), las entradas de los IOAPIC enmascaradas
; y el HPET sin la ruta heredada, para que el PIC 8259 y el PIT funcionen como tras la
; BIOS. Después mueve el kernel a su sitio (1 MB) si hacía falta y salta al kernel con
; EAX = BOOT_MAGICO y EBX = boot_info, igual que la etapa 2.
;
; El bloque de parámetros (dirección en RCX en x64, en [esp+4] en IA32) lo rellena el
; cargador; los desplazamentos P_* están repetidos en cargador.c (struct parametros).

BOOT_MAGICO     equ 0x53414755
SEL_CODIGO32    equ 0x08
SEL_DADOS       equ 0x10

P_DESTINO       equ 0           ; u32 dirección física del kernel (1 MB)
P_ORIGEM        equ 4           ; u32 donde el cargador dejó el kernel
P_BYTES         equ 8           ; u32 bytes a mover (múltiplo de 4)
P_ENTRADA       equ 12          ; u32 entrada del kernel
P_BOOTINFO      equ 16          ; u32 boot_info
P_PILHA         equ 20          ; u32 cima de la pila provisional (en la zona)
P_OPCOES        equ 24          ; u32 bit 0 = APIC local en cable virtual, bit 1 = enmascarar IOAPIC,
                                ;     bit 2 = HPET sin ruta heredada
P_IOAPIC        equ 28          ; u32[4] direcciones de los IOAPIC de la MADT (0 = no hay)
P_HPET          equ 44          ; u32 dirección del HPET (tabla HPET), 0 = no hay

bits 32
cabecera:
        dd entrada64 - cabecera         ; el cargador lee aquí dónde está cada entrada
        dd entrada32 - cabecera

; ---------------------------------------------------------------- x64
bits 64
entrada64:
        cli
        mov esi, ecx                    ; bloque de parámetros (por debajo de 4 GB)
        lea rax, [rel gdt]
        mov [rel gdtr + 2], rax
        lgdt [rel gdtr]
        lea rax, [rel compat64]
        push qword SEL_CODIGO32
        push rax
        o64 retf                        ; a 0x08:compat64, código de 32 bits

bits 32
compat64:
        ; Modo de compatibilidad: 32 bits, aún con la paginación del firmware (1:1).
        ; Hasta cargar la pila nueva no se usa la pila: ESP es la mitad baja de RSP.
        mov eax, cr0
        and eax, 0x7FFFFFFF             ; PG = 0: la CPU sale del modo largo
        mov cr0, eax
        mov ecx, 0xC0000080             ; EFER
        rdmsr
        and eax, ~0x00000D00            ; LME (8), LMA (10, solo lectura) y NXE (11)
        wrmsr
        jmp comum

; ---------------------------------------------------------------- IA32
entrada32:
        cli
        mov esi, [esp + 4]              ; bloque de parámetros
        call .base
.base:  pop ebp
        lea eax, [ebp + gdt - .base]
        mov [ebp + gdtr + 2 - .base], eax
        lgdt [ebp + gdtr - .base]
        lea eax, [ebp + ia32_sem_pg - .base]
        push dword SEL_CODIGO32
        push eax
        retf                            ; recarga CS con el selector de 32 bits propio
ia32_sem_pg:
        mov eax, cr0
        and eax, 0x7FFFFFFF             ; sin paginación (el firmware IA32 puede tenerla con PAE)
        mov cr0, eax
        jmp comum

; ---------------------------------------------------------------- común (32 bits, sin paginación)
comum:
        mov ax, SEL_DADOS
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov esp, [esi + P_PILHA]
        xor eax, eax
        mov cr4, eax                    ; sin PAE, PGE, OSFXSR...: el kernel pone lo suyo
        mov eax, cr0
        and eax, 0x1FFAFFF3             ; fuera PG, CD, NW, AM, WP, TS y EM
        or eax, 0x11                    ; PE y ET
        mov cr0, eax
        call .base
.base:  pop ebp
        lidt [ebp + idtr_nula - .base]  ; sin IDT hasta que el kernel ponga la suya

        test dword [esi + P_OPCOES], 1
        jz .sem_lapic
        call apic_cabo_virtual
.sem_lapic:
        test dword [esi + P_OPCOES], 2
        jz .sem_ioapic
        call ioapic_mascarar
.sem_ioapic:
        test dword [esi + P_OPCOES], 4
        jz .sem_hpet
        mov edi, [esi + P_HPET]
        test edi, edi
        jz .sem_hpet
        and dword [edi + 0x10], ~2      ; GEN_CONF.LEG_RT_CNF = 0: el PIT vuelve a la IRQ 0
.sem_hpet:

        ; Mueve el kernel a su sitio (el ramdisk se queda donde está). Origen y destino pueden solaparse
        ; (si el firmware no dejó reservar 1 MB): se copia en el sentido seguro.
        push dword [esi + P_ENTRADA]
        push dword [esi + P_BOOTINFO]
        mov edi, [esi + P_DESTINO]
        mov ecx, [esi + P_BYTES]
        mov esi, [esi + P_ORIGEM]
        shr ecx, 2
        cmp edi, esi
        je .copiado
        jb .adelante
        lea esi, [esi + ecx * 4 - 4]
        lea edi, [edi + ecx * 4 - 4]
        std
        rep movsd
        cld
        jmp .copiado
.adelante:
        cld
        rep movsd
.copiado:
        pop ebx                         ; boot_info
        pop edx                         ; entrada del kernel
        mov eax, BOOT_MAGICO
        jmp edx

; apic_cabo_virtual: si el APIC local está encendido, lo deja como la BIOS: modo
; xAPIC (si el firmware lo dejó en x2APIC, se apaga y se enciende en xAPIC, la
; única transición permitida), LINT0 = ExtINT, LINT1 = NMI, temporizador y errores
; enmascarados, TPR 0 y activo por software con el vector espurio 0xFF (el kernel
; lo conoce, idt.c). Sin APIC, o apagado, no hace nada: el 8259 llega por INTR.
apic_cabo_virtual:
        push esi
        mov eax, 1
        cpuid
        test edx, 1 << 9
        jz .fim
        mov ecx, 0x1B                   ; IA32_APIC_BASE
        rdmsr
        test eax, 1 << 11               ; EN
        jz .fim
        test edx, edx                   ; base por encima de 4 GB: no se toca
        jnz .fim
        test eax, 1 << 10               ; EXTD: x2APIC
        jz .xapic
        and eax, ~((1 << 11) | (1 << 10))
        wrmsr                           ; apagado
        or eax, 1 << 11
        wrmsr                           ; encendido en xAPIC
.xapic:
        and eax, 0xFFFFF000
        mov edi, eax
        mov dword [edi + 0x320], 0x10000        ; temporizador: enmascarado
        mov dword [edi + 0x370], 0x10000        ; errores: enmascarado
        mov dword [edi + 0x350], 0x700          ; LINT0: ExtINT (el 8259)
        mov dword [edi + 0x360], 0x400          ; LINT1: NMI
        mov dword [edi + 0x080], 0              ; TPR
        mov dword [edi + 0x0F0], 0x1FF          ; activo, vector espurio 0xFF
        mov ecx, 64                     ; acaba las que el firmware dejara en servicio
.eoi:   mov dword [edi + 0x0B0], 0
        loop .eoi
.fim:
        pop esi
        ret

; ioapic_mascarar: todas las entradas de redirección de cada IOAPIC a su valor de
; reinicio (enmascaradas). Con el PIC 8259 (lo normal en SaguiOS) ninguna IRQ debe
; llegar además por el IOAPIC; con --ioapic el kernel las programa él (D166).
ioapic_mascarar:
        push esi
        lea ebx, [esi + P_IOAPIC]
        mov edx, 4
.ioapic:
        mov edi, [ebx]
        test edi, edi
        jz .proximo
        mov dword [edi], 1              ; IOREGSEL = versión
        mov ecx, [edi + 0x10]
        shr ecx, 16
        and ecx, 0xFF                   ; última entrada
        inc ecx
        mov eax, 0x10
.entrada:
        mov [edi], eax
        mov dword [edi + 0x10], 0x10000 ; parte baja: enmascarada, vector 0
        inc eax
        mov [edi], eax
        mov dword [edi + 0x10], 0       ; parte alta: destino 0
        inc eax
        loop .entrada
.proximo:
        add ebx, 4
        dec edx
        jnz .ioapic
        pop esi
        ret

align 8
gdt:
        dq 0
        dq 0x00CF9A000000FFFF           ; 0x08 código de 32 bits, base 0, límite 4 GB
        dq 0x00CF92000000FFFF           ; 0x10 datos
        dq 0x00AF9A000000FFFF           ; 0x18 código de 64 bits (no se usa; por si acaso)
gdt_fim:
align 4
        dw 0
gdtr:
        dw gdt_fim - gdt - 1
        dq 0                            ; base: la rellena la entrada (8 bytes en x64, 4 en IA32)
        dw 0
idtr_nula:
        dw 0
        dq 0
