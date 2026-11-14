# BipsPl/file.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/file.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F66696C65__a0,@function
.globl X1_247573655F66696C65__a0

X1_247573655F66696C65__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_absolute_file_name__a2,@function
.globl X0_absolute_file_name__a2

X0_absolute_file_name__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Absolute_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_is_absolute_file_name__a1,@function
.globl X0_is_absolute_file_name__a1

X0_is_absolute_file_name__a1:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Is_Absolute_File_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_is_relative_file_name__a1,@function
.globl X0_is_relative_file_name__a1

X0_is_relative_file_name__a1:
	movl	at+20, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Is_Relative_File_Name_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_decompose_file_name__a4,@function
.globl X0_decompose_file_name__a4

X0_decompose_file_name__a4:
	movl	at+24, %eax
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
	call	Pl_Decompose_File_Name_4
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_prolog_file_name__a2,@function
.globl X0_prolog_file_name__a2

X0_prolog_file_name__a2:
	movl	at+28, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Prolog_File_Name_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2470726F6C6F675F66696C655F737566666978__a1,@function
.globl X1_2470726F6C6F675F66696C655F737566666978__a1

X1_2470726F6C6F675F66696C655F737566666978__a1:
	movl	at+36, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Prolog_File_Suffix_1
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
	movl	%eax, at+32
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+20
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F66696C65__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_absolute_file_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_is_absolute_file_name__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_is_relative_file_name__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$4, %edx
	movl	at+0, %ecx
	movl	$65, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_decompose_file_name__a4+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$72, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_prolog_file_name__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$79, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2470726F6C6F675F66696C655F737566666978__a1+0, 8(%esp)
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
	.string	"$prolog_file_suffix"
.LC1:
	.string	"$use_file"
.LC2:
	.string	"/home/diaz/GP/src/BipsPl/file.pl"
.LC3:
	.string	"absolute_file_name"
.LC4:
	.string	"decompose_file_name"
.LC5:
	.string	"is_absolute_file_name"
.LC6:
	.string	"is_relative_file_name"
.LC7:
	.string	"prolog_file_name"
.LC8:
	.string	"prolog_file_suffix"
.LC9:
	.string	"system"
	.data	
	.align	4
	.local	at
	.comm	at,40,4
	.section	.note.GNU-stack,"",@progbits
