# BipsPl/debugger.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/debugger.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_24696E69745F6465627567676572__a0,@function
.globl X1_24696E69745F6465627567676572__a0

X1_24696E69745F6465627567676572__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$-2147483593, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont1:
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	call	Pl_Deallocate
	jmp	X1_2464656275675F7377697463685F6F6666__a0
	.p2align	4,,15
	.type	X1_2464656275675F7377697463685F6F6666__a0,@function
.globl X1_2464656275675F7377697463685F6F6666__a0

X1_2464656275675F7377697463685F6F6666__a0:
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483521, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Reset_Debug_Call_Code_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F7377697463685F6F6E__a1,@function
.globl X1_2464656275675F7377697463685F6F6E__a1

X1_2464656275675F7377697463685F6F6E__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Assign
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_G_Assign
	movl	ta+0, %eax
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
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_setarg__a4
.Lcont2:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_setarg__a4
.Lcont3:
	call	Pl_Deallocate
	call	Pl_Set_Debug_Call_Code_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_wam_debug__a0,@function
.globl X0_wam_debug__a0

X0_wam_debug__a0:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Debug_Wam
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_notrace__a0,@function
.globl X0_notrace__a0

X0_notrace__a0:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X0_nodebug__a0
	.p2align	4,,15
	.type	X0_nodebug__a0,@function
.globl X0_nodebug__a0

X0_nodebug__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_2464656275675F7377697463685F6F6666__a0
.Lcont4:
	call	Pl_Deallocate
	jmp	X1_2473686F775F64656275676765725F6D6F6465__a0
	.p2align	4,,15
	.type	X0_trace__a0,@function
.globl X0_trace__a0

X0_trace__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_2464656275675F7377697463685F6F6E__a1
.Lcont5:
	call	Pl_Deallocate
	jmp	X1_2473686F775F64656275676765725F6D6F6465__a0
	.p2align	4,,15
	.type	X0_debug__a0,@function
.globl X0_debug__a0

X0_debug__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_2464656275675F7377697463685F6F6E__a1
.Lcont6:
	call	Pl_Deallocate
	jmp	X1_2473686F775F64656275676765725F6D6F6465__a0
	.p2align	4,,15
	.type	X1_2473686F775F64656275676765725F6D6F6465__a0,@function
.globl X1_2473686F775F64656275676765725F6D6F6465__a0

X1_2473686F775F64656275676765725F6D6F6465__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+4, %eax
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
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_2473686F775F64656275676765725F6D6F646531__a1
.Lcont7:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_2473686F775F64656275676765725F6D6F646531__a1,@function
.globl X1_2473686F775F64656275676765725F6D6F646531__a1

X1_2473686F775F64656275676765725F6D6F646531__a1:
	movl	$.Lpred10_2+0, %eax
	movl	$.Lpred10_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred10_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred10_2:
	movl	$.Lpred10_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred10_3:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_write__a2

.Lpred10_4:
	movl	$.Lpred10_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred10_5:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont8:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_write__a2

.Lpred10_6:
	call	Pl_Delete_Choice_Point1

.Lpred10_7:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont9:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_write__a2
	.p2align	4,,15
	.type	X0_debugging__a0,@function
.globl X0_debugging__a0

X0_debugging__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_2473686F775F64656275676765725F6D6F6465__a0
.Lcont10:
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_2473686F775F6C65617368696E675F696E666F__a0
.Lcont11:
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_2473686F775F756E646566696E65645F616374696F6E__a0
.Lcont12:
	call	Pl_Deallocate
	jmp	X1_2473686F775F7370795F706F696E7473__a0
	.p2align	4,,15
	.type	X0_leash__a1,@function
.globl X0_leash__a1

X0_leash__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred12_1:
	movl	$.Lpred12_13+0, %eax
	call	Pl_Update_Choice_Point2
	movl	$.Lpred12_3+0, %eax
	movl	$.Lpred12_2+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred12_2:
	movl	st+4, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred12_3:
	movl	$.Lpred12_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred12_4:
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
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
	movl	ta+72, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+80, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_leash__a1

.Lpred12_5:
	movl	$.Lpred12_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred12_6:
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+72, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_leash__a1

.Lpred12_7:
	movl	$.Lpred12_9+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred12_8:
	movl	ta+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_leash__a1

.Lpred12_9:
	movl	$.Lpred12_11+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred12_10:
	movl	ta+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+0
	jmp	X0_leash__a1

.Lpred12_11:
	call	Pl_Delete_Choice_Point2

.Lpred12_12:
	movl	ta+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+72, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	ta+80, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	X0_leash__a1

.Lpred12_13:
	movl	$.Lpred12_14+0, %eax
	call	Pl_Update_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_246C656173685F6D616B655F6D61736B__a2
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	X1_2473686F775F6C65617368696E675F696E666F__a0

.Lpred12_14:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+100, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_246C656173685F6D616B655F6D61736B__a2,@function
.globl X1_246C656173685F6D616B655F6D61736B__a2

X1_246C656173685F6D616B655F6D61736B__a2:
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
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_3:
	call	Pl_Delete_Choice_Point2

.Lpred13_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_246C656173685F6D616B655F6D61736B__a2
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-28(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Or
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473686F775F6C65617368696E675F696E666F__a0,@function
.globl X1_2473686F775F6C65617368696E675F696E666F__a0

X1_2473686F775F6C65617368696E675F696E666F__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+16, %eax
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
	jmp	X1_2473686F775F6C65617368696E675F696E666F31__a1
.Lcont16:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_2473686F775F6C65617368696E675F696E666F31__a1,@function
.globl X1_2473686F775F6C65617368696E675F696E666F31__a1

X1_2473686F775F6C65617368696E675F696E666F31__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+104, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_write__a2

.Lpred15_1:
	call	Pl_Delete_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+108, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont17:
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483281, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_2473686F775F6C65617368696E675F696E666F32__a1
.Lcont18:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+116, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_write__a2
	.p2align	4,,15
	.type	X1_2473686F775F6C65617368696E675F696E666F32__a1,@function
.globl X1_2473686F775F6C65617368696E675F696E666F32__a1

X1_2473686F775F6C65617368696E675F696E666F32__a1:
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point1
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
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-24(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
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
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483469, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+120, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont20:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred16_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F69735F6E6F745F6C656173686564__a1,@function
.globl X1_2464656275675F69735F6E6F745F6C656173686564__a1

X1_2464656275675F69735F6E6F745F6C656173686564__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-16(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_And
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Eq
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F706F72745F6D61736B__a2,@function
.globl X1_2464656275675F706F72745F6D61736B__a2

X1_2464656275675F706F72745F6D61736B__a2:
	movl	$.Lpred18_2+0, %eax
	movl	$.Lpred18_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred18_1:
	movl	st+8, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred18_2:
	movl	$.Lpred18_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred18_3:
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
	jmp	*pl_reg_bank+1040

.Lpred18_4:
	movl	$.Lpred18_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_5:
	movl	ta+68, %eax
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

.Lpred18_6:
	movl	$.Lpred18_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_7:
	movl	ta+72, %eax
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

.Lpred18_8:
	movl	$.Lpred18_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_9:
	movl	ta+76, %eax
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

.Lpred18_10:
	call	Pl_Delete_Choice_Point2

.Lpred18_11:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483581, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473686F775F756E646566696E65645F616374696F6E__a0,@function
.globl X1_2473686F775F756E646566696E65645F616374696F6E__a0

X1_2473686F775F756E646566696E65645F616374696F6E__a0:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+124, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont22:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+128, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_2473686F775F756E646566696E65645F616374696F6E31__a1
.Lcont24:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_2473686F775F756E646566696E65645F616374696F6E31__a1,@function
.globl X1_2473686F775F756E646566696E65645F616374696F6E31__a1

X1_2473686F775F756E646566696E65645F616374696F6E31__a1:
	movl	$.Lpred20_2+0, %eax
	movl	$.Lpred20_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred20_1:
	movl	st+12, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred20_2:
	movl	$.Lpred20_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred20_3:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+136, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_write__a2

.Lpred20_4:
	movl	$.Lpred20_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred20_5:
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+144, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_write__a2

.Lpred20_6:
	call	Pl_Delete_Choice_Point1

.Lpred20_7:
	movl	ta+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_write__a2
	.p2align	4,,15
	.type	X0_spypoint_condition__a3,@function
.globl X0_spypoint_condition__a3

X0_spypoint_condition__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	at+136, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred21_1:
	movl	$.Lpred21_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_24737079706F696E745F636F6E646974696F6E31__a3

.Lpred21_2:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+148, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_24737079706F696E745F636F6E646974696F6E31__a3,@function
.globl X1_24737079706F696E745F636F6E646974696F6E31__a3

X1_24737079706F696E745F636F6E646974696F6E31__a3:
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$0, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+20, %edx
	movl	pl_reg_bank+24, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_2424737079706F696E745F636F6E646974696F6E312F335F2461757831__a5
.Lcont25:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred22_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424737079706F696E745F636F6E646974696F6E312F335F2461757831__a5,@function

X1_2424737079706F696E745F636F6E646974696F6E312F335F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred23_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
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
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -36(%edi)
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
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F616E79__a1
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Cut
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_2464656275675F7370795F736574__a2
.Lcont27:
	movl	ta+4, %eax
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
	call	Pl_Deallocate
	jmp	X0_debug__a0

.Lpred23_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+152, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X0_spy__a1,@function
.globl X0_spy__a1

X0_spy__a1:
	movl	$.Lpred24_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+152, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F70726564__a2
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X1_2464656275675F7370795F736574__a2
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
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
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X0_debug__a0
.Lcont30:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred24_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nospy__a1,@function
.globl X0_nospy__a1

X0_nospy__a1:
	movl	$.Lpred25_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+156, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F70726564__a2
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X1_2464656275675F7370795F7265736574__a1
.Lcont32:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred25_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nospyall__a0,@function
.globl X0_nospyall__a0

X0_nospyall__a0:
	movl	$.Lpred26_1+0, %eax
	movl	$0, %edx
	call	Pl_Create_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+160, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$3, %eax
	call	Pl_Unify_Void
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X0_retractall__a1
.Lcont33:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+156, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont34:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont35:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred26_1:
	movl	$0, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F7370795F736574__a2,@function
.globl X1_2464656275675F7370795F736574__a2

X1_2464656275675F7370795F736574__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred27_1+0, %eax
	movl	$.Lpred27_2+0, %edx
	movl	$.Lpred27_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred27_1:
	movl	$.Lpred27_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred27_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred27_3:
	call	Pl_Delete_Choice_Point3

.Lpred27_4:
	movl	$6, %eax
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
	movl	fn+4, %eax
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
	movl	pl_reg_bank+8, %eax
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X1_242464656275675F7370795F7365742F325F2461757831__a3
.Lcont36:
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X0_assertz__a1
.Lcont37:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+160, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
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
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2464656275675F7370795F736574__a2
	.p2align	4,,15
	.type	X1_242464656275675F7370795F7365742F325F2461757831__a3,@function

X1_242464656275675F7370795F7365742F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred28_1+0, %eax
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
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+164, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred28_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+168, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F7370795F7265736574__a1,@function
.globl X1_2464656275675F7370795F7265736574__a1

X1_2464656275675F7370795F7265736574__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred29_1+0, %eax
	movl	$.Lpred29_2+0, %edx
	movl	$.Lpred29_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred29_1:
	movl	$.Lpred29_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred29_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred29_3:
	call	Pl_Delete_Choice_Point2

.Lpred29_4:
	movl	$5, %eax
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
	movl	fn+4, %eax
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_242464656275675F7370795F72657365742F315F2461757831__a3
.Lcont40:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+160, %eax
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
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2464656275675F7370795F7265736574__a1
	.p2align	4,,15
	.type	X1_242464656275675F7370795F72657365742F315F2461757831__a3,@function

X1_242464656275675F7370795F72657365742F315F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred30_1+0, %eax
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
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X0_retract__a1
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+172, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred30_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+176, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246861735F7370795F706F696E74__a2,@function
.globl X1_246861735F7370795F706F696E74__a2

X1_246861735F7370795F706F696E74__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+16, %edx
	movl	pl_reg_bank+20, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246861735F6E6F5F7370795F706F696E74__a1,@function
.globl X1_246861735F6E6F5F7370795F706F696E74__a1

X1_246861735F6E6F5F7370795F706F696E74__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred32_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+12, %eax
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
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred32_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247370795F746573745F636F6E646974696F6E__a3,@function
.globl X1_247370795F746573745F636F6E646974696F6E__a3

X1_247370795F746573745F636F6E646974696F6E__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	fn+8, %eax
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
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_24247370795F746573745F636F6E646974696F6E2F335F2461757831__a2
	.p2align	4,,15
	.type	X1_24247370795F746573745F636F6E646974696F6E2F335F2461757831__a2,@function

X1_24247370795F746573745F636F6E646974696F6E2F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred34_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred34_1:
	call	Pl_Delete_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+180, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473686F775F7370795F706F696E7473__a0,@function
.globl X1_2473686F775F7370795F706F696E7473__a0

X1_2473686F775F7370795F706F696E7473__a0:
	movl	$.Lpred35_1+0, %eax
	movl	$0, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+184, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont46:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont47:
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X0_clause__a2
.Lcont48:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+188, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
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
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont49:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred35_1:
	movl	$0, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F6C6973745F6F665F70726564__a2,@function
.globl X1_2464656275675F6C6973745F6F665F70726564__a2

X1_2464656275675F6C6973745F6F665F70726564__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred36_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred36_1:
	movl	$.Lpred36_8+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$.Lpred36_2+0, %eax
	movl	$.Lpred36_3+0, %edx
	movl	$0, %ecx
	movl	$.Lpred36_5+0, 0(%esp)
	movl	$.Lpred36_7+0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred36_2:
	movl	$.Lpred36_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred36_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred36_4:
	movl	$.Lpred36_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred36_5:
	movl	$4, %eax
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
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F70726564__a2
.Lcont50:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F70726564__a2
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_append__a3

.Lpred36_6:
	call	Pl_Delete_Choice_Point3

.Lpred36_7:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F7072656431__a4
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred36_8:
	movl	$.Lpred36_9+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$4, %eax
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
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X1_2464656275675F6C6973745F6F665F7072656431__a4
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred36_9:
	call	Pl_Delete_Choice_Point3
	movl	$3, %eax
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
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X1_246765745F707265645F696E646963__a3
.Lcont55:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2464656275675F6C6973745F6F665F7072656431__a4
	.p2align	4,,15
	.type	X1_2464656275675F6C6973745F6F665F7072656431__a4,@function
.globl X1_2464656275675F6C6973745F6F665F7072656431__a4

X1_2464656275675F6C6973745F6F665F7072656431__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred37_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred37_1:
	movl	$.Lpred37_2+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred37_2:
	movl	$.Lpred37_3+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred37_3:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	ta+192, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont56:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-32(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-32(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757831__a4
.Lcont57:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757832__a5
	.p2align	4,,15
	.type	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757832__a5,@function

X1_242464656275675F6C6973745F6F665F70726564312F345F2461757832__a5:
	movl	$.Lpred38_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757833__a4
.Lcont58:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+196, %eax
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a3

.Lpred38_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757833__a4,@function

X1_242464656275675F6C6973745F6F665F70726564312F345F2461757833__a4:
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred39_1:
	movl	$.Lpred39_2+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+200, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred39_2:
	call	Pl_Delete_Choice_Point4
	movl	fn+16, %eax
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
	.type	X1_242464656275675F6C6973745F6F665F70726564312F345F2461757831__a4,@function

X1_242464656275675F6C6973745F6F665F70726564312F345F2461757831__a4:
	movl	$.Lpred40_1+0, %eax
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
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F616E79__a1
.Lcont59:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-20(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-24(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	ta+112, %eax
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
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Value
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
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred40_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+112, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F63616C6C__a2,@function
.globl X1_2464656275675F63616C6C__a2

X1_2464656275675F63616C6C__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred41_23+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred41_3+0, %eax
	movl	$.Lpred41_1+0, %edx
	movl	$.Lpred41_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred41_1:
	movl	st+16, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred41_2:
	movl	st+20, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred41_3:
	movl	$.Lpred41_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred41_4:
	movl	ta+204, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_notrace__a0

.Lpred41_5:
	movl	$.Lpred41_7+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_6:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_nodebug__a0

.Lpred41_7:
	movl	$.Lpred41_9+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_8:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_trace__a0

.Lpred41_9:
	movl	$.Lpred41_11+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_10:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_debug__a0

.Lpred41_11:
	movl	$.Lpred41_13+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_12:
	movl	ta+208, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_debugging__a0

.Lpred41_13:
	movl	$.Lpred41_15+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_14:
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_leash__a1

.Lpred41_15:
	movl	$.Lpred41_17+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_16:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_spy__a1

.Lpred41_17:
	movl	$.Lpred41_19+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_18:
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
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X0_spypoint_condition__a3

.Lpred41_19:
	movl	$.Lpred41_21+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred41_20:
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_nospy__a1

.Lpred41_21:
	call	Pl_Delete_Choice_Point3

.Lpred41_22:
	movl	ta+212, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X0_nospyall__a0

.Lpred41_23:
	call	Pl_Delete_Choice_Point3
	movl	$10, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+0, %eax
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
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X1_242464656275675F63616C6C2F325F2461757831__a2
.Lcont60:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -40(%edi)
	movl	-36(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Fast_Inc
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -44(%edi)
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont61:
	movl	pl_reg_bank+1044, %edi
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
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
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-48(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X0_setarg__a3
.Lcont62:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X0_setarg__a3
.Lcont63:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+28
	call	Pl_Deallocate
	jmp	X1_2464656275675F63616C6C31__a8
	.p2align	4,,15
	.type	X1_242464656275675F63616C6C2F325F2461757831__a2,@function

X1_242464656275675F63616C6C2F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
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
	call	Pl_Cut
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred42_3:
	call	Pl_Delete_Choice_Point3

.Lpred42_4:
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
	movl	$2, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F63616C6C31__a8,@function
.globl X1_2464656275675F63616C6C31__a8

X1_2464656275675F63616C6C31__a8:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+32
	movl	$.Lpred43_1+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point
	movl	$10, %eax
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
	movl	pl_reg_bank+28, %eax
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, -44(%edi)
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont64, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont64:
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+44, %eax
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
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont65, pl_reg_bank+1040
	jmp	X1_2463617463685F696E7465726E616C__a4
.Lcont65:
	movl	pl_reg_bank+1044, %edi
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont66, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont66:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont67, pl_reg_bank+1040
	jmp	X1_2464656275675F656E645F63616C6C__a6
.Lcont67:
	movl	pl_reg_bank+1044, %edi
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_242464656275675F63616C6C312F385F2461757831__a3

.Lpred43_1:
	movl	$.Lpred43_2+0, %eax
	movl	$9, %edx
	call	Pl_Update_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont68, pl_reg_bank+1040
	jmp	X1_2464656275675F706F7274__a5
.Lcont68:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred43_2:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+28, %eax
	movl	%eax, -28(%edi)
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont69, pl_reg_bank+1040
	jmp	X0_setarg__a3
.Lcont69:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont70, pl_reg_bank+1040
	jmp	X0_setarg__a3
.Lcont70:
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
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
	call	Pl_Deallocate
	jmp	X1_2464656275675F63616C6C__a2
	.p2align	4,,15
	.type	X1_242464656275675F63616C6C312F385F2461757831__a3,@function

X1_242464656275675F63616C6C312F385F2461757831__a3:
	movl	$.Lpred44_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred44_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F63616C6C5F706F7274__a5,@function
.globl X1_2464656275675F63616C6C5F706F7274__a5

X1_2464656275675F63616C6C5F706F7274__a5:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+216, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont71, pl_reg_bank+1040
	jmp	X1_2464656275675F706F7274__a5
.Lcont71:
	movl	ta+216, %eax
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
	call	Pl_Deallocate
	jmp	X1_242464656275675F63616C6C5F706F72742F355F2461757831__a3
	.p2align	4,,15
	.type	X1_242464656275675F63616C6C5F706F72742F355F2461757831__a3,@function

X1_242464656275675F63616C6C5F706F72742F355F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred46_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+220, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_2463616C6C5F66726F6D5F6465627567676572__a2

.Lpred46_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F656E645F63616C6C__a6,@function
.globl X1_2464656275675F656E645F63616C6C__a6

X1_2464656275675F656E645F63616C6C__a6:
	movl	$.Lpred47_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -20(%edi)
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont72, pl_reg_bank+1040
	jmp	X1_2464656275675F706F7274__a5
.Lcont72:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_setarg__a3

.Lpred47_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont73, pl_reg_bank+1040
	jmp	X1_2464656275675F706F7274__a5
.Lcont73:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_2464656275675F657863657074696F6E5F706F7274__a5,@function
.globl X1_2464656275675F657863657074696F6E5F706F7274__a5

X1_2464656275675F657863657074696F6E5F706F7274__a5:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+224, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Assign
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont74, pl_reg_bank+1040
	jmp	X1_2464656275675F706F7274__a5
.Lcont74:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+228, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+232, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_247468726F77__a4
	.p2align	4,,15
	.type	X1_2464656275675F706F7274__a5,@function
.globl X1_2464656275675F706F7274__a5

X1_2464656275675F706F7274__a5:
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
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont75, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont75:
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
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2464656275675F706F727431__a6
	.p2align	4,,15
	.type	X1_2464656275675F706F727431__a6,@function
.globl X1_2464656275675F706F727431__a6

X1_2464656275675F706F727431__a6:
	movl	$.Lpred50_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	$.Lcont76, pl_reg_bank+1040
	jmp	X1_2464656275675F706F727432__a6
.Lcont76:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred50_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	ta+12, %eax
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
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F706F727432__a6,@function
.globl X1_2464656275675F706F727432__a6

X1_2464656275675F706F727432__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred51_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont77, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F69676E6F7265__a4
.Lcont77:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred51_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	X1_2464656275675F706F72745F70726F6D7074__a6
	.p2align	4,,15
	.type	X1_2464656275675F706F72745F69676E6F7265__a4,@function
.globl X1_2464656275675F706F72745F69676E6F7265__a4

X1_2464656275675F706F72745F69676E6F7265__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred52_4+0, %eax
	movl	$.Lpred52_1+0, %edx
	movl	$.Lpred52_3+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred52_1:
	movl	st+24, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred52_2:
	movl	$.Lpred52_sub_0+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred52_7

.Lpred52_sub_0:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred52_9

.Lpred52_3:
	movl	st+28, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred52_4:
	movl	$.Lpred52_6+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred52_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred52_6:
	movl	$.Lpred52_8+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred52_7:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_246861735F6E6F5F7370795F706F696E74__a1

.Lpred52_8:
	movl	$.Lpred52_10+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred52_9:
	movl	$3, %eax
	call	Pl_Allocate
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont78, pl_reg_bank+1040
	jmp	X1_246861735F7370795F706F696E74__a2
.Lcont78:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_242464656275675F706F72745F69676E6F72652F345F2461757831__a3

.Lpred52_10:
	movl	$.Lpred52_12+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred52_11:
	movl	$5, %eax
	call	Pl_Allocate
	movl	ta+236, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -20(%edi)
	movl	ta+240, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont79, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont79:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_242464656275675F706F72745F69676E6F72652F345F2461757832__a5

.Lpred52_12:
	movl	$.Lpred52_14+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point

.Lpred52_13:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont80, pl_reg_bank+1040
	jmp	X1_242464656275675F706F72745F69676E6F72652F345F2461757833__a3
.Lcont80:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred52_14:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred52_15:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F706F72745F69676E6F72652F345F2461757833__a3,@function

X1_242464656275675F706F72745F69676E6F72652F345F2461757833__a3:
	movl	$.Lpred53_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred53_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F706F72745F69676E6F72652F345F2461757832__a5,@function

X1_242464656275675F706F72745F69676E6F72652F345F2461757832__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred54_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+12, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
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
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred54_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F706F72745F69676E6F72652F345F2461757831__a3,@function

X1_242464656275675F706F72745F69676E6F72652F345F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred55_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont81, pl_reg_bank+1040
	jmp	X1_247370795F746573745F636F6E646974696F6E__a3
.Lcont81:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred55_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F706F72745F70726F6D7074__a6,@function
.globl X1_2464656275675F706F72745F70726F6D7074__a6

X1_2464656275675F706F72745F70726F6D7074__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$7, %eax
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
	movl	$.Lcont82, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont82:
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
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
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont83, pl_reg_bank+1040
	jmp	X1_2464656275675F77726974655F676F616C__a4
.Lcont83:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont84, pl_reg_bank+1040
	jmp	X1_242464656275675F706F72745F70726F6D70742F365F2461757831__a5
.Lcont84:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F706F72745F70726F6D70742F365F2461757831__a5,@function

X1_242464656275675F706F72745F70726F6D70742F365F2461757831__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred57_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont85, pl_reg_bank+1040
	jmp	X1_246861735F6E6F5F7370795F706F696E74__a1
.Lcont85:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont86, pl_reg_bank+1040
	jmp	X1_2464656275675F69735F6E6F745F6C656173686564__a1
.Lcont86:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1

.Lpred57_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
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
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont87, pl_reg_bank+1040
	jmp	X1_2464656275675F726561645F636D64__a1
.Lcont87:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2464656275675F657865635F636D64__a6
	.p2align	4,,15
	.type	X1_2464656275675F726561645F636D64__a1,@function
.globl X1_2464656275675F726561645F636D64__a1

X1_2464656275675F726561645F636D64__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+244, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont88, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont88:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont89, pl_reg_bank+1040
	jmp	X0_flush_output__a1
.Lcont89:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont90, pl_reg_bank+1040
	jmp	X0_get_key__a2
.Lcont90:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147482625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lt
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont91, pl_reg_bank+1040
	jmp	X0_char_code__a2
.Lcont91:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+252, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_2464656275675F726561645F696E7465676572__a1,@function
.globl X1_2464656275675F726561645F696E7465676572__a1

X1_2464656275675F726561645F696E7465676572__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont92, pl_reg_bank+1040
	jmp	X0_read_integer__a2
.Lcont92:
	movl	$.Lcont93, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont93:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont94, pl_reg_bank+1040
	jmp	X0_get_code__a2
.Lcont94:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F657865635F636D64__a6,@function
.globl X1_2464656275675F657865635F636D64__a6

X1_2464656275675F657865635F636D64__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred60_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont95, pl_reg_bank+1040
	jmp	X0_char_code__a2
.Lcont95:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483605, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Gte
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	$-2147483593, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Fast_Lte
	testl	%eax, %eax
	je	fail
	movl	ta+256, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2464656275675F657865635F636D64__a6

.Lpred60_1:
	movl	$.Lpred60_66+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point
	movl	$.Lpred60_6+0, %eax
	movl	$.Lpred60_2+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred60_2:
	movl	st+32, %eax
	movl	$27, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred60_3:
	movl	$.Lpred60_sub_0+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred60_11

.Lpred60_sub_0:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred60_13

.Lpred60_4:
	movl	$.Lpred60_sub_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred60_17

.Lpred60_sub_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred60_19

.Lpred60_5:
	movl	$.Lpred60_sub_2+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	jmp	.Lpred60_21

.Lpred60_sub_2:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	.Lpred60_23

.Lpred60_6:
	movl	$.Lpred60_8+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred60_7:
	movl	ta+256, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred60_8:
	movl	$.Lpred60_10+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_9:
	movl	ta+260, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred60_10:
	movl	$.Lpred60_12+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_11:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+264, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont96, pl_reg_bank+1040
	jmp	X1_242464656275675F657865635F636D642F365F2461757831__a1
.Lcont96:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	ta+256, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2464656275675F657865635F636D64__a6

.Lpred60_12:
	movl	$.Lpred60_14+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_13:
	movl	$4, %eax
	call	Pl_Allocate
	movl	ta+264, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont97, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont97:
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont98, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont98:
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont99, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F6D61736B__a2
.Lcont99:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Fast_Load_Value
	movl	-24(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Or
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Fast_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Fast_Or
	movl	%eax, pl_reg_bank+8
	movl	ta+240, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred60_14:
	movl	$.Lpred60_16+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_15:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+268, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+272, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont100, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont100:
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont101, pl_reg_bank+1040
	jmp	X1_2464656275675F726561645F696E7465676572__a1
.Lcont101:
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+236, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+240, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+52, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$-2147483521, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred60_16:
	movl	$.Lpred60_18+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_17:
	movl	ta+276, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+64, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_18:
	movl	$.Lpred60_20+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_19:
	movl	ta+276, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+48, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred60_20:
	movl	$.Lpred60_22+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_21:
	movl	ta+280, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_22:
	movl	$.Lpred60_24+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_23:
	movl	ta+280, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+56, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred60_24:
	movl	$.Lpred60_26+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_25:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+284, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -20(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+288, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont102, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont102:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont103, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont103:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont104, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont104:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_26:
	movl	$.Lpred60_28+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_27:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+292, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -20(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+288, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont105, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont105:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont106, pl_reg_bank+1040
	jmp	X0_display__a2
.Lcont106:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont107, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont107:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_28:
	movl	$.Lpred60_30+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_29:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+296, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -20(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+288, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont108, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont108:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont109, pl_reg_bank+1040
	jmp	X0_print__a2
.Lcont109:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont110, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont110:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_30:
	movl	$.Lpred60_32+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_31:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+300, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont111, pl_reg_bank+1040
	jmp	X1_242464656275675F657865635F636D642F365F2461757832__a1
.Lcont111:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_32:
	movl	$.Lpred60_34+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_33:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+304, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont112, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616E635F6C7374__a1
.Lcont112:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_34:
	movl	$.Lpred60_36+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_35:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+308, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont113, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616C7465726E617469766573__a2
.Lcont113:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_36:
	movl	$.Lpred60_38+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_37:
	movl	ta+312, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_242464656275675F657865635F636D642F365F2461757833__a2

.Lpred60_38:
	movl	$.Lpred60_40+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_39:
	movl	ta+316, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X1_2464656275675F7377697463685F6F6666__a0

.Lpred60_40:
	movl	$.Lpred60_42+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_41:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+320, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont114, pl_reg_bank+1040
	jmp	X0_debugging__a0
.Lcont114:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_42:
	movl	$.Lpred60_44+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_43:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+324, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$.Lcont115, pl_reg_bank+1040
	jmp	X1_242464656275675F657865635F636D642F365F2461757834__a2
.Lcont115:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_44:
	movl	$.Lpred60_46+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_45:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+328, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
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
	movl	$.Lcont116, pl_reg_bank+1040
	jmp	X0_spy__a1
.Lcont116:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_46:
	movl	$.Lpred60_48+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_47:
	movl	$5, %eax
	call	Pl_Allocate
	movl	ta+332, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont117, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont117:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+336, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont118, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont118:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+60, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	fn+60, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	$.Lcont119, pl_reg_bank+1040
	jmp	X0_read__a2
.Lcont119:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont120, pl_reg_bank+1040
	jmp	X0_spypoint_condition__a3
.Lcont120:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_48:
	movl	$.Lpred60_50+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_49:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+340, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
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
	movl	$.Lcont121, pl_reg_bank+1040
	jmp	X0_nospy__a1
.Lcont121:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_50:
	movl	$.Lpred60_52+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_51:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+344, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+24, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	movl	pl_reg_bank+16, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont122, pl_reg_bank+1040
	jmp	X1_242464656275675F657865635F636D642F365F2461757835__a3
.Lcont122:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_52:
	movl	$.Lpred60_54+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_53:
	movl	ta+348, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	X0_abort__a0

.Lpred60_54:
	movl	$.Lpred60_56+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_55:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+352, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont123, pl_reg_bank+1040
	jmp	X0_break__a0
.Lcont123:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_56:
	movl	$.Lpred60_58+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_57:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+356, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+360, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont124, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont124:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont125, pl_reg_bank+1040
	jmp	X0_read__a2
.Lcont125:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont126, pl_reg_bank+1040
	jmp	X1_242464656275675F657865635F636D642F365F2461757836__a1
.Lcont126:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_58:
	movl	$.Lpred60_60+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_59:
	movl	$2, %eax
	call	Pl_Allocate
	movl	ta+364, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+368, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont127, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont127:
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont128, pl_reg_bank+1040
	jmp	X1_2464656275675F726561645F696E7465676572__a1
.Lcont128:
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_60:
	movl	$.Lpred60_62+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_61:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+372, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont129, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F68656C70__a0
.Lcont129:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_62:
	movl	$.Lpred60_64+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred60_63:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+376, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont130, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F68656C70__a0
.Lcont130:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_64:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred60_65:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+380, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont131, pl_reg_bank+1040
	jmp	X0_wam_debug__a0
.Lcont131:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred60_66:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+384, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont132, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont132:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont133, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont133:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757836__a1,@function

X1_242464656275675F657865635F636D642F365F2461757836__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred61_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+64, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+36, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+388, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
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
	movl	ta+392, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont134, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont134:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred61_1:
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+396, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757835__a3,@function

X1_242464656275675F657865635F636D642F365F2461757835__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred62_1+0, %eax
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
	movl	$.Lcont135, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F616E79__a1
.Lcont135:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_242464656275675F657865635F636D642F365F2461757837__a3

.Lpred62_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
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
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+400, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757837__a3,@function

X1_242464656275675F657865635F636D642F365F2461757837__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred63_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+404, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont136, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F706572747931__a3
.Lcont136:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+408, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	call	Pl_Deallocate
	jmp	X0_format__a3

.Lpred63_1:
	call	Pl_Delete_Choice_Point4
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Reset_Debug_Call_Code_0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont137, pl_reg_bank+1040
	jmp	X1_246C697374696E675F616E79__a1
.Lcont137:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont138, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont138:
	call	Pl_Deallocate
	call	Pl_Set_Debug_Call_Code_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757834__a2,@function

X1_242464656275675F657865635F636D642F365F2461757834__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred64_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
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
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont139, pl_reg_bank+1040
	jmp	X1_246765745F7072656469636174655F66696C655F696E666F__a3
.Lcont139:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+412, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
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
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X0_format__a3

.Lpred64_1:
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
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+416, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757833__a2,@function

X1_242464656275675F657865635F636D642F365F2461757833__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred65_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+420, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont140, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont140:
	movl	ta+248, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont141, pl_reg_bank+1040
	jmp	X0_read__a2
.Lcont141:
	movl	ta+216, %eax
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

.Lpred65_1:
	call	Pl_Delete_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+424, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont142, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont142:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont143, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont143:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757832__a1,@function

X1_242464656275675F657865635F636D642F365F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred66_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+224, %eax
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
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+428, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3

.Lpred66_1:
	call	Pl_Delete_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+424, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont144, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont144:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_242464656275675F657865635F636D642F365F2461757831__a1,@function

X1_242464656275675F657865635F636D642F365F2461757831__a1:
	movl	$.Lpred67_2+0, %eax
	movl	$.Lpred67_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred67_1:
	movl	st+36, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred67_2:
	movl	$.Lpred67_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred67_3:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_4:
	movl	$.Lpred67_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred67_5:
	movl	ta+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred67_6:
	call	Pl_Delete_Choice_Point1

.Lpred67_7:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F77726974655F676F616C__a4,@function
.globl X1_2464656275675F77726974655F676F616C__a4

X1_2464656275675F77726974655F676F616C__a4:
	movl	$7, %eax
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
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont145, pl_reg_bank+1040
	jmp	X1_242464656275675F77726974655F676F616C2F345F2461757831__a2
.Lcont145:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont146, pl_reg_bank+1040
	jmp	X1_2464656275675F706F72745F707265747479__a2
.Lcont146:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+432, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
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
	movl	-40(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont147, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont147:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+68, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+232, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+72, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
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
	jmp	X0_write_term__a3
	.p2align	4,,15
	.type	X1_242464656275675F77726974655F676F616C2F345F2461757831__a2,@function

X1_242464656275675F77726974655F676F616C2F345F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred69_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	$.Lcont148, pl_reg_bank+1040
	jmp	X1_246861735F6E6F5F7370795F706F696E74__a1
.Lcont148:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+436, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred69_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+328, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F706F72745F707265747479__a2,@function
.globl X1_2464656275675F706F72745F707265747479__a2

X1_2464656275675F706F72745F707265747479__a2:
	movl	$.Lpred70_2+0, %eax
	movl	$.Lpred70_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred70_1:
	movl	st+40, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred70_2:
	movl	$.Lpred70_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred70_3:
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+440, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_4:
	movl	$.Lpred70_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_5:
	movl	ta+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+444, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_6:
	movl	$.Lpred70_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_7:
	movl	ta+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+448, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_8:
	movl	$.Lpred70_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_9:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+452, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_10:
	movl	$.Lpred70_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred70_11:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+456, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred70_12:
	call	Pl_Delete_Choice_Point2

.Lpred70_13:
	movl	ta+460, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+220, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616E635F6C7374__a1,@function
.globl X1_2464656275675F646973705F616E635F6C7374__a1

X1_2464656275675F646973705F616E635F6C7374__a1:
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
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$4, %eax
	call	Pl_Unify_Void
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+464, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont149, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont149:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont150, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont150:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2464656275675F646973705F616E635F6C737431__a1
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616E635F6C737431__a1,@function
.globl X1_2464656275675F646973705F616E635F6C737431__a1

X1_2464656275675F646973705F616E635F6C737431__a1:
	movl	$.Lpred72_1+0, %eax
	movl	$.Lpred72_2+0, %edx
	movl	$.Lpred72_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred72_1:
	movl	$.Lpred72_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred72_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred72_3:
	call	Pl_Delete_Choice_Point1

.Lpred72_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	$.Lcont151, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616E635F6C737431__a1
.Lcont151:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+460, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2464656275675F77726974655F676F616C__a4
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616C7465726E617469766573__a2,@function
.globl X1_2464656275675F646973705F616C7465726E617469766573__a2

X1_2464656275675F646973705F616C7465726E617469766573__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+468, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont152, pl_reg_bank+1040
	jmp	X0_write__a2
.Lcont152:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont153, pl_reg_bank+1040
	jmp	X0_nl__a1
.Lcont153:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont154, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616C7465726E61746976657331__a2
.Lcont154:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+472, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616C7465726E61746976657331__a2,@function
.globl X1_2464656275675F646973705F616C7465726E61746976657331__a2

X1_2464656275675F646973705F616C7465726E61746976657331__a2:
	movl	$.Lpred74_1+0, %eax
	movl	$.Lpred74_2+0, %edx
	movl	$.Lpred74_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred74_1:
	movl	$.Lpred74_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred74_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred74_3:
	call	Pl_Delete_Choice_Point2

.Lpred74_4:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont155, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616C7465726E61746976657331__a2
.Lcont155:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+460, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont156, pl_reg_bank+1040
	jmp	X1_2464656275675F77726974655F676F616C__a4
.Lcont156:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2464656275675F646973705F616C74__a2
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616C74__a2,@function
.globl X1_2464656275675F646973705F616C74__a2

X1_2464656275675F646973705F616C74__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred75_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred75_1:
	call	Pl_Delete_Choice_Point3
	movl	$8, %eax
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
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont157, pl_reg_bank+1040
	jmp	X1_2463686F6963655F706F696E745F696E666F__a4
.Lcont157:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont158, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616C74__a2
.Lcont158:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont159, pl_reg_bank+1040
	jmp	X1_24707265645F776974686F75745F617578__a4
.Lcont159:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont160, pl_reg_bank+1040
	jmp	X1_2464656275675F646973705F616C7431__a3
.Lcont160:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616C7431__a3,@function
.globl X1_2464656275675F646973705F616C7431__a3

X1_2464656275675F646973705F616C7431__a3:
	movl	$.Lpred76_1+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	X1_2464656275675F69735F64656275675F707265646963617465__a1

.Lpred76_1:
	movl	$.Lpred76_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 8(%esp)
	call	Pl_Scan_Choice_Point_Info_3
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont161, pl_reg_bank+1040
	jmp	X1_24707265645F776974686F75745F617578__a4
.Lcont161:
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
	call	Pl_Deallocate
	jmp	X1_2464656275675F646973705F616C7432__a1

.Lpred76_2:
	movl	$.Lpred76_8+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$.Lpred76_4+0, %eax
	movl	$.Lpred76_3+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred76_3:
	movl	st+44, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred76_4:
	movl	$.Lpred76_6+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred76_5:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+476, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont162, pl_reg_bank+1040
	jmp	X1_2463686F6963655F706F696E745F617267__a3
.Lcont162:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+12, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X1_2464656275675F69735F64656275675F707265646963617465__a1

.Lpred76_6:
	call	Pl_Delete_Choice_Point3

.Lpred76_7:
	movl	ta+480, %eax
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

.Lpred76_8:
	movl	$.Lpred76_9+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
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
	movl	ta+484, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont163, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont163:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+404, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont164, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F706572747931__a3
.Lcont164:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_242464656275675F646973705F616C74312F335F2461757831__a2

.Lpred76_9:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
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
	jmp	X1_2464656275675F646973705F616C7432__a1
	.p2align	4,,15
	.type	X1_242464656275675F646973705F616C74312F335F2461757831__a2,@function

X1_242464656275675F646973705F616C74312F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred77_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont165, pl_reg_bank+1040
	jmp	X1_242464656275675F646973705F616C74312F335F2461757832__a3
.Lcont165:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	fn+4, %eax
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
	call	Pl_Deallocate
	jmp	X1_2464656275675F646973705F616C7432__a1

.Lpred77_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+76, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	fn+4, %eax
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
	jmp	X1_2464656275675F646973705F616C7432__a1
	.p2align	4,,15
	.type	X1_242464656275675F646973705F616C74312F335F2461757832__a3,@function

X1_242464656275675F646973705F616C74312F335F2461757832__a3:
	movl	$.Lpred78_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont166, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont166:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2464656275675F636865636B5F626970__a2

.Lpred78_1:
	call	Pl_Delete_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	$.Lcont167, pl_reg_bank+1040
	jmp	X0_sub_atom__a5
.Lcont167:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2464656275675F636865636B5F626970__a2
	.p2align	4,,15
	.type	X1_2464656275675F646973705F616C7432__a1,@function
.globl X1_2464656275675F646973705F616C7432__a1

X1_2464656275675F646973705F616C7432__a1:
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+488, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_2464656275675F69735F64656275675F707265646963617465__a1,@function
.globl X1_2464656275675F69735F64656275675F707265646963617465__a1

X1_2464656275675F69735F64656275675F707265646963617465__a1:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+12
	movl	ta+492, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	jmp	X0_sub_atom__a5
	.p2align	4,,15
	.type	X1_2464656275675F636865636B5F626970__a2,@function
.globl X1_2464656275675F636865636B5F626970__a2

X1_2464656275675F636865636B5F626970__a2:
	movl	ta+496, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_247072656469636174655F70726F706572747931__a3
	.p2align	4,,15
	.type	X1_2464656275675F646973705F68656C70__a0,@function
.globl X1_2464656275675F646973705F68656C70__a0

X1_2464656275675F646973705F68656C70__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+500, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont168, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont168:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+504, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont169, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont169:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+508, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont170, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont170:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+512, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont171, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont171:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+516, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont172, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont172:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+520, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont173, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont173:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+524, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont174, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont174:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+528, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont175, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont175:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+532, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont176, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont176:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+536, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont177, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont177:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+540, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont178, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont178:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+544, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont179, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont179:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+548, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont180, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont180:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+552, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont181, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont181:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_nl__a1
	.p2align	4,,15
	.type	X1_2463686F6963655F706F696E745F696E666F__a4,@function
.globl X1_2463686F6963655F706F696E745F696E666F__a4

X1_2463686F6963655F706F696E745F696E666F__a4:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Choice_Point_Info_4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463686F6963655F706F696E745F617267__a3,@function
.globl X1_2463686F6963655F706F696E745F617267__a3

X1_2463686F6963655F706F696E745F617267__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Choice_Point_Arg_3
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
	movl	%eax, at+224
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+492
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+496
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+440
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+436
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+432
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+428
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+420
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+416
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+424
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+448
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+484
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+520
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+516
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+508
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+476
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+480
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+500
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+468
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+472
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+460
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+464
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+512
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+504
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+452
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+444
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+488
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+376
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+372
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+380
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+368
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+400
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+364
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+404
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+396
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+352
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+384
	movl	$.LC87, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+412
	movl	$.LC88, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+388
	movl	$.LC89, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+392
	movl	$.LC90, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC91, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC92, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC93, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC94, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC95, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC96, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC97, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC98, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC99, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC100, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC101, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC102, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+348
	movl	$.LC103, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+408
	movl	$.LC104, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC105, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC106, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC107, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC108, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+360
	movl	$.LC109, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+456
	movl	$.LC110, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC111, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC112, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC113, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC114, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC115, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC116, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC117, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC118, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC119, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC120, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC121, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC122, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC123, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC124, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC125, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC126, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+356
	movl	$.LC127, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC128, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC129, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC130, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC131, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+220
	movl	$.LC132, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+436
	movl	$.LC133, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+288
	movl	$.LC134, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+536
	movl	$.LC135, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+540
	movl	$.LC136, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+548
	movl	$.LC137, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+544
	movl	$.LC138, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+524
	movl	$.LC139, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+532
	movl	$.LC140, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+520
	movl	$.LC141, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+512
	movl	$.LC142, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+508
	movl	$.LC143, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+528
	movl	$.LC144, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+516
	movl	$.LC145, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+188
	movl	$.LC146, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+552
	movl	$.LC147, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+244
	movl	$.LC148, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+484
	movl	at+484, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+476
	movl	$.LC149, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+492
	movl	$.LC150, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+224
	movl	$.LC151, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+248, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+228
	movl	$.LC152, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC153, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC154, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC155, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC156, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+240
	movl	$.LC157, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+216
	movl	$.LC158, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+112
	movl	at+488, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+480
	movl	at+376, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+332
	movl	at+372, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+328
	movl	at+380, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+340
	movl	at+368, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+324
	movl	at+400, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+364
	movl	at+364, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+320
	movl	at+404, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+372
	movl	at+396, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+356
	movl	at+352, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+308
	movl	$.LC159, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC160, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+468
	movl	$.LC161, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+464
	movl	$.LC162, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+440
	movl	$.LC163, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+360
	movl	$.LC164, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+500
	movl	$.LC165, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+428
	movl	$.LC166, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+456
	movl	$.LC167, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+452
	movl	$.LC168, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+448
	movl	at+320, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+268
	movl	$.LC169, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+336
	movl	$.LC170, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+420
	movl	$.LC171, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+272
	movl	at+384, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+344
	movl	$.LC172, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	$.LC173, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+424
	movl	$.LC174, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+368
	movl	$.LC175, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+504
	movl	$.LC176, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+444
	movl	$.LC177, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+168
	movl	$.LC178, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	$.LC179, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+184
	movl	$.LC180, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC181, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC182, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC183, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC184, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+176
	movl	$.LC185, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+128
	movl	$.LC186, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+384
	movl	$.LC187, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	at+412, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+380
	movl	$.LC188, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC189, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+196
	movl	$.LC190, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+388
	movl	$.LC191, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+396
	movl	$.LC192, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+116
	movl	at+388, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+348
	movl	$.LC193, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+200
	movl	at+392, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+352
	movl	$.LC194, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+496
	movl	at+308, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+256
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	$.LC195, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC196, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+400
	movl	at+336, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+292
	movl	at+36, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	$.LC197, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+392
	movl	$.LC198, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+248
	movl	$.LC199, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	at+48, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+208
	movl	$.LC200, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	at+344, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+300
	movl	at+128, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+80
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+68
	movl	at+328, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+280
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+76
	movl	$.LC201, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	at+56, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	at+348, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+304
	movl	at+408, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+376
	movl	at+60, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+84
	movl	at+312, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+260
	movl	$.LC202, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	at+64, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+88
	movl	$.LC203, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+192
	movl	at+360, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+316
	movl	$.LC204, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+408
	movl	$.LC205, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+404
	movl	$.LC206, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+416
	movl	at+456, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+460
	movl	at+28, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+68, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+212
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+204
	movl	at+340, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+296
	movl	at+324, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+276
	movl	$.LC207, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+136
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+72
	movl	at+316, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+264
	movl	$.LC208, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	$.LC209, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	at+268, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+236
	movl	$.LC210, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+180
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+96
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC211, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+232
	movl	at+356, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+312
	movl	$.LC212, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+124
	movl	at+332, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+284
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	$.LC213, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+252
	movl	$.LC214, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+488
	movl	$.LC215, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+432
	movl	$.LC216, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+472
	movl	$.LC217, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	$.LC218, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+412
	movl	$.LC219, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	$.LC29, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC40, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC220, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC221, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC77, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC222, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC88, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC90, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC92, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC100, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC223, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC106, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC224, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC112, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC225, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC118, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC119, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC121, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC122, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC226, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24696E69745F6465627567676572__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F7377697463685F6F6666__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F7377697463685F6F6E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_wam_debug__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$79, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_notrace__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$86, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nodebug__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$92, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_trace__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$98, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_debug__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F64656275676765725F6D6F6465__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$112, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F64656275676765725F6D6F646531__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+28, %ecx
	movl	$.Lpred10_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+32, %ecx
	movl	$.Lpred10_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+36, %ecx
	movl	$.Lpred10_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+48, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$126, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_debugging__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$136, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_leash__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$5, %edx
	movl	at+56, %ecx
	movl	$.Lpred12_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$5, %edx
	movl	at+60, %ecx
	movl	$.Lpred12_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$5, %edx
	movl	at+64, %ecx
	movl	$.Lpred12_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$5, %edx
	movl	at+68, %ecx
	movl	$.Lpred12_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$5, %edx
	movl	at+72, %ecx
	movl	$.Lpred12_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$169, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C656173685F6D616B655F6D61736B__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$179, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F6C65617368696E675F696E666F__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$185, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F6C65617368696E675F696E666F31__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$196, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F6C65617368696E675F696E666F32__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$209, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F69735F6E6F745F6C656173686564__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$217, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F72745F6D61736B__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+100, %ecx
	movl	$.Lpred18_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+104, %ecx
	movl	$.Lpred18_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+108, %ecx
	movl	$.Lpred18_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+112, %ecx
	movl	$.Lpred18_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+116, %ecx
	movl	$.Lpred18_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+120, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$230, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F756E646566696E65645F616374696F6E__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$237, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F756E646566696E65645F616374696F6E31__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+128, %ecx
	movl	$.Lpred20_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+132, %ecx
	movl	$.Lpred20_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+112, %ecx
	movl	$.Lpred20_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+136, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$249, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_spypoint_condition__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$262, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24737079706F696E745F636F6E646974696F6E31__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$262, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424737079706F696E745F636F6E646974696F6E312F335F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$278, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_spy__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$292, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nospy__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$303, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nospyall__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$315, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F7370795F736574__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$317, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F7370795F7365742F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$329, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F7370795F7265736574__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$331, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F7370795F72657365742F315F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$342, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246861735F7370795F706F696E74__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$347, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246861735F6E6F5F7370795F706F696E74__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$357, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247370795F746573745F636F6E646974696F6E__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$357, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24247370795F746573745F636F6E646974696F6E2F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$366, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473686F775F7370795F706F696E7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+200, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$378, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F6C6973745F6F665F70726564__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$406, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F6C6973745F6F665F7072656431__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$418, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F6C6973745F6F665F70726564312F345F2461757832__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$418, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F6C6973745F6F665F70726564312F345F2461757833__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$418, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F6C6973745F6F665F70726564312F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+220, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$456, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F63616C6C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+24, %ecx
	movl	$.Lpred41_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+28, %ecx
	movl	$.Lpred41_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+32, %ecx
	movl	$.Lpred41_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+36, %ecx
	movl	$.Lpred41_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+48, %ecx
	movl	$.Lpred41_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$6, %edx
	movl	at+160, %ecx
	movl	$.Lpred41_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$4, %edx
	movl	at+52, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred41_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+20, %eax
	movl	$4, %edx
	movl	at+152, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred41_16+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+20, %eax
	movl	$4, %edx
	movl	at+136, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred41_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+20, %eax
	movl	$4, %edx
	movl	at+156, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred41_20+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+224, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$488, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F63616C6C2F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+228, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$511, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F63616C6C31__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+232, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$511, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F63616C6C312F385F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+236, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$542, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F63616C6C5F706F7274__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+240, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$542, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F63616C6C5F706F72742F355F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+244, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$558, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F656E645F63616C6C__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+248, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$569, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F657863657074696F6E5F706F7274__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$579, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F7274__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+256, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$584, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F727431__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+260, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$595, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F727432__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+264, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$605, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F72745F69676E6F7265__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$3, %edx
	movl	at+28, %ecx
	movl	$.Lpred52_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$3, %edx
	movl	at+36, %ecx
	movl	$.Lpred52_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$3, %edx
	movl	at+268, %ecx
	movl	$.Lpred52_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+28
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+112, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$2, %edx
	movl	at+272, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred52_15+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+276, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$626, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F706F72745F69676E6F72652F345F2461757833__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+280, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$617, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F706F72745F69676E6F72652F345F2461757832__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+284, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$610, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F706F72745F69676E6F72652F345F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+288, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$636, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F72745F70726F6D7074__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+292, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$636, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F706F72745F70726F6D70742F365F2461757831__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+296, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$650, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F726561645F636D64__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+300, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$662, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F726561645F696E7465676572__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+304, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$670, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F657865635F636D64__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$27, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+32
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+308, %ecx
	movl	$.Lpred60_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+312, %ecx
	movl	$.Lpred60_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+316, %ecx
	movl	$.Lpred60_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+320, %ecx
	movl	$.Lpred60_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+324, %ecx
	movl	$.Lpred60_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+328, %ecx
	movl	$.Lpred60_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+332, %ecx
	movl	$.Lpred60_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+336, %ecx
	movl	$.Lpred60_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+340, %ecx
	movl	$.Lpred60_29+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+344, %ecx
	movl	$.Lpred60_31+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+348, %ecx
	movl	$.Lpred60_33+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+352, %ecx
	movl	$.Lpred60_35+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+356, %ecx
	movl	$.Lpred60_37+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+360, %ecx
	movl	$.Lpred60_39+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+364, %ecx
	movl	$.Lpred60_41+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+368, %ecx
	movl	$.Lpred60_43+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+372, %ecx
	movl	$.Lpred60_45+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+376, %ecx
	movl	$.Lpred60_47+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+380, %ecx
	movl	$.Lpred60_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+384, %ecx
	movl	$.Lpred60_51+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+388, %ecx
	movl	$.Lpred60_53+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+392, %ecx
	movl	$.Lpred60_55+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+396, %ecx
	movl	$.Lpred60_57+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+400, %ecx
	movl	$.Lpred60_59+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+404, %ecx
	movl	$.Lpred60_61+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+408, %ecx
	movl	$.Lpred60_63+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$27, %edx
	movl	at+412, %ecx
	movl	$.Lpred60_65+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+416, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$822, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757836__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+420, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$798, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757835__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+424, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$798, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757837__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+428, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$769, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757834__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+432, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$752, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757833__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+436, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$735, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+440, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$682, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F657865635F636D642F365F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+36
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+104, %ecx
	movl	$.Lpred67_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+112, %ecx
	movl	$.Lpred67_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+116, %ecx
	movl	$.Lpred67_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+444, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$861, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F77726974655F676F616C__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+448, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$861, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F77726974655F676F616C2F345F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+452, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$875, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F706F72745F707265747479__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+40
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+100, %ecx
	movl	$.Lpred70_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+108, %ecx
	movl	$.Lpred70_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+112, %ecx
	movl	$.Lpred70_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+104, %ecx
	movl	$.Lpred70_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+116, %ecx
	movl	$.Lpred70_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+40, %eax
	movl	$6, %edx
	movl	at+456, %ecx
	movl	$.Lpred70_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+460, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$890, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616E635F6C7374__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+464, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$896, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616E635F6C737431__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+468, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$905, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616C7465726E617469766573__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+472, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$912, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616C7465726E61746976657331__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+476, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$920, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616C74__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+480, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$930, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616C7431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+44
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+484, %ecx
	movl	$.Lpred76_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+488, %ecx
	movl	$.Lpred76_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+492, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$945, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F646973705F616C74312F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+496, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$945, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242464656275675F646973705F616C74312F335F2461757832__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+500, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$963, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F616C7432__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+504, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$969, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F69735F64656275675F707265646963617465__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+508, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$975, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F636865636B5F626970__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+512, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$986, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2464656275675F646973705F68656C70__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+516, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$1007, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463686F6963655F706F696E745F696E666F__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+520, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$1011, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463686F6963655F706F696E745F617267__a3+0, 8(%esp)
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
.LC133:
	.string	"                    "
.LC134:
	.string	"   +  spy this     *  spy conditionally~n"
.LC135:
	.string	"   -  nospy this   L  listing~n"
.LC136:
	.string	"   @  command      <  set printdepth~n"
.LC137:
	.string	"   a  abort        b  break~n"
.LC138:
	.string	"   g  ancestors    A  alternatives~n"
.LC139:
	.string	"   n  nodebug      =  debugging~n"
.LC140:
	.string	"   p  print        e  exception~n"
.LC141:
	.string	"   r  retry        f  fail~n"
.LC142:
	.string	"   s  skip         G  goto~n"
.LC143:
	.string	"   u  unify        .  father file~n"
.LC144:
	.string	"   w  write        d  display~n"
.LC145:
	.string	"   ~a/~d~n"
.LC146:
	.string	"  h/? help         W  WAM debugger~n"
.LC132:
	.string	" "
.LC147:
	.string	" ? "
.LC131:
	.string	""
.LC148:
	.string	"$"
.LC0:
	.string	"$$debug_call/2_$aux1"
.LC1:
	.string	"$$debug_call1/8_$aux1"
.LC2:
	.string	"$$debug_call_port/5_$aux1"
.LC3:
	.string	"$$debug_disp_alt1/3_$aux1"
.LC4:
	.string	"$$debug_disp_alt1/3_$aux2"
.LC5:
	.string	"$$debug_exec_cmd/6_$aux1"
.LC6:
	.string	"$$debug_exec_cmd/6_$aux2"
.LC7:
	.string	"$$debug_exec_cmd/6_$aux3"
.LC8:
	.string	"$$debug_exec_cmd/6_$aux4"
.LC9:
	.string	"$$debug_exec_cmd/6_$aux5"
.LC10:
	.string	"$$debug_exec_cmd/6_$aux6"
.LC11:
	.string	"$$debug_exec_cmd/6_$aux7"
.LC12:
	.string	"$$debug_list_of_pred1/4_$aux1"
.LC13:
	.string	"$$debug_list_of_pred1/4_$aux2"
.LC14:
	.string	"$$debug_list_of_pred1/4_$aux3"
.LC15:
	.string	"$$debug_port_ignore/4_$aux1"
.LC16:
	.string	"$$debug_port_ignore/4_$aux2"
.LC17:
	.string	"$$debug_port_ignore/4_$aux3"
.LC18:
	.string	"$$debug_port_prompt/6_$aux1"
.LC19:
	.string	"$$debug_spy_reset/1_$aux1"
.LC20:
	.string	"$$debug_spy_set/2_$aux1"
.LC21:
	.string	"$$debug_write_goal/4_$aux1"
.LC22:
	.string	"$$spy_test_condition/3_$aux1"
.LC23:
	.string	"$$spypoint_condition1/3_$aux1"
.LC24:
	.string	"$catch_internal1"
.LC25:
	.string	"$choice_point_arg"
.LC26:
	.string	"$choice_point_info"
.LC149:
	.string	"$debug_"
.LC150:
	.string	"$debug_ball"
.LC27:
	.string	"$debug_call"
.LC28:
	.string	"$debug_call1"
.LC29:
	.string	"$debug_call_port"
.LC30:
	.string	"$debug_check_bip"
.LC151:
	.string	"$debug_depth"
.LC31:
	.string	"$debug_disp_alt"
.LC32:
	.string	"$debug_disp_alt1"
.LC33:
	.string	"$debug_disp_alt2"
.LC34:
	.string	"$debug_disp_alternatives"
.LC35:
	.string	"$debug_disp_alternatives1"
.LC36:
	.string	"$debug_disp_anc_lst"
.LC37:
	.string	"$debug_disp_anc_lst1"
.LC38:
	.string	"$debug_disp_help"
.LC39:
	.string	"$debug_end_call"
.LC40:
	.string	"$debug_exception_port"
.LC41:
	.string	"$debug_exec_cmd"
.LC152:
	.string	"$debug_info"
.LC42:
	.string	"$debug_is_debug_predicate"
.LC43:
	.string	"$debug_is_not_leashed"
.LC153:
	.string	"$debug_leash"
.LC44:
	.string	"$debug_list_of_pred"
.LC45:
	.string	"$debug_list_of_pred1"
.LC154:
	.string	"$debug_mode"
.LC155:
	.string	"$debug_next"
.LC46:
	.string	"$debug_port"
.LC47:
	.string	"$debug_port1"
.LC48:
	.string	"$debug_port2"
.LC49:
	.string	"$debug_port_ignore"
.LC50:
	.string	"$debug_port_mask"
.LC51:
	.string	"$debug_port_pretty"
.LC52:
	.string	"$debug_port_prompt"
.LC53:
	.string	"$debug_read_cmd"
.LC54:
	.string	"$debug_read_integer"
.LC156:
	.string	"$debug_skip"
.LC220:
	.string	"$debug_spy_point"
.LC55:
	.string	"$debug_spy_reset"
.LC56:
	.string	"$debug_spy_set"
.LC57:
	.string	"$debug_switch_off"
.LC58:
	.string	"$debug_switch_on"
.LC157:
	.string	"$debug_unify"
.LC158:
	.string	"$debug_work"
.LC59:
	.string	"$debug_write_goal"
.LC60:
	.string	"$has_no_spy_point"
.LC61:
	.string	"$has_spy_point"
.LC62:
	.string	"$init_debugger"
.LC63:
	.string	"$leash_make_mask"
.LC64:
	.string	"$show_debugger_mode"
.LC65:
	.string	"$show_debugger_mode1"
.LC66:
	.string	"$show_leashing_info"
.LC67:
	.string	"$show_leashing_info1"
.LC68:
	.string	"$show_leashing_info2"
.LC69:
	.string	"$show_spy_points"
.LC70:
	.string	"$show_undefined_action"
.LC71:
	.string	"$show_undefined_action1"
.LC72:
	.string	"$spy_test_condition"
.LC73:
	.string	"$spypoint_condition1"
.LC74:
	.string	"$trail_handler"
.LC75:
	.string	"*"
.LC76:
	.string	"+"
.LC221:
	.string	","
.LC77:
	.string	"-"
.LC78:
	.string	"."
.LC222:
	.string	"/"
.LC79:
	.string	"/home/diaz/GP/src/BipsPl/debugger.pl"
.LC80:
	.string	"<"
.LC81:
	.string	"="
.LC82:
	.string	"?"
.LC83:
	.string	"@"
.LC84:
	.string	"A"
.LC159:
	.string	"All spypoints removed"
.LC160:
	.string	"Alternatives:"
.LC161:
	.string	"Ancestors:"
.LC162:
	.string	"Call: "
.LC163:
	.string	"Command: "
.LC164:
	.string	"Debugging commands:~n~n"
.LC165:
	.string	"Exception raised: ~q~n"
.LC166:
	.string	"Exception: "
.LC167:
	.string	"Exit: "
.LC168:
	.string	"Fail: "
.LC85:
	.string	"G"
.LC169:
	.string	"Goal,Port,Test: "
.LC170:
	.string	"Head: "
.LC171:
	.string	"Inovcation nb: "
.LC86:
	.string	"L"
.LC172:
	.string	"No leashing"
.LC173:
	.string	"Option not applicable at this port"
.LC174:
	.string	"Print Depth: "
.LC175:
	.string	"RET/c creep        l  leap ~n"
.LC176:
	.string	"Redo: "
.LC177:
	.string	"Spypoint placed on"
.LC178:
	.string	"Spypoint removed from"
.LC179:
	.string	"Spypoints:"
.LC180:
	.string	"The debugger is switched off"
.LC181:
	.string	"The debugger will first creep -- "
.LC182:
	.string	"The debugger will first leap -- "
.LC183:
	.string	"There is already a spypoint on"
.LC184:
	.string	"There is no spypoint on"
.LC185:
	.string	"Undefined predicates will "
.LC186:
	.string	"Unknown command (type h for help)"
.LC187:
	.string	"Using leashing stopping at "
.LC87:
	.string	"W"
.LC188:
	.string	"Warning: The predicate ~a/~d is undefined~n"
.LC189:
	.string	"Warning: spy ~a/~w - no matching predicate~n"
.LC190:
	.string	"Warning: ~w - exception raised ~w~n"
.LC191:
	.string	"Warning: ~w - goal failed~n"
.LC192:
	.string	"] ports"
.LC88:
	.string	"a"
.LC193:
	.string	"any"
.LC89:
	.string	"b"
.LC194:
	.string	"built_in"
.LC90:
	.string	"c"
.LC91:
	.string	"call"
.LC195:
	.string	"callable"
.LC196:
	.string	"cannot find any info on ~a/~d~n"
.LC92:
	.string	"d"
.LC93:
	.string	"debug"
.LC197:
	.string	"debugger_exec_cmd"
.LC198:
	.string	"debugger_input"
.LC199:
	.string	"debugger_output"
.LC94:
	.string	"debugging"
.LC200:
	.string	"display a warning message and fail"
.LC95:
	.string	"e"
.LC96:
	.string	"error"
.LC97:
	.string	"exception"
.LC98:
	.string	"exit"
.LC99:
	.string	"f"
.LC100:
	.string	"fail"
.LC201:
	.string	"false"
.LC223:
	.string	"format"
.LC101:
	.string	"full"
.LC102:
	.string	"g"
.LC103:
	.string	"h"
.LC104:
	.string	"half"
.LC105:
	.string	"l"
.LC106:
	.string	"leash"
.LC202:
	.string	"leash_ports"
.LC107:
	.string	"loose"
.LC203:
	.string	"max_arity"
.LC224:
	.string	"max_depth"
.LC108:
	.string	"n"
.LC204:
	.string	"native code predicate ~a/~d~n"
.LC205:
	.string	"native_code"
.LC206:
	.string	"no file information for ~a/~d~n"
.LC109:
	.string	"no_port"
.LC110:
	.string	"nodebug"
.LC111:
	.string	"none"
.LC112:
	.string	"nospy"
.LC113:
	.string	"nospyall"
.LC114:
	.string	"notrace"
.LC115:
	.string	"p"
.LC225:
	.string	"quoted"
.LC116:
	.string	"r"
.LC207:
	.string	"raise an existence_error"
.LC117:
	.string	"redo"
.LC118:
	.string	"retry"
.LC119:
	.string	"s"
.LC208:
	.string	"showing everything (trace)"
.LC209:
	.string	"showing spypoints (debug)"
.LC120:
	.string	"skip"
.LC121:
	.string	"spy"
.LC210:
	.string	"spy_conditional"
.LC122:
	.string	"spypoint_condition"
.LC226:
	.string	"system predicate"
.LC123:
	.string	"system"
.LC124:
	.string	"tight"
.LC125:
	.string	"trace"
.LC211:
	.string	"true"
.LC126:
	.string	"u"
.LC212:
	.string	"unknown"
.LC127:
	.string	"user"
.LC128:
	.string	"w"
.LC129:
	.string	"wam_debug"
.LC130:
	.string	"warning"
.LC214:
	.string	"~N   1 choice-point for ~w~n"
.LC215:
	.string	"~N ~a %4d %4d  ~a"
.LC213:
	.string	"~N"
.LC216:
	.string	"~N--------------~n"
.LC217:
	.string	"~a ~a/~d~n"
.LC218:
	.string	"~a/~d defined in ~a:~d~n"
.LC219:
	.string	"~c~a"
	.data	
	.align	4
	.local	at
	.comm	at,524,4
	.local	fn
	.comm	fn,80,4
	.local	st
	.comm	st,48,4
	.local	ta
	.comm	ta,556,4
	.section	.note.GNU-stack,"",@progbits
