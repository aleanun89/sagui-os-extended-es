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
    . = ALIGN(4096);
    /* D351 (W^X): la tabla de constructores es una sección escribible: va con
     * los datos, en su página, para que el código quede R-X y los datos RW-. */
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array .ctors))
        __init_array_end = .;
    }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
