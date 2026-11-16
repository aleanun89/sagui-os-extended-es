// SPDX-License-Identifier: GPL-3.0-only
/* bootinfo.h - lo que la etapa 2 entrega al kernel (espejo de boot/bootinfo.inc). */
#ifndef SAGUI_BOOTINFO_H
#define SAGUI_BOOTINFO_H
#include "tipos.h"

#define BOOT_MAGICO      0x53414755u
#define BOOTINFO_ASSIN   0x49424753u  /* 'SGBI' */
#define MAPA_MAX         64
#define E820_LIVRE       1

typedef struct EMPACOTADO {
    uint64_t base;
    uint64_t tamanho;
    uint32_t tipo;      /* 1 = RAM libre; los demás están reservados para el firmware/ACPI */
    uint32_t extra;     /* atributo extendido ACPI 3.0: bit 0 = entrada válida */
} regiao_e820_t;

typedef struct EMPACOTADO {
    uint32_t assinatura;
    uint32_t versao;
    uint32_t disco;
    uint32_t mapa_qtd;
    uint32_t mapa_end;
    uint32_t kernel_inicio;
    uint32_t kernel_bytes;
    uint32_t ramdisk_inicio;
    uint32_t ramdisk_bytes;
    uint32_t a20_metodo;
    uint32_t vesa_ok;
    uint32_t vesa_modo;
    uint32_t vesa_largura;
    uint32_t vesa_altura;
    uint32_t vesa_pitch;
    uint32_t vesa_bpp;
    uint32_t vesa_fb;
    uint32_t vesa_ativo;
    uint8_t  verm_tam, verm_pos, verde_tam, verde_pos, azul_tam, azul_pos;
    uint8_t  reservado[2];
    uint32_t setores_lidos;
    char     teste[64];   /* autoprueba pedida por el build de prueba; vacío en el uso normal */
    uint32_t opcoes;      /* bit 0 VESA, bit 1 terminal de pantalla completa sin área de trabajo, bit 2 sin WC, bit 3 CD */
    uint32_t modos_qtd;   /* modos VBE aceptables (D058), lista en modos_end */
    uint32_t modos_end;
    uint32_t res_pref;    /* resolución preferida de la cabecera: anchura | altura << 16 (0 = automática) */
    uint32_t bpp_pref;    /* profundidad preferida de la cabecera (D078): 15, 16, 24, 32; 0 = automática */
    uint32_t leitura_chs; /* 1 = la BIOS no tenía las extensiones del INT 13h y el cargador leyó por CHS (D077) */
    /* D264: versión 2 (cargador UEFI). La etapa 2 pone toda la boot_info a cero: 0 = como siempre. */
    uint32_t firmware;    /* FIRMWARE_BIOS, FIRMWARE_UEFI_X64 o FIRMWARE_UEFI_IA32 */
    uint32_t cab_end;     /* copia de la cabecera de arranque (0 = 0x7E00, donde la deja el MBR) */
    uint32_t rsdp;        /* RSDP de ACPI dado por el firmware (0 = buscarlo en la EBDA y 0xE0000) */
    uint32_t smbios;      /* punto de entrada "_SM_" o "_SM3_" dado por el firmware (0 = buscarlo en 0xF0000) */
    uint32_t reservado2;
} info_boot_t;

#define FIRMWARE_BIOS      0
#define FIRMWARE_UEFI_X64  1
#define FIRMWARE_UEFI_IA32 2

/* La cabecera de arranque que dejó el cargador: 0x7E00 por BIOS, la zona del cargador UEFI. */
#define CABECALHO_BOOT_END(bi) ((bi)->cab_end ? (bi)->cab_end : 0x7E00u)

#define OPC_BOOT_CONSOLE 2
#define OPC_BOOT_SEM_WC  4   /* D057: no activar write-combining (mkimagem.py --sem-wc) */
#define OPC_BOOT_CD      8   /* D077: arrancó de un CD (el disco de arranque es solo lectura) */
#define OPC_BOOT_SEM_ACPI 16  /* D113: no usar el ACPI (mkimagem.py --sem-acpi): sin modo ACPI, sin botón, apagar solo avisa */
#define OPC_BOOT_SGL_SOFTWARE 32  /* D158: solo el motor por software (SGL_MOTOR=software): no se acelera con la GPU */
#define OPC_BOOT_SMP     64  /* D164: encender los procesadores secundarios (mkimagem.py --smp); sin él, uno solo */
#define OPC_BOOT_IOAPIC 128  /* D166: las IRQ por el IOAPIC (mkimagem.py --ioapic); sin él, el PIC 8259 */
#define OPC_BOOT_TURNOS 256  /* D169: el cerrojo general por turnos también bajo un hipervisor (mkimagem.py --trava-turnos) */
#define OPC_BOOT_UACPI  0x1000  /* D192: arrancar el intérprete de AML y la batería (mkimagem.py --uacpi); sin él, ni AML ni batería */
#define OPC_BOOT_SEM_SERVICOS 0x2000  /* D216: no lanzar el gestor de servicios (mkimagem.py --sin-servicios) */
#define OPC_BOOT_RESCATE  0x4000  /* D255: modo rescate: consola con un shell de root, sin servicios (Ctrl al arrancar, o mkimagem.py --rescate) */
#define OPC_BOOT_ACEL_SEGURO 0x10000  /* D323: modo seguro de la aceleración (Mayús al arrancar): 2D por la CPU, 3D por software y sin aplicar la configuración de aceleración. No cabe en CAB_OPCOES (u16): la etapa 2 lo pone en boot_info; con la cabecera, el bit CAB_ACEL_SEGURO de CAB_ACEL2D */
#define CAB_ACEL_SEGURO 0x80  /* D323: bit de CAB_ACEL2D = lo mismo que Mayús al arrancar (mkimagem.py --acel-segura, y para las pruebas) */
#define OPC_BOOT_SESION_RAIZ 0x8000  /* D255: imagen de prueba: el escritorio y el Terminal como root (mkimagem.py --sesion-root) */
/* D174: el planificador elegido al arrancar (mkimagem.py --planificador), en los
 * bits 9-11: 0 = el de por defecto, o PLANIFICADOR_* de abi.h. */
#define OPC_BOOT_PLANIFICADOR      0xE00u
#define OPC_BOOT_PLANIFICADOR_DESL 9

/* Un modo de la lista del VBE (boot/bootinfo.inc, VBE_MODOS). */
#define MODOS_MAX 128   /* boot/bootinfo.inc; con 15 y 16 bits (D078) el QEMU pasa de 64 */
typedef struct EMPACOTADO {
    uint16_t modo;
    uint16_t largura, altura;
    uint8_t  bpp, reservado;   /* bpp = profundidad: 15 (RGB555), 16 (RGB565), 24 o 32 (D078) */
} modo_vbe_t;
extern modo_vbe_t modos_vbe[MODOS_MAX];

/* La cabecera de arranque (LBA 1) que dejó el MBR en 0x7E00: el kernel guarda
 * una copia al inicio para comprobar el disco antes de regrabar la resolución
 * preferida (D058). Desplazamientos en boot/cabecalho.inc. */
#define CAB_K_SOMA   56
#define CAB_R_SOMA   60
#define CAB_RES_LARG 128
#define CAB_RES_ALT  130
#define CAB_BPP_PREF 132
#define CAB_MOTOR    134   /* u8 motor 3D (D158): 0 = automático, 1 = solo software */
#define CAB_ACEL2D   135   /* u8 2D por la GPU (D195): ACEL2D_* de acel2d.h; 0 = automático */
#define CAB_IRQ_PRUEBA 140 /* u8 fallos simulados de la autoprueba de las IRQ (D211, IRQ_PRUEBA_*); 0 = ninguno */
#define CAB_USB_PRUEBA 141 /* u8 fallos simulados de la enumeración USB (D227, USB_PRUEBA_* de usb.h); 0 = ninguno */
#define CAB_GPU3D    142   /* u8 3D por la GPU (D285): GPU3D_CAB_* de gpu3d.h; 0 = automático */
#define CAB_DISCO_OPC  143 /* u8 D281 (mkimagem.py): bit 0 --sem-dma (todo por PIO), bit 1 --sem-cache-disco, bit 2 --sin-escritura-diferida, bit 3 --cache-disco-baja (D350) */
#define CAB_DISCO_SEM_DMA     1
#define CAB_DISCO_SEM_CACHE   2
#define CAB_DISCO_SEM_DIFERIDA 4
#define CAB_DISCO_CACHE_BAIXA  8   /* D350: --cache-disco-baja: la caché en la baja aunque haya alta */
#define CAB_MANDOS_PRUEBA 156 /* u8 D307 (no 144: 144-155 es CAB_ID_INSTALACAO, D179): bit 0 = mandos de prueba y MANDOS_INYECTAR (mkimagem.py --mandos-prueba) */
#define CAB_NE2K_OPC 157   /* u8 D329 (mkimagem.py): bit 0 = sondear NE2000 ISA (--ne2k-isa); sin él no se tocan esos puertos */
#define CAB_NE2K_IRQ 158   /* u8 D329 (mkimagem.py --ne2k-irq N): IRQ de la NE2000 ISA; 0 = autodetectarla */
#define CAB_MEM_OPC  159   /* u8 D349 (mkimagem.py): bit 0 --sem-pae (paginación clásica aunque haya PAE), bit 1 --sem-nx, bit 2 --sin-swap (D352) */
#define CAB_MEM_SEM_PAE 1
#define CAB_MEM_SEM_NX  2
#define CAB_MEM_SEM_TROCA 4   /* D352: bit 2 de CAB_MEM_OPC: sin swap comprimida (mkimagem.py --sin-swap) */
#define CAB_TROCA_MB 160   /* u16 D352 (mkimagem.py --swap-mb N): tope del almacén de la swap en MB; 0 = automático */
/* D353: 192-255 = lo que guarda kconfig (KCONF_* de kconf.h). Quien lee CAB_MEM_OPC, CAB_DISCO_OPC o CAB_FREQ
 * usa kconf_cab(), que pone lo guardado encima de la imagen. */
extern uint8_t cabecalho_boot[512];

/* Copia hecha al inicio del kernel; vale durante toda la ejecución. */
extern info_boot_t info_boot;
extern regiao_e820_t mapa_memoria[MAPA_MAX];

#endif
