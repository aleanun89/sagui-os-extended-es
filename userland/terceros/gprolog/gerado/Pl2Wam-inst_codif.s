# Pl2Wam/inst_codif.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/inst_codif.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_alias_stop_instruction__a1,@function
.globl X0_alias_stop_instruction__a1

X0_alias_stop_instruction__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757831__a2
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757831__a2,@function

X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757831__a2:
	movl	$.Lpred2_2+0, %eax
	movl	$.Lpred2_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred2_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred2_2:
	movl	$.Lpred2_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred2_3:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_4:
	movl	$.Lpred2_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred2_5:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_6:
	call	Pl_Delete_Choice_Point2

.Lpred2_7:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	jmp	X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757832__a1
	.p2align	4,,15
	.type	X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757832__a1,@function

X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757832__a1:
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_memberchk__a2

.Lpred3_1:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_memberchk__a2
	.p2align	4,,15
	.type	X0_codification__a2,@function
.globl X0_codification__a2

X0_codification__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_codif__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_codif__a2,@function
.globl X0_codif__a2

X0_codif__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred5_80+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred5_8+0, %eax
	movl	$.Lpred5_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred5_1:
	movl	st+4, %eax
	movl	$30, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred5_2:
	movl	$.Lpred5_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_9

.Lpred5_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_13

.Lpred5_3:
	movl	$.Lpred5_sub_1+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_11

.Lpred5_sub_1:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_15

.Lpred5_4:
	movl	$.Lpred5_sub_2+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_29

.Lpred5_sub_2:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_35

.Lpred5_5:
	movl	$.Lpred5_sub_3+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_33

.Lpred5_sub_3:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_37

.Lpred5_6:
	movl	$.Lpred5_sub_4+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_53

.Lpred5_sub_4:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_55

.Lpred5_7:
	movl	$.Lpred5_sub_5+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred5_57

.Lpred5_sub_5:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred5_59

.Lpred5_8:
	movl	$.Lpred5_10+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred5_9:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
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
	jmp	*pl_reg_bank+1040

.Lpred5_10:
	movl	$.Lpred5_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_11:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
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

.Lpred5_12:
	movl	$.Lpred5_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_13:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_14:
	movl	$.Lpred5_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_15:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_16:
	movl	$.Lpred5_18+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_17:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_18:
	movl	$.Lpred5_20+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_19:
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_20:
	movl	$.Lpred5_22+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_21:
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_22:
	movl	$.Lpred5_24+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_23:
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_24:
	movl	$.Lpred5_26+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_25:
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_26:
	movl	$.Lpred5_28+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_27:
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_28:
	movl	$.Lpred5_30+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_29:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_30:
	movl	$.Lpred5_32+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_31:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_32:
	movl	$.Lpred5_34+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_33:
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
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
	jmp	*pl_reg_bank+1040

.Lpred5_34:
	movl	$.Lpred5_36+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_35:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_36:
	movl	$.Lpred5_38+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_37:
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_38:
	movl	$.Lpred5_40+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_39:
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_40:
	movl	$.Lpred5_42+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_41:
	movl	fn+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_42:
	movl	$.Lpred5_44+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_43:
	movl	fn+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_44:
	movl	$.Lpred5_46+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_45:
	movl	fn+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_46:
	movl	$.Lpred5_48+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_47:
	movl	fn+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_48:
	movl	$.Lpred5_50+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_49:
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_50:
	movl	$.Lpred5_52+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_51:
	movl	fn+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_52:
	movl	$.Lpred5_54+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_53:
	movl	fn+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_54:
	movl	$.Lpred5_56+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_55:
	movl	fn+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_56:
	movl	$.Lpred5_58+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_57:
	movl	fn+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_58:
	movl	$.Lpred5_60+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_59:
	movl	fn+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_60:
	movl	$.Lpred5_62+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_61:
	movl	fn+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_62:
	movl	$.Lpred5_64+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_63:
	movl	fn+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_64:
	movl	$.Lpred5_66+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_65:
	movl	fn+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_66:
	movl	$.Lpred5_68+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_67:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24636F6469662F325F2461757831__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_lst_r_for_call_execute__a3

.Lpred5_68:
	movl	$.Lpred5_70+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_69:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24636F6469662F325F2461757832__a2
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_lst_r_for_call_execute__a3

.Lpred5_70:
	movl	$.Lpred5_72+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_71:
	movl	fn+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_72:
	movl	$.Lpred5_74+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_73:
	movl	fn+124, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_74:
	movl	$.Lpred5_76+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_75:
	movl	fn+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
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

.Lpred5_76:
	movl	$.Lpred5_78+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred5_77:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_24636F6469662F325F2461757833__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_lst_rw_for_c_call__a3

.Lpred5_78:
	call	Pl_Delete_Choice_Point3

.Lpred5_79:
	movl	fn+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_lst_rw_for_foreign_c_call__a3

.Lpred5_80:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636F6469662F325F2461757833__a2,@function

X1_24636F6469662F325F2461757833__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_member__a2
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
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
	movl	-24(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636F6469662F325F2461757832__a2,@function

X1_24636F6469662F325F2461757832__a2:
	movl	$.Lpred7_2+0, %eax
	movl	$.Lpred7_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred7_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred7_2:
	movl	$.Lpred7_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred7_3:
	movl	fn+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred7_4:
	call	Pl_Delete_Choice_Point2

.Lpred7_5:
	movl	fn+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+140, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636F6469662F325F2461757831__a2,@function

X1_24636F6469662F325F2461757831__a2:
	movl	$.Lpred8_2+0, %eax
	movl	$.Lpred8_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred8_1:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred8_2:
	movl	$.Lpred8_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred8_3:
	movl	fn+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_4:
	call	Pl_Delete_Choice_Point2

.Lpred8_5:
	movl	fn+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+140, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_lst_r_for_call_execute__a3,@function
.globl X0_lst_r_for_call_execute__a3

X0_lst_r_for_call_execute__a3:
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	jmp	X0_lst_r_for_call_execute__a3
	.p2align	4,,15
	.type	X0_lst_rw_for_foreign_c_call__a3,@function
.globl X0_lst_rw_for_foreign_c_call__a3

X0_lst_rw_for_foreign_c_call__a3:
	movl	$.Lpred10_1+0, %eax
	movl	$.Lpred10_2+0, %edx
	movl	$.Lpred10_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred10_1:
	movl	$.Lpred10_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred10_2:
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

.Lpred10_3:
	call	Pl_Delete_Choice_Point3

.Lpred10_4:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+16, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_lst_rw_for_foreign_c_call__a3
	.p2align	4,,15
	.type	X0_lst_rw_for_c_call__a3,@function
.globl X0_lst_rw_for_c_call__a3

X0_lst_rw_for_c_call__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred11_2+0, %eax
	movl	$.Lpred11_3+0, %edx
	movl	$.Lpred11_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred11_1:
	movl	$.Lpred11_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred11_5

.Lpred11_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred11_7

.Lpred11_2:
	movl	$.Lpred11_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred11_3:
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

.Lpred11_4:
	movl	$.Lpred11_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred11_5:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_lst_rw_for_c_call__a3

.Lpred11_6:
	call	Pl_Delete_Choice_Point4

.Lpred11_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_lst_rw_for_c_call__a3
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
	movl	%eax, at+164
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+16, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+20, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC45, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC46, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC5, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+140
	movl	$.LC7, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+144
	movl	$.LC47, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC9, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+112
	movl	$.LC10, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+132
	movl	$.LC13, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+124
	movl	$.LC14, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+116
	movl	$.LC15, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+136
	movl	$.LC16, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC17, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+120
	movl	$.LC18, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC20, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC21, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC22, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC23, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC24, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC28, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+96
	movl	$.LC29, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC30, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC31, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC32, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC33, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	$.LC34, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC35, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC36, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC37, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC38, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC39, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC48, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC40, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+128
	movl	$.LC41, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+108
	movl	$.LC42, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+104
	movl	$.LC43, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+100
	movl	$.LC49, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC50, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC51, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	at+4, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_alias_stop_instruction__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+16, %ecx
	movl	$.Lpred2_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+20, %ecx
	movl	$.Lpred2_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+24, %ecx
	movl	$.Lpred2_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24616C6961735F73746F705F696E737472756374696F6E2F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$55, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_codification__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_codif__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$30, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+40, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+44, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+48, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_17+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+52, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_19+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+56, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_21+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+60, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_23+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_25+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+68, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_27+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+72, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_31+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+80, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+84, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_39+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+88, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_41+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+92, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_43+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+96, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_45+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_47+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+104, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_49+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+108, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_51+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+112, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+120, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_61+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+124, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_63+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_65+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+16, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_67+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+20, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_69+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+132, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_71+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+136, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_73+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+140, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_75+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+24, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred5_77+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$30, %edx
	movl	at+144, %ecx
	movl	$4, 0(%esp)
	movl	$.Lpred5_79+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$131, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6469662F325F2461757833__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$121, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6469662F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+156, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+160, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred7_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+164, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6469662F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+156, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred8_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+160, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred8_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+168, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$148, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_lst_r_for_call_execute__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$157, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_lst_rw_for_foreign_c_call__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$165, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_lst_rw_for_c_call__a3+0, 8(%esp)
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
	.string	"$alias_stop_instruction/1_$aux1"
.LC1:
	.string	"$alias_stop_instruction/1_$aux2"
.LC2:
	.string	"$codif/2_$aux1"
.LC3:
	.string	"$codif/2_$aux2"
.LC4:
	.string	"$codif/2_$aux3"
.LC5:
	.string	"/"
.LC6:
	.string	"/home/diaz/GP/src/Pl2Wam/inst_codif.pl"
.LC7:
	.string	":"
.LC8:
	.string	"alias_stop_instruction"
.LC47:
	.string	"c"
.LC9:
	.string	"call"
.LC10:
	.string	"call_c"
.LC11:
	.string	"codif"
.LC12:
	.string	"codification"
.LC13:
	.string	"cut"
.LC14:
	.string	"execute"
.LC15:
	.string	"foreign_call_c"
.LC16:
	.string	"get_atom"
.LC17:
	.string	"get_current_choice"
.LC18:
	.string	"get_float"
.LC19:
	.string	"get_integer"
.LC20:
	.string	"get_list"
.LC21:
	.string	"get_nil"
.LC22:
	.string	"get_structure"
.LC23:
	.string	"get_value"
.LC24:
	.string	"get_variable"
.LC45:
	.string	"jump"
.LC25:
	.string	"lst_r_for_call_execute"
.LC26:
	.string	"lst_rw_for_c_call"
.LC27:
	.string	"lst_rw_for_foreign_c_call"
.LC28:
	.string	"math_fast_load_value"
.LC29:
	.string	"math_load_value"
.LC30:
	.string	"put_atom"
.LC31:
	.string	"put_float"
.LC32:
	.string	"put_integer"
.LC33:
	.string	"put_list"
.LC34:
	.string	"put_nil"
.LC35:
	.string	"put_structure"
.LC36:
	.string	"put_unsafe_value"
.LC37:
	.string	"put_value"
.LC38:
	.string	"put_variable"
.LC39:
	.string	"put_void"
.LC48:
	.string	"r"
.LC40:
	.string	"soft_cut"
.LC41:
	.string	"unify_local_value"
.LC42:
	.string	"unify_value"
.LC43:
	.string	"unify_variable"
.LC46:
	.string	"use_x_args"
.LC44:
	.string	"user"
.LC49:
	.string	"w"
.LC50:
	.string	"x"
.LC51:
	.string	"y"
	.data	
	.align	4
	.local	at
	.comm	at,180,4
	.local	fn
	.comm	fn,148,4
	.local	st
	.comm	st,16,4
	.local	ta
	.comm	ta,20,4
	.section	.note.GNU-stack,"",@progbits
