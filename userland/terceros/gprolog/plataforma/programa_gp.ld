/* programa_gp.ld - como userland/programa.ld (D049), más los constructores
 * (.init_array y .ctors) que GNU Prolog usa para registrar sus módulos
 * (D189): se guardan todos, en el orden del enlace, entre
 * __init_array_start y __init_array_end, y principal_gp.c los llama. */
ENTRY(_inicio)
SECTIONS
{
    . = 0x80000000;
    .text : { *(.text.inicio) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;
    }
    . = ALIGN(4096);
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
