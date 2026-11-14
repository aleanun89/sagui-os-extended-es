/* SPDX-License-Identifier: GPL-2.0-or-later */
/* programa_cxx.ld - como se enlazan los programas de C++ del SaguiOS (D228).
 * Es programa.ld (dirección 0x80000000, D049; datos en otra página;
 * constructores en .init_array, mejora 094) y, además, conserva las tablas
 * de desenrollado (.eh_frame) y las de C++ (.gcc_except_table) que necesitan
 * las excepciones: userland/libcxx/rt/desenrolar.c recorre .eh_frame entre
 * __eh_frame_inicio y __eh_frame_fim. Con KEEP, como en el guion por
 * defecto de ld: sin él, --gc-sections las tira enteras (nadie las nombra);
 * con él, ld trata .eh_frame aparte y solo conserva los FDE de las funciones
 * que se quedan. Los programas de C siguen con programa.ld, que las descarta. */
ENTRY(_inicio)
SECTIONS
{
    . = 0x80000000;
    .text : { *(.text.inicio) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) *(.gcc_except_table .gcc_except_table.*) }
    . = ALIGN(4);
    .eh_frame : {
        __eh_frame_inicio = .;
        KEEP(*(.eh_frame))
        __eh_frame_fim = .;
        LONG(0)
    }
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
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame_hdr) }
}
