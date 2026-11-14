# BipsPl/write.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/write.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F7772697465__a0,@function
.globl X1_247573655F7772697465__a0

X1_247573655F7772697465__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write__a1,@function
.globl X0_write__a1

X0_write__a1:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Write_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write__a2,@function
.globl X0_write__a2

X0_write__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_writeq__a1,@function
.globl X0_writeq__a1

X0_writeq__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Writeq_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_writeq__a2,@function
.globl X0_writeq__a2

X0_writeq__a2:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Writeq_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_canonical__a1,@function
.globl X0_write_canonical__a1

X0_write_canonical__a1:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Write_Canonical_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_canonical__a2,@function
.globl X0_write_canonical__a2

X0_write_canonical__a2:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Canonical_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_display__a1,@function
.globl X0_display__a1

X0_display__a1:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Display_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_display__a2,@function
.globl X0_display__a2

X0_display__a2:
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Display_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_term__a2,@function
.globl X0_write_term__a2

X0_write_term__a2:
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247365745F77726974655F64656661756C7473__a0
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E73__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Write_Term_1
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred10_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_term__a3,@function
.globl X0_write_term__a3

X0_write_term__a3:
	movl	$.Lpred11_1+0, %eax
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
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247365745F77726974655F64656661756C7473__a0
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E73__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Write_Term_2
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred11_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F77726974655F64656661756C7473__a0,@function
.globl X1_247365745F77726974655F64656661756C7473__a0

X1_247365745F77726974655F64656661756C7473__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont5:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont6:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147478845, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont7:
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X1_246765745F77726974655F6F7074696F6E73__a1,@function
.globl X1_246765745F77726974655F6F7074696F6E73__a1

X1_246765745F77726974655F6F7074696F6E73__a1:
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
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E7331__a1
.Lcont9:
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E732F315F2461757831__a0
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E732F315F2461757831__a0,@function

X1_24246765745F77726974655F6F7074696F6E732F315F2461757831__a0:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+0
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483621, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_247379735F7661725F6765745F626974__a3
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred14_1:
	call	Pl_Delete_Choice_Point1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F77726974655F6F7074696F6E7331__a1,@function
.globl X1_246765745F77726974655F6F7074696F6E7331__a1

X1_246765745F77726974655F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred15_1+0, %eax
	movl	$.Lpred15_2+0, %edx
	movl	$.Lpred15_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred15_1:
	movl	$.Lpred15_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred15_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred15_3:
	call	Pl_Delete_Choice_Point2

.Lpred15_4:
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
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_246765745F77726974655F6F7074696F6E7332__a1
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F77726974655F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F77726974655F6F7074696F6E7332__a1,@function
.globl X1_246765745F77726974655F6F7074696F6E7332__a1

X1_246765745F77726974655F6F7074696F6E7332__a1:
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred16_1:
	movl	$.Lpred16_23+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred16_3+0, %eax
	movl	$.Lpred16_2+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred16_2:
	movl	st+0, %eax
	movl	$10, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred16_3:
	movl	$.Lpred16_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred16_4:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+0, %eax
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
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757831__a1

.Lpred16_5:
	movl	$.Lpred16_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_6:
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
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757832__a1

.Lpred16_7:
	movl	$.Lpred16_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_8:
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
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757833__a1

.Lpred16_9:
	movl	$.Lpred16_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_10:
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
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757834__a1

.Lpred16_11:
	movl	$.Lpred16_13+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_12:
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
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont16:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred16_13:
	movl	$.Lpred16_15+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_14:
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
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757835__a1

.Lpred16_15:
	movl	$.Lpred16_17+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_16:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont18:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757836__a1

.Lpred16_17:
	movl	$.Lpred16_19+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_18:
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
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont19:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483621, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7365745F626974__a2
.Lcont20:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246E616D655F7661726961626C6573__a1

.Lpred16_19:
	movl	$.Lpred16_21+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred16_20:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont21:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred16_21:
	call	Pl_Delete_Choice_Point1

.Lpred16_22:
	movl	$1, %eax
	call	Pl_Allocate
	movl	fn+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont22:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred16_23:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757836__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757836__a1:
	movl	$.Lpred17_2+0, %eax
	movl	$.Lpred17_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred17_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred17_2:
	movl	$.Lpred17_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred17_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred17_4:
	call	Pl_Delete_Choice_Point1

.Lpred17_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483625, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757835__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757835__a1:
	movl	$.Lpred18_2+0, %eax
	movl	$.Lpred18_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred18_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred18_2:
	movl	$.Lpred18_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred18_3:
	movl	ta+4, %eax
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
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred18_4:
	call	Pl_Delete_Choice_Point1

.Lpred18_5:
	movl	ta+8, %eax
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
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757834__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757834__a1:
	movl	$.Lpred19_2+0, %eax
	movl	$.Lpred19_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred19_1:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred19_2:
	movl	$.Lpred19_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred19_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred19_4:
	call	Pl_Delete_Choice_Point1

.Lpred19_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757833__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757833__a1:
	movl	$.Lpred20_2+0, %eax
	movl	$.Lpred20_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred20_1:
	movl	st+16, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred20_2:
	movl	$.Lpred20_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred20_3:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred20_4:
	call	Pl_Delete_Choice_Point1

.Lpred20_5:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2
	.p2align	4,,15
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757832__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757832__a1:
	movl	$.Lpred21_2+0, %eax
	movl	$.Lpred21_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred21_1:
	movl	st+20, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred21_2:
	movl	$.Lpred21_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred21_3:
	movl	ta+4, %eax
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

.Lpred21_4:
	call	Pl_Delete_Choice_Point1

.Lpred21_5:
	movl	ta+8, %eax
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
	.type	X1_24246765745F77726974655F6F7074696F6E73322F315F2461757831__a1,@function

X1_24246765745F77726974655F6F7074696F6E73322F315F2461757831__a1:
	movl	$.Lpred22_2+0, %eax
	movl	$.Lpred22_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred22_1:
	movl	st+24, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred22_2:
	movl	$.Lpred22_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred22_3:
	movl	ta+4, %eax
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

.Lpred22_4:
	call	Pl_Delete_Choice_Point1

.Lpred22_5:
	movl	ta+8, %eax
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
	.type	X1_246E616D655F7661726961626C6573__a1,@function
.globl X1_246E616D655F7661726961626C6573__a1

X1_246E616D655F7661726961626C6573__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred23_1:
	call	Pl_Delete_Choice_Point2
	movl	$.Lpred23_2+0, %eax
	movl	$.Lpred23_3+0, %edx
	movl	$.Lpred23_5+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred23_2:
	movl	$.Lpred23_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred23_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_4:
	call	Pl_Delete_Choice_Point2

.Lpred23_5:
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
	movl	fn+40, %eax
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
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont23:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_24246E616D655F7661726961626C65732F315F2461757831__a3
.Lcont24:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246E616D655F7661726961626C6573__a1
	.p2align	4,,15
	.type	X1_24246E616D655F7661726961626C65732F315F2461757831__a3,@function

X1_24246E616D655F7661726961626C65732F315F2461757831__a3:
	movl	$.Lpred24_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred24_1:
	call	Pl_Delete_Choice_Point3
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2469735F76616C69645F7661725F6E616D65__a1,@function
.globl X1_2469735F76616C69645F7661725F6E616D65__a1

X1_2469735F76616C69645F7661725F6E616D65__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Is_Valid_Var_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nl__a0,@function
.globl X0_nl__a0

X0_nl__a0:
	movl	at+140, %eax
	movl	%eax, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	call	Pl_Nl_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nl__a1,@function
.globl X0_nl__a1

X0_nl__a1:
	movl	at+140, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Nl_1
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
	movl	%eax, at+40
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	$.LC36, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC37, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC8, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC38, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC19, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC20, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC21, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC23, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC24, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC26, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC27, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC31, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F7772697465__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$53, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_writeq__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_writeq__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_canonical__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$75, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_canonical__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$82, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_display__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$86, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_display__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$110, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$120, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_term__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$131, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F77726974655F64656661756C7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$140, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F77726974655F6F7074696F6E73__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$140, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E732F315F2461757831__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$151, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F77726974655F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$158, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F77726974655F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$10, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+56, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+60, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+68, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+72, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+76, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_14+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+80, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_16+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+84, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+88, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_20+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$10, %edx
	movl	at+92, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred16_22+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+96, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$207, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757836__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred17_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred17_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+108, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$199, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757835__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred18_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred18_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+112, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$186, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757834__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred19_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred19_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+116, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$178, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757833__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred20_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred20_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+120, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$170, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+20
	movl	st+20, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred21_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+20, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred21_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+124, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$162, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F77726974655F6F7074696F6E73322F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+24
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+100, %ecx
	movl	$.Lpred22_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+24, %eax
	movl	$2, %edx
	movl	at+104, %ecx
	movl	$.Lpred22_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+128, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246E616D655F7661726961626C6573__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$242, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246E616D655F7661726961626C65732F315F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$251, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2469735F76616C69645F7661725F6E616D65__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$256, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nl__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$260, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nl__a1+0, 8(%esp)
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
	.string	"$$get_write_options/1_$aux1"
.LC1:
	.string	"$$get_write_options2/1_$aux1"
.LC2:
	.string	"$$get_write_options2/1_$aux2"
.LC3:
	.string	"$$get_write_options2/1_$aux3"
.LC4:
	.string	"$$get_write_options2/1_$aux4"
.LC5:
	.string	"$$get_write_options2/1_$aux5"
.LC6:
	.string	"$$get_write_options2/1_$aux6"
.LC7:
	.string	"$$name_variables/1_$aux1"
.LC37:
	.string	"$VARNAME"
.LC8:
	.string	"$above"
.LC9:
	.string	"$get_write_options"
.LC10:
	.string	"$get_write_options1"
.LC11:
	.string	"$get_write_options2"
.LC12:
	.string	"$is_valid_var_name"
.LC13:
	.string	"$name_variables"
.LC14:
	.string	"$set_write_defaults"
.LC15:
	.string	"$use_write"
.LC16:
	.string	"/home/diaz/GP/src/BipsPl/write.pl"
.LC38:
	.string	"="
.LC17:
	.string	"display"
.LC18:
	.string	"false"
.LC19:
	.string	"ignore_ops"
.LC20:
	.string	"max_depth"
.LC21:
	.string	"namevars"
.LC22:
	.string	"nl"
.LC23:
	.string	"numbervars"
.LC24:
	.string	"portrayed"
.LC25:
	.string	"priority"
.LC26:
	.string	"quoted"
.LC27:
	.string	"space_args"
.LC28:
	.string	"system"
.LC29:
	.string	"true"
.LC30:
	.string	"user"
.LC31:
	.string	"variable_names"
.LC32:
	.string	"write"
.LC33:
	.string	"write_canonical"
.LC36:
	.string	"write_option"
.LC34:
	.string	"write_term"
.LC35:
	.string	"writeq"
	.data	
	.align	4
	.local	at
	.comm	at,144,4
	.local	fn
	.comm	fn,48,4
	.local	st
	.comm	st,28,4
	.local	ta
	.comm	ta,12,4
	.section	.note.GNU-stack,"",@progbits
