# BipsPl/pretty.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/pretty.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F707265747479__a0,@function
.globl X1_247573655F707265747479__a0

X1_247573655F707265747479__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_portray_clause__a1,@function
.globl X0_portray_clause__a1

X0_portray_clause__a1:
	movl	$.Lpred2_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24706F72747261795F636C61757365__a3
.Lcont1:
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
	call	Pl_Portray_Clause_2
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred2_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_portray_clause__a2,@function
.globl X0_portray_clause__a2

X0_portray_clause__a2:
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_24706F72747261795F636C61757365__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Portray_Clause_3
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred3_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24706F72747261795F636C61757365__a3,@function
.globl X1_24706F72747261795F636C61757365__a3

X1_24706F72747261795F636C61757365__a3:
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont3:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_2424706F72747261795F636C617573652F335F2461757831__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_2424706F72747261795F636C617573652F335F2461757832__a1
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_name_singleton_vars__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_bind_variables__a2
.Lcont7:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_2424706F72747261795F636C617573652F335F2461757832__a1,@function

X1_2424706F72747261795F636C617573652F335F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_2424706F72747261795F636C617573652F335F2461757831__a1,@function

X1_2424706F72747261795F636C617573652F335F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred6_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_name_singleton_vars__a1,@function
.globl X0_name_singleton_vars__a1

X0_name_singleton_vars__a1:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Name_Singleton_Vars_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_name_query_vars__a2,@function
.globl X0_name_query_vars__a2

X0_name_query_vars__a2:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Name_Query_Vars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_bind_variables__a2,@function
.globl X0_bind_variables__a2

X0_bind_variables__a2:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E73__a4
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2462696E645F7661726961626C6573__a4
	.p2align	4,,15
	.type	X1_2462696E645F7661726961626C6573__a4,@function
.globl X1_2462696E645F7661726961626C6573__a4

X1_2462696E645F7661726961626C6573__a4:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Bind_Variables_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0,@function
.globl X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0

X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E73__a4,@function
.globl X1_246765745F62696E645F7661726961626C65735F6F7074696F6E73__a4

X1_246765745F62696E645F7661726961626C65735F6F7074696F6E73__a4:
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
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C697374__a1
.Lcont10:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1
.Lcont11:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1,@function
.globl X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1

X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred13_1+0, %eax
	movl	$.Lpred13_2+0, %edx
	movl	$.Lpred13_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred13_1:
	movl	$.Lpred13_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred13_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_3:
	call	Pl_Delete_Choice_Point2

.Lpred13_4:
	movl	$2, %eax
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
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7332__a1
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7332__a1,@function
.globl X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7332__a1

X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7332__a1:
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred14_1:
	movl	$.Lpred14_14+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred14_4+0, %eax
	movl	$.Lpred14_2+0, %edx
	movl	$.Lpred14_3+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred14_2:
	movl	st+0, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred14_3:
	movl	st+4, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred14_4:
	movl	$.Lpred14_6+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred14_5:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	jmp	*pl_reg_bank+1040

.Lpred14_6:
	movl	$.Lpred14_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred14_7:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_8:
	movl	$.Lpred14_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred14_9:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_10:
	movl	$.Lpred14_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred14_11:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred14_12:
	call	Pl_Delete_Choice_Point1

.Lpred14_13:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred14_14:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X0_numbervars__a1,@function
.globl X0_numbervars__a1

X0_numbervars__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2462696E645F7661726961626C6573__a4
	.p2align	4,,15
	.type	X0_numbervars__a3,@function
.globl X0_numbervars__a3

X0_numbervars__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2462696E645F7661726961626C6573__a4
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
	movl	%eax, at+28
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC21, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC22, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC23, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC24, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	at+68, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+64, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+12, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	$.LC25, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC11, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC12, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC16, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F707265747479__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_portray_clause__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_portray_clause__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706F72747261795F636C61757365__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424706F72747261795F636C617573652F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424706F72747261795F636C617573652F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$84, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_name_singleton_vars__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_name_query_vars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$100, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_bind_variables__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$107, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462696E645F7661726961626C6573__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$113, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F62696E645F7661726961626C65735F64656661756C7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$119, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F62696E645F7661726961626C65735F6F7074696F6E73__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$130, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$137, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F62696E645F7661726961626C65735F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+64, %ecx
	movl	$.Lpred14_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+68, %ecx
	movl	$.Lpred14_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+72, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$164, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_numbervars__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$170, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_numbervars__a3+0, 8(%esp)
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
	.string	"$$portray_clause/3_$aux1"
.LC1:
	.string	"$$portray_clause/3_$aux2"
.LC21:
	.string	"$bind_exclude"
.LC22:
	.string	"$bind_from"
.LC23:
	.string	"$bind_next"
.LC2:
	.string	"$bind_variables"
.LC3:
	.string	"$get_bind_variables_options"
.LC4:
	.string	"$get_bind_variables_options1"
.LC5:
	.string	"$get_bind_variables_options2"
.LC6:
	.string	"$portray_clause"
.LC7:
	.string	"$set_bind_variables_defaults"
.LC8:
	.string	"$use_pretty"
.LC9:
	.string	"/home/diaz/GP/src/BipsPl/pretty.pl"
.LC10:
	.string	"bind_variables"
.LC24:
	.string	"callable"
.LC11:
	.string	"exclude"
.LC12:
	.string	"from"
.LC13:
	.string	"name_query_vars"
.LC14:
	.string	"name_singleton_vars"
.LC15:
	.string	"namevars"
.LC16:
	.string	"next"
.LC17:
	.string	"numbervars"
.LC18:
	.string	"portray_clause"
.LC19:
	.string	"system"
.LC20:
	.string	"user"
.LC25:
	.string	"var_binding_option"
	.data	
	.align	4
	.local	at
	.comm	at,84,4
	.local	fn
	.comm	fn,12,4
	.local	st
	.comm	st,8,4
	.local	ta
	.comm	ta,32,4
	.section	.note.GNU-stack,"",@progbits
