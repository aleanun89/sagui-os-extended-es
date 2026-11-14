# BipsPl/g_var_inl.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/g_var_inl.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F675F7661725F696E6C__a0,@function
.globl X1_247573655F675F7661725F696E6C__a0

X1_247573655F675F7661725F696E6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_assign__a2,@function
.globl X0_g_assign__a2

X0_g_assign__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_assignb__a2,@function
.globl X0_g_assignb__a2

X0_g_assignb__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assignb
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_link__a2,@function
.globl X0_g_link__a2

X0_g_link__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_read__a2,@function
.globl X0_g_read__a2

X0_g_read__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_array_size__a2,@function
.globl X0_g_array_size__a2

X0_g_array_size__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Array_Size
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_inc__a1,@function
.globl X0_g_inc__a1

X0_g_inc__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_G_Inc
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_inco__a2,@function
.globl X0_g_inco__a2

X0_g_inco__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Inco
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_inc__a2,@function
.globl X0_g_inc__a2

X0_g_inc__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Inc_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_inc__a3,@function
.globl X0_g_inc__a3

X0_g_inc__a3:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_G_Inc_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_dec__a1,@function
.globl X0_g_dec__a1

X0_g_dec__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_G_Dec
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_deco__a2,@function
.globl X0_g_deco__a2

X0_g_deco__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Deco
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_dec__a2,@function
.globl X0_g_dec__a2

X0_g_dec__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Dec_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_dec__a3,@function
.globl X0_g_dec__a3

X0_g_dec__a3:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_G_Dec_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_set_bit__a2,@function
.globl X0_g_set_bit__a2

X0_g_set_bit__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Set_Bit
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_reset_bit__a2,@function
.globl X0_g_reset_bit__a2

X0_g_reset_bit__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Reset_Bit
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_test_set_bit__a2,@function
.globl X0_g_test_set_bit__a2

X0_g_test_set_bit__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Test_Set_Bit
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_g_test_reset_bit__a2,@function
.globl X0_g_test_reset_bit__a2

X0_g_test_reset_bit__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Test_Reset_Bit
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
	movl	%eax, at+28
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F675F7661725F696E6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_assign__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_assignb__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_link__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$62, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_read__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$68, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_array_size__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_inc__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_inco__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_inc__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$85, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_inc__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_dec__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_deco__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_dec__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$103, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_dec__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$109, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_set_bit__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$115, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_reset_bit__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$121, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_test_set_bit__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_g_test_reset_bit__a2+0, 8(%esp)
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
	.string	"$use_g_var_inl"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/g_var_inl.pl"
.LC2:
	.string	"g_array_size"
.LC3:
	.string	"g_assign"
.LC4:
	.string	"g_assignb"
.LC5:
	.string	"g_dec"
.LC6:
	.string	"g_deco"
.LC7:
	.string	"g_inc"
.LC8:
	.string	"g_inco"
.LC9:
	.string	"g_link"
.LC10:
	.string	"g_read"
.LC11:
	.string	"g_reset_bit"
.LC12:
	.string	"g_set_bit"
.LC13:
	.string	"g_test_reset_bit"
.LC14:
	.string	"g_test_set_bit"
.LC15:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,64,4
	.section	.note.GNU-stack,"",@progbits
