# BipsPl/list.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/list.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6C697374__a0,@function
.globl X1_247573655F6C697374__a0

X1_247573655F6C697374__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_append__a3,@function
.globl X0_append__a3

X0_append__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Append_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24617070656E645F616C74__a0,@function
.globl X1_24617070656E645F616C74__a0

X1_24617070656E645F616C74__a0:
	call	Pl_Append_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_member__a2,@function
.globl X0_member__a2

X0_member__a2:
	call	Pl_Member_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D656D6265725F616C74__a0,@function
.globl X1_246D656D6265725F616C74__a0

X1_246D656D6265725F616C74__a0:
	call	Pl_Member_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_memberchk__a2,@function
.globl X0_memberchk__a2

X0_memberchk__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Memberchk_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_reverse__a2,@function
.globl X0_reverse__a2

X0_reverse__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Reverse_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726576657273655F616C74__a0,@function
.globl X1_24726576657273655F616C74__a0

X1_24726576657273655F616C74__a0:
	call	Pl_Reverse_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_delete__a3,@function
.globl X0_delete__a3

X0_delete__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred9_2+0, %eax
	movl	$.Lpred9_3+0, %edx
	movl	$.Lpred9_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred9_1:
	movl	$.Lpred9_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred9_5

.Lpred9_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred9_7

.Lpred9_2:
	movl	$.Lpred9_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred9_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred9_4:
	movl	$.Lpred9_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred9_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X0_delete__a3

.Lpred9_6:
	call	Pl_Delete_Choice_Point4

.Lpred9_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_delete__a3
	.p2align	4,,15
	.type	X0_select__a3,@function
.globl X0_select__a3

X0_select__a3:
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred10_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_select__a3
	.p2align	4,,15
	.type	X0_subtract__a3,@function
.globl X0_subtract__a3

X0_subtract__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred11_2+0, %eax
	movl	$.Lpred11_3+0, %edx
	movl	$.Lpred11_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred11_1:
	movl	$.Lpred11_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred11_5

.Lpred11_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred11_7

.Lpred11_2:
	movl	$.Lpred11_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred11_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_4:
	movl	$.Lpred11_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred11_5:
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
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X0_memberchk__a2
.Lcont1:
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
	jmp	X0_subtract__a3

.Lpred11_6:
	call	Pl_Delete_Choice_Point4

.Lpred11_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X0_subtract__a3
	.p2align	4,,15
	.type	X0_permutation__a2,@function
.globl X0_permutation__a2

X0_permutation__a2:
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point2
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_select__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X0_permutation__a2
	.p2align	4,,15
	.type	X0_prefix__a2,@function
.globl X0_prefix__a2

X0_prefix__a2:
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
	jmp	*pl_reg_bank+1040

.Lpred13_3:
	call	Pl_Delete_Choice_Point2

.Lpred13_4:
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
	jmp	X0_prefix__a2
	.p2align	4,,15
	.type	X0_suffix__a2,@function
.globl X0_suffix__a2

X0_suffix__a2:
	movl	$.Lpred14_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred14_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X0_suffix__a2
	.p2align	4,,15
	.type	X0_sublist__a2,@function
.globl X0_sublist__a2

X0_sublist__a2:
	movl	$.Lpred15_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred15_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_247375626C69737431__a3
	.p2align	4,,15
	.type	X1_247375626C69737431__a3,@function
.globl X1_247375626C69737431__a3

X1_247375626C69737431__a3:
	movl	$.Lpred16_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred16_1:
	call	Pl_Delete_Choice_Point3
	movl	$.Lpred16_3+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred16_2+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred16_2:
	movl	$.Lpred16_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred16_4

.Lpred16_sub_0:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred16_6

.Lpred16_3:
	movl	$.Lpred16_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred16_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_247375626C69737431__a3

.Lpred16_5:
	call	Pl_Delete_Choice_Point3

.Lpred16_6:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	jmp	X1_247375626C69737431__a3
	.p2align	4,,15
	.type	X0_last__a2,@function
.globl X0_last__a2

X0_last__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246C61737431__a3
	.p2align	4,,15
	.type	X1_246C61737431__a3,@function
.globl X1_246C61737431__a3

X1_246C61737431__a3:
	movl	$.Lpred18_1+0, %eax
	movl	$.Lpred18_2+0, %edx
	movl	$.Lpred18_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred18_1:
	movl	$.Lpred18_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred18_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred18_3:
	call	Pl_Delete_Choice_Point3

.Lpred18_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246C61737431__a3
	.p2align	4,,15
	.type	X0_length__a2,@function
.globl X0_length__a2

X0_length__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Length_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246C656E6774685F616C74__a0,@function
.globl X1_246C656E6774685F616C74__a0

X1_246C656E6774685F616C74__a0:
	call	Pl_Length_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nth__a3,@function
.globl X0_nth__a3

X0_nth__a3:
	jmp	X0_nth1__a3
	.p2align	4,,15
	.type	X0_nth1__a3,@function
.globl X0_nth1__a3

X0_nth1__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	$1, 12(%esp)
	call	Pl_Nth0_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_246E74685F67656E6572__a4
	.p2align	4,,15
	.type	X0_nth0__a3,@function
.globl X0_nth0__a3

X0_nth0__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred23_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	$0, 12(%esp)
	call	Pl_Nth0_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_246E74685F67656E6572__a4
	.p2align	4,,15
	.type	X1_246E74685F67656E6572__a4,@function
.globl X1_246E74685F67656E6572__a4

X1_246E74685F67656E6572__a4:
	movl	$.Lpred24_2+0, %eax
	movl	$0, %edx
	movl	$0, %ecx
	movl	$.Lpred24_1+0, 0(%esp)
	movl	$0, 4(%esp)
	call	Pl_Switch_On_Term
	jmp	*%eax

.Lpred24_1:
	movl	$.Lpred24_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred24_3

.Lpred24_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred24_5

.Lpred24_2:
	movl	$.Lpred24_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred24_3:
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred24_4:
	call	Pl_Delete_Choice_Point4

.Lpred24_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	at+104, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+8, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+8, %eax
	call	Pl_Fct_Inc
	movl	%eax, pl_reg_bank+8
	jmp	X1_246E74685F67656E6572__a4
	.p2align	4,,15
	.type	X0_max_list__a2,@function
.globl X0_max_list__a2

X0_max_list__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246D61785F6C69737431__a3
	.p2align	4,,15
	.type	X1_246D61785F6C69737431__a3,@function
.globl X1_246D61785F6C69737431__a3

X1_246D61785F6C69737431__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred26_2+0, %eax
	movl	$.Lpred26_3+0, %edx
	movl	$.Lpred26_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred26_1:
	movl	$.Lpred26_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred26_5

.Lpred26_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred26_7

.Lpred26_2:
	movl	$.Lpred26_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred26_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred26_4:
	movl	$.Lpred26_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred26_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	at+116, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+16, %eax
	leal	pl_reg_bank+16, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+20, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Lte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_246D61785F6C69737431__a3

.Lpred26_6:
	call	Pl_Delete_Choice_Point4

.Lpred26_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246D61785F6C69737431__a3
	.p2align	4,,15
	.type	X0_min_list__a2,@function
.globl X0_min_list__a2

X0_min_list__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246D696E5F6C69737431__a3
	.p2align	4,,15
	.type	X1_246D696E5F6C69737431__a3,@function
.globl X1_246D696E5F6C69737431__a3

X1_246D696E5F6C69737431__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred28_2+0, %eax
	movl	$.Lpred28_3+0, %edx
	movl	$.Lpred28_1+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred28_1:
	movl	$.Lpred28_sub_0+0, %eax
	call	Pl_Create_Choice_Point4
	jmp	.Lpred28_5

.Lpred28_sub_0:
	call	Pl_Delete_Choice_Point4
	jmp	.Lpred28_7

.Lpred28_2:
	movl	$.Lpred28_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred28_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred28_4:
	movl	$.Lpred28_6+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred28_5:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	at+128, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+16, %eax
	leal	pl_reg_bank+16, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+20, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Blt_Gte
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_246D696E5F6C69737431__a3

.Lpred28_6:
	call	Pl_Delete_Choice_Point4

.Lpred28_7:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	jmp	X1_246D696E5F6C69737431__a3
	.p2align	4,,15
	.type	X0_sum_list__a2,@function
.globl X0_sum_list__a2

X0_sum_list__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473756D5F6C69737431__a3
	.p2align	4,,15
	.type	X1_2473756D5F6C69737431__a3,@function
.globl X1_2473756D5F6C69737431__a3

X1_2473756D5F6C69737431__a3:
	movl	$.Lpred30_1+0, %eax
	movl	$.Lpred30_2+0, %edx
	movl	$.Lpred30_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred30_1:
	movl	$.Lpred30_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred30_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred30_3:
	call	Pl_Delete_Choice_Point3

.Lpred30_4:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	at+104, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+12, %eax
	leal	pl_reg_bank+12, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Fct_Add
	movl	%eax, pl_reg_bank+4
	jmp	X1_2473756D5F6C69737431__a3
	.p2align	4,,15
	.type	X0_flatten__a2,@function
.globl X0_flatten__a2

X0_flatten__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24666C617474656E__a3
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24666C617474656E__a3,@function
.globl X1_24666C617474656E__a3

X1_24666C617474656E__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred32_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
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
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred32_1:
	movl	$.Lpred32_6+0, %eax
	call	Pl_Update_Choice_Point4
	movl	$.Lpred32_2+0, %eax
	movl	$.Lpred32_3+0, %edx
	movl	$.Lpred32_5+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred32_2:
	movl	$.Lpred32_4+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred32_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred32_4:
	call	Pl_Delete_Choice_Point4

.Lpred32_5:
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
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24666C617474656E__a3
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_24666C617474656E__a3

.Lpred32_6:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
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
	.type	X0_maplist__a2,@function
.globl X0_maplist__a2

X0_maplist__a2:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_246D61706C697374__a2
	.p2align	4,,15
	.type	X1_246D61706C697374__a2,@function
.globl X1_246D61706C697374__a2

X1_246D61706C697374__a2:
	movl	$.Lpred34_1+0, %eax
	movl	$.Lpred34_2+0, %edx
	movl	$.Lpred34_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred34_1:
	movl	$.Lpred34_3+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred34_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred34_3:
	call	Pl_Delete_Choice_Point2

.Lpred34_4:
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
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X0_call__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a2
	.p2align	4,,15
	.type	X0_maplist__a3,@function
.globl X0_maplist__a3

X0_maplist__a3:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_246D61706C697374__a3
	.p2align	4,,15
	.type	X1_246D61706C697374__a3,@function
.globl X1_246D61706C697374__a3

X1_246D61706C697374__a3:
	movl	$.Lpred36_1+0, %eax
	movl	$.Lpred36_2+0, %edx
	movl	$.Lpred36_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred36_1:
	movl	$.Lpred36_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred36_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred36_3:
	call	Pl_Delete_Choice_Point3

.Lpred36_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X0_call__a3
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a3
	.p2align	4,,15
	.type	X0_maplist__a4,@function
.globl X0_maplist__a4

X0_maplist__a4:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_246D61706C697374__a4
	.p2align	4,,15
	.type	X1_246D61706C697374__a4,@function
.globl X1_246D61706C697374__a4

X1_246D61706C697374__a4:
	movl	$.Lpred38_1+0, %eax
	movl	$.Lpred38_2+0, %edx
	movl	$.Lpred38_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred38_1:
	movl	$.Lpred38_3+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred38_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred38_3:
	call	Pl_Delete_Choice_Point4

.Lpred38_4:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X0_call__a4
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a4
	.p2align	4,,15
	.type	X0_maplist__a5,@function
.globl X0_maplist__a5

X0_maplist__a5:
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X1_246D61706C697374__a5
	.p2align	4,,15
	.type	X1_246D61706C697374__a5,@function
.globl X1_246D61706C697374__a5

X1_246D61706C697374__a5:
	movl	$.Lpred40_1+0, %eax
	movl	$.Lpred40_2+0, %edx
	movl	$.Lpred40_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred40_1:
	movl	$.Lpred40_3+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred40_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred40_3:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred40_4:
	movl	$5, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+16
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X0_call__a5
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
	jmp	X1_246D61706C697374__a5
	.p2align	4,,15
	.type	X0_maplist__a6,@function
.globl X0_maplist__a6

X0_maplist__a6:
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X1_246D61706C697374__a6
	.p2align	4,,15
	.type	X1_246D61706C697374__a6,@function
.globl X1_246D61706C697374__a6

X1_246D61706C697374__a6:
	movl	$.Lpred42_1+0, %eax
	movl	$.Lpred42_2+0, %edx
	movl	$.Lpred42_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred42_1:
	movl	$.Lpred42_3+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point

.Lpred42_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred42_3:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point

.Lpred42_4:
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+20, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+20
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X0_call__a6
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
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+16
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a6
	.p2align	4,,15
	.type	X0_maplist__a7,@function
.globl X0_maplist__a7

X0_maplist__a7:
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+28
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+20
	jmp	X1_246D61706C697374__a7
	.p2align	4,,15
	.type	X1_246D61706C697374__a7,@function
.globl X1_246D61706C697374__a7

X1_246D61706C697374__a7:
	movl	$.Lpred44_1+0, %eax
	movl	$.Lpred44_2+0, %edx
	movl	$.Lpred44_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred44_1:
	movl	$.Lpred44_3+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred44_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred44_3:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred44_4:
	movl	$7, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+24, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+40
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+36
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+24
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+40, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X0_call__a7
.Lcont10:
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
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a7
	.p2align	4,,15
	.type	X0_maplist__a8,@function
.globl X0_maplist__a8

X0_maplist__a8:
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+28
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+24
	jmp	X1_246D61706C697374__a8
	.p2align	4,,15
	.type	X1_246D61706C697374__a8,@function
.globl X1_246D61706C697374__a8

X1_246D61706C697374__a8:
	movl	$.Lpred46_1+0, %eax
	movl	$.Lpred46_2+0, %edx
	movl	$.Lpred46_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred46_1:
	movl	$.Lpred46_3+0, %eax
	movl	$8, %edx
	call	Pl_Create_Choice_Point

.Lpred46_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred46_3:
	movl	$8, %eax
	call	Pl_Delete_Choice_Point

.Lpred46_4:
	movl	$8, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+28, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -44(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+48
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+44
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+40
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+36
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+28
	call	Pl_Unify_Variable
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+48, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+44, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+40, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+24
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X0_call__a8
.Lcont11:
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
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+28
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a8
	.p2align	4,,15
	.type	X0_maplist__a9,@function
.globl X0_maplist__a9

X0_maplist__a9:
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+36
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+28
	jmp	X1_246D61706C697374__a9
	.p2align	4,,15
	.type	X1_246D61706C697374__a9,@function
.globl X1_246D61706C697374__a9

X1_246D61706C697374__a9:
	movl	$.Lpred48_1+0, %eax
	movl	$.Lpred48_2+0, %edx
	movl	$.Lpred48_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred48_1:
	movl	$.Lpred48_3+0, %eax
	movl	$9, %edx
	call	Pl_Create_Choice_Point

.Lpred48_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+28, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred48_3:
	movl	$9, %eax
	call	Pl_Delete_Choice_Point

.Lpred48_4:
	movl	$9, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+32, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -48(%edi)
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+56
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+52
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+48
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+44
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	pl_reg_bank+16, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+40
	call	Pl_Unify_Variable
	movl	%eax, -32(%edi)
	movl	pl_reg_bank+20, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+36
	call	Pl_Unify_Variable
	movl	%eax, -36(%edi)
	movl	pl_reg_bank+24, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -40(%edi)
	movl	pl_reg_bank+28, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+32
	call	Pl_Unify_Variable
	movl	%eax, -44(%edi)
	movl	pl_reg_bank+56, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+52, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+48, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+44, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+40, %eax
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+28
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X0_call__a9
.Lcont12:
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
	movl	%eax, pl_reg_bank+20
	movl	-40(%edi), %eax
	movl	%eax, pl_reg_bank+24
	movl	-44(%edi), %eax
	movl	%eax, pl_reg_bank+28
	movl	-48(%edi), %eax
	movl	%eax, pl_reg_bank+32
	call	Pl_Deallocate
	jmp	X1_246D61706C697374__a9
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
	movl	%eax, at+16
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6C697374__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_append__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24617070656E645F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_member__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$70, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D656D6265725F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$84, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_memberchk__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$103, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_reverse__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726576657273655F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$112, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_delete__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$125, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_select__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$134, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_subtract__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$142, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_permutation__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$152, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_prefix__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$160, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_suffix__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$168, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sublist__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$174, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247375626C69737431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$185, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_last__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$188, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C61737431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$223, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_length__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$227, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246C656E6774685F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$261, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nth__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$264, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nth1__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+96, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$273, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nth0__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+100, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$283, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246E74685F67656E6572__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+108, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$293, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_max_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+112, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$296, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61785F6C69737431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$308, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_min_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$311, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D696E5F6C69737431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$324, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sum_list__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$327, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2473756D5F6C69737431__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$335, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_flatten__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+144, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$339, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24666C617474656E__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$354, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$357, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$366, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$369, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$378, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$381, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$390, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$393, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$402, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$405, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$414, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$417, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$426, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$8, %edx
	movl	at+0, %ecx
	movl	$429, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a8+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+148, %eax
	movl	$9, %edx
	movl	at+0, %ecx
	movl	$438, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_maplist__a9+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+152, %eax
	movl	$9, %edx
	movl	at+0, %ecx
	movl	$441, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D61706C697374__a9+0, 8(%esp)
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
	.string	"$append_alt"
.LC1:
	.string	"$flatten"
.LC2:
	.string	"$last1"
.LC3:
	.string	"$length_alt"
.LC4:
	.string	"$maplist"
.LC5:
	.string	"$max_list1"
.LC6:
	.string	"$member_alt"
.LC7:
	.string	"$min_list1"
.LC8:
	.string	"$nth_gener"
.LC9:
	.string	"$reverse_alt"
.LC10:
	.string	"$sublist1"
.LC11:
	.string	"$sum_list1"
.LC12:
	.string	"$use_list"
.LC13:
	.string	"/home/diaz/GP/src/BipsPl/list.pl"
.LC14:
	.string	"=<"
.LC15:
	.string	">="
.LC16:
	.string	"append"
.LC17:
	.string	"delete"
.LC18:
	.string	"flatten"
.LC19:
	.string	"is"
.LC20:
	.string	"last"
.LC21:
	.string	"length"
.LC22:
	.string	"maplist"
.LC23:
	.string	"max_list"
.LC24:
	.string	"member"
.LC25:
	.string	"memberchk"
.LC26:
	.string	"min_list"
.LC27:
	.string	"nth"
.LC28:
	.string	"nth0"
.LC29:
	.string	"nth1"
.LC30:
	.string	"permutation"
.LC31:
	.string	"prefix"
.LC32:
	.string	"reverse"
.LC33:
	.string	"select"
.LC34:
	.string	"sublist"
.LC35:
	.string	"subtract"
.LC36:
	.string	"suffix"
.LC37:
	.string	"sum_list"
.LC38:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,156,4
	.section	.note.GNU-stack,"",@progbits
