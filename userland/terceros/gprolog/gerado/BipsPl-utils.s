# BipsPl/utils.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/utils.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247465726D5F746F5F676F616C__a3,@function
.globl X1_247465726D5F746F5F676F616C__a3

X1_247465726D5F746F5F676F616C__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred1_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Assign
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C31__a2
.Lcont1:
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred1_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247465726D5F746F5F676F616C31__a2,@function
.globl X1_247465726D5F746F5F676F616C31__a2

X1_247465726D5F746F5F676F616C31__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred2_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C32__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred2_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_247465726D5F746F5F676F616C32__a2,@function
.globl X1_247465726D5F746F5F676F616C32__a2

X1_247465726D5F746F5F676F616C32__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred3_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+12, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Read
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+16, %edx
	call	Pl_Blt_G_Assign
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_24247465726D5F746F5F676F616C322F325F2461757831__a3

.Lpred3_1:
	movl	$.Lpred3_9+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$.Lpred3_3+0, %eax
	movl	$.Lpred3_2+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred3_2:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred3_3:
	movl	$.Lpred3_5+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred3_4:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C32__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247465726D5F746F5F676F616C32__a2

.Lpred3_5:
	movl	$.Lpred3_7+0, %eax
	call	Pl_Update_Choice_Point3

.Lpred3_6:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C32__a2
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247465726D5F746F5F676F616C32__a2

.Lpred3_7:
	call	Pl_Delete_Choice_Point3

.Lpred3_8:
	movl	$2, %eax
	call	Pl_Allocate
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	fn+8, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C32__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247465726D5F746F5F676F616C32__a2

.Lpred3_9:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24247465726D5F746F5F676F616C322F325F2461757831__a3,@function

X1_24247465726D5F746F5F676F616C322F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	ta+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	fn+12, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	call	Pl_Delete_Choice_Point4
	movl	fn+16, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636865636B5F6C697374__a1,@function
.globl X1_24636865636B5F6C697374__a1

X1_24636865636B5F6C697374__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred5_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred5_1:
	movl	$.Lpred5_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred5_2:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_24636865636B5F6C6973745F6F725F7061727469616C5F6C697374__a1,@function
.globl X1_24636865636B5F6C6973745F6F725F7061727469616C5F6C697374__a1

X1_24636865636B5F6C6973745F6F725F7061727469616C5F6C697374__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred6_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1,@function
.globl X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1

X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred7_1:
	call	Pl_Delete_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1,@function
.globl X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1

X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1:
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point1
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred8_1:
	movl	$.Lpred8_6+0, %eax
	call	Pl_Update_Choice_Point1
	movl	$.Lpred8_2+0, %eax
	movl	$.Lpred8_3+0, %edx
	movl	$.Lpred8_5+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred8_2:
	movl	$.Lpred8_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred8_3:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_4:
	call	Pl_Delete_Choice_Point1

.Lpred8_5:
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
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737432__a1
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1

.Lpred8_6:
	call	Pl_Delete_Choice_Point1
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+24, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737432__a1,@function
.globl X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737432__a1

X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737432__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	movl	$.Lpred9_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred9_2:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+28, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_246765745F686561645F616E645F626F6479__a3,@function
.globl X1_246765745F686561645F616E645F626F6479__a3

X1_246765745F686561645F616E645F626F6479__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred10_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_24636865636B5F68656164__a1

.Lpred10_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+32, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_24636865636B5F68656164__a1
	.p2align	4,,15
	.type	X1_24636865636B5F68656164__a1,@function
.globl X1_24636865636B5F68656164__a1

X1_24636865636B5F68656164__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred11_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_2424636865636B5F686561642F315F2461757831__a1
	.p2align	4,,15
	.type	X1_2424636865636B5F686561642F315F2461757831__a1,@function

X1_2424636865636B5F686561642F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred12_1:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2
	.p2align	4,,15
	.type	X1_24636865636B5F6E6F6E766172__a1,@function
.globl X1_24636865636B5F6E6F6E766172__a1

X1_24636865636B5F6E6F6E766172__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred13_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred13_1:
	call	Pl_Delete_Choice_Point2
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0
	.p2align	4,,15
	.type	X1_246765745F707265645F696E646963__a3,@function
.globl X1_246765745F707265645F696E646963__a3

X1_246765745F707265645F696E646963__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Get_Pred_Indic_3
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
	movl	%eax, at+68
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC20, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC21, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC22, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+28
	movl	$.LC23, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	$.LC24, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC25, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+24
	movl	$.LC26, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+20
	movl	$.LC27, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+12
	movl	$.LC28, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+32
	movl	$.LC29, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC14, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC15, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC30, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC17, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC31, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	at+4, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$42, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247465726D5F746F5F676F616C__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247465726D5F746F5F676F616C31__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247465726D5F746F5F676F616C32__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+20, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred3_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+24, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred3_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+28, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred3_8+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+32, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24247465726D5F746F5F676F616C322F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$93, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F6C697374__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F6C6973745F6F725F7061727469616C5F6C697374__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$115, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C697374__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$122, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737431__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$136, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F61746F6D5F6F725F61746F6D5F6C69737432__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$149, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F686561645F616E645F626F6479__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$159, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F68656164__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$163, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424636865636B5F686561642F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$172, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636865636B5F6E6F6E766172__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$181, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F707265645F696E646963__a3+0, 8(%esp)
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
	.string	"$$check_head/1_$aux1"
.LC1:
	.string	"$$term_to_goal2/2_$aux1"
.LC20:
	.string	"$call_call_info"
.LC29:
	.string	"$call_internal"
.LC2:
	.string	"$check_atom_or_atom_list"
.LC3:
	.string	"$check_atom_or_atom_list1"
.LC4:
	.string	"$check_atom_or_atom_list2"
.LC5:
	.string	"$check_head"
.LC6:
	.string	"$check_list"
.LC7:
	.string	"$check_list_or_partial_list"
.LC8:
	.string	"$check_nonvar"
.LC9:
	.string	"$get_head_and_body"
.LC10:
	.string	"$get_pred_indic"
.LC21:
	.string	"$new_term"
.LC11:
	.string	"$term_to_goal"
.LC12:
	.string	"$term_to_goal1"
.LC13:
	.string	"$term_to_goal2"
.LC14:
	.string	","
.LC15:
	.string	"->"
.LC16:
	.string	"/home/diaz/GP/src/BipsPl/utils.pl"
.LC30:
	.string	":-"
.LC17:
	.string	";"
.LC22:
	.string	"atom"
.LC31:
	.string	"call"
.LC23:
	.string	"callable"
.LC24:
	.string	"f"
.LC25:
	.string	"list"
.LC26:
	.string	"none"
.LC18:
	.string	"system"
.LC27:
	.string	"t"
.LC28:
	.string	"true"
.LC19:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,80,4
	.local	fn
	.comm	fn,24,4
	.local	st
	.comm	st,4,4
	.local	ta
	.comm	ta,36,4
	.section	.note.GNU-stack,"",@progbits
