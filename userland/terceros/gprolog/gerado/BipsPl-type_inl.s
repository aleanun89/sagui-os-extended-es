# BipsPl/type_inl.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/type_inl.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F747970655F696E6C__a0,@function
.globl X1_247573655F747970655F696E6C__a0

X1_247573655F747970655F696E6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_var__a1,@function
.globl X0_var__a1

X0_var__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_nonvar__a1,@function
.globl X0_nonvar__a1

X0_nonvar__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atom__a1,@function
.globl X0_atom__a1

X0_atom__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atom
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_integer__a1,@function
.globl X0_integer__a1

X0_integer__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Integer
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_float__a1,@function
.globl X0_float__a1

X0_float__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Float
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_number__a1,@function
.globl X0_number__a1

X0_number__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Number
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_atomic__a1,@function
.globl X0_atomic__a1

X0_atomic__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Atomic
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_compound__a1,@function
.globl X0_compound__a1

X0_compound__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Compound
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_callable__a1,@function
.globl X0_callable__a1

X0_callable__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Callable
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_ground__a1,@function
.globl X0_ground__a1

X0_ground__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Ground
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_is_list__a1,@function
.globl X0_is_list__a1

X0_is_list__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_list__a1,@function
.globl X0_list__a1

X0_list__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_partial_list__a1,@function
.globl X0_partial_list__a1

X0_partial_list__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Partial_List
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_list_or_partial_list__a1,@function
.globl X0_list_or_partial_list__a1

X0_list_or_partial_list__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_List_Or_Partial_List
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_fd_var__a1,@function
.globl X0_fd_var__a1

X0_fd_var__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Fd_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_non_fd_var__a1,@function
.globl X0_non_fd_var__a1

X0_non_fd_var__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Fd_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_generic_var__a1,@function
.globl X0_generic_var__a1

X0_generic_var__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Generic_Var
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_non_generic_var__a1,@function
.globl X0_non_generic_var__a1

X0_non_generic_var__a1:
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Non_Generic_Var
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
	movl	%eax, at+4
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F747970655F696E6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_var__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$47, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_nonvar__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$50, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atom__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$53, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_integer__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$56, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_float__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$59, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_number__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$62, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_atomic__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_compound__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$68, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_callable__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_ground__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$74, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_is_list__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_list__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$80, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_partial_list__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_list_or_partial_list__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$145, 4(%esp)
	movl	$X0_fd_var__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$92, 0(%esp)
	movl	$145, 4(%esp)
	movl	$X0_non_fd_var__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$145, 4(%esp)
	movl	$X0_generic_var__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$98, 0(%esp)
	movl	$145, 4(%esp)
	movl	$X0_non_generic_var__a1+0, 8(%esp)
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
	.string	"$use_type_inl"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/type_inl.pl"
.LC2:
	.string	"atom"
.LC3:
	.string	"atomic"
.LC4:
	.string	"callable"
.LC5:
	.string	"compound"
.LC6:
	.string	"fd_var"
.LC7:
	.string	"float"
.LC8:
	.string	"generic_var"
.LC9:
	.string	"ground"
.LC10:
	.string	"integer"
.LC11:
	.string	"is_list"
.LC12:
	.string	"list"
.LC13:
	.string	"list_or_partial_list"
.LC14:
	.string	"non_fd_var"
.LC15:
	.string	"non_generic_var"
.LC16:
	.string	"nonvar"
.LC17:
	.string	"number"
.LC18:
	.string	"partial_list"
.LC19:
	.string	"system"
.LC20:
	.string	"var"
	.data	
	.align	4
	.local	at
	.comm	at,84,4
	.section	.note.GNU-stack,"",@progbits
