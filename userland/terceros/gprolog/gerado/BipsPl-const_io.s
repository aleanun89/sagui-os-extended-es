# BipsPl/const_io.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/const_io.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F636F6E73745F696F__a0,@function
.globl X1_247573655F636F6E73745F696F__a0

X1_247573655F636F6E73745F696F__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_to_atom__a2,@function
.globl X0_write_to_atom__a2

X0_write_to_atom__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_To_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_to_chars__a2,@function
.globl X0_write_to_chars__a2

X0_write_to_chars__a2:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_To_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_to_codes__a2,@function
.globl X0_write_to_codes__a2

X0_write_to_codes__a2:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_To_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_writeq_to_atom__a2,@function
.globl X0_writeq_to_atom__a2

X0_writeq_to_atom__a2:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Writeq_To_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_writeq_to_chars__a2,@function
.globl X0_writeq_to_chars__a2

X0_writeq_to_chars__a2:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Writeq_To_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_writeq_to_codes__a2,@function
.globl X0_writeq_to_codes__a2

X0_writeq_to_codes__a2:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Writeq_To_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_canonical_to_atom__a2,@function
.globl X0_write_canonical_to_atom__a2

X0_write_canonical_to_atom__a2:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Canonical_To_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_canonical_to_chars__a2,@function
.globl X0_write_canonical_to_chars__a2

X0_write_canonical_to_chars__a2:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Canonical_To_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_canonical_to_codes__a2,@function
.globl X0_write_canonical_to_codes__a2

X0_write_canonical_to_codes__a2:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Canonical_To_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_display_to_atom__a2,@function
.globl X0_display_to_atom__a2

X0_display_to_atom__a2:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Display_To_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_display_to_chars__a2,@function
.globl X0_display_to_chars__a2

X0_display_to_chars__a2:
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Display_To_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_display_to_codes__a2,@function
.globl X0_display_to_codes__a2

X0_display_to_codes__a2:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Display_To_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_print_to_atom__a2,@function
.globl X0_print_to_atom__a2

X0_print_to_atom__a2:
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Print_To_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_print_to_chars__a2,@function
.globl X0_print_to_chars__a2

X0_print_to_chars__a2:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Print_To_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_print_to_codes__a2,@function
.globl X0_print_to_codes__a2

X0_print_to_codes__a2:
	movl	at+68, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Print_To_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_term_to_atom__a3,@function
.globl X0_write_term_to_atom__a3

X0_write_term_to_atom__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+72, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247365745F77726974655F64656661756C7473__a0
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E73__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Term_To_Atom_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_term_to_chars__a3,@function
.globl X0_write_term_to_chars__a3

X0_write_term_to_chars__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+76, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247365745F77726974655F64656661756C7473__a0
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E73__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Term_To_Chars_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_term_to_codes__a3,@function
.globl X0_write_term_to_codes__a3

X0_write_term_to_codes__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+80, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247365745F77726974655F64656661756C7473__a0
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E73__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Term_To_Codes_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_format_to_atom__a3,@function
.globl X0_format_to_atom__a3

X0_format_to_atom__a3:
	movl	at+84, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Format_To_Atom_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_format_to_chars__a3,@function
.globl X0_format_to_chars__a3

X0_format_to_chars__a3:
	movl	at+88, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Format_To_Chars_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_format_to_codes__a3,@function
.globl X0_format_to_codes__a3

X0_format_to_codes__a3:
	movl	at+92, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Format_To_Codes_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_from_atom__a2,@function
.globl X0_read_from_atom__a2

X0_read_from_atom__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+96, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_From_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_from_chars__a2,@function
.globl X0_read_from_chars__a2

X0_read_from_chars__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+100, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_From_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_from_codes__a2,@function
.globl X0_read_from_codes__a2

X0_read_from_codes__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+104, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_From_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_term_from_atom__a3,@function
.globl X0_read_term_from_atom__a3

X0_read_term_from_atom__a3:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+108, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E73__a4
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 16(%esp)
	call	Pl_Read_Term_From_Atom_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_term_from_chars__a3,@function
.globl X0_read_term_from_chars__a3

X0_read_term_from_chars__a3:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E73__a4
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 16(%esp)
	call	Pl_Read_Term_From_Chars_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_term_from_codes__a3,@function
.globl X0_read_term_from_codes__a3

X0_read_term_from_codes__a3:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+116, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E73__a4
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 16(%esp)
	call	Pl_Read_Term_From_Codes_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_token_from_atom__a2,@function
.globl X0_read_token_from_atom__a2

X0_read_token_from_atom__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+120, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_Token_From_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_token_from_chars__a2,@function
.globl X0_read_token_from_chars__a2

X0_read_token_from_chars__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+124, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_Token_From_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_token_from_codes__a2,@function
.globl X0_read_token_from_codes__a2

X0_read_token_from_codes__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+128, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_Token_From_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	Object_Initializer,@function

Object_Initializer:
	pushl	%esi
	subl	$132, %esp
	movl	$Prolog_Object_Initializer+0, 0(%esp)
	movl	$System_Directives+0, 4(%esp)
	movl	$User_Directives+0, 8(%esp)
	call	Pl_New_Object
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	Prolog_Object_Initializer,@function

Prolog_Object_Initializer:
	pushl	%esi
	subl	$132, %esp
	movl	$.LC0, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F636F6E73745F696F__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$46, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_to_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_to_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_to_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$61, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_writeq_to_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_writeq_to_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$69, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_writeq_to_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$76, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_canonical_to_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$80, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_canonical_to_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$84, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_canonical_to_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_display_to_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_display_to_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_display_to_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_print_to_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$110, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_print_to_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$114, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_print_to_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$121, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_term_to_atom__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_term_to_chars__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$133, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_term_to_codes__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$142, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_format_to_atom__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_format_to_chars__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$150, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_format_to_codes__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$157, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_from_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_from_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$167, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_from_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$175, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_term_from_atom__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$181, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_term_from_chars__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$187, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_term_from_codes__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$196, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_token_from_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$201, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_token_from_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$206, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_token_from_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	System_Directives,@function

System_Directives:
	pushl	%esi
	subl	$132, %esp
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	User_Directives,@function

User_Directives:
	pushl	%esi
	subl	$132, %esp
	addl	$132, %esp
	popl	%esi
	ret	
	.section	.ctors,"aw",@progbits
	.align	4
	.long	Object_Initializer
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"$use_const_io"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/const_io.pl"
.LC2:
	.string	"display_to_atom"
.LC3:
	.string	"display_to_chars"
.LC4:
	.string	"display_to_codes"
.LC5:
	.string	"format_to_atom"
.LC6:
	.string	"format_to_chars"
.LC7:
	.string	"format_to_codes"
.LC8:
	.string	"print_to_atom"
.LC9:
	.string	"print_to_chars"
.LC10:
	.string	"print_to_codes"
.LC11:
	.string	"read_from_atom"
.LC12:
	.string	"read_from_chars"
.LC13:
	.string	"read_from_codes"
.LC14:
	.string	"read_term_from_atom"
.LC15:
	.string	"read_term_from_chars"
.LC16:
	.string	"read_term_from_codes"
.LC17:
	.string	"read_token_from_atom"
.LC18:
	.string	"read_token_from_chars"
.LC19:
	.string	"read_token_from_codes"
.LC20:
	.string	"system"
.LC21:
	.string	"write_canonical_to_atom"
.LC22:
	.string	"write_canonical_to_chars"
.LC23:
	.string	"write_canonical_to_codes"
.LC24:
	.string	"write_term_to_atom"
.LC25:
	.string	"write_term_to_chars"
.LC26:
	.string	"write_term_to_codes"
.LC27:
	.string	"write_to_atom"
.LC28:
	.string	"write_to_chars"
.LC29:
	.string	"write_to_codes"
.LC30:
	.string	"writeq_to_atom"
.LC31:
	.string	"writeq_to_chars"
.LC32:
	.string	"writeq_to_codes"
	.data	
	.align	4
	.local	at
	.comm	at,132,4
	.section	.note.GNU-stack,"",@progbits
