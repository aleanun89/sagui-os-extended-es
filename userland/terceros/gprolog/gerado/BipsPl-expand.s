# BipsPl/expand.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/expand.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F657870616E64__a0,@function
.globl X1_247573655F657870616E64__a0

X1_247573655F657870616E64__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_expand_term__a2,@function
.globl X0_expand_term__a2

X0_expand_term__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_24657870616E645F7465726D31__a2
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-16(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24657870616E645F7465726D31__a2,@function
.globl X1_24657870616E645F7465726D31__a2

X1_24657870616E645F7465726D31__a2:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+8
	movl	$1, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+8, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_2424657870616E645F7465726D312F325F2461757831__a2
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Cut
	call	Pl_Deallocate
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2424657870616E645F7465726D312F325F2461757831__a2,@function

X1_2424657870616E645F7465726D312F325F2461757831__a2:
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040

.Lpred4_1:
	movl	$.Lpred4_2+0, %eax
	call	Pl_Update_Choice_Point2
	jmp	X1_2463616C6C5F7465726D5F657870616E73696F6E__a2

.Lpred4_2:
	movl	$.Lpred4_3+0, %eax
	call	Pl_Update_Choice_Point2
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	jmp	X1_246463675F7472616E735F72756C65__a2

.Lpred4_3:
	call	Pl_Delete_Choice_Point2
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+4, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463616C6C5F7465726D5F657870616E73696F6E__a2,@function
.globl X1_2463616C6C5F7465726D5F657870616E73696F6E__a2

X1_2463616C6C5F7465726D5F657870616E73696F6E__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	fn+0, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	ta+0, %eax
	call	Pl_Unify_Atom_Tagged
	testl	%eax, %eax
	je	fail
	movl	$-2147483637, %eax
	call	Pl_Unify_Integer_Tagged
	testl	%eax, %eax
	je	fail
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X0_current_predicate__a1
.Lcont3:
	movl	fn+4, %eax
	call	Pl_Put_Structure_Tagged
	movl	%eax, pl_reg_bank+0
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	-20(%edi), %eax
	call	Pl_Unify_Local_Value
	testl	%eax, %eax
	je	fail
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2463616C6C__a4
	.p2align	4,,15
	.type	X0_phrase__a2,@function
.globl X0_phrase__a2

X0_phrase__a2:
	call	Pl_Put_Nil
	movl	%eax, pl_reg_bank+8
	movl	$-2147483637, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_24706872617365__a4
	.p2align	4,,15
	.type	X0_phrase__a3,@function
.globl X0_phrase__a3

X0_phrase__a3:
	movl	$-2147483633, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_24706872617365__a4
	.p2align	4,,15
	.type	X1_24706872617365__a4,@function
.globl X1_24706872617365__a4

X1_24706872617365__a4:
	movl	$7, %eax
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
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+0
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Bip_Name_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_24247068726173652F345F2461757831__a1
.Lcont4:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	leal	-40(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+12
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_246463675F7472616E735F626F6479__a4
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-32(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	movl	pl_reg_bank+0, %edx
	call	Pl_Unify
	testl	%eax, %eax
	je	fail
	movl	-40(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	ta+12, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	movl	-28(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	ta+8, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	call	Pl_Deallocate
	jmp	X1_2463616C6C__a4
	.p2align	4,,15
	.type	X1_24247068726173652F345F2461757831__a1,@function

X1_24247068726173652F345F2461757831__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred9_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	pl_reg_bank+0, %eax
	call	Pl_Blt_Var
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	X1_24706C5F6572725F696E7374616E74696174696F6E__a0

.Lpred9_1:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246463675F7472616E735F72756C65__a2,@function
.globl X1_246463675F7472616E735F72756C65__a2

X1_246463675F7472616E735F72756C65__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Dcg_Trans_Rule_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246463675F7472616E735F626F6479__a4,@function
.globl X1_246463675F7472616E735F626F6479__a4

X1_246463675F7472616E735F626F6479__a4:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	movl	pl_reg_bank+12, %eax
	movl	%eax, 12(%esp)
	call	Pl_Dcg_Trans_Body_4
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
	movl	%eax, at+40
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	at+28, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+4
	movl	at+32, %eax
	call	Pl_Put_Atom
	movl	%eax, ta+12
	movl	$.LC13, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC14, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+8
	movl	$.LC15, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+0
	movl	$.LC13, %eax
	movl	$2, %edx
	call	Pl_Create_Functor_Arity_Tagged
	movl	%eax, fn+4
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F657870616E64__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$48, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_expand_term__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24657870616E645F7465726D31__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$52, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_2424657870616E645F7465726D312F325F2461757831__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463616C6C5F7465726D5F657870616E73696F6E__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$71, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_phrase__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_phrase__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24706872617365__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$1, 4(%esp)
	movl	$X1_24247068726173652F345F2461757831__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$100, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246463675F7472616E735F72756C65__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$106, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246463675F7472616E735F626F6479__a4+0, 8(%esp)
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
	.string	"$$expand_term1/2_$aux1"
.LC1:
	.string	"$$phrase/4_$aux1"
.LC2:
	.string	"$call_term_expansion"
.LC3:
	.string	"$dcg_trans_body"
.LC4:
	.string	"$dcg_trans_rule"
.LC5:
	.string	"$expand_term1"
.LC6:
	.string	"$phrase"
.LC7:
	.string	"$use_expand"
.LC15:
	.string	"/"
.LC8:
	.string	"/home/diaz/GP/src/BipsPl/expand.pl"
.LC9:
	.string	"expand_term"
.LC10:
	.string	"phrase"
.LC11:
	.string	"system"
.LC13:
	.string	"term_expansion"
.LC14:
	.string	"true"
.LC12:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,52,4
	.local	fn
	.comm	fn,8,4
	.local	ta
	.comm	ta,16,4
	.section	.note.GNU-stack,"",@progbits
