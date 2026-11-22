; SPDX-License-Identifier: GPL-3.0-only
; cabecalho.inc - formato de la cabecera de arranque de SaguiOS (LBA 1 de la imagen).
;
; El MBR y la etapa 2 leen esta cabecera para saber dónde está cada parte del
; sistema. La rellena tools/mkimagem.py en el build, así que ningún LBA queda
; fijo en el código salvo el propio LBA 1. Los mismos desplazamientos están repetidos en el
; script Python (clase Cabecalho); si cambia aquí, cambie allí.

CAB_LBA             equ 1          ; único LBA fijo del proyecto
CAB_ENDERECO        equ 0x7E00     ; donde el MBR deja la cabecera en memoria
CAB_ASSINATURA_VAL  equ 0x44484753 ; 'SGHD' en little-endian

CAB_ASSINATURA      equ 0          ; u32 'SGHD'
CAB_VERSAO          equ 4          ; u16
CAB_OPCOES          equ 6          ; u16, bit 0 = cambiar al modo VESA
CAB_E2_LBA          equ 8          ; u32 LBA de la etapa 2
CAB_E2_SETORES      equ 12         ; u32
CAB_K_LBA           equ 16         ; u32 LBA del kernel
CAB_K_SETORES       equ 20         ; u32
CAB_K_BYTES         equ 24         ; u32 tamaño exacto de kernel.bin
CAB_K_ENDERECO      equ 28         ; u32 dirección física de carga (1 MB)
CAB_R_LBA           equ 32         ; u32 LBA del ramdisk
CAB_R_SETORES       equ 36         ; u32
CAB_R_BYTES         equ 40         ; u32
CAB_R_ENDERECO      equ 44         ; u32 dirección física del ramdisk (después del fin del kernel, BSS incluida)
CAB_IMG_SETORES     equ 48         ; u32 tamaño total útil
CAB_K_ENTRADA       equ 52         ; u32 dirección de entrada del kernel

OPC_VESA            equ 1
OPC_CONSOLE         equ 2      ; sin escritorio: terminal a pantalla completa (pruebas)
OPC_SEM_WC          equ 4      ; no activar write-combining en el framebuffer (D057)
OPC_CD              equ 8      ; el disco de arranque es un CD (solo lectura, D077); la etapa 2 también activa
                               ; este bit en boot_info si la BIOS dice que la emulación es de El Torito
OPC_SEM_ACPI        equ 16     ; el kernel no usa el ACPI: sin modo ACPI ni botón de encendido (D113)
OPC_SMP             equ 64     ; encender los procesadores secundarios (D164)
OPC_IOAPIC          equ 128    ; las IRQ de los dispositivos por el IOAPIC en vez del 8259 (D166)
OPC_TURNOS          equ 256    ; el cerrojo general por turnos también bajo un hipervisor (D169)
OPC_UACPI           equ 0x1000 ; arrancar el intérprete de AML (uACPI, D192; sin él, apagado de siempre). La etapa 2 no lo mira
OPC_SEM_SERVICOS    equ 0x2000 ; no lanzar el gestor de servicios (D216). La etapa 2 no lo mira
OPC_RESCATE         equ 0x4000 ; modo rescate (D255): la etapa 2 lo pone también si Ctrl está pulsado
OPC_ACEL_SEGURA      equ 0x10000 ; modo seguro de la aceleración (D323): Mayús al arrancar; solo en boot_info (u32)
OPC_SESION_RAIZ     equ 0x8000 ; imagen de prueba: la sesión como root (D255). La etapa 2 no lo mira
OPC_PLANIFICADOR    equ 0xE00  ; bits 9-11: el planificador al arrancar (D174); 0 = el de por defecto.
                               ; La etapa 2 no los mira: los copia con el resto de CAB_OPCOES
OPC_SGL_SOFTWARE    equ 32     ; solo el motor por software: la GPU no acelera nada (SGL_MOTOR=software, D158)
                               ; este bit no cabe en CAB_OPCOES (u16): va en CAB_MOTOR y la etapa 2
                               ; lo pone en boot_info.opcoes, que sí es u32

; Direcciones de trabajo en modo real (todas por debajo de 0x7C00 o entre la
; etapa 2 y 0x9F000, lejos de la EBDA).
ESTAGIO2_ENDERECO   equ 0x8000     ; hasta 0xFFFF (32 KB como máximo)
ESTAGIO2_MAX_SET    equ 64

CAB_K_SOMA          equ 56         ; u32 suma simple de los bytes del kernel (detecta lectura corrompida)
CAB_R_SOMA          equ 60         ; u32 suma simple de los bytes del ramdisk
CAB_TESTE           equ 64         ; char[64] autoprueba del kernel, o programa inicial con argumentos (vacío = arranque normal)
CAB_RES_LARG        equ 128        ; u16 resolución preferida (D058); 0 = automática (1024x768, 800x600, 640x480)
CAB_RES_ALT         equ 130        ; u16 (el programa "pantalla --guardar" vuelve a grabar estos 4 bytes en el disco)
CAB_BPP_PREF        equ 132        ; u8 profundidad preferida (D078): 15, 16, 24 o 32; 0 = automática
CAB_FREQ            equ 133        ; u8 driver de frecuencia (D106): bit 0 = desactivado, bit 1 = simulado (pruebas)
CAB_MOTOR           equ 134        ; u8 motor 3D (D158): 0 = automático, 1 = solo software (SGL_MOTOR=software)
CAB_ACEL2D          equ 135        ; u8 2D por la GPU (D195): bits 0-2 = 0 automático, 1 CPU (--sin-acel2d),
                                   ; 2 vmware, 3 intel, 4 radeon (--acel2d=...); bit 4 y 5 = fallos simulados
                                   ; de la autoprueba (pruebas). Solo lo lee el kernel
CAB_SIS_SETORES     equ 136        ; u32 sectores de las particiones del sistema (D181: la de juegos), para «Acerca de»
CAB_IRQ_PRUEBA      equ 140        ; u8 fallos simulados de la autoprueba de las IRQ (D211, pruebas): bit 0 = GSI
                                   ; del _PRT equivocada, bit 1 = la entrada ISA no entrega. Solo lo lee el kernel
CAB_USB_PRUEBA      equ 141        ; u8 fallos simulados de la enumeración USB (D227, pruebas): bit 0 = el primer
                                   ; intento de cada conexión falla, bit 1 = fallan todos. Solo lo lee el kernel
CAB_DISCO_OPC       equ 143        ; u8 opciones del disco (D281, pruebas): bit 0 sin DMA IDE, bit 1 sin caché de
                                   ; bloques, bit 2 sin escritura diferida. Solo lo lee el kernel
CAB_ID_INSTALACAO   equ 144        ; u32 "SGIN", u32 aleatorio, u32 hora (144-155): lo escribe "instalar" (D179) en
CAB_NE2K_OPC        equ 157        ; u8 D329: bit 0 = sondear NE2000 ISA (mkimagem.py --ne2k-isa); apagado por defecto. Solo lo lee el kernel
CAB_NE2K_IRQ        equ 158        ; u8 D329: IRQ de la NE2000 ISA (mkimagem.py --ne2k-irq); 0 = autodetectarla
CAB_MEM_OPC         equ 159        ; u8 D349: bit 0 = sin PAE (mkimagem.py --sem-pae), bit 1 = sin NX (--sem-nx), bit 2 = sin swap (--sin-swap, D352). Solo lo lee el kernel
CAB_RAIZ_OPC        equ 163        ; u8 D379: modo de la raíz (0 auto/raiz.conf, 1 ram/rescate, 2 disco, 3 live, 4 toram). Solo lo lee el kernel
CAB_TROCA_MB        equ 160        ; u16 D352: tope de la swap comprimida en MB (mkimagem.py --swap-mb); 0 = automático. Solo lo lee el kernel
CAB_KCONF           equ 192        ; 64 bytes D353 (192-255): lo que guarda «kconfig» desde el sistema (firma 'SGKC', bits
                                   ; de presencia, u16 por opción y suma; kernel/inc/kconf.h). mkimagem.py la deja a cero (los
                                   ; valores de la imagen son los bits de arriba). Solo la lee el kernel (en el modo rescate, no la aplica)
CAB_MANDOS_PRUEBA   equ 156        ; u8 mandos de prueba (D307; lo pone mkimagem.py --mandos-prueba): no 144, que es CAB_ID_INSTALACAO
                                   ; cada disco instalado; mkimagem.py lo deja a cero. El kernel compara los 256
                                   ; primeros bytes de la cabecera para saber de qué disco arrancó

; D392: el kernel de prueba que se arranca UNA vez («arranque probar»). Va en la segunda mitad de la
; cabecera (bytes 256-511), que el kernel no compara para reconocer su disco (solo los 256 primeros):
; prepararlo o gastarlo no cambia qué disco es el de arranque. mkimagem.py la deja a cero.
CAB_PR              equ 256        ; u32 'SGPR': hay una ranura descrita
CAB_PR_ASSINATURA_VAL equ 0x52504753
CAB_PR_ESTADO       equ 260        ; u8, bits PR_* (fuera de la suma: la etapa 2 lo cambia)
CAB_PR_NK           equ 261        ; u8 tramos del kernel (1-4)
CAB_PR_NR           equ 262        ; u8 tramos del ramdisk (0-4; 0 = el ramdisk normal, cargado tras este kernel)
CAB_PR_ONDE         equ 263        ; u8 0 = en la partición de SaguiOS, 1 = fichero de la ESP (solo informa)
CAB_PR_K_BYTES      equ 264        ; u32
CAB_PR_K_SOMA       equ 268        ; u32 la misma suma simple que CAB_K_SOMA
CAB_PR_K_ENTRADA    equ 272        ; u32
CAB_PR_R_ENDERECO   equ 276        ; u32 dónde va el ramdisk con este kernel (tras su BSS, alineado a 4 KB)
CAB_PR_R_BYTES      equ 280        ; u32 (con NR = 0, no se usa)
CAB_PR_R_SOMA       equ 284        ; u32
CAB_PR_TRAMOS       equ 288        ; 8 x (u32 LBA, u32 sectores): 4 del kernel y 4 del ramdisk (hasta 351)
CAB_PR_SOMA         equ 352        ; u32 suma de los bytes 261-351 (la ranura no es basura)
CAB_PR_HORA         equ 356        ; u32 cuándo se preparó (segundos desde 1970; solo informa)
CAB_PR_NOTA         equ 360        ; char[32] el nombre del fichero (solo informa)
PR_PENDENTE         equ 1          ; arrancar la prueba en el siguiente arranque, UNA vez (la etapa 2 lo borra antes)
PR_ARRANCADA        equ 2          ; la etapa 2 la arrancó (lo pone al borrar PR_PENDENTE)
PR_FIXA             equ 4          ; se arranca siempre: en la ranura del último bueno (392); en la de prueba
                                   ; solo la dejaba el primer «arranque confirmar» (ESP), y se sigue respetando
; D392 (último bueno): el kernel confirmado va en una segunda ranura, bytes 392-511, con la MISMA
; disposición relativa que la de prueba hasta la nota (firma, estado, tramos, sumas; la nota, de 16
; bytes). La etapa 2 la copia en memoria encima de la de prueba (bytes 256-375) y la carga igual.
; Orden: prueba PR_PENDENTE (una vez) > prueba PR_FIXA (formato de antes) > último bueno PR_FIXA >
; el kernel de la imagen (CAB_K_*, que «arranque» ya no cambia nunca).
CAB_PB              equ 392        ; u32 'SGPR': hay un último bueno
CAB_PB_ESTADO       equ 396        ; u8 PR_FIXA
CAB_PB_COPIA        equ 120        ; bytes 392-511 -> 256-375 (la nota, cortada a 16)
CAB_PB_NOTA         equ 496        ; char[16]
OPC_KERNEL_PRUEBA   equ 0x100000   ; en boot_info (u32): este arranque es el kernel de una ranura (prueba o último bueno)
OPC_KERNEL_FIXO     equ 0x200000   ; y además fijado (PR_FIXA: el último bueno)
OPC_UEFI_RANURA     equ 0x400000   ; D439: lo pone el cargador UEFI que sabe de las ranuras (boot/uefi/cargador.c)
E2_CAPACIDADES      equ 3          ; marca SGE2 de la etapa 2: bit 0 ranura de prueba, bit 1 último bueno
