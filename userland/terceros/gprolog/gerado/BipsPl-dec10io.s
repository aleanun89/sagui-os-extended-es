# BipsPl/dec10io.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/dec10io.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6465633130696F__a0,@function
.globl X1_247573655F6465633130696F__a0

X1_247573655F6465633130696F__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2466696E645F6578697374696E675F73747265616D__a3,@function
.globl X1_2466696E645F6578697374696E675F73747265616D__a3

X1_2466696E645F6578697374696E675F73747265616D__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred2_11+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred2_3+0, %eax
	movl	$.Lpred2_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred2_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred2_2:
	movl	$.Lpred2_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred2_4

.Lpred2_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred2_8

.Lpred2_3:
	movl	$.Lpred2_5+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred2_4:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_5:
	movl	$.Lpred2_7+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred2_6:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_7:
	movl	$.Lpred2_9+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred2_8:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_9:
	call	Pl_Delete_Choice_Point4

.Lpred2_10:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred2_11:
	movl	$.Lpred2_12+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$4, %eax
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
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_242466696E645F6578697374696E675F73747265616D2F335F2461757831__a3

.Lpred2_12:
	call	Pl_Delete_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_current_stream__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_stream_property__a2
	.p2align	4,,15
	.type	X1_242466696E645F6578697374696E675F73747265616D2F335F2461757831__a3,@function

X1_242466696E645F6578697374696E675F73747265616D2F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$4, %eax
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_current_stream__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_stream_property__a2
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_stream_property__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred3_1:
	call	Pl_Delete_Choice_Point4
	movl	$0, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont6:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_see__a1,@function
.globl X0_see__a1

X0_see__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Cut
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247365652F315F2461757831__a2
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_set_input__a1

.Lpred4_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0
	.p2align	4,,15
	.type	X1_247365652F315F2461757831__a2,@function

X1_247365652F315F2461757831__a2:
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3

.Lpred5_1:
	call	Pl_Delete_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_246F70656E__a4
.Lcont8:
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_seeing__a1,@function
.globl X0_seeing__a1

X0_seeing__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_current_input__a1
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_seen__a0,@function
.globl X0_seen__a0

X0_seen__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_current_input__a1
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_close__a1
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_247365656E2F305F2461757831__a1
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365656E2F305F2461757831__a1,@function

X1_247365656E2F305F2461757831__a1:
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3

.Lpred8_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_tell__a1,@function
.globl X0_tell__a1

X0_tell__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Cut
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_2474656C6C2F315F2461757831__a2
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_set_output__a1

.Lpred9_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0
	.p2align	4,,15
	.type	X1_2474656C6C2F315F2461757831__a2,@function

X1_2474656C6C2F315F2461757831__a2:
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3

.Lpred10_1:
	call	Pl_Delete_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_246F70656E__a4
.Lcont15:
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_telling__a1,@function
.globl X0_telling__a1

X0_telling__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_current_output__a1
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_told__a0,@function
.globl X0_told__a0

X0_told__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_current_output__a1
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_close__a1
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_24746F6C642F305F2461757831__a1
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24746F6C642F305F2461757831__a1,@function

X1_24746F6C642F305F2461757831__a1:
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3

.Lpred13_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_append__a1,@function
.globl X0_append__a1

X0_append__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+72, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Cut
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_24617070656E642F315F2461757831__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_set_output__a1

.Lpred14_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0
	.p2align	4,,15
	.type	X1_24617070656E642F315F2461757831__a2,@function

X1_24617070656E642F315F2461757831__a2:
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_2466696E645F6578697374696E675F73747265616D__a3

.Lpred15_1:
	call	Pl_Delete_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+72, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_246F70656E__a4
.Lcont22:
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_get0__a1,@function
.globl X0_get0__a1

X0_get0__a1:
	movl	at+80, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Code_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get__a1,@function
.globl X0_get__a1

X0_get__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+84, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_24636865636B5F696E5F6368617261637465725F636F6465__a1
.Lcont23:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Code_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246765742F315F2461757831__a2
	.p2align	4,,15
	.type	X1_246765742F315F2461757831__a2,@function

X1_246765742F315F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	at+92, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483517, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_get__a1

.Lpred18_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_put__a1,@function
.globl X0_put__a1

X0_put__a1:
	movl	at+96, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Put_Code_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_skip__a1,@function
.globl X0_skip__a1

X0_skip__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+100, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_24636865636B5F696E5F6368617261637465725F636F6465__a1
.Lcont24:
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont25:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Code_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636865636B5F696E5F6368617261637465725F636F6465__a1,@function
.globl X1_24636865636B5F696E5F6368617261637465725F636F6465__a1

X1_24636865636B5F696E5F6368617261637465725F636F6465__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred21_1:
	movl	$.Lpred21_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_2424636865636B5F696E5F6368617261637465725F636F64652F315F2461757831__a1

.Lpred21_2:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_2424636865636B5F696E5F6368617261637465725F636F64652F315F2461757831__a1,@function

X1_2424636865636B5F696E5F6368617261637465725F636F64652F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Load_Value
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Gte
	testl	%eax, %eax
	je	fail
	movl	at+92, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	$-2147482625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred22_1:
	call	Pl_Delete_Choice_Point2
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F726570726573656E746174696F6E__a1
	.p2align	4,,15
	.type	X0_tab__a1,@function
.globl X0_tab__a1

X0_tab__a1:
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+116, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_2461726974685F6576616C__a2
.Lcont26:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_for__a3
.Lcont27:
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_put_char__a1
.Lcont28:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred23_1:
	call	Pl_Delete_Choice_Point1
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
	movl	%eax, at+108
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC30, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC31, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC32, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC33, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC34, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC35, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+16, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	at+20, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	$.LC36, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC37, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC38, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC39, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6465633130696F__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2466696E645F6578697374696E675F73747265616D__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+16, %ecx
	movl	$.Lpred2_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+20, %ecx
	movl	$.Lpred2_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+24, %ecx
	movl	$.Lpred2_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$53, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242466696E645F6578697374696E675F73747265616D2F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_see__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365652F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_seeing__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$97, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_seen__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$97, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365656E2F305F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$107, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_tell__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$107, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2474656C6C2F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$123, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_telling__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$131, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_told__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$131, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24746F6C642F305F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$140, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_append__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$140, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24617070656E642F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$156, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get0__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_get__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765742F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$171, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_put__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$176, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_skip__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$186, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F696E5F6368617261637465725F636F6465__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$189, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636865636B5F696E5F6368617261637465725F636F64652F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$201, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_tab__a1+0, 8(%esp)
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
.LC30:
	.string	" "
.LC0:
	.string	"$$check_in_character_code/1_$aux1"
.LC1:
	.string	"$$find_existing_stream/3_$aux1"
.LC2:
	.string	"$append/1_$aux1"
.LC3:
	.string	"$check_in_character_code"
.LC37:
	.string	"$dec10_stream"
.LC4:
	.string	"$find_existing_stream"
.LC5:
	.string	"$get/1_$aux1"
.LC6:
	.string	"$see/1_$aux1"
.LC7:
	.string	"$seen/0_$aux1"
.LC38:
	.string	"$stream"
.LC8:
	.string	"$tell/1_$aux1"
.LC9:
	.string	"$told/0_$aux1"
.LC10:
	.string	"$use_dec10io"
.LC11:
	.string	"/home/diaz/GP/src/BipsPl/dec10io.pl"
.LC12:
	.string	"=<"
.LC13:
	.string	">="
.LC14:
	.string	"append"
.LC39:
	.string	"file_name"
.LC15:
	.string	"get"
.LC16:
	.string	"get0"
.LC31:
	.string	"in_character_code"
.LC32:
	.string	"input"
.LC33:
	.string	"integer"
.LC34:
	.string	"output"
.LC17:
	.string	"put"
.LC35:
	.string	"read"
.LC18:
	.string	"see"
.LC19:
	.string	"seeing"
.LC20:
	.string	"seen"
.LC21:
	.string	"skip"
.LC22:
	.string	"system"
.LC23:
	.string	"tab"
.LC24:
	.string	"tell"
.LC25:
	.string	"telling"
.LC26:
	.string	"told"
.LC27:
	.string	"user"
.LC28:
	.string	"user_input"
.LC29:
	.string	"user_output"
.LC36:
	.string	"write"
	.data	
	.align	4
	.local	at
	.comm	at,120,4
	.local	fn
	.comm	fn,12,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,44,4
	.section	.note.GNU-stack,"",@progbits
