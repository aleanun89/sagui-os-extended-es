# BipsPl/pred.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/pred.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F70726564__a0,@function
.globl X1_247573655F70726564__a0

X1_247573655F70726564__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_predicate__a1,@function
.globl X0_current_predicate__a1

X0_current_predicate__a1:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_2463757272656E745F707265646963617465__a1
	.p2align	4,,15
	.type	X1_2463757272656E745F707265646963617465__a1,@function
.globl X1_2463757272656E745F707265646963617465__a1

X1_2463757272656E745F707265646963617465__a1:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Predicate_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F7072656469636174655F62697073__a1,@function
.globl X1_2463757272656E745F7072656469636174655F62697073__a1

X1_2463757272656E745F7072656469636174655F62697073__a1:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Predicate_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F7072656469636174655F616E79__a1,@function
.globl X1_2463757272656E745F7072656469636174655F616E79__a1

X1_2463757272656E745F7072656469636174655F616E79__a1:
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Predicate_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F7072656469636174655F616C74__a0,@function
.globl X1_2463757272656E745F7072656469636174655F616C74__a0

X1_2463757272656E745F7072656469636174655F616C74__a0:
	call	Pl_Current_Predicate_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_predicate_property__a2,@function
.globl X0_predicate_property__a2

X0_predicate_property__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+16
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+16, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+20
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	pl_reg_bank+8, %edx
	movl	pl_reg_bank+20, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_247072656469636174655F70726F706572747931__a3

.Lpred7_1:
	movl	$.Lpred7_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -24(%edi)
	call	Pl_Unify_Variable
	movl	%eax, -28(%edi)
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F62697073__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_247072656469636174655F70726F706572747931__a3

.Lpred7_2:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_247072656469636174655F70726F70657274795F7069__a2,@function
.globl X1_247072656469636174655F70726F70657274795F7069__a2

X1_247072656469636174655F70726F70657274795F7069__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F62697073__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_247072656469636174655F70726F706572747931__a3
	.p2align	4,,15
	.type	X1_247072656469636174655F70726F70657274795F70695F616E79__a2,@function
.globl X1_247072656469636174655F70726F70657274795F70695F616E79__a2

X1_247072656469636174655F70726F70657274795F70695F616E79__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2463757272656E745F7072656469636174655F616E79__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_247072656469636174655F70726F706572747931__a3
	.p2align	4,,15
	.type	X1_247072656469636174655F70726F706572747931__a3,@function
.globl X1_247072656469636174655F70726F706572747931__a3

X1_247072656469636174655F70726F706572747931__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
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
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24636865636B5F707265645F70726F70__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_247072656469636174655F70726F706572747932__a3
	.p2align	4,,15
	.type	X1_24636865636B5F707265645F70726F70__a1,@function
.globl X1_24636865636B5F707265645F70726F70__a1

X1_24636865636B5F707265645F70726F70__a1:
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_1:
	movl	$.Lpred11_32+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred11_4+0, %eax
	movl	$.Lpred11_2+0, %edx
	movl	$.Lpred11_3+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred11_2:
	movl	st+0, %eax
	movl	$11, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred11_3:
	movl	st+4, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred11_4:
	movl	$.Lpred11_6+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred11_5:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_6:
	movl	$.Lpred11_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_7:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_8:
	movl	$.Lpred11_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_9:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_10:
	movl	$.Lpred11_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_11:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_12:
	movl	$.Lpred11_14+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_13:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_14:
	movl	$.Lpred11_16+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_15:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_16:
	movl	$.Lpred11_18+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_17:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_18:
	movl	$.Lpred11_20+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_19:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_20:
	movl	$.Lpred11_22+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_21:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_22:
	movl	$.Lpred11_24+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_23:
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_24:
	movl	$.Lpred11_26+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_25:
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred11_26:
	movl	$.Lpred11_28+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_27:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred11_28:
	movl	$.Lpred11_30+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred11_29:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred11_30:
	call	Pl_Delete_Choice_Point1

.Lpred11_31:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred11_32:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+48, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_247072656469636174655F70726F706572747932__a3,@function
.globl X1_247072656469636174655F70726F706572747932__a3

X1_247072656469636174655F70726F706572747932__a3:
	movl	$.Lpred12_3+0, %eax
	movl	$.Lpred12_1+0, %edx
	movl	$.Lpred12_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred12_1:
	movl	st+8, %eax
	movl	$11, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred12_2:
	movl	st+12, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred12_3:
	movl	$.Lpred12_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred12_4:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Static_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_5:
	movl	$.Lpred12_7+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_6:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Dynamic_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_7:
	movl	$.Lpred12_9+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_8:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Private_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_9:
	movl	$.Lpred12_11+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_10:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Public_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_11:
	movl	$.Lpred12_13+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_12:
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Monofile_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_13:
	movl	$.Lpred12_15+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_14:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Multifile_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_15:
	movl	$.Lpred12_17+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_16:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_User_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_17:
	movl	$.Lpred12_19+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_18:
	movl	ta+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Built_In_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_19:
	movl	$.Lpred12_21+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_20:
	movl	ta+36, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Built_In_Fd_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_21:
	movl	$.Lpred12_23+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_22:
	movl	ta+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Control_Construct_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_23:
	movl	$.Lpred12_25+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_24:
	movl	ta+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	call	Pl_Pred_Prop_Native_Code_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_25:
	movl	$.Lpred12_27+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_26:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 8(%esp)
	call	Pl_Pred_Prop_Prolog_File_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_27:
	movl	$.Lpred12_29+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred12_28:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 8(%esp)
	call	Pl_Pred_Prop_Prolog_Line_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred12_29:
	call	Pl_Delete_Choice_Point3

.Lpred12_30:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+4
	jmp	X1_2470726F705F6D6574615F70726564__a3
	.p2align	4,,15
	.type	X1_2470726F705F6D6574615F70726564__a3,@function
.globl X1_2470726F705F6D6574615F70726564__a3

X1_2470726F705F6D6574615F70726564__a3:
	movl	$.Lpred13_6+0, %eax
	movl	$.Lpred13_1+0, %edx
	call	Pl_Switch_On_Term_Var_Atm
	jmp	*%eax

.Lpred13_1:
	movl	st+16, %eax
	movl	$31, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred13_2:
	movl	$.Lpred13_sub_0+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred13_15

.Lpred13_sub_0:
	movl	$.Lpred13_sub_1+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_29

.Lpred13_sub_1:
	movl	$.Lpred13_sub_2+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_31

.Lpred13_sub_2:
	movl	$.Lpred13_sub_3+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_33

.Lpred13_sub_3:
	movl	$.Lpred13_sub_4+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_35

.Lpred13_sub_4:
	movl	$.Lpred13_sub_5+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_37

.Lpred13_sub_5:
	movl	$.Lpred13_sub_6+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_39

.Lpred13_sub_6:
	movl	$.Lpred13_sub_7+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_41

.Lpred13_sub_7:
	movl	$.Lpred13_sub_8+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_43

.Lpred13_sub_8:
	movl	$.Lpred13_sub_9+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_45

.Lpred13_sub_9:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred13_47

.Lpred13_3:
	movl	$.Lpred13_sub_10+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred13_51

.Lpred13_sub_10:
	movl	$.Lpred13_sub_11+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_53

.Lpred13_sub_11:
	movl	$.Lpred13_sub_12+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_55

.Lpred13_sub_12:
	movl	$.Lpred13_sub_13+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_57

.Lpred13_sub_13:
	movl	$.Lpred13_sub_14+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_59

.Lpred13_sub_14:
	movl	$.Lpred13_sub_15+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_61

.Lpred13_sub_15:
	movl	$.Lpred13_sub_16+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_63

.Lpred13_sub_16:
	movl	$.Lpred13_sub_17+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_65

.Lpred13_sub_17:
	movl	$.Lpred13_sub_18+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_67

.Lpred13_sub_18:
	movl	$.Lpred13_sub_19+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_69

.Lpred13_sub_19:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred13_71

.Lpred13_4:
	movl	$.Lpred13_sub_20+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred13_85

.Lpred13_sub_20:
	movl	$.Lpred13_sub_21+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_87

.Lpred13_sub_21:
	movl	$.Lpred13_sub_22+0, %eax
	call	Pl_Update_Choice_Point3
	jmp	.Lpred13_89

.Lpred13_sub_22:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred13_91

.Lpred13_5:
	movl	$.Lpred13_sub_23+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	.Lpred13_99

.Lpred13_sub_23:
	call	Pl_Delete_Choice_Point3
	jmp	.Lpred13_101

.Lpred13_6:
	movl	$.Lpred13_8+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred13_7:
	movl	ta+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+16, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_8:
	movl	$.Lpred13_10+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_9:
	movl	ta+56, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+20, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_10:
	movl	$.Lpred13_12+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_11:
	movl	ta+60, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+24, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_12:
	movl	$.Lpred13_14+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_13:
	movl	ta+64, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_14:
	movl	$.Lpred13_16+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_15:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+32, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_16:
	movl	$.Lpred13_18+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_17:
	movl	ta+72, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_18:
	movl	$.Lpred13_20+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_19:
	movl	ta+80, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+40, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_20:
	movl	$.Lpred13_22+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_21:
	movl	ta+84, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+44, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_22:
	movl	$.Lpred13_24+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_23:
	movl	ta+92, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+48, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_24:
	movl	$.Lpred13_26+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_25:
	movl	ta+96, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_26:
	movl	$.Lpred13_28+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_27:
	movl	ta+100, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+56, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+104, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_28:
	movl	$.Lpred13_30+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_29:
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
	movl	fn+60, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_30:
	movl	$.Lpred13_32+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_31:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+64, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_32:
	movl	$.Lpred13_34+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_33:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+68, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_34:
	movl	$.Lpred13_36+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_35:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+72, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_36:
	movl	$.Lpred13_38+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_37:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483621, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+76, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_38:
	movl	$.Lpred13_40+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_39:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483617, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+80, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483621, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_40:
	movl	$.Lpred13_42+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_41:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483613, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+84, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483617, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_42:
	movl	$.Lpred13_44+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_43:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483609, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+88, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483613, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_44:
	movl	$.Lpred13_46+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_45:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483605, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+92, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483609, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_46:
	movl	$.Lpred13_48+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_47:
	movl	ta+68, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483601, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+96, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483605, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_48:
	movl	$.Lpred13_50+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_49:
	movl	ta+108, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+100, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_50:
	movl	$.Lpred13_52+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_51:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+104, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_52:
	movl	$.Lpred13_54+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_53:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+108, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_54:
	movl	$.Lpred13_56+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_55:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+112, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_56:
	movl	$.Lpred13_58+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_57:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+116, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_58:
	movl	$.Lpred13_60+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_59:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+120, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_60:
	movl	$.Lpred13_62+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_61:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483621, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+124, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_62:
	movl	$.Lpred13_64+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_63:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483617, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+128, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483621, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_64:
	movl	$.Lpred13_66+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_65:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483613, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+132, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483617, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_66:
	movl	$.Lpred13_68+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_67:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483609, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+136, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483613, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_68:
	movl	$.Lpred13_70+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_69:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483605, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+140, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483609, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_70:
	movl	$.Lpred13_72+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_71:
	movl	ta+112, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483601, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+144, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483605, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_72:
	movl	$.Lpred13_74+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_73:
	movl	ta+116, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+148, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_74:
	movl	$.Lpred13_76+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_75:
	movl	ta+120, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+152, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_76:
	movl	$.Lpred13_78+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_77:
	movl	ta+124, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+128, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_78:
	movl	$.Lpred13_80+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_79:
	movl	ta+132, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+156, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_80:
	movl	$.Lpred13_82+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_81:
	movl	ta+136, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+160, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+104, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_82:
	movl	$.Lpred13_84+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_83:
	movl	ta+140, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+164, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_84:
	movl	$.Lpred13_86+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_85:
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+168, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_86:
	movl	$.Lpred13_88+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_87:
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+172, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_88:
	movl	$.Lpred13_90+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_89:
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+176, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_90:
	movl	$.Lpred13_92+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_91:
	movl	ta+144, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483625, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+180, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483629, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_92:
	movl	$.Lpred13_94+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_93:
	movl	ta+148, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+184, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_94:
	movl	$.Lpred13_96+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_95:
	movl	ta+152, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+188, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_96:
	movl	$.Lpred13_98+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_97:
	movl	ta+156, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+192, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_98:
	movl	$.Lpred13_100+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_99:
	movl	ta+160, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+196, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_100:
	movl	$.Lpred13_102+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_101:
	movl	ta+160, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+200, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_102:
	movl	$.Lpred13_104+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_103:
	movl	ta+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+204, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_104:
	movl	$.Lpred13_106+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_105:
	movl	ta+164, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+208, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_106:
	movl	$.Lpred13_108+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_107:
	movl	ta+168, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+212, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_108:
	movl	$.Lpred13_110+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_109:
	movl	ta+172, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483633, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+216, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+104, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_110:
	movl	$.Lpred13_112+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_111:
	movl	ta+176, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+220, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+88, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_112:
	movl	$.Lpred13_114+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred13_113:
	movl	ta+180, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+224, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred13_114:
	call	Pl_Delete_Choice_Point3

.Lpred13_115:
	movl	ta+184, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+228, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+76, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F707265645F696E64696361746F72__a3,@function
.globl X1_246765745F707265645F696E64696361746F72__a3

X1_246765745F707265645F696E64696361746F72__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Get_Pred_Indicator_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F7072656469636174655F66696C655F696E666F__a3,@function
.globl X1_246765745F7072656469636174655F66696C655F696E666F__a3

X1_246765745F7072656469636174655F66696C655F696E666F__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Get_Predicate_File_Info_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F7072656469636174655F66696C655F696E666F__a3,@function
.globl X1_247365745F7072656469636174655F66696C655F696E666F__a3

X1_247365745F7072656469636174655F66696C655F696E666F__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Set_Predicate_File_Info_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246175785F6E616D65__a1,@function
.globl X1_246175785F6E616D65__a1

X1_246175785F6E616D65__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Aux_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246E6F745F6175785F6E616D65__a1,@function
.globl X1_246E6F745F6175785F6E616D65__a1

X1_246E6F745F6175785F6E616D65__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Not_Aux_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246661746865725F6F665F6175785F6E616D65__a3,@function
.globl X1_246661746865725F6F665F6175785F6E616D65__a3

X1_246661746865725F6F665F6175785F6E616D65__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Father_Of_Aux_Name_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24707265645F776974686F75745F617578__a4,@function
.globl X1_24707265645F776974686F75745F617578__a4

X1_24707265645F776974686F75745F617578__a4:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Pred_Without_Aux_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246D616B655F6175785F6E616D65__a4,@function
.globl X1_246D616B655F6175785F6E616D65__a4

X1_246D616B655F6175785F6E616D65__a4:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Make_Aux_Name_4
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
	movl	%eax, at+244
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+252
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+232
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+236
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+260
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+248
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+256
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+240
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+176
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+144
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+148
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+152
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+156
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+160
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+164
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC36, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+168
	movl	$.LC37, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+172
	movl	$.LC38, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC39, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC40, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC41, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+228
	movl	$.LC42, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+224
	movl	$.LC43, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+180
	movl	$.LC44, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+184
	movl	$.LC45, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+196
	movl	$.LC46, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+188
	movl	$.LC47, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC48, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC49, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC50, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC51, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+192
	movl	$.LC52, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+200
	movl	$.LC53, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+204
	movl	$.LC54, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC55, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC56, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC57, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC58, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC59, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+208
	movl	$.LC60, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+212
	movl	$.LC61, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+216
	movl	$.LC62, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+220
	movl	$.LC63, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC64, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC65, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	at+128, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+64
	movl	$.LC66, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+128
	movl	at+116, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+52
	movl	$.LC67, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+104
	movl	at+124, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+60
	movl	at+176, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+124
	movl	$.LC68, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+88
	movl	at+120, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+56
	movl	$.LC69, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+76
	movl	at+140, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+80
	movl	at+144, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+84
	movl	at+148, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+92
	movl	at+152, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+96
	movl	at+156, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+100
	movl	at+80, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	at+84, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+36
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+68
	movl	at+160, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+108
	movl	at+164, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+112
	movl	$.LC70, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+136, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+72
	movl	at+168, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+116
	movl	at+172, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+120
	movl	at+88, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+40
	movl	at+12, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+132
	movl	at+56, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+228, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+184
	movl	at+224, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+180
	movl	at+180, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+136
	movl	at+184, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+140
	movl	at+196, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+152
	movl	at+188, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+144
	movl	at+68, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+92, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+44
	movl	at+192, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+148
	movl	at+200, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+156
	movl	at+204, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+160
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+48
	movl	at+60, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	at+64, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+208, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+164
	movl	at+212, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+168
	movl	at+216, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+172
	movl	at+220, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+176
	movl	at+52, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+76, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC21, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC71, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC24, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC26, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC27, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC28, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC29, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC32, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC32, %eax
	movl	$10, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+92
	movl	$.LC32, %eax
	movl	$11, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+96
	movl	$.LC32, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+60
	movl	$.LC32, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+64
	movl	$.LC32, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+68
	movl	$.LC32, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+72
	movl	$.LC32, %eax
	movl	$6, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+76
	movl	$.LC32, %eax
	movl	$7, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+80
	movl	$.LC32, %eax
	movl	$8, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+84
	movl	$.LC32, %eax
	movl	$9, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+88
	movl	$.LC33, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+100
	movl	$.LC34, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+104
	movl	$.LC34, %eax
	movl	$10, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+140
	movl	$.LC34, %eax
	movl	$11, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+144
	movl	$.LC34, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+108
	movl	$.LC34, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+112
	movl	$.LC34, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+116
	movl	$.LC34, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+120
	movl	$.LC34, %eax
	movl	$6, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+124
	movl	$.LC34, %eax
	movl	$7, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+128
	movl	$.LC34, %eax
	movl	$8, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+132
	movl	$.LC34, %eax
	movl	$9, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+136
	movl	$.LC35, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC36, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+148
	movl	$.LC37, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+152
	movl	$.LC39, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+156
	movl	$.LC41, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+228
	movl	$.LC42, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+224
	movl	$.LC43, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+160
	movl	$.LC44, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+164
	movl	$.LC45, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+188
	movl	$.LC46, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+168
	movl	$.LC46, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+172
	movl	$.LC46, %eax
	movl	$4, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+176
	movl	$.LC46, %eax
	movl	$5, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+180
	movl	$.LC47, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC51, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+184
	movl	$.LC52, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+192
	movl	$.LC53, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+196
	movl	$.LC53, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+200
	movl	$.LC54, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+204
	movl	$.LC56, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC57, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC59, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+208
	movl	$.LC60, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+212
	movl	$.LC61, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+216
	movl	$.LC62, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+220
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F70726564__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_predicate__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$48, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F707265646963617465__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F7072656469636174655F62697073__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F7072656469636174655F616E79__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F7072656469636174655F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_predicate_property__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$90, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247072656469636174655F70726F70657274795F7069__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$96, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247072656469636174655F70726F70657274795F70695F616E79__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$104, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247072656469636174655F70726F706572747931__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$111, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F707265645F70726F70__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$11, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+52, %ecx
	movl	$.Lpred11_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+56, %ecx
	movl	$.Lpred11_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+60, %ecx
	movl	$.Lpred11_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+64, %ecx
	movl	$.Lpred11_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+68, %ecx
	movl	$.Lpred11_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+72, %ecx
	movl	$.Lpred11_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+76, %ecx
	movl	$.Lpred11_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+80, %ecx
	movl	$.Lpred11_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+84, %ecx
	movl	$.Lpred11_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+88, %ecx
	movl	$.Lpred11_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$11, %edx
	movl	at+92, %ecx
	movl	$.Lpred11_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred11_27+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred11_29+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$3, %edx
	movl	at+104, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred11_31+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+108, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$148, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247072656469636174655F70726F706572747932__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$11, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+52, %ecx
	movl	$.Lpred12_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+56, %ecx
	movl	$.Lpred12_6+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+60, %ecx
	movl	$.Lpred12_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+64, %ecx
	movl	$.Lpred12_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+68, %ecx
	movl	$.Lpred12_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+72, %ecx
	movl	$.Lpred12_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+76, %ecx
	movl	$.Lpred12_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+80, %ecx
	movl	$.Lpred12_18+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+84, %ecx
	movl	$.Lpred12_20+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+88, %ecx
	movl	$.Lpred12_22+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$11, %edx
	movl	at+92, %ecx
	movl	$.Lpred12_24+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+96, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred12_26+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred12_28+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$3, %edx
	movl	at+104, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred12_30+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+112, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$194, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2470726F705F6D6574615F70726564__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$31, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+16
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+116, %ecx
	movl	$.Lpred13_7+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+120, %ecx
	movl	$.Lpred13_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+124, %ecx
	movl	$.Lpred13_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+128, %ecx
	movl	$.Lpred13_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+132, %ecx
	movl	$.Lpred13_2+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+136, %ecx
	movl	$.Lpred13_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+140, %ecx
	movl	$.Lpred13_19+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+144, %ecx
	movl	$.Lpred13_21+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+148, %ecx
	movl	$.Lpred13_23+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+152, %ecx
	movl	$.Lpred13_25+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+156, %ecx
	movl	$.Lpred13_27+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+160, %ecx
	movl	$.Lpred13_49+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+164, %ecx
	movl	$.Lpred13_3+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+168, %ecx
	movl	$.Lpred13_73+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+172, %ecx
	movl	$.Lpred13_75+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+176, %ecx
	movl	$.Lpred13_77+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+12, %ecx
	movl	$.Lpred13_79+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+180, %ecx
	movl	$.Lpred13_81+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+184, %ecx
	movl	$.Lpred13_83+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+188, %ecx
	movl	$.Lpred13_4+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+192, %ecx
	movl	$.Lpred13_93+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+196, %ecx
	movl	$.Lpred13_95+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+200, %ecx
	movl	$.Lpred13_97+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+204, %ecx
	movl	$.Lpred13_5+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+32, %ecx
	movl	$.Lpred13_103+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+208, %ecx
	movl	$.Lpred13_105+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+212, %ecx
	movl	$.Lpred13_107+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+216, %ecx
	movl	$.Lpred13_109+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+220, %ecx
	movl	$.Lpred13_111+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+224, %ecx
	movl	$.Lpred13_113+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+16, %eax
	movl	$31, %edx
	movl	at+228, %ecx
	movl	$.Lpred13_115+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	at+232, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$257, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F707265645F696E64696361746F72__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+236, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$263, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F7072656469636174655F66696C655F696E666F__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+240, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$269, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F7072656469636174655F66696C655F696E666F__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+244, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$275, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246175785F6E616D65__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+248, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$281, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246E6F745F6175785F6E616D65__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+252, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$287, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246661746865725F6F665F6175785F6E616D65__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+256, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$293, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24707265645F776974686F75745F617578__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+260, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$299, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246D616B655F6175785F6E616D65__a4+0, 8(%esp)
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
	.string	"$aux_name"
.LC1:
	.string	"$check_pred_prop"
.LC2:
	.string	"$current_predicate"
.LC3:
	.string	"$current_predicate_alt"
.LC4:
	.string	"$current_predicate_any"
.LC5:
	.string	"$current_predicate_bips"
.LC6:
	.string	"$father_of_aux_name"
.LC7:
	.string	"$get_pred_indicator"
.LC8:
	.string	"$get_predicate_file_info"
.LC9:
	.string	"$make_aux_name"
.LC10:
	.string	"$not_aux_name"
.LC11:
	.string	"$pred_without_aux"
.LC12:
	.string	"$predicate_property1"
.LC13:
	.string	"$predicate_property2"
.LC14:
	.string	"$predicate_property_pi"
.LC15:
	.string	"$predicate_property_pi_any"
.LC16:
	.string	"$prop_meta_pred"
.LC17:
	.string	"$set_predicate_file_info"
.LC18:
	.string	"$use_pred"
.LC19:
	.string	"*->"
.LC66:
	.string	"+"
.LC20:
	.string	","
.LC67:
	.string	"-"
.LC21:
	.string	"->"
.LC22:
	.string	"."
.LC71:
	.string	"/"
.LC23:
	.string	"/home/diaz/GP/src/BipsPl/pred.pl"
.LC68:
	.string	":"
.LC24:
	.string	";"
.LC69:
	.string	"?"
.LC25:
	.string	"\\+"
.LC26:
	.string	"abolish"
.LC27:
	.string	"asserta"
.LC28:
	.string	"assertz"
.LC29:
	.string	"bagof"
.LC30:
	.string	"built_in"
.LC31:
	.string	"built_in_fd"
.LC32:
	.string	"call"
.LC33:
	.string	"call_det"
.LC34:
	.string	"call_with_args"
.LC70:
	.string	"callable"
.LC35:
	.string	"catch"
.LC36:
	.string	"clause"
.LC37:
	.string	"consult"
.LC38:
	.string	"control_construct"
.LC39:
	.string	"current_predicate"
.LC40:
	.string	"dynamic"
.LC41:
	.string	"fd_maximize"
.LC42:
	.string	"fd_minimize"
.LC43:
	.string	"findall"
.LC44:
	.string	"forall"
.LC45:
	.string	"listing"
.LC46:
	.string	"maplist"
.LC47:
	.string	"meta_predicate"
.LC48:
	.string	"monofile"
.LC49:
	.string	"multifile"
.LC50:
	.string	"native_code"
.LC51:
	.string	"nospy"
.LC52:
	.string	"once"
.LC53:
	.string	"phrase"
.LC54:
	.string	"predicate_property"
.LC55:
	.string	"private"
.LC56:
	.string	"prolog_file"
.LC57:
	.string	"prolog_line"
.LC58:
	.string	"public"
.LC59:
	.string	"retract"
.LC60:
	.string	"retractall"
.LC61:
	.string	"setof"
.LC62:
	.string	"spy"
.LC63:
	.string	"static"
.LC64:
	.string	"system"
.LC65:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,264,4
	.local	fn
	.comm	fn,232,4
	.local	st
	.comm	st,20,4
	.local	ta
	.comm	ta,188,4
	.section	.note.GNU-stack,"",@progbits
