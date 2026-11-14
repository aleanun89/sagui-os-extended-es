# Pl2Wam/wam_emit.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/wam_emit.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_emit_code_init__a2,@function
.globl X0_emit_code_init__a2

X0_emit_code_init__a2:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_prolog_file_name__a2
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_emit_code_files__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24656D69745F636F64655F696E69742F325F2461757831__a2
.Lcont3:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_prolog_name__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_prolog_version__a1
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont7:
	movl	ta+20, %eax
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
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_24656D69745F636F64655F696E69742F325F2461757832__a2
	.p2align	4,,15
	.type	X1_24656D69745F636F64655F696E69742F325F2461757832__a2,@function

X1_24656D69745F636F64655F696E69742F325F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred2_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred2_1:
	call	Pl_Delete_Choice_Point3
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_24656D69745F636F64655F696E69742F325F2461757831__a2,@function

X1_24656D69745F636F64655F696E69742F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_current_output__a1

.Lpred3_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_open__a3
	.p2align	4,,15
	.type	X0_emit_code_files__a3,@function
.globl X0_emit_code_files__a3

X0_emit_code_files__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred4_7+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred4_3+0, %eax
	movl	$.Lpred4_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred4_1:
	movl	st+0, %eax
	movl	$1, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred4_2:
	movl	$.Lpred4_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred4_4

.Lpred4_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred4_6

.Lpred4_3:
	movl	$.Lpred4_5+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred4_4:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred4_5:
	call	Pl_Delete_Choice_Point4

.Lpred4_6:
	movl	$4, %eax
	call	Pl_Allocate
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_decompose_file_name__a4
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_24656D69745F636F64655F66696C65732F335F2461757831__a1
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_24656D69745F636F64655F66696C65732F335F2461757832__a4

.Lpred4_7:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F636F64655F66696C65732F335F2461757832__a4,@function

X1_24656D69745F636F64655F66696C65732F335F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred5_1+0, %eax
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
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_2470726F6C6F675F66696C655F737566666978__a1
.Lcont10:
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
	jmp	X0_atom_concat__a3

.Lpred5_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$3, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_atom_concat__a3
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_atom_concat__a3
	.p2align	4,,15
	.type	X1_24656D69745F636F64655F66696C65732F335F2461757831__a1,@function

X1_24656D69745F636F64655F66696C65732F335F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point2
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_emit_code_term__a2,@function
.globl X0_emit_code_term__a2

X0_emit_code_term__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
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
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_character_count__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X0_line_count__a2
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_close__a1
	.p2align	4,,15
	.type	X0_emit_code__a5,@function
.globl X0_emit_code__a5

X0_emit_code__a5:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X0_emit_pred_start__a6
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X0_emit_wam_code__a3
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X0_emit_pred_start__a6,@function
.globl X0_emit_pred_start__a6

X0_emit_pred_start__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred9_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$5, %eax
	call	Pl_Allocate
	movl	$-2147483645, %eax
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -32(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_24656D69745F707265645F73746172742F365F2461757831__a2
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_emit_file_name_if_needed__a2
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
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
	jmp	X0_format__a3

.Lpred9_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$8, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X0_emit_file_name_if_needed__a2
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_24656D69745F707265645F73746172742F365F2461757832__a3
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_24656D69745F707265645F73746172742F365F2461757833__a3
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_24656D69745F707265645F73746172742F365F2461757834__a3
.Lcont22:
	movl	ta+60, %eax
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
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_export_type__a5
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
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
	movl	-44(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_24656D69745F707265645F73746172742F365F2461757834__a3,@function

X1_24656D69745F707265645F73746172742F365F2461757834__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred10_1+0, %eax
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
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+76, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F707265645F73746172742F365F2461757833__a3,@function

X1_24656D69745F707265645F73746172742F365F2461757833__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred11_1+0, %eax
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
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred11_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+88, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F707265645F73746172742F365F2461757832__a3,@function

X1_24656D69745F707265645F73746172742F365F2461757832__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred12_1+0, %eax
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
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+100, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F707265645F73746172742F365F2461757831__a2,@function

X1_24656D69745F707265645F73746172742F365F2461757831__a2:
	movl	$.Lpred13_2+0, %eax
	movl	$.Lpred13_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred13_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred13_2:
	movl	$.Lpred13_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred13_3:
	movl	ta+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_4:
	call	Pl_Delete_Choice_Point2

.Lpred13_5:
	movl	ta+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+112, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_export_type__a5,@function
.globl X0_export_type__a5

X0_export_type__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred14_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+116, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_246175785F6E616D65__a1
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	movl	$.Lpred14_2+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+116, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_2:
	movl	$.Lpred14_3+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+112, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+124, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_3:
	movl	$.Lpred14_4+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+112, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+128, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+132, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_test_pred_info__a3
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_4:
	movl	$.Lpred14_5+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+112, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+112, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_is_exported__a2
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_5:
	movl	$.Lpred14_6+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	ta+136, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+140, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+144, %eax
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

.Lpred14_6:
	movl	$.Lpred14_7+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+136, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_is_exported__a2
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_7:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	ta+116, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_emit_file_name_if_needed__a2,@function
.globl X0_emit_file_name_if_needed__a2

X0_emit_file_name_if_needed__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred15_1:
	call	Pl_Delete_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+148, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont33:
	movl	ta+4, %eax
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
	.type	X0_emit_wam_code__a3,@function
.globl X0_emit_wam_code__a3

X0_emit_wam_code__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred16_5+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred16_1+0, %eax
	movl	$.Lpred16_2+0, %edx
	movl	$.Lpred16_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred16_1:
	movl	$.Lpred16_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred16_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred16_3:
	call	Pl_Delete_Choice_Point4

.Lpred16_4:
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
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_emit_wam_code__a3
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_emit_wam_code__a3
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred16_5:
	movl	$.Lpred16_6+0, %eax
	call	Pl_Update_Choice_Point4
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
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_special_form__a2
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_emit_wam_code__a3

.Lpred16_6:
	movl	$.Lpred16_7+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+152, %eax
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
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X0_dummy_instruction__a2
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred16_7:
	movl	$.Lpred16_8+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_24656D69745F77616D5F636F64652F335F2461757831__a2
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_emit_one_inst__a2

.Lpred16_8:
	call	Pl_Delete_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_24656D69745F77616D5F636F64652F335F2461757832__a2
.Lcont40:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_emit_one_inst__a2
	.p2align	4,,15
	.type	X1_24656D69745F77616D5F636F64652F335F2461757832__a2,@function

X1_24656D69745F77616D5F636F64652F335F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred17_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+164, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred17_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+168, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F77616D5F636F64652F335F2461757831__a2,@function

X1_24656D69745F77616D5F636F64652F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+164, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+168, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_emit_one_inst__a2,@function
.globl X0_emit_one_inst__a2

X0_emit_one_inst__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred19_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_writeq__a2

.Lpred19_1:
	call	Pl_Delete_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_writeq__a2
.Lcont42:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_emit_args__a4
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+172, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_write__a2
	.p2align	4,,15
	.type	X0_emit_args__a4,@function
.globl X0_emit_args__a4

X0_emit_args__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred20_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred20_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
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
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -28(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X1_24656D69745F617267732F345F2461757831__a2
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_emit_one_arg__a2
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X0_emit_args__a4
	.p2align	4,,15
	.type	X1_24656D69745F617267732F345F2461757831__a2,@function

X1_24656D69745F617267732F345F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$-2147483641, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_put_char__a2

.Lpred21_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+168, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_put_char__a2
	.p2align	4,,15
	.type	X0_emit_one_arg__a2,@function
.globl X0_emit_one_arg__a2

X0_emit_one_arg__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point3
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_length__a2
.Lcont46:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483525, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+164, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_put_char__a2
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_line_position__a2
.Lcont48:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147479649, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_write_term__a3
.Lcont49:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_emit_list__a3

.Lpred22_1:
	movl	$.Lpred22_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X0_emit_one_f_n__a2
.Lcont50:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred22_2:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_writeq__a2
	.p2align	4,,15
	.type	X0_emit_one_f_n__a2,@function
.globl X0_emit_one_f_n__a2

X0_emit_one_f_n__a2:
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
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	fn+0, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
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
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3

.Lpred23_4:
	call	Pl_Delete_Choice_Point2

.Lpred23_5:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
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
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X0_emit_list__a3,@function
.globl X0_emit_list__a3

X0_emit_list__a3:
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_put_char__a2

.Lpred24_3:
	call	Pl_Delete_Choice_Point3

.Lpred24_4:
	movl	$4, %eax
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
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	$-2147483517, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147479649, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X0_write_term__a3
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_emit_list__a3
	.p2align	4,,15
	.type	X0_emit_ensure_linked__a0,@function
.globl X0_emit_ensure_linked__a0

X0_emit_ensure_linked__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred25_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
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
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+200, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X1_24656D69745F656E737572655F6C696E6B65642F305F2461757831__a1
.Lcont55:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont56:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1

.Lpred25_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656D69745F656E737572655F6C696E6B65642F305F2461757831__a1,@function

X1_24656D69745F656E737572655F6C696E6B65642F305F2461757831__a1:
	movl	$.Lpred26_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont57:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+204, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont58:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred26_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_bc_emit_code__a5,@function
.globl X0_bc_emit_code__a5

X0_bc_emit_code__a5:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X0_emit_pred_start__a6
.Lcont59:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2462635F656D69745F636F64652F355F2461757831__a3
	.p2align	4,,15
	.type	X1_2462635F656D69745F636F64652F355F2461757831__a3,@function

X1_2462635F656D69745F636F64652F355F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred28_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X0_bc_emit_prolog_term__a2
.Lcont60:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_format__a3

.Lpred28_1:
	call	Pl_Delete_Choice_Point4
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X0_length__a2
.Lcont61:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X1_2462635F656D69745F636F64652F355F2461757832__a4
.Lcont62:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+212, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont63:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_bc_emit_lst_clause__a2
	.p2align	4,,15
	.type	X1_2462635F656D69745F636F64652F355F2461757832__a4,@function

X1_2462635F656D69745F636F64652F355F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred29_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+216, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+220, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_bc_emit_lst_clause__a2,@function
.globl X0_bc_emit_lst_clause__a2

X0_bc_emit_lst_clause__a2:
	movl	$.Lpred30_1+0, %eax
	movl	$.Lpred30_2+0, %edx
	movl	$.Lpred30_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred30_1:
	movl	$.Lpred30_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred30_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred30_3:
	call	Pl_Delete_Choice_Point2

.Lpred30_4:
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
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -28(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont64:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X0_bc_emit_prolog_term__a2
.Lcont65:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+168, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont66:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X0_emit_wam_code__a3
.Lcont67:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont68:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_bc_emit_lst_clause__a2
	.p2align	4,,15
	.type	X0_bc_emit_prolog_term__a2,@function
.globl X0_bc_emit_prolog_term__a2

X0_bc_emit_prolog_term__a2:
	movl	$.Lpred31_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont69:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X0_name_singleton_vars__a1
.Lcont70:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
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
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont71, pl_reg_bank+1040
	jmp	X0_bind_variables__a2
.Lcont71:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+28
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+24
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+44, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+20
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+28, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont72, pl_reg_bank+1040
	jmp	X0_write_term__a3
.Lcont72:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred31_1:
	call	Pl_Delete_Choice_Point2
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
	movl	%eax, at+24
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	$.LC38, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+216
	movl	at+68, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+108
	movl	at+64, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+104
	movl	$.LC39, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC40, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC41, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC42, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+176
	movl	$.LC43, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC44, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+184
	movl	$.LC45, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC46, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+208
	movl	$.LC47, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+168
	movl	$.LC48, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC49, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+204
	movl	$.LC50, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC51, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	$.LC52, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC53, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+192
	movl	$.LC54, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC55, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+228
	movl	$.LC56, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+132
	movl	$.LC57, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+124
	movl	$.LC58, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC59, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+128
	movl	$.LC60, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC61, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+92
	movl	$.LC62, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	$.LC63, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	$.LC64, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+136
	movl	$.LC65, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC66, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC67, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+60
	movl	$.LC68, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+140
	movl	$.LC69, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	$.LC70, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC71, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	$.LC72, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC73, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC74, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	$.LC75, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC76, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	$.LC77, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	$.LC78, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC79, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	$.LC80, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC81, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+180
	movl	at+8, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC82, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC83, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC84, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	$.LC85, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC86, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+212
	movl	$.LC87, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+224
	movl	$.LC88, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	$.LC89, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+200
	movl	$.LC90, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC91, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	$.LC92, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC17, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC93, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC94, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC95, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC96, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC97, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC98, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC99, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC100, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC101, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC102, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	at+4, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_code_init__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F636F64655F696E69742F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F636F64655F696E69742F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$125, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_code_files__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$1, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$1, %edx
	movl	at+24, %ecx
	movl	$.Lpred4_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+28, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$128, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F636F64655F66696C65732F335F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$128, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F636F64655F66696C65732F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$154, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_code_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$163, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_code__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$173, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_pred_start__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F707265645F73746172742F365F2461757834__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F707265645F73746172742F365F2461757833__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F707265645F73746172742F365F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$173, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F707265645F73746172742F365F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+64, %ecx
	movl	$.Lpred13_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+68, %ecx
	movl	$.Lpred13_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+72, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$204, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_export_type__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$230, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_file_name_if_needed__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$240, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_wam_code__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$264, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F77616D5F636F64652F335F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$254, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F77616D5F636F64652F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$276, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_one_inst__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_args__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$289, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F617267732F345F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$299, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_one_arg__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$323, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_one_f_n__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+112, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred23_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+116, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred23_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+120, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$332, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_list__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$343, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_emit_ensure_linked__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$343, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24656D69745F656E737572655F6C696E6B65642F305F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$360, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_bc_emit_code__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$360, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2462635F656D69745F636F64652F355F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$360, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2462635F656D69745F636F64652F355F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$381, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_bc_emit_lst_clause__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$394, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_bc_emit_prolog_term__a2+0, 8(%esp)
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
	.string	""
.LC38:
	.string	"$$empty$$predicate$$clause$$"
.LC92:
	.string	"$above"
.LC1:
	.string	"$bc_emit_code/5_$aux1"
.LC2:
	.string	"$bc_emit_code/5_$aux2"
.LC3:
	.string	"$emit_args/4_$aux1"
.LC4:
	.string	"$emit_code_files/3_$aux1"
.LC5:
	.string	"$emit_code_files/3_$aux2"
.LC6:
	.string	"$emit_code_init/2_$aux1"
.LC7:
	.string	"$emit_code_init/2_$aux2"
.LC8:
	.string	"$emit_ensure_linked/0_$aux1"
.LC9:
	.string	"$emit_pred_start/6_$aux1"
.LC10:
	.string	"$emit_pred_start/6_$aux2"
.LC11:
	.string	"$emit_pred_start/6_$aux3"
.LC12:
	.string	"$emit_pred_start/6_$aux4"
.LC13:
	.string	"$emit_wam_code/3_$aux1"
.LC14:
	.string	"$emit_wam_code/3_$aux2"
.LC15:
	.string	"$exe_system"
.LC16:
	.string	"$exe_user"
.LC39:
	.string	"%%           ~a~n"
.LC40:
	.string	"%% compiler: ~a ~a~n"
.LC41:
	.string	"%% file    : ~a~n"
.LC42:
	.string	"("
.LC43:
	.string	"(~q)/~q"
.LC44:
	.string	"(~q):(~q)/~q"
.LC45:
	.string	")"
.LC46:
	.string	").~n"
.LC47:
	.string	","
.LC48:
	.string	",~n~*c"
.LC49:
	.string	",~q"
.LC50:
	.string	".wam"
.LC51:
	.string	".wbc"
.LC17:
	.string	"/"
.LC18:
	.string	"/home/diaz/GP/src/Pl2Wam/wam_emit.pl"
.LC19:
	.string	":"
.LC93:
	.string	":-"
.LC52:
	.string	"["
.LC53:
	.string	"]"
.LC54:
	.string	"])."
.LC55:
	.string	"]).~n"
.LC94:
	.string	"bc"
.LC20:
	.string	"bc_emit_code"
.LC21:
	.string	"bc_emit_lst_clause"
.LC22:
	.string	"bc_emit_prolog_term"
.LC56:
	.string	"bfd"
.LC57:
	.string	"bpl"
.LC58:
	.string	"built_in"
.LC59:
	.string	"built_in_fd"
.LC60:
	.string	"cur_pl_file"
.LC61:
	.string	"dyn"
.LC62:
	.string	"dynamic"
.LC23:
	.string	"emit_args"
.LC24:
	.string	"emit_code"
.LC25:
	.string	"emit_code_files"
.LC26:
	.string	"emit_code_init"
.LC27:
	.string	"emit_code_term"
.LC28:
	.string	"emit_ensure_linked"
.LC29:
	.string	"emit_file_name_if_needed"
.LC30:
	.string	"emit_list"
.LC31:
	.string	"emit_one_arg"
.LC32:
	.string	"emit_one_f_n"
.LC33:
	.string	"emit_one_inst"
.LC34:
	.string	"emit_pred_start"
.LC35:
	.string	"emit_wam_code"
.LC95:
	.string	"ensure_linked"
.LC96:
	.string	"exclude"
.LC36:
	.string	"export_type"
.LC63:
	.string	"f"
.LC64:
	.string	"global"
.LC97:
	.string	"ignore_ops"
.LC65:
	.string	"keep_void_inst"
.LC98:
	.string	"label"
.LC66:
	.string	"local"
.LC67:
	.string	"module"
.LC68:
	.string	"module_already_seen"
.LC69:
	.string	"monofile"
.LC70:
	.string	"multi"
.LC71:
	.string	"multifile"
.LC99:
	.string	"namevars"
.LC72:
	.string	"native_code"
.LC100:
	.string	"numbervars"
.LC101:
	.string	"priority"
.LC73:
	.string	"private"
.LC74:
	.string	"proceed"
.LC75:
	.string	"pub"
.LC76:
	.string	"public"
.LC102:
	.string	"quoted"
.LC77:
	.string	"static"
.LC78:
	.string	"streamwamfile"
.LC79:
	.string	"system"
.LC80:
	.string	"t"
.LC81:
	.string	"true"
.LC37:
	.string	"user"
.LC82:
	.string	"wam_comment"
.LC83:
	.string	"write"
.LC84:
	.string	"~a~n    "
.LC85:
	.string	"~a~n~n"
.LC86:
	.string	"~d).~n"
.LC87:
	.string	"~n~nclause("
.LC88:
	.string	"~n~ndirective(~d,~a,"
.LC89:
	.string	"~n~nensure_linked([~q"
.LC90:
	.string	"~n~nfile_name(~q).~n"
.LC91:
	.string	"~n~npredicate(~q,~d,~a,~a,~a,~a,"
	.data	
	.align	4
	.local	at
	.comm	at,152,4
	.local	fn
	.comm	fn,52,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,232,4
	.section	.note.GNU-stack,"",@progbits
