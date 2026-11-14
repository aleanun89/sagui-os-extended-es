# BipsPl/random.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/random.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F72616E646F6D__a0,@function
.globl X1_247573655F72616E646F6D__a0

X1_247573655F72616E646F6D__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_randomize__a0,@function
.globl X0_randomize__a0

X0_randomize__a0:
	call	Pl_M_Randomize
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_seed__a1,@function
.globl X0_set_seed__a1

X0_set_seed__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Set_Seed_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_seed__a1,@function
.globl X0_get_seed__a1

X0_get_seed__a1:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Seed_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_random__a1,@function
.globl X0_random__a1

X0_random__a1:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Random_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_random__a3,@function
.globl X0_random__a3

X0_random__a3:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Random_3
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
	movl	%eax, at+20
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F72616E646F6D__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_randomize__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_seed__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get_seed__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_random__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_random__a3+0, 8(%esp)
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
	.string	"$use_random"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/random.pl"
.LC2:
	.string	"get_seed"
.LC3:
	.string	"random"
.LC4:
	.string	"randomize"
.LC5:
	.string	"set_seed"
.LC6:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,28,4
	.section	.note.GNU-stack,"",@progbits
