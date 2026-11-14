# BipsPl/src_rdr.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/src_rdr.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F7372635F726472__a0,@function
.globl X1_247573655F7372635F726472__a0

X1_247573655F7372635F726472__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_open__a3,@function
.globl X0_sr_open__a3

X0_sr_open__a3:
	movl	$4, %eax
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
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247365745F73725F64656661756C7473__a0
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_246765745F73725F6F7074696F6E73__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2473725F6F70656E2F335F2461757831__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Init_Open_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2473725F6F70656E2F335F2461757832__a1
	.p2align	4,,15
	.type	X1_2473725F6F70656E2F335F2461757832__a1,@function

X1_2473725F6F70656E2F335F2461757832__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Open_File_2
	jmp	*pl_reg_bank+1040

.Lpred3_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1
	.p2align	4,,15
	.type	X1_2473725F6F70656E2F335F2461757831__a1,@function

X1_2473725F6F70656E2F335F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_247365745F73725F64656661756C7473__a0,@function
.globl X1_247365745F73725F64656661756C7473__a0

X1_247365745F73725F64656661756C7473__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147479557, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont4:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483569, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont5:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X1_246765745F73725F6F7074696F6E73__a2,@function
.globl X1_246765745F73725F6F7074696F6E73__a2

X1_246765745F73725F6F7074696F6E73__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C697374__a1
.Lcont6:
	movl	ta+8, %eax
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
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_246765745F73725F6F7074696F6E7331__a1
.Lcont7:
	movl	ta+8, %eax
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
	.type	X1_246765745F73725F6F7074696F6E7331__a1,@function
.globl X1_246765745F73725F6F7074696F6E7331__a1

X1_246765745F73725F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
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
	movl	%eax, -20(%edi)
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_246765745F73725F6F7074696F6E7332__a1
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F73725F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F73725F6F7074696F6E7332__a1,@function
.globl X1_246765745F73725F6F7074696F6E7332__a1

X1_246765745F73725F6F7074696F6E7332__a1:
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred8_1:
	movl	$.Lpred8_2+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_2473725F74726561745F706173735F6E6F__a3
.Lcont10:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Mul
	movl	%eax, pl_reg_bank+4
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Inc
	movl	%eax, pl_reg_bank+8
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_2473725F7365745F74726561745F706173735F62697473__a3

.Lpred8_2:
	movl	$.Lpred8_14+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred8_4+0, %eax
	movl	$.Lpred8_3+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred8_3:
	movl	st+0, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred8_4:
	movl	$.Lpred8_6+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred8_5:
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
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F73725F6F7074696F6E73322F315F2461757831__a1

.Lpred8_6:
	movl	$.Lpred8_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred8_7:
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
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F73725F6F7074696F6E73322F315F2461757832__a1

.Lpred8_8:
	movl	$.Lpred8_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred8_9:
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
	call	Pl_Deallocate
	jmp	X1_24246765745F73725F6F7074696F6E73322F315F2461757833__a1

.Lpred8_10:
	movl	$.Lpred8_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred8_11:
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
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F73725F6F7074696F6E73322F315F2461757834__a1

.Lpred8_12:
	call	Pl_Delete_Choice_Point1

.Lpred8_13:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+20, %eax
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
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred8_14:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24246765745F73725F6F7074696F6E73322F315F2461757834__a1,@function

X1_24246765745F73725F6F7074696F6E73322F315F2461757834__a1:
	movl	$.Lpred9_2+0, %eax
	movl	$.Lpred9_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred9_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred9_2:
	movl	$.Lpred9_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred9_3:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483569, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred9_4:
	call	Pl_Delete_Choice_Point1

.Lpred9_5:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483569, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F73725F6F7074696F6E73322F315F2461757833__a1,@function

X1_24246765745F73725F6F7074696F6E73322F315F2461757833__a1:
	movl	$.Lpred10_2+0, %eax
	movl	$.Lpred10_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred10_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred10_2:
	movl	$.Lpred10_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred10_3:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483573, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred10_4:
	call	Pl_Delete_Choice_Point1

.Lpred10_5:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483573, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F73725F6F7074696F6E73322F315F2461757832__a1,@function

X1_24246765745F73725F6F7074696F6E73322F315F2461757832__a1:
	movl	$.Lpred11_2+0, %eax
	movl	$.Lpred11_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred11_1:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred11_2:
	movl	$.Lpred11_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred11_3:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483577, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred11_4:
	call	Pl_Delete_Choice_Point1

.Lpred11_5:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483577, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F73725F6F7074696F6E73322F315F2461757831__a1,@function

X1_24246765745F73725F6F7074696F6E73322F315F2461757831__a1:
	movl	$.Lpred12_2+0, %eax
	movl	$.Lpred12_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred12_1:
	movl	st+16, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred12_2:
	movl	$.Lpred12_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred12_3:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483581, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred12_4:
	call	Pl_Delete_Choice_Point1

.Lpred12_5:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483581, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_2473725F74726561745F706173735F6E6F__a3,@function
.globl X1_2473725F74726561745F706173735F6E6F__a3

X1_2473725F74726561745F706173735F6E6F__a3:
	movl	$.Lpred13_2+0, %eax
	movl	$.Lpred13_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred13_1:
	movl	st+20, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred13_2:
	movl	$.Lpred13_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred13_3:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_4:
	movl	$.Lpred13_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_5:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_6:
	movl	$.Lpred13_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_7:
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
	movl	$-2147483637, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_8:
	movl	$.Lpred13_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_9:
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
	movl	$-2147483633, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_10:
	movl	$.Lpred13_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_11:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_12:
	movl	$.Lpred13_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_13:
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
	movl	$-2147483629, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_14:
	movl	$.Lpred13_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_15:
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_16:
	call	Pl_Delete_Choice_Point3

.Lpred13_17:
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F7365745F74726561745F706173735F62697473__a3,@function
.globl X1_2473725F7365745F74726561745F706173735F62697473__a3

X1_2473725F7365745F74726561745F706173735F62697473__a3:
	movl	$.Lpred14_2+0, %eax
	movl	$.Lpred14_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred14_1:
	movl	st+24, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred14_2:
	movl	$.Lpred14_4+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred14_3:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont16:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred14_4:
	movl	$.Lpred14_6+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred14_5:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont17:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred14_6:
	movl	$.Lpred14_8+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred14_7:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72657365745F626974__a2
.Lcont18:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred14_8:
	call	Pl_Delete_Choice_Point3

.Lpred14_9:
	movl	$1, %eax
	call	Pl_Allocate
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont19:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1,@function
.globl X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1

X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Prolog_File_Name_2
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Open_File_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_change_options__a2,@function
.globl X0_sr_change_options__a2

X0_sr_change_options__a2:
	movl	$0, %eax
	call	Pl_Allocate
	movl	at+152, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Check_Descriptor_1
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_246765745F73725F6F7074696F6E7331__a1
.Lcont20:
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Change_Options_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_close__a1,@function
.globl X0_sr_close__a1

X0_sr_close__a1:
	movl	at+156, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Close_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_new_pass__a1,@function
.globl X0_sr_new_pass__a1

X0_sr_new_pass__a1:
	movl	at+160, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_2473725F6E65775F706173732F315F2461757831__a1
	.p2align	4,,15
	.type	X1_2473725F6E65775F706173732F315F2461757831__a1,@function

X1_2473725F6E65775F706173732F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred19_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_New_Pass_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred19_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+68, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+72, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_24706C5F6572725F7065726D697373696F6E__a3
	.p2align	4,,15
	.type	X0_sr_read_term__a4,@function
.globl X0_sr_read_term__a4

X0_sr_read_term__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Check_Descriptor_1
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X0_repeat__a0
.Lcont21:
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Get_Stm_For_Read_Term_1
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	at+168, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+76, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_246361746368__a6
.Lcont22:
	call	Pl_SR_Update_Position_0
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_2473725F726561645F7465726D2F345F2461757831__a3
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_2473725F726561645F7465726D2F345F2461757832__a2
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F726561645F7465726D2F345F2461757832__a2,@function

X1_2473725F726561645F7465726D2F345F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred21_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$2, %eax
	call	Pl_Unify_Void
	movl	$-2147483569, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Is_Bit_Set_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_sr_write_error__a2

.Lpred21_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F726561645F7465726D2F345F2461757831__a3,@function

X1_2473725F726561645F7465726D2F345F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F74726561745F7465726D__a2

.Lpred22_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+80, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2
	.p2align	4,,15
	.type	X1_2473725F74726561745F7465726D__a2,@function
.globl X1_2473725F74726561745F7465726D__a2

X1_2473725F74726561745F7465726D__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	ta+84, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_EOF_Reached_1
	testl	%eax, %eax
	je	fail
	jmp	X1_242473725F74726561745F7465726D2F325F2461757831__a2

.Lpred23_1:
	movl	$.Lpred23_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
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
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_2473725F74726561745F706173735F6E6F__a3
.Lcont25:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-28(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Mul
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, -32(%edi)
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-32(%edi), %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	call	Pl_Fct_Inc
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_242473725F74726561745F7465726D2F325F2461757832__a4
.Lcont26:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_242473725F74726561745F7465726D2F325F2461757833__a2

.Lpred23_2:
	call	Pl_Delete_Choice_Point3
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242473725F74726561745F7465726D2F325F2461757833__a2,@function

X1_242473725F74726561745F7465726D2F325F2461757833__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred24_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Is_Bit_Set_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred24_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242473725F74726561745F7465726D2F325F2461757832__a4,@function

X1_242473725F74726561745F7465726D2F325F2461757832__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred25_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Is_Bit_Set_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+92, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+20
	jmp	X1_246361746368__a6

.Lpred25_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242473725F74726561745F7465726D2F325F2461757831__a2,@function

X1_242473725F74726561745F7465726D2F325F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred26_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred26_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F657865635F646972656374697665__a2,@function
.globl X1_2473725F657865635F646972656374697665__a2

X1_2473725F657865635F646972656374697665__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred27_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_2473725F64697265637469766531__a2
.Lcont27:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred27_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+100, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F64697265637469766531__a2,@function
.globl X1_2473725F64697265637469766531__a2

X1_2473725F64697265637469766531__a2:
	movl	$.Lpred28_2+0, %eax
	movl	$.Lpred28_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred28_1:
	movl	st+28, %eax
	movl	$8, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred28_2:
	movl	$.Lpred28_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred28_3:
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1

.Lpred28_4:
	movl	$.Lpred28_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_5:
	movl	$5, %eax
	call	Pl_Allocate
	movl	fn+48, %eax
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
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_242473725F646972656374697665312F325F2461757831__a4
.Lcont28:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+24
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	movl	pl_reg_bank+24, %eax
	movl	%eax, 24(%esp)
	call	Pl_SR_Add_Directive_7
	jmp	*pl_reg_bank+1040

.Lpred28_6:
	movl	$.Lpred28_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_7:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X1_242473725F646972656374697665312F325F2461757832__a2
.Lcont29:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+24
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	movl	pl_reg_bank+24, %eax
	movl	%eax, 24(%esp)
	call	Pl_SR_Add_Directive_7
	jmp	*pl_reg_bank+1040

.Lpred28_8:
	movl	$.Lpred28_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_9:
	movl	$3, %eax
	call	Pl_Allocate
	movl	fn+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X1_242473725F646972656374697665312F325F2461757833__a2
.Lcont30:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+24
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	movl	pl_reg_bank+24, %eax
	movl	%eax, 24(%esp)
	call	Pl_SR_Add_Directive_7
	jmp	*pl_reg_bank+1040

.Lpred28_10:
	movl	$.Lpred28_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_11:
	movl	fn+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F73746172745F6D6F64756C65__a3

.Lpred28_12:
	movl	$.Lpred28_14+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_13:
	movl	fn+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F73746172745F6D6F64756C65__a3

.Lpred28_14:
	movl	$.Lpred28_16+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred28_15:
	movl	fn+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F73746F705F6D6F64756C65__a3

.Lpred28_16:
	call	Pl_Delete_Choice_Point2

.Lpred28_17:
	movl	fn+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473725F73746F705F6D6F64756C65__a3
	.p2align	4,,15
	.type	X1_242473725F646972656374697665312F325F2461757833__a2,@function

X1_242473725F646972656374697665312F325F2461757833__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred29_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X0_current_char_conversion__a2
.Lcont31:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred29_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242473725F646972656374697665312F325F2461757832__a2,@function

X1_242473725F646972656374697665312F325F2461757832__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred30_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X0_current_prolog_flag__a2
.Lcont32:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred30_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_242473725F646972656374697665312F325F2461757831__a4,@function

X1_242473725F646972656374697665312F325F2461757831__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred31_1+0, %eax
	movl	$5, %edx
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
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_2473725F6F705F74797065__a2
.Lcont33:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X0_current_op__a3
.Lcont34:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont35, pl_reg_bank+1040
	jmp	X1_2473725F6F705F74797065__a2
.Lcont35:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred31_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	$-2147483645, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F6F705F74797065__a2,@function
.globl X1_2473725F6F705F74797065__a2

X1_2473725F6F705F74797065__a2:
	movl	$.Lpred32_2+0, %eax
	movl	$.Lpred32_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred32_1:
	movl	st+32, %eax
	movl	$7, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred32_2:
	movl	$.Lpred32_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred32_3:
	movl	ta+104, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+108, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_4:
	movl	$.Lpred32_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred32_5:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+108, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_6:
	movl	$.Lpred32_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred32_7:
	movl	ta+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_8:
	movl	$.Lpred32_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred32_9:
	movl	ta+124, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_10:
	movl	$.Lpred32_12+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred32_11:
	movl	ta+128, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+120, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred32_12:
	movl	$.Lpred32_14+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred32_13:
	movl	ta+132, %eax
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

.Lpred32_14:
	call	Pl_Delete_Choice_Point2

.Lpred32_15:
	movl	ta+140, %eax
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
	.p2align	4,,15
	.type	X1_2473725F73746172745F6D6F64756C65__a3,@function
.globl X1_2473725F73746172745F6D6F64756C65__a3

X1_2473725F73746172745F6D6F64756C65__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Start_Module_3
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_242473725F73746172745F6D6F64756C652F335F2461757831__a2
	.p2align	4,,15
	.type	X1_242473725F73746172745F6D6F64756C652F335F2461757831__a2,@function

X1_242473725F73746172745F6D6F64756C652F335F2461757831__a2:
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
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred34_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F73746F705F6D6F64756C65__a3,@function
.globl X1_2473725F73746F705F6D6F64756C65__a3

X1_2473725F73746F705F6D6F64756C65__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Stop_Module_3
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_242473725F73746F705F6D6F64756C652F335F2461757831__a2
	.p2align	4,,15
	.type	X1_242473725F73746F705F6D6F64756C652F335F2461757831__a2,@function

X1_242473725F73746F705F6D6F64756C652F335F2461757831__a2:
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
	movl	ta+88, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred36_1:
	call	Pl_Delete_Choice_Point3
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+96, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_current_descriptor__a1,@function
.globl X0_sr_current_descriptor__a1

X0_sr_current_descriptor__a1:
	movl	at+264, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_SR_Current_Descriptor_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F63757272656E745F64657363726970746F725F616C74__a0,@function
.globl X1_2473725F63757272656E745F64657363726970746F725F616C74__a0

X1_2473725F63757272656E745F64657363726970746F725F616C74__a0:
	call	Pl_SR_Current_Descriptor_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_stream__a2,@function
.globl X0_sr_get_stream__a2

X0_sr_get_stream__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+272, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont36, pl_reg_bank+1040
	jmp	X1_24636865636B5F73747265616D5F6F725F766172__a2
.Lcont36:
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
	call	Pl_SR_Get_Stm_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_module__a3,@function
.globl X0_sr_get_module__a3

X0_sr_get_module__a3:
	movl	at+276, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Get_Module_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_file_name__a2,@function
.globl X0_sr_get_file_name__a2

X0_sr_get_file_name__a2:
	movl	at+280, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Get_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_position__a3,@function
.globl X0_sr_get_position__a3

X0_sr_get_position__a3:
	movl	at+284, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Get_Position_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_include_list__a2,@function
.globl X0_sr_get_include_list__a2

X0_sr_get_include_list__a2:
	movl	at+288, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Get_Include_List_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_include_stream_list__a2,@function
.globl X0_sr_get_include_stream_list__a2

X0_sr_get_include_stream_list__a2:
	movl	at+292, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_SR_Get_Include_Stream_List_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_size_counters__a3,@function
.globl X0_sr_get_size_counters__a3

X0_sr_get_size_counters__a3:
	movl	at+296, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Get_Size_Counters_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_get_error_counters__a3,@function
.globl X0_sr_get_error_counters__a3

X0_sr_get_error_counters__a3:
	movl	at+300, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Get_Error_Counters_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_set_error_counters__a3,@function
.globl X0_sr_set_error_counters__a3

X0_sr_set_error_counters__a3:
	movl	at+304, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_SR_Set_Error_Counters_3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_message__a4,@function
.globl X0_sr_write_message__a4

X0_sr_write_message__a4:
	movl	at+308, %eax
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
	call	Pl_SR_Write_Message_4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_message__a6,@function
.globl X0_sr_write_message__a6

X0_sr_write_message__a6:
	movl	at+308, %eax
	movl	%eax, 0(%esp)
	movl	$6, 4(%esp)
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	call	Pl_SR_Write_Message_6
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_message__a8,@function
.globl X0_sr_write_message__a8

X0_sr_write_message__a8:
	movl	at+308, %eax
	movl	%eax, 0(%esp)
	movl	$8, 4(%esp)
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
	movl	pl_reg_bank+20, %eax
	movl	%eax, 20(%esp)
	movl	pl_reg_bank+24, %eax
	movl	%eax, 24(%esp)
	movl	pl_reg_bank+28, %eax
	movl	%eax, 28(%esp)
	call	Pl_SR_Write_Message_8
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_error__a2,@function
.globl X0_sr_write_error__a2

X0_sr_write_error__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred51_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	at+312, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
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
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont37, pl_reg_bank+1040
	jmp	X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6
.Lcont37:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont38, pl_reg_bank+1040
	jmp	X1_2473725F77726974655F6572726F722F325F2461757831__a6
.Lcont38:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred51_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F77726974655F6572726F722F325F2461757831__a6,@function

X1_2473725F77726974655F6572726F722F325F2461757831__a6:
	movl	$.Lpred52_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 12(%esp)
	call	Pl_SR_Write_Message_4
	jmp	*pl_reg_bank+1040

.Lpred52_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+20, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 16(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 20(%esp)
	call	Pl_SR_Write_Message_6
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_error__a4,@function
.globl X0_sr_write_error__a4

X0_sr_write_error__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred53_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$9, %eax
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
	movl	at+312, %eax
	movl	%eax, 0(%esp)
	movl	$4, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont39, pl_reg_bank+1040
	jmp	X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6
.Lcont39:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont40, pl_reg_bank+1040
	jmp	X1_2473725F77726974655F6572726F722F345F2461757831__a4
.Lcont40:
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
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
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
	call	Pl_SR_Write_Message_6
	movl	-28(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred53_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F77726974655F6572726F722F345F2461757831__a4,@function

X1_2473725F77726974655F6572726F722F345F2461757831__a4:
	movl	$.Lpred54_1+0, %eax
	call	Pl_Create_Choice_Point4
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
	jmp	*pl_reg_bank+1040

.Lpred54_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_write_error__a6,@function
.globl X0_sr_write_error__a6

X0_sr_write_error__a6:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+24
	movl	$.Lpred55_1+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point
	movl	$11, %eax
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
	movl	pl_reg_bank+24, %eax
	movl	%eax, -36(%edi)
	movl	at+312, %eax
	movl	%eax, 0(%esp)
	movl	$6, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-44(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-48(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	leal	-52(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+16
	leal	-56(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+20
	movl	$.Lcont41, pl_reg_bank+1040
	jmp	X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6
.Lcont41:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont42, pl_reg_bank+1040
	jmp	X1_2473725F77726974655F6572726F722F365F2461757831__a4
.Lcont42:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	movl	-44(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	movl	-48(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+20
	movl	-52(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+24
	movl	-56(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+28
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
	call	Pl_SR_Write_Message_8
	movl	-36(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred55_1:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F77726974655F6572726F722F365F2461757831__a4,@function

X1_2473725F77726974655F6572726F722F365F2461757831__a4:
	movl	$.Lpred56_1+0, %eax
	call	Pl_Create_Choice_Point4
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
	jmp	*pl_reg_bank+1040

.Lpred56_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6,@function
.globl X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6

X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6:
	movl	$.Lpred57_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred57_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X1_2473725F73696D706C5F6572726F72__a5
	.p2align	4,,15
	.type	X1_2473725F73696D706C5F6572726F72__a5,@function
.globl X1_2473725F73696D706C5F6572726F72__a5

X1_2473725F73696D706C5F6572726F72__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred58_8+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred58_2+0, %eax
	movl	$.Lpred58_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred58_1:
	movl	st+36, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred58_2:
	movl	$.Lpred58_4+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred58_3:
	movl	fn+76, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	call	Pl_Fct_Neg
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	ta+144, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred58_4:
	movl	$.Lpred58_6+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point

.Lpred58_5:
	movl	fn+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+148, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	ta+152, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred58_6:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred58_7:
	movl	fn+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+156, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+148, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	ta+160, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred58_8:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	ta+164, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sr_error_from_exception__a2,@function
.globl X0_sr_error_from_exception__a2

X0_sr_error_from_exception__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+348, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont43, pl_reg_bank+1040
	jmp	X1_2473725F6572726F725F66726F6D5F657863657074696F6E2F325F2461757831__a1
.Lcont43:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2
	.p2align	4,,15
	.type	X1_2473725F6572726F725F66726F6D5F657863657074696F6E2F325F2461757831__a1,@function

X1_2473725F6572726F725F66726F6D5F657863657074696F6E2F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred60_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred60_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2,@function
.globl X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2

X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred61_7+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$.Lpred61_3+0, %eax
	movl	$.Lpred61_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred61_1:
	movl	st+40, %eax
	movl	$1, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred61_2:
	movl	$.Lpred61_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred61_4

.Lpred61_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred61_6

.Lpred61_3:
	movl	$.Lpred61_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred61_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	fn+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont44, pl_reg_bank+1040
	jmp	X0_syntax_error_info__a4
.Lcont44:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+168, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+76, %eax
	call	Pl_Unify_Structure_Tagged
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
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred61_5:
	call	Pl_Delete_Choice_Point3

.Lpred61_6:
	movl	fn+88, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$1, %eax
	call	Pl_Unify_Void
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+168, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred61_7:
	call	Pl_Delete_Choice_Point3
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+172, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
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
	movl	%eax, at+88
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+268
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+356
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+352
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+328
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+332
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+316
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+320
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+324
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+360
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+340
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+344
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+264
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+348
	movl	$.LC66, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+300
	movl	$.LC67, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+280
	movl	$.LC68, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+288
	movl	$.LC69, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+292
	movl	$.LC70, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+276
	movl	$.LC71, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+284
	movl	$.LC72, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+296
	movl	$.LC73, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+272
	movl	$.LC74, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC75, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC76, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC77, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+304
	movl	$.LC78, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+312
	movl	$.LC79, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+308
	movl	$.LC80, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+336
	movl	$.LC81, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC82, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC83, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC84, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC85, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC86, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC87, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC88, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC89, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+244
	movl	$.LC90, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC91, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC92, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+80
	movl	$.LC93, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+92
	movl	at+120, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	$.LC94, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+152
	movl	$.LC95, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+160
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	$.LC96, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+100
	movl	at+124, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+40
	movl	$.LC97, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+84
	movl	at+360, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+168
	movl	$.LC98, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+172
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+220, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+104
	movl	at+224, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+112
	movl	at+140, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+56
	movl	at+96, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	$.LC99, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+120
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+52
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+36
	movl	$.LC100, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+68
	movl	$.LC101, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+72
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	$.LC102, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+156
	movl	$.LC103, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+136
	movl	$.LC104, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+108
	movl	at+144, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC105, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+148
	movl	$.LC106, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	$.LC107, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+168, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+76
	movl	at+76, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	$.LC108, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC109, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+96
	movl	at+240, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	at+228, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+116
	movl	at+236, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+128
	movl	at+244, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	at+232, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+124
	movl	$.LC110, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+144
	movl	$.LC111, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+164
	movl	$.LC112, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC18, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC113, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC114, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC40, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC41, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC42, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC43, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC44, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC45, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC51, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC54, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC55, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC56, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC57, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	$.LC59, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC60, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC61, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC115, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC80, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC116, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC83, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC85, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F7372635F726472__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_open__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F6F70656E2F335F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F6F70656E2F335F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$104, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F73725F64656661756C7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$110, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F73725F6F7074696F6E73__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F73725F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$124, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F73725F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+48, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred8_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+52, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred8_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+56, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred8_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+60, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred8_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred8_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F73725F6F7074696F6E73322F315F2461757834__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+72, %ecx
	movl	$.Lpred9_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+76, %ecx
	movl	$.Lpred9_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+80, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$154, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F73725F6F7074696F6E73322F315F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+72, %ecx
	movl	$.Lpred10_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+76, %ecx
	movl	$.Lpred10_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+84, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F73725F6F7074696F6E73322F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+72, %ecx
	movl	$.Lpred11_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+76, %ecx
	movl	$.Lpred11_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+88, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$138, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F73725F6F7074696F6E73322F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+72, %ecx
	movl	$.Lpred12_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+76, %ecx
	movl	$.Lpred12_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+92, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$183, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F74726561745F706173735F6E6F__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+96, %ecx
	movl	$.Lpred13_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+100, %ecx
	movl	$.Lpred13_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+104, %ecx
	movl	$.Lpred13_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+108, %ecx
	movl	$.Lpred13_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+112, %ecx
	movl	$.Lpred13_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+116, %ecx
	movl	$.Lpred13_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+120, %ecx
	movl	$.Lpred13_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$8, %edx
	movl	at+124, %ecx
	movl	$.Lpred13_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+128, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$195, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F7365745F74726561745F706173735F62697473__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+132, %ecx
	movl	$.Lpred14_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+136, %ecx
	movl	$.Lpred14_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+140, %ecx
	movl	$.Lpred14_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$4, %edx
	movl	at+144, %ecx
	movl	$.Lpred14_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+148, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$214, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F6F70656E5F6E65775F70726F6C6F675F66696C65__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$226, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_change_options__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+156, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$235, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_close__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+160, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_new_pass__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+164, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$241, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F6E65775F706173732F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+168, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$251, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_read_term__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+172, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$251, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F726561645F7465726D2F345F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+176, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$251, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F726561645F7465726D2F345F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+180, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$278, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F74726561745F7465726D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+184, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F74726561745F7465726D2F325F2461757833__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+188, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$286, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F74726561745F7465726D2F325F2461757832__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+192, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$278, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F74726561745F7465726D2F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+196, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$311, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F657865635F646972656374697665__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+200, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$321, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F64697265637469766531__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$8, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+28
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred28_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+100, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred28_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+104, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred28_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+108, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred28_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+112, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred28_11+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+120, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred28_13+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+116, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred28_15+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+28, %eax
	movl	$8, %edx
	movl	at+124, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred28_17+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+204, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$347, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F646972656374697665312F325F2461757833__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+208, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$338, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F646972656374697665312F325F2461757832__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+212, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$324, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F646972656374697665312F325F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+216, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$371, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F6F705F74797065__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$7, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+32
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+220, %ecx
	movl	$.Lpred32_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+224, %ecx
	movl	$.Lpred32_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+228, %ecx
	movl	$.Lpred32_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+232, %ecx
	movl	$.Lpred32_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+236, %ecx
	movl	$.Lpred32_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+240, %ecx
	movl	$.Lpred32_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+32, %eax
	movl	$7, %edx
	movl	at+244, %ecx
	movl	$.Lpred32_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+248, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$382, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F73746172745F6D6F64756C65__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$382, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F73746172745F6D6F64756C652F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+256, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$391, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F73746F705F6D6F64756C65__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+260, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$391, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242473725F73746F705F6D6F64756C652F335F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+264, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$400, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_current_descriptor__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+268, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$407, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F63757272656E745F64657363726970746F725F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+272, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$413, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_stream__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+276, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$421, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_module__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+280, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$428, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_file_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+284, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$435, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_position__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+288, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$442, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_include_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+292, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$449, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_include_stream_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+296, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$456, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_size_counters__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+300, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$463, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_get_error_counters__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+304, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$470, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_set_error_counters__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+308, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$477, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_message__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+308, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$484, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_message__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+308, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$491, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_message__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+312, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$498, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_error__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+316, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$498, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F77726974655F6572726F722F325F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+312, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$512, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_error__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+320, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$512, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F77726974655F6572726F722F345F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+312, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$523, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_write_error__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+324, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$523, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F77726974655F6572726F722F365F2461757831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+328, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$535, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F6765745F666F726D61745F617267735F6572726F72__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+332, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$546, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F73696D706C5F6572726F72__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+36
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+336, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred58_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+340, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred58_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+36, %eax
	movl	$3, %edx
	movl	at+344, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred58_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+348, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$569, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sr_error_from_exception__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+352, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$569, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2473725F6572726F725F66726F6D5F657863657074696F6E2F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+356, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$578, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473725F6572726F725F66726F6D5F657863657074696F6E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$1, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+40
	movl	st+40, %eax
	movl	$1, %edx
	movl	at+360, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred61_2+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
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
	.string	"$$get_sr_options2/1_$aux1"
.LC1:
	.string	"$$get_sr_options2/1_$aux2"
.LC2:
	.string	"$$get_sr_options2/1_$aux3"
.LC3:
	.string	"$$get_sr_options2/1_$aux4"
.LC4:
	.string	"$$sr_directive1/2_$aux1"
.LC5:
	.string	"$$sr_directive1/2_$aux2"
.LC6:
	.string	"$$sr_directive1/2_$aux3"
.LC7:
	.string	"$$sr_start_module/3_$aux1"
.LC8:
	.string	"$$sr_stop_module/3_$aux1"
.LC9:
	.string	"$$sr_treat_term/2_$aux1"
.LC10:
	.string	"$$sr_treat_term/2_$aux2"
.LC11:
	.string	"$$sr_treat_term/2_$aux3"
.LC12:
	.string	"$get_sr_options"
.LC13:
	.string	"$get_sr_options1"
.LC14:
	.string	"$get_sr_options2"
.LC112:
	.string	"$read_term"
.LC15:
	.string	"$set_sr_defaults"
.LC16:
	.string	"$sr_current_descriptor_alt"
.LC17:
	.string	"$sr_directive1"
.LC18:
	.string	"$sr_error_from_exception"
.LC19:
	.string	"$sr_error_from_exception/2_$aux1"
.LC20:
	.string	"$sr_exec_directive"
.LC21:
	.string	"$sr_get_format_args_error"
.LC22:
	.string	"$sr_new_pass/1_$aux1"
.LC23:
	.string	"$sr_op_type"
.LC24:
	.string	"$sr_open/3_$aux1"
.LC25:
	.string	"$sr_open/3_$aux2"
.LC26:
	.string	"$sr_open_new_prolog_file"
.LC91:
	.string	"$sr_output_stream"
.LC27:
	.string	"$sr_read_term/4_$aux1"
.LC28:
	.string	"$sr_read_term/4_$aux2"
.LC92:
	.string	"$sr_read_term_error"
.LC29:
	.string	"$sr_set_treat_pass_bits"
.LC30:
	.string	"$sr_simpl_error"
.LC31:
	.string	"$sr_start_module"
.LC32:
	.string	"$sr_stop_module"
.LC33:
	.string	"$sr_treat_pass_no"
.LC34:
	.string	"$sr_treat_term"
.LC35:
	.string	"$sr_write_error/2_$aux1"
.LC36:
	.string	"$sr_write_error/4_$aux1"
.LC37:
	.string	"$sr_write_error/6_$aux1"
.LC113:
	.string	"$stream"
.LC38:
	.string	"$use_src_rdr"
.LC39:
	.string	"/home/diaz/GP/src/BipsPl/src_rdr.pl"
.LC114:
	.string	":-"
.LC93:
	.string	"any"
.LC40:
	.string	"body"
.LC94:
	.string	"cannot open file ~a - does not exist~n"
.LC95:
	.string	"cannot open file ~a - permission error~n"
.LC41:
	.string	"char_conversion"
.LC96:
	.string	"directive failed"
.LC42:
	.string	"end_body"
.LC43:
	.string	"end_module"
.LC97:
	.string	"end_of_file"
.LC44:
	.string	"error"
.LC98:
	.string	"exception"
.LC45:
	.string	"existence_error"
.LC46:
	.string	"false"
.LC47:
	.string	"fx"
.LC48:
	.string	"fy"
.LC49:
	.string	"hide"
.LC50:
	.string	"ignore"
.LC51:
	.string	"include"
.LC99:
	.string	"infix"
.LC52:
	.string	"is"
.LC53:
	.string	"kill"
.LC54:
	.string	"module"
.LC100:
	.string	"new_pass"
.LC101:
	.string	"one_pass_reader"
.LC55:
	.string	"op"
.LC102:
	.string	"open"
.LC56:
	.string	"output_stream"
.LC57:
	.string	"permission_error"
.LC103:
	.string	"postfix"
.LC104:
	.string	"prefix"
.LC58:
	.string	"reflect"
.LC59:
	.string	"reflect_eof"
.LC60:
	.string	"restart"
.LC61:
	.string	"set_prolog_flag"
.LC105:
	.string	"source_sink"
.LC62:
	.string	"sr_change_options"
.LC63:
	.string	"sr_close"
.LC64:
	.string	"sr_current_descriptor"
.LC115:
	.string	"sr_error"
.LC65:
	.string	"sr_error_from_exception"
.LC66:
	.string	"sr_get_error_counters"
.LC67:
	.string	"sr_get_file_name"
.LC68:
	.string	"sr_get_include_list"
.LC69:
	.string	"sr_get_include_stream_list"
.LC70:
	.string	"sr_get_module"
.LC71:
	.string	"sr_get_position"
.LC72:
	.string	"sr_get_size_counters"
.LC73:
	.string	"sr_get_stream"
.LC74:
	.string	"sr_new_pass"
.LC106:
	.string	"sr_ok"
.LC75:
	.string	"sr_open"
.LC107:
	.string	"sr_option"
.LC76:
	.string	"sr_read_term"
.LC77:
	.string	"sr_set_error_counters"
.LC78:
	.string	"sr_write_error"
.LC79:
	.string	"sr_write_message"
.LC80:
	.string	"syntax"
.LC116:
	.string	"syntax_error"
.LC81:
	.string	"system"
.LC82:
	.string	"true"
.LC83:
	.string	"undo_directives"
.LC84:
	.string	"user"
.LC108:
	.string	"variable"
.LC109:
	.string	"warning"
.LC85:
	.string	"write_error"
.LC86:
	.string	"xf"
.LC87:
	.string	"xfx"
.LC88:
	.string	"xfy"
.LC89:
	.string	"yf"
.LC90:
	.string	"yfx"
.LC110:
	.string	"~a~n"
.LC111:
	.string	"~w~n"
	.data	
	.align	4
	.local	at
	.comm	at,364,4
	.local	fn
	.comm	fn,96,4
	.local	st
	.comm	st,44,4
	.local	ta
	.comm	ta,176,4
	.section	.note.GNU-stack,"",@progbits
