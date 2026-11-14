/* SPDX-License-Identifier: GPL-2.0-or-later */
/* netsurf.ld - cómo se enlaza /bin/netsurf (D184): el userland/programa.ld
 * de todos los programas más la tabla de constructores (.init_array). libnsfb
 * registra sus superficies con __attribute__((constructor)); desde la mejora
 * 094 los llama inicio.asm para todos los programas, con esta misma tabla. */
ENTRY(_inicio)
SECTIONS
{
    . = 0x80000000;
    .text : { *(.text.inicio) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    . = ALIGN(4);
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;
    }
    . = ALIGN(4096);
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
