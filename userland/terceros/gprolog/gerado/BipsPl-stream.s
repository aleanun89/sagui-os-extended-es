# BipsPl/stream.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/stream.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F73747265616D__a0,@function
.globl X1_247573655F73747265616D__a0

X1_247573655F73747265616D__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_input__a1,@function
.globl X0_current_input__a1

X0_current_input__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Current_Input_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_output__a1,@function
.globl X0_current_output__a1

X0_current_output__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Current_Output_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636865636B5F73747265616D5F6F725F766172__a2,@function
.globl X1_24636865636B5F73747265616D5F6F725F766172__a2

X1_24636865636B5F73747265616D5F6F725F766172__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2424636865636B5F73747265616D5F6F725F7661722F325F2461757831__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_2424636865636B5F73747265616D5F6F725F7661722F325F2461757831__a1,@function

X1_2424636865636B5F73747265616D5F6F725F7661722F325F2461757831__a1:
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_input__a1,@function
.globl X0_set_input__a1

X0_set_input__a1:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Set_Input_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_output__a1,@function
.globl X0_set_output__a1

X0_set_output__a1:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Set_Output_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F746F705F6C6576656C5F73747265616D73__a2,@function
.globl X1_247365745F746F705F6C6576656C5F73747265616D73__a2

X1_247365745F746F705F6C6576656C5F73747265616D73__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Top_Level_Streams_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F64656275676765725F73747265616D73__a2,@function
.globl X1_247365745F64656275676765725F73747265616D73__a2

X1_247365745F64656275676765725F73747265616D73__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Debugger_Streams_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open__a3,@function
.globl X0_open__a3

X0_open__a3:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+12
	jmp	X1_246F70656E__a4
	.p2align	4,,15
	.type	X0_open__a4,@function
.globl X0_open__a4

X0_open__a4:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$4, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_246F70656E__a4
	.p2align	4,,15
	.type	X1_246F70656E__a4,@function
.globl X1_246F70656E__a4

X1_246F70656E__a4:
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
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247365745F6F70656E5F64656661756C7473__a0
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont5:
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F6F7074696F6E73__a1
.Lcont6:
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
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
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Open_3
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_246164645F616C69617365735F746F5F73747265616D__a2
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246164645F6D6972726F72735F746F5F73747265616D__a2
	.p2align	4,,15
	.type	X1_247365745F6F70656E5F64656661756C7473__a0,@function
.globl X1_247365745F6F70656E5F64656661756C7473__a0

X1_247365745F6F70656E5F64656661756C7473__a0:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X1_246765745F6F70656E5F73746D__a2,@function
.globl X1_246765745F6F70656E5F73746D__a2

X1_246765745F6F70656E5F73746D__a2:
	jmp	X1_24246765745F6F70656E5F73746D2F325F2461757831__a2
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F73746D2F325F2461757831__a2,@function

X1_24246765745F6F70656E5F73746D2F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1

.Lpred15_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F6F70656E5F6F7074696F6E73__a1,@function
.globl X1_246765745F6F70656E5F6F7074696F6E73__a1

X1_246765745F6F70656E5F6F7074696F6E73__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C697374__a1
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F6F70656E5F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F6F70656E5F6F7074696F6E7331__a1,@function
.globl X1_246765745F6F70656E5F6F7074696F6E7331__a1

X1_246765745F6F70656E5F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred17_1+0, %eax
	movl	$.Lpred17_2+0, %edx
	movl	$.Lpred17_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred17_1:
	movl	$.Lpred17_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred17_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred17_3:
	call	Pl_Delete_Choice_Point2

.Lpred17_4:
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
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F6F7074696F6E7332__a1
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F6F70656E5F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F6F70656E5F6F7074696F6E7332__a1,@function
.globl X1_246765745F6F70656E5F6F7074696F6E7332__a1

X1_246765745F6F70656E5F6F7074696F6E7332__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred18_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred18_1:
	movl	$.Lpred18_15+0, %eax
	call	Pl_Update_Choice_Point2
	movl	$.Lpred18_3+0, %eax
	movl	$.Lpred18_2+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred18_2:
	movl	st+0, %eax
	movl	$6, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred18_3:
	movl	$.Lpred18_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred18_4:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757831__a1

.Lpred18_5:
	movl	$.Lpred18_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_6:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757832__a1
.Lcont12:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred18_7:
	movl	$.Lpred18_9+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_8:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757833__a1
.Lcont14:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred18_9:
	movl	$.Lpred18_11+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_10:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757834__a1
.Lcont16:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483613, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred18_11:
	movl	$.Lpred18_13+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred18_12:
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757835__a1

.Lpred18_13:
	call	Pl_Delete_Choice_Point2

.Lpred18_14:
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757836__a1
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Check_Valid_Mirror_1
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+8
	movl	-16(%edi), %eax
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
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred18_15:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757836__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757836__a1:
	movl	$.Lpred19_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred19_1:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757835__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757835__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred20_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Test_Alias_Not_Assigned_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_List
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	jmp	*pl_reg_bank+1040

.Lpred20_1:
	call	Pl_Delete_Choice_Point2
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_24706C5F6572725F7065726D697373696F6E__a3
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757834__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757834__a1:
	movl	$.Lpred21_2+0, %eax
	movl	$.Lpred21_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred21_1:
	movl	st+4, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred21_2:
	movl	$.Lpred21_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred21_3:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont19:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483621, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred21_4:
	movl	$.Lpred21_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred21_5:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont20:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483621, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred21_6:
	call	Pl_Delete_Choice_Point1

.Lpred21_7:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont21:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483621, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757833__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757833__a1:
	movl	$.Lpred22_2+0, %eax
	movl	$.Lpred22_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred22_1:
	movl	st+8, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred22_2:
	movl	$.Lpred22_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred22_3:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont22:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred22_4:
	movl	$.Lpred22_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_5:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont23:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred22_6:
	call	Pl_Delete_Choice_Point1

.Lpred22_7:
	movl	$0, %eax
	call	Pl_Allocate
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483629, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont24:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757832__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757832__a1:
	movl	$.Lpred23_2+0, %eax
	movl	$.Lpred23_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred23_1:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred23_2:
	movl	$.Lpred23_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred23_3:
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred23_4:
	call	Pl_Delete_Choice_Point1

.Lpred23_5:
	movl	ta+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757831__a1,@function

X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757831__a1:
	movl	$.Lpred24_2+0, %eax
	movl	$.Lpred24_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred24_1:
	movl	st+16, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred24_2:
	movl	$.Lpred24_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred24_3:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred24_4:
	call	Pl_Delete_Choice_Point1

.Lpred24_5:
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2
	.p2align	4,,15
	.type	X1_246164645F616C69617365735F746F5F73747265616D__a2,@function
.globl X1_246164645F616C69617365735F746F5F73747265616D__a2

X1_246164645F616C69617365735F746F5F73747265616D__a2:
	movl	$.Lpred25_1+0, %eax
	movl	$.Lpred25_2+0, %edx
	movl	$.Lpred25_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred25_1:
	movl	$.Lpred25_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred25_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred25_3:
	call	Pl_Delete_Choice_Point2

.Lpred25_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Add_Stream_Alias_2
	jmp	X1_246164645F616C69617365735F746F5F73747265616D__a2
	.p2align	4,,15
	.type	X1_246164645F6D6972726F72735F746F5F73747265616D__a2,@function
.globl X1_246164645F6D6972726F72735F746F5F73747265616D__a2

X1_246164645F6D6972726F72735F746F5F73747265616D__a2:
	movl	$.Lpred26_1+0, %eax
	movl	$.Lpred26_2+0, %edx
	movl	$.Lpred26_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred26_1:
	movl	$.Lpred26_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred26_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred26_3:
	call	Pl_Delete_Choice_Point2

.Lpred26_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Add_Stream_Mirror_2
	jmp	X1_246164645F6D6972726F72735F746F5F73747265616D__a2
	.p2align	4,,15
	.type	X0_close__a1,@function
.globl X0_close__a1

X0_close__a1:
	movl	at+176, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	jmp	X1_24636C6F7365__a2
	.p2align	4,,15
	.type	X0_close__a2,@function
.globl X0_close__a2

X0_close__a2:
	movl	at+176, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_24636C6F7365__a2
	.p2align	4,,15
	.type	X1_24636C6F7365__a2,@function
.globl X1_24636C6F7365__a2

X1_24636C6F7365__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
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
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_246765745F636C6F73655F6F7074696F6E73__a1
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Close_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F636C6F73655F6F7074696F6E73__a1,@function
.globl X1_246765745F636C6F73655F6F7074696F6E73__a1

X1_246765745F636C6F73655F6F7074696F6E73__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C697374__a1
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F636C6F73655F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F636C6F73655F6F7074696F6E7331__a1,@function
.globl X1_246765745F636C6F73655F6F7074696F6E7331__a1

X1_246765745F636C6F73655F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred31_1+0, %eax
	movl	$.Lpred31_2+0, %edx
	movl	$.Lpred31_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred31_1:
	movl	$.Lpred31_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred31_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred31_3:
	call	Pl_Delete_Choice_Point2

.Lpred31_4:
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
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_246765745F636C6F73655F6F7074696F6E7332__a1
.Lcont28:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F636C6F73655F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F636C6F73655F6F7074696F6E7332__a1,@function
.globl X1_246765745F636C6F73655F6F7074696F6E7332__a1

X1_246765745F636C6F73655F6F7074696F6E7332__a1:
	movl	$.Lpred32_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred32_1:
	movl	$.Lpred32_2+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont29:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F636C6F73655F6F7074696F6E73322F315F2461757831__a1

.Lpred32_2:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+64, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24246765745F636C6F73655F6F7074696F6E73322F315F2461757831__a1,@function

X1_24246765745F636C6F73655F6F7074696F6E73322F315F2461757831__a1:
	movl	$.Lpred33_2+0, %eax
	movl	$.Lpred33_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred33_1:
	movl	st+20, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred33_2:
	movl	$.Lpred33_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred33_3:
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred33_4:
	call	Pl_Delete_Choice_Point1

.Lpred33_5:
	movl	ta+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X0_add_stream_alias__a2,@function
.globl X0_add_stream_alias__a2

X0_add_stream_alias__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred34_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	at+200, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Add_Stream_Alias_2
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred34_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_24706C5F6572725F7065726D697373696F6E__a3
	.p2align	4,,15
	.type	X0_add_stream_mirror__a2,@function
.globl X0_add_stream_mirror__a2

X0_add_stream_mirror__a2:
	movl	at+204, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Add_Stream_Mirror_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_remove_stream_mirror__a2,@function
.globl X0_remove_stream_mirror__a2

X0_remove_stream_mirror__a2:
	movl	at+208, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Remove_Stream_Mirror_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_stream_type__a2,@function
.globl X0_set_stream_type__a2

X0_set_stream_type__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred37_1+0, %eax
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
	movl	at+212, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F747970652F325F2461757831__a1
.Lcont30:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F747970652F325F2461757832__a2
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Stream_Type_2
	jmp	*pl_reg_bank+1040

.Lpred37_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_247365745F73747265616D5F747970652F325F2461757832__a2,@function

X1_247365745F73747265616D5F747970652F325F2461757832__a2:
	movl	$.Lpred38_2+0, %eax
	movl	$.Lpred38_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred38_1:
	movl	st+24, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred38_2:
	movl	$.Lpred38_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred38_3:
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

.Lpred38_4:
	call	Pl_Delete_Choice_Point2

.Lpred38_5:
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
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F73747265616D5F747970652F325F2461757831__a1,@function

X1_247365745F73747265616D5F747970652F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred39_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred39_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_stream_eof_action__a2,@function
.globl X0_set_stream_eof_action__a2

X0_set_stream_eof_action__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred40_1+0, %eax
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
	movl	at+224, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757831__a1
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757832__a2
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Stream_Eof_Action_2
	jmp	*pl_reg_bank+1040

.Lpred40_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757832__a2,@function

X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757832__a2:
	movl	$.Lpred41_2+0, %eax
	movl	$.Lpred41_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred41_1:
	movl	st+28, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred41_2:
	movl	$.Lpred41_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred41_3:
	movl	ta+36, %eax
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

.Lpred41_4:
	movl	$.Lpred41_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred41_5:
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

.Lpred41_6:
	call	Pl_Delete_Choice_Point2

.Lpred41_7:
	movl	ta+44, %eax
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
	.type	X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757831__a1,@function

X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred42_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred42_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_stream_buffering__a2,@function
.globl X0_set_stream_buffering__a2

X0_set_stream_buffering__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred43_1+0, %eax
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
	movl	at+236, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F627566666572696E672F325F2461757831__a1
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X1_247365745F73747265616D5F627566666572696E672F325F2461757832__a2
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Stream_Buffering_2
	jmp	*pl_reg_bank+1040

.Lpred43_1:
	call	Pl_Delete_Choice_Point3
	movl	ta+80, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_247365745F73747265616D5F627566666572696E672F325F2461757832__a2,@function

X1_247365745F73747265616D5F627566666572696E672F325F2461757832__a2:
	movl	$.Lpred44_2+0, %eax
	movl	$.Lpred44_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred44_1:
	movl	st+32, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred44_2:
	movl	$.Lpred44_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred44_3:
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
	jmp	*pl_reg_bank+1040

.Lpred44_4:
	movl	$.Lpred44_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred44_5:
	movl	ta+28, %eax
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

.Lpred44_6:
	call	Pl_Delete_Choice_Point2

.Lpred44_7:
	movl	ta+32, %eax
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
	.type	X1_247365745F73747265616D5F627566666572696E672F325F2461757831__a1,@function

X1_247365745F73747265616D5F627566666572696E672F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred45_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred45_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2450425F656D7074795F627566666572__a1,@function
.globl X1_2450425F656D7074795F627566666572__a1

X1_2450425F656D7074795F627566666572__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_PB_Empty_Buffer_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_flush_output__a0,@function
.globl X0_flush_output__a0

X0_flush_output__a0:
	movl	at+252, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Flush_Output_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_flush_output__a1,@function
.globl X0_flush_output__a1

X0_flush_output__a1:
	movl	at+252, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Flush_Output_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_stream__a1,@function
.globl X0_current_stream__a1

X0_current_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+256, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont36:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2463757272656E745F73747265616D__a1
	.p2align	4,,15
	.type	X1_2463757272656E745F73747265616D__a1,@function
.globl X1_2463757272656E745F73747265616D__a1

X1_2463757272656E745F73747265616D__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Current_Stream_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F73747265616D5F616C74__a0,@function
.globl X1_2463757272656E745F73747265616D5F616C74__a0

X1_2463757272656E745F73747265616D5F616C74__a0:
	call	Pl_Current_Stream_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_stream_property__a2,@function
.globl X0_stream_property__a2

X0_stream_property__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	at+268, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2473747265616D5F70726F70657274792F325F2461757831__a3
	.p2align	4,,15
	.type	X1_2473747265616D5F70726F70657274792F325F2461757831__a3,@function

X1_2473747265616D5F70726F70657274792F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred53_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_From_Alias_To_Stream_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred53_1:
	call	Pl_Delete_Choice_Point4
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
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F70726F70__a1
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_2463757272656E745F73747265616D__a1
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2473747265616D5F70726F706572747931__a2
	.p2align	4,,15
	.type	X1_24636865636B5F73747265616D5F70726F70__a1,@function
.globl X1_24636865636B5F73747265616D5F70726F70__a1

X1_24636865636B5F73747265616D5F70726F70__a1:
	movl	$.Lpred54_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred54_1:
	movl	$.Lpred54_28+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred54_4+0, %eax
	movl	$.Lpred54_2+0, %edx
	movl	$.Lpred54_3+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred54_2:
	movl	st+36, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred54_3:
	movl	st+40, %eax
	movl	$10, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred54_4:
	movl	$.Lpred54_6+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred54_5:
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_6:
	movl	$.Lpred54_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_7:
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_8:
	movl	$.Lpred54_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_9:
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred54_10:
	movl	$.Lpred54_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_11:
	movl	ta+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred54_12:
	movl	$.Lpred54_14+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_13:
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_14:
	movl	$.Lpred54_16+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_15:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_16:
	movl	$.Lpred54_18+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_17:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_18:
	movl	$.Lpred54_20+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_19:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_20:
	movl	$.Lpred54_22+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_21:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_22:
	movl	$.Lpred54_24+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_23:
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_24:
	movl	$.Lpred54_26+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred54_25:
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_26:
	call	Pl_Delete_Choice_Point1

.Lpred54_27:
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred54_28:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_2473747265616D5F70726F706572747931__a2,@function
.globl X1_2473747265616D5F70726F706572747931__a2

X1_2473747265616D5F70726F706572747931__a2:
	movl	$.Lpred55_3+0, %eax
	movl	$.Lpred55_1+0, %edx
	movl	$.Lpred55_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred55_1:
	movl	st+44, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred55_2:
	movl	st+48, %eax
	movl	$10, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred55_3:
	movl	$.Lpred55_5+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred55_4:
	movl	fn+32, %eax
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
	call	Pl_Stream_Prop_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_5:
	movl	$.Lpred55_7+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_6:
	movl	fn+36, %eax
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
	call	Pl_Stream_Prop_Mode_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_7:
	movl	$.Lpred55_9+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_8:
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_Stream_Prop_Input_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_9:
	movl	$.Lpred55_11+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_10:
	movl	ta+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_Stream_Prop_Output_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_11:
	movl	$.Lpred55_13+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_12:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2463757272656E745F616C696173__a2

.Lpred55_13:
	movl	$.Lpred55_15+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_14:
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
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_2463757272656E745F6D6972726F72__a2
.Lcont40:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
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

.Lpred55_15:
	movl	$.Lpred55_17+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_16:
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
	call	Pl_Stream_Prop_Type_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_17:
	movl	$.Lpred55_19+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_18:
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
	call	Pl_Stream_Prop_Reposition_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_19:
	movl	$.Lpred55_21+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_20:
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
	call	Pl_Stream_Prop_Eof_Action_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_21:
	movl	$.Lpred55_23+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_22:
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
	call	Pl_Stream_Prop_Buffering_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred55_23:
	movl	$.Lpred55_25+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred55_24:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_2473747265616D5F706F736974696F6E__a2

.Lpred55_25:
	call	Pl_Delete_Choice_Point2

.Lpred55_26:
	movl	fn+40, %eax
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
	call	Pl_Stream_Prop_End_Of_Stream_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_at_end_of_stream__a0,@function
.globl X0_at_end_of_stream__a0

X0_at_end_of_stream__a0:
	movl	at+308, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	at+308, %eax
	movl	%eax, 0(%esp)
	call	Pl_At_End_Of_Stream_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_at_end_of_stream__a1,@function
.globl X0_at_end_of_stream__a1

X0_at_end_of_stream__a1:
	movl	at+308, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_At_End_Of_Stream_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_alias__a2,@function
.globl X0_current_alias__a2

X0_current_alias__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+312, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463757272656E745F616C6961732F325F2461757831__a2
	.p2align	4,,15
	.type	X1_2463757272656E745F616C6961732F325F2461757831__a2,@function

X1_2463757272656E745F616C6961732F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred59_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_From_Alias_To_Stream_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred59_1:
	call	Pl_Delete_Choice_Point3
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X1_2463757272656E745F73747265616D__a1
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463757272656E745F616C696173__a2
	.p2align	4,,15
	.type	X1_2463757272656E745F616C696173__a2,@function
.globl X1_2463757272656E745F616C696173__a2

X1_2463757272656E745F616C696173__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Alias_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F616C6961735F616C74__a0,@function
.globl X1_2463757272656E745F616C6961735F616C74__a0

X1_2463757272656E745F616C6961735F616C74__a0:
	call	Pl_Current_Alias_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_mirror__a2,@function
.globl X0_current_mirror__a2

X0_current_mirror__a2:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+328, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont45, pl_reg_bank+1040
	jmp	X1_2463757272656E745F73747265616D__a1
.Lcont45:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463757272656E745F6D6972726F72__a2
	.p2align	4,,15
	.type	X1_2463757272656E745F6D6972726F72__a2,@function
.globl X1_2463757272656E745F6D6972726F72__a2

X1_2463757272656E745F6D6972726F72__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Mirror_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F6D6972726F725F616C74__a0,@function
.globl X1_2463757272656E745F6D6972726F725F616C74__a0

X1_2463757272656E745F6D6972726F725F616C74__a0:
	call	Pl_Current_Mirror_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_stream_position__a2,@function
.globl X0_stream_position__a2

X0_stream_position__a2:
	movl	at+340, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_2473747265616D5F706F736974696F6E__a2
	.p2align	4,,15
	.type	X1_2473747265616D5F706F736974696F6E__a2,@function
.globl X1_2473747265616D5F706F736974696F6E__a2

X1_2473747265616D5F706F736974696F6E__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Stream_Position_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_stream_position__a2,@function
.globl X0_set_stream_position__a2

X0_set_stream_position__a2:
	movl	at+348, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Stream_Position_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_seek__a4,@function
.globl X0_seek__a4

X0_seek__a4:
	movl	at+352, %eax
	movl	%eax, 0(%esp)
	movl	$4, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Seek_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_character_count__a2,@function
.globl X0_character_count__a2

X0_character_count__a2:
	movl	at+356, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Character_Count_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_line_count__a2,@function
.globl X0_line_count__a2

X0_line_count__a2:
	movl	at+360, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Line_Count_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_line_position__a2,@function
.globl X0_line_position__a2

X0_line_position__a2:
	movl	at+364, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Line_Position_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_stream_line_column__a3,@function
.globl X0_stream_line_column__a3

X0_stream_line_column__a3:
	movl	at+368, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Stream_Line_Column_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_stream_line_column__a3,@function
.globl X0_set_stream_line_column__a3

X0_set_stream_line_column__a3:
	movl	at+372, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Set_Stream_Line_Column_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_input_atom_stream__a2,@function
.globl X0_open_input_atom_stream__a2

X0_open_input_atom_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+376, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont46, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont46:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont47, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont47:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Open_Input_Term_Stream_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_input_chars_stream__a2,@function
.globl X0_open_input_chars_stream__a2

X0_open_input_chars_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+380, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont48, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont48:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont49, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont49:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Open_Input_Term_Stream_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_input_codes_stream__a2,@function
.globl X0_open_input_codes_stream__a2

X0_open_input_codes_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+384, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont50, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont50:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont51, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont51:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Open_Input_Term_Stream_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_input_atom_stream__a1,@function
.globl X0_close_input_atom_stream__a1

X0_close_input_atom_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+388, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont52, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont52:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Close_Input_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_input_chars_stream__a1,@function
.globl X0_close_input_chars_stream__a1

X0_close_input_chars_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+392, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont53, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont53:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Close_Input_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_input_codes_stream__a1,@function
.globl X0_close_input_codes_stream__a1

X0_close_input_codes_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+396, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont54, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont54:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Close_Input_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_output_atom_stream__a1,@function
.globl X0_open_output_atom_stream__a1

X0_open_output_atom_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+400, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont55, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont55:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont56, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont56:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Open_Output_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_output_chars_stream__a1,@function
.globl X0_open_output_chars_stream__a1

X0_open_output_chars_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+404, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont57, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont57:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont58, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont58:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Open_Output_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_open_output_codes_stream__a1,@function
.globl X0_open_output_codes_stream__a1

X0_open_output_codes_stream__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	at+408, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont59, pl_reg_bank+1040
	jmp	X1_246765745F6F70656E5F73746D__a2
.Lcont59:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont60, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont60:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Open_Output_Term_Stream_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_output_atom_stream__a2,@function
.globl X0_close_output_atom_stream__a2

X0_close_output_atom_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+412, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont61, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont61:
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
	call	Pl_Close_Output_Term_Stream_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_output_chars_stream__a2,@function
.globl X0_close_output_chars_stream__a2

X0_close_output_chars_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+416, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont62, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont62:
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
	call	Pl_Close_Output_Term_Stream_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_close_output_codes_stream__a2,@function
.globl X0_close_output_codes_stream__a2

X0_close_output_codes_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+420, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont63, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont63:
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
	call	Pl_Close_Output_Term_Stream_2
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
	movl	%eax, at+24
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+356
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+388
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+392
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+396
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+412
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+416
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+420
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+360
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+364
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+376
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+380
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+384
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+400
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+404
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+408
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC87, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC88, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC89, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC90, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+352
	movl	$.LC91, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC92, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC93, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC94, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC95, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+372
	movl	$.LC96, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+348
	movl	$.LC97, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC98, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+368
	movl	$.LC99, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC100, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC101, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC102, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC103, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC104, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC105, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC106, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC107, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC108, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	at+164, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	at+124, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	$.LC109, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC110, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+64
	movl	at+88, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+76
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+40
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+36
	movl	at+148, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	at+280, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+84
	movl	at+120, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+48, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+284, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+88
	movl	at+140, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	$.LC111, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC112, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC113, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+268, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	$.LC114, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+56
	movl	at+152, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+52
	movl	$.LC115, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC46, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC50, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC64, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC65, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC69, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC116, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC75, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC76, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC86, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC88, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC104, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F73747265616D__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_input__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_output__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F73747265616D5F6F725F766172__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636865636B5F73747265616D5F6F725F7661722F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_input__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$78, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_output__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$85, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F746F705F6C6576656C5F73747265616D73__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F64656275676765725F73747265616D73__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$110, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$115, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$120, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246F70656E__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$135, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F6F70656E5F64656661756C7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F6F70656E5F73746D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F73746D2F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$150, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F6F70656E5F6F7074696F6E73__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$155, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F6F70656E5F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F6F70656E5F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$6, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+92, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$6, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred18_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+104, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$219, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757836__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$211, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757835__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$197, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757834__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+116, %ecx
	movl	$.Lpred21_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+120, %ecx
	movl	$.Lpred21_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+124, %ecx
	movl	$.Lpred21_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+128, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$183, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+132, %ecx
	movl	$.Lpred22_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+136, %ecx
	movl	$.Lpred22_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+140, %ecx
	movl	$.Lpred22_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+144, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$174, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+148, %ecx
	movl	$.Lpred23_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+152, %ecx
	movl	$.Lpred23_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+156, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$166, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F6F70656E5F6F7074696F6E73322F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+160, %ecx
	movl	$.Lpred24_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+164, %ecx
	movl	$.Lpred24_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+168, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$233, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246164645F616C69617365735F746F5F73747265616D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246164645F6D6972726F72735F746F5F73747265616D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$259, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$263, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$268, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636C6F7365__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$276, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F636C6F73655F6F7074696F6E73__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$281, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F636C6F73655F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$288, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F636C6F73655F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$292, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F636C6F73655F6F7074696F6E73322F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$2, %edx
	movl	at+148, %ecx
	movl	$.Lpred33_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$2, %edx
	movl	at+152, %ecx
	movl	$.Lpred33_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+200, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$306, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_add_stream_alias__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+204, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$316, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_add_stream_mirror__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$323, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_remove_stream_mirror__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$330, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_stream_type__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$330, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F747970652F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+160, %ecx
	movl	$.Lpred38_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+164, %ecx
	movl	$.Lpred38_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+220, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$330, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F747970652F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+224, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$349, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_stream_eof_action__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+228, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$349, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+28
	movl	st+28, %eax
	movl	$3, %edx
	movl	at+132, %ecx
	movl	$.Lpred41_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+28, %eax
	movl	$3, %edx
	movl	at+136, %ecx
	movl	$.Lpred41_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+28, %eax
	movl	$3, %edx
	movl	at+140, %ecx
	movl	$.Lpred41_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+232, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$349, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F656F665F616374696F6E2F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+236, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$370, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_stream_buffering__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+240, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$370, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F627566666572696E672F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+32
	movl	st+32, %eax
	movl	$3, %edx
	movl	at+116, %ecx
	movl	$.Lpred44_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$3, %edx
	movl	at+120, %ecx
	movl	$.Lpred44_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$3, %edx
	movl	at+124, %ecx
	movl	$.Lpred44_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+244, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$370, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_247365745F73747265616D5F627566666572696E672F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+248, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$391, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2450425F656D7074795F627566666572__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$397, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_flush_output__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$401, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_flush_output__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+256, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$408, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+260, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$416, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F73747265616D__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+264, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$420, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F73747265616D5F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+268, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$426, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_stream_property__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+272, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$426, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473747265616D5F70726F70657274792F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+276, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$441, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F73747265616D5F70726F70__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+36
	movl	st+36, %eax
	movl	$2, %edx
	movl	at+280, %ecx
	movl	$.Lpred54_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+36, %eax
	movl	$2, %edx
	movl	at+284, %ecx
	movl	$.Lpred54_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$10, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+40
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+288, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+292, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_15+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_17+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_19+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_21+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+92, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_23+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+296, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_25+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+40, %eax
	movl	$10, %edx
	movl	at+300, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred54_27+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+304, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$474, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473747265616D5F70726F706572747931__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+44
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+280, %ecx
	movl	$.Lpred55_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+44, %eax
	movl	$2, %edx
	movl	at+284, %ecx
	movl	$.Lpred55_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$10, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+48
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+288, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+292, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_16+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_20+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+92, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_22+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+300, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_24+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+48, %eax
	movl	$10, %edx
	movl	at+296, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred55_26+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+308, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$515, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_at_end_of_stream__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+308, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$519, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_at_end_of_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+312, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$526, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_alias__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+316, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$526, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2463757272656E745F616C6961732F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+320, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$538, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F616C696173__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+324, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$541, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F616C6961735F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+328, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$547, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_mirror__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+332, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$557, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F6D6972726F72__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+336, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$560, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F6D6972726F725F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+340, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$566, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_stream_position__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+344, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$571, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473747265616D5F706F736974696F6E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+348, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$577, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_stream_position__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+352, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$584, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_seek__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+356, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$591, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_character_count__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+360, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$598, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_line_count__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+364, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$605, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_line_position__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+368, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$612, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_stream_line_column__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+372, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$619, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_stream_line_column__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+376, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$632, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_input_atom_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+380, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$641, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_input_chars_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+384, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$650, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_input_codes_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+388, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$659, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_input_atom_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+392, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$667, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_input_chars_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+396, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$675, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_input_codes_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+400, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$683, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_output_atom_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+404, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$692, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_output_chars_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+408, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$701, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_open_output_codes_stream__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+412, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$710, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_output_atom_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+416, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$718, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_output_chars_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+420, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$726, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_close_output_codes_stream__a2+0, 8(%esp)
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
	.string	"$$check_stream_or_var/2_$aux1"
.LC1:
	.string	"$$get_close_options2/1_$aux1"
.LC2:
	.string	"$$get_open_options2/1_$aux1"
.LC3:
	.string	"$$get_open_options2/1_$aux2"
.LC4:
	.string	"$$get_open_options2/1_$aux3"
.LC5:
	.string	"$$get_open_options2/1_$aux4"
.LC6:
	.string	"$$get_open_options2/1_$aux5"
.LC7:
	.string	"$$get_open_options2/1_$aux6"
.LC8:
	.string	"$$get_open_stm/2_$aux1"
.LC9:
	.string	"$PB_empty_buffer"
.LC10:
	.string	"$add_aliases_to_stream"
.LC11:
	.string	"$add_mirrors_to_stream"
.LC12:
	.string	"$check_stream_or_var"
.LC13:
	.string	"$check_stream_prop"
.LC14:
	.string	"$close"
.LC15:
	.string	"$current_alias"
.LC16:
	.string	"$current_alias/2_$aux1"
.LC17:
	.string	"$current_alias_alt"
.LC18:
	.string	"$current_mirror"
.LC19:
	.string	"$current_mirror_alt"
.LC20:
	.string	"$current_stream"
.LC21:
	.string	"$current_stream_alt"
.LC22:
	.string	"$get_close_options"
.LC23:
	.string	"$get_close_options1"
.LC24:
	.string	"$get_close_options2"
.LC25:
	.string	"$get_open_options"
.LC26:
	.string	"$get_open_options1"
.LC27:
	.string	"$get_open_options2"
.LC28:
	.string	"$get_open_stm"
.LC29:
	.string	"$open"
.LC106:
	.string	"$open_aliases"
.LC107:
	.string	"$open_mirrors"
.LC30:
	.string	"$set_debugger_streams"
.LC31:
	.string	"$set_open_defaults"
.LC32:
	.string	"$set_stream_buffering/2_$aux1"
.LC33:
	.string	"$set_stream_buffering/2_$aux2"
.LC34:
	.string	"$set_stream_eof_action/2_$aux1"
.LC35:
	.string	"$set_stream_eof_action/2_$aux2"
.LC36:
	.string	"$set_stream_type/2_$aux1"
.LC37:
	.string	"$set_stream_type/2_$aux2"
.LC38:
	.string	"$set_top_level_streams"
.LC115:
	.string	"$stream"
.LC39:
	.string	"$stream_position"
.LC40:
	.string	"$stream_property/2_$aux1"
.LC41:
	.string	"$stream_property1"
.LC42:
	.string	"$use_stream"
.LC43:
	.string	"/home/diaz/GP/src/BipsPl/stream.pl"
.LC108:
	.string	"add_alias"
.LC44:
	.string	"add_stream_alias"
.LC45:
	.string	"add_stream_mirror"
.LC46:
	.string	"alias"
.LC47:
	.string	"at_end_of_stream"
.LC48:
	.string	"binary"
.LC49:
	.string	"block"
.LC50:
	.string	"buffering"
.LC109:
	.string	"buffering_mode"
.LC51:
	.string	"character_count"
.LC52:
	.string	"close"
.LC53:
	.string	"close_input_atom_stream"
.LC54:
	.string	"close_input_chars_stream"
.LC55:
	.string	"close_input_codes_stream"
.LC110:
	.string	"close_option"
.LC56:
	.string	"close_output_atom_stream"
.LC57:
	.string	"close_output_chars_stream"
.LC58:
	.string	"close_output_codes_stream"
.LC59:
	.string	"current_alias"
.LC60:
	.string	"current_input"
.LC61:
	.string	"current_mirror"
.LC62:
	.string	"current_output"
.LC63:
	.string	"current_stream"
.LC64:
	.string	"end_of_stream"
.LC65:
	.string	"eof_action"
.LC66:
	.string	"eof_code"
.LC67:
	.string	"error"
.LC68:
	.string	"false"
.LC69:
	.string	"file_name"
.LC70:
	.string	"flush_output"
.LC116:
	.string	"force"
.LC71:
	.string	"input"
.LC72:
	.string	"line"
.LC73:
	.string	"line_count"
.LC74:
	.string	"line_position"
.LC75:
	.string	"mirror"
.LC76:
	.string	"mode"
.LC77:
	.string	"none"
.LC78:
	.string	"open"
.LC79:
	.string	"open_input_atom_stream"
.LC80:
	.string	"open_input_chars_stream"
.LC81:
	.string	"open_input_codes_stream"
.LC82:
	.string	"open_output_atom_stream"
.LC83:
	.string	"open_output_chars_stream"
.LC84:
	.string	"open_output_codes_stream"
.LC85:
	.string	"output"
.LC86:
	.string	"position"
.LC87:
	.string	"remove_stream_mirror"
.LC88:
	.string	"reposition"
.LC89:
	.string	"reset"
.LC90:
	.string	"seek"
.LC91:
	.string	"set_input"
.LC92:
	.string	"set_output"
.LC93:
	.string	"set_stream_buffering"
.LC94:
	.string	"set_stream_eof_action"
.LC95:
	.string	"set_stream_line_column"
.LC96:
	.string	"set_stream_position"
.LC97:
	.string	"set_stream_type"
.LC111:
	.string	"source_sink"
.LC112:
	.string	"stream"
.LC98:
	.string	"stream_line_column"
.LC113:
	.string	"stream_option"
.LC99:
	.string	"stream_position"
.LC100:
	.string	"stream_property"
.LC114:
	.string	"stream_type"
.LC101:
	.string	"system"
.LC102:
	.string	"text"
.LC103:
	.string	"true"
.LC104:
	.string	"type"
.LC105:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,424,4
	.local	fn
	.comm	fn,48,4
	.local	st
	.comm	st,52,4
	.local	ta
	.comm	ta,96,4
	.section	.note.GNU-stack,"",@progbits
