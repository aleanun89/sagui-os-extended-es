# BipsPl/stat.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/stat.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F73746174__a0,@function
.globl X1_247573655F73746174__a0

X1_247573655F73746174__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_statistics__a0,@function
.globl X0_statistics__a0

X0_statistics__a0:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Statistics_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_statistics__a2,@function
.globl X0_statistics__a2

X0_statistics__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F737461745F6B6579__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_24737461746973746963732F325F2461757831__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2473746174__a3
	.p2align	4,,15
	.type	X1_24737461746973746963732F325F2461757831__a3,@function

X1_24737461746973746963732F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24636865636B5F737461745F6B6579__a1,@function
.globl X1_24636865636B5F737461745F6B6579__a1

X1_24636865636B5F737461745F6B6579__a1:
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	movl	$.Lpred5_23+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred5_3+0, %eax
	movl	$.Lpred5_2+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred5_2:
	movl	st+0, %eax
	movl	$10, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred5_3:
	movl	$.Lpred5_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred5_4:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_5:
	movl	$.Lpred5_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_6:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_7:
	movl	$.Lpred5_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_8:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_9:
	movl	$.Lpred5_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_10:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_11:
	movl	$.Lpred5_13+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_12:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_13:
	movl	$.Lpred5_15+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_14:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_15:
	movl	$.Lpred5_17+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_16:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_17:
	movl	$.Lpred5_19+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_18:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_19:
	movl	$.Lpred5_21+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred5_20:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_21:
	call	Pl_Delete_Choice_Point1

.Lpred5_22:
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_23:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+44, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_2473746174__a3,@function
.globl X1_2473746174__a3

X1_2473746174__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2424737461742F335F2461757831__a2
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
	call	Pl_Statistics_User_Time_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point4
	movl	$.Lpred6_3+0, %eax
	movl	$.Lpred6_2+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred6_2:
	movl	st+4, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred6_3:
	movl	$.Lpred6_5+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred6_4:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_System_Time_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_5:
	movl	$.Lpred6_7+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_6:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Cpu_Time_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_7:
	movl	$.Lpred6_9+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_8:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Real_Time_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_9:
	movl	$.Lpred6_11+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_10:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Local_Stack_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_11:
	movl	$.Lpred6_13+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_12:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Global_Stack_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_13:
	movl	$.Lpred6_15+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_14:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Trail_Stack_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_15:
	movl	$.Lpred6_17+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred6_16:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Cstr_Stack_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_17:
	call	Pl_Delete_Choice_Point4

.Lpred6_18:
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Statistics_Atoms_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424737461742F335F2461757831__a2,@function

X1_2424737461742F335F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_2424737461742F335F2461757832__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred7_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424737461742F335F2461757832__a1,@function

X1_2424737461742F335F2461757832__a1:
	movl	$.Lpred8_2+0, %eax
	movl	$.Lpred8_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred8_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred8_2:
	movl	$.Lpred8_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred8_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_4:
	call	Pl_Delete_Choice_Point1

.Lpred8_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_user_time__a1,@function
.globl X0_user_time__a1

X0_user_time__a1:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_User_Time_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_system_time__a1,@function
.globl X0_system_time__a1

X0_system_time__a1:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_System_Time_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_cpu_time__a1,@function
.globl X0_cpu_time__a1

X0_cpu_time__a1:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Cpu_Time_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_real_time__a1,@function
.globl X0_real_time__a1

X0_real_time__a1:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Real_Time_1
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
	movl	%eax, at+72
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	at+64, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+40
	movl	at+40, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+60, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+36
	movl	at+52, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	at+48, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+44, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	$.LC20, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+44
	movl	$.LC21, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+36, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	at+56, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	at+28, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F73746174__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_statistics__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_statistics__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24737461746973746963732F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$61, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F737461745F6B6579__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$10, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+28, %ecx
	movl	$.Lpred5_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+32, %ecx
	movl	$.Lpred5_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+36, %ecx
	movl	$.Lpred5_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+40, %ecx
	movl	$.Lpred5_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+44, %ecx
	movl	$.Lpred5_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+48, %ecx
	movl	$.Lpred5_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+52, %ecx
	movl	$.Lpred5_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+56, %ecx
	movl	$.Lpred5_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+60, %ecx
	movl	$.Lpred5_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+64, %ecx
	movl	$.Lpred5_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+68, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473746174__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+36, %ecx
	movl	$.Lpred6_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+40, %ecx
	movl	$.Lpred6_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+44, %ecx
	movl	$.Lpred6_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+48, %ecx
	movl	$.Lpred6_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+52, %ecx
	movl	$.Lpred6_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+56, %ecx
	movl	$.Lpred6_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+60, %ecx
	movl	$.Lpred6_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$8, %edx
	movl	at+64, %ecx
	movl	$.Lpred6_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+72, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424737461742F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424737461742F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+28, %ecx
	movl	$.Lpred8_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+32, %ecx
	movl	$.Lpred8_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$125, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_user_time__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_system_time__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$139, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_cpu_time__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_real_time__a1+0, 8(%esp)
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
	.string	"$$stat/3_$aux1"
.LC1:
	.string	"$$stat/3_$aux2"
.LC2:
	.string	"$check_stat_key"
.LC3:
	.string	"$stat"
.LC4:
	.string	"$statistics/2_$aux1"
.LC5:
	.string	"$use_stat"
.LC6:
	.string	"/home/diaz/GP/src/BipsPl/stat.pl"
.LC7:
	.string	"atoms"
.LC8:
	.string	"cpu_time"
.LC9:
	.string	"cstr_stack"
.LC10:
	.string	"global_stack"
.LC11:
	.string	"local_stack"
.LC12:
	.string	"real_time"
.LC13:
	.string	"runtime"
.LC14:
	.string	"statistics"
.LC20:
	.string	"statistics_key"
.LC21:
	.string	"statistics_value"
.LC15:
	.string	"system"
.LC16:
	.string	"system_time"
.LC17:
	.string	"trail_stack"
.LC18:
	.string	"user"
.LC19:
	.string	"user_time"
	.data	
	.align	4
	.local	at
	.comm	at,80,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,48,4
	.section	.note.GNU-stack,"",@progbits
