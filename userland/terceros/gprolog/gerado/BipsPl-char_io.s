# BipsPl/char_io.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/char_io.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F636861725F696F__a0,@function
.globl X1_247573655F636861725F696F__a0

X1_247573655F636861725F696F__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_key__a1,@function
.globl X0_get_key__a1

X0_get_key__a1:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Key_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_key__a2,@function
.globl X0_get_key__a2

X0_get_key__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Get_Key_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_key_no_echo__a1,@function
.globl X0_get_key_no_echo__a1

X0_get_key_no_echo__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Key_No_Echo_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_key_no_echo__a2,@function
.globl X0_get_key_no_echo__a2

X0_get_key_no_echo__a2:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Get_Key_No_Echo_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_char__a1,@function
.globl X0_get_char__a1

X0_get_char__a1:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Char_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_char__a2,@function
.globl X0_get_char__a2

X0_get_char__a2:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Get_Char_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_code__a1,@function
.globl X0_get_code__a1

X0_get_code__a1:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Code_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_code__a2,@function
.globl X0_get_code__a2

X0_get_code__a2:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Get_Code_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_byte__a1,@function
.globl X0_get_byte__a1

X0_get_byte__a1:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Byte_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_byte__a2,@function
.globl X0_get_byte__a2

X0_get_byte__a2:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Get_Byte_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_char__a1,@function
.globl X0_unget_char__a1

X0_unget_char__a1:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Unget_Char_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_char__a2,@function
.globl X0_unget_char__a2

X0_unget_char__a2:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Unget_Char_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_code__a1,@function
.globl X0_unget_code__a1

X0_unget_code__a1:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Unget_Code_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_code__a2,@function
.globl X0_unget_code__a2

X0_unget_code__a2:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Unget_Code_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_byte__a1,@function
.globl X0_unget_byte__a1

X0_unget_byte__a1:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Unget_Byte_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unget_byte__a2,@function
.globl X0_unget_byte__a2

X0_unget_byte__a2:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Unget_Byte_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_char__a1,@function
.globl X0_peek_char__a1

X0_peek_char__a1:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Peek_Char_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_char__a2,@function
.globl X0_peek_char__a2

X0_peek_char__a2:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Peek_Char_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_code__a1,@function
.globl X0_peek_code__a1

X0_peek_code__a1:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Peek_Code_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_code__a2,@function
.globl X0_peek_code__a2

X0_peek_code__a2:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Peek_Code_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_byte__a1,@function
.globl X0_peek_byte__a1

X0_peek_byte__a1:
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Peek_Byte_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_peek_byte__a2,@function
.globl X0_peek_byte__a2

X0_peek_byte__a2:
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Peek_Byte_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_char__a1,@function
.globl X0_put_char__a1

X0_put_char__a1:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Put_Char_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_char__a2,@function
.globl X0_put_char__a2

X0_put_char__a2:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Put_Char_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_code__a1,@function
.globl X0_put_code__a1

X0_put_code__a1:
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Put_Code_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_code__a2,@function
.globl X0_put_code__a2

X0_put_code__a2:
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Put_Code_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_byte__a1,@function
.globl X0_put_byte__a1

X0_put_byte__a1:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Put_Byte_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put_byte__a2,@function
.globl X0_put_byte__a2

X0_put_byte__a2:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Put_Byte_2
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
	movl	%eax, at+28
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F636861725F696F__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_key__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$48, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_key__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$53, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_key_no_echo__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$57, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_key_no_echo__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_char__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$70, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_char__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_code__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_code__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$88, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_byte__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$92, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_byte__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_char__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_char__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$113, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_code__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_code__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$124, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_byte__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$128, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unget_byte__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$137, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_char__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_char__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$148, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_code__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$152, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_code__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$159, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_byte__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$163, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_peek_byte__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$172, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_char__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$176, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_char__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$183, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_code__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$187, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_code__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$194, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_byte__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$198, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put_byte__a2+0, 8(%esp)
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
	.string	"$use_char_io"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/char_io.pl"
.LC2:
	.string	"get_byte"
.LC3:
	.string	"get_char"
.LC4:
	.string	"get_code"
.LC5:
	.string	"get_key"
.LC6:
	.string	"get_key_no_echo"
.LC7:
	.string	"peek_byte"
.LC8:
	.string	"peek_char"
.LC9:
	.string	"peek_code"
.LC10:
	.string	"put_byte"
.LC11:
	.string	"put_char"
.LC12:
	.string	"put_code"
.LC13:
	.string	"system"
.LC14:
	.string	"unget_byte"
.LC15:
	.string	"unget_char"
.LC16:
	.string	"unget_code"
	.data	
	.align	4
	.local	at
	.comm	at,68,4
	.section	.note.GNU-stack,"",@progbits
