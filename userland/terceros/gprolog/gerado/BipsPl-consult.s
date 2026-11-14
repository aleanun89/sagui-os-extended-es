# BipsPl/consult.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/consult.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F636F6E73756C74__a0,@function
.globl X1_247573655F636F6E73756C74__a0

X1_247573655F636F6E73756C74__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2E__a2,@function
.globl X1_2E__a2

X1_2E__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X0_consult__a1
	.p2align	4,,15
	.type	X0_consult__a1,@function
.globl X0_consult__a1

X0_consult__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24636F6E73756C742F315F2461757831__a1
	.p2align	4,,15
	.type	X1_24636F6E73756C742F315F2461757831__a1,@function

X1_24636F6E73756C742F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24636F6E73756C7432__a1

.Lpred4_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_24636F6E73756C7431__a1
	.p2align	4,,15
	.type	X1_24636F6E73756C7431__a1,@function
.globl X1_24636F6E73756C7431__a1

X1_24636F6E73756C7431__a1:
	movl	$.Lpred5_1+0, %eax
	movl	$.Lpred5_2+0, %edx
	movl	$.Lpred5_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred5_1:
	movl	$.Lpred5_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred5_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_3:
	call	Pl_Delete_Choice_Point1

.Lpred5_4:
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
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_24636F6E73756C7432__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24636F6E73756C7431__a1
	.p2align	4,,15
	.type	X1_24636F6E73756C7432__a1,@function
.globl X1_24636F6E73756C7432__a1

X1_24636F6E73756C7432__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Prolog_File_Name_2
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2424636F6E73756C74322F315F2461757831__a2
.Lcont3:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X0_temporary_file__a3
.Lcont4:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2424636F6E73756C74322F315F2461757832__a2
	.p2align	4,,15
	.type	X1_2424636F6E73756C74322F315F2461757832__a2,@function

X1_2424636F6E73756C74322F315F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_24636F6E73756C7433__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246C6F61645F66696C65__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_unlink__a1

.Lpred7_1:
	call	Pl_Delete_Choice_Point3
	movl	$0, %eax
	call	Pl_Allocate
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_unlink__a1
.Lcont7:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont8:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_2424636F6E73756C74322F315F2461757831__a2,@function

X1_2424636F6E73756C74322F315F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Absolute_File_Name_2
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2424636F6E73756C74322F315F2461757833__a2
	.p2align	4,,15
	.type	X1_2424636F6E73756C74322F315F2461757833__a2,@function

X1_2424636F6E73756C74322F315F2461757833__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_file_exists__a1
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	call	Pl_Delete_Choice_Point3
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F6578697374656E6365__a2
	.p2align	4,,15
	.type	X1_24636F6E73756C7433__a2,@function
.globl X1_24636F6E73756C7433__a2

X1_24636F6E73756C7433__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Consult_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246C6F61645F66696C65__a1,@function
.globl X1_246C6F61645F66696C65__a1

X1_246C6F61645F66696C65__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_open__a3
.Lcont10:
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_read__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_24246C6F61645F66696C652F315F2461757831__a3
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_close__a1
	.p2align	4,,15
	.type	X1_24246C6F61645F66696C652F315F2461757831__a3,@function

X1_24246C6F61645F66696C652F315F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point4
	movl	$0, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_246C6F61645F70726564__a2
.Lcont14:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_246C6F61645F70726564__a2,@function
.globl X1_246C6F61645F70726564__a2

X1_246C6F61645F70726564__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred13_2+0, %eax
	movl	$.Lpred13_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred13_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred13_2:
	movl	$.Lpred13_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred13_3:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	jmp	*pl_reg_bank+1040

.Lpred13_4:
	movl	$.Lpred13_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_5:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_24246C6F61645F707265642F325F2461757831__a3

.Lpred13_6:
	call	Pl_Delete_Choice_Point3

.Lpred13_7:
	movl	$12, %eax
	call	Pl_Allocate
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
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
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -44(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -48(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -52(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -56(%edi)
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
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_24636865636B5F707265645F74797065__a4
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_24246C6F61645F707265642F325F2461757832__a6
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-52(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-56(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+28
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_2462635F73746172745F70726564__a8
.Lcont17:
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont18:
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
	movl	at+76, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Inc
	movl	%eax, pl_reg_bank+8
	movl	ta+36, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-60(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_24246C6F61645F707265642F325F2461757833__a4
.Lcont19:
	movl	pl_reg_bank+1044, %edi
	movl	-48(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24246C6F61645F707265642F325F2461757833__a4,@function

X1_24246C6F61645F707265642F325F2461757833__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred14_1+0, %eax
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

.Lpred14_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X0_read__a2
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_246164645F636C617573655F7465726D5F616E645F6263__a3
.Lcont21:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_24246C6F61645F707265642F325F2461757832__a6,@function

X1_24246C6F61645F707265642F325F2461757832__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred15_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+40, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F706572747931__a3
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred15_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_24636865636B5F6F776E65725F66696C6573__a3
	.p2align	4,,15
	.type	X1_24246C6F61645F707265642F325F2461757831__a3,@function

X1_24246C6F61645F707265642F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred16_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
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
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+52, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_246C6F61645F6469726563746976655F657863657074696F6E__a3,@function
.globl X1_246C6F61645F6469726563746976655F657863657074696F6E__a3

X1_246C6F61645F6469726563746976655F657863657074696F6E__a3:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_G_Read
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
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
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
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+56, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_24636865636B5F707265645F74797065__a4,@function
.globl X1_24636865636B5F707265645F74797065__a4

X1_24636865636B5F707265645F74797065__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred18_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
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
	movl	pl_reg_bank+16, %eax
	movl	%eax, -32(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F706572747931__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_2424636865636B5F707265645F747970652F345F2461757831__a4
.Lcont25:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred18_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424636865636B5F707265645F747970652F345F2461757831__a4,@function

X1_2424636865636B5F707265645F747970652F345F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred19_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_246175785F6E616D65__a1
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred19_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+20
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_format__a3
	.p2align	4,,15
	.type	X1_24636865636B5F6F776E65725F66696C6573__a3,@function
.globl X1_24636865636B5F6F776E65725F66696C6573__a3

X1_24636865636B5F6F776E65725F66696C6573__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred20_1+0, %eax
	call	Pl_Create_Choice_Point4
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_246765745F7072656469636174655F66696C655F696E666F__a3
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_2424636865636B5F6F776E65725F66696C65732F335F2461757831__a6

.Lpred20_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424636865636B5F6F776E65725F66696C65732F335F2461757831__a6,@function

X1_2424636865636B5F6F776E65725F66696C65732F335F2461757831__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred21_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_246175785F6E616D65__a1
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred21_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -20(%edi)
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
	call	Pl_Unify_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X0_format__a3
.Lcont29:
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+72, %eax
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
	.p2align	4,,15
	.type	X0_load__a1,@function
.globl X0_load__a1

X0_load__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246C6F61642F315F2461757831__a1
	.p2align	4,,15
	.type	X1_246C6F61642F315F2461757831__a1,@function

X1_246C6F61642F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_246C6F616432__a1

.Lpred23_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_246C6F616431__a1
	.p2align	4,,15
	.type	X1_246C6F616431__a1,@function
.globl X1_246C6F616431__a1

X1_246C6F616431__a1:
	movl	$.Lpred24_1+0, %eax
	movl	$.Lpred24_2+0, %edx
	movl	$.Lpred24_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred24_1:
	movl	$.Lpred24_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred24_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred24_3:
	call	Pl_Delete_Choice_Point1

.Lpred24_4:
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
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_246C6F616432__a1
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246C6F616431__a1
	.p2align	4,,15
	.type	X1_246C6F616432__a1,@function
.globl X1_246C6F616432__a1

X1_246C6F616432__a1:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_decompose_file_name__a4
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_24246C6F6164322F315F2461757831__a3
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Absolute_File_Name_2
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X1_24246C6F6164322F315F2461757832__a2
.Lcont34:
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246C6F61645F66696C65__a1
	.p2align	4,,15
	.type	X1_24246C6F6164322F315F2461757832__a2,@function

X1_24246C6F6164322F315F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred26_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X0_file_exists__a1
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred26_1:
	call	Pl_Delete_Choice_Point3
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F6578697374656E6365__a2
	.p2align	4,,15
	.type	X1_24246C6F6164322F315F2461757831__a3,@function

X1_24246C6F6164322F315F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred27_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_atom_concat__a3

.Lpred27_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2462635F73746172745F70726564__a8,@function
.globl X1_2462635F73746172745F70726564__a8

X1_2462635F73746172745F70726564__a8:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 16(%esp)
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	movl	pl_reg_bank+24, %eax
	movl	%eax, 24(%esp)
	movl	pl_reg_bank+28, %eax
	movl	%eax, 28(%esp)
	call	Pl_BC_Start_Pred_8
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2462635F73746172745F656D6974__a0,@function
.globl X1_2462635F73746172745F656D6974__a0

X1_2462635F73746172745F656D6974__a0:
	call	Pl_BC_Start_Emit_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2462635F73746F705F656D6974__a0,@function
.globl X1_2462635F73746F705F656D6974__a0

X1_2462635F73746F705F656D6974__a0:
	call	Pl_BC_Stop_Emit_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2462635F656D6974__a1,@function
.globl X1_2462635F656D6974__a1

X1_2462635F656D6974__a1:
	movl	$.Lpred31_1+0, %eax
	movl	$.Lpred31_2+0, %edx
	movl	$.Lpred31_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred31_1:
	movl	$.Lpred31_3+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred31_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_3:
	call	Pl_Delete_Choice_Point1

.Lpred31_4:
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
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X1_2462635F656D69745F696E7374__a1
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2462635F656D6974__a1
	.p2align	4,,15
	.type	X1_2462635F656D69745F696E7374__a1,@function
.globl X1_2462635F656D69745F696E7374__a1

X1_2462635F656D69745F696E7374__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_BC_Emit_Inst_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2462635F656D756C6174655F636F6E74__a0,@function
.globl X1_2462635F656D756C6174655F636F6E74__a0

X1_2462635F656D756C6174655F636F6E74__a0:
	call	Pl_BC_Emulate_Cont_0
	jmp	*%eax
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246164645F636C617573655F7465726D__a2,@function
.globl X1_246164645F636C617573655F7465726D__a2

X1_246164645F636C617573655F7465726D__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_24617373657274__a4
	.p2align	4,,15
	.type	X1_246164645F636C617573655F7465726D5F616E645F6263__a3,@function
.globl X1_246164645F636C617573655F7465726D5F616E645F6263__a3

X1_246164645F636C617573655F7465726D5F616E645F6263__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_2462635F73746172745F656D6974__a0
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_2462635F656D6974__a1
.Lcont38:
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_2462635F73746F705F656D6974__a0
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246164645F636C617573655F7465726D__a2
	.p2align	4,,15
	.type	X0_listing__a0,@function
.globl X0_listing__a0

X0_listing__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+168, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont40:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246C697374696E675F616C6C__a1
	.p2align	4,,15
	.type	X0_listing__a1,@function
.globl X0_listing__a1

X0_listing__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+168, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred37_1:
	movl	$.Lpred37_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	X1_246C697374696E675F616C6C__a1

.Lpred37_2:
	call	Pl_Delete_Choice_Point2
	jmp	X1_246C697374696E675F616C6C__a1
	.p2align	4,,15
	.type	X1_246C697374696E675F616E79__a0,@function
.globl X1_246C697374696E675F616E79__a0

X1_246C697374696E675F616E79__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+172, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont42:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246C697374696E675F616C6C__a1
	.p2align	4,,15
	.type	X1_246C697374696E675F616E79__a1,@function
.globl X1_246C697374696E675F616E79__a1

X1_246C697374696E675F616E79__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+172, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred39_1:
	movl	$.Lpred39_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	X1_246C697374696E675F616C6C__a1

.Lpred39_2:
	call	Pl_Delete_Choice_Point2
	jmp	X1_246C697374696E675F616C6C__a1
	.p2align	4,,15
	.type	X1_246C697374696E675F616C6C__a1,@function
.globl X1_246C697374696E675F616C6C__a1

X1_246C697374696E675F616C6C__a1:
	movl	$.Lpred40_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_setof__a3
.Lcont44:
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X0_format__a2
.Lcont45:
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X0_member__a2
.Lcont46:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X1_246C697374696E675F6F6E65__a1
.Lcont47:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred40_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246C697374696E675F6F6E655F7069__a3,@function
.globl X1_246C697374696E675F6F6E655F7069__a3

X1_246C697374696E675F6F6E655F7069__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X1_24246C697374696E675F6F6E655F70692F335F2461757831__a1
.Lcont48:
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+60, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X1_5C5C2B__a1
.Lcont49:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F70657274795F70695F616E79__a2
.Lcont50:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	X1_247072656469636174655F70726F70657274795F70695F616E79__a2
	.p2align	4,,15
	.type	X1_24246C697374696E675F6F6E655F70692F335F2461757831__a1,@function

X1_24246C697374696E675F6F6E655F70692F335F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred42_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2463757272656E745F707265646963617465__a1

.Lpred42_1:
	call	Pl_Delete_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F616E79__a1
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Deallocate
	jmp	X1_246E6F745F6175785F6E616D65__a1
	.p2align	4,,15
	.type	X1_246C697374696E675F6F6E65__a1,@function
.globl X1_246C697374696E675F6F6E65__a1

X1_246C697374696E675F6F6E65__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred43_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+60, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X1_247072656469636174655F70726F70657274795F70695F616E79__a2
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred43_1:
	movl	$.Lpred43_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X1_246765745F707265645F696E646963__a3
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X0_nl__a0
.Lcont55:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X1_24636C61757365__a3
.Lcont56:
	movl	fn+44, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-28(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X0_portray_clause__a1
.Lcont57:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred43_2:
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
	movl	%eax, at+108
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC48, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC49, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	$.LC50, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+36
	movl	$.LC51, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC52, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	$.LC53, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC54, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC55, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	$.LC56, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	$.LC57, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+40
	movl	$.LC58, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+60
	movl	$.LC59, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC60, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC61, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC62, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+48
	movl	at+24, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	$.LC63, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC64, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+56
	movl	$.LC65, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+52
	movl	$.LC66, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC29, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC33, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC67, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC68, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC69, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC70, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC71, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC40, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC41, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC45, %eax
	movl	$7, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC72, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC73, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F636F6E73756C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_consult__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24636F6E73756C742F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636F6E73756C7431__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636F6E73756C7432__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636F6E73756C74322F315F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636F6E73756C74322F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636F6E73756C74322F315F2461757833__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636F6E73756C7433__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$108, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C6F61645F66696C65__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$108, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F61645F66696C652F315F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$122, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C6F61645F70726564__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+68, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred13_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+72, %ecx
	movl	$7, 0(%esp)
	movl	$.Lpred13_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+80, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$133, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F61645F707265642F325F2461757833__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$133, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F61645F707265642F325F2461757832__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$125, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F61645F707265642F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$159, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C6F61645F6469726563746976655F657863657074696F6E__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$166, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F707265645F74797065__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$166, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636865636B5F707265645F747970652F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$179, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F6F776E65725F66696C6573__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$179, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636865636B5F6F776E65725F66696C65732F335F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$194, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_load__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$194, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_246C6F61642F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$204, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C6F616431__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$211, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C6F616432__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$211, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F6164322F315F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$211, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C6F6164322F315F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$229, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F73746172745F70726564__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$233, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F73746172745F656D6974__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F73746F705F656D6974__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$239, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F656D6974__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$245, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F656D69745F696E7374__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$251, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2462635F656D756C6174655F636F6E74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$257, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246164645F636C617573655F7465726D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$263, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246164645F636C617573655F7465726D5F616E645F6263__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$274, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_listing__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$281, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_listing__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$298, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C697374696E675F616E79__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$305, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C697374696E675F616E79__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$325, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C697374696E675F616C6C__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C697374696E675F6F6E655F7069__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246C697374696E675F6F6E655F70692F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$361, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C697374696E675F6F6E65__a1+0, 8(%esp)
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
	.string	"         ~a:~d: previous definition~n"
.LC48:
	.string	""
.LC0:
	.string	"$$check_owner_files/3_$aux1"
.LC1:
	.string	"$$check_pred_type/4_$aux1"
.LC2:
	.string	"$$consult2/1_$aux1"
.LC3:
	.string	"$$consult2/1_$aux2"
.LC4:
	.string	"$$consult2/1_$aux3"
.LC5:
	.string	"$$listing_one_pi/3_$aux1"
.LC6:
	.string	"$$load2/1_$aux1"
.LC7:
	.string	"$$load2/1_$aux2"
.LC8:
	.string	"$$load_file/1_$aux1"
.LC9:
	.string	"$$load_pred/2_$aux1"
.LC10:
	.string	"$$load_pred/2_$aux2"
.LC11:
	.string	"$$load_pred/2_$aux3"
.LC12:
	.string	"$add_clause_term"
.LC13:
	.string	"$add_clause_term_and_bc"
.LC14:
	.string	"$bc_emit"
.LC15:
	.string	"$bc_emit_inst"
.LC16:
	.string	"$bc_emulate_cont"
.LC17:
	.string	"$bc_start_emit"
.LC18:
	.string	"$bc_start_pred"
.LC19:
	.string	"$bc_stop_emit"
.LC20:
	.string	"$check_owner_files"
.LC21:
	.string	"$check_pred_type"
.LC22:
	.string	"$consult/1_$aux1"
.LC23:
	.string	"$consult1"
.LC24:
	.string	"$consult2"
.LC25:
	.string	"$consult3"
.LC50:
	.string	"$ctr"
.LC26:
	.string	"$listing_all"
.LC27:
	.string	"$listing_any"
.LC28:
	.string	"$listing_one"
.LC29:
	.string	"$listing_one_pi"
.LC30:
	.string	"$load/1_$aux1"
.LC31:
	.string	"$load1"
.LC32:
	.string	"$load2"
.LC33:
	.string	"$load_directive_exception"
.LC34:
	.string	"$load_file"
.LC35:
	.string	"$load_pred"
.LC51:
	.string	"$pl_file"
.LC67:
	.string	"$predicate_property_pi_any"
.LC36:
	.string	"$use_consult"
.LC68:
	.string	"-"
.LC37:
	.string	"."
.LC52:
	.string	".wbc"
.LC69:
	.string	"/"
.LC38:
	.string	"/home/diaz/GP/src/BipsPl/consult.pl"
.LC70:
	.string	":-"
.LC71:
	.string	"clause"
.LC53:
	.string	"compilation failed~n"
.LC39:
	.string	"consult"
.LC40:
	.string	"directive"
.LC54:
	.string	"end_of_file"
.LC55:
	.string	"error: ~a:~d: native code procedure ~q cannot be redefined (ignored)~n"
.LC41:
	.string	"file_name"
.LC56:
	.string	"gplc"
.LC42:
	.string	"is"
.LC43:
	.string	"listing"
.LC44:
	.string	"load"
.LC57:
	.string	"multifile"
.LC58:
	.string	"native_code"
.LC45:
	.string	"predicate"
.LC72:
	.string	"prolog_file"
.LC73:
	.string	"prolog_line"
.LC59:
	.string	"read"
.LC60:
	.string	"source_sink"
.LC46:
	.string	"system"
.LC61:
	.string	"top_level_output"
.LC62:
	.string	"true"
.LC47:
	.string	"user"
.LC63:
	.string	"warning: ~a:~d: redefining procedure ~q~n"
.LC64:
	.string	"~Nwarning: ~a:~d: ~a directive caused exception: ~q~n"
.LC65:
	.string	"~Nwarning: ~a:~d: ~a directive failed~n"
.LC66:
	.string	"~n%% file: ~w~n"
	.data	
	.align	4
	.local	at
	.comm	at,192,4
	.local	fn
	.comm	fn,48,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,84,4
	.section	.note.GNU-stack,"",@progbits
