# BipsPl/assert.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/assert.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F617373657274__a0,@function
.globl X1_247573655F617373657274__a0

X1_247573655F617373657274__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_asserta__a1,@function
.globl X0_asserta__a1

X0_asserta__a1:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_24617373657274__a4
	.p2align	4,,15
	.type	X0_assertz__a1,@function
.globl X0_assertz__a1

X0_assertz__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+4
	movl	$-2147483641, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	ta+0, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+12
	jmp	X1_24617373657274__a4
	.p2align	4,,15
	.type	X1_24617373657274__a4,@function
.globl X1_24617373657274__a4

X1_24617373657274__a4:
	movl	$.Lpred4_1+0, %eax
	call	Pl_Create_Choice_Point4
	movl	$6, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+4, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+8, %eax
	movl	%eax, -20(%edi)
	movl	pl_reg_bank+12, %eax
	movl	%eax, -24(%edi)
	leal	-28(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-32(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_246765745F686561645F616E645F626F6479__a3
.Lcont1:
	movl	pl_reg_bank+1044, %edi
	movl	-32(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	ta+4, %eax
	call	Pl_Put_Atom_Tagged
	movl	%eax, pl_reg_bank+4
	leal	-36(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247465726D5F746F5F676F616C__a3
.Lcont2:
	movl	pl_reg_bank+1044, %edi
	movl	-28(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-36(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+8
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+12
	movl	-24(%edi), %eax
	movl	%eax, pl_reg_bank+16
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
	call	Pl_Assert_5
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred4_1:
	call	Pl_Delete_Choice_Point4
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_retract__a1,@function
.globl X0_retract__a1

X0_retract__a1:
	movl	$2, %eax
	call	Pl_Allocate
	movl	at+24, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+1044, %edi
	leal	-16(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+4
	leal	-20(%edi), %eax
	call	Pl_Put_Y_Variable
	movl	%eax, pl_reg_bank+8
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_246765745F686561645F616E645F626F6479__a3
.Lcont3:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	call	Pl_Put_Unsafe_Value
	movl	%eax, pl_reg_bank+4
	call	Pl_Deallocate
	jmp	X1_2472657472616374__a2
	.p2align	4,,15
	.type	X1_2472657472616374__a2,@function
.globl X1_2472657472616374__a2

X1_2472657472616374__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Retract_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_retractall__a1,@function
.globl X0_retractall__a1

X0_retractall__a1:
	call	Pl_Get_Current_Choice
	movl	%eax, pl_reg_bank+4
	movl	$.Lpred7_1+0, %eax
	call	Pl_Create_Choice_Point2
	movl	at+32, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Retractall_If_Empty_Head_1
	testl	%eax, %eax
	je	fail
	movl	pl_reg_bank+4, %eax
	call	Pl_Cut
	jmp	*pl_reg_bank+1040

.Lpred7_1:
	movl	$.Lpred7_2+0, %eax
	call	Pl_Update_Choice_Point2
	movl	$0, %eax
	call	Pl_Allocate
	call	Pl_Put_X_Variable
	movl	%eax, pl_reg_bank+4
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_2472657472616374__a2
.Lcont4:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)

.Lpred7_2:
	call	Pl_Delete_Choice_Point2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24726574726163745F6C6173745F666F756E64__a0,@function
.globl X1_24726574726163745F6C6173745F666F756E64__a0

X1_24726574726163745F6C6173745F666F756E64__a0:
	call	Pl_Retract_Last_Found_0
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_clause__a2,@function
.globl X0_clause__a2

X0_clause__a2:
	movl	$2, %eax
	call	Pl_Allocate
	movl	pl_reg_bank+0, %eax
	movl	pl_reg_bank+1044, %edi
	movl	%eax, -16(%edi)
	movl	pl_reg_bank+4, %eax
	movl	%eax, -20(%edi)
	movl	at+40, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_24636865636B5F68656164__a1
.Lcont5:
	movl	pl_reg_bank+1044, %edi
	movl	-16(%edi), %eax
	movl	%eax, pl_reg_bank+0
	movl	-20(%edi), %eax
	movl	%eax, pl_reg_bank+4
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	call	Pl_Deallocate
	jmp	X1_24636C61757365__a3
	.p2align	4,,15
	.type	X1_24636C61757365__a3,@function
.globl X1_24636C61757365__a3

X1_24636C61757365__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Clause_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24696E7374616E63655F666F725F736574617267__a2,@function
.globl X1_24696E7374616E63655F666F725F736574617267__a2

X1_24696E7374616E63655F666F725F736574617267__a2:
	movl	$-2147483645, %eax
	call	Pl_Put_Integer_Tagged
	movl	%eax, pl_reg_bank+8
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Clause_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365746172675F696E5F6C6173745F666F756E64__a2,@function
.globl X1_247365746172675F696E5F6C6173745F666F756E64__a2

X1_247365746172675F696E5F6C6173745F666F756E64__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Setarg_Of_Last_Found_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_abolish__a1,@function
.globl X0_abolish__a1

X0_abolish__a1:
	movl	at+56, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Abolish_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2472656D6F76655F707265646963617465__a2,@function
.globl X1_2472656D6F76655F707265646963617465__a2

X1_2472656D6F76655F707265646963617465__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Remove_Predicate_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247363616E5F64796E5F746573745F616C74__a0,@function
.globl X1_247363616E5F64796E5F746573745F616C74__a0

X1_247363616E5F64796E5F746573745F616C74__a0:
	call	Pl_Scan_Dynamic_Pred_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247363616E5F64796E5F6A756D705F616C74__a0,@function
.globl X1_247363616E5F64796E5F6A756D705F616C74__a0

X1_247363616E5F64796E5F6A756D705F616C74__a0:
	call	Pl_Scan_Dynamic_Pred_Alt_0
	jmp	*%eax
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
	movl	%eax, at+44
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC18, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+0
	movl	$.LC19, %eax
	call	Pl_Create_Atom_Tagged
	movl	%eax, ta+4
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F617373657274__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_asserta__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_assertz__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24617373657274__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$69, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_retract__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$75, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2472657472616374__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$81, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_retractall__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$94, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24726574726163745F6C6173745F666F756E64__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$99, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_clause__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$105, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24636C61757365__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$111, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24696E7374616E63655F666F725F736574617267__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$114, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365746172675F696E5F6C6173745F666F756E64__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$119, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_abolish__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$126, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2472656D6F76655F707265646963617465__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$132, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247363616E5F64796E5F746573745F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$135, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247363616E5F64796E5F6A756D705F616C74__a0+0, 8(%esp)
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
.LC18:
	.string	""
.LC0:
	.string	"$assert"
.LC1:
	.string	"$clause"
.LC2:
	.string	"$instance_for_setarg"
.LC3:
	.string	"$remove_predicate"
.LC4:
	.string	"$retract"
.LC5:
	.string	"$retract_last_found"
.LC6:
	.string	"$scan_dyn_jump_alt"
.LC7:
	.string	"$scan_dyn_test_alt"
.LC8:
	.string	"$setarg_in_last_found"
.LC9:
	.string	"$use_assert"
.LC10:
	.string	"/home/diaz/GP/src/BipsPl/assert.pl"
.LC11:
	.string	"abolish"
.LC12:
	.string	"asserta"
.LC13:
	.string	"assertz"
.LC14:
	.string	"clause"
.LC19:
	.string	"none"
.LC15:
	.string	"retract"
.LC16:
	.string	"retractall"
.LC17:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,72,4
	.local	ta
	.comm	ta,8,4
	.section	.note.GNU-stack,"",@progbits
