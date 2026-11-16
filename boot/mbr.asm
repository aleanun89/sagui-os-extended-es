; SPDX-License-Identifier: GPL-3.0-only
; mbr.asm - etapa 1 del cargador de arranque de SaguiOS.
;
; La BIOS copia este sector (512 bytes) a 0x7C00 y salta aquí en modo real.
; Su trabajo es pequeño a propósito: descubrir de qué disco venimos (DL),
; leer la cabecera de arranque en el LBA 1 y, con su información, traer la
; etapa 2 a 0x8000. Todo lo demás (A20, memoria, kernel, vídeo) queda en la
; etapa 2, que no tiene el límite de 512 bytes.
;
; Mapa del sector:
;   0x000-0x002  salto corto + NOP (formato esperado por BIOS que buscan BPB)
;   0x003-0x059  área de BPB falso, a cero. Algunas BIOS, al arrancar por USB,
;                escriben la geometría aquí. Por eso ningún código va en ella.
;   0x05A-0x1B7  código y mensajes
;   0x1B8-0x1BD  identificador del disco
;   0x1BE-0x1FD  tabla de particiones (la rellena tools/mkimagem.py)
;   0x1FE-0x1FF  firma 0x55AA (exigida por la BIOS)

bits 16
org 0x7C00

%include "cabecalho.inc"

        jmp short inicio
        nop
        times 0x5A - ($ - $$) db 0

inicio:
        ; Segmentos y pila en un estado conocido. La pila crece hacia abajo a
        ; desde 0x7C00, en un espacio libre de la memoria baja.
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00
        ; Hay BIOS que entran en 07C0:0000 en vez de 0000:7C00. El salto largo
        ; deja CS=0 para que las direcciones de "org 0x7C00" valgan siempre.
        jmp 0x0000:.cs_zerado
.cs_zerado:
        sti
        cld
        mov [disco], dl                 ; DL = número del disco de arranque dado por la BIOS

        mov si, msg_carregando
        call escrever

        ; Lectura por LBA (INT 13h AH=42h) cuando la BIOS tiene las extensiones:
        ; pendrive emulado como disco duro en el G470 y las VM. Sin ellas (CD
        ; con emulación de disco duro, D077, o BIOS antigua) leemos por CHS
        ; (AH=02h), con la geometría que da la propia BIOS (AH=08h).
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [disco]
        int 0x13
        jc .chs
        cmp bx, 0xAA55
        je .lba_ok
.chs:
        mov ah, 0x08
        mov dl, [disco]
        push es                         ; el AH=08h puede tocar ES (disquetes)
        int 0x13
        pop es
        jc ler_setores.falha
        and cl, 0x3F                    ; sectores por pista (bits 0-5)
        jz .sem_cabecalho               ; sin geometría útil
        mov [spt], cl
        inc dh                          ; DH = última cabeza
        mov [cabecas], dh
.lba_ok:

        ; Cabecera de arranque (LBA 1) a 0x7E00, justo después de este sector. El
        ; destino y el LBA ya vienen así en el paquete (la lectura por LBA no los
        ; cambia); la cantidad sí puede cambiarla la BIOS, y se repone.
        mov word [dap_qtd], 1
        call ler_setores
        cmp dword [CAB_ENDERECO + CAB_ASSINATURA], CAB_ASSINATURA_VAL
        jne .sem_cabecalho

        ; Etapa 2 a 0x8000, con tamaño y posición sacados de la cabecera.
        mov eax, [CAB_ENDERECO + CAB_E2_SETORES]
        test eax, eax
        jz .sem_cabecalho
        cmp eax, ESTAGIO2_MAX_SET
        ja .sem_cabecalho
        mov [dap_qtd], ax
        mov word [dap_destino], ESTAGIO2_ENDERECO
        mov eax, [CAB_ENDERECO + CAB_E2_LBA]
        mov [dap_lba], eax
        call ler_setores

        mov dl, [disco]                 ; la etapa 2 recibe el disco en DL, como nosotros
        jmp 0x0000:ESTAGIO2_ENDERECO

.sem_cabecalho:
        ; D330: hay BIOS que anuncian las extensiones y leen mal por LBA un disco sin
        ; LBA (86Box: la Award de la P3B-F trae el sector anterior). Si lo leído por
        ; LBA no es la cabecera, se repite por CHS; por CHS (spt != 0) ya no hay plan B.
        cmp byte [spt], 0
        je .chs
        mov si, msg_cabecalho
        jmp parar

; escribir: muestra la cadena terminada en cero apuntada por SI (teletipo de la BIOS).
escrever:
        lodsb
        test al, al
        jz .fim
        mov ah, 0x0E
        mov bx, 0x0007
        int 0x10
        jmp escrever
.fim:
        ret

; ler_setores: lee según el paquete "dap". Por CHS (spt != 0), un sector
; cada vez: así ninguna lectura cruza el fin de una pista. Por LBA, prueba 3
; veces, reiniciando el disco entre intentos (los pendrives a veces fallan en la
; primera lectura). Si no sale, cae en "parar", justo abajo.
ler_setores:
        cmp byte [spt], 0
        je .lba
.chs:
        ; Los LBA del MBR son pequeños (cabecera y etapa 2, < 70): las divisiones
        ; de 8 bits bastan y el cilindro nunca pasa de 255.
        mov ax, [dap_lba]
        div byte [spt]                  ; AL = pista, AH = sector - 1
        inc ah
        mov cl, ah
        xor ah, ah
        div byte [cabecas]              ; AL = cilindro, AH = cabeza
        mov dh, ah
        mov ch, al
        mov bx, [dap_destino]
        mov ax, 0x0201
        mov dl, [disco]
        int 0x13
        jc .falha
        add word [dap_destino], 512
        inc word [dap_lba]
        dec word [dap_qtd]
        jnz .chs
.ok:
        ret
.lba:
        mov di, 3
.tentar:
        mov si, dap
        mov ah, 0x42
        mov dl, [disco]
        int 0x13
        jnc .ok
        xor ah, ah
        mov dl, [disco]
        int 0x13
        dec di
        jnz .tentar
.falha:
        mov si, msg_leitura
        ; sigue en "parar"

; parar: muestra el mensaje en SI y se queda parado. Sin salida de depuración en el
; G470, el mensaje en pantalla es la única pista de lo que pasó.
parar:
        call escrever
.laco:
        hlt
        jmp .laco

disco:          db 0
spt:            db 0                    ; 0 = lectura por LBA
cabecas:        db 0

; Paquete de dirección de disco (formato definido por la especificación de las
; extensiones del INT 13h): tamaño, reservado, cantidad, destino seg:off, LBA.
align 4
dap:            db 0x10, 0
dap_qtd:        dw 0
dap_destino:    dw CAB_ENDERECO, 0      ; offset, segmento (primero, la cabecera)
dap_lba:        dd CAB_LBA, 0

msg_carregando: db "SaguiOS: cargando...", 13, 10, 0
msg_cabecalho:  db "Cabecera invalida", 0
msg_leitura:    db "Error de lectura", 0

        times 0x1B8 - ($ - $$) db 0     ; falla al ensamblar si el código pasa del límite
id_disco:       dd 0x53475549           ; 'IUGS' - identificador del disco
                dw 0
tabela_part:    times 64 db 0           ; la rellena tools/mkimagem.py
                dw 0xAA55
