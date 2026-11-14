# BipsPl/oper.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/oper.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6F706572__a0,@function
.globl X1_247573655F6F706572__a0

X1_247573655F6F706572__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_op__a3,@function
.globl X0_op__a3

X0_op__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246F702F335F2461757831__a3
	.p2align	4,,15
	.type	X1_246F702F335F2461757831__a3,@function

X1_246F702F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246F7032__a3

.Lpred3_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246F7031__a3
	.p2align	4,,15
	.type	X1_246F7031__a3,@function
.globl X1_246F7031__a3

X1_246F7031__a3:
	movl	$.Lpred4_1+0, %eax
	movl	$.Lpred4_2+0, %edx
	movl	$.Lpred4_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred4_1:
	movl	$.Lpred4_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred4_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred4_3:
	call	Pl_Delete_Choice_Point3

.Lpred4_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_246F7032__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246F7031__a3
	.p2align	4,,15
	.type	X1_246F7032__a3,@function
.globl X1_246F7032__a3

X1_246F7032__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Op_3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_op__a3,@function
.globl X0_current_op__a3

X0_current_op__a3:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Current_Op_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F6F705F616C74__a0,@function
.globl X1_2463757272656E745F6F705F616C74__a0

X1_2463757272656E745F6F705F616C74__a0:
	call	Pl_Current_Op_Alt_0
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
	movl	%eax, at+36
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6F706572__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$43, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_op__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$43, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246F702F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246F7031__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246F7032__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_op__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$70, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F6F705F616C74__a0+0, 8(%esp)
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
	.string	"$current_op_alt"
.LC1:
	.string	"$op/3_$aux1"
.LC2:
	.string	"$op1"
.LC3:
	.string	"$op2"
.LC4:
	.string	"$use_oper"
.LC5:
	.string	"/home/diaz/GP/src/BipsPl/oper.pl"
.LC6:
	.string	"current_op"
.LC7:
	.string	"op"
.LC8:
	.string	"system"
.LC9:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,40,4
	.section	.note.GNU-stack,"",@progbits
