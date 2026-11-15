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
CAB_MANDOS_PRUEBA   equ 156        ; u8 mandos de prueba (D307; lo pone mkimagem.py --mandos-prueba): no 144, que es CAB_ID_INSTALACAO
                                   ; cada disco instalado; mkimagem.py lo deja a cero. El kernel compara los 256
                                   ; primeros bytes de la cabecera para saber de qué disco arrancó
