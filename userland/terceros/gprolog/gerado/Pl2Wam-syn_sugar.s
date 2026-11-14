# Pl2Wam/syn_sugar.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/syn_sugar.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_syntactic_sugar_init_pred__a3,@function
.globl X0_syntactic_sugar_init_pred__a3

X0_syntactic_sugar_init_pred__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred1_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_246175785F6E616D65__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred1_1:
	call	Pl_Delete_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_2473796E7461637469635F73756761725F696E69745F707265642F335F2461757831__a4
.Lcont2:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473796E7461637469635F73756761725F696E69745F707265642F335F2461757831__a4,@function

X1_2473796E7461637469635F73756761725F696E69745F707265642F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred2_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_randomize__a0
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_term_hash__a2
.Lcont5:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483541, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Shl
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -32(%edi)
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_random__a3
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-36(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Fast_Dec
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_And
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred2_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$-2147483641, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_syntactic_sugar__a3,@function
.globl X0_syntactic_sugar__a3

X0_syntactic_sugar__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_2473796E7461637469635F73756761722F335F2461757831__a3
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_normalize_cuts__a3
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_normalize_alts__a3
	.p2align	4,,15
	.type	X1_2473796E7461637469635F73756761722F335F2461757831__a3,@function

X1_2473796E7461637469635F73756761722F335F2461757831__a3:
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	fn+0, %eax
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
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_normalize_cuts__a3,@function
.globl X0_normalize_cuts__a3

X0_normalize_cuts__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_246E6F726D616C697A655F637574732F335F2461757831__a4
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F637574732F335F2461757831__a4,@function

X1_246E6F726D616C697A655F637574732F335F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred6_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_normalize_cuts1__a4,@function
.globl X0_normalize_cuts1__a4

X0_normalize_cuts1__a4:
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_normalize_cuts1__a4

.Lpred7_1:
	movl	$.Lpred7_24+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$.Lpred7_4+0, %eax
	movl	$.Lpred7_5+0, %edx
	movl	$.Lpred7_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred7_2:
	movl	st+0, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred7_3:
	movl	$.Lpred7_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred7_7

.Lpred7_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred7_13

.Lpred7_4:
	movl	$.Lpred7_6+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred7_5:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred7_6:
	movl	$.Lpred7_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_7:
	movl	$11, %eax
	call	Pl_Allocate
	movl	fn+20, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_246E6F726D616C697A655F63757473312F345F2461757831__a7
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-56(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+24
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+28
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+32
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+36
	call	Pl_Deallocate
	jmp	X1_246E6F726D616C697A655F63757473312F345F2461757832__a10

.Lpred7_8:
	movl	$.Lpred7_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_9:
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
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
	movl	fn+16, %eax
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

.Lpred7_10:
	movl	$.Lpred7_12+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_11:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_normalize_cuts_in_if__a2
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_normalize_cuts1__a4

.Lpred7_12:
	movl	$.Lpred7_14+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_13:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246E6F726D616C697A655F63757473312F345F2461757833__a3

.Lpred7_14:
	movl	$.Lpred7_16+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_15:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_normalize_cuts1__a4

.Lpred7_16:
	movl	$.Lpred7_18+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_17:
	movl	$7, %eax
	call	Pl_Allocate
	movl	fn+36, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_check_module_name__a2
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_distrib_module_qualif__a3
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246E6F726D616C697A655F63757473312F345F2461757834__a3

.Lpred7_18:
	movl	$.Lpred7_20+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_19:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+40, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X0_cur_pred_without_aux__a2

.Lpred7_20:
	movl	$.Lpred7_22+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred7_21:
	movl	fn+44, %eax
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
	movl	fn+48, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X0_cur_pred_without_aux__a2

.Lpred7_22:
	call	Pl_Delete_Choice_Point4

.Lpred7_23:
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+56, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X0_cur_pred_without_aux__a2

.Lpred7_24:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246E6F726D616C697A655F63757473312F345F2461757835__a2
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F63757473312F345F2461757835__a2,@function

X1_246E6F726D616C697A655F63757473312F345F2461757835__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_meta_pred_rewriting__a2

.Lpred8_1:
	call	Pl_Delete_Choice_Point3
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F63757473312F345F2461757834__a3,@function

X1_246E6F726D616C697A655F63757473312F345F2461757834__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+36, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	jmp	X0_normalize_cuts1__a4

.Lpred9_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F63757473312F345F2461757833__a3,@function

X1_246E6F726D616C697A655F63757473312F345F2461757833__a3:
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	movl	$.Lpred10_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_2:
	call	Pl_Delete_Choice_Point3
	movl	fn+20, %eax
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
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F63757473312F345F2461757832__a10,@function

X1_246E6F726D616C697A655F63757473312F345F2461757832__a10:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+40
	movl	$.Lpred11_1+0, %eax
	movl	$11, %edx
	call	Pl_Create_Choice_Point
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+40, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_normalize_cuts1__a4

.Lpred11_1:
	movl	$11, %eax
	call	Pl_Delete_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+36, %eax
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_normalize_cuts_in_if__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F63757473312F345F2461757831__a7,@function

X1_246E6F726D616C697A655F63757473312F345F2461757831__a7:
	movl	$.Lpred12_2+0, %eax
	movl	$.Lpred12_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred12_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred12_2:
	movl	$.Lpred12_4+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred12_3:
	movl	fn+24, %eax
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
	movl	fn+20, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_4:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred12_5:
	movl	fn+32, %eax
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
	movl	fn+20, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_normalize_cuts_in_if__a2,@function
.globl X0_normalize_cuts_in_if__a2

X0_normalize_cuts_in_if__a2:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_normalize_cuts1__a4
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_246E6F726D616C697A655F637574735F696E5F69662F325F2461757831__a4
	.p2align	4,,15
	.type	X1_246E6F726D616C697A655F637574735F696E5F69662F325F2461757831__a4,@function

X1_246E6F726D616C697A655F637574735F696E5F69662F325F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred14_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_distrib_module_qualif__a3,@function
.globl X0_distrib_module_qualif__a3

X0_distrib_module_qualif__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred15_12+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred15_2+0, %eax
	movl	$.Lpred15_11+0, %edx
	movl	$.Lpred15_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred15_1:
	movl	st+8, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred15_2:
	movl	$.Lpred15_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred15_3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_distrib_module_qualif__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_distrib_module_qualif__a3

.Lpred15_4:
	movl	$.Lpred15_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred15_5:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_distrib_module_qualif__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_distrib_module_qualif__a3

.Lpred15_6:
	movl	$.Lpred15_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred15_7:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_distrib_module_qualif__a3
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_distrib_module_qualif__a3

.Lpred15_8:
	movl	$.Lpred15_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred15_9:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_check_module_name__a2
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_distrib_module_qualif__a3

.Lpred15_10:
	call	Pl_Delete_Choice_Point4

.Lpred15_11:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred15_12:
	call	Pl_Delete_Choice_Point4
	movl	fn+36, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_distrib_module_qualif_goal__a3,@function
.globl X0_distrib_module_qualif_goal__a3

X0_distrib_module_qualif_goal__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_check_module_name__a2

.Lpred16_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+36, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_normalize_alts__a3,@function
.globl X0_normalize_alts__a3

X0_normalize_alts__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+24, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Assign
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Assign
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_normalize_alts1__a3
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_normalize_alts1__a3,@function
.globl X0_normalize_alts1__a3

X0_normalize_alts1__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	fn+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_1:
	movl	$.Lpred18_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$4, %eax
	call	Pl_Allocate
	movl	fn+4, %eax
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
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_normalize_alts1__a3
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_normalize_alts1__a3

.Lpred18_2:
	movl	$.Lpred18_3+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_lst_var__a3
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_lst_var__a3
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_set_inter__a3
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_length__a2
.Lcont33:
	movl	ta+40, %eax
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
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_init_aux_pred_name__a4
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_linearize__a4
.Lcont35:
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_asserta__a1

.Lpred18_3:
	call	Pl_Delete_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_init_aux_pred_name__a4,@function
.globl X0_init_aux_pred_name__a4

X0_init_aux_pred_name__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$5, %eax
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
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483541, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Fct_Fast_Shl
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Fast_Dec
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_And
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_246D616B655F6175785F6E616D65__a4
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_24696E69745F6175785F707265645F6E616D652F345F2461757831__a4
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24696E69745F6175785F707265645F6E616D652F345F2461757831__a4,@function

X1_24696E69745F6175785F707265645F6E616D652F345F2461757831__a4:
	movl	$.Lpred20_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont39:
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_pred_info__a3

.Lpred20_1:
	movl	$.Lpred20_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont40:
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_pred_info__a3

.Lpred20_2:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_linearize__a4,@function
.globl X0_linearize__a4

X0_linearize__a4:
	jmp	X1_246C696E656172697A652F345F2461757831__a4
	.p2align	4,,15
	.type	X1_246C696E656172697A652F345F2461757831__a4,@function

X1_246C696E656172697A652F345F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred22_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_linearize__a4
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_linearize1__a4
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_append__a3

.Lpred22_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	X0_linearize1__a4
	.p2align	4,,15
	.type	X0_linearize1__a4,@function
.globl X0_linearize1__a4

X0_linearize1__a4:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_copy_term__a2
	.p2align	4,,15
	.type	X0_lst_var__a3,@function
.globl X0_lst_var__a3

X0_lst_var__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred24_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_add__a3

.Lpred24_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+20, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_lst_var_args__a5
	.p2align	4,,15
	.type	X0_lst_var_args__a5,@function
.globl X0_lst_var_args__a5

X0_lst_var_args__a5:
	jmp	X1_246C73745F7661725F617267732F355F2461757831__a5
	.p2align	4,,15
	.type	X1_246C73745F7661725F617267732F355F2461757831__a5,@function

X1_246C73745F7661725F617267732F355F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred26_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-20(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_lst_var__a3
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X0_lst_var_args__a5

.Lpred26_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_pred_rewriting__a2,@function
.globl X0_pred_rewriting__a2

X0_pred_rewriting__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred27_6+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred27_2+0, %eax
	movl	$.Lpred27_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred27_1:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred27_2:
	movl	$.Lpred27_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred27_3:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_test_c_call_allowed__a1
.Lcont44:
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_pred_rewriting__a2

.Lpred27_4:
	call	Pl_Delete_Choice_Point3

.Lpred27_5:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X1_24707265645F726577726974696E672F325F2461757831__a3
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_pred_rewriting__a2

.Lpred27_6:
	movl	$.Lpred27_7+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X1_24707265645F726577726974696E672F325F2461757832__a1
.Lcont46:
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_7:
	movl	$.Lpred27_21+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$.Lpred27_9+0, %eax
	movl	$.Lpred27_8+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred27_8:
	movl	st+16, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred27_9:
	movl	$.Lpred27_11+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred27_10:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+88, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont48:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+84, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont49:
	movl	fn+92, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont50:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_11:
	movl	$.Lpred27_13+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred27_12:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+96, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+84, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont52:
	movl	fn+92, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X0_pred_rewriting__a2
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_13:
	movl	$.Lpred27_15+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred27_14:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+80, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+92, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X0_test_c_call_allowed__a1
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_no_internal_transf__a2

.Lpred27_15:
	movl	$.Lpred27_17+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred27_16:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+92, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X0_test_c_call_allowed__a1
.Lcont55:
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_pred_rewriting__a2

.Lpred27_17:
	movl	$.Lpred27_19+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred27_18:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+96, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_test_c_call_allowed__a1
.Lcont56:
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_pred_rewriting__a2

.Lpred27_19:
	call	Pl_Delete_Choice_Point3

.Lpred27_20:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_test_c_call_allowed__a1
.Lcont57:
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+104, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_pred_rewriting__a2

.Lpred27_21:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24707265645F726577726974696E672F325F2461757832__a1,@function

X1_24707265645F726577726974696E672F325F2461757832__a1:
	movl	$.Lpred28_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred28_1:
	call	Pl_Delete_Choice_Point1
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_math_cmp_functor_name__a2
	.p2align	4,,15
	.type	X1_24707265645F726577726974696E672F325F2461757831__a3,@function

X1_24707265645F726577726974696E672F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred29_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+80, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+100, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+116, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_test_c_call_allowed__a1,@function
.globl X0_test_c_call_allowed__a1

X0_test_c_call_allowed__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred30_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred30_1:
	call	Pl_Delete_Choice_Point2
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+116, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X0_not_dangerous_c_call__a1,@function
.globl X0_not_dangerous_c_call__a1

X0_not_dangerous_c_call__a1:
	movl	$.Lpred31_1+0, %eax
	movl	$.Lpred31_2+0, %edx
	movl	$.Lpred31_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred31_1:
	movl	$.Lpred31_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred31_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_3:
	call	Pl_Delete_Choice_Point1

.Lpred31_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	X0_not_dangerous_c_call__a1
	.p2align	4,,15
	.type	X0_add_wrapper_to_dyn_clause__a4,@function
.globl X0_add_wrapper_to_dyn_clause__a4

X0_add_wrapper_to_dyn_clause__a4:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+68, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X0_init_aux_pred_name__a4
.Lcont58:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X1_246164645F777261707065725F746F5F64796E5F636C617573652F345F2461757831__a3
.Lcont59:
	movl	fn+68, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X1_246164645F777261707065725F746F5F64796E5F636C617573652F345F2461757831__a3,@function

X1_246164645F777261707065725F746F5F64796E5F636C617573652F345F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred33_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X0_head_wrapper__a3
.Lcont60:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred33_1:
	call	Pl_Delete_Choice_Point4
	jmp	X0_head_wrapper__a3
	.p2align	4,,15
	.type	X0_head_wrapper__a3,@function
.globl X0_head_wrapper__a3

X0_head_wrapper__a3:
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+16
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_meta_pred_rewriting__a2,@function
.globl X0_meta_pred_rewriting__a2

X0_meta_pred_rewriting__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred35_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+120, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont61:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X0_meta_pred_rewrite_args__a5

.Lpred35_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_meta_pred_rewrite_args__a5,@function
.globl X0_meta_pred_rewrite_args__a5

X0_meta_pred_rewrite_args__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred36_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$5, %eax
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
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-20(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	movl	pl_reg_bank+20, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X0_meta_pred_rewrite_arg__a3
.Lcont62:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X0_meta_pred_rewrite_args__a5

.Lpred36_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_meta_pred_rewrite_arg__a3,@function
.globl X0_meta_pred_rewrite_arg__a3

X0_meta_pred_rewrite_arg__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred37_1:
	movl	$.Lpred37_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	ta+124, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_meta_pred_rewrite_arg__a3

.Lpred37_2:
	movl	$.Lpred37_3+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+124, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_get_module_of_cur_pred__a1

.Lpred37_3:
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
	movl	%eax, at+192
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC53, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	at+168, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+100
	movl	at+164, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+96
	movl	at+52, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+124
	movl	at+36, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	$.LC54, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC55, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+60
	movl	$.LC56, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC57, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC58, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	$.LC59, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC60, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	$.LC61, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC62, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC63, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	at+148, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	$.LC64, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC65, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC66, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	$.LC67, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	$.LC68, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	$.LC69, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC70, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC71, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	at+144, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+80
	movl	$.LC72, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+156, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+84
	movl	$.LC73, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC74, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC75, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC76, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC77, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC1, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+100
	movl	$.LC1, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC2, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+108
	movl	$.LC3, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+104
	movl	$.LC78, %eax
	movl	$6, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC79, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC80, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC81, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC82, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+124
	movl	$.LC83, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC84, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC18, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC85, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC86, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC87, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC22, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC23, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC88, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+116
	movl	$.LC89, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+112
	movl	$.LC90, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC91, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC26, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC29, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC92, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+120
	movl	$.LC46, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	$.LC49, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC49, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+96
	movl	$.LC51, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	at+4, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_syntactic_sugar_init_pred__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473796E7461637469635F73756761725F696E69745F707265642F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_syntactic_sugar__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473796E7461637469635F73756761722F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$148, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_normalize_cuts__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$148, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F637574732F335F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$159, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_normalize_cuts1__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+36, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+44, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+48, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_15+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+52, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_17+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+56, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred7_19+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+60, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred7_21+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred7_23+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+68, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$226, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F63757473312F345F2461757835__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$206, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F63757473312F345F2461757834__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$192, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F63757473312F345F2461757833__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$10, %edx
	movl	at+0, %ecx
	movl	$165, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F63757473312F345F2461757832__a10+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$165, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F63757473312F345F2461757831__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred12_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+44, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred12_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_normalize_cuts_in_if__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246E6F726D616C697A655F637574735F696E5F69662F325F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$252, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_distrib_module_qualif__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+36, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+48, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+52, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+100, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$277, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_distrib_module_qualif_goal__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_normalize_alts__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$293, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_normalize_alts1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$320, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_init_aux_pred_name__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$320, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24696E69745F6175785F707265645F6E616D652F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_linearize__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C696E656172697A652F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$348, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_linearize1__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$354, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_lst_var__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$364, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_lst_var_args__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$364, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C73745F7661725F617267732F355F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$378, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_pred_rewriting__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+156, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred27_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+156, %ecx
	movl	$4, 0(%esp)
	movl	$.Lpred27_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+160, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred27_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+160, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred27_16+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+164, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred27_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+168, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred27_20+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+148, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred27_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+152, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred27_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+172, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$391, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24707265645F726577726974696E672F325F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$382, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24707265645F726577726974696E672F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$451, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_test_c_call_allowed__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$461, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_not_dangerous_c_call__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$471, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_wrapper_to_dyn_clause__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$471, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246164645F777261707065725F746F5F64796E5F636C617573652F345F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$481, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_head_wrapper__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+200, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$489, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_meta_pred_rewriting__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$501, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_meta_pred_rewrite_args__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$515, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_meta_pred_rewrite_arg__a3+0, 8(%esp)
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
.LC53:
	.string	"!"
.LC0:
	.string	"$add_wrapper_to_dyn_clause/4_$aux1"
.LC77:
	.string	"$call"
.LC1:
	.string	"$call_c"
.LC2:
	.string	"$call_c_jump"
.LC3:
	.string	"$call_c_test"
.LC78:
	.string	"$catch"
.LC79:
	.string	"$cut"
.LC80:
	.string	"$get_current_choice"
.LC81:
	.string	"$get_cut_level"
.LC4:
	.string	"$init_aux_pred_name/4_$aux1"
.LC5:
	.string	"$linearize/4_$aux1"
.LC6:
	.string	"$lst_var_args/5_$aux1"
.LC82:
	.string	"$mt"
.LC7:
	.string	"$normalize_cuts/3_$aux1"
.LC8:
	.string	"$normalize_cuts1/4_$aux1"
.LC9:
	.string	"$normalize_cuts1/4_$aux2"
.LC10:
	.string	"$normalize_cuts1/4_$aux3"
.LC11:
	.string	"$normalize_cuts1/4_$aux4"
.LC12:
	.string	"$normalize_cuts1/4_$aux5"
.LC13:
	.string	"$normalize_cuts_in_if/2_$aux1"
.LC14:
	.string	"$pred_rewriting/2_$aux1"
.LC15:
	.string	"$pred_rewriting/2_$aux2"
.LC83:
	.string	"$soft_cut"
.LC16:
	.string	"$syntactic_sugar/3_$aux1"
.LC17:
	.string	"$syntactic_sugar_init_pred/3_$aux1"
.LC84:
	.string	"$throw"
.LC18:
	.string	"*->"
.LC85:
	.string	"+"
.LC19:
	.string	","
.LC20:
	.string	"->"
.LC86:
	.string	"/"
.LC21:
	.string	"/home/diaz/GP/src/Pl2Wam/syn_sugar.pl"
.LC22:
	.string	":"
.LC87:
	.string	":-"
.LC23:
	.string	";"
.LC88:
	.string	"Pl_Set_Bip_Name_2"
.LC89:
	.string	"Pl_Set_Bip_Name_Untagged_2"
.LC90:
	.string	"Pl_Un_Integer_Check"
.LC24:
	.string	"add_wrapper_to_dyn_clause"
.LC54:
	.string	"aux"
.LC55:
	.string	"bfd"
.LC56:
	.string	"body goal is not callable (~q)"
.LC57:
	.string	"boolean"
.LC58:
	.string	"bpl"
.LC91:
	.string	"buff_aux_pred"
.LC59:
	.string	"by_value"
.LC25:
	.string	"call"
.LC60:
	.string	"call_c"
.LC26:
	.string	"catch"
.LC27:
	.string	"distrib_module_qualif"
.LC28:
	.string	"distrib_module_qualif_goal"
.LC61:
	.string	"f"
.LC62:
	.string	"fail"
.LC63:
	.string	"fast_math"
.LC29:
	.string	"fd_tell"
.LC64:
	.string	"head_arity"
.LC65:
	.string	"head_functor"
.LC30:
	.string	"head_wrapper"
.LC31:
	.string	"init_aux_pred_name"
.LC66:
	.string	"inline"
.LC67:
	.string	"is"
.LC68:
	.string	"jump"
.LC32:
	.string	"linearize"
.LC33:
	.string	"linearize1"
.LC34:
	.string	"lst_var"
.LC35:
	.string	"lst_var_args"
.LC92:
	.string	"meta_pred"
.LC36:
	.string	"meta_pred_rewrite_arg"
.LC37:
	.string	"meta_pred_rewrite_args"
.LC38:
	.string	"meta_pred_rewriting"
.LC69:
	.string	"multi"
.LC70:
	.string	"native_code"
.LC39:
	.string	"normalize_alts"
.LC40:
	.string	"normalize_alts1"
.LC41:
	.string	"normalize_cuts"
.LC42:
	.string	"normalize_cuts1"
.LC43:
	.string	"normalize_cuts_in_if"
.LC44:
	.string	"not_dangerous_c_call"
.LC71:
	.string	"optim_fail"
.LC45:
	.string	"pred_rewriting"
.LC46:
	.string	"set_bip_name"
.LC47:
	.string	"syntactic_sugar"
.LC48:
	.string	"syntactic_sugar_init_pred"
.LC72:
	.string	"t"
.LC49:
	.string	"term_hash"
.LC50:
	.string	"test_c_call_allowed"
.LC51:
	.string	"throw"
.LC73:
	.string	"true"
.LC74:
	.string	"use_x_regs"
.LC52:
	.string	"user"
.LC75:
	.string	"where"
.LC76:
	.string	"~q not allowed in this mode"
	.data	
	.align	4
	.local	at
	.comm	at,212,4
	.local	fn
	.comm	fn,128,4
	.local	st
	.comm	st,20,4
	.local	ta
	.comm	ta,128,4
	.section	.note.GNU-stack,"",@progbits
