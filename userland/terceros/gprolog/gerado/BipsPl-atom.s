# BipsPl/atom.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/atom.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F61746F6D__a0,@function
.globl X1_247573655F61746F6D__a0

X1_247573655F61746F6D__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom_length__a2,@function
.globl X0_atom_length__a2

X0_atom_length__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Atom_Length_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom_concat__a3,@function
.globl X0_atom_concat__a3

X0_atom_concat__a3:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Atom_Concat_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2461746F6D5F636F6E6361745F616C74__a0,@function
.globl X1_2461746F6D5F636F6E6361745F616C74__a0

X1_2461746F6D5F636F6E6361745F616C74__a0:
	call	Pl_Atom_Concat_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_sub_atom__a5,@function
.globl X0_sub_atom__a5

X0_sub_atom__a5:
	movl	at+24, %eax
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
	call	Pl_Sub_Atom_5
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247375625F61746F6D5F616C74__a0,@function
.globl X1_247375625F61746F6D5F616C74__a0

X1_247375625F61746F6D5F616C74__a0:
	call	Pl_Sub_Atom_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom_chars__a2,@function
.globl X0_atom_chars__a2

X0_atom_chars__a2:
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Atom_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom_codes__a2,@function
.globl X0_atom_codes__a2

X0_atom_codes__a2:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Atom_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_number_atom__a2,@function
.globl X0_number_atom__a2

X0_number_atom__a2:
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Number_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_number_chars__a2,@function
.globl X0_number_chars__a2

X0_number_chars__a2:
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Number_Chars_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_number_codes__a2,@function
.globl X0_number_codes__a2

X0_number_codes__a2:
	movl	at+48, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Number_Codes_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_char_code__a2,@function
.globl X0_char_code__a2

X0_char_code__a2:
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Char_Code_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_name__a2,@function
.globl X0_name__a2

X0_name__a2:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_lower_upper__a2,@function
.globl X0_lower_upper__a2

X0_lower_upper__a2:
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Lower_Upper_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_atom__a1,@function
.globl X0_current_atom__a1

X0_current_atom__a1:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_2463757272656E745F61746F6D__a1
	.p2align	4,,15
	.type	X1_2463757272656E745F61746F6D__a1,@function
.globl X1_2463757272656E745F61746F6D__a1

X1_2463757272656E745F61746F6D__a1:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F61746F6D5F616E79__a1,@function
.globl X1_2463757272656E745F61746F6D5F616E79__a1

X1_2463757272656E745F61746F6D5F616E79__a1:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F61746F6D5F616C74__a0,@function
.globl X1_2463757272656E745F61746F6D5F616C74__a0

X1_2463757272656E745F61746F6D5F616C74__a0:
	call	Pl_Current_Atom_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom_property__a2,@function
.globl X0_atom_property__a2

X0_atom_property__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+80, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_2463757272656E745F61746F6D__a1
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2461746F6D5F70726F706572747931__a2
	.p2align	4,,15
	.type	X1_2461746F6D5F70726F70657274795F616E79__a2,@function
.globl X1_2461746F6D5F70726F70657274795F616E79__a2

X1_2461746F6D5F70726F70657274795F616E79__a2:
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
	jmp	X1_2463757272656E745F61746F6D5F616E79__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2461746F6D5F70726F706572747931__a2
	.p2align	4,,15
	.type	X1_2461746F6D5F70726F706572747931__a2,@function
.globl X1_2461746F6D5F70726F706572747931__a2

X1_2461746F6D5F70726F706572747931__a2:
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F70726F70__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Cut
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+28
	movl	pl_reg_bank+28, %eax
	movl	%eax, pl_reg_bank+12
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+32
	movl	pl_reg_bank+32, %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+36
	movl	pl_reg_bank+36, %eax
	movl	%eax, pl_reg_bank+20
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+40
	movl	pl_reg_bank+40, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+28, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+32, %eax
	movl	%eax, 12(%esp)
	movl	pl_reg_bank+36, %eax
	movl	%eax, 16(%esp)
	movl	pl_reg_bank+40, %eax
	movl	%eax, 20(%esp)
	call	Pl_Atom_Property_6
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2461746F6D5F70726F706572747932__a7
	.p2align	4,,15
	.type	X1_24636865636B5F61746F6D5F70726F70__a1,@function
.globl X1_24636865636B5F61746F6D5F70726F70__a1

X1_24636865636B5F61746F6D5F70726F70__a1:
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_1:
	movl	$.Lpred22_18+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred22_4+0, %eax
	movl	$.Lpred22_2+0, %edx
	movl	$.Lpred22_3+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred22_2:
	movl	st+0, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred22_3:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred22_4:
	movl	$.Lpred22_6+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred22_5:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred22_6:
	movl	$.Lpred22_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_7:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred22_8:
	movl	$.Lpred22_10+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_9:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_10:
	movl	$.Lpred22_12+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_11:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_12:
	movl	$.Lpred22_14+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_13:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_14:
	movl	$.Lpred22_16+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred22_15:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_16:
	call	Pl_Delete_Choice_Point1

.Lpred22_17:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred22_18:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F646F6D61696E__a2
	.p2align	4,,15
	.type	X1_2461746F6D5F70726F706572747932__a7,@function
.globl X1_2461746F6D5F70726F706572747932__a7

X1_2461746F6D5F70726F706572747932__a7:
	movl	$.Lpred23_3+0, %eax
	movl	$.Lpred23_1+0, %edx
	movl	$.Lpred23_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred23_1:
	movl	st+8, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred23_2:
	movl	st+12, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred23_3:
	movl	$.Lpred23_5+0, %eax
	movl	$7, %edx
	call	Pl_Create_Choice_Point

.Lpred23_4:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 4(%esp)
	call	Pl_Atom_Length_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_5:
	movl	$.Lpred23_7+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred23_6:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 4(%esp)
	call	Pl_Term_Hash_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_7:
	movl	$.Lpred23_9+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred23_8:
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_9:
	movl	$.Lpred23_11+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred23_10:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_11:
	movl	$.Lpred23_13+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred23_12:
	movl	ta+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_13:
	movl	$.Lpred23_15+0, %eax
	movl	$7, %edx
	call	Pl_Update_Choice_Point

.Lpred23_14:
	movl	ta+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+20, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred23_15:
	movl	$7, %eax
	call	Pl_Delete_Choice_Point

.Lpred23_16:
	movl	ta+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483641, %eax
	movl	pl_reg_bank+24, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_new_atom__a1,@function
.globl X0_new_atom__a1

X0_new_atom__a1:
	movl	at+128, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+0, %eax
	movl	%eax, 4(%esp)
	call	Pl_New_Atom_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_new_atom__a2,@function
.globl X0_new_atom__a2

X0_new_atom__a2:
	movl	at+128, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_New_Atom_2
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
	movl	%eax, at+20
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC33, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	at+80, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+100, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+108, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	at+112, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+16
	movl	at+104, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	at+96, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	$.LC18, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC20, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F61746F6D__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom_length__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom_concat__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$55, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2461746F6D5F636F6E6361745F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_sub_atom__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247375625F61746F6D5F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$78, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$85, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_number_atom__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$92, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_number_chars__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_number_codes__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_char_code__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$113, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$120, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_lower_upper__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_atom__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F61746F6D__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$135, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F61746F6D5F616E79__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$139, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F61746F6D5F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom_property__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$151, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2461746F6D5F70726F70657274795F616E79__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$158, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2461746F6D5F70726F706572747931__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$165, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F61746F6D5F70726F70__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+96, %ecx
	movl	$.Lpred22_9+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+100, %ecx
	movl	$.Lpred22_11+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+104, %ecx
	movl	$.Lpred22_13+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+108, %ecx
	movl	$.Lpred22_15+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$5, %edx
	movl	at+112, %ecx
	movl	$.Lpred22_17+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+116, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred22_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+120, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred22_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+124, %eax
	movl	$7, %edx
	movl	at+0, %ecx
	movl	$188, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2461746F6D5F70726F706572747932__a7+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+96, %ecx
	movl	$.Lpred23_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+100, %ecx
	movl	$.Lpred23_10+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+104, %ecx
	movl	$.Lpred23_12+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+108, %ecx
	movl	$.Lpred23_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+112, %ecx
	movl	$.Lpred23_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+12
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+116, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred23_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+12, %eax
	movl	$2, %edx
	movl	at+120, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred23_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+128, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$207, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_new_atom__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$211, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_new_atom__a2+0, 8(%esp)
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
	.string	"$atom_concat_alt"
.LC1:
	.string	"$atom_property1"
.LC2:
	.string	"$atom_property2"
.LC3:
	.string	"$atom_property_any"
.LC4:
	.string	"$check_atom_prop"
.LC5:
	.string	"$current_atom"
.LC6:
	.string	"$current_atom_alt"
.LC7:
	.string	"$current_atom_any"
.LC8:
	.string	"$sub_atom_alt"
.LC9:
	.string	"$use_atom"
.LC10:
	.string	"/home/diaz/GP/src/BipsPl/atom.pl"
.LC33:
	.string	"atom_"
.LC11:
	.string	"atom_chars"
.LC12:
	.string	"atom_codes"
.LC13:
	.string	"atom_concat"
.LC14:
	.string	"atom_length"
.LC15:
	.string	"atom_property"
.LC16:
	.string	"char_code"
.LC17:
	.string	"current_atom"
.LC18:
	.string	"hash"
.LC19:
	.string	"infix_op"
.LC20:
	.string	"length"
.LC21:
	.string	"lower_upper"
.LC22:
	.string	"name"
.LC23:
	.string	"needs_quotes"
.LC24:
	.string	"needs_scan"
.LC25:
	.string	"new_atom"
.LC26:
	.string	"number_atom"
.LC27:
	.string	"number_chars"
.LC28:
	.string	"number_codes"
.LC29:
	.string	"postfix_op"
.LC30:
	.string	"prefix_op"
.LC31:
	.string	"sub_atom"
.LC32:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,132,4
	.local	fn
	.comm	fn,8,4
	.local	st
	.comm	st,16,4
	.local	ta
	.comm	ta,28,4
	.section	.note.GNU-stack,"",@progbits
