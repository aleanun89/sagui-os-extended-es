# Pl2Wam/pl2wam.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/pl2wam.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_pl2wam__a1,@function
.globl X0_pl2wam__a1

X0_pl2wam__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred1_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_pl2wam__a1

.Lpred1_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	jmp	X1_246361746368__a6
	.p2align	4,,15
	.type	X0_pl2wam1__a1,@function
.globl X0_pl2wam1__a1

X0_pl2wam1__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$13, %eax
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
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_cmd_line_args__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_prolog_file_name__a2
.Lcont2:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_compile_msg_start__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_read_file_init__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_emit_code_init__a2
.Lcont5:
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_init_counters__a0
.Lcont6:
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_read_predicate__a3
.Lcont8:
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_24706C3277616D312F315F2461757831__a5
.Lcont10:
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_emit_ensure_linked__a0
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_read_file_term__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_emit_code_term__a2
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	leal	-64(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_read_file_error_nb__a1
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-64(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-56(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-60(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_24706C3277616D312F315F2461757832__a6
	.p2align	4,,15
	.type	X1_24706C3277616D312F315F2461757832__a6,@function

X1_24706C3277616D312F315F2461757832__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred3_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$5, %eax
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, -32(%edi)
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_display_counters__a0
.Lcont15:
	movl	pl_reg_bank+1044, %edi
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
	call	Pl_Deallocate
	jmp	X0_compile_msg_end__a5

.Lpred3_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont16:
	call	Pl_Deallocate
	jmp	X0_abort__a0
	.p2align	4,,15
	.type	X1_24706C3277616D312F315F2461757831__a5,@function

X1_24706C3277616D312F315F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred4_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$4, %eax
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
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_read_file_error_nb__a1
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_compile_and_emit_pred__a4
.Lcont18:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_compile_and_emit_pred__a4,@function
.globl X0_compile_and_emit_pred__a4

X0_compile_and_emit_pred__a4:
	movl	$.Lpred5_2+0, %eax
	movl	$.Lpred5_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred5_1:
	movl	st+0, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred5_2:
	movl	$.Lpred5_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred5_3:
	movl	$7, %eax
	call	Pl_Allocate
	movl	ta+24, %eax
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
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_compile_emit_inits__a5
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_compile_lst_clause__a2
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_indexing__a2
.Lcont21:
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_emit_code__a5
.Lcont23:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_counter__a2

.Lpred5_4:
	call	Pl_Delete_Choice_Point4

.Lpred5_5:
	movl	$6, %eax
	call	Pl_Allocate
	movl	ta+44, %eax
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
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_compile_emit_inits__a5
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_bc_compile_lst_clause__a2
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_bc_emit_code__a5
.Lcont26:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_counter__a2
	.p2align	4,,15
	.type	X0_compile_emit_inits__a5,@function
.globl X0_compile_emit_inits__a5

X0_compile_emit_inits__a5:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+12, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
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
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_absolute_file_name__a2
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_syntactic_sugar_init_pred__a3
	.p2align	4,,15
	.type	X0_compile_lst_clause__a2,@function
.globl X0_compile_lst_clause__a2

X0_compile_lst_clause__a2:
	movl	$.Lpred7_1+0, %eax
	movl	$.Lpred7_2+0, %edx
	movl	$.Lpred7_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred7_1:
	movl	$.Lpred7_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred7_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred7_3:
	call	Pl_Delete_Choice_Point2

.Lpred7_4:
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
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+20, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_compile_clause__a3
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_compile_lst_clause__a2
	.p2align	4,,15
	.type	X0_compile_clause__a3,@function
.globl X0_compile_clause__a3

X0_compile_clause__a3:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_syntactic_sugar__a3
.Lcont29:
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
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
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_internal_format__a6
.Lcont31:
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_code_generation__a5
.Lcont33:
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_allocate_registers__a1
.Lcont35:
	movl	ta+84, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_add_counter__a2
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X0_find_first_arg__a2
.Lcont37:
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_add_counter__a2
	.p2align	4,,15
	.type	X0_bc_compile_lst_clause__a2,@function
.globl X0_bc_compile_lst_clause__a2

X0_bc_compile_lst_clause__a2:
	movl	$.Lpred9_1+0, %eax
	movl	$.Lpred9_2+0, %edx
	movl	$.Lpred9_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred9_1:
	movl	$.Lpred9_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred9_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred9_3:
	call	Pl_Delete_Choice_Point2

.Lpred9_4:
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
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+24, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X0_compile_clause__a3
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_bc_compile_lst_clause__a2
	.p2align	4,,15
	.type	X0_compile_msg_start__a2,@function
.globl X0_compile_msg_start__a2

X0_compile_msg_start__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	call	Pl_Delete_Choice_Point3
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
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_24636F6D70696C655F6D73675F73746172742F325F2461757831__a2
.Lcont39:
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
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
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont40:
	call	Pl_Deallocate
	jmp	X0_flush_output__a0
	.p2align	4,,15
	.type	X1_24636F6D70696C655F6D73675F73746172742F325F2461757831__a2,@function

X1_24636F6D70696C655F6D73675F73746172742F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+108, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+112, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_compile_msg_end__a5,@function
.globl X0_compile_msg_end__a5

X0_compile_msg_end__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred12_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
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
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_real_time__a1
.Lcont41:
	movl	ta+116, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
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
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X0_cur_pred__a2,@function
.globl X0_cur_pred__a2

X0_cur_pred__a2:
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_cur_pred_without_aux__a2,@function
.globl X0_cur_pred_without_aux__a2

X0_cur_pred_without_aux__a2:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_cur_pred__a2
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_24707265645F776974686F75745F617578__a4
	.p2align	4,,15
	.type	X0_init_counters__a0,@function
.globl X0_init_counters__a0

X0_init_counters__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred15_1:
	call	Pl_Delete_Choice_Point1
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
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+84, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
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
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_last_times__a2
	.p2align	4,,15
	.type	X0_add_counter__a2,@function
.globl X0_add_counter__a2

X0_add_counter__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred16_1:
	call	Pl_Delete_Choice_Point3
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_last_times__a2
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_last_times__a2,@function
.globl X0_last_times__a2

X0_last_times__a2:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+124, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_statistics__a2
.Lcont44:
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_statistics__a2
	.p2align	4,,15
	.type	X0_display_counters__a0,@function
.globl X0_display_counters__a0

X0_display_counters__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred18_1:
	call	Pl_Delete_Choice_Point1
	movl	$22, %eax
	call	Pl_Allocate
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
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
	movl	ta+60, %eax
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
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
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
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+84, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-64(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-68(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-72(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-76(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-24(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-48(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-72(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -80(%edi)
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-28(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-52(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-60(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-60(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	-76(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Add
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -84(%edi)
	leal	-88(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_user_time__a1
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	leal	-92(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_real_time__a1
.Lcont46:
	movl	pl_reg_bank+1044, %edi
	movl	-88(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-80(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Sub
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -96(%edi)
	movl	-92(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-84(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Sub
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -100(%edi)
	movl	ta+132, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont47:
	movl	ta+136, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
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
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont48:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont49:
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont50:
	movl	ta+148, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont51:
	movl	ta+152, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-52(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont52:
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-56(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-60(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont53:
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-64(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-68(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont54:
	movl	ta+164, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-72(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-76(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont55:
	movl	ta+168, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-96(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-100(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont56:
	movl	ta+172, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-88(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-92(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X0_cmd_line_args__a3,@function
.globl X0_cmd_line_args__a3

X0_cmd_line_args__a3:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+180, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+180, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+180, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+200, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+204, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+212, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+216, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+232, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_cmd_line_args__a1
.Lcont57:
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X1_24636D645F6C696E655F617267732F335F2461757831__a1
.Lcont58:
	movl	ta+184, %eax
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
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267732F335F2461757831__a1,@function

X1_24636D645F6C696E655F617267732F335F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred20_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+240, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont59:
	call	Pl_Deallocate
	jmp	X0_abort__a0

.Lpred20_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_cmd_line_args__a1,@function
.globl X0_cmd_line_args__a1

X0_cmd_line_args__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred21_1+0, %eax
	movl	$.Lpred21_2+0, %edx
	movl	$.Lpred21_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred21_1:
	movl	$.Lpred21_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred21_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred21_3:
	call	Pl_Delete_Choice_Point2

.Lpred21_4:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X0_cmd_line_arg1__a3
.Lcont60:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_cmd_line_args__a1
	.p2align	4,,15
	.type	X0_cmd_line_arg1__a3,@function
.globl X0_cmd_line_arg1__a3

X0_cmd_line_arg1__a3:
	movl	$.Lpred22_52+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred22_2+0, %eax
	movl	$.Lpred22_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred22_1:
	movl	st+4, %eax
	movl	$25, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred22_2:
	movl	$.Lpred22_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred22_3:
	movl	ta+244, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_cmd_line_arg1__a3

.Lpred22_4:
	movl	$.Lpred22_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_5:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+248, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X1_24636D645F6C696E655F617267312F335F2461757831__a3
.Lcont61:
	movl	ta+184, %eax
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
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X1_24636D645F6C696E655F617267312F335F2461757832__a1
.Lcont62:
	movl	ta+184, %eax
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

.Lpred22_6:
	movl	$.Lpred22_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_7:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+252, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X0_read_pl_state_file__a1
.Lcont63:
	call	Pl_Deallocate
	jmp	X1_24636D645F6C696E655F617267312F335F2461757833__a0

.Lpred22_8:
	movl	$.Lpred22_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_9:
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_cmd_line_arg1__a3

.Lpred22_10:
	movl	$.Lpred22_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_11:
	movl	ta+260, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_12:
	movl	$.Lpred22_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_13:
	movl	ta+264, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+268, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_cmd_line_arg1__a3

.Lpred22_14:
	movl	$.Lpred22_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_15:
	movl	ta+268, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+212, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_16:
	movl	$.Lpred22_18+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_17:
	movl	ta+272, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_18:
	movl	$.Lpred22_20+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_19:
	movl	ta+276, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_20:
	movl	$.Lpred22_22+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_21:
	movl	ta+280, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_22:
	movl	$.Lpred22_24+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_23:
	movl	ta+284, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+200, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_24:
	movl	$.Lpred22_26+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_25:
	movl	ta+288, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+204, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_26:
	movl	$.Lpred22_28+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_27:
	movl	ta+292, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_28:
	movl	$.Lpred22_30+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_29:
	movl	ta+296, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+212, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_30:
	movl	$.Lpred22_32+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_31:
	movl	ta+300, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_32:
	movl	$.Lpred22_34+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_33:
	movl	ta+304, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_34:
	movl	$.Lpred22_36+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_35:
	movl	ta+308, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_36:
	movl	$.Lpred22_38+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_37:
	movl	ta+312, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_38:
	movl	$.Lpred22_40+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_39:
	movl	ta+316, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_40:
	movl	$.Lpred22_42+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_41:
	movl	ta+320, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+232, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_42:
	movl	$.Lpred22_44+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_43:
	movl	ta+324, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_44:
	movl	$.Lpred22_46+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_45:
	movl	ta+328, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred22_46:
	movl	$.Lpred22_48+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_47:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+332, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X0_display_copying__a0
.Lcont64:
	call	Pl_Deallocate
	jmp	X0_stop__a0

.Lpred22_48:
	movl	$.Lpred22_50+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred22_49:
	movl	ta+336, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+340, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_cmd_line_arg1__a3

.Lpred22_50:
	call	Pl_Delete_Choice_Point3

.Lpred22_51:
	movl	ta+340, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	X1_24636D645F6C696E655F617267312F335F2461757834__a0

.Lpred22_52:
	movl	$.Lpred22_53+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+344, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont65:
	movl	ta+348, %eax
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
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont66:
	call	Pl_Deallocate
	jmp	X0_abort__a0

.Lpred22_53:
	call	Pl_Delete_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+176, %eax
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
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X1_24636D645F6C696E655F617267312F335F2461757835__a1
.Lcont67:
	movl	ta+176, %eax
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
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267312F335F2461757835__a1,@function

X1_24636D645F6C696E655F617267312F335F2461757835__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred23_1:
	call	Pl_Delete_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+352, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont68:
	call	Pl_Deallocate
	jmp	X0_abort__a0
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267312F335F2461757834__a0,@function

X1_24636D645F6C696E655F617267312F335F2461757834__a0:
	movl	$.Lpred24_1+0, %eax
	movl	$0, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X0_h__a1
.Lcont69:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X0_write__a1
.Lcont70:
	movl	$.Lcont71, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont71:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred24_1:
	movl	$0, %eax
	call	Pl_Delete_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	$.Lcont72, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont72:
	movl	ta+356, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont73, pl_reg_bank+1040
	jmp	X0_write__a1
.Lcont73:
	movl	$.Lcont74, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont74:
	call	Pl_Deallocate
	jmp	X0_stop__a0
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267312F335F2461757833__a0,@function

X1_24636D645F6C696E655F617267312F335F2461757833__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred25_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+360, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+364, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont75, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont75:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred25_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267312F335F2461757832__a1,@function

X1_24636D645F6C696E655F617267312F335F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred26_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred26_1:
	call	Pl_Delete_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+368, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont76, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont76:
	call	Pl_Deallocate
	jmp	X0_abort__a0
	.p2align	4,,15
	.type	X1_24636D645F6C696E655F617267312F335F2461757831__a3,@function

X1_24636D645F6C696E655F617267312F335F2461757831__a3:
	movl	$.Lpred27_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
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
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont77, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont77:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	ta+344, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_1:
	call	Pl_Delete_Choice_Point3
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+372, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont78, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont78:
	call	Pl_Deallocate
	jmp	X0_abort__a0
	.p2align	4,,15
	.type	X0_display_copying__a0,@function
.globl X0_display_copying__a0

X0_display_copying__a0:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont79, pl_reg_bank+1040
	jmp	X0_prolog_name__a1
.Lcont79:
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont80, pl_reg_bank+1040
	jmp	X0_prolog_version__a1
.Lcont80:
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont81, pl_reg_bank+1040
	jmp	X0_prolog_copyright__a1
.Lcont81:
	movl	ta+376, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
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
	movl	$.Lcont82, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont82:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont83, pl_reg_bank+1040
	jmp	X0_write__a1
.Lcont83:
	movl	$.Lcont84, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont84:
	movl	$.Lcont85, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont85:
	movl	ta+380, %eax
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
	movl	$.Lcont86, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont86:
	movl	ta+384, %eax
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
	movl	$.Lcont87, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont87:
	movl	ta+388, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont88, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont88:
	movl	ta+392, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont89, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont89:
	movl	ta+396, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X0_prolog_name__a1,@function
.globl X0_prolog_name__a1

X0_prolog_name__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+400, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_prolog_flag__a2
	.p2align	4,,15
	.type	X0_prolog_version__a1,@function
.globl X0_prolog_version__a1

X0_prolog_version__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+404, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_prolog_flag__a2
	.p2align	4,,15
	.type	X0_prolog_date__a1,@function
.globl X0_prolog_date__a1

X0_prolog_date__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+408, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_prolog_flag__a2
	.p2align	4,,15
	.type	X0_prolog_copyright__a1,@function
.globl X0_prolog_copyright__a1

X0_prolog_copyright__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+412, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_prolog_flag__a2
	.p2align	4,,15
	.type	X0_h__a1,@function
.globl X0_h__a1

X0_h__a1:
	movl	$.Lpred33_3+0, %eax
	movl	$.Lpred33_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred33_1:
	movl	st+8, %eax
	movl	$25, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred33_2:
	movl	$.Lpred33_sub_0+0, %eax
	call	Pl_Create_Choice_Point1
	jmp	.Lpred33_6

.Lpred33_sub_0:
	call	Pl_Delete_Choice_Point1
	jmp	.Lpred33_52

.Lpred33_3:
	movl	$.Lpred33_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred33_4:
	movl	ta+416, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_5:
	movl	$.Lpred33_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_6:
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_7:
	movl	$.Lpred33_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_8:
	movl	ta+420, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_9:
	movl	$.Lpred33_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_10:
	movl	ta+424, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_11:
	movl	$.Lpred33_13+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_12:
	movl	ta+428, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_13:
	movl	$.Lpred33_15+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_14:
	movl	ta+432, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_15:
	movl	$.Lpred33_17+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_16:
	movl	ta+436, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_17:
	movl	$.Lpred33_19+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_18:
	movl	ta+440, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_19:
	movl	$.Lpred33_21+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_20:
	movl	ta+444, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_21:
	movl	$.Lpred33_23+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_22:
	movl	ta+448, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_23:
	movl	$.Lpred33_25+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_24:
	movl	ta+452, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_25:
	movl	$.Lpred33_27+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_26:
	movl	ta+456, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_27:
	movl	$.Lpred33_29+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_28:
	movl	ta+460, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_29:
	movl	$.Lpred33_31+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_30:
	movl	ta+464, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_31:
	movl	$.Lpred33_33+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_32:
	movl	ta+468, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_33:
	movl	$.Lpred33_35+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_34:
	movl	ta+472, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_35:
	movl	$.Lpred33_37+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_36:
	movl	ta+476, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_37:
	movl	$.Lpred33_39+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_38:
	movl	ta+480, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_39:
	movl	$.Lpred33_41+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_40:
	movl	ta+484, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_41:
	movl	$.Lpred33_43+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_42:
	movl	ta+488, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_43:
	movl	$.Lpred33_45+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_44:
	movl	ta+492, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_45:
	movl	$.Lpred33_47+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_46:
	movl	ta+496, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_47:
	movl	$.Lpred33_49+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_48:
	movl	ta+500, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_49:
	movl	$.Lpred33_51+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_50:
	movl	ta+504, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_51:
	movl	$.Lpred33_53+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred33_52:
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_53:
	call	Pl_Delete_Choice_Point1

.Lpred33_54:
	movl	ta+508, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_go__a0,@function
.globl X0_go__a0

X0_go__a0:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont90, pl_reg_bank+1040
	jmp	X0_argument_list__a1
.Lcont90:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_pl2wam__a1
	.p2align	4,,15
	.type	directive_1,@function

directive_1:
	jmp	X0_go__a0
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
	movl	%eax, at+248
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+248, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+180
	movl	$.LC87, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC88, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+132
	movl	$.LC89, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC90, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC91, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	$.LC92, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC93, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	$.LC94, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+168
	movl	$.LC95, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC96, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+136
	movl	$.LC97, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+140
	movl	at+324, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+492
	movl	at+316, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+484
	movl	at+288, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+456
	movl	at+332, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+500
	movl	at+320, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+488
	movl	at+308, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+476
	movl	at+292, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+460
	movl	at+296, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+464
	movl	at+312, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+480
	movl	at+284, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+452
	movl	at+304, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+472
	movl	at+300, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+468
	movl	at+280, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+448
	movl	at+276, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+444
	movl	at+268, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+436
	movl	at+328, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+496
	movl	at+336, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+504
	movl	at+272, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+440
	movl	at+260, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+428
	movl	at+256, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+424
	movl	at+264, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+432
	movl	at+340, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+508
	movl	$.LC98, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+344
	movl	at+184, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+328
	movl	at+172, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+316
	movl	at+144, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+288
	movl	at+196, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+340
	movl	at+176, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+320
	movl	at+164, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+308
	movl	at+148, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+292
	movl	at+152, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+296
	movl	at+168, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+312
	movl	at+140, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+284
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+304
	movl	at+156, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+300
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+280
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+276
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+248
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+252
	movl	at+180, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+324
	movl	at+188, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+332
	movl	at+128, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+272
	movl	at+124, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+268
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+260
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+256
	movl	at+192, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+336
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+244
	movl	at+120, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+264
	movl	$.LC99, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+372
	movl	$.LC100, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+396
	movl	at+252, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+420
	movl	$.LC101, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+376
	movl	$.LC102, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+356
	movl	at+244, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+416
	movl	$.LC103, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+384
	movl	$.LC104, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	$.LC105, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+208
	movl	$.LC106, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	$.LC107, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	$.LC108, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC109, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	$.LC110, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+236
	movl	$.LC111, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+204
	movl	$.LC112, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+212
	movl	$.LC113, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+352
	movl	$.LC114, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+232
	movl	$.LC115, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	$.LC116, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC117, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+240
	movl	$.LC118, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+364
	movl	$.LC119, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+228
	movl	$.LC120, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+216
	movl	$.LC121, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+392
	movl	$.LC122, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+368
	movl	at+4, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	$.LC123, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+176
	movl	at+236, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+412
	movl	at+232, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+408
	movl	at+224, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+400
	movl	at+228, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+404
	movl	$.LC124, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC125, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	$.LC126, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC127, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	$.LC128, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC129, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC130, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	$.LC131, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+124
	movl	$.LC132, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC133, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+200
	movl	$.LC134, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+224
	movl	$.LC135, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	$.LC136, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+128
	movl	$.LC137, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC138, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+360
	movl	$.LC139, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC140, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+192
	movl	at+28, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	$.LC141, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC142, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+388
	movl	$.LC143, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+348
	movl	$.LC144, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	$.LC145, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+92
	movl	$.LC146, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC147, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC148, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC149, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	$.LC150, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+60
	movl	$.LC151, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC152, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC153, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+184
	movl	$.LC154, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	$.LC155, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC156, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+380
	movl	$.LC157, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC158, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC159, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC98, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC160, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC161, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC162, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC80, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$39, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_pl2wam__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_pl2wam1__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24706C3277616D312F315F2461757832__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24706C3277616D312F315F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$82, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_and_emit_pred__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+28, %ecx
	movl	$.Lpred5_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+32, %ecx
	movl	$.Lpred5_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+36, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_emit_inits__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$109, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_lst_clause__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$118, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_clause__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$134, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_bc_compile_lst_clause__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$143, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_msg_start__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6D70696C655F6D73675F73746172742F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$157, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_compile_msg_end__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$167, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_cur_pred__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$172, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_cur_pred_without_aux__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$179, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_init_counters__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$204, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_add_counter__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$219, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_last_times__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$226, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_display_counters__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$269, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_cmd_line_args__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$269, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267732F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$300, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_cmd_line_args__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$307, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_cmd_line_arg1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$25, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+100, %ecx
	movl	$.Lpred22_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+104, %ecx
	movl	$.Lpred22_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+108, %ecx
	movl	$.Lpred22_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+112, %ecx
	movl	$.Lpred22_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+116, %ecx
	movl	$.Lpred22_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+120, %ecx
	movl	$.Lpred22_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+124, %ecx
	movl	$.Lpred22_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+128, %ecx
	movl	$.Lpred22_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+132, %ecx
	movl	$.Lpred22_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+136, %ecx
	movl	$.Lpred22_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+140, %ecx
	movl	$.Lpred22_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+144, %ecx
	movl	$.Lpred22_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+148, %ecx
	movl	$.Lpred22_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+152, %ecx
	movl	$.Lpred22_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+156, %ecx
	movl	$.Lpred22_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+160, %ecx
	movl	$.Lpred22_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+164, %ecx
	movl	$.Lpred22_35+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+168, %ecx
	movl	$.Lpred22_37+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+172, %ecx
	movl	$.Lpred22_39+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+176, %ecx
	movl	$.Lpred22_41+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+180, %ecx
	movl	$.Lpred22_43+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+184, %ecx
	movl	$.Lpred22_45+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+188, %ecx
	movl	$.Lpred22_47+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+192, %ecx
	movl	$.Lpred22_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$25, %edx
	movl	at+196, %ecx
	movl	$.Lpred22_51+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+200, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$414, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267312F335F2461757835__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$398, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267312F335F2461757834__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$325, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267312F335F2461757833__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$310, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267312F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$310, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636D645F6C696E655F617267312F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+220, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$428, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_display_copying__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+224, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$444, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_prolog_name__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+228, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$447, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_prolog_version__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+232, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$450, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_prolog_date__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+236, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$453, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_prolog_copyright__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+240, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$461, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_h__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$25, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+244, %ecx
	movl	$.Lpred33_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+248, %ecx
	movl	$.Lpred33_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+252, %ecx
	movl	$.Lpred33_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+256, %ecx
	movl	$.Lpred33_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+260, %ecx
	movl	$.Lpred33_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+264, %ecx
	movl	$.Lpred33_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+268, %ecx
	movl	$.Lpred33_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+272, %ecx
	movl	$.Lpred33_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+276, %ecx
	movl	$.Lpred33_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+280, %ecx
	movl	$.Lpred33_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+284, %ecx
	movl	$.Lpred33_24+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+288, %ecx
	movl	$.Lpred33_26+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+292, %ecx
	movl	$.Lpred33_28+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+296, %ecx
	movl	$.Lpred33_30+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+300, %ecx
	movl	$.Lpred33_32+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+304, %ecx
	movl	$.Lpred33_34+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+308, %ecx
	movl	$.Lpred33_36+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+312, %ecx
	movl	$.Lpred33_38+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+316, %ecx
	movl	$.Lpred33_40+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+320, %ecx
	movl	$.Lpred33_42+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+324, %ecx
	movl	$.Lpred33_44+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+328, %ecx
	movl	$.Lpred33_46+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+332, %ecx
	movl	$.Lpred33_48+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+336, %ecx
	movl	$.Lpred33_50+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$25, %edx
	movl	at+340, %ecx
	movl	$.Lpred33_54+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+344, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$493, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_go__a0+0, 8(%esp)
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
	movl	at+0, %eax
	movl	%eax, 0(%esp)
	movl	$497, 4(%esp)
	movl	$0, 8(%esp)
	movl	$directive_1+0, 12(%esp)
	call	Pl_Execute_Directive
	addl	$132, %esp
	popl	%esi
	ret	
	.section	.ctors,"aw",@progbits
	.align	4
	.long	Object_Initializer
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC87:
	.string	"                Total : %6d   %6d~n"
.LC88:
	.string	"   Statistics (in ms)     user     real~n"
.LC89:
	.string	"   code emission      : %6d   %6d~n"
.LC90:
	.string	"   code generation    : %6d   %6d~n"
.LC91:
	.string	"   first arg computing: %6d   %6d~n"
.LC92:
	.string	"   indexing           : %6d   %6d~n"
.LC93:
	.string	"   internal format    : %6d   %6d~n"
.LC94:
	.string	"   other              : %6d   %6d~n"
.LC95:
	.string	"   register allocation: %6d   %6d~n"
.LC96:
	.string	"   source reading     : %6d   %6d~n"
.LC97:
	.string	"   syntactic sugar    : %6d   %6d~n"
.LC1:
	.string	"  --compile-msg               print a compile message"
.LC2:
	.string	"  --fast-math                 fast mathematical mode (assume integer arithmetics)"
.LC3:
	.string	"  --foreign-only              only compile foreign/1-2 directives"
.LC4:
	.string	"  --help                      print this help and exit"
.LC5:
	.string	"  --keep-void-inst            keep void instructions in the output file"
.LC6:
	.string	"  --min-reg-opt               minimally optimize registers"
.LC7:
	.string	"  --no-call-c                 do not allow the use of fd_tell, '$call_c',..."
.LC8:
	.string	"  --no-inline                 do not inline predicates"
.LC9:
	.string	"  --no-opt-last-subterm       do not optimize last subterm compilation"
.LC10:
	.string	"  --no-redef-error            do not show errors for built-in redefinitions"
.LC11:
	.string	"  --no-reg-opt                do not optimize registers"
.LC12:
	.string	"  --no-reorder                do not reorder predicate arguments"
.LC13:
	.string	"  --no-singl-warn             do not show warnings for named singleton variables"
.LC14:
	.string	"  --no-susp-warn              do not show warnings for suspicious predicates"
.LC15:
	.string	"  --pl-state FILE             read FILE to set the initial Prolog state"
.LC16:
	.string	"  --statistics                print statistics information"
.LC17:
	.string	"  --version                   print version number and exit"
.LC18:
	.string	"  --wam-comment COMMENT       emit COMMENT as a comment in the WAM file"
.LC19:
	.string	"  -W, --wam-for-native        produce a WAM file for native code"
.LC20:
	.string	"  -o FILE, --output FILE      set output file name"
.LC21:
	.string	"  -w, --wam-for-byte-code     produce a WAM file for byte-code (force --no-call-c)"
.LC0:
	.string	""
.LC22:
	.string	"$cmd_line_arg1/3_$aux1"
.LC23:
	.string	"$cmd_line_arg1/3_$aux2"
.LC24:
	.string	"$cmd_line_arg1/3_$aux3"
.LC25:
	.string	"$cmd_line_arg1/3_$aux4"
.LC26:
	.string	"$cmd_line_arg1/3_$aux5"
.LC27:
	.string	"$cmd_line_args/3_$aux1"
.LC28:
	.string	"$compile_msg_start/2_$aux1"
.LC29:
	.string	"$pl2wam1/1_$aux1"
.LC30:
	.string	"$pl2wam1/1_$aux2"
.LC31:
	.string	"'user' can be given as FILE for the standard input/output"
.LC158:
	.string	"*"
.LC159:
	.string	"+"
.LC98:
	.string	"-"
.LC32:
	.string	"--compile-msg"
.LC33:
	.string	"--fast-math"
.LC34:
	.string	"--foreign-only"
.LC35:
	.string	"--help"
.LC36:
	.string	"--keep-void-inst"
.LC37:
	.string	"--min-reg-opt"
.LC38:
	.string	"--no-call-c"
.LC39:
	.string	"--no-inline"
.LC40:
	.string	"--no-opt-last-subterm"
.LC41:
	.string	"--no-redef-error"
.LC42:
	.string	"--no-reg-opt"
.LC43:
	.string	"--no-reorder"
.LC44:
	.string	"--no-singl-warn"
.LC45:
	.string	"--no-susp-warn"
.LC46:
	.string	"--output"
.LC47:
	.string	"--pl-state"
.LC48:
	.string	"--statistics"
.LC49:
	.string	"--version"
.LC50:
	.string	"--wam-comment"
.LC51:
	.string	"--wam-for-byte-code"
.LC52:
	.string	"--wam-for-native"
.LC53:
	.string	"-W"
.LC54:
	.string	"-h"
.LC55:
	.string	"-o"
.LC56:
	.string	"-w"
.LC57:
	.string	"/home/diaz/GP/src/Pl2Wam/pl2wam.pl"
.LC99:
	.string	"FILE missing after --output option~n"
.LC100:
	.string	"For more information about these matters, see the files named COPYING.~n"
.LC58:
	.string	"Options:"
.LC101:
	.string	"Prolog to Wam Compiler (~a) ~a~n"
.LC102:
	.string	"Report bugs to bug-prolog@gnu.org."
.LC59:
	.string	"Usage: pl2wam [OPTION...] FILE"
.LC103:
	.string	"You may redistribute copies of ~a~n"
.LC60:
	.string	"add_counter"
.LC160:
	.string	"bc"
.LC61:
	.string	"bc_compile_lst_clause"
.LC104:
	.string	"byte code"
.LC105:
	.string	"call_c"
.LC161:
	.string	"cl"
.LC62:
	.string	"cmd_line_arg1"
.LC63:
	.string	"cmd_line_args"
.LC64:
	.string	"compile_and_emit_pred"
.LC65:
	.string	"compile_clause"
.LC66:
	.string	"compile_emit_inits"
.LC67:
	.string	"compile_lst_clause"
.LC106:
	.string	"compile_msg"
.LC68:
	.string	"compile_msg_end"
.LC69:
	.string	"compile_msg_start"
.LC107:
	.string	"compiling ~a for ~a...~n"
.LC108:
	.string	"cur_arity"
.LC109:
	.string	"cur_func"
.LC70:
	.string	"cur_pred"
.LC71:
	.string	"cur_pred_without_aux"
.LC72:
	.string	"display_copying"
.LC73:
	.string	"display_counters"
.LC162:
	.string	"exception"
.LC74:
	.string	"f"
.LC110:
	.string	"fast_math"
.LC111:
	.string	"foreign_only"
.LC75:
	.string	"go"
.LC76:
	.string	"h"
.LC77:
	.string	"init_counters"
.LC112:
	.string	"inline"
.LC113:
	.string	"input file already specified (~a)~n"
.LC114:
	.string	"keep_void_inst"
.LC78:
	.string	"last_times"
.LC115:
	.string	"native code"
.LC116:
	.string	"native_code"
.LC117:
	.string	"no input file~n"
.LC118:
	.string	"off"
.LC119:
	.string	"opt_last_subterm"
.LC120:
	.string	"optim_fail"
.LC121:
	.string	"or of the terms of the GNU General Public License (or both in parallel)~n"
.LC122:
	.string	"output file already specified (~a)~n"
.LC79:
	.string	"pl2wam"
.LC80:
	.string	"pl2wam1"
.LC123:
	.string	"plfile"
.LC81:
	.string	"prolog_copyright"
.LC82:
	.string	"prolog_date"
.LC83:
	.string	"prolog_name"
.LC84:
	.string	"prolog_version"
.LC124:
	.string	"real_code_gen"
.LC125:
	.string	"real_first_arg"
.LC126:
	.string	"real_indexing"
.LC127:
	.string	"real_internal"
.LC128:
	.string	"real_read_file"
.LC129:
	.string	"real_reg_alloc"
.LC130:
	.string	"real_syn_sugar"
.LC131:
	.string	"real_time"
.LC132:
	.string	"real_wam_emit"
.LC133:
	.string	"redef_error"
.LC134:
	.string	"reg_opt"
.LC135:
	.string	"reorder"
.LC136:
	.string	"runtime"
.LC137:
	.string	"singl_warn"
.LC138:
	.string	"singleton_warning"
.LC139:
	.string	"statistics"
.LC140:
	.string	"susp_warn"
.LC85:
	.string	"t"
.LC141:
	.string	"true"
.LC142:
	.string	"under the terms of the GNU Lesser General Public License~n"
.LC143:
	.string	"unknown option ~a - try pl2wam --help~n"
.LC86:
	.string	"user"
.LC144:
	.string	"user_code_gen"
.LC145:
	.string	"user_first_arg"
.LC146:
	.string	"user_indexing"
.LC147:
	.string	"user_internal"
.LC148:
	.string	"user_read_file"
.LC149:
	.string	"user_reg_alloc"
.LC150:
	.string	"user_syn_sugar"
.LC151:
	.string	"user_wam_emit"
.LC152:
	.string	"wam_comment"
.LC153:
	.string	"wamfile"
.LC154:
	.string	"where"
.LC155:
	.string	"~N\t~d error(s)~n"
.LC156:
	.string	"~a comes with ABSOLUTELY NO WARRANTY.~n"
.LC157:
	.string	"~a compiled, ~d lines read - ~d bytes written, ~d ms~n"
	.data	
	.align	4
	.local	at
	.comm	at,348,4
	.local	fn
	.comm	fn,28,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,512,4
	.section	.note.GNU-stack,"",@progbits
