# BipsPl/no_le_interf.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/no_le_interf.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6C655F696E74657266__a0,@function
.globl X1_247573655F6C655F696E74657266__a0

X1_247573655F6C655F696E74657266__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6C696E656469745F70726F6D7074__a1,@function
.globl X1_246765745F6C696E656469745F70726F6D7074__a1

X1_246765745F6C696E656469745F70726F6D7074__a1:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F6C696E656469745F70726F6D7074__a1,@function
.globl X1_247365745F6C696E656469745F70726F6D7074__a1

X1_247365745F6C696E656469745F70726F6D7074__a1:
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
	movl	%eax, at+12
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6C655F696E74657266__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F6C696E656469745F70726F6D7074__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$46, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F6C696E656469745F70726F6D7074__a1+0, 8(%esp)
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
	.string	"$get_linedit_prompt"
.LC1:
	.string	"$set_linedit_prompt"
.LC2:
	.string	"$use_le_interf"
.LC3:
	.string	"/home/diaz/GP/src/BipsPl/no_le_interf.pl"
.LC4:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,20,4
	.section	.note.GNU-stack,"",@progbits
