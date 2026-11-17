; SPDX-License-Identifier: GPL-3.0-only
; estagio2.asm - etapa 2 del cargador de arranque de SaguiOS.
;
; El MBR nos deja en 0000:8000, modo real, con el disco de arranque en DL y la
; cabecera de arranque en 0x7E00. Aquí preparamos todo lo que solo la BIOS sabe hacer,
; porque después del modo protegido ya no hay BIOS:
;
;   [1/6] línea A20         (sin ella, el bit 20 de la dirección queda fijo en cero)
;   [2/6] mapa de memoria   (INT 15h E820)
;   [3/6] kernel            (INT 13h, por bloques, copiado a 1 MB)
;   [4/6] ramdisk           (idem, justo después del fin del kernel)
;   [5/6] vídeo             (INT 10h VBE: elegir modo; cambiar solo si se pide)
;   [6/6] modo protegido    (GDT propia y salto al kernel)
;
; Antes de todo eso, conferir_cpu (D310): el kernel y los programas se compilan
; para i686 y llevan CMOV/FCMOV; en una CPU i586 o anterior la primera CMOV daba
; #UD, triple fallo y un reinicio en bucle. Si falta, se avisa y la máquina se queda
; parada (sin reiniciar). En una i686 no se escribe nada.
;
; Cada etapa muestra una línea en pantalla. En el G470 no hay serie: si se bloquea, la
; foto de la pantalla muestra en qué etapa se detuvo.
;
; Cargar por encima de 1 MB: el INT 13h solo escribe por debajo de 1 MB. Leemos cada
; bloque en un búfer en 0x10000 y lo copiamos hacia arriba usando "unreal mode":
; entramos brevemente en modo protegido solo para cargar DS/ES con un
; descriptor de límite 4 GB y volvemos al modo real. El procesador guarda el
; límite en caché, así que las direcciones de 32 bits siguen funcionando en modo real
; y la BIOS sigue disponible. La entrada se rehace antes de cada copia,
; porque una rutina de la BIOS puede haber recargado los segmentos.

bits 16
org 0x8000

%include "cabecalho.inc"
%include "bootinfo.inc"

BUFFER_SEG      equ 0x1000              ; búfer de lectura en 0x10000
BUFFER_LINEAR   equ 0x10000
BLOCO_SETORES   equ 64                  ; 32 KB por lectura: dentro del límite de 127 sectores
                                        ; y sin cruzar frontera de 64 KB

SEL_CODIGO      equ 0x08
SEL_DADOS       equ 0x10

inicio:
        cli
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, 0x7C00
        sti
        cld
        mov [disco], dl

        ; Pone a cero la boot_info y rellena lo que ya sabemos.
        mov di, BOOTINFO
        mov cx, BOOTINFO_TAM / 2
        xor ax, ax
        rep stosw
        mov dword [BOOTINFO + BI_ASSINATURA], 0x49424753     ; 'SGBI'
        mov dword [BOOTINFO + BI_VERSAO], 1
        movzx eax, byte [disco]
        mov [BOOTINFO + BI_DISCO], eax
        mov si, CAB_ENDERECO + CAB_TESTE        ; repasa el nombre del autoprueba (si hay)
        mov di, BOOTINFO + BI_TESTE
        mov cx, 64
        rep movsb
        movzx eax, word [CAB_ENDERECO + CAB_OPCOES]
        mov [BOOTINFO + BI_OPCOES], eax
        movzx eax, byte [CAB_ENDERECO + CAB_MOTOR]   ; motor 3D (D158): 1 = solo software
        test al, al
        jz .sem_motor
        or dword [BOOTINFO + BI_OPCOES], OPC_SGL_SOFTWARE
.sem_motor:
        movzx eax, word [CAB_ENDERECO + CAB_RES_ALT]   ; resolução preferida (D058)
        shl eax, 16
        mov ax, [CAB_ENDERECO + CAB_RES_LARG]
        mov [BOOTINFO + BI_RES_PREF], eax
        movzx eax, byte [CAB_ENDERECO + CAB_BPP_PREF]  ; profundidade preferida (D078)
        mov [BOOTINFO + BI_BPP_PREF], eax

        mov si, msg_titulo
        call escrever

        ; D310: la CPU tiene que ser i686 (CMOV). Se mira lo primero, antes de leer
        ; nada del disco, para que el aviso salga enseguida.
        call conferir_cpu
        jc .falha_cpu

        ; D255: Ctrl pulsado al arrancar = modo rescate (consola con un shell de
        ; root, sin escritorio ni servicios). Solo se mira el estado del teclado
        ; de la BIOS en este momento: no retrasa el arranque normal.
        mov ah, 0x02
        int 0x16
        test al, 0x04
        jz .sem_rescate
        or dword [BOOTINFO + BI_OPCOES], OPC_RESCATE
        mov si, msg_rescate
        call escrever
.sem_rescate:
        ; D323: Mayús (izquierda o derecha) pulsado al arrancar = modo seguro de la
        ; aceleración (2D por la CPU, 3D por software, sin la configuración guardada).
        mov ah, 0x02
        int 0x16
        test al, 0x03
        jz .sem_seguro
        or dword [BOOTINFO + BI_OPCOES], OPC_ACEL_SEGURA
        mov si, msg_acel_segura
        call escrever
.sem_seguro:
        ; v8.5.4.2 (D379): M (de «modo») pulsada al arrancar = menú del modo de la raíz (live desde el
        ; medio, todo a la RAM, instalado, rescate). Solo se mira si ya hay una M en el búfer del
        ; teclado: sin ella no se espera nada. Lo elegido va en BI_OPCOES (bits 17-19), no en la cabecera.
        call olhar_m

        call preparar_disco

; ---------------------------------------------------------------- [1/6] A20
        call habilitar_a20
        jc .falha_a20
        mov si, msg_a20
        call escrever

; ---------------------------------------------------------------- [2/6] memoria
        call ler_mapa_memoria
        cmp dword [BOOTINFO + BI_MAPA_QTD], 0
        je .falha_memoria
        mov si, msg_mem
        call escrever
        mov eax, [BOOTINFO + BI_MAPA_QTD]
        call escrever_decimal
        mov si, msg_mem2
        call escrever
        call somar_memoria_mb
        call escrever_decimal
        mov si, msg_mb
        call escrever

; ---------------------------------------------------------------- [3/6] kernel
        mov si, msg_kernel
        call escrever
.ler_kernel:
        mov eax, [CAB_ENDERECO + CAB_K_ENDERECO]
        mov [BOOTINFO + BI_K_INICIO], eax
        mov eax, [CAB_ENDERECO + CAB_K_BYTES]
        mov [BOOTINFO + BI_K_BYTES], eax
        mov eax, [CAB_ENDERECO + CAB_K_LBA]
        mov ecx, [CAB_ENDERECO + CAB_K_SETORES]
        mov edi, [CAB_ENDERECO + CAB_K_ENDERECO]
        call carregar_regiao
        mov esi, [CAB_ENDERECO + CAB_K_ENDERECO]
        mov ecx, [CAB_ENDERECO + CAB_K_BYTES]
        call somar_bytes
        cmp eax, [CAB_ENDERECO + CAB_K_SOMA]
        je .kernel_ok
        call recorrer_a_chs             ; D330: lo leído por LBA no cuadra: otra vez por CHS
        jc .falha_soma
        jmp .ler_kernel
.kernel_ok:
        mov si, msg_ok
        call escrever

; ---------------------------------------------------------------- [4/6] ramdisk
        mov si, msg_ramdisk
        call escrever
        mov eax, [CAB_ENDERECO + CAB_R_ENDERECO]
        mov [BOOTINFO + BI_R_INICIO], eax
        mov eax, [CAB_ENDERECO + CAB_R_BYTES]
        mov [BOOTINFO + BI_R_BYTES], eax
        mov edi, [CAB_ENDERECO + CAB_R_ENDERECO]
        mov ecx, [CAB_ENDERECO + CAB_R_BYTES]
        call conferir_ram               ; D181: avisa si el ramdisk no cabe en la RAM del E820
.ler_ramdisk:
        mov eax, [CAB_ENDERECO + CAB_R_LBA]
        mov ecx, [CAB_ENDERECO + CAB_R_SETORES]
        mov edi, [CAB_ENDERECO + CAB_R_ENDERECO]
        call carregar_regiao
        mov esi, [CAB_ENDERECO + CAB_R_ENDERECO]
        mov ecx, [CAB_ENDERECO + CAB_R_BYTES]
        call somar_bytes
        cmp eax, [CAB_ENDERECO + CAB_R_SOMA]
        je .ramdisk_ok
        call recorrer_a_chs             ; D330
        jc .falha_soma
        jmp .ler_ramdisk
.ramdisk_ok:
        mov si, msg_ok
        call escrever

        ; v8.5.4.2: y otra vez tras leer el núcleo y el initramfs (la M que llegó mientras tanto).
        call olhar_m

; ---------------------------------------------------------------- [5/6] vídeo
        mov si, msg_video
        call escrever
        call procurar_vesa
        cmp dword [BOOTINFO + BI_VESA_OK], 0
        je .sem_vesa
        mov eax, [BOOTINFO + BI_VESA_LARG]
        call escrever_decimal
        mov al, 'x'
        call escrever_char
        mov eax, [BOOTINFO + BI_VESA_ALT]
        call escrever_decimal
        mov al, 'x'
        call escrever_char
        mov eax, [BOOTINFO + BI_VESA_BPP]
        call escrever_decimal
        cmp byte [s3_bancos], 0
        je .video_crlf
        mov si, msg_s3_janela
        call escrever
.video_crlf:
        mov si, msg_crlf
        call escrever
        jmp .video_fim
.sem_vesa:
        mov si, msg_sem_vesa
        call escrever
.video_fim:

; ---------------------------------------------------------------- [6/6] modo protegido
        mov si, msg_pm
        call escrever

        ; Cambio de modo de vídeo solo cuando el build lo pide (fase 4 en adelante) y
        ; siempre al final, porque después de él el texto de la BIOS desaparece.
        test word [CAB_ENDERECO + CAB_OPCOES], OPC_VESA
        jz .entrar_pm
        cmp dword [BOOTINFO + BI_VESA_OK], 0
        je .entrar_pm
        mov bx, [BOOTINFO + BI_VESA_MODO]
        cmp byte [s3_bancos], 0         ; D331: modo con bancos de una S3, sin el bit 14
        jne .trocar_modo
        or bx, 0x4000                   ; bit 14: framebuffer lineal
.trocar_modo:
        mov ax, 0x4F02
        int 0x10
        cmp ax, 0x004F
        jne .entrar_pm                  ; falló: el kernel sigue en modo texto y avisa
        cmp byte [s3_bancos], 0
        je .modo_ativo
        call s3_abrir_janela            ; D331: si no responde, vuelve al modo texto
        jc .entrar_pm
.modo_ativo:
        mov dword [BOOTINFO + BI_VESA_ATIVO], 1

.entrar_pm:
        cli
        lgdt [gdt_descritor]
        mov eax, cr0
        or eax, 1
        mov cr0, eax
        jmp SEL_CODIGO:modo_protegido   ; salto largo: recarga CS con el selector de 32 bits

.falha_a20:
        mov si, msg_falha_a20
        jmp parar
.falha_memoria:
        mov si, msg_falha_mem
        jmp parar
.falha_soma:
        mov si, msg_falha_soma
        jmp parar
.falha_cpu:
        ; SI = la causa (sin CPUID o sin CMOV); después, lo que hace falta.
        call escrever
        mov si, msg_cpu_requisito
        call escrever
        cli                             ; sin interrupciones: nada puede despertarla
.parada_cpu:
        hlt
        jmp .parada_cpu

; ============================================================================
; CPU (D310)
; ============================================================================

; conferir_cpu: comprueba que la CPU tiene CPUID y, en la hoja 1, CMOV (EDX bit 15).
; Toda CPU con CMOV (familia 6 o posterior) tiene también TSC y FPU; FXSR/SSE no
; se exigen (el kernel los mira por CPUID). Salida: CF=0 vale; CF=1 no vale, con
; SI apuntando a la causa. Conserva todos los registros salvo SI y los indicadores.
conferir_cpu:
        pushad
        ; El CPUID existe si el bit ID (21) del EFLAGS se puede cambiar (un 386 o un
        ; 486 antiguo no lo deja).
        pushfd
        pop eax
        mov ecx, eax
        xor eax, 0x00200000
        push eax
        popfd
        pushfd
        pop eax
        push ecx
        popfd                           ; deja el EFLAGS como estaba
        xor eax, ecx
        test eax, 0x00200000
        jz .sem_cpuid
        xor eax, eax                    ; hoja 0: la hoja más alta que hay
        cpuid
        cmp eax, 1
        jb .sem_cpuid
        mov eax, 1
        cpuid
        test edx, 1 << 15               ; CMOV
        jz .sem_cmov
        popad
        clc
        ret
.sem_cpuid:
        popad
        mov si, msg_cpu_sem_cpuid
        stc
        ret
.sem_cmov:
        popad
        mov si, msg_cpu_sem_cmov
        stc
        ret

; ============================================================================
; A20
; ============================================================================

; a20_ligada: escribe valores distintos en 0x000500 y 0x100500 (FFFF:0510).
; Si la escritura en el segundo aparece en el primero, la dirección dio la vuelta y la
; A20 está desactivada. Salida: ZF=1 desactivada, ZF=0 activada. Usa AX, DX.
a20_ligada:
        pushf
        push ds
        push es
        push si
        push di
        cli
        xor ax, ax
        mov ds, ax
        not ax
        mov es, ax
        mov si, 0x0500
        mov di, 0x0510
        mov dl, [ds:si]                 ; guarda los valores originales
        mov dh, [es:di]
        mov byte [ds:si], 0x00
        mov byte [es:di], 0xFF
        mov al, [ds:si]
        mov [es:di], dh
        mov [ds:si], dl
        pop di
        pop si
        pop es
        pop ds
        popf
        cmp al, 0xFF
        ret

; habilitar_a20: prueba en orden del más seguro al más antiguo y comprueba
; después de cada intento. CF=1 si ningún método funcionó.
habilitar_a20:
        call a20_ligada
        jne .metodo0
        ; Método 1: servicio de la BIOS (INT 15h AX=2401).
        mov ax, 0x2401
        int 0x15
        call a20_ligada
        jne .metodo1
        ; Método 2: "A20 rápida" por el puerto 0x92 (bit 1). El bit 0 de ese puerto
        ; reinicia la máquina, así que siempre se escribe como cero.
        in al, 0x92
        or al, 0x02
        and al, 0xFE
        out 0x92, al
        call esperar_um_pouco
        call a20_ligada
        jne .metodo2
        ; Método 3: por el puerto de salida del controlador de teclado (i8042).
        call kbc_livre_escrita
        mov al, 0xAD                    ; desactiva el teclado durante la operación
        out 0x64, al
        call kbc_livre_escrita
        mov al, 0xD0                    ; pide el byte del puerto de salida
        out 0x64, al
        call kbc_tem_dado
        in al, 0x60
        mov bl, al
        call kbc_livre_escrita
        mov al, 0xD1                    ; va a escribir en el puerto de salida
        out 0x64, al
        call kbc_livre_escrita
        mov al, bl
        or al, 0x02                     ; bit 1 = A20
        out 0x60, al
        call kbc_livre_escrita
        mov al, 0xAE                    ; vuelve a activar el teclado
        out 0x64, al
        call kbc_livre_escrita
        mov cx, 16                      ; el controlador puede tardar en aplicar
.espera_kbc:
        call esperar_um_pouco
        call a20_ligada
        jne .metodo3
        loop .espera_kbc
        stc
        ret
.metodo0:
        xor eax, eax
        jmp .registra
.metodo1:
        mov eax, 1
        jmp .registra
.metodo2:
        mov eax, 2
        jmp .registra
.metodo3:
        mov eax, 3
.registra:
        mov [BOOTINFO + BI_A20], eax
        clc
        ret

; Esperas con límite: si el controlador no responde, seguimos adelante en
; vez de bloquearnos para siempre.
kbc_livre_escrita:
        push cx
        mov cx, 0xFFFF
.l:     in al, 0x64
        test al, 2
        jz .fim
        loop .l
.fim:   pop cx
        ret

kbc_tem_dado:
        push cx
        mov cx, 0xFFFF
.l:     in al, 0x64
        test al, 1
        jnz .fim
        loop .l
.fim:   pop cx
        ret

; Unos pocos milisegundos: el puerto 0x80 (diagnóstico POST) es seguro
; para escribir y cada acceso a él tarda ~1 µs en el bus.
esperar_um_pouco:
        push cx
        mov cx, 2000
.l:     out 0x80, al
        loop .l
        pop cx
        ret

; ============================================================================
; Mapa de memoria (E820)
; ============================================================================

ler_mapa_memoria:
        xor ebx, ebx                    ; EBX = "continuación", 0 en la primera llamada
        mov di, MAPA_E820
        xor bp, bp
.proxima:
        mov dword [di + 20], 1          ; atributo extendido ACPI: "entrada válida"
        mov eax, 0xE820
        mov edx, 0x534D4150             ; 'SMAP'
        mov ecx, 24
        int 0x15
        jc .fim                         ; carry: sin soporte o fin de la lista
        cmp eax, 0x534D4150
        jne .fim
        mov eax, [di + 8]               ; ignora regiones de tamaño cero
        or eax, [di + 12]
        jz .pula
        inc bp
        add di, 24
        cmp bp, MAPA_MAX
        jae .fim
.pula:
        test ebx, ebx
        jnz .proxima
.fim:
        movzx eax, bp
        mov [BOOTINFO + BI_MAPA_QTD], eax
        mov dword [BOOTINFO + BI_MAPA_END], MAPA_E820
        ret

; somar_memoria_mb: suma las regiones del tipo 1 (RAM libre) y devuelve en EAX el
; total en MB, solo para el mensaje de progreso.
somar_memoria_mb:
        xor esi, esi                    ; suma baja
        xor edi, edi                    ; suma alta
        mov cx, [BOOTINFO + BI_MAPA_QTD]
        mov bx, MAPA_E820
.l:     jcxz .fim
        cmp dword [bx + 16], 1
        jne .prox
        add esi, [bx + 8]
        adc edi, [bx + 12]
.prox:  add bx, 24
        dec cx
        jmp .l
.fim:   mov eax, esi
        shrd eax, edi, 20               ; (alta:baja) >> 20 = MB
        ret

; ============================================================================
; Disco
; ============================================================================

; preparar_disco: como el MBR, lee por LBA (AH=42h) si la BIOS tiene las
; extensiones y, si no, por CHS (AH=02h) con la geometría de AH=08h. Sin las
; extensiones quedan el CD con emulación de disco duro (D077), en el que la BIOS
; presenta la imagen de dentro del CD como el disco 0x80, y las BIOS antiguas.
; También pregunta por El Torito (AX=4B01h): si el "disco" es la imagen de un
; CD, el kernel lo sabe (OPC_CD) y no intenta grabar en él.
;
; D330: hay BIOS de la época que anuncian las extensiones y leen mal por LBA un
; disco sin LBA (86Box: la Award de la P3B-F devuelve el sector anterior; la AMI
; del 430HX, con traducción LARGE, se equivoca de cilindro a partir del primero).
; Por eso la geometría se pide siempre (plan B) y, si la cabecera (LBA 1) leída por
; LBA no es la que dejó el MBR en 0x7E00, se lee por CHS. Solo se cambia cuando la
; lectura por LBA terminó bien y trajo otra cosa: un fallo de lectura (un pendrive
; que falla la primera vez) sigue por LBA, con los reintentos de siempre.
preparar_disco:
        mov byte [tem_ext], 0
        mov ah, 0x41
        mov bx, 0x55AA
        mov dl, [disco]
        int 0x13
        jc .geometria
        cmp bx, 0xAA55
        jne .geometria
        mov byte [tem_ext], 1
.geometria:
        mov byte [spt_bios], 0
        mov ah, 0x08
        mov dl, [disco]
        push es
        int 0x13
        pop es
        jc .decidir                     ; sin geometría: se queda en LBA y avisa si falla
        and cl, 0x3F
        mov [spt_bios], cl
        inc dh
        mov [cab_bios], dh
.decidir:
        cmp byte [tem_ext], 0
        je .chs
        call conferir_lba
        jnc .el_torito
        call recorrer_a_chs             ; la BIOS lee mal por LBA (avisa en pantalla)
        jmp .el_torito
.chs:
        call passar_a_chs               ; sin geometría (CF=1): se queda en LBA y avisa si falla
.el_torito:
        mov byte [pacote_cd], 0
        mov ax, 0x4B01                  ; "devuelve el estado de la emulación", sin terminarla
        mov dl, [disco]
        mov si, pacote_cd
        int 0x13
        jc .fim
        cmp byte [pacote_cd], 0x13      ; tamaño del paquete de la especificación
        jne .fim
        mov al, [pacote_cd + 1]         ; tipo de medio: 0 = sin emulación, 4 = disco duro
        and al, 0x0F
        jz .fim
        or dword [BOOTINFO + BI_OPCOES], OPC_CD
.fim:
        ret

; passar_a_chs (D330): lee desde ahora por CHS con la geometría de AH=08h. CF=1 si
; la BIOS no la dio (se sigue por LBA).
passar_a_chs:
        cmp byte [spt_bios], 0
        je .sem
        mov al, [spt_bios]
        mov [spt], al
        mov al, [cab_bios]
        mov [cabecas], al
        mov dword [BOOTINFO + BI_CHS], 1
        clc
        ret
.sem:
        stc
        ret

; recorrer_a_chs (D330): plan B cuando lo leído por LBA no es lo que tenía que ser.
; Pasa a CHS y lo dice en pantalla; CF=1 si ya se leía por CHS o no hay geometría
; (entonces no hay nada más que probar).
recorrer_a_chs:
        cmp byte [spt], 0
        jne .nao
        call passar_a_chs
        jc .nao
        mov si, msg_lba_ruim
        call escrever
        clc
        ret
.nao:
        stc
        ret

; conferir_lba (D330): lee por LBA la cabecera (LBA 1) al búfer bajo y la compara con
; la que dejó el MBR en CAB_ENDERECO. CF=1 solo si la lectura terminó bien y los datos
; son otros; si la BIOS da error, CF=0 (lo resuelven los reintentos de ler_setores).
conferir_lba:
        push es
        mov word [dap_qtd], 1
        mov word [dap_destino], 0
        mov word [dap_destino + 2], BUFFER_SEG
        mov dword [dap_lba], CAB_LBA
        mov dword [dap_lba + 4], 0
        mov si, dap
        mov ah, 0x42
        mov dl, [disco]
        int 0x13
        jc .igual
        mov ax, BUFFER_SEG
        mov es, ax
        xor di, di
        mov si, CAB_ENDERECO
        mov cx, 256
        cld
        repe cmpsw
        jne .distinta
.igual:
        pop es
        clc
        ret
.distinta:
        pop es
        stc
        ret

; ler_setores: ejecuta el paquete "dap" con 3 intentos. Por CHS, hasta el fin
; de la pista en cada llamada (ninguna lectura cruza una pista: hay BIOS que
; no lo aceptan).
ler_setores:
        cmp byte [spt], 0
        jne ler_chs
        push di
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
        mov si, msg_falha_leitura
        jmp parar
.ok:
        pop di
        ret

ler_chs:
        push es
        push di
        mov es, [dap_destino + 2]
.trecho:
        mov di, 3
.tentar:
        mov eax, [dap_lba]
        xor edx, edx
        movzx ebx, byte [spt]
        div ebx                         ; EAX = pista, EDX = sector - 1
        mov si, bx
        sub si, dx                      ; sectores hasta el fin de la pista
        cmp si, [dap_qtd]
        jbe .qtd_ok
        mov si, [dap_qtd]
.qtd_ok:
        inc dl
        mov cl, dl
        xor edx, edx
        movzx ebx, byte [cabecas]
        div ebx                         ; EAX = cilindro, EDX = cabeza
        cmp eax, 1023
        ja .falha                       ; más allá de lo que alcanza el CHS
        mov dh, dl
        mov ch, al
        shl ah, 6
        or cl, ah                       ; bits 8-9 del cilindro en CL[7:6]
        mov bx, [dap_destino]
        mov ax, si
        mov ah, 0x02
        mov dl, [disco]
        int 0x13
        jnc .lido
        xor ah, ah
        mov dl, [disco]
        int 0x13
        dec di
        jnz .tentar
.falha:
        mov si, msg_falha_leitura
        jmp parar
.lido:
        mov ax, si
        shl ax, 9
        add [dap_destino], ax
        movzx eax, si
        add [dap_lba], eax
        sub [dap_qtd], si
        jnz .trecho
        pop di
        pop es
        ret

; conferir_ram (D181): EDI = inicio físico, ECX = bytes. Comprueba que
; [EDI, EDI+ECX) está entera dentro de RAM libre (tipo 1) del mapa E820,
; uniendo regiones contiguas. Si no, avisa con los MB que hacen falta y SIGUE:
; no se deja de arrancar en una máquina en la que antes arrancaba (una BIOS con
; un mapa raro); si de verdad falta RAM, la suma del final lo detecta, y el
; aviso de aquí dice por qué. Con 38 MB de ramdisk hace falta la RAM hasta
; ~40 MB; sin esto, en una máquina de 32 MB la copia caía fuera de la RAM y
; solo salía "datos leidos corruptos".
conferir_ram:
        pushad
        mov ebx, edi
        add ebx, ecx                    ; EBX = fin (exclusivo)
        jc .falta                       ; pasa de 4 GB
        mov edx, edi                    ; EDX = hasta dónde está cubierto
        mov bp, [BOOTINFO + BI_MAPA_QTD]
        inc bp                          ; como mucho una vuelta por región, y una más
.vuelta:
        cmp edx, ebx
        jae .ok
        dec bp
        jz .falta
        mov cx, [BOOTINFO + BI_MAPA_QTD]
        mov si, MAPA_E820
        xor edi, edi                    ; EDI = nuevo fin cubierto (0 = ninguna región sirve)
.region:
        jcxz .fin_regiones
        cmp dword [si + 16], 1          ; tipo 1: RAM libre
        jne .prox
        test byte [si + 20], 1          ; atributo extendido: entrada válida
        jz .prox
        cmp dword [si + 4], 0           ; empieza por encima de 4 GB
        jne .prox
        mov eax, [si]
        cmp eax, edx
        ja .prox                        ; empieza después de lo cubierto
        add eax, [si + 8]
        jc .hasta_4g
        cmp dword [si + 12], 0
        je .fin_ok
.hasta_4g:
        mov eax, 0xFFFFFFFF
.fin_ok:
        cmp eax, edx
        jbe .prox                       ; acaba antes de lo cubierto
        cmp eax, edi
        jbe .prox
        mov edi, eax
.prox:
        add si, 24
        dec cx
        jmp .region
.fin_regiones:
        test edi, edi
        jz .falta
        mov edx, edi
        jmp .vuelta
.falta:
        mov si, msg_aviso_ram
        call escrever
        mov eax, ebx
        add eax, 0xFFFFF
        jnc .redondeo
        mov eax, 0xFFFFFFFF
.redondeo:
        shr eax, 20                     ; MB, hacia arriba
        call escrever_decimal
        mov si, msg_aviso_ram2
        call escrever
.ok:
        popad
        ret

; carregar_regiao: EAX = LBA inicial, ECX = sectores, EDI = destino lineal (puede
; estar por encima de 1 MB). Lee por bloques al búfer bajo y copia hacia arriba.
carregar_regiao:
        mov word [blocos_lidos], 0
.laco:
        test ecx, ecx
        jz .fim
        mov edx, ecx
        cmp edx, BLOCO_SETORES
        jbe .tam_ok
        mov edx, BLOCO_SETORES
.tam_ok:
        push eax
        push ecx
        push edx
        mov [dap_qtd], dx
        mov word [dap_destino], 0
        mov word [dap_destino + 2], BUFFER_SEG
        mov [dap_lba], eax
        mov dword [dap_lba + 4], 0
        push edi                        ; la BIOS no promete preservar la mitad alta
        call ler_setores
        pop edi
        pop edx
        add [BOOTINFO + BI_SETORES_LIDOS], edx

        call entrar_unreal
        mov esi, BUFFER_LINEAR
        mov ecx, edx
        shl ecx, 7                      ; sectores * 512 / 4 = palabras dobles
        a32 rep movsd                   ; DS:ESI -> ES:EDI con direcciones de 32 bits

        inc word [blocos_lidos]         ; un punto tras el 1.er bloque y luego cada 16 (512 KB):
        mov bx, [blocos_lidos]          ; con 38 MB, 76 puntos y no 1216 (la pantalla no se
        and bx, 15                      ; desplaza y no se pierden las líneas de las etapas)
        cmp bx, 1
        jne .sem_ponto
        mov al, '.'
        call escrever_char
.sem_ponto:
        pop ecx
        pop eax
        add eax, edx
        sub ecx, edx
        jmp .laco
.fim:
        ret

; entrar_unreal: carga DS y ES con límite de 4 GB y vuelve al modo real.
entrar_unreal:
        cli
        push ds
        push es
        lgdt [gdt_descritor]
        mov eax, cr0
        or al, 1
        mov cr0, eax
        jmp short .pm                   ; vacía la cola de instrucciones
.pm:
        mov bx, SEL_DADOS
        mov ds, bx
        mov es, bx
        and al, 0xFE
        mov cr0, eax
        pop es                          ; los selectores vuelven a 0, el límite en caché queda
        pop ds
        sti
        ret

; somar_bytes: ESI = inicio (32 bits), ECX = cantidad. Devuelve la suma en EAX.
; Detecta un kernel o ramdisk corrupto en la lectura antes de saltar a él.
somar_bytes:
        call entrar_unreal
        xor eax, eax
        xor edx, edx
.l:     test ecx, ecx
        jz .fim
        mov dl, [esi]
        add eax, edx
        inc esi
        dec ecx
        jmp .l
.fim:   ret

; ============================================================================
; Vídeo (VBE)
; ============================================================================

; procurar_vesa: recorre la lista de modos y elige, en este orden de
; preferencia, la resolución grabada en la cabecera (CAB_RES_*, D058), 1024x768,
; 800x600, 640x480 y, por último, cualquier otra de al menos 640x480, con
; framebuffer lineal y color directo de 32, 24, 16 (RGB565) o 15 bits (RGB555,
; D078). En la misma resolución 32 > 24 > 16 > 15, salvo que la cabecera pida
; una profundidad (CAB_BPP_PREF, mkimagem.py --bpp): esa gana a las demás.
; Todos los modos aceptables van a la lista VBE_MODOS, que el kernel muestra
; en el programa "pantalla". Si la lista de la BIOS no da ninguno (hay BIOS antiguas
; con la lista vacía o errónea), prueba los números fijos de VESA 1.2 para
; 640x480 (0x112, 0x111, 0x110). Solo registra en boot_info; no cambia de modo.
; Sin preferencias (0) el resultado es el mismo de antes, y las capturas de las
; pruebas siguen en 1024x768x32.
procurar_vesa:
        mov dword [VBE_INFO], 0x32454256        ; 'VBE2': pide la respuesta de VBE 2.0+
        mov di, VBE_INFO
        mov ax, 0x4F00
        int 0x10
        cmp ax, 0x004F
        jne .fim
        cmp dword [VBE_INFO], 0x41534556        ; 'VESA'
        jne .fim
        mov si, [VBE_INFO + 14]                 ; puntero lejano a la lista de modos
        mov ax, [VBE_INFO + 16]
        mov fs, ax
        mov word [melhor_pontos], 0
        mov dword [BOOTINFO + BI_MODOS_END], VBE_MODOS
.laco:
        mov cx, [fs:si]
        add si, 2
        cmp cx, 0xFFFF
        je .reserva
        push si
        push fs
        call avaliar_modo
        pop fs
        pop si
        jmp .laco
.reserva:
        cmp dword [BOOTINFO + BI_VESA_OK], 0
        jne .fim
        mov cx, 0x112                           ; 640x480, 24 bits (VESA 1.2)
        call avaliar_modo
        mov cx, 0x111                           ; 640x480, 16 bits
        call avaliar_modo
        mov cx, 0x110                           ; 640x480, 15 bits
        call avaliar_modo
        ; D331: ningún modo lineal. Si la tarjeta es una S3 conocida (Trio64 o ViRGE,
        ; las BIOS VBE 1.2 de la época), se acepta un modo de 16 bits con bancos y la
        ; ventana lineal del chip se abrirá después del cambio de modo (s3_abrir_janela).
        cmp dword [BOOTINFO + BI_VESA_OK], 0
        jne .fim
        call s3_procurar
        jc .fim
        mov byte [s3_bancos], 1
        mov cx, 0x114                           ; 800x600, 16 bits (VESA 1.2)
        call avaliar_modo
        mov cx, 0x111                           ; 640x480, 16 bits
        call avaliar_modo
        cmp dword [BOOTINFO + BI_VESA_OK], 0
        jne .fim
        mov byte [s3_bancos], 0                 ; ninguno: modo texto, como antes
.fim:
        xor ax, ax
        mov fs, ax
        ret

; avaliar_modo: CX = número del modo. Si es aceptable, va a la lista y,
; si tiene más puntos que el mejor hasta ahora, a boot_info. Los puntos:
; resolución (preferida 40, 1024x768 30, 800x600 20, 640x480 10, otra 0) +
; profundidad (la pedida en la cabecera 5; si no, 32 = 4, 24 = 3, 16 = 2, 15 = 1).
avaliar_modo:
        mov [modo_atual], cx
        mov di, VBE_MODO
        mov ax, 0x4F01
        int 0x10
        cmp ax, 0x004F
        jne .fim
        mov ax, [VBE_MODO]                      ; atributos
        cmp byte [s3_bancos], 0
        jne .atributos_s3
        and ax, 0x0091                          ; soportado + gráfico + lineal
        cmp ax, 0x0091
        jne .fim
        cmp byte [VBE_MODO + 0x1B], 6           ; modelo de memoria: color directo
        jne .fim
        jmp .atributos_ok
.atributos_s3:                                  ; D331: modo con bancos de una S3
        and ax, 0x0019                          ; soportado + color + gráfico
        cmp ax, 0x0019
        jne .fim
        mov al, [VBE_MODO + 0x1B]               ; color directo, o «empaquetado» en
        cmp al, 6                               ; las BIOS que no distinguen
        je .cabe_s3
        cmp al, 4
        jne .fim
.cabe_s3:
        movzx eax, word [VBE_MODO + 0x10]       ; bytes por línea * líneas, dentro de
        movzx edx, word [VBE_MODO + 0x14]       ; la memoria que declara la BIOS
        imul eax, edx
        movzx edx, word [VBE_INFO + 0x12]       ; en bloques de 64 KB
        shl edx, 16
        cmp eax, edx
        ja .fim
.atributos_ok:
        ; Profundidad "de verdad": hay BIOS que dicen 16 bits por píxel en un
        ; modo 5-5-5; el tamaño del verde (desplazamiento 0x21) desempata.
        mov al, [VBE_MODO + 0x19]
        mov bl, 4
        cmp al, 32
        je .bpp_ok
        mov bl, 3
        cmp al, 24
        je .bpp_ok
        mov bl, 1
        cmp al, 15
        je .bpp_ok
        cmp al, 16
        jne .fim
        cmp byte [VBE_MODO + 0x21], 5
        je .bpp15
        mov bl, 2
        jmp .bpp_ok
.bpp15:
        mov al, 15
.bpp_ok:
        mov [profundidade], al
        cmp al, [CAB_ENDERECO + CAB_BPP_PREF]
        jne .guardar
        mov bl, 5                               ; la profundidad pedida gana a las demás
.guardar:
        call guardar_modo
        mov ax, [VBE_MODO + 0x12]               ; anchura
        mov dx, [VBE_MODO + 0x14]               ; altura
        mov bh, 40                              ; la preferida gana a todas
        cmp ax, [CAB_ENDERECO + CAB_RES_LARG]
        jne .r1024
        cmp dx, [CAB_ENDERECO + CAB_RES_ALT]
        je .pontua
.r1024: mov bh, 30
        cmp ax, 1024
        jne .r800
        cmp dx, 768
        je .pontua
.r800:  mov bh, 20
        cmp ax, 800
        jne .r640
        cmp dx, 600
        je .pontua
.r640:  mov bh, 10
        cmp ax, 640
        jne .outra
        cmp dx, 480
        je .pontua
.outra: mov bh, 0                               ; cualquier otra, si no es menor que 640x480
        cmp ax, 640
        jb .fim
        cmp dx, 480
        jb .fim
.pontua:
        add bh, bl
        movzx ax, bh
        cmp ax, [melhor_pontos]
        jbe .fim
        mov [melhor_pontos], ax
        mov dword [BOOTINFO + BI_VESA_OK], 1
        movzx eax, word [modo_atual]
        mov [BOOTINFO + BI_VESA_MODO], eax
        movzx eax, word [VBE_MODO + 0x12]
        mov [BOOTINFO + BI_VESA_LARG], eax
        movzx eax, word [VBE_MODO + 0x14]
        mov [BOOTINFO + BI_VESA_ALT], eax
        movzx eax, word [VBE_MODO + 0x10]
        mov [BOOTINFO + BI_VESA_PITCH], eax
        movzx eax, byte [profundidade]
        mov [BOOTINFO + BI_VESA_BPP], eax
        mov eax, [VBE_MODO + 0x28]
        mov [BOOTINFO + BI_VESA_FB], eax
        mov eax, [VBE_MODO + 0x1F]              ; tam/pos rojo, tam/pos verde
        mov [BOOTINFO + BI_VESA_CORES], eax
        mov ax, [VBE_MODO + 0x23]               ; tam/pos azul
        mov [BOOTINFO + BI_VESA_CORES + 4], ax
        cmp byte [s3_bancos], 0
        je .fim
        mov eax, [s3_bar0]                      ; D331: la ventana lineal estará al
        mov [BOOTINFO + BI_VESA_FB], eax        ; principio del BAR 0
        cmp byte [VBE_MODO + 0x1F], 0           ; sin máscaras (BIOS VBE 1.1): las
        jne .fim                                ; de los modos estándar de 16 bits
        mov dword [BOOTINFO + BI_VESA_CORES], 0x05060B05    ; R 5@11, G 6@5
        mov word [BOOTINFO + BI_VESA_CORES + 4], 0x0005     ; B 5@0
.fim:
        ret

;  ---------------------------------------------------------------------------
; S3 Trio64 / ViRGE con BIOS VBE 1.2 (D331, issue #40)
;
; Estas BIOS solo ofrecen modos con bancos (ventana de 64 KB en A0000), pero el chip
; tiene una ventana lineal propia sobre el BAR 0 que se abre con sus registros:
;   CR38 = 0x48 y CR39 = 0xA5 desbloquean los registros de S3 (CR2D-CR3F, CR40-CRFF);
;   CR59/CR5A = bits 31-24 y 23-16 de la dirección física de la ventana;
;   CR58 bit 4 = ventana lineal encendida, bits 1-0 = tamaño (1, 2 o 4 MB).
; El modo (temporización, DAC, pitch en CR13/CR51) lo pone la BIOS con el INT 10h, como
; siempre: aquí solo se abre la ventana. Nunca se toca otra tarjeta: hace falta el
; fabricante 5333, un identificador de la tabla y un BAR 0 de memoria de 32 bits.
;  ---------------------------------------------------------------------------

; s3_procurar: busca en los buses PCI 0 a 3 (función 0) una S3 de la tabla con clase
; de pantalla y un BAR 0 válido. CF = 0 si la encuentra (s3_bar0 queda con la dirección).
s3_procurar:
        mov dx, 0xCF8                           ; ¿hay mecanismo de configuración n.º 1?
        in eax, dx
        mov ebx, eax
        mov eax, 0x80000000
        out dx, eax
        in eax, dx
        xchg eax, ebx
        out dx, eax                             ; deja CF8 como estaba
        cmp ebx, 0x80000000
        jne .nao
        xor ecx, ecx                            ; bus << 16 | dispositivo << 11
.laco:
        mov eax, ecx
        call pci_ler
        cmp ax, 0x5333
        jne .proximo
        shr eax, 16
        mov si, s3_ids
.id:    mov bx, [si]
        test bx, bx
        jz .proximo
        add si, 2
        cmp ax, bx
        jne .id
        mov eax, ecx
        or al, 0x08                             ; clase 03: pantalla
        call pci_ler
        shr eax, 24
        cmp al, 3
        jne .proximo
        mov eax, ecx
        or al, 0x04                             ; orden: decodificación de memoria
        call pci_ler
        test al, 2
        jz .nao
        mov eax, ecx
        or al, 0x10                             ; BAR 0
        call pci_ler
        test al, 0x07                           ; E/S o de 64 bits: no es lo esperado
        jnz .nao
        test eax, 0x0000FFF0                    ; CR59/CR5A solo guardan los bits 31-16
        jnz .nao
        cmp eax, 0x01000000                     ; sin asignar o dentro de la RAM baja
        jb .nao
        mov [s3_bar0], eax
        clc
        ret
.proximo:
        add ecx, 0x800
        cmp ecx, 0x40000
        jb .laco
.nao:   stc
        ret

; pci_ler: EAX = bus/dispositivo/registro (sin el bit 31). Devuelve el valor en EAX.
pci_ler:
        push dx
        or eax, 0x80000000
        mov dx, 0xCF8
        out dx, eax
        mov dx, 0xCFC
        in eax, dx
        pop dx
        ret

; s3_cr_ler: AL = índice del CRTC. Devuelve el valor en AL (DX = 0x3D4 al salir).
s3_cr_ler:
        mov dx, 0x3D4
        out dx, al
        inc dx
        in al, dx
        dec dx
        ret

; s3_abrir_janela: tras el INT 10h del modo con bancos, abre la ventana lineal en el
; BAR 0 y comprueba que la memoria responde allí (escribe y relee dos patrones, por
; «unreal mode»). Si no, deja los registros como estaban, vuelve al modo texto y
; devuelve CF = 1 (el kernel sigue en texto, como antes).
s3_abrir_janela:
        mov al, 0x38
        call s3_cr_ler
        mov [s3_cr38], al
        mov al, 0x39
        call s3_cr_ler
        mov [s3_cr39], al
        mov ax, 0x4838                          ; CR38 = 0x48
        out dx, ax
        mov ax, 0xA539                          ; CR39 = 0xA5
        out dx, ax
        mov al, 0x58
        call s3_cr_ler
        mov [s3_cr58], al
        mov al, 0x59
        call s3_cr_ler
        mov [s3_cr59], al
        mov al, 0x5A
        call s3_cr_ler
        mov [s3_cr5a], al
        mov ebx, [s3_bar0]
        shr ebx, 16                             ; BH = bits 31-24, BL = bits 23-16
        mov al, 0x59
        mov ah, bh
        out dx, ax
        mov al, 0x5A
        mov ah, bl
        out dx, ax
        mov bl, 3                               ; tamaño de la ventana según la memoria
        cmp word [VBE_INFO + 0x12], 64          ; que declara la BIOS (bloques de 64 KB)
        jae .tamanho
        mov bl, 2
        cmp word [VBE_INFO + 0x12], 32
        jae .tamanho
        mov bl, 1
.tamanho:
        mov ah, [s3_cr58]
        and ah, 0xEC                            ; fuera el tamaño y el bit de encendido
        or ah, bl
        or ah, 0x10                             ; ventana lineal encendida
        mov al, 0x58
        out dx, ax
        mov al, 0x58                            ; ¿lo aceptó?
        call s3_cr_ler
        and al, 0x13
        or bl, 0x10
        cmp al, bl
        jne .falha
        call entrar_unreal                      ; DS con límite de 4 GB
        mov esi, [s3_bar0]
        mov ebx, [esi]
        mov dword [esi], 0x5AA5C33C
        cmp dword [esi], 0x5AA5C33C
        jne .falha_mem
        mov dword [esi], 0xA55A3CC3
        cmp dword [esi], 0xA55A3CC3
        jne .falha_mem
        mov [esi], ebx
        call s3_fechar
        clc
        ret
.falha_mem:
        mov [esi], ebx
.falha:
        mov dx, 0x3D4
        mov al, 0x58
        mov ah, [s3_cr58]
        out dx, ax
        mov al, 0x59
        mov ah, [s3_cr59]
        out dx, ax
        mov al, 0x5A
        mov ah, [s3_cr5a]
        out dx, ax
        call s3_fechar
        mov ax, 0x0003                          ; vuelta al modo texto
        int 0x10
        mov byte [s3_bancos], 0
        stc
        ret

; s3_fechar: vuelve a dejar CR39 y CR38 como estaban (bloqueados, normalmente).
s3_fechar:
        mov dx, 0x3D4
        mov al, 0x39
        mov ah, [s3_cr39]
        out dx, ax
        mov al, 0x38
        mov ah, [s3_cr38]
        out dx, ax
        ret

; S3 que llevan la ventana lineal en CR58-CR5A (la tabla de kernel/dispositivos/acel_s3.c).
s3_ids:         dw 0x8811, 0x8812, 0x8814, 0x8901                   ; Trio64, Aurora64V+, Trio64UV+, Trio64V2
                dw 0x5631, 0x883D, 0x8A01, 0x8A10, 0x8904, 0x8A13   ; ViRGE, VX, DX/GX, GX2, Trio3D, Trio3D/2X
                dw 0

; guardar_modo: añade el modo de VBE_MODO a la lista VBE_MODOS (si cabe).
; Entrada de 8 bytes: número de modo, anchura, altura, profundidad (15, 16,
; 24 o 32), 0.
guardar_modo:
        mov edi, [BOOTINFO + BI_MODOS_QTD]
        cmp edi, MODOS_MAX
        jae .fim
        shl di, 3
        add di, VBE_MODOS
        mov ax, [modo_atual]
        mov [di], ax
        mov ax, [VBE_MODO + 0x12]
        mov [di + 2], ax
        mov ax, [VBE_MODO + 0x14]
        mov [di + 4], ax
        mov al, [profundidade]
        mov [di + 6], al
        mov byte [di + 7], 0
        inc dword [BOOTINFO + BI_MODOS_QTD]
.fim:
        ret

; ============================================================================
; Texto por la BIOS
; ============================================================================

;-----------------------------------------------------------------------------
; olhar_m: v8.5.4.2 (D379). Si hay una M en el búfer del teclado (y el menú no ha salido
; ya), el menú del modo de la raíz. No espera nada si no la hay.
olhar_m:
        cmp byte [menu_raiz_visto], 0
        jne .fim
        push ax
        mov ah, 0x01
        int 0x16
        jz .nada
        and al, 0xDF
        cmp al, 'M'
        jne .nada
        mov byte [menu_raiz_visto], 1
        call menu_raiz
.nada:
        pop ax
.fim:
        ret

menu_raiz_visto: db 0

;-----------------------------------------------------------------------------
; menu_raiz: v8.5.4.2 (D379). Vacía el búfer (la M repetida), enseña las opciones
; y espera un número; Intro o Esc = lo de siempre. Pone RAIZ_OPC_* << 17 en BI_OPCOES.
menu_raiz:
        push eax
        push bx
        push si
.vaciar:
        mov ah, 0x01
        int 0x16
        jz .pedir
        mov ah, 0x00
        int 0x16
        jmp .vaciar
.pedir:
        mov si, msg_menu_raiz
        call escrever
.ler:
        mov ah, 0x00
        int 0x16
        cmp al, 13
        je .nada
        cmp al, 27
        je .nada
        cmp al, '1'
        jb .ler
        cmp al, '4'
        ja .ler
        call escrever_char
        movzx bx, al
        sub bx, '1'
        movzx eax, byte [tabela_menu_raiz + bx]
        shl eax, 17
        or dword [BOOTINFO + BI_OPCOES], eax
        mov si, msg_crlf
        call escrever
        jmp .fim
.nada:
        mov si, msg_menu_nada
        call escrever
.fim:
        pop si
        pop bx
        pop eax
        ret

; 1 live (RAIZ_OPC_LIVE), 2 todo a la RAM (TORAM), 3 instalado (DISCO), 4 rescate (RAM: solo el initramfs)
tabela_menu_raiz: db 3, 4, 2, 1

escrever:
        push ax
.l:     lodsb
        test al, al
        jz .fim
        call escrever_char
        jmp .l
.fim:   pop ax
        ret

escrever_char:
        push ax
        push bx
        mov ah, 0x0E
        mov bx, 0x0007
        int 0x10
        pop bx
        pop ax
        ret

; escribir_decimal: EAX sin signo, en base 10.
escrever_decimal:
        push eax
        push ecx
        push edx
        push ebx
        mov ebx, 10
        xor ecx, ecx
.div:   xor edx, edx
        div ebx
        push dx
        inc cx
        test eax, eax
        jnz .div
.mostra:
        pop ax
        add al, '0'
        call escrever_char
        loop .mostra
        pop ebx
        pop edx
        pop ecx
        pop eax
        ret

parar:
        call escrever
.l:     hlt
        jmp .l

; ============================================================================
; Modo protegido (32 bits)
; ============================================================================

bits 32
modo_protegido:
        mov ax, SEL_DADOS
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov esp, 0x7C00                 ; pila provisional; el kernel monta la suya
        mov eax, BOOT_MAGICO
        mov ebx, BOOTINFO
        jmp [CAB_ENDERECO + CAB_K_ENTRADA]

; ============================================================================
; Dados
; ============================================================================

disco:          db 0
spt:            db 0                    ; 0 = lectura por LBA (AH=42h)
cabecas:        db 0
tem_ext:        db 0                    ; D330: la BIOS anuncia las extensiones (AH=41h)
spt_bios:       db 0                    ; D330: geometría de AH=08h (0 = no la dio)
cab_bios:       db 0
pacote_cd:      times 0x14 db 0         ; respuesta del INT 13h AX=4B01h (El Torito)
melhor_pontos:  dw 0
modo_atual:     dw 0
blocos_lidos:   dw 0
profundidade:   db 0
s3_bancos:      db 0                    ; D331: modo con bancos de una S3 + ventana lineal
s3_cr38:        db 0
s3_cr39:        db 0
s3_cr58:        db 0
s3_cr59:        db 0
s3_cr5a:        db 0
align 4
s3_bar0:        dd 0

align 4
dap:            db 0x10, 0
dap_qtd:        dw 0
dap_destino:    dw 0, 0
dap_lba:        dd 0, 0

; GDT mínima del cargador: código y datos de 32 bits, base 0 y límite 4 GB.
; El kernel la cambia por una GDT propia en cuanto arranca.
align 8
gdt:
        dq 0                            ; descriptor nulo (obligatorio)
        dw 0xFFFF, 0x0000               ; 0x08 código: límite 0-15, base 0-15
        db 0x00, 0x9A, 0xCF, 0x00       ;      base 16-23, presente/anillo 0/código, 4 KB + 32 bits, base 24-31
        dw 0xFFFF, 0x0000               ; 0x10 datos
        db 0x00, 0x92, 0xCF, 0x00
gdt_fim:
gdt_descritor:
        dw gdt_fim - gdt - 1
        dd gdt

msg_titulo:     db "SaguiOS - etapa 2 del cargador", 13, 10, 0
msg_rescate:    db "Ctrl pulsado: MODO RESCATE", 13, 10, 0
msg_acel_segura: db "Mayus pulsado: aceleracion en modo seguro", 13, 10, 0
msg_menu_raiz:  db "M pulsada: modo de arranque", 13, 10
                db "  1 live desde el medio (los cambios, en la RAM)", 13, 10
                db "  2 todo a la RAM (despues se puede sacar el medio)", 13, 10
                db "  3 instalado (la raiz del disco, con escritura)", 13, 10
                db "  4 rescate (solo el sistema minimo)", 13, 10
                db "  Intro: el de siempre.  Elige: ", 0
msg_menu_nada:  db "el de siempre", 13, 10, 0
msg_a20:        db "[1/6] A20 ok", 13, 10, 0
msg_mem:        db "[2/6] memoria ok: ", 0
msg_mem2:       db " regiones, ", 0
msg_mb:         db " MB libres", 13, 10, 0
msg_kernel:     db "[3/6] kernel ", 0
msg_ramdisk:    db "[4/6] ramdisk ", 0
msg_ok:         db " ok", 13, 10, 0
msg_video:      db "[5/6] video: ", 0
msg_sem_vesa:   db "VESA no encontrado, sigue en modo texto", 13, 10, 0
msg_crlf:       db 13, 10, 0
msg_s3_janela:  db " (S3: bancos + ventana lineal)", 0
msg_pm:         db "[6/6] entrando en modo protegido", 13, 10, 0
; D310. La etapa 2 escribe con la fuente de la BIOS (página de códigos 437): 0xA2 es la «ó».
msg_cpu_sem_cmov:  db "[0/6] CPU: Esta CPU no es i686 (falta la instrucci", 0xA2, "n CMOV): ", 0
msg_cpu_sem_cpuid: db "[0/6] CPU: Esta CPU no es i686 (no tiene CPUID, y por tanto tampoco CMOV): ", 0
msg_cpu_requisito: db "SaguiOS necesita un Pentium Pro / Pentium II / AMD Athlon o posterior. "
                   db "Las AMD K6, K6-2, K6-III y los Pentium/Pentium MMX no son compatibles.", 13, 10
                   db "La maquina queda detenida; ya se puede apagar.", 13, 10, 0
msg_falha_a20:  db "ERROR: no se pudo activar la linea A20", 0
msg_falha_mem:  db "ERROR: la BIOS no dio el mapa de memoria (E820)", 0
msg_falha_leitura: db 13, 10, "ERROR: fallo de lectura del disco (INT 13h)", 0
msg_lba_ruim:   db 13, 10, "Aviso: la BIOS lee mal el disco por LBA; se lee por CHS (D330)", 13, 10, 0
msg_falha_soma: db 13, 10, "ERROR: datos leidos corruptos (la suma no coincide)", 0
msg_aviso_ram:  db 13, 10, "AVISO: el ramdisk no cabe en la RAM que da la BIOS; hacen falta ", 0
msg_aviso_ram2: db " MB", 13, 10, 0
