# BipsPl/os_interf.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/os_interf.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6F735F696E74657266__a0,@function
.globl X1_247573655F6F735F696E74657266__a0

X1_247573655F6F735F696E74657266__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_make_directory__a1,@function
.globl X0_make_directory__a1

X0_make_directory__a1:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Make_Directory_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_delete_directory__a1,@function
.globl X0_delete_directory__a1

X0_delete_directory__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Delete_Directory_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_working_directory__a1,@function
.globl X0_working_directory__a1

X0_working_directory__a1:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Working_Directory_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_change_directory__a1,@function
.globl X0_change_directory__a1

X0_change_directory__a1:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Change_Directory_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_directory_files__a2,@function
.globl X0_directory_files__a2

X0_directory_files__a2:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Directory_Files_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_rename_file__a2,@function
.globl X0_rename_file__a2

X0_rename_file__a2:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Rename_File_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_unlink__a1,@function
.globl X0_unlink__a1

X0_unlink__a1:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Unlink_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_delete_file__a1,@function
.globl X0_delete_file__a1

X0_delete_file__a1:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Delete_File_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_file_exists__a1,@function
.globl X0_file_exists__a1

X0_file_exists__a1:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_File_Exists_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_file_permission__a2,@function
.globl X0_file_permission__a2

X0_file_permission__a2:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Permission_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_file_property__a2,@function
.globl X0_file_property__a2

X0_file_property__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F66696C655F70726F70__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2466696C655F70726F70__a2
	.p2align	4,,15
	.type	X1_24636865636B5F66696C655F70726F70__a1,@function
.globl X1_24636865636B5F66696C655F70726F70__a1

X1_24636865636B5F66696C655F70726F70__a1:
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	movl	$.Lpred13_19+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred13_3+0, %eax
	movl	$.Lpred13_2+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred13_2:
	movl	st+0, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred13_3:
	movl	$.Lpred13_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred13_4:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_5:
	movl	$.Lpred13_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_6:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_7:
	movl	$.Lpred13_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_8:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_9:
	movl	$.Lpred13_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_10:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_11:
	movl	$.Lpred13_13+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_12:
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_13:
	movl	$.Lpred13_15+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_14:
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_15:
	movl	$.Lpred13_17+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred13_16:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_17:
	call	Pl_Delete_Choice_Point1

.Lpred13_18:
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred13_19:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_2466696C655F70726F70__a2,@function
.globl X1_2466696C655F70726F70__a2

X1_2466696C655F70726F70__a2:
	movl	$.Lpred14_2+0, %eax
	movl	$.Lpred14_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred14_1:
	movl	st+4, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred14_2:
	movl	$.Lpred14_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred14_3:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Absolute_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_4:
	movl	$.Lpred14_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_5:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Real_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_6:
	movl	$.Lpred14_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_7:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Type_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_8:
	movl	$.Lpred14_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_9:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Size_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_10:
	movl	$.Lpred14_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_11:
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Check_Prop_Perm_And_File_2
	testl	%eax, %eax
	je	fail
	jmp	X1_2466696C655F70726F705F7065726D__a2

.Lpred14_12:
	movl	$.Lpred14_14+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_13:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Date_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_14:
	movl	$.Lpred14_16+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred14_15:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Date_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_16:
	call	Pl_Delete_Choice_Point2

.Lpred14_17:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_File_Prop_Date_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2466696C655F70726F705F7065726D__a2,@function
.globl X1_2466696C655F70726F705F7065726D__a2

X1_2466696C655F70726F705F7065726D__a2:
	movl	$.Lpred15_2+0, %eax
	movl	$.Lpred15_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred15_1:
	movl	st+8, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred15_2:
	movl	$.Lpred15_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred15_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_file_permission__a2

.Lpred15_4:
	movl	$.Lpred15_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred15_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_file_permission__a2

.Lpred15_6:
	movl	$.Lpred15_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred15_7:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_file_permission__a2

.Lpred15_8:
	call	Pl_Delete_Choice_Point2

.Lpred15_9:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X0_file_permission__a2
	.p2align	4,,15
	.type	X0_temporary_name__a2,@function
.globl X0_temporary_name__a2

X0_temporary_name__a2:
	movl	at+116, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Temporary_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_temporary_file__a3,@function
.globl X0_temporary_file__a3

X0_temporary_file__a3:
	movl	at+120, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Temporary_File_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_date_time__a1,@function
.globl X0_date_time__a1

X0_date_time__a1:
	movl	at+124, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Date_Time_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_host_name__a1,@function
.globl X0_host_name__a1

X0_host_name__a1:
	movl	at+128, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Host_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_os_version__a1,@function
.globl X0_os_version__a1

X0_os_version__a1:
	movl	at+132, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Os_Version_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_architecture__a1,@function
.globl X0_architecture__a1

X0_architecture__a1:
	movl	at+136, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Architecture_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_shell__a0,@function
.globl X0_shell__a0

X0_shell__a0:
	movl	at+140, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Shell_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_shell__a1,@function
.globl X0_shell__a1

X0_shell__a1:
	movl	at+140, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Shell_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_shell__a2,@function
.globl X0_shell__a2

X0_shell__a2:
	movl	at+140, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Shell_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_system__a1,@function
.globl X0_system__a1

X0_system__a1:
	movl	at+8, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_System_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_system__a2,@function
.globl X0_system__a2

X0_system__a2:
	movl	at+8, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_System_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_spawn__a2,@function
.globl X0_spawn__a2

X0_spawn__a2:
	movl	at+144, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Spawn_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_spawn__a3,@function
.globl X0_spawn__a3

X0_spawn__a3:
	movl	at+144, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Spawn_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sleep__a1,@function
.globl X0_sleep__a1

X0_sleep__a1:
	movl	at+148, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Sleep_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_popen__a3,@function
.globl X0_popen__a3

X0_popen__a3:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+152, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Popen_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_exec__a5,@function
.globl X0_exec__a5

X0_exec__a5:
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
	movl	at+156, %eax
	movl	%eax, 0(%esp)
	movl	$5, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_24657865632F355F2461757831__a1
.Lcont6:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont7:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont8:
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
	jmp	X1_2465786563__a5
	.p2align	4,,15
	.type	X1_24657865632F355F2461757831__a1,@function

X1_24657865632F355F2461757831__a1:
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
	jmp	X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1

.Lpred32_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_exec__a4,@function
.globl X0_exec__a4

X0_exec__a4:
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
	movl	at+156, %eax
	movl	%eax, 0(%esp)
	movl	$4, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2465786563__a5
	.p2align	4,,15
	.type	X1_2465786563__a5,@function
.globl X1_2465786563__a5

X1_2465786563__a5:
	movl	$7, %eax
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
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
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
	call	Pl_Exec_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_create_pipe__a2,@function
.globl X0_create_pipe__a2

X0_create_pipe__a2:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+172, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Create_Pipe_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_fork_prolog__a1,@function
.globl X0_fork_prolog__a1

X0_fork_prolog__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+176, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_24666F726B5F70726F6C6F672F315F2461757831__a1
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Fork_Prolog_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666F726B5F70726F6C6F672F315F2461757831__a1,@function

X1_24666F726B5F70726F6C6F672F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred37_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1

.Lpred37_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_select__a5,@function
.globl X0_select__a5

X0_select__a5:
	movl	at+184, %eax
	movl	%eax, 0(%esp)
	movl	$5, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
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
	call	Pl_Select_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_prolog_pid__a1,@function
.globl X0_prolog_pid__a1

X0_prolog_pid__a1:
	movl	at+188, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Prolog_Pid_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_send_signal__a2,@function
.globl X0_send_signal__a2

X0_send_signal__a2:
	movl	at+192, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Send_Signal_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_wait__a2,@function
.globl X0_wait__a2

X0_wait__a2:
	movl	at+196, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Wait_2
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
	movl	%eax, at+56
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC50, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	$.LC51, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	$.LC8, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC12, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC24, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC28, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC32, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC38, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC44, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6F735F696E74657266__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_make_directory__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_delete_directory__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_working_directory__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_change_directory__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_directory_files__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$78, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_rename_file__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$85, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_unlink__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$92, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_delete_file__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_file_exists__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$104, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_file_permission__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$111, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_file_property__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$119, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F66696C655F70726F70__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+60, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+68, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+72, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_16+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$8, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred13_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+92, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$144, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2466696C655F70726F70__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+60, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+68, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+72, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_15+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_17+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+96, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$173, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2466696C655F70726F705F7065726D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+100, %ecx
	movl	$.Lpred15_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+104, %ecx
	movl	$.Lpred15_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+108, %ecx
	movl	$.Lpred15_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$4, %edx
	movl	at+112, %ecx
	movl	$.Lpred15_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+116, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$188, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_temporary_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$195, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_temporary_file__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$202, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_date_time__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$209, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_host_name__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$216, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_os_version__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$223, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_architecture__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$230, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_shell__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$234, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_shell__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$238, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_shell__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+8, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$245, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_system__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+8, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$249, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_system__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$256, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_spawn__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$260, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_spawn__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$267, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sleep__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$274, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_popen__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_exec__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24657865632F355F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$298, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_exec__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$306, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2465786563__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$316, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_create_pipe__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$325, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_fork_prolog__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$325, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24666F726B5F70726F6C6F672F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$336, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_select__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$343, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_prolog_pid__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$350, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_send_signal__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$357, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_wait__a2+0, 8(%esp)
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
.LC50:
	.string	""
.LC0:
	.string	"$check_file_prop"
.LC1:
	.string	"$exec"
.LC2:
	.string	"$exec/5_$aux1"
.LC3:
	.string	"$file_prop"
.LC4:
	.string	"$file_prop_perm"
.LC5:
	.string	"$fork_prolog/1_$aux1"
.LC6:
	.string	"$use_os_interf"
.LC7:
	.string	"/home/diaz/GP/src/BipsPl/os_interf.pl"
.LC8:
	.string	"absolute_file_name"
.LC9:
	.string	"architecture"
.LC10:
	.string	"change_directory"
.LC11:
	.string	"create_pipe"
.LC12:
	.string	"creation"
.LC13:
	.string	"date_time"
.LC14:
	.string	"delete_directory"
.LC15:
	.string	"delete_file"
.LC16:
	.string	"directory_files"
.LC17:
	.string	"exec"
.LC18:
	.string	"execute"
.LC19:
	.string	"file_exists"
.LC20:
	.string	"file_permission"
.LC21:
	.string	"file_property"
.LC22:
	.string	"fork_prolog"
.LC23:
	.string	"host_name"
.LC24:
	.string	"last_access"
.LC25:
	.string	"last_modification"
.LC26:
	.string	"make_directory"
.LC51:
	.string	"os_file_property"
.LC27:
	.string	"os_version"
.LC28:
	.string	"permission"
.LC29:
	.string	"popen"
.LC30:
	.string	"prolog_pid"
.LC31:
	.string	"read"
.LC32:
	.string	"real_file_name"
.LC33:
	.string	"rename_file"
.LC34:
	.string	"search"
.LC35:
	.string	"select"
.LC36:
	.string	"send_signal"
.LC37:
	.string	"shell"
.LC38:
	.string	"size"
.LC39:
	.string	"sleep"
.LC40:
	.string	"spawn"
.LC41:
	.string	"system"
.LC42:
	.string	"temporary_file"
.LC43:
	.string	"temporary_name"
.LC44:
	.string	"type"
.LC45:
	.string	"unlink"
.LC46:
	.string	"user"
.LC47:
	.string	"wait"
.LC48:
	.string	"working_directory"
.LC49:
	.string	"write"
	.data	
	.align	4
	.local	at
	.comm	at,200,4
	.local	fn
	.comm	fn,32,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,24,4
	.section	.note.GNU-stack,"",@progbits
