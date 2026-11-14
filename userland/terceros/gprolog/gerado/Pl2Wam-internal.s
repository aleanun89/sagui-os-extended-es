# Pl2Wam/internal.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/internal.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_internal_format__a6,@function
.globl X0_internal_format__a6

X0_internal_format__a6:
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
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_format_head__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_format_body__a4
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_classif_vars__a3
	.p2align	4,,15
	.type	X0_format_head__a3,@function
.globl X0_format_head__a3

X0_format_head__a3:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	jmp	X0_format_pred__a5
	.p2align	4,,15
	.type	X0_format_body__a4,@function
.globl X0_format_body__a4

X0_format_body__a4:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+28
	jmp	X0_format_body1__a8
	.p2align	4,,15
	.type	X0_format_body1__a8,@function
.globl X0_format_body1__a8

X0_format_body1__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred4_5+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred4_1+0, %eax
	movl	$.Lpred4_4+0, %edx
	movl	$.Lpred4_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred4_1:
	movl	$.Lpred4_3+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point

.Lpred4_2:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -32(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_format_body1__a8
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+28
	call	Pl_Deallocate
	jmp	X0_format_body1__a8

.Lpred4_3:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point

.Lpred4_4:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
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
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred4_5:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24666F726D61745F626F6479312F385F2461757831__a3
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_format_pred__a5
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_24666F726D61745F626F6479312F385F2461757832__a2
	.p2align	4,,15
	.type	X1_24666F726D61745F626F6479312F385F2461757832__a2,@function

X1_24666F726D61745F626F6479312F385F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666F726D61745F626F6479312F385F2461757831__a3,@function

X1_24666F726D61745F626F6479312F385F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_format_pred__a5,@function
.globl X0_format_pred__a5

X0_format_pred__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred7_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+28, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_format_pred__a5

.Lpred7_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -40(%edi)
	movl	fn+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_get_owner_module__a3
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_format_arg_lst__a4
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_24666F726D61745F707265642F355F2461757831__a4
	.p2align	4,,15
	.type	X1_24666F726D61745F707265642F355F2461757831__a4,@function

X1_24666F726D61745F707265642F355F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred8_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_24666F726D61745F707265642F355F2461757832__a3
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred8_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	ta+12, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666F726D61745F707265642F355F2461757832__a3,@function

X1_24666F726D61745F707265642F355F2461757832__a3:
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	X0_inline_predicate__a2

.Lpred9_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_not_dangerous_c_call__a1
	.p2align	4,,15
	.type	X0_format_arg_lst__a4,@function
.globl X0_format_arg_lst__a4

X0_format_arg_lst__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred10_1+0, %eax
	movl	$.Lpred10_2+0, %edx
	movl	$.Lpred10_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred10_1:
	movl	$.Lpred10_3+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred10_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_3:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred10_4:
	movl	$5, %eax
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
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_format_arg__a4
.Lcont9:
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
	jmp	X0_format_arg_lst__a4
	.p2align	4,,15
	.type	X0_format_arg__a4,@function
.globl X0_format_arg__a4

X0_format_arg__a4:
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_add_var_to_dico__a4

.Lpred11_1:
	movl	$.Lpred11_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_no_internal_transf__a2

.Lpred11_2:
	movl	$.Lpred11_3+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_3:
	movl	$.Lpred11_4+0, %eax
	call	Pl_Update_Choice_Point4
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
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_4:
	movl	$.Lpred11_5+0, %eax
	call	Pl_Update_Choice_Point4
	movl	fn+20, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_5:
	movl	$.Lpred11_6+0, %eax
	call	Pl_Update_Choice_Point4
	movl	fn+24, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Float
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_6:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+0, %edx
	movl	pl_reg_bank+20, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+20
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	jmp	X0_format_arg_lst__a4
	.p2align	4,,15
	.type	X0_no_internal_transf__a2,@function
.globl X0_no_internal_transf__a2

X0_no_internal_transf__a2:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+0, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_add_var_to_dico__a4,@function
.globl X0_add_var_to_dico__a4

X0_add_var_to_dico__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred13_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+32, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+36, %eax
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
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$.Lpred13_3+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred13_2+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred13_2:
	movl	$.Lpred13_sub_0+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred13_4

.Lpred13_sub_0:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred13_6

.Lpred13_3:
	movl	$.Lpred13_5+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred13_4:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+32, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+12, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X1_246164645F7661725F746F5F6469636F2F345F2461757831__a3

.Lpred13_5:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred13_6:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_add_var_to_dico__a4
	.p2align	4,,15
	.type	X1_246164645F7661725F746F5F6469636F2F345F2461757831__a3,@function

X1_246164645F7661725F746F5F6469636F2F345F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_classif_vars__a3,@function
.globl X0_classif_vars__a3

X0_classif_vars__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred15_2+0, %eax
	movl	$.Lpred15_3+0, %edx
	movl	$.Lpred15_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred15_1:
	movl	$.Lpred15_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred15_5

.Lpred15_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred15_7

.Lpred15_2:
	movl	$.Lpred15_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred15_3:
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

.Lpred15_4:
	movl	$.Lpred15_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred15_5:
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
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+32, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_24636C61737369665F766172732F335F2461757831__a2
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_classif_vars__a3

.Lpred15_6:
	call	Pl_Delete_Choice_Point4

.Lpred15_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	fn+32, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+40, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+4
	jmp	X0_classif_vars__a3
	.p2align	4,,15
	.type	X1_24636C61737369665F766172732F335F2461757831__a2,@function

X1_24636C61737369665F766172732F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	fn+44, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred16_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+44, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_inline_predicate__a2,@function
.globl X0_inline_predicate__a2

X0_inline_predicate__a2:
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	X0_inline_predicate__a3
	.p2align	4,,15
	.type	X0_inline_predicate__a3,@function
.globl X0_inline_predicate__a3

X0_inline_predicate__a3:
	movl	$.Lpred18_4+0, %eax
	movl	$.Lpred18_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred18_1:
	movl	st+0, %eax
	movl	$54, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred18_2:
	movl	$.Lpred18_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred18_97

.Lpred18_sub_0:
	movl	$.Lpred18_sub_1+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred18_101

.Lpred18_sub_1:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred18_103

.Lpred18_3:
	movl	$.Lpred18_sub_2+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred18_105

.Lpred18_sub_2:
	movl	$.Lpred18_sub_3+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred18_109

.Lpred18_sub_3:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred18_111

.Lpred18_4:
	movl	$.Lpred18_6+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred18_5:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_6:
	movl	$.Lpred18_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_7:
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_8:
	movl	$.Lpred18_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_9:
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_10:
	movl	$.Lpred18_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_11:
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_12:
	movl	$.Lpred18_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_13:
	movl	ta+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_14:
	movl	$.Lpred18_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_15:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_16:
	movl	$.Lpred18_18+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_17:
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_18:
	movl	$.Lpred18_20+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_19:
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_20:
	movl	$.Lpred18_22+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_21:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_22:
	movl	$.Lpred18_24+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_23:
	movl	ta+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_24:
	movl	$.Lpred18_26+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_25:
	movl	ta+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_26:
	movl	$.Lpred18_28+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_27:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_28:
	movl	$.Lpred18_30+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_29:
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_30:
	movl	$.Lpred18_32+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_31:
	movl	ta+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_32:
	movl	$.Lpred18_34+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_33:
	movl	ta+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_34:
	movl	$.Lpred18_36+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_35:
	movl	ta+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_36:
	movl	$.Lpred18_38+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_37:
	movl	ta+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_38:
	movl	$.Lpred18_40+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_39:
	movl	ta+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_40:
	movl	$.Lpred18_42+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_41:
	movl	ta+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_42:
	movl	$.Lpred18_44+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_43:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_44:
	movl	$.Lpred18_46+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_45:
	movl	ta+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_46:
	movl	$.Lpred18_48+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_47:
	movl	ta+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_48:
	movl	$.Lpred18_50+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_49:
	movl	ta+124, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_50:
	movl	$.Lpred18_52+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_51:
	movl	ta+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_52:
	movl	$.Lpred18_54+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_53:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_54:
	movl	$.Lpred18_56+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_55:
	movl	ta+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_56:
	movl	$.Lpred18_58+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_57:
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_58:
	movl	$.Lpred18_60+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_59:
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_60:
	movl	$.Lpred18_62+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_61:
	movl	ta+148, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_62:
	movl	$.Lpred18_64+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_63:
	movl	ta+152, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_64:
	movl	$.Lpred18_66+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_65:
	movl	ta+156, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_66:
	movl	$.Lpred18_68+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_67:
	movl	ta+160, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_68:
	movl	$.Lpred18_70+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_69:
	movl	ta+164, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_70:
	movl	$.Lpred18_72+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_71:
	movl	ta+168, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_72:
	movl	$.Lpred18_74+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_73:
	movl	ta+172, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_74:
	movl	$.Lpred18_76+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_75:
	movl	ta+176, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_76:
	movl	$.Lpred18_78+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_77:
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_78:
	movl	$.Lpred18_80+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_79:
	movl	ta+184, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_80:
	movl	$.Lpred18_82+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_81:
	movl	ta+188, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_82:
	movl	$.Lpred18_84+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_83:
	movl	ta+192, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_84:
	movl	$.Lpred18_86+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_85:
	movl	ta+196, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_86:
	movl	$.Lpred18_88+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_87:
	movl	ta+200, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_88:
	movl	$.Lpred18_90+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_89:
	movl	ta+204, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_90:
	movl	$.Lpred18_92+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_91:
	movl	ta+208, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_92:
	movl	$.Lpred18_94+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_93:
	movl	ta+212, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_94:
	movl	$.Lpred18_96+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_95:
	movl	ta+216, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_96:
	movl	$.Lpred18_98+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_97:
	movl	ta+220, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_98:
	movl	$.Lpred18_100+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_99:
	movl	ta+224, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_100:
	movl	$.Lpred18_102+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_101:
	movl	ta+220, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_102:
	movl	$.Lpred18_104+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_103:
	movl	ta+220, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_104:
	movl	$.Lpred18_106+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_105:
	movl	ta+228, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_106:
	movl	$.Lpred18_108+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_107:
	movl	ta+232, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_108:
	movl	$.Lpred18_110+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_109:
	movl	ta+228, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_110:
	movl	$.Lpred18_112+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_111:
	movl	ta+228, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_112:
	movl	$.Lpred18_114+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_113:
	movl	ta+236, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_114:
	movl	$.Lpred18_116+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_115:
	movl	ta+240, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_116:
	movl	$.Lpred18_118+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred18_117:
	movl	ta+244, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_118:
	call	Pl_Delete_Choice_Point3

.Lpred18_119:
	movl	ta+248, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
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
	movl	%eax, at+60
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC73, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	at+84, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	at+96, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+56
	movl	at+80, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+40
	movl	at+76, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+36
	movl	$.LC74, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	at+88, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	at+224, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+184
	movl	at+92, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+52
	movl	at+184, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+144
	movl	at+216, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+176
	movl	at+228, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+188
	movl	at+188, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+148
	movl	at+220, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+180
	movl	at+232, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+192
	movl	at+236, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+196
	movl	at+196, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+156
	movl	at+200, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+160
	movl	at+204, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+164
	movl	at+208, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+168
	movl	at+192, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+152
	movl	at+176, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+136
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+68
	movl	at+124, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+84
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	at+180, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	at+128, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+88
	movl	$.LC75, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+156, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+116
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+76
	movl	at+172, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	at+256, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+216
	movl	at+240, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+200
	movl	at+244, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+204
	movl	at+268, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+228
	movl	at+272, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+232
	movl	at+260, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+220
	movl	at+264, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+224
	movl	at+248, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+208
	movl	at+252, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+212
	movl	at+280, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+240
	movl	at+276, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+236
	movl	at+288, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+248
	movl	at+284, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+244
	movl	at+164, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+124
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+96
	movl	$.LC76, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+72
	movl	at+212, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+172
	movl	at+140, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+100
	movl	at+144, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+104
	movl	at+152, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+112
	movl	$.LC77, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC78, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+120
	movl	at+168, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+128
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	at+120, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+80
	movl	at+148, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+108
	movl	$.LC79, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC80, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	$.LC81, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC82, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC83, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC84, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC85, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC86, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC87, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC88, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC89, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC90, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC72, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC91, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC92, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	at+4, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$73, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_internal_format__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_head__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$88, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_body__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_body1__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726D61745F626F6479312F385F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726D61745F626F6479312F385F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$118, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_pred__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$122, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726D61745F707265642F355F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$122, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726D61745F707265642F355F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$140, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$149, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_format_arg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_no_internal_transf__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$196, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_var_to_dico__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$201, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246164645F7661725F746F5F6469636F2F345F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$218, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_classif_vars__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$221, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636C61737369665F766172732F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_inline_predicate__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$247, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_inline_predicate__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$54, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+76, %ecx
	movl	$.Lpred18_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+80, %ecx
	movl	$.Lpred18_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+84, %ecx
	movl	$.Lpred18_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+88, %ecx
	movl	$.Lpred18_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+92, %ecx
	movl	$.Lpred18_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+96, %ecx
	movl	$.Lpred18_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+100, %ecx
	movl	$.Lpred18_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+104, %ecx
	movl	$.Lpred18_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+108, %ecx
	movl	$.Lpred18_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+112, %ecx
	movl	$.Lpred18_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+116, %ecx
	movl	$.Lpred18_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+120, %ecx
	movl	$.Lpred18_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+124, %ecx
	movl	$.Lpred18_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+128, %ecx
	movl	$.Lpred18_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+132, %ecx
	movl	$.Lpred18_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+136, %ecx
	movl	$.Lpred18_35+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+140, %ecx
	movl	$.Lpred18_37+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+144, %ecx
	movl	$.Lpred18_39+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+148, %ecx
	movl	$.Lpred18_41+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+152, %ecx
	movl	$.Lpred18_43+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+156, %ecx
	movl	$.Lpred18_45+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+160, %ecx
	movl	$.Lpred18_47+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+164, %ecx
	movl	$.Lpred18_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+168, %ecx
	movl	$.Lpred18_51+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+172, %ecx
	movl	$.Lpred18_53+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+176, %ecx
	movl	$.Lpred18_55+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+180, %ecx
	movl	$.Lpred18_57+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+184, %ecx
	movl	$.Lpred18_59+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+188, %ecx
	movl	$.Lpred18_61+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+192, %ecx
	movl	$.Lpred18_63+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+196, %ecx
	movl	$.Lpred18_65+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+200, %ecx
	movl	$.Lpred18_67+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+204, %ecx
	movl	$.Lpred18_69+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+208, %ecx
	movl	$.Lpred18_71+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+212, %ecx
	movl	$.Lpred18_73+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+216, %ecx
	movl	$.Lpred18_75+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+220, %ecx
	movl	$.Lpred18_77+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+224, %ecx
	movl	$.Lpred18_79+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+228, %ecx
	movl	$.Lpred18_81+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+232, %ecx
	movl	$.Lpred18_83+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+236, %ecx
	movl	$.Lpred18_85+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+240, %ecx
	movl	$.Lpred18_87+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+244, %ecx
	movl	$.Lpred18_89+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+248, %ecx
	movl	$.Lpred18_91+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+252, %ecx
	movl	$.Lpred18_93+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+256, %ecx
	movl	$.Lpred18_95+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+260, %ecx
	movl	$.Lpred18_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+264, %ecx
	movl	$.Lpred18_99+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+268, %ecx
	movl	$.Lpred18_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+272, %ecx
	movl	$.Lpred18_107+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+276, %ecx
	movl	$.Lpred18_113+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+280, %ecx
	movl	$.Lpred18_115+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+284, %ecx
	movl	$.Lpred18_117+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$54, %edx
	movl	at+288, %ecx
	movl	$.Lpred18_119+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
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
	.string	"$add_var_to_dico/4_$aux1"
.LC73:
	.string	"$call_c"
.LC1:
	.string	"$classif_vars/3_$aux1"
.LC2:
	.string	"$cut"
.LC3:
	.string	"$foreign_call_c"
.LC4:
	.string	"$format_body1/8_$aux1"
.LC5:
	.string	"$format_body1/8_$aux2"
.LC6:
	.string	"$format_pred/5_$aux1"
.LC7:
	.string	"$format_pred/5_$aux2"
.LC8:
	.string	"$get_current_choice"
.LC9:
	.string	"$get_cut_level"
.LC74:
	.string	"$no_internal_tranf$"
.LC10:
	.string	"$soft_cut"
.LC82:
	.string	","
.LC83:
	.string	"/"
.LC11:
	.string	"/home/diaz/GP/src/Pl2Wam/internal.pl"
.LC84:
	.string	":"
.LC12:
	.string	"<"
.LC13:
	.string	"="
.LC14:
	.string	"=.."
.LC15:
	.string	"=:="
.LC16:
	.string	"=<"
.LC17:
	.string	"=="
.LC18:
	.string	"=\\="
.LC19:
	.string	">"
.LC20:
	.string	">="
.LC21:
	.string	"@<"
.LC22:
	.string	"@=<"
.LC23:
	.string	"@>"
.LC24:
	.string	"@>="
.LC25:
	.string	"\\=="
.LC26:
	.string	"add_var_to_dico"
.LC27:
	.string	"arg"
.LC85:
	.string	"atm"
.LC28:
	.string	"atom"
.LC29:
	.string	"atomic"
.LC30:
	.string	"callable"
.LC31:
	.string	"classif_vars"
.LC32:
	.string	"compare"
.LC33:
	.string	"compound"
.LC75:
	.string	"f"
.LC34:
	.string	"fd_var"
.LC35:
	.string	"float"
.LC86:
	.string	"flt"
.LC36:
	.string	"format_arg"
.LC37:
	.string	"format_arg_lst"
.LC38:
	.string	"format_body"
.LC39:
	.string	"format_body1"
.LC40:
	.string	"format_head"
.LC41:
	.string	"format_pred"
.LC42:
	.string	"functor"
.LC43:
	.string	"g_array_size"
.LC44:
	.string	"g_assign"
.LC45:
	.string	"g_assignb"
.LC46:
	.string	"g_dec"
.LC47:
	.string	"g_deco"
.LC48:
	.string	"g_inc"
.LC49:
	.string	"g_inco"
.LC50:
	.string	"g_link"
.LC51:
	.string	"g_read"
.LC52:
	.string	"g_reset_bit"
.LC53:
	.string	"g_set_bit"
.LC54:
	.string	"g_test_reset_bit"
.LC55:
	.string	"g_test_set_bit"
.LC56:
	.string	"generic_var"
.LC57:
	.string	"ground"
.LC76:
	.string	"inline"
.LC58:
	.string	"inline_predicate"
.LC87:
	.string	"int"
.LC59:
	.string	"integer"
.LC60:
	.string	"internal_format"
.LC61:
	.string	"is"
.LC62:
	.string	"is_list"
.LC63:
	.string	"list"
.LC64:
	.string	"list_or_partial_list"
.LC77:
	.string	"module"
.LC78:
	.string	"nil"
.LC65:
	.string	"no_internal_transf"
.LC66:
	.string	"non_fd_var"
.LC67:
	.string	"non_generic_var"
.LC68:
	.string	"nonvar"
.LC69:
	.string	"number"
.LC88:
	.string	"p"
.LC70:
	.string	"partial_list"
.LC89:
	.string	"stc"
.LC79:
	.string	"t"
.LC80:
	.string	"true"
.LC71:
	.string	"user"
.LC90:
	.string	"v"
.LC72:
	.string	"var"
.LC81:
	.string	"void"
.LC91:
	.string	"x"
.LC92:
	.string	"y"
	.data	
	.align	4
	.local	at
	.comm	at,292,4
	.local	fn
	.comm	fn,48,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,252,4
	.section	.note.GNU-stack,"",@progbits
