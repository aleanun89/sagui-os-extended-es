; SPDX-License-Identifier: GPL-2.0-or-later
; trampolim_blob.asm - mete el trampolín ya ensamblado (trampolim.bin, binario plano e
; independiente de la posición) en el cargador UEFI como datos de solo lectura (D264).
; El Makefile pasa la ruta con -DTRAMPOLIM_BIN.

section .rodata
global trampolim_ini:data hidden
global trampolim_fim:data hidden
trampolim_ini:
        incbin TRAMPOLIM_BIN
trampolim_fim:
