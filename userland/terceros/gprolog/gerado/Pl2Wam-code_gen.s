# Pl2Wam/code_gen.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/code_gen.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_code_generation__a5,@function
.globl X0_code_generation__a5

X0_code_generation__a5:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_generate_head__a5
.Lcont1:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_generate_body__a3
	.p2align	4,,15
	.type	X0_generate_head__a5,@function
.globl X0_generate_head__a5

X0_generate_head__a5:
	movl	$9, %eax
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
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_gen_list_integers__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2467656E65726174655F686561642F355F2461757831__a4
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_gen_unif_arg_lst__a4
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2467656E65726174655F686561642F355F2461757832__a4
	.p2align	4,,15
	.type	X1_2467656E65726174655F686561642F355F2461757832__a4,@function

X1_2467656E65726174655F686561642F355F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred3_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
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

.Lpred3_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F686561642F355F2461757831__a4,@function

X1_2467656E65726174655F686561642F355F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred4_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X0_reorder_head_arg_lst__a4

.Lpred4_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_reorder_head_arg_lst__a4,@function
.globl X0_reorder_head_arg_lst__a4

X0_reorder_head_arg_lst__a4:
	movl	$12, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_split_arg_lst__a8
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_reverse__a2
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_reverse__a2
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-60(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_append__a3
	.p2align	4,,15
	.type	X0_generate_body__a3,@function
.globl X0_generate_body__a3

X0_generate_body__a3:
	movl	$.Lpred6_1+0, %eax
	movl	$.Lpred6_2+0, %edx
	movl	$.Lpred6_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred6_1:
	movl	$.Lpred6_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred6_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_3:
	call	Pl_Delete_Choice_Point3

.Lpred6_4:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -36(%edi)
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -44(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_2467656E65726174655F626F64792F335F2461757831__a2
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+28
	call	Pl_Deallocate
	jmp	X0_generate_body1__a8
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F64792F335F2461757831__a2,@function

X1_2467656E65726174655F626F64792F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred7_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_generate_body1__a8,@function
.globl X0_generate_body1__a8

X0_generate_body1__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred8_6+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred8_2+0, %eax
	movl	$.Lpred8_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred8_1:
	movl	st+0, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred8_2:
	movl	$.Lpred8_4+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point

.Lpred8_3:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred8_4:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point

.Lpred8_5:
	movl	$10, %eax
	call	Pl_Allocate
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	call	Pl_Cut
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_2467656E65726174655F626F6479312F385F2461757831__a3
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_2467656E65726174655F626F6479312F385F2461757832__a3
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_load_c_call_args__a5
.Lcont14:
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2467656E65726174655F626F6479312F385F2461757833__a5

.Lpred8_6:
	movl	$.Lpred8_7+0, %eax
	movl	$9, %edx
	call	Pl_Update_Choice_Point
	movl	$9, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
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
	movl	pl_reg_bank+28, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -44(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_inline_predicate__a2
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_gen_inline_pred__a5
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	call	Pl_Cut
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2467656E65726174655F626F6479312F385F2461757834__a4

.Lpred8_7:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point
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
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_gen_list_integers__a3
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_2467656E65726174655F626F6479312F385F2461757835__a4
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-60(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_gen_load_arg_lst__a4
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-64(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_qualif_with_module__a4
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-60(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-64(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2467656E65726174655F626F6479312F385F2461757836__a5
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757836__a5,@function

X1_2467656E65726174655F626F6479312F385F2461757836__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred9_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E65726174655F626F6479312F385F2461757837__a3

.Lpred9_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_generate_body__a3
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757837__a3,@function

X1_2467656E65726174655F626F6479312F385F2461757837__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
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
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757835__a4,@function

X1_2467656E65726174655F626F6479312F385F2461757835__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred11_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X0_reorder_body_arg_lst__a4

.Lpred11_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757834__a4,@function

X1_2467656E65726174655F626F6479312F385F2461757834__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred12_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_2467656E65726174655F626F6479312F385F2461757838__a2

.Lpred12_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_generate_body__a3
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757838__a2,@function

X1_2467656E65726174655F626F6479312F385F2461757838__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757833__a5,@function

X1_2467656E65726174655F626F6479312F385F2461757833__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred14_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E65726174655F626F6479312F385F2461757839__a3

.Lpred14_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_generate_body__a3
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757839__a3,@function

X1_2467656E65726174655F626F6479312F385F2461757839__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483641, %eax
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
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred15_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+20, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757832__a3,@function

X1_2467656E65726174655F626F6479312F385F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred16_1+0, %eax
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
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_memberchk__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+40, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred16_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E65726174655F626F6479312F385F2461757831__a3,@function

X1_2467656E65726174655F626F6479312F385F2461757831__a3:
	movl	$.Lpred17_2+0, %eax
	movl	$.Lpred17_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred17_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred17_2:
	movl	$.Lpred17_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred17_3:
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
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred17_4:
	call	Pl_Delete_Choice_Point3

.Lpred17_5:
	movl	fn+28, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_qualif_with_module__a4,@function
.globl X0_qualif_with_module__a4

X0_qualif_with_module__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred18_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	fn+32, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
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
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred18_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+4, %eax
	movl	pl_reg_bank+12, %edx
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
	.p2align	4,,15
	.type	X0_reorder_body_arg_lst__a4,@function
.globl X0_reorder_body_arg_lst__a4

X0_reorder_body_arg_lst__a4:
	movl	$10, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_split_arg_lst__a8
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_append__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_append__a3
	.p2align	4,,15
	.type	X0_split_arg_lst__a8,@function
.globl X0_split_arg_lst__a8

X0_split_arg_lst__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred20_1+0, %eax
	movl	$.Lpred20_2+0, %edx
	movl	$.Lpred20_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred20_1:
	movl	$.Lpred20_3+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point

.Lpred20_2:
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
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred20_3:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point

.Lpred20_4:
	movl	$9, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+44
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+28
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
	movl	%eax, pl_reg_bank+48
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+36
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+24
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+32
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+40
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+52
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_2473706C69745F6172675F6C73742F385F2461757831__a14
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+24
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+28
	call	Pl_Deallocate
	jmp	X0_split_arg_lst__a8
	.p2align	4,,15
	.type	X1_2473706C69745F6172675F6C73742F385F2461757831__a14,@function

X1_2473706C69745F6172675F6C73742F385F2461757831__a14:
	movl	$.Lpred21_1+0, %eax
	movl	$14, %edx
	call	Pl_Create_Choice_Point
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+56
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+40, %eax
	movl	pl_reg_bank+56, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+56
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+60
	movl	pl_reg_bank+56, %eax
	movl	pl_reg_bank+60, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+28, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+36, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+40, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+44, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+48, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+52, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred21_1:
	movl	$.Lpred21_2+0, %eax
	movl	$14, %edx
	call	Pl_Update_Choice_Point
	movl	$14, %eax
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
	movl	pl_reg_bank+32, %eax
	movl	%eax, -48(%edi)
	movl	pl_reg_bank+36, %eax
	movl	%eax, -52(%edi)
	movl	pl_reg_bank+40, %eax
	movl	%eax, -56(%edi)
	movl	pl_reg_bank+44, %eax
	movl	%eax, -60(%edi)
	movl	pl_reg_bank+48, %eax
	movl	%eax, -64(%edi)
	movl	pl_reg_bank+52, %eax
	movl	%eax, -68(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X0_has_temporaries__a1
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
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
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	-64(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-68(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred21_2:
	movl	$14, %eax
	call	Pl_Delete_Choice_Point
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+48, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+28, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+40, %eax
	movl	pl_reg_bank+36, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+52, %eax
	movl	pl_reg_bank+44, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_has_temporaries__a1,@function
.globl X0_has_temporaries__a1

X0_has_temporaries__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
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
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_246861735F74656D706F7261726965732F315F2461757831__a2
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246861735F74656D706F7261726965732F315F2461757831__a2,@function

X1_246861735F74656D706F7261726965732F315F2461757831__a2:
	movl	$.Lpred23_6+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$.Lpred23_2+0, %eax
	movl	$.Lpred23_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred23_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred23_2:
	movl	$.Lpred23_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred23_3:
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_4:
	call	Pl_Delete_Choice_Point2

.Lpred23_5:
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X0_has_temporaries__a1

.Lpred23_6:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_has_temporaries__a1
	.p2align	4,,15
	.type	X0_gen_unif_arg_lst__a4,@function
.globl X0_gen_unif_arg_lst__a4

X0_gen_unif_arg_lst__a4:
	movl	$.Lpred24_1+0, %eax
	movl	$.Lpred24_2+0, %edx
	movl	$.Lpred24_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred24_1:
	movl	$.Lpred24_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred24_2:
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

.Lpred24_3:
	call	Pl_Delete_Choice_Point4

.Lpred24_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_gen_unif_arg__a4
.Lcont29:
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
	jmp	X0_gen_unif_arg_lst__a4
	.p2align	4,,15
	.type	X0_gen_unif_arg__a4,@function
.globl X0_gen_unif_arg__a4

X0_gen_unif_arg__a4:
	movl	$.Lpred25_2+0, %eax
	movl	$.Lpred25_11+0, %edx
	movl	$.Lpred25_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred25_1:
	movl	st+12, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred25_2:
	movl	$.Lpred25_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred25_3:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E5F756E69665F6172672F345F2461757831__a5

.Lpred25_4:
	movl	$.Lpred25_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred25_5:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+44, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_6:
	movl	$.Lpred25_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred25_7:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_8:
	movl	$.Lpred25_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred25_9:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+60, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_10:
	movl	$.Lpred25_12+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred25_11:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_12:
	call	Pl_Delete_Choice_Point4

.Lpred25_13:
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X1_2467656E5F756E69665F6172672F345F2461757832__a4
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_flat_stc_arg_lst__a5
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_gen_subterm_arg_lst__a3
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_gen_unif_arg_lst__a4
	.p2align	4,,15
	.type	X1_2467656E5F756E69665F6172672F345F2461757832__a4,@function

X1_2467656E5F756E69665F6172672F345F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred26_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+68, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred26_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+72, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
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
	.p2align	4,,15
	.type	X1_2467656E5F756E69665F6172672F345F2461757831__a5,@function

X1_2467656E5F756E69665F6172672F345F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred27_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E5F756E69665F6172672F345F2461757833__a5

.Lpred27_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F756E69665F6172672F345F2461757833__a5,@function

X1_2467656E5F756E69665F6172672F345F2461757833__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred28_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+52, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred28_1:
	movl	$6, %eax
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
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_2467656E5F756E69665F6172672F345F2461757834__a2
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F756E69665F6172672F345F2461757834__a2,@function

X1_2467656E5F756E69665F6172672F345F2461757834__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred29_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+68, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+72, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_gen_load_arg_lst__a4,@function
.globl X0_gen_load_arg_lst__a4

X0_gen_load_arg_lst__a4:
	movl	$.Lpred30_1+0, %eax
	movl	$.Lpred30_2+0, %edx
	movl	$.Lpred30_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred30_1:
	movl	$.Lpred30_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred30_2:
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

.Lpred30_3:
	call	Pl_Delete_Choice_Point4

.Lpred30_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont34:
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
	jmp	X0_gen_load_arg_lst__a4
	.p2align	4,,15
	.type	X0_gen_load_arg__a4,@function
.globl X0_gen_load_arg__a4

X0_gen_load_arg__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred31_3+0, %eax
	movl	$.Lpred31_12+0, %edx
	movl	$.Lpred31_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred31_1:
	movl	st+16, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred31_2:
	movl	$.Lpred31_sub_0+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred31_14

.Lpred31_sub_0:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred31_16

.Lpred31_3:
	movl	$.Lpred31_5+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred31_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757831__a5
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757832__a4

.Lpred31_5:
	movl	$.Lpred31_7+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred31_6:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+88, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_7:
	movl	$.Lpred31_9+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred31_8:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+92, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_9:
	movl	$.Lpred31_11+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred31_10:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+96, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_11:
	movl	$.Lpred31_13+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred31_12:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_13:
	movl	$.Lpred31_15+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred31_14:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred31_15:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred31_16:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757833__a4
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
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
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X0_flat_stc_arg_lst__a5
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -44(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X0_gen_load_arg_lst__a4
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_gen_subterm_arg_lst__a3
	.p2align	4,,15
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757833__a4,@function

X1_2467656E5F6C6F61645F6172672F345F2461757833__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred32_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+108, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+112, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
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
	.p2align	4,,15
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757832__a4,@function

X1_2467656E5F6C6F61645F6172672F345F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred33_1+0, %eax
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred33_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757831__a5,@function

X1_2467656E5F6C6F61645F6172672F345F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred34_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757834__a5

.Lpred34_1:
	movl	$.Lpred34_2+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	fn+116, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
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

.Lpred34_2:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+120, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
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
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757834__a5,@function

X1_2467656E5F6C6F61645F6172672F345F2461757834__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred35_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+52, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	fn+124, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred35_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757835__a4
.Lcont40:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757835__a4,@function

X1_2467656E5F6C6F61645F6172672F345F2461757835__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred36_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	ta+84, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred36_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+68, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E5F6C6F61645F6172672F345F2461757836__a3
	.p2align	4,,15
	.type	X1_2467656E5F6C6F61645F6172672F345F2461757836__a3,@function

X1_2467656E5F6C6F61645F6172672F345F2461757836__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+116, %eax
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

.Lpred37_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_flat_stc_arg_lst__a5,@function
.globl X0_flat_stc_arg_lst__a5

X0_flat_stc_arg_lst__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred38_2+0, %eax
	movl	$.Lpred38_3+0, %edx
	movl	$.Lpred38_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred38_1:
	movl	$.Lpred38_sub_0+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred38_5

.Lpred38_sub_0:
	movl	$.Lpred38_sub_1+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	jmp	.Lpred38_7

.Lpred38_sub_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred38_9

.Lpred38_2:
	movl	$.Lpred38_4+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred38_3:
	movl	pl_reg_bank+0, %eax
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
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred38_4:
	movl	$.Lpred38_6+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred38_5:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -36(%edi)
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_simple_stc_arg__a1
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
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
	call	Pl_Deallocate
	jmp	X0_flat_stc_arg_lst__a5

.Lpred38_6:
	movl	$.Lpred38_8+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred38_7:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -32(%edi)
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X1_24666C61745F7374635F6172675F6C73742F355F2461757831__a2
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X0_flat_stc_arg_lst__a5

.Lpred38_8:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred38_9:
	movl	$5, %eax
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
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X1_24666C61745F7374635F6172675F6C73742F355F2461757832__a3
.Lcont43:
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
	jmp	X0_flat_stc_arg_lst__a5
	.p2align	4,,15
	.type	X1_24666C61745F7374635F6172675F6C73742F355F2461757832__a3,@function

X1_24666C61745F7374635F6172675F6C73742F355F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred39_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	ta+84, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666C61745F7374635F6172675F6C73742F355F2461757831__a2,@function

X1_24666C61745F7374635F6172675F6C73742F355F2461757831__a2:
	movl	$.Lpred40_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred40_1:
	call	Pl_Delete_Choice_Point2
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_simple_stc_arg__a1,@function
.globl X0_simple_stc_arg__a1

X0_simple_stc_arg__a1:
	movl	$.Lpred41_2+0, %eax
	movl	$.Lpred41_9+0, %edx
	movl	$.Lpred41_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred41_1:
	movl	st+20, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred41_2:
	movl	$.Lpred41_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred41_3:
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred41_4:
	movl	$.Lpred41_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred41_5:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred41_6:
	movl	$.Lpred41_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred41_7:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred41_8:
	call	Pl_Delete_Choice_Point1

.Lpred41_9:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_gen_subterm_arg_lst__a3,@function
.globl X0_gen_subterm_arg_lst__a3

X0_gen_subterm_arg_lst__a3:
	movl	$.Lpred42_1+0, %eax
	movl	$.Lpred42_2+0, %edx
	movl	$.Lpred42_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred42_1:
	movl	$.Lpred42_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred42_2:
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

.Lpred42_3:
	call	Pl_Delete_Choice_Point3

.Lpred42_4:
	movl	$6, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, -28(%edi)
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_gen_compte_void__a4
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2467656E5F7375627465726D5F6172675F6C73742F335F2461757831__a6
	.p2align	4,,15
	.type	X1_2467656E5F7375627465726D5F6172675F6C73742F335F2461757831__a6,@function

X1_2467656E5F7375627465726D5F6172675F6C73742F335F2461757831__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred43_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_gen_subterm_arg__a3
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_gen_subterm_arg_lst__a3

.Lpred43_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+132, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X0_gen_subterm_arg_lst__a3
	.p2align	4,,15
	.type	X0_gen_compte_void__a4,@function
.globl X0_gen_compte_void__a4

X0_gen_compte_void__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred44_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+40, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+4
	jmp	X0_gen_compte_void__a4

.Lpred44_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_gen_subterm_arg__a3,@function
.globl X0_gen_subterm_arg__a3

X0_gen_subterm_arg__a3:
	movl	$.Lpred45_2+0, %eax
	movl	$.Lpred45_9+0, %edx
	movl	$.Lpred45_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred45_1:
	movl	st+24, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred45_2:
	movl	$.Lpred45_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred45_3:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
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
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2467656E5F7375627465726D5F6172672F335F2461757831__a3

.Lpred45_4:
	movl	$.Lpred45_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred45_5:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
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
	movl	fn+136, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred45_6:
	movl	$.Lpred45_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred45_7:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
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
	movl	fn+140, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred45_8:
	movl	$.Lpred45_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred45_9:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+92, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred45_10:
	call	Pl_Delete_Choice_Point3

.Lpred45_11:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X1_2467656E5F7375627465726D5F6172672F335F2461757832__a3
.Lcont46:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_gen_subterm_arg_lst__a3
	.p2align	4,,15
	.type	X1_2467656E5F7375627465726D5F6172672F335F2461757832__a3,@function

X1_2467656E5F7375627465726D5F6172672F335F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred46_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+96, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred46_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+144, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
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
	.type	X1_2467656E5F7375627465726D5F6172672F335F2461757831__a3,@function

X1_2467656E5F7375627465726D5F6172672F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred47_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+148, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred47_1:
	movl	$.Lpred47_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+152, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred47_2:
	call	Pl_Delete_Choice_Point4
	movl	fn+156, %eax
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
	.type	X0_gen_list_integers__a3,@function
.globl X0_gen_list_integers__a3

X0_gen_list_integers__a3:
	jmp	X1_2467656E5F6C6973745F696E7465676572732F335F2461757831__a3
	.p2align	4,,15
	.type	X1_2467656E5F6C6973745F696E7465676572732F335F2461757831__a3,@function

X1_2467656E5F6C6973745F696E7465676572732F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred49_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+16, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+20, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Fast_Lt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
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
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	jmp	X0_gen_list_integers__a3

.Lpred49_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_special_form__a2,@function
.globl X0_special_form__a2

X0_special_form__a2:
	movl	fn+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
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
	movl	fn+124, %eax
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
	.type	X0_dummy_instruction__a2,@function
.globl X0_dummy_instruction__a2

X0_dummy_instruction__a2:
	movl	$.Lpred51_2+0, %eax
	movl	$.Lpred51_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred51_1:
	movl	st+28, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred51_2:
	movl	$.Lpred51_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred51_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+40, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred51_4:
	call	Pl_Delete_Choice_Point2

.Lpred51_5:
	movl	ta+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+40, %eax
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
	.type	X0_equal__a4,@function
.globl X0_equal__a4

X0_equal__a4:
	movl	$.Lpred52_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_1:
	movl	$.Lpred52_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_2:
	movl	$.Lpred52_3+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_3:
	movl	$.Lpred52_4+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
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
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_4:
	movl	$.Lpred52_5+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+20
	movl	fn+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_24657175616C2F345F2461757831__a6

.Lpred52_5:
	movl	$.Lpred52_6+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+36, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_24657175616C2F345F2461757832__a6

.Lpred52_6:
	movl	$.Lpred52_7+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+40, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_inline_unif_reg_term__a5

.Lpred52_7:
	movl	$.Lpred52_8+0, %eax
	call	Pl_Update_Choice_Point4
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_equal_lst__a4

.Lpred52_8:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_warn__a2
	.p2align	4,,15
	.type	X1_24657175616C2F345F2461757832__a6,@function

X1_24657175616C2F345F2461757832__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred53_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	jmp	X1_24657175616C2F345F2461757833__a5

.Lpred53_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_gen_unif_arg__a4
	.p2align	4,,15
	.type	X1_24657175616C2F345F2461757833__a5,@function

X1_24657175616C2F345F2461757833__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred54_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred54_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_inline_unif_reg_term__a5
	.p2align	4,,15
	.type	X1_24657175616C2F345F2461757831__a6,@function

X1_24657175616C2F345F2461757831__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred55_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	jmp	X1_24657175616C2F345F2461757834__a5

.Lpred55_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont48:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_gen_unif_arg__a4
	.p2align	4,,15
	.type	X1_24657175616C2F345F2461757834__a5,@function

X1_24657175616C2F345F2461757834__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred56_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred56_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_inline_unif_reg_term__a5
	.p2align	4,,15
	.type	X0_equal_lst__a4,@function
.globl X0_equal_lst__a4

X0_equal_lst__a4:
	movl	$.Lpred57_1+0, %eax
	movl	$.Lpred57_2+0, %edx
	movl	$.Lpred57_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred57_1:
	movl	$.Lpred57_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred57_2:
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

.Lpred57_3:
	call	Pl_Delete_Choice_Point4

.Lpred57_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_equal__a4
.Lcont49:
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
	jmp	X0_equal_lst__a4
	.p2align	4,,15
	.type	X0_inline_unif_reg_term__a5,@function
.globl X0_inline_unif_reg_term__a5

X0_inline_unif_reg_term__a5:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757831__a5
	.p2align	4,,15
	.type	X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757831__a5,@function

X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred59_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont50:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757832__a4

.Lpred59_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_gen_unif_arg__a4
	.p2align	4,,15
	.type	X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757832__a4,@function

X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred60_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred60_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X0_warn__a2
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_load_math_expr__a4,@function
.globl X0_load_math_expr__a4

X0_load_math_expr__a4:
	movl	$.Lpred61_12+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred61_2+0, %eax
	movl	$.Lpred61_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred61_1:
	movl	st+32, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred61_2:
	movl	$.Lpred61_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred61_3:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X1_246C6F61645F6D6174685F657870722F345F2461757831__a1
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_246C6F61645F6D6174685F657870722F345F2461757832__a4

.Lpred61_4:
	movl	$.Lpred61_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred61_5:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_gen_load_arg__a4

.Lpred61_6:
	movl	$.Lpred61_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred61_7:
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_gen_load_arg__a4

.Lpred61_8:
	movl	$.Lpred61_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred61_9:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_load_math_expr1__a6

.Lpred61_10:
	call	Pl_Delete_Choice_Point4

.Lpred61_11:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+20
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	jmp	X0_load_math_expr1__a6

.Lpred61_12:
	call	Pl_Delete_Choice_Point4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+108, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246C6F61645F6D6174685F657870722F345F2461757832__a4,@function

X1_246C6F61645F6D6174685F657870722F345F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred62_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+160, %eax
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

.Lpred62_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+164, %eax
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
	.p2align	4,,15
	.type	X1_246C6F61645F6D6174685F657870722F345F2461757831__a1,@function

X1_246C6F61645F6D6174685F657870722F345F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred63_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+116, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X0_error__a2

.Lpred63_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_load_math_expr1__a6,@function
.globl X0_load_math_expr1__a6

X0_load_math_expr1__a6:
	movl	$.Lpred64_11+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred64_3+0, %eax
	movl	$.Lpred64_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred64_1:
	movl	st+36, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred64_2:
	movl	$.Lpred64_sub_0+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred64_6

.Lpred64_sub_0:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred64_8

.Lpred64_3:
	movl	$.Lpred64_5+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred64_4:
	movl	ta+64, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+56, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_load_math_expr__a4

.Lpred64_5:
	movl	$.Lpred64_7+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred64_6:
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X0_load_math_expr__a4

.Lpred64_7:
	movl	$.Lpred64_9+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred64_8:
	movl	ta+120, %eax
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
	movl	fn+48, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+124, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_load_math_expr1__a6

.Lpred64_9:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred64_10:
	movl	ta+128, %eax
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
	movl	fn+48, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+132, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_load_math_expr1__a6

.Lpred64_11:
	movl	$.Lpred64_12+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
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
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X1_246C6F61645F6D6174685F65787072312F365F2461757831__a3
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X0_load_math_arg_lst__a4
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+136, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred64_12:
	movl	$.Lpred64_13+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$2, %eax
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
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X0_math_exp_functor_name__a3
.Lcont55:
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
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
	jmp	X0_error__a2

.Lpred64_13:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
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
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_error__a2
	.p2align	4,,15
	.type	X1_246C6F61645F6D6174685F65787072312F365F2461757831__a3,@function

X1_246C6F61645F6D6174685F65787072312F365F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred65_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+112, %eax
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_fast_exp_functor_name__a3

.Lpred65_1:
	call	Pl_Delete_Choice_Point4
	jmp	X0_math_exp_functor_name__a3
	.p2align	4,,15
	.type	X0_load_math_arg_lst__a4,@function
.globl X0_load_math_arg_lst__a4

X0_load_math_arg_lst__a4:
	movl	$.Lpred66_1+0, %eax
	movl	$.Lpred66_2+0, %edx
	movl	$.Lpred66_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred66_1:
	movl	$.Lpred66_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred66_2:
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

.Lpred66_3:
	call	Pl_Delete_Choice_Point4

.Lpred66_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+40, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_load_math_expr__a4
.Lcont56:
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
	jmp	X0_load_math_arg_lst__a4
	.p2align	4,,15
	.type	X0_fast_exp_functor_name__a3,@function
.globl X0_fast_exp_functor_name__a3

X0_fast_exp_functor_name__a3:
	movl	$.Lpred67_3+0, %eax
	movl	$.Lpred67_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred67_1:
	movl	st+40, %eax
	movl	$22, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred67_2:
	movl	$.Lpred67_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred67_4

.Lpred67_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred67_12

.Lpred67_3:
	movl	$.Lpred67_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred67_4:
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
	movl	ta+148, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_5:
	movl	$.Lpred67_7+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_6:
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
	movl	ta+152, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_7:
	movl	$.Lpred67_9+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_8:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+156, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_9:
	movl	$.Lpred67_11+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_10:
	movl	ta+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+160, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_11:
	movl	$.Lpred67_13+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_12:
	movl	ta+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+164, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_13:
	movl	$.Lpred67_15+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_14:
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
	movl	ta+172, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_15:
	movl	$.Lpred67_17+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_16:
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
	movl	ta+180, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_17:
	movl	$.Lpred67_19+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_18:
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
	movl	ta+188, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_19:
	movl	$.Lpred67_21+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_20:
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
	movl	ta+196, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_21:
	movl	$.Lpred67_23+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_22:
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
	movl	ta+204, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_23:
	movl	$.Lpred67_25+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_24:
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
	movl	ta+212, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_25:
	movl	$.Lpred67_27+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_26:
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
	movl	ta+220, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_27:
	movl	$.Lpred67_29+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_28:
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
	movl	ta+228, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_29:
	movl	$.Lpred67_31+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_30:
	movl	ta+232, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+236, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_31:
	movl	$.Lpred67_33+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_32:
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
	movl	ta+244, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_33:
	movl	$.Lpred67_35+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_34:
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
	movl	ta+252, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_35:
	movl	$.Lpred67_37+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_36:
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+260, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_37:
	movl	$.Lpred67_39+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_38:
	movl	ta+264, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+268, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_39:
	movl	$.Lpred67_41+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_40:
	movl	ta+272, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+276, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_41:
	movl	$.Lpred67_43+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_42:
	movl	ta+280, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+284, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_43:
	movl	$.Lpred67_45+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_44:
	movl	ta+288, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+292, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_45:
	movl	$.Lpred67_47+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred67_46:
	movl	ta+296, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+300, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_47:
	call	Pl_Delete_Choice_Point3

.Lpred67_48:
	movl	ta+304, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+308, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_math_exp_functor_name__a3,@function
.globl X0_math_exp_functor_name__a3

X0_math_exp_functor_name__a3:
	movl	$.Lpred68_4+0, %eax
	movl	$.Lpred68_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred68_1:
	movl	st+44, %eax
	movl	$53, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred68_2:
	movl	$.Lpred68_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred68_5

.Lpred68_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred68_13

.Lpred68_3:
	movl	$.Lpred68_sub_1+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred68_89

.Lpred68_sub_1:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred68_93

.Lpred68_4:
	movl	$.Lpred68_6+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred68_5:
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
	movl	ta+312, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_6:
	movl	$.Lpred68_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_7:
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
	movl	ta+316, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_8:
	movl	$.Lpred68_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_9:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+320, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_10:
	movl	$.Lpred68_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_11:
	movl	ta+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+324, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_12:
	movl	$.Lpred68_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_13:
	movl	ta+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+328, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_14:
	movl	$.Lpred68_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_15:
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
	movl	ta+332, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_16:
	movl	$.Lpred68_18+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_17:
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
	movl	ta+336, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_18:
	movl	$.Lpred68_20+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_19:
	movl	ta+340, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+344, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_20:
	movl	$.Lpred68_22+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_21:
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
	movl	ta+348, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_22:
	movl	$.Lpred68_24+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_23:
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
	movl	ta+352, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_24:
	movl	$.Lpred68_26+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_25:
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
	movl	ta+356, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_26:
	movl	$.Lpred68_28+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_27:
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
	movl	ta+360, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_28:
	movl	$.Lpred68_30+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_29:
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
	movl	ta+364, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_30:
	movl	$.Lpred68_32+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_31:
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
	movl	ta+368, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_32:
	movl	$.Lpred68_34+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_33:
	movl	ta+232, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+372, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_34:
	movl	$.Lpred68_36+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_35:
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
	movl	ta+376, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_36:
	movl	$.Lpred68_38+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_37:
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
	movl	ta+380, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_38:
	movl	$.Lpred68_40+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_39:
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+384, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_40:
	movl	$.Lpred68_42+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_41:
	movl	ta+264, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+388, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_42:
	movl	$.Lpred68_44+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_43:
	movl	ta+272, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+392, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_44:
	movl	$.Lpred68_46+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_45:
	movl	ta+280, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+396, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_46:
	movl	$.Lpred68_48+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_47:
	movl	ta+288, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+400, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_48:
	movl	$.Lpred68_50+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_49:
	movl	ta+296, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+404, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_50:
	movl	$.Lpred68_52+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_51:
	movl	ta+408, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+412, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_52:
	movl	$.Lpred68_54+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_53:
	movl	ta+416, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+420, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_54:
	movl	$.Lpred68_56+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_55:
	movl	ta+304, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+424, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_56:
	movl	$.Lpred68_58+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_57:
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
	movl	ta+432, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_58:
	movl	$.Lpred68_60+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_59:
	movl	ta+436, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+440, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_60:
	movl	$.Lpred68_62+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_61:
	movl	ta+444, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+448, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_62:
	movl	$.Lpred68_64+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_63:
	movl	ta+452, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+456, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_64:
	movl	$.Lpred68_66+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_65:
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
	movl	ta+464, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_66:
	movl	$.Lpred68_68+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_67:
	movl	ta+468, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+472, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_68:
	movl	$.Lpred68_70+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_69:
	movl	ta+476, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+480, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_70:
	movl	$.Lpred68_72+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_71:
	movl	ta+484, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+488, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_72:
	movl	$.Lpred68_74+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_73:
	movl	ta+492, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+496, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_74:
	movl	$.Lpred68_76+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_75:
	movl	ta+500, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+504, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_76:
	movl	$.Lpred68_78+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_77:
	movl	ta+508, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+512, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_78:
	movl	$.Lpred68_80+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_79:
	movl	ta+516, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+520, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_80:
	movl	$.Lpred68_82+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_81:
	movl	ta+524, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+528, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_82:
	movl	$.Lpred68_84+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_83:
	movl	ta+532, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+536, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_84:
	movl	$.Lpred68_86+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_85:
	movl	ta+540, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+544, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_86:
	movl	$.Lpred68_88+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_87:
	movl	ta+548, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+552, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_88:
	movl	$.Lpred68_90+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_89:
	movl	ta+556, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+560, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_90:
	movl	$.Lpred68_92+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_91:
	movl	ta+564, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+568, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_92:
	movl	$.Lpred68_94+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_93:
	movl	ta+556, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+572, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_94:
	movl	$.Lpred68_96+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_95:
	movl	ta+576, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+580, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_96:
	movl	$.Lpred68_98+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_97:
	movl	ta+584, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+588, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_98:
	movl	$.Lpred68_100+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_99:
	movl	ta+592, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+596, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_100:
	movl	$.Lpred68_102+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_101:
	movl	ta+600, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+604, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_102:
	movl	$.Lpred68_104+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_103:
	movl	ta+608, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+612, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_104:
	movl	$.Lpred68_106+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_105:
	movl	ta+616, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+620, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_106:
	movl	$.Lpred68_108+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_107:
	movl	ta+624, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+628, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_108:
	movl	$.Lpred68_110+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_109:
	movl	ta+632, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+636, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_110:
	movl	$.Lpred68_112+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred68_111:
	movl	ta+640, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+644, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred68_112:
	call	Pl_Delete_Choice_Point3

.Lpred68_113:
	movl	ta+648, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+652, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_fast_cmp_functor_name__a2,@function
.globl X0_fast_cmp_functor_name__a2

X0_fast_cmp_functor_name__a2:
	movl	$.Lpred69_2+0, %eax
	movl	$.Lpred69_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred69_1:
	movl	st+48, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred69_2:
	movl	$.Lpred69_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred69_3:
	movl	ta+656, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+660, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred69_4:
	movl	$.Lpred69_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred69_5:
	movl	ta+664, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+668, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred69_6:
	movl	$.Lpred69_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred69_7:
	movl	ta+672, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+676, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred69_8:
	movl	$.Lpred69_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred69_9:
	movl	ta+680, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+684, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred69_10:
	movl	$.Lpred69_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred69_11:
	movl	ta+688, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+692, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred69_12:
	call	Pl_Delete_Choice_Point2

.Lpred69_13:
	movl	ta+696, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+700, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_math_cmp_functor_name__a2,@function
.globl X0_math_cmp_functor_name__a2

X0_math_cmp_functor_name__a2:
	movl	$.Lpred70_2+0, %eax
	movl	$.Lpred70_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred70_1:
	movl	st+52, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred70_2:
	movl	$.Lpred70_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred70_3:
	movl	ta+656, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+704, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_4:
	movl	$.Lpred70_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_5:
	movl	ta+664, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+708, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_6:
	movl	$.Lpred70_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_7:
	movl	ta+672, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+712, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_8:
	movl	$.Lpred70_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_9:
	movl	ta+680, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+716, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_10:
	movl	$.Lpred70_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_11:
	movl	ta+688, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+720, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_12:
	call	Pl_Delete_Choice_Point2

.Lpred70_13:
	movl	ta+696, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+724, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_load_c_call_args__a5,@function
.globl X0_load_c_call_args__a5

X0_load_c_call_args__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred71_1+0, %eax
	movl	$6, %edx
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
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+728, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_memberchk__a2
.Lcont57:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X0_load_by_value_arg_lst__a4
.Lcont58:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred71_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X0_load_by_reg_arg_lst__a4
.Lcont59:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_load_by_reg_arg_lst__a4,@function
.globl X0_load_by_reg_arg_lst__a4

X0_load_by_reg_arg_lst__a4:
	movl	$.Lpred72_1+0, %eax
	movl	$.Lpred72_2+0, %edx
	movl	$.Lpred72_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred72_1:
	movl	$.Lpred72_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred72_2:
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

.Lpred72_3:
	call	Pl_Delete_Choice_Point4

.Lpred72_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	fn+40, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X0_gen_load_arg__a4
.Lcont60:
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
	jmp	X0_load_by_reg_arg_lst__a4
	.p2align	4,,15
	.type	X0_load_by_value_arg_lst__a4,@function
.globl X0_load_by_value_arg_lst__a4

X0_load_by_value_arg_lst__a4:
	movl	$.Lpred73_1+0, %eax
	movl	$.Lpred73_2+0, %edx
	movl	$.Lpred73_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred73_1:
	movl	$.Lpred73_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred73_2:
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

.Lpred73_3:
	call	Pl_Delete_Choice_Point4

.Lpred73_4:
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X0_load_by_value_arg__a4
.Lcont61:
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
	jmp	X0_load_by_value_arg_lst__a4
	.p2align	4,,15
	.type	X0_load_by_value_arg__a4,@function
.globl X0_load_by_value_arg__a4

X0_load_by_value_arg__a4:
	movl	$.Lpred74_12+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred74_2+0, %eax
	movl	$.Lpred74_9+0, %edx
	movl	$.Lpred74_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred74_1:
	movl	st+56, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred74_2:
	movl	$.Lpred74_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred74_3:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred74_4:
	movl	$.Lpred74_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred74_5:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred74_6:
	movl	$.Lpred74_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred74_7:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred74_8:
	movl	$.Lpred74_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred74_9:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
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

.Lpred74_10:
	call	Pl_Delete_Choice_Point4

.Lpred74_11:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+340, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
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

.Lpred74_12:
	call	Pl_Delete_Choice_Point4
	movl	fn+40, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_gen_load_arg__a4
	.p2align	4,,15
	.type	X0_c_fct_name__a4,@function
.globl X0_c_fct_name__a4

X0_c_fct_name__a4:
	movl	$.Lpred75_4+0, %eax
	movl	$.Lpred75_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred75_1:
	movl	st+60, %eax
	movl	$41, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred75_2:
	movl	$.Lpred75_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred75_71

.Lpred75_sub_0:
	movl	$.Lpred75_sub_1+0, %eax
	call	Pl_Update_Choice_Point4
	jmp	.Lpred75_75

.Lpred75_sub_1:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred75_77

.Lpred75_3:
	movl	$.Lpred75_sub_2+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred75_79

.Lpred75_sub_2:
	movl	$.Lpred75_sub_3+0, %eax
	call	Pl_Update_Choice_Point4
	jmp	.Lpred75_83

.Lpred75_sub_3:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred75_85

.Lpred75_4:
	movl	$.Lpred75_6+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred75_5:
	movl	ta+732, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+736, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_6:
	movl	$.Lpred75_8+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_7:
	movl	ta+744, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+748, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_8:
	movl	$.Lpred75_10+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_9:
	movl	ta+752, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+756, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_10:
	movl	$.Lpred75_12+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_11:
	movl	ta+760, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+764, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_12:
	movl	$.Lpred75_14+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_13:
	movl	ta+576, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+768, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_14:
	movl	$.Lpred75_16+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_15:
	movl	ta+772, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+776, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_16:
	movl	$.Lpred75_18+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_17:
	movl	ta+780, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+784, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_18:
	movl	$.Lpred75_20+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_19:
	movl	ta+788, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+792, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_20:
	movl	$.Lpred75_22+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_21:
	movl	ta+796, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+800, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_22:
	movl	$.Lpred75_24+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_23:
	movl	ta+804, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+808, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_24:
	movl	$.Lpred75_26+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_25:
	movl	ta+812, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+816, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_26:
	movl	$.Lpred75_28+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_27:
	movl	ta+820, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+816, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_28:
	movl	$.Lpred75_30+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_29:
	movl	ta+824, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+828, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_30:
	movl	$.Lpred75_32+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_31:
	movl	ta+832, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+836, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_32:
	movl	$.Lpred75_34+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_33:
	movl	ta+840, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+844, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_34:
	movl	$.Lpred75_36+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_35:
	movl	ta+848, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+852, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_36:
	movl	$.Lpred75_38+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_37:
	movl	ta+856, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+860, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_38:
	movl	$.Lpred75_40+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_39:
	movl	ta+864, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+868, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_40:
	movl	$.Lpred75_42+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_41:
	movl	ta+872, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+876, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_42:
	movl	$.Lpred75_44+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_43:
	movl	ta+880, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+884, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_44:
	movl	$.Lpred75_46+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_45:
	movl	ta+888, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+892, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_46:
	movl	$.Lpred75_48+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_47:
	movl	ta+896, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+900, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_48:
	movl	$.Lpred75_50+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_49:
	movl	ta+904, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+908, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_50:
	movl	$.Lpred75_52+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_51:
	movl	ta+912, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+916, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_52:
	movl	$.Lpred75_54+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_53:
	movl	ta+920, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+924, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_54:
	movl	$.Lpred75_56+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_55:
	movl	ta+928, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+932, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_56:
	movl	$.Lpred75_58+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_57:
	movl	ta+936, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+940, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_58:
	movl	$.Lpred75_60+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_59:
	movl	ta+944, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+948, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_60:
	movl	$.Lpred75_62+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_61:
	movl	ta+952, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+956, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_62:
	movl	$.Lpred75_64+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_63:
	movl	ta+960, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+964, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_64:
	movl	$.Lpred75_66+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_65:
	movl	ta+968, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+972, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_66:
	movl	$.Lpred75_68+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_67:
	movl	ta+976, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+980, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_68:
	movl	$.Lpred75_70+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_69:
	movl	ta+984, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+988, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_70:
	movl	$.Lpred75_72+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_71:
	movl	ta+992, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+996, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_72:
	movl	$.Lpred75_74+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_73:
	movl	ta+1000, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1004, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_74:
	movl	$.Lpred75_76+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_75:
	movl	ta+992, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1008, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_76:
	movl	$.Lpred75_78+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_77:
	movl	ta+992, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1012, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_78:
	movl	$.Lpred75_80+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_79:
	movl	ta+1016, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1020, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_80:
	movl	$.Lpred75_82+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_81:
	movl	ta+1024, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1028, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_82:
	movl	$.Lpred75_84+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_83:
	movl	ta+1016, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1032, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_84:
	movl	$.Lpred75_86+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_85:
	movl	ta+1016, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1036, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_86:
	movl	$.Lpred75_88+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_87:
	movl	ta+1040, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1044, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_88:
	movl	$.Lpred75_90+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_89:
	movl	ta+1048, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1052, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+52, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_90:
	movl	$.Lpred75_92+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred75_91:
	movl	ta+1056, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1060, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred75_92:
	call	Pl_Delete_Choice_Point4

.Lpred75_93:
	movl	ta+1064, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+1068, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+740, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_gen_inline_pred__a5,@function
.globl X0_gen_inline_pred__a5

X0_gen_inline_pred__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred76_17+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred76_3+0, %eax
	movl	$.Lpred76_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred76_1:
	movl	st+64, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred76_2:
	movl	$.Lpred76_sub_0+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred76_4

.Lpred76_sub_0:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred76_6

.Lpred76_3:
	movl	$.Lpred76_5+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred76_4:
	movl	ta+1072, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+52, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred76_5:
	movl	$.Lpred76_7+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred76_6:
	movl	$5, %eax
	call	Pl_Allocate
	movl	ta+1072, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X0_cur_pred__a2
.Lcont62:
	movl	ta+1076, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X0_set_pred_info__a3
.Lcont63:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_gen_unif_arg__a4

.Lpred76_7:
	movl	$.Lpred76_9+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred76_8:
	movl	ta+1080, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+168, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred76_9:
	movl	$.Lpred76_11+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred76_10:
	movl	ta+1084, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+172, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred76_11:
	movl	$.Lpred76_13+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred76_12:
	movl	ta+1088, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+176, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred76_13:
	movl	$.Lpred76_15+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred76_14:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+1092, %eax
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
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X0_equal__a4
.Lcont64:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred76_15:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred76_16:
	movl	$5, %eax
	call	Pl_Allocate
	movl	ta+1096, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X0_load_math_expr__a4
.Lcont65:
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
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_gen_unif_arg__a4

.Lpred76_17:
	movl	$.Lpred76_18+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X1_2467656E5F696E6C696E655F707265642F355F2461757831__a2
.Lcont66:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X0_load_math_arg_lst__a4
.Lcont67:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+136, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+1100, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred76_18:
	movl	$.Lpred76_19+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	ta+1104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+184, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
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
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred76_19:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$8, %eax
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
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X0_c_fct_name__a4
.Lcont68:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X1_2467656E5F696E6C696E655F707265642F355F2461757832__a2
.Lcont69:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X0_load_c_call_args__a5
.Lcont70:
	movl	pl_reg_bank+1044, %edi
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-36(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F696E6C696E655F707265642F355F2461757832__a2,@function

X1_2467656E5F696E6C696E655F707265642F355F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred77_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+740, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+136, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+1100, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred77_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+136, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2467656E5F696E6C696E655F707265642F355F2461757831__a2,@function

X1_2467656E5F696E6C696E655F707265642F355F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred78_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_fast_cmp_functor_name__a2

.Lpred78_1:
	call	Pl_Delete_Choice_Point3
	jmp	X0_math_cmp_functor_name__a2
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
	movl	%eax, at+44
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+752
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+772
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+768
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+748
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+744
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+756
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+420
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+408
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+352
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+544
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+368
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+760
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+660
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+536
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+548
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+664
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+540
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+552
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+556
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+372
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+672
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+676
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+680
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+684
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+364
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+356
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+668
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+400
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+388
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+444
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+468
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+648
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+452
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+476
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+432
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+436
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+460
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+588
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+600
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+580
	movl	$.LC87, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+608
	movl	$.LC88, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+496
	movl	$.LC89, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC90, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+656
	movl	$.LC91, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+604
	movl	$.LC92, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+440
	movl	$.LC93, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+464
	movl	$.LC94, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC95, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+348
	movl	$.LC96, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC97, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+524
	movl	$.LC98, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+528
	movl	$.LC99, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC100, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC101, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+480
	movl	$.LC102, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC103, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+532
	movl	$.LC104, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC105, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+632
	movl	$.LC106, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC107, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+492
	movl	$.LC108, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+512
	movl	$.LC109, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+516
	movl	$.LC110, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+500
	movl	$.LC111, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC112, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+652
	movl	$.LC113, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+704
	movl	$.LC114, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+688
	movl	$.LC115, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+692
	movl	$.LC116, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+716
	movl	$.LC117, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+720
	movl	$.LC118, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+708
	movl	$.LC119, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+712
	movl	$.LC120, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+696
	movl	$.LC121, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+700
	movl	$.LC122, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+728
	movl	$.LC123, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+724
	movl	$.LC124, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+736
	movl	$.LC125, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+732
	movl	$.LC126, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+396
	movl	$.LC127, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC128, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+740
	movl	$.LC129, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC130, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC131, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC132, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC133, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC134, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC135, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC136, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC137, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC138, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC139, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+640
	movl	$.LC140, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC141, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+612
	movl	$.LC142, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC143, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC144, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC145, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC146, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+592
	movl	$.LC147, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+764
	movl	$.LC148, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+616
	movl	$.LC149, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+620
	movl	$.LC150, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+628
	movl	$.LC151, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+568
	movl	$.LC152, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+576
	movl	$.LC153, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+572
	movl	$.LC154, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+564
	movl	$.LC155, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC156, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC157, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC158, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+484
	movl	$.LC159, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+488
	movl	$.LC160, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+376
	movl	$.LC161, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+560
	movl	$.LC162, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+404
	movl	$.LC163, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+416
	movl	$.LC164, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+412
	movl	$.LC165, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC166, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+380
	movl	$.LC167, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+636
	movl	$.LC168, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+644
	movl	$.LC169, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+584
	movl	$.LC170, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+596
	movl	$.LC171, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+624
	movl	$.LC172, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+520
	movl	$.LC173, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+384
	movl	$.LC174, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC175, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC176, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC177, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC178, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC179, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+504
	movl	$.LC180, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+392
	movl	$.LC181, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC182, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+448
	movl	$.LC183, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+472
	movl	$.LC184, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC185, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC186, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+424
	movl	$.LC187, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC188, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+428
	movl	$.LC189, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+456
	movl	$.LC190, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+508
	movl	$.LC191, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC192, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC193, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+360
	movl	at+44, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	at+752, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1084
	movl	$.LC194, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1104
	movl	at+748, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1080
	movl	at+744, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1072
	movl	$.LC195, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	at+756, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1088
	movl	at+332, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+168
	movl	at+420, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+428
	movl	at+304, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+120
	movl	at+308, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+128
	movl	at+300, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	at+408, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+340
	movl	at+336, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+176
	movl	at+352, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+208
	movl	at+544, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+672
	movl	at+368, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+240
	movl	at+760, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1092
	movl	at+660, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+896
	movl	at+536, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+656
	movl	at+548, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+680
	movl	at+664, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+904
	movl	at+540, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+664
	movl	at+552, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+688
	movl	at+556, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+696
	movl	at+372, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+248
	movl	at+672, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+920
	movl	at+676, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+928
	movl	at+680, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+936
	movl	at+684, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+944
	movl	$.LC196, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+876
	movl	$.LC197, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+756
	movl	$.LC198, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+784
	movl	$.LC199, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+800
	movl	$.LC200, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+892
	movl	$.LC201, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+792
	movl	$.LC202, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+704
	movl	$.LC203, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+660
	movl	$.LC204, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+692
	movl	$.LC205, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+700
	movl	$.LC206, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+676
	movl	$.LC207, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+684
	movl	$.LC208, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+668
	movl	$.LC209, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+844
	movl	$.LC210, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+768
	movl	$.LC211, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+884
	movl	$.LC212, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+988
	movl	$.LC213, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+956
	movl	$.LC214, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+964
	movl	$.LC215, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1020
	movl	$.LC216, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1032
	movl	$.LC217, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1036
	movl	$.LC218, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1028
	movl	$.LC219, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+996
	movl	$.LC220, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1008
	movl	$.LC221, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1012
	movl	$.LC222, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1004
	movl	$.LC223, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+972
	movl	$.LC224, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+980
	movl	$.LC225, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1052
	movl	$.LC226, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1044
	movl	$.LC227, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1068
	movl	$.LC228, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1060
	movl	$.LC229, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+860
	movl	$.LC230, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+808
	movl	$.LC231, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+720
	movl	$.LC232, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+724
	movl	$.LC233, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+764
	movl	$.LC234, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+816
	movl	$.LC235, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+836
	movl	$.LC236, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+712
	movl	$.LC237, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+716
	movl	$.LC238, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+708
	movl	$.LC239, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+852
	movl	$.LC240, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+868
	movl	$.LC241, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+748
	movl	$.LC242, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+776
	movl	$.LC243, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+828
	movl	$.LC244, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+908
	movl	$.LC245, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+940
	movl	$.LC246, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+948
	movl	$.LC247, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+924
	movl	$.LC248, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+932
	movl	$.LC249, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+916
	movl	$.LC250, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+900
	movl	$.LC251, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+736
	movl	$.LC252, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+396
	movl	$.LC253, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+480
	movl	$.LC254, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+528
	movl	$.LC255, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+324
	movl	$.LC256, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+360
	movl	$.LC257, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+496
	movl	$.LC258, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+544
	movl	$.LC259, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+456
	movl	$.LC260, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+464
	movl	$.LC261, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+512
	movl	$.LC262, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+588
	movl	$.LC263, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+472
	movl	$.LC264, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+520
	movl	$.LC265, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+320
	movl	$.LC266, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+336
	movl	$.LC267, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+356
	movl	$.LC268, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+644
	movl	$.LC269, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+652
	movl	$.LC270, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+552
	movl	$.LC271, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+284
	movl	$.LC272, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	$.LC273, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+212
	movl	$.LC274, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC275, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+180
	movl	$.LC276, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+204
	movl	$.LC277, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+300
	movl	$.LC278, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC279, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+308
	movl	$.LC280, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+260
	movl	$.LC281, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+268
	movl	$.LC282, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC283, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC284, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC285, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+236
	movl	$.LC286, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	$.LC287, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+276
	movl	$.LC288, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC289, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+244
	movl	$.LC290, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+252
	movl	$.LC291, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+292
	movl	$.LC292, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC293, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+228
	movl	$.LC294, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+580
	movl	$.LC295, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+344
	movl	$.LC296, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+620
	movl	$.LC297, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+628
	movl	$.LC298, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+596
	movl	$.LC299, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+404
	movl	$.LC300, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+316
	movl	$.LC301, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+424
	movl	$.LC302, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+384
	movl	$.LC303, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+560
	movl	$.LC304, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+568
	movl	$.LC305, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+572
	movl	$.LC306, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+388
	movl	$.LC307, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+420
	movl	$.LC308, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+412
	movl	$.LC309, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+352
	movl	$.LC310, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+332
	movl	$.LC311, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+312
	movl	$.LC312, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+372
	movl	$.LC313, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+364
	movl	$.LC314, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+636
	movl	$.LC315, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+392
	movl	$.LC316, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+432
	movl	$.LC317, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+348
	movl	$.LC318, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+604
	movl	$.LC319, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+376
	movl	$.LC320, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+380
	movl	$.LC321, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+400
	movl	$.LC322, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+488
	movl	$.LC323, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+536
	movl	$.LC324, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+440
	movl	$.LC325, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+328
	movl	$.LC326, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+448
	movl	$.LC327, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+504
	movl	$.LC328, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+612
	movl	$.LC329, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+368
	movl	at+364, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+232
	movl	at+356, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+216
	movl	at+668, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+912
	movl	at+400, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+304
	movl	at+388, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+280
	movl	at+444, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+476
	movl	at+468, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+524
	movl	at+648, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+872
	movl	$.LC330, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+140
	movl	at+452, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+492
	movl	at+476, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+540
	movl	at+432, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+452
	movl	at+436, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+460
	movl	at+460, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+508
	movl	at+588, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+752
	movl	at+600, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+780
	movl	$.LC331, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC332, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+740
	movl	$.LC333, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1100
	movl	$.LC334, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+728
	movl	at+608, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+796
	movl	at+496, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+584
	movl	at+656, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+888
	movl	at+604, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+788
	movl	at+440, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+468
	movl	at+464, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+516
	movl	$.LC335, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	at+328, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	at+348, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+200
	movl	at+524, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+640
	movl	at+528, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+648
	movl	at+480, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+548
	movl	$.LC336, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	$.LC337, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	$.LC338, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	at+40, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	$.LC339, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+136
	movl	$.LC340, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	at+632, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+840
	movl	at+492, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+576
	movl	at+512, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+616
	movl	at+516, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+624
	movl	at+500, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+592
	movl	at+652, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+880
	movl	at+704, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+984
	movl	at+688, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+952
	movl	at+692, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+960
	movl	at+716, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1016
	movl	at+720, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1024
	movl	at+708, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+992
	movl	at+712, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1000
	movl	at+696, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+968
	movl	at+700, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+976
	movl	at+728, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1048
	movl	at+724, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1040
	movl	at+736, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1064
	movl	at+732, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1056
	movl	at+396, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+296
	movl	at+640, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+856
	movl	at+612, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+804
	movl	$.LC341, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+60
	movl	$.LC342, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	at+324, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+124
	movl	at+592, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+760
	movl	at+764, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+1096
	movl	at+616, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+812
	movl	$.LC343, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC344, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+620, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+820
	movl	at+628, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+832
	movl	at+484, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+556
	movl	at+488, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+564
	movl	at+376, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+256
	movl	at+416, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+416
	movl	at+412, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+408
	movl	at+344, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+192
	movl	at+380, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+264
	movl	$.LC345, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+1076
	movl	$.LC346, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	at+636, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+848
	movl	at+644, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+864
	movl	at+584, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+744
	movl	$.LC347, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	at+596, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+772
	movl	$.LC348, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	at+624, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+824
	movl	at+520, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+632
	movl	at+384, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+272
	movl	$.LC349, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+340, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+184
	movl	$.LC350, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	at+504, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+600
	movl	$.LC351, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	at+392, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+288
	movl	at+448, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+484
	movl	at+472, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+532
	movl	at+424, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+436
	movl	$.LC352, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC353, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+428, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+444
	movl	at+456, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+500
	movl	$.LC354, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	at+508, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+608
	movl	$.LC355, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC356, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	$.LC357, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+92
	movl	$.LC358, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	$.LC359, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	$.LC360, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	at+8, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+732
	movl	$.LC361, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	at+360, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+224
	movl	$.LC51, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC362, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC363, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC364, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+180
	movl	$.LC83, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC365, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC366, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC367, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+172
	movl	$.LC368, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC111, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC369, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+184
	movl	$.LC370, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC371, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+168
	movl	$.LC372, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC373, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC374, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC375, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC376, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC377, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC140, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC145, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC378, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+160
	movl	$.LC379, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+164
	movl	$.LC380, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC381, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC382, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+96
	movl	$.LC383, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC384, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+108
	movl	$.LC385, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+104
	movl	$.LC386, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+100
	movl	$.LC387, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+112
	movl	$.LC388, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+116
	movl	$.LC174, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+120
	movl	$.LC389, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+128
	movl	$.LC390, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+124
	movl	$.LC391, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+176
	movl	$.LC187, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC392, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+136
	movl	$.LC393, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+140
	movl	$.LC394, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+156
	movl	$.LC395, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+144
	movl	$.LC396, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+152
	movl	$.LC397, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+148
	movl	$.LC398, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+132
	movl	$.LC192, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC399, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC400, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	at+4, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$39, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_code_generation__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_generate_head__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F686561642F355F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F686561642F355F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_reorder_head_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$79, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_generate_body__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F64792F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$90, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_generate_body1__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+40, %ecx
	movl	$.Lpred8_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$2, %edx
	movl	at+44, %ecx
	movl	$.Lpred8_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+48, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757836__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757837__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757835__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$118, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757834__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$118, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757838__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$93, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757833__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$93, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757839__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$93, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$93, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E65726174655F626F6479312F385F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred17_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred17_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+92, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$155, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_qualif_with_module__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$166, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_reorder_body_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$181, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_split_arg_lst__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$14, %edx
	movl	at+0, %ecx
	movl	$183, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473706C69745F6172675F6C73742F385F2461757831__a14+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$214, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_has_temporaries__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$214, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246861735F74656D706F7261726965732F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred23_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred23_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+120, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$229, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_unif_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_unif_arg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$5, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred25_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$5, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred25_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$5, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred25_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$5, %edx
	movl	at+132, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred25_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$5, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred25_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+136, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$264, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F756E69665F6172672F345F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F756E69665F6172672F345F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F756E69665F6172672F345F2461757833__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F756E69665F6172672F345F2461757834__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$279, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_load_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_load_arg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$5, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred31_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$5, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred31_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$5, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred31_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$5, %edx
	movl	at+132, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred31_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+16, %eax
	movl	$5, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred31_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+160, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757833__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757834__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757835__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$290, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6F61645F6172672F345F2461757836__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$351, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_flat_stc_arg_lst__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$363, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666C61745F7374635F6172675F6C73742F355F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$357, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666C61745F7374635F6172675F6C73742F355F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$373, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_simple_stc_arg__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$3, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred41_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+20, %eax
	movl	$3, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred41_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+20, %eax
	movl	$3, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred41_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+200, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$386, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_subterm_arg_lst__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$388, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F7375627465726D5F6172675F6C73742F335F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$401, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_compte_void__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$411, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_subterm_arg__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred45_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred45_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred45_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred45_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+216, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$428, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F7375627465726D5F6172672F335F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+220, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$411, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F7375627465726D5F6172672F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+224, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$440, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_list_integers__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+228, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$440, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F6C6973745F696E7465676572732F335F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+232, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$454, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_special_form__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+236, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$459, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_dummy_instruction__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+28
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+240, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred51_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+244, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred51_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+248, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$505, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_equal__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$532, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24657175616C2F345F2461757832__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+256, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$532, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24657175616C2F345F2461757833__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+260, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$519, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24657175616C2F345F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+264, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$519, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24657175616C2F345F2461757834__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+268, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$557, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_equal_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+272, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$566, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_inline_unif_reg_term__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+276, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$566, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+280, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$566, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24696E6C696E655F756E69665F7265675F7465726D2F355F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+284, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$601, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_math_expr__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+32
	movl	st+32, %eax
	movl	$5, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred61_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+32, %eax
	movl	$5, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred61_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+32, %eax
	movl	$5, %edx
	movl	at+132, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred61_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+32, %eax
	movl	$5, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred61_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+32, %eax
	movl	$5, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred61_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+288, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$601, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F61645F6D6174685F657870722F345F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+292, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$601, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F61645F6D6174685F657870722F345F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+296, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$628, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_math_expr1__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+36
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+300, %ecx
	movl	$.Lpred64_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+304, %ecx
	movl	$.Lpred64_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+308, %ecx
	movl	$.Lpred64_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+312, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$640, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F61645F6D6174685F65787072312F365F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+316, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$659, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_math_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+320, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$668, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_fast_exp_functor_name__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$22, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+40
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+308, %ecx
	movl	$.Lpred67_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+324, %ecx
	movl	$.Lpred67_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+328, %ecx
	movl	$.Lpred67_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+304, %ecx
	movl	$.Lpred67_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+332, %ecx
	movl	$.Lpred67_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+336, %ecx
	movl	$.Lpred67_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+340, %ecx
	movl	$.Lpred67_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+344, %ecx
	movl	$.Lpred67_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+348, %ecx
	movl	$.Lpred67_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+352, %ecx
	movl	$.Lpred67_24+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+356, %ecx
	movl	$.Lpred67_26+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+360, %ecx
	movl	$.Lpred67_28+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+364, %ecx
	movl	$.Lpred67_30+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+368, %ecx
	movl	$.Lpred67_32+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+372, %ecx
	movl	$.Lpred67_34+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+376, %ecx
	movl	$.Lpred67_36+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+380, %ecx
	movl	$.Lpred67_38+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+384, %ecx
	movl	$.Lpred67_40+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+388, %ecx
	movl	$.Lpred67_42+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+392, %ecx
	movl	$.Lpred67_44+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+396, %ecx
	movl	$.Lpred67_46+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$22, %edx
	movl	at+400, %ecx
	movl	$.Lpred67_48+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+404, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$694, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_math_exp_functor_name__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$53, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+44
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+308, %ecx
	movl	$.Lpred68_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+324, %ecx
	movl	$.Lpred68_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+328, %ecx
	movl	$.Lpred68_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+304, %ecx
	movl	$.Lpred68_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+332, %ecx
	movl	$.Lpred68_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+336, %ecx
	movl	$.Lpred68_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+408, %ecx
	movl	$.Lpred68_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+340, %ecx
	movl	$.Lpred68_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+344, %ecx
	movl	$.Lpred68_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+348, %ecx
	movl	$.Lpred68_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+352, %ecx
	movl	$.Lpred68_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+356, %ecx
	movl	$.Lpred68_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+360, %ecx
	movl	$.Lpred68_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+364, %ecx
	movl	$.Lpred68_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+368, %ecx
	movl	$.Lpred68_35+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+372, %ecx
	movl	$.Lpred68_37+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+376, %ecx
	movl	$.Lpred68_39+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+380, %ecx
	movl	$.Lpred68_41+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+384, %ecx
	movl	$.Lpred68_43+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+388, %ecx
	movl	$.Lpred68_45+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+392, %ecx
	movl	$.Lpred68_47+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+396, %ecx
	movl	$.Lpred68_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+412, %ecx
	movl	$.Lpred68_51+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+416, %ecx
	movl	$.Lpred68_53+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+400, %ecx
	movl	$.Lpred68_55+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+420, %ecx
	movl	$.Lpred68_57+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+424, %ecx
	movl	$.Lpred68_59+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+428, %ecx
	movl	$.Lpred68_61+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+432, %ecx
	movl	$.Lpred68_63+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+436, %ecx
	movl	$.Lpred68_65+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+440, %ecx
	movl	$.Lpred68_67+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+444, %ecx
	movl	$.Lpred68_69+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+448, %ecx
	movl	$.Lpred68_71+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+452, %ecx
	movl	$.Lpred68_73+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+456, %ecx
	movl	$.Lpred68_75+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+460, %ecx
	movl	$.Lpred68_77+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+464, %ecx
	movl	$.Lpred68_79+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+468, %ecx
	movl	$.Lpred68_81+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+472, %ecx
	movl	$.Lpred68_83+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+476, %ecx
	movl	$.Lpred68_85+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+480, %ecx
	movl	$.Lpred68_87+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+484, %ecx
	movl	$.Lpred68_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+488, %ecx
	movl	$.Lpred68_91+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+492, %ecx
	movl	$.Lpred68_95+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+496, %ecx
	movl	$.Lpred68_97+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+500, %ecx
	movl	$.Lpred68_99+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+504, %ecx
	movl	$.Lpred68_101+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+508, %ecx
	movl	$.Lpred68_103+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+512, %ecx
	movl	$.Lpred68_105+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+516, %ecx
	movl	$.Lpred68_107+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+520, %ecx
	movl	$.Lpred68_109+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+524, %ecx
	movl	$.Lpred68_111+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$53, %edx
	movl	at+528, %ecx
	movl	$.Lpred68_113+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+532, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$765, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_fast_cmp_functor_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+48
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+536, %ecx
	movl	$.Lpred69_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+540, %ecx
	movl	$.Lpred69_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+544, %ecx
	movl	$.Lpred69_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+548, %ecx
	movl	$.Lpred69_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+552, %ecx
	movl	$.Lpred69_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+48, %eax
	movl	$6, %edx
	movl	at+556, %ecx
	movl	$.Lpred69_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+560, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$772, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_math_cmp_functor_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+52
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+536, %ecx
	movl	$.Lpred70_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+540, %ecx
	movl	$.Lpred70_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+544, %ecx
	movl	$.Lpred70_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+548, %ecx
	movl	$.Lpred70_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+552, %ecx
	movl	$.Lpred70_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+52, %eax
	movl	$6, %edx
	movl	at+556, %ecx
	movl	$.Lpred70_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+564, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$793, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_c_call_args__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+568, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$804, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_by_reg_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+572, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$813, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_by_value_arg_lst__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+576, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$820, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_load_by_value_arg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+56
	movl	st+56, %eax
	movl	$4, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred74_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+56, %eax
	movl	$4, %edx
	movl	at+128, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred74_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+56, %eax
	movl	$4, %edx
	movl	at+132, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred74_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+56, %eax
	movl	$4, %edx
	movl	at+88, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred74_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+580, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$850, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_c_fct_name__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$41, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+60
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+116, %ecx
	movl	$.Lpred75_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+584, %ecx
	movl	$.Lpred75_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+588, %ecx
	movl	$.Lpred75_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+592, %ecx
	movl	$.Lpred75_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+492, %ecx
	movl	$.Lpred75_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+596, %ecx
	movl	$.Lpred75_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+600, %ecx
	movl	$.Lpred75_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+604, %ecx
	movl	$.Lpred75_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+608, %ecx
	movl	$.Lpred75_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+612, %ecx
	movl	$.Lpred75_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+616, %ecx
	movl	$.Lpred75_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+620, %ecx
	movl	$.Lpred75_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+624, %ecx
	movl	$.Lpred75_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+628, %ecx
	movl	$.Lpred75_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+632, %ecx
	movl	$.Lpred75_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+636, %ecx
	movl	$.Lpred75_35+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+640, %ecx
	movl	$.Lpred75_37+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+644, %ecx
	movl	$.Lpred75_39+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+648, %ecx
	movl	$.Lpred75_41+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+652, %ecx
	movl	$.Lpred75_43+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+656, %ecx
	movl	$.Lpred75_45+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+660, %ecx
	movl	$.Lpred75_47+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+664, %ecx
	movl	$.Lpred75_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+668, %ecx
	movl	$.Lpred75_51+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+672, %ecx
	movl	$.Lpred75_53+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+676, %ecx
	movl	$.Lpred75_55+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+680, %ecx
	movl	$.Lpred75_57+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+684, %ecx
	movl	$.Lpred75_59+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+688, %ecx
	movl	$.Lpred75_61+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+692, %ecx
	movl	$.Lpred75_63+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+696, %ecx
	movl	$.Lpred75_65+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+700, %ecx
	movl	$.Lpred75_67+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+704, %ecx
	movl	$.Lpred75_69+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+708, %ecx
	movl	$.Lpred75_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+712, %ecx
	movl	$.Lpred75_73+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+716, %ecx
	movl	$.Lpred75_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+720, %ecx
	movl	$.Lpred75_81+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+724, %ecx
	movl	$.Lpred75_87+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+728, %ecx
	movl	$.Lpred75_89+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+732, %ecx
	movl	$.Lpred75_91+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+60, %eax
	movl	$41, %edx
	movl	at+736, %ecx
	movl	$.Lpred75_93+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+740, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$476, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_gen_inline_pred__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+64
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+744, %ecx
	movl	$.Lpred76_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+748, %ecx
	movl	$.Lpred76_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+752, %ecx
	movl	$.Lpred76_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+756, %ecx
	movl	$.Lpred76_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+760, %ecx
	movl	$.Lpred76_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+64, %eax
	movl	$6, %edx
	movl	at+764, %ecx
	movl	$.Lpred76_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+768, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$838, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F696E6C696E655F707265642F355F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+772, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$754, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2467656E5F696E6C696E655F707265642F355F2461757831__a2+0, 8(%esp)
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
	.string	"$call_c"
.LC1:
	.string	"$cut"
.LC2:
	.string	"$equal/4_$aux1"
.LC3:
	.string	"$equal/4_$aux2"
.LC4:
	.string	"$equal/4_$aux3"
.LC5:
	.string	"$equal/4_$aux4"
.LC6:
	.string	"$flat_stc_arg_lst/5_$aux1"
.LC7:
	.string	"$flat_stc_arg_lst/5_$aux2"
.LC194:
	.string	"$foreign_call_c"
.LC8:
	.string	"$gen_inline_pred/5_$aux1"
.LC9:
	.string	"$gen_inline_pred/5_$aux2"
.LC10:
	.string	"$gen_list_integers/3_$aux1"
.LC11:
	.string	"$gen_load_arg/4_$aux1"
.LC12:
	.string	"$gen_load_arg/4_$aux2"
.LC13:
	.string	"$gen_load_arg/4_$aux3"
.LC14:
	.string	"$gen_load_arg/4_$aux4"
.LC15:
	.string	"$gen_load_arg/4_$aux5"
.LC16:
	.string	"$gen_load_arg/4_$aux6"
.LC17:
	.string	"$gen_subterm_arg/3_$aux1"
.LC18:
	.string	"$gen_subterm_arg/3_$aux2"
.LC19:
	.string	"$gen_subterm_arg_lst/3_$aux1"
.LC20:
	.string	"$gen_unif_arg/4_$aux1"
.LC21:
	.string	"$gen_unif_arg/4_$aux2"
.LC22:
	.string	"$gen_unif_arg/4_$aux3"
.LC23:
	.string	"$gen_unif_arg/4_$aux4"
.LC24:
	.string	"$generate_body/3_$aux1"
.LC25:
	.string	"$generate_body1/8_$aux1"
.LC26:
	.string	"$generate_body1/8_$aux2"
.LC27:
	.string	"$generate_body1/8_$aux3"
.LC28:
	.string	"$generate_body1/8_$aux4"
.LC29:
	.string	"$generate_body1/8_$aux5"
.LC30:
	.string	"$generate_body1/8_$aux6"
.LC31:
	.string	"$generate_body1/8_$aux7"
.LC32:
	.string	"$generate_body1/8_$aux8"
.LC33:
	.string	"$generate_body1/8_$aux9"
.LC34:
	.string	"$generate_head/5_$aux1"
.LC35:
	.string	"$generate_head/5_$aux2"
.LC36:
	.string	"$get_current_choice"
.LC37:
	.string	"$get_cut_level"
.LC38:
	.string	"$has_temporaries/1_$aux1"
.LC39:
	.string	"$inline_unif_reg_term/5_$aux1"
.LC40:
	.string	"$inline_unif_reg_term/5_$aux2"
.LC41:
	.string	"$load_math_expr/4_$aux1"
.LC42:
	.string	"$load_math_expr/4_$aux2"
.LC43:
	.string	"$load_math_expr1/6_$aux1"
.LC195:
	.string	"$mt"
.LC44:
	.string	"$soft_cut"
.LC45:
	.string	"$split_arg_lst/8_$aux1"
.LC46:
	.string	"*"
.LC47:
	.string	"**"
.LC48:
	.string	"+"
.LC49:
	.string	"-"
.LC50:
	.string	"."
.LC51:
	.string	"/"
.LC52:
	.string	"//"
.LC53:
	.string	"/\\"
.LC54:
	.string	"/home/diaz/GP/src/Pl2Wam/code_gen.pl"
.LC362:
	.string	":"
.LC55:
	.string	"<"
.LC56:
	.string	"<<"
.LC57:
	.string	"="
.LC58:
	.string	"=.."
.LC59:
	.string	"=:="
.LC60:
	.string	"=<"
.LC61:
	.string	"=="
.LC62:
	.string	"=\\="
.LC63:
	.string	">"
.LC64:
	.string	">="
.LC65:
	.string	">>"
.LC66:
	.string	"@<"
.LC67:
	.string	"@=<"
.LC68:
	.string	"@>"
.LC69:
	.string	"@>="
.LC196:
	.string	"Pl_Blt_Arg"
.LC197:
	.string	"Pl_Blt_Atom"
.LC198:
	.string	"Pl_Blt_Atomic"
.LC199:
	.string	"Pl_Blt_Callable"
.LC200:
	.string	"Pl_Blt_Compare"
.LC201:
	.string	"Pl_Blt_Compound"
.LC202:
	.string	"Pl_Blt_Eq"
.LC203:
	.string	"Pl_Blt_Fast_Eq"
.LC204:
	.string	"Pl_Blt_Fast_Gt"
.LC205:
	.string	"Pl_Blt_Fast_Gte"
.LC206:
	.string	"Pl_Blt_Fast_Lt"
.LC207:
	.string	"Pl_Blt_Fast_Lte"
.LC208:
	.string	"Pl_Blt_Fast_Neq"
.LC209:
	.string	"Pl_Blt_Fd_Var"
.LC210:
	.string	"Pl_Blt_Float"
.LC211:
	.string	"Pl_Blt_Functor"
.LC212:
	.string	"Pl_Blt_G_Array_Size"
.LC213:
	.string	"Pl_Blt_G_Assign"
.LC214:
	.string	"Pl_Blt_G_Assignb"
.LC215:
	.string	"Pl_Blt_G_Dec"
.LC216:
	.string	"Pl_Blt_G_Dec_2"
.LC217:
	.string	"Pl_Blt_G_Dec_3"
.LC218:
	.string	"Pl_Blt_G_Deco"
.LC219:
	.string	"Pl_Blt_G_Inc"
.LC220:
	.string	"Pl_Blt_G_Inc_2"
.LC221:
	.string	"Pl_Blt_G_Inc_3"
.LC222:
	.string	"Pl_Blt_G_Inco"
.LC223:
	.string	"Pl_Blt_G_Link"
.LC224:
	.string	"Pl_Blt_G_Read"
.LC225:
	.string	"Pl_Blt_G_Reset_Bit"
.LC226:
	.string	"Pl_Blt_G_Set_Bit"
.LC227:
	.string	"Pl_Blt_G_Test_Reset_Bit"
.LC228:
	.string	"Pl_Blt_G_Test_Set_Bit"
.LC229:
	.string	"Pl_Blt_Generic_Var"
.LC230:
	.string	"Pl_Blt_Ground"
.LC231:
	.string	"Pl_Blt_Gt"
.LC232:
	.string	"Pl_Blt_Gte"
.LC233:
	.string	"Pl_Blt_Integer"
.LC234:
	.string	"Pl_Blt_List"
.LC235:
	.string	"Pl_Blt_List_Or_Partial_List"
.LC236:
	.string	"Pl_Blt_Lt"
.LC237:
	.string	"Pl_Blt_Lte"
.LC238:
	.string	"Pl_Blt_Neq"
.LC239:
	.string	"Pl_Blt_Non_Fd_Var"
.LC240:
	.string	"Pl_Blt_Non_Generic_Var"
.LC241:
	.string	"Pl_Blt_Non_Var"
.LC242:
	.string	"Pl_Blt_Number"
.LC243:
	.string	"Pl_Blt_Partial_List"
.LC244:
	.string	"Pl_Blt_Term_Eq"
.LC245:
	.string	"Pl_Blt_Term_Gt"
.LC246:
	.string	"Pl_Blt_Term_Gte"
.LC247:
	.string	"Pl_Blt_Term_Lt"
.LC248:
	.string	"Pl_Blt_Term_Lte"
.LC249:
	.string	"Pl_Blt_Term_Neq"
.LC250:
	.string	"Pl_Blt_Univ"
.LC251:
	.string	"Pl_Blt_Var"
.LC252:
	.string	"Pl_Fct_Abs"
.LC253:
	.string	"Pl_Fct_Acos"
.LC254:
	.string	"Pl_Fct_Acosh"
.LC255:
	.string	"Pl_Fct_Add"
.LC256:
	.string	"Pl_Fct_And"
.LC257:
	.string	"Pl_Fct_Asin"
.LC258:
	.string	"Pl_Fct_Asinh"
.LC259:
	.string	"Pl_Fct_Atan"
.LC260:
	.string	"Pl_Fct_Atan2"
.LC261:
	.string	"Pl_Fct_Atanh"
.LC262:
	.string	"Pl_Fct_Ceiling"
.LC263:
	.string	"Pl_Fct_Cos"
.LC264:
	.string	"Pl_Fct_Cosh"
.LC265:
	.string	"Pl_Fct_Dec"
.LC266:
	.string	"Pl_Fct_Div"
.LC267:
	.string	"Pl_Fct_Div2"
.LC268:
	.string	"Pl_Fct_E"
.LC269:
	.string	"Pl_Fct_Epsilon"
.LC270:
	.string	"Pl_Fct_Exp"
.LC271:
	.string	"Pl_Fct_Fast_Abs"
.LC272:
	.string	"Pl_Fct_Fast_Add"
.LC273:
	.string	"Pl_Fct_Fast_And"
.LC274:
	.string	"Pl_Fct_Fast_Dec"
.LC275:
	.string	"Pl_Fct_Fast_Div"
.LC276:
	.string	"Pl_Fct_Fast_Div2"
.LC277:
	.string	"Pl_Fct_Fast_GCD"
.LC278:
	.string	"Pl_Fct_Fast_Inc"
.LC279:
	.string	"Pl_Fct_Fast_Integer_Pow"
.LC280:
	.string	"Pl_Fct_Fast_LSB"
.LC281:
	.string	"Pl_Fct_Fast_MSB"
.LC282:
	.string	"Pl_Fct_Fast_Mod"
.LC283:
	.string	"Pl_Fct_Fast_Mul"
.LC284:
	.string	"Pl_Fct_Fast_Neg"
.LC285:
	.string	"Pl_Fct_Fast_Not"
.LC286:
	.string	"Pl_Fct_Fast_Or"
.LC287:
	.string	"Pl_Fct_Fast_Popcount"
.LC288:
	.string	"Pl_Fct_Fast_Rem"
.LC289:
	.string	"Pl_Fct_Fast_Shl"
.LC290:
	.string	"Pl_Fct_Fast_Shr"
.LC291:
	.string	"Pl_Fct_Fast_Sign"
.LC292:
	.string	"Pl_Fct_Fast_Sub"
.LC293:
	.string	"Pl_Fct_Fast_Xor"
.LC294:
	.string	"Pl_Fct_Float"
.LC295:
	.string	"Pl_Fct_Float_Div"
.LC296:
	.string	"Pl_Fct_Float_Fract_Part"
.LC297:
	.string	"Pl_Fct_Float_Integ_Part"
.LC298:
	.string	"Pl_Fct_Floor"
.LC299:
	.string	"Pl_Fct_GCD"
.LC300:
	.string	"Pl_Fct_Inc"
.LC301:
	.string	"Pl_Fct_Integer_Pow"
.LC302:
	.string	"Pl_Fct_LSB"
.LC303:
	.string	"Pl_Fct_Log"
.LC304:
	.string	"Pl_Fct_Log10"
.LC305:
	.string	"Pl_Fct_Log_Radix"
.LC306:
	.string	"Pl_Fct_MSB"
.LC307:
	.string	"Pl_Fct_Max"
.LC308:
	.string	"Pl_Fct_Min"
.LC309:
	.string	"Pl_Fct_Mod"
.LC310:
	.string	"Pl_Fct_Mul"
.LC311:
	.string	"Pl_Fct_Neg"
.LC312:
	.string	"Pl_Fct_Not"
.LC313:
	.string	"Pl_Fct_Or"
.LC314:
	.string	"Pl_Fct_PI"
.LC315:
	.string	"Pl_Fct_Popcount"
.LC316:
	.string	"Pl_Fct_Pow"
.LC317:
	.string	"Pl_Fct_Rem"
.LC318:
	.string	"Pl_Fct_Round"
.LC319:
	.string	"Pl_Fct_Shl"
.LC320:
	.string	"Pl_Fct_Shr"
.LC321:
	.string	"Pl_Fct_Sign"
.LC322:
	.string	"Pl_Fct_Sin"
.LC323:
	.string	"Pl_Fct_Sinh"
.LC324:
	.string	"Pl_Fct_Sqrt"
.LC325:
	.string	"Pl_Fct_Sub"
.LC326:
	.string	"Pl_Fct_Tan"
.LC327:
	.string	"Pl_Fct_Tanh"
.LC328:
	.string	"Pl_Fct_Truncate"
.LC329:
	.string	"Pl_Fct_Xor"
.LC70:
	.string	"\\"
.LC71:
	.string	"\\/"
.LC72:
	.string	"\\=="
.LC73:
	.string	"^"
.LC74:
	.string	"abs"
.LC75:
	.string	"acos"
.LC76:
	.string	"acosh"
.LC363:
	.string	"allocate"
.LC77:
	.string	"arg"
.LC364:
	.string	"args"
.LC330:
	.string	"arithmetic operation not allowed in fast math (~q)"
.LC78:
	.string	"asin"
.LC79:
	.string	"asinh"
.LC80:
	.string	"atan"
.LC81:
	.string	"atan2"
.LC82:
	.string	"atanh"
.LC83:
	.string	"atm"
.LC84:
	.string	"atom"
.LC85:
	.string	"atomic"
.LC331:
	.string	"body"
.LC332:
	.string	"bool"
.LC333:
	.string	"boolean"
.LC334:
	.string	"by_value"
.LC86:
	.string	"c_fct_name"
.LC365:
	.string	"call"
.LC366:
	.string	"call_c"
.LC87:
	.string	"callable"
.LC88:
	.string	"ceiling"
.LC89:
	.string	"code_generation"
.LC90:
	.string	"compare"
.LC91:
	.string	"compound"
.LC92:
	.string	"cos"
.LC93:
	.string	"cosh"
.LC367:
	.string	"cut"
.LC335:
	.string	"deallocate"
.LC94:
	.string	"dec"
.LC95:
	.string	"div"
.LC96:
	.string	"dummy_instruction"
.LC97:
	.string	"e"
.LC98:
	.string	"epsilon"
.LC99:
	.string	"equal"
.LC100:
	.string	"equal_lst"
.LC368:
	.string	"execute"
.LC101:
	.string	"exp"
.LC337:
	.string	"explicit unification will fail due to cyclic term (occurs check)"
.LC336:
	.string	"explicit unification will fail"
.LC338:
	.string	"f"
.LC102:
	.string	"fail"
.LC339:
	.string	"fast_call"
.LC103:
	.string	"fast_cmp_functor_name"
.LC104:
	.string	"fast_exp_functor_name"
.LC340:
	.string	"fast_math"
.LC105:
	.string	"fd_var"
.LC106:
	.string	"flat_stc_arg_lst"
.LC107:
	.string	"float"
.LC108:
	.string	"float_fractional_part"
.LC109:
	.string	"float_integer_part"
.LC110:
	.string	"floor"
.LC111:
	.string	"flt"
.LC369:
	.string	"foreign_call_c"
.LC112:
	.string	"functor"
.LC113:
	.string	"g_array_size"
.LC114:
	.string	"g_assign"
.LC115:
	.string	"g_assignb"
.LC116:
	.string	"g_dec"
.LC117:
	.string	"g_deco"
.LC118:
	.string	"g_inc"
.LC119:
	.string	"g_inco"
.LC120:
	.string	"g_link"
.LC121:
	.string	"g_read"
.LC122:
	.string	"g_reset_bit"
.LC123:
	.string	"g_set_bit"
.LC124:
	.string	"g_test_reset_bit"
.LC125:
	.string	"g_test_set_bit"
.LC126:
	.string	"gcd"
.LC127:
	.string	"gen_compte_void"
.LC128:
	.string	"gen_inline_pred"
.LC129:
	.string	"gen_list_integers"
.LC130:
	.string	"gen_load_arg"
.LC131:
	.string	"gen_load_arg_lst"
.LC132:
	.string	"gen_subterm_arg"
.LC133:
	.string	"gen_subterm_arg_lst"
.LC134:
	.string	"gen_unif_arg"
.LC135:
	.string	"gen_unif_arg_lst"
.LC136:
	.string	"generate_body"
.LC137:
	.string	"generate_body1"
.LC138:
	.string	"generate_head"
.LC139:
	.string	"generic_var"
.LC370:
	.string	"get_atom"
.LC371:
	.string	"get_current_choice"
.LC372:
	.string	"get_float"
.LC373:
	.string	"get_integer"
.LC374:
	.string	"get_list"
.LC375:
	.string	"get_nil"
.LC376:
	.string	"get_structure"
.LC377:
	.string	"get_value"
.LC140:
	.string	"get_variable"
.LC141:
	.string	"ground"
.LC142:
	.string	"has_temporaries"
.LC341:
	.string	"head"
.LC342:
	.string	"in_heap"
.LC143:
	.string	"inc"
.LC144:
	.string	"inline_unif_reg_term"
.LC145:
	.string	"int"
.LC146:
	.string	"integer"
.LC147:
	.string	"is"
.LC148:
	.string	"is_list"
.LC343:
	.string	"jump"
.LC344:
	.string	"last_pred"
.LC149:
	.string	"list"
.LC150:
	.string	"list_or_partial_list"
.LC151:
	.string	"load_by_reg_arg_lst"
.LC152:
	.string	"load_by_value_arg"
.LC153:
	.string	"load_by_value_arg_lst"
.LC154:
	.string	"load_c_call_args"
.LC155:
	.string	"load_math_arg_lst"
.LC156:
	.string	"load_math_expr"
.LC157:
	.string	"load_math_expr1"
.LC158:
	.string	"log"
.LC159:
	.string	"log10"
.LC160:
	.string	"lsb"
.LC161:
	.string	"math_cmp_functor_name"
.LC162:
	.string	"math_exp_functor_name"
.LC378:
	.string	"math_fast_load_value"
.LC379:
	.string	"math_load_value"
.LC163:
	.string	"max"
.LC164:
	.string	"min"
.LC165:
	.string	"mod"
.LC166:
	.string	"msb"
.LC345:
	.string	"need_cut_level"
.LC346:
	.string	"nil"
.LC167:
	.string	"non_fd_var"
.LC168:
	.string	"non_generic_var"
.LC169:
	.string	"nonvar"
.LC347:
	.string	"not_in_cur_env"
.LC170:
	.string	"number"
.LC348:
	.string	"opt_last_subterm"
.LC380:
	.string	"p"
.LC171:
	.string	"partial_list"
.LC172:
	.string	"pi"
.LC173:
	.string	"popcount"
.LC349:
	.string	"proceed"
.LC381:
	.string	"put_atom"
.LC382:
	.string	"put_float"
.LC383:
	.string	"put_integer"
.LC384:
	.string	"put_list"
.LC385:
	.string	"put_meta_term"
.LC386:
	.string	"put_nil"
.LC387:
	.string	"put_structure"
.LC388:
	.string	"put_unsafe_value"
.LC174:
	.string	"put_value"
.LC389:
	.string	"put_variable"
.LC390:
	.string	"put_void"
.LC175:
	.string	"qualif_with_module"
.LC176:
	.string	"rem"
.LC350:
	.string	"reorder"
.LC177:
	.string	"reorder_body_arg_lst"
.LC178:
	.string	"reorder_head_arg_lst"
.LC179:
	.string	"round"
.LC351:
	.string	"set_cp"
.LC180:
	.string	"sign"
.LC181:
	.string	"simple_stc_arg"
.LC182:
	.string	"sin"
.LC183:
	.string	"sinh"
.LC391:
	.string	"soft_cut"
.LC184:
	.string	"special_form"
.LC185:
	.string	"split_arg_lst"
.LC186:
	.string	"sqrt"
.LC187:
	.string	"stc"
.LC352:
	.string	"system"
.LC353:
	.string	"t"
.LC188:
	.string	"tan"
.LC189:
	.string	"tanh"
.LC354:
	.string	"treat_body"
.LC190:
	.string	"truncate"
.LC355:
	.string	"unbound variable in arithmetic expression"
.LC392:
	.string	"unify_atom"
.LC393:
	.string	"unify_integer"
.LC356:
	.string	"unify_list"
.LC394:
	.string	"unify_local_value"
.LC357:
	.string	"unify_nil"
.LC395:
	.string	"unify_structure"
.LC396:
	.string	"unify_value"
.LC397:
	.string	"unify_variable"
.LC398:
	.string	"unify_void"
.LC358:
	.string	"unknown expression in arithmetic expression (~q)"
.LC359:
	.string	"unknown operation in arithmetic expression (~q)"
.LC360:
	.string	"unsafe"
.LC191:
	.string	"user"
.LC192:
	.string	"var"
.LC361:
	.string	"void"
.LC399:
	.string	"x"
.LC193:
	.string	"xor"
.LC400:
	.string	"y"
	.data	
	.align	4
	.local	at
	.comm	at,776,4
	.local	fn
	.comm	fn,188,4
	.local	st
	.comm	st,68,4
	.local	ta
	.comm	ta,1108,4
	.section	.note.GNU-stack,"",@progbits
