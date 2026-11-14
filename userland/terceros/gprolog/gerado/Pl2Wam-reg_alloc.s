# Pl2Wam/reg_alloc.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/reg_alloc.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_allocate_registers__a1,@function
.globl X0_allocate_registers__a1

X0_allocate_registers__a1:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_allocate_registers__a2
	.p2align	4,,15
	.type	X0_allocate_registers__a2,@function
.globl X0_allocate_registers__a2

X0_allocate_registers__a2:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24616C6C6F636174655F7265676973746572732F325F2461757831__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_create_lst_tmp__a4
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_assign_lst_tmp__a2
	.p2align	4,,15
	.type	X1_24616C6C6F636174655F7265676973746572732F325F2461757831__a3,@function

X1_24616C6C6F636174655F7265676973746572732F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_aliases__a3

.Lpred3_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_aliases__a3,@function
.globl X0_aliases__a3

X0_aliases__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred4_1+0, %eax
	movl	$.Lpred4_2+0, %edx
	movl	$.Lpred4_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred4_1:
	movl	$.Lpred4_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred4_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred4_3:
	call	Pl_Delete_Choice_Point4

.Lpred4_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24616C69617365732F335F2461757831__a3
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_aliases__a3
	.p2align	4,,15
	.type	X1_24616C69617365732F335F2461757831__a3,@function

X1_24616C69617365732F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_alias_stop_instruction__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	call	Pl_Delete_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_codification__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_aliases1__a3
	.p2align	4,,15
	.type	X0_aliases1__a3,@function
.globl X0_aliases1__a3

X0_aliases1__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred6_1+0, %eax
	movl	$.Lpred6_2+0, %edx
	movl	$.Lpred6_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred6_1:
	movl	$.Lpred6_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred6_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_3:
	call	Pl_Delete_Choice_Point4

.Lpred6_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_24616C6961736573312F335F2461757831__a3
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_aliases1__a3
	.p2align	4,,15
	.type	X1_24616C6961736573312F335F2461757831__a3,@function

X1_24616C6961736573312F335F2461757831__a3:
	movl	$.Lpred7_2+0, %eax
	movl	$.Lpred7_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred7_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred7_2:
	movl	$.Lpred7_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred7_3:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred7_4:
	movl	$.Lpred7_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred7_5:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_remove_aliases_of__a3

.Lpred7_6:
	call	Pl_Delete_Choice_Point3

.Lpred7_7:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_remove_aliases_of__a3
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_add_alias__a4
	.p2align	4,,15
	.type	X0_add_alias__a4,@function
.globl X0_add_alias__a4

X0_add_alias__a4:
	movl	$.Lpred8_1+0, %eax
	movl	$.Lpred8_2+0, %edx
	movl	$.Lpred8_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred8_1:
	movl	$.Lpred8_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred8_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_3:
	call	Pl_Delete_Choice_Point4

.Lpred8_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	jmp	X1_246164645F616C6961732F345F2461757831__a6
	.p2align	4,,15
	.type	X1_246164645F616C6961732F345F2461757831__a6,@function

X1_246164645F616C6961732F345F2461757831__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred9_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -36(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_set_elt__a2
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_set_add__a3
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_add_alias__a4
	.p2align	4,,15
	.type	X0_find_aliases_of__a3,@function
.globl X0_find_aliases_of__a3

X0_find_aliases_of__a3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	jmp	X1_2466696E645F616C69617365735F6F662F335F2461757831__a4
	.p2align	4,,15
	.type	X1_2466696E645F616C69617365735F6F662F335F2461757831__a4,@function

X1_2466696E645F616C69617365735F6F662F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred11_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_set_delete__a3
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred11_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_find_aliases_of__a3
	.p2align	4,,15
	.type	X0_remove_aliases_of__a3,@function
.globl X0_remove_aliases_of__a3

X0_remove_aliases_of__a3:
	movl	$.Lpred12_1+0, %eax
	movl	$.Lpred12_2+0, %edx
	movl	$.Lpred12_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred12_1:
	movl	$.Lpred12_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred12_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_3:
	call	Pl_Delete_Choice_Point3

.Lpred12_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	jmp	X1_2472656D6F76655F616C69617365735F6F662F335F2461757831__a4
	.p2align	4,,15
	.type	X1_2472656D6F76655F616C69617365735F6F662F335F2461757831__a4,@function

X1_2472656D6F76655F616C69617365735F6F662F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred13_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_set_delete__a3
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2472656D6F76655F616C69617365735F6F662F335F2461757832__a3

.Lpred13_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_remove_aliases_of__a3
	.p2align	4,,15
	.type	X1_2472656D6F76655F616C69617365735F6F662F335F2461757832__a3,@function

X1_2472656D6F76655F616C69617365735F6F662F335F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_2472656D6F76655F616C69617365735F6F662F335F2461757833__a1
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2472656D6F76655F616C69617365735F6F662F335F2461757833__a1,@function

X1_2472656D6F76655F616C69617365735F6F662F335F2461757833__a1:
	movl	$.Lpred15_1+0, %eax
	movl	$.Lpred15_2+0, %edx
	movl	$.Lpred15_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred15_1:
	movl	$.Lpred15_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred15_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred15_3:
	call	Pl_Delete_Choice_Point1

.Lpred15_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_create_lst_tmp__a4,@function
.globl X0_create_lst_tmp__a4

X0_create_lst_tmp__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred16_1+0, %eax
	movl	$.Lpred16_2+0, %edx
	movl	$.Lpred16_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred16_1:
	movl	$.Lpred16_3+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred16_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred16_3:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred16_4:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_create_lst_tmp__a4
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_codification__a2
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X0_handle_lst_code__a6
	.p2align	4,,15
	.type	X0_handle_lst_code__a6,@function
.globl X0_handle_lst_code__a6

X0_handle_lst_code__a6:
	movl	$.Lpred17_1+0, %eax
	movl	$.Lpred17_2+0, %edx
	movl	$.Lpred17_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred17_1:
	movl	$.Lpred17_3+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred17_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred17_3:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred17_4:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_handle_lst_code__a6
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+24
	call	Pl_Deallocate
	jmp	X0_handle_one_code__a7
	.p2align	4,,15
	.type	X0_handle_one_code__a7,@function
.globl X0_handle_one_code__a7

X0_handle_one_code__a7:
	movl	$.Lpred18_2+0, %eax
	movl	$.Lpred18_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred18_1:
	movl	st+4, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred18_2:
	movl	$.Lpred18_4+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred18_3:
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+28
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+24
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+20
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757831__a7

.Lpred18_4:
	movl	$.Lpred18_6+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred18_5:
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+28
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+24
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+20
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757832__a7

.Lpred18_6:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred18_7:
	movl	$7, %eax
	call	Pl_Allocate
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -32(%edi)
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_handle_one_code__a7
.Lcont16:
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+24
	call	Pl_Deallocate
	jmp	X0_handle_one_code__a7
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757832__a7,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757832__a7:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+28
	movl	$.Lpred19_1+0, %eax
	movl	$8, %edx
	call	Pl_Create_Choice_Point
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_set_delete__a3
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757833__a4

.Lpred19_1:
	movl	$8, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757834__a6
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757834__a6,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757834__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred20_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_constraints__a4
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757835__a3
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_make_imposs__a4
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X0_update_tmp__a5

.Lpred20_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757835__a3,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757835__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_diff__a3

.Lpred21_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757833__a4,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757833__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred22_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	jmp	X0_update_tmp__a5

.Lpred22_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757831__a7,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757831__a7:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+28
	movl	$.Lpred23_1+0, %eax
	movl	$8, %edx
	call	Pl_Create_Choice_Point
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
	movl	pl_reg_bank+28, %eax
	movl	%eax, -40(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_set_elt__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757836__a4

.Lpred23_1:
	movl	$8, %eax
	call	Pl_Delete_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_constraints__a4
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_make_imposs__a4
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2468616E646C655F6F6E655F636F64652F375F2461757837__a5
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757837__a5,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757837__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred24_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_update_tmp__a5

.Lpred24_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6F6E655F636F64652F375F2461757836__a4,@function

X1_2468616E646C655F6F6E655F636F64652F375F2461757836__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred25_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	jmp	X0_update_tmp__a5

.Lpred25_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_constraints__a4,@function
.globl X0_constraints__a4

X0_constraints__a4:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_24636F6E73747261696E74732F345F2461757831__a4
	.p2align	4,,15
	.type	X1_24636F6E73747261696E74732F345F2461757831__a4,@function

X1_24636F6E73747261696E74732F345F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred27_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_find_aliases_of__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_diff__a3

.Lpred27_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_update_tmp__a5,@function
.globl X0_update_tmp__a5

X0_update_tmp__a5:
	movl	$.Lpred28_1+0, %eax
	movl	$.Lpred28_2+0, %edx
	movl	$.Lpred28_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred28_1:
	movl	$.Lpred28_3+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred28_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred28_3:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred28_4:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+40
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+44
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+36
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	movl	fn+12, %eax
	movl	pl_reg_bank+36, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+44, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+40, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X1_247570646174655F746D702F355F2461757831__a10
	.p2align	4,,15
	.type	X1_247570646174655F746D702F355F2461757831__a10,@function

X1_247570646174655F746D702F355F2461757831__a10:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+40
	movl	$.Lpred29_1+0, %eax
	movl	$11, %edx
	call	Pl_Create_Choice_Point
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -36(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+40, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_set_union__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_set_union__a3
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	movl	$11, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+36, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X0_update_tmp__a5
	.p2align	4,,15
	.type	X0_remove_tmp__a5,@function
.globl X0_remove_tmp__a5

X0_remove_tmp__a5:
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+36
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	movl	fn+12, %eax
	movl	pl_reg_bank+32, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2472656D6F76655F746D702F355F2461757831__a9
	.p2align	4,,15
	.type	X1_2472656D6F76655F746D702F355F2461757831__a9,@function

X1_2472656D6F76655F746D702F355F2461757831__a9:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+36
	movl	$.Lpred31_1+0, %eax
	movl	$10, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+36, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_1:
	movl	$10, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_remove_tmp__a5
	.p2align	4,,15
	.type	X0_make_imposs__a4,@function
.globl X0_make_imposs__a4

X0_make_imposs__a4:
	movl	$.Lpred32_1+0, %eax
	movl	$.Lpred32_2+0, %edx
	movl	$.Lpred32_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred32_1:
	movl	$.Lpred32_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred32_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_3:
	call	Pl_Delete_Choice_Point4

.Lpred32_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_246D616B655F696D706F73732F345F2461757831__a4
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_make_imposs__a4
	.p2align	4,,15
	.type	X1_246D616B655F696D706F73732F345F2461757831__a4,@function

X1_246D616B655F696D706F73732F345F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred33_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	jmp	X0_update_tmp__a5

.Lpred33_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_assign_lst_tmp__a2,@function
.globl X0_assign_lst_tmp__a2

X0_assign_lst_tmp__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_2461737369676E5F6C73745F746D702F325F2461757831__a3
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_assign_values__a3
	.p2align	4,,15
	.type	X1_2461737369676E5F6C73745F746D702F325F2461757831__a3,@function

X1_2461737369676E5F6C73745F746D702F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred35_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$-2147483637, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_assign_wishes__a2

.Lpred35_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_no_wish__a3
	.p2align	4,,15
	.type	X0_assign_wishes__a2,@function
.globl X0_assign_wishes__a2

X0_assign_wishes__a2:
	movl	$.Lpred36_1+0, %eax
	movl	$.Lpred36_2+0, %edx
	movl	$.Lpred36_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred36_1:
	movl	$.Lpred36_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred36_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred36_3:
	call	Pl_Delete_Choice_Point2

.Lpred36_4:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_collapse_tmps__a7
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_try_a_whish__a3
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_2461737369676E5F7769736865732F325F2461757831__a4
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_assign_wishes__a2
	.p2align	4,,15
	.type	X1_2461737369676E5F7769736865732F325F2461757831__a4,@function

X1_2461737369676E5F7769736865732F325F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred37_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred37_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_collapse_tmps__a7,@function
.globl X0_collapse_tmps__a7

X0_collapse_tmps__a7:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+28
	movl	$.Lpred38_2+0, %eax
	movl	$.Lpred38_3+0, %edx
	movl	$.Lpred38_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred38_1:
	movl	$.Lpred38_sub_0+0, %eax
	movl	$8, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred38_5

.Lpred38_sub_0:
	movl	$.Lpred38_sub_1+0, %eax
	movl	$8, %edx
	call	Pl_Update_Choice_Point
	jmp	.Lpred38_7

.Lpred38_sub_1:
	movl	$8, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred38_9

.Lpred38_2:
	movl	$.Lpred38_4+0, %eax
	movl	$8, %edx
	call	Pl_Create_Choice_Point

.Lpred38_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred38_4:
	movl	$.Lpred38_6+0, %eax
	movl	$8, %edx
	call	Pl_Update_Choice_Point

.Lpred38_5:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -44(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X1_24636F6C6C617073655F746D70732F375F2461757831__a3
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	call	Pl_Deallocate
	jmp	X0_collapse_tmps__a7

.Lpred38_6:
	movl	$.Lpred38_8+0, %eax
	movl	$8, %edx
	call	Pl_Update_Choice_Point

.Lpred38_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+32, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	call	Pl_Cut
	jmp	X0_collapse_tmps__a7

.Lpred38_8:
	movl	$8, %eax
	call	Pl_Delete_Choice_Point

.Lpred38_9:
	movl	$12, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_remove_tmp__a5
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_set_union__a3
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_set_union__a3
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-60(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	call	Pl_Deallocate
	jmp	X0_collapse_tmps__a7
	.p2align	4,,15
	.type	X1_24636F6C6C617073655F746D70732F375F2461757831__a3,@function

X1_24636F6C6C617073655F746D70732F375F2461757831__a3:
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred39_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_elt__a2
	.p2align	4,,15
	.type	X0_try_a_whish__a3,@function
.globl X0_try_a_whish__a3

X0_try_a_whish__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred40_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_set_diff__a3
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred40_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_no_wish__a3,@function
.globl X0_no_wish__a3

X0_no_wish__a3:
	movl	$.Lpred41_1+0, %eax
	movl	$.Lpred41_2+0, %edx
	movl	$.Lpred41_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred41_1:
	movl	$.Lpred41_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred41_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred41_3:
	call	Pl_Delete_Choice_Point3

.Lpred41_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_246E6F5F776973682F335F2461757831__a4
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_no_wish__a3
	.p2align	4,,15
	.type	X1_246E6F5F776973682F335F2461757831__a4,@function

X1_246E6F5F776973682F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred42_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_set_union__a3

.Lpred42_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_assign_values__a3,@function
.globl X0_assign_values__a3

X0_assign_values__a3:
	movl	$.Lpred43_1+0, %eax
	movl	$.Lpred43_2+0, %edx
	movl	$.Lpred43_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred43_1:
	movl	$.Lpred43_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred43_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred43_3:
	call	Pl_Delete_Choice_Point3

.Lpred43_4:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X0_sort__a2
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X0_find_hole__a3
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_2461737369676E5F76616C7565732F335F2461757831__a3
.Lcont40:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_assign_values__a3
	.p2align	4,,15
	.type	X1_2461737369676E5F76616C7565732F335F2461757831__a3,@function

X1_2461737369676E5F76616C7565732F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred44_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+16, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred44_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_find_hole__a3,@function
.globl X0_find_hole__a3

X0_find_hole__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred45_2+0, %eax
	movl	$.Lpred45_3+0, %edx
	movl	$.Lpred45_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred45_1:
	movl	$.Lpred45_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred45_5

.Lpred45_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred45_7

.Lpred45_2:
	movl	$.Lpred45_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred45_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred45_4:
	movl	$.Lpred45_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred45_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_find_hole__a3

.Lpred45_6:
	call	Pl_Delete_Choice_Point4

.Lpred45_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	jmp	X1_2466696E645F686F6C652F335F2461757831__a4
	.p2align	4,,15
	.type	X1_2466696E645F686F6C652F335F2461757831__a4,@function

X1_2466696E645F686F6C652F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred46_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+12, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred46_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X1_2466696E645F686F6C652F335F2461757832__a3
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_find_hole__a3
	.p2align	4,,15
	.type	X1_2466696E645F686F6C652F335F2461757832__a3,@function

X1_2466696E645F686F6C652F335F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred47_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred47_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_add__a3,@function
.globl X0_set_add__a3

X0_set_add__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred48_2+0, %eax
	movl	$.Lpred48_3+0, %edx
	movl	$.Lpred48_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred48_1:
	movl	$.Lpred48_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred48_5

.Lpred48_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred48_7

.Lpred48_2:
	movl	$.Lpred48_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred48_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred48_4:
	movl	$.Lpred48_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred48_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred48_6:
	call	Pl_Delete_Choice_Point4

.Lpred48_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_set_add__a3
	.p2align	4,,15
	.type	X0_set_delete__a3,@function
.globl X0_set_delete__a3

X0_set_delete__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred49_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred49_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred49_1:
	movl	$.Lpred49_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred49_3

.Lpred49_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred49_5

.Lpred49_2:
	movl	$.Lpred49_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred49_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred49_4:
	call	Pl_Delete_Choice_Point4

.Lpred49_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_set_delete__a3
	.p2align	4,,15
	.type	X0_set_elt__a2,@function
.globl X0_set_elt__a2

X0_set_elt__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred50_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred50_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred50_1:
	movl	$.Lpred50_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred50_3

.Lpred50_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred50_5

.Lpred50_2:
	movl	$.Lpred50_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred50_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred50_4:
	call	Pl_Delete_Choice_Point3

.Lpred50_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_elt__a2
	.p2align	4,,15
	.type	X0_set_inter__a3,@function
.globl X0_set_inter__a3

X0_set_inter__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred51_2+0, %eax
	movl	$.Lpred51_3+0, %edx
	movl	$.Lpred51_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred51_1:
	movl	$.Lpred51_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred51_5

.Lpred51_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred51_7

.Lpred51_2:
	movl	$.Lpred51_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred51_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred51_4:
	movl	$.Lpred51_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred51_5:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_set_elt__a2
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_inter__a3

.Lpred51_6:
	call	Pl_Delete_Choice_Point4

.Lpred51_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_inter__a3
	.p2align	4,,15
	.type	X0_set_union__a3,@function
.globl X0_set_union__a3

X0_set_union__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred52_2+0, %eax
	movl	$.Lpred52_3+0, %edx
	movl	$.Lpred52_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred52_1:
	movl	$.Lpred52_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred52_5

.Lpred52_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred52_7

.Lpred52_2:
	movl	$.Lpred52_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred52_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_4:
	movl	$.Lpred52_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred52_5:
	movl	$4, %eax
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
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_set_elt__a2
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_union__a3

.Lpred52_6:
	call	Pl_Delete_Choice_Point4

.Lpred52_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_set_union__a3
	.p2align	4,,15
	.type	X0_set_diff__a3,@function
.globl X0_set_diff__a3

X0_set_diff__a3:
	movl	$.Lpred53_1+0, %eax
	movl	$.Lpred53_2+0, %edx
	movl	$.Lpred53_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred53_1:
	movl	$.Lpred53_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred53_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred53_3:
	call	Pl_Delete_Choice_Point3

.Lpred53_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X1_247365745F646966662F335F2461757831__a4
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_diff__a3
	.p2align	4,,15
	.type	X1_247365745F646966662F335F2461757831__a4,@function

X1_247365745F646966662F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred54_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_set_elt__a2
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred54_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
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
	movl	%eax, at+48
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC58, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC35, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC45, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC59, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC59, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC57, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	at+4, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$98, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_allocate_registers__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+4, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_allocate_registers__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24616C6C6F636174655F7265676973746572732F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$116, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_aliases__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$118, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24616C69617365732F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_aliases1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$129, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24616C6961736573312F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+32, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred7_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+36, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred7_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+44, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$143, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_alias__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$145, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246164645F616C6961732F345F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$156, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_find_aliases_of__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$156, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2466696E645F616C69617365735F6F662F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$165, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_remove_aliases_of__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$167, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2472656D6F76655F616C69617365735F6F662F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$167, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2472656D6F76655F616C69617365735F6F662F335F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$167, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2472656D6F76655F616C69617365735F6F662F335F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$184, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_create_lst_tmp__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$194, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_handle_lst_code__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$203, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_handle_one_code__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+32, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+36, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred18_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+88, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$220, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757832__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$220, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757834__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$220, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757835__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$220, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757833__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$203, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757831__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$203, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757837__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$203, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6F6E655F636F64652F375F2461757836__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$247, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_constraints__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$247, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6E73747261696E74732F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$257, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_update_tmp__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$10, %edx
	movl	at+0, %ecx
	movl	$259, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247570646174655F746D702F355F2461757831__a10+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$273, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_remove_tmp__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$9, %edx
	movl	at+0, %ecx
	movl	$273, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2472656D6F76655F746D702F355F2461757831__a9+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$287, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_make_imposs__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$289, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D616B655F696D706F73732F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$301, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_assign_lst_tmp__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$301, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2461737369676E5F6C73745F746D702F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$312, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_assign_wishes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$314, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2461737369676E5F7769736865732F325F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$326, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_collapse_tmps__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$328, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6C6C617073655F746D70732F375F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$348, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_try_a_whish__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$356, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_no_wish__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$358, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F5F776973682F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$368, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_assign_values__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$370, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2461737369676E5F76616C7565732F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$382, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_find_hole__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$388, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2466696E645F686F6C652F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+200, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$388, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2466696E645F686F6C652F335F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$403, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_add__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$414, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_delete__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$424, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_elt__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$433, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_inter__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+220, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$445, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_union__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+224, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$457, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_diff__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+228, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$459, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F646966662F335F2461757831__a4+0, 8(%esp)
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
	.string	"$add_alias/4_$aux1"
.LC1:
	.string	"$aliases/3_$aux1"
.LC2:
	.string	"$aliases1/3_$aux1"
.LC3:
	.string	"$allocate_registers/2_$aux1"
.LC4:
	.string	"$assign_lst_tmp/2_$aux1"
.LC5:
	.string	"$assign_values/3_$aux1"
.LC6:
	.string	"$assign_wishes/2_$aux1"
.LC7:
	.string	"$collapse_tmps/7_$aux1"
.LC8:
	.string	"$constraints/4_$aux1"
.LC9:
	.string	"$find_aliases_of/3_$aux1"
.LC10:
	.string	"$find_hole/3_$aux1"
.LC11:
	.string	"$find_hole/3_$aux2"
.LC12:
	.string	"$handle_one_code/7_$aux1"
.LC13:
	.string	"$handle_one_code/7_$aux2"
.LC14:
	.string	"$handle_one_code/7_$aux3"
.LC15:
	.string	"$handle_one_code/7_$aux4"
.LC16:
	.string	"$handle_one_code/7_$aux5"
.LC17:
	.string	"$handle_one_code/7_$aux6"
.LC18:
	.string	"$handle_one_code/7_$aux7"
.LC19:
	.string	"$make_imposs/4_$aux1"
.LC20:
	.string	"$no_wish/3_$aux1"
.LC21:
	.string	"$remove_aliases_of/3_$aux1"
.LC22:
	.string	"$remove_aliases_of/3_$aux2"
.LC23:
	.string	"$remove_aliases_of/3_$aux3"
.LC24:
	.string	"$remove_tmp/5_$aux1"
.LC25:
	.string	"$set_diff/3_$aux1"
.LC26:
	.string	"$update_tmp/5_$aux1"
.LC27:
	.string	"/home/diaz/GP/src/Pl2Wam/reg_alloc.pl"
.LC28:
	.string	"add_alias"
.LC29:
	.string	"aliases"
.LC30:
	.string	"aliases1"
.LC31:
	.string	"allocate_registers"
.LC32:
	.string	"assign_lst_tmp"
.LC33:
	.string	"assign_values"
.LC34:
	.string	"assign_wishes"
.LC35:
	.string	"c"
.LC36:
	.string	"collapse_tmps"
.LC37:
	.string	"constraints"
.LC38:
	.string	"create_lst_tmp"
.LC39:
	.string	"find_aliases_of"
.LC40:
	.string	"find_hole"
.LC41:
	.string	"handle_lst_code"
.LC42:
	.string	"handle_one_code"
.LC43:
	.string	"make_imposs"
.LC44:
	.string	"no_wish"
.LC45:
	.string	"r"
.LC58:
	.string	"reg_opt"
.LC46:
	.string	"remove_aliases_of"
.LC47:
	.string	"remove_tmp"
.LC48:
	.string	"set_add"
.LC49:
	.string	"set_delete"
.LC50:
	.string	"set_diff"
.LC51:
	.string	"set_elt"
.LC52:
	.string	"set_inter"
.LC53:
	.string	"set_union"
.LC59:
	.string	"tmp"
.LC54:
	.string	"try_a_whish"
.LC55:
	.string	"update_tmp"
.LC56:
	.string	"user"
.LC57:
	.string	"w"
	.data	
	.align	4
	.local	at
	.comm	at,232,4
	.local	fn
	.comm	fn,20,4
	.local	st
	.comm	st,8,4
	.local	ta
	.comm	ta,4,4
	.section	.note.GNU-stack,"",@progbits
