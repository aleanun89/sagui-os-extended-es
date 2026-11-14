# BipsPl/top_level.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/top_level.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_top_level__a0,@function
.globl X0_top_level__a0

X0_top_level__a0:
	movl	$6, %eax
	call	Pl_Allocate
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont1:
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont2:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont3:
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont4:
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont5:
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont6:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont7:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont8:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont9:
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont10:
	call	Pl_Deallocate
	jmp	X0_break__a0
	.p2align	4,,15
	.type	X0_break__a0,@function
.globl X0_break__a0

X0_break__a0:
	movl	$3, %eax
	call	Pl_Allocate
	call	Pl_Set_Ctrl_C_Handler_0
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont11:
	movl	$-2147483601, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont12:
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
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
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_24627265616B2F305F2461757831__a1
.Lcont13:
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_247379735F7661725F696E63__a1
.Lcont14:
	movl	ta+44, %eax
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
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1
.Lcont15:
	movl	ta+48, %eax
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
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1
.Lcont16:
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_24746F705F6C6576656C31__a0
.Lcont17:
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_247379735F7661725F646563__a1
.Lcont18:
	movl	$-2147483601, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont19:
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24627265616B2F305F2461757832__a1
	.p2align	4,,15
	.type	X1_24627265616B2F305F2461757832__a1,@function

X1_24627265616B2F305F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3

.Lpred3_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24627265616B2F305F2461757831__a1,@function

X1_24627265616B2F305F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3

.Lpred4_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24746F705F6C6576656C31__a0,@function
.globl X1_24746F705F6C6576656C31__a0

X1_24746F705F6C6576656C31__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont20:
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_2463617463685F696E7465726E616C__a4
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24746F705F6C6576656C5F61626F7274__a0,@function
.globl X1_24746F705F6C6576656C5F61626F7274__a0

X1_24746F705F6C6576656C5F61626F7274__a0:
	movl	$1, %eax
	call	Pl_Allocate
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_247265696E69745F61667465725F657863657074696F6E__a0
.Lcont22:
	movl	$-2147483601, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont23:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2463617463685F73796E635F666F725F6661696C5F6174__a1
	.p2align	4,,15
	.type	X1_24746F705F6C6576656C5F73746F70__a0,@function
.globl X1_24746F705F6C6576656C5F73746F70__a0

X1_24746F705F6C6576656C5F73746F70__a0:
	movl	$1, %eax
	call	Pl_Allocate
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_247265696E69745F61667465725F657863657074696F6E__a0
.Lcont25:
	movl	$-2147483601, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2463617463685F73796E635F666F725F6661696C5F6174__a1
	.p2align	4,,15
	.type	X1_24746F705F6C6576656C5F657863657074696F6E__a1,@function
.globl X1_24746F705F6C6576656C5F657863657074696F6E__a1

X1_24746F705F6C6576656C5F657863657074696F6E__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_247265696E69745F61667465725F657863657074696F6E__a0
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont28:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_write_term__a3
.Lcont29:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont30:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred8_1:
	call	Pl_Delete_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_247265696E69745F61667465725F657863657074696F6E__a0
.Lcont31:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont32:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_write_term__a3
.Lcont33:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont34:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247265696E69745F61667465725F657863657074696F6E__a0,@function
.globl X1_247265696E69745F61667465725F657863657074696F6E__a0

X1_247265696E69745F61667465725F657863657074696F6E__a0:
	jmp	X1_24247265696E69745F61667465725F657863657074696F6E2F305F2461757831__a0
	.p2align	4,,15
	.type	X1_24247265696E69745F61667465725F657863657074696F6E2F305F2461757831__a0,@function

X1_24247265696E69745F61667465725F657863657074696F6E2F305F2461757831__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483597, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+84, %eax
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
	call	Pl_Deallocate
	jmp	X1_247365745F6C696E656469745F70726F6D7074__a1

.Lpred10_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24746F705F6C6576656C32__a0,@function
.globl X1_24746F705F6C6576656C32__a0

X1_24746F705F6C6576656C32__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont37:
	movl	$-2147483601, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont38:
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_2477726974655F696E64696361746F72__a0
.Lcont39:
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_2424746F705F6C6576656C322F305F2461757831__a2
.Lcont40:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_flush_output__a1
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X1_24726561645F7175657279__a2
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X1_2424746F705F6C6576656C322F305F2461757832__a1
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_sort__a2
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2424746F705F6C6576656C322F305F2461757833__a3
	.p2align	4,,15
	.type	X1_2424746F705F6C6576656C322F305F2461757833__a3,@function

X1_2424746F705F6C6576656C322F305F2461757833__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point4
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_user_time__a1
.Lcont46:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X1_2424746F705F6C6576656C322F305F2461757834__a3
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_user_time__a1
.Lcont48:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	-24(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Sub
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -36(%edi)
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+96, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont49:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X1_2424746F705F6C6576656C322F305F2461757835__a1
.Lcont50:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont51:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_2424746F705F6C6576656C322F305F2461757835__a1,@function

X1_2424746F705F6C6576656C322F305F2461757835__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	call	Pl_Delete_Choice_Point2
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_2424746F705F6C6576656C322F305F2461757834__a3,@function

X1_2424746F705F6C6576656C322F305F2461757834__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X1_24657865635F7175657279__a2
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+108, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424746F705F6C6576656C322F305F2461757832__a1,@function

X1_2424746F705F6C6576656C322F305F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483597, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_247365745F6C696E656469745F70726F6D7074__a1

.Lpred15_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424746F705F6C6576656C322F305F2461757831__a2,@function

X1_2424746F705F6C6576656C322F305F2461757831__a2:
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
	movl	$-2147483597, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X1_246765745F6C696E656469745F70726F6D7074__a1
.Lcont55:
	movl	ta+84, %eax
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
	call	Pl_Deallocate
	jmp	X1_247365745F6C696E656469745F70726F6D7074__a1

.Lpred16_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_write__a2
	.p2align	4,,15
	.type	X1_2477726974655F696E64696361746F72__a0,@function
.globl X1_2477726974655F696E64696361746F72__a0

X1_2477726974655F696E64696361746F72__a0:
	movl	$.Lpred17_1+0, %eax
	movl	$0, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+112, %eax
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
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X1_246462675F696E64696361746F72__a2
.Lcont56:
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont57:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X1_242477726974655F696E64696361746F722F305F2461757831__a2
.Lcont58:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred17_1:
	movl	$0, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242477726974655F696E64696361746F722F305F2461757831__a2,@function

X1_242477726974655F696E64696361746F722F305F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Dec
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_242477726974655F696E64696361746F722F305F2461757832__a2

.Lpred18_1:
	movl	$.Lpred18_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	ta+116, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred18_2:
	call	Pl_Delete_Choice_Point3
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_242477726974655F696E64696361746F722F305F2461757832__a2,@function

X1_242477726974655F696E64696361746F722F305F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred19_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+124, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3

.Lpred19_1:
	call	Pl_Delete_Choice_Point3
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_246462675F696E64696361746F72__a2,@function
.globl X1_246462675F696E64696361746F72__a2

X1_246462675F696E64696361746F72__a2:
	movl	$.Lpred20_2+0, %eax
	movl	$.Lpred20_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred20_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred20_2:
	movl	$.Lpred20_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred20_3:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+132, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred20_4:
	movl	$.Lpred20_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred20_5:
	movl	ta+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+136, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred20_6:
	call	Pl_Delete_Choice_Point2

.Lpred20_7:
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+116, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F7175657279__a2,@function
.globl X1_24726561645F7175657279__a2

X1_24726561645F7175657279__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred21_1+0, %eax
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
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont59:
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+88, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X1_2424726561645F71756572792F325F2461757831__a1
.Lcont60:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+148, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont61:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+152, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
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
	call	Pl_Deallocate
	jmp	X0_read_term_from_atom__a3

.Lpred21_1:
	call	Pl_Delete_Choice_Point3
	movl	$0, %eax
	call	Pl_Allocate
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
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
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X0_read_term__a3
.Lcont62:
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2450425F656D7074795F627566666572__a1
	.p2align	4,,15
	.type	X1_2424726561645F71756572792F325F2461757831__a1,@function

X1_2424726561645F71756572792F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483597, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont63:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_write__a2

.Lpred22_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24657865635F7175657279__a2,@function
.globl X1_24657865635F7175657279__a2

X1_24657865635F7175657279__a2:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+164, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont64:
	call	Pl_Save_Regs_For_Signal
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+168, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont65:
	call	Pl_Save_Regs_For_Signal
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont66:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+172, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont67:
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X1_2463617463685F696E7465726E616C__a4
.Lcont68:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2424657865635F71756572792F325F2461757831__a3
	.p2align	4,,15
	.type	X1_2424657865635F71756572792F325F2461757831__a3,@function

X1_2424657865635F71756572792F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred24_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred24_1:
	call	Pl_Delete_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X1_2477726974655F736F6C7574696F6E__a3
.Lcont69:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2424657865635F71756572792F325F2461757832__a2
	.p2align	4,,15
	.type	X1_2424657865635F71756572792F325F2461757832__a2,@function

X1_2424657865635F71756572792F325F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred25_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+164, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+176, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont70:
	call	Pl_Deallocate
	jmp	X1_24726561645F72657475726E__a0

.Lpred25_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F71756572795F766172735F6E616D6573__a2,@function
.globl X1_247365745F71756572795F766172735F6E616D6573__a2

X1_247365745F71756572795F766172735F6E616D6573__a2:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont71, pl_reg_bank+1040
	jmp	X0_name_query_vars__a2
.Lcont71:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont72, pl_reg_bank+1040
	jmp	X1_2472656D6F76655F756E64657273636F72655F76617273__a2
.Lcont72:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont73, pl_reg_bank+1040
	jmp	X0_name_singleton_vars__a1
.Lcont73:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
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
	movl	ta+180, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_bind_variables__a2
	.p2align	4,,15
	.type	X1_2472656D6F76655F756E64657273636F72655F76617273__a2,@function
.globl X1_2472656D6F76655F756E64657273636F72655F76617273__a2

X1_2472656D6F76655F756E64657273636F72655F76617273__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred27_2+0, %eax
	movl	$.Lpred27_3+0, %edx
	movl	$.Lpred27_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred27_1:
	movl	$.Lpred27_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred27_5

.Lpred27_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred27_7

.Lpred27_2:
	movl	$.Lpred27_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred27_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred27_4:
	movl	$.Lpred27_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred27_5:
	movl	$3, %eax
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
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont74, pl_reg_bank+1040
	jmp	X1_242472656D6F76655F756E64657273636F72655F766172732F325F2461757831__a2
.Lcont74:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2472656D6F76655F756E64657273636F72655F76617273__a2

.Lpred27_6:
	call	Pl_Delete_Choice_Point3

.Lpred27_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
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
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X1_2472656D6F76655F756E64657273636F72655F76617273__a2
	.p2align	4,,15
	.type	X1_242472656D6F76655F756E64657273636F72655F766172732F325F2461757831__a2,@function

X1_242472656D6F76655F756E64657273636F72655F766172732F325F2461757831__a2:
	movl	$.Lpred28_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	jmp	X0_sub_atom__a5

.Lpred28_1:
	call	Pl_Delete_Choice_Point2
	movl	fn+44, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	jmp	X0_sub_atom__a5
	.p2align	4,,15
	.type	X1_2477726974655F736F6C7574696F6E__a3,@function
.globl X1_2477726974655F736F6C7574696F6E__a3

X1_2477726974655F736F6C7574696F6E__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred29_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_242477726974655F736F6C7574696F6E2F335F2461757831__a2

.Lpred29_1:
	call	Pl_Delete_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont75, pl_reg_bank+1040
	jmp	X1_242477726974655F736F6C7574696F6E2F335F2461757832__a1
.Lcont75:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Dec
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2477726974655F736F6C7574696F6E31__a2
	.p2align	4,,15
	.type	X1_242477726974655F736F6C7574696F6E2F335F2461757832__a1,@function

X1_242477726974655F736F6C7574696F6E2F335F2461757832__a1:
	movl	$.Lpred30_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X0_current_op__a3

.Lpred30_1:
	call	Pl_Delete_Choice_Point1
	movl	$-2147480845, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242477726974655F736F6C7574696F6E2F335F2461757831__a2,@function

X1_242477726974655F736F6C7574696F6E2F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred31_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+196, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	jmp	X0_format__a3

.Lpred31_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2477726974655F736F6C7574696F6E31__a2,@function
.globl X1_2477726974655F736F6C7574696F6E31__a2

X1_2477726974655F736F6C7574696F6E31__a2:
	movl	$.Lpred32_1+0, %eax
	movl	$.Lpred32_2+0, %edx
	movl	$.Lpred32_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred32_1:
	movl	$.Lpred32_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred32_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_3:
	call	Pl_Delete_Choice_Point2

.Lpred32_4:
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
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont76, pl_reg_bank+1040
	jmp	X1_242477726974655F736F6C7574696F6E312F325F2461757831__a3
.Lcont76:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2477726974655F736F6C7574696F6E31__a2
	.p2align	4,,15
	.type	X1_242477726974655F736F6C7574696F6E312F325F2461757831__a3,@function

X1_242477726974655F736F6C7574696F6E312F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred33_1+0, %eax
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
	movl	$.Lcont77, pl_reg_bank+1040
	jmp	X0_acyclic_term__a1
.Lcont77:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+200, %eax
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
	movl	$.Lcont78, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont78:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+24
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+20
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	call	Pl_Deallocate
	jmp	X0_write_term__a3

.Lpred33_1:
	call	Pl_Delete_Choice_Point4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+204, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_24726561645F72657475726E__a0,@function
.globl X1_24726561645F72657475726E__a0

X1_24726561645F72657475726E__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont79, pl_reg_bank+1040
	jmp	X0_flush_output__a1
.Lcont79:
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont80, pl_reg_bank+1040
	jmp	X0_get_key__a2
.Lcont80:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont81, pl_reg_bank+1040
	jmp	X1_24726561645F72657475726E__a1
.Lcont81:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726561645F72657475726E__a1,@function
.globl X1_24726561645F72657475726E__a1

X1_24726561645F72657475726E__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred35_12+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$.Lpred35_2+0, %eax
	movl	$0, %edx
	movl	$.Lpred35_1+0, %ecx
	movl	$0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred35_1:
	call	Pl_Switch_On_Integer
	cmpl	$32, %eax
	je	.Lpred35_11
	jg	.Lcont82
	cmpl	$10, %eax
	je	.Lpred35_3
	cmpl	$13, %eax
	je	.Lpred35_5
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lcont82:
	cmpl	$59, %eax
	je	.Lpred35_9
	cmpl	$97, %eax
	je	.Lpred35_7
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred35_2:
	movl	$.Lpred35_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred35_3:
	movl	$-2147483605, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred35_4:
	movl	$.Lpred35_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred35_5:
	movl	$-2147483593, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred35_6:
	movl	$.Lpred35_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred35_7:
	movl	$-2147483257, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+208, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred35_8:
	movl	$.Lpred35_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred35_9:
	movl	$1, %eax
	call	Pl_Allocate
	movl	$-2147483409, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+172, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont83, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont83:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred35_10:
	call	Pl_Delete_Choice_Point2

.Lpred35_11:
	movl	$1, %eax
	call	Pl_Allocate
	movl	$-2147483517, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+212, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont84, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont84:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred35_12:
	call	Pl_Delete_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont85, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont85:
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+216, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont86, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont86:
	call	Pl_Deallocate
	jmp	X1_24726561645F72657475726E__a0
	.p2align	4,,15
	.type	X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1,@function
.globl X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1

X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred36_6+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$.Lpred36_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred36_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred36_1:
	movl	$.Lpred36_sub_0+0, %eax
	call	Pl_Create_Choice_Point2
	jmp	.Lpred36_3

.Lpred36_sub_0:
	call	Pl_Delete_Choice_Point2
	jmp	.Lpred36_5

.Lpred36_2:
	movl	$.Lpred36_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred36_3:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont87, pl_reg_bank+1040
	jmp	X1_2463617463685F696E7465726E616C__a4
.Lcont87:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont88, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont88:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred36_4:
	call	Pl_Delete_Choice_Point2

.Lpred36_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1

.Lpred36_6:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1,@function
.globl X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1

X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point2
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
	movl	$.Lcont89, pl_reg_bank+1040
	jmp	X1_24657865635F636D645F6C696E655F676F616C__a1
.Lcont89:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1

.Lpred37_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24657865635F636D645F6C696E655F676F616C__a1,@function
.globl X1_24657865635F636D645F6C696E655F676F616C__a1

X1_24657865635F636D645F6C696E655F676F616C__a1:
	jmp	X1_2424657865635F636D645F6C696E655F676F616C2F315F2461757831__a1
	.p2align	4,,15
	.type	X1_2424657865635F636D645F6C696E655F676F616C2F315F2461757831__a1,@function

X1_2424657865635F636D645F6C696E655F676F616C2F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont90, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont90:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred39_1:
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
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_format__a2
	.p2align	4,,15
	.type	X1_24657865635F636D6431__a1,@function
.globl X1_24657865635F636D6431__a1

X1_24657865635F636D6431__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	ta+152, %eax
	call	Pl_Unify_Atom_Tagged
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
	movl	$.Lcont91, pl_reg_bank+1040
	jmp	X0_read_term_from_atom__a3
.Lcont91:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2463616C6C__a4
	.p2align	4,,15
	.type	X1_24657865635F636D645F657272__a2,@function
.globl X1_24657865635F636D645F657272__a2

X1_24657865635F636D645F657272__a2:
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
	movl	ta+232, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X0_format__a2
	.p2align	4,,15
	.type	ensure_linked,@function

ensure_linked:
	jmp	X0_consult__a1
	jmp	X0_load__a1
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
	movl	%eax, at+180
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC48, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	$.LC49, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+176
	movl	$.LC50, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC51, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC52, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	$.LC53, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	$.LC54, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	$.LC55, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	at+56, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	$.LC56, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	$.LC57, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	$.LC58, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+192
	movl	$.LC59, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+216
	movl	$.LC60, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC61, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+212
	movl	$.LC62, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+184
	movl	$.LC63, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC64, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC65, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+224
	movl	$.LC66, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+136
	movl	$.LC67, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+92
	movl	$.LC68, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC69, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC70, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC71, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	$.LC72, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+180
	movl	$.LC73, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	$.LC74, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC75, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC76, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC77, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+208
	movl	at+4, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+168
	movl	$.LC78, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC79, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	$.LC80, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	$.LC81, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC82, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	$.LC83, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+128
	movl	$.LC84, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC85, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+124
	movl	$.LC86, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC87, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC88, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	$.LC89, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC90, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	$.LC91, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+232
	movl	$.LC92, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+228
	movl	$.LC93, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	$.LC94, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC95, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC96, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	$.LC97, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC98, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC99, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+204
	movl	$.LC100, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC101, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+200
	movl	$.LC102, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC103, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC19, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC104, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC29, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC33, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC58, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC105, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC106, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC107, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC72, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC108, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC109, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC110, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC111, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC112, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_top_level__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_break__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24627265616B2F305F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24627265616B2F305F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$87, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24746F705F6C6576656C31__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$94, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24746F705F6C6576656C5F61626F7274__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$103, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24746F705F6C6576656C5F73746F70__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$111, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24746F705F6C6576656C5F657863657074696F6E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$129, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247265696E69745F61667465725F657863657074696F6E__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$129, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24247265696E69745F61667465725F657863657074696F6E2F305F2461757831__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24746F705F6C6576656C32__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424746F705F6C6576656C322F305F2461757833__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424746F705F6C6576656C322F305F2461757835__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424746F705F6C6576656C322F305F2461757834__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424746F705F6C6576656C322F305F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424746F705F6C6576656C322F305F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$187, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2477726974655F696E64696361746F72__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$187, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242477726974655F696E64696361746F722F305F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$187, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242477726974655F696E64696361746F722F305F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$208, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246462675F696E64696361746F72__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+100, %ecx
	movl	$.Lpred20_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+104, %ecx
	movl	$.Lpred20_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+108, %ecx
	movl	$.Lpred20_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+112, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$217, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726561645F7175657279__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$217, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424726561645F71756572792F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F7175657279__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424657865635F71756572792F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424657865635F71756572792F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$264, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F71756572795F766172735F6E616D6573__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$273, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2472656D6F76655F756E64657273636F72655F76617273__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$275, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242472656D6F76655F756E64657273636F72655F766172732F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$288, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2477726974655F736F6C7574696F6E__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$295, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242477726974655F736F6C7574696F6E2F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$288, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242477726974655F736F6C7574696F6E2F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$301, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2477726974655F736F6C7574696F6E31__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$303, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242477726974655F736F6C7574696F6E312F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$315, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726561645F72657475726E__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$323, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726561645F72657475726E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$348, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F636D645F6C696E655F636F6E73756C745F66696C6573__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$363, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F636D645F6C696E655F656E7472795F676F616C73__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$373, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F636D645F6C696E655F676F616C__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$373, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424657865635F636D645F6C696E655F676F616C2F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$379, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F636D6431__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$384, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657865635F636D645F657272__a2+0, 8(%esp)
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
.LC49:
	.string	" ? "
.LC48:
	.string	""
.LC0:
	.string	"$$exec_cmd_line_goal/1_$aux1"
.LC1:
	.string	"$$exec_query/2_$aux1"
.LC2:
	.string	"$$exec_query/2_$aux2"
.LC3:
	.string	"$$read_query/2_$aux1"
.LC4:
	.string	"$$reinit_after_exception/0_$aux1"
.LC5:
	.string	"$$remove_underscore_vars/2_$aux1"
.LC6:
	.string	"$$top_level2/0_$aux1"
.LC7:
	.string	"$$top_level2/0_$aux2"
.LC8:
	.string	"$$top_level2/0_$aux3"
.LC9:
	.string	"$$top_level2/0_$aux4"
.LC10:
	.string	"$$top_level2/0_$aux5"
.LC11:
	.string	"$$write_indicator/0_$aux1"
.LC12:
	.string	"$$write_indicator/0_$aux2"
.LC13:
	.string	"$$write_solution/3_$aux1"
.LC14:
	.string	"$$write_solution/3_$aux2"
.LC15:
	.string	"$$write_solution1/2_$aux1"
.LC102:
	.string	"$VARNAME"
.LC50:
	.string	"$all_solutions"
.LC16:
	.string	"$break/0_$aux1"
.LC17:
	.string	"$break/0_$aux2"
.LC51:
	.string	"$cmd_line_consult_file"
.LC52:
	.string	"$cmd_line_entry_goal"
.LC53:
	.string	"$cmd_line_query_goal"
.LC103:
	.string	"$consult2"
.LC18:
	.string	"$dbg_indicator"
.LC54:
	.string	"$debug_mode"
.LC55:
	.string	"$debug_next"
.LC19:
	.string	"$exec_cmd1"
.LC20:
	.string	"$exec_cmd_err"
.LC21:
	.string	"$exec_cmd_line_consult_files"
.LC22:
	.string	"$exec_cmd_line_entry_goals"
.LC23:
	.string	"$exec_cmd_line_goal"
.LC24:
	.string	"$exec_query"
.LC104:
	.string	"$post_query_exception"
.LC25:
	.string	"$read_query"
.LC26:
	.string	"$read_return"
.LC27:
	.string	"$reinit_after_exception"
.LC28:
	.string	"$remove_underscore_vars"
.LC29:
	.string	"$set_query_vars_names"
.LC30:
	.string	"$top_level1"
.LC31:
	.string	"$top_level2"
.LC32:
	.string	"$top_level_abort"
.LC33:
	.string	"$top_level_exception"
.LC34:
	.string	"$top_level_stop"
.LC56:
	.string	"$user_prompt"
.LC35:
	.string	"$write_indicator"
.LC36:
	.string	"$write_solution"
.LC37:
	.string	"$write_solution1"
.LC57:
	.string	"(~d ms) "
.LC38:
	.string	"/home/diaz/GP/src/BipsPl/top_level.pl"
.LC58:
	.string	"="
.LC39:
	.string	">"
.LC59:
	.string	"Action (; for next solution, a for all solutions, RET to stop) ? "
.LC60:
	.string	"Compiled ~a with ~a~n"
.LC61:
	.string	"\b;~N"
.LC62:
	.string	"_"
.LC63:
	.string	"address_bits"
.LC40:
	.string	"break"
.LC64:
	.string	"c_cc"
.LC65:
	.string	"command-line"
.LC66:
	.string	"compiled_at"
.LC41:
	.string	"debug"
.LC67:
	.string	"end_of_file"
.LC105:
	.string	"end_of_term"
.LC68:
	.string	"eof"
.LC106:
	.string	"error"
.LC107:
	.string	"exclude"
.LC69:
	.string	"execution aborted\n"
.LC70:
	.string	"f"
.LC71:
	.string	"false"
.LC42:
	.string	"is"
.LC72:
	.string	"namevars"
.LC73:
	.string	"no"
.LC43:
	.string	"nodebug"
.LC108:
	.string	"numbervars"
.LC109:
	.string	"priority"
.LC74:
	.string	"prolog_copyright"
.LC75:
	.string	"prolog_name"
.LC76:
	.string	"prolog_version"
.LC110:
	.string	"quoted"
.LC44:
	.string	"system"
.LC77:
	.string	"t"
.LC111:
	.string	"throw"
.LC45:
	.string	"top_level"
.LC78:
	.string	"top_level_input"
.LC79:
	.string	"top_level_output"
.LC46:
	.string	"trace"
.LC80:
	.string	"true"
.LC47:
	.string	"user"
.LC112:
	.string	"variable_names"
.LC81:
	.string	"xfx"
.LC82:
	.string	"yes"
.LC83:
	.string	"{~a,~d}~n"
.LC84:
	.string	"{~a}~n"
.LC85:
	.string	"{~d}~n"
.LC86:
	.string	"| ?- "
.LC87:
	.string	"~N"
.LC88:
	.string	"~Ntop-level exception: "
.LC89:
	.string	"~Nuncaught exception: "
.LC90:
	.string	"~Nwarning: command-line consulting file ~q failed due to ~q~n"
.LC91:
	.string	"~Nwarning: command-line goal ~q caused exception: ~q~n"
.LC92:
	.string	"~Nwarning: command-line goal ~q failed~n"
.LC93:
	.string	"~N{Break Level ~d}~n"
.LC94:
	.string	"~N{End Break}~n"
.LC95:
	.string	"~N~a ~a (~d bits)~n"
.LC96:
	.string	"~N~n"
.LC97:
	.string	"~a.~n"
.LC98:
	.string	"~a~n"
.LC99:
	.string	"~ncannot display cyclic term for ~a"
.LC100:
	.string	"~ntrue"
.LC101:
	.string	"~n~a = "
	.data	
	.align	4
	.local	at
	.comm	at,192,4
	.local	fn
	.comm	fn,68,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,236,4
	.section	.note.GNU-stack,"",@progbits
