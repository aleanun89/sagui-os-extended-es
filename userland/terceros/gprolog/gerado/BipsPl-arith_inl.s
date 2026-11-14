# BipsPl/arith_inl.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/arith_inl.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F61726974685F696E6C__a0,@function
.globl X1_247573655F61726974685F696E6C__a0

X1_247573655F61726974685F696E6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_is__a2,@function
.globl X0_is__a2

X0_is__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D3A3D__a2,@function
.globl X1_3D3A3D__a2

X1_3D3A3D__a2:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D5C5C3D__a2,@function
.globl X1_3D5C5C3D__a2

X1_3D5C5C3D__a2:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3C__a2,@function
.globl X1_3C__a2

X1_3C__a2:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Lt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D3C__a2,@function
.globl X1_3D3C__a2

X1_3D3C__a2:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Lte
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3E__a2,@function
.globl X1_3E__a2

X1_3E__a2:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3E3D__a2,@function
.globl X1_3E3D__a2

X1_3E3D__a2:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Gte
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2461726974685F6576616C__a2,@function
.globl X1_2461726974685F6576616C__a2

X1_2461726974685F6576616C__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Arith_Eval_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_succ__a2,@function
.globl X0_succ__a2

X0_succ__a2:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Succ_2
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
	movl	%eax, at+40
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F61726974685F696E6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_is__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$47, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D3A3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D5C5C3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$53, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D3C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$62, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3E3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2461726974685F6576616C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$70, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_succ__a2+0, 8(%esp)
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
	.string	"$arith_eval"
.LC1:
	.string	"$use_arith_inl"
.LC2:
	.string	"/home/diaz/GP/src/BipsPl/arith_inl.pl"
.LC3:
	.string	"<"
.LC4:
	.string	"=:="
.LC5:
	.string	"=<"
.LC6:
	.string	"=\\="
.LC7:
	.string	">"
.LC8:
	.string	">="
.LC9:
	.string	"is"
.LC10:
	.string	"succ"
.LC11:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,48,4
	.section	.note.GNU-stack,"",@progbits
