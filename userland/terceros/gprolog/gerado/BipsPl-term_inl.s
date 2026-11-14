# BipsPl/term_inl.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/term_inl.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F7465726D5F696E6C__a0,@function
.globl X1_247573655F7465726D5F696E6C__a0

X1_247573655F7465726D5F696E6C__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_compare__a3,@function
.globl X0_compare__a3

X0_compare__a3:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Compare
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D3D__a2,@function
.globl X1_3D3D__a2

X1_3D3D__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Eq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_5C5C3D3D__a2,@function
.globl X1_5C5C3D3D__a2

X1_5C5C3D3D__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Neq
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_403C__a2,@function
.globl X1_403C__a2

X1_403C__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Lt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_403D3C__a2,@function
.globl X1_403D3C__a2

X1_403D3C__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Lte
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_403E__a2,@function
.globl X1_403E__a2

X1_403E__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Gt
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_403E3D__a2,@function
.globl X1_403E3D__a2

X1_403E3D__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Term_Gte
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_arg__a3,@function
.globl X0_arg__a3

X0_arg__a3:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Arg
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_functor__a3,@function
.globl X0_functor__a3

X0_functor__a3:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	movl	pl_reg_bank+8, %ecx
	call	Pl_Blt_Functor
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_3D2E2E__a2,@function
.globl X1_3D2E2E__a2

X1_3D2E2E__a2:
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Blt_Univ
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_copy_term__a2,@function
.globl X0_copy_term__a2

X0_copy_term__a2:
	movl	at+52, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Copy_Term_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_setarg__a3,@function
.globl X0_setarg__a3

X0_setarg__a3:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Setarg_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_setarg__a4,@function
.globl X0_setarg__a4

X0_setarg__a4:
	movl	at+56, %eax
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
	call	Pl_Setarg_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_term_ref__a2,@function
.globl X0_term_ref__a2

X0_term_ref__a2:
	movl	at+60, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Term_Ref_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_term_variables__a2,@function
.globl X0_term_variables__a2

X0_term_variables__a2:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Term_Variables_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_term_variables__a3,@function
.globl X0_term_variables__a3

X0_term_variables__a3:
	movl	at+64, %eax
	movl	%eax, 0(%esp)
	movl	$3, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Term_Variables_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_subsumes_term__a2,@function
.globl X0_subsumes_term__a2

X0_subsumes_term__a2:
	movl	at+68, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Subsumes_Term_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_acyclic_term__a1,@function
.globl X0_acyclic_term__a1

X0_acyclic_term__a1:
	movl	at+72, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Acyclic_Term_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_term_hash__a4,@function
.globl X0_term_hash__a4

X0_term_hash__a4:
	movl	at+76, %eax
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
	call	Pl_Term_Hash_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_term_hash__a2,@function
.globl X0_term_hash__a2

X0_term_hash__a2:
	movl	at+76, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Term_Hash_2
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
	movl	%eax, at+48
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC20, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F7465726D5F696E6C__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_compare__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$48, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_5C5C3D3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$54, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_403C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$57, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_403D3C__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$60, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_403E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$63, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_403E3D__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$69, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_arg__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$75, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_functor__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_3D2E2E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$87, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_copy_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$94, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_setarg__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_setarg__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_term_ref__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$112, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_term_variables__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$117, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_term_variables__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$123, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_subsumes_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$129, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_acyclic_term__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$135, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_term_hash__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$139, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_term_hash__a2+0, 8(%esp)
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
	.string	"$use_term_inl"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/term_inl.pl"
.LC2:
	.string	"=.."
.LC3:
	.string	"=="
.LC4:
	.string	"@<"
.LC5:
	.string	"@=<"
.LC6:
	.string	"@>"
.LC7:
	.string	"@>="
.LC8:
	.string	"\\=="
.LC9:
	.string	"acyclic_term"
.LC10:
	.string	"arg"
.LC11:
	.string	"compare"
.LC12:
	.string	"copy_term"
.LC13:
	.string	"functor"
.LC14:
	.string	"setarg"
.LC15:
	.string	"subsumes_term"
.LC16:
	.string	"system"
.LC17:
	.string	"term_hash"
.LC18:
	.string	"term_ref"
.LC19:
	.string	"term_variables"
.LC20:
	.string	"true"
	.data	
	.align	4
	.local	at
	.comm	at,80,4
	.local	ta
	.comm	ta,4,4
	.section	.note.GNU-stack,"",@progbits
