# BipsPl/catch.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/catch.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F6361746368__a0,@function
.globl X1_247573655F6361746368__a0

X1_247573655F6361746368__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246361746368__a6,@function
.globl X1_246361746368__a6

X1_246361746368__a6:
	movl	pl_reg_bank+12, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+16, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+20, %eax
	movl	%eax, 8(%esp)
	call	Pl_Save_Call_Info_3
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_24636174636831__a4
	.p2align	4,,15
	.type	X1_24636174636831__a4,@function
.globl X1_24636174636831__a4

X1_24636174636831__a4:
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+16
	movl	pl_reg_bank+16, %eax
	movl	%eax, 0(%esp)
	call	Pl_Load_Call_Info_Arg_1
	jmp	X1_2463617463685F696E7465726E616C__a4
	.p2align	4,,15
	.type	X1_2463617463685F696E7465726E616C__a4,@function
.globl X1_2463617463685F696E7465726E616C__a4

X1_2463617463685F696E7465726E616C__a4:
	movl	$5, %eax
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
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont1:
	movl	$-2147483613, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247379735F7661725F707574__a2
.Lcont2:
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
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2463617463685F696E7465726E616C31__a5
	.p2align	4,,15
	.type	X1_2463617463685F696E7465726E616C31__a5,@function
.globl X1_2463617463685F696E7465726E616C31__a5

X1_2463617463685F696E7465726E616C31__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred5_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+16, %eax
	movl	%eax, -24(%edi)
	movl	pl_reg_bank+20, %eax
	movl	%eax, -28(%edi)
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont3:
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_2463616C6C5F696E7465726E616C__a2
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_246765745F63757272656E745F42__a1
.Lcont6:
	movl	pl_reg_bank+1044, %edi
	movl	-36(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_242463617463685F696E7465726E616C312F355F2461757831__a3
.Lcont7:
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_247379735F7661725F7772697465__a2

.Lpred5_1:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
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
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont8:
	movl	$-2147483613, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_247379735F7661725F676574__a2
.Lcont9:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	movl	-32(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+16
	call	Pl_Deallocate
	jmp	X1_2463617463685F615F7468726F77__a5
	.p2align	4,,15
	.type	X1_242463617463685F696E7465726E616C312F355F2461757831__a3,@function

X1_242463617463685F696E7465726E616C312F355F2461757831__a3:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+12
	movl	$.Lpred6_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+8, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+8, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+12, %eax
	call	Pl_Cut
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_24747261696C5F68616E646C6572__a1

.Lpred6_1:
	call	Pl_Delete_Choice_Point4
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463617463685F615F7468726F77__a5,@function
.globl X1_2463617463685F615F7468726F77__a5

X1_2463617463685F615F7468726F77__a5:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+20
	movl	$.Lpred7_1+0, %eax
	movl	$6, %edx
	call	Pl_Create_Choice_Point
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	movl	fn+0, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Get_Structure_Tagged
	testl	%eax, %eax
	je	fail
	call	Pl_Unify_Variable
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	pl_reg_bank+16, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_242463617463685F615F7468726F772F355F2461757831__a2

.Lpred7_1:
	movl	$.Lpred7_2+0, %eax
	movl	$6, %edx
	call	Pl_Update_Choice_Point
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+20, %eax
	call	Pl_Cut
	movl	$-2147483613, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_247379735F7661725F707574__a2
.Lcont10:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2463616C6C5F696E7465726E616C__a2

.Lpred7_2:
	movl	$6, %eax
	call	Pl_Delete_Choice_Point
	jmp	X1_24756E77696E64__a1
	.p2align	4,,15
	.type	X1_242463617463685F615F7468726F772F355F2461757831__a2,@function

X1_242463617463685F615F7468726F772F355F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred8_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+12, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24756E77696E64__a1

.Lpred8_1:
	call	Pl_Delete_Choice_Point3
	movl	pl_reg_bank+4, %eax
	movl	%eax, pl_reg_bank+0
	jmp	X1_2463617463685F6661696C5F6E6F77__a1
	.p2align	4,,15
	.type	X1_24747261696C5F68616E646C6572__a1,@function
.globl X1_24747261696C5F68616E646C6572__a1

X1_24747261696C5F68616E646C6572__a1:
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point1
	jmp	*pl_reg_bank+1040

.Lpred9_1:
	call	Pl_Delete_Choice_Point1
	movl	$0, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_247379735F7661725F7772697465__a2
.Lcont11:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_2463617463685F73796E635F666F725F6661696C5F6174__a1,@function
.globl X1_2463617463685F73796E635F666F725F6661696C5F6174__a1

X1_2463617463685F73796E635F666F725F6661696C5F6174__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$-2147483617, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+0
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_247379735F7661725F72656164__a2
.Lcont12:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_242463617463685F73796E635F666F725F6661696C5F61742F315F2461757831__a2
	.p2align	4,,15
	.type	X1_242463617463685F73796E635F666F725F6661696C5F61742F315F2461757831__a2,@function

X1_242463617463685F73796E635F666F725F6661696C5F61742F315F2461757831__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$.Lpred11_1+0, %eax
	call	Pl_Create_Choice_Point3
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	leal	pl_reg_bank+0, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+4, %eax
	leal	pl_reg_bank+12, %edx
	call	Pl_Math_Load_Value
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+12, %edx
	call	Pl_Blt_Gt
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+8, %eax
	call	Pl_Cut
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_247468726F77__a4

.Lpred11_1:
	call	Pl_Delete_Choice_Point3
	jmp	X1_2463617463685F6661696C5F6E6F77__a1
	.p2align	4,,15
	.type	X1_2463617463685F6661696C5F6E6F77__a1,@function
.globl X1_2463617463685F6661696C5F6E6F77__a1

X1_2463617463685F6661696C5F6E6F77__a1:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_247365745F63757272656E745F42__a1
.Lcont13:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
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
	movl	%eax, at+44
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	at+52, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC16, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC17, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC18, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F6361746368__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$6, %edx
	movl	at+0, %ecx
	movl	$47, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246361746368__a6+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636174636831__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463617463685F696E7465726E616C__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$62, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463617463685F696E7465726E616C31__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$62, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463617463685F696E7465726E616C312F355F2461757831__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$5, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463617463685F615F7468726F77__a5+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$91, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463617463685F615F7468726F772F355F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24747261696C5F68616E646C6572__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$126, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463617463685F73796E635F666F725F6661696C5F6174__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$126, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_242463617463685F73796E635F666F725F6661696C5F61742F315F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$138, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463617463685F6661696C5F6E6F77__a1+0, 8(%esp)
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
	.string	"$$catch_a_throw/5_$aux1"
.LC1:
	.string	"$$catch_internal1/5_$aux1"
.LC2:
	.string	"$$catch_sync_for_fail_at/1_$aux1"
.LC3:
	.string	"$catch"
.LC4:
	.string	"$catch1"
.LC5:
	.string	"$catch_a_throw"
.LC6:
	.string	"$catch_fail_now"
.LC7:
	.string	"$catch_internal"
.LC8:
	.string	"$catch_internal1"
.LC18:
	.string	"$catch_sync"
.LC9:
	.string	"$catch_sync_for_fail_at"
.LC16:
	.string	"$no_ball$"
.LC10:
	.string	"$trail_handler"
.LC11:
	.string	"$use_catch"
.LC12:
	.string	"/home/diaz/GP/src/BipsPl/catch.pl"
.LC13:
	.string	">"
.LC14:
	.string	"system"
.LC17:
	.string	"true"
.LC15:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,64,4
	.local	fn
	.comm	fn,4,4
	.local	ta
	.comm	ta,12,4
	.section	.note.GNU-stack,"",@progbits
