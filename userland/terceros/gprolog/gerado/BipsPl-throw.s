# BipsPl/throw.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/throw.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F7468726F77__a0,@function
.globl X1_247573655F7468726F77__a0

X1_247573655F7468726F77__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247468726F77__a4,@function
.globl X1_247468726F77__a4

X1_247468726F77__a4:
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 8(%esp)
	call	Pl_Save_Call_Info_3
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247468726F7731__a2
	.p2align	4,,15
	.type	X1_247468726F7731__a2,@function
.globl X1_247468726F7731__a2

X1_247468726F7731__a2:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	call	Pl_Load_Call_Info_Arg_1
	jmp	X1_247468726F775F696E7465726E616C__a2
	.p2align	4,,15
	.type	X1_247468726F775F696E7465726E616C__a2,@function
.globl X1_247468726F775F696E7465726E616C__a2

X1_247468726F775F696E7465726E616C__a2:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24247468726F775F696E7465726E616C2F325F2461757831__a2
.Lcont1:
	movl	$-2147483613, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247379735F7661725F707574__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24756E77696E64__a1
	.p2align	4,,15
	.type	X1_24247468726F775F696E7465726E616C2F325F2461757831__a2,@function

X1_24247468726F775F696E7465726E616C2F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_Call_Info_Bip_Name_1
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred5_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24756E77696E64__a1,@function
.globl X1_24756E77696E64__a1

X1_24756E77696E64__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Throw_2
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
	movl	%eax, at+24
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F7468726F77__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247468726F77__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247468726F7731__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247468726F775F696E7465726E616C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24247468726F775F696E7465726E616C2F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24756E77696E64__a1+0, 8(%esp)
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
	.string	"$$throw_internal/2_$aux1"
.LC1:
	.string	"$throw"
.LC2:
	.string	"$throw1"
.LC3:
	.string	"$throw_internal"
.LC4:
	.string	"$unwind"
.LC5:
	.string	"$use_throw"
.LC6:
	.string	"/home/diaz/GP/src/BipsPl/throw.pl"
.LC7:
	.string	"system"
.LC8:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,36,4
	.section	.note.GNU-stack,"",@progbits
