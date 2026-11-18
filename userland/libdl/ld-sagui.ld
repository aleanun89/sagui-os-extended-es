/* SPDX-License-Identifier: GPL-3.0-only */
/* ld-sagui.ld - el intérprete de los programas dinámicos (D378): como
 * programa.ld, pero en la zona del intérprete (INTERP_BASE, 0xD8000000), que el
 * kernel deja fuera del área de datos de esos programas. */
ENTRY(_ld_inicio)
SECTIONS
{
    . = 0xD8000000;   /* INTERP_BASE (kernel/inc/processo.h, D378) */
    .text : { *(.text.inicio) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    . = ALIGN(4096);
    /* Constructores globales (__attribute__((constructor)), .init_array):
     * inicio.asm los llama antes de principal (mejora 094, punto 10).
     * D351 (W^X): la tabla de constructores es una sección escribible: va con
     * los datos, en su página, para que el código quede R-X y los datos RW-. */
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;
    }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
