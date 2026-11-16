/* SPDX-License-Identifier: GPL-3.0-only */
/* programa.ld - como os programas do SaguiOS são ligados.
 * Endereço 0x80000000 (D049; antes 0x40000000, PRD 4); dados numa página
 * separada do código. Desde la mejora 094, con la tabla de constructores. */
ENTRY(_inicio)
SECTIONS
{
    . = 0x80000000;
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
