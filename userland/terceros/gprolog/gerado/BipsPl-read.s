# BipsPl/read.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/read.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F72656164__a0,@function
.globl X1_247573655F72656164__a0

X1_247573655F72656164__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read__a1,@function
.globl X0_read__a1

X0_read__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read__a2,@function
.globl X0_read__a2

X0_read__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
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
	call	Pl_Read_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_term__a2,@function
.globl X0_read_term__a2

X0_read_term__a2:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
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
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E73__a4
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Read_Term_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_term__a3,@function
.globl X0_read_term__a3

X0_read_term__a3:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_24726561645F7465726D__a3
	.p2align	4,,15
	.type	X1_24726561645F7465726D__a3,@function
.globl X1_24726561645F7465726D__a3

X1_24726561645F7465726D__a3:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
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
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E73__a4
.Lcont6:
	movl	pl_reg_bank+1044, %edi
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
	call	Pl_Read_Term_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F726561645F64656661756C7473__a0,@function
.globl X1_247365745F726561645F64656661756C7473__a0

X1_247365745F726561645F64656661756C7473__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont7:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-1, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X1_246765745F726561645F6F7074696F6E73__a4,@function
.globl X1_246765745F726561645F6F7074696F6E73__a4

X1_246765745F726561645F6F7074696F6E73__a4:
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
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_24636865636B5F6C697374__a1
.Lcont8:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E7331__a1
.Lcont9:
	movl	ta+0, %eax
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
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F726561645F6F7074696F6E7331__a1,@function
.globl X1_246765745F726561645F6F7074696F6E7331__a1

X1_246765745F726561645F6F7074696F6E7331__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred9_1+0, %eax
	movl	$.Lpred9_2+0, %edx
	movl	$.Lpred9_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred9_1:
	movl	$.Lpred9_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred9_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred9_3:
	call	Pl_Delete_Choice_Point2

.Lpred9_4:
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
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_246765745F726561645F6F7074696F6E7332__a1
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_246765745F726561645F6F7074696F6E7331__a1
	.p2align	4,,15
	.type	X1_246765745F726561645F6F7074696F6E7332__a1,@function
.globl X1_246765745F726561645F6F7074696F6E7332__a1

X1_246765745F726561645F6F7074696F6E7332__a1:
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred10_1:
	movl	$.Lpred10_13+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred10_3+0, %eax
	movl	$.Lpred10_2+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred10_2:
	movl	st+0, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred10_3:
	movl	$.Lpred10_5+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred10_4:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred10_5:
	movl	$.Lpred10_7+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred10_6:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred10_7:
	movl	$.Lpred10_9+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred10_8:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Link
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7365745F626974__a2

.Lpred10_9:
	movl	$.Lpred10_11+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred10_10:
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
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F726561645F6F7074696F6E73322F315F2461757831__a1

.Lpred10_11:
	call	Pl_Delete_Choice_Point1

.Lpred10_12:
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
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_24636865636B5F6E6F6E766172__a1
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24246765745F726561645F6F7074696F6E73322F315F2461757832__a1

.Lpred10_13:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_24246765745F726561645F6F7074696F6E73322F315F2461757832__a1,@function

X1_24246765745F726561645F6F7074696F6E73322F315F2461757832__a1:
	movl	$.Lpred11_2+0, %eax
	movl	$.Lpred11_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred11_1:
	movl	st+4, %eax
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
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F72657365745F626974__a2

.Lpred11_4:
	call	Pl_Delete_Choice_Point1

.Lpred11_5:
	movl	ta+20, %eax
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
	.type	X1_24246765745F726561645F6F7074696F6E73322F315F2461757831__a1,@function

X1_24246765745F726561645F6F7074696F6E73322F315F2461757831__a1:
	movl	$.Lpred12_2+0, %eax
	movl	$.Lpred12_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred12_1:
	movl	st+8, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred12_2:
	movl	$.Lpred12_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred12_3:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred12_4:
	movl	$.Lpred12_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred12_5:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred12_6:
	call	Pl_Delete_Choice_Point1

.Lpred12_7:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_247379735F7661725F7772697465__a2
	.p2align	4,,15
	.type	X0_read_atom__a1,@function
.globl X0_read_atom__a1

X0_read_atom__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+92, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_Atom_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_atom__a2,@function
.globl X0_read_atom__a2

X0_read_atom__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+92, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont14:
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
	call	Pl_Read_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_integer__a1,@function
.globl X0_read_integer__a1

X0_read_integer__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+96, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont15:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_Integer_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_integer__a2,@function
.globl X0_read_integer__a2

X0_read_integer__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+96, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont16:
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
	call	Pl_Read_Integer_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_number__a1,@function
.globl X0_read_number__a1

X0_read_number__a1:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	at+100, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont17:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_Number_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_number__a2,@function
.globl X0_read_number__a2

X0_read_number__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+100, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_247365745F726561645F64656661756C7473__a0
.Lcont18:
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
	call	Pl_Read_Number_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_token__a1,@function
.globl X0_read_token__a1

X0_read_token__a1:
	movl	at+104, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_Token_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_token__a2,@function
.globl X0_read_token__a2

X0_read_token__a2:
	movl	at+104, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Read_Token_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_last_read_start_line_column__a2,@function
.globl X0_last_read_start_line_column__a2

X0_last_read_start_line_column__a2:
	movl	at+108, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Last_Read_Start_Line_Column_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_char_conversion__a2,@function
.globl X0_char_conversion__a2

X0_char_conversion__a2:
	movl	at+112, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Char_Conversion_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_char_conversion__a2,@function
.globl X0_current_char_conversion__a2

X0_current_char_conversion__a2:
	movl	at+116, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Char_Conversion_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F636861725F636F6E76657273696F6E5F616C74__a0,@function
.globl X1_2463757272656E745F636861725F636F6E76657273696F6E5F616C74__a0

X1_2463757272656E745F636861725F636F6E76657273696F6E5F616C74__a0:
	call	Pl_Current_Char_Conversion_Alt_0
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
	movl	%eax, at+76
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC31, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC32, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC33, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+68, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+80, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+88, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	$.LC34, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	at+84, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC13, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC24, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC28, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC29, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F72656164__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$76, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_term__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$87, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726561645F7465726D__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F726561645F64656661756C7473__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$102, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F726561645F6F7074696F6E73__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$113, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F726561645F6F7074696F6E7331__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$120, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F726561645F6F7074696F6E7332__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+40, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred10_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+44, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred10_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+48, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred10_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+52, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred10_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+56, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred10_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$149, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F726561645F6F7074696F6E73322F315F2461757832__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+68, %ecx
	movl	$.Lpred11_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+72, %ecx
	movl	$.Lpred11_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$139, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246765745F726561645F6F7074696F6E73322F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+80, %ecx
	movl	$.Lpred12_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+84, %ecx
	movl	$.Lpred12_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$3, %edx
	movl	at+88, %ecx
	movl	$.Lpred12_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+92, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$163, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_atom__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$169, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$177, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_integer__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$183, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_integer__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$191, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_number__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$197, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_number__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$205, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_token__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+104, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$209, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_token__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$215, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_last_read_start_line_column__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$222, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_char_conversion__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+116, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$229, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_char_conversion__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$236, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F636861725F636F6E76657273696F6E5F616C74__a0+0, 8(%esp)
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
	.string	"$$get_read_options2/1_$aux1"
.LC1:
	.string	"$$get_read_options2/1_$aux2"
.LC2:
	.string	"$current_char_conversion_alt"
.LC3:
	.string	"$get_read_options"
.LC4:
	.string	"$get_read_options1"
.LC5:
	.string	"$get_read_options2"
.LC31:
	.string	"$read_singletons"
.LC6:
	.string	"$read_term"
.LC32:
	.string	"$read_variable_names"
.LC33:
	.string	"$read_variables"
.LC7:
	.string	"$set_read_defaults"
.LC8:
	.string	"$use_read"
.LC9:
	.string	"/home/diaz/GP/src/BipsPl/read.pl"
.LC10:
	.string	"char_conversion"
.LC11:
	.string	"current_char_conversion"
.LC12:
	.string	"dot"
.LC13:
	.string	"end_of_term"
.LC14:
	.string	"eof"
.LC15:
	.string	"error"
.LC16:
	.string	"fail"
.LC17:
	.string	"last_read_start_line_column"
.LC18:
	.string	"read"
.LC19:
	.string	"read_atom"
.LC20:
	.string	"read_integer"
.LC21:
	.string	"read_number"
.LC34:
	.string	"read_option"
.LC22:
	.string	"read_term"
.LC23:
	.string	"read_token"
.LC24:
	.string	"singletons"
.LC25:
	.string	"syntax_error"
.LC26:
	.string	"system"
.LC27:
	.string	"user"
.LC28:
	.string	"variable_names"
.LC29:
	.string	"variables"
.LC30:
	.string	"warning"
	.data	
	.align	4
	.local	at
	.comm	at,124,4
	.local	fn
	.comm	fn,20,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,36,4
	.section	.note.GNU-stack,"",@progbits
