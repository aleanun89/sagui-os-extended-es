/* SPDX-License-Identifier: GPL-2.0-or-later */
/* ligacao.ld - enlazado del cargador UEFI (D264): un ELF independiente de la posición
 * con la imagen a partir de 0x1000 (las cabeceras PE ocupan la primera página) y dos
 * segmentos alineados a página: código (lectura y ejecución) y datos (lectura y
 * escritura). tools/mkefi.py copia los segmentos al PE en las mismas direcciones
 * relativas y convierte las reubicaciones RELATIVE en las del PE. */
ENTRY(efi_main)

PHDRS
{
    codigo PT_LOAD FLAGS(5);
    dados  PT_LOAD FLAGS(6);
    dinamico PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
    . = 0x1000;
    .text : { *(.text .text.*) } :codigo
    . = ALIGN(0x1000);
    .data : {
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.got .got.plt)
        *(.bss .bss.*)
        *(COMMON)
    } :dados
    .dynamic : { *(.dynamic) } :dados :dinamico
    .rela.dyn : { *(.rela.*) } :dados
    .rel.dyn : { *(.rel.*) } :dados
    .dynsym : { *(.dynsym) } :dados
    .dynstr : { *(.dynstr) } :dados
    .hash : { *(.hash) *(.gnu.hash) } :dados
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) *(.interp) }
}
