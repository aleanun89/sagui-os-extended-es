# Pl2Wam/first_arg.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/Pl2Wam/first_arg.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_find_first_arg__a2,@function
.globl X0_find_first_arg__a2

X0_find_first_arg__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred1_1+0, %eax
	movl	$.Lpred1_2+0, %edx
	movl	$.Lpred1_4+0, %ecx
	call	Pl_Switch_On_Term_Var_Atm_Lst
	jmp	*%eax

.Lpred1_1:
	movl	$.Lpred1_3+0, %eax
	call	Pl_Create_Choice_Point3

.Lpred1_2:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_Nil
	testl	%eax, %eax
	je	fail
	movl	ta+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred1_3:
	call	Pl_Delete_Choice_Point3

.Lpred1_4:
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_2466696E645F66697273745F6172672F325F2461757831__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2466696E645F66697273745F6172672F325F2461757831__a3,@function

X1_2466696E645F66697273745F6172672F325F2461757831__a3:
	movl	$.Lpred2_1+0, %eax
	call	Pl_Create_Choice_Point3
	jmp	X0_defines_first_arg__a2

.Lpred2_1:
	movl	$.Lpred2_2+0, %eax
	call	Pl_Update_Choice_Point3
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X0_stopping_inst__a1
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040

.Lpred2_2:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+8, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_find_first_arg__a2
	.p2align	4,,15
	.type	X0_stopping_inst__a1,@function
.globl X0_stopping_inst__a1

X0_stopping_inst__a1:
	movl	$.Lpred3_10+0, %eax
	call	Pl_Create_Choice_Point1
	movl	$.Lpred3_2+0, %eax
	movl	$.Lpred3_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred3_1:
	movl	st+0, %eax
	movl	$4, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred3_2:
	movl	$.Lpred3_4+0, %eax
	call	Pl_Create_Choice_Point1

.Lpred3_3:
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred3_4:
	movl	$.Lpred3_6+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred3_5:
	movl	fn+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred3_6:
	movl	$.Lpred3_8+0, %eax
	call	Pl_Update_Choice_Point1

.Lpred3_7:
	movl	fn+8, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred3_8:
	call	Pl_Delete_Choice_Point1

.Lpred3_9:
	movl	fn+12, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$1, %eax
	call	Pl_Unify_Void
	jmp	*pl_reg_bank+1040

.Lpred3_10:
	call	Pl_Delete_Choice_Point1
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_codification__a2
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	call	Pl_Deallocate
	jmp	X0_assign_x0__a1
	.p2align	4,,15
	.type	X0_assign_x0__a1,@function
.globl X0_assign_x0__a1

X0_assign_x0__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Get_List
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	jmp	X1_2461737369676E5F78302F315F2461757831__a2
	.p2align	4,,15
	.type	X1_2461737369676E5F78302F315F2461757831__a2,@function

X1_2461737369676E5F78302F315F2461757831__a2:
	movl	$.Lpred5_6+0, %eax
	call	Pl_Create_Choice_Point2
	movl	$.Lpred5_2+0, %eax
	movl	$.Lpred5_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred5_1:
	movl	st+4, %eax
	movl	$2, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred5_2:
	movl	$.Lpred5_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred5_3:
	movl	fn+16, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_4:
	call	Pl_Delete_Choice_Point2

.Lpred5_5:
	movl	fn+20, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Integer_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred5_6:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X0_assign_x0__a1
	.p2align	4,,15
	.type	X0_defines_first_arg__a2,@function
.globl X0_defines_first_arg__a2

X0_defines_first_arg__a2:
	movl	$.Lpred6_2+0, %eax
	movl	$.Lpred6_1+0, %edx
	call	Pl_Switch_On_Term_Var_Stc
	jmp	*%eax

.Lpred6_1:
	movl	st+8, %eax
	movl	$5, %edx
	call	Pl_Switch_On_Structure
	jmp	*%eax

.Lpred6_2:
	movl	$.Lpred6_4+0, %eax
	call	Pl_Create_Choice_Point2

.Lpred6_3:
	movl	fn+24, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_4:
	movl	$.Lpred6_6+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred6_5:
	movl	fn+32, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+36, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_6:
	movl	$.Lpred6_8+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred6_7:
	movl	fn+40, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+28, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Nil
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_8:
	movl	$.Lpred6_10+0, %eax
	call	Pl_Update_Choice_Point2

.Lpred6_9:
	movl	fn+44, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Atom_Tagged
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred6_10:
	call	Pl_Delete_Choice_Point2

.Lpred6_11:
	movl	fn+48, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	$-2147483645, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	fn+52, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+8
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+0
	movl	fn+56, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	call	Pl_Unify_Value
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
	movl	%eax, at+40
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC19, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	$.LC20, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC21, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+52
	movl	$.LC22, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC4, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC5, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC6, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC8, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC10, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC11, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC12, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+44
	movl	$.LC13, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC14, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+48
	movl	$.LC23, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC15, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC24, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+56
	movl	$.LC18, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	at+4, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$39, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_find_first_arg__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2466696E645F66697273745F6172672F325F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_stopping_inst__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$4, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+0
	movl	st+0, %eax
	movl	$4, %edx
	movl	at+20, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred3_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$4, %edx
	movl	at+24, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred3_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$4, %edx
	movl	at+28, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred3_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+0, %eax
	movl	$4, %edx
	movl	at+32, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred3_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_assign_x0__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2461737369676E5F78302F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$2, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+4
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+44, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred5_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+4, %eax
	movl	$2, %edx
	movl	at+48, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred5_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X0_defines_first_arg__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	$5, %eax
	call	Pl_Create_Swt_Table
	movl	%eax, st+8
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+56, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred6_3+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+60, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred6_5+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+64, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred6_7+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+68, %ecx
	movl	$1, 0(%esp)
	movl	$.Lpred6_9+0, 4(%esp)
	call	Pl_Create_Swt_Stc_Element
	movl	st+8, %eax
	movl	$5, %edx
	movl	at+72, %ecx
	movl	$2, 0(%esp)
	movl	$.Lpred6_11+0, 4(%esp)
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
	.string	"$assign_x0/1_$aux1"
.LC1:
	.string	"$find_first_arg/2_$aux1"
.LC21:
	.string	"/"
.LC2:
	.string	"/home/diaz/GP/src/Pl2Wam/first_arg.pl"
.LC3:
	.string	"assign_x0"
.LC22:
	.string	"atm"
.LC4:
	.string	"c"
.LC5:
	.string	"call"
.LC6:
	.string	"cut"
.LC7:
	.string	"defines_first_arg"
.LC8:
	.string	"execute"
.LC9:
	.string	"find_first_arg"
.LC10:
	.string	"get_atom"
.LC11:
	.string	"get_integer"
.LC12:
	.string	"get_list"
.LC13:
	.string	"get_nil"
.LC14:
	.string	"get_structure"
.LC23:
	.string	"int"
.LC19:
	.string	"lst"
.LC15:
	.string	"soft_cut"
.LC24:
	.string	"stc"
.LC16:
	.string	"stopping_inst"
.LC17:
	.string	"user"
.LC20:
	.string	"var"
.LC18:
	.string	"w"
	.data	
	.align	4
	.local	at
	.comm	at,76,4
	.local	fn
	.comm	fn,60,4
	.local	st
	.comm	st,12,4
	.local	ta
	.comm	ta,8,4
	.section	.note.GNU-stack,"",@progbits
