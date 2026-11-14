# BipsPl/unify.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/unify.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F756E696679__a0,@function
.globl X1_247573655F756E696679__a0

X1_247573655F756E696679__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D__a2,@function
.globl X1_3D__a2

X1_3D__a2:
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unify_with_occurs_check__a2,@function
.globl X0_unify_with_occurs_check__a2

X0_unify_with_occurs_check__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify_Occurs_Check
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_5C5C3D__a2,@function
.globl X1_5C5C3D__a2

X1_5C5C3D__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_5C5C2B__a1
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
	movl	%eax, at+12
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC2, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$42, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F756E696679__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unify_with_occurs_check__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_5C5C3D__a2+0, 8(%esp)
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
	.string	"$use_unify"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/unify.pl"
.LC2:
	.string	"="
.LC3:
	.string	"\\="
.LC4:
	.string	"system"
.LC5:
	.string	"unify_with_occurs_check"
	.data	
	.align	4
	.local	at
	.comm	at,24,4
	.local	fn
	.comm	fn,4,4
	.section	.note.GNU-stack,"",@progbits
