# Pl2Wam/indexing.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/indexing.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_indexing__a2,@function
.globl X0_indexing__a2

X0_indexing__a2:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+12
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_indexing1__a4
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_cur_pred__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24696E646578696E672F325F2461757831__a4
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_allocate_labels__a3
	.p2align	4,,15
	.type	X1_24696E646578696E672F325F2461757831__a4,@function

X1_24696E646578696E672F325F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred2_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred2_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_indexing1__a4,@function
.globl X0_indexing1__a4

X0_indexing1__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_look_for_var__a5
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_mk_indexing__a6
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_look_for_var__a5,@function
.globl X0_look_for_var__a5

X0_look_for_var__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred4_2+0, %eax
	movl	$.Lpred4_3+0, %edx
	movl	$.Lpred4_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred4_1:
	movl	$.Lpred4_sub_0+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred4_5

.Lpred4_sub_0:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred4_7

.Lpred4_2:
	movl	$.Lpred4_4+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred4_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred4_4:
	movl	$.Lpred4_6+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred4_5:
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	ta+8, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_246C6F6F6B5F666F725F7661722F355F2461757831__a2

.Lpred4_6:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred4_7:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_look_for_var__a5
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246C6F6F6B5F666F725F7661722F355F2461757832__a2
	.p2align	4,,15
	.type	X1_246C6F6F6B5F666F725F7661722F355F2461757832__a2,@function

X1_246C6F6F6B5F666F725F7661722F355F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred5_6+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred5_2+0, %eax
	movl	$0, %edx
	movl	$.Lpred5_1+0, %ecx
	movl	$0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred5_1:
	call	Pl_Switch_On_Integer
	cmpl	$13, %eax
	je	.Lpred5_3
	cmpl	$14, %eax
	je	.Lpred5_5
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred5_2:
	movl	$.Lpred5_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred5_3:
	movl	$-2147483593, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$-2147483601, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_4:
	call	Pl_Delete_Choice_Point3

.Lpred5_5:
	movl	$-2147483589, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$-2147483597, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_6:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246C6F6F6B5F666F725F7661722F355F2461757831__a2,@function

X1_246C6F6F6B5F666F725F7661722F355F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$-2147483589, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point3
	movl	$-2147483593, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_mk_indexing__a6,@function
.globl X0_mk_indexing__a6

X0_mk_indexing__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred7_2+0, %eax
	movl	$0, %edx
	movl	$.Lpred7_1+0, %ecx
	movl	$0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred7_1:
	call	Pl_Switch_On_Integer
	cmpl	$12, %eax
	je	.Lpred7_5
	jg	.Lcont8
	cmpl	$2, %eax
	je	.Lpred7_11
	cmpl	$11, %eax
	je	.Lpred7_3
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lcont8:
	cmpl	$13, %eax
	je	.Lpred7_7
	cmpl	$14, %eax
	je	.Lpred7_9
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred7_2:
	movl	$.Lpred7_4+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred7_3:
	movl	$9, %eax
	call	Pl_Allocate
	movl	$-2147483601, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_246D6B5F696E646578696E672F365F2461757831__a3
.Lcont9:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_mk_indexing__a6
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_indexing1__a4
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred7_4:
	movl	$.Lpred7_6+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred7_5:
	movl	$6, %eax
	call	Pl_Allocate
	movl	$-2147483597, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_246D6B5F696E646578696E672F365F2461757832__a3
.Lcont12:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_mk_indexing__a6
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred7_6:
	movl	$.Lpred7_8+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred7_7:
	movl	$6, %eax
	call	Pl_Allocate
	movl	$-2147483593, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_246D6B5F696E646578696E672F365F2461757833__a3
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_indexing1__a4
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred7_8:
	movl	$.Lpred7_10+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred7_9:
	movl	$-2147483589, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246D6B5F696E646578696E672F365F2461757834__a3

.Lpred7_10:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred7_11:
	movl	$18, %eax
	call	Pl_Allocate
	movl	$-2147483637, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_246D6B5F696E646578696E672F365F2461757835__a3
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_246D6B5F696E646578696E672F365F2461757836__a8
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -52(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-64(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-68(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_split__a5
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-72(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_gen_switch__a5
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-76(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-72(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_gen_switch__a5
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-64(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-80(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-76(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_gen_list__a4
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-68(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-84(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-80(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_gen_switch__a5
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-84(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_gen_insts__a3
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757836__a8,@function

X1_246D6B5F696E646578696E672F365F2461757836__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred8_1+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_1:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757835__a3,@function

X1_246D6B5F696E646578696E672F365F2461757835__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757834__a3,@function

X1_246D6B5F696E646578696E672F365F2461757834__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757833__a3,@function

X1_246D6B5F696E646578696E672F365F2461757833__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757832__a3,@function

X1_246D6B5F696E646578696E672F365F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D6B5F696E646578696E672F365F2461757831__a3,@function

X1_246D6B5F696E646578696E672F365F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_split__a5,@function
.globl X0_split__a5

X0_split__a5:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+28
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+16
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+32
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_split1__a9
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_terminate_list__a2
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_terminate_list__a2
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_terminate_list__a2
	.p2align	4,,15
	.type	X0_split1__a9,@function
.globl X0_split1__a9

X0_split1__a9:
	movl	$.Lpred15_1+0, %eax
	movl	$.Lpred15_2+0, %edx
	movl	$.Lpred15_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred15_1:
	movl	$.Lpred15_3+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point

.Lpred15_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
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

.Lpred15_3:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point

.Lpred15_4:
	movl	$9, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+28
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+32
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+36
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_split2__a10
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+28
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+32
	call	Pl_Deallocate
	jmp	X0_split1__a9
	.p2align	4,,15
	.type	X0_split2__a10,@function
.globl X0_split2__a10

X0_split2__a10:
	movl	$.Lpred16_2+0, %eax
	movl	$.Lpred16_7+0, %edx
	movl	$.Lpred16_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred16_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred16_2:
	movl	$.Lpred16_4+0, %eax
	movl	$10, %edx
	call	Pl_Create_Choice_Point

.Lpred16_3:
	movl	pl_reg_bank+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_add_to_list__a4

.Lpred16_4:
	movl	$.Lpred16_6+0, %eax
	movl	$10, %edx
	call	Pl_Update_Choice_Point

.Lpred16_5:
	movl	pl_reg_bank+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_add_to_list__a4

.Lpred16_6:
	movl	$.Lpred16_8+0, %eax
	movl	$10, %edx
	call	Pl_Update_Choice_Point

.Lpred16_7:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
	movl	pl_reg_bank+32, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred16_8:
	movl	$10, %eax
	call	Pl_Delete_Choice_Point

.Lpred16_9:
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_add_to_list__a4
	.p2align	4,,15
	.type	X0_add_to_list__a4,@function
.globl X0_add_to_list__a4

X0_add_to_list__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred17_2+0, %eax
	movl	$.Lpred17_3+0, %edx
	movl	$.Lpred17_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred17_1:
	movl	$.Lpred17_sub_0+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred17_5

.Lpred17_sub_0:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred17_7

.Lpred17_2:
	movl	$.Lpred17_4+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred17_3:
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
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred17_4:
	movl	$.Lpred17_6+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred17_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred17_6:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred17_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	jmp	X0_add_to_list__a4
	.p2align	4,,15
	.type	X0_terminate_list__a2,@function
.globl X0_terminate_list__a2

X0_terminate_list__a2:
	movl	$.Lpred18_1+0, %eax
	movl	$.Lpred18_2+0, %edx
	movl	$.Lpred18_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred18_1:
	movl	$.Lpred18_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred18_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_3:
	call	Pl_Delete_Choice_Point2

.Lpred18_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+52, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_terminate_list__a2
	.p2align	4,,15
	.type	X0_gen_switch__a5,@function
.globl X0_gen_switch__a5

X0_gen_switch__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred19_5+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred19_1+0, %eax
	movl	$.Lpred19_2+0, %edx
	movl	$.Lpred19_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred19_1:
	movl	$.Lpred19_3+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred19_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred19_3:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred19_4:
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
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
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred19_5:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	fn+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_create_switch_list__a4
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_create_switch_list__a4,@function
.globl X0_create_switch_list__a4

X0_create_switch_list__a4:
	movl	$.Lpred20_1+0, %eax
	movl	$.Lpred20_2+0, %edx
	movl	$.Lpred20_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred20_1:
	movl	$.Lpred20_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred20_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred20_3:
	call	Pl_Delete_Choice_Point4

.Lpred20_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+56, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_gen_list__a4
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_create_switch_list__a4
	.p2align	4,,15
	.type	X0_gen_list__a4,@function
.globl X0_gen_list__a4

X0_gen_list__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred21_2+0, %eax
	movl	$.Lpred21_3+0, %edx
	movl	$.Lpred21_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred21_1:
	movl	$.Lpred21_sub_0+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred21_5

.Lpred21_sub_0:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred21_7

.Lpred21_2:
	movl	$.Lpred21_4+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred21_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred21_4:
	movl	$.Lpred21_6+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred21_5:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred21_6:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred21_7:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+12, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+60, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_gen_list1__a3
	.p2align	4,,15
	.type	X0_gen_list1__a3,@function
.globl X0_gen_list1__a3

X0_gen_list1__a3:
	movl	$.Lpred22_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred22_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred22_1:
	movl	$.Lpred22_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred22_3

.Lpred22_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred22_5

.Lpred22_2:
	movl	$.Lpred22_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred22_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_4:
	call	Pl_Delete_Choice_Point3

.Lpred22_5:
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+68, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_gen_list1__a3
	.p2align	4,,15
	.type	X0_gen_insts__a3,@function
.globl X0_gen_insts__a3

X0_gen_insts__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred23_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred23_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred23_1:
	movl	$.Lpred23_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred23_3

.Lpred23_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred23_5

.Lpred23_2:
	movl	$.Lpred23_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred23_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred23_4:
	call	Pl_Delete_Choice_Point4

.Lpred23_5:
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
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_gen_insts1__a3
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_gen_insts1__a3,@function
.globl X0_gen_insts1__a3

X0_gen_insts1__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred24_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred24_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred24_1:
	movl	$.Lpred24_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred24_3

.Lpred24_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred24_5

.Lpred24_2:
	movl	$.Lpred24_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred24_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred24_4:
	call	Pl_Delete_Choice_Point4

.Lpred24_5:
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
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_gen_insts1__a3
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_allocate_labels__a3,@function
.globl X0_allocate_labels__a3

X0_allocate_labels__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred25_7+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred25_1+0, %eax
	movl	$.Lpred25_2+0, %edx
	movl	$0, %ecx
	movl	$.Lpred25_4+0, 0(%esp)
	movl	$.Lpred25_6+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred25_1:
	movl	$.Lpred25_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred25_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred25_3:
	movl	$.Lpred25_5+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred25_4:
	movl	$3, %eax
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
	call	Pl_Cut
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_allocate_labels__a3
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_allocate_labels__a3

.Lpred25_5:
	call	Pl_Delete_Choice_Point4

.Lpred25_6:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
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

.Lpred25_7:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
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
	movl	%eax, at+12
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC30, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC31, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC32, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC33, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC34, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC35, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC36, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC37, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC38, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC39, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC40, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC41, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC42, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC42, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC12, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC43, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC44, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC21, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC45, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC46, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC47, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC48, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC27, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC49, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC50, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC51, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC52, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC53, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	at+4, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$134, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_indexing__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$134, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24696E646578696E672F325F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$147, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_indexing1__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$154, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_look_for_var__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$163, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F6F6B5F666F725F7661722F355F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$156, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F6F6B5F666F725F7661722F355F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$175, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_mk_indexing__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$206, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757836__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$206, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757835__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$200, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757834__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$192, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757833__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$184, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$175, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246D6B5F696E646578696E672F365F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$227, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_split__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$9, %edx
	movl	at+0, %ecx
	movl	$234, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_split1__a9+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$10, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_split2__a10+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+72, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+80, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred16_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+84, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$255, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_to_list__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$266, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_terminate_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$274, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_switch__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$295, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_create_switch_list__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$304, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_list__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$314, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_list1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$323, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_insts__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$332, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_insts1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$342, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_allocate_labels__a3+0, 8(%esp)
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
	.string	"$indexing/2_$aux1"
.LC1:
	.string	"$look_for_var/5_$aux1"
.LC2:
	.string	"$look_for_var/5_$aux2"
.LC3:
	.string	"$mk_indexing/6_$aux1"
.LC4:
	.string	"$mk_indexing/6_$aux2"
.LC5:
	.string	"$mk_indexing/6_$aux3"
.LC6:
	.string	"$mk_indexing/6_$aux4"
.LC7:
	.string	"$mk_indexing/6_$aux5"
.LC8:
	.string	"$mk_indexing/6_$aux6"
.LC40:
	.string	","
.LC41:
	.string	"/"
.LC9:
	.string	"/home/diaz/GP/src/Pl2Wam/indexing.pl"
.LC42:
	.string	"a"
.LC10:
	.string	"add_to_list"
.LC11:
	.string	"allocate_labels"
.LC12:
	.string	"atm"
.LC43:
	.string	"cl"
.LC13:
	.string	"create_switch_list"
.LC30:
	.string	"f"
.LC31:
	.string	"fail"
.LC14:
	.string	"gen_insts"
.LC15:
	.string	"gen_insts1"
.LC16:
	.string	"gen_list"
.LC17:
	.string	"gen_list1"
.LC18:
	.string	"gen_switch"
.LC44:
	.string	"get_current_choice"
.LC19:
	.string	"indexing"
.LC20:
	.string	"indexing1"
.LC21:
	.string	"int"
.LC45:
	.string	"label"
.LC22:
	.string	"look_for_var"
.LC32:
	.string	"lst"
.LC23:
	.string	"mk_indexing"
.LC33:
	.string	"need_cut_level"
.LC46:
	.string	"pragma_arity"
.LC47:
	.string	"retry"
.LC48:
	.string	"retry_me_else"
.LC24:
	.string	"split"
.LC25:
	.string	"split1"
.LC26:
	.string	"split2"
.LC27:
	.string	"stc"
.LC34:
	.string	"switch_on_atom"
.LC35:
	.string	"switch_on_integer"
.LC36:
	.string	"switch_on_structure"
.LC49:
	.string	"switch_on_term"
.LC37:
	.string	"t"
.LC28:
	.string	"terminate_list"
.LC50:
	.string	"trust"
.LC38:
	.string	"trust_me_else_fail"
.LC51:
	.string	"try"
.LC52:
	.string	"try_me_else"
.LC29:
	.string	"user"
.LC39:
	.string	"var"
.LC53:
	.string	"x"
	.data	
	.align	4
	.local	at
	.comm	at,120,4
	.local	fn
	.comm	fn,72,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,40,4
	.section	.note.GNU-stack,"",@progbits
