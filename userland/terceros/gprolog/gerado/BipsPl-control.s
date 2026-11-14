# BipsPl/control.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/control.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F636F6E74726F6C__a0,@function
.globl X1_247573655F636F6E74726F6C__a0

X1_247573655F636F6E74726F6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_repeat__a0,@function
.globl X0_repeat__a0

X0_repeat__a0:
	movl	$.Lpred2_1+0, %eax
	movl	$0, %edx
	call	Pl_Create_Choice_Point
	jmp	*pl_reg_bank+1040

.Lpred2_1:
	movl	$0, %eax
	call	Pl_Delete_Choice_Point
	jmp	X0_repeat__a0
	.p2align	4,,15
	.type	X0_abort__a0,@function
.globl X0_abort__a0

X0_abort__a0:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Halt_If_No_Top_Level_1
	jmp	*%eax
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_stop__a0,@function
.globl X0_stop__a0

X0_stop__a0:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Halt_If_No_Top_Level_1
	jmp	*%eax
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_halt__a0,@function
.globl X0_halt__a0

X0_halt__a0:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_halt__a1
	.p2align	4,,15
	.type	X0_halt__a1,@function
.globl X0_halt__a1

X0_halt__a1:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Halt_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_between__a3,@function
.globl X0_between__a3

X0_between__a3:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Between_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246265747765656E5F616C74__a0,@function
.globl X1_246265747765656E5F616C74__a0

X1_246265747765656E5F616C74__a0:
	call	Pl_Between_Alt_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_for__a3,@function
.globl X0_for__a3

X0_for__a3:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 8(%esp)
	call	Pl_Between_3
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
	movl	%eax, at+32
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F636F6E74726F6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$46, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_repeat__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_abort__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_stop__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_halt__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_halt__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$79, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_between__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246265747765656E5F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$88, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_for__a3+0, 8(%esp)
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
	.string	"$between_alt"
.LC1:
	.string	"$use_control"
.LC2:
	.string	"/home/diaz/GP/src/BipsPl/control.pl"
.LC3:
	.string	"abort"
.LC4:
	.string	"between"
.LC5:
	.string	"for"
.LC6:
	.string	"halt"
.LC7:
	.string	"repeat"
.LC8:
	.string	"stop"
.LC9:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,40,4
	.section	.note.GNU-stack,"",@progbits
