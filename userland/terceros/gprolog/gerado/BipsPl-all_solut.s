# BipsPl/all_solut.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/all_solut.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F616C6C5F736F6C7574__a0,@function
.globl X1_247573655F616C6C5F736F6C7574__a0

X1_247573655F616C6C5F736F6C7574__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_findall__a3,@function
.globl X0_findall__a3

X0_findall__a3:
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+20
	jmp	X1_2466696E64616C6C__a6
	.p2align	4,,15
	.type	X0_findall__a4,@function
.globl X0_findall__a4

X0_findall__a4:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+20
	jmp	X1_2466696E64616C6C__a6
	.p2align	4,,15
	.type	X1_2466696E64616C6C__a6,@function
.globl X1_2466696E64616C6C__a6

X1_2466696E64616C6C__a6:
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -36(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C6973745F617267__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C6973745F617267__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2473746F72655F736F6C7574696F6E73__a5
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Recover_Solutions_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_setof__a3,@function
.globl X0_setof__a3

X0_setof__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C6973745F617267__a3
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_246261676F66__a5
	.p2align	4,,15
	.type	X0_bagof__a3,@function
.globl X0_bagof__a3

X0_bagof__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C6973745F617267__a3
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_246261676F66__a5
	.p2align	4,,15
	.type	X1_246261676F66__a5,@function
.globl X1_246261676F66__a5

X1_246261676F66__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred7_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 12(%esp)
	call	Pl_Free_Variables_4
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_2473746F72655F736F6C7574696F6E73__a5
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Recover_Solutions_4
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_24246261676F662F355F2461757831__a2
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2467726F75705F736F6C7574696F6E73__a3

.Lpred7_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	call	Pl_Recover_Generator_1
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_2466696E64616C6C__a6
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_24246261676F662F355F2461757832__a2
	.p2align	4,,15
	.type	X1_24246261676F662F355F2461757832__a2,@function

X1_24246261676F662F355F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred8_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_sort__a1
	.p2align	4,,15
	.type	X1_24246261676F662F355F2461757831__a2,@function

X1_24246261676F662F355F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_keysort__a1

.Lpred9_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_sort__a1
	.p2align	4,,15
	.type	X1_2473746F72655F736F6C7574696F6E73__a5,@function
.globl X1_2473746F72655F736F6C7574696F6E73__a5

X1_2473746F72655F736F6C7574696F6E73__a5:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	call	Pl_Stop_Mark_1
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_242473746F72655F736F6C7574696F6E732F355F2461757831__a4
	.p2align	4,,15
	.type	X1_242473746F72655F736F6C7574696F6E732F355F2461757831__a4,@function

X1_242473746F72655F736F6C7574696F6E732F355F2461757831__a4:
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Store_Solution_1
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred11_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467726F75705F736F6C7574696F6E73__a3,@function
.globl X1_2467726F75705F736F6C7574696F6E73__a3

X1_2467726F75705F736F6C7574696F6E73__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Group_Solutions_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467726F75705F736F6C7574696F6E735F616C74__a0,@function
.globl X1_2467726F75705F736F6C7574696F6E735F616C74__a0

X1_2467726F75705F736F6C7574696F6E735F616C74__a0:
	call	Pl_Group_Solutions_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636865636B5F6C6973745F617267__a3,@function
.globl X1_24636865636B5F6C6973745F617267__a3

X1_24636865636B5F6C6973745F617267__a3:
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	jmp	X1_24636865636B5F6C6973745F6F725F7061727469616C5F6C697374__a1
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
	movl	%eax, at+32
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+12, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	at+20, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC16, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC17, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F616C6C5F736F6C7574__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_findall__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$47, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_findall__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2466696E64616C6C__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_setof__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_bagof__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246261676F66__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246261676F662F355F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246261676F662F355F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473746F72655F736F6C7574696F6E73__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473746F72655F736F6C7574696F6E732F355F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2467726F75705F736F6C7574696F6E73__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$109, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2467726F75705F736F6C7574696F6E735F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F6C6973745F617267__a3+0, 8(%esp)
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
	.string	"$$bagof/5_$aux1"
.LC1:
	.string	"$$bagof/5_$aux2"
.LC2:
	.string	"$$store_solutions/5_$aux1"
.LC3:
	.string	"$bagof"
.LC4:
	.string	"$check_list_arg"
.LC5:
	.string	"$findall"
.LC6:
	.string	"$group_solutions"
.LC7:
	.string	"$group_solutions_alt"
.LC8:
	.string	"$store_solutions"
.LC9:
	.string	"$use_all_solut"
.LC17:
	.string	"-"
.LC10:
	.string	"/home/diaz/GP/src/BipsPl/all_solut.pl"
.LC11:
	.string	"bagof"
.LC12:
	.string	"findall"
.LC13:
	.string	"setof"
.LC14:
	.string	"system"
.LC16:
	.string	"true"
.LC15:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,64,4
	.local	fn
	.comm	fn,4,4
	.local	ta
	.comm	ta,16,4
	.section	.note.GNU-stack,"",@progbits
