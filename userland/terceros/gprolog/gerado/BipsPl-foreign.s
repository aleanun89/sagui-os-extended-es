# BipsPl/foreign.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/foreign.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_24666F7263655F666F726569676E5F6C696E6B__a0,@function
.globl X1_24666F7263655F666F726569676E5F6C696E6B__a0

X1_24666F7263655F666F726569676E5F6C696E6B__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24706C5F71756572795F7265636F7665725F616C74__a0,@function
.globl X1_24706C5F71756572795F7265636F7665725F616C74__a0

X1_24706C5F71756572795F7265636F7665725F616C74__a0:
	call	Pl_Query_Recover_Alt_0
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
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
	movl	%eax, at+12
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24666F7263655F666F726569676E5F6C696E6B__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F71756572795F7265636F7665725F616C74__a0+0, 8(%esp)
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
	.string	"$force_foreign_link"
.LC1:
	.string	"$pl_query_recover_alt"
.LC2:
	.string	"/home/diaz/GP/src/BipsPl/foreign.pl"
.LC3:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,16,4
	.section	.note.GNU-stack,"",@progbits
