# BipsPl/pl_error.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/pl_error.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X0_set_bip_name__a2,@function
.globl X0_set_bip_name__a2

X0_set_bip_name__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_bip_name__a2,@function
.globl X0_current_bip_name__a2

X0_current_bip_name__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Bip_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24706C5F6572725F696E7374616E74696174696F6E__a0,@function
.globl X1_24706C5F6572725F696E7374616E74696174696F6E__a0

X1_24706C5F6572725F696E7374616E74696174696F6E__a0:
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1,@function
.globl X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1

X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F74797065__a2,@function
.globl X1_24706C5F6572725F74797065__a2

X1_24706C5F6572725F74797065__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F646F6D61696E__a2,@function
.globl X1_24706C5F6572725F646F6D61696E__a2

X1_24706C5F6572725F646F6D61696E__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+8, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F6578697374656E6365__a2,@function
.globl X1_24706C5F6572725F6578697374656E6365__a2

X1_24706C5F6572725F6578697374656E6365__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	movl	fn+12, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F7065726D697373696F6E__a3,@function
.globl X1_24706C5F6572725F7065726D697373696F6E__a3

X1_24706C5F6572725F7065726D697373696F6E__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+12
	movl	fn+16, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+12, %eax
	call	Pl_Unify_Local_Value
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
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F726570726573656E746174696F6E__a1,@function
.globl X1_24706C5F6572725F726570726573656E746174696F6E__a1

X1_24706C5F6572725F726570726573656E746174696F6E__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+20, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F6576616C756174696F6E__a1,@function
.globl X1_24706C5F6572725F6576616C756174696F6E__a1

X1_24706C5F6572725F6576616C756174696F6E__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+24, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F7265736F75726365__a1,@function
.globl X1_24706C5F6572725F7265736F75726365__a1

X1_24706C5F6572725F7265736F75726365__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+28, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F73796E746178__a1,@function
.globl X1_24706C5F6572725F73796E746178__a1

X1_24706C5F6572725F73796E746178__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+32, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572725F73797374656D__a1,@function
.globl X1_24706C5F6572725F73797374656D__a1

X1_24706C5F6572725F73797374656D__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	fn+36, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	jmp	X1_24706C5F6572726F72__a1
	.p2align	4,,15
	.type	X1_24706C5F6572726F72__a1,@function
.globl X1_24706C5F6572726F72__a1

X1_24706C5F6572726F72__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+8
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+0, %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Context_Error_1
	movl	fn+40, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+8, %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Unify_Value
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
	.p2align	4,,15
	.type	X0_syntax_error_info__a4,@function
.globl X0_syntax_error_info__a4

X0_syntax_error_info__a4:
	movl	at+64, %eax
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
	call	Pl_Syntax_Error_Info_4
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
	movl	%eax, at+28
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+60, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	$.LC17, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC18, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC19, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+8
	movl	$.LC20, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+40
	movl	$.LC21, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+24
	movl	$.LC22, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+12
	movl	$.LC23, %eax
	movl	$3, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+16
	movl	$.LC24, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+20
	movl	$.LC25, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+28
	movl	$.LC26, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+32
	movl	$.LC27, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+36
	movl	$.LC28, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	$.LC29, %eax
	movl	$1, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	at+4, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$42, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_bip_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$45, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_bip_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F696E7374616E74696174696F6E__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F756E696E7374616E74696174696F6E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$57, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F74797065__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F646F6D61696E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$63, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F6578697374656E6365__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$66, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F7065726D697373696F6E__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$69, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F726570726573656E746174696F6E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F6576616C756174696F6E__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$75, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F7265736F75726365__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$78, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F73796E746178__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572725F73797374656D__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$88, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706C5F6572726F72__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_syntax_error_info__a4+0, 8(%esp)
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
	.string	"$pl_err_domain"
.LC1:
	.string	"$pl_err_evaluation"
.LC2:
	.string	"$pl_err_existence"
.LC3:
	.string	"$pl_err_instantiation"
.LC4:
	.string	"$pl_err_permission"
.LC5:
	.string	"$pl_err_representation"
.LC6:
	.string	"$pl_err_resource"
.LC7:
	.string	"$pl_err_syntax"
.LC8:
	.string	"$pl_err_system"
.LC9:
	.string	"$pl_err_type"
.LC10:
	.string	"$pl_err_uninstantiation"
.LC11:
	.string	"$pl_error"
.LC12:
	.string	"/home/diaz/GP/src/BipsPl/pl_error.pl"
.LC13:
	.string	"current_bip_name"
.LC19:
	.string	"domain_error"
.LC20:
	.string	"error"
.LC21:
	.string	"evaluation_error"
.LC22:
	.string	"existence_error"
.LC17:
	.string	"instantiation_error"
.LC23:
	.string	"permission_error"
.LC24:
	.string	"representation_error"
.LC25:
	.string	"resource_error"
.LC14:
	.string	"set_bip_name"
.LC26:
	.string	"syntax_error"
.LC15:
	.string	"syntax_error_info"
.LC16:
	.string	"system"
.LC27:
	.string	"system_error"
.LC18:
	.string	"true"
.LC28:
	.string	"type_error"
.LC29:
	.string	"uninstantiation_error"
	.data	
	.align	4
	.local	at
	.comm	at,68,4
	.local	fn
	.comm	fn,44,4
	.local	ta
	.comm	ta,12,4
	.section	.note.GNU-stack,"",@progbits
