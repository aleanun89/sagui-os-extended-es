# Pl2Wam/read_file.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/read_file.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_read_file_init__a1,@function
.globl X0_read_file_init__a1

X0_read_file_init__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont1:
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont2:
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont3:
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont4:
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont5:
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont6:
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont7:
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$2, %eax
	call	Pl_Unify_Void
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont8:
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont9:
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont10:
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont11:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_open_new_prolog_file__a1
	.p2align	4,,15
	.type	X0_read_file_term__a2,@function
.globl X0_read_file_term__a2

X0_read_file_term__a2:
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_file_error_nb__a1,@function
.globl X0_read_file_error_nb__a1

X0_read_file_error_nb__a1:
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_new_prolog_file__a1,@function
.globl X0_open_new_prolog_file__a1

X0_open_new_prolog_file__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_prolog_file_name__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_open_new_prolog_file1__a4
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+44, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757831__a1
	.p2align	4,,15
	.type	X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757831__a1,@function

X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757831__a1:
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_peek_char__a2
.Lcont14:
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_get_char__a2
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757832__a1

.Lpred5_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757832__a1,@function

X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757832__a1:
	movl	$.Lpred6_2+0, %eax
	movl	$.Lpred6_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred6_1:
	movl	st+0, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred6_2:
	movl	$.Lpred6_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred6_3:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_4:
	call	Pl_Delete_Choice_Point1

.Lpred6_5:
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_new_prolog_file1__a4,@function
.globl X0_open_new_prolog_file1__a4

X0_open_new_prolog_file1__a4:
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_input__a1

.Lpred7_1:
	movl	$.Lpred7_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	jmp	X1_246361746368__a6

.Lpred7_2:
	movl	$.Lpred7_3+0, %eax
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_is_relative_file_name__a1
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_try_other_directory__a4

.Lpred7_3:
	call	Pl_Delete_Choice_Point4
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+60, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+84, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_247468726F77__a4
	.p2align	4,,15
	.type	X0_try_other_directory__a4,@function
.globl X0_try_other_directory__a4

X0_try_other_directory__a4:
	movl	$.Lpred8_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred8_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred8_1:
	movl	$.Lpred8_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred8_3

.Lpred8_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred8_5

.Lpred8_2:
	movl	$.Lpred8_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred8_3:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_decompose_file_name__a4
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_atom_concat__a3
.Lcont19:
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_246361746368__a6

.Lpred8_4:
	call	Pl_Delete_Choice_Point4

.Lpred8_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_try_other_directory__a4
	.p2align	4,,15
	.type	X0_close_last_prolog_file__a0,@function
.globl X0_close_last_prolog_file__a0

X0_close_last_prolog_file__a0:
	movl	$5, %eax
	call	Pl_Allocate
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+44, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_character_count__a2
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_line_count__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-28(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-32(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+4
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_close__a1
	.p2align	4,,15
	.type	X0_read_predicate__a3,@function
.globl X0_read_predicate__a3

X0_read_predicate__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_read_predicate1__a3
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_24726561645F7072656469636174652F335F2461757831__a3
	.p2align	4,,15
	.type	X1_24726561645F7072656469636174652F335F2461757831__a3,@function

X1_24726561645F7072656469636174652F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_read_predicate_next__a3

.Lpred11_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_predicate_next__a3,@function
.globl X0_read_predicate_next__a3

X0_read_predicate_next__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred12_1+0, %eax
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_24726561645F7072656469636174655F6E6578742F335F2461757831__a2
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_add_dyn_interf_clause__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_create_exe_clauses_for_dyn_pred__a3
.Lcont26:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred12_1:
	movl	$.Lpred12_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_create_exe_clauses_for_pub_pred__a1

.Lpred12_2:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F7072656469636174655F6E6578742F335F2461757831__a2,@function

X1_24726561645F7072656469636174655F6E6578742F335F2461757831__a2:
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+108, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_test_pred_info__a3

.Lpred13_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_test_pred_info__a3
	.p2align	4,,15
	.type	X0_read_predicate1__a3,@function
.globl X0_read_predicate1__a3

X0_read_predicate1__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
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
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	movl	$.Lpred14_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$6, %eax
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
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_get_next_clause__a3
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	fn+24, %eax
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
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X1_24726561645F707265646963617465312F335F2461757831__a3
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_24726561645F707265646963617465312F335F2461757832__a2
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_define_predicate__a2
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_group_clauses_by_pred__a4

.Lpred14_2:
	movl	$.Lpred14_3+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+8, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont35:
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
	jmp	X0_recover_discontig_clauses__a3

.Lpred14_3:
	movl	$.Lpred14_4+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Assign
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
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
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_4:
	movl	$.Lpred14_5+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	fn+24, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X0_define_predicate__a2
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_24726561645F707265646963617465312F335F2461757833__a4
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_5:
	movl	$.Lpred14_6+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$6, %eax
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
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont40:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
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
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_6:
	movl	$.Lpred14_7+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$6, %eax
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
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
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
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_7:
	call	Pl_Delete_Choice_Point4
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F707265646963617465312F335F2461757833__a4,@function

X1_24726561645F707265646963617465312F335F2461757833__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred15_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X0_create_dyn_interf_clause__a4

.Lpred15_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+64, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+124, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F707265646963617465312F335F2461757832__a2,@function

X1_24726561645F707265646963617465312F335F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+132, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_warn__a2
.Lcont43:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred16_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F707265646963617465312F335F2461757831__a3,@function

X1_24726561645F707265646963617465312F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred17_1+0, %eax
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
	movl	ta+136, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	fn+8, %eax
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
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_assertz__a1
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_define_predicate__a2
.Lcont46:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred17_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_group_clauses_by_pred__a4,@function
.globl X0_group_clauses_by_pred__a4

X0_group_clauses_by_pred__a4:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_get_next_clause__a3
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2467726F75705F636C61757365735F62795F707265642F345F2461757831__a6
	.p2align	4,,15
	.type	X1_2467726F75705F636C61757365735F62795F707265642F345F2461757831__a6,@function

X1_2467726F75705F636C61757365735F62795F707265642F345F2461757831__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred19_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_group_clauses_by_pred__a4

.Lpred19_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467726F75705F636C61757365735F62795F707265642F345F2461757832__a3
	.p2align	4,,15
	.type	X1_2467726F75705F636C61757365735F62795F707265642F345F2461757832__a3,@function

X1_2467726F75705F636C61757365735F62795F707265642F345F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred20_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred20_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
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
	jmp	X0_asserta__a1
	.p2align	4,,15
	.type	X0_add_dyn_interf_clause__a3,@function
.globl X0_add_dyn_interf_clause__a3

X0_add_dyn_interf_clause__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont48:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred21_1:
	call	Pl_Delete_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_create_dyn_interf_clause__a4
.Lcont49:
	movl	fn+12, %eax
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
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_create_dyn_interf_clause__a4,@function
.globl X0_create_dyn_interf_clause__a4

X0_create_dyn_interf_clause__a4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X0_length__a2
.Lcont50:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+64, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+72, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_recover_discontig_clauses__a3,@function
.globl X0_recover_discontig_clauses__a3

X0_recover_discontig_clauses__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+8, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont51:
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
	jmp	X0_recover_discontig_clauses__a3

.Lpred23_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_create_exe_clauses_for_dyn_pred__a3,@function
.globl X0_create_exe_clauses_for_dyn_pred__a3

X0_create_exe_clauses_for_dyn_pred__a3:
	movl	$.Lpred24_1+0, %eax
	movl	$.Lpred24_2+0, %edx
	movl	$.Lpred24_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred24_1:
	movl	$.Lpred24_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred24_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred24_3:
	call	Pl_Delete_Choice_Point3

.Lpred24_4:
	movl	$7, %eax
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
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X0_get_file_name__a2
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X0_add_wrapper_to_dyn_clause__a4
.Lcont53:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+144, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+148, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+88, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X0_handle_initialization__a3
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_create_exe_clauses_for_dyn_pred__a3
	.p2align	4,,15
	.type	X0_create_exe_clauses_for_pub_pred__a1,@function
.globl X0_create_exe_clauses_for_pub_pred__a1

X0_create_exe_clauses_for_pub_pred__a1:
	movl	$.Lpred25_1+0, %eax
	movl	$.Lpred25_2+0, %edx
	movl	$.Lpred25_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred25_1:
	movl	$.Lpred25_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred25_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_3:
	call	Pl_Delete_Choice_Point1

.Lpred25_4:
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
	movl	%eax, -24(%edi)
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X0_get_file_name__a2
.Lcont55:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+88, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_handle_initialization__a3
.Lcont56:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_create_exe_clauses_for_pub_pred__a1
	.p2align	4,,15
	.type	X0_get_file_name__a2,@function
.globl X0_get_file_name__a2

X0_get_file_name__a2:
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+44, %eax
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
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_next_clause__a3,@function
.globl X0_get_next_clause__a3

X0_get_next_clause__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred27_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont57:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_1:
	call	Pl_Delete_Choice_Point4
	movl	$9, %eax
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
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	fn+92, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -44(%edi)
	movl	fn+96, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -40(%edi)
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
	movl	fn+100, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -48(%edi)
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+152, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont58:
	movl	pl_reg_bank+1044, %edi
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C617573652F335F2461757831__a8
.Lcont59:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C617573652F335F2461757831__a8,@function

X1_246765745F6E6578745F636C617573652F335F2461757831__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred28_1+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point
	movl	$13, %eax
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
	movl	pl_reg_bank+24, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -44(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X0_last_read_start_line_column__a2
.Lcont60:
	movl	fn+104, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -52(%edi)
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+108, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont61:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X0_stream_line_column__a3
.Lcont62:
	movl	pl_reg_bank+1044, %edi
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-64(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C617573652F335F2461757832__a3
.Lcont63:
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-64(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X0_get_next_clause1__a6

.Lpred28_1:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_get_next_clause__a3
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C617573652F335F2461757832__a3,@function

X1_246765745F6E6578745F636C617573652F335F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred29_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$-2147483641, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Dec
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_next_clause1__a6,@function
.globl X0_get_next_clause1__a6

X0_get_next_clause1__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred30_13+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred30_3+0, %eax
	movl	$.Lpred30_4+0, %edx
	movl	$.Lpred30_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred30_1:
	movl	st+4, %eax
	movl	$1, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred30_2:
	movl	$.Lpred30_sub_0+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred30_6

.Lpred30_sub_0:
	movl	$.Lpred30_sub_1+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	jmp	.Lpred30_8

.Lpred30_sub_1:
	movl	$.Lpred30_sub_2+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	jmp	.Lpred30_10

.Lpred30_sub_2:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred30_12

.Lpred30_3:
	movl	$.Lpred30_5+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred30_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X0_close_last_prolog_file__a0
.Lcont64:
	movl	ta+28, %eax
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
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757831__a4

.Lpred30_5:
	movl	$.Lpred30_7+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred30_6:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+120, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	ta+32, %eax
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
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757832__a2
.Lcont65:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_get_next_clause__a3

.Lpred30_7:
	movl	$.Lpred30_9+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred30_8:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+124, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757833__a1
.Lcont66:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_get_next_clause__a3

.Lpred30_9:
	movl	$.Lpred30_11+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred30_10:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+164, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757834__a0
.Lcont67:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_get_next_clause__a3

.Lpred30_11:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred30_12:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+168, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757835__a0
.Lcont68:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_get_next_clause__a3

.Lpred30_13:
	movl	$.Lpred30_14+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+28
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+28, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_get_next_clause__a3

.Lpred30_14:
	movl	$.Lpred30_15+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	fn+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+172, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X0_display_singletons__a2
.Lcont69:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757836__a1
.Lcont70:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont71, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757837__a2
.Lcont71:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_get_next_clause__a3

.Lpred30_15:
	movl	$.Lpred30_16+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	fn+64, %eax
	movl	pl_reg_bank+20, %edx
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
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont72, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757838__a2
.Lcont72:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont73, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F2461757839__a1
.Lcont73:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont74, pl_reg_bank+1040
	jmp	X1_246765745F6E6578745F636C61757365312F365F246175783130__a1
.Lcont74:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont75, pl_reg_bank+1040
	jmp	X0_check_head_is_module_free__a1
.Lcont75:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont76, pl_reg_bank+1040
	jmp	X0_check_module_clash__a2
.Lcont76:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont77, pl_reg_bank+1040
	jmp	X0_check_predicate__a2
.Lcont77:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_display_singletons__a2

.Lpred30_16:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_get_next_clause__a3
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F246175783130__a1,@function

X1_246765745F6E6578745F636C61757365312F365F246175783130__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred31_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred31_1:
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
	movl	ta+180, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757839__a1,@function

X1_246765745F6E6578745F636C61757365312F365F2461757839__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred32_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred32_1:
	call	Pl_Delete_Choice_Point2
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757838__a2,@function

X1_246765745F6E6578745F636C61757365312F365F2461757838__a2:
	movl	$.Lpred33_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	fn+68, %eax
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
	jmp	*pl_reg_bank+1040

.Lpred33_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757837__a2,@function

X1_246765745F6E6578745F636C61757365312F365F2461757837__a2:
	movl	$.Lpred34_1+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	X0_handle_directive__a2

.Lpred34_1:
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
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757836__a1,@function

X1_246765745F6E6578745F636C61757365312F365F2461757836__a1:
	movl	$.Lpred35_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred35_1:
	call	Pl_Delete_Choice_Point1
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757835__a0,@function

X1_246765745F6E6578745F636C61757365312F365F2461757835__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred36_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred36_1:
	call	Pl_Delete_Choice_Point1
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757834__a0,@function

X1_246765745F6E6578745F636C61757365312F365F2461757834__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Sub
	movl	%eax, pl_reg_bank+4
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+164, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred37_1:
	call	Pl_Delete_Choice_Point1
	movl	ta+204, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757833__a1,@function

X1_246765745F6E6578745F636C61757365312F365F2461757833__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred38_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246765745F6E6578745F636C61757365312F365F246175783131__a3

.Lpred38_1:
	call	Pl_Delete_Choice_Point2
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F246175783131__a3,@function

X1_246765745F6E6578745F636C61757365312F365F246175783131__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$-2147483645, %eax
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
	jmp	X1_246765745F6E6578745F636C61757365312F365F246175783132__a2

.Lpred39_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F246175783132__a2,@function

X1_246765745F6E6578745F636C61757365312F365F246175783132__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred40_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	fn+132, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+212, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	ta+72, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont78, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont78:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred40_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757832__a2,@function

X1_246765745F6E6578745F636C61757365312F365F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred41_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	fn+132, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+216, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+84, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	ta+72, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont79, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont79:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred41_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+200, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F2461757831__a4,@function

X1_246765745F6E6578745F636C61757365312F365F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred42_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	ta+60, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+60, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	X1_246765745F6E6578745F636C61757365312F365F246175783133__a0

.Lpred42_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_get_next_clause__a3
	.p2align	4,,15
	.type	X1_246765745F6E6578745F636C61757365312F365F246175783133__a0,@function

X1_246765745F6E6578745F636C61757365312F365F246175783133__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred43_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred43_1:
	call	Pl_Delete_Choice_Point1
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X0_after_syn_error__a0,@function
.globl X0_after_syn_error__a0

X0_after_syn_error__a0:
	movl	$3, %eax
	call	Pl_Allocate
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
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+4
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont80, pl_reg_bank+1040
	jmp	X0_syntax_error_info__a4
.Lcont80:
	movl	ta+28, %eax
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_disp_msg__a4
	.p2align	4,,15
	.type	X0_dcg_error__a1,@function
.globl X0_dcg_error__a1

X0_dcg_error__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont81, pl_reg_bank+1040
	jmp	X0_last_read_start_line_column__a2
.Lcont81:
	movl	ta+28, %eax
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+232, %eax
	call	Pl_Put_Atom_Tagged
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
	call	Pl_Deallocate
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X0_display_singletons__a2,@function
.globl X0_display_singletons__a2

X0_display_singletons__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred46_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont82, pl_reg_bank+1040
	jmp	X0_get_singletons__a2
.Lcont82:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_24646973706C61795F73696E676C65746F6E732F325F2461757831__a2

.Lpred46_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24646973706C61795F73696E676C65746F6E732F325F2461757831__a2,@function

X1_24646973706C61795F73696E676C65746F6E732F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred47_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred47_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
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
	movl	ta+240, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_warn__a2
	.p2align	4,,15
	.type	X0_get_singletons__a2,@function
.globl X0_get_singletons__a2

X0_get_singletons__a2:
	movl	$.Lpred48_1+0, %eax
	movl	$.Lpred48_2+0, %edx
	movl	$.Lpred48_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred48_1:
	movl	$.Lpred48_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred48_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred48_3:
	call	Pl_Delete_Choice_Point2

.Lpred48_4:
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
	movl	fn+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont83, pl_reg_bank+1040
	jmp	X1_246765745F73696E676C65746F6E732F325F2461757831__a3
.Lcont83:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_get_singletons__a2
	.p2align	4,,15
	.type	X1_246765745F73696E676C65746F6E732F325F2461757831__a3,@function

X1_246765745F73696E676C65746F6E732F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred49_1+0, %eax
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
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+244, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont84, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont84:
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

.Lpred49_1:
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
	.type	X0_handle_directive__a2,@function
.globl X0_handle_directive__a2

X0_handle_directive__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	jmp	X0_handle_directive__a3
	.p2align	4,,15
	.type	X0_foreign_get_options__a1,@function
.globl X0_foreign_get_options__a1

X0_foreign_get_options__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred51_1+0, %eax
	movl	$.Lpred51_2+0, %edx
	movl	$.Lpred51_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred51_1:
	movl	$.Lpred51_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred51_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred51_3:
	call	Pl_Delete_Choice_Point2

.Lpred51_4:
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
	movl	$.Lcont85, pl_reg_bank+1040
	jmp	X0_foreign_get_options1__a1
.Lcont85:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_foreign_get_options__a1
	.p2align	4,,15
	.type	X0_foreign_get_options1__a1,@function
.globl X0_foreign_get_options1__a1

X0_foreign_get_options1__a1:
	movl	$.Lpred52_3+0, %eax
	movl	$.Lpred52_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred52_1:
	movl	st+8, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred52_2:
	movl	$.Lpred52_sub_0+0, %eax
	call	Pl_Create_Choice_Point1
	jmp	.Lpred52_8

.Lpred52_sub_0:
	call	Pl_Delete_Choice_Point1
	jmp	.Lpred52_12

.Lpred52_3:
	movl	$.Lpred52_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred52_4:
	movl	fn+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred52_5:
	movl	$.Lpred52_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred52_6:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont86, pl_reg_bank+1040
	jmp	X1_24666F726569676E5F6765745F6F7074696F6E73312F315F2461757831__a1
.Lcont86:
	movl	ta+252, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred52_7:
	movl	$.Lpred52_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred52_8:
	movl	fn+148, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-1, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred52_9:
	movl	$.Lpred52_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred52_10:
	movl	fn+152, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred52_11:
	movl	$.Lpred52_13+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred52_12:
	movl	fn+148, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+60, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred52_13:
	call	Pl_Delete_Choice_Point1

.Lpred52_14:
	movl	fn+156, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	ta+264, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666F726569676E5F6765745F6F7074696F6E73312F315F2461757831__a1,@function

X1_24666F726569676E5F6765745F6F7074696F6E73312F315F2461757831__a1:
	movl	$.Lpred53_2+0, %eax
	movl	$.Lpred53_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred53_1:
	movl	st+12, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred53_2:
	movl	$.Lpred53_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred53_3:
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred53_4:
	movl	$.Lpred53_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred53_5:
	movl	ta+268, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred53_6:
	call	Pl_Delete_Choice_Point1

.Lpred53_7:
	movl	ta+272, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_foreign_check_types__a4,@function
.globl X0_foreign_check_types__a4

X0_foreign_check_types__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred54_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred54_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	fn+84, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -32(%edi)
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont87, pl_reg_bank+1040
	jmp	X1_24666F726569676E5F636865636B5F74797065732F345F2461757831__a3
.Lcont87:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont88, pl_reg_bank+1040
	jmp	X0_foreign_check_arg__a1
.Lcont88:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_foreign_check_types__a4
	.p2align	4,,15
	.type	X1_24666F726569676E5F636865636B5F74797065732F345F2461757831__a3,@function

X1_24666F726569676E5F636865636B5F74797065732F345F2461757831__a3:
	movl	$.Lpred55_2+0, %eax
	movl	$.Lpred55_9+0, %edx
	movl	$.Lpred55_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred55_1:
	movl	st+16, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred55_2:
	movl	$.Lpred55_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred55_3:
	movl	fn+160, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+276, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_4:
	movl	$.Lpred55_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred55_5:
	movl	fn+164, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+280, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_6:
	movl	$.Lpred55_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred55_7:
	movl	fn+168, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+284, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_8:
	call	Pl_Delete_Choice_Point3

.Lpred55_9:
	movl	ta+288, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+288, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+276, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_foreign_check_arg__a1,@function
.globl X0_foreign_check_arg__a1

X0_foreign_check_arg__a1:
	movl	$.Lpred56_2+0, %eax
	movl	$.Lpred56_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred56_1:
	movl	st+20, %eax
	movl	$16, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred56_2:
	movl	$.Lpred56_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred56_3:
	movl	ta+292, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_4:
	movl	$.Lpred56_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_5:
	movl	ta+296, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_6:
	movl	$.Lpred56_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_7:
	movl	ta+300, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_8:
	movl	$.Lpred56_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_9:
	movl	ta+304, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_10:
	movl	$.Lpred56_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_11:
	movl	ta+308, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_12:
	movl	$.Lpred56_14+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_13:
	movl	ta+268, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_14:
	movl	$.Lpred56_16+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_15:
	movl	ta+312, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_16:
	movl	$.Lpred56_18+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_17:
	movl	ta+316, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_18:
	movl	$.Lpred56_20+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_19:
	movl	ta+320, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_20:
	movl	$.Lpred56_22+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_21:
	movl	ta+324, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_22:
	movl	$.Lpred56_24+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_23:
	movl	ta+328, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_24:
	movl	$.Lpred56_26+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_25:
	movl	ta+332, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_26:
	movl	$.Lpred56_28+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_27:
	movl	ta+336, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_28:
	movl	$.Lpred56_30+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_29:
	movl	ta+340, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_30:
	movl	$.Lpred56_32+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred56_31:
	movl	ta+344, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_32:
	call	Pl_Delete_Choice_Point1

.Lpred56_33:
	movl	ta+288, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_handle_initialization__a3,@function
.globl X0_handle_initialization__a3

X0_handle_initialization__a3:
	movl	$.Lpred57_2+0, %eax
	movl	$.Lpred57_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred57_1:
	movl	st+24, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred57_2:
	movl	$.Lpred57_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred57_3:
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_assertz__a1

.Lpred57_4:
	call	Pl_Delete_Choice_Point3

.Lpred57_5:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_exec_directive__a1,@function
.globl X0_exec_directive__a1

X0_exec_directive__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred58_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+172, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont89, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont89:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred58_1:
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
	movl	ta+348, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_warn__a2
	.p2align	4,,15
	.type	X0_exec_directive_exception__a2,@function
.globl X0_exec_directive_exception__a2

X0_exec_directive_exception__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
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
	movl	ta+352, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_warn__a2
	.p2align	4,,15
	.type	X0_used_bips_via_call__a0,@function
.globl X0_used_bips_via_call__a0

X0_used_bips_via_call__a0:
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont90, pl_reg_bank+1040
	jmp	X0_op__a3
.Lcont90:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont91, pl_reg_bank+1040
	jmp	X0_char_conversion__a2
.Lcont91:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont92, pl_reg_bank+1040
	jmp	X0_set_prolog_flag__a2
.Lcont92:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_expand_term__a2
	.p2align	4,,15
	.type	X0_add_empty_dyn__a2,@function
.globl X0_add_empty_dyn__a2

X0_add_empty_dyn__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred61_2+0, %eax
	movl	$.Lpred61_3+0, %edx
	movl	$0, %ecx
	movl	$.Lpred61_5+0, 0(%esp)
	movl	$.Lpred61_1+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred61_1:
	movl	st+28, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred61_2:
	movl	$.Lpred61_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred61_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred61_4:
	movl	$.Lpred61_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred61_5:
	movl	$2, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont93, pl_reg_bank+1040
	jmp	X0_add_empty_dyn__a2
.Lcont93:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_empty_dyn__a2

.Lpred61_6:
	movl	$.Lpred61_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred61_7:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont94, pl_reg_bank+1040
	jmp	X0_add_empty_dyn__a2
.Lcont94:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_empty_dyn__a2

.Lpred61_8:
	call	Pl_Delete_Choice_Point3

.Lpred61_9:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X1_246164645F656D7074795F64796E2F325F2461757831__a3
	.p2align	4,,15
	.type	X1_246164645F656D7074795F64796E2F325F2461757831__a3,@function

X1_246164645F656D7074795F64796E2F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred62_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont95, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont95:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred62_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
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
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_add_ensure_linked__a1,@function
.globl X0_add_ensure_linked__a1

X0_add_ensure_linked__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred63_3+0, %eax
	movl	$.Lpred63_4+0, %edx
	movl	$0, %ecx
	movl	$.Lpred63_6+0, 0(%esp)
	movl	$.Lpred63_1+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred63_1:
	movl	st+32, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred63_2:
	movl	$.Lpred63_sub_0+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	.Lpred63_10

.Lpred63_sub_0:
	call	Pl_Delete_Choice_Point2
	jmp	.Lpred63_12

.Lpred63_3:
	movl	$.Lpred63_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred63_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred63_5:
	movl	$.Lpred63_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred63_6:
	movl	$1, %eax
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
	call	Pl_Cut
	movl	$.Lcont96, pl_reg_bank+1040
	jmp	X0_add_ensure_linked__a1
.Lcont96:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_add_ensure_linked__a1

.Lpred63_7:
	movl	$.Lpred63_9+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred63_8:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	$.Lcont97, pl_reg_bank+1040
	jmp	X0_add_ensure_linked__a1
.Lcont97:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_add_ensure_linked__a1

.Lpred63_9:
	movl	$.Lpred63_11+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred63_10:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont98, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont98:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred63_11:
	call	Pl_Delete_Choice_Point2

.Lpred63_12:
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X0_add_module_export_info__a2,@function
.globl X0_add_module_export_info__a2

X0_add_module_export_info__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred64_3+0, %eax
	movl	$.Lpred64_4+0, %edx
	movl	$0, %ecx
	movl	$.Lpred64_6+0, 0(%esp)
	movl	$.Lpred64_1+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred64_1:
	movl	st+36, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred64_2:
	movl	$.Lpred64_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred64_10

.Lpred64_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred64_12

.Lpred64_3:
	movl	$.Lpred64_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred64_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred64_5:
	movl	$.Lpred64_7+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred64_6:
	movl	$2, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont99, pl_reg_bank+1040
	jmp	X0_add_module_export_info__a2
.Lcont99:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_module_export_info__a2

.Lpred64_7:
	movl	$.Lpred64_9+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred64_8:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont100, pl_reg_bank+1040
	jmp	X0_add_module_export_info__a2
.Lcont100:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_module_export_info__a2

.Lpred64_9:
	movl	$.Lpred64_11+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred64_10:
	movl	$4, %eax
	call	Pl_Allocate
	movl	fn+60, %eax
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
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont101, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont101:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+356, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred64_11:
	call	Pl_Delete_Choice_Point3

.Lpred64_12:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont102, pl_reg_bank+1040
	jmp	X0_assertz__a1
.Lcont102:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246164645F6D6F64756C655F6578706F72745F696E666F2F325F2461757831__a2
	.p2align	4,,15
	.type	X1_246164645F6D6F64756C655F6578706F72745F696E666F2F325F2461757831__a2,@function

X1_246164645F6D6F64756C655F6578706F72745F696E666F2F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred65_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont103, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont103:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_check_module_clash__a2

.Lpred65_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_check_module_name__a2,@function
.globl X0_check_module_name__a2

X0_check_module_name__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred66_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+80, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred66_1:
	movl	$.Lpred66_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred66_2:
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
	movl	ta+360, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X0_check_head_is_module_free__a1,@function
.globl X0_check_head_is_module_free__a1

X0_check_head_is_module_free__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred67_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	fn+176, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	fn+176, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+364, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2

.Lpred67_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_check_module_clash__a2,@function
.globl X0_check_module_clash__a2

X0_check_module_clash__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred68_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	fn+36, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont104, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont104:
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
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+368, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred68_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_owner_module__a3,@function
.globl X0_get_owner_module__a3

X0_get_owner_module__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred69_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont105, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont105:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred69_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_is_exported__a2,@function
.globl X0_is_exported__a2

X0_is_exported__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont106, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont106:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_get_module_of_cur_pred__a1,@function
.globl X0_get_module_of_cur_pred__a1

X0_get_module_of_cur_pred__a1:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont107, pl_reg_bank+1040
	jmp	X0_cur_pred__a2
.Lcont107:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246765745F6D6F64756C655F6F665F6375725F707265642F315F2461757831__a3
	.p2align	4,,15
	.type	X1_246765745F6D6F64756C655F6F665F6375725F707265642F315F2461757831__a3,@function

X1_246765745F6D6F64756C655F6F665F6375725F707265642F315F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred72_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont108, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont108:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred72_1:
	movl	$.Lpred72_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont109, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont109:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred72_2:
	call	Pl_Delete_Choice_Point4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_flag_for_preds__a2,@function
.globl X0_set_flag_for_preds__a2

X0_set_flag_for_preds__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred73_2+0, %eax
	movl	$.Lpred73_3+0, %edx
	movl	$0, %ecx
	movl	$.Lpred73_5+0, 0(%esp)
	movl	$.Lpred73_1+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred73_1:
	movl	st+40, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred73_2:
	movl	$.Lpred73_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred73_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred73_4:
	movl	$.Lpred73_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred73_5:
	movl	$2, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont110, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont110:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_set_flag_for_preds__a2

.Lpred73_6:
	movl	$.Lpred73_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred73_7:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont111, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont111:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_set_flag_for_preds__a2

.Lpred73_8:
	call	Pl_Delete_Choice_Point3

.Lpred73_9:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_247365745F666C61675F666F725F70726564732F325F2461757831__a4
	.p2align	4,,15
	.type	X1_247365745F666C61675F666F725F70726564732F325F2461757831__a4,@function

X1_247365745F666C61675F666F725F70726564732F325F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred74_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont112, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont112:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+380, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_warn__a2

.Lpred74_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
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
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont113, pl_reg_bank+1040
	jmp	X1_247365745F666C61675F666F725F70726564732F325F2461757832__a3
.Lcont113:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_set_pred_info__a3
	.p2align	4,,15
	.type	X1_247365745F666C61675F666F725F70726564732F325F2461757832__a3,@function

X1_247365745F666C61675F666F725F70726564732F325F2461757832__a3:
	movl	$.Lpred75_6+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred75_2+0, %eax
	movl	$.Lpred75_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred75_1:
	movl	st+44, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred75_2:
	movl	$.Lpred75_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred75_3:
	movl	ta+372, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_unset_pred_info__a3

.Lpred75_4:
	call	Pl_Delete_Choice_Point3

.Lpred75_5:
	movl	ta+376, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_unset_pred_info__a3

.Lpred75_6:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_define_predicate__a2,@function
.globl X0_define_predicate__a2

X0_define_predicate__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred76_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont114, pl_reg_bank+1040
	jmp	X0_set_pred_info__a3
.Lcont114:
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont115, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont115:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred76_1:
	movl	$.Lpred76_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont116, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont116:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred76_2:
	movl	$.Lpred76_3+0, %eax
	call	Pl_Update_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+384, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_pred_info__a3

.Lpred76_3:
	movl	$.Lpred76_4+0, %eax
	call	Pl_Update_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+388, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_set_pred_info__a3

.Lpred76_4:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_flag_bit__a2,@function
.globl X0_flag_bit__a2

X0_flag_bit__a2:
	movl	$.Lpred77_2+0, %eax
	movl	$.Lpred77_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred77_1:
	movl	st+48, %eax
	movl	$9, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred77_2:
	movl	$.Lpred77_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred77_3:
	movl	ta+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_4:
	movl	$.Lpred77_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_5:
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
	jmp	*pl_reg_bank+1040

.Lpred77_6:
	movl	$.Lpred77_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_7:
	movl	ta+104, %eax
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

.Lpred77_8:
	movl	$.Lpred77_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_9:
	movl	ta+372, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_10:
	movl	$.Lpred77_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_11:
	movl	ta+376, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_12:
	movl	$.Lpred77_14+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_13:
	movl	ta+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_14:
	movl	$.Lpred77_16+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_15:
	movl	ta+392, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483621, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_16:
	movl	$.Lpred77_18+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred77_17:
	movl	ta+396, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483617, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_18:
	call	Pl_Delete_Choice_Point2

.Lpred77_19:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483613, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_pred_info__a3,@function
.globl X0_set_pred_info__a3

X0_set_pred_info__a3:
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
	movl	$.Lcont117, pl_reg_bank+1040
	jmp	X0_flag_bit__a2
.Lcont117:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont118, pl_reg_bank+1040
	jmp	X1_247365745F707265645F696E666F2F335F2461757831__a3
.Lcont118:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-32(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Fct_Fast_Shl
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Or
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1
	.p2align	4,,15
	.type	X1_247365745F707265645F696E666F2F335F2461757831__a3,@function

X1_247365745F707265645F696E666F2F335F2461757831__a3:
	movl	$.Lpred79_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
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
	jmp	X0_retract__a1

.Lpred79_1:
	call	Pl_Delete_Choice_Point3
	movl	$-2147483645, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unset_pred_info__a3,@function
.globl X0_unset_pred_info__a3

X0_unset_pred_info__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred80_1+0, %eax
	call	Pl_Create_Choice_Point4
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
	movl	$.Lcont119, pl_reg_bank+1040
	jmp	X0_flag_bit__a2
.Lcont119:
	movl	fn+32, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	$.Lcont120, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont120:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-32(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Fct_Fast_Shl
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Fast_Not
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_And
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1

.Lpred80_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_test_pred_info__a3,@function
.globl X0_test_pred_info__a3

X0_test_pred_info__a3:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont121, pl_reg_bank+1040
	jmp	X0_flag_bit__a2
.Lcont121:
	movl	fn+32, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont122, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont122:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Fct_Fast_Shl
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_And
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_check_predicate__a2,@function
.globl X0_check_predicate__a2

X0_check_predicate__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred82_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+400, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont123, pl_reg_bank+1040
	jmp	X0_control_construct__a2
.Lcont123:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+404, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred82_1:
	movl	$.Lpred82_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+400, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont124, pl_reg_bank+1040
	jmp	X0_bip__a2
.Lcont124:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+408, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred82_2:
	movl	$.Lpred82_3+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+412, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont125, pl_reg_bank+1040
	jmp	X0_suspicious_predicate__a2
.Lcont125:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+416, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_warn__a2

.Lpred82_3:
	movl	$.Lpred82_4+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$3, %eax
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
	movl	$.Lcont126, pl_reg_bank+1040
	jmp	X1_246175785F6E616D65__a1
.Lcont126:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+420, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_warn__a2

.Lpred82_4:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_bip__a2,@function
.globl X0_bip__a2

X0_bip__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+384, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont127, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F706572747931__a3
.Lcont127:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_control_construct__a2,@function
.globl X0_control_construct__a2

X0_control_construct__a2:
	movl	$.Lpred84_2+0, %eax
	movl	$.Lpred84_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred84_1:
	movl	st+52, %eax
	movl	$9, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred84_2:
	movl	$.Lpred84_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred84_3:
	movl	ta+424, %eax
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

.Lpred84_4:
	movl	$.Lpred84_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_5:
	movl	ta+428, %eax
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

.Lpred84_6:
	movl	$.Lpred84_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_7:
	movl	ta+432, %eax
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

.Lpred84_8:
	movl	$.Lpred84_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_9:
	movl	ta+436, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred84_10:
	movl	$.Lpred84_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_11:
	movl	ta+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred84_12:
	movl	$.Lpred84_14+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_13:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred84_14:
	movl	$.Lpred84_16+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_15:
	movl	ta+440, %eax
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

.Lpred84_16:
	movl	$.Lpred84_18+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred84_17:
	movl	ta+444, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred84_18:
	call	Pl_Delete_Choice_Point2

.Lpred84_19:
	movl	ta+448, %eax
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
	.p2align	4,,15
	.type	X0_suspicious_predicate__a2,@function
.globl X0_suspicious_predicate__a2

X0_suspicious_predicate__a2:
	movl	$.Lpred85_3+0, %eax
	movl	$.Lpred85_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred85_1:
	movl	st+56, %eax
	movl	$9, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred85_2:
	movl	$.Lpred85_sub_0+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	.Lpred85_6

.Lpred85_sub_0:
	call	Pl_Delete_Choice_Point2
	jmp	.Lpred85_8

.Lpred85_3:
	movl	$.Lpred85_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred85_4:
	movl	ta+452, %eax
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

.Lpred85_5:
	movl	$.Lpred85_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_6:
	movl	ta+456, %eax
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

.Lpred85_7:
	movl	$.Lpred85_9+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_8:
	movl	ta+456, %eax
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

.Lpred85_9:
	movl	$.Lpred85_11+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_10:
	movl	ta+460, %eax
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

.Lpred85_11:
	movl	$.Lpred85_13+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_12:
	movl	ta+464, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred85_13:
	movl	$.Lpred85_15+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_14:
	movl	ta+468, %eax
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

.Lpred85_15:
	movl	$.Lpred85_17+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_16:
	movl	ta+472, %eax
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

.Lpred85_17:
	movl	$.Lpred85_19+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_18:
	movl	ta+476, %eax
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

.Lpred85_19:
	movl	$.Lpred85_21+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred85_20:
	movl	ta+480, %eax
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

.Lpred85_21:
	call	Pl_Delete_Choice_Point2

.Lpred85_22:
	movl	ta+484, %eax
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
	.p2align	4,,15
	.type	X0_warn__a2,@function
.globl X0_warn__a2

X0_warn__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+488, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_disp_msg__a4
	.p2align	4,,15
	.type	X0_error__a2,@function
.globl X0_error__a2

X0_error__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+492, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont128, pl_reg_bank+1040
	jmp	X0_disp_msg__a4
.Lcont128:
	movl	$.Lcont129, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont129:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont130, pl_reg_bank+1040
	jmp	X1_246572726F722F325F2461757831__a1
.Lcont130:
	call	Pl_Deallocate
	jmp	X0_abort__a0
	.p2align	4,,15
	.type	X1_246572726F722F325F2461757831__a1,@function

X1_246572726F722F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred88_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont131, pl_reg_bank+1040
	jmp	X0_close_last_prolog_file__a0
.Lcont131:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred88_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_disp_msg__a4,@function
.globl X0_disp_msg__a4

X0_disp_msg__a4:
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
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont132, pl_reg_bank+1040
	jmp	X0_numbervars__a1
.Lcont132:
	movl	ta+36, %eax
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
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont133, pl_reg_bank+1040
	jmp	X1_24646973705F6D73672F345F2461757831__a2
.Lcont133:
	movl	ta+496, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont134, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont134:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont135, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont135:
	movl	ta+500, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_24646973705F6D73672F345F2461757831__a2,@function

X1_24646973705F6D73672F345F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred90_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+112, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont136, pl_reg_bank+1040
	jmp	X0_disp_file_name__a2
.Lcont136:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont137, pl_reg_bank+1040
	jmp	X0_disp_lines__a1
.Lcont137:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_disp_column__a1

.Lpred90_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_disp_file_name__a2,@function
.globl X0_disp_file_name__a2

X0_disp_file_name__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred91_1+0, %eax
	movl	$.Lpred91_2+0, %edx
	movl	$.Lpred91_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred91_1:
	movl	$.Lpred91_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred91_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred91_3:
	call	Pl_Delete_Choice_Point3

.Lpred91_4:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+504, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+44, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont138, pl_reg_bank+1040
	jmp	X0_disp_file_name__a2
.Lcont138:
	movl	ta+508, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X0_disp_lines__a1,@function
.globl X0_disp_lines__a1

X0_disp_lines__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred92_3+0, %eax
	movl	$.Lpred92_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred92_1:
	movl	st+60, %eax
	movl	$1, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred92_2:
	movl	$.Lpred92_sub_0+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	.Lpred92_4

.Lpred92_sub_0:
	call	Pl_Delete_Choice_Point2
	jmp	.Lpred92_6

.Lpred92_3:
	movl	$.Lpred92_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred92_4:
	movl	fn+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+512, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_format__a2

.Lpred92_5:
	call	Pl_Delete_Choice_Point2

.Lpred92_6:
	movl	fn+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+516, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X0_disp_column__a1,@function
.globl X0_disp_column__a1

X0_disp_column__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred93_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+520, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_format__a2

.Lpred93_1:
	call	Pl_Delete_Choice_Point2
	movl	ta+524, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_write__a1
	.p2align	4,,15
	.type	X0_exception__a1,@function
.globl X0_exception__a1

X0_exception__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred94_9+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$.Lpred94_3+0, %eax
	movl	$.Lpred94_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred94_1:
	movl	st+64, %eax
	movl	$1, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred94_2:
	movl	$.Lpred94_sub_0+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	.Lpred94_4

.Lpred94_sub_0:
	movl	$.Lpred94_sub_1+0, %eax
	call	Pl_Update_Choice_Point2
	jmp	.Lpred94_6

.Lpred94_sub_1:
	call	Pl_Delete_Choice_Point2
	jmp	.Lpred94_8

.Lpred94_3:
	movl	$.Lpred94_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred94_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont139, pl_reg_bank+1040
	jmp	X0_syntax_error_info__a4
.Lcont139:
	movl	ta+28, %eax
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+528, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred94_5:
	movl	$.Lpred94_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred94_6:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+532, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2

.Lpred94_7:
	call	Pl_Delete_Choice_Point2

.Lpred94_8:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+84, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+536, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2

.Lpred94_9:
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
	movl	ta+540, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X0_handle_directive__a3,@function
.globl X0_handle_directive__a3

X0_handle_directive__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred95_43+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred95_3+0, %eax
	movl	$.Lpred95_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred95_1:
	movl	st+68, %eax
	movl	$18, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred95_2:
	movl	$.Lpred95_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred95_38

.Lpred95_sub_0:
	movl	$.Lpred95_sub_1+0, %eax
	call	Pl_Update_Choice_Point4
	jmp	.Lpred95_40

.Lpred95_sub_1:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred95_42

.Lpred95_3:
	movl	$.Lpred95_5+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred95_4:
	movl	ta+544, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_set_flag_for_preds__a2

.Lpred95_5:
	movl	$.Lpred95_7+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_6:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+548, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+108, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont140, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont140:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont141, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont141:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_empty_dyn__a2

.Lpred95_7:
	movl	$.Lpred95_9+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_8:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+552, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont142, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont142:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_empty_dyn__a2

.Lpred95_9:
	movl	$.Lpred95_11+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_10:
	movl	ta+556, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+136, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_set_flag_for_preds__a2

.Lpred95_11:
	movl	$.Lpred95_13+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_12:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+384, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont143, pl_reg_bank+1040
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757831__a1
.Lcont143:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred95_13:
	movl	$.Lpred95_15+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_14:
	movl	ta+388, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757832__a1

.Lpred95_15:
	movl	$.Lpred95_17+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_16:
	movl	ta+560, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757833__a1

.Lpred95_17:
	movl	$.Lpred95_19+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_18:
	movl	ta+564, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+568, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_warn__a2

.Lpred95_19:
	movl	$.Lpred95_21+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_20:
	movl	ta+572, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+576, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_warn__a2

.Lpred95_21:
	movl	$.Lpred95_23+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_22:
	movl	ta+580, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_open_new_prolog_file__a1

.Lpred95_23:
	movl	$.Lpred95_25+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_24:
	movl	$4, %eax
	call	Pl_Allocate
	movl	ta+584, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+184, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont144, pl_reg_bank+1040
	jmp	X0_exec_directive__a1
.Lcont144:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+184, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_handle_initialization__a3

.Lpred95_25:
	movl	$.Lpred95_27+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_26:
	movl	$3, %eax
	call	Pl_Allocate
	movl	ta+588, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+188, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont145, pl_reg_bank+1040
	jmp	X0_exec_directive__a1
.Lcont145:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+188, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_handle_initialization__a3

.Lpred95_27:
	movl	$.Lpred95_29+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_28:
	movl	$3, %eax
	call	Pl_Allocate
	movl	ta+592, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+192, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont146, pl_reg_bank+1040
	jmp	X0_exec_directive__a1
.Lcont146:
	movl	$.Lcont147, pl_reg_bank+1040
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757834__a0
.Lcont147:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+192, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_handle_initialization__a3

.Lpred95_29:
	movl	$.Lpred95_31+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_30:
	movl	ta+596, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_handle_initialization__a3

.Lpred95_31:
	movl	$.Lpred95_33+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_32:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757835__a2

.Lpred95_33:
	movl	$.Lpred95_35+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_34:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+600, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont148, pl_reg_bank+1040
	jmp	X0_check_module_name__a2
.Lcont148:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_module_export_info__a2

.Lpred95_35:
	movl	$.Lpred95_37+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_36:
	movl	ta+604, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757836__a1

.Lpred95_37:
	movl	$.Lpred95_39+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_38:
	movl	ta+192, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_handle_directive__a3

.Lpred95_39:
	movl	$.Lpred95_41+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred95_40:
	movl	ta+192, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+608, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+612, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_warn__a2

.Lpred95_41:
	call	Pl_Delete_Choice_Point4

.Lpred95_42:
	movl	$7, %eax
	call	Pl_Allocate
	movl	ta+192, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List
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
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont149, pl_reg_bank+1040
	jmp	X1_2468616E646C655F6469726563746976652F335F2461757837__a2
.Lcont149:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont150, pl_reg_bank+1040
	jmp	X0_define_predicate__a2
.Lcont150:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+252, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+268, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+260, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+264, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont151, pl_reg_bank+1040
	jmp	X0_foreign_get_options__a1
.Lcont151:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont152, pl_reg_bank+1040
	jmp	X0_foreign_check_types__a4
.Lcont152:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+252, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
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
	movl	ta+264, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+196, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont153, pl_reg_bank+1040
	jmp	X0_no_internal_transf__a2
.Lcont153:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+200, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont154, pl_reg_bank+1040
	jmp	X0_assertz__a1
.Lcont154:
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+616, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_add_ensure_linked__a1

.Lpred95_43:
	call	Pl_Delete_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont155, pl_reg_bank+1040
	jmp	X0_length__a2
.Lcont155:
	movl	ta+620, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_warn__a2
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757837__a2,@function

X1_2468616E646C655F6469726563746976652F335F2461757837__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred96_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont156, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont156:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+624, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_error__a2

.Lpred96_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757836__a1,@function

X1_2468616E646C655F6469726563746976652F335F2461757836__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred97_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+396, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont157, pl_reg_bank+1040
	jmp	X0_set_flag_for_preds__a2
.Lcont157:
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_assertz__a1

.Lpred97_1:
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
	movl	ta+628, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757835__a2,@function

X1_2468616E646C655F6469726563746976652F335F2461757835__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred98_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont158, pl_reg_bank+1040
	jmp	X0_check_module_name__a2
.Lcont158:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_module_export_info__a2

.Lpred98_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+632, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757834__a0,@function

X1_2468616E646C655F6469726563746976652F335F2461757834__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred99_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+636, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+640, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont159, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont159:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred99_1:
	call	Pl_Delete_Choice_Point1
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757833__a1,@function

X1_2468616E646C655F6469726563746976652F335F2461757833__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred100_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+644, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_warn__a2

.Lpred100_1:
	call	Pl_Delete_Choice_Point2
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	X0_add_ensure_linked__a1
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757832__a1,@function

X1_2468616E646C655F6469726563746976652F335F2461757832__a1:
	movl	$.Lpred101_1+0, %eax
	call	Pl_Create_Choice_Point1
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+388, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred101_1:
	call	Pl_Delete_Choice_Point1
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_set_flag_for_preds__a2
	.p2align	4,,15
	.type	X1_2468616E646C655F6469726563746976652F335F2461757831__a1,@function

X1_2468616E646C655F6469726563746976652F335F2461757831__a1:
	movl	$.Lpred102_1+0, %eax
	call	Pl_Create_Choice_Point1
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+384, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred102_1:
	call	Pl_Delete_Choice_Point1
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_set_flag_for_preds__a2
	.p2align	4,,15
	.type	directive_1,@function

directive_1:
	movl	$-2147482845, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+648, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+652, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X0_op__a3
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
	movl	%eax, at+512
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+376
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+388
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+576
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+568
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+416
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+692
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+688
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+684
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+680
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+676
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+672
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+668
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+424
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+428
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+480
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+552
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+368
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+544
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+508
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+372
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+556
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+540
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+504
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+364
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+380
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+384
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+436
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+496
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+432
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+612
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+616
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+524
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+528
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+640
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+396
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+400
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+392
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+492
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+500
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC87, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC88, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC89, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC90, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+448
	movl	$.LC91, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+440
	movl	$.LC92, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+460
	movl	$.LC93, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+608
	movl	$.LC94, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+588
	movl	$.LC95, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+580
	movl	$.LC96, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+584
	movl	$.LC97, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+572
	movl	$.LC98, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC99, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+452
	movl	$.LC100, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+600
	movl	$.LC101, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+624
	movl	$.LC102, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC103, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+620
	movl	$.LC104, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+628
	movl	$.LC105, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+564
	movl	$.LC106, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+592
	movl	$.LC107, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+352
	movl	$.LC108, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+356
	movl	$.LC109, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+516
	movl	$.LC110, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC111, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+444
	movl	$.LC112, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC113, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+664
	movl	$.LC114, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC115, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC116, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC117, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC118, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC119, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+412
	movl	$.LC120, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC121, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC122, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+404
	movl	$.LC123, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC124, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC125, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC126, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC127, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC128, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC129, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC130, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+632
	movl	$.LC131, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+648
	movl	$.LC132, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC133, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+408
	movl	$.LC134, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC135, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+468
	movl	$.LC136, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+660
	movl	$.LC137, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+652
	movl	$.LC138, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+472
	movl	$.LC139, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+604
	movl	$.LC140, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+464
	movl	$.LC141, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC142, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC143, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+636
	movl	$.LC144, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC145, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC146, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC147, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+456
	movl	$.LC148, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+596
	movl	$.LC149, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC150, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC151, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC152, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC153, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC154, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC155, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC156, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC157, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+420
	movl	$.LC158, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+476
	movl	$.LC159, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+644
	movl	$.LC160, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC161, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+536
	movl	$.LC162, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+348
	movl	$.LC163, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC164, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+488
	movl	$.LC165, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+532
	movl	$.LC166, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+520
	movl	$.LC167, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC168, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+484
	movl	$.LC169, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+656
	movl	$.LC170, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+360
	movl	$.LC171, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC172, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+560
	movl	$.LC173, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+548
	movl	$.LC174, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC175, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+504
	movl	at+512, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+436
	movl	$.LC176, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC177, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+124
	movl	$.LC178, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC179, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC180, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+616
	movl	$.LC181, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	at+552, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+476
	movl	at+268, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+468
	movl	at+368, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+424
	movl	at+272, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+472
	movl	at+544, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+460
	movl	at+508, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+432
	movl	at+372, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+480
	movl	at+556, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+484
	movl	at+540, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+452
	movl	$.LC182, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+524
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+456
	movl	$.LC183, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+512
	movl	$.LC184, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+516
	movl	$.LC185, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+520
	movl	at+504, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+428
	movl	at+276, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+652
	movl	$.LC186, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+232
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+56
	movl	$.LC187, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+244
	movl	at+192, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+152
	movl	$.LC188, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	at+300, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+308
	movl	at+436, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+376
	movl	at+252, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+268
	movl	at+432, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+372
	movl	at+612, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+384
	movl	at+616, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+388
	movl	$.LC189, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	at+320, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+328
	movl	at+524, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+440
	movl	$.LC190, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+608
	movl	$.LC191, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+532
	movl	$.LC192, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+536
	movl	at+528, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+444
	movl	at+304, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+312
	movl	at+640, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+588
	movl	at+332, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+340
	movl	$.LC193, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+368
	movl	at+312, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+320
	movl	at+336, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+344
	movl	at+448, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+128
	movl	$.LC194, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC195, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC196, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+348
	movl	$.LC197, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+352
	movl	$.LC198, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+632
	movl	$.LC199, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+380
	movl	at+460, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+136
	movl	at+608, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+556
	movl	$.LC200, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+132
	movl	at+452, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+108
	movl	at+600, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+548
	movl	$.LC201, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+212
	movl	$.LC202, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	at+624, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+564
	movl	$.LC203, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+568
	movl	at+36, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	$.LC204, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+168
	movl	$.LC205, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	at+620, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+560
	movl	$.LC206, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+644
	movl	at+628, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+572
	movl	$.LC207, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+576
	movl	$.LC208, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC209, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+540
	movl	$.LC210, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+516, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+72
	movl	$.LC211, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	$.LC212, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+492
	movl	at+292, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+300
	movl	at+664, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+192
	movl	$.LC213, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+612
	movl	$.LC214, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+624
	movl	$.LC215, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+260
	movl	$.LC216, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+264
	movl	$.LC217, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+248
	movl	$.LC218, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+176
	movl	$.LC219, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+252
	movl	$.LC220, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+648
	movl	$.LC221, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+184
	movl	$.LC222, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+180
	movl	$.LC223, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+216
	movl	$.LC224, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC225, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+276
	movl	at+324, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+332
	movl	$.LC226, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	at+308, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+316
	movl	at+316, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+324
	movl	$.LC227, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC228, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+284
	movl	at+632, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+580
	movl	at+648, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+596
	movl	at+284, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+292
	movl	$.LC229, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC230, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+360
	movl	$.LC231, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+628
	movl	at+256, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+272
	movl	at+468, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+396
	movl	at+660, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+604
	movl	at+652, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	$.LC232, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+364
	movl	$.LC233, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	at+472, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+112
	movl	at+604, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+552
	movl	$.LC234, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	at+464, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+392
	movl	at+248, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+256
	movl	at+296, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+304
	movl	$.LC235, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+640
	movl	at+636, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+584
	movl	$.LC236, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	$.LC237, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	at+40, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+76
	movl	$.LC238, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+280
	movl	at+288, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+296
	movl	$.LC239, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+356
	movl	at+456, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+104
	movl	at+596, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+544
	movl	$.LC240, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	$.LC241, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC242, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+400
	movl	$.LC243, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+408
	movl	$.LC244, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+404
	movl	at+644, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+592
	movl	$.LC245, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+236
	movl	$.LC246, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+240
	movl	$.LC247, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+636
	movl	$.LC248, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	at+328, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+336
	movl	$.LC249, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+412
	movl	$.LC250, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+416
	movl	$.LC251, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC252, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+224
	movl	$.LC253, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+528
	movl	at+348, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	$.LC254, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	at+340, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+288
	movl	$.LC255, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+200
	movl	at+532, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+448
	movl	at+520, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+80
	movl	at+44, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	$.LC256, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+208
	movl	$.LC257, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+204
	movl	$.LC258, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC259, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+620
	movl	at+656, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+600
	movl	at+8, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC260, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+500
	movl	$.LC261, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+420
	movl	$.LC262, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+488
	movl	$.LC263, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	at+548, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+464
	movl	$.LC264, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+228
	movl	$.LC265, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+496
	movl	$.LC266, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+508
	movl	$.LC267, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC268, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC269, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+200
	movl	$.LC44, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC45, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+160
	movl	$.LC45, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC46, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	$.LC47, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+164
	movl	$.LC47, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+112
	movl	$.LC50, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC54, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+116
	movl	$.LC54, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC53, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+176
	movl	$.LC270, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+136
	movl	$.LC56, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+168
	movl	$.LC271, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC272, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+196
	movl	$.LC66, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+148
	movl	$.LC66, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+152
	movl	$.LC273, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC274, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC275, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC276, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC277, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC278, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC72, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC75, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+188
	movl	$.LC81, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+156
	movl	$.LC89, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+108
	movl	$.LC279, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+124
	movl	$.LC280, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC103, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC105, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC108, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+172
	movl	$.LC281, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC282, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+104
	movl	$.LC110, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+140
	movl	$.LC283, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+120
	movl	$.LC283, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+128
	movl	$.LC284, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC285, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC143, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+184
	movl	$.LC236, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC286, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+180
	movl	$.LC287, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC288, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+96
	movl	$.LC156, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+144
	movl	$.LC159, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+192
	movl	$.LC289, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC290, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+100
	movl	$.LC172, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+132
	movl	at+4, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_file_init__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$155, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_file_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_file_error_nb__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$168, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_open_new_prolog_file__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$168, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$168, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246F70656E5F6E65775F70726F6C6F675F66696C652F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+32, %ecx
	movl	$.Lpred6_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+36, %ecx
	movl	$.Lpred6_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+40, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_open_new_prolog_file1__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$202, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_try_other_directory__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$217, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_close_last_prolog_file__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$235, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_predicate__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$235, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24726561645F7072656469636174652F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$248, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_predicate_next__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$248, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24726561645F7072656469636174655F6E6578742F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$264, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_read_predicate1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$298, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24726561645F707265646963617465312F335F2461757833__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$268, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24726561645F707265646963617465312F335F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$268, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24726561645F707265646963617465312F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$329, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_group_clauses_by_pred__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$329, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467726F75705F636C61757365735F62795F707265642F345F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$329, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467726F75705F636C61757365735F62795F707265642F345F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$345, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_dyn_interf_clause__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$355, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_create_dyn_interf_clause__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$363, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_recover_discontig_clauses__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$372, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_create_exe_clauses_for_dyn_pred__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$384, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_create_exe_clauses_for_pub_pred__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$392, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_file_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$397, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_next_clause__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$402, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C617573652F335F2461757831__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$402, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C617573652F335F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$422, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_next_clause1__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$1, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$1, %edx
	movl	at+136, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred30_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+140, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$502, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F246175783130__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$502, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757839__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$502, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757838__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$492, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757837__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$492, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757836__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$478, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757835__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$467, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757834__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$450, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$450, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F246175783131__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$450, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F246175783132__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$440, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$422, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$422, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6E6578745F636C61757365312F365F246175783133__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$528, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_after_syn_error__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$540, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_dcg_error__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+200, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$549, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_display_singletons__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$549, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24646973706C61795F73696E676C65746F6E732F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$562, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_singletons__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$564, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F73696E676C65746F6E732F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$576, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_handle_directive__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+220, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$727, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_foreign_get_options__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+224, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$734, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_foreign_get_options1__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+228, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+232, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+236, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+236, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred52_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+240, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+244, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$738, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726569676E5F6765745F6F7074696F6E73312F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+248, %ecx
	movl	$.Lpred53_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+252, %ecx
	movl	$.Lpred53_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+256, %ecx
	movl	$.Lpred53_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+260, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$768, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_foreign_check_types__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+264, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$771, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726569676E5F636865636B5F74797065732F345F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$3, %edx
	movl	at+268, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$3, %edx
	movl	at+272, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$3, %edx
	movl	at+276, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+280, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$789, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_foreign_check_arg__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$16, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+284, %ecx
	movl	$.Lpred56_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+288, %ecx
	movl	$.Lpred56_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+292, %ecx
	movl	$.Lpred56_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+296, %ecx
	movl	$.Lpred56_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+300, %ecx
	movl	$.Lpred56_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+252, %ecx
	movl	$.Lpred56_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+304, %ecx
	movl	$.Lpred56_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+308, %ecx
	movl	$.Lpred56_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+312, %ecx
	movl	$.Lpred56_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+316, %ecx
	movl	$.Lpred56_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+320, %ecx
	movl	$.Lpred56_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+324, %ecx
	movl	$.Lpred56_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+328, %ecx
	movl	$.Lpred56_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+332, %ecx
	movl	$.Lpred56_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+336, %ecx
	movl	$.Lpred56_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$16, %edx
	movl	at+340, %ecx
	movl	$.Lpred56_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+344, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$816, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_handle_initialization__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+348, %ecx
	movl	$.Lpred57_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+8, %ecx
	movl	$.Lpred57_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+352, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$825, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_exec_directive__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+356, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$831, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_exec_directive_exception__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+360, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$836, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_used_bips_via_call__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+364, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$845, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_empty_dyn__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+28
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+368, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred61_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+372, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred61_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+376, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$858, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246164645F656D7074795F64796E2F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+380, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$868, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_ensure_linked__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+32
	movl	st+32, %eax
	movl	$2, %edx
	movl	at+368, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred63_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+32, %eax
	movl	$2, %edx
	movl	at+372, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred63_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+384, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$890, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_module_export_info__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+36
	movl	st+36, %eax
	movl	$2, %edx
	movl	at+368, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred64_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+36, %eax
	movl	$2, %edx
	movl	at+372, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred64_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+388, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$907, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246164645F6D6F64756C655F6578706F72745F696E666F2F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+392, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$916, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_check_module_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+396, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$936, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_check_head_is_module_free__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+400, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$945, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_check_module_clash__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+404, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$957, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_owner_module__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+408, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$965, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_is_exported__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+412, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$971, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_get_module_of_cur_pred__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+416, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$971, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246765745F6D6F64756C655F6F665F6375725F707265642F315F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+420, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$984, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_flag_for_preds__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+40
	movl	st+40, %eax
	movl	$2, %edx
	movl	at+368, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred73_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$2, %edx
	movl	at+372, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred73_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+424, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$997, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F666C61675F666F725F70726564732F325F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+428, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$997, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F666C61675F666F725F70726564732F325F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+44
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+432, %ecx
	movl	$.Lpred75_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+436, %ecx
	movl	$.Lpred75_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+440, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1014, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_define_predicate__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+444, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1034, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_flag_bit__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$9, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+48
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+448, %ecx
	movl	$.Lpred77_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+452, %ecx
	movl	$.Lpred77_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+456, %ecx
	movl	$.Lpred77_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+432, %ecx
	movl	$.Lpred77_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+436, %ecx
	movl	$.Lpred77_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+460, %ecx
	movl	$.Lpred77_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+464, %ecx
	movl	$.Lpred77_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+468, %ecx
	movl	$.Lpred77_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$9, %edx
	movl	at+472, %ecx
	movl	$.Lpred77_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+476, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$1047, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_set_pred_info__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+480, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$1047, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F707265645F696E666F2F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+484, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$1058, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_unset_pred_info__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+488, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$1069, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_test_pred_info__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+492, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1077, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_check_predicate__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+496, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1100, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_bip__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+500, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1108, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_control_construct__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$9, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+52
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+368, %ecx
	movl	$.Lpred84_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+504, %ecx
	movl	$.Lpred84_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+508, %ecx
	movl	$.Lpred84_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+512, %ecx
	movl	$.Lpred84_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+516, %ecx
	movl	$.Lpred84_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+520, %ecx
	movl	$.Lpred84_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+524, %ecx
	movl	$.Lpred84_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+528, %ecx
	movl	$.Lpred84_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$9, %edx
	movl	at+532, %ecx
	movl	$.Lpred84_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+536, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1122, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_suspicious_predicate__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$9, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+56
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+540, %ecx
	movl	$.Lpred85_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+136, %ecx
	movl	$.Lpred85_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+544, %ecx
	movl	$.Lpred85_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+548, %ecx
	movl	$.Lpred85_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+268, %ecx
	movl	$.Lpred85_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+272, %ecx
	movl	$.Lpred85_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+552, %ecx
	movl	$.Lpred85_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+372, %ecx
	movl	$.Lpred85_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+56, %eax
	movl	$9, %edx
	movl	at+556, %ecx
	movl	$.Lpred85_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+560, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1136, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_warn__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+564, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1142, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_error__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+568, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$1142, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246572726F722F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+572, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$1154, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_disp_msg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+576, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1154, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24646973705F6D73672F345F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+580, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$1171, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_disp_file_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+584, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$1181, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_disp_lines__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$1, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+60
	movl	st+60, %eax
	movl	$1, %edx
	movl	at+272, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred92_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+588, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$1190, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_disp_column__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+592, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$1202, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_exception__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$1, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+64
	movl	st+64, %eax
	movl	$1, %edx
	movl	at+564, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred94_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+216, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$581, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_handle_directive__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$18, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+68
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+596, %ecx
	movl	$.Lpred95_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+600, %ecx
	movl	$.Lpred95_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+604, %ecx
	movl	$.Lpred95_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+608, %ecx
	movl	$.Lpred95_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+612, %ecx
	movl	$.Lpred95_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+616, %ecx
	movl	$.Lpred95_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+620, %ecx
	movl	$.Lpred95_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+624, %ecx
	movl	$.Lpred95_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+628, %ecx
	movl	$.Lpred95_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+632, %ecx
	movl	$.Lpred95_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+636, %ecx
	movl	$.Lpred95_24+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+640, %ecx
	movl	$.Lpred95_26+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+644, %ecx
	movl	$.Lpred95_28+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+648, %ecx
	movl	$.Lpred95_30+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+652, %ecx
	movl	$.Lpred95_32+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+656, %ecx
	movl	$.Lpred95_34+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+660, %ecx
	movl	$.Lpred95_36+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+68, %eax
	movl	$18, %edx
	movl	at+664, %ecx
	movl	$.Lpred95_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+668, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$698, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757837__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+672, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$680, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757836__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+676, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$664, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757835__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+680, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$650, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757834__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+684, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$620, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+688, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$612, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+692, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$604, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2468616E646C655F6469726563746976652F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	System_Directives,@function

System_Directives:
	pushl	%esi
	subl	$132, %esp
	movl	at+0, %eax
	movl	%eax, 0(%esp)
	movl	$125, 4(%esp)
	movl	$1, 8(%esp)
	movl	$directive_1+0, 12(%esp)
	call	Pl_Execute_Directive
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
.LC175:
	.string	" including "
.LC0:
	.string	"!"
.LC174:
	.string	""
.LC176:
	.string	"#"
.LC177:
	.string	"$$empty$$predicate$$clause$$"
.LC267:
	.string	"$add_clause_term"
.LC1:
	.string	"$add_empty_dyn/2_$aux1"
.LC2:
	.string	"$add_module_export_info/2_$aux1"
.LC268:
	.string	"$call_c"
.LC3:
	.string	"$disp_msg/4_$aux1"
.LC4:
	.string	"$display_singletons/2_$aux1"
.LC5:
	.string	"$error/2_$aux1"
.LC178:
	.string	"$exe_system"
.LC179:
	.string	"$exe_user"
.LC180:
	.string	"$force_foreign_link"
.LC269:
	.string	"$foreign_call_c"
.LC6:
	.string	"$foreign_check_types/4_$aux1"
.LC7:
	.string	"$foreign_get_options1/1_$aux1"
.LC8:
	.string	"$get_module_of_cur_pred/1_$aux1"
.LC9:
	.string	"$get_next_clause/3_$aux1"
.LC10:
	.string	"$get_next_clause/3_$aux2"
.LC11:
	.string	"$get_next_clause1/6_$aux1"
.LC12:
	.string	"$get_next_clause1/6_$aux10"
.LC13:
	.string	"$get_next_clause1/6_$aux11"
.LC14:
	.string	"$get_next_clause1/6_$aux12"
.LC15:
	.string	"$get_next_clause1/6_$aux13"
.LC16:
	.string	"$get_next_clause1/6_$aux2"
.LC17:
	.string	"$get_next_clause1/6_$aux3"
.LC18:
	.string	"$get_next_clause1/6_$aux4"
.LC19:
	.string	"$get_next_clause1/6_$aux5"
.LC20:
	.string	"$get_next_clause1/6_$aux6"
.LC21:
	.string	"$get_next_clause1/6_$aux7"
.LC22:
	.string	"$get_next_clause1/6_$aux8"
.LC23:
	.string	"$get_next_clause1/6_$aux9"
.LC24:
	.string	"$get_singletons/2_$aux1"
.LC25:
	.string	"$group_clauses_by_pred/4_$aux1"
.LC26:
	.string	"$group_clauses_by_pred/4_$aux2"
.LC27:
	.string	"$handle_directive/3_$aux1"
.LC28:
	.string	"$handle_directive/3_$aux2"
.LC29:
	.string	"$handle_directive/3_$aux3"
.LC30:
	.string	"$handle_directive/3_$aux4"
.LC31:
	.string	"$handle_directive/3_$aux5"
.LC32:
	.string	"$handle_directive/3_$aux6"
.LC33:
	.string	"$handle_directive/3_$aux7"
.LC34:
	.string	"$open_new_prolog_file/1_$aux1"
.LC35:
	.string	"$open_new_prolog_file/1_$aux2"
.LC36:
	.string	"$read_predicate/3_$aux1"
.LC37:
	.string	"$read_predicate1/3_$aux1"
.LC38:
	.string	"$read_predicate1/3_$aux2"
.LC39:
	.string	"$read_predicate1/3_$aux3"
.LC40:
	.string	"$read_predicate_next/3_$aux1"
.LC41:
	.string	"$set_flag_for_preds/2_$aux1"
.LC42:
	.string	"$set_flag_for_preds/2_$aux2"
.LC43:
	.string	"$set_pred_info/3_$aux1"
.LC181:
	.string	"&"
.LC44:
	.string	"*"
.LC45:
	.string	"+"
.LC46:
	.string	","
.LC47:
	.string	"-"
.LC48:
	.string	"-->"
.LC49:
	.string	"->"
.LC50:
	.string	"/"
.LC51:
	.string	"//"
.LC52:
	.string	"/home/diaz/GP/src/Pl2Wam/read_file.pl"
.LC182:
	.string	": "
.LC53:
	.string	":"
.LC54:
	.string	":-"
.LC183:
	.string	":~d"
.LC184:
	.string	":~d-~d"
.LC185:
	.string	":~d: "
.LC55:
	.string	";"
.LC270:
	.string	"="
.LC56:
	.string	"?"
.LC186:
	.string	"DCG error raised: ~w"
.LC271:
	.string	"Pl_Emit_BC_Execute_Wrapper"
.LC57:
	.string	"\n"
.LC187:
	.string	"_"
.LC58:
	.string	"add_dyn_interf_clause"
.LC59:
	.string	"add_empty_dyn"
.LC60:
	.string	"add_ensure_linked"
.LC61:
	.string	"add_module_export_info"
.LC62:
	.string	"after_syn_error"
.LC188:
	.string	"any"
.LC272:
	.string	"args"
.LC63:
	.string	"atom"
.LC64:
	.string	"bfd"
.LC65:
	.string	"bip"
.LC66:
	.string	"bip_name"
.LC67:
	.string	"boolean"
.LC68:
	.string	"bpl"
.LC273:
	.string	"buff_aux_pred"
.LC274:
	.string	"buff_clause"
.LC275:
	.string	"buff_discontig_clause"
.LC276:
	.string	"buff_dyn_interf_clause"
.LC277:
	.string	"buff_exe_system"
.LC278:
	.string	"buff_exe_user"
.LC69:
	.string	"built_in"
.LC70:
	.string	"built_in_fd"
.LC189:
	.string	"by_value"
.LC71:
	.string	"byte"
.LC72:
	.string	"call"
.LC190:
	.string	"call_c"
.LC191:
	.string	"cannot open file ~a - does not exist"
.LC192:
	.string	"cannot open file ~a - permission error"
.LC73:
	.string	"catch"
.LC74:
	.string	"char"
.LC75:
	.string	"char_conversion"
.LC76:
	.string	"chars"
.LC77:
	.string	"check_head_is_module_free"
.LC78:
	.string	"check_module_clash"
.LC79:
	.string	"check_module_name"
.LC80:
	.string	"check_predicate"
.LC81:
	.string	"choice_size"
.LC193:
	.string	"clash on ~q - defined in module ~q (here) and imported from ~w"
.LC82:
	.string	"close_last_prolog_file"
.LC83:
	.string	"code"
.LC84:
	.string	"codes"
.LC85:
	.string	"control_construct"
.LC86:
	.string	"create_dyn_interf_clause"
.LC87:
	.string	"create_exe_clauses_for_dyn_pred"
.LC88:
	.string	"create_exe_clauses_for_pub_pred"
.LC89:
	.string	"dcg_error"
.LC90:
	.string	"def"
.LC194:
	.string	"default_kind"
.LC91:
	.string	"define_predicate"
.LC197:
	.string	"directive failed (~q) with exception (~q)"
.LC196:
	.string	"directive failed (~q)"
.LC198:
	.string	"directive module/2 already declared"
.LC199:
	.string	"directive occurs after definition of ~q - directive ignored"
.LC195:
	.string	"directive"
.LC92:
	.string	"discontig"
.LC200:
	.string	"discontiguous predicate ~q - clause ignored"
.LC93:
	.string	"discontiguous"
.LC94:
	.string	"disp_column"
.LC95:
	.string	"disp_file_name"
.LC96:
	.string	"disp_lines"
.LC97:
	.string	"disp_msg"
.LC98:
	.string	"display_singletons"
.LC99:
	.string	"dyn"
.LC100:
	.string	"dynamic"
.LC201:
	.string	"elif directive caused exception: ~w"
.LC279:
	.string	"elif"
.LC202:
	.string	"else"
.LC280:
	.string	"empty_dyn_pred"
.LC203:
	.string	"encoding directive not supported - directive ignored"
.LC101:
	.string	"encoding"
.LC102:
	.string	"end_of_file"
.LC205:
	.string	"endif directive expected"
.LC204:
	.string	"endif"
.LC206:
	.string	"ensure_linked directive ignored in byte-code compilation mode"
.LC103:
	.string	"ensure_linked"
.LC207:
	.string	"ensure_loaded directive not supported - directive ignored"
.LC104:
	.string	"ensure_loaded"
.LC208:
	.string	"eof_reached"
.LC105:
	.string	"error"
.LC209:
	.string	"exception raised: ~q"
.LC106:
	.string	"exception"
.LC107:
	.string	"exec_directive"
.LC108:
	.string	"exec_directive_exception"
.LC281:
	.string	"existence_error"
.LC282:
	.string	"expand_term"
.LC210:
	.string	"f"
.LC109:
	.string	"fail"
.LC211:
	.string	"false"
.LC212:
	.string	"fatal error"
.LC110:
	.string	"fct_name"
.LC111:
	.string	"flag_bit"
.LC112:
	.string	"float"
.LC213:
	.string	"foreign directive ignored (not allowed in this mode)"
.LC214:
	.string	"foreign predicate ~q should not be public/dynamic"
.LC113:
	.string	"foreign"
.LC215:
	.string	"foreign_bip"
.LC114:
	.string	"foreign_check_arg"
.LC115:
	.string	"foreign_check_types"
.LC216:
	.string	"foreign_choice_size"
.LC217:
	.string	"foreign_fct_name"
.LC116:
	.string	"foreign_get_options"
.LC117:
	.string	"foreign_get_options1"
.LC218:
	.string	"foreign_only"
.LC219:
	.string	"foreign_return"
.LC220:
	.string	"fx"
.LC118:
	.string	"get_file_name"
.LC119:
	.string	"get_module_of_cur_pred"
.LC120:
	.string	"get_next_clause"
.LC121:
	.string	"get_next_clause1"
.LC122:
	.string	"get_owner_module"
.LC123:
	.string	"get_singletons"
.LC124:
	.string	"group_clauses_by_pred"
.LC125:
	.string	"handle_directive"
.LC126:
	.string	"handle_initialization"
.LC221:
	.string	"head is a variable"
.LC222:
	.string	"head is not a callable (~q)"
.LC223:
	.string	"if directive caused exception: ~w"
.LC283:
	.string	"if"
.LC224:
	.string	"if_stack"
.LC225:
	.string	"in"
.LC127:
	.string	"in_byte"
.LC226:
	.string	"in_bytes"
.LC128:
	.string	"in_char"
.LC129:
	.string	"in_code"
.LC227:
	.string	"in_lines"
.LC228:
	.string	"in_out"
.LC130:
	.string	"include"
.LC131:
	.string	"initialization"
.LC132:
	.string	"integer"
.LC229:
	.string	"invalid directive ~q"
.LC230:
	.string	"invalid module name (~q) should be an atom"
.LC231:
	.string	"invalide directive meta_predicate/1 ~w"
.LC133:
	.string	"is_exported"
.LC134:
	.string	"jump"
.LC135:
	.string	"meta"
.LC284:
	.string	"meta_pred"
.LC136:
	.string	"meta_predicate"
.LC232:
	.string	"module qualification is not allowed for the head of a clause (~w)"
.LC137:
	.string	"module"
.LC233:
	.string	"module_already_seen"
.LC285:
	.string	"module_export"
.LC138:
	.string	"multi"
.LC139:
	.string	"multifile"
.LC234:
	.string	"native_code"
.LC140:
	.string	"need_cut_level"
.LC141:
	.string	"none"
.LC142:
	.string	"number"
.LC235:
	.string	"off"
.LC143:
	.string	"op"
.LC236:
	.string	"open"
.LC237:
	.string	"open_file_stack"
.LC144:
	.string	"open_new_prolog_file"
.LC145:
	.string	"open_new_prolog_file1"
.LC238:
	.string	"out"
.LC286:
	.string	"permission_error"
.LC146:
	.string	"positive"
.LC287:
	.string	"pred_info"
.LC239:
	.string	"predicate ~w already exported from module ~w"
.LC147:
	.string	"pub"
.LC148:
	.string	"public"
.LC240:
	.string	"read"
.LC149:
	.string	"read_file_error_nb"
.LC150:
	.string	"read_file_init"
.LC151:
	.string	"read_file_term"
.LC152:
	.string	"read_predicate"
.LC153:
	.string	"read_predicate1"
.LC154:
	.string	"read_predicate_next"
.LC288:
	.string	"read_term"
.LC241:
	.string	"reading_dyn_pred"
.LC155:
	.string	"recover_discontig_clauses"
.LC242:
	.string	"redef_error"
.LC243:
	.string	"redefining built-in predicate ~q"
.LC244:
	.string	"redefining control construct ~q"
.LC156:
	.string	"return"
.LC157:
	.string	"set_flag_for_preds"
.LC158:
	.string	"set_pred_info"
.LC159:
	.string	"set_prolog_flag"
.LC245:
	.string	"singl_warn"
.LC246:
	.string	"singleton variables ~w for ~q"
.LC247:
	.string	"singleton_warning"
.LC289:
	.string	"singletons"
.LC248:
	.string	"source_sink"
.LC160:
	.string	"string"
.LC249:
	.string	"susp_warn"
.LC250:
	.string	"suspicious predicate ~q"
.LC161:
	.string	"suspicious_predicate"
.LC251:
	.string	"syn_error_nb"
.LC252:
	.string	"syntax error"
.LC253:
	.string	"syntax error: ~a (char:~d)"
.LC290:
	.string	"syntax_error"
.LC162:
	.string	"system"
.LC254:
	.string	"t"
.LC163:
	.string	"term"
.LC164:
	.string	"test_pred_info"
.LC255:
	.string	"then"
.LC165:
	.string	"throw"
.LC166:
	.string	"true"
.LC167:
	.string	"try_other_directory"
.LC256:
	.string	"unexpected elif directive"
.LC257:
	.string	"unexpected else directive"
.LC258:
	.string	"unexpected endif directive"
.LC259:
	.string	"unknown directive ~q - maybe use initialization/1 - directive ignored"
.LC168:
	.string	"unset_pred_info"
.LC169:
	.string	"use_module"
.LC170:
	.string	"used_bips_via_call"
.LC171:
	.string	"user"
.LC260:
	.string	"user_output"
.LC261:
	.string	"using system auxiliary predicate ~q"
.LC172:
	.string	"warn"
.LC262:
	.string	"warning"
.LC263:
	.string	"where"
.LC173:
	.string	"{}"
.LC264:
	.string	"~a"
.LC265:
	.string	"~a: "
.LC266:
	.string	"~a~a"
	.data	
	.align	4
	.local	at
	.comm	at,696,4
	.local	fn
	.comm	fn,204,4
	.local	st
	.comm	st,72,4
	.local	ta
	.comm	ta,656,4
	.section	.note.GNU-stack,"",@progbits
