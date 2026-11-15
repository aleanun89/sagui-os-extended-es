; SPDX-License-Identifier: GPL-3.0-only
; bootinfo.inc - estructura que la etapa 2 entrega al kernel.
;
; Está en 0x5000 (memoria baja libre). El kernel recibe la dirección en EBX y la
; firma BOOT_MAGICO en EAX, copia todo dentro de él al principio y
; ya no depende de esta área. La misma estructura está en kernel/inc/bootinfo.h.

BOOTINFO            equ 0x5000
MAPA_E820           equ 0x5100     ; hasta 64 entradas de 24 bytes (hasta 0x5700)
MAPA_MAX            equ 64
VBE_INFO            equ 0x6000     ; 512 bytes (bloque de información de VBE)
VBE_MODO            equ 0x6200     ; 256 bytes (información de un modo)
VBE_MODOS           equ 0x5700     ; lista de modos aceptables (D058): hasta 128 de 8 bytes (hasta 0x5B00)
MODOS_MAX           equ 128        ; con 15 y 16 bits (D078) QEMU supera los 64 modos
BOOTINFO_TAM        equ 192

BOOT_MAGICO         equ 0x53414755 ; 'UGAS' - "me cargó la etapa 2 de SaguiOS"

BI_ASSINATURA       equ 0          ; 'SGBI'
BI_VERSAO           equ 4
BI_DISCO            equ 8          ; DL recibido de la BIOS
BI_MAPA_QTD         equ 12
BI_MAPA_END         equ 16
BI_K_INICIO         equ 20
BI_K_BYTES          equ 24
BI_R_INICIO         equ 28
BI_R_BYTES          equ 32
BI_A20              equ 36         ; 0 ya activa, 1 BIOS, 2 puerto 0x92, 3 controlador de teclado
BI_VESA_OK          equ 40         ; 1 si encontró un modo aceptable
BI_VESA_MODO        equ 44
BI_VESA_LARG        equ 48
BI_VESA_ALT         equ 52
BI_VESA_PITCH       equ 56
BI_VESA_BPP         equ 60
BI_VESA_FB          equ 64         ; dirección física del framebuffer lineal
BI_VESA_ATIVO       equ 68         ; 1 si la etapa 2 cambió de modo
BI_VESA_CORES       equ 72         ; 6 bytes: tam/pos del rojo, verde, azul
BI_SETORES_LIDOS    equ 80
BI_OPCOES           equ 148        ; u32 copia de CAB_OPCOES
BI_TESTE            equ 84         ; char[64] copiado de CAB_TESTE (hasta el byte 148)
BI_MODOS_QTD        equ 152        ; modos VBE de 15/16/24/32 bits con framebuffer lineal (D058, D078)
BI_MODOS_END        equ 156        ; dirección de la lista (VBE_MODOS): u16 modo, u16 anchura, u16 altura, u8 profundidad, u8 0
BI_RES_PREF         equ 160        ; u32 anchura | altura << 16 copiado de CAB_RES_* (0 = automática)
BI_BPP_PREF         equ 164        ; u32 profundidad preferida copiada de CAB_BPP_PREF (D078)
BI_CHS              equ 168        ; u32 1 = la BIOS no tiene las extensiones del INT 13h: lectura por CHS (D077)
; D264 (versión 2): los rellena el cargador UEFI (boot/uefi/cargador.c); la etapa 2 los deja a cero.
BI_FIRMWARE         equ 172        ; u32 0 BIOS, 1 UEFI x64, 2 UEFI IA32
BI_CAB_END          equ 176        ; u32 dirección de la copia de la cabecera (0 = CAB_ENDERECO)
BI_RSDP             equ 180        ; u32 RSDP de ACPI (0 = buscarlo en la memoria de la BIOS)
BI_SMBIOS           equ 184        ; u32 punto de entrada SMBIOS (0 = buscarlo)
