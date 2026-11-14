# BipsPl/call.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/call.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F63616C6C__a0,@function
.globl X1_247573655F63616C6C__a0

X1_247573655F63616C6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_once__a1,@function
.globl X0_once__a1

X0_once__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_5C5C2B__a1,@function
.globl X1_5C5C2B__a1

X1_5C5C2B__a1:
	jmp	X1_245C5C2B2F315F2461757831__a1
	.p2align	4,,15
	.type	X1_245C5C2B2F315F2461757831__a1,@function

X1_245C5C2B2F315F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred4_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_call_det__a2,@function
.globl X0_call_det__a2

X0_call_det__a2:
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
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_2463616C6C5F6465742F325F2461757831__a1
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F646574__a2
	.p2align	4,,15
	.type	X1_2463616C6C5F6465742F325F2461757831__a1,@function

X1_2463616C6C5F6465742F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	ta+16, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572725F74797065__a2

.Lpred6_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463616C6C5F646574__a2,@function
.globl X1_2463616C6C5F646574__a2

X1_2463616C6C5F646574__a2:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+20, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_242463616C6C5F6465742F325F2461757831__a3
	.p2align	4,,15
	.type	X1_242463616C6C5F6465742F325F2461757831__a3,@function

X1_242463616C6C5F6465742F325F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	at+44, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+4, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	ta+12, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred8_1:
	call	Pl_Delete_Choice_Point4
	movl	ta+4, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463616C6C__a4,@function
.globl X1_2463616C6C__a4

X1_2463616C6C__a4:
	movl	pl_reg_bank+4, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 8(%esp)
	call	Pl_Save_Call_Info_3
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	jmp	X1_2463616C6C31__a2
	.p2align	4,,15
	.type	X1_2463616C6C31__a2,@function
.globl X1_2463616C6C31__a2

X1_2463616C6C31__a2:
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+8, %eax
	movl	%eax, 0(%esp)
	call	Pl_Load_Call_Info_Arg_1
	jmp	X1_2463616C6C5F696E7465726E616C__a2
	.p2align	4,,15
	.type	X1_2463616C6C5F696E7465726E616C__a2,@function
.globl X1_2463616C6C5F696E7465726E616C__a2

X1_2463616C6C5F696E7465726E616C__a2:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Call_Info_Bip_Name_1
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_242463616C6C5F696E7465726E616C2F325F2461757831__a1
.Lcont7:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	leal	-24(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C__a3
.Lcont8:
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C31__a2
	.p2align	4,,15
	.type	X1_242463616C6C5F696E7465726E616C2F325F2461757831__a1,@function

X1_242463616C6C5F696E7465726E616C2F325F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred12_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred12_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463616C6C5F696E7465726E616C31__a2,@function
.globl X1_2463616C6C5F696E7465726E616C31__a2

X1_2463616C6C5F696E7465726E616C31__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3
	.p2align	4,,15
	.type	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3,@function
.globl X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred14_23+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$.Lpred14_3+0, %eax
	movl	$.Lpred14_1+0, %edx
	movl	$.Lpred14_2+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Stc
	jmp	*%eax

.Lpred14_1:
	movl	st+0, %eax
	movl	$3, %edx
	call	Pl_Switch_On_Atom
	jmp	*%eax

.Lpred14_2:
	movl	st+4, %eax
	movl	$7, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred14_3:
	movl	$.Lpred14_5+0, %eax
	call	Pl_Create_Choice_Point4

.Lpred14_4:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred14_5:
	movl	$.Lpred14_7+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_6:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X1_2463616C6C5F696E7465726E616C5F6F72__a4

.Lpred14_7:
	movl	$.Lpred14_9+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_8:
	movl	ta+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred14_9:
	movl	$.Lpred14_11+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_10:
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -28(%edi)
	movl	-28(%edi), %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C__a2
.Lcont10:
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
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred14_11:
	movl	$.Lpred14_13+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_12:
	movl	$3, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -24(%edi)
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C__a2
.Lcont11:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred14_13:
	movl	$.Lpred14_15+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_14:
	movl	ta+28, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred14_15:
	movl	$.Lpred14_17+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_16:
	movl	ta+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred14_17:
	movl	$.Lpred14_19+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_18:
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_2463616C6C5F696E7465726E616C__a2

.Lpred14_19:
	movl	$.Lpred14_21+0, %eax
	call	Pl_Update_Choice_Point4

.Lpred14_20:
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+16
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+12
	jmp	X1_2463617463685F696E7465726E616C__a4

.Lpred14_21:
	call	Pl_Delete_Choice_Point4

.Lpred14_22:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	jmp	X1_247468726F775F696E7465726E616C__a2

.Lpred14_23:
	call	Pl_Delete_Choice_Point4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_BC_Call_Terminal_Pred_3
	jmp	*%eax
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463616C6C5F696E7465726E616C5F6F72__a4,@function
.globl X1_2463616C6C5F696E7465726E616C5F6F72__a4

X1_2463616C6C5F696E7465726E616C5F6F72__a4:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+16
	movl	$.Lpred15_6+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$.Lpred15_2+0, %eax
	movl	$.Lpred15_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred15_1:
	movl	st+8, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred15_2:
	movl	$.Lpred15_4+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point

.Lpred15_3:
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757831__a6

.Lpred15_4:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point

.Lpred15_5:
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+24
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+20
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+16, %eax
	call	Pl_Cut
	movl	pl_reg_bank+24, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+16
	jmp	X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757832__a5

.Lpred15_6:
	movl	$.Lpred15_7+0, %eax
	movl	$5, %edx
	call	Pl_Update_Choice_Point
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred15_7:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3
	.p2align	4,,15
	.type	X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757832__a5,@function

X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757832__a5:
	movl	$.Lpred16_1+0, %eax
	movl	$5, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	call	Pl_Get_Current_Choice
	movl	%eax, -28(%edi)
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Soft_Cut
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred16_1:
	movl	$5, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3
	.p2align	4,,15
	.type	X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757831__a6,@function

X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757831__a6:
	movl	$.Lpred17_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$4, %eax
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
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C__a2
.Lcont13:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Cut
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3

.Lpred17_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	movl	pl_reg_bank+20, %eax
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+8
	jmp	X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3
	.p2align	4,,15
	.type	X1_2463616C6C5F66726F6D5F6465627567676572__a2,@function
.globl X1_2463616C6C5F66726F6D5F6465627567676572__a2

X1_2463616C6C5F66726F6D5F6465627567676572__a2:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_BC_Call_Terminal_Pred_3
	jmp	*%eax
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_false__a0,@function
.globl X0_false__a0

X0_false__a0:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_forall__a2,@function
.globl X0_forall__a2

X0_forall__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	call	Pl_Unify_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+32, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	jmp	X1_246E6F74__a3
	.p2align	4,,15
	.type	X1_246E6F74__a3,@function
.globl X1_246E6F74__a3

X1_246E6F74__a3:
	jmp	X1_24246E6F742F335F2461757831__a3
	.p2align	4,,15
	.type	X1_24246E6F742F335F2461757831__a3,@function

X1_24246E6F742F335F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred22_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+12, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_2463616C6C__a4
.Lcont14:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred22_1:
	call	Pl_Delete_Choice_Point4
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
	movl	%eax, at+40
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+120
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+116
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+140
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+124
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+112
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+136
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+96
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC24, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC25, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+100
	movl	$.LC26, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC27, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+104
	movl	$.LC28, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC29, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+128
	movl	$.LC30, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+132
	movl	$.LC31, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC32, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC33, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+108
	movl	$.LC34, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC35, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	at+72, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+24
	movl	at+36, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+20
	movl	at+16, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+8
	movl	$.LC36, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+16
	movl	at+76, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+28
	movl	at+128, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	at+132, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+32
	movl	at+12, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+0
	movl	at+80, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC16, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC18, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC22, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC27, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC33, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F63616C6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_once__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_5C5C2B__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$49, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_245C5C2B2F315F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$57, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_call_det__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$57, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2463616C6C5F6465742F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F646574__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463616C6C5F6465742F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$79, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C31__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$87, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F696E7465726E616C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$87, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463616C6C5F696E7465726E616C2F325F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$97, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F696E7465726E616C31__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$104, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F696E7465726E616C5F776974685F637574__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$3, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+72, %ecx
	movl	$.Lpred14_8+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+76, %ecx
	movl	$.Lpred14_14+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	st+0, %eax
	movl	$3, %edx
	movl	at+80, %ecx
	movl	$.Lpred14_16+0, 0(%esp)
	call	Pl_Create_Swt_Atm_Element
	movl	$7, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+84, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred14_4+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+88, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred14_6+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+92, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred14_10+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+96, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred14_12+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+100, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_18+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+104, %ecx
	movl	$3, 0(%esp)
	movl	$.Lpred14_20+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$7, %edx
	movl	at+108, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred14_22+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+112, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$154, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F696E7465726E616C5F6F72__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+92, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$2, %edx
	movl	at+96, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred15_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+116, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$161, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757832__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+120, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$154, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463616C6C5F696E7465726E616C5F6F722F345F2461757831__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+124, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$177, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F66726F6D5F6465627567676572__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+128, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$182, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_false__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+132, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$186, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_forall__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+136, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$190, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246E6F74__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+140, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$190, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24246E6F742F335F2461757831__a3+0, 8(%esp)
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
	.string	"!"
.LC1:
	.string	"$$call_det/2_$aux1"
.LC2:
	.string	"$$call_internal/2_$aux1"
.LC3:
	.string	"$$call_internal_or/4_$aux1"
.LC4:
	.string	"$$call_internal_or/4_$aux2"
.LC5:
	.string	"$$not/3_$aux1"
.LC6:
	.string	"$\\+/1_$aux1"
.LC7:
	.string	"$call"
.LC8:
	.string	"$call1"
.LC9:
	.string	"$call_det"
.LC10:
	.string	"$call_det/2_$aux1"
.LC11:
	.string	"$call_from_debugger"
.LC12:
	.string	"$call_internal"
.LC13:
	.string	"$call_internal1"
.LC14:
	.string	"$call_internal_or"
.LC15:
	.string	"$call_internal_with_cut"
.LC16:
	.string	"$not"
.LC17:
	.string	"$use_call"
.LC18:
	.string	"*->"
.LC19:
	.string	","
.LC20:
	.string	"->"
.LC21:
	.string	"/home/diaz/GP/src/BipsPl/call.pl"
.LC22:
	.string	";"
.LC23:
	.string	">"
.LC24:
	.string	"\\+"
.LC36:
	.string	"boolean"
.LC25:
	.string	"call"
.LC26:
	.string	"call_det"
.LC27:
	.string	"catch"
.LC28:
	.string	"fail"
.LC29:
	.string	"false"
.LC30:
	.string	"forall"
.LC31:
	.string	"once"
.LC32:
	.string	"system"
.LC33:
	.string	"throw"
.LC34:
	.string	"true"
.LC35:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,144,4
	.local	fn
	.comm	fn,32,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,36,4
	.section	.note.GNU-stack,"",@progbits
